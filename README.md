# HotKeyManager

Windows tray utility for launching apps, files, folders, and URLs with global hotkeys.

## Scope

- C++17 by default, C++14 for Qt 5.6.3 compatibility, Qt Widgets, CMake, Win32 API.
- Uses `WH_KEYBOARD_LL` to intercept global keyboard events in user mode.
- Runs as administrator through the embedded application manifest.
- Stores rules as JSON under the user's AppData location.

This is a user-mode implementation. It can intercept many ordinary Windows and application shortcuts, but it cannot guarantee control over secure system sequences such as `Ctrl+Alt+Del`, `Win+L`, UAC secure desktop, sign-in screens, or other protected OS flows.

## Build

```powershell
cmake -S . -B build -G "Ninja" -DCMAKE_PREFIX_PATH="C:\Qt\6.7.0\msvc2019_64"
cmake --build build
ctest --test-dir build --output-on-failure
```

Adjust `CMAKE_PREFIX_PATH` to your Qt installation.

## Multi-Variant Presets

```powershell
cmake --list-presets
cmake --build --list-presets
ctest --list-presets
```

Build outputs are organized under `out/build/<qt>-<toolchain>-<linkage>-<arch>`.
Dynamic Qt variants run `windeployqt` after build when the Qt version supports reliable deployment; older compatibility variants may copy runtime files manually.

## Variant Intent

- Qt 4.8.7 is kept as the Qt4 compatibility line and for the broadest legacy Windows coverage.
- Qt 5.6.3 is kept as the legacy Qt5 line for Windows XP-capable builds and older toolchain coverage. The explicit XP presets are x86 MSVC2015 builds; additional MSVC2015 and MinGW x86/x64 ABI variants are also maintained.
- Qt 5.15.2 is kept as the final Qt5 LTS line and the main Win7-capable Qt5 target.
- Qt 6.8.3 is kept as the Qt6 LTS line.
- Qt 6.11.1 is kept as the current latest Qt line.

## Maintained Variants

Qt 4.8.7:

- `qt4.8.7-mingw482-dynamic-x86`
- `qt4.8.7-mingw482-static-x86`
- `qt4.8.7-mingw730-dynamic-x64`
- `qt4.8.7-mingw730-static-x64`
- `qt4.8.7-msvc2015-dynamic-x86`
- `qt4.8.7-msvc2015-dynamic-x64`
- `qt4.8.7-msvc2015-static-x86`
- `qt4.8.7-msvc2015-static-x64`

Qt 5.6.3 legacy:

- `qt5.6.3-msvc2015-xp-dynamic-x86`
- `qt5.6.3-msvc2015-xp-static-x86`
- `qt5.6.3-msvc2015-dynamic-x86`
- `qt5.6.3-msvc2015-dynamic-x64`
- `qt5.6.3-msvc2015-static-x86`
- `qt5.6.3-msvc2015-static-x64`
- `qt5.6.3-mingw492-dynamic-x86`
- `qt5.6.3-mingw492-static-x86`
- `qt5.6.3-mingw730-dynamic-x64`
- `qt5.6.3-mingw730-static-x64`

Qt 5.15.2 LTS:

- `qt5.15.2-msvc2019-dynamic-x86`
- `qt5.15.2-msvc2019-dynamic-x64`
- `qt5.15.2-msvc2019-static-x86`
- `qt5.15.2-msvc2019-static-x64`
- `qt5.15.2-mingw81-dynamic-x86`
- `qt5.15.2-mingw81-dynamic-x64`
- `qt5.15.2-mingw81-static-x86`
- `qt5.15.2-mingw81-static-x64`

Qt 6.8.3 LTS:

- `qt6.8.3-msvc2022-dynamic-x86`
- `qt6.8.3-msvc2022-dynamic-x64`
- `qt6.8.3-msvc2022-static-x86`
- `qt6.8.3-msvc2022-static-x64`
- `qt6.8.3-mingw13-dynamic-x64`
- `qt6.8.3-mingw13-static-x64`
- `qt6.8.3-llvm-mingw17-dynamic-x86`
- `qt6.8.3-llvm-mingw17-static-x86`

Qt 6.11.1 latest:

- `qt6.11.1-msvc2022-dynamic-x86`
- `qt6.11.1-msvc2022-dynamic-x64`
- `qt6.11.1-msvc2022-static-x86`
- `qt6.11.1-msvc2022-static-x64`
- `qt6.11.1-mingw13-dynamic-x64`
- `qt6.11.1-mingw13-static-x64`
- `qt6.11.1-llvm-mingw17-dynamic-x86`
- `qt6.11.1-llvm-mingw17-static-x86`

Qt 6 MinGW x86 variants use LLVM-MinGW 17 with the `i686-w64-mingw32` target because current Qt 6 MinGW packages no longer provide a conventional 32-bit MinGW toolchain.

## Qt 4.8.7

Qt 4.8.7 uses a dedicated compatibility path. MinGW x86 is maintained with the legacy MinGW 4.8.2 toolchain, MinGW x64 is maintained with MinGW 7.3, and MSVC2015 supports x86 and x64:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-qt-dynamic.ps1 -QtVersion 4.8.7 -Toolchain mingw -Architecture x86
powershell -ExecutionPolicy Bypass -File scripts/build-qt-dynamic.ps1 -QtVersion 4.8.7 -Toolchain mingw -Architecture x64
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 4.8.7 -Toolchain mingw -Architecture x86
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 4.8.7 -Toolchain mingw -Architecture x64
powershell -ExecutionPolicy Bypass -File scripts/build-qt-dynamic.ps1 -QtVersion 4.8.7 -Toolchain msvc -Architecture x64
powershell -ExecutionPolicy Bypass -File scripts/build-qt-dynamic.ps1 -QtVersion 4.8.7 -Toolchain msvc -Architecture x86
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 4.8.7 -Toolchain msvc -Architecture x64
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 4.8.7 -Toolchain msvc -Architecture x86
```

By default, the scripts expect the Qt source tree at `D:/Qt/4.8.7/Src`. If the source is elsewhere, pass `-QtSourceDir <path>`.

Then build and test the application variants:

```powershell
cmake --preset qt4.8.7-mingw482-dynamic-x86
cmake --build --preset qt4.8.7-mingw482-dynamic-x86-release
ctest --preset qt4.8.7-mingw482-dynamic-x86-release

cmake --preset qt4.8.7-mingw482-static-x86
cmake --build --preset qt4.8.7-mingw482-static-x86-release
ctest --preset qt4.8.7-mingw482-static-x86-release

cmake --preset qt4.8.7-mingw730-dynamic-x64
cmake --build --preset qt4.8.7-mingw730-dynamic-x64-release
ctest --preset qt4.8.7-mingw730-dynamic-x64-release

cmake --preset qt4.8.7-mingw730-static-x64
cmake --build --preset qt4.8.7-mingw730-static-x64-release
ctest --preset qt4.8.7-mingw730-static-x64-release

cmake --preset qt4.8.7-msvc2015-dynamic-x64
cmake --build --preset qt4.8.7-msvc2015-dynamic-x64-release
ctest --preset qt4.8.7-msvc2015-dynamic-x64-release

cmake --preset qt4.8.7-msvc2015-dynamic-x86
cmake --build --preset qt4.8.7-msvc2015-dynamic-x86-release
ctest --preset qt4.8.7-msvc2015-dynamic-x86-release

cmake --preset qt4.8.7-msvc2015-static-x64
cmake --build --preset qt4.8.7-msvc2015-static-x64-release
ctest --preset qt4.8.7-msvc2015-static-x64-release

cmake --preset qt4.8.7-msvc2015-static-x86
cmake --build --preset qt4.8.7-msvc2015-static-x86-release
ctest --preset qt4.8.7-msvc2015-static-x86-release
```

Qt 4.8.7 support uses local Qt4 compatibility shims for APIs that were introduced in Qt 5, including JSON document handling, standard paths, lock files, and screen helpers.

## Static Qt / Single EXE

The normal Qt installer packages under `D:/Qt` are shared Qt builds. A single-file executable requires a separate static Qt build. Static Qt installations are kept outside git under `extern/qt-static/<variant>`.

Build and install a minimal static Qt for one variant:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 6.11.1 -Toolchain msvc -Architecture x64
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 6.11.1 -Toolchain mingw -Architecture x64
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 6.11.1 -Toolchain mingw -Architecture x86
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 6.8.3 -Toolchain msvc -Architecture x64
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 6.8.3 -Toolchain mingw -Architecture x64
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 6.8.3 -Toolchain mingw -Architecture x86
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 5.15.2 -Toolchain msvc -Architecture x64
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 5.15.2 -Toolchain msvc -Architecture x86
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 5.15.2 -Toolchain mingw -Architecture x64
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 5.15.2 -Toolchain mingw -Architecture x86
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 5.6.3 -Toolchain msvc -Architecture x86 -WindowsXp
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 5.6.3 -Toolchain msvc -Architecture x64
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 5.6.3 -Toolchain mingw -Architecture x86
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 5.6.3 -Toolchain mingw -Architecture x64
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 4.8.7 -Toolchain msvc -Architecture x64
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 4.8.7 -Toolchain msvc -Architecture x86
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 4.8.7 -Toolchain mingw -Architecture x86
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 4.8.7 -Toolchain mingw -Architecture x64
```

Then build the application against that static Qt:

```powershell
cmake --preset qt6.11.1-msvc2022-static-x64
cmake --build --preset qt6.11.1-msvc2022-static-x64-release
ctest --preset qt6.11.1-msvc2022-static-x64-release
```

The static presets set `HKM_REQUIRE_STATIC_QT=ON`, disable `windeployqt`, and import the required static Qt plugins into `HotKeyManager.exe`. MSVC static variants also set `HKM_STATIC_RUNTIME=ON` for `/MT`; MinGW static variants link with `-static -static-libgcc -static-libstdc++`.

## Qt 5.6.3 Legacy Builds

Build and install the XP-targeted dynamic Qt:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-qt-dynamic.ps1 -QtVersion 5.6.3 -Toolchain msvc -Architecture x86 -WindowsXp
```

Then build and test the XP dynamic application variant:

```powershell
cmake --preset qt5.6.3-msvc2015-xp-dynamic-x86
cmake --build --preset qt5.6.3-msvc2015-xp-dynamic-x86-release
ctest --preset qt5.6.3-msvc2015-xp-dynamic-x86-release
```

Build and install the XP-targeted static Qt:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 5.6.3 -Toolchain msvc -Architecture x86 -WindowsXp
```

Then build and test the XP static application variant:

```powershell
cmake --preset qt5.6.3-msvc2015-xp-static-x86
cmake --build --preset qt5.6.3-msvc2015-xp-static-x86-release
ctest --preset qt5.6.3-msvc2015-xp-static-x86-release
```

Expected output locations:

- Dynamic Qt: `extern/qt/5.6.3/msvc2015_xp`
- Dynamic application: `out/build/qt5.6.3-msvc2015-xp-dynamic-x86/HotKeyManager.exe`
- Static Qt: `extern/qt-static/qt5.6.3-msvc2015-xp-static-x86`
- Static application: `out/build/qt5.6.3-msvc2015-xp-static-x86/HotKeyManager.exe`

The XP variants are intentionally x86. They use the NMake generator in this environment because the full VS2015 MSBuild integration is not required for this target. The Qt XP builds disable PCH and qmake batch inference for this variant to avoid VS2015 compiler crashes seen during Qt 5.6.3 XP builds. The dynamic application preset deploys the minimal Qt DLL and plugin set manually because Qt 5.6.3 `windeployqt` can mis-detect XP-subsystem executables.

Additional Qt 5.6.3 ABI variants are available for coverage of older MSVC and MinGW deployments:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-qt-dynamic.ps1 -QtVersion 5.6.3 -Toolchain msvc -Architecture x86
powershell -ExecutionPolicy Bypass -File scripts/build-qt-dynamic.ps1 -QtVersion 5.6.3 -Toolchain msvc -Architecture x64
powershell -ExecutionPolicy Bypass -File scripts/build-qt-dynamic.ps1 -QtVersion 5.6.3 -Toolchain mingw -Architecture x86
powershell -ExecutionPolicy Bypass -File scripts/build-qt-dynamic.ps1 -QtVersion 5.6.3 -Toolchain mingw -Architecture x64
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 5.6.3 -Toolchain msvc -Architecture x86
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 5.6.3 -Toolchain msvc -Architecture x64
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 5.6.3 -Toolchain mingw -Architecture x86
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 5.6.3 -Toolchain mingw -Architecture x64
```

Qt 5.6.3 MinGW x86 uses MinGW 4.9.2. Qt 5.6.3 MinGW x64 uses MinGW 7.3.
