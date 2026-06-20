param(
    [string]$Version = "0.2.0",
    [string]$DynamicBuildPreset = "qt6.8.3-msvc2022-dynamic-x64-release",
    [string]$DynamicConfigurePreset = "qt6.8.3-msvc2022-dynamic-x64",
    [string]$StaticBuildPreset = "qt6.8.3-msvc2022-static-x64-release",
    [string]$StaticConfigurePreset = "qt6.8.3-msvc2022-static-x64",
    [string]$BinaryCreator = "D:\Qt\Tools\QtInstallerFramework\4.10\bin\binarycreator.exe",
    [string]$OutputDir = "out\release"
)

$ErrorActionPreference = "Stop"

function Resolve-RepoPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return Join-Path $repoRoot $Path
}

function Copy-DirectoryContents([string]$Source, [string]$Destination) {
    if (-not (Test-Path -LiteralPath $Source)) {
        throw "Missing source directory: $Source"
    }
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Copy-Item -Path (Join-Path $Source "*") -Destination $Destination -Recurse -Force
}

function Copy-RuntimePayload([string]$Source, [string]$Destination) {
    if (-not (Test-Path -LiteralPath $Source)) {
        throw "Missing runtime source directory: $Source"
    }

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null

    $exe = Join-Path $Source "WStart.exe"
    if (-not (Test-Path -LiteralPath $exe)) {
        $exe = Join-Path $Source "HotKeyManager.exe"
    }
    if (-not (Test-Path -LiteralPath $exe)) {
        throw "Missing runtime executable: $exe"
    }
    Copy-Item -LiteralPath $exe -Destination (Join-Path $Destination "WStart.exe") -Force

    Get-ChildItem -LiteralPath $Source -File -Filter "*.dll" |
        Copy-Item -Destination $Destination -Force

    $pluginDirs = @(
        "generic",
        "iconengines",
        "imageformats",
        "networkinformation",
        "platforms",
        "styles",
        "tls"
    )
    foreach ($pluginDir in $pluginDirs) {
        $sourceDir = Join-Path $Source $pluginDir
        if (Test-Path -LiteralPath $sourceDir) {
            $destDir = Join-Path $Destination $pluginDir
            New-Item -ItemType Directory -Force -Path $destDir | Out-Null
            Get-ChildItem -LiteralPath $sourceDir -File -Filter "*.dll" -ErrorAction SilentlyContinue |
                Copy-Item -Destination $destDir -Force
        }
    }
}

function Expand-Template([string]$Source, [string]$Destination, [hashtable]$Values) {
    $content = Get-Content -Raw -LiteralPath $Source
    foreach ($key in $Values.Keys) {
        $content = $content.Replace("@$key@", [string]$Values[$key])
    }
    $parent = Split-Path -Parent $Destination
    if ($parent) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    Set-Content -LiteralPath $Destination -Value $content -Encoding UTF8
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$outputRoot = Resolve-RepoPath $OutputDir
$installerName = "WStart-$Version-setup-qt6.8.3-msvc2022-dynamic-x64.exe"
$portableName = "WStart-$Version-portable-qt6.8.3-msvc2022-static-x64.zip"
$dynamicBuildDir = Join-Path $repoRoot "out\build\$DynamicConfigurePreset"
$staticBuildDir = Join-Path $repoRoot "out\build\$StaticConfigurePreset"
$dynamicReleaseDir = Join-Path $dynamicBuildDir "Release"
$staticReleaseDir = Join-Path $staticBuildDir "Release"

New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null

Write-Host "Building dynamic preset: $DynamicBuildPreset"
cmake --build --preset $DynamicBuildPreset

Write-Host "Building static preset: $StaticBuildPreset"
cmake --build --preset $StaticBuildPreset

$dynamicExe = Join-Path $dynamicReleaseDir "WStart.exe"
if (-not (Test-Path -LiteralPath $dynamicExe)) {
    $dynamicExe = Join-Path $dynamicBuildDir "WStart.exe"
    $dynamicReleaseDir = $dynamicBuildDir
}
if (-not (Test-Path -LiteralPath $dynamicExe)) {
    $dynamicExe = Join-Path $dynamicReleaseDir "HotKeyManager.exe"
}
if (-not (Test-Path -LiteralPath $dynamicExe)) {
    $dynamicExe = Join-Path $dynamicBuildDir "HotKeyManager.exe"
    $dynamicReleaseDir = $dynamicBuildDir
}
if (-not (Test-Path -LiteralPath $dynamicExe)) {
    throw "Dynamic executable was not found."
}

$staticExe = Join-Path $staticReleaseDir "WStart.exe"
if (-not (Test-Path -LiteralPath $staticExe)) {
    $staticExe = Join-Path $staticBuildDir "WStart.exe"
    $staticReleaseDir = $staticBuildDir
}
if (-not (Test-Path -LiteralPath $staticExe)) {
    $staticExe = Join-Path $staticReleaseDir "HotKeyManager.exe"
}
if (-not (Test-Path -LiteralPath $staticExe)) {
    $staticExe = Join-Path $staticBuildDir "HotKeyManager.exe"
    $staticReleaseDir = $staticBuildDir
}
if (-not (Test-Path -LiteralPath $staticExe)) {
    throw "Static executable was not found."
}

$ifwWork = Join-Path $outputRoot "ifw-work"
$ifwConfig = Join-Path $ifwWork "config"
$ifwPackage = Join-Path $ifwWork "packages\com.wstart.hotkeymanager"
$ifwData = Join-Path $ifwPackage "data"
$ifwMeta = Join-Path $ifwPackage "meta"
if (Test-Path -LiteralPath $ifwWork) {
    Remove-Item -LiteralPath $ifwWork -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $ifwConfig, $ifwData, $ifwMeta | Out-Null

Copy-RuntimePayload -Source $dynamicReleaseDir -Destination $ifwData
Copy-Item -LiteralPath (Join-Path $repoRoot "resources\app.ico") -Destination (Join-Path $ifwConfig "app.ico") -Force

$templateValues = @{
    VERSION = $Version
    RELEASE_DATE = (Get-Date -Format "yyyy-MM-dd")
}
Expand-Template -Source (Join-Path $repoRoot "packaging\ifw\config\config.xml.in") `
                -Destination (Join-Path $ifwConfig "config.xml") `
                -Values $templateValues
Expand-Template -Source (Join-Path $repoRoot "packaging\ifw\packages\com.wstart.hotkeymanager\meta\package.xml.in") `
                -Destination (Join-Path $ifwMeta "package.xml") `
                -Values $templateValues
Copy-Item -LiteralPath (Join-Path $repoRoot "packaging\ifw\packages\com.wstart.hotkeymanager\meta\installscript.qs") `
          -Destination (Join-Path $ifwMeta "installscript.qs") -Force

if (-not (Test-Path -LiteralPath $BinaryCreator)) {
    throw "binarycreator.exe was not found: $BinaryCreator"
}

$installerPath = Join-Path $outputRoot $installerName
Write-Host "Creating installer: $installerPath"
& $BinaryCreator --offline-only -c (Join-Path $ifwConfig "config.xml") -p (Join-Path $ifwWork "packages") $installerPath

$portableRoot = Join-Path $outputRoot "portable"
if (Test-Path -LiteralPath $portableRoot) {
    Remove-Item -LiteralPath $portableRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $portableRoot | Out-Null
Copy-Item -LiteralPath $staticExe -Destination (Join-Path $portableRoot "WStart.exe") -Force

$portablePath = Join-Path $outputRoot $portableName
if (Test-Path -LiteralPath $portablePath) {
    Remove-Item -LiteralPath $portablePath -Force
}
Write-Host "Creating portable archive: $portablePath"
Compress-Archive -Path (Join-Path $portableRoot "*") -DestinationPath $portablePath -Force

[PSCustomObject]@{
    Installer = $installerPath
    Portable = $portablePath
} | ConvertTo-Json -Depth 3
