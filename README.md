# Universal Multi-Stream — Free OBS Studio Multistream Plugin

Native OBS Studio plugin with a Qt dock for independent RTMP/RTMPS destinations, optional platform API connections, OS credential storage, and English/Persian UI. Plugin ID: `obs-universal-multistream`. Version: 0.1.0. License: GPL-2.0-or-later.

**Universal Multi-Stream is a free, open-source OBS multistream plugin for live streaming to multiple platforms at once.** Send your OBS program video and audio to Twitch, YouTube Live, Kick, Facebook Live, Trovo, Restream, or custom RTMP/RTMPS servers from one OBS Studio dock. Use it for Twitch live streaming, simultaneous Twitch and YouTube streaming, and multi-platform broadcasts with independent destination controls.

## Multistreaming features

- **Multiple RTMP outputs:** stream to multiple destinations simultaneously using RTMP or secure RTMPS server URLs and stream keys.
- **Twitch, YouTube, and Kick streaming:** optional platform API connections for Twitch, YouTube, Kick, Facebook, Restream, and Trovo; available features vary by platform.
- **Custom RTMP streaming:** configure your own live streaming server, or use provider-issued ingest credentials for DLive, Rumble, and Vimeo. Instagram Live, TikTok Live, LinkedIn Live, and X (Twitter) require account access and manual ingest credentials; see [platform support](docs/PLATFORM_SUPPORT.md).
- **Live stream controls:** Start All / Stop All, individual destination Start / Stop, and optional automatic start/stop with the main OBS stream.
- **Encoder sharing and output options:** reuse OBS encoders or configure per-destination encoder and bitrate overrides. Every destination still needs upload bandwidth.
- **Stream monitoring:** view output state, bitrate, dropped-frame percentage, and redacted logs in the OBS dock; configure retries for output failures.
- **Secure credential storage:** keep stream keys and API credentials in the operating system credential store.
- **English and Persian interface:** includes Persian (Farsi) labels and right-to-left UI support.

The plugin itself is free under the GPL-2.0-or-later license. Streaming services may have their own fees, account requirements, or access restrictions. See [installation](docs/INSTALLATION.md) to set up this OBS Studio plugin and the [user guide](docs/USER_GUIDE.md) to configure simultaneous live streams.

## Compatibility and quick start

Windows x64 builds against pinned OBS 32.0.4 and Qt 6.8.3. Windows 10/11 are intended targets; verification in this workspace uses Windows 11. A portable OBS 32.2.2 smoke test verified plugin loading, the dock, manual destination dialogs, local RTMP H.264/AAC output, main-stream start/stop coupling, statistics, logs, and Persian RTL labels. macOS has build presets and Keychain code but has **not been compiled or tested**. API implementations have automated simulated-response tests; these do not certify live account access or platform eligibility.

Open **Docks → Universal Multi-Stream**, add a platform, choose Manual mode, paste the server URL and stream key, and save. Enable the destination for Start All, or use its individual Start / Stop button. API connections require your own registered application credentials in Settings.

- [Windows build](docs/BUILD_WINDOWS.md), [macOS build](docs/BUILD_MACOS.md), [installation](docs/INSTALLATION.md)
- [User guide](docs/USER_GUIDE.md), [platform support](docs/PLATFORM_SUPPORT.md), [API setup](docs/API_SETUP.md)
- [Security](docs/SECURITY.md), [contributing](docs/CONTRIBUTING.md)

The plugin sends the current OBS program video/audio to each destination. Encoder sharing reduces encoding work; each output still consumes upload bandwidth. SRT, chat, analytics, and universal platform-side broadcast start/stop are not implemented.

## Developer

- **Developer:** 6arshid
- **Website:** [www.6arshid.com](https://www.6arshid.com)
- **Email:** [info@6arshid.com](mailto:info@6arshid.com)
