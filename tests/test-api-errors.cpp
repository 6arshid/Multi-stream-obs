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
class ApiErrorsTest : public QObject
{
	Q_OBJECT
      private slots:
	void unauthorizedAndTransport();
};
#ifndef Q_MOC_RUN
void ApiErrorsTest::unauthorizedAndTransport()
{
	auto &registry = AdapterRegistry::instance();
	registry.ensureBuiltins();
	for (auto id : {"twitch", "facebook"}) {
		auto *adapter = const_cast<IPlatformAdapter *>(registry.get(id));
		for (int status : {401, 500, 0}) {
			FakeHttpClient http;
			http.responses.append({status != 0,
					       status,
					       R"({"error":{"message":"failed"}})",
					       {},
					       status == 0 ? QString("offline") : QString()});
			AuthContext auth;
			auth.http = &http;
			auth.token.accessToken = "test-token";
			DestinationConfig d;
			d.id = "a";
			d.platform = id;
			d.authMode = AuthMode::Api;
			bool called = false;
			adapter->prepareIngest(auth, d, [&](const IngestEndpoint &endpoint) {
				called = true;
				QVERIFY(!endpoint.ok());
				QVERIFY(!endpoint.error.isEmpty());
			});
			QVERIFY(called);
		}
	}
}
#endif

int runApiErrors(int argc, char **argv)
{
	ApiErrorsTest test;
	return QTest::qExec(&test, argc, argv);
}
#include "test-api-errors.moc"
