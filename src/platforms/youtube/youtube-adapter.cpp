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

constexpr const char *kApiBase = "https://www.googleapis.com/youtube/v3/";

QString apiErrorMessage(const HttpResponse &response)
{
	if (response.status == 401)
		return QObject::tr("YouTube authorization expired - reconnect the account");
	if (response.status == 403)
		return QObject::tr("YouTube denied the request - check API access and app review");
	if (response.status == 429)
		return QObject::tr("YouTube API rate limit exceeded - try again shortly");

	const QJsonObject error =
	    QJsonDocument::fromJson(response.body).object().value(QStringLiteral("error"))
	        .toObject();
	QString message = error.value(QStringLiteral("message")).toString();
	if (message.isEmpty())
		message = response.transportError;
	if (message.isEmpty())
		message = QObject::tr("YouTube API request failed (HTTP %1)").arg(response.status);
	return message;
}

QHash<QString, QString> authHeaders(const AuthContext &auth)
{
	return bearerHeaders(auth.token.accessToken);
}

class YouTubeAdapter final : public IPlatformAdapter {
public:
	PlatformInfo info() const override
	{
		PlatformInfo info;
		info.id = QStringLiteral("youtube");
		info.displayName = QObject::tr("YouTube");
		info.description =
		    QObject::tr("Google account sign-in creates the live broadcast and resolves "
		                "the RTMPS ingest endpoint through the YouTube Live Streaming API.");
		info.docsUrl =
		    QStringLiteral("https://developers.google.com/youtube/v3/live/docs");
		info.portalUrl = QStringLiteral("https://studio.youtube.com/");
		info.defaultIngestUrl = QStringLiteral("rtmp://a.rtmp.youtube.com/live2");
		info.ingestHint =
		    QObject::tr("API mode creates a reusable live stream and broadcast automatically; "
		                "manual mode uses the standard YouTube ingest server and key from "
		                "YouTube Studio.");
		info.maxBitrateKbps = 51000;
		info.caps.oauth = FeatureStatus::Full;
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
		spec.authUrl = QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth");
		spec.tokenUrl = QStringLiteral("https://oauth2.googleapis.com/token");
		spec.revokeUrl = QStringLiteral("https://oauth2.googleapis.com/revoke");
		spec.scopes = {
			QStringLiteral("https://www.googleapis.com/auth/youtube.force-ssl"),
			QStringLiteral("https://www.googleapis.com/auth/youtube.readonly"),
		};
		spec.requiresClientSecret = true; /* Google issues one even to desktop apps */
		spec.extraAuthParams = {
			{QStringLiteral("access_type"), QStringLiteral("offline")},
			{QStringLiteral("prompt"), QStringLiteral("consent")},
		};
		return spec;
	}

	QString defaultClientId() const override { return {}; }

	void listAccounts(const AuthContext &auth, AccountsCallback callback) override
	{
		fetchChannel(
		    auth, [callback](bool ok, const QJsonObject &channel, const QString &error) {
			    if (!ok) {
				    callback(false, {}, error);
				    return;
			    }
			    AccountInfo account;
			    account.alias = channel.value(QStringLiteral("id")).toString();
			    account.displayName =
			        channel.value(QStringLiteral("snippet")).toObject()
			            .value(QStringLiteral("title")).toString();
			    if (account.displayName.isEmpty())
				    account.displayName = account.alias;
			    account.metadata = channel;
			    callback(true, {account}, {});
		    });
	}

	void validateToken(const AuthContext &auth, TokenCheckCallback callback) override
	{
		if (auth.token.accessToken.isEmpty()) {
			callback(false, {}, QObject::tr("no YouTube account connected"));
			return;
		}
		fetchChannel(
		    auth, [callback](bool ok, const QJsonObject &channel, const QString &error) {
			    if (!ok) {
				    callback(false, {}, error);
				    return;
			    }
			    callback(true,
			             channel.value(QStringLiteral("snippet")).toObject()
			                 .value(QStringLiteral("title")).toString(),
			             {});
		    });
	}

	void prepareIngest(const AuthContext &auth, const DestinationConfig &destination,
	                   IngestCallback callback) override
	{
		if (destination.authMode == AuthMode::Manual) {
			callback(resolveManualIngest(info(), auth, destination));
			return;
		}
		if (auth.token.accessToken.isEmpty()) {
			IngestEndpoint endpoint;
			endpoint.error = QObject::tr("connect a YouTube account first");
			callback(endpoint);
			return;
		}

		/* 1. reusable liveStream (server + key source) */
		ensureStream(auth, [this, auth, destination, callback](const QJsonObject &stream,
		                                                       const QString &error) {
			if (stream.isEmpty()) {
				IngestEndpoint endpoint;
				endpoint.error = error;
				callback(endpoint);
				return;
			}
			/* 2. broadcast (reuse stored remoteId or create) */
			ensureBroadcast(
			    auth, destination,
			    [this, auth, stream, destination, callback](const QString &broadcastId,
			                                                const QString &error) {
				    if (broadcastId.isEmpty()) {
					    IngestEndpoint endpoint;
					    endpoint.error = error;
					    callback(endpoint);
					    return;
				    }
				    /* 3. bind broadcast to stream */
				    bind(auth, broadcastId, stream.value(QStringLiteral("id")).toString(),
				         [stream, destination, broadcastId,
				          callback](const QString &error) {
					         if (!error.isEmpty()) {
						         IngestEndpoint endpoint;
						         endpoint.error = error;
						         callback(endpoint);
						         return;
					         }
					         const QJsonObject ingestionInfo =
					             stream.value(QStringLiteral("cdn")).toObject()
					                 .value(QStringLiteral("ingestionInfo")).toObject();
					         IngestEndpoint endpoint;
					         endpoint.server =
					             ingestionInfo.value(QStringLiteral("rtmpsIngestionAddress"))
					                 .toString();
					         if (endpoint.server.isEmpty())
						         endpoint.server = ingestionInfo
						                                .value(QStringLiteral("ingestionAddress"))
						                                .toString();
					         endpoint.key =
					             ingestionInfo.value(QStringLiteral("streamName")).toString();
					         endpoint.remoteId = broadcastId;
					         if (endpoint.server.isEmpty() || endpoint.key.isEmpty()) {
						         endpoint.error =
						             QObject::tr("YouTube returned an incomplete ingest endpoint");
						         endpoint.server.clear();
					         }
					         callback(endpoint);
				         });
			    });
		});
	}

private:
	void fetchChannel(const AuthContext &auth,
	                  std::function<void(bool, const QJsonObject &, const QString &)> callback)
	{
		if (!auth.http) {
			callback(false, {}, QStringLiteral("http client unavailable"));
			return;
		}
		const QString url =
		    QStringLiteral("%1channels?part=id,snippet&mine=true&maxResults=1").arg(kApiBase);
		auth.http->send(QtHttpClient::get(url, authHeaders(auth)),
		                [callback](const HttpResponse &response) {
			                const QJsonObject root =
			                    QJsonDocument::fromJson(response.body).object();
			                const QJsonArray items =
			                    root.value(QStringLiteral("items")).toArray();
			                if (!response.ok || response.status >= 400 || items.isEmpty()) {
				                QString error = apiErrorMessage(response);
				                if (items.isEmpty() && response.status < 400)
					                error = QObject::tr("the Google account has no YouTube channel");
				                callback(false, {}, error);
				                return;
			                }
			                callback(true, items.at(0).toObject(), {});
		                });
	}

	void ensureStream(const AuthContext &auth,
	                  std::function<void(const QJsonObject &, const QString &)> callback)
	{
		const QString url = QStringLiteral("%1liveStreams?part=id,cdn,snippet,status"
		                                   "&mine=true&maxResults=5")
		                        .arg(kApiBase);
		auth.http->send(QtHttpClient::get(url, authHeaders(auth)),
		                [this, auth, callback](const HttpResponse &response) {
			                const QJsonObject root =
			                    QJsonDocument::fromJson(response.body).object();
			                if (!response.ok || response.status >= 400) {
				                callback({}, apiErrorMessage(response));
				                return;
			                }
			                const QJsonArray items =
			                    root.value(QStringLiteral("items")).toArray();
			                for (const QJsonValue &value : items) {
				                const QJsonObject stream = value.toObject();
				                const QString state = stream.value(QStringLiteral("status"))
				                                          .toObject()
				                                          .value(QStringLiteral("streamStatus"))
				                                          .toString();
				                if (state == QLatin1String("ready") ||
				                    state == QLatin1String("active") ||
				                    state == QLatin1String("inactive")) {
					                callback(stream, {});
					                return;
				                }
			                }
			                createStream(auth, callback);
		                });
	}

	void createStream(const AuthContext &auth,
	                  std::function<void(const QJsonObject &, const QString &)> callback)
	{
		QJsonObject body;
		body.insert(QStringLiteral("snippet"),
		            QJsonObject{{QStringLiteral("title"),
		                         QObject::tr("OBS multi-stream feed")}});
		body.insert(QStringLiteral("cdn"),
		            QJsonObject{
		                {QStringLiteral("ingestionType"), QStringLiteral("rtmp")},
		                {QStringLiteral("resolution"), QStringLiteral("variable")},
		                {QStringLiteral("frameRate"), QStringLiteral("variable")},
		            });
		body.insert(QStringLiteral("contentDetails"),
		            QJsonObject{{QStringLiteral("isReusable"), true}});

		const QString url = QStringLiteral("%1liveStreams?part=id,cdn,snippet,status").arg(kApiBase);
		auth.http->send(
		    QtHttpClient::post(url, QJsonDocument(body).toJson(QJsonDocument::Compact),
		                       authHeaders(auth)),
		    [callback](const HttpResponse &response) {
			    const QJsonObject stream =
			        QJsonDocument::fromJson(response.body).object();
			    if (!response.ok || response.status >= 400) {
				    callback({}, apiErrorMessage(response));
				    return;
			    }
			    callback(stream, {});
		    });
	}

	void ensureBroadcast(const AuthContext &auth, const DestinationConfig &destination,
	                     std::function<void(const QString &, const QString &)> callback)
	{
		if (!destination.remoteId.isEmpty()) {
			const QString url = QStringLiteral("%1liveBroadcasts?part=id,status&id=%2")
			                        .arg(kApiBase, destination.remoteId);
			auth.http->send(QtHttpClient::get(url, authHeaders(auth)),
			                                [this, auth, destination, callback](const HttpResponse &response) {
				                const QJsonArray items =
				                    QJsonDocument::fromJson(response.body).object()
				                        .value(QStringLiteral("items")).toArray();
				                if (response.ok && response.status < 400 &&
				                    !items.isEmpty()) {
					                const QString lifeCycle =
					                    items.at(0).toObject().value(QStringLiteral("status"))
					                        .toObject()
					                        .value(QStringLiteral("lifeCycleStatus"))
					                        .toString();
					                if (lifeCycle != QLatin1String("complete") &&
					                    lifeCycle != QLatin1String("revoked")) {
						                callback(destination.remoteId, {});
						                return;
					                }
				                }
				                createBroadcast(auth, destination, callback);
			                });
			return;
		}
		createBroadcast(auth, destination, callback);
	}

	void createBroadcast(const AuthContext &auth, const DestinationConfig &destination,
	                     std::function<void(const QString &, const QString &)> callback)
	{
		QString title = destination.options.value(QStringLiteral("title")).toString();
		if (title.isEmpty())
			title = QObject::tr("OBS multi-stream");
		QString privacy =
		    destination.options.value(QStringLiteral("privacy")).toString();
		if (privacy.isEmpty())
			privacy = QStringLiteral("unlisted");
		QString description =
		    destination.options.value(QStringLiteral("description")).toString();

		QJsonObject body;
		body.insert(QStringLiteral("snippet"),
		            QJsonObject{
		                {QStringLiteral("title"), title},
		                {QStringLiteral("description"), description},
		            });
		body.insert(QStringLiteral("status"),
		            QJsonObject{{QStringLiteral("privacyStatus"), privacy}});
		body.insert(QStringLiteral("contentDetails"),
		            QJsonObject{
		                {QStringLiteral("enableAutoStart"), true},
		                {QStringLiteral("enableAutoStop"), true},
		            });

		const QString url =
		    QStringLiteral("%1liveBroadcasts?part=id,snippet,status,contentDetails").arg(kApiBase);
		auth.http->send(
		    QtHttpClient::post(url, QJsonDocument(body).toJson(QJsonDocument::Compact),
		                       authHeaders(auth)),
		    [callback](const HttpResponse &response) {
			    const QJsonObject broadcast =
			        QJsonDocument::fromJson(response.body).object();
			    const QString id = broadcast.value(QStringLiteral("id")).toString();
			    if (!response.ok || response.status >= 400 || id.isEmpty()) {
				    callback({}, apiErrorMessage(response));
				    return;
			    }
			    callback(id, {});
		    });
	}

	void bind(const AuthContext &auth, const QString &broadcastId, const QString &streamId,
	          std::function<void(const QString &)> callback)
	{
		const QString url = QStringLiteral(
		                        "%1liveBroadcasts/bind?id=%2&streamId=%3&part=id")
		                        .arg(kApiBase, broadcastId, streamId);
		auth.http->send(
		    QtHttpClient::post(url, {}, authHeaders(auth)),
		    [callback](const HttpResponse &response) {
			    if (!response.ok || response.status >= 400) {
				    callback(apiErrorMessage(response));
				    return;
			    }
			    callback({});
		    });
	}
};

} // namespace

AdapterPtr createYouTubeAdapter()
{
	return std::make_unique<YouTubeAdapter>();
}

} // namespace ums
