/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

namespace ums {

/* Logical credential namespaces. Secrets never live in the JSON config. */
namespace creds {
inline const QString kStreamKey = QStringLiteral("stream-key");
inline const QString kOAuthToken = QStringLiteral("oauth-token");
inline const QString kOAuthSecret = QStringLiteral("oauth-secret");
} // namespace creds

/* Secret storage backed by the OS credential vault:
 *  - Windows: Credential Manager (wincred)
 *  - macOS:   Keychain (Security framework)
 * Values are opaque byte blobs (UTF-8 JSON). */
class ICredentialStore {
public:
	virtual ~ICredentialStore() = default;

	virtual bool set(const QString &service, const QString &account, const QByteArray &blob) = 0;
	virtual QByteArray get(const QString &service, const QString &account) const = 0;
	virtual bool remove(const QString &service, const QString &account) = 0;
	virtual bool available() const = 0;

	bool contains(const QString &service, const QString &account) const
	{
		return !get(service, account).isEmpty();
	}
	QStringList listAccounts(const QString &service) const;
};

/* Native OS-backed store (production). If the OS vault cannot be used,
 * available() returns false and the plugin refuses to persist secrets
 * rather than falling back to plaintext. */
class NativeCredentialStore : public ICredentialStore {
public:
	NativeCredentialStore();

	bool set(const QString &service, const QString &account, const QByteArray &blob) override;
	QByteArray get(const QString &service, const QString &account) const override;
	bool remove(const QString &service, const QString &account) override;
	bool available() const override { return available_; }

private:
	bool available_ = false;
};

/* In-memory store for unit tests. */
class MemoryCredentialStore : public ICredentialStore {
public:
	bool set(const QString &service, const QString &account, const QByteArray &blob) override
	{
		data_.insert(key(service, account), blob);
		return true;
	}
	QByteArray get(const QString &service, const QString &account) const override
	{
		return data_.value(key(service, account));
	}
	bool remove(const QString &service, const QString &account) override
	{
		return data_.remove(key(service, account));
	}
	bool available() const override { return true; }
	void clearAll() { data_.clear(); }

private:
	static QString key(const QString &service, const QString &account)
	{
		return service + QLatin1Char('/') + account;
	}
	QHash<QString, QByteArray> data_;
};

} // namespace ums
