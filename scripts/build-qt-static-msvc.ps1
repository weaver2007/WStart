param(
    [string]$QtSourceDir = "D:\Qt\6.10.1\Src",
    [string]$InstallDir = (Join-Path (Resolve-Path "$PSScriptRoot\..").Path "qt-static-msvc"),
    [string]$BuildDir = (Join-Path (Resolve-Path "$PSScriptRoot\..").Path "build-qt-static-msvc"),
    [string]$Generator = "Visual Studio 18 2026",
    [string]$Architecture = "x64"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path "$PSScriptRoot\..").Path
$qtSource = (Resolve-Path $QtSourceDir).Path
$installPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($InstallDir)
$buildPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BuildDir)

New-Item -ItemType Directory -Force -Path $buildPath | Out-Null
New-Item -ItemType Directory -Force -Path $installPath | Out-Null

Push-Location $buildPath
try {
    $cmakeConfigureArgs = @("-DCMAKE_INSTALL_PREFIX=$installPath")
    if ($Generator -like "Visual Studio*") {
        $cmakeConfigureArgs += @("-A", $Architecture)
    }

    $configureArgs = @(
        "-release",
        "-static",
        "-static-runtime",
        "-opensource",
        "-confirm-license",
        "-force-bundled-libs",
        "-prefix", $installPath,
        "-submodules", "qtbase,qtsvg",
        "-nomake", "examples",
        "-nomake", "tests",
        "-no-icu",
        "-no-feature-winsdkicu",
        "-no-opengl",
        "-no-openssl",
        "-schannel",
        "-cmake-generator", $Generator,
        "--"
    ) + $cmakeConfigureArgs

    & (Join-Path $qtSource "configure.bat") @configureArgs

    if ($LASTEXITCODE -ne 0) {
        throw "Qt configure failed with exit code $LASTEXITCODE."
    }

    $buildArgs = @("--build", ".", "--parallel")
    if ($Generator -like "Visual Studio*") {
        $buildArgs += @("--config", "Release")
    }
    cmake @buildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Qt build failed with exit code $LASTEXITCODE."
    }

    $installArgs = @("--install", ".")
    if ($Generator -like "Visual Studio*") {
        $installArgs += @("--config", "Release")
    }
    cmake @installArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Qt install failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

Write-Host "Static Qt installed to: $installPath"
Write-Host "Build the application with: cmake --preset vs2026-msvc-static"
Write-Host "Then: cmake --build --preset vs2026-msvc-static-release"
