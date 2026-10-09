# Platform support

Generated from the compiled AdapterRegistry / PlatformInfo declarations. Supported means code exists for this capability; automated simulated HTTP tests do not prove live-provider access. Partial means implemented with limitations. Partner/eligibility/manual labels describe access constraints. Not implemented and unavailable are distinct. Only Full capabilities may receive a green supported indicator.

Windows x64: built against OBS 32.0.4; 20 test cases across eight Qt suites pass. Local RTMP start/stop, dock statistics/logs, the platform picker/manual editor, and Persian labels/RTL were verified in portable OBS 32.2.2. The local receiver recorded 320×180 H.264 video and AAC audio using black video and silent audio; no provider account was used. Windows 10 remains untested. macOS: preset and Keychain implementation exist; compilation and runtime are unverified for every row below. Platform API account flows have not been live-account certified in this workspace.

| Platform | OAuth | Key retrieval | Manual ingest | RTMP | Broadcast creation | Metadata |
|---|---|---|---|---|---|---|
| BIGO LIVE | Unavailable through the public API | Unavailable through the public API | Unavailable through the public API | Unavailable through the public API | Unavailable through the public API | Unavailable through the public API |
| Custom RTMP | Unavailable through the public API | Unavailable through the public API | Supported | Supported | Unavailable through the public API | Unavailable through the public API |
| Discord | Unavailable through the public API | Unavailable through the public API | Unavailable through the public API | Unavailable through the public API | Unavailable through the public API | Unavailable through the public API |
| DLive | Unavailable through the public API | Unavailable through the public API | Supported | Supported | Unavailable through the public API | Unavailable through the public API |
| Facebook | Partially supported | Supported | Supported | Supported | Supported | Partially supported |
| Instagram | Unavailable through the public API | Unavailable through the public API | Supported | Manual configuration required | Unavailable through the public API | Unavailable through the public API |
| Kick | Supported | Supported | Supported | Supported | Unavailable through the public API | Partially supported |
| LinkedIn | Unavailable through the public API | Unavailable through the public API | Supported | Manual configuration required | Unavailable through the public API | Unavailable through the public API |
| Restream | Supported | Supported | Supported | Supported | Not yet implemented | Not yet implemented |
| Rumble | Unavailable through the public API | Unavailable through the public API | Supported | Supported | Unavailable through the public API | Unavailable through the public API |
| StreamYard | Unavailable through the public API | Unavailable through the public API | Unavailable through the public API | Unavailable through the public API | Unavailable through the public API | Unavailable through the public API |
| TikTok | Unavailable through the public API | Unavailable through the public API | Supported | Manual configuration required | Unavailable through the public API | Unavailable through the public API |
| Trovo | Supported | Supported | Supported | Supported | Unavailable through the public API | Partially supported |
| Twitch | Supported | Supported | Supported | Supported | Unavailable through the public API | Partially supported |
| Vimeo | Requires partner or enterprise access | Requires partner or enterprise access | Supported | Supported | Requires partner or enterprise access | Requires partner or enterprise access |
| X (Twitter) | Unavailable through the public API | Unavailable through the public API | Supported | Manual configuration required | Unavailable through the public API | Unavailable through the public API |
| YouTube | Supported | Supported | Supported | Supported | Supported | Partially supported |

Manual destinations still require provider-issued credentials and an eligible account. BIGO LIVE, Discord, and StreamYard are informational and disabled. Vimeo has no automatic API integration. Scheduling, chat, analytics, and live status polling are not implemented in the plugin. RTMP start/stop controls the local output, not a universal provider broadcast lifecycle.
