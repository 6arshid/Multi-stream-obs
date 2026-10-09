# Build on Windows

Install Visual Studio 2022 with Desktop development with C++, a Windows SDK, CMake 3.28 or newer, and Python. Run in the repository root:

```powershell
.\scripts\Setup-QtTest.ps1
.\scripts\Build-Windows.ps1 -Install
```

The first command downloads the official Qt 6.8.3 MSVC 2022 qtbase SDK using aqtinstall 3.3.0. The OBS dependency bundle omits Qt Test, so CMake finds that component in `.deps/qt-test-sdk/6.8.3/msvc2022_64`. It keeps the pinned OBS Qt bundle for production components. Tests add both DLL directories to their process search path. An existing matching Qt Test installation can be supplied through `Qt6Test_DIR` instead.

The build script configures `windows-x64`, builds RelWithDebInfo, runs CTest, and stages installation in `release/RelWithDebInfo`. Configure downloads pinned dependencies in buildspec.json. `-Configuration Debug` or `Release` selects another configuration. `-ConfigureOnly` stops after configuration. `-NoTests` skips executing tests; use `-DENABLE_TESTS=OFF` with CMake to omit building them.

Individual commands, using your CMake executable:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo
ctest --preset windows-x64
cmake --install build_x64 --config RelWithDebInfo --prefix release/RelWithDebInfo
```

The DLL is `build_x64/RelWithDebInfo/obs-universal-multistream.dll`. Do not replace OBS's Qt libraries with the supplemental test SDK. If a previous interrupted compiler locks source files, stop that repository's stale compiler process before rebuilding.
