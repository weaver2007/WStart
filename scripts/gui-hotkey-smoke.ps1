param(
    [string]$ExePath = (Join-Path $PSScriptRoot '..\out\build\qt6.8.3-msvc2022-dynamic-x64\WStart.exe'),
    [string]$ConfigPath = (Join-Path $env:LOCALAPPDATA 'WStart\WStart\rules.json'),
    [switch]$StrictStartMenuCheck,
    [switch]$KeepWStartRunning
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-Step([string]$Message) { Write-Host "[WStart GUI smoke] $Message" }

function Add-NativeApis {
    if ('WStartSmoke.Native' -as [type]) { return }
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
namespace WStartSmoke {
  public static class Native {
    [StructLayout(LayoutKind.Sequential)] public struct INPUT { public uint type; public KEYBDINPUT ki; }
    [StructLayout(LayoutKind.Sequential)] public struct KEYBDINPUT { public ushort wVk; public ushort wScan; public uint dwFlags; public uint time; public UIntPtr dwExtraInfo; }
    [DllImport("user32.dll", SetLastError=true)] public static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);
    public const uint INPUT_KEYBOARD = 1;
    public const uint KEYEVENTF_EXTENDEDKEY = 0x0001;
    public const uint KEYEVENTF_KEYUP = 0x0002;
    public static void SendKey(ushort vk, bool up, bool ext) {
      INPUT[] inputs = new INPUT[1];
      inputs[0].type = INPUT_KEYBOARD;
      inputs[0].ki.wVk = vk;
      inputs[0].ki.dwFlags = (up ? KEYEVENTF_KEYUP : 0) | (ext ? KEYEVENTF_EXTENDEDKEY : 0);
      uint sent = SendInput(1, inputs, Marshal.SizeOf(typeof(INPUT)));
      if (sent != 1) throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
    }
  }
}
"@
}

function Send-Key([ushort]$Vk, [bool]$Up = $false, [bool]$Extended = $false) {
    [WStartSmoke.Native]::SendKey($Vk, $Up, $Extended)
    Start-Sleep -Milliseconds 45
}

function Send-WinShiftF24 {
    Add-NativeApis
    Send-Key 0x5B $false $true
    Send-Key 0xA0 $false $false
    Send-Key 0x87 $false $false
    Send-Key 0x87 $true $false
    Send-Key 0xA0 $true $false
    Send-Key 0x5B $true $true
}

function Send-BareWinAndCheckStartMenu {
    Add-NativeApis
    Send-Key 0x5B $false $true
    Send-Key 0x5B $true $true
    Start-Sleep -Milliseconds 700
    try {
        Add-Type -AssemblyName UIAutomationClient | Out-Null
        Add-Type -AssemblyName UIAutomationTypes | Out-Null
        $root = [System.Windows.Automation.AutomationElement]::RootElement
        $children = $root.FindAll([System.Windows.Automation.TreeScope]::Children, [System.Windows.Automation.Condition]::TrueCondition)
        foreach ($child in $children) {
            $name = $child.Current.Name
            $className = $child.Current.ClassName
            $startText = [string]([char]0x5F00) + [string]([char]0x59CB)
            $startMenuText = $startText + [string]([char]0x83DC) + [string]([char]0x5355)
            $looksLikeStartMenu = (
                $name -eq 'Start' -or
                $name -eq $startText -or
                $name -match 'Start menu' -or
                $name -eq $startMenuText -or
                $className -match 'Start|Windows\.UI\.Core\.CoreWindow|XamlExplorerHostIslandWindow'
            )
            if ($looksLikeStartMenu) {
                Send-Key 0x1B $false $false
                Send-Key 0x1B $true $false
                return [pscustomobject]@{ Visible = $true; Name = $name; ClassName = $className }
            }
        }
    } catch {
        Write-Warning "Start menu best-effort check failed: $($_.Exception.Message)"
    }
    Send-Key 0x1B $false $false
    Send-Key 0x1B $true $false
    return [pscustomobject]@{ Visible = $false; Name = ''; ClassName = '' }
}

function Write-TestConfig([string]$Path, [string]$MarkerPath) {
    $directory = Split-Path -Parent $Path
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $markerForPowerShell = $MarkerPath -replace "'", "''"
    $payload = "Set-Content -LiteralPath '$markerForPowerShell' -Value triggered -Encoding UTF8"
    $encodedPayload = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($payload))
    $arguments = "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -EncodedCommand $encodedPayload"

    $document = [ordered]@{
        version = 2
        settings = [ordered]@{
            language = 'zh-CN'
            hotkeysEnabled = $true
            updatesEnabled = $false
            startupEnabled = $false
            themeMode = 'system'
        }
        sections = @(
            [ordered]@{
                id = 'program-user'
                category = 'Program'
                name = 'GUI Smoke'
                iconKey = 'app'
                sortOrder = 1
                encrypted = $false
                passwordHash = ''
                collapsed = $false
            }
        )
        rules = @(
            [ordered]@{
                id = 'gui-hotkey-smoke'
                enabled = $true
                category = 'Program'
                sectionId = 'program-user'
                hotkey = [ordered]@{ modifiers = 12; key = 135; displayText = 'Win+Shift+F24' }
                action = [ordered]@{
                    type = 'Application'
                    target = 'powershell.exe'
                    arguments = $arguments
                    workingDirectory = ''
                    windowState = 'Minimized'
                    singleInstance = $false
                }
                description = 'GUI hotkey smoke'
            }
        )
    }
    $document | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Path -Encoding UTF8
}

if (-not (Test-Path -LiteralPath $ExePath)) { throw "WStart.exe not found: $ExePath" }

$markerPath = Join-Path $env:TEMP ('wstart-hotkey-smoke-{0}.txt' -f ([guid]::NewGuid().ToString('N')))
$backupPath = $null
$configExisted = Test-Path -LiteralPath $ConfigPath
if ($configExisted) {
    $backupPath = "$ConfigPath.gui-smoke.bak.$((Get-Date).ToString('yyyyMMddHHmmss'))"
}

try {
    Write-Step 'Stopping existing WStart processes'
    Get-Process -Name WStart -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 1

    if ($configExisted) {
        Write-Step "Backing up config to $backupPath"
        Copy-Item -LiteralPath $ConfigPath -Destination $backupPath -Force
    }

    Write-Step "Writing temporary config: Win+Shift+F24 -> marker file"
    Write-TestConfig -Path $ConfigPath -MarkerPath $markerPath

    Write-Step "Starting $ExePath"
    $process = Start-Process -FilePath $ExePath -PassThru
    Start-Sleep -Seconds 3

    Write-Step 'Sending Win+Shift+F24'
    Send-WinShiftF24

    $deadline = (Get-Date).AddSeconds(10)
    while ((Get-Date) -lt $deadline -and -not (Test-Path -LiteralPath $markerPath)) {
        Start-Sleep -Milliseconds 250
    }
    if (-not (Test-Path -LiteralPath $markerPath)) {
        throw "Hotkey action marker was not created: $markerPath"
    }
    Write-Step "Hotkey action marker created: $markerPath"

    Write-Step 'Sending bare Win for best-effort Start menu check'
    $startMenu = Send-BareWinAndCheckStartMenu
    if ($startMenu.Visible) {
        Write-Step "Bare Win appears to open Start menu: name='$($startMenu.Name)' class='$($startMenu.ClassName)'"
    } else {
        $message = 'Bare Win Start menu visibility was not detected by UIAutomation. Verify manually if needed.'
        if ($StrictStartMenuCheck) { throw $message }
        Write-Warning $message
    }

    Write-Step 'PASS'
} finally {
    if (-not $KeepWStartRunning) {
        Get-Process -Name WStart -ErrorAction SilentlyContinue | Stop-Process -Force
    }
    if ($configExisted -and $backupPath -and (Test-Path -LiteralPath $backupPath)) {
        Copy-Item -LiteralPath $backupPath -Destination $ConfigPath -Force
        Remove-Item -LiteralPath $backupPath -Force
        Write-Step 'Original config restored'
    } elseif (-not $configExisted -and (Test-Path -LiteralPath $ConfigPath)) {
        Remove-Item -LiteralPath $ConfigPath -Force
        Write-Step 'Temporary config removed'
    }
    if (Test-Path -LiteralPath $markerPath) {
        Remove-Item -LiteralPath $markerPath -Force
    }
}
