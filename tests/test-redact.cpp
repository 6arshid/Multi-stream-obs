/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/credential-store.hpp"
#include "core/redact.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>
using namespace ums;
class RedactTest : public QObject
{
	Q_OBJECT
      private slots:
	void secrets();
	void vault();
};
#ifndef Q_MOC_RUN
void RedactTest::secrets()
{
	Redactor r;
	r.addSecret("abc");
	QVERIFY(!r.redact("failure abc").contains("abc"));
	QVERIFY(!r.redact("Bearer long-token").contains("long-token"));
	auto j = Redactor::redactJson({{"nested", QJsonObject{{"ACCESS_TOKEN", "hidden"}}}});
	QVERIFY(!QJsonDocument(j).toJson().contains("hidden"));
}

void RedactTest::vault()
{
	MemoryCredentialStore v;
	QVERIFY(v.available());
	QVERIFY(v.set(creds::kStreamKey, "a", "secret"));
	QCOMPARE(v.get(creds::kStreamKey, "a"), QByteArray("secret"));
	QVERIFY(v.get(creds::kOAuthToken, "a").isEmpty());
	QVERIFY(v.remove(creds::kStreamKey, "a"));
	QVERIFY(!v.contains(creds::kStreamKey, "a"));
}
#endif

int runRedact(int argc, char **argv)
{
	RedactTest test;
	return QTest::qExec(&test, argc, argv);
}
#include "test-redact.moc"
