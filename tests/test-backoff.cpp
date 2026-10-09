/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/backoff.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>
using namespace ums;
class BackoffTest : public QObject
{
	Q_OBJECT
      private slots:
	void bounds();
	void jitter();
};
#ifndef Q_MOC_RUN
void BackoffTest::bounds()
{
	Backoff b(2, 20, 2.0, 0.0);
	QCOMPARE(b.next(0.5), 2);
	QCOMPARE(b.next(0.5), 4);
	QCOMPARE(b.delaySeconds(9, 0.5), 20);
	b.reset();
	QCOMPARE(b.attempt(), 0);
}

void BackoffTest::jitter()
{
	Backoff b(10, 100, 2, 0.2);
	QVERIFY(b.delaySeconds(1, 0) <= b.delaySeconds(1, 1));
	QVERIFY(b.delaySeconds(30, 1) <= 100);
}
#endif

int runBackoff(int argc, char **argv)
{
	BackoffTest test;
	return QTest::qExec(&test, argc, argv);
}
#include "test-backoff.moc"
