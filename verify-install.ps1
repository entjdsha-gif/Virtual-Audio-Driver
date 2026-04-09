<#
.SYNOPSIS
    AO Virtual Cable - Installation Verification Diagnostic
.DESCRIPTION
    Checks installed driver state and compares against build manifest:
    1. PnP device status (via Service name, not InstanceId)
    2. Service registry state
    3. Installed .sys file existence
    4. Hash comparison: built .sys vs installed .sys
    5. Driver store entries
    6. Control device access (PowerShell P/Invoke, no Python dependency)
#>
param(
    [string]$ManifestPath
)

$ErrorActionPreference = "Continue"
$pass = 0; $fail = 0; $warn = 0
$serviceInfo = @{}
$hashMismatch = $false
$activeDriverPackages = @()

function Test-Check($name, $ok, $detail = "") {
    if ($ok) {
        Write-Host "  [PASS] $name $detail" -ForegroundColor Green
        $script:pass++
    } else {
        Write-Host "  [FAIL] $name $detail" -ForegroundColor Red
        $script:fail++
    }
    return $ok
}
function Test-Warn($name, $detail = "") {
    Write-Host "  [WARN] $name $detail" -ForegroundColor Yellow
    $script:warn++
}

function Get-AoServiceInfo {
    param([string]$Name)

    $svc = Get-Service $Name -ErrorAction SilentlyContinue
    $imagePath = $null
    try {
        $imagePath = (Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Services\$Name" -Name ImagePath -ErrorAction Stop).ImagePath
    } catch {}

    [pscustomobject]@{
        Name = $Name
        Exists = ($null -ne $svc)
        Running = ($svc -and $svc.Status -eq 'Running')
        Status = if ($svc) { $svc.Status.ToString() } else { $null }
        ImagePath = $imagePath
    }
}

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $ManifestPath) { $ManifestPath = "$root\build-manifest.json" }

Write-Host "============================================" -ForegroundColor Cyan
Write-Host " AO Virtual Cable - Install Verification" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

# ============================================================
# 1. PnP Device Status (by Service name, not InstanceId)
# ============================================================
Write-Host "`n[1/6] PnP Device Status" -ForegroundColor Cyan

# Find devices by Service name — works regardless of InstanceId format
$devA = Get-PnpDevice -Class MEDIA -ErrorAction SilentlyContinue |
    Where-Object { $_.Service -eq 'AOCableA' }
$devB = Get-PnpDevice -Class MEDIA -ErrorAction SilentlyContinue |
    Where-Object { $_.Service -eq 'AOCableB' }

if ($devA) {
    Test-Check "AOCableA device found" $true "(Instance: $($devA.InstanceId))" | Out-Null
    Test-Check "AOCableA status OK" ($devA.Status -eq 'OK') "($($devA.Status))" | Out-Null
} else {
    Test-Check "AOCableA device found" $false | Out-Null
}

if ($devB) {
    Test-Check "AOCableB device found" $true "(Instance: $($devB.InstanceId))" | Out-Null
    Test-Check "AOCableB status OK" ($devB.Status -eq 'OK') "($($devB.Status))" | Out-Null
} else {
    Test-Check "AOCableB device found" $false | Out-Null
}

# Audio endpoints by FriendlyName
$endpoints = Get-PnpDevice -Class AudioEndpoint -ErrorAction SilentlyContinue |
    Where-Object { $_.FriendlyName -like '*AO Cable*' -and $_.Status -eq 'OK' }
$epCount = ($endpoints | Measure-Object).Count
Test-Check "Audio endpoints >= 4" ($epCount -ge 4) "($epCount found)" | Out-Null

# ============================================================
# 2. Service State
# ============================================================
Write-Host "`n[2/6] Service State" -ForegroundColor Cyan

foreach ($svc in @('AOCableA', 'AOCableB')) {
    $info = Get-AoServiceInfo $svc
    $serviceInfo[$svc] = $info

    Test-Check "$svc service exists" $info.Exists | Out-Null
    if ($info.Exists) {
        Test-Check "$svc service running" $info.Running "($($info.Status))" | Out-Null
        if ($info.ImagePath) {
            Write-Host "       $svc ImagePath: $($info.ImagePath)" -ForegroundColor Gray
        }
    }
}

# ============================================================
# 3. Installed .sys Files
# ============================================================
Write-Host "`n[3/6] Installed Driver Files" -ForegroundColor Cyan

$installedA = "$env:SystemRoot\System32\drivers\aocablea.sys"
$installedB = "$env:SystemRoot\System32\drivers\aocableb.sys"

foreach ($f in @($installedA, $installedB)) {
    $name = Split-Path -Leaf $f
    if (Test-Path $f) {
        $item = Get-Item $f
        Test-Check "$name exists" $true "($($item.Length) bytes, $($item.LastWriteTime.ToString('yyyy-MM-dd HH:mm:ss')))" | Out-Null
    } else {
        Test-Check "$name exists" $false | Out-Null
    }
}

# ============================================================
# 4. Hash Comparison: Built vs Installed
# ============================================================
Write-Host "`n[4/6] Hash Comparison (Installed vs Manifest)" -ForegroundColor Cyan

# Prefer install-manifest.json (has post-signing hashes) over build-manifest.json
$manifest = $null
$installManifestPath = Join-Path $root "install-manifest.json"
if (Test-Path $installManifestPath) {
    $manifest = Get-Content $installManifestPath -Raw | ConvertFrom-Json
    Write-Host "       Manifest: $installManifestPath (post-install)" -ForegroundColor Gray
    Write-Host "       Installed at: $($manifest.timestamp)" -ForegroundColor Gray
} elseif (Test-Path $ManifestPath) {
    $manifest = Get-Content $ManifestPath -Raw | ConvertFrom-Json
    Write-Host "       Manifest: $ManifestPath (pre-signing, may differ)" -ForegroundColor Gray
    Write-Host "       Built at: $($manifest.timestamp)" -ForegroundColor Gray
} else {
    Test-Warn "No manifest found" "Run build-verify.ps1 or install.ps1 first"
}

$hashMap = @{
    "aocablea.sys" = $installedA
    "aocableb.sys" = $installedB
}

foreach ($name in $hashMap.Keys) {
    $installedPath = $hashMap[$name]
    if (-not (Test-Path $installedPath)) { continue }

    $installedHash = (Get-FileHash $installedPath -Algorithm SHA256).Hash

    if ($manifest -and $manifest.artifacts.$name) {
        $builtHash = $manifest.artifacts.$name.sha256
        $builtSize = $manifest.artifacts.$name.size
        $installedSize = (Get-Item $installedPath).Length

        $hashMatch = ($installedHash -eq $builtHash)
        if (-not $hashMatch) { $hashMismatch = $true }
        Test-Check "$name hash match (built==installed)" $hashMatch `
            $(if(-not $hashMatch){"MISMATCH! Built=$($builtHash.Substring(0,12))... Installed=$($installedHash.Substring(0,12))..."}) | Out-Null

        if (-not $hashMatch) {
            Write-Host "       Built size:     $builtSize bytes" -ForegroundColor Yellow
            Write-Host "       Installed size: $installedSize bytes" -ForegroundColor Yellow
        }
    } else {
        Write-Host "       $name installed hash: $($installedHash.Substring(0,16))... (no manifest to compare)" -ForegroundColor Gray
    }
}

# ============================================================
# 5. Driver Store
# ============================================================
Write-Host "`n[5/6] Driver Store" -ForegroundColor Cyan

$pnpOutput = pnputil /enum-drivers 2>$null
$oemEntries = @()
$curOem = $null
foreach ($line in ($pnpOutput -split "`n")) {
    if ($line -match '(oem\d+\.inf)') { $curOem = $Matches[1] }
    if ($curOem -and $line -match 'aocable') {
        $oemEntries += $curOem.ToLowerInvariant()
        $curOem = $null
    }
}

$oemEntries = $oemEntries | Select-Object -Unique

foreach ($device in @($devA) + @($devB)) {
    if (-not $device) { continue }
    try {
        $activeDriverPackages += (Get-PnpDeviceProperty `
            -InstanceId $device.InstanceId `
            -KeyName 'DEVPKEY_Device_DriverInfPath' `
            -ErrorAction Stop).Data.ToLowerInvariant()
    } catch {}
}

$activeDriverPackages = $activeDriverPackages | Select-Object -Unique
$staleDriverPackages = @($oemEntries | Where-Object { $_ -notin $activeDriverPackages })

if ($oemEntries.Count -gt 0) {
    if ($activeDriverPackages.Count -gt 0) {
        Test-Check "Active AO Driver Store packages detected" $true "($($activeDriverPackages -join ', '))" | Out-Null
    } else {
        Test-Check "Active AO Driver Store packages detected" $false | Out-Null
    }

    if ($staleDriverPackages.Count -eq 0) {
        Test-Check "No stale AO Driver Store packages" $true | Out-Null
    } else {
        Test-Warn "Stale AO Driver Store packages remain" "($($staleDriverPackages -join ', '))"
    }
} else {
    Test-Check "Driver store entries found" $false "(no aocable* in driver store)" | Out-Null
}

# ============================================================
# 6. Control Device Access (PowerShell P/Invoke — no Python)
# ============================================================
Write-Host "`n[6/6] Control Device Access" -ForegroundColor Cyan

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public class AoDeviceTest {
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern IntPtr CreateFileW(
        string lpFileName, uint dwDesiredAccess, uint dwShareMode,
        IntPtr lpSecurityAttributes, uint dwCreationDisposition,
        uint dwFlagsAndAttributes, IntPtr hTemplateFile);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool CloseHandle(IntPtr hObject);

    static readonly IntPtr INVALID_HANDLE_VALUE = new IntPtr(-1);

    public static string TestOpen(string devicePath) {
        IntPtr h = CreateFileW(devicePath,
            0xC0000000u, // GENERIC_READ | GENERIC_WRITE
            0, IntPtr.Zero, 3, // OPEN_EXISTING
            0, IntPtr.Zero);
        if (h == INVALID_HANDLE_VALUE || h == IntPtr.Zero) {
            int err = Marshal.GetLastWin32Error();
            return "FAIL:" + err;
        }
        CloseHandle(h);
        return "OK";
    }
}
"@ -ErrorAction SilentlyContinue

foreach ($devName in @('\\.\AOCableA', '\\.\AOCableB')) {
    try {
        $result = [AoDeviceTest]::TestOpen($devName)
        if ($result -eq "OK") {
            Test-Check "Control device $devName" $true | Out-Null
        } else {
            $errCode = $result.Split(':')[1]
            Test-Check "Control device $devName" $false "(Win32 error $errCode)" | Out-Null
        }
    } catch {
        Test-Warn "Control device $devName test error" "$_"
    }
}

# ============================================================
# Summary
# ============================================================
Write-Host "`n============================================" -ForegroundColor Cyan
$color = if ($fail -eq 0 -and $warn -eq 0) {"Green"} elseif ($fail -eq 0) {"Yellow"} else {"Red"}
Write-Host " RESULT: $pass PASS / $fail FAIL / $warn WARN" -ForegroundColor $color
Write-Host "============================================" -ForegroundColor Cyan

$runningSystem32 = @(
    $serviceInfo['AOCableA'],
    $serviceInfo['AOCableB']
) | Where-Object { $_ -and $_.Running -and $_.ImagePath -like '*System32\\drivers\\aocable*' }

if ($hashMismatch -and $runningSystem32.Count -gt 0) {
    Write-Host "" 
    Test-Warn "Loaded kernel driver may still be stale" "running service points at System32\\drivers and built hash does not match"
    Write-Host "       This usually means the current boot session is still using the previous driver binary." -ForegroundColor Yellow
    Write-Host "       If install.ps1 already removed the old package, reboot once and let the deferred install resume." -ForegroundColor Yellow
}

if ($fail -gt 0) {
    Write-Host "`n Troubleshooting:" -ForegroundColor Yellow
    if ($hashMismatch -and $runningSystem32.Count -gt 0) {
        Write-Host "   Hash mismatch + running kernel service -> reboot once, sign in, then let install.ps1 resume" -ForegroundColor Yellow
    } else {
        Write-Host "   Hash mismatch -> Reinstall: .\install.ps1 -Action upgrade" -ForegroundColor Yellow
    }
    if ($staleDriverPackages.Count -gt 0) {
        Write-Host "   Stale AO packages -> Run: .\install.ps1 -Action cleanup" -ForegroundColor Yellow
    }
    Write-Host "   Device missing -> Install:  .\install.ps1 -Action install" -ForegroundColor Yellow
    Write-Host "   Service stopped -> Reboot or: sc start AOCableA" -ForegroundColor Yellow
} elseif ($staleDriverPackages.Count -gt 0) {
    Write-Host "`n Maintenance:" -ForegroundColor Yellow
    Write-Host "   Stale AO packages -> Run: .\install.ps1 -Action cleanup" -ForegroundColor Yellow
}

exit $(if($fail -eq 0){0}else{1})
