<#
.SYNOPSIS
    AO Virtual Cable - Installer Core (no WDK/SDK dependency)

.DESCRIPTION
    Installs, upgrades, or uninstalls AO Virtual Cable from pre-packaged
    driver files. Designed to run on any Windows 10/11 x64 PC without
    development tools.

    This script is called by Setup.bat or can be run directly.

.PARAMETER Action
    install   - Fresh install
    upgrade   - Remove existing + install new (with in-session quiesce)
    uninstall - Remove all AO Virtual Cable components

.PARAMETER Silent
    Suppress interactive prompts (for automated/EXE wrapper usage)
#>
param(
    [Parameter(Mandatory)]
    [ValidateSet('install','upgrade','uninstall')]
    [string]$Action,

    [switch]$Silent
)

$ErrorActionPreference = "Stop"

# --- Elevation check ---
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator
)
if (-not $isAdmin) {
    if ($Silent) {
        Write-Host "[ERROR] Administrator privileges required." -ForegroundColor Red
        exit 1
    }
    # Re-launch elevated, capture exit code via -PassThru
    $argList = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $MyInvocation.MyCommand.Path, '-Action', $Action)
    if ($Silent) { $argList += '-Silent' }
    $proc = Start-Process powershell -ArgumentList $argList -Verb RunAs -PassThru -Wait
    exit $proc.ExitCode
}

# --- Paths ---
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$driverDirA = Join-Path $scriptDir "drivers\CableA"
$driverDirB = Join-Path $scriptDir "drivers\CableB"
$cpExe = Join-Path $scriptDir "AOControlPanel.exe"

$resumeTaskName = "AOAudioVirtualCableResume"

# --- Helpers ---
function Write-Step($msg) { Write-Host "`n>> $msg" -ForegroundColor Cyan }
function Write-OK($msg)   { Write-Host "   $msg" -ForegroundColor Green }
function Write-Warn($msg) { Write-Host "   $msg" -ForegroundColor Yellow }
function Write-Err($msg)  { Write-Host "   $msg" -ForegroundColor Red }

function Get-AoMediaDevices {
    Get-PnpDevice -Class MEDIA -ErrorAction SilentlyContinue |
        Where-Object { $_.Service -eq 'AOCableA' -or $_.Service -eq 'AOCableB' -or $_.Service -eq 'VirtualAudioDriver' }
}

function Get-AoOemPackages {
    $pkgs = @()
    pnputil /enum-drivers 2>$null | ForEach-Object {
        if ($_ -match 'Published Name\s*:\s*(oem\d+\.inf)') { $script:lastOem = $Matches[1] }
        if ($_ -match 'Original Name\s*:\s*aocable[ab]\.inf') { $pkgs += $script:lastOem }
        if ($_ -match 'Original Name\s*:\s*virtualaudiodriver\.inf') { $pkgs += $script:lastOem }
    }
    return ($pkgs | Select-Object -Unique)
}

function Wait-ServiceStop([string]$Name, [int]$TimeoutSec = 10) {
    for ($i = 0; $i -lt ($TimeoutSec * 2); $i++) {
        $svc = Get-Service $Name -ErrorAction SilentlyContinue
        if (-not $svc -or $svc.Status -eq 'Stopped') { return $true }
        Start-Sleep -Milliseconds 500
    }
    return $false
}

function Test-FileUnlocked([string]$Path) {
    if (-not (Test-Path $Path)) { return $true }
    try {
        $fs = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
        $fs.Close()
        return $true
    } catch { return $false }
}

# P/Invoke
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class AoNative {
    public static readonly IntPtr INVALID_HANDLE = new IntPtr(-1);
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern IntPtr CreateFileW(
        string lpFileName, uint dwDesiredAccess, uint dwShareMode,
        IntPtr lpSecurityAttributes, uint dwCreationDisposition,
        uint dwFlagsAndAttributes, IntPtr hTemplateFile);
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool DeviceIoControl(
        IntPtr hDevice, uint dwIoControlCode,
        IntPtr lpInBuffer, uint nInBufferSize,
        IntPtr lpOutBuffer, uint nOutBufferSize,
        ref uint lpBytesReturned, IntPtr lpOverlapped);
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool CloseHandle(IntPtr hObject);
}
"@

$IOCTL_AO_PREPARE_UNLOAD = [uint32]0x0022A014

function Send-PrepareUnload([string]$DevicePath) {
    $hDevice = [AoNative]::CreateFileW($DevicePath,
        [uint32]0xC0000000, [uint32]3, [IntPtr]::Zero,
        [uint32]3, [uint32]0, [IntPtr]::Zero)
    if ($hDevice -eq [AoNative]::INVALID_HANDLE) {
        return $true  # device already gone
    }
    $br = [uint32]0
    $result = [AoNative]::DeviceIoControl($hDevice, $IOCTL_AO_PREPARE_UNLOAD,
        [IntPtr]::Zero, 0, [IntPtr]::Zero, 0, [ref]$br, [IntPtr]::Zero)
    [AoNative]::CloseHandle($hDevice) | Out-Null
    return $result
}

function Test-ControlDeviceGone([string]$DevicePath) {
    $h = [AoNative]::CreateFileW($DevicePath,
        [uint32]0x80000000, [uint32]3, [IntPtr]::Zero,
        [uint32]3, [uint32]0, [IntPtr]::Zero)
    if ($h -ne [AoNative]::INVALID_HANDLE) {
        [AoNative]::CloseHandle($h) | Out-Null
        return $false
    }
    return $true
}

# --- Remove all AO components ---
function Remove-AllAO {
    # PnP devices
    $devices = @(Get-AoMediaDevices)
    foreach ($dev in $devices) {
        pnputil /remove-device "$($dev.InstanceId)" 2>$null | Out-Null
    }
    Start-Sleep -Seconds 1

    # OEM packages
    foreach ($oem in (Get-AoOemPackages)) {
        pnputil /delete-driver $oem /uninstall /force 2>$null | Out-Null
    }

    # Services
    foreach ($svc in @('AOCableA', 'AOCableB', 'VirtualAudioDriver')) {
        sc.exe stop $svc 2>$null | Out-Null
        Wait-ServiceStop $svc | Out-Null
        sc.exe delete $svc 2>$null | Out-Null
    }

    # Stale binaries
    foreach ($name in @('aocablea.sys', 'aocableb.sys')) {
        $path = Join-Path "$env:SystemRoot\System32\drivers" $name
        Remove-Item $path -Force -ErrorAction SilentlyContinue
    }
}

# --- Pre-upgrade quiesce ---
function Invoke-Quiesce {
    Write-Step "Quiescing running driver..."

    # Kill Control Panel process (keep app files)
    Stop-Process -Name AOControlPanel -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500

    # Send PREPARE_UNLOAD
    foreach ($dev in @('\\.\AOCableA', '\\.\AOCableB')) {
        $ok = Send-PrepareUnload $dev
        if ($ok) { Write-OK "PREPARE_UNLOAD: $dev" }
        else     { Write-Warn "PREPARE_UNLOAD failed: $dev" }
    }

    Start-Sleep -Seconds 1

    # Verify control devices gone
    foreach ($dev in @('\\.\AOCableA', '\\.\AOCableB')) {
        if (-not (Test-ControlDeviceGone $dev)) {
            Write-Warn "$dev still reachable"
            return $false
        }
    }

    # Remove PnP + OEM + services
    Remove-AllAO

    # Wait for services to stop
    foreach ($svc in @('AOCableA', 'AOCableB')) {
        if (-not (Wait-ServiceStop $svc)) {
            Write-Warn "$svc did not stop"
            return $false
        }
    }

    # Verify .sys unlocked
    foreach ($name in @('aocablea.sys', 'aocableb.sys')) {
        $path = Join-Path "$env:SystemRoot\System32\drivers" $name
        if (-not (Test-FileUnlocked $path)) {
            Write-Warn "$name still locked"
            return $false
        }
    }

    Write-OK "Driver quiesced successfully"
    return $true
}

# --- Install Control Panel ---
function Install-ControlPanel {
    if (-not (Test-Path $cpExe)) { return }
    $destDir = "$env:ProgramFiles\AOAudio"
    New-Item -ItemType Directory -Path $destDir -Force | Out-Null
    Copy-Item $cpExe (Join-Path $destDir "AOControlPanel.exe") -Force
    # Auto-start on login
    reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "AOControlPanel" /t REG_SZ /d "`"$destDir\AOControlPanel.exe`"" /f 2>$null | Out-Null
    Write-OK "Control Panel installed"
}

function Remove-ControlPanel {
    Stop-Process -Name AOControlPanel -Force -ErrorAction SilentlyContinue
    reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "AOControlPanel" /f 2>$null | Out-Null
    Remove-Item "$env:ProgramFiles\AOAudio\AOControlPanel.exe" -Force -ErrorAction SilentlyContinue
    Remove-Item "$env:ProgramFiles\AOAudio" -Force -ErrorAction SilentlyContinue
}

# --- Reboot-resume ---
function Register-Resume {
    $args = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        $MyInvocation.MyCommand.Path, '-Action', 'install', '-Silent')
    $action = New-ScheduledTaskAction -Execute "powershell.exe" -Argument ($args -join ' ')
    $trigger = New-ScheduledTaskTrigger -AtLogOn -User ([System.Security.Principal.WindowsIdentity]::GetCurrent().Name)
    $principal = New-ScheduledTaskPrincipal -UserId ([System.Security.Principal.WindowsIdentity]::GetCurrent().Name) -LogonType Interactive -RunLevel Highest
    $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable -ExecutionTimeLimit (New-TimeSpan -Minutes 30)
    Register-ScheduledTask -TaskName $resumeTaskName -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Force | Out-Null
}

function Clear-Resume {
    Unregister-ScheduledTask -TaskName $resumeTaskName -Confirm:$false -ErrorAction SilentlyContinue
}

# --- Test signing ---
function Ensure-TestSigning {
    <#
    .SYNOPSIS
        Ensure test signing is enabled. Returns $true if ready, $false if reboot needed.
    #>
    $bcd = bcdedit 2>$null | Out-String
    if ($bcd -match 'testsigning\s+Yes') {
        Write-OK "Test signing is enabled"
        return $true
    }

    # Check Secure Boot
    try {
        $sb = Confirm-SecureBootUEFI -ErrorAction SilentlyContinue
    } catch { $sb = $false }
    if ($sb) {
        Write-Err "Secure Boot is enabled. Test signing cannot be activated."
        Write-Err "Disable Secure Boot in BIOS/UEFI settings, then run this installer again."
        return $false
    }

    bcdedit /set testsigning on 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Err "Failed to enable test signing (bcdedit error $LASTEXITCODE)"
        return $false
    }

    Write-Warn "Test signing has been enabled. A REBOOT is required before driver installation."
    Write-Warn "After reboot, run this installer again."
    return $false
}

# =========================================================================
# Main
# =========================================================================

Clear-Resume

Write-Host ""
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host " AO Virtual Cable Installer" -ForegroundColor Cyan
Write-Host " Action: $Action" -ForegroundColor Cyan
Write-Host "=======================================" -ForegroundColor Cyan

# --- UNINSTALL ---
if ($Action -eq 'uninstall') {
    Write-Step "Removing AO Virtual Cable..."
    $quiesce = Invoke-Quiesce
    if (-not $quiesce) {
        Remove-AllAO
    }
    Remove-ControlPanel
    Write-OK "AO Virtual Cable removed"
    if (-not $Silent) { Read-Host "Press Enter to close" }
    exit 0
}

# --- INSTALL / UPGRADE ---

# Validate package files
foreach ($dir in @($driverDirA, $driverDirB)) {
    foreach ($ext in @('sys', 'inf')) {
        $pattern = Join-Path $dir "*.$ext"
        if (-not (Get-ChildItem $pattern -ErrorAction SilentlyContinue)) {
            Write-Err "Missing driver files in $dir"
            exit 1
        }
    }
}

# Check existing installation
$existing = @(Get-AoMediaDevices | Where-Object { $_.Status -eq 'OK' })
$isInstalled = $existing.Count -gt 0

if ($Action -eq 'install' -and $isInstalled) {
    Write-Warn "AO Virtual Cable is already installed. Switching to upgrade."
    $Action = 'upgrade'
}

# Detect if driver is live
$driverLive = $false
foreach ($dev in @('\\.\AOCableA', '\\.\AOCableB')) {
    if (-not (Test-ControlDeviceGone $dev)) { $driverLive = $true; break }
}
if (-not $driverLive) {
    foreach ($name in @('aocablea.sys', 'aocableb.sys')) {
        $path = Join-Path "$env:SystemRoot\System32\drivers" $name
        if ((Test-Path $path) -and -not (Test-FileUnlocked $path)) {
            $driverLive = $true; break
        }
    }
}

if ($driverLive -and $Action -eq 'upgrade') {
    Write-Step "Driver is running - attempting in-session quiesce..."
    $quiesceOk = Invoke-Quiesce

    if (-not $quiesceOk) {
        # COMMIT POINT PASSED - must reboot
        Write-Warn "In-session quiesce failed - scheduling reboot-resume"
        Register-Resume
        Remove-AllAO
        Write-Err "Reboot required. Install will resume automatically after sign-in."
        if (-not $Silent) { Read-Host "Press Enter to reboot" }
        shutdown.exe /r /t 5 /c "AO Virtual Cable upgrade will resume after sign-in."
        exit 3010
    }
} elseif ($driverLive) {
    # Fresh install but something is loaded (shouldn't happen normally)
    Remove-AllAO
}

# Enable test signing
Write-Step "Checking test signing..."
$signingReady = Ensure-TestSigning
if (-not $signingReady) {
    if (-not $Silent) { Read-Host "Press Enter to close" }
    exit 1
}

# Install drivers
Write-Step "Installing driver packages..."
$installFailed = $false
foreach ($pkg in @(
    @{Name="Cable A"; Dir=$driverDirA; HwId="ROOT\AOCableA"},
    @{Name="Cable B"; Dir=$driverDirB; HwId="ROOT\AOCableB"}
)) {
    $inf = (Get-ChildItem (Join-Path $pkg.Dir "*.inf") | Select-Object -First 1).FullName
    Write-Host "   Staging $($pkg.Name) from $inf"
    $output = pnputil /add-driver $inf /install 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Err "Failed to stage $($pkg.Name)"
        Write-Host $output
        $installFailed = $true
        break
    }
    Write-OK "$($pkg.Name) staged"
}

if ($installFailed) {
    # Post-commit install failure: register resume so user isn't left without driver
    Write-Warn "Driver installation failed after quiesce. Registering reboot-resume..."
    Register-Resume
    Write-Err "Reboot and sign in to retry installation automatically."
    if (-not $Silent) { Read-Host "Press Enter to close" }
    exit 1
}

# Ensure root-enumerated device instances exist.
# On a clean PC, pnputil /add-driver stages the package but does NOT create
# device instances. devgen.exe is required to create ROOT\AOCableA/B instances.
Write-Step "Ensuring device instances..."

# Locate devgen.exe: bundled in package, or from WDK on build machine
$devgen = $null
$bundledDevgen = Join-Path $scriptDir "devgen.exe"
if (Test-Path $bundledDevgen) {
    $devgen = $bundledDevgen
} else {
    # Search WDK paths
    $wdkPaths = @(
        "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.26100.0\x64\devgen.exe",
        "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.22621.0\x64\devgen.exe"
    )
    foreach ($p in $wdkPaths) {
        if (Test-Path $p) { $devgen = $p; break }
    }
}

$deviceCreationOk = $true
foreach ($hwid in @('ROOT\AOCableA', 'ROOT\AOCableB')) {
    # Check if any instance with matching service already exists
    $existing = @(Get-AoMediaDevices | Where-Object {
        $_.InstanceId -match ($hwid -replace '\\','\\')
    })
    if ($existing.Count -gt 0) {
        Write-OK "$hwid already present"
        continue
    }

    if ($devgen) {
        $output = & $devgen /add /bus ROOT /hardwareid $hwid 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-OK "$hwid device instance created"
        } else {
            Write-Err "$hwid creation failed: $output"
            $deviceCreationOk = $false
        }
    } else {
        Write-Err "devgen.exe not found - cannot create root device instance"
        Write-Err "Bundle devgen.exe with the installer package or install WDK"
        $deviceCreationOk = $false
    }
}

if (-not $deviceCreationOk) {
    Write-Err "Device instance creation failed. Registering reboot-resume..."
    Register-Resume
    if (-not $Silent) { Read-Host "Press Enter to close" }
    exit 1
}

# Trigger driver load by scanning for new hardware
pnputil /scan-devices 2>$null | Out-Null
Start-Sleep -Seconds 3

# Restart any AO devices that appeared
foreach ($dev in @(Get-AoMediaDevices)) {
    pnputil /restart-device "$($dev.InstanceId)" 2>$null | Out-Null
}
Start-Sleep -Seconds 2

# Verify
Write-Step "Verifying installation..."
$devices = @(Get-AoMediaDevices | Where-Object { $_.Status -eq 'OK' })
if ($devices.Count -ge 2) {
    Write-OK "$($devices.Count) AO devices active"
} else {
    Write-Err "Expected 2 active devices, found $($devices.Count)"
    Write-Err "Installation may be incomplete. Try rebooting."
    Register-Resume
    if (-not $Silent) { Read-Host "Press Enter to close" }
    exit 1
}

# Install Control Panel
Install-ControlPanel

# Launch Control Panel
$cpDest = "$env:ProgramFiles\AOAudio\AOControlPanel.exe"
if (Test-Path $cpDest) {
    Start-Process $cpDest -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "=======================================" -ForegroundColor Green
Write-Host " Installation complete!" -ForegroundColor Green
Write-Host "=======================================" -ForegroundColor Green
Write-Host ""

if (-not $Silent) { Read-Host "Press Enter to close" }
exit 0
