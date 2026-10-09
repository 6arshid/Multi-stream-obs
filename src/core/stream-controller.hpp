/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/adapter.hpp"
#include "core/config-store.hpp"
#include "core/credential-store.hpp"
#include "core/http-client.hpp"
#include "core/multi-output-engine.hpp"
#include "core/oauth-flow.hpp"
#include "core/redact.hpp"

#include <QHash>
#include <QObject>
#include <QSet>

#include <functional>
#include <memory>

namespace ums {

class IPlatformAdapter;

/* Orchestrates configuration, credentials, platform adapters, OAuth flows
 * and the multi-output engine. Created on the OBS UI thread. */
class StreamController : public QObject {
	Q_OBJECT
public:
	enum class NotifyLevel { Info, Warning, Error };
	Q_ENUM(NotifyLevel)

	static StreamController &instance();

	bool initialize(const QString &configFilePath);
	void shutdown();
	bool isInitialized() const { return initialized_; }

	ConfigStore &configStore() { return *config_; }
	const ConfigFile &config() const { return config_->config(); }
	ICredentialStore &vault() { return *vault_; }
	bool vaultAvailable() const { return vault_ && vault_->available(); }
	IHttpClient &http() { return *http_; }
	MultiOutputEngine &engine() { return *engine_; }

	/* --- destinations ------------------------------------------------- */
	QString addDestination(const QString &platformId);
	bool removeDestination(const QString &id);
	bool updateDestination(const DestinationConfig &updated, QStringList *errors = nullptr);
	void setDestinationEnabled(const QString &id, bool enabled);
	const DestinationConfig *destination(const QString &id) const;
	QString labelFor(const DestinationConfig &destination) const;

	/* --- secrets (OS credential vault) -------------------------------- */
	bool setStreamKey(const QString &destId, const QString &key);
	void clearStreamKey(const QString &destId);
	bool hasStreamKey(const QString &destId) const;

	/* --- platform application credentials ----------------------------- */
	QString platformClientId(const QString &platformId) const;
	QString platformRedirectUri(const QString &platformId) const;
	void setPlatformApp(const QString &platformId, const QString &clientId,
	                    const QString &redirectUri);
	void setPlatformClientSecret(const QString &platformId, const QString &secret);
	bool hasPlatformClientSecret(const QString &platformId) const;

	/* --- stream settings ---------------------------------------------- */
	StreamSettings streamSettings() const;
	void setStreamSettings(const StreamSettings &settings);

	/* --- accounts / OAuth --------------------------------------------- */
	bool isAccountConnected(const QString &platformId) const;
	QString accountDisplayName(const QString &platformId) const;
	void connectAccount(const QString &platformId);
	void cancelConnect();
	bool oauthFlowActive() const;
	bool oauthUsesManualRedirect(const QString &platformId) const;
	void handleManualRedirect(const QString &platformId, const QString &url);
	void storeAccessToken(const QString &platformId, const QString &token);
	void disconnectAccount(const QString &platformId);

	/* --- streaming ----------------------------------------------------- */
	void startAll();
	void stopAll();
	void startDestination(const QString &id);
	void stopDestination(const QString &id);
	bool isDestinationActive(const QString &id) const;
	bool anyActive() const;
	OutputState destinationState(const QString &id) const;
	OutputStats destinationStats(const QString &id) const;

	/* Frontend coupling (called from plugin-main's event callback). */
	void onMainStreamingStarted();
	void onMainStreamingStopped();

signals:
	void destinationsChanged();
	void destinationStateChanged(const QString &id, ums::OutputState state,
	                             const QString &detail);
	void destinationStatsChanged(const QString &id, const ums::OutputStats &stats);
	void accountStateChanged(const QString &platformId, bool connected,
	                         const QString &displayName);
	void oauthOpenBrowser(const QString &url);
	void oauthDevicePrompt(const QString &platformId, const QString &verificationUri,
	                       const QString &userCode, int expiresInSeconds);
	void notify(ums::StreamController::NotifyLevel level, const QString &message);

private:
	StreamController() = default;

	IPlatformAdapter *adapterFor(const QString &platformId) const;
	OAuthClientConfig makeClient(const QString &platformId) const;
	TokenSet loadToken(const QString &platformId) const;
	bool persistToken(const QString &platformId, const TokenSet &token);
	void registerTokenSecrets(const TokenSet &token);
	void withAuthContext(const QString &platformId,
	                     std::function<void(const AuthContext &)> callback);
	void handleIngest(const QString &destId, const IngestEndpoint &endpoint);
	void finishAccountConnect(const QString &platformId, const TokenSet &token);
	QJsonObject accountMeta(const QString &platformId) const;
	void setAccountMeta(const QString &platformId, const QJsonObject &meta);
	void notifyInfo(const QString &message);
	void notifyWarn(const QString &message);
	void notifyError(const QString &message);
	QString redact(const QString &text) const;

	bool initialized_ = false;
	std::unique_ptr<ConfigStore> config_;
	std::unique_ptr<ICredentialStore> vault_;
	std::unique_ptr<IHttpClient> http_;
	MultiOutputEngine *engine_ = nullptr;
	OAuthFlow *flow_ = nullptr;
	Redactor redactor_;
	QHash<QString, OutputStats> statsCache_;
	QSet<QString> pendingStarts_;
	QHash<QString, quint64> startRequests_;
	quint64 nextRequest_ = 0;
};

} // namespace ums
