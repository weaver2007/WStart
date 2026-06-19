param(
    [string]$QtSourceDir = "D:\Qt\6.10.1\Src",
    [string]$InstallDir = (Join-Path (Resolve-Path "$PSScriptRoot\..").Path "extern\qt-static\qt6.10.1-msvc2022-static-x64"),
    [string]$BuildDir = (Join-Path (Resolve-Path "$PSScriptRoot\..").Path "out\qt-build\qt6.10.1-msvc2022-static-x64"),
    [string]$Generator = "Visual Studio 18 2026",
    [string]$Architecture = "x64",
    [switch]$Force
)

& (Join-Path $PSScriptRoot "build-qt-static.ps1") `
    -QtVersion "6.10.1" `
    -Toolchain "msvc" `
    -QtSourceDir $QtSourceDir `
    -InstallDir $InstallDir `
    -BuildDir $BuildDir `
    -Generator $Generator `
    -Architecture $Architecture `
    -Force:$Force
