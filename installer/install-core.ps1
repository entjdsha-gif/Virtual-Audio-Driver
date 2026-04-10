<#
.SYNOPSIS
    AO Virtual Cable - Installer Core (no WDK/SDK dependency)

.DESCRIPTION
    Installs, upgrades, or uninstalls AO Virtual Cable from pre-packaged
    driver files. Designed to run on any Windows 10/11 x64 PC without
    development tools.

    Called by Setup.bat, external launchers, or directly.
    See docs/LAUNCHER_CONTRACT.md for integration spec.

.PARAMETER Action
    install      - Fresh install (auto-upgrades if already installed)
    upgrade      - Remove existing + install new (with in-session quiesce)
    uninstall    - Remove all AO Virtual Cable components
    repair       - Reinstall over existing (preserves registry settings)
    health-check - Report installation status (no admin required, no modifications)

.PARAMETER Silent
    Suppress interactive prompts. Auto-reboot on failure paths.

.PARAMETER JsonOutput
    Emit a JSON summary as the last line of stdout (machine-readable).
    For health-check, this is the primary output.
#>
param(
    [Parameter(Mandatory)]
    [ValidateSet('install','upgrade','uninstall','repair','health-check')]
    [string]$Action,

    [switch]$Silent,

    [switch]$JsonOutput
)

# --- Exit code constants ---
$EXIT_SUCCESS          = 0
$EXIT_FAILURE          = 1
$EXIT_HEALTHY          = 10
$EXIT_NOT_INSTALLED    = 20
$EXIT_UPGRADE_AVAIL    = 21
$EXIT_DEGRADED         = 22
$EXIT_REBOOT_REQUIRED  = 30
$EXIT_ADMIN_REQUIRED   = 40
$EXIT_BLOCKED          = 41
$EXIT_INSTALL_FAILED   = 50

$ErrorActionPreference = "Stop"

# --- Elevation check ---
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator
)
if (-not $isAdmin -and $Action -ne 'health-check') {
    if ($Silent) {
        Write-Host "[ERROR] Administrator privileges required." -ForegroundColor Red
        exit $EXIT_ADMIN_REQUIRED
    }
    # Re-launch elevated, capture exit code via -PassThru
    $argList = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $MyInvocation.MyCommand.Path, '-Action', $Action)
    if ($Silent) { $argList += '-Silent' }
    if ($JsonOutput) { $argList += '-JsonOutput' }
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
    # Parse pnputil /enum-drivers output. Field names vary by locale:
    #   English: "Published Name", "Original Name"
    #   Korean:  "게시된 이름", "원래 이름"
    # Match by pattern: oem*.inf line for published name, aocable/virtualaudiodriver for original.
    $pkgs = @()
    $lastOem = $null
    pnputil /enum-drivers 2>$null | ForEach-Object {
        if ($_ -match '(oem\d+\.inf)') {
            $lastOem = $Matches[1]
        }
        if ($lastOem -and $_ -match 'aocable[ab]\.inf') {
            $pkgs += $lastOem
            $lastOem = $null
        }
        if ($lastOem -and $_ -match 'virtualaudiodriver\.inf') {
            $pkgs += $lastOem
            $lastOem = $null
        }
    }
    return ($pkgs | Select-Object -Unique)
}

function Test-DriverPackagePresent {
    param([Parameter(Mandatory)][string]$OriginalInfName)
    $found = $false
    pnputil /enum-drivers 2>$null | ForEach-Object {
        if ($_ -match [regex]::Escape($OriginalInfName)) { $found = $true }
    }
    return $found
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

function Sync-System32DriverBinary {
    param(
        [string]$Label,
        [string]$SourcePath,
        [string]$TargetFileName
    )

    $targetPath = Join-Path "$env:SystemRoot\System32\drivers" $TargetFileName
    Write-Host "   Syncing $Label into System32\\drivers..."
    Copy-Item $SourcePath $targetPath -Force -ErrorAction Stop

    $sourceHash = (Get-FileHash $SourcePath -Algorithm SHA256).Hash
    $targetHash = (Get-FileHash $targetPath -Algorithm SHA256).Hash
    if ($sourceHash -ne $targetHash) {
        throw "$Label sync failed: System32 copy hash mismatch"
    }

    Write-OK "$Label synced to $targetPath"
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
        [uint32]3221225472, [uint32]3, [IntPtr]::Zero,
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
        [uint32]2147483648, [uint32]3, [IntPtr]::Zero,
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
    if (-not (Test-Path $cpExe)) {
        Write-Warn "AOControlPanel.exe not found in package - skipping"
        return
    }
    $destDir = "$env:ProgramFiles\AOAudio"
    $destPath = Join-Path $destDir "AOControlPanel.exe"
    New-Item -ItemType Directory -Path $destDir -Force | Out-Null

    # Kill any running instance first
    Stop-Process -Name AOControlPanel -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500

    # Retry copy up to 3 times (process may take a moment to release the file)
    $copied = $false
    for ($attempt = 1; $attempt -le 3; $attempt++) {
        try {
            Copy-Item $cpExe $destPath -Force -ErrorAction Stop
            $copied = $true
            break
        } catch {
            if ($attempt -lt 3) {
                Write-Warn "Control Panel copy attempt $attempt failed, retrying..."
                Stop-Process -Name AOControlPanel -Force -ErrorAction SilentlyContinue
                Start-Sleep -Seconds 1
            }
        }
    }

    if ($copied) {
        # Auto-start on login
        reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "AOControlPanel" /t REG_SZ /d "`"$destPath`"" /f 2>$null | Out-Null
        Write-OK "Control Panel installed"
    } else {
        # Driver is already installed successfully - don't fail the whole installer
        Write-Warn "Could not copy AOControlPanel.exe (file in use). Control Panel can be updated manually."
    }
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

# --- JSON output helper ---
function Write-JsonResult($obj) {
    if ($JsonOutput) {
        $obj | ConvertTo-Json -Depth 4 -Compress | Write-Host
    }
}

# --- Health check ---
function Invoke-HealthCheck {
    $result = @{
        status = "unknown"
        exitCode = $EXIT_FAILURE
        devices = @{}
        controlDevices = @{}
        driverStore = @{ activePackages = @(); stalePackages = @() }
        installedVersion = @{}
        bundledVersion = @{}
        versionMatch = $false
        testSigning = $false
        secureBoot = $false
    }

    # Test signing detection:
    #   1. bcdedit (requires admin) - authoritative
    #   2. Loaded AO driver presence as indirect signal (if AO services are
    #      running, test signing must be enabled since AO is test-signed)
    #   3. null = could not determine (non-admin, no indirect signal)
    $bcd = bcdedit 2>$null | Out-String
    if ($bcd -match 'testsigning\s+Yes') {
        $result.testSigning = $true
    } elseif ($bcd -match 'testsigning\s+No') {
        $result.testSigning = $false
    } else {
        # bcdedit failed (non-admin). Use indirect signal:
        # If AO services are running with test-signed drivers, test signing is on.
        $aoRunning = (Get-Service 'AOCableA' -ErrorAction SilentlyContinue)
        if ($aoRunning -and $aoRunning.Status -eq 'Running') {
            $result.testSigning = $true
        } else {
            $result.testSigning = $null  # unknown
        }
    }

    # Secure Boot
    try { $result.secureBoot = [bool](Confirm-SecureBootUEFI -ErrorAction SilentlyContinue) }
    catch { $result.secureBoot = $false }

    # Devices
    foreach ($svcName in @('AOCableA', 'AOCableB')) {
        $dev = Get-PnpDevice -Class MEDIA -ErrorAction SilentlyContinue |
            Where-Object { $_.Service -eq $svcName -and $_.Status -eq 'OK' } |
            Select-Object -First 1
        $svc = Get-Service $svcName -ErrorAction SilentlyContinue
        $result.devices[$svcName] = @{
            present = ($null -ne $dev)
            status  = if ($dev) { $dev.Status.ToString() } else { "NotFound" }
            service = if ($svc) { $svc.Status.ToString() } else { "NotFound" }
        }
    }

    # Control devices
    foreach ($devPath in @('AOCableA', 'AOCableB')) {
        $h = [AoNative]::CreateFileW("\\.\$devPath",
            2147483648, [uint32]3, [IntPtr]::Zero,
            [uint32]3, [uint32]0, [IntPtr]::Zero)
        $reachable = ($h -ne [AoNative]::INVALID_HANDLE)
        if ($reachable) { [AoNative]::CloseHandle($h) | Out-Null }
        $result.controlDevices[$devPath] = $reachable
    }

    # Driver store packages
    $activeOem = @()
    foreach ($device in @(Get-AoMediaDevices | Where-Object { $_.Status -eq 'OK' })) {
        try {
            $infPath = (Get-PnpDeviceProperty -InstanceId $device.InstanceId `
                -KeyName 'DEVPKEY_Device_DriverInfPath' -ErrorAction Stop).Data
            if ($infPath) { $activeOem += $infPath.ToLowerInvariant() }
        } catch {}
    }
    $activeOem = @($activeOem | Select-Object -Unique)
    $allAo = @(Get-AoOemPackages | ForEach-Object { $_.ToLowerInvariant() })
    $staleOem = @($allAo | Where-Object { $_ -notin $activeOem })
    $result.driverStore.activePackages = $activeOem
    $result.driverStore.stalePackages = $staleOem

    # Installed version
    foreach ($name in @('aocablea.sys', 'aocableb.sys')) {
        $path = Join-Path "$env:SystemRoot\System32\drivers" $name
        if (Test-Path $path) {
            $item = Get-Item $path
            $result.installedVersion[$name] = @{
                sha256   = (Get-FileHash $path -Algorithm SHA256).Hash
                size     = $item.Length
                modified = $item.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss")
            }
        }
    }

    # Bundled version
    $bundledPaths = @{
        'aocablea.sys' = Join-Path $driverDirA 'aocablea.sys'
        'aocableb.sys' = Join-Path $driverDirB 'aocableb.sys'
    }
    foreach ($name in $bundledPaths.Keys) {
        $bpath = $bundledPaths[$name]
        if (Test-Path $bpath) {
            $item = Get-Item $bpath
            $result.bundledVersion[$name] = @{
                sha256 = (Get-FileHash $bpath -Algorithm SHA256).Hash
                size   = $item.Length
            }
        }
    }

    # Version match
    $hasInstalled = $result.installedVersion.Count -gt 0
    $hasBundled = $result.bundledVersion.Count -gt 0
    if ($hasInstalled -and $hasBundled) {
        $result.versionMatch = ($result.installedVersion.'aocablea.sys'.sha256 -eq $result.bundledVersion.'aocablea.sys'.sha256)
    }

    # Determine status
    $devicesOk = ($result.devices.AOCableA.present -and $result.devices.AOCableB.present)
    $servicesOk = ($result.devices.AOCableA.service -eq 'Running' -and $result.devices.AOCableB.service -eq 'Running')
    $controlOk = ($result.controlDevices.AOCableA -and $result.controlDevices.AOCableB)

    if ($devicesOk -and $servicesOk -and $controlOk) {
        if ($hasBundled -and -not $result.versionMatch) {
            $result.status = "upgrade_available"
            $result.exitCode = $EXIT_UPGRADE_AVAIL
        } else {
            $result.status = "healthy"
            $result.exitCode = $EXIT_HEALTHY
        }
    } elseif ($hasInstalled -or $devicesOk) {
        $result.status = "degraded"
        $result.exitCode = $EXIT_DEGRADED
    } else {
        $result.status = "not_installed"
        $result.exitCode = $EXIT_NOT_INSTALLED
    }

    return $result
}

# =========================================================================
# Main
# =========================================================================

# --- HEALTH CHECK (no admin, no modifications) ---
if ($Action -eq 'health-check') {
    $hc = Invoke-HealthCheck
    if (-not $JsonOutput) {
        Write-Host ""
        Write-Host "AO Virtual Cable Health Check" -ForegroundColor Cyan
        Write-Host "  Status:     $($hc.status)" -ForegroundColor $(
            switch ($hc.status) { 'healthy' {'Green'} 'upgrade_available' {'Yellow'} 'degraded' {'Red'} default {'Red'} })
        Write-Host "  Devices:    A=$($hc.devices.AOCableA.present) B=$($hc.devices.AOCableB.present)"
        Write-Host "  Services:   A=$($hc.devices.AOCableA.service) B=$($hc.devices.AOCableB.service)"
        Write-Host "  Control:    A=$($hc.controlDevices.AOCableA) B=$($hc.controlDevices.AOCableB)"
        Write-Host "  Version:    match=$($hc.versionMatch)"
        Write-Host "  TestSign:   $($hc.testSigning)"
        Write-Host "  SecureBoot: $($hc.secureBoot)"
        Write-Host "  Store:      active=$($hc.driverStore.activePackages -join ',') stale=$($hc.driverStore.stalePackages -join ',')"
    }
    Write-JsonResult $hc
    exit $hc.exitCode
}

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
    Start-Sleep -Seconds 3
    exit 0
}

# --- REPAIR maps to upgrade internally ---
if ($Action -eq 'repair') {
    Write-Host "   Repair mode: reinstalling over existing (settings preserved)"
    $Action = 'upgrade'
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

# Check existing installation / stale residue
$allDevices = @(Get-AoMediaDevices)
$existing = @($allDevices | Where-Object { $_.Status -eq 'OK' })
$isInstalled = $existing.Count -gt 0
$hasResidualAo = ($allDevices.Count -gt 0) -or (@(Get-AoOemPackages).Count -gt 0)
foreach ($dev in @('\\.\AOCableA', '\\.\AOCableB')) {
    if (-not (Test-ControlDeviceGone $dev)) {
        $hasResidualAo = $true
        break
    }
}
foreach ($name in @('aocablea.sys', 'aocableb.sys', 'virtualaudiodriver.sys')) {
    $path = Join-Path "$env:SystemRoot\System32\drivers" $name
    if (Test-Path $path) {
        $hasResidualAo = $true
        break
    }
}
$existingRemoved = $false

if ($Action -eq 'install' -and $hasResidualAo) {
    Write-Warn "AO Virtual Cable residue detected. Switching to upgrade cleanup."
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
        exit $EXIT_REBOOT_REQUIRED
    }
    $existingRemoved = $true
} elseif ($driverLive) {
    # Fresh install but something is loaded (shouldn't happen normally)
    Remove-AllAO
    $existingRemoved = $true
}

# Upgrade must always remove the previous installation first, even if the driver
# is no longer live in this session. Otherwise pnputil can report "already latest"
# against stale/broken device instances and never rebind cleanly.
if ($Action -eq 'upgrade' -and -not $existingRemoved) {
    Write-Step "Removing existing installation..."
    Remove-AllAO
    $existingRemoved = $true
}

# Enable test signing
Write-Step "Checking test signing..."
$signingReady = Ensure-TestSigning
if (-not $signingReady) {
    if (-not $Silent) { Read-Host "Press Enter to close" }
    exit $EXIT_BLOCKED
}

# Sync service binaries into System32\drivers so service ImagePath resolves
Write-Step "Syncing service binaries into System32\\drivers..."
Sync-System32DriverBinary -Label "Cable A" -SourcePath (Join-Path $driverDirA "aocablea.sys") -TargetFileName "aocablea.sys"
Sync-System32DriverBinary -Label "Cable B" -SourcePath (Join-Path $driverDirB "aocableb.sys") -TargetFileName "aocableb.sys"

# Install drivers
Write-Step "Installing driver packages..."
foreach ($pkg in @(
    @{Name="Cable A"; Dir=$driverDirA; HwId="ROOT\AOCableA"},
    @{Name="Cable B"; Dir=$driverDirB; HwId="ROOT\AOCableB"}
)) {
    $inf = (Get-ChildItem (Join-Path $pkg.Dir "*.inf") | Select-Object -First 1).FullName
    $infName = [System.IO.Path]::GetFileName($inf)
    Write-Host "   Staging $($pkg.Name) from $inf"
    $output = pnputil /add-driver $inf /install 2>&1
    $pnputilExit = $LASTEXITCODE
    $alreadyPresent = Test-DriverPackagePresent -OriginalInfName $infName
    if ($pnputilExit -ne 0 -and $alreadyPresent) {
        Write-Warn "$($pkg.Name) package already present - continuing with driver rebind"
        Write-Host $output
    } elseif ($pnputilExit -ne 0) {
        Write-Warn "$($pkg.Name) staging returned exit $pnputilExit - continuing to explicit device bind/verify"
        Write-Host $output
    }
    Write-OK "$($pkg.Name) staged"
}

# Ensure root-enumerated device instances exist.
# On a clean PC, pnputil /add-driver stages the package but does NOT create
# device instances. devgen.exe creates the ROOT instance and devcon.exe
# explicitly binds/updates the matching driver.
Write-Step "Ensuring device instances..."

# Locate devgen.exe / devcon.exe: bundled in package, or from WDK on build machine
$devgen = $null
$devcon = $null
$bundledDevgen = Join-Path $scriptDir "devgen.exe"
$bundledDevcon = Join-Path $scriptDir "devcon.exe"
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
if (Test-Path $bundledDevcon) {
    $devcon = $bundledDevcon
} else {
    $wdkPaths = @(
        "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.26100.0\x64\devcon.exe",
        "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.22621.0\x64\devcon.exe"
    )
    foreach ($p in $wdkPaths) {
        if (Test-Path $p) { $devcon = $p; break }
    }
}

# Map hardware ID to service name for existing-device detection
$hwIdToService = @{
    'ROOT\AOCableA' = 'AOCableA'
    'ROOT\AOCableB' = 'AOCableB'
}

$deviceCreationOk = $true
foreach ($hwid in @('ROOT\AOCableA', 'ROOT\AOCableB')) {
    # Check if a device using this service already exists (any instance ID form)
    $svcName = $hwIdToService[$hwid]
    $existing = @(Get-AoMediaDevices | Where-Object { $_.Service -eq $svcName })
    if ($existing.Count -gt 0) {
        Write-OK "$hwid already present (service $svcName, instance $($existing[0].InstanceId))"
        continue
    }

    $infPath = if ($svcName -eq 'AOCableA') {
        (Get-ChildItem (Join-Path $driverDirA "*.inf") | Select-Object -First 1).FullName
    } else {
        (Get-ChildItem (Join-Path $driverDirB "*.inf") | Select-Object -First 1).FullName
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

    if ($devcon) {
        $devconCmd = '"' + $devcon + '" update "' + $infPath + '" "' + $hwid + '" 2>&1'
        $devconOutput = (& cmd.exe /d /c $devconCmd | Out-String).Trim()
        $devconExit = $LASTEXITCODE
        if ($devconExit -eq 0) {
            Write-OK "$hwid driver binding refreshed"
        } elseif ($devconOutput) {
            Write-Warn "$hwid devcon update did not fully succeed: $devconOutput"
        } else {
            Write-Warn "$hwid devcon update did not fully succeed (exit $devconExit)"
        }
    } else {
        Write-Warn "devcon.exe not found - relying on scan/restart only"
    }
}

if (-not $deviceCreationOk) {
    Write-Err "Device instance creation failed."
    Write-Err "Scheduling automatic retry after reboot..."
    Register-Resume
    if ($Silent) {
        shutdown.exe /r /t 10 /c "AO Virtual Cable install will retry after reboot."
    } else {
        $reply = Read-Host "Reboot now? (Y/n)"
        if ($reply -ne 'n') {
            shutdown.exe /r /t 5 /c "AO Virtual Cable install will retry after sign-in."
        }
    }
    exit $EXIT_REBOOT_REQUIRED
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
    Write-Err "Scheduling automatic retry after reboot..."
    Register-Resume
    if ($Silent) {
        shutdown.exe /r /t 10 /c "AO Virtual Cable install will retry after reboot."
    } else {
        $reply = Read-Host "Reboot now? (Y/n)"
        if ($reply -ne 'n') {
            shutdown.exe /r /t 5 /c "AO Virtual Cable install will retry after sign-in."
        }
    }
    exit $EXIT_REBOOT_REQUIRED
}

# Write install-manifest.json with actually installed hashes
Write-Step "Writing install manifest..."
$installManifest = @{
    timestamp = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
    artifacts = @{}
}
foreach ($pair in @(
    @{Name="aocablea.sys"; Path=(Join-Path "$env:SystemRoot\System32\drivers" "aocablea.sys")},
    @{Name="aocableb.sys"; Path=(Join-Path "$env:SystemRoot\System32\drivers" "aocableb.sys")}
)) {
    if (Test-Path $pair.Path) {
        $item = Get-Item $pair.Path
        $installManifest.artifacts[$pair.Name] = @{
            sha256   = (Get-FileHash $pair.Path -Algorithm SHA256).Hash
            size     = $item.Length
            modified = $item.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss")
        }
    }
}
# Write to script directory (packaged installer root)
$manifestPath = Join-Path $scriptDir "install-manifest.json"
$manifestJson = $installManifest | ConvertTo-Json -Depth 4
Set-Content -Path $manifestPath -Value $manifestJson -Encoding UTF8

# Find repo root by walking upward looking for verify-install.ps1
$searchDir = $scriptDir
$repoRoot = $null
for ($i = 0; $i -lt 5; $i++) {
    $candidate = Split-Path -Parent $searchDir
    if (-not $candidate -or $candidate -eq $searchDir) { break }
    if (Test-Path (Join-Path $candidate "verify-install.ps1")) {
        $repoRoot = $candidate
        break
    }
    $searchDir = $candidate
}
if ($repoRoot) {
    $repoManifest = Join-Path $repoRoot "install-manifest.json"
    Set-Content -Path $repoManifest -Value $manifestJson -Encoding UTF8
    Write-OK "Install manifest written (package + repo root)"
} else {
    Write-OK "Install manifest written (package only)"
}

# Clean stale AO driver store packages
Write-Step "Cleaning stale driver packages..."
$activeOemPkgs = @()
foreach ($device in @(Get-AoMediaDevices | Where-Object { $_.Status -eq 'OK' })) {
    try {
        $infPath = (Get-PnpDeviceProperty -InstanceId $device.InstanceId `
            -KeyName 'DEVPKEY_Device_DriverInfPath' -ErrorAction Stop).Data
        if ($infPath) { $activeOemPkgs += $infPath.ToLowerInvariant() }
    } catch {}
}
$activeOemPkgs = @($activeOemPkgs | Select-Object -Unique)
Write-Host "   Active: $($activeOemPkgs -join ', ')"

$allAoPkgs = @(Get-AoOemPackages | ForEach-Object { $_.ToLowerInvariant() })
$stalePkgs = @($allAoPkgs | Where-Object { $_ -notin $activeOemPkgs })

Write-Host "   All AO: $($allAoPkgs -join ', ')"
Write-Host "   Stale:  $($stalePkgs -join ', ')"

if ($stalePkgs.Count -eq 0) {
    Write-OK "No stale packages"
} else {
    # First pass: remove stale packages
    foreach ($oem in $stalePkgs) {
        Write-Host "   Deleting $oem..."
        $output = pnputil /delete-driver $oem /uninstall /force 2>&1
        $exitCode = $LASTEXITCODE
        Write-Host "   pnputil exit=$exitCode output: $($output | Out-String)".Trim()
    }

    Start-Sleep -Seconds 1

    # Verify: re-enumerate and check if any stale remain
    $remainingAll = @(Get-AoOemPackages | ForEach-Object { $_.ToLowerInvariant() })
    $stillStale = @($remainingAll | Where-Object { $_ -notin $activeOemPkgs })

    if ($stillStale.Count -gt 0) {
        Write-Warn "Stale packages remain after first pass: $($stillStale -join ', ')"
        # Retry: some packages need a second attempt after the first batch frees references
        foreach ($oem in $stillStale) {
            Write-Host "   Retry deleting $oem..."
            pnputil /delete-driver $oem /uninstall /force 2>$null | Out-Null
        }
        Start-Sleep -Milliseconds 500

        $finalAll = @(Get-AoOemPackages | ForEach-Object { $_.ToLowerInvariant() })
        $finalStale = @($finalAll | Where-Object { $_ -notin $activeOemPkgs })
        if ($finalStale.Count -eq 0) {
            Write-OK "All stale packages removed (after retry)"
        } else {
            Write-Warn "Could not remove: $($finalStale -join ', ') (non-blocking)"
        }
    } else {
        Write-OK "All stale packages removed"
    }
}

# Install Control Panel
Install-ControlPanel

# Launch Control Panel
$cpDest = "$env:ProgramFiles\AOAudio\AOControlPanel.exe"
if (Test-Path $cpDest) {
    Start-Process $cpDest -ErrorAction SilentlyContinue
}

Clear-Resume

Write-Host ""
Write-Host "=======================================" -ForegroundColor Green
Write-Host " Installation complete!" -ForegroundColor Green
Write-Host "=======================================" -ForegroundColor Green
Write-Host ""
Start-Sleep -Seconds 3
exit 0
