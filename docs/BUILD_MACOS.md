# Build on macOS (unverified)

The repository includes an Xcode universal arm64/x86_64 preset targeting macOS 12.0 and a Security.framework Keychain implementation. No macOS compile, runtime test, signing validation, or CI run has been performed. The preset's deployment target is a build setting, not a verified support promise; OBS and dependency minimum versions also apply.

Install Xcode and CMake 3.28+. Configure with the pinned OBS dependencies:

```sh
cmake --preset macos -DENABLE_TESTS=OFF
cmake --build --preset macos --config RelWithDebInfo
cmake --install build_macos --config RelWithDebInfo
```

For tests, provide a matching Qt 6.8.3 Qt Test component with `-DQt6Test_DIR=/path/to/lib/cmake/Qt6Test` and configure with `ENABLE_TESTS=ON`. Do not mix architecture slices or Qt versions. Test with `ctest --test-dir build_macos -C RelWithDebInfo --output-on-failure`.

The default plugin install directory is `~/Library/Application Support/obs-studio/plugins`. Signing variables are `CODESIGN_IDENT` and `CODESIGN_TEAM`; absent credentials use ad-hoc signing. Distribution still needs validation of the produced bundle, signing, notarization, OBS loading, and Keychain prompts on real Intel and Apple Silicon hosts.
