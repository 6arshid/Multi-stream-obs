/*
Universal Multi-Stream - OBS Studio multi-destination streaming plugin
Copyright (C) 2026 Universal Multi-Stream Contributors
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platforms/factories.hpp"

#include <QObject>

namespace ums {

namespace {

PlatformCapabilities manualCaps()
{
	PlatformCapabilities caps;
	caps.oauth = FeatureStatus::Unavailable;
	caps.manualIngest = FeatureStatus::Full;
	caps.rtmpIngest = FeatureStatus::Full;
	return caps;
}

QSet<Feature> manualImplemented()
{
	return {Feature::RtmpIngest, Feature::ManualIngest};
}

/* Adapter used for every manual-only destination. */
class GenericManualAdapter final : public IPlatformAdapter {
public:
	explicit GenericManualAdapter(PlatformInfo info) : info_(std::move(info)) {}

	PlatformInfo info() const override { return info_; }

	QStringList validate(const DestinationConfig &destination) const override
	{
		if (info_.informationalOnly)
			return {QObject::tr("%1 does not accept RTMP streams from OBS")
			        .arg(info_.displayName)};

		QStringList errors = ConfigStore::validate(destination);
		if (destination.authMode == AuthMode::Api) {
			errors << QObject::tr("%1 has no API integration in this plugin; use manual mode")
			               .arg(info_.displayName);
		} else if (destination.server.trimmed().isEmpty() &&
		           info_.defaultIngestUrl.isEmpty()) {
			errors << QObject::tr("paste the ingest server URL from %1").arg(info_.displayName);
		}
		return errors;
	}

	void prepareIngest(const AuthContext &auth, const DestinationConfig &destination,
	                   IngestCallback callback) override
	{
		if (info_.informationalOnly) {
			IngestEndpoint endpoint;
			endpoint.error = QObject::tr("%1 does not accept RTMP streams from OBS")
			                     .arg(info_.displayName);
			callback(endpoint);
			return;
		}
		callback(resolveManualIngest(info_, auth, destination));
	}

private:
	PlatformInfo info_;
};

AdapterPtr make(const PlatformInfo &info)
{
	return std::make_unique<GenericManualAdapter>(info);
}

} // namespace

AdapterPtr createManualAdapter()
{
	PlatformInfo info;
	info.id = QStringLiteral("custom");
	info.displayName = QObject::tr("Custom RTMP");
	info.description =
	    QObject::tr("Any RTMP or RTMPS server: self-hosted ingest, Nginx-RTMP, "
	                "and platforms not listed here.");
	info.ingestHint = QObject::tr("Server URL and stream key from your ingest provider.");
	info.caps = manualCaps();
	info.implemented = manualImplemented();
	return make(info);
}

AdapterPtr createTikTokAdapter()
{
	PlatformInfo info;
	info.id = QStringLiteral("tiktok");
	info.displayName = QObject::tr("TikTok");
	info.description =
	    QObject::tr("TikTok LIVE via the server URL and key shown in TikTok Live Studio "
	                "or the Live Center. Access depends on account eligibility.");
	info.portalUrl = QStringLiteral("https://www.tiktok.com/live");
	info.ingestHint =
	    QObject::tr("Session-based: copy the Server URL and Stream key from TikTok before "
	                "each stream.");
	info.maxBitrateKbps = 4000;
	info.sessionBasedKey = true;
	info.caps = manualCaps();
	info.caps.rtmpIngest = FeatureStatus::Manual;
	info.caps.oauth = FeatureStatus::Unavailable;
	info.implemented = manualImplemented();
	return make(info);
}

AdapterPtr createInstagramAdapter()
{
	PlatformInfo info;
	info.id = QStringLiteral("instagram");
	info.displayName = QObject::tr("Instagram");
	info.description =
	    QObject::tr("Instagram Live (Live Producer in the browser): paste the RTMPS URL "
	                "and key while you set up the live video. No official broadcast API.");
	info.portalUrl = QStringLiteral("https://www.instagram.com/");
	info.ingestHint =
	    QObject::tr("Create a live video on instagram.com, then copy the Stream URL and "
	                "Stream key. Eligibility rules apply.");
	info.maxBitrateKbps = 4000;
	info.sessionBasedKey = true;
	info.caps = manualCaps();
	info.caps.rtmpIngest = FeatureStatus::Manual;
	info.implemented = manualImplemented();
	return make(info);
}

AdapterPtr createXAdapter()
{
	PlatformInfo info;
	info.id = QStringLiteral("x");
	info.displayName = QObject::tr("X (Twitter)");
	info.description =
	    QObject::tr("X Live for eligible accounts (X Premium). The ingest URL is static, "
	                "the key is issued per live event. The official Livestream API is "
	                "closed to third parties.");
	info.portalUrl = QStringLiteral("https://x.com/");
	info.ingestHint =
	    QObject::tr("Start a live event in X to receive a stream key; server: "
	                "rtmp://va.pscp.tv:80/x");
	info.maxBitrateKbps = 5000;
	info.sessionBasedKey = true;
	info.caps = manualCaps();
	info.caps.rtmpIngest = FeatureStatus::Manual;
	info.implemented = manualImplemented();
	return make(info);
}

AdapterPtr createLinkedInAdapter()
{
	PlatformInfo info;
	info.id = QStringLiteral("linkedin");
	info.displayName = QObject::tr("LinkedIn");
	info.description =
	    QObject::tr("LinkedIn Live events. Ingest credentials exist only while a scheduled "
	                "event is running; the Live Events API is limited to approved partners.");
	info.portalUrl = QStringLiteral("https://www.linkedin.com/");
	info.docsUrl = QStringLiteral("https://learn.microsoft.com/en-us/linkedin/");
	info.ingestHint =
	    QObject::tr("Schedule the event on LinkedIn, then paste its Server URL and "
	                "Stream key before it starts.");
	info.sessionBasedKey = true;
	info.caps = manualCaps();
	info.caps.rtmpIngest = FeatureStatus::Manual;
	info.implemented = manualImplemented();
	return make(info);
}

AdapterPtr createRumbleAdapter()
{
	PlatformInfo info;
	info.id = QStringLiteral("rumble");
	info.displayName = QObject::tr("Rumble");
	info.description =
	    QObject::tr("Rumble live streaming with the static server URL and channel stream "
	                "key from the Live Control Room. The API cannot start broadcasts.");
	info.portalUrl = QStringLiteral("https://rumble.com/");
	info.docsUrl = QStringLiteral("https://support.rumble.com/");
	info.ingestHint =
	    QObject::tr("Settings -> Live streaming in Rumble provides the Server URL and "
	                "Stream key.");
	info.caps = manualCaps();
	info.implemented = manualImplemented();
	return make(info);
}

AdapterPtr createDLiveAdapter()
{
	PlatformInfo info;
	info.id = QStringLiteral("dlive");
	info.displayName = QObject::tr("DLive");
	info.description =
	    QObject::tr("DLive broadcasting with the static ingest URL and the stream key from "
	                "the creator dashboard. The GraphQL API requires an approved "
	                "developer account.");
	info.portalUrl = QStringLiteral("https://dlive.tv/");
	info.docsUrl = QStringLiteral("https://docs.dlive.tv/");
	info.ingestHint = QObject::tr("Creator dashboard -> Stream key; server: "
	                               "rtmp://stream.dlive.tv/live");
	info.caps = manualCaps();
	info.implemented = manualImplemented();
	return make(info);
}

AdapterPtr createBigoAdapter()
{
	PlatformInfo info;
	info.id = QStringLiteral("bigo");
	info.displayName = QObject::tr("BIGO LIVE");
	info.description =
	    QObject::tr("BIGO LIVE streams through its own apps; there is no public API for "
	                "third-party RTMP ingest from OBS, so this entry is informational.");
	info.portalUrl = QStringLiteral("https://www.bigo.live/");
	info.caps = {}; /* everything Unavailable */
	info.informationalOnly = true;
	return make(info);
}

AdapterPtr createDiscordAdapter()
{
	PlatformInfo info;
	info.id = QStringLiteral("discord");
	info.displayName = QObject::tr("Discord");
	info.description =
	    QObject::tr("Discord Go Live uses Discord's own voice gateway (UDP/Opus), not "
	                "RTMP - OBS cannot feed it. Listed for clarity only.");
	info.portalUrl = QStringLiteral("https://discord.com/");
	info.docsUrl = QStringLiteral("https://support.discord.com/hc/en-us");
	info.caps = {};
	info.informationalOnly = true;
	return make(info);
}

AdapterPtr createStreamYardAdapter()
{
	PlatformInfo info;
	info.id = QStringLiteral("streamyard");
	info.displayName = QObject::tr("StreamYard");
	info.description =
	    QObject::tr("StreamYard is a browser studio that only sends RTMP out - it has no "
	                "inbound RTMP ingest for OBS. Use StreamYard as a destination from "
	                "your broadcast software, not as an OBS destination.");
	info.portalUrl = QStringLiteral("https://www.streamyard.com/");
	info.docsUrl = QStringLiteral("https://support.streamyard.com/");
	info.caps = {};
	info.informationalOnly = true;
	return make(info);
}

} // namespace ums
