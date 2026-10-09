/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <functional>

namespace ums {

struct HttpRequest {
	QString method = QStringLiteral("GET");
	QString url;
	QHash<QString, QString> headers;
	QByteArray body;
	int timeoutMs = 20000;
};

struct HttpResponse {
	bool ok = false;         /* transport succeeded (HTTP status may still be >= 400) */
	int status = 0;
	QByteArray body;
	QHash<QString, QString> headers;
	QString transportError;  /* non-empty when the request never completed */

	QString header(const QString &name) const
	{
		for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
			if (it.key().compare(name, Qt::CaseInsensitive) == 0)
				return it.value();
		}
		return {};
	}
};

using HttpCallback = std::function<void(const HttpResponse &)>;

/* Abstraction over the HTTPS transport so adapters and OAuth logic can be
 * unit tested with canned responses (no network, no credentials). */
class IHttpClient {
public:
	virtual ~IHttpClient() = default;
	virtual void send(const HttpRequest &request, HttpCallback callback) = 0;
};

/* Production transport backed by QNetworkAccessManager (Qt TLS backends). */
class QtHttpClient : public IHttpClient {
public:
	QtHttpClient();
	~QtHttpClient() override;
	QtHttpClient(const QtHttpClient &) = delete;
	QtHttpClient &operator=(const QtHttpClient &) = delete;

	void send(const HttpRequest &request, HttpCallback callback) override;

	/* Blocks the calling thread until the response arrives. Only used by
	 * non-UI contexts; UI code must use the async form. */
	HttpResponse sendBlocking(const HttpRequest &request, int timeoutMs = 0);

	static HttpRequest get(const QString &url, const QHash<QString, QString> &headers = {});
	static HttpRequest post(const QString &url, const QByteArray &body,
	                        const QHash<QString, QString> &headers = {});

private:
	struct Impl;
	Impl *impl_;
};

/* Helpers shared by adapters. */
QHash<QString, QString> bearerHeaders(const QString &token, const QString &clientId = {});
QByteArray formEncode(const QHash<QString, QString> &fields);

} // namespace ums
