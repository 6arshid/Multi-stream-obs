/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

namespace ums {

/* Support status for a single platform capability.
 *
 * The UI must only render a "full/green" indicator for Full, which is
 * reserved for capabilities that are both implemented in this plugin and
 * verified against the platform's official documentation. */
enum class FeatureStatus {
	Full,               /* implemented and documented as working */
	Partial,            /* implemented with documented limitations */
	Manual,             /* possible, but requires manual configuration */
	PartnerOnly,        /* requires partner/enterprise access */
	EligibilityRequired,/* requires account eligibility / app review */
	Unavailable,        /* not available through the public platform API */
	NotImplemented      /* platform supports it, this plugin does not implement it */
};

enum class Feature {
	OAuth,
	BroadcastCreate,
	BroadcastSchedule,
	StreamKeyRetrieval,
	ManualIngest,
	MetadataManagement,
	LiveStatusMonitoring,
	ApiStartStop,
	RtmpIngest,
	Chat,
	Analytics
};

const char *featureKey(Feature feature);
Feature featureFromKey(const QString &key);
QString featureName(Feature feature);
QString featureStatusText(FeatureStatus status);
/* Short badge label used in the platform picker. */
QString featureStatusBadge(FeatureStatus status);

struct PlatformCapabilities {
	FeatureStatus oauth = FeatureStatus::Unavailable;
	FeatureStatus broadcastCreate = FeatureStatus::Unavailable;
	FeatureStatus broadcastSchedule = FeatureStatus::Unavailable;
	FeatureStatus streamKeyRetrieval = FeatureStatus::Unavailable;
	FeatureStatus manualIngest = FeatureStatus::Unavailable;
	FeatureStatus metadataManagement = FeatureStatus::Unavailable;
	FeatureStatus liveStatusMonitoring = FeatureStatus::Unavailable;
	FeatureStatus apiStartStop = FeatureStatus::Unavailable;
	FeatureStatus rtmpIngest = FeatureStatus::Unavailable;
	FeatureStatus chat = FeatureStatus::Unavailable;
	FeatureStatus analytics = FeatureStatus::Unavailable;

	FeatureStatus status(Feature feature) const;
};

struct PlatformInfo {
	QString id;
	QString displayName;
	QString description;
	QString docsUrl;
	QString portalUrl;
	QString defaultIngestUrl;
	QString ingestHint;
	PlatformCapabilities caps;
	/* Features with real, doc-backed implementation in this plugin. Any
	 * capability marked Full/Partial must appear here (validated by tests). */
	QSet<Feature> implemented;
	/* Advisory bitrate ceiling in kbps (0 = no platform guidance). */
	int maxBitrateKbps = 0;
	/* True when the platform issues a fresh stream key per live session. */
	bool sessionBasedKey = false;
	/* True when the platform is listed only to explain unsupported status. */
	bool informationalOnly = false;

	bool canStream() const
	{
		return caps.rtmpIngest == FeatureStatus::Full ||
		       caps.rtmpIngest == FeatureStatus::Manual ||
		       caps.rtmpIngest == FeatureStatus::Partial;
	}

	bool has(Feature feature) const { return implemented.contains(feature); }
};

/* Returns a list of human-readable problems; empty means the declaration is
 * internally consistent. Used by unit tests for every registered adapter. */
QStringList validatePlatformInfo(const PlatformInfo &info);

} // namespace ums
