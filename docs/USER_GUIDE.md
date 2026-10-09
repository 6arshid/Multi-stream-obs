# User guide

Add a destination from the platform picker. Informational platforms cannot be selected. Capability labels describe the implemented integration; account eligibility and application permissions still apply.

In **Manual** mode, paste the server URL and stream key separately. Supported output protocols are RTMP and RTMPS. A blank server uses the adapter default when one exists. Password fields do not reveal stored credentials. A blank key preserves the vault value; Clear stored stream key removes it. A newly added destination remains disabled until its checkbox is enabled.

In **API** mode, configure the platform's application credentials in Settings, press Connect account, and complete browser or device authorization. Select the account returned by the platform. Facebook also supports Paste access token. A registered redirect URI uses a paste-back flow; paste the complete final browser URL into the protected prompt. Blank redirect uses a local loopback listener.

Set a label, title, and privacy, then save. Platform options is a non-secret JSON object for adapter-supported options such as description. Credentials are rejected there. Encoder overrides allow bitrate and encoder ID per destination; zero bitrate and blank ID inherit the policy. Shared encoding can reuse main OBS encoders or a fallback shared pool. Per-destination bitrate/encoder overrides create independent encoders. Use an OBS encoder ID supported on the local installation.

Start All starts enabled destinations; Stop All cancels pending API resolution and stops outputs. Individual Start / Stop works independently of the enable checkbox. Rows show state, bitrate, and dropped-frame percentage. Logs are redacted and bounded. Editing an active destination stops and discards that output so changes apply on the next start. Removing a destination also removes its stream key.

Settings can start enabled destinations with the main OBS stream and stop them when it stops. Stop all on fatal error stops remaining destinations when an output reports a terminal error. Configure retry count/delay and encoder sharing there. Server-side publication may still require a platform dashboard action; an active RTMP connection does not prove a broadcast is publicly visible.

For a smoke test, use a private/test destination: add it in Manual mode, start it, verify audio/video at the receiving server, confirm row state/stats/logs, stop it, and confirm no late restart. Exercise Start All, main-stream coupling, edit/remove, failed credentials, and Persian RTL. Real-provider keys, authenticated accounts, and interactive OBS access are required for complete end-to-end certification.
