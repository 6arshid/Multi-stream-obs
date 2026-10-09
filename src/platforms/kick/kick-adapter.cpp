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

constexpr const char *kApiBase = "https://api.kick.com/public/v1/";

QString kickError(const HttpResponse &response)
{
	if (response.status == 401)
		return QObject::tr("Kick authorization expired or invalid - reconnect the account");
	if (response.status == 403)
		return QObject::tr("Kick denied the request - check the granted scopes");
	if (response.status == 429)
		return QObject::tr("Kick API rate limit exceeded - try again shortly");

	const QJsonObject root = QJsonDocument::fromJson(response.body).object();
	QString message = root.value(QStringLiteral("message")).toString();
	if (message.isEmpty()) {
		const QJsonArray errors = root.value(QStringLiteral("errors")).toArray();
		if (!errors.isEmpty())
			message = errors.at(0).toObject().value(QStringLiteral("message")).toString();
	}
	if (message.isEmpty() && !response.transportError.isEmpty())
		message = response.transportError;
	if (message.isEmpty())
		message = QObject::tr("Kick API request failed (HTTP %1)").arg(response.status);
	return message;
}

QJsonValue kickData(const QByteArray &body)
{
	return QJsonDocument::fromJson(body).object().value(QStringLiteral("data"));
}

QJsonArray kickDataArray(const QByteArray &body)
{
	const QJsonValue data = kickData(body);
	if (data.isArray())
		return data.toArray();
	if (data.isObject())
		return QJsonArray{data.toObject()};
	return {};
}

QHash<QString, QString> kickHeaders(const AuthContext &auth)
{
	return bearerHeaders(auth.token.accessToken, auth.client.clientId);
}

class KickAdapter final : public IPlatformAdapter {
public:
	PlatformInfo info() const override
	{
		PlatformInfo info;
		info.id = QStringLiteral("kick");
		info.displayName = QObject::tr("Kick");
		info.description =
		    QObject::tr("OAuth 2.1 (PKCE) sign-in reads your channel's stream URL and key "
		                "through the public Kick API; the API can also update the stream "
		                "title before you go live.");
		info.docsUrl = QStringLiteral("https://dev.kick.com/");
		info.portalUrl = QStringLiteral("https://kick.com/");
		info.ingestHint =
		    QObject::tr("API mode resolves the endpoint automatically. Manual mode: copy the "
		                "Server URL and Stream key from Kick channel settings.");
		info.maxBitrateKbps = 8000;
		info.sessionBasedKey = false;
		info.caps.oauth = FeatureStatus::Full;
		info.caps.broadcastCreate = FeatureStatus::Unavailable;
		info.caps.broadcastSchedule = FeatureStatus::Unavailable;
		info.caps.streamKeyRetrieval = FeatureStatus::Full;
		info.caps.manualIngest = FeatureStatus::Full;
		info.caps.metadataManagement = FeatureStatus::Partial;
		info.caps.liveStatusMonitoring = FeatureStatus::NotImplemented;
		info.caps.apiStartStop = FeatureStatus::Unavailable;
		info.caps.rtmpIngest = FeatureStatus::Full;
		info.caps.chat = FeatureStatus::NotImplemented;
		info.caps.analytics = FeatureStatus::NotImplemented;
		info.implemented = {Feature::OAuth,         Feature::StreamKeyRetrieval,
		                    Feature::ManualIngest, Feature::MetadataManagement,
		                    Feature::RtmpIngest};
		return info;
	}

	OAuthSpec oauthSpec() const override
	{
		OAuthSpec spec;
		spec.authUrl = QStringLiteral("https://id.kick.com/oauth/authorize");
		spec.tokenUrl = QStringLiteral("https://id.kick.com/oauth/token");
		spec.scopes = {
			QStringLiteral("channel:read"),
			QStringLiteral("streamkey:read"),
			QStringLiteral("channel:write"),
			QStringLiteral("user:read"),
		};
		spec.pkce = true; /* OAuth 2.1: S256 PKCE is mandatory for public clients */
		return spec;
	}

	QStringList validate(const DestinationConfig &destination) const override
	{
		QStringList errors = IPlatformAdapter::validate(destination);
		if (destination.authMode == AuthMode::Manual && destination.server.trimmed().isEmpty())
			errors << QObject::tr("paste the Kick Server URL (rtmps://stream.kick.com/<channel id>)");
		return errors;
	}

	void listAccounts(const AuthContext &auth, AccountsCallback callback) override
	{
		if (!auth.http || auth.token.accessToken.isEmpty()) {
			callback(false, {}, QObject::tr("connect a Kick account first"));
			return;
		}
		const QString url = QStringLiteral("%1users").arg(kApiBase);
		auth.http->send(
		    QtHttpClient::get(url, kickHeaders(auth)),
		    [callback](const HttpResponse &response) {
			    const QJsonArray data = kickDataArray(response.body);
			    if (!response.ok || response.status >= 400 || data.isEmpty()) {
				    callback(false, {}, kickError(response));
				    return;
			    }
			    const QJsonObject user = data.at(0).toObject();
			    AccountInfo account;
			    const QJsonValue userId = user.value(QStringLiteral("user_id"));
			    account.alias = userId.isString() ? userId.toString()
			                                      : QString::number(
			                                            static_cast<qint64>(userId.toDouble()));
			    account.displayName = user.value(QStringLiteral("name")).toString();
			    if (account.displayName.isEmpty())
				    account.displayName = account.alias;
			    account.metadata = user;
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
			endpoint.error = QObject::tr("connect a Kick account first");
			callback(endpoint);
			return;
		}

		const QString title = destination.options.value(QStringLiteral("title")).toString();
		auto fetchKey = [this, auth, destination, callback]() {
			fetchChannelStream(auth, destination, callback);
		};
		if (title.isEmpty()) {
			fetchKey();
			return;
		}
		updateTitle(auth, title, [fetchKey](const QString &) { fetchKey(); });
	}

private:
	void fetchChannelStream(const AuthContext &auth, const DestinationConfig &destination,
	                        IngestCallback callback)
	{
		Q_UNUSED(destination);
		const QString url = QStringLiteral("%1channels").arg(kApiBase);
		auth.http->send(
		    QtHttpClient::get(url, kickHeaders(auth)),
		    [callback](const HttpResponse &response) {
			    IngestEndpoint endpoint;
			    const QJsonArray data = kickDataArray(response.body);
			    if (!response.ok || response.status >= 400 || data.isEmpty()) {
				    endpoint.error = kickError(response);
				    callback(endpoint);
				    return;
			    }
			    const QJsonObject stream =
			        data.at(0).toObject().value(QStringLiteral("stream")).toObject();
			    endpoint.server = stream.value(QStringLiteral("url")).toString();
			    endpoint.key = stream.value(QStringLiteral("key")).toString();
			    if (endpoint.server.isEmpty() || endpoint.key.isEmpty()) {
				    endpoint.server.clear();
				    endpoint.error = QObject::tr(
				        "Kick returned no stream endpoint - confirm the streamkey:read "
				        "scope was granted");
			    }
			    callback(endpoint);
		    });
	}

	void updateTitle(const AuthContext &auth, const QString &title,
	                 std::function<void(const QString &)> callback)
	{
		QJsonObject body;
		body.insert(QStringLiteral("stream_title"), title);
		HttpRequest request;
		request.method = QStringLiteral("PATCH");
		request.url = QStringLiteral("%1channels").arg(kApiBase);
		request.headers = kickHeaders(auth);
		request.headers.insert(QStringLiteral("Content-Type"),
		                       QStringLiteral("application/json"));
		request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
		auth.http->send(request, [callback](const HttpResponse &response) {
			if (!response.ok || response.status >= 400)
				callback(kickError(response));
			else
				callback({});
		});
	}
};

} // namespace

AdapterPtr createKickAdapter()
{
	return std::make_unique<KickAdapter>();
}

} // namespace ums
