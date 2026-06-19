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
Dynamic Qt variants run `windeployqt` after build, so the required Qt DLLs and plugins are copied next to `HotKeyManager.exe`.

Currently maintained variants:

- `qt4.8.7-mingw482-dynamic-x86`
- `qt4.8.7-mingw482-static-x86`
- `qt5.6.3-mingw492-dynamic-x86`
- `qt5.6.3-mingw492-static-x86`
- `qt5.15.2-msvc2019-dynamic-x64`
- `qt5.15.2-mingw81-dynamic-x64`
- `qt6.8.3-msvc2022-dynamic-x64`
- `qt6.8.3-mingw13-dynamic-x64`
- `qt6.10.1-msvc2022-dynamic-x64`
- `qt6.10.1-mingw13-dynamic-x64`
- matching `*-static-x64` variants for the same Qt/toolchain pairs

The `qt5.6.3-msvc2015-*` presets are defined, but they require a VS2015/v140 toolchain and a matching Qt 5.6.3 MSVC2015 build under `extern/qt/5.6.3/msvc2015_64` or `extern/qt-static/qt5.6.3-msvc2015-static-x64`.

Windows XP is maintained as separate Qt 5.6.3 MSVC2015 x86 variants:

- `qt5.6.3-msvc2015-xp-dynamic-x86`
- `qt5.6.3-msvc2015-xp-static-x86`

This variant uses the VS2015 x86 compiler with XP-targeted Windows SDK libraries, builds Qt with `-target xp`, sets `WINVER/_WIN32_WINNT=0x0501`, links the MSVC runtime statically with `/MT`, and sets the executable subsystem version to `5.01`.

## Qt 5.6.3 MinGW

Qt 5.6.3 MinGW uses Qt's legacy 32-bit MinGW 4.9.2 toolchain:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-qt-dynamic.ps1 -QtVersion 5.6.3 -Toolchain mingw
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 5.6.3 -Toolchain mingw
```

Then build and test the application:

```powershell
cmake --preset qt5.6.3-mingw492-dynamic-x86
cmake --build --preset qt5.6.3-mingw492-dynamic-x86-release
ctest --preset qt5.6.3-mingw492-dynamic-x86-release

cmake --preset qt5.6.3-mingw492-static-x86
cmake --build --preset qt5.6.3-mingw492-static-x86-release
ctest --preset qt5.6.3-mingw492-static-x86-release
```

## Qt 4.8.7 MinGW

Qt 4.8.7 uses a dedicated compatibility path and the legacy 32-bit MinGW 4.8.2 toolchain:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-qt-dynamic.ps1 -QtVersion 4.8.7 -Toolchain mingw
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 4.8.7 -Toolchain mingw
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
```

Qt 4.8.7 support is maintained as a 32-bit MinGW compatibility target. It uses local Qt4 compatibility shims for APIs that were introduced in Qt 5, including JSON document handling, standard paths, lock files, and screen helpers.

## Static Qt / Single EXE

The normal Qt installer packages under `D:/Qt` are shared Qt builds. A single-file executable requires a separate static Qt build. Static Qt installations are kept outside git under `extern/qt-static/<variant>`.

Build and install a minimal static Qt for one variant:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 6.10.1 -Toolchain msvc
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 6.10.1 -Toolchain mingw
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 6.8.3 -Toolchain msvc
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 6.8.3 -Toolchain mingw
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 5.6.3 -Toolchain mingw
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 5.15.2 -Toolchain msvc
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static.ps1 -QtVersion 5.15.2 -Toolchain mingw
```

Then build the application against that static Qt:

```powershell
cmake --preset qt6.10.1-msvc2022-static-x64
cmake --build --preset qt6.10.1-msvc2022-static-x64-release
ctest --preset qt6.10.1-msvc2022-static-x64-release
```

The static presets set `HKM_REQUIRE_STATIC_QT=ON`, disable `windeployqt`, and import the required static Qt plugins into `HotKeyManager.exe`. MSVC static variants also set `HKM_STATIC_RUNTIME=ON` for `/MT`; MinGW static variants link with `-static -static-libgcc -static-libstdc++`.

## Qt 5.6.3 MSVC2015 Windows XP

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
