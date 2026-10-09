# Contributing

Use tabs and the existing GPL-2.0-or-later SPDX source header. Keep OBS-independent code in multistream-core and OBS calls in the module/engine. Add a platform factory, register it in AdapterRegistry::ensureBuiltins, and supply honest PlatformInfo declarations.

Full and Partial capabilities must have implementations listed in `implemented`. Unavailable and NotImplemented must not. Full is reserved for a documented implemented capability; it does not substitute for an authenticated runtime test. Keep eligibility/partner restrictions visible. Use monogram badges rather than trademarked logos.

Run the Windows pipeline and all Qt tests before submitting. Add simulated HTTP tests for meaningful API behavior and error paths; never use real credentials in tests. Do not commit build outputs, `.deps`, OAuth client secrets, or account tokens. Qt Q_OBJECT headers must be explicitly listed in target sources for reliable AUTOMOC discovery.

Update every user-visible translation in `data/locale/en-US.ini` and `fa-IR.ini`, keeping the flat `source=translation` format and placeholder numbering. Update support documentation when capabilities change. Generate the support table from the compiled registry with `multistream-tests --support-matrix` rather than maintaining unsupported claims by hand.

Changes to asynchronous start, stop, removal, or shutdown must preserve cancellation: an old API response must not start an output after Stop or apply to a later request. Check actual OBS loading and a private receiving server for engine changes. Document unavailable platform tests explicitly.
