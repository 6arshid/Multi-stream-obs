/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/config-store.hpp"

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QTimer>

typedef struct obs_data obs_data_t;
struct obs_output;
struct obs_service;
struct obs_encoder;
struct calldata;
typedef struct obs_output obs_output_t;
typedef struct obs_service obs_service_t;
typedef struct obs_encoder obs_encoder_t;
typedef struct calldata calldata_t;

namespace ums {

enum class OutputState {
	Idle,
	Starting,
	Active,
	Reconnecting,
	Stopping,
	Stopped
};

/* Per-entry context for libobs signal callbacks (multi-output-engine.cpp). */
struct SignalCtx;

struct OutputStats {
	double kbps = 0.0;
	int droppedPercent = 0;
};

/* Per-destination lifecycle mirror of one rtmp_output. */
class MultiOutputEngine : public QObject {
	Q_OBJECT
public:
	explicit MultiOutputEngine(QObject *parent = nullptr);
	~MultiOutputEngine() override;

	void setStreamSettings(const StreamSettings &settings);

	/* Creates (or replaces) output + service for a destination. */
	bool prepare(const QString &destId, const QString &server, const QString &key,
	             const DestinationConfig &overrides);
	/* Drops any prepared/running output for a destination. */
	void discard(const QString &destId);

	bool start(const QString &destId);
	void stop(const QString &destId);
	void stopAll();

	bool isActive(const QString &destId) const;
	bool anyActive() const;
	QStringList activeIds() const;
	OutputState state(const QString &destId) const;

signals:
	void stateChanged(const QString &destId, ums::OutputState state, const QString &detail);
	void statsChanged(const QString &destId, const ums::OutputStats &stats);

	private:
	struct Entry {
		QString id;
		quint64 generation = 0;
		obs_output_t *output = nullptr;
		obs_service_t *service = nullptr;
		obs_encoder_t *ownVideo = nullptr;
		obs_encoder_t *ownAudio = nullptr;
		DestinationConfig overrides;
		SignalCtx *ctx = nullptr;
		bool usesPool = false;
		bool starting = false;
		bool userStopped = false;
		OutputState state = OutputState::Idle;
		uint64_t lastBytes = 0;
		qint64 lastStatsMs = 0;
		OutputStats lastStats;
	};

	/* OBS signal callbacks (worker threads) - marshal into the Qt thread. */
	static void cbStart(void *data, calldata_t *params);
	static void cbStop(void *data, calldata_t *params);
	static void cbReconnect(void *data, calldata_t *params);
	static void cbReconnectSuccess(void *data, calldata_t *params);

	void attachSignals(Entry &entry);
	void detachSignals(Entry &entry);
	void handleState(const QString &id, OutputState state, const QString &detail, quint64 generation);
	void pollStats();

	/* Encoder policy resolution for one destination. */
	void attachEncoders(Entry &entry, const DestinationConfig &overrides);
	obs_encoder_t *createVideoEncoder(const QString &encoderId, int bitrateKbps,
	                                  int keyframeSec, obs_data_t *baseSettings,
	                                  const QString &name);
	obs_encoder_t *createAudioEncoder(const QString &encoderId, int bitrateKbps,
	                                  obs_data_t *baseSettings, const QString &name);
	obs_encoder_t *poolVideo();
	obs_encoder_t *poolAudio();
	void releasePoolIfIdle();
	void destroyEntry(Entry &entry);

	StreamSettings settings_;
	QHash<QString, Entry> entries_;
	obs_encoder_t *poolVideo_ = nullptr;
	obs_encoder_t *poolAudio_ = nullptr;
	int poolUsers_ = 0;
	quint64 nextGeneration_ = 0;
	QTimer *statsTimer_ = nullptr;
};

} // namespace ums

Q_DECLARE_METATYPE(ums::OutputState)
Q_DECLARE_METATYPE(ums::OutputStats)
