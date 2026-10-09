/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "stream-controller.hpp"

#include "core/adapter-registry.hpp"

#include <obs.h>

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace ums
{

StreamController &StreamController::instance()
{
	static StreamController controller;
	return controller;
}

IPlatformAdapter *StreamController::adapterFor(const QString &platformId) const
{
	auto &registry = AdapterRegistry::instance();
	registry.ensureBuiltins();
	return registry.get(platformId);
}

bool StreamController::initialize(const QString &configFilePath)
{
	if (initialized_)
		return true;

	config_ = std::make_unique<ConfigStore>(configFilePath);
	const LoadResult result = config_->load();
	if (result.recoveredFromCorruption) {
		blog(LOG_WARNING, "[ums] config was corrupted and reset (backup: %s)",
		     result.backupPath.toUtf8().constData());
	}
	if (!result.ok) {
		blog(LOG_ERROR, "[ums] config load failed: %s", result.error.toUtf8().constData());
		return false;
	}

	vault_ = std::make_unique<NativeCredentialStore>();
	if (!vault_->available())
		blog(LOG_WARNING,
		     "[ums] OS credential vault unavailable - secrets cannot be stored");

	http_ = std::make_unique<QtHttpClient>();

	engine_ = new MultiOutputEngine(this);
	engine_->setStreamSettings(config_->config().settings);

	connect(engine_, &MultiOutputEngine::stateChanged, this,
		[this](const QString &id, OutputState state, const QString &detail) {
			if (state == OutputState::Stopped && !detail.isEmpty()) {
				const DestinationConfig *dest = destination(id);
				const QString name = dest ? labelFor(*dest) : id;
				notifyError(tr("%1: %2").arg(name, detail));
				if (config_->config().settings.stopOthersOnFatal)
					stopAll();
			}
			emit destinationStateChanged(id, state, redact(detail));
		});
	connect(engine_, &MultiOutputEngine::statsChanged, this,
		[this](const QString &id, const OutputStats &stats) {
			statsCache_.insert(id, stats);
			emit destinationStatsChanged(id, stats);
		});

	AdapterRegistry::instance().ensureBuiltins();
	initialized_ = true;
	blog(LOG_INFO, "[ums] initialized (%d destination(s))",
	     config_->config().destinations.size());
	return true;
}

void StreamController::shutdown()
{
	if (!initialized_)
		return;
	cancelConnect();
	pendingStarts_.clear();
	startRequests_.clear();
	if (engine_) {
		engine_->stopAll();
		delete engine_;
		engine_ = nullptr;
	}
	config_->save();
	initialized_ = false;
}

/* --- destinations ------------------------------------------------------ */

QString StreamController::addDestination(const QString &platformId)
{
	if (!initialized_)
		return {};
	IPlatformAdapter *adapter = adapterFor(platformId);
	if (!adapter) {
		notifyError(tr("Unknown platform: %1").arg(platformId));
		return {};
	}

	const PlatformInfo info = adapter->info();
	DestinationConfig dest;
	dest.id = config_->config().generateDestinationId();
	dest.platform = platformId;
	dest.enabled = false;
	dest.authMode = adapter->oauthSpec().isValid() ? AuthMode::Api : AuthMode::Manual;
	dest.keyframeIntervalSec = 2;

	QString base = info.displayName;
	QString label = base;
	int n = 2;
	const auto &existing = config_->config().destinations;
	const auto labelTaken = [&existing](const QString &candidate) {
		for (const auto &d : existing) {
			if (d.label.compare(candidate, Qt::CaseInsensitive) == 0)
				return true;
		}
		return false;
	};
	while (labelTaken(label))
		label = QStringLiteral("%1 %2").arg(base).arg(n++);
	dest.label = label;

	config_->config().destinations.append(dest);
	config_->save();
	emit destinationsChanged();
	return dest.id;
}

bool StreamController::removeDestination(const QString &id)
{
	if (!initialized_)
		return false;
	pendingStarts_.remove(id);
	startRequests_.remove(id);
	if (!config_->config().removeDestination(id))
		return false;
	engine_->discard(id);
	vault_->remove(creds::kStreamKey, id);
	statsCache_.remove(id);
	config_->save();
	emit destinationsChanged();
	return true;
}

bool StreamController::updateDestination(const DestinationConfig &updated, QStringList *errors)
{
	if (!initialized_)
		return false;
	DestinationConfig *dest = config_->config().findDestination(updated.id);
	if (!dest) {
		if (errors)
			*errors << tr("destination not found");
		return false;
	}
	IPlatformAdapter *adapter = adapterFor(updated.platform);
	QStringList problems;
	if (adapter)
		problems = adapter->validate(updated);
	else
		problems = ConfigStore::validate(updated);
	if (!problems.isEmpty()) {
		if (errors)
			*errors = problems;
		return false;
	}
	stopDestination(updated.id);
	*dest = updated;
	engine_->discard(dest->id); /* re-resolve ingest on next start */
	config_->save();
	emit destinationsChanged();
	return true;
}

void StreamController::setDestinationEnabled(const QString &id, bool enabled)
{
	if (!initialized_)
		return;
	DestinationConfig *dest = config_->config().findDestination(id);
	if (!dest || dest->enabled == enabled)
		return;
	dest->enabled = enabled;
	config_->save();
	emit destinationsChanged();
}

const DestinationConfig *StreamController::destination(const QString &id) const
{
	return initialized_ ? config_->config().findDestination(id) : nullptr;
}

QString StreamController::labelFor(const DestinationConfig &destination) const
{
	if (!destination.label.trimmed().isEmpty())
		return destination.label;
	IPlatformAdapter *adapter = adapterFor(destination.platform);
	return adapter ? adapter->info().displayName : destination.platform;
}

/* --- secrets ------------------------------------------------------------ */

bool StreamController::setStreamKey(const QString &destId, const QString &key)
{
	if (!initialized_ || destId.isEmpty())
		return false;
	const QString trimmed = key.trimmed();
	if (trimmed.isEmpty())
		return clearStreamKey(destId), true;
	if (!vault_->set(creds::kStreamKey, destId, trimmed.toUtf8())) {
		notifyError(tr("Could not store the stream key in the credential vault."));
		return false;
	}
	redactor_.addSecret(trimmed);
	if (DestinationConfig *dest = config_->config().findDestination(destId)) {
		if (!dest->hasKey) {
			dest->hasKey = true;
			config_->save();
		}
	}
	emit destinationsChanged();
	return true;
}

void StreamController::clearStreamKey(const QString &destId)
{
	if (!initialized_)
		return;
	stopDestination(destId);
	vault_->remove(creds::kStreamKey, destId);
	if (DestinationConfig *dest = config_->config().findDestination(destId)) {
		if (dest->hasKey) {
			dest->hasKey = false;
			config_->save();
		}
	}
	engine_->discard(destId);
	emit destinationsChanged();
}

bool StreamController::hasStreamKey(const QString &destId) const
{
	return initialized_ && vault_->contains(creds::kStreamKey, destId);
}

/* --- platform application credentials ----------------------------------- */

QString StreamController::platformClientId(const QString &platformId) const
{
	if (!initialized_)
		return {};
	return accountMeta(platformId).value(QStringLiteral("clientId")).toString();
}

QString StreamController::platformRedirectUri(const QString &platformId) const
{
	if (!initialized_)
		return {};
	return accountMeta(platformId).value(QStringLiteral("redirectUri")).toString();
}

void StreamController::setPlatformApp(const QString &platformId, const QString &clientId,
				      const QString &redirectUri)
{
	if (!initialized_)
		return;
	QJsonObject meta = accountMeta(platformId);
	meta.insert(QStringLiteral("clientId"), clientId.trimmed());
	meta.insert(QStringLiteral("redirectUri"), redirectUri.trimmed());
	setAccountMeta(platformId, meta);
	config_->save();
}

void StreamController::setPlatformClientSecret(const QString &platformId, const QString &secret)
{
	if (!initialized_)
		return;
	const QString trimmed = secret.trimmed();
	if (trimmed.isEmpty()) {
		vault_->remove(creds::kOAuthSecret, platformId);
		return;
	}
	if (!vault_->set(creds::kOAuthSecret, platformId, trimmed.toUtf8())) {
		notifyError(tr("Could not store the client secret in the credential vault."));
		return;
	}
	redactor_.addSecret(trimmed);
}

bool StreamController::hasPlatformClientSecret(const QString &platformId) const
{
	return initialized_ && vault_->contains(creds::kOAuthSecret, platformId);
}

/* --- stream settings ------------------------------------------------------ */

StreamSettings StreamController::streamSettings() const
{
	return initialized_ ? config_->config().settings : StreamSettings();
}

void StreamController::setStreamSettings(const StreamSettings &settings)
{
	if (!initialized_)
		return;
	config_->config().settings = settings;
	config_->save();
	engine_->setStreamSettings(settings);
}

/* --- accounts / OAuth ------------------------------------------------------ */

OAuthClientConfig StreamController::makeClient(const QString &platformId) const
{
	OAuthClientConfig client;
	client.clientId = platformClientId(platformId);
	if (client.clientId.isEmpty()) {
		IPlatformAdapter *adapter = adapterFor(platformId);
		if (adapter)
			client.clientId = adapter->defaultClientId();
	}
	if (initialized_)
		client.clientSecret =
		    QString::fromUtf8(vault_->get(creds::kOAuthSecret, platformId));
	return client;
}

TokenSet StreamController::loadToken(const QString &platformId) const
{
	if (!initialized_)
		return {};
	const QByteArray raw = vault_->get(creds::kOAuthToken, platformId);
	if (raw.isEmpty())
		return {};
	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
	if (parseError.error != QJsonParseError::NoError || !doc.isObject())
		return {};
	return TokenSet::fromJson(doc.object());
}

bool StreamController::persistToken(const QString &platformId, const TokenSet &token)
{
	if (!initialized_)
		return false;
	if (token.isEmpty()) {
		vault_->remove(creds::kOAuthToken, platformId);
		return true;
	}
	const QByteArray blob = QJsonDocument(token.toJson()).toJson(QJsonDocument::Compact);
	return vault_->set(creds::kOAuthToken, platformId, blob);
}

void StreamController::registerTokenSecrets(const TokenSet &token)
{
	if (!token.accessToken.isEmpty())
		redactor_.addSecret(token.accessToken);
	if (!token.refreshToken.isEmpty())
		redactor_.addSecret(token.refreshToken);
}

bool StreamController::isAccountConnected(const QString &platformId) const
{
	return initialized_ && vault_->contains(creds::kOAuthToken, platformId);
}

QString StreamController::accountDisplayName(const QString &platformId) const
{
	if (!initialized_)
		return {};
	return accountMeta(platformId).value(QStringLiteral("displayName")).toString();
}

QJsonObject StreamController::accountMeta(const QString &platformId) const
{
	return initialized_ ? config_->config().accounts.value(platformId) : QJsonObject();
}

void StreamController::setAccountMeta(const QString &platformId, const QJsonObject &meta)
{
	if (!initialized_)
		return;
	if (meta.isEmpty())
		config_->config().accounts.remove(platformId);
	else
		config_->config().accounts.insert(platformId, meta);
}

void StreamController::connectAccount(const QString &platformId)
{
	if (!initialized_)
		return;
	IPlatformAdapter *adapter = adapterFor(platformId);
	if (!adapter) {
		notifyError(tr("Unknown platform: %1").arg(platformId));
		return;
	}
	const OAuthSpec spec = adapter->oauthSpec();
	if (!spec.isValid()) {
		notifyInfo(tr("%1 does not offer API sign-in; use manual mode.")
			       .arg(adapter->info().displayName));
		return;
	}
	const OAuthClientConfig client = makeClient(platformId);
	if (client.clientId.isEmpty()) {
		notifyError(
		    tr("No OAuth client id for %1. Register an application with the platform and "
		       "add its client id in Settings (see docs/API_SETUP.md).")
			.arg(adapter->info().displayName));
		return;
	}
	if (spec.requiresClientSecret && client.clientSecret.isEmpty()) {
		notifyError(tr("%1 requires an OAuth client secret. Add it in Settings (see "
			       "docs/API_SETUP.md).")
				.arg(adapter->info().displayName));
		return;
	}

	cancelConnect();
	flow_ = new OAuthFlow(*http_, this);
	connect(flow_, &OAuthFlow::openBrowser, this, &StreamController::oauthOpenBrowser);
	connect(flow_, &OAuthFlow::devicePrompt, this,
		[this, platformId](const QString &uri, const QString &code, int seconds) {
			emit oauthDevicePrompt(platformId, uri, code, seconds);
		});
	connect(flow_, &OAuthFlow::authorized, this,
		[this, platformId](const QString &, const TokenSet &token) {
			if (flow_) {
				flow_->deleteLater();
				flow_ = nullptr;
			}
			persistToken(platformId, token);
			registerTokenSecrets(token);
			notifyInfo(tr("Signed in. Fetching account details..."));
			finishAccountConnect(platformId, token);
		});
	connect(
	    flow_, &OAuthFlow::failed, this,
	    [this, platformId](const QString &, const QString &error, const QString &description) {
		    if (flow_) {
			    flow_->deleteLater();
			    flow_ = nullptr;
		    }
		    const QString detail = description.isEmpty() ? error : description;
		    notifyError(tr("Sign-in failed: %1").arg(redact(detail)));
		    emit accountStateChanged(platformId, isAccountConnected(platformId),
					     accountDisplayName(platformId));
	    });

	bool started = false;
	if (spec.mode == OAuthMode::DeviceCode)
		started = flow_->startDeviceCode(spec, client, platformId);
	else
		started = flow_->startAuthorizationCode(spec, client, platformId,
							platformRedirectUri(platformId));
	if (!started) {
		notifyError(tr("Could not start the sign-in flow."));
		flow_->deleteLater();
		flow_ = nullptr;
		return;
	}
	if (oauthUsesManualRedirect(platformId))
		notifyInfo(tr("Complete the sign-in in your browser, then paste the address it "
			      "redirects to."));
	else
		notifyInfo(tr("Complete the sign-in in the browser window that opened."));
}

void StreamController::cancelConnect()
{
	if (!flow_)
		return;
	flow_->cancel();
	flow_->deleteLater();
	flow_ = nullptr;
}

bool StreamController::oauthFlowActive() const { return flow_ && flow_->isActive(); }

bool StreamController::oauthUsesManualRedirect(const QString &platformId) const
{
	IPlatformAdapter *adapter = adapterFor(platformId);
	if (!adapter)
		return false;
	const OAuthSpec spec = adapter->oauthSpec();
	return spec.isValid() && spec.mode == OAuthMode::AuthorizationCode &&
	       !platformRedirectUri(platformId).isEmpty();
}

void StreamController::handleManualRedirect(const QString &platformId, const QString &url)
{
	Q_UNUSED(platformId);
	if (flow_)
		flow_->handleManualRedirect(url);
}

void StreamController::storeAccessToken(const QString &platformId, const QString &token)
{
	if (!initialized_)
		return;
	const QString trimmed = token.trimmed();
	if (trimmed.isEmpty()) {
		notifyError(tr("The access token is empty."));
		return;
	}
	TokenSet set;
	set.accessToken = trimmed;
	set.obtainedAtEpochSec = QDateTime::currentSecsSinceEpoch();
	persistToken(platformId, set);
	registerTokenSecrets(set);
	finishAccountConnect(platformId, set);
}

void StreamController::disconnectAccount(const QString &platformId)
{
	if (!initialized_)
		return;
	vault_->remove(creds::kOAuthToken, platformId);
	QJsonObject meta = accountMeta(platformId);
	meta.remove(QStringLiteral("displayName"));
	meta.remove(QStringLiteral("accounts"));
	setAccountMeta(platformId, meta);
	config_->save();
	emit accountStateChanged(platformId, false, {});
	notifyInfo(tr("Account disconnected."));
}

void StreamController::finishAccountConnect(const QString &platformId, const TokenSet &token)
{
	IPlatformAdapter *adapter = adapterFor(platformId);
	if (!adapter) {
		emit accountStateChanged(platformId, true, {});
		return;
	}
	auto ctx = std::make_shared<AuthContext>();
	ctx->http = http_.get();
	ctx->vault = vault_.get();
	ctx->token = token;
	ctx->client = makeClient(platformId);

	adapter->validateToken(*ctx, [this, platformId, adapter, ctx, token, client = ctx->client](
					 bool ok, const QString &name, const QString &error) {
		QJsonObject meta = accountMeta(platformId);
		if (ok && !name.isEmpty())
			meta.insert(QStringLiteral("displayName"), name);
		if (!ok)
			notifyWarn(
			    tr("Connected, but token validation failed: %1").arg(redact(error)));

		adapter->listAccounts(*ctx, [this, platformId, meta, token, client](
						bool listOk, const QVector<AccountInfo> &accounts,
						const QString &listError) mutable {
			QJsonObject updated = meta;
			if (listOk) {
				QJsonArray list;
				for (const AccountInfo &account : accounts) {
					QJsonObject entry;
					entry.insert(QStringLiteral("alias"), account.alias);
					entry.insert(QStringLiteral("name"), account.displayName);
					list.append(entry);
				}
				updated.insert(QStringLiteral("accounts"), list);
				if (updated.value(QStringLiteral("displayName"))
					.toString()
					.isEmpty() &&
				    !accounts.isEmpty())
					updated.insert(QStringLiteral("displayName"),
						       accounts.first().displayName);
			} else {
				notifyWarn(
				    tr("Could not enumerate accounts: %1").arg(redact(listError)));
			}
			setAccountMeta(platformId, updated);
			config_->save();
			emit accountStateChanged(
			    platformId, true,
			    updated.value(QStringLiteral("displayName")).toString());
			Q_UNUSED(token);
			Q_UNUSED(client);
		});
	});
}

/* --- streaming --------------------------------------------------------- */

void StreamController::withAuthContext(const QString &platformId,
				       std::function<void(const AuthContext &)> callback)
{
	IPlatformAdapter *adapter = adapterFor(platformId);
	auto token = std::make_shared<TokenSet>(loadToken(platformId));

	auto deliver = [this, callback, token, platformId]() {
		if (!initialized_)
			return;
		AuthContext ctx;
		ctx.http = http_.get();
		ctx.vault = vault_.get();
		ctx.token = *token;
		ctx.client = makeClient(platformId);
		callback(ctx);
	};

	if (token->isEmpty() || !adapter || !adapter->oauthSpec().isValid() ||
	    !token->isExpired(QDateTime::currentSecsSinceEpoch()) ||
	    token->refreshToken.isEmpty()) {
		deliver();
		return;
	}

	refreshAccessToken(
	    *http_, adapter->oauthSpec(), makeClient(platformId), *token,
	    [this, platformId, deliver, token](const RefreshOutcome &outcome) {
		    if (!initialized_)
			    return;
		    if (outcome.ok) {
			    persistToken(platformId, *token);
			    registerTokenSecrets(*token);
		    } else {
			    token->accessToken.clear();
			    IPlatformAdapter *adapter = adapterFor(platformId);
			    const QString name = adapter ? adapter->info().displayName : platformId;
			    notifyWarn(tr("Session for %1 expired and could not be refreshed: %2")
					   .arg(name, redact(outcome.error)));
		    }
		    deliver();
	    });
}

void StreamController::handleIngest(const QString &destId, const IngestEndpoint &endpoint)
{
	if (!endpoint.ok()) {
		const DestinationConfig *dest = destination(destId);
		const QString name = dest ? labelFor(*dest) : destId;
		const QString message = redact(endpoint.error);
		notifyWarn(tr("Cannot start %1: %2").arg(name, message));
		emit destinationStateChanged(destId, OutputState::Stopped, message);
		return;
	}

	DestinationConfig *dest = config_->config().findDestination(destId);
	if (!dest)
		return;

	bool changed = false;
	if (!endpoint.remoteId.isEmpty() && dest->remoteId != endpoint.remoteId) {
		dest->remoteId = endpoint.remoteId;
		changed = true;
	}
	if (!endpoint.options.isEmpty()) {
		QJsonObject merged = dest->options;
		for (auto it = endpoint.options.begin(); it != endpoint.options.end(); ++it)
			merged.insert(it.key(), it.value());
		if (merged != dest->options) {
			dest->options = merged;
			changed = true;
		}
	}
	if (changed)
		config_->save();

	if (!endpoint.key.isEmpty())
		redactor_.addSecret(endpoint.key);

	if (!engine_->prepare(destId, endpoint.server, endpoint.key, *dest)) {
		notifyError(tr("Failed to create the output for %1.").arg(labelFor(*dest)));
		emit destinationStateChanged(destId, OutputState::Stopped,
					     tr("output creation failed"));
		return;
	}
	engine_->start(destId);
}

void StreamController::startDestination(const QString &id)
{
	if (!initialized_)
		return;
	const DestinationConfig *found = destination(id);
	if (!found) {
		notifyError(tr("Destination not found."));
		return;
	}
	if (isDestinationActive(id))
		return;
	const DestinationConfig copy = *found;
	IPlatformAdapter *adapter = adapterFor(copy.platform);
	if (!adapter) {
		notifyError(tr("Unknown platform: %1").arg(copy.platform));
		emit destinationStateChanged(id, OutputState::Stopped, tr("unknown platform"));
		return;
	}

	const QStringList errors = adapter->validate(copy);
	if (!errors.isEmpty()) {
		notifyWarn(tr("%1: %2").arg(labelFor(copy), errors.join(QStringLiteral("; "))));
		emit destinationStateChanged(id, OutputState::Stopped, errors.first());
		return;
	}

	const quint64 request = ++nextRequest_;
	startRequests_.insert(id, request);
	pendingStarts_.insert(id);
	emit destinationStateChanged(id, OutputState::Starting, {});
	auto complete = [this, id, request](const IngestEndpoint &endpoint) {
		if (!initialized_ || startRequests_.value(id) != request ||
		    !pendingStarts_.contains(id))
			return;
		pendingStarts_.remove(id);
		handleIngest(id, endpoint);
	};
	if (copy.authMode == AuthMode::Api && adapter->oauthSpec().isValid()) {
		withAuthContext(copy.platform, [this, id, adapter, request,
						complete](const AuthContext &ctx) {
			if (!initialized_ || startRequests_.value(id) != request ||
			    !pendingStarts_.contains(id))
				return;
			if (ctx.token.isEmpty()) {
				pendingStarts_.remove(id);
				const DestinationConfig *dest = destination(id);
				const QString name = dest ? labelFor(*dest) : id;
				notifyError(
				    tr("Connect the %1 account before starting this destination.")
					.arg(name));
				emit destinationStateChanged(id, OutputState::Stopped,
							     tr("account not connected"));
				return;
			}
			const DestinationConfig *live = destination(id);
			if (!live)
				return;
			adapter->prepareIngest(ctx, *live, complete);
		});
		return;
	}

	AuthContext ctx;
	ctx.http = http_.get();
	ctx.vault = vault_.get();
	adapter->prepareIngest(ctx, copy, complete);
}

void StreamController::stopDestination(const QString &id)
{
	if (initialized_) {
		startRequests_.remove(id);
		if (pendingStarts_.remove(id))
			emit destinationStateChanged(id, OutputState::Stopped, {});
		engine_->stop(id);
	}
}

void StreamController::startAll()
{
	if (!initialized_)
		return;
	int candidates = 0;
	const auto destinations = config_->config().destinations;
	for (const DestinationConfig &dest : destinations) {
		if (!dest.enabled)
			continue;
		candidates++;
		startDestination(dest.id);
	}
	if (candidates == 0)
		notifyInfo(tr("No enabled destinations."));
	else
		notifyInfo(tr("Starting %1 destination(s)...").arg(candidates));
}

void StreamController::stopAll()
{
	if (initialized_) {
		const auto pending = pendingStarts_;
		pendingStarts_.clear();
		startRequests_.clear();
		for (const auto &id : pending)
			emit destinationStateChanged(id, OutputState::Stopped, {});
		engine_->stopAll();
	}
}

bool StreamController::isDestinationActive(const QString &id) const
{
	return initialized_ && (pendingStarts_.contains(id) || engine_->isActive(id));
}

bool StreamController::anyActive() const
{
	return initialized_ && (!pendingStarts_.isEmpty() || engine_->anyActive());
}

OutputState StreamController::destinationState(const QString &id) const
{
	return initialized_
		   ? (pendingStarts_.contains(id) ? OutputState::Starting : engine_->state(id))
		   : OutputState::Idle;
}

OutputStats StreamController::destinationStats(const QString &id) const
{
	return statsCache_.value(id);
}

void StreamController::onMainStreamingStarted()
{
	if (!initialized_)
		return;
	if (config_->config().settings.startWithMain && !anyActive()) {
		notifyInfo(tr("OBS started streaming - starting all enabled destinations."));
		startAll();
	}
}

void StreamController::onMainStreamingStopped()
{
	if (!initialized_)
		return;
	if (config_->config().settings.startWithMain && anyActive()) {
		notifyInfo(tr("OBS stopped streaming - stopping destinations."));
		stopAll();
	}
}

/* --- helpers ------------------------------------------------------------ */

void StreamController::notifyInfo(const QString &message)
{
	emit notify(NotifyLevel::Info, redact(message));
}

void StreamController::notifyWarn(const QString &message)
{
	emit notify(NotifyLevel::Warning, redact(message));
}

void StreamController::notifyError(const QString &message)
{
	emit notify(NotifyLevel::Error, redact(message));
}

QString StreamController::redact(const QString &text) const { return redactor_.redact(text); }

} // namespace ums
