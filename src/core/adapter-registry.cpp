/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/adapter-registry.hpp"

#include "platforms/factories.hpp"

#include <algorithm>

namespace ums {

AdapterRegistry &AdapterRegistry::instance()
{
	static AdapterRegistry registry;
	return registry;
}

void AdapterRegistry::add(AdapterPtr adapter)
{
	if (!adapter)
		return;
	const QString id = adapter->info().id;
	adapters_.insert_or_assign(id, std::move(adapter));
}

IPlatformAdapter *AdapterRegistry::get(const QString &id) const
{
	const auto it = adapters_.find(id);
	return it == adapters_.end() ? nullptr : it->second.get();
}

bool AdapterRegistry::contains(const QString &id) const
{
	return adapters_.count(id) > 0;
}

QVector<const IPlatformAdapter *> AdapterRegistry::all() const
{
	QVector<const IPlatformAdapter *> result;
	result.reserve(static_cast<int>(adapters_.size()));
	for (const auto &entry : adapters_)
		result.append(entry.second.get());
	std::sort(result.begin(), result.end(),
	          [](const IPlatformAdapter *a, const IPlatformAdapter *b) {
		          return a->info().displayName.localeAwareCompare(
		                     b->info().displayName) < 0;
	          });
	return result;
}

QStringList AdapterRegistry::ids() const
{
	QStringList result;
	for (const auto &entry : adapters_)
		result.append(entry.first);
	result.sort(Qt::CaseInsensitive);
	return result;
}

void AdapterRegistry::clear()
{
	adapters_.clear();
	builtinsRegistered_ = false;
}

void AdapterRegistry::ensureBuiltins()
{
	if (builtinsRegistered_)
		return;
	builtinsRegistered_ = true;
	add(createManualAdapter());
	add(createTikTokAdapter());
	add(createInstagramAdapter());
	add(createXAdapter());
	add(createLinkedInAdapter());
	add(createRumbleAdapter());
	add(createDLiveAdapter());
	add(createBigoAdapter());
	add(createDiscordAdapter());
	add(createStreamYardAdapter());

	add(createYouTubeAdapter());
	add(createTwitchAdapter());
	add(createKickAdapter());
	add(createVimeoAdapter());
	add(createFacebookAdapter());
	add(createRestreamAdapter());
	add(createTrovoAdapter());
}

} // namespace ums
