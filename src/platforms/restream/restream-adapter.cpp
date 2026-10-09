/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platforms/factories.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QObject>

namespace ums {

namespace {

constexpr const char *kApiBase = "https://api.restream.io/v2/";
constexpr const char *kDefaultIngest = "rtmp://live.restream.io/live";

QString restreamError(const HttpResponse &response)
{
	if (response.status == 401)
		return QObject::tr("Restream authorization expired or invalid - reconnect the account");
	if (response.status == 403)
		return QObject::tr("Restream denied the request - check the granted scopes");
	if (response.status == 429)
		return QObject::tr("Restream API rate limit exceeded - try again shortly");

	const QJsonObject root = QJsonDocument::fromJson(response.body).object();
	QString message = root.value(QStringLiteral("message")).toString();
	if (message.isEmpty())
		message = root.value(QStringLiteral("error")).toString();
	if (message.isEmpty() && !response.transportError.isEmpty())
		message = response.transportError;
	if (message.isEmpty())
		message = QObject::tr("Restream API request failed (HTTP %1)").arg(response.status);
	return message;
}

QHash<QString, QString> restreamHeaders(const AuthContext &auth)
{
	return bearerHeaders(auth.token.accessToken);
}

class RestreamAdapter final : public IPlatformAdapter {
public:
	PlatformInfo info() const override
	{
		PlatformInfo info;
		info.id = QStringLiteral("restream");
		info.displayName = QObject::tr("Restream");
		info.description =
		    QObject::tr("Sign in to Restream to read your stream key and choose the "
		                "nearest ingest server from Restream's own server list, then "
		                "Restream fans your single feed out to every connected channel.");
		info.docsUrl = QStringLiteral("https://developers.restream.io/");
		info.portalUrl = QStringLiteral("https://restream.io/");
		info.defaultIngestUrl = QString::fromLatin1(kDefaultIngest);
		info.ingestHint =
		    QObject::tr("API mode resolves the key and the best server automatically; "
		                "manual mode uses the key from the Restream dashboard.");
		info.sessionBasedKey = false;
		info.caps.oauth = FeatureStatus::Full;
		info.caps.broadcastCreate = FeatureStatus::NotImplemented;
		info.caps.broadcastSchedule = FeatureStatus::NotImplemented;
		info.caps.streamKeyRetrieval = FeatureStatus::Full;
		info.caps.manualIngest = FeatureStatus::Full;
		info.caps.metadataManagement = FeatureStatus::NotImplemented;
		info.caps.liveStatusMonitoring = FeatureStatus::NotImplemented;
		info.caps.apiStartStop = FeatureStatus::Unavailable;
		info.caps.rtmpIngest = FeatureStatus::Full;
		info.caps.chat = FeatureStatus::NotImplemented;
		info.caps.analytics = FeatureStatus::NotImplemented;
		info.implemented = {Feature::OAuth, Feature::StreamKeyRetrieval,
		                    Feature::ManualIngest, Feature::RtmpIngest};
		return info;
	}

	OAuthSpec oauthSpec() const override
	{
		OAuthSpec spec;
		spec.authUrl = QStringLiteral("https://api.restream.io/oauth/authorize");
		spec.tokenUrl = QStringLiteral("https://api.restream.io/oauth/token");
		spec.refreshUrl = QStringLiteral("https://api.restream.io/oauth/token");
		spec.scopes = {
			QStringLiteral("profile.read"),
			QStringLiteral("stream.read"),
			QStringLiteral("channels.read"),
		};
		spec.requiresClientSecret = true;
		return spec;
	}

	void listAccounts(const AuthContext &auth, AccountsCallback callback) override
	{
		if (!auth.http || auth.token.accessToken.isEmpty()) {
			callback(false, {}, QObject::tr("connect a Restream account first"));
			return;
		}
		const QString url = QStringLiteral("%1user/profile").arg(kApiBase);
		auth.http->send(
		    QtHttpClient::get(url, restreamHeaders(auth)),
		    [callback](const HttpResponse &response) {
			    const QJsonObject profile =
			        QJsonDocument::fromJson(response.body).object();
			    if (!response.ok || response.status >= 400) {
				    callback(false, {}, restreamError(response));
				    return;
			    }
			    AccountInfo account;
			    const QJsonValue id = profile.value(QStringLiteral("id"));
			    account.alias = id.isString() ? id.toString()
			                                  : QString::number(
			                                        static_cast<qint64>(id.toDouble()));
			    account.displayName =
			        profile.value(QStringLiteral("username")).toString();
			    if (account.displayName.isEmpty())
				    account.displayName = account.alias;
			    account.metadata = profile;
			    callback(true, {account}, {});
		    });
	}

	void validateToken(const AuthContext &auth, TokenCheckCallback callback) override
	{
		listAccounts(auth, [callback](bool ok, const QVector<AccountInfo> &accounts,
		                              const QString &error) {
			if (!ok) {
				callback(false, {}, error);
				return;
			}
			callback(true, accounts.first().displayName, {});
		});
	}

	void prepareIngest(const AuthContext &auth, const DestinationConfig &destination,
	                   IngestCallback callback) override
	{
		if (destination.authMode == AuthMode::Manual) {
			callback(resolveManualIngest(info(), auth, destination));
			return;
		}
		if (!auth.http || auth.token.accessToken.isEmpty()) {
			IngestEndpoint endpoint;
			endpoint.error = QObject::tr("connect a Restream account first");
			callback(endpoint);
			return;
		}
		const QString url = QStringLiteral("%1user/streamKey").arg(kApiBase);
		auth.http->send(
		    QtHttpClient::get(url, restreamHeaders(auth)),
		    [this, auth, destination, callback](const HttpResponse &response) {
			    IngestEndpoint endpoint;
			    const QJsonObject object =
			        QJsonDocument::fromJson(response.body).object();
			    if (!response.ok || response.status >= 400) {
				    endpoint.error = restreamError(response);
				    callback(endpoint);
				    return;
			    }
			    endpoint.key = object.value(QStringLiteral("streamKey")).toString();
			    if (endpoint.key.isEmpty()) {
				    endpoint.error =
				        QObject::tr("Restream returned an empty stream key - check the "
				                    "stream.read authorization");
				    callback(endpoint);
				    return;
			    }
			    if (!destination.server.trimmed().isEmpty()) {
				    endpoint.server = destination.server.trimmed();
				    callback(endpoint);
				    return;
			    }
			    resolveServer(auth, [endpoint, callback](QString server) mutable {
				    if (server.isEmpty())
				        server = QString::fromLatin1(kDefaultIngest);
				    endpoint.server = server;
				    callback(endpoint);
			    });
		    });
	}

private:
	void resolveServer(const AuthContext &auth, std::function<void(QString)> callback)
	{
		/* The selected ingest id maps to a server from the public list. */
		const QString ingestUrl = QStringLiteral("%1user/ingest").arg(kApiBase);
		auth.http->send(
		    QtHttpClient::get(ingestUrl, restreamHeaders(auth)),
		    [this, auth, callback](const HttpResponse &ingestResponse) {
			    int ingestId = -1;
			    if (ingestResponse.ok && ingestResponse.status < 400)
				    ingestId = QJsonDocument::fromJson(ingestResponse.body).object()
				                   .value(QStringLiteral("ingestId")).toInt(-1);
			    const QString serversUrl = QStringLiteral("%1server/all").arg(kApiBase);
			    auth.http->send(
			        QtHttpClient::get(serversUrl, restreamHeaders(auth)),
			        [ingestId, callback](const HttpResponse &serversResponse) {
				        QString server;
				        if (serversResponse.ok && serversResponse.status < 400) {
					        const QJsonArray servers =
					            QJsonDocument::fromJson(serversResponse.body).array();
					        for (const QJsonValue &value : servers) {
						        const QJsonObject entry = value.toObject();
						        if (ingestId >= 0 &&
						            entry.value(QStringLiteral("id")).toInt(-1) == ingestId) {
							        server = entry.value(QStringLiteral("rtmpUrl")).toString();
							        break;
						        }
					        }
				        }
				        callback(server);
			        });
		    });
	}
};

} // namespace

AdapterPtr createRestreamAdapter()
{
	return std::make_unique<RestreamAdapter>();
}

} // namespace ums
