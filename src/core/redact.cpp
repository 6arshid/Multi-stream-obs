/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/redact.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace ums {


static const char *kMask = "********";

void Redactor::addSecret(const QString &secret)
{
	if (!secret.isEmpty() && !secrets_.contains(secret))
		secrets_.append(secret);
}

void Redactor::removeSecret(const QString &secret)
{
	secrets_.removeAll(secret);
}

void Redactor::clear()
{
	secrets_.clear();
}

QString Redactor::redact(const QString &text) const
{
	QString result = text;

	for (const QString &secret : secrets_) {
		if (result.contains(secret))
			result.replace(secret, QLatin1String(kMask));
	}

	static const QRegularExpression bearerRe(
		QStringLiteral(R"(Bearer\s+[A-Za-z0-9\-._~+/]+=*)"),
		QRegularExpression::CaseInsensitiveOption);
	result.replace(bearerRe, QStringLiteral("Bearer ") + QLatin1String(kMask));

	static const QRegularExpression pairRe(
		QStringLiteral(R"((access_token|refresh_token|stream_key|client_secret|token|key|secret|password|authorization)\s*[=:]\s*([^\s&"',;]+))"),
		QRegularExpression::CaseInsensitiveOption);
	result.replace(pairRe, QStringLiteral("\\1=") + QLatin1String(kMask));

	return result;
}

bool Redactor::isSensitiveKey(const QString &key)
{
	static const QSet<QString> sensitive = {
		"access_token",      "refresh_token", "id_token",     "stream_key",
		"streamkey",         "client_secret", "secret",       "password",
		"authorization",     "key",           "token",        "code_verifier",
		"server_url",        "stream_url",    "secure_stream_url", "rtmps_link",
	};
	return sensitive.contains(key.toLower());
}

static QJsonObject redactJson(const QJsonObject &object);

static QJsonValue redactValue(const QString &key, const QJsonValue &value)
{
	if (Redactor::isSensitiveKey(key) && value.isString())
		return QString(QLatin1String(kMask));

	if (value.isObject())
		return redactJson(value.toObject());
	if (value.isArray()) {
		QJsonArray out;
		const QJsonArray arr = value.toArray();
		for (const QJsonValue &v : arr)
			out.append(redactValue(key, v));
		return out;
	}
	return value;
}

static QJsonObject redactJson(const QJsonObject &object)
{
	QJsonObject out;
	for (auto it = object.begin(); it != object.end(); ++it)
		out.insert(it.key(), redactValue(it.key(), it.value()));
	return out;
}

QJsonObject Redactor::redactJson(const QJsonObject &object)
{
	QJsonObject out;
	for (auto it = object.begin(); it != object.end(); ++it)
		out.insert(it.key(), redactValue(it.key(), it.value()));
	return out;
}

} // namespace ums
