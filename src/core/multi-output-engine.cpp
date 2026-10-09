/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "multi-output-engine.hpp"

#include "core/obs-utils.hpp"

#include <callback/calldata.h>
#include <callback/signal.h>
#include <obs.h>

#include <QDateTime>

namespace ums
{

/* Per-entry context handed to libobs signal callbacks. Output signals fire
 * on worker threads; the context carries only stable pointers/ids. */
struct SignalCtx {
	MultiOutputEngine *engine = nullptr;
	QString id;
	quint64 generation = 0;
};

MultiOutputEngine::MultiOutputEngine(QObject *parent) : QObject(parent)
{
	qRegisterMetaType<OutputState>("ums::OutputState");
	qRegisterMetaType<OutputStats>("ums::OutputStats");

	statsTimer_ = new QTimer(this);
	statsTimer_->setInterval(1000);
	connect(statsTimer_, &QTimer::timeout, this, &MultiOutputEngine::pollStats);
	statsTimer_->start();
}

MultiOutputEngine::~MultiOutputEngine()
{
	statsTimer_->stop();
	for (auto it = entries_.begin(); it != entries_.end(); ++it)
		destroyEntry(it.value());
	entries_.clear();
	releasePoolIfIdle();
}

void MultiOutputEngine::setStreamSettings(const StreamSettings &settings) { settings_ = settings; }

bool MultiOutputEngine::prepare(const QString &destId, const QString &server, const QString &key,
				const DestinationConfig &overrides)
{
	if (destId.isEmpty() || server.isEmpty())
		return false;

	auto existing = entries_.find(destId);
	if (existing != entries_.end())
		destroyEntry(existing.value());

	Entry entry;
	entry.id = destId;
	entry.generation = ++nextGeneration_;
	entry.overrides = overrides;

	const QString name = QStringLiteral("ums-%1").arg(destId);
	entry.output =
	    obs_output_create("rtmp_output", name.toUtf8().constData(), nullptr, nullptr);
	if (!entry.output) {
		blog(LOG_ERROR, "[ums] failed to create output for destination %s",
		     destId.toUtf8().constData());
		return false;
	}

	entry.service = obsutil::createRtmpService(name, server, key);
	if (!entry.service) {
		obs_output_release(entry.output);
		blog(LOG_ERROR, "[ums] failed to create service for destination %s",
		     destId.toUtf8().constData());
		return false;
	}
	obs_output_set_service(entry.output, entry.service);
	obs_output_set_reconnect_settings(entry.output, settings_.reconnectAttempts,
					  settings_.reconnectDelaySec);

	entry.ctx = new SignalCtx{this, destId, entry.generation};
	attachSignals(entry);

	entries_.insert(destId, entry);
	return true;
}

void MultiOutputEngine::discard(const QString &destId)
{
	auto it = entries_.find(destId);
	if (it == entries_.end())
		return;
	destroyEntry(it.value());
	entries_.remove(destId);
}

bool MultiOutputEngine::start(const QString &destId)
{
	auto it = entries_.find(destId);
	if (it == entries_.end())
		return false;
	Entry &entry = it.value();
	if (entry.state == OutputState::Starting || entry.state == OutputState::Active ||
	    entry.state == OutputState::Reconnecting || entry.state == OutputState::Stopping)
		return false;

	attachEncoders(entry, entry.overrides);
	entry.userStopped = false;
	entry.starting = true;
	entry.lastBytes = 0;
	entry.lastStatsMs = 0;
	entry.lastStats = OutputStats();
	entry.state = OutputState::Starting;
	emit stateChanged(destId, OutputState::Starting, {});

	const bool ok = obs_output_start(entry.output);
	if (!ok) {
		entry.starting = false;
		entry.state = OutputState::Stopped;
		const char *lastError = obs_output_get_last_error(entry.output);
		QString detail = obsutil::outputStopText(OBS_OUTPUT_CONNECT_FAILED);
		if (lastError && *lastError)
			detail += QStringLiteral(": ") + QString::fromUtf8(lastError);
		emit stateChanged(destId, OutputState::Stopped, detail);
	}
	return ok;
}

void MultiOutputEngine::stop(const QString &destId)
{
	auto it = entries_.find(destId);
	if (it == entries_.end())
		return;
	Entry &entry = it.value();
	if (entry.state != OutputState::Starting && entry.state != OutputState::Active &&
	    entry.state != OutputState::Reconnecting)
		return;
	entry.userStopped = true;
	entry.state = OutputState::Stopping;
	emit stateChanged(destId, OutputState::Stopping, {});
	obs_output_stop(entry.output);
}

void MultiOutputEngine::stopAll()
{
	QStringList ids = entries_.keys();
	for (const QString &id : ids)
		stop(id);
}

bool MultiOutputEngine::isActive(const QString &destId) const
{
	const auto it = entries_.constFind(destId);
	if (it == entries_.constEnd())
		return false;
	return it->state == OutputState::Active || it->state == OutputState::Starting ||
	       it->state == OutputState::Reconnecting || it->state == OutputState::Stopping;
}

bool MultiOutputEngine::anyActive() const
{
	for (const auto &entry : entries_) {
		if (entry.state == OutputState::Active || entry.state == OutputState::Starting ||
		    entry.state == OutputState::Reconnecting ||
		    entry.state == OutputState::Stopping)
			return true;
	}
	return false;
}

QStringList MultiOutputEngine::activeIds() const
{
	QStringList ids;
	for (const auto &entry : entries_) {
		if (entry.state == OutputState::Active || entry.state == OutputState::Starting ||
		    entry.state == OutputState::Reconnecting ||
		    entry.state == OutputState::Stopping)
			ids.append(entry.id);
	}
	return ids;
}

OutputState MultiOutputEngine::state(const QString &destId) const
{
	const auto it = entries_.constFind(destId);
	return it == entries_.constEnd() ? OutputState::Idle : it->state;
}

void MultiOutputEngine::attachSignals(Entry &entry)
{
	signal_handler_t *handler = obs_output_get_signal_handler(entry.output);
	signal_handler_connect(handler, "start", cbStart, entry.ctx);
	signal_handler_connect(handler, "stop", cbStop, entry.ctx);
	signal_handler_connect(handler, "reconnect", cbReconnect, entry.ctx);
	signal_handler_connect(handler, "reconnect_success", cbReconnectSuccess, entry.ctx);
}

void MultiOutputEngine::detachSignals(Entry &entry)
{
	if (!entry.ctx || !entry.output)
		return;
	signal_handler_t *handler = obs_output_get_signal_handler(entry.output);
	signal_handler_disconnect(handler, "start", cbStart, entry.ctx);
	signal_handler_disconnect(handler, "stop", cbStop, entry.ctx);
	signal_handler_disconnect(handler, "reconnect", cbReconnect, entry.ctx);
	signal_handler_disconnect(handler, "reconnect_success", cbReconnectSuccess, entry.ctx);
}

void MultiOutputEngine::cbStart(void *data, calldata_t *)
{
	auto *ctx = static_cast<SignalCtx *>(data);
	MultiOutputEngine *engine = ctx->engine;
	const QString id = ctx->id;
	const quint64 generation = ctx->generation;
	QMetaObject::invokeMethod(
	    engine,
	    [engine, id, generation] {
		    engine->handleState(id, OutputState::Active, {}, generation);
	    },
	    Qt::QueuedConnection);
}

void MultiOutputEngine::cbStop(void *data, calldata_t *params)
{
	auto *ctx = static_cast<SignalCtx *>(data);
	MultiOutputEngine *engine = ctx->engine;
	const QString id = ctx->id;
	const quint64 generation = ctx->generation;
	const int code = static_cast<int>(calldata_int(params, "code"));
	const char *rawError = calldata_string(params, "last_error");
	QString detail;
	if (code != OBS_OUTPUT_SUCCESS) {
		detail = obsutil::outputStopText(code);
		if (rawError && *rawError)
			detail += QStringLiteral(": ") + QString::fromUtf8(rawError);
	}
	QMetaObject::invokeMethod(
	    engine,
	    [engine, id, detail, generation] {
		    engine->handleState(id, OutputState::Stopped, detail, generation);
	    },
	    Qt::QueuedConnection);
}

void MultiOutputEngine::cbReconnect(void *data, calldata_t *)
{
	auto *ctx = static_cast<SignalCtx *>(data);
	MultiOutputEngine *engine = ctx->engine;
	const QString id = ctx->id;
	const quint64 generation = ctx->generation;
	QMetaObject::invokeMethod(
	    engine,
	    [engine, id, generation] {
		    engine->handleState(id, OutputState::Reconnecting, {}, generation);
	    },
	    Qt::QueuedConnection);
}

void MultiOutputEngine::cbReconnectSuccess(void *data, calldata_t *)
{
	auto *ctx = static_cast<SignalCtx *>(data);
	MultiOutputEngine *engine = ctx->engine;
	const QString id = ctx->id;
	const quint64 generation = ctx->generation;
	QMetaObject::invokeMethod(
	    engine,
	    [engine, id, generation] {
		    engine->handleState(id, OutputState::Active, {}, generation);
	    },
	    Qt::QueuedConnection);
}

void MultiOutputEngine::handleState(const QString &id, OutputState state, const QString &detail,
				    quint64 generation)
{
	auto it = entries_.find(id);
	if (it == entries_.end() || it->generation != generation)
		return;
	Entry &entry = it.value();
	if (state == OutputState::Stopped) {
		entry.starting = false;
		entry.userStopped = false;
	}
	entry.state = state;
	emit stateChanged(id, state, detail);
}

void MultiOutputEngine::pollStats()
{
	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	for (auto it = entries_.begin(); it != entries_.end(); ++it) {
		Entry &entry = it.value();
		if (entry.state != OutputState::Active && entry.state != OutputState::Reconnecting)
			continue;
		if (!entry.output)
			continue;

		const uint64_t bytes = obs_output_get_total_bytes(entry.output);
		const int totalFrames = obs_output_get_total_frames(entry.output);
		const int droppedFrames = obs_output_get_frames_dropped(entry.output);

		OutputStats stats;
		stats.droppedPercent =
		    totalFrames > 0 ? static_cast<int>((droppedFrames * 100) / totalFrames) : 0;

		if (entry.lastStatsMs != 0 && bytes >= entry.lastBytes) {
			const double dtSec = (now - entry.lastStatsMs) / 1000.0;
			if (dtSec >= 0.4) {
				stats.kbps = static_cast<double>(bytes - entry.lastBytes) * 8.0 /
					     1000.0 / dtSec;
			} else {
				stats.kbps = entry.lastStats.kbps;
			}
		}
		entry.lastBytes = bytes;
		entry.lastStatsMs = now;

		if (stats.kbps != entry.lastStats.kbps ||
		    stats.droppedPercent != entry.lastStats.droppedPercent) {
			entry.lastStats = stats;
			emit statsChanged(entry.id, stats);
		}
	}
}

obs_encoder_t *MultiOutputEngine::createVideoEncoder(const QString &encoderId, int bitrateKbps,
						     int keyframeSec, obs_data_t *baseSettings,
						     const QString &name)
{
	obs_data_t *settings = obs_data_create();
	if (baseSettings)
		obs_data_apply(settings, baseSettings);
	obs_data_set_int(settings, "bitrate", bitrateKbps);
	obs_data_set_int(settings, "keyint_sec", keyframeSec);
	obs_data_set_string(settings, "rate_control", "CBR");

	obs_encoder_t *encoder = obs_video_encoder_create(
	    encoderId.toUtf8().constData(), name.toUtf8().constData(), settings, nullptr);
	obs_data_release(settings);
	if (!encoder) {
		blog(LOG_WARNING, "[ums] could not create video encoder '%s'",
		     encoderId.toUtf8().constData());
		return nullptr;
	}
	obs_encoder_set_video(encoder, obs_get_video());
	return encoder;
}

obs_encoder_t *MultiOutputEngine::createAudioEncoder(const QString &encoderId, int bitrateKbps,
						     obs_data_t *baseSettings, const QString &name)
{
	obs_data_t *settings = obs_data_create();
	if (baseSettings)
		obs_data_apply(settings, baseSettings);
	obs_data_set_int(settings, "bitrate", bitrateKbps);

	obs_encoder_t *encoder = obs_audio_encoder_create(
	    encoderId.toUtf8().constData(), name.toUtf8().constData(), settings, 0, nullptr);
	obs_data_release(settings);
	if (!encoder) {
		blog(LOG_WARNING, "[ums] could not create audio encoder '%s'",
		     encoderId.toUtf8().constData());
		return nullptr;
	}
	obs_encoder_set_audio(encoder, obs_get_audio());
	return encoder;
}

obs_encoder_t *MultiOutputEngine::poolVideo()
{
	if (poolVideo_)
		return poolVideo_;
	obs_data_t *base = obsutil::mainVideoEncoderSettings();
	poolVideo_ =
	    createVideoEncoder(settings_.defaultVideoEncoder, settings_.defaultVideoBitrateKbps, 2,
			       base, QStringLiteral("ums-pool-video"));
	if (base)
		obs_data_release(base);
	return poolVideo_;
}

obs_encoder_t *MultiOutputEngine::poolAudio()
{
	if (poolAudio_)
		return poolAudio_;
	poolAudio_ =
	    createAudioEncoder(settings_.defaultAudioEncoder, settings_.defaultAudioBitrateKbps,
			       nullptr, QStringLiteral("ums-pool-audio"));
	return poolAudio_;
}

void MultiOutputEngine::releasePoolIfIdle()
{
	if (poolUsers_ > 0)
		return;
	if (poolVideo_) {
		obs_encoder_release(poolVideo_);
		poolVideo_ = nullptr;
	}
	if (poolAudio_) {
		obs_encoder_release(poolAudio_);
		poolAudio_ = nullptr;
	}
}

void MultiOutputEngine::attachEncoders(Entry &entry, const DestinationConfig &overrides)
{
	if (!entry.output)
		return;

	/* Detach whatever was attached before. */
	obs_output_set_video_encoder(entry.output, nullptr);
	obs_output_set_audio_encoder(entry.output, nullptr, 0);
	if (entry.usesPool) {
		entry.usesPool = false;
		poolUsers_--;
		releasePoolIfIdle();
	}
	if (entry.ownVideo) {
		obs_encoder_release(entry.ownVideo);
		entry.ownVideo = nullptr;
	}
	if (entry.ownAudio) {
		obs_encoder_release(entry.ownAudio);
		entry.ownAudio = nullptr;
	}

	const bool hasOverride = !overrides.encoderId.isEmpty() || overrides.videoBitrateKbps > 0 ||
				 overrides.audioBitrateKbps > 0;
	const bool canShare = settings_.shareMainEncoder && obsutil::mainVideoEncoder() &&
			      obsutil::mainAudioEncoder();

	if (!hasOverride && canShare) {
		obs_output_set_video_encoder(entry.output, obsutil::mainVideoEncoder());
		obs_output_set_audio_encoder(entry.output, obsutil::mainAudioEncoder(), 0);
		return;
	}

	if (!hasOverride && settings_.useSharedEncoderPool) {
		obs_encoder_t *video = poolVideo();
		obs_encoder_t *audio = poolAudio();
		if (video && audio) {
			obs_output_set_video_encoder(entry.output, video);
			obs_output_set_audio_encoder(entry.output, audio, 0);
			entry.usesPool = true;
			poolUsers_++;
			return;
		}
	}

	/* Per-destination encoders (overrides or no pool available). */
	obs_data_t *base = obsutil::mainVideoEncoderSettings();
	const QString videoId =
	    overrides.encoderId.isEmpty() ? settings_.defaultVideoEncoder : overrides.encoderId;
	const int videoBitrate = overrides.videoBitrateKbps > 0 ? overrides.videoBitrateKbps
								: settings_.defaultVideoBitrateKbps;
	const int keyframe = overrides.keyframeIntervalSec > 0 ? overrides.keyframeIntervalSec : 2;
	entry.ownVideo = createVideoEncoder(videoId, videoBitrate, keyframe, base,
					    QStringLiteral("ums-%1-video").arg(entry.id));
	if (base)
		obs_data_release(base);

	const int audioBitrate = overrides.audioBitrateKbps > 0 ? overrides.audioBitrateKbps
								: settings_.defaultAudioBitrateKbps;
	entry.ownAudio = createAudioEncoder(settings_.defaultAudioEncoder, audioBitrate, nullptr,
					    QStringLiteral("ums-%1-audio").arg(entry.id));

	if (entry.ownVideo)
		obs_output_set_video_encoder(entry.output, entry.ownVideo);
	if (entry.ownAudio)
		obs_output_set_audio_encoder(entry.output, entry.ownAudio, 0);
}

void MultiOutputEngine::destroyEntry(Entry &entry)
{
	const QString id = entry.id;
	detachSignals(entry);
	delete entry.ctx;
	entry.ctx = nullptr;

	if (entry.output) {
		obs_output_release(entry.output);
		entry.output = nullptr;
	}
	if (entry.service) {
		obs_service_release(entry.service);
		entry.service = nullptr;
	}
	if (entry.usesPool) {
		entry.usesPool = false;
		poolUsers_--;
	}
	if (entry.ownVideo) {
		obs_encoder_release(entry.ownVideo);
		entry.ownVideo = nullptr;
	}
	if (entry.ownAudio) {
		obs_encoder_release(entry.ownAudio);
		entry.ownAudio = nullptr;
	}
	releasePoolIfIdle();

	if (entry.state != OutputState::Idle && entry.state != OutputState::Stopped)
		emit stateChanged(id, OutputState::Stopped, {});
	entry.state = OutputState::Idle;
}

} // namespace ums
