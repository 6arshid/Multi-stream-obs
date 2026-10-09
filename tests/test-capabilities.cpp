/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/adapter-registry.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>
using namespace ums;
class CapabilitiesTest : public QObject
{
	Q_OBJECT
      private slots:
	void registeredAdapters();
	void inconsistentDeclaration();
};
#ifndef Q_MOC_RUN
void CapabilitiesTest::registeredAdapters()
{
	auto &registry = AdapterRegistry::instance();
	registry.ensureBuiltins();
	QVERIFY(registry.ids().size() >= 17);
	for (const auto *adapter : registry.all()) {
		const auto info = adapter->info();
		auto errors = validatePlatformInfo(info);
		QVERIFY2(errors.isEmpty(), qPrintable(info.id + ": " + errors.join(", ")));
		for (int i = int(Feature::OAuth); i <= int(Feature::Analytics); ++i) {
			auto f = Feature(i);
			auto status = info.caps.status(f);
			if (status == FeatureStatus::Full || status == FeatureStatus::Partial)
				QVERIFY(info.has(f));
			if (status == FeatureStatus::Unavailable ||
			    status == FeatureStatus::NotImplemented)
				QVERIFY(!info.has(f));
		}
	}
}

void CapabilitiesTest::inconsistentDeclaration()
{
	PlatformInfo info;
	info.id = "test";
	info.displayName = "Test";
	info.caps.oauth = FeatureStatus::Full;
	QVERIFY(!validatePlatformInfo(info).isEmpty());
}
#endif

int runCapabilities(int argc, char **argv)
{
	CapabilitiesTest test;
	return QTest::qExec(&test, argc, argv);
}
#include "test-capabilities.moc"
