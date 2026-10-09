# Platform API setup

Register your own application; the plugin ships no application IDs or secrets. In Settings enter the platform client ID, redirect URI, and secret when required. IDs and redirect URIs go in configuration; secrets go in the OS vault. Save Settings before opening Connect account. Each platform currently has one connected token shared by that platform's destinations.

A blank redirect uses a random-port HTTP loopback callback on 127.0.0.1. Use that only when the provider/application type permits it. Otherwise register an exact redirect URI and enter it in Settings; finish the browser login and paste the complete returned URL. Do not invent an unregistered redirect or publish the authorization URL/code. Providers may require developer approval, verified applications, account eligibility, quotas, or additional permissions.

| Platform | Setup matching this implementation | Requested scopes |
|---|---|---|
| YouTube | Enable YouTube Data API v3, configure consent/test users, create a desktop OAuth client, and supply its ID and secret. Leave redirect blank for loopback. The channel must have live streaming enabled. | `youtube.force-ssl`, `youtube.readonly` (full Google scope URLs in the adapter) |
| Twitch | Register a device-flow application. Public device clients do not need a secret; confidential clients need their registered secret where required. This adapter opens device authorization and asks you to enter its code. | `channel:read:stream_key`, `channel:manage:broadcast` |
| Kick | Register an application with an allowed redirect and the scopes below. The plugin uses authorization code with S256 PKCE. Supply a secret if the registered client requires one. | `channel:read`, `streamkey:read`, `channel:write`, `user:read` |
| Facebook | Create an appropriate Meta application with reviewed access for the intended live-video target. Enter app ID/secret and an exact accepted redirect. Paste access token is a fallback for a user token generated for your own application. Select a managed page account where available. | Adapter requests `publish_video`, `pages_show_list`, `pages_read_engagement`, `pages_manage_posts`; availability depends on current app products and review |
| Restream | Register an application and accepted redirect in the developer portal; enter ID and secret. The integration reads the user's stream key and ingest/server information. | `profile.read`, `stream.read`, `channels.read` |
| Trovo | Register the application and exact redirect with Trovo. Obtain the client secret through its developer program when needed. This adapter sends JSON token bodies and a `Client-ID` header. | `channel_details_self+channel_update_self+user_details_self` |
| Vimeo | Automatic OAuth/key retrieval is not implemented. Use a live event's manual RTMPS URL/key. Partner API access is not enabled merely by having a Vimeo subscription. | None |

YouTube uses offline access and consent to obtain refresh credentials. Other providers may rotate or omit refresh tokens. The plugin preserves an existing refresh token when a refresh response omits it. A failed refresh requires reconnecting; a successful API connection is not evidence that an account may publicly broadcast.

The Facebook adapter currently targets Graph v26.0. Its scope/product availability and personal/group publication behavior require real application testing; do not interpret its implemented capability labels as a guarantee that every target is supported by Meta. Page tokens are fetched transiently for page publication and are not persisted.

Official references: [Google installed-app OAuth](https://developers.google.com/identity/protocols/oauth2/native-app), [YouTube Live API](https://developers.google.com/youtube/v3/live/docs), [Twitch OAuth/device grants](https://dev.twitch.tv/docs/authentication/getting-tokens-oauth/), [Kick scopes](https://github.com/KickEngineering/KickDevDocs/blob/main/scopes/scopes.md), [Meta Video API](https://developers.facebook.com/docs/video-api/), [Restream developers](https://developers.restream.io/), [Trovo OAuth/API](https://developer.trovo.live/docs/APIs.html), [Vimeo Live API](https://developer.vimeo.com/api/reference/live).

The table describes the code's requested permissions and workflow. Provider documentation and the application's registration control what is accepted. Use Manual mode when developer access or review is unavailable.
