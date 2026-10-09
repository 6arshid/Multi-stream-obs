/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QJsonObject>
#include <QString>

namespace ums {

/* Removes/masks secret material before text reaches logs or error dialogs. */
class Redactor {
public:
	/* Registers an exact secret value that must never appear in output. */
	void addSecret(const QString &secret);
	void removeSecret(const QString &secret);
	void clear();

	/* Masks registered secrets plus common token/key patterns. */
	QString redact(const QString &text) const;

	/* Recursively masks values of sensitive JSON object keys. */
	static QJsonObject redactJson(const QJsonObject &object);

	static bool isSensitiveKey(const QString &key);

private:
	QStringList secrets_;
};

} // namespace ums
