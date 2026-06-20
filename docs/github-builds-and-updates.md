# GitHub Builds And Updates

## Build workflows

The workflow in `.github/workflows/build.yml` builds the GitHub-hosted UI matrix:

- Windows / Qt 6.8.3 / MSVC / dynamic Qt / x64
- macOS / Qt 6.8.3 / clang / dynamic Qt / arm64 runner
- Linux / Qt 6.8.3 / gcc / dynamic Qt / x64

The workflow uploads a Windows zip artifact for each successful run. macOS and Linux currently run configure, build,
and tests only; packaging is intentionally deferred until their native behavior and distribution targets are defined.

The full local variant matrix remains represented by `CMakePresets.json`. Running every variant in GitHub Actions would
require a custom image or a self-hosted Windows runner with the same local Qt/toolchain layout referenced by the presets,
including legacy Qt 4.8.7, Qt 5.6.3 XP builds, static Qt builds, MSVC toolsets, and MinGW toolchains.

At this stage, macOS and Linux are UI-first targets. They are expected to display the launcher and manage rules, but
global hotkeys remain disabled until platform-specific backends are implemented.

## Releases

`.github/workflows/release.yml` publishes the primary variant when a tag matching `v*` is pushed:

```powershell
git tag v0.2.0
git push origin v0.2.0
```

It can also be started manually with `workflow_dispatch` and a tag input. Normal pushes to `main` do not create tags or
releases automatically.

## Update manifest

Release builds receive this manifest URL from GitHub Actions:

```text
https://raw.githubusercontent.com/<owner>/<repo>/main/update.json
```

`update.json` should be updated after publishing a release:

```json
{
  "version": "0.1.1",
  "downloadUrl": "https://github.com/<owner>/<repo>/releases/download/v0.2.0/WStart-qt6.8.3-msvc2022-dynamic-x64.zip",
  "sha256": "<optional sha256>",
  "releaseNotes": "Short release notes shown in WStart."
}
```

The application checks this JSON with `QNetworkAccessManager`. When a newer version is found, it prompts the user and opens the download page or URL. It does not replace the running executable silently.
