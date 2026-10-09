/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/http-client.hpp"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

class QTcpServer;
class QTcpSocket;
class QTimer;

namespace ums {

struct TokenSet {
	QString accessToken;
	QString refreshToken;
	QString tokenType;
	QStringList scope;
	qint64 expiresAtEpochSec = 0;  /* 0 = token does not expire */
	qint64 obtainedAtEpochSec = 0;

	bool isEmpty() const { return accessToken.isEmpty(); }
	bool isExpired(qint64 nowEpochSec, qint64 skewSec = 60) const;
	QJsonObject toJson() const;
	static TokenSet fromJson(const QJsonObject &object);
};

enum class OAuthMode { AuthorizationCode, DeviceCode };

struct OAuthSpec {
	QString authUrl;
	QString tokenUrl;
	QString deviceAuthUrl;
	QString revokeUrl;
	/* Refresh endpoint; empty = tokenUrl. */
	QString refreshUrl;
	QStringList scopes;
	/* PKCE (RFC 7636, S256) - required by Kick, recommended for native apps. */
	bool pkce = false;
	OAuthMode mode = OAuthMode::AuthorizationCode;
	/* Some providers require a client_secret even for installed apps
	 * (Google desktop apps, Twitch confidential clients, Facebook). The
	 * plugin never embeds secrets; the developer supplies their own. */
	bool requiresClientSecret = false;
	/* Extra query parameters for the authorization request
	 * (e.g. access_type=offline, prompt=consent for Google). */
	QHash<QString, QString> extraAuthParams;
	QHash<QString, QString> extraTokenParams;
	/* Parameter name for scopes in a device-auth request ("scope" default,
	 * Twitch uses "scopes"). */
	QString deviceScopeParam = QStringLiteral("scope");
	/* Token responses vary: Twitch returns scope as array, Google/Kick as
	 * space-delimited string. Auto-detect unless forced. */
	bool scopeAsString = false;
	/* Google-style end_session_url style revocation is a POST with token. */
	bool revokeViaPost = true;
	/* Token requests sent as a JSON object instead of form-encoded (Trovo). */
	bool jsonTokenBody = false;
	/* Extra header carrying the client id on token requests, e.g. "Client-ID"
	 * (Trovo). Empty = client id stays in the body. */
	QString clientIdHeader;

	bool isValid() const { return !authUrl.isEmpty() && !tokenUrl.isEmpty(); }
	QString refreshEndpoint() const
	{
		return refreshUrl.isEmpty() ? tokenUrl : refreshUrl;
	}
};

struct OAuthClientConfig {
	QString clientId;
	QString clientSecret; /* stored in the OS credential vault, never in config */
};

struct RefreshOutcome {
	bool ok = false;
	TokenSet token;
	QString error;
	bool unauthorized = false;
};

/* --- Pure helpers (unit tested) --------------------------------------- */

QString generateCodeVerifier();
QString pkceChallengeS256(const QString &codeVerifier);
QString generateState();
TokenSet parseTokenResponse(const QJsonObject &json, qint64 nowEpochSec);
/* Builds the token-endpoint request for a refresh grant. */
HttpRequest buildRefreshRequest(const OAuthSpec &spec, const OAuthClientConfig &client,
                                const TokenSet &current);
RefreshOutcome applyTokenResponse(const OAuthSpec &spec, const QJsonObject &json,
                                  TokenSet &current, qint64 nowEpochSec);
/* Async refresh: updates `current` in place on success, then invokes
 * `callback` on the HTTP completion. */
void refreshAccessToken(IHttpClient &http, const OAuthSpec &spec,
                        const OAuthClientConfig &client, TokenSet &current,
                        std::function<void(const RefreshOutcome &)> callback);

/* --- Interactive flow -------------------------------------------------- */

/* Runs the browser-based loopback authorization flow (or the device-code
 * flow) and exchanges the result for a TokenSet. All network work is async;
 * results are delivered via signals on the owning (UI) thread. */
class OAuthFlow : public QObject {
	Q_OBJECT
public:
	OAuthFlow(IHttpClient &http, QObject *parent = nullptr);
	~OAuthFlow() override;

	bool isActive() const;
	void cancel();

	/* Authorization-code flow with a loopback redirect on 127.0.0.1, or -
	 * when `fixedRedirectUri` is set (registered at the provider) - a manual
	 * flow where the caller feeds the final browser URL to
	 * handleManualRedirect(). */
	bool startAuthorizationCode(const OAuthSpec &spec, const OAuthClientConfig &client,
	                            const QString &platformId,
	                            const QString &fixedRedirectUri = {});
	/* Complete a manual flow with the full redirect URL the browser landed
	 * on (the user pastes it from the address bar). */
	void handleManualRedirect(const QString &url);
	/* Device-code flow (Twitch): emits devicePrompt, then polls. */
	bool startDeviceCode(const OAuthSpec &spec, const OAuthClientConfig &client,
	                     const QString &platformId);

signals:
	void authorized(const QString &platformId, const TokenSet &token);
	void failed(const QString &platformId, const QString &error, const QString &description);
	void devicePrompt(const QString &verificationUri, const QString &userCode,
	                  int expiresInSeconds);
	/* The UI must open this URL in the system browser. */
	void openBrowser(const QString &url);

private:
	void handleConnection();
	void handleRequest();
	/* Shared callback handling for loopback and manual flows. */
	void processCallback(const QString &error, const QString &description,
	                     const QString &state, const QString &code);
	void respondToBrowser(int status, const QString &title, const QString &message);
	void exchangeCode(const QString &code);
	void beginDevicePoll(int intervalSec);
	void pollDeviceToken();

	IHttpClient &http_;
	QTcpServer *server_ = nullptr;
	QTimer *deviceTimer_ = nullptr;
	QTcpSocket *pendingSocket_ = nullptr;

	OAuthSpec spec_;
	OAuthClientConfig client_;
	QString platformId_;
	QString expectedState_;
	QString codeVerifier_;
	QString redirectUri_;
	QString deviceCode_;
	int deviceIntervalSec_ = 5;
	int deviceElapsedSec_ = 0;
	int deviceExpiresInSec_ = 0;
	bool finished_ = false;
	bool manualPending_ = false;
};

} // namespace ums
