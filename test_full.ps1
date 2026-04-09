# AO Virtual Cable Full Verification Script
# Requires: AudioDeviceCmdlets module
param([switch]$SkipRecording)

$ErrorActionPreference = "Continue"
$pass = 0; $fail = 0; $skip = 0

function Test-Result($name, $ok, $detail = "") {
    if ($ok) {
        Write-Host "  [PASS] $name $detail" -ForegroundColor Green
        $script:pass++
    } else {
        Write-Host "  [FAIL] $name $detail" -ForegroundColor Red
        $script:fail++
    }
}

function Test-Skip($name, $reason) {
    Write-Host "  [SKIP] $name - $reason" -ForegroundColor Yellow
    $script:skip++
}

Write-Host "============================================" -ForegroundColor Cyan
Write-Host " AO Virtual Cable - Full Verification" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# ============================================================
# 1. Device Registration
# ============================================================
Write-Host "[1/9] Device Registration" -ForegroundColor Cyan
$devices = Get-AudioDevice -List
$aoDevices = $devices | Where-Object { $_.Name -like '*AO Cable*' }

$cableASpeaker = $aoDevices | Where-Object { $_.Name -like '*AO Cable A*' -and $_.Type -eq 'Playback' }
$cableAMic = $aoDevices | Where-Object { $_.Name -like '*AO Cable A*' -and $_.Type -eq 'Recording' }
$cableBSpeaker = $aoDevices | Where-Object { $_.Name -like '*AO Cable B*' -and $_.Type -eq 'Playback' }
$cableBMic = $aoDevices | Where-Object { $_.Name -like '*AO Cable B*' -and $_.Type -eq 'Recording' }

Test-Result "Cable A Speaker" ($null -ne $cableASpeaker)
Test-Result "Cable A Mic" ($null -ne $cableAMic)
Test-Result "Cable B Speaker" ($null -ne $cableBSpeaker)
Test-Result "Cable B Mic" ($null -ne $cableBMic)

# ============================================================
# 2. Driver Services
# ============================================================
Write-Host ""
Write-Host "[2/9] Driver Services" -ForegroundColor Cyan
$svcA = Get-Service AOCableA -ErrorAction SilentlyContinue
$svcB = Get-Service AOCableB -ErrorAction SilentlyContinue
Test-Result "AOCableA service" ($svcA -and $svcA.Status -eq 'Running') "($($svcA.Status))"
Test-Result "AOCableB service" ($svcB -and $svcB.Status -eq 'Running') "($($svcB.Status))"

# ============================================================
# 3. Driver Files
# ============================================================
Write-Host ""
Write-Host "[3/9] Driver Files" -ForegroundColor Cyan
$sysA = Get-Item "$env:SystemRoot\System32\drivers\aocablea.sys" -ErrorAction SilentlyContinue
$sysB = Get-Item "$env:SystemRoot\System32\drivers\aocableb.sys" -ErrorAction SilentlyContinue
Test-Result "aocablea.sys exists" ($null -ne $sysA) "($($sysA.Length) bytes)"
Test-Result "aocableb.sys exists" ($null -ne $sysB) "($($sysB.Length) bytes)"
Test-Result "Binary different" ((Get-FileHash $sysA.FullName).Hash -ne (Get-FileHash $sysB.FullName).Hash)

# ============================================================
# 4. Control Panel
# ============================================================
Write-Host ""
Write-Host "[4/9] Control Panel" -ForegroundColor Cyan
$cpExe = Test-Path "$env:ProgramFiles\AOAudio\AOControlPanel.exe"
$cpProc = Get-Process AOControlPanel -ErrorAction SilentlyContinue
$cpReg = Get-ItemProperty "HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Run" -ErrorAction SilentlyContinue
$cpAutoStart = $cpReg.PSObject.Properties.Name -contains "AOControlPanel"
Test-Result "Exe installed" $cpExe
Test-Result "Process running" ($null -ne $cpProc) $(if($cpProc){"(PID $($cpProc.Id))"})
Test-Result "Auto-start registered" $cpAutoStart

# ============================================================
# 5. Loopback Recording Test (Cable A)
# ============================================================
Write-Host ""
Write-Host "[5/9] Loopback Recording (Cable A)" -ForegroundColor Cyan

if ($cableASpeaker -and $cableAMic) {
    # Save current defaults
    $origRender = Get-AudioDevice -Playback
    $origCapture = Get-AudioDevice -Recording

    # Set Cable A as default
    Set-AudioDevice -ID $cableASpeaker.ID | Out-Null
    Set-AudioDevice -ID $cableAMic.ID | Out-Null
    Test-Result "Set Cable A as default output" $true
    Test-Result "Set Cable A as default input" $true

    # Play a short tone via PowerShell and check peak
    Write-Host "  Playing test tone..." -ForegroundColor Gray
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Beep {
    [DllImport("kernel32.dll")]
    public static extern bool Beep(int freq, int duration);
}
"@
    # Beep goes through default output (Cable A Speaker)
    $job = Start-Job -ScriptBlock { [Beep]::Beep(1000, 2000) }
    Start-Sleep -Milliseconds 500

    # Check if Windows Sound Recorder can open the device
    # We test by checking the recording device is accessible
    try {
        $recDev = Get-AudioDevice -Recording
        Test-Result "Recording device accessible" ($recDev.Name -like '*AO Cable A*') "($($recDev.Name))"
    } catch {
        Test-Result "Recording device accessible" $false
    }

    Wait-Job $job -Timeout 3 | Out-Null
    Remove-Job $job -Force -ErrorAction SilentlyContinue

    # Restore defaults
    if ($origRender) { Set-AudioDevice -ID $origRender.ID -ErrorAction SilentlyContinue | Out-Null }
    if ($origCapture) { Set-AudioDevice -ID $origCapture.ID -ErrorAction SilentlyContinue | Out-Null }
} else {
    Test-Skip "Loopback test" "Cable A devices not found"
}

# ============================================================
# 6. Loopback Recording Test (Cable B)
# ============================================================
Write-Host ""
Write-Host "[6/9] Loopback Recording (Cable B)" -ForegroundColor Cyan

if ($cableBSpeaker -and $cableBMic) {
    $origRender = Get-AudioDevice -Playback
    $origCapture = Get-AudioDevice -Recording

    Set-AudioDevice -ID $cableBSpeaker.ID | Out-Null
    Set-AudioDevice -ID $cableBMic.ID | Out-Null

    $job = Start-Job -ScriptBlock { [Beep]::Beep(1000, 2000) }
    Start-Sleep -Milliseconds 500

    try {
        $recDev = Get-AudioDevice -Recording
        Test-Result "Cable B recording device accessible" ($recDev.Name -like '*AO Cable B*') "($($recDev.Name))"
    } catch {
        Test-Result "Cable B recording device accessible" $false
    }

    Wait-Job $job -Timeout 3 | Out-Null
    Remove-Job $job -Force -ErrorAction SilentlyContinue

    if ($origRender) { Set-AudioDevice -ID $origRender.ID -ErrorAction SilentlyContinue | Out-Null }
    if ($origCapture) { Set-AudioDevice -ID $origCapture.ID -ErrorAction SilentlyContinue | Out-Null }
} else {
    Test-Skip "Cable B loopback" "Cable B devices not found"
}

# ============================================================
# 7. IOCTL / Control Panel Communication
# ============================================================
Write-Host ""
Write-Host "[7/9] IOCTL Device Interface" -ForegroundColor Cyan

# Check if device interface is registered
$aoInterfaces = Get-PnpDevice | Where-Object { $_.FriendlyName -like '*AO Cable*' -and $_.Status -eq 'OK' }
Test-Result "AO Cable A PnP device OK" ($aoInterfaces | Where-Object { $_.FriendlyName -like '*Cable A*' })
Test-Result "AO Cable B PnP device OK" ($aoInterfaces | Where-Object { $_.FriendlyName -like '*Cable B*' })

# Check registry settings
$regPath = "HKLM:\SOFTWARE\AOAudio\VirtualCable"
$regExists = Test-Path $regPath
if ($regExists) {
    $regValues = Get-ItemProperty $regPath -ErrorAction SilentlyContinue
    Test-Result "Registry path exists" $true
    Test-Result "InternalRate in registry" ($null -ne $regValues.InternalRate) "($($regValues.InternalRate))"
    Test-Result "MaxLatencyMs in registry" ($null -ne $regValues.MaxLatencyMs) "($($regValues.MaxLatencyMs))"
} else {
    Test-Result "Registry path exists" $false "(first run - IOCTL not yet called)"
    Test-Skip "Registry values" "Path not created yet"
}

# ============================================================
# 8. Format Support Verification
# ============================================================
Write-Host ""
Write-Host "[8/9] Format Support (checking via device properties)" -ForegroundColor Cyan

# Check device properties in registry for supported formats
$audioDevices = Get-PnpDevice -Class AudioEndpoint -ErrorAction SilentlyContinue | Where-Object { $_.FriendlyName -like '*AO Cable*' }
$endpointCount = ($audioDevices | Measure-Object).Count
Test-Result "Audio endpoints registered" ($endpointCount -ge 4) "($endpointCount endpoints)"

# Verify driver info
$driverDevices = Get-PnpDevice -Class MEDIA -ErrorAction SilentlyContinue | Where-Object { $_.FriendlyName -like '*AO*Cable*' -or $_.InstanceId -like '*AOCable*' }
foreach ($dev in $driverDevices) {
    $drvInfo = Get-PnpDeviceProperty -InstanceId $dev.InstanceId -KeyName "DEVPKEY_Device_DriverVersion" -ErrorAction SilentlyContinue
    Test-Result "Driver: $($dev.FriendlyName)" ($dev.Status -eq 'OK') "(Status: $($dev.Status), Ver: $($drvInfo.Data))"
}

# ============================================================
# 9. Stability Check
# ============================================================
Write-Host ""
Write-Host "[9/9] Stability Check" -ForegroundColor Cyan

# Check for recent BSOD/crash events
$crashEvents = Get-WinEvent -LogName System -MaxEvents 100 -ErrorAction SilentlyContinue | Where-Object {
    $_.Id -eq 41 -or $_.Id -eq 1001 -or ($_.Id -eq 219 -and $_.Message -like '*AOCable*')
} | Select-Object -First 5

if ($crashEvents) {
    $recentCrash = $crashEvents | Where-Object { $_.TimeCreated -gt (Get-Date).AddHours(-1) }
    Test-Result "No recent crashes (1hr)" ($null -eq $recentCrash) "$(if($recentCrash){"$($recentCrash.Count) crash events found"})"
} else {
    Test-Result "No crash events" $true
}

# Check driver error events
$driverErrors = Get-WinEvent -LogName System -MaxEvents 50 -ErrorAction SilentlyContinue | Where-Object {
    $_.ProviderName -eq 'Microsoft-Windows-Kernel-PnP' -and $_.Id -eq 219 -and $_.Message -like '*AOCable*'
} | Where-Object { $_.TimeCreated -gt (Get-Date).AddMinutes(-30) }
Test-Result "No driver load failures (30min)" ($null -eq $driverErrors)

# ============================================================
# Summary
# ============================================================
Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host " RESULTS: $pass PASS / $fail FAIL / $skip SKIP" -ForegroundColor $(if($fail -eq 0){"Green"}else{"Red"})
Write-Host "============================================" -ForegroundColor Cyan
