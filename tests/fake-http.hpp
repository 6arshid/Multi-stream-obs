/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include "core/http-client.hpp"
#include <QVector>
class FakeHttpClient : public ums::IHttpClient
{
      public:
	QVector<ums::HttpResponse> responses;
	QVector<ums::HttpRequest> requests;
	void send(const ums::HttpRequest &request, ums::HttpCallback callback) override
	{
		requests.append(request);
		if (responses.isEmpty()) {
			callback({false, 0, {}, {}, "missing fake response"});
			return;
		}
		auto response = responses.takeFirst();
		callback(response);
	}
};
