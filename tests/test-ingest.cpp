/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/adapter-registry.hpp"
#include "fake-http.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>
using namespace ums;
class IngestTest : public QObject
{
	Q_OBJECT
      private slots:
	void manualVault();
	void apiHappyPaths();
	void facebookMalformedUrl();
	void twitchTitleAndHeaders();
};
#ifndef Q_MOC_RUN
void IngestTest::manualVault()
{
	MemoryCredentialStore vault;
	PlatformInfo info;
	info.defaultIngestUrl = "rtmps://example.test/app";
	AuthContext auth;
	auth.vault = &vault;
	DestinationConfig d;
	d.id = "a";
	QVERIFY(!resolveManualIngest(info, auth, d).ok());
	vault.set(creds::kStreamKey, "a", "key");
	auto endpoint = resolveManualIngest(info, auth, d);
	QVERIFY(endpoint.ok());
	QCOMPARE(endpoint.key, QString("key"));
	QCOMPARE(endpoint.server, info.defaultIngestUrl);
}

void IngestTest::apiHappyPaths()
{
	auto &registry = AdapterRegistry::instance();
	registry.ensureBuiltins();
	for (auto id : {"twitch", "facebook"}) {
		FakeHttpClient http;
		if (id == QString("twitch"))
			http.responses.append({true,
					       200,
					       R"({"data":[{"id":"123","display_name":"Test"}]})",
					       {},
					       {}});
		http.responses.append(
		    {true,
		     200,
		     id == QString("facebook")
			 ? QByteArray(
			       R"({"id":"video","stream_url":"rtmps://host:443/rtmp/session-key"})")
			 : QByteArray(R"({"data":[{"stream_key":"session-key"}]})"),
		     {},
		     {}});
		AuthContext auth;
		auth.http = &http;
		auth.token.accessToken = "token";
		DestinationConfig d;
		d.id = "a";
		d.platform = id;
		d.authMode = AuthMode::Api;
		bool called = false;
		const_cast<IPlatformAdapter *>(registry.get(id))
		    ->prepareIngest(auth, d, [&](const IngestEndpoint &endpoint) {
			    called = true;
			    QVERIFY(endpoint.ok());
			    QCOMPARE(endpoint.key, QString("session-key"));
			    if (d.platform == "facebook")
				    QCOMPARE(endpoint.server, QString("rtmps://host:443/rtmp/"));
		    });
		QVERIFY(called);
		QCOMPARE(http.requests.size(), id == QString("twitch") ? 2 : 1);
		if (id == QString("twitch"))
			QVERIFY(http.requests.last().url.contains("broadcaster_id=123"));
	}
}

void IngestTest::facebookMalformedUrl()
{
	auto &registry = AdapterRegistry::instance();
	registry.ensureBuiltins();
	FakeHttpClient http;
	http.responses.append({true, 200, R"({"stream_url":"rtmps://host/wrong/key"})", {}, {}});
	AuthContext auth;
	auth.http = &http;
	auth.token.accessToken = "token";
	DestinationConfig d;
	d.authMode = AuthMode::Api;
	bool called = false;
	const_cast<IPlatformAdapter *>(registry.get("facebook"))
	    ->prepareIngest(auth, d, [&](const IngestEndpoint &endpoint) {
		    called = true;
		    QVERIFY(!endpoint.ok());
	    });
	QVERIFY(called);
}
void IngestTest::twitchTitleAndHeaders()
{
	auto &registry = AdapterRegistry::instance();
	registry.ensureBuiltins();
	FakeHttpClient http;
	http.responses = {{true, 200, R"({"data":[{"id":"123","display_name":"Tester"}]})", {}, {}},
			  {true, 204, {}, {}, {}},
			  {true, 200, R"({"data":[{"stream_key":"live-key"}]})", {}, {}}};
	AuthContext auth;
	auth.http = &http;
	auth.token.accessToken = "token";
	auth.client.clientId = "client";
	DestinationConfig d;
	d.authMode = AuthMode::Api;
	d.options.insert("title", "New title");
	bool called = false;
	registry.get("twitch")->prepareIngest(auth, d, [&](const IngestEndpoint &endpoint) {
		called = true;
		QVERIFY(endpoint.ok());
	});
	QVERIFY(called);
	QCOMPARE(http.requests.size(), 3);
	QCOMPARE(http.requests[1].method, QString("PATCH"));
	QVERIFY(http.requests[1].url.contains("broadcaster_id=123"));
	QCOMPARE(QJsonDocument::fromJson(http.requests[1].body).object().value("title").toString(),
		 QString("New title"));
	QCOMPARE(http.requests[2].headers.value("Client-Id"), QString("client"));
}

#endif

int runIngest(int argc, char **argv)
{
	IngestTest test;
	return QTest::qExec(&test, argc, argv);
}
#include "test-ingest.moc"
