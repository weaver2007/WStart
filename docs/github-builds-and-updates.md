# GitHub Builds And Updates

## Build workflows

The workflow in `.github/workflows/build.yml` builds the GitHub-hosted UI matrix:

- Windows / Qt 6.8.3 / MSVC / dynamic Qt / x64
- macOS / Qt 6.8.3 / clang / dynamic Qt / x64
- Linux / Qt 6.8.3 / gcc / dynamic Qt / x64

The CI workflow uploads a Windows zip artifact for successful main builds. Release packaging is handled separately by
`.github/workflows/release-quick.yml` and `.github/workflows/release.yml`.

The full local variant matrix remains represented by `CMakePresets.json`. Running every variant in GitHub Actions would
require a custom image or a self-hosted Windows runner with the same local Qt/toolchain layout referenced by the presets,
including legacy Qt 4.8.7, Qt 5.6.3 XP builds, static Qt builds, MSVC toolsets, and MinGW toolchains.

At this stage, macOS and Linux are UI-first targets. They are expected to display the launcher and manage rules, but
global hotkeys remain disabled until platform-specific backends are implemented.

## Releases

`.github/workflows/release-quick.yml` publishes the default low-cost release when a tag matching `v*` is pushed:

```powershell
git tag v0.3.0
git push origin v0.3.0
```

The quick release builds and publishes only:

- `WStart-<version>-windows-x64-portable.zip`
- `update.json`

This quick channel is intended for low-cost portable distribution. Installed builds need the full release workflow to
publish installer assets.

`.github/workflows/release.yml` is the full release workflow. It is started manually with `workflow_dispatch` and a tag
input when all platform packages are needed. Normal pushes to `main` do not create tags or releases automatically.

To run the full release for an existing tag, open GitHub Actions, choose `Full Release WStart`, click `Run workflow`, and
enter the tag, for example `v0.3.0`. The full workflow can update the same GitHub Release created by the quick workflow
and replace `update.json` with the complete asset list.

The full release workflow currently attaches these assets to the GitHub Release:

- `WStart-<version>-windows-x64-setup.exe`
- `WStart-<version>-windows-x64-portable.zip`
- `WStart-<version>-windows-arm64-setup.exe`
- `WStart-<version>-windows-arm64-portable.zip`
- `WStart-<version>-macos-x64.dmg`
- `WStart-<version>-macos-x64-portable.zip`
- `WStart-<version>-macos-arm64.dmg`
- `WStart-<version>-macos-arm64-portable.zip`
- `WStart-<version>-linux-x64.deb`
- `WStart-<version>-linux-x64-portable.tar.gz`
- `WStart-<version>-linux-arm64.deb`
- `WStart-<version>-linux-arm64-portable.tar.gz`

The Windows installer is an offline Qt Installer Framework executable. The Windows portable package is a dynamic Qt zip
containing `WStart.exe`, Qt DLLs, plugins, and required runtime DLLs.

The macOS installer package is an unsigned `.dmg` generated from `WStart.app` with `macdeployqt`. The macOS portable
package is a zipped `WStart.app`.

The Linux installer package is a `.deb` containing the same portable payload under `/opt/WStart` plus a desktop file and
SVG icon. The Linux portable `.tar.gz` contains the installed executable, Qt libraries/plugins copied from the
GitHub-hosted Qt installation, the desktop file, the SVG icon, and a `wstart.sh` launcher script.

After publishing the versioned release, the workflow marks that release as the repository's GitHub latest release. The
release contains the newest distribution packages plus `update.json`; WStart uses GitHub's latest release API as its
stable update channel.

## Update manifest

Release builds receive this manifest URL from GitHub Actions:

```text
https://api.github.com/repos/<owner>/<repo>/releases/latest
```

For private repositories, WStart reads this URL with a user-provided fine-grained GitHub PAT. The token should have
read-only access to repository contents/releases. WStart stores the token in the local credential store on Windows.

WStart first reads the latest release JSON, finds the `update.json` release asset, then downloads and parses that asset.
The `update.json` file is not stored in the source tree.

`update.json` uses platform-specific assets:

```json
{
  "version": "0.3.5",
  "pageUrl": "https://github.com/<owner>/<repo>/releases/tag/v0.3.5",
  "releaseNotes": "Short release notes shown in WStart.",
  "assets": [
    {
      "platform": "windows",
      "arch": "x64",
      "type": "installer",
      "fileName": "WStart-0.3.5-windows-x64-setup.exe",
      "url": "https://github.com/<owner>/<repo>/releases/download/v0.3.5/WStart-0.3.5-windows-x64-setup.exe",
      "sha256": "<sha256>"
    },
    {
      "platform": "windows",
      "arch": "x64",
      "type": "portable",
      "fileName": "WStart-0.3.5-windows-x64-portable.zip",
      "url": "https://github.com/<owner>/<repo>/releases/download/v0.3.5/WStart-0.3.5-windows-x64-portable.zip",
      "sha256": "<sha256>"
    }
  ]
}
```

The application checks this JSON with `QNetworkAccessManager`. When a newer version is found, installed builds download
and open the installer package. Portable Windows builds download the portable zip, start `WStartUpdater.exe`, exit, and
let the updater replace the portable directory and restart WStart. Portable packages include a `WStart.portable` marker
file so the runtime can distinguish portable builds from installed builds.
