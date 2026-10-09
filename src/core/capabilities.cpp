/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/capabilities.hpp"

#include <QObject>

namespace ums {

const char *featureKey(Feature feature)
{
	switch (feature) {
	case Feature::OAuth:
		return "oauth";
	case Feature::BroadcastCreate:
		return "broadcastCreate";
	case Feature::BroadcastSchedule:
		return "broadcastSchedule";
	case Feature::StreamKeyRetrieval:
		return "streamKeyRetrieval";
	case Feature::ManualIngest:
		return "manualIngest";
	case Feature::MetadataManagement:
		return "metadataManagement";
	case Feature::LiveStatusMonitoring:
		return "liveStatusMonitoring";
	case Feature::ApiStartStop:
		return "apiStartStop";
	case Feature::RtmpIngest:
		return "rtmpIngest";
	case Feature::Chat:
		return "chat";
	case Feature::Analytics:
		return "analytics";
	}
	return "unknown";
}

Feature featureFromKey(const QString &key)
{
	static const QSet<QString> keys = {
		"oauth",          "broadcastCreate",     "broadcastSchedule", "streamKeyRetrieval",
		"manualIngest",   "metadataManagement",  "liveStatusMonitoring",
		"apiStartStop",   "rtmpIngest",          "chat",              "analytics",
	};
	if (!keys.contains(key))
		return Feature::OAuth;
	if (key == "oauth")
		return Feature::OAuth;
	if (key == "broadcastCreate")
		return Feature::BroadcastCreate;
	if (key == "broadcastSchedule")
		return Feature::BroadcastSchedule;
	if (key == "streamKeyRetrieval")
		return Feature::StreamKeyRetrieval;
	if (key == "manualIngest")
		return Feature::ManualIngest;
	if (key == "metadataManagement")
		return Feature::MetadataManagement;
	if (key == "liveStatusMonitoring")
		return Feature::LiveStatusMonitoring;
	if (key == "apiStartStop")
		return Feature::ApiStartStop;
	if (key == "rtmpIngest")
		return Feature::RtmpIngest;
	if (key == "chat")
		return Feature::Chat;
	return Feature::Analytics;
}

QString featureName(Feature feature)
{
	switch (feature) {
	case Feature::OAuth:
		return QObject::tr("Account connection (OAuth)");
	case Feature::BroadcastCreate:
		return QObject::tr("Broadcast creation");
	case Feature::BroadcastSchedule:
		return QObject::tr("Broadcast scheduling");
	case Feature::StreamKeyRetrieval:
		return QObject::tr("Stream key retrieval");
	case Feature::ManualIngest:
		return QObject::tr("Manual ingest configuration");
	case Feature::MetadataManagement:
		return QObject::tr("Stream metadata management");
	case Feature::LiveStatusMonitoring:
		return QObject::tr("Live status monitoring");
	case Feature::ApiStartStop:
		return QObject::tr("Start/stop via API");
	case Feature::RtmpIngest:
		return QObject::tr("RTMP/RTMPS ingest");
	case Feature::Chat:
		return QObject::tr("Chat integration");
	case Feature::Analytics:
		return QObject::tr("Analytics");
	}
	return {};
}

QString featureStatusText(FeatureStatus status)
{
	switch (status) {
	case FeatureStatus::Full:
		return QObject::tr("Supported");
	case FeatureStatus::Partial:
		return QObject::tr("Partially supported");
	case FeatureStatus::Manual:
		return QObject::tr("Manual configuration required");
	case FeatureStatus::PartnerOnly:
		return QObject::tr("Requires partner or enterprise access");
	case FeatureStatus::EligibilityRequired:
		return QObject::tr("Requires account eligibility or app review");
	case FeatureStatus::Unavailable:
		return QObject::tr("Unavailable through the public API");
	case FeatureStatus::NotImplemented:
		return QObject::tr("Not yet implemented");
	}
	return {};
}

QString featureStatusBadge(FeatureStatus status)
{
	switch (status) {
	case FeatureStatus::Full:
		return QObject::tr("Supported");
	case FeatureStatus::Partial:
		return QObject::tr("Partial");
	case FeatureStatus::Manual:
		return QObject::tr("Manual");
	case FeatureStatus::PartnerOnly:
		return QObject::tr("Partner");
	case FeatureStatus::EligibilityRequired:
		return QObject::tr("Eligibility");
	case FeatureStatus::Unavailable:
		return QObject::tr("Unavailable");
	case FeatureStatus::NotImplemented:
		return QObject::tr("Not implemented");
	}
	return {};
}

FeatureStatus PlatformCapabilities::status(Feature feature) const
{
	switch (feature) {
	case Feature::OAuth:
		return oauth;
	case Feature::BroadcastCreate:
		return broadcastCreate;
	case Feature::BroadcastSchedule:
		return broadcastSchedule;
	case Feature::StreamKeyRetrieval:
		return streamKeyRetrieval;
	case Feature::ManualIngest:
		return manualIngest;
	case Feature::MetadataManagement:
		return metadataManagement;
	case Feature::LiveStatusMonitoring:
		return liveStatusMonitoring;
	case Feature::ApiStartStop:
		return apiStartStop;
	case Feature::RtmpIngest:
		return rtmpIngest;
	case Feature::Chat:
		return chat;
	case Feature::Analytics:
		return analytics;
	}
	return FeatureStatus::Unavailable;
}

static bool isPositiveStatus(FeatureStatus status)
{
	return status == FeatureStatus::Full || status == FeatureStatus::Partial;
}

QStringList validatePlatformInfo(const PlatformInfo &info)
{
	QStringList problems;

	if (info.id.trimmed().isEmpty())
		problems << QStringLiteral("platform id is empty");
	if (info.displayName.trimmed().isEmpty())
		problems << QStringLiteral("display name is empty");
	if (!info.docsUrl.startsWith(QStringLiteral("https://")) &&
	    !info.docsUrl.isEmpty())
		problems << QStringLiteral("documentation URL is not https: ") + info.docsUrl;

	static const Feature allFeatures[] = {
		Feature::OAuth,
		Feature::BroadcastCreate,
		Feature::BroadcastSchedule,
		Feature::StreamKeyRetrieval,
		Feature::ManualIngest,
		Feature::MetadataManagement,
		Feature::LiveStatusMonitoring,
		Feature::ApiStartStop,
		Feature::RtmpIngest,
		Feature::Chat,
		Feature::Analytics,
	};

	for (Feature feature : allFeatures) {
		const FeatureStatus status = info.caps.status(feature);
		const bool positive = isPositiveStatus(status);

		/* Green/implemented claims must be backed by real implementation. */
		if (positive && !info.has(feature) && status == FeatureStatus::Full) {
			problems << QStringLiteral("feature %1 claims Full support but has no implementation")
			                .arg(QLatin1String(featureKey(feature)));
		}
		/* Implementation must not overstate capability declarations. */
		if (info.has(feature) && !positive &&
		    (status == FeatureStatus::Unavailable || status == FeatureStatus::NotImplemented)) {
			problems << QStringLiteral("feature %1 is implemented but declared as %2")
			                .arg(QLatin1String(featureKey(feature)))
			                .arg(featureStatusText(status));
		}
	}

	if (info.has(Feature::OAuth) && !isPositiveStatus(info.caps.oauth))
		problems << QStringLiteral("OAuth is implemented but caps.oauth is not Full/Partial");

	if (info.has(Feature::StreamKeyRetrieval) &&
	    !isPositiveStatus(info.caps.streamKeyRetrieval))
		problems << QStringLiteral("stream key retrieval implemented but not declared");

	if (!info.informationalOnly && !info.canStream() &&
	    (info.has(Feature::StreamKeyRetrieval) || info.has(Feature::ManualIngest)))
		problems << QStringLiteral("platform %1 cannot stream via RTMP but exposes ingest features")
		                .arg(info.id);

	if (isPositiveStatus(info.caps.rtmpIngest) && !info.has(Feature::RtmpIngest) &&
	    info.caps.rtmpIngest == FeatureStatus::Full)
		problems << QStringLiteral("rtmpIngest Full without implementation flag");

	return problems;
}

} // namespace ums
