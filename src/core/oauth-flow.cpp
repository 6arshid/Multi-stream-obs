/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/oauth-flow.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace ums {

/* --- TokenSet ---------------------------------------------------------- */

bool TokenSet::isExpired(qint64 nowEpochSec, qint64 skewSec) const
{
	if (accessToken.isEmpty())
		return true;
	if (expiresAtEpochSec <= 0)
		return false;
	return nowEpochSec >= (expiresAtEpochSec - skewSec);
}

QJsonObject TokenSet::toJson() const
{
	QJsonObject object;
	object.insert(QStringLiteral("access_token"), accessToken);
	object.insert(QStringLiteral("refresh_token"), refreshToken);
	object.insert(QStringLiteral("token_type"), tokenType);
	object.insert(QStringLiteral("expires_at"), expiresAtEpochSec);
	object.insert(QStringLiteral("obtained_at"), obtainedAtEpochSec);
	object.insert(QStringLiteral("scope"), scope.join(QLatin1Char(' ')));
	return object;
}

TokenSet TokenSet::fromJson(const QJsonObject &object)
{
	TokenSet token;
	token.accessToken = object.value(QStringLiteral("access_token")).toString();
	token.refreshToken = object.value(QStringLiteral("refresh_token")).toString();
	token.tokenType = object.value(QStringLiteral("token_type")).toString();
	token.expiresAtEpochSec =
	    static_cast<qint64>(object.value(QStringLiteral("expires_at")).toDouble());
	token.obtainedAtEpochSec =
	    static_cast<qint64>(object.value(QStringLiteral("obtained_at")).toDouble());
	const QString scope = object.value(QStringLiteral("scope")).toString();
	if (!scope.isEmpty())
		token.scope = scope.split(QLatin1Char(' '), Qt::SkipEmptyParts);
	return token;
}

/* --- Pure helpers ------------------------------------------------------ */

static QString base64Url(const QByteArray &data)
{
	QString encoded = QString::fromLatin1(data.toBase64(QByteArray::Base64Encoding |
	                                                    QByteArray::OmitTrailingEquals));
	/* Qt's Base64UrlEncoding keeps -/_ - use it directly instead. */
	encoded = QString::fromLatin1(
	    data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
	return encoded;
}

QString generateCodeVerifier()
{
	QByteArray random(48, Qt::Uninitialized);
	QRandomGenerator::system()->generate(random.begin(), random.end());
	return base64Url(random);
}

QString pkceChallengeS256(const QString &codeVerifier)
{
	const QByteArray hash =
	    QCryptographicHash::hash(codeVerifier.toLatin1(), QCryptographicHash::Sha256);
	return base64Url(hash);
}

QString generateState()
{
	QByteArray random(32, Qt::Uninitialized);
	QRandomGenerator::system()->generate(random.begin(), random.end());
	return base64Url(random);
}

TokenSet parseTokenResponse(const QJsonObject &json, qint64 nowEpochSec)
{
	TokenSet token;
	token.accessToken = json.value(QStringLiteral("access_token")).toString();
	token.refreshToken = json.value(QStringLiteral("refresh_token")).toString();
	token.tokenType = json.value(QStringLiteral("token_type")).toString();
	token.obtainedAtEpochSec = nowEpochSec;

	const QJsonValue expiresIn = json.value(QStringLiteral("expires_in"));
	if (expiresIn.isDouble())
		token.expiresAtEpochSec =
		    nowEpochSec + static_cast<qint64>(expiresIn.toDouble());
	else if (expiresIn.isString())
		token.expiresAtEpochSec =
		    nowEpochSec + expiresIn.toString().toLongLong();

	const QJsonValue scope = json.value(QStringLiteral("scope"));
	if (scope.isString()) {
		token.scope = scope.toString().split(QRegularExpression(QStringLiteral("\\s+")),
		                                     Qt::SkipEmptyParts);
	} else if (scope.isArray()) {
		const QJsonArray array = scope.toArray();
		for (const QJsonValue &value : array)
			token.scope.append(value.toString());
	}
	return token;
}

HttpRequest buildRefreshRequest(const OAuthSpec &spec, const OAuthClientConfig &client,
                                const TokenSet &current)
{
	QHash<QString, QString> fields;
	fields.insert(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
	fields.insert(QStringLiteral("refresh_token"), current.refreshToken);
	/* When a dedicated client-id header exists the id stays out of the body. */
	if (spec.clientIdHeader.isEmpty())
		fields.insert(QStringLiteral("client_id"), client.clientId);
	if (!client.clientSecret.isEmpty())
		fields.insert(QStringLiteral("client_secret"), client.clientSecret);
	for (auto it = spec.extraTokenParams.constBegin(); it != spec.extraTokenParams.constEnd(); ++it)
		fields.insert(it.key(), it.value());

	HttpRequest request;
	request.method = QStringLiteral("POST");
	request.url = spec.refreshEndpoint();
	if (!spec.clientIdHeader.isEmpty() && !client.clientId.isEmpty())
		request.headers.insert(spec.clientIdHeader, client.clientId);
	if (spec.jsonTokenBody) {
		request.headers.insert(QStringLiteral("Content-Type"), QStringLiteral("application/json"));
		QJsonObject object;
		for (auto it = fields.constBegin(); it != fields.constEnd(); ++it)
			object.insert(it.key(), it.value());
		request.body = QJsonDocument(object).toJson(QJsonDocument::Compact);
	} else {
		request.headers.insert(QStringLiteral("Content-Type"),
		                       QStringLiteral("application/x-www-form-urlencoded"));
		request.body = formEncode(fields);
	}
	return request;
}

void refreshAccessToken(IHttpClient &http, const OAuthSpec &spec,
                        const OAuthClientConfig &client, TokenSet &current,
                        std::function<void(const RefreshOutcome &)> callback)
{
	const HttpRequest request = buildRefreshRequest(spec, client, current);
	http.send(request, [spec, &current, callback](const HttpResponse &response) {
		RefreshOutcome outcome;
		const QJsonObject json = QJsonDocument::fromJson(response.body).object();
		if (!response.ok) {
			outcome.error = response.transportError;
			if (outcome.error.isEmpty())
				outcome.error = QStringLiteral("token refresh request failed");
			return callback(outcome);
		}
		if (response.status >= 400) {
			outcome.error = json.value(QStringLiteral("error_description")).toString();
			if (outcome.error.isEmpty()) outcome.error = QStringLiteral("HTTP %1").arg(response.status);
			outcome.unauthorized = response.status == 401 || response.status == 403;
			return callback(outcome);
		}
		/* Provider errors arrive with 200 or 4xx - both carry JSON. */
		if (!json.isEmpty() || response.status >= 400)
			outcome = applyTokenResponse(spec, json, current,
			                             QDateTime::currentSecsSinceEpoch());
		if (!outcome.ok && outcome.error.isEmpty())
			outcome.error = QStringLiteral("HTTP %1").arg(response.status);
		callback(outcome);
	});
}

RefreshOutcome applyTokenResponse(const OAuthSpec &spec, const QJsonObject &json,
                                  TokenSet &current, qint64 nowEpochSec)
{
	RefreshOutcome outcome;

	const QString error = json.value(QStringLiteral("error")).toString();
	if (!error.isEmpty()) {
		outcome.error = json.value(QStringLiteral("error_description")).toString();
		if (outcome.error.isEmpty())
			outcome.error = error;
		outcome.unauthorized =
		    error == QLatin1String("invalid_grant") ||
		    error == QLatin1String("invalid_token") ||
		    error == QLatin1String("unauthorized") ||
		    error == QLatin1String("invalid_client");
		return outcome;
	}

	const TokenSet parsed = parseTokenResponse(json, nowEpochSec);
	if (parsed.accessToken.isEmpty()) {
		outcome.error = QStringLiteral("token response did not contain an access token");
		return outcome;
	}

	current.accessToken = parsed.accessToken;
	if (!parsed.refreshToken.isEmpty()) {
		/* Providers such as Kick rotate refresh tokens on every use. */
		current.refreshToken = parsed.refreshToken;
	}
	if (parsed.expiresAtEpochSec > 0)
		current.expiresAtEpochSec = parsed.expiresAtEpochSec;
	if (!parsed.scope.isEmpty())
		current.scope = parsed.scope;
	current.obtainedAtEpochSec = nowEpochSec;
	if (current.tokenType.isEmpty())
		current.tokenType = parsed.tokenType;
	Q_UNUSED(spec);

	outcome.ok = true;
	outcome.token = current;
	return outcome;
}

/* --- OAuthFlow --------------------------------------------------------- */

OAuthFlow::OAuthFlow(IHttpClient &http, QObject *parent)
    : QObject(parent), http_(http)
{
}

OAuthFlow::~OAuthFlow()
{
	cancel();
}

bool OAuthFlow::isActive() const
{
	return !finished_ && (server_ != nullptr || deviceTimer_ != nullptr || manualPending_);
}

void OAuthFlow::cancel()
{
	finished_ = true;
	manualPending_ = false;
	if (server_) {
		server_->close();
		server_->deleteLater();
		server_ = nullptr;
	}
	if (deviceTimer_) {
		deviceTimer_->stop();
		deviceTimer_->deleteLater();
		deviceTimer_ = nullptr;
	}
	if (pendingSocket_) {
		pendingSocket_->abort();
		pendingSocket_->deleteLater();
		pendingSocket_ = nullptr;
	}
}

bool OAuthFlow::startAuthorizationCode(const OAuthSpec &spec, const OAuthClientConfig &client,
                                       const QString &platformId,
                                       const QString &fixedRedirectUri)
{
	if (isActive() || !spec.isValid())
		return false;
	if (spec.mode == OAuthMode::DeviceCode)
		return startDeviceCode(spec, client, platformId);
	if (client.clientId.isEmpty()) {
		emit failed(platformId, QStringLiteral("missing_client_id"),
		            tr("No OAuth client ID is configured for this platform."));
		return false;
	}
	if (spec.requiresClientSecret && client.clientSecret.isEmpty()) {
		emit failed(platformId, QStringLiteral("missing_client_secret"),
		            tr("This platform requires an OAuth client secret. Add your application "
		               "credentials in the platform settings."));
		return false;
	}

	spec_ = spec;
	client_ = client;
	platformId_ = platformId;
	expectedState_ = generateState();
	codeVerifier_ = spec.pkce ? generateCodeVerifier() : QString();
	finished_ = false;

	if (fixedRedirectUri.isEmpty()) {
		/* Loopback redirect (native-app pattern): any port is allowed by
		 * providers that support it (e.g. Google). */
		server_ = new QTcpServer(this);
		if (!server_->listen(QHostAddress::LocalHost, 0)) {
			const QString message = server_->errorString();
			server_->deleteLater();
			server_ = nullptr;
			emit failed(platformId, QStringLiteral("loopback_bind_failed"), message);
			return false;
		}
		connect(server_, &QTcpServer::newConnection, this, &OAuthFlow::handleConnection);
		redirectUri_ = QStringLiteral("http://127.0.0.1:%1/").arg(server_->serverPort());
		manualPending_ = false;
	} else {
		/* Registered redirect: the user completes sign-in in the browser and
		 * pastes the resulting URL back into the plugin. */
		redirectUri_ = fixedRedirectUri;
		manualPending_ = true;
	}

	QUrl url(spec.authUrl);
	QUrlQuery query(url.query());
	query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
	query.addQueryItem(QStringLiteral("client_id"), client.clientId);
	query.addQueryItem(QStringLiteral("redirect_uri"), redirectUri_);
	query.addQueryItem(QStringLiteral("state"), expectedState_);
	if (!spec.scopes.isEmpty())
		query.addQueryItem(QStringLiteral("scope"), spec.scopes.join(QLatin1Char(' ')));
	if (spec.pkce) {
		query.addQueryItem(QStringLiteral("code_challenge"), pkceChallengeS256(codeVerifier_));
		query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
	}
	for (auto it = spec.extraAuthParams.constBegin(); it != spec.extraAuthParams.constEnd(); ++it)
		query.addQueryItem(it.key(), it.value());
	url.setQuery(query);

	emit openBrowser(url.toString());
	return true;
}

void OAuthFlow::handleManualRedirect(const QString &rawUrl)
{
	if (finished_ || redirectUri_.isEmpty() || server_)
		return;
	const QUrl url(rawUrl);
	QUrlQuery query(url.query());
	if (query.queryItems().isEmpty() && !url.fragment().isEmpty())
		query = QUrlQuery(url.fragment());
	processCallback(query.queryItemValue(QStringLiteral("error")),
	               query.queryItemValue(QStringLiteral("error_description")),
	               query.queryItemValue(QStringLiteral("state")),
	               query.queryItemValue(QStringLiteral("code")));
}

bool OAuthFlow::startDeviceCode(const OAuthSpec &spec, const OAuthClientConfig &client,
                                const QString &platformId)
{
	if (isActive() || spec.deviceAuthUrl.isEmpty() || client.clientId.isEmpty())
		return false;

	spec_ = spec;
	client_ = client;
	platformId_ = platformId;
	finished_ = false;

	QHash<QString, QString> fields;
	fields.insert(QStringLiteral("client_id"), client.clientId);
	if (!spec.scopes.isEmpty())
		fields.insert(spec.deviceScopeParam, spec.scopes.join(QLatin1Char(' ')));

	http_.send(QtHttpClient::post(spec.deviceAuthUrl, formEncode(fields)),
	           [this](const HttpResponse &response) {
		           if (finished_)
			           return;
		           if (!response.ok) {
			           emit failed(platformId_, QStringLiteral("device_start_failed"),
			                       response.transportError);
			           return;
		           }
		           const QJsonObject json =
			           QJsonDocument::fromJson(response.body).object();
		           const QString error = json.value(QStringLiteral("error")).toString();
		           if (response.status >= 400 || !error.isEmpty()) {
			           emit failed(platformId_, error.isEmpty() ? QStringLiteral("device_start_failed")
			                                                    : error,
			                       json.value(QStringLiteral("error_description")).toString());
			           return;
		           }
		           deviceCode_ = json.value(QStringLiteral("device_code")).toString();
		           deviceIntervalSec_ =
			           json.value(QStringLiteral("interval")).toInt(5);
		           if (deviceIntervalSec_ < 1)
			           deviceIntervalSec_ = 5;
		           deviceExpiresInSec_ = json.value(QStringLiteral("expires_in")).toInt(1800);
		           deviceElapsedSec_ = 0;
		           const QString userCode = json.value(QStringLiteral("user_code")).toString();
		           QString uri = json.value(QStringLiteral("verification_uri")).toString();
		           const QString complete =
			           json.value(QStringLiteral("verification_uri_complete")).toString();
		           if (!complete.isEmpty())
			           uri = complete;
		           emit devicePrompt(uri, userCode, deviceExpiresInSec_);
		           beginDevicePoll(deviceIntervalSec_);
	           });
	return true;
}

void OAuthFlow::handleConnection()
{
	if (!server_)
		return;
	QTcpSocket *socket = server_->nextPendingConnection();
	if (pendingSocket_) {
		pendingSocket_->abort();
		pendingSocket_->deleteLater();
	}
	pendingSocket_ = socket;
	connect(socket, &QTcpSocket::readyRead, this, &OAuthFlow::handleRequest);
	connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
	/* Browsers sometimes keep the connection open; nudge with a timer. */
	QTimer::singleShot(10000, this, [this]() {
		if (pendingSocket_ && !finished_) {
			pendingSocket_->abort();
			pendingSocket_ = nullptr;
			emit failed(platformId_, QStringLiteral("callback_timeout"),
			            tr("The browser did not complete the sign-in flow."));
			cancel();
		}
	});
}

void OAuthFlow::handleRequest()
{
	QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
	if (!socket)
		return;
	const QByteArray data = socket->readAll();
	const QByteArray firstLine = data.left(data.indexOf('\n')).trimmed();
	const QList<QByteArray> parts = firstLine.split(' ');
	if (parts.size() < 2)
		return;

	const QString target = QString::fromUtf8(parts[1]);
	const QUrl url(QStringLiteral("http://127.0.0.1") + target);
	const QUrlQuery query(url.query());
	const QString error = query.queryItemValue(QStringLiteral("error"));
	const QString state = query.queryItemValue(QStringLiteral("state"));
	const QString code = query.queryItemValue(QStringLiteral("code"));
	const QString description = query.queryItemValue(QStringLiteral("error_description"));

	if (!error.isEmpty())
		respondToBrowser(200, tr("Authorization cancelled"), description);
	else if (state != expectedState_ || code.isEmpty())
		respondToBrowser(400, tr("Sign-in failed"),
		                 state != expectedState_
		                     ? tr("The sign-in response could not be verified "
		                          "(state mismatch).")
		                     : tr("No authorization code was returned."));
	else
		respondToBrowser(200, tr("Signed in"),
		                 tr("You can close this window and return to OBS Studio."));

	processCallback(error, description, state, code);
}

void OAuthFlow::processCallback(const QString &error, const QString &description,
                                const QString &state, const QString &code)
{
	if (finished_)
		return;
	if (!error.isEmpty()) {
		emit failed(platformId_, error, description);
		cancel();
		return;
	}
	if (state != expectedState_) {
		emit failed(platformId_, QStringLiteral("state_mismatch"),
		            tr("The sign-in response could not be verified."));
		cancel();
		return;
	}
	if (code.isEmpty()) {
		emit failed(platformId_, QStringLiteral("missing_code"),
		            tr("No authorization code was returned."));
		cancel();
		return;
	}
	exchangeCode(code);
}

void OAuthFlow::respondToBrowser(int status, const QString &title, const QString &message)
{
	if (!pendingSocket_)
		return;
	const QString reason = status == 200 ? QStringLiteral("OK") : QStringLiteral("Bad Request");
	const QByteArray body =
	    QStringLiteral(
	        "<!doctype html><html><head><meta charset=\"utf-8\"><title>%1</title></head>"
	        "<body style=\"font-family:sans-serif;padding:3em\"><h1>%1</h1><p>%2</p>"
	        "<p>Universal Multi-Stream for OBS Studio</p></body></html>")
	        .arg(title.toHtmlEscaped(), message.toHtmlEscaped())
	        .toUtf8();
	QByteArray response;
	response += "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason.toUtf8() +
	            "\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " +
	            QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n";
	response += body;
	pendingSocket_->write(response);
	pendingSocket_->flush();
	pendingSocket_->disconnectFromHost();
}

void OAuthFlow::exchangeCode(const QString &code)
{
	QHash<QString, QString> fields;
	fields.insert(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
	fields.insert(QStringLiteral("code"), code);
	if (!spec_.jsonTokenBody || spec_.clientIdHeader.isEmpty())
		fields.insert(QStringLiteral("client_id"), client_.clientId);
	fields.insert(QStringLiteral("redirect_uri"), redirectUri_);
	if (!client_.clientSecret.isEmpty())
		fields.insert(QStringLiteral("client_secret"), client_.clientSecret);
	if (spec_.pkce)
		fields.insert(QStringLiteral("code_verifier"), codeVerifier_);
	for (auto it = spec_.extraTokenParams.constBegin(); it != spec_.extraTokenParams.constEnd(); ++it)
		fields.insert(it.key(), it.value());

	/* The loopback server is no longer needed once we have the code. */
	manualPending_ = false;
	if (server_) {
		server_->close();
		server_->deleteLater();
		server_ = nullptr;
	}

	HttpRequest request;
	request.method = QStringLiteral("POST");
	request.url = spec_.tokenUrl;
	if (!spec_.clientIdHeader.isEmpty() && !client_.clientId.isEmpty())
		request.headers.insert(spec_.clientIdHeader, client_.clientId);
	if (spec_.jsonTokenBody) {
		request.headers.insert(QStringLiteral("Content-Type"),
		                       QStringLiteral("application/json"));
		QJsonObject object;
		for (auto it = fields.constBegin(); it != fields.constEnd(); ++it)
			object.insert(it.key(), it.value());
		request.body = QJsonDocument(object).toJson(QJsonDocument::Compact);
	} else {
		request.headers.insert(QStringLiteral("Content-Type"),
		                       QStringLiteral("application/x-www-form-urlencoded"));
		request.body = formEncode(fields);
	}

	const QString platform = platformId_;
	http_.send(request, [this, platform](const HttpResponse &response) {
		           if (finished_)
			           return;
		           const QJsonObject json =
			           QJsonDocument::fromJson(response.body).object();
		           const qint64 now = QDateTime::currentSecsSinceEpoch();
		           TokenSet token = parseTokenResponse(json, now);
		           if (!response.ok || token.accessToken.isEmpty()) {
			           QString error = json.value(QStringLiteral("error")).toString();
			           if (error.isEmpty())
				           error = response.transportError.isEmpty()
				                       ? QStringLiteral("token_exchange_failed")
				                       : response.transportError;
			           emit failed(platform, error,
			                       json.value(QStringLiteral("error_description")).toString());
			           cancel();
			           return;
		           }
		           finished_ = true;
		           emit authorized(platform, token);
	           });
}

void OAuthFlow::beginDevicePoll(int intervalSec)
{
	if (!deviceTimer_) {
		deviceTimer_ = new QTimer(this);
		deviceTimer_->setSingleShot(true);
		connect(deviceTimer_, &QTimer::timeout, this, &OAuthFlow::pollDeviceToken);
	}
	deviceTimer_->start(intervalSec * 1000);
}

void OAuthFlow::pollDeviceToken()
{
	if (finished_)
		return;

	deviceElapsedSec_ += deviceIntervalSec_;
	if (deviceExpiresInSec_ > 0 && deviceElapsedSec_ > deviceExpiresInSec_) {
		emit failed(platformId_, QStringLiteral("device_code_expired"),
		            tr("The sign-in code expired before it was confirmed."));
		cancel();
		return;
	}

	QHash<QString, QString> fields;
	fields.insert(QStringLiteral("grant_type"), QStringLiteral("urn:ietf:params:oauth:grant-type:device_code"));
	fields.insert(QStringLiteral("device_code"), deviceCode_);
	fields.insert(QStringLiteral("client_id"), client_.clientId);

	http_.send(QtHttpClient::post(spec_.tokenUrl, formEncode(fields)),
	           [this](const HttpResponse &response) {
		           if (finished_)
			           return;
		           const QJsonObject json =
			           QJsonDocument::fromJson(response.body).object();
		           const QString error = json.value(QStringLiteral("error")).toString();

		           if (error == QLatin1String("authorization_pending")) {
			           beginDevicePoll(deviceIntervalSec_);
			           return;
		           }
		           if (error == QLatin1String("slow_down")) {
			           deviceIntervalSec_ += 5;
			           beginDevicePoll(deviceIntervalSec_);
			           return;
		           }
		           if (!error.isEmpty()) {
			           emit failed(platformId_, error,
			                       json.value(QStringLiteral("error_description")).toString());
			           cancel();
			           return;
		           }

		           const qint64 now = QDateTime::currentSecsSinceEpoch();
		           const TokenSet token = parseTokenResponse(json, now);
		           if (token.accessToken.isEmpty()) {
			           emit failed(platformId_, QStringLiteral("token_exchange_failed"),
			                       tr("The token response was empty."));
			           cancel();
			           return;
		           }
		           finished_ = true;
		           if (deviceTimer_) {
			           deviceTimer_->stop();
			           deviceTimer_->deleteLater();
			           deviceTimer_ = nullptr;
		           }
		           emit authorized(platformId_, token);
	           });
}

} // namespace ums
