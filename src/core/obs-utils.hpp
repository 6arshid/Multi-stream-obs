/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

#include <obs.h>
#include <obs-data.h>

namespace ums {
namespace obsutil {

struct CanvasInfo {
	uint32_t fpsNum = 0;
	uint32_t fpsDen = 1;
	uint32_t width = 0;
	uint32_t height = 0;
};

/* Creates an "rtmp_custom" service carrying the destination server/key. */
obs_service_t *createRtmpService(const QString &name, const QString &server,
                                 const QString &key);

/* Human-readable text for obs output stop codes (obs-defs.h). */
QString outputStopText(int code);

/* The streaming output is owned by the caller; encoder pointers are borrowed. */
obs_output_t *mainStreamingOutput();
obs_encoder_t *mainVideoEncoder();
obs_encoder_t *mainAudioEncoder();
/* Owned copy of the main video encoder settings (caller releases), or null. */
obs_data_t *mainVideoEncoderSettings();

CanvasInfo canvasInfo();

/* Module config file path with parent directories created. */
QString moduleConfigFilePath(const QString &fileName);

} // namespace obsutil
} // namespace ums
