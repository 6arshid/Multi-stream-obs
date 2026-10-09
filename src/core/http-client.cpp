/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/http-client.hpp"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace ums
{

struct QtHttpClient::Impl {
	QNetworkAccessManager nam;
};

QtHttpClient::QtHttpClient() : impl_(new Impl)
{
	impl_->nam.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

QtHttpClient::~QtHttpClient() { delete impl_; }

static void finishRequest(QNetworkReply *reply, const HttpCallback &callback)
{
	HttpResponse response;
	response.ok = (reply->error() == QNetworkReply::NoError ||
		       reply->error() == QNetworkReply::ContentAccessDenied ||
		       reply->error() == QNetworkReply::ContentOperationNotPermittedError ||
		       reply->error() == QNetworkReply::ContentConflictError ||
		       reply->error() == QNetworkReply::ContentGoneError ||
		       reply->error() == QNetworkReply::UnknownContentError ||
		       reply->error() == QNetworkReply::ProtocolInvalidOperationError);
	/* QNetworkReply reports HTTP-level failures as ContentReceived errors;
	 * status code remains the authoritative signal for API error mapping. */
	response.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	response.body = reply->readAll();
	const QList<QByteArray> headerNames = reply->rawHeaderList();
	for (const QByteArray &name : headerNames)
		response.headers.insert(QString::fromUtf8(name),
					QString::fromUtf8(reply->rawHeader(name)));

	if (reply->error() != QNetworkReply::NoError && response.status == 0) {
		response.ok = false;
		response.transportError = reply->errorString();
	} else if (response.status >= 400) {
		response.ok = true; /* HTTP response received; adapters map status codes */
	} else {
		response.ok = (reply->error() == QNetworkReply::NoError);
		if (!response.ok)
			response.transportError = reply->errorString();
	}

	reply->deleteLater();
	callback(response);
}

void QtHttpClient::send(const HttpRequest &request, HttpCallback callback)
{
	QUrl url(request.url);
	if (!url.isValid()) {
		HttpResponse response;
		response.transportError = QStringLiteral("invalid URL: %1").arg(request.url);
		callback(response);
		return;
	}

	QNetworkRequest netReq(url);
	netReq.setTransferTimeout(request.timeoutMs);
	for (auto it = request.headers.constBegin(); it != request.headers.constEnd(); ++it)
		netReq.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
	if (!request.headers.contains(QStringLiteral("Content-Type")) &&
	    !request.headers.contains(QStringLiteral("content-type")))
		netReq.setHeader(QNetworkRequest::ContentTypeHeader,
				 QStringLiteral("application/x-www-form-urlencoded"));

	QNetworkReply *reply = nullptr;
	if (request.method == QLatin1String("GET")) {
		reply = impl_->nam.get(netReq);
	} else if (request.method == QLatin1String("POST")) {
		reply = impl_->nam.post(netReq, request.body);
	} else if (request.method == QLatin1String("PUT")) {
		reply = impl_->nam.put(netReq, request.body);
	} else if (request.method == QLatin1String("DELETE")) {
		reply = impl_->nam.deleteResource(netReq);
	} else if (request.method == QLatin1String("PATCH")) {
		reply =
		    impl_->nam.sendCustomRequest(netReq, QByteArrayLiteral("PATCH"), request.body);
	} else {
		reply = impl_->nam.sendCustomRequest(netReq, request.method.toUtf8(), request.body);
	}

	QObject::connect(reply, &QNetworkReply::finished, reply,
			 [reply, callback]() { finishRequest(reply, callback); });
}

HttpResponse QtHttpClient::sendBlocking(const HttpRequest &request, int timeoutMs)
{
	HttpResponse result;
	bool done = false;
	send(request, [&](const HttpResponse &response) {
		result = response;
		done = true;
	});

	if (!done) {
		QEventLoop loop;
		QTimer timer;
		timer.setSingleShot(true);
		QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
		timer.start(timeoutMs > 0 ? timeoutMs : request.timeoutMs + 1000);
		while (!done)
			loop.exec(QEventLoop::AllEvents | QEventLoop::ExcludeUserInputEvents);
	}
	return result;
}

HttpRequest QtHttpClient::get(const QString &url, const QHash<QString, QString> &headers)
{
	HttpRequest request;
	request.method = QStringLiteral("GET");
	request.url = url;
	request.headers = headers;
	return request;
}

HttpRequest QtHttpClient::post(const QString &url, const QByteArray &body,
			       const QHash<QString, QString> &headers)
{
	HttpRequest request;
	request.method = QStringLiteral("POST");
	request.url = url;
	request.body = body;
	request.headers = headers;
	return request;
}

QHash<QString, QString> bearerHeaders(const QString &token, const QString &clientId)
{
	QHash<QString, QString> headers;
	headers.insert(QStringLiteral("Authorization"), QStringLiteral("Bearer %1").arg(token));
	if (!clientId.isEmpty())
		headers.insert(QStringLiteral("Client-Id"), clientId);
	return headers;
}

QByteArray formEncode(const QHash<QString, QString> &fields)
{
	QByteArray encoded;
	for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
		if (!encoded.isEmpty())
			encoded += '&';
		encoded +=
		    QUrl::toPercentEncoding(it.key()) + '=' + QUrl::toPercentEncoding(it.value());
	}
	return encoded;
}

} // namespace ums
