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

constexpr const char *kGraphBase = "https://graph.facebook.com/v26.0/";
constexpr const char *kDefaultIngest = "rtmps://live-api-s.facebook.com:443/rtmp/";

QString fbError(const HttpResponse &response)
{
	if (response.status == 401)
		return QObject::tr("Facebook authorization expired or invalid - reconnect the account");
	if (response.status == 400)
		return QObject::tr("Facebook rejected the request - check permissions and app review");

	const QJsonObject error = QJsonDocument::fromJson(response.body).object()
	                               .value(QStringLiteral("error")).toObject();
	QString message = error.value(QStringLiteral("message")).toString();
	if (message.isEmpty() && !response.transportError.isEmpty())
		message = response.transportError;
	if (message.isEmpty())
		message = QObject::tr("Facebook API request failed (HTTP %1)").arg(response.status);
	return message;
}

/* Facebook hands out one URL containing both the ingest server and the
 * per-broadcast key: rtmps://host:443/rtmp/<key>. Split it the way OBS
 * wants it: server keeps everything through "/rtmp/", the rest is key. */
bool splitStreamUrl(const QString &streamUrl, QString *server, QString *key)
{
	const QString lower = streamUrl.toLower();
	const int marker = lower.indexOf(QLatin1String("/rtmp/"));
	if (marker < 0)
		return false;
	*server = streamUrl.left(marker + 6); /* keep trailing slash */
	*key = streamUrl.mid(marker + 6);
	return !key->isEmpty();
}

QHash<QString, QString> graphHeaders(const AuthContext &auth)
{
	return bearerHeaders(auth.token.accessToken);
}

class FacebookAdapter final : public IPlatformAdapter {
public:
	PlatformInfo info() const override
	{
		PlatformInfo info;
		info.id = QStringLiteral("facebook");
		info.displayName = QObject::tr("Facebook");
		info.description =
		    QObject::tr("Facebook Live through the Graph Video API: creates the live video "
		                "on your profile, page or group and returns a per-broadcast RTMPS "
		                "endpoint. Requires a reviewed Facebook app.");
		info.docsUrl =
		    QStringLiteral("https://developers.facebook.com/docs/video-api/");
		info.portalUrl = QStringLiteral("https://www.facebook.com/live/producer");
		info.defaultIngestUrl = QString::fromLatin1(kDefaultIngest);
		info.ingestHint =
		    QObject::tr("Manual mode: Facebook Live Producer shows the Server and Stream "
		                "key while you configure the live video.");
		info.maxBitrateKbps = 4000;
		info.sessionBasedKey = true;
		info.caps.oauth = FeatureStatus::Partial;
		info.caps.broadcastCreate = FeatureStatus::Full;
		info.caps.broadcastSchedule = FeatureStatus::NotImplemented;
		info.caps.streamKeyRetrieval = FeatureStatus::Full;
		info.caps.manualIngest = FeatureStatus::Full;
		info.caps.metadataManagement = FeatureStatus::Partial;
		info.caps.liveStatusMonitoring = FeatureStatus::NotImplemented;
		info.caps.apiStartStop = FeatureStatus::Partial;
		info.caps.rtmpIngest = FeatureStatus::Full;
		info.caps.chat = FeatureStatus::NotImplemented;
		info.caps.analytics = FeatureStatus::NotImplemented;
		info.implemented = {Feature::OAuth,         Feature::BroadcastCreate,
		                    Feature::StreamKeyRetrieval, Feature::ManualIngest,
		                    Feature::MetadataManagement, Feature::ApiStartStop,
		                    Feature::RtmpIngest};
		return info;
	}

	OAuthSpec oauthSpec() const override
	{
		OAuthSpec spec;
		spec.authUrl = QStringLiteral("https://www.facebook.com/v26.0/dialog/oauth");
		spec.tokenUrl = QStringLiteral("https://graph.facebook.com/v26.0/oauth/access_token");
		spec.revokeUrl = QStringLiteral("https://graph.facebook.com/v26.0/me/permissions");
		spec.scopes = {
			QStringLiteral("publish_video"),
			QStringLiteral("pages_show_list"),
			QStringLiteral("pages_read_engagement"),
			QStringLiteral("pages_manage_posts"),
		};
		spec.requiresClientSecret = true;
		/* Facebook may reject loopback redirects for some app types; the UI
		 * offers a manual access-token paste as fallback. */
		return spec;
	}

	QStringList validate(const DestinationConfig &destination) const override
	{
		QStringList errors = IPlatformAdapter::validate(destination);
		if (destination.authMode == AuthMode::Manual && destination.server.trimmed().isEmpty())
			errors << QObject::tr("paste the Server URL from Facebook Live Producer");
		return errors;
	}

	void listAccounts(const AuthContext &auth, AccountsCallback callback) override
	{
		if (!auth.http || auth.token.accessToken.isEmpty()) {
			callback(false, {}, QObject::tr("connect a Facebook account first"));
			return;
		}
		const QString meUrl = QStringLiteral("%1me?fields=id,name").arg(kGraphBase);
		auth.http->send(
		    QtHttpClient::get(meUrl, graphHeaders(auth)),
		    [this, auth, callback](const HttpResponse &meResponse) {
			    const QJsonObject me = QJsonDocument::fromJson(meResponse.body).object();
			    if (!meResponse.ok || meResponse.status >= 400 ||
			        me.value(QStringLiteral("id")).toString().isEmpty()) {
				    callback(false, {}, fbError(meResponse));
				    return;
			    }
			    QVector<AccountInfo> accounts;
			    AccountInfo personal;
			    personal.alias = me.value(QStringLiteral("id")).toString();
			    personal.displayName = me.value(QStringLiteral("name")).toString();
			    personal.metadata = QJsonObject{{QStringLiteral("type"),
			                                     QStringLiteral("user")}};
			    accounts.append(personal);

			    /* Managed pages (optional scope). Page tokens are secrets and
			     * are never persisted - refetched on demand. */
			    const QString pagesUrl =
			        QStringLiteral("%1me/accounts?fields=id,name").arg(kGraphBase);
			    auth.http->send(
			        QtHttpClient::get(pagesUrl, graphHeaders(auth)),
			        [callback, accounts](const HttpResponse &pagesResponse) mutable {
				        const QJsonArray data =
				            QJsonDocument::fromJson(pagesResponse.body).object()
				                .value(QStringLiteral("data")).toArray();
				        if (pagesResponse.ok && pagesResponse.status < 400) {
					        for (const QJsonValue &value : data) {
						        const QJsonObject page = value.toObject();
						        AccountInfo account;
						        account.alias =
						            page.value(QStringLiteral("id")).toString();
						        account.displayName =
						            page.value(QStringLiteral("name")).toString();
						        account.metadata =
						            QJsonObject{{QStringLiteral("type"),
						                         QStringLiteral("page")}};
						        if (!account.alias.isEmpty())
							        accounts.append(account);
					        }
				        }
				        callback(true, accounts, {});
			        });
		    });
	}

	void validateToken(const AuthContext &auth, TokenCheckCallback callback) override
	{
		if (auth.token.accessToken.isEmpty()) {
			callback(false, {}, QObject::tr("connect a Facebook account first"));
			return;
		}
		const QString url = QStringLiteral("%1me?fields=id,name").arg(kGraphBase);
		auth.http->send(QtHttpClient::get(url, graphHeaders(auth)),
		                [callback](const HttpResponse &response) {
			                const QJsonObject me =
			                    QJsonDocument::fromJson(response.body).object();
			                const QString name = me.value(QStringLiteral("name")).toString();
			                if (!response.ok || response.status >= 400 || name.isEmpty()) {
				                callback(false, {}, fbError(response));
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
			endpoint.error = QObject::tr("connect a Facebook account first");
			callback(endpoint);
			return;
		}
		createLiveVideo(auth, destination, callback);
	}

private:
	void createLiveVideo(const AuthContext &auth, const DestinationConfig &destination,
	                     IngestCallback callback)
	{
		auto post = [auth, destination, callback](const QString &targetId,
		                                          const QString &token) {
			QString title = destination.options.value(QStringLiteral("title")).toString();
			if (title.isEmpty())
				title = QObject::tr("OBS multi-stream");
			QString description =
			    destination.options.value(QStringLiteral("description")).toString();

			QJsonObject body;
			body.insert(QStringLiteral("title"), title);
			body.insert(QStringLiteral("description"), description);

			QHash<QString, QString> headers;
			headers.insert(QStringLiteral("Authorization"),
			               QStringLiteral("Bearer %1").arg(token));
			headers.insert(QStringLiteral("Content-Type"),
			               QStringLiteral("application/json"));
			const QString url = QStringLiteral("%1%2/live_videos?fields=id,stream_url,status")
			                        .arg(kGraphBase, targetId);
			HttpRequest request;
			request.method = QStringLiteral("POST");
			request.url = url;
			request.headers = headers;
			request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);

			auth.http->send(request, [callback](const HttpResponse &response) {
				IngestEndpoint endpoint;
				const QJsonObject video =
				    QJsonDocument::fromJson(response.body).object();
				if (!response.ok || response.status >= 400) {
					endpoint.error = fbError(response);
					callback(endpoint);
					return;
				}
				const QString streamUrl =
				    video.value(QStringLiteral("stream_url")).toString();
				if (!splitStreamUrl(streamUrl, &endpoint.server, &endpoint.key)) {
					endpoint.error = QObject::tr(
					    "Facebook returned an unexpected stream URL format");
					callback(endpoint);
					return;
				}
				endpoint.remoteId = video.value(QStringLiteral("id")).toString();
				callback(endpoint);
			});
		};

		const QString target = destination.accountAlias;
		if (target.isEmpty() || target == QLatin1String("me")) {
			post(QStringLiteral("me"), auth.token.accessToken);
			return;
		}
		/* Page target: fetch a fresh page token (never persisted). */
		const QString pagesUrl =
		    QStringLiteral("%1me/accounts?fields=id,name,access_token").arg(kGraphBase);
		auth.http->send(
		    QtHttpClient::get(pagesUrl, graphHeaders(auth)),
		    [target, post, callback](const HttpResponse &response) {
			    const QJsonArray data =
			        QJsonDocument::fromJson(response.body).object()
			            .value(QStringLiteral("data")).toArray();
			    for (const QJsonValue &value : data) {
				    const QJsonObject page = value.toObject();
				    if (page.value(QStringLiteral("id")).toString() == target) {
					    const QString pageToken =
					        page.value(QStringLiteral("access_token")).toString();
					    if (!pageToken.isEmpty()) {
						    post(target, pageToken);
						    return;
					    }
				    }
			    }
			    IngestEndpoint endpoint;
			    endpoint.error = response.status >= 400
			                         ? fbError(response)
			                         : QObject::tr("managed Facebook page not found or "
			                                        "pages_show_list was not granted");
			    callback(endpoint);
		    });
	}
};

} // namespace

AdapterPtr createFacebookAdapter()
{
	return std::make_unique<FacebookAdapter>();
}

} // namespace ums
