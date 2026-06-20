param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("4.8.7", "5.6.3", "5.15.2", "6.8.3", "6.11.1")]
    [string]$QtVersion,

    [Parameter(Mandatory = $true)]
    [ValidateSet("msvc", "mingw")]
    [string]$Toolchain,

    [string]$QtSourceDir,
    [string]$InstallDir,
    [string]$BuildDir,
    [string]$MingwRoot,
    [string]$MingwTriplet,
    [string]$Generator,
    [string]$Architecture = "x64",
    [switch]$WindowsXp,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path "$PSScriptRoot\..").Path

if ($WindowsXp -and ($QtVersion -ne "5.6.3" -or $Toolchain -ne "msvc")) {
    throw "-WindowsXp is only supported for the Qt 5.6.3 MSVC2015 x86 static variant."
}
if ($WindowsXp -and $Toolchain -eq "msvc" -and $Architecture -ne "x86") {
    Write-Host "Windows XP builds are forced to x86 because the maintained XP target uses v140_xp/Win32."
    $Architecture = "x86"
}

function ConvertTo-CMakePath([string]$Path) {
    return $Path.Replace('\', '/')
}

function Test-QtConfigLineContainsStatic([string]$Line) {
    if ($Line -notmatch '^\s*(QT_CONFIG|CONFIG|QT\.global\.enabled_features)[^=]*=') {
        return $false
    }
    return ($Line -match '(^|[\s;])static([\s;]|$)')
}

function Test-QtStaticInstall([string]$InstallPath) {
    $qmake = Join-Path $InstallPath "bin\qmake.exe"
    if (Test-Path $qmake) {
        $qtConfig = & $qmake -query QT_CONFIG 2>$null
        if ($qtConfig -match '(^|[\s;])static($|[\s;])') {
            return $true
        }
    }

    $qconfig = Join-Path $InstallPath "mkspecs\qconfig.pri"
    if (Test-Path $qconfig) {
        foreach ($line in Get-Content -LiteralPath $qconfig) {
            if (Test-QtConfigLineContainsStatic $line) {
                return $true
            }
        }
    }

    foreach ($targetsFile in @(
        (Join-Path $InstallPath "lib\cmake\Qt6Core\Qt6CoreTargets.cmake"),
        (Join-Path $InstallPath "lib\cmake\Qt5Core\Qt5CoreTargets.cmake")
    )) {
        if (Test-Path $targetsFile) {
            if (Select-String -Path $targetsFile -Pattern 'QT_QMAKE_PUBLIC_CONFIG.*(^|[\s;])static([\s;]|$)' -Quiet) {
                return $true
            }
        }
    }

    return $false
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

function Add-ProcessEnvironmentPaths([string]$Name, [string[]]$Paths) {
    $items = New-Object System.Collections.Generic.List[string]
    foreach ($path in $Paths) {
        if (-not [string]::IsNullOrWhiteSpace($path) -and (Test-Path $path) -and -not $items.Contains($path)) {
            $items.Add($path)
        }
    }

    $current = [Environment]::GetEnvironmentVariable($Name, "Process")
    if (-not [string]::IsNullOrWhiteSpace($current)) {
        foreach ($path in ($current -split ';')) {
            if (-not [string]::IsNullOrWhiteSpace($path) -and -not $items.Contains($path)) {
                $items.Add($path)
            }
        }
    }

    $value = $items -join ';'
    [Environment]::SetEnvironmentVariable($Name, $value, "Process")
    Set-Item -Path "Env:$Name" -Value $value
}

function Get-WindowsSdkVersion() {
    $kitRoot = "C:\Program Files (x86)\Windows Kits\10"
    $includeRoot = Join-Path $kitRoot "Include"
    if (-not (Test-Path $includeRoot)) {
        return $null
    }

    $versions = Get-ChildItem -LiteralPath $includeRoot -Directory |
        Where-Object {
            (Test-Path (Join-Path $_.FullName "ucrt")) -and
            (Test-Path (Join-Path $_.FullName "um\windows.h"))
        } |
        Sort-Object Name -Descending
    if ($versions.Count -eq 0) {
        return $null
    }
    return $versions[0].Name
}

function Ensure-ModernMsvcEnvironment([string]$Arch) {
    $clCommand = Get-Command cl.exe -ErrorAction SilentlyContinue
    if (-not $clCommand) {
        throw "cl.exe was not found after Visual Studio environment setup."
    }
    $clPath = $clCommand.Source
    if ($clPath -notmatch '^(.*\\VC\\Tools\\MSVC\\[^\\]+)\\bin\\') {
        throw "Unable to locate MSVC tool root from cl.exe path: $clPath"
    }

    $msvcRoot = $matches[1]
    $targetArch = if ($Arch -eq "x86") { "x86" } else { "x64" }
    $sdkVersion = Get-WindowsSdkVersion
    if (-not $sdkVersion) {
        throw "Unable to locate a Windows 10 SDK include directory."
    }
    [Environment]::SetEnvironmentVariable("WindowsSDKVersion", "$sdkVersion\", "Process")
    Set-Item -Path "Env:WindowsSDKVersion" -Value "$sdkVersion\"

    $kitRoot = "C:\Program Files (x86)\Windows Kits\10"
    Add-ProcessEnvironmentPaths -Name "INCLUDE" -Paths @(
        (Join-Path $msvcRoot "include"),
        (Join-Path $msvcRoot "atlmfc\include"),
        (Join-Path $kitRoot "Include\$sdkVersion\ucrt"),
        (Join-Path $kitRoot "Include\$sdkVersion\shared"),
        (Join-Path $kitRoot "Include\$sdkVersion\um"),
        (Join-Path $kitRoot "Include\$sdkVersion\winrt"),
        (Join-Path $kitRoot "Include\$sdkVersion\cppwinrt")
    )

    Add-ProcessEnvironmentPaths -Name "LIB" -Paths @(
        (Join-Path $msvcRoot "lib\$targetArch"),
        (Join-Path $msvcRoot "atlmfc\lib\$targetArch"),
        (Join-Path $kitRoot "Lib\$sdkVersion\ucrt\$targetArch"),
        (Join-Path $kitRoot "Lib\$sdkVersion\um\$targetArch")
    )

    $frameworkRoot = if ($targetArch -eq "x86") {
        "C:\Windows\Microsoft.NET\Framework\v4.0.30319"
    } else {
        "C:\Windows\Microsoft.NET\Framework64\v4.0.30319"
    }
    Add-ProcessEnvironmentPaths -Name "LIBPATH" -Paths @(
        $frameworkRoot,
        (Join-Path $msvcRoot "lib\$targetArch"),
        (Join-Path $msvcRoot "atlmfc\lib\$targetArch")
    )

    $initializerList = Join-Path $msvcRoot "include\initializer_list"
    if (-not (Test-Path $initializerList)) {
        throw "MSVC standard library header not found: $initializerList"
    }
}

function Get-ToolchainTag([string]$Version, [string]$Kind, [string]$Arch) {
    if ($Kind -eq "msvc") {
        if ($Version -eq "4.8.7") {
            return "msvc2015"
        }
        if ($Version -eq "5.6.3") {
            return "msvc2015"
        }
        if ($Version.StartsWith("5.")) {
            return "msvc2019"
        }
        return "msvc2022"
    }

    if ($Version -eq "4.8.7") {
        return $(if ($Arch -eq "x86") { "mingw482" } else { "mingw730" })
    }
    if ($Version -eq "5.6.3") {
        return $(if ($Arch -eq "x86") { "mingw492" } else { "mingw730" })
    }
    if ($Version.StartsWith("6.") -and $Arch -eq "x86") {
        return "llvm-mingw17"
    }
    if ($Version.StartsWith("5.")) {
        return "mingw81"
    }
    return "mingw13"
}

function Get-DefaultMingwRoot([string]$Version, [string]$Arch) {
    if ($Version -eq "4.8.7") {
        return $(if ($Arch -eq "x86") { "D:\Qt\Tools\mingw482_32" } else { "D:\Qt\Tools\mingw730_64" })
    }
    if ($Version -eq "5.6.3") {
        return $(if ($Arch -eq "x86") { "D:\Qt\Tools\mingw492_32" } else { "D:\Qt\Tools\mingw730_64" })
    }
    if ($Version -eq "5.15.2") {
        return $(if ($Arch -eq "x86") { "D:\Qt\Tools\mingw810_32" } else { "D:\Qt\Tools\mingw810_64" })
    }
    if ($Version.StartsWith("6.")) {
        return $(if ($Arch -eq "x86") { "D:\Qt\Tools\llvm-mingw1706_64" } else { "D:\Qt\Tools\mingw1310_64" })
    }
    return "D:\Qt\Tools\mingw810_64"
}

function Get-DefaultMingwTriplet([string]$Version, [string]$Arch) {
    if ($Version.StartsWith("6.") -and $Arch -eq "x86") {
        return "i686-w64-mingw32"
    }
    return ""
}

function Get-MingwToolPath([string]$ToolName) {
    if (-not [string]::IsNullOrWhiteSpace($MingwTriplet)) {
        $tripletTool = Join-Path $MingwRoot "bin\$MingwTriplet-$ToolName.exe"
        if (Test-Path $tripletTool) {
            return $tripletTool
        }
    }
    $tool = Join-Path $MingwRoot "bin\$ToolName.exe"
    if (Test-Path $tool) {
        return $tool
    }
    throw "Unable to locate $ToolName.exe in $MingwRoot\bin."
}

function Get-MingwRuntimePathEntries() {
    $paths = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($MingwTriplet)) {
        $tripletBin = Join-Path $MingwRoot "$MingwTriplet\bin"
        if (Test-Path $tripletBin) {
            $paths.Add($tripletBin)
        }
    }
    $bin = Join-Path $MingwRoot "bin"
    if (Test-Path $bin) {
        $paths.Add($bin)
    }
    return $paths
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

    $externSource = Join-Path $repoRoot "extern\src\qt-everywhere-src-$Version"
    if (Test-Path (Join-Path $externSource "configure.exe")) {
        return (Resolve-Path $externSource).Path
    }
    if (Test-Path (Join-Path $externSource "configure.bat")) {
        return (Resolve-Path $externSource).Path
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

function Update-Qt4MinGWSourceCompatibility([string]$SourcePath) {
    $itemViews = Join-Path $SourcePath "src\plugins\accessible\widgets\itemviews.cpp"
    if (-not (Test-Path $itemViews)) {
        return
    }

    $content = Get-Content -LiteralPath $itemViews -Raw
    $old = "QItemSelectionModel::Columns & QItemSelectionModel::Deselect"
    $new = "QItemSelectionModel::Columns | QItemSelectionModel::Deselect"
    if ($content.Contains($old)) {
        $content = $content.Replace($old, $new)
        Set-Content -LiteralPath $itemViews -Value $content -Encoding ASCII
    }
}

function Ensure-Qt4MsvcStaticMkspec([string]$SourcePath) {
    $sourceSpec = Join-Path $SourcePath "mkspecs\win32-msvc2015"
    if (-not (Test-Path (Join-Path $sourceSpec "qmake.conf"))) {
        throw "Qt 4.8.7 win32-msvc2015 mkspec not found: $sourceSpec"
    }

    $qmakeConf = Join-Path $sourceSpec "qmake.conf"
    $content = Get-Content -LiteralPath $qmakeConf -Raw
    if (-not $script:Qt4MsvcOriginalQmakeConfPath) {
        $script:Qt4MsvcOriginalQmakeConfPath = $qmakeConf
        $script:Qt4MsvcOriginalQmakeConfContent = $content
    }
    $content = $content -replace '-MDd', '-MTd'
    $content = $content -replace '-MD', '-MT'
    if ($content -notmatch '(^|\r?\n)CONFIG\s*\+=.*\bno_batch\b') {
        $content = $content.TrimEnd() + "`r`nCONFIG += no_batch`r`n"
    }
    Set-Content -LiteralPath $qmakeConf -Value $content -Encoding ASCII
    return "win32-msvc2015"
}

function Update-Qt563MsvcStaticMkspec([string]$SourcePath) {
    $msvcDesktopConf = Join-Path $SourcePath "qtbase\mkspecs\common\msvc-desktop.conf"
    if (-not (Test-Path $msvcDesktopConf)) {
        throw "Qt 5.6.3 msvc-desktop.conf not found: $msvcDesktopConf"
    }

    $content = Get-Content -LiteralPath $msvcDesktopConf -Raw
    if (-not $script:Qt563MsvcOriginalMsvcDesktopConfPath) {
        $script:Qt563MsvcOriginalMsvcDesktopConfPath = $msvcDesktopConf
        $script:Qt563MsvcOriginalMsvcDesktopConfContent = $content
    }

    $content = $content -replace '\s+\bembed_manifest_exe\b', ''
    $content = $content -replace '(?m)^QMAKE_LFLAGS_EXE\s*=.*$', 'QMAKE_LFLAGS_EXE        = /MANIFEST:NO'
    Set-Content -LiteralPath $msvcDesktopConf -Value $content -Encoding ASCII
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

function Get-MakefileVariableValue([string]$MakefilePath, [string]$VariableName, [string]$DefaultValue) {
    foreach ($line in (Get-Content -LiteralPath $MakefilePath)) {
        if ($line -match "^\s*$([regex]::Escape($VariableName))\s*=\s*(.+?)\s*$") {
            return $matches[1]
        }
    }
    return $DefaultValue
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

function Invoke-Qt4MsvcCompileObject([string]$BuildPath, [string]$SourcePath, [string]$Mkspec, [string]$ObjectName) {
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
        "-I$SourcePath\mkspecs\$Mkspec",
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
        $mkspec = Get-MakefileVariableValue -MakefilePath $makefile -VariableName "QMAKESPEC" -DefaultValue "win32-msvc2015"
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
            if (-not (Invoke-Qt4MsvcCompileObject -BuildPath $BuildPath -SourcePath $qtSource -Mkspec $mkspec -ObjectName $obj)) {
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

function Get-VsWherePath() {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        return $vswhere
    }
    return $null
}

function Get-VcVarsAllCandidates() {
    $candidates = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($env:HKM_VCVARSALL) -and (Test-Path $env:HKM_VCVARSALL)) {
        $candidates.Add($env:HKM_VCVARSALL)
    }

    foreach ($path in @(
        "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat",
        "D:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat",
        "C:\Program Files\Microsoft Visual Studio 14.0\VC\vcvarsall.bat",
        "D:\Program Files\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"
    )) {
        if (Test-Path $path) {
            $candidates.Add($path)
        }
    }

    $vswhere = Get-VsWherePath
    if ($vswhere) {
        $installations = & $vswhere -all -products * -property installationPath 2>$null
        foreach ($installation in $installations) {
            $vcvars = Join-Path $installation "VC\Auxiliary\Build\vcvarsall.bat"
            if (Test-Path $vcvars) {
                $candidates.Add($vcvars)
            }
        }
    }

    foreach ($path in @(
        "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat",
        "D:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
    )) {
        if (Test-Path $path) {
            $candidates.Add($path)
        }
    }

    return $candidates | Select-Object -Unique
}

function Import-VsDevEnvironment([string]$Version, [string]$Arch) {
    $vcvarsCandidates = Get-VcVarsAllCandidates
    if (-not $vcvarsCandidates) {
        throw "Visual Studio vcvarsall.bat not found. Install the required MSVC build tools or set HKM_VCVARSALL."
    }
    if ($Version.StartsWith("6.")) {
        $vcvarsCandidates = $vcvarsCandidates |
            Sort-Object @{ Expression = { if ($_ -match "\\Microsoft Visual Studio\\18\\") { 0 } else { 1 } } }, @{ Expression = { $_ } }
    }

    $requestedArch = $Arch
    if ($Version -eq "5.6.3" -and $Arch -eq "x64") {
        $requestedArch = "amd64"
    }

    $vcvarsVersion = $null
    if ($Version -eq "5.15.2") {
        $vcvarsVersion = "14.29"
    }

    foreach ($vcvars in $vcvarsCandidates) {
        $vcvarsArgs = @($requestedArch)
        if ($vcvarsVersion) {
            $vcvarsArgs += "-vcvars_ver=$vcvarsVersion"
        }

        $cmd = '"' + $vcvars + '" ' + ($vcvarsArgs -join ' ') + ' >nul && set'
        $environmentLines = cmd.exe /d /s /c $cmd 2>$null
        if ($LASTEXITCODE -ne 0 -and $Version -eq "5.6.3" -and $vcvarsVersion) {
            $cmd = '"' + $vcvars + '" ' + $requestedArch + ' >nul && set'
            $environmentLines = cmd.exe /d /s /c $cmd 2>$null
        }

        if ($LASTEXITCODE -eq 0) {
            $pathCandidates = New-Object System.Collections.Generic.List[string]
            $environmentLines | ForEach-Object {
                if ($_ -match '^([^=]+)=(.*)$') {
                    $key = $matches[1]
                    $value = $matches[2]
                    if ($key -ieq "PATH") {
                        $pathCandidates.Add($value)
                    } else {
                        [Environment]::SetEnvironmentVariable($key, $value, "Process")
                    }
                }
            }
            $selectedPath = $pathCandidates |
                Where-Object { $_ -match [regex]::Escape("Microsoft Visual Studio 14.0\VC\BIN") -or $_ -match "\\VC\\Tools\\MSVC\\" } |
                Select-Object -First 1
            if (-not $selectedPath -and $pathCandidates.Count -gt 0) {
                $selectedPath = $pathCandidates[0]
            }
            if ($selectedPath) {
                Set-ProcessPathValue -Value $selectedPath
            }
            if ($Version.StartsWith("6.")) {
                Ensure-ModernMsvcEnvironment -Arch $Arch
            }
            if ($Version -eq "5.6.3") {
                Use-Vs2015CompatibleWindowsSdk
            }
            $requiredTools = if ($Version.StartsWith("5.")) { @("cl.exe", "link.exe", "nmake.exe") } else { @("cl.exe", "link.exe") }
            $missingTools = @()
            foreach ($tool in $requiredTools) {
                if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
                    $missingTools += $tool
                }
            }
            if ($missingTools.Count -eq 0) {
                Write-Host "Using Visual Studio environment: $vcvars"
                return
            }
            Write-Host "Skipping incomplete Visual Studio environment from $vcvars. Missing: $($missingTools -join ', ')"
        }
    }

    throw "Failed to initialize Visual Studio build environment for Qt $Version."
}

function Import-Vs2015BuildEnvironment([string]$Arch) {
    if ($Arch -eq "x64") {
        $Arch = "amd64"
    }

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

$toolchainTag = Get-ToolchainTag -Version $QtVersion -Kind $Toolchain -Arch $Architecture
$platformSuffix = if ($WindowsXp) { "-xp" } else { "" }

if ([string]::IsNullOrWhiteSpace($QtSourceDir)) {
    $QtSourceDir = "D:\Qt\$QtVersion\Src"
}
if ([string]::IsNullOrWhiteSpace($InstallDir)) {
    $archTag = if ($Architecture -eq "x86") { "x86" } else { "x64" }
    if ($WindowsXp -and $Toolchain -eq "msvc") {
        $archTag = "x86"
    }
    $InstallDir = Join-Path $repoRoot "extern\qt-static\qt$QtVersion-$toolchainTag$platformSuffix-static-$archTag"
}
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $archTag = if ($Architecture -eq "x86") { "x86" } else { "x64" }
    if ($WindowsXp -and $Toolchain -eq "msvc") {
        $archTag = "x86"
    }
    $BuildDir = Join-Path $repoRoot "out\qt-build\qt$QtVersion-$toolchainTag$platformSuffix-static-$archTag"
}
if ([string]::IsNullOrWhiteSpace($Generator)) {
    if ($Toolchain -eq "msvc") {
        $Generator = if ($QtVersion.StartsWith("6.")) { "Ninja" } else { "Visual Studio 18 2026" }
    } else {
        $Generator = "Ninja"
    }
}
if ($Toolchain -eq "mingw" -and [string]::IsNullOrWhiteSpace($MingwRoot)) {
    $MingwRoot = Get-DefaultMingwRoot -Version $QtVersion -Arch $Architecture
}
if ($Toolchain -eq "mingw" -and [string]::IsNullOrWhiteSpace($MingwTriplet)) {
    $MingwTriplet = Get-DefaultMingwTriplet -Version $QtVersion -Arch $Architecture
}

$qtSource = Resolve-QtSourceDirectory -RequestedPath $QtSourceDir -Version $QtVersion
$installPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($InstallDir)
$buildPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BuildDir)
$installCMakePath = ConvertTo-CMakePath $installPath
$buildCMakePath = ConvertTo-CMakePath $buildPath

if ($QtVersion -eq "4.8.7" -and $Toolchain -eq "mingw") {
    Update-Qt4MinGWSourceCompatibility -SourcePath $qtSource
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
    if (Test-QtStaticInstall $installPath) {
        Write-Host "Static Qt already exists: $installPath"
        exit 0
    }
}

New-Item -ItemType Directory -Force -Path $buildPath | Out-Null
New-Item -ItemType Directory -Force -Path $installPath | Out-Null

$oldPath = $env:PATH
if ($Toolchain -eq "mingw") {
    $ninjaBin = "D:\Qt\Tools\Ninja"
    $mingwPaths = Get-MingwRuntimePathEntries
    $env:PATH = "$($mingwPaths -join ';');$ninjaBin;$oldPath"
} else {
    if ($QtVersion -eq "4.8.7") {
        Import-Vs2015BuildEnvironment -Arch $Architecture
    } elseif ($WindowsXp -and $QtVersion -eq "5.6.3") {
        Set-Vs2015XpBuildEnvironment -OriginalPath $oldPath
        Write-Host "XP INCLUDE=$env:INCLUDE"
        Write-Host "XP LIB=$env:LIB"
        Write-Host "XP PATH=$env:PATH"
    } else {
        $vsArchitecture = if ($WindowsXp -and $Architecture -eq "x86") { "x86" } else { $Architecture }
        Import-VsDevEnvironment -Version $QtVersion -Arch $vsArchitecture
    }
}
if ($Generator -eq "Ninja") {
    $ninjaBin = "D:\Qt\Tools\Ninja"
    if (Test-Path (Join-Path $ninjaBin "ninja.exe")) {
        Set-ProcessPathValue -Value "$ninjaBin;$env:PATH"
    }
}

Push-Location $buildPath
try {
    $configureName = if ($QtVersion -eq "4.8.7") { "configure.exe" } else { "configure.bat" }
    $configure = Join-Path $qtSource $configureName

    if ($QtVersion -eq "4.8.7") {
        $configureArgs = @(
            "-release",
            "-static",
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
            "-platform", $(if ($Toolchain -eq "msvc") { Ensure-Qt4MsvcStaticMkspec -SourcePath $qtSource } else { "win32-g++" })
        )

        & $configure @configureArgs
        $configureExitCode = $LASTEXITCODE
        if ($Toolchain -eq "msvc") {
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

        if ($Toolchain -eq "msvc") {
            $qmodulePri = Join-Path $buildPath "src\corelib\global\qmodule.pri"
            if (Test-Path $qmodulePri) {
                Add-Content -LiteralPath $qmodulePri -Value "CONFIG += no_batch"
            }
        }

        $makeTool = if ($Toolchain -eq "msvc") { "nmake" } else { "mingw32-make" }
        if ($Toolchain -eq "msvc") {
            & $makeTool "sub-src"
        } else {
            & $makeTool "-j$([Environment]::ProcessorCount)" "sub-src"
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
    } elseif ($QtVersion.StartsWith("5.")) {
        $skipModules = @(
            "qt3d",
            "qtactiveqt",
            "qtcharts",
            "qtconnectivity",
            "qtdatavis3d",
            "qtdeclarative",
            "qtdoc",
            "qtgamepad",
            "qtgraphicaleffects",
            "qtimageformats",
            "qtlocation",
            "qtlottie",
            "qtmultimedia",
            "qtnetworkauth",
            "qtpurchasing",
            "qtquick3d",
            "qtquickcontrols",
            "qtquickcontrols2",
            "qtquicktimeline",
            "qtremoteobjects",
            "qtscript",
            "qtscxml",
            "qtsensors",
            "qtserialbus",
            "qtserialport",
            "qtspeech",
            "qttools",
            "qttranslations",
            "qtvirtualkeyboard",
            "qtwayland",
            "qtwebchannel",
            "qtwebengine",
            "qtwebglplugin",
            "qtwebsockets",
            "qtwebview",
            "qtwinextras",
            "qtxmlpatterns"
        )
        if ($QtVersion -eq "5.6.3") {
            $skipModules = @(
                "qt3d",
                "qtactiveqt",
                "qtconnectivity",
                "qtdeclarative",
                "qtdoc",
                "qtimageformats",
                "qtlocation",
                "qtmultimedia",
                "qtquickcontrols",
                "qtquickcontrols2",
                "qtscript",
                "qtsensors",
                "qtserialbus",
                "qtserialport",
                "qttools",
                "qttranslations",
                "qtwayland",
                "qtwebchannel",
                "qtwebengine",
                "qtwebsockets",
                "qtwebview",
                "qtwinextras",
                "qtxmlpatterns"
            )
        }

        $configureArgs = @(
            "-release",
            "-static",
            "-static-runtime",
            "-opensource",
            "-confirm-license",
            "-prefix", $installCMakePath,
            "-nomake", "examples",
            "-nomake", "tests",
            "-no-icu",
            "-no-opengl",
            "-no-openssl",
            "-no-pch"
        )
        if ($WindowsXp) {
            $configureArgs += @("-target", "xp")
        }
        foreach ($module in $skipModules) {
            $configureArgs += @("-skip", $module)
        }

        if ($QtVersion -eq "5.6.3") {
            $configureArgs += @("-no-angle")
        } else {
            $configureArgs += @("-schannel")
        }

        if ($Toolchain -eq "mingw") {
            $configureArgs += @("-platform", "win32-g++")
        } else {
            $msvcPlatform = if ($QtVersion -eq "5.6.3") { "win32-msvc2015" } else { "win32-msvc" }
            $configureArgs += @("-platform", $msvcPlatform)
        }

        & $configure @configureArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Qt configure failed with exit code $LASTEXITCODE."
        }

        if ($QtVersion -eq "5.6.3" -and $Toolchain -eq "msvc") {
            $qmodulePri = Join-Path $buildPath "qtbase\mkspecs\qmodule.pri"
            if (-not (Test-Path $qmodulePri)) {
                throw "Qt qmodule.pri was not generated: $qmodulePri"
            }
            Add-Content -LiteralPath $qmodulePri -Value "CONFIG += no_batch"
        }

        $useNmakeForXpMsvc = $WindowsXp -and $QtVersion -eq "5.6.3" -and $Toolchain -eq "msvc"
        $makeTool = if ($Toolchain -eq "mingw") {
            "mingw32-make"
        } elseif ($useNmakeForXpMsvc) {
            "nmake"
        } else {
            "D:\Qt\Tools\QtCreator\bin\jom\jom.exe"
        }

        if ($useNmakeForXpMsvc) {
            & $makeTool
        } else {
            $makeJobs = [Environment]::ProcessorCount
            & $makeTool "-j$makeJobs"
        }
        if ($LASTEXITCODE -ne 0) {
            throw "Qt build failed with exit code $LASTEXITCODE."
        }
        & $makeTool install
        if ($LASTEXITCODE -ne 0) {
            throw "Qt install failed with exit code $LASTEXITCODE."
        }
    } else {
        $cmakeConfigureArgs = @("-DCMAKE_INSTALL_PREFIX=$installCMakePath")
        if ($Toolchain -eq "msvc" -and $Generator -like "Visual Studio*") {
            $vsCmakeArchitecture = if ($Architecture -eq "x86") { "Win32" } else { $Architecture }
            $cmakeConfigureArgs += @("-A", $vsCmakeArchitecture)
            $sdkVersion = Get-WindowsSdkVersion
            if (-not $sdkVersion) {
                throw "Unable to locate a Windows 10 SDK include directory."
            }
            $cmakeConfigureArgs += "-DCMAKE_SYSTEM_VERSION=$sdkVersion"
        }
        if ($Toolchain -eq "mingw") {
            $mingwCc = ConvertTo-CMakePath (Get-MingwToolPath "gcc")
            $mingwCxx = ConvertTo-CMakePath (Get-MingwToolPath "g++")
            $mingwRc = ConvertTo-CMakePath (Get-MingwToolPath "windres")
            $cmakeConfigureArgs += @(
                "-DCMAKE_C_COMPILER=$mingwCc",
                "-DCMAKE_CXX_COMPILER=$mingwCxx",
                "-DCMAKE_RC_COMPILER=$mingwRc"
            )
        }

        $qt6CompatibilityArgs = @()
        if ($QtVersion -eq "6.11.1") {
            $qt6CompatibilityArgs = @("-force-bundled-libs", "-no-feature-winsdkicu")
        }
        if ($QtVersion.StartsWith("6.") -and $Toolchain -eq "msvc" -and $Architecture -eq "x86") {
            $qt6CompatibilityArgs += "-no-feature-windows-ioring"
        }

        $configureArgs = @(
            "-release",
            "-static",
            "-static-runtime",
            "-opensource",
            "-confirm-license"
        ) + $qt6CompatibilityArgs + @(
            "-prefix", $installCMakePath,
            "-submodules", "qtbase,qtsvg",
            "-nomake", "examples",
            "-nomake", "tests",
            "-no-icu",
            "-no-opengl",
            "-no-openssl",
            "-schannel",
            "-cmake-generator", $Generator,
            "--"
        ) + $cmakeConfigureArgs

        & $configure @configureArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Qt configure failed with exit code $LASTEXITCODE."
        }

        $buildArgs = @("--build", ".", "--parallel")
        if ($Toolchain -eq "msvc" -and $Generator -like "Visual Studio*") {
            $buildArgs += @("--config", "Release")
        }
        cmake @buildArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Qt build failed with exit code $LASTEXITCODE."
        }

        $installArgs = @("--install", ".")
        if ($Toolchain -eq "msvc" -and $Generator -like "Visual Studio*") {
            $installArgs += @("--config", "Release")
        }
        cmake @installArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Qt install failed with exit code $LASTEXITCODE."
        }
    }
}
finally {
    Pop-Location
    if ($script:Qt4MsvcOriginalQmakeConfPath -and $script:Qt4MsvcOriginalQmakeConfContent) {
        Set-Content -LiteralPath $script:Qt4MsvcOriginalQmakeConfPath -Value $script:Qt4MsvcOriginalQmakeConfContent -Encoding ASCII
    }
    if ($script:Qt563MsvcOriginalMsvcDesktopConfPath -and $script:Qt563MsvcOriginalMsvcDesktopConfContent) {
        Set-Content -LiteralPath $script:Qt563MsvcOriginalMsvcDesktopConfPath -Value $script:Qt563MsvcOriginalMsvcDesktopConfContent -Encoding ASCII
    }
    $env:PATH = $oldPath
}

Write-Host "Static Qt installed to: $installPath"
