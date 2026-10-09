/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platforms/factories.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QObject>

namespace ums
{

namespace
{

constexpr const char *kHelix = "https://api.twitch.tv/helix/";

QString twitchError(const HttpResponse &response, const QByteArray &body)
{
	if (response.status == 401)
		return QObject::tr(
		    "Twitch authorization expired or invalid - reconnect the account");
	if (response.status == 429)
		return QObject::tr("Twitch API rate limit exceeded - try again shortly");

	const QJsonObject error =
	    QJsonDocument::fromJson(body).object().value(QStringLiteral("error")).toObject();
	QString message = error.value(QStringLiteral("message")).toString();
	if (message.isEmpty())
		message = QJsonDocument::fromJson(body)
			      .object()
			      .value(QStringLiteral("message"))
			      .toString();
	if (message.isEmpty() && !response.transportError.isEmpty())
		message = response.transportError;
	if (message.isEmpty())
		message = QObject::tr("Twitch API request failed (HTTP %1)").arg(response.status);
	return message;
}

QHash<QString, QString> helixHeaders(const AuthContext &auth)
{
	/* Helix requires the token AND the client id that issued it. */
	return bearerHeaders(auth.token.accessToken, auth.client.clientId);
}

class TwitchAdapter final : public IPlatformAdapter
{
      public:
	PlatformInfo info() const override
	{
		PlatformInfo info;
		info.id = QStringLiteral("twitch");
		info.displayName = QObject::tr("Twitch");
		info.description =
		    QObject::tr("Device-code sign-in retrieves your channel stream key and can "
				"update the channel title. The ingest URL is Twitch's global "
				"ingest server.");
		info.docsUrl = QStringLiteral("https://dev.twitch.tv/docs/api/");
		info.portalUrl = QStringLiteral("https://dashboard.twitch.tv/");
		info.defaultIngestUrl = QStringLiteral("rtmp://live.twitch.tv/app");
		info.ingestHint =
		    QObject::tr("Manual mode: Home -> Settings -> Stream on the Twitch dashboard "
				"shows the URL and key. API mode reads the key for you.");
		info.maxBitrateKbps = 6000;
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
		info.implemented = {Feature::OAuth, Feature::StreamKeyRetrieval,
				    Feature::ManualIngest, Feature::MetadataManagement,
				    Feature::RtmpIngest};
		return info;
	}

	OAuthSpec oauthSpec() const override
	{
		OAuthSpec spec;
		spec.mode = OAuthMode::DeviceCode;
		spec.authUrl = QStringLiteral("https://id.twitch.tv/oauth2/authorize");
		spec.tokenUrl = QStringLiteral("https://id.twitch.tv/oauth2/token");
		spec.deviceAuthUrl = QStringLiteral("https://id.twitch.tv/oauth2/device");
		spec.scopes = {
		    QStringLiteral("channel:read:stream_key"),
		    QStringLiteral("channel:manage:broadcast"),
		};
		spec.deviceScopeParam = QStringLiteral("scopes");
		return spec;
	}

	void listAccounts(const AuthContext &auth, AccountsCallback callback) override
	{
		if (!auth.http || auth.token.accessToken.isEmpty()) {
			callback(false, {}, QObject::tr("connect a Twitch account first"));
			return;
		}
		const QString url = QStringLiteral("%1users").arg(kHelix);
		auth.http->send(
		    QtHttpClient::get(url, helixHeaders(auth)),
		    [callback](const HttpResponse &response) {
			    const QJsonArray data = QJsonDocument::fromJson(response.body)
							.object()
							.value(QStringLiteral("data"))
							.toArray();
			    if (!response.ok || response.status >= 400 || data.isEmpty()) {
				    callback(false, {}, twitchError(response, response.body));
				    return;
			    }
			    QVector<AccountInfo> accounts;
			    for (const QJsonValue &value : data) {
				    const QJsonObject user = value.toObject();
				    AccountInfo account;
				    account.alias = user.value(QStringLiteral("id")).toString();
				    account.displayName =
					user.value(QStringLiteral("display_name")).toString();
				    if (account.displayName.isEmpty())
					    account.displayName = account.alias;
				    account.metadata = user;
				    accounts.append(account);
			    }
			    callback(true, accounts, {});
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
			endpoint.error = QObject::tr("connect a Twitch account first");
			callback(endpoint);
			return;
		}

		listAccounts(auth, [this, auth, destination,
				    callback](bool ok, const QVector<AccountInfo> &accounts,
					      const QString &error) {
			if (!ok || accounts.isEmpty()) {
				IngestEndpoint endpoint;
				endpoint.error = error.isEmpty()
						     ? QObject::tr("connect a Twitch account first")
						     : error;
				callback(endpoint);
				return;
			}
			DestinationConfig selected = destination;
			selected.accountAlias = accounts.first().alias;
			const QString title =
			    selected.options.value(QStringLiteral("title")).toString();
			auto next = [this, auth, selected, callback]() {
				fetchStreamKey(auth, selected, callback);
			};
			if (title.isEmpty()) {
				next();
				return;
			}
			updateTitle(auth, selected.accountAlias, title,
				    [next](const QString &) { next(); });
		});
	}

      private:
	void fetchStreamKey(const AuthContext &auth, const DestinationConfig &destination,
			    IngestCallback callback)
	{
		const QString url = QStringLiteral("%1streams/key?broadcaster_id=%2")
					.arg(kHelix, destination.accountAlias);
		auth.http->send(
		    QtHttpClient::get(url, helixHeaders(auth)),
		    [auth, destination, callback](const HttpResponse &response) {
			    IngestEndpoint endpoint;
			    const QJsonArray data = QJsonDocument::fromJson(response.body)
							.object()
							.value(QStringLiteral("data"))
							.toArray();
			    if (!response.ok || response.status >= 400 || data.isEmpty()) {
				    endpoint.error = twitchError(response, response.body);
				    callback(endpoint);
				    return;
			    }
			    endpoint.key = data.at(0)
					       .toObject()
					       .value(QStringLiteral("stream_key"))
					       .toString();
			    endpoint.server = destination.server.trimmed();
			    if (endpoint.server.isEmpty())
				    endpoint.server = QStringLiteral("rtmp://live.twitch.tv/app");
			    if (endpoint.key.isEmpty())
				    endpoint.error = QObject::tr(
					"Twitch returned an empty stream key - check the "
					"channel:read:stream_key authorization");
			    callback(endpoint);
		    });
	}

	void updateTitle(const AuthContext &auth, const QString &broadcasterId,
			 const QString &title, std::function<void(const QString &)> callback)
	{
		QJsonObject body;
		body.insert(QStringLiteral("title"), title);
		HttpRequest request;
		request.method = QStringLiteral("PATCH");
		request.url =
		    QStringLiteral("%1channels?broadcaster_id=%2").arg(kHelix, broadcasterId);
		request.headers = helixHeaders(auth);
		request.headers.insert(QStringLiteral("Content-Type"),
				       QStringLiteral("application/json"));
		request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
		auth.http->send(request, [callback](const HttpResponse &response) {
			if (!response.ok || response.status >= 400)
				callback(twitchError(response, response.body));
			else
				callback({});
		});
	}
};

} // namespace

AdapterPtr createTwitchAdapter() { return std::make_unique<TwitchAdapter>(); }

} // namespace ums
