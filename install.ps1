<#
.SYNOPSIS
    AO Virtual Cable - Driver Installer (pnputil-based)
.DESCRIPTION
    Installs, upgrades, or uninstalls AO Virtual Cable drivers.
    No direct .sys copy is used. Driver packages are staged from the newest
    built binaries and fresh INF files are stamped from the source .inx files.
    This avoids stale output-tree artifacts and invalid INF placeholders.

.PARAMETER Action
    install   - Fresh install (aborts if already installed)
    upgrade   - Remove existing + install new
    uninstall - Remove all AO Virtual Cable components
    cleanup   - Remove stale AO driver packages from Driver Store

.PARAMETER Config
    Debug or Release (default: Release)

.PARAMETER SkipSign
    Skip test-signing and catalog generation.

.PARAMETER AutoReboot
    When a loaded kernel driver blocks in-place replacement, schedule resume
    after reboot and automatically restart Windows.

.PARAMETER ResumeAfterReboot
    Internal flag used by install.ps1 when resuming a deferred install.
#>
param(
    [Parameter(Mandatory)]
    [ValidateSet('install','upgrade','uninstall','cleanup')]
    [string]$Action,

    [ValidateSet('Debug','Release')]
    [string]$Config = 'Release',

    [switch]$SkipSign,

    [switch]$AutoReboot,

    [switch]$ResumeAfterReboot
)

$ErrorActionPreference = "Stop"

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator
)
if (-not $isAdmin) {
    Write-Host "[ERROR] Run as Administrator." -ForegroundColor Red
    exit 1
}

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$scriptPath = $MyInvocation.MyCommand.Path
$platform = "x64"
$archName = "amd64"
$osName = "10_X64"
$certName = "AO Audio Test"
$resumeRunOnceName = "AOAudioVirtualCableResume"
$resumeTaskName = "AOAudioVirtualCableResume"

function Write-Step($num, $total, $msg) {
    Write-Host "`n[$num/$total] $msg" -ForegroundColor Cyan
}

function Write-OK($msg)   { Write-Host "       $msg" -ForegroundColor Green }
function Write-Err($msg)  { Write-Host "       $msg" -ForegroundColor Red }
function Write-Info($msg) { Write-Host "       $msg" -ForegroundColor Gray }

function Quote-Argument {
    param([string]$Value)

    if ($null -eq $Value) {
        return '""'
    }

    return '"' + ($Value -replace '"', '\"') + '"'
}

function Resolve-NewestExistingPath {
    param([string[]]$Candidates)

    $existing = foreach ($candidate in $Candidates) {
        if (Test-Path $candidate) {
            Get-Item $candidate
        }
    }

    if (-not $existing) {
        return $null
    }

    return ($existing | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
}

function Test-InfStamped {
    param([string]$Path)

    if (-not (Test-Path $Path)) {
        return $false
    }

    $content = Get-Content $Path -Raw
    if ($content -match 'NT\$ARCH\$') {
        return $false
    }
    if ($content -match 'DriverVer\s*=\s*02/22/2016,\s*1\.0\.0\.1') {
        return $false
    }

    return $true
}

function Resolve-BuildArtifacts {
    param([string]$BuildConfig)

    $sysA = Resolve-NewestExistingPath @(
        "$root\Source\Main\$platform\$BuildConfig\CableA\aocablea.sys",
        "$root\$platform\$BuildConfig\CableA\aocablea.sys"
    )
    $sysB = Resolve-NewestExistingPath @(
        "$root\Source\Main\$platform\$BuildConfig\CableB\aocableb.sys",
        "$root\$platform\$BuildConfig\CableB\aocableb.sys"
    )
    $infA = Resolve-NewestExistingPath @(
        "$root\Source\Main\$platform\$BuildConfig\CableA\aocablea.inf",
        "$root\$platform\$BuildConfig\CableA\aocablea.inf"
    )
    $infB = Resolve-NewestExistingPath @(
        "$root\Source\Main\$platform\$BuildConfig\CableB\aocableb.inf",
        "$root\$platform\$BuildConfig\CableB\aocableb.inf"
    )
    $cpExe = Resolve-NewestExistingPath @(
        "$root\Source\ControlPanel\$platform\$BuildConfig\AOControlPanel.exe",
        "$root\$platform\$BuildConfig\AOControlPanel.exe"
    )

    [pscustomobject]@{
        SysA = $sysA
        SysB = $sysB
        InfA = $infA
        InfB = $infB
        InxA = "$root\Source\Main\aocablea.inx"
        InxB = "$root\Source\Main\aocableb.inx"
        ControlPanel = $cpExe
    }
}

function Get-AoMediaDevices {
    Get-PnpDevice -Class MEDIA -ErrorAction SilentlyContinue |
        Where-Object { $_.Service -eq 'AOCableA' -or $_.Service -eq 'AOCableB' }
}

function Get-LoadedAoServices {
    $loaded = @()
    foreach ($service in @('AOCableA', 'AOCableB')) {
        $query = sc.exe query $service 2>$null
        if ($LASTEXITCODE -ne 0) {
            continue
        }

        $joined = ($query | Out-String)
        if ($joined -match 'STATE\s*:\s*4\s+RUNNING') {
            $loaded += [pscustomobject]@{
                Name = $service
                Query = $joined
                NotStoppable = ($joined -match 'NOT_STOPPABLE')
            }
        }
    }
    return $loaded
}

function Register-ResumeInstall {
    param(
        [string]$BuildConfig,
        [switch]$SkipSignature
    )

    $args = @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', $scriptPath,
        '-Action', 'install',
        '-Config', $BuildConfig,
        '-ResumeAfterReboot'
    )

    if ($SkipSignature) {
        $args += '-SkipSign'
    }

    $command = 'powershell.exe ' + (($args | ForEach-Object { Quote-Argument $_ }) -join ' ')
    $currentUser = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
    $action = New-ScheduledTaskAction -Execute "powershell.exe" -Argument (($args | ForEach-Object { Quote-Argument $_ }) -join ' ')
    $trigger = New-ScheduledTaskTrigger -AtLogOn -User $currentUser
    $principal = New-ScheduledTaskPrincipal -UserId $currentUser -LogonType Interactive -RunLevel Highest
    $settings = New-ScheduledTaskSettingsSet `
        -AllowStartIfOnBatteries `
        -DontStopIfGoingOnBatteries `
        -StartWhenAvailable `
        -ExecutionTimeLimit (New-TimeSpan -Minutes 30)

    Register-ScheduledTask `
        -TaskName $resumeTaskName `
        -Action $action `
        -Trigger $trigger `
        -Principal $principal `
        -Settings $settings `
        -Force | Out-Null

    # Legacy fallback cleanup so older failed entries do not linger.
    Remove-ItemProperty `
        -Path 'HKCU:\Software\Microsoft\Windows\CurrentVersion\RunOnce' `
        -Name $resumeRunOnceName `
        -ErrorAction SilentlyContinue

    return $command
}

function Clear-ResumeInstall {
    Unregister-ScheduledTask -TaskName $resumeTaskName -Confirm:$false -ErrorAction SilentlyContinue
    Remove-ItemProperty `
        -Path 'HKCU:\Software\Microsoft\Windows\CurrentVersion\RunOnce' `
        -Name $resumeRunOnceName `
        -ErrorAction SilentlyContinue
}

function Get-OemPackages {
    $pkgs = @()
    $currentOem = $null
    foreach ($line in (pnputil /enum-drivers 2>$null)) {
        if ($line -match '(oem\d+\.inf)') {
            $currentOem = $Matches[1]
            continue
        }
        if ($currentOem -and $line -match 'aocable[ab]\.inf') {
            $pkgs += $currentOem
            $currentOem = $null
        }
    }
    return $pkgs | Select-Object -Unique
}

function Get-ActiveAoOemPackages {
    $active = @()

    foreach ($device in @(Get-AoMediaDevices)) {
        try {
            $infPath = (Get-PnpDeviceProperty `
                -InstanceId $device.InstanceId `
                -KeyName 'DEVPKEY_Device_DriverInfPath' `
                -ErrorAction Stop).Data
            if ($infPath) {
                $active += $infPath.ToLowerInvariant()
            }
        } catch {}
    }

    return $active | Select-Object -Unique
}

function Remove-StaleAoDriverPackages {
    $allPackages = @(Get-OemPackages | ForEach-Object { $_.ToLowerInvariant() })
    $activePackages = @(Get-ActiveAoOemPackages)
    $stalePackages = @($allPackages | Where-Object { $_ -notin $activePackages })

    if ($activePackages.Count -gt 0) {
        Write-Info "Active AO packages: $($activePackages -join ', ')"
    } else {
        Write-Info "No active AO packages detected"
    }

    if ($stalePackages.Count -eq 0) {
        Write-OK "No stale AO driver packages found"
        return [pscustomobject]@{
            Removed = @()
            Failed = @()
            Active = $activePackages
        }
    }

    $removed = @()
    $failed = @()

    foreach ($oem in $stalePackages) {
        Write-Info "Removing stale Driver Store package: $oem"
        $output = pnputil /delete-driver $oem /uninstall /force 2>&1
        if ($LASTEXITCODE -eq 0) {
            $removed += $oem
            Write-OK "Removed $oem"
        } else {
            $failed += [pscustomobject]@{
                Package = $oem
                Output = ($output | Out-String).Trim()
            }
            Write-Info "Could not remove $oem right now"
        }
    }

    return [pscustomobject]@{
        Removed = $removed
        Failed = $failed
        Active = $activePackages
    }
}

function Save-DefaultDevices {
    $result = @{ Render = $null; Capture = $null }
    try {
        $module = Get-Module -ListAvailable AudioDeviceCmdlets 2>$null
        if ($module) {
            Import-Module AudioDeviceCmdlets -ErrorAction SilentlyContinue
            $result.Render = (Get-AudioDevice -Playback 2>$null).Name
            $result.Capture = (Get-AudioDevice -Recording 2>$null).Name
        }
    } catch {}
    return $result
}

function Restore-DefaultDevices {
    param($Saved)

    try {
        $module = Get-Module -ListAvailable AudioDeviceCmdlets 2>$null
        if (-not $module) {
            Write-Info "AudioDeviceCmdlets not installed - skip device restore"
            return
        }

        Import-Module AudioDeviceCmdlets -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 3

        if ($Saved.Render) {
            $render = Get-AudioDevice -List | Where-Object {
                $_.Type -eq 'Playback' -and $_.Name -like "*$($Saved.Render)*"
            } | Select-Object -First 1
            if ($render) {
                Set-AudioDevice -ID $render.ID | Out-Null
                Write-OK "Render restored: $($Saved.Render)"
            }
        }

        if ($Saved.Capture) {
            $capture = Get-AudioDevice -List | Where-Object {
                $_.Type -eq 'Recording' -and $_.Name -like "*$($Saved.Capture)*"
            } | Select-Object -First 1
            if ($capture) {
                Set-AudioDevice -ID $capture.ID | Out-Null
                Write-OK "Capture restored: $($Saved.Capture)"
            }
        }
    } catch {
        Write-Info "Device restore skipped: $_"
    }
}

function Find-Tool {
    param(
        [string]$Name,
        [string[]]$SearchPaths
    )

    foreach ($dir in $SearchPaths) {
        $path = Join-Path $dir $Name
        if (Test-Path $path) {
            return $path
        }
    }

    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    return $null
}

function Get-DevconPath {
    return Find-Tool "devcon.exe" @(
        "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.26100.0\x64",
        "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.22621.0\x64",
        "C:\Program Files (x86)\Windows Kits\10\Tools\x64",
        "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.26100.0\x86",
        "C:\Program Files (x86)\Windows Kits\10\Tools\x86"
    )
}

function Get-DevgenPath {
    return Find-Tool "devgen.exe" @(
        "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.26100.0\x64",
        "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.22621.0\x64",
        "C:\Program Files (x86)\Windows Kits\10\Tools\x64"
    )
}

function Ensure-TestSigning {
    $enabled = bcdedit /enum "{current}" 2>$null | Select-String "testsigning\s+Yes"
    if ($enabled) {
        Write-OK "Test signing already enabled"
        return
    }

    bcdedit /set testsigning on 2>$null | Out-Null
    Write-OK "Test signing enabled (reboot may be needed on first run)"
}

function Wait-For-ServiceStop {
    param([string]$Name)

    for ($i = 0; $i -lt 10; $i++) {
        $svc = Get-Service $Name -ErrorAction SilentlyContinue
        if (-not $svc -or $svc.Status -eq 'Stopped') {
            return
        }
        Start-Sleep -Milliseconds 500
    }
}

function Remove-AllAODevices {
    $lockedFiles = @()

    $devcon = Get-DevconPath
    if ($devcon) {
        foreach ($hwid in @('ROOT\AOCableA', 'ROOT\AOCableB', 'ROOT\AOVirtualCable', 'ROOT\AOVirtualAudio')) {
            Write-Info "Removing root device via devcon: $hwid"
            & $devcon remove $hwid 2>$null | Out-Null
        }
    }

    $devices = @(Get-AoMediaDevices)
    foreach ($device in $devices) {
        Write-Info "Removing device instance: $($device.InstanceId)"
        pnputil /remove-device "$($device.InstanceId)" 2>$null | Out-Null
    }

    Start-Sleep -Seconds 1

    foreach ($oem in (Get-OemPackages)) {
        Write-Info "Removing driver store package: $oem"
        pnputil /delete-driver $oem /uninstall /force 2>$null | Out-Null
    }

    foreach ($service in @('AOCableA', 'AOCableB', 'VirtualAudioDriver')) {
        sc.exe stop $service 2>$null | Out-Null
        Wait-For-ServiceStop $service
        sc.exe delete $service 2>$null | Out-Null
    }

    foreach ($name in @('aocablea.sys', 'aocableb.sys', 'virtualaudiodriver.sys')) {
        $path = Join-Path "$env:SystemRoot\System32\drivers" $name
        if (Test-Path $path) {
            try {
                Remove-Item $path -Force -ErrorAction Stop
                Write-OK "Removed stale $name from System32\\drivers"
            } catch {
                Write-Info "Could not remove $name immediately: $($_.Exception.Message)"
                $lockedFiles += $path
            }
        }
    }

    return [pscustomobject]@{
        LockedFiles = $lockedFiles
    }
}

function Stop-ControlPanelProcess {
    Stop-Process -Name AOControlPanel -Force -ErrorAction SilentlyContinue
}

function Remove-ControlPanel {
    Stop-ControlPanelProcess
    reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "AOControlPanel" /f 2>$null | Out-Null
    Remove-Item "$env:ProgramFiles\AOAudio\AOControlPanel.exe" -Force -ErrorAction SilentlyContinue
    Remove-Item "$env:ProgramFiles\AOAudio" -Force -ErrorAction SilentlyContinue
}

# ---------------------------------------------------------------------------
# Pre-upgrade quiesce: attempt to unload driver in-session
# ---------------------------------------------------------------------------

# IOCTL_AO_PREPARE_UNLOAD = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_WRITE_ACCESS)
# = ((0x22) << 16) | ((0x2) << 14) | ((0x805) << 2) | (0) = 0x0022A014
$IOCTL_AO_PREPARE_UNLOAD = 0x0022A014

function Send-PrepareUnload {
    param([string]$DevicePath)

    # Open device handle via Win32 CreateFile (not .NET File.Open, which
    # rejects non-file device paths like \\.\AOCableA).
    $GENERIC_READ_WRITE = [uint32]3221225472  # GENERIC_READ | GENERIC_WRITE
    $OPEN_EXISTING = [uint32]3
    $FILE_SHARE_RW = [uint32]3               # FILE_SHARE_READ | FILE_SHARE_WRITE

    $hDevice = [AoNative]::CreateFileW(
        $DevicePath,
        $GENERIC_READ_WRITE,
        $FILE_SHARE_RW,
        [IntPtr]::Zero,
        $OPEN_EXISTING,
        0,
        [IntPtr]::Zero
    )

    if ($hDevice -eq [AoNative]::INVALID_HANDLE) {
        $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
        Write-Info "Cannot open $DevicePath (Win32 error $err - may already be gone)"
        return $true  # device gone = success
    }

    # Send IOCTL
    $bytesReturned = [uint32]0
    $result = [AoNative]::DeviceIoControl(
        $hDevice,
        $IOCTL_AO_PREPARE_UNLOAD,
        [IntPtr]::Zero, 0,
        [IntPtr]::Zero, 0,
        [ref]$bytesReturned,
        [IntPtr]::Zero
    )

    if ($result) {
        Write-OK "PREPARE_UNLOAD sent to $DevicePath"
    } else {
        $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()
        Write-Info "PREPARE_UNLOAD failed on $DevicePath (Win32 error $err)"
    }

    # Close handle - this triggers the driver's last-handle cleanup
    # (control device deletion if refcount reaches 0 during quiesce)
    [AoNative]::CloseHandle($hDevice) | Out-Null

    return $result
}

function Test-FileUnlocked {
    param([string]$Path)
    if (-not (Test-Path $Path)) { return $true }
    try {
        $fs = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
        $fs.Close()
        return $true
    } catch {
        return $false
    }
}

function Invoke-PreUpgradeQuiesce {
    <#
    .SYNOPSIS
        Attempt to fully unload AO driver in-session via PREPARE_UNLOAD protocol.
        Returns $true if .sys files are unlocked and ready for replacement.
        This is a COMMIT POINT: after PREPARE_UNLOAD, the control device is
        permanently destroyed. If anything fails after this, fall back to reboot.
    #>

    Write-Info "Attempting in-session quiesce..."

    # 1. Kill Control Panel process (release user-mode handles, keep app installed)
    Stop-ControlPanelProcess
    Start-Sleep -Milliseconds 500

    # 2. Send PREPARE_UNLOAD to each cable's control device
    #    This deletes the symlink immediately and arms the quiesce flag.
    #    When our handle closes (end of Send-PrepareUnload), the driver
    #    deletes the control device object if no other handles remain.
    foreach ($devPath in @('\\.\AOCableA', '\\.\AOCableB')) {
        Send-PrepareUnload -DevicePath $devPath | Out-Null
    }

    # === COMMIT POINT ===
    # Symlinks are deleted, control devices are being torn down.
    # There is no rollback from here - if anything fails below,
    # we must go to reboot-resume.

    Start-Sleep -Seconds 1

    # 3. Verify control devices are gone (reopen should fail)
    $GENERIC_READ = [uint32]2147483648
    $OPEN_EXISTING = [uint32]3
    $FILE_SHARE_RW = [uint32]3
    foreach ($devPath in @('\\.\AOCableA', '\\.\AOCableB')) {
        $hCheck = [AoNative]::CreateFileW($devPath, $GENERIC_READ, $FILE_SHARE_RW,
            [IntPtr]::Zero, $OPEN_EXISTING, 0, [IntPtr]::Zero)
        if ($hCheck -ne [AoNative]::INVALID_HANDLE) {
            [AoNative]::CloseHandle($hCheck) | Out-Null
            Write-Info "WARNING: $devPath is still reachable after PREPARE_UNLOAD"
            return $false
        }
        Write-OK "$devPath is closed"
    }

    # 4. Remove PnP devices (broad: devcon root + Get-PnpDevice all AO services + OEM packages)
    $devcon = Get-DevconPath
    if ($devcon) {
        foreach ($hwid in @('ROOT\AOCableA', 'ROOT\AOCableB', 'ROOT\AOVirtualCable', 'ROOT\AOVirtualAudio')) {
            Write-Info "Removing root device via devcon: $hwid"
            & $devcon remove $hwid 2>$null | Out-Null
        }
    }

    # Remove all AO PnP instances (any status, any service variant)
    $allAoDevices = @(Get-PnpDevice -Class MEDIA -ErrorAction SilentlyContinue |
        Where-Object { $_.Service -eq 'AOCableA' -or $_.Service -eq 'AOCableB' -or $_.Service -eq 'VirtualAudioDriver' })
    foreach ($device in $allAoDevices) {
        Write-Info "Removing PnP device: $($device.InstanceId) (status=$($device.Status))"
        pnputil /remove-device "$($device.InstanceId)" 2>$null | Out-Null
    }

    Start-Sleep -Seconds 1

    # Remove OEM driver store packages
    foreach ($oem in (Get-OemPackages)) {
        Write-Info "Removing driver store package: $oem"
        pnputil /delete-driver $oem /uninstall /force 2>$null | Out-Null
    }

    # 5. Wait for services to stop (driver module unload)
    $allStopped = $true
    foreach ($service in @('AOCableA', 'AOCableB', 'VirtualAudioDriver')) {
        Wait-For-ServiceStop $service
        $svc = Get-Service $service -ErrorAction SilentlyContinue
        if ($svc -and $svc.Status -ne 'Stopped') {
            Write-Info "$service still running after wait"
            $allStopped = $false
        }
    }

    if (-not $allStopped) {
        Write-Info "Services did not stop in time"
        return $false
    }

    # 6. Delete services
    foreach ($service in @('AOCableA', 'AOCableB', 'VirtualAudioDriver')) {
        sc.exe delete $service 2>$null | Out-Null
    }

    Start-Sleep -Milliseconds 500

    # 7. Verify AO .sys files are unlocked
    foreach ($name in @('aocablea.sys', 'aocableb.sys')) {
        $path = Join-Path "$env:SystemRoot\System32\drivers" $name
        if (-not (Test-FileUnlocked $path)) {
            Write-Info "$name is still locked"
            return $false
        }
    }

    # Legacy virtualaudiodriver.sys: only block if it has a live service/PnP instance.
    # A stale TrustedInstaller-owned file without a running service is not a blocker.
    $legacyPath = Join-Path "$env:SystemRoot\System32\drivers" 'virtualaudiodriver.sys'
    if ((Test-Path $legacyPath) -and -not (Test-FileUnlocked $legacyPath)) {
        $legacySvc = Get-Service 'VirtualAudioDriver' -ErrorAction SilentlyContinue
        $legacyLive = $legacySvc -and $legacySvc.Status -ne 'Stopped'
        if ($legacyLive) {
            Write-Info "virtualaudiodriver.sys is locked by live VirtualAudioDriver service"
            return $false
        }
        Write-Info "virtualaudiodriver.sys is locked but no live service - treating as stale (non-blocking)"
    }

    Write-OK "In-session quiesce succeeded - driver fully unloaded"
    return $true
}

# P/Invoke for CreateFile, DeviceIoControl, CloseHandle
if (-not ([System.Management.Automation.PSTypeName]'AoNative').Type) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class AoNative {
    public static readonly IntPtr INVALID_HANDLE = new IntPtr(-1);

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern IntPtr CreateFileW(
        string lpFileName,
        uint dwDesiredAccess,
        uint dwShareMode,
        IntPtr lpSecurityAttributes,
        uint dwCreationDisposition,
        uint dwFlagsAndAttributes,
        IntPtr hTemplateFile);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool DeviceIoControl(
        IntPtr hDevice, uint dwIoControlCode,
        IntPtr lpInBuffer, uint nInBufferSize,
        IntPtr lpOutBuffer, uint nOutBufferSize,
        ref uint lpBytesReturned,
        IntPtr lpOverlapped);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool CloseHandle(IntPtr hObject);
}
"@
}

function New-StagedDriverPackage {
    param(
        [string]$Name,
        [string]$SysPath,
        [string]$InxPath,
        [string]$StageDir,
        [string]$StampInf,
        [string]$SignTool,
        [string]$Inf2Cat,
        [switch]$DoSign
    )

    New-Item -ItemType Directory -Path $StageDir -Force | Out-Null

    $targetSys = Join-Path $StageDir "$Name.sys"
    $targetInf = Join-Path $StageDir "$Name.inf"
    $targetCat = Join-Path $StageDir "$Name.cat"

    Copy-Item $SysPath $targetSys -Force
    Copy-Item $InxPath $targetInf -Force

    if (-not $StampInf) {
        throw "stampinf.exe not found"
    }

    & $StampInf -f $targetInf -d "*" -a $archName -v "*" -k "1.15" -x 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $targetInf)) {
        throw "stampinf failed for $Name"
    }
    if (-not (Test-InfStamped $targetInf)) {
        throw "Generated INF is not stamped correctly for $Name"
    }

    if ($DoSign) {
        & $SignTool sign /v /s My /n $certName /fd SHA256 $targetSys 2>$null | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "signtool failed for $Name.sys"
        }

        if ($Inf2Cat) {
            Remove-Item $targetCat -Force -ErrorAction SilentlyContinue
            & $Inf2Cat /driver:$StageDir /os:$osName /uselocaltime 2>$null | Out-Null
            if ($LASTEXITCODE -ne 0 -or -not (Test-Path $targetCat)) {
                throw "inf2cat failed for $Name"
            }

            & $SignTool sign /v /s My /n $certName /fd SHA256 $targetCat 2>$null | Out-Null
            if ($LASTEXITCODE -ne 0) {
                throw "signtool failed for $Name.cat"
            }
        } else {
            Write-Info "inf2cat not found - skipping .cat generation for $Name"
        }
    }

    return [pscustomobject]@{
        Sys = $targetSys
        Inf = $targetInf
        Cat = $targetCat
        Dir = $StageDir
    }
}

function Sync-System32DriverBinary {
    param(
        [string]$Label,
        [string]$SourcePath,
        [string]$TargetFileName
    )

    $targetPath = Join-Path "$env:SystemRoot\System32\drivers" $TargetFileName
    Write-Info "Syncing $Label into System32\\drivers..."

    Copy-Item $SourcePath $targetPath -Force -ErrorAction Stop

    $sourceHash = (Get-FileHash $SourcePath -Algorithm SHA256).Hash
    $targetHash = (Get-FileHash $targetPath -Algorithm SHA256).Hash
    if ($sourceHash -ne $targetHash) {
        throw "$Label sync failed: System32 copy hash mismatch"
    }

    Write-OK "$Label synced to $targetPath"
}

function Install-DriverPackage {
    param(
        [string]$Label,
        [string]$InfPath,
        [string]$HardwareId
    )

    Write-Info "Installing $Label..."
    $output = pnputil /add-driver $InfPath /install 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Err "$Label failed while staging package: $output"
        return $false
    }

    $devgen = Get-DevgenPath
    if ($devgen) {
        $devgenOutput = & $devgen /add /bus ROOT /hardwareid $HardwareId 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Err "$Label failed while creating ROOT device: $devgenOutput"
            return $false
        }
    } else {
        Write-Info "devgen.exe not found - no explicit ROOT device creation"
    }

    $devcon = Get-DevconPath
    if ($devcon) {
        $devconCmd = '"' + $devcon + '" update "' + $InfPath + '" "' + $HardwareId + '" 2>&1'
        $devconOutput = (& cmd.exe /d /c $devconCmd | Out-String).Trim()
        $devconExit = $LASTEXITCODE

        if ($devconExit -ne 0) {
            if ($devconOutput) {
                Write-Info "$Label devcon update did not fully succeed: $devconOutput"
            } else {
                Write-Info "$Label devcon update did not fully succeed (exit $devconExit)"
            }
        }
    }

    Write-OK "$Label installed"
    return $true
}

if ($Action -eq 'uninstall') {
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host " AO Virtual Cable - Uninstall" -ForegroundColor Cyan
    Write-Host "============================================" -ForegroundColor Cyan

    Clear-ResumeInstall

    Write-Step 1 3 "Removing devices and services..."
    $removal = Remove-AllAODevices
    Write-OK "Devices and services removed"

    Write-Step 2 3 "Removing Control Panel..."
    Remove-ControlPanel
    Write-OK "Control Panel removed"

    Write-Step 3 3 "Verifying clean removal..."
    if (@(Get-AoMediaDevices).Count -eq 0) {
        Write-OK "All AO Virtual Cable components removed"
    } else {
        Write-Err "Some devices still present - a reboot may be required"
    }
    if ($removal.LockedFiles.Count -gt 0) {
        Write-Info "Locked driver binaries remain loaded until reboot:"
        foreach ($path in $removal.LockedFiles) {
            Write-Info "  $path"
        }
    }

    Write-Host "`n============================================" -ForegroundColor Cyan
    Write-Host " Uninstall complete." -ForegroundColor Green
    Write-Host "============================================" -ForegroundColor Cyan
    exit 0
}

if ($Action -eq 'cleanup') {
    Write-Host "============================================" -ForegroundColor Cyan
    Write-Host " AO Virtual Cable - Driver Store Cleanup" -ForegroundColor Cyan
    Write-Host "============================================" -ForegroundColor Cyan

    $cleanup = Remove-StaleAoDriverPackages

    Write-Host "`n============================================" -ForegroundColor Cyan
    if ($cleanup.Failed.Count -eq 0) {
        Write-Host " Cleanup complete!" -ForegroundColor Green
    } else {
        Write-Host " Cleanup completed with warnings." -ForegroundColor Yellow
    }
    Write-Host "============================================" -ForegroundColor Cyan

    if ($cleanup.Removed.Count -gt 0) {
        Write-Host " Removed: $($cleanup.Removed -join ', ')" -ForegroundColor Gray
    }
    if ($cleanup.Failed.Count -gt 0) {
        foreach ($item in $cleanup.Failed) {
            Write-Host " Failed: $($item.Package) -> $($item.Output)" -ForegroundColor Yellow
        }
        exit 1
    }

    exit 0
}

$totalSteps = if ($Action -eq 'upgrade') { 11 } else { 10 }
$step = 0

Write-Host "============================================" -ForegroundColor Cyan
Write-Host " AO Virtual Cable - $(if ($Action -eq 'upgrade') { 'Upgrade' } else { 'Install' })" -ForegroundColor Cyan
Write-Host " Config: $Config" -ForegroundColor Cyan
if ($ResumeAfterReboot) {
    Write-Host " Resume: post-reboot continuation" -ForegroundColor Cyan
}
Write-Host "============================================" -ForegroundColor Cyan

if ($ResumeAfterReboot) {
    Clear-ResumeInstall
    Write-Info "Cleared deferred resume registration"
}

$step++
Write-Step $step $totalSteps "Pre-flight: resolving build artifacts..."
$artifacts = Resolve-BuildArtifacts $Config

foreach ($path in @($artifacts.SysA, $artifacts.SysB, $artifacts.InxA, $artifacts.InxB)) {
    if (-not $path -or -not (Test-Path $path)) {
        Write-Err "Missing required artifact: $path"
        Write-Err "Run build first: build.bat $Config $platform"
        exit 1
    }
}

Write-OK "Cable A sys: $($artifacts.SysA)"
Write-OK "Cable B sys: $($artifacts.SysB)"
if ($artifacts.InfA) { Write-Info "Detected build INF A: $($artifacts.InfA)" }
if ($artifacts.InfB) { Write-Info "Detected build INF B: $($artifacts.InfB)" }

$hashA = (Get-FileHash $artifacts.SysA -Algorithm SHA256).Hash
$hashB = (Get-FileHash $artifacts.SysB -Algorithm SHA256).Hash
if ($hashA -eq $hashB) {
    Write-Err "aocablea.sys and aocableb.sys are IDENTICAL - build error"
    exit 1
}
Write-OK "CableA/B binaries are distinct"

$manifestPath = "$root\build-manifest.json"
if (Test-Path $manifestPath) {
    $manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
    $manifestOk = $true
    foreach ($pair in @(
        @{Name="aocablea.sys"; Hash=$hashA},
        @{Name="aocableb.sys"; Hash=$hashB}
    )) {
        if ($manifest.artifacts.($pair.Name) -and $manifest.artifacts.($pair.Name).sha256 -eq $pair.Hash) {
            Write-OK "$($pair.Name) matches build-manifest.json"
        } else {
            Write-Err "$($pair.Name) does not match build-manifest.json"
            $manifestOk = $false
        }
    }
    if (-not $manifestOk) {
        Write-Err "Re-run build-verify.ps1 for the same config before installing."
        exit 1
    }
} else {
    Write-Info "No build-manifest.json found (run build-verify.ps1 after build)"
}

$step++
Write-Step $step $totalSteps "Checking existing installation..."
$existingDevices = @(Get-AoMediaDevices | Where-Object { $_.Status -eq 'OK' })
$isInstalled = $existingDevices.Count -gt 0
$loadedServices = @(Get-LoadedAoServices)

if ($Action -eq 'install' -and $isInstalled) {
    Write-Err "AO Virtual Cable is already installed."
    Write-Err "Use -Action upgrade to replace, or -Action uninstall first."
    exit 1
}

if ($Action -eq 'upgrade' -and -not $isInstalled) {
    Write-Info "No existing installation found - proceeding as fresh install"
}

# Determine if driver is live: services running, OR control devices reachable,
# OR legacy binary locked. Any of these means we need quiesce before upgrade.
$driverIsLive = $loadedServices.Count -gt 0

if (-not $driverIsLive -and $Action -eq 'upgrade') {
    # Services might not show as RUNNING but control device may still be alive
    $GENERIC_READ = [uint32]2147483648
    $OPEN_EXISTING = [uint32]3
    $FILE_SHARE_RW = [uint32]3
    foreach ($devPath in @('\\.\AOCableA', '\\.\AOCableB')) {
        $hProbe = [AoNative]::CreateFileW($devPath, $GENERIC_READ, $FILE_SHARE_RW,
            [IntPtr]::Zero, $OPEN_EXISTING, [uint32]0, [IntPtr]::Zero)
        if ($hProbe -ne [AoNative]::INVALID_HANDLE) {
            [AoNative]::CloseHandle($hProbe) | Out-Null
            Write-Info "$devPath is still reachable - driver is live"
            $driverIsLive = $true
            break
        }
    }
}

if (-not $driverIsLive -and $Action -eq 'upgrade') {
    # Check if AO .sys files are locked
    foreach ($name in @('aocablea.sys', 'aocableb.sys')) {
        $path = Join-Path "$env:SystemRoot\System32\drivers" $name
        if ((Test-Path $path) -and -not (Test-FileUnlocked $path)) {
            Write-Info "$name is locked - driver is live"
            $driverIsLive = $true
            break
        }
    }
    # Legacy: only treat as live if service actually exists
    if (-not $driverIsLive) {
        $legacySvc = Get-Service 'VirtualAudioDriver' -ErrorAction SilentlyContinue
        if ($legacySvc -and $legacySvc.Status -ne 'Stopped') {
            $legacyPath = Join-Path "$env:SystemRoot\System32\drivers" 'virtualaudiodriver.sys'
            if ((Test-Path $legacyPath) -and -not (Test-FileUnlocked $legacyPath)) {
                Write-Info "virtualaudiodriver.sys is locked by live service"
                $driverIsLive = $true
            }
        }
    }
}

if ($driverIsLive) {
    foreach ($service in $loadedServices) {
        Write-Info "$($service.Name) is still loaded"
    }

    if ($ResumeAfterReboot) {
        Write-Err "AO kernel driver is still loaded even on the post-reboot resume path."
        Write-Err "The previous driver removal has not completed yet."
        Write-Err "Reboot once more, then run install.ps1 manually if the issue persists."
        exit 3010
    }

    # --- Try in-session quiesce (upgrade only - PREPARE_UNLOAD is a commit point) ---
    $quiesceOk = $false
    if ($Action -eq 'upgrade') {
        $step++
        Write-Step $step $totalSteps "Attempting in-session driver unload..."
        $quiesceOk = Invoke-PreUpgradeQuiesce
    }

    if ($quiesceOk) {
        Write-OK "Driver unloaded in-session - no reboot needed"
        # Clear state; fall through to normal install
        $loadedServices = @()
        $driverIsLive = $false
    } else {
        # === COMMIT POINT PASSED BUT UNLOAD FAILED ===
        # Control device may be destroyed; cannot recover in this session.
        # Fall back to reboot-resume immediately.
        Write-Info "In-session quiesce failed after commit point - falling back to reboot"

        $resumeCommand = Register-ResumeInstall -BuildConfig $Config -SkipSignature:$SkipSign
        Write-OK "Resume command registered for next sign-in"
        Write-Info $resumeCommand

        # Clean up whatever remains
        Remove-AllAODevices | Out-Null

        Write-Err "AO kernel driver could not be fully unloaded in this session."
        Write-Err "Reboot once and sign in; install.ps1 will resume automatically."

        if ($AutoReboot) {
            Write-Info "Restarting Windows in 5 seconds..."
            shutdown.exe /r /t 5 /c "AO Virtual Cable upgrade will resume after sign-in." | Out-Null
            exit 0
        }

        exit 3010
    }
}

$savedDevices = Save-DefaultDevices
if ($savedDevices.Render) { Write-Info "Saved render:  $($savedDevices.Render)" }
if ($savedDevices.Capture) { Write-Info "Saved capture: $($savedDevices.Capture)" }

if ($Action -eq 'upgrade') {
    $step++
    Write-Step $step $totalSteps "Removing existing installation..."
    $removal = Remove-AllAODevices
    Write-OK "Existing installation removed"

    if ($removal.LockedFiles.Count -gt 0) {
        $resumeCommand = Register-ResumeInstall -BuildConfig $Config -SkipSignature:$SkipSign
        Write-OK "Resume command registered for next sign-in"
        Write-Info $resumeCommand
        Write-Err "One or more loaded driver binaries are still locked in System32\\drivers."
        Write-Err "Windows will not activate the new package until after reboot."
        foreach ($path in $removal.LockedFiles) {
            Write-Info "Locked: $path"
        }
        Write-Err "Reboot once and sign in; install.ps1 will resume automatically."

        if ($AutoReboot) {
            Write-Info "Restarting Windows in 5 seconds..."
            shutdown.exe /r /t 5 /c "AO Virtual Cable upgrade will resume after sign-in." | Out-Null
            exit 0
        }

        exit 3010
    }
}

$step++
Write-Step $step $totalSteps "Checking test signing..."
Ensure-TestSigning

$sdkX64 = @(
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64",
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64",
    "C:\Program Files (x86)\Windows Kits\10\bin\x64"
)
$sdkX86 = @(
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x86",
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x86",
    "C:\Program Files (x86)\Windows Kits\10\bin\x86"
)
$stampInf = Find-Tool "stampinf.exe" ($sdkX86 + $sdkX64)
$signTool = Find-Tool "signtool.exe" $sdkX64
$inf2Cat = Find-Tool "inf2cat.exe" ($sdkX86 + $sdkX64)

if ($SkipSign) {
    Write-Info "Signing skipped (--SkipSign)"
} else {
    if (-not $signTool) {
        Write-Err "signtool.exe not found. Install Windows SDK."
        exit 1
    }
    if (-not $stampInf) {
        Write-Err "stampinf.exe not found. Install WDK."
        exit 1
    }
}

$step++
Write-Step $step $totalSteps "Staging fresh driver packages..."
$stageRoot = Join-Path $env:TEMP ("AOAudio\Virtual-Audio-Driver\{0}-{1:yyyyMMdd-HHmmss}" -f $Config, (Get-Date))
$stageA = Join-Path $stageRoot "CableA"
$stageB = Join-Path $stageRoot "CableB"

$pkgA = New-StagedDriverPackage -Name "aocablea" -SysPath $artifacts.SysA -InxPath $artifacts.InxA `
    -StageDir $stageA -StampInf $stampInf -SignTool $signTool -Inf2Cat $inf2Cat -DoSign:(-not $SkipSign)
$pkgB = New-StagedDriverPackage -Name "aocableb" -SysPath $artifacts.SysB -InxPath $artifacts.InxB `
    -StageDir $stageB -StampInf $stampInf -SignTool $signTool -Inf2Cat $inf2Cat -DoSign:(-not $SkipSign)

Write-OK "Staged Cable A package: $($pkgA.Dir)"
Write-OK "Staged Cable B package: $($pkgB.Dir)"

$step++
Write-Step $step $totalSteps "Syncing service binaries into System32\\drivers..."
Sync-System32DriverBinary -Label "Cable A" -SourcePath $pkgA.Sys -TargetFileName "aocablea.sys"
Sync-System32DriverBinary -Label "Cable B" -SourcePath $pkgB.Sys -TargetFileName "aocableb.sys"

$step++
Write-Step $step $totalSteps "Installing drivers via pnputil..."
$okA = Install-DriverPackage -Label "Cable A" -InfPath $pkgA.Inf -HardwareId "ROOT\AOCableA"
$okB = Install-DriverPackage -Label "Cable B" -InfPath $pkgB.Inf -HardwareId "ROOT\AOCableB"
if (-not $okA -and -not $okB) {
    Write-Err "Both drivers failed to install"
    exit 1
}

pnputil /scan-devices 2>$null | Out-Null
Start-Sleep -Seconds 2

$step++
Write-Step $step $totalSteps "Restarting AO device instances..."
foreach ($device in (Get-AoMediaDevices | Where-Object { $_.Status -eq 'OK' })) {
    Write-Info "Restarting $($device.InstanceId)"
    pnputil /restart-device "$($device.InstanceId)" 2>$null | Out-Null
}
Write-OK "Device restart pass complete"

if ($artifacts.ControlPanel) {
    $step++
    Write-Step $step $totalSteps "Installing Control Panel..."
    if (-not (Test-Path "$env:ProgramFiles\AOAudio")) {
        New-Item -ItemType Directory -Path "$env:ProgramFiles\AOAudio" -Force | Out-Null
    }
    Stop-Process -Name AOControlPanel -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
    $cpTarget = "$env:ProgramFiles\AOAudio\AOControlPanel.exe"
    $copied = $false
    for ($attempt = 0; $attempt -lt 3 -and -not $copied; $attempt++) {
        try {
            Copy-Item $artifacts.ControlPanel $cpTarget -Force -ErrorAction Stop
            $copied = $true
        } catch {
            if ($attempt -lt 2) {
                Start-Sleep -Seconds 1
            } else {
                throw
            }
        }
    }
    reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "AOControlPanel" `
        /t REG_SZ /d "`"$env:ProgramFiles\AOAudio\AOControlPanel.exe`"" /f 2>$null | Out-Null
    Write-OK "Control Panel installed"
} else {
    $step++
    Write-Step $step $totalSteps "Installing Control Panel..."
    Write-Info "Control Panel build not found - skipping"
}

$step++
Write-Step $step $totalSteps "Post-install verification..."
$installedA = "$env:SystemRoot\System32\drivers\aocablea.sys"
$installedB = "$env:SystemRoot\System32\drivers\aocableb.sys"
Start-Sleep -Seconds 2

foreach ($pair in @(
    @{Name="aocablea.sys"; Staged=$pkgA.Sys; Installed=$installedA},
    @{Name="aocableb.sys"; Staged=$pkgB.Sys; Installed=$installedB}
)) {
    if (-not (Test-Path $pair.Installed)) {
        Write-Err "$($pair.Name): not found in System32\\drivers"
        continue
    }

    $stagedHash = (Get-FileHash $pair.Staged -Algorithm SHA256).Hash
    $installedHash = (Get-FileHash $pair.Installed -Algorithm SHA256).Hash
    if ($stagedHash -eq $installedHash) {
        Write-OK "$($pair.Name): staged == installed (hash match)"
    } else {
        Write-Err "$($pair.Name): HASH MISMATCH"
        Write-Info "  Staged:    $($stagedHash.Substring(0,16))..."
        Write-Info "  Installed: $($installedHash.Substring(0,16))..."
    }
}

# Write install-manifest.json with actual installed hashes (post-signing)
$installManifest = @{
    timestamp = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
    config    = $Config
    artifacts = @{}
}
foreach ($pair in @(
    @{Name="aocablea.sys"; Path=$installedA},
    @{Name="aocableb.sys"; Path=$installedB}
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
$installManifest | ConvertTo-Json -Depth 4 | Set-Content -Path "$root\install-manifest.json" -Encoding UTF8
Write-OK "install-manifest.json written"

Restore-DefaultDevices $savedDevices

$step++
Write-Step $step $totalSteps "Cleaning stale AO Driver Store packages..."
$cleanup = Remove-StaleAoDriverPackages
if ($cleanup.Failed.Count -eq 0) {
    Write-OK "Driver Store cleanup complete"
} else {
    Write-Info "Some stale packages could not be removed right now"
}

Clear-ResumeInstall

Write-Host "`n============================================" -ForegroundColor Cyan
Write-Host " Installation complete!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host " Run verify-install.ps1 for full diagnostic." -ForegroundColor Gray
Write-Host ""

exit 0
