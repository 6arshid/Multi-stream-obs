/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/config-store.hpp"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
using namespace ums;
class ConfigTest : public QObject
{
	Q_OBJECT
      private slots:
	void migration();
	void rejectsSecretsAndUnsupportedProtocols();
	void corruptionAndRoundTrip();
};
#ifndef Q_MOC_RUN
void ConfigTest::migration()
{
	QJsonObject root{{"schemaVersion", 1},
			 {"streams", QJsonArray{QJsonObject{
					 {"id", "a"}, {"platform", "custom"}, {"key", "secret"}}}}};
	QString error;
	QVERIFY(ConfigStore::migrate(root, &error));
	QCOMPARE(root.value("schemaVersion").toInt(), 3);
	auto d = root.value("destinations").toArray().first().toObject();
	QVERIFY(!d.contains("key"));
	QCOMPARE(d.value("authMode").toString(), QString("manual"));
	QJsonObject future{{"schemaVersion", 99}};
	QVERIFY(!ConfigStore::migrate(future, &error));
}

void ConfigTest::corruptionAndRoundTrip()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	auto path = dir.filePath("config.json");
	QFile f(path);
	QVERIFY(f.open(QIODevice::WriteOnly));
	f.write("broken-json");
	f.close();
	ConfigStore store(path);
	auto load = store.load();
	QVERIFY(load.ok);
	QVERIFY(load.recoveredFromCorruption);
	QVERIFY(QFile::exists(load.backupPath));
	DestinationConfig d;
	d.id = "a";
	d.platform = "custom";
	d.server = "rtmps://example.test/app";
	store.config().destinations.append(d);
	QVERIFY(store.save());
	ConfigStore copy(path);
	QVERIFY(copy.load().ok);
	QCOMPARE(copy.config().destinations.size(), 1);
	QVERIFY(!copy.exportableJson().contains("accounts"));
}
void ConfigTest::rejectsSecretsAndUnsupportedProtocols()
{
	DestinationConfig d;
	d.id = "a";
	d.platform = "custom";
	d.server = "srt://example.test:9000";
	QVERIFY(!ConfigStore::validate(d).isEmpty());
	d.server = "rtmps://example.test/app";
	QVERIFY(ConfigStore::validate(d).isEmpty());
	d.options = {{"nested", QJsonObject{{"ACCESS_TOKEN", "secret"}}}};
	QVERIFY(!ConfigStore::validate(d).isEmpty());
}

#endif

int runConfig(int argc, char **argv)
{
	ConfigTest test;
	return QTest::qExec(&test, argc, argv);
}
#include "test-config.moc"
