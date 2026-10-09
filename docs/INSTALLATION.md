# Installation

Close OBS before installing or updating. Keep a copy of an existing plugin if replacing it. For a Windows x64 build against OBS 32.0.4, copy:

| Source | Destination |
|---|---|
| `build_x64/RelWithDebInfo/obs-universal-multistream.dll` | `C:\Program Files\obs-studio\obs-plugins\64bit\` |
| `data/locale/*.ini` | `C:\Program Files\obs-studio\data\obs-plugins\obs-universal-multistream\locale\` |

Program Files may require an elevated file copy. A supported alternative is to copy the entire staged `release/RelWithDebInfo/obs-universal-multistream` folder into `C:\ProgramData\obs-studio\plugins\`; OBS discovers its `bin/64bit` and `data` directories on restart. This workspace used that location after Program Files denied writes. Use one installation location to avoid duplicate module copies.

The build script's `-Install` stages a release directory; it does not install into the running OBS application. Do not ship Qt Test or replace OBS's Qt DLLs. OBS supplies its runtime dependencies.

Start OBS and enable **Docks → Universal Multi-Stream**. The registered dock starts hidden/floating and can be docked normally. Check the OBS log for `[ums] initialized` and module loading errors. Persian uses the OBS `fa-IR` locale and right-to-left dock layout.

Uninstall by closing OBS and removing only this plugin's DLL and data folder. Configuration lives in OBS's module configuration directory as `multistream.json`; removing the DLL does not remove saved configuration or OS vault credentials. Clear stream keys and disconnect accounts before uninstalling if you want to remove their stored secrets. Application client secrets can also be removed through OS credential management.

macOS installation is unverified; see [BUILD_MACOS](BUILD_MACOS.md).
