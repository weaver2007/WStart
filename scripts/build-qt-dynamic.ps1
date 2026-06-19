param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("4.8.7", "5.6.3")]
    [string]$QtVersion,

    [Parameter(Mandatory = $true)]
    [ValidateSet("mingw", "msvc")]
    [string]$Toolchain,

    [string]$QtSourceDir,
    [string]$InstallDir,
    [string]$BuildDir,
    [string]$MingwRoot,
    [string]$Architecture = "x64",
    [switch]$WindowsXp,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path "$PSScriptRoot\..").Path

if ($WindowsXp -and $Toolchain -ne "msvc") {
    throw "-WindowsXp is only supported for the Qt 5.6.3 MSVC2015 x86 dynamic variant."
}
if ($QtVersion -eq "4.8.7" -and $Toolchain -eq "msvc" -and $Architecture -ne "x86") {
    Write-Host "Qt 4.8.7 MSVC builds are forced to x86 because Qt 4.8.7 ships a win32-msvc2015 mkspec."
    $Architecture = "x86"
}
if ($WindowsXp -and $Architecture -ne "x86") {
    Write-Host "Windows XP builds are forced to x86 because the maintained XP target uses VS2015/Win32."
    $Architecture = "x86"
}

function ConvertTo-CMakePath([string]$Path) {
    return $Path.Replace('\', '/')
}

function Set-ProcessPathValue([string]$Value) {
    if ([string]::IsNullOrWhiteSpace($Value)) {
        return
    }
    $env:PATH = $Value
    [Environment]::SetEnvironmentVariable("PATH", $Value, "Process")
    [Environment]::SetEnvironmentVariable("Path", $Value, "Process")
}

function Use-Vs2015CompatibleWindowsSdk() {
    $kitRoot = "C:\Program Files (x86)\Windows Kits\10"
    $sdkVersion = $null
    foreach ($candidate in @("10.0.10240.0", "10.0.19041.0")) {
        if ((Test-Path (Join-Path $kitRoot "include\$candidate\ucrt")) -and
            (Test-Path (Join-Path $kitRoot "include\$candidate\um\windows.h")) -and
            (Test-Path (Join-Path $kitRoot "lib\$candidate\ucrt\x86")) -and
            (Test-Path (Join-Path $kitRoot "lib\$candidate\um\x86"))) {
            $sdkVersion = $candidate
            break
        }
    }
    if (-not $sdkVersion) {
        return
    }
    foreach ($name in @("INCLUDE", "LIB")) {
        $value = [Environment]::GetEnvironmentVariable($name, "Process")
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            $value = $value.Replace("10.0.26100.0", $sdkVersion)
            [Environment]::SetEnvironmentVariable($name, $value, "Process")
        }
    }
}

function Resolve-QtSourceDirectory([string]$RequestedPath, [string]$Version) {
    if (-not [string]::IsNullOrWhiteSpace($RequestedPath) -and (Test-Path (Join-Path $RequestedPath "configure.exe"))) {
        return (Resolve-Path $RequestedPath).Path
    }
    if (-not [string]::IsNullOrWhiteSpace($RequestedPath) -and (Test-Path (Join-Path $RequestedPath "configure.bat"))) {
        return (Resolve-Path $RequestedPath).Path
    }

    $expandedSource = Join-Path $repoRoot "out\qt-src\qt-everywhere-opensource-src-$Version"
    if (Test-Path (Join-Path $expandedSource "configure.exe")) {
        return (Resolve-Path $expandedSource).Path
    }
    if (Test-Path (Join-Path $expandedSource "configure.bat")) {
        return (Resolve-Path $expandedSource).Path
    }

    $zipPath = Join-Path "D:\Qt\$Version\Src" "qt-everywhere-opensource-src-$Version.zip"
    if (Test-Path $zipPath) {
        New-Item -ItemType Directory -Force -Path (Split-Path $expandedSource -Parent) | Out-Null
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        [System.IO.Compression.ZipFile]::ExtractToDirectory($zipPath, (Split-Path $expandedSource -Parent))
        if (Test-Path (Join-Path $expandedSource "configure.exe")) {
            return (Resolve-Path $expandedSource).Path
        }
        if (Test-Path (Join-Path $expandedSource "configure.bat")) {
            return (Resolve-Path $expandedSource).Path
        }
    }

    throw "Qt source directory not found or does not contain configure: $RequestedPath"
}

function Import-Vs2015BuildEnvironment([string]$Arch) {
    $vcvarsCandidates = @(
        "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat",
        "D:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat",
        "C:\Program Files\Microsoft Visual Studio 14.0\VC\vcvarsall.bat",
        "D:\Program Files\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"
    ) | Where-Object { Test-Path $_ }
    if (-not $vcvarsCandidates) {
        throw "Visual Studio 2015 vcvarsall.bat not found."
    }

    foreach ($vcvars in $vcvarsCandidates) {
        $cmd = '"' + $vcvars + '" ' + $Arch + ' >nul && set'
        $environmentLines = cmd.exe /d /s /c $cmd 2>$null
        if ($LASTEXITCODE -ne 0) {
            continue
        }
        $environmentLines | ForEach-Object {
            if ($_ -match '^([^=]+)=(.*)$') {
                if ($matches[1] -ieq "PATH") {
                    Set-ProcessPathValue -Value $matches[2]
                } else {
                    [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
                }
            }
        }
        Use-Vs2015CompatibleWindowsSdk
        $sdkToolDirs = @(
            "C:\Program Files (x86)\Windows Kits\10\bin\10.0.10240.0\x86",
            "C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x86",
            "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x86",
            "C:\Program Files (x86)\Windows Kits\10\bin\x86",
            "C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Bin"
        ) | Where-Object { Test-Path (Join-Path $_ "rc.exe") }
        if ($sdkToolDirs) {
            Set-ProcessPathValue -Value "$($sdkToolDirs[0]);$env:PATH"
        }
        foreach ($tool in @("cl.exe", "link.exe", "lib.exe", "nmake.exe", "rc.exe", "mt.exe")) {
            if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
                throw "Required MSVC build tool not found after environment setup: $tool"
            }
        }
        Write-Host "Using Visual Studio 2015 environment: $vcvars $Arch"
        return
    }

    throw "Failed to initialize Visual Studio 2015 build environment."
}

function Update-Qt4MsvcMkspec([string]$SourcePath, [bool]$StaticRuntime) {
    $qmakeConf = Join-Path $SourcePath "mkspecs\win32-msvc2015\qmake.conf"
    if (-not (Test-Path $qmakeConf)) {
        throw "Qt 4.8.7 win32-msvc2015 mkspec not found: $qmakeConf"
    }

    $content = Get-Content -LiteralPath $qmakeConf -Raw
    if ($StaticRuntime) {
        $content = $content -replace '-MDd', '-MTd'
        $content = $content -replace '-MD', '-MT'
    } else {
        $content = $content -replace '-MTd', '-MDd'
        $content = $content -replace '-MT', '-MD'
    }
    if ($content -notmatch '(^|\r?\n)CONFIG\s*\+=.*\bno_batch\b') {
        $content = $content.TrimEnd() + "`r`nCONFIG += no_batch`r`n"
    }
    Set-Content -LiteralPath $qmakeConf -Value $content -Encoding ASCII
}

function Get-MakefileObjectList([string]$MakefilePath, [string]$VariableName) {
    $lines = Get-Content -LiteralPath $MakefilePath
    $items = New-Object System.Collections.Generic.List[string]
    $collecting = $false
    foreach ($line in $lines) {
        $text = $line
        if (-not $collecting) {
            if ($text -notmatch "^\s*$([regex]::Escape($VariableName))\s*=") {
                continue
            }
            $text = $text.Substring($text.IndexOf("=") + 1)
            $collecting = $true
        } elseif ($text -notmatch "^\s") {
            break
        }

        $continued = $text.TrimEnd().EndsWith("\")
        $text = $text.Trim().TrimEnd("\").Trim()
        foreach ($part in ($text -split "\s+")) {
            if ($part -match "\.obj$") {
                $items.Add($part)
            }
        }
        if (-not $continued) {
            break
        }
    }
    return $items
}

function Get-Qt4QmakeBootstrapSource([string]$BuildPath, [string]$SourcePath, [string]$ObjectName) {
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($ObjectName)
    foreach ($candidate in @(
        (Join-Path (Join-Path $BuildPath "qmake") "$baseName.cpp"),
        (Join-Path (Join-Path $BuildPath "qmake\generators") "$baseName.cpp"),
        (Join-Path (Join-Path $BuildPath "qmake\generators\unix") "$baseName.cpp"),
        (Join-Path (Join-Path $BuildPath "qmake\generators\win32") "$baseName.cpp"),
        (Join-Path (Join-Path $BuildPath "qmake\generators\mac") "$baseName.cpp"),
        (Join-Path (Join-Path $BuildPath "qmake\generators\symbian") "$baseName.cpp"),
        (Join-Path (Join-Path $BuildPath "qmake\generators\integrity") "$baseName.cpp")
    )) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    $map = @{
        "project.obj" = "qmake\project.cpp"
        "main.obj" = "qmake\main.cpp"
        "makefile.obj" = "qmake\generators\makefile.cpp"
        "unixmake.obj" = "qmake\generators\unix\unixmake.cpp"
        "unixmake2.obj" = "qmake\generators\unix\unixmake2.cpp"
        "mingw_make.obj" = "qmake\generators\win32\mingw_make.cpp"
        "option.obj" = "qmake\option.cpp"
        "winmakefile.obj" = "qmake\generators\win32\winmakefile.cpp"
        "projectgenerator.obj" = "qmake\generators\projectgenerator.cpp"
        "property.obj" = "qmake\property.cpp"
        "meta.obj" = "qmake\meta.cpp"
        "makefiledeps.obj" = "qmake\generators\makefiledeps.cpp"
        "metamakefile.obj" = "qmake\generators\metamakefile.cpp"
        "xmloutput.obj" = "qmake\generators\xmloutput.cpp"
        "pbuilder_pbx.obj" = "qmake\generators\mac\pbuilder_pbx.cpp"
        "borland_bmake.obj" = "qmake\generators\win32\borland_bmake.cpp"
        "msvc_nmake.obj" = "qmake\generators\win32\msvc_nmake.cpp"
        "msvc_vcproj.obj" = "qmake\generators\win32\msvc_vcproj.cpp"
        "msvc_vcxproj.obj" = "qmake\generators\win32\msvc_vcxproj.cpp"
        "msvc_objectmodel.obj" = "qmake\generators\win32\msvc_objectmodel.cpp"
        "msbuild_objectmodel.obj" = "qmake\generators\win32\msbuild_objectmodel.cpp"
        "symmake.obj" = "qmake\generators\symbian\symmake.cpp"
        "initprojectdeploy_symbian.obj" = "qmake\generators\symbian\initprojectdeploy_symbian.cpp"
        "symmake_abld.obj" = "qmake\generators\symbian\symmake_abld.cpp"
        "symmake_sbsv2.obj" = "qmake\generators\symbian\symmake_sbsv2.cpp"
        "symbiancommon.obj" = "qmake\generators\symbian\symbiancommon.cpp"
        "registry.obj" = "tools\shared\windows\registry.cpp"
        "epocroot.obj" = "tools\shared\symbian\epocroot.cpp"
        "gbuild.obj" = "qmake\generators\integrity\gbuild.cpp"
        "qfilesystemengine_win.obj" = "src\corelib\io\qfilesystemengine_win.cpp"
        "qfilesystemiterator_win.obj" = "src\corelib\io\qfilesystemiterator_win.cpp"
        "qfsfileengine_win.obj" = "src\corelib\io\qfsfileengine_win.cpp"
        "qsettings_win.obj" = "src\corelib\io\qsettings_win.cpp"
        "qsystemlibrary.obj" = "src\corelib\plugin\qsystemlibrary.cpp"
        "qvsnprintf.obj" = "src\corelib\tools\qvsnprintf.cpp"
        "qlocale_tools.obj" = "src\corelib\tools\qlocale_tools.cpp"
        "qlocale_win.obj" = "src\corelib\tools\qlocale_win.cpp"
    }
    if ($map.ContainsKey($ObjectName)) {
        return Join-Path $SourcePath $map[$ObjectName]
    }

    foreach ($dir in @(
        "src\corelib\tools",
        "src\corelib\io",
        "src\corelib\global",
        "src\corelib\codecs",
        "src\corelib\xml",
        "src\corelib\kernel",
        "src\corelib\plugin"
    )) {
        $candidate = Join-Path $SourcePath (Join-Path $dir "$baseName.cpp")
        if (Test-Path $candidate) {
            return $candidate
        }
    }
    return $null
}

function Invoke-Qt4MsvcCompileObject([string]$BuildPath, [string]$SourcePath, [string]$ObjectName) {
    $qmakeDir = Join-Path $BuildPath "qmake"
    $sourceFile = Get-Qt4QmakeBootstrapSource -BuildPath $BuildPath -SourcePath $SourcePath -ObjectName $ObjectName
    if (-not $sourceFile -or -not (Test-Path $sourceFile)) {
        Write-Host "Qt 4 qmake bootstrap source not found for $ObjectName"
        return $false
    }

    $includeArgs = @(
        "-I.",
        "-Igenerators",
        "-Igenerators\unix",
        "-Igenerators\win32",
        "-Igenerators\mac",
        "-Igenerators\symbian",
        "-Igenerators\integrity",
        "-I$BuildPath\include",
        "-I$BuildPath\include\QtCore",
        "-I$SourcePath\include",
        "-I$SourcePath\include\QtCore",
        "-I$BuildPath\src\corelib\global",
        "-I$BuildPath\src\corelib\xml",
        "-I$SourcePath\mkspecs\win32-msvc2015",
        "-I$SourcePath\tools\shared"
    )
    $defineArgs = @(
        "-DQT_NO_TEXTCODEC",
        "-DQT_NO_UNICODETABLES",
        "-DQT_LITE_COMPONENT",
        "-DQT_NODLL",
        "-DQT_NO_STL",
        "-DQT_NO_COMPRESS",
        "-DUNICODE",
        "-DHAVE_QCONFIG_CPP",
        "-DQT_BUILD_QMAKE",
        "-DQT_NO_THREAD",
        "-DQT_NO_QOBJECT",
        "-DQT_NO_GEOM_VARIANT",
        "-DQT_NO_DATASTREAM",
        "-DQT_NO_PCRE",
        "-DQT_BOOTSTRAPPED",
        "-DQLIBRARYINFO_EPOCROOT",
        "-DQMAKE_OPENSOURCE_EDITION"
    )
    $args = @(
        "-Yuqmake_pch.h",
        "-FIqmake_pch.h",
        "-Fpqmake_pch.pch",
        "-c",
        "-Fo$ObjectName",
        "-W3",
        "-nologo",
        "-O2"
    ) + $includeArgs + $defineArgs + $sourceFile

    Push-Location $qmakeDir
    try {
        Write-Host "Compiling Qt 4 qmake bootstrap object: $ObjectName"
        & cl @args | ForEach-Object { Write-Host $_ }
        return ($LASTEXITCODE -eq 0)
    }
    finally {
        Pop-Location
    }
}

function Invoke-Qt4MsvcLinkQmake([string]$BuildPath, [System.Collections.Generic.List[string]]$Objects) {
    $qmakeDir = Join-Path $BuildPath "qmake"
    $binDir = Join-Path $BuildPath "bin"
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null
    $linkArgs = @("-nologo", "-OUT:qmake.exe")
    foreach ($obj in $Objects) {
        $linkArgs += $obj
    }
    $linkArgs += @("qmake_pch.obj", "ole32.lib", "advapi32.lib")

    Push-Location $qmakeDir
    try {
        & link @linkArgs | ForEach-Object { Write-Host $_ }
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path "qmake.exe")) {
            return $false
        }
        Copy-Item -LiteralPath "qmake.exe" -Destination (Join-Path $binDir "qmake.exe") -Force
        return (Test-Path (Join-Path $binDir "qmake.exe"))
    }
    finally {
        Pop-Location
    }
}

function Invoke-Qt4MsvcQmakeBootstrap([string]$BuildPath) {
    $qmakeDir = Join-Path $BuildPath "qmake"
    $makefile = Join-Path $qmakeDir "Makefile"
    if (-not (Test-Path $makefile)) {
        Write-Host "Qt 4 qmake bootstrap Makefile not found: $makefile"
        return $false
    }
    $makefileText = Get-Content -LiteralPath $makefile -Raw
    if ($makefileText -match 'QMAKESPEC\s*=\s*win32-msvc2015-static') {
        $makefileText = $makefileText -replace 'QMAKESPEC\s*=\s*win32-msvc2015-static', 'QMAKESPEC = win32-msvc2015'
        Set-Content -LiteralPath $makefile -Value $makefileText -Encoding ASCII
    }

    Push-Location $qmakeDir
    try {
        Write-Host "Retrying Qt 4 qmake bootstrap with one-object-at-a-time MSVC compilation."
        Write-Host "cl.exe: $((Get-Command cl.exe -ErrorAction SilentlyContinue).Source)"
        & nmake qmake_pch.obj | ForEach-Object { Write-Host $_ }
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Qt 4 qmake bootstrap failed while building qmake_pch.obj."
            return $false
        }
        $objects = New-Object System.Collections.Generic.List[string]
        foreach ($obj in (Get-MakefileObjectList -MakefilePath $makefile -VariableName "OBJS")) {
            $objects.Add($obj)
        }
        foreach ($obj in (Get-MakefileObjectList -MakefilePath $makefile -VariableName "QTOBJS")) {
            $objects.Add($obj)
        }
        if ($objects.Count -eq 0) {
            Write-Host "Qt 4 qmake bootstrap object list is empty."
            return $false
        }
        foreach ($obj in $objects) {
            if (Test-Path $obj) {
                continue
            }
            if (-not (Invoke-Qt4MsvcCompileObject -BuildPath $BuildPath -SourcePath $qtSource -ObjectName $obj)) {
                return $false
            }
        }
        if (-not (Invoke-Qt4MsvcLinkQmake -BuildPath $BuildPath -Objects $objects)) {
            Write-Host "Qt 4 qmake bootstrap link failed."
            return $false
        }
        return $true
    }
    finally {
        Pop-Location
    }
}

function Set-Vs2015XpBuildEnvironment([string]$OriginalPath) {
    $programFilesX86 = ${env:ProgramFiles(x86)}
    $vsRoot = Join-Path $programFilesX86 "Microsoft Visual Studio 14.0"
    $vcRoot = Join-Path $vsRoot "VC"

    $ucrtVersion = "10.0.10240.0"
    $windowsKitsRoot = Join-Path $programFilesX86 "Windows Kits\10"
    $windowsSdk71Root = Join-Path $programFilesX86 "Microsoft SDKs\Windows\v7.1A"

    $requiredPaths = @(
        (Join-Path $vsRoot "Common7\IDE"),
        (Join-Path $vsRoot "Common7\Tools"),
        (Join-Path $vcRoot "BIN"),
        (Join-Path $vcRoot "INCLUDE"),
        (Join-Path $vcRoot "LIB"),
        (Join-Path $windowsKitsRoot "Include\$ucrtVersion\ucrt"),
        (Join-Path $windowsKitsRoot "Lib\$ucrtVersion\ucrt\x86"),
        (Join-Path $windowsSdk71Root "Include"),
        (Join-Path $windowsSdk71Root "Lib"),
        (Join-Path $windowsSdk71Root "Bin")
    )
    foreach ($path in $requiredPaths) {
        if (-not (Test-Path $path)) {
            throw "Required Windows XP SDK path not found: $path"
        }
    }

    $includePaths = @(
        (Join-Path $vcRoot "INCLUDE"),
        (Join-Path $vcRoot "ATLMFC\INCLUDE"),
        (Join-Path $windowsKitsRoot "Include\$ucrtVersion\ucrt"),
        (Join-Path $windowsSdk71Root "Include")
    ) | Where-Object { Test-Path $_ }

    $libPaths = @(
        (Join-Path $vcRoot "LIB"),
        (Join-Path $vcRoot "ATLMFC\LIB"),
        (Join-Path $windowsKitsRoot "Lib\$ucrtVersion\ucrt\x86"),
        (Join-Path $windowsSdk71Root "Lib")
    ) | Where-Object { Test-Path $_ }

    $libPathPaths = @(
        "C:\Windows\Microsoft.NET\Framework\v4.0.30319",
        (Join-Path $vcRoot "LIB"),
        (Join-Path $vcRoot "ATLMFC\LIB")
    ) | Where-Object { Test-Path $_ }

    $env:INCLUDE = $includePaths -join ';'
    $env:LIB = $libPaths -join ';'
    $env:LIBPATH = $libPathPaths -join ';'

    $pathParts = @(
        (Join-Path $vsRoot "Common7\IDE"),
        (Join-Path $vcRoot "BIN"),
        (Join-Path $vsRoot "Common7\Tools"),
        "C:\Windows\Microsoft.NET\Framework\v4.0.30319",
        "C:\Program Files (x86)\HTML Help Workshop",
        (Join-Path $windowsSdk71Root "Bin"),
        $OriginalPath
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    $env:PATH = $pathParts -join ';'

    $env:VSINSTALLDIR = "$vsRoot\"
    $env:VCINSTALLDIR = "$vcRoot\"
    $env:DevEnvDir = "$(Join-Path $vsRoot "Common7\IDE")\"
    $env:FrameworkDir = "C:\Windows\Microsoft.NET\Framework\"
    $env:FrameworkVersion = "v4.0.30319"
    $env:WindowsSdkDir = "$windowsSdk71Root\"
    $env:WindowsSDK_ExecutablePath = "$(Join-Path $windowsSdk71Root "Bin")\"
    $env:UniversalCRTSdkDir = "$windowsKitsRoot\"
    $env:UCRTVersion = $ucrtVersion
    $env:CL = $null
    $env:_CL_ = $null

    foreach ($tool in @("cl.exe", "link.exe", "lib.exe", "nmake.exe", "rc.exe", "mt.exe")) {
        if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
            throw "Required Windows XP build tool not found after environment setup: $tool"
        }
    }

    Write-Host "Using Visual Studio XP environment: VS2015 x86 + Windows SDK 7.1A + UCRT $ucrtVersion"
}

if ([string]::IsNullOrWhiteSpace($QtSourceDir)) {
    $QtSourceDir = "D:\Qt\$QtVersion\Src"
}
if ([string]::IsNullOrWhiteSpace($InstallDir)) {
    if ($QtVersion -eq "4.8.7") {
        if ($Toolchain -eq "msvc") {
            $InstallDir = Join-Path $repoRoot "extern\qt\$QtVersion\msvc2015_32"
        } else {
            $InstallDir = Join-Path $repoRoot "extern\qt\$QtVersion\mingw482_32"
        }
    } elseif ($Toolchain -eq "msvc" -and $WindowsXp) {
        $InstallDir = Join-Path $repoRoot "extern\qt\$QtVersion\msvc2015_xp"
    } else {
        $InstallDir = Join-Path $repoRoot "extern\qt\$QtVersion\mingw492_32"
    }
}
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    if ($QtVersion -eq "4.8.7") {
        if ($Toolchain -eq "msvc") {
            $BuildDir = Join-Path $repoRoot "out\qt-build\qt$QtVersion-msvc2015-dynamic-x86"
        } else {
            $BuildDir = Join-Path $repoRoot "out\qt-build\qt$QtVersion-mingw482-dynamic-x86"
        }
    } elseif ($Toolchain -eq "msvc" -and $WindowsXp) {
        $BuildDir = Join-Path $repoRoot "out\qt-build\qt$QtVersion-msvc2015-xp-dynamic-x86"
    } else {
        $BuildDir = Join-Path $repoRoot "out\qt-build\qt$QtVersion-mingw492-dynamic-x86"
    }
}
if ([string]::IsNullOrWhiteSpace($MingwRoot)) {
    if ($QtVersion -eq "4.8.7") {
        $MingwRoot = "D:\Qt\Tools\mingw482_32"
    } else {
        $MingwRoot = "D:\Qt\Tools\mingw492_32"
    }
}

$qtSource = Resolve-QtSourceDirectory -RequestedPath $QtSourceDir -Version $QtVersion
$installPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($InstallDir)
$buildPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BuildDir)
$installCMakePath = ConvertTo-CMakePath $installPath

if ($QtVersion -eq "4.8.7" -and $Toolchain -eq "msvc") {
    Update-Qt4MsvcMkspec -SourcePath $qtSource -StaticRuntime:$false
}

if ($Force) {
    $repoRootWithSeparator = $repoRoot.TrimEnd('\') + '\'
    foreach ($pathToRemove in @($buildPath, $installPath)) {
        if ((Test-Path $pathToRemove) -and $pathToRemove.StartsWith($repoRootWithSeparator, [System.StringComparison]::OrdinalIgnoreCase)) {
            Remove-Item -LiteralPath $pathToRemove -Recurse -Force
        } elseif (Test-Path $pathToRemove) {
            throw "Refusing to remove path outside repository: $pathToRemove"
        }
    }
}

$qmake = Join-Path $installPath "bin\qmake.exe"
if ((Test-Path $qmake) -and -not $Force) {
    Write-Host "Dynamic Qt already exists: $installPath"
    exit 0
}

New-Item -ItemType Directory -Force -Path $buildPath | Out-Null
New-Item -ItemType Directory -Force -Path $installPath | Out-Null

$oldPath = $env:PATH
if ($Toolchain -eq "mingw") {
    $mingwBin = Join-Path $MingwRoot "bin"
    $env:PATH = "$mingwBin;$oldPath"
} elseif ($WindowsXp) {
    Set-Vs2015XpBuildEnvironment -OriginalPath $oldPath
} elseif ($Toolchain -eq "msvc" -and $QtVersion -eq "4.8.7") {
    Import-Vs2015BuildEnvironment -Arch "x86"
} else {
    throw "Only the MSVC Windows XP dynamic variant is currently supported by this script."
}

Push-Location $buildPath
try {
    $configureName = if ($QtVersion -eq "4.8.7") { "configure.exe" } else { "configure.bat" }
    $configure = Join-Path $qtSource $configureName

    if ($QtVersion -eq "4.8.7") {
        $configureArgs = @(
            "-release",
            "-shared",
            "-opensource",
            "-confirm-license",
            "-prefix", $installCMakePath,
            "-nomake", "examples",
            "-nomake", "demos",
            "-nomake", "docs",
            "-nomake", "translations",
            "-no-qt3support",
            "-no-phonon",
            "-no-phonon-backend",
            "-no-webkit",
            "-no-script",
            "-no-scripttools",
            "-no-declarative",
            "-no-opengl",
            "-no-openssl",
            "-no-dbus",
            "-no-xmlpatterns",
            "-no-multimedia",
            "-no-audio-backend",
            "-no-native-gestures",
            "-platform", $(if ($Toolchain -eq "msvc") { "win32-msvc2015" } else { "win32-g++" })
        )
    } else {
        $configureArgs = @(
            "-release",
            "-shared",
            "-opensource",
            "-confirm-license",
            "-prefix", $installCMakePath,
            "-nomake", "examples",
            "-nomake", "tests",
            "-skip", "qt3d",
            "-skip", "qtactiveqt",
            "-skip", "qtconnectivity",
            "-skip", "qtdeclarative",
            "-skip", "qtdoc",
            "-skip", "qtimageformats",
            "-skip", "qtlocation",
            "-skip", "qtmultimedia",
            "-skip", "qtquickcontrols",
            "-skip", "qtquickcontrols2",
            "-skip", "qtscript",
            "-skip", "qtsensors",
            "-skip", "qtserialbus",
            "-skip", "qtserialport",
            "-skip", "qttools",
            "-skip", "qttranslations",
            "-skip", "qtwayland",
            "-skip", "qtwebchannel",
            "-skip", "qtwebengine",
            "-skip", "qtwebsockets",
            "-skip", "qtwebview",
            "-skip", "qtwinextras",
            "-skip", "qtxmlpatterns",
            "-no-icu",
            "-no-opengl",
            "-no-angle",
            "-no-openssl",
            "-no-pch"
        )
        if ($WindowsXp) {
            $configureArgs += @("-target", "xp")
        }
        if ($Toolchain -eq "mingw") {
            $configureArgs += @("-platform", "win32-g++")
        } else {
            $configureArgs += @("-platform", "win32-msvc2015")
        }
    }

    & $configure @configureArgs
    $configureExitCode = $LASTEXITCODE
    if ($QtVersion -eq "4.8.7" -and $Toolchain -eq "msvc") {
        $qmakePath = Join-Path $buildPath "bin\qmake.exe"
        if ($configureExitCode -ne 0 -or -not (Test-Path $qmakePath)) {
            Write-Host "Qt configure did not produce qmake.exe (exit code $configureExitCode); trying Qt 4 MSVC qmake bootstrap fallback."
            if (-not (Invoke-Qt4MsvcQmakeBootstrap -BuildPath $buildPath)) {
                throw "Qt 4 qmake bootstrap fallback failed."
            }
            & $configure @configureArgs
            $configureExitCode = $LASTEXITCODE
        }
        $global:LASTEXITCODE = $configureExitCode
    }
    if ($configureExitCode -ne 0) {
        throw "Qt configure failed with exit code $configureExitCode."
    }

    if ($QtVersion -eq "4.8.7" -and $Toolchain -eq "msvc") {
        $qmodulePri = Join-Path $buildPath "src\corelib\global\qmodule.pri"
        if (Test-Path $qmodulePri) {
            Add-Content -LiteralPath $qmodulePri -Value "CONFIG += no_batch"
        }
    }

    if ($WindowsXp -and $Toolchain -eq "msvc") {
        $qmodulePri = Join-Path $buildPath "qtbase\mkspecs\qmodule.pri"
        if (-not (Test-Path $qmodulePri)) {
            throw "Qt qmodule.pri was not generated: $qmodulePri"
        }
        Add-Content -LiteralPath $qmodulePri -Value "CONFIG += no_batch"
    }

    $makeTool = if ($Toolchain -eq "mingw") { "mingw32-make" } else { "nmake" }

    if ($QtVersion -eq "4.8.7") {
        if ($Toolchain -eq "mingw") {
            & $makeTool "-j$([Environment]::ProcessorCount)" "sub-src"
        } else {
            & $makeTool "sub-src"
        }
        if ($LASTEXITCODE -ne 0) {
            throw "Qt build failed with exit code $LASTEXITCODE."
        }

        $qt4InstallTargets = @(
            "install_qmake",
            "install_mkspecs",
            "sub-tools-bootstrap-install_subtargets",
            "sub-moc-install_subtargets",
            "sub-rcc-install_subtargets",
            "sub-uic-install_subtargets",
            "sub-winmain-install_subtargets",
            "sub-corelib-install_subtargets",
            "sub-xml-install_subtargets",
            "sub-network-install_subtargets",
            "sub-sql-install_subtargets",
            "sub-testlib-install_subtargets",
            "sub-gui-install_subtargets",
            "sub-svg-install_subtargets",
            "sub-plugins-install_subtargets"
        )
        foreach ($target in $qt4InstallTargets) {
            & $makeTool $target
            if ($LASTEXITCODE -ne 0) {
                throw "Qt install target '$target' failed with exit code $LASTEXITCODE."
            }
        }

        Write-Host "Dynamic Qt installed to: $installPath"
        exit 0
    } elseif ($Toolchain -eq "mingw") {
        & $makeTool "-j$([Environment]::ProcessorCount)"
    } else {
        & $makeTool
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Qt build failed with exit code $LASTEXITCODE."
    }
    & $makeTool install
    if ($LASTEXITCODE -ne 0) {
        throw "Qt install failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
    $env:PATH = $oldPath
}

Write-Host "Dynamic Qt installed to: $installPath"
