# Security

Stream keys, serialized OAuth tokens, and application client secrets are stored through Windows Credential Manager or macOS Keychain. There is no plaintext fallback. Windows writes generic `ums/<service>/<account>` credentials; macOS uses Security.framework. Windows vault writes can fail even when the backend exists. The UI reports storage failures.

Configuration contains destination settings, non-secret application IDs/redirect URIs, account aliases, and remote resource IDs. Credential fields in platform options are rejected. Do not place secrets in titles, labels, URLs, or arbitrary settings. Export removes account metadata and volatile destination references. Migration removes legacy `key` and `keyMaterial` fields; legacy/corrupt backups may still contain old secrets and need secure handling.

The redactor masks registered secret values, bearer tokens, and common credential assignments. Nested JSON redaction handles sensitive keys case-insensitively. API-resolved keys are registered before preparing OBS services. Password inputs conceal typed secrets, but this does not protect against process memory inspection, clipboard history, screenshots from provider pages, or software with access to the same user account. RTMP is unencrypted; prefer RTMPS when available.

OAuth state checks bind authorization responses to the pending request. Kick uses S256 PKCE. Loopback authorization binds locally; fixed registered redirects require pasting the complete returned URL. Do not share that URL, device codes, token JSON, vault exports, or stream keys. Revoke access at the provider when needed; local disconnection removes the local token and metadata and is not a promise of remote revocation.

No embedded provider credentials are included. Report security issues privately to the project maintainers using a verified repository contact; buildspec.json's placeholder website/email are not an established reporting channel. Avoid posting unredacted OBS logs or credentials in issues.
