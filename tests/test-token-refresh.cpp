/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/oauth-flow.hpp"
#include "fake-http.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrlQuery>
#include <QtTest>
using namespace ums;
class RefreshTest : public QObject
{
	Q_OBJECT
      private slots:
	void requestBodies();
	void asyncRefresh();
	void invalidGrant();
	void failedHttpCannotReplaceToken();
};
#ifndef Q_MOC_RUN
void RefreshTest::requestBodies()
{
	OAuthSpec spec;
	spec.tokenUrl = "https://example.test/token";
	OAuthClientConfig client{"id", "secret"};
	TokenSet token;
	token.refreshToken = "refresh+&";
	auto form = buildRefreshRequest(spec, client, token);
	QUrlQuery q(QString::fromUtf8(form.body));
	QCOMPARE(q.queryItemValue("grant_type"), QString("refresh_token"));
	QVERIFY(form.body.contains("refresh%2B%26"));
	spec.jsonTokenBody = true;
	spec.clientIdHeader = "Client-ID";
	spec.refreshUrl = "https://example.test/refresh";
	auto json = buildRefreshRequest(spec, client, token);
	QCOMPARE(json.url, spec.refreshUrl);
	QCOMPARE(json.headers.value("Client-ID"), QString("id"));
	auto body = QJsonDocument::fromJson(json.body).object();
	QCOMPARE(body.value("refresh_token").toString(), token.refreshToken);
	QVERIFY(!body.contains("client_id"));
}

void RefreshTest::asyncRefresh()
{
	FakeHttpClient http;
	http.responses.append({true, 200, R"({"access_token":"new","expires_in":3600})", {}, {}});
	TokenSet current;
	current.accessToken = "old";
	current.refreshToken = "keep";
	OAuthSpec spec;
	spec.tokenUrl = "https://example.test/token";
	bool called = false;
	refreshAccessToken(http, spec, {}, current, [&](const RefreshOutcome &outcome) {
		called = true;
		QVERIFY(outcome.ok);
	});
	QVERIFY(called);
	QCOMPARE(current.accessToken, QString("new"));
	QCOMPARE(current.refreshToken, QString("keep"));
}

void RefreshTest::invalidGrant()
{
	TokenSet current;
	current.accessToken = "old";
	auto result = applyTokenResponse({}, {{"error", "invalid_grant"}}, current, 100);
	QVERIFY(!result.ok);
	QVERIFY(result.unauthorized);
	QCOMPARE(current.accessToken, QString("old"));
}
void RefreshTest::failedHttpCannotReplaceToken()
{
	FakeHttpClient http;
	http.responses.append({true, 401, R"({"access_token":"unexpected"})", {}, {}});
	TokenSet token;
	token.accessToken = "original";
	token.refreshToken = "refresh";
	bool called = false;
	refreshAccessToken(http, {}, {}, token, [&](const RefreshOutcome &outcome) {
		called = true;
		QVERIFY(!outcome.ok);
		QVERIFY(outcome.unauthorized);
	});
	QVERIFY(called);
	QCOMPARE(token.accessToken, QString("original"));
}

#endif

int runRefresh(int argc, char **argv)
{
	RefreshTest test;
	return QTest::qExec(&test, argc, argv);
}
#include "test-token-refresh.moc"
