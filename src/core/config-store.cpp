/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/config-store.hpp"
#include "core/redact.hpp"
#include <QUrl>
#include <functional>

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>

namespace ums
{

/* --- AuthMode ---------------------------------------------------------- */

QString authModeToString(AuthMode mode)
{
	switch (mode) {
	case AuthMode::Api:
		return QStringLiteral("api");
	case AuthMode::Manual:
	default:
		return QStringLiteral("manual");
	}
}

AuthMode authModeFromString(const QString &value)
{
	return value == QLatin1String("api") ? AuthMode::Api : AuthMode::Manual;
}

/* --- DestinationConfig ------------------------------------------------- */

QJsonObject DestinationConfig::toJson() const
{
	QJsonObject object;
	object.insert(QStringLiteral("id"), id);
	object.insert(QStringLiteral("platform"), platform);
	object.insert(QStringLiteral("label"), label);
	object.insert(QStringLiteral("enabled"), enabled);
	object.insert(QStringLiteral("authMode"), authModeToString(authMode));
	object.insert(QStringLiteral("server"), server);
	object.insert(QStringLiteral("hasKey"), hasKey);
	object.insert(QStringLiteral("accountAlias"), accountAlias);
	object.insert(QStringLiteral("remoteId"), remoteId);
	if (!options.isEmpty())
		object.insert(QStringLiteral("options"), options);
	if (videoBitrateKbps > 0)
		object.insert(QStringLiteral("videoBitrateKbps"), videoBitrateKbps);
	if (audioBitrateKbps > 0)
		object.insert(QStringLiteral("audioBitrateKbps"), audioBitrateKbps);
	if (!encoderId.isEmpty())
		object.insert(QStringLiteral("encoderId"), encoderId);
	if (keyframeIntervalSec > 0)
		object.insert(QStringLiteral("keyframeIntervalSec"), keyframeIntervalSec);
	return object;
}

DestinationConfig DestinationConfig::fromJson(const QJsonObject &object)
{
	DestinationConfig destination;
	destination.id = object.value(QStringLiteral("id")).toString();
	destination.platform = object.value(QStringLiteral("platform")).toString();
	destination.label = object.value(QStringLiteral("label")).toString();
	destination.enabled = object.value(QStringLiteral("enabled")).toBool();
	destination.authMode =
	    authModeFromString(object.value(QStringLiteral("authMode")).toString());
	destination.server = object.value(QStringLiteral("server")).toString();
	destination.hasKey = object.value(QStringLiteral("hasKey")).toBool();
	destination.accountAlias = object.value(QStringLiteral("accountAlias")).toString();
	destination.remoteId = object.value(QStringLiteral("remoteId")).toString();
	destination.options = object.value(QStringLiteral("options")).toObject();
	destination.videoBitrateKbps = object.value(QStringLiteral("videoBitrateKbps")).toInt(0);
	destination.audioBitrateKbps = object.value(QStringLiteral("audioBitrateKbps")).toInt(0);
	destination.encoderId = object.value(QStringLiteral("encoderId")).toString();
	destination.keyframeIntervalSec =
	    object.value(QStringLiteral("keyframeIntervalSec")).toInt(2);
	return destination;
}

/* --- StreamSettings ---------------------------------------------------- */

QJsonObject StreamSettings::toJson() const
{
	QJsonObject object;
	object.insert(QStringLiteral("startWithMain"), startWithMain);
	object.insert(QStringLiteral("stopOthersOnFatal"), stopOthersOnFatal);
	object.insert(QStringLiteral("shareMainEncoder"), shareMainEncoder);
	object.insert(QStringLiteral("useSharedEncoderPool"), useSharedEncoderPool);
	object.insert(QStringLiteral("defaultVideoEncoder"), defaultVideoEncoder);
	object.insert(QStringLiteral("defaultAudioEncoder"), defaultAudioEncoder);
	object.insert(QStringLiteral("defaultVideoBitrateKbps"), defaultVideoBitrateKbps);
	object.insert(QStringLiteral("defaultAudioBitrateKbps"), defaultAudioBitrateKbps);
	object.insert(QStringLiteral("reconnectAttempts"), reconnectAttempts);
	object.insert(QStringLiteral("reconnectDelaySec"), reconnectDelaySec);
	object.insert(QStringLiteral("showNotifications"), showNotifications);
	return object;
}

StreamSettings StreamSettings::fromJson(const QJsonObject &object)
{
	StreamSettings settings;
	settings.startWithMain = object.value(QStringLiteral("startWithMain")).toBool(false);
	settings.stopOthersOnFatal =
	    object.value(QStringLiteral("stopOthersOnFatal")).toBool(false);
	settings.shareMainEncoder = object.value(QStringLiteral("shareMainEncoder")).toBool(true);
	settings.useSharedEncoderPool =
	    object.value(QStringLiteral("useSharedEncoderPool")).toBool(true);
	settings.defaultVideoEncoder = object.value(QStringLiteral("defaultVideoEncoder"))
					   .toString(QStringLiteral("obs_x264"));
	settings.defaultAudioEncoder = object.value(QStringLiteral("defaultAudioEncoder"))
					   .toString(QStringLiteral("ffmpeg_aac"));
	settings.defaultVideoBitrateKbps =
	    object.value(QStringLiteral("defaultVideoBitrateKbps")).toInt(2500);
	settings.defaultAudioBitrateKbps =
	    object.value(QStringLiteral("defaultAudioBitrateKbps")).toInt(160);
	settings.reconnectAttempts = object.value(QStringLiteral("reconnectAttempts")).toInt(10);
	settings.reconnectDelaySec = object.value(QStringLiteral("reconnectDelaySec")).toInt(2);
	settings.showNotifications = object.value(QStringLiteral("showNotifications")).toBool(true);
	return settings;
}

/* --- ConfigFile -------------------------------------------------------- */

DestinationConfig *ConfigFile::findDestination(const QString &id)
{
	for (DestinationConfig &destination : destinations) {
		if (destination.id == id)
			return &destination;
	}
	return nullptr;
}

const DestinationConfig *ConfigFile::findDestination(const QString &id) const
{
	for (const DestinationConfig &destination : destinations) {
		if (destination.id == id)
			return &destination;
	}
	return nullptr;
}

bool ConfigFile::removeDestination(const QString &id)
{
	for (int i = 0; i < destinations.size(); ++i) {
		if (destinations.at(i).id == id) {
			destinations.removeAt(i);
			return true;
		}
	}
	return false;
}

QString ConfigFile::generateDestinationId() const
{
	QString id;
	do {
		id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(12);
	} while (findDestination(id) != nullptr);
	return id;
}

QJsonObject ConfigFile::toJson() const
{
	QJsonObject object;
	object.insert(QStringLiteral("schemaVersion"), kConfigSchemaVersion);
	object.insert(QStringLiteral("settings"), settings.toJson());

	QJsonArray array;
	for (const DestinationConfig &destination : destinations)
		array.append(destination.toJson());
	object.insert(QStringLiteral("destinations"), array);

	QJsonObject accountsObject;
	for (auto it = accounts.constBegin(); it != accounts.constEnd(); ++it)
		accountsObject.insert(it.key(), it.value());
	object.insert(QStringLiteral("accounts"), accountsObject);
	return object;
}

ConfigFile ConfigFile::fromJson(const QJsonObject &object)
{
	ConfigFile file;
	file.schemaVersion = object.value(QStringLiteral("schemaVersion")).toInt();
	file.settings =
	    StreamSettings::fromJson(object.value(QStringLiteral("settings")).toObject());

	const QJsonArray array = object.value(QStringLiteral("destinations")).toArray();
	for (const QJsonValue &value : array) {
		DestinationConfig destination = DestinationConfig::fromJson(value.toObject());
		if (destination.isValid())
			file.destinations.append(destination);
	}

	const QJsonObject accountsObject = object.value(QStringLiteral("accounts")).toObject();
	for (auto it = accountsObject.constBegin(); it != accountsObject.constEnd(); ++it)
		file.accounts.insert(it.key(), it.value().toObject());
	return file;
}

/* --- ConfigStore ------------------------------------------------------- */

ConfigStore::ConfigStore(const QString &filePath) : path_(filePath)
{
	if (path_.isEmpty())
		path_ = QStringLiteral("multistream.json");
}

LoadResult ConfigStore::load()
{
	LoadResult result;

	QFile file(path_);
	if (!file.exists()) {
		config_ = ConfigFile();
		config_.schemaVersion = kSchemaVersion;
		return result;
	}
	if (!file.open(QIODevice::ReadOnly)) {
		result.ok = false;
		result.error = file.errorString();
		return result;
	}
	const QByteArray raw = file.readAll();
	file.close();

	QJsonParseError parseError;
	const QJsonDocument document = QJsonDocument::fromJson(raw, &parseError);
	if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
		/* Corrupt file: move it aside, keep a backup, start fresh. */
		const QString backup =
		    path_ + QStringLiteral(".bak-") +
		    QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
		QFile::remove(backup);
		QFile::rename(path_, backup);

		result.ok = true;
		result.recoveredFromCorruption = true;
		result.backupPath = backup;
		result.error = parseError.error == QJsonParseError::NoError
				   ? tr("configuration file did not contain a JSON object")
				   : parseError.errorString();
		config_ = ConfigFile();
		config_.schemaVersion = kSchemaVersion;
		save();
		return result;
	}

	QJsonObject root = document.object();
	const int version = root.value(QStringLiteral("schemaVersion")).toInt(1);
	if (version > kSchemaVersion) {
		result.ok = false;
		result.error = tr("configuration schema %1 is newer than supported version %2")
				   .arg(version)
				   .arg(kSchemaVersion);
		return result;
	}
	if (version < kSchemaVersion) {
		QString migrateError;
		if (!migrate(root, &migrateError)) {
			result.ok = false;
			result.error = migrateError;
			return result;
		}
	}

	config_ = ConfigFile::fromJson(root);
	config_.schemaVersion = kSchemaVersion;
	return result;
}

bool ConfigStore::save()
{
	if (path_.isEmpty())
		return false;

	QSaveFile file(path_);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;

	const QJsonDocument document(config_.toJson());
	file.write(document.toJson(QJsonDocument::Indented));
	return file.commit();
}

QJsonObject ConfigStore::exportableJson() const
{
	/* The config never contains secrets; export additionally drops account
	 * metadata so an exported file can be shared safely. */
	QJsonObject root = config_.toJson();
	root.remove(QStringLiteral("accounts"));
	QJsonArray array = root.value(QStringLiteral("destinations")).toArray();
	for (int i = 0; i < array.size(); ++i) {
		QJsonObject destination = array.at(i).toObject();
		destination.remove(QStringLiteral("hasKey"));
		destination.remove(QStringLiteral("accountAlias"));
		destination.remove(QStringLiteral("remoteId"));
		destination.remove(QStringLiteral("authMode"));
		destination.insert(QStringLiteral("authMode"), QStringLiteral("manual"));
		array.replace(i, destination);
	}
	root.insert(QStringLiteral("destinations"), array);
	return root;
}

bool ConfigStore::migrate(QJsonObject &root, QString *error)
{
	int version = root.value(QStringLiteral("schemaVersion")).toInt(1);
	if (version == kSchemaVersion)
		return true;
	if (version > kSchemaVersion) {
		if (error)
			*error = tr("configuration schema %1 is newer than supported version %2")
				     .arg(version)
				     .arg(kSchemaVersion);
		return false;
	}

	/* v1 -> v2: "streams" renamed to "destinations", settings block added. */
	if (version == 1) {
		if (root.contains(QStringLiteral("streams")) &&
		    !root.contains(QStringLiteral("destinations")))
			root.insert(QStringLiteral("destinations"),
				    root.take(QStringLiteral("streams")));
		if (!root.contains(QStringLiteral("settings")))
			root.insert(QStringLiteral("settings"), QJsonObject());
		version = 2;
	}

	/* v2 -> v3: explicit authMode per destination; keyMaterial removed. */
	if (version == 2) {
		QJsonArray array = root.value(QStringLiteral("destinations")).toArray();
		for (int i = 0; i < array.size(); ++i) {
			QJsonObject destination = array.at(i).toObject();
			destination.remove(QStringLiteral("keyMaterial"));
			destination.remove(QStringLiteral("key"));
			if (!destination.contains(QStringLiteral("authMode")))
				destination.insert(QStringLiteral("authMode"),
						   QStringLiteral("manual"));
			array.replace(i, destination);
		}
		root.insert(QStringLiteral("destinations"), array);
		version = 3;
	}

	if (version != kSchemaVersion) {
		if (error)
			*error = tr("no migration path from schema version %1").arg(version);
		return false;
	}
	root.insert(QStringLiteral("schemaVersion"), kSchemaVersion);
	return true;
}

QStringList ConfigStore::validate(const DestinationConfig &destination)
{
	QStringList errors;
	if (destination.platform.isEmpty())
		errors.append(tr("no platform selected"));

	if (destination.label.length() > 64)
		errors.append(tr("label is longer than 64 characters"));

	switch (destination.authMode) {
	case AuthMode::Manual: {
		/* Empty server means the platform default; a key must exist by
		 * the time the output starts (checked by the controller). */
		const QUrl server(destination.server.trimmed());
		if (!destination.server.trimmed().isEmpty() &&
		    (!server.isValid() || server.host().isEmpty() ||
		     (server.scheme() != "rtmp" && server.scheme() != "rtmps")))
			errors.append(tr("server must be an RTMP/RTMPS URL"));
		if (!server.userInfo().isEmpty() || !server.query().isEmpty())
			errors.append(tr("Keep ingest credentials in the stream key field"));
		break;
	}
	case AuthMode::Api:
		if (destination.accountAlias.isEmpty())
			errors.append(tr("no authenticated account selected"));
		break;
	}
	std::function<bool(const QJsonValue &)> hasSecret = [&](const QJsonValue &value) {
		if (value.isObject()) {
			auto object = value.toObject();
			for (auto it = object.begin(); it != object.end(); ++it)
				if (Redactor::isSensitiveKey(it.key()) || hasSecret(it.value()))
					return true;
		}
		if (value.isArray())
			for (const auto &item : value.toArray())
				if (hasSecret(item))
					return true;
		return false;
	};
	if (hasSecret(destination.options))
		errors.append(tr("Platform options cannot contain credentials"));
	return errors;
}

} // namespace ums
