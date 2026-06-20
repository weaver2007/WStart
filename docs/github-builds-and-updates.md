# GitHub Builds And Updates

## Build workflows

The workflow in `.github/workflows/build.yml` has two build paths:

- `hosted-qt6-smoke` runs on GitHub-hosted `windows-latest` and installs Qt 6 dynamically with `aqtinstall`.
- `all-variants` runs on a self-hosted Windows runner with labels `self-hosted`, `Windows`, and `hstart-build`.

The full variant matrix uses the release presets from `CMakePresets.json`. That runner must have the same local Qt and toolchain layout referenced by the presets, including legacy Qt 4.8.7, Qt 5.6.3 XP builds, static Qt builds, MSVC toolsets, and MinGW toolchains.

## Update manifest

Release builds receive this manifest URL from GitHub Actions:

```text
https://raw.githubusercontent.com/<owner>/<repo>/main/update.json
```

`update.json` should be updated after publishing a release:

```json
{
  "version": "0.1.1",
  "downloadUrl": "https://github.com/<owner>/<repo>/releases/download/v0.1.1/HotKeyManager-qt6.11.1-msvc2022-dynamic-x64.zip",
  "sha256": "<optional sha256>",
  "releaseNotes": "Short release notes shown in HStart."
}
```

The application checks this JSON with `QNetworkAccessManager`. When a newer version is found, it prompts the user and opens the download page or URL. It does not replace the running executable silently.
