/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QCoreApplication>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace ums {

/* Bump when the JSON schema changes; keep migration steps in sync. */
inline constexpr int kConfigSchemaVersion = 3;

/* How ingest credentials are obtained for a destination. */
enum class AuthMode {
	Manual, /* user pastes URL + stream key */
	Api,    /* adapter resolves the key (and possibly server) via platform API */
};

QString authModeToString(AuthMode mode);
AuthMode authModeFromString(const QString &value);

struct DestinationConfig {
	QString id;      /* uuid, also the credential-vault reference */
	QString platform; /* platform id, e.g. "youtube" */
	QString label;
	bool enabled = false;
	AuthMode authMode = AuthMode::Manual;

	/* Manual mode: ingest server ("rtmps://..." or "rtmp://..."; empty = platform default). */
	QString server;
	/* True when a stream key exists in the credential vault under this destination id. */
	bool hasKey = false;

	/* API mode: which authenticated account to use and the remote resource. */
	QString accountAlias;
	QString remoteId; /* broadcast / live-video / channel resource id */
	/* Adapter-specific non-secret options (page id, channel id, ...). */
	QJsonObject options;

	/* Encoder overrides; 0 / empty = inherit from the shared encoder policy. */
	int videoBitrateKbps = 0;
	int audioBitrateKbps = 0;
	QString encoderId;
	int keyframeIntervalSec = 2;

	bool isValid() const { return !id.isEmpty() && !platform.isEmpty(); }
	QJsonObject toJson() const;
	static DestinationConfig fromJson(const QJsonObject &object);
};

struct StreamSettings {
	bool startWithMain = false;        /* begin all destinations when OBS starts streaming */
	bool stopOthersOnFatal = false;    /* stop remaining outputs when one fails fatally */
	bool shareMainEncoder = true;      /* reuse OBS main-output encoders when available */
	bool useSharedEncoderPool = true;  /* otherwise build one shared encoder pair */
	QString defaultVideoEncoder = QStringLiteral("obs_x264");
	QString defaultAudioEncoder = QStringLiteral("ffmpeg_aac");
	int defaultVideoBitrateKbps = 2500;
	int defaultAudioBitrateKbps = 160;
	int reconnectAttempts = 10;        /* per destination (0 disables auto-reconnect) */
	int reconnectDelaySec = 2;
	bool showNotifications = true;

	QJsonObject toJson() const;
	static StreamSettings fromJson(const QJsonObject &object);
};

struct ConfigFile {
	int schemaVersion = 0;
	StreamSettings settings;
	QVector<DestinationConfig> destinations;
	/* Non-secret platform account metadata (account ids, channel ids). */
	QHash<QString, QJsonObject> accounts;

	DestinationConfig *findDestination(const QString &id);
	const DestinationConfig *findDestination(const QString &id) const;
	bool removeDestination(const QString &id);
	QString generateDestinationId() const;

	QJsonObject toJson() const;
	static ConfigFile fromJson(const QJsonObject &object);
};

struct LoadResult {
	bool ok = true;
	bool recoveredFromCorruption = false;
	QString backupPath;
	QString error;
};

/* Versioned JSON configuration stored in the OBS module config directory.
 * Secrets (stream keys, OAuth tokens) are NEVER part of this file - they
 * live in the OS credential vault, referenced by destination id / alias. */
class ConfigStore {
public:
	static constexpr int kSchemaVersion = kConfigSchemaVersion;
	Q_DECLARE_TR_FUNCTIONS(ConfigStore)

public:
	explicit ConfigStore(const QString &filePath);

	const QString &filePath() const { return path_; }
	/* Loads the file; a corrupt file is renamed aside and defaults are used. */
	LoadResult load();
	/* Atomic save (QSaveFile). Returns false on I/O failure. */
	bool save();

	ConfigFile &config() { return config_; }
	const ConfigFile &config() const { return config_; }

	/* Serializes with volatile bookkeeping stripped - for export. */
	QJsonObject exportableJson() const;

	/* Migration entry point (exposed for tests). Applies steps up to
	 * kSchemaVersion. Returns false when the version is unknown/newer. */
	static bool migrate(QJsonObject &root, QString *error);

	/* Destination sanity checks used by the UI and the controller. */
	static QStringList validate(const DestinationConfig &destination);

private:
	QString path_;
	ConfigFile config_;
};

} // namespace ums
