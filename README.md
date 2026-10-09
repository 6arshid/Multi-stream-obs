# Universal Multi-Stream

Native OBS Studio plugin with a Qt dock for independent RTMP/RTMPS destinations, optional platform API connections, OS credential storage, and English/Persian UI. Plugin ID: `obs-universal-multistream`. Version: 0.1.0. License: GPL-2.0-or-later.

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
