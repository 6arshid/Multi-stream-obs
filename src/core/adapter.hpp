/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/capabilities.hpp"
#include "core/config-store.hpp"
#include "core/credential-store.hpp"
#include "core/http-client.hpp"
#include "core/oauth-flow.hpp"

#include <QJsonObject>
#include <QVector>
#include <functional>
#include <memory>

namespace ums {

/* Result of resolving ingest credentials for a destination. The key is
 * transient: it is written straight into the OBS service settings and
 * never persisted in the JSON config. */
struct IngestEndpoint {
	QString server;
	QString key;
	/* Resources created/updated by the platform API (broadcast id, live
	 * video id, ...). Persisted to config.remoteId when non-empty. */
	QString remoteId;
	/* Non-secret metadata worth persisting (e.g. playback urls). */
	QJsonObject options;
	QString error;

	bool ok() const { return error.isEmpty() && !server.isEmpty(); }
};

struct AccountInfo {
	QString alias; /* stable identifier (channel/account id) */
	QString displayName;
	QJsonObject metadata;
};

/* Everything an adapter needs to talk to a platform. */
struct AuthContext {
	IHttpClient *http = nullptr;
	ICredentialStore *vault = nullptr;
	TokenSet token;              /* may be empty (manual mode / not signed in) */
	OAuthClientConfig client;    /* client id from config, secret from vault */
};

using IngestCallback = std::function<void(const IngestEndpoint &)>;
using AccountsCallback = std::function<void(bool ok, const QVector<AccountInfo> &accounts,
                                            const QString &error)>;
using TokenCheckCallback = std::function<void(bool ok, const QString &displayName,
                                              const QString &error)>;

/* One platform integration. Adapters are stateless: all state lives in the
 * config store and the credential vault. */
class IPlatformAdapter {
public:
	virtual ~IPlatformAdapter() = default;

	/* Static platform metadata: capabilities, default ingest, docs. */
	virtual PlatformInfo info() const = 0;

	/* OAuth definition for API integrations. Empty = manual-only. */
	virtual OAuthSpec oauthSpec() const { return {}; }

	/* Application client id shipped as a default (empty: the user must
	 * register their own application - see docs/API_SETUP.md). Never a
	 * secret. */
	virtual QString defaultClientId() const { return {}; }

	/* Platform-specific validation on top of the generic rules. */
	virtual QStringList validate(const DestinationConfig &destination) const
	{
		return ConfigStore::validate(destination);
	}

	/* Resolve server + stream key immediately before streaming starts.
	 * Manual adapters read the vault; API adapters call the platform. */
	virtual void prepareIngest(const AuthContext &auth, const DestinationConfig &destination,
	                           IngestCallback callback) = 0;

	/* Enumerate accounts the current token can manage. Default: succeed
	 * with no accounts (platforms without account APIs). */
	virtual void listAccounts(const AuthContext &auth, AccountsCallback callback)
	{
		Q_UNUSED(auth);
		callback(true, {}, {});
	}

	/* Validate the stored token and return a displayable account name.
	 * Default: accept any non-empty token. */
	virtual void validateToken(const AuthContext &auth, TokenCheckCallback callback)
	{
		if (auth.token.accessToken.isEmpty()) {
			callback(false, {}, QStringLiteral("no token"));
			return;
		}
		callback(true, auth.client.clientId, {});
	}
};

using AdapterPtr = std::unique_ptr<IPlatformAdapter>;

/* Resolves a manual destination: server (or the platform default) plus the
 * stream key held in the credential vault. */
inline IngestEndpoint resolveManualIngest(const PlatformInfo &info,
                                          const AuthContext &auth,
                                          const DestinationConfig &destination)
{
	IngestEndpoint endpoint;
	endpoint.server = destination.server.trimmed();
	if (endpoint.server.isEmpty())
		endpoint.server = info.defaultIngestUrl;
	if (endpoint.server.isEmpty()) {
		endpoint.error = QStringLiteral("no ingest server configured");
		return endpoint;
	}
	if (!auth.vault) {
		endpoint.error = QStringLiteral("credential store unavailable");
		return endpoint;
	}
	endpoint.key = QString::fromUtf8(
	    auth.vault->get(creds::kStreamKey, destination.id));
	if (endpoint.key.isEmpty()) {
		endpoint.error = QStringLiteral("no stream key stored");
		return endpoint;
	}
	return endpoint;
}

} // namespace ums
