/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platforms/factories.hpp"

#include <QObject>

namespace ums {

namespace {

/* Vimeo Live is streamable through manual RTMPS credentials, but its Live
 * API is granted on request to enterprise/development partners - so the
 * adapter is manual-only and the API columns stay PartnerOnly. */
class VimeoAdapter final : public IPlatformAdapter {
public:
	PlatformInfo info() const override
	{
		PlatformInfo info;
		info.id = QStringLiteral("vimeo");
		info.displayName = QObject::tr("Vimeo");
		info.description =
		    QObject::tr("Vimeo Live through manual RTMPS credentials: create a live event "
		                "in Vimeo and paste its RTMPS URL and stream key. The Vimeo Live "
		                "API (automatic key handling) is available only to approved "
		                "development/enterprise partners.");
		info.docsUrl = QStringLiteral("https://developer.vimeo.com/api/reference/live");
		info.portalUrl = QStringLiteral("https://vimeo.com/");
		info.ingestHint =
		    QObject::tr("Vimeo -> Live Events: create or open the event and copy the "
		                "RTMPS URL and stream key. Each event has its own endpoint.");
		info.sessionBasedKey = true;
		info.caps.oauth = FeatureStatus::PartnerOnly;
		info.caps.broadcastCreate = FeatureStatus::PartnerOnly;
		info.caps.broadcastSchedule = FeatureStatus::PartnerOnly;
		info.caps.streamKeyRetrieval = FeatureStatus::PartnerOnly;
		info.caps.manualIngest = FeatureStatus::Full;
		info.caps.metadataManagement = FeatureStatus::PartnerOnly;
		info.caps.liveStatusMonitoring = FeatureStatus::PartnerOnly;
		info.caps.apiStartStop = FeatureStatus::PartnerOnly;
		info.caps.rtmpIngest = FeatureStatus::Full;
		info.caps.chat = FeatureStatus::Unavailable;
		info.caps.analytics = FeatureStatus::PartnerOnly;
		info.implemented = {Feature::ManualIngest, Feature::RtmpIngest};
		return info;
	}

	QStringList validate(const DestinationConfig &destination) const override
	{
		QStringList errors = ConfigStore::validate(destination);
		if (destination.authMode == AuthMode::Api) {
			errors << QObject::tr("Vimeo's Live API requires partner access - use manual mode");
		} else if (destination.server.trimmed().isEmpty()) {
			errors << QObject::tr("paste the RTMPS URL of your Vimeo live event");
		}
		return errors;
	}

	void prepareIngest(const AuthContext &auth, const DestinationConfig &destination,
	                   IngestCallback callback) override
	{
		callback(resolveManualIngest(info(), auth, destination));
	}
};

} // namespace

AdapterPtr createVimeoAdapter()
{
	return std::make_unique<VimeoAdapter>();
}

} // namespace ums
