/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platforms/factories.hpp"

#include <QJsonDocument>
#include <QObject>

namespace ums {

namespace {

constexpr const char *kApiBase = "https://open-api.trovo.live/openplatform/";
constexpr const char *kDefaultIngest = "rtmp://livepush.trovo.live/live";

QString trovoError(const HttpResponse &response)
{
	if (response.status == 401)
		return QObject::tr("Trovo authorization expired or invalid - reconnect the account");

	const QJsonObject root = QJsonDocument::fromJson(response.body).object();
	QString message = root.value(QStringLiteral("msg")).toString();
	if (message.isEmpty())
		message = root.value(QStringLiteral("message")).toString();
	if (message.isEmpty() && !response.transportError.isEmpty())
		message = response.transportError;
	if (message.isEmpty())
		message = QObject::tr("Trovo API request failed (HTTP %1)").arg(response.status);
	return message;
}

QHash<QString, QString> trovoHeaders(const AuthContext &auth)
{
	QHash<QString, QString> headers;
	headers.insert(QStringLiteral("Accept"), QStringLiteral("application/json"));
	headers.insert(QStringLiteral("Client-ID"), auth.client.clientId);
	if (!auth.token.accessToken.isEmpty())
		headers.insert(QStringLiteral("Authorization"),
		               QStringLiteral("OAuth %1").arg(auth.token.accessToken));
	return headers;
}

/* Trovo's API returns stream keys as "live/<key>" while the documented
 * ingest server already contains the /live application path. */
QString normalizeTrovoKey(const QString *rawKey, QString *server)
{
	QString key = *rawKey;
	if (key.startsWith(QLatin1String("live/"))) {
		key = key.mid(5);
		*server = QStringLiteral("rtmp://livepush.trovo.live/live");
	}
	return key;
}

class TrovoAdapter final : public IPlatformAdapter {
public:
	PlatformInfo info() const override
	{
		PlatformInfo info;
		info.id = QStringLiteral("trovo");
		info.displayName = QObject::tr("Trovo");
		info.description =
		    QObject::tr("Trovo Open Platform API reads your stream key, can update the "
		                "stream title, and validates tokens through the official "
		                "developer program.");
		info.docsUrl =
		    QStringLiteral("https://developer.trovo.live/docs/APIs.html");
		info.portalUrl = QStringLiteral("https://studio.trovo.live/");
		info.defaultIngestUrl = QString::fromLatin1(kDefaultIngest);
		info.ingestHint =
		    QObject::tr("Manual mode: Creator Studio -> Stream shows the Host URL and "
		                "Stream key. API mode reads the key for you.");
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
		spec.authUrl = QStringLiteral("https://open.trovo.live/page/login.html");
		spec.tokenUrl =
		    QStringLiteral("https://open-api.trovo.live/openplatform/exchangetoken");
		spec.refreshUrl =
		    QStringLiteral("https://open-api.trovo.live/openplatform/refreshtoken");
		/* Trovo requires "+"-separated scopes in one parameter value. */
		spec.scopes = {QStringLiteral("channel_details_self+channel_update_self"
		                              "+user_details_self")};
		spec.requiresClientSecret = true;
		spec.jsonTokenBody = true;
		spec.clientIdHeader = QStringLiteral("Client-ID");
		return spec;
	}

	void listAccounts(const AuthContext &auth, AccountsCallback callback) override
	{
		if (!auth.http || auth.token.accessToken.isEmpty()) {
			callback(false, {}, QObject::tr("connect a Trovo account first"));
			return;
		}
		const QString url = QStringLiteral("%1getuserinfo").arg(kApiBase);
		auth.http->send(
		    QtHttpClient::get(url, trovoHeaders(auth)),
		    [callback](const HttpResponse &response) {
			    const QJsonObject user =
			        QJsonDocument::fromJson(response.body).object();
			    if (!response.ok || response.status >= 400) {
				    callback(false, {}, trovoError(response));
				    return;
			    }
			    AccountInfo account;
			    account.alias = user.value(QStringLiteral("userId")).toString();
			    account.displayName = user.value(QStringLiteral("nickName")).toString();
			    if (account.displayName.isEmpty())
				    account.displayName = user.value(QStringLiteral("userName")).toString();
			    if (account.displayName.isEmpty())
				    account.displayName = account.alias;
			    account.metadata = user;
			    if (account.alias.isEmpty()) {
				    callback(false, {}, trovoError(response));
				    return;
			    }
			    callback(true, {account}, {});
		    });
	}

	void validateToken(const AuthContext &auth, TokenCheckCallback callback) override
	{
		if (auth.token.accessToken.isEmpty()) {
			callback(false, {}, QObject::tr("connect a Trovo account first"));
			return;
		}
		const QString url = QStringLiteral("%1validate").arg(kApiBase);
		auth.http->send(
		    QtHttpClient::get(url, trovoHeaders(auth)),
		    [callback](const HttpResponse &response) {
			    const QJsonObject object =
			        QJsonDocument::fromJson(response.body).object();
			    const QString name = object.value(QStringLiteral("nick_name")).toString();
			    if (!response.ok || response.status >= 400 || name.isEmpty()) {
				    callback(false, {}, trovoError(response));
				    return;
			    }
			    callback(true, name, {});
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
			endpoint.error = QObject::tr("connect a Trovo account first");
			callback(endpoint);
			return;
		}

		const QString url = QStringLiteral("%1channel").arg(kApiBase);
		auth.http->send(
		    QtHttpClient::get(url, trovoHeaders(auth)),
		    [this, auth, destination, callback](const HttpResponse &response) {
			    IngestEndpoint endpoint;
			    const QJsonObject channel =
			        QJsonDocument::fromJson(response.body).object();
			    if (!response.ok || response.status >= 400) {
				    endpoint.error = trovoError(response);
				    callback(endpoint);
				    return;
			    }
			    const QString rawKey = channel.value(QStringLiteral("stream_key")).toString();
			    if (rawKey.isEmpty()) {
				    endpoint.error = QObject::tr(
				        "Trovo returned no stream key - confirm the channel_details_self "
				        "scope was granted");
				    callback(endpoint);
				    return;
			    }
			    endpoint.remoteId =
			        channel.value(QStringLiteral("channel_id")).toString();
			    QString server = destination.server.trimmed();
			    QString key = rawKey;
			    if (server.isEmpty()) {
				    server = QString::fromLatin1(kDefaultIngest);
				    /* Rewrites the key when it carries the "live/" app prefix. */
				    key = normalizeTrovoKey(&rawKey, &server);
			    }
			    endpoint.server = server;
			    endpoint.key = key;
			    if (!destination.server.trimmed().isEmpty()) {
				    callback(endpoint);
				    return;
			    }
			    const QString title =
			        destination.options.value(QStringLiteral("title")).toString();
			    if (title.isEmpty() ||
			        title == channel.value(QStringLiteral("live_title")).toString()) {
				    callback(endpoint);
				    return;
			    }
			    const qint64 channelId = channel.value(QStringLiteral("channel_id"))
			                                 .toVariant().toLongLong();
			    updateTitle(auth, channelId, title,
			                [endpoint, callback](const QString &) mutable {
				                /* Metadata failures do not block streaming. */
				                callback(endpoint);
			                });
		    });
	}

private:
	void updateTitle(const AuthContext &auth, qint64 channelId, const QString &title,
	                 std::function<void(const QString &)> callback)
	{
		QJsonObject body;
		body.insert(QStringLiteral("channel_id"), static_cast<double>(channelId));
		body.insert(QStringLiteral("live_title"), title);

		HttpRequest request;
		request.method = QStringLiteral("POST");
		request.url = QStringLiteral("%1channels/update").arg(kApiBase);
		request.headers = trovoHeaders(auth);
		request.headers.insert(QStringLiteral("Content-Type"),
		                       QStringLiteral("application/json"));
		request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
		auth.http->send(request, [callback](const HttpResponse &response) {
			if (!response.ok || response.status >= 400)
				callback(trovoError(response));
			else
				callback({});
		});
	}
};

} // namespace

AdapterPtr createTrovoAdapter()
{
	return std::make_unique<TrovoAdapter>();
}

} // namespace ums
