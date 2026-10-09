/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/oauth-flow.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>
using namespace ums;
class OAuthTest : public QObject
{
	Q_OBJECT
      private slots:
	void pkceVector();
	void tokenVariants();
};
#ifndef Q_MOC_RUN
void OAuthTest::pkceVector()
{
	QCOMPARE(pkceChallengeS256("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"),
		 QString("E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM"));
	QVERIFY(generateCodeVerifier().size() >= 43);
	QVERIFY(generateState() != generateState());
}

void OAuthTest::tokenVariants()
{
	auto twitch = parseTokenResponse(
	    {{"access_token", "a"}, {"expires_in", 3600}, {"scope", QJsonArray{"one", "two"}}},
	    100);
	QCOMPARE(twitch.scope.size(), 2);
	QCOMPARE(twitch.expiresAtEpochSec, qint64(3700));
	auto trovo = parseTokenResponse(
	    {{"access_token", "b"}, {"expires_in", "1800"}, {"scope", "one two"}}, 100);
	QCOMPARE(trovo.expiresAtEpochSec, qint64(1900));
	QCOMPARE(trovo.scope.size(), 2);
	QCOMPARE(TokenSet::fromJson(trovo.toJson()).accessToken, QString("b"));
	QVERIFY(trovo.isExpired(1900));
}
#endif

int runOAuth(int argc, char **argv)
{
	OAuthTest test;
	return QTest::qExec(&test, argc, argv);
}
#include "test-oauth.moc"
