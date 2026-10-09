/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "obs-utils.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>

namespace ums {
namespace obsutil {

obs_service_t *createRtmpService(const QString &name, const QString &server,
                                 const QString &key)
{
	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "server", server.toUtf8().constData());
	obs_data_set_string(settings, "key", key.toUtf8().constData());
	obs_service_t *service =
	    obs_service_create("rtmp_custom", name.toUtf8().constData(), settings, nullptr);
	obs_data_release(settings);
	return service;
}

QString outputStopText(int code)
{
	const char *key;
	switch (code) {
	case OBS_OUTPUT_SUCCESS:
		key = "stopped";
		break;
	case OBS_OUTPUT_BAD_PATH:
		key = "invalid server address";
		break;
	case OBS_OUTPUT_CONNECT_FAILED:
		key = "connection failed";
		break;
	case OBS_OUTPUT_INVALID_STREAM:
		key = "stream key rejected by the server";
		break;
	case OBS_OUTPUT_ERROR:
		key = "output error";
		break;
	case OBS_OUTPUT_DISCONNECTED:
		key = "disconnected";
		break;
	case OBS_OUTPUT_UNSUPPORTED:
		key = "unsupported configuration";
		break;
	case OBS_OUTPUT_NO_SPACE:
		key = "disk full";
		break;
	case OBS_OUTPUT_ENCODE_ERROR:
		key = "encoder error";
		break;
	case OBS_OUTPUT_HDR_DISABLED:
		key = "HDR is disabled";
		break;
	default:
		return QStringLiteral("stopped (code %1)").arg(code);
	}
	return QCoreApplication::translate("OutputState", key);
}

obs_output_t *mainStreamingOutput()
{
	return obs_frontend_get_streaming_output();
}

obs_encoder_t *mainVideoEncoder()
{
	obs_output_t *output = mainStreamingOutput();
	obs_encoder_t *encoder = output ? obs_output_get_video_encoder2(output, 0) : nullptr;
	if (output) obs_output_release(output);
	return encoder;
}

obs_encoder_t *mainAudioEncoder()
{
	obs_output_t *output = mainStreamingOutput();
	obs_encoder_t *encoder = output ? obs_output_get_audio_encoder(output, 0) : nullptr;
	if (output) obs_output_release(output);
	return encoder;
}

obs_data_t *mainVideoEncoderSettings()
{
	obs_encoder_t *encoder = mainVideoEncoder();
	if (!encoder)
		return nullptr;
	obs_data_t *settings = obs_encoder_get_settings(encoder);
	return settings; /* owned by caller */
}

CanvasInfo canvasInfo()
{
	CanvasInfo info;
	obs_video_info ovi = {};
	if (obs_get_video_info(&ovi)) {
		info.fpsNum = ovi.fps_num;
		info.fpsDen = ovi.fps_den;
		info.width = ovi.output_width;
		info.height = ovi.output_height;
	}
	return info;
}

QString moduleConfigFilePath(const QString &fileName)
{
	char *raw = obs_module_config_path(fileName.toUtf8().constData());
	if (!raw)
		return {};
	QString path = QString::fromUtf8(raw);
	bfree(raw);
	if (path.isEmpty())
		return {};
	QFileInfo info(path);
	QDir().mkpath(info.absolutePath());
	return path;
}

} // namespace obsutil
} // namespace ums
