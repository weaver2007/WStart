# HotKeyManager

Windows tray utility for launching apps, files, folders, and URLs with global hotkeys.

## Scope

- C++17, Qt 6 Widgets, CMake, Win32 API.
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

## Visual Studio 2026 / MSVC

```powershell
cmake --preset vs2026-msvc
cmake --build --preset vs2026-msvc-release
ctest --preset vs2026-msvc-release
```

The `HotKeyManager` target runs `windeployqt` after build, so the required Qt DLLs and plugins are copied next to `HotKeyManager.exe`.

## Static Qt / Single EXE

The normal Qt installer package under `D:/Qt/6.10.1/msvc2022_64` is a shared Qt build. A single-file executable requires a separate static Qt build and the MSVC static runtime.

Build and install a minimal static Qt for this project:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-qt-static-msvc.ps1
```

Then build the application against that static Qt:

```powershell
cmake --preset vs2026-msvc-static
cmake --build --preset vs2026-msvc-static-release
ctest --preset vs2026-msvc-static-release
```

The static preset uses `qt-static-msvc`, sets `HKM_STATIC_RUNTIME=ON` for `/MT`, requires a static Qt installation, disables `windeployqt`, and imports the required static Qt plugins into `HotKeyManager.exe`.
