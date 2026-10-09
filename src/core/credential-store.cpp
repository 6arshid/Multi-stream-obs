/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/credential-store.hpp"

#include <QHash>

#ifdef _WIN32
#include <windows.h>
#include <wincred.h>
#include <string>
#elif defined(__APPLE__)
#include <Security/Security.h>
#endif

namespace ums {

QStringList ICredentialStore::listAccounts(const QString &service) const
{
	QStringList accounts;
	const QString prefix = service + QLatin1Char('/');
	/* Native store implements enumeration via its own data; the generic
	 * fallback walks a probe list maintained by the config layer instead.
	 * Enumeration is optional: returns empty when unsupported. */
	Q_UNUSED(prefix);
	return accounts;
}

#ifdef _WIN32

static std::wstring wide(const QString &value)
{
	return value.toStdWString();
}

static QString targetName(const QString &service, const QString &account)
{
	return QStringLiteral("ums/%1/%2").arg(service, account);
}

NativeCredentialStore::NativeCredentialStore() : available_(true) {}

bool NativeCredentialStore::set(const QString &service, const QString &account, const QByteArray &blob)
{
	if (!available_)
		return false;

	if (blob.isEmpty())
		return remove(service, account);

	const std::wstring target = wide(targetName(service, account));
	const std::wstring user = wide(account);

	CREDENTIALW credential = {};
	credential.Type = CRED_TYPE_GENERIC;
	credential.TargetName = const_cast<LPWSTR>(target.c_str());
	credential.CredentialBlobSize = static_cast<DWORD>(blob.size());
	credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(blob.constData()));
	credential.Persist = CRED_PERSIST_ENTERPRISE;
	credential.UserName = const_cast<LPWSTR>(user.c_str());

	if (CredWriteW(&credential, 0) == TRUE)
		return true;

	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0);
		return CredWriteW(&credential, 0) == TRUE;
	}
	return false;
}

QByteArray NativeCredentialStore::get(const QString &service, const QString &account) const
{
	if (!available_)
		return {};

	const std::wstring target = wide(targetName(service, account));
	PCREDENTIALW credential = nullptr;
	if (CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential) != TRUE || !credential)
		return {};

	QByteArray blob(reinterpret_cast<const char *>(credential->CredentialBlob),
	                static_cast<int>(credential->CredentialBlobSize));
	CredFree(credential);
	return blob;
}

bool NativeCredentialStore::remove(const QString &service, const QString &account)
{
	if (!available_)
		return false;
	const std::wstring target = wide(targetName(service, account));
	return CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0) == TRUE;
}

#elif defined(__APPLE__)

static QString targetName(const QString &service, const QString &account)
{
	return QStringLiteral("%1|%2").arg(service, account);
}

NativeCredentialStore::NativeCredentialStore() : available_(true) {}

bool NativeCredentialStore::set(const QString &service, const QString &account, const QByteArray &blob)
{
	if (!available_)
		return false;

	remove(service, account);

	NSDictionary *query = @{
		(__bridge NSString *)kSecClass : (__bridge NSString *)kSecClassGenericPassword,
		(__bridge NSString *)kSecAttrService : (__bridge NSString *)service,
		(__bridge NSString *)kSecAttrAccount : (__bridge NSString *)account,
	};
	NSData *data = [NSData dataWithBytes:blob.constData() length:static_cast<NSUInteger>(blob.size())];
	NSMutableDictionary *attributes = [query mutableCopy];
	attributes[(__bridge NSString *)kSecValueData] = data;
	attributes[(__bridge NSString *)kSecAttrAccessible] =
	    (__bridge NSString *)kSecAttrAccessibleAfterFirstUnlock;

	const OSStatus status = SecItemAdd((__bridge CFDictionaryRef)attributes, nullptr);
	return status == errSecSuccess;
}

QByteArray NativeCredentialStore::get(const QString &service, const QString &account) const
{
	if (!available_)
		return {};

	NSDictionary *query = @{
		(__bridge NSString *)kSecClass : (__bridge NSString *)kSecClassGenericPassword,
		(__bridge NSString *)kSecAttrService : (__bridge NSString *)service,
		(__bridge NSString *)kSecAttrAccount : (__bridge NSString *)account,
		(__bridge NSString *)kSecReturnData : @YES,
		(__bridge NSString *)kSecMatchLimit : (__bridge NSString *)kSecMatchLimitOne,
	};

	CFTypeRef result = nullptr;
	const OSStatus status =
	    SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
	if (status != errSecSuccess || !result)
		return {};

	NSData *data = (__bridge_transfer NSData *)result;
	return QByteArray(static_cast<const char *>(data.bytes), static_cast<int>(data.length));
}

bool NativeCredentialStore::remove(const QString &service, const QString &account)
{
	if (!available_)
		return false;

	NSDictionary *query = @{
		(__bridge NSString *)kSecClass : (__bridge NSString *)kSecClassGenericPassword,
		(__bridge NSString *)kSecAttrService : (__bridge NSString *)service,
		(__bridge NSString *)kSecAttrAccount : (__bridge NSString *)account,
	};
	const OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
	return status == errSecSuccess || status == errSecItemNotFound;
}

#else

NativeCredentialStore::NativeCredentialStore() : available_(false) {}
bool NativeCredentialStore::set(const QString &, const QString &, const QByteArray &)
{
	return false;
}
QByteArray NativeCredentialStore::get(const QString &, const QString &) const
{
	return {};
}
bool NativeCredentialStore::remove(const QString &, const QString &)
{
	return false;
}

#endif

} // namespace ums
