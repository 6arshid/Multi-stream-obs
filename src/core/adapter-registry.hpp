/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/adapter.hpp"

#include <map>
#include <QStringList>

namespace ums {

/* Process-wide registry of built-in platform adapters. */
class AdapterRegistry {
public:
	static AdapterRegistry &instance();

	void add(AdapterPtr adapter);
	IPlatformAdapter *get(const QString &id) const;
	bool contains(const QString &id) const;
	/* Sorted by display name. */
	QVector<const IPlatformAdapter *> all() const;
	QStringList ids() const;
	/* For tests: drops every adapter. */
	void clear();
	/* Registers the built-in set (idempotent). */
	void ensureBuiltins();

private:
	AdapterRegistry() = default;
	std::map<QString, AdapterPtr> adapters_;
	bool builtinsRegistered_ = false;
};

} // namespace ums
