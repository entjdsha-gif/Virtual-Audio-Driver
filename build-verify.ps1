<#
.SYNOPSIS
    AO Virtual Cable - Build Output Verification
.DESCRIPTION
    Validates build artifacts after compilation:
    - Resolves the newest valid output tree per artifact
    - Ensures Utilities/Filters were built before CableA/B
    - Detects unstamped INF files
    - Records SHA256 hashes to build-manifest.json
#>
param(
    [ValidateSet('Debug','Release')]
    [string]$Config = 'Debug',

    [ValidateSet('x64','ARM64')]
    [string]$Platform = 'x64'
)

$ErrorActionPreference = "Continue"
$pass = 0
$fail = 0

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

$root = Split-Path -Parent $MyInvocation.MyCommand.Path

$utilLib = Resolve-NewestExistingPath @(
    "$root\Source\Utilities\$Platform\$Config\Utilities.lib",
    "$root\$Platform\$Config\Utilities.lib"
)
$filtLib = Resolve-NewestExistingPath @(
    "$root\Source\Filters\$Platform\$Config\Filters.lib",
    "$root\$Platform\$Config\Filters.lib"
)
$cableASys = Resolve-NewestExistingPath @(
    "$root\Source\Main\$Platform\$Config\CableA\aocablea.sys",
    "$root\$Platform\$Config\CableA\aocablea.sys"
)
$cableBSys = Resolve-NewestExistingPath @(
    "$root\Source\Main\$Platform\$Config\CableB\aocableb.sys",
    "$root\$Platform\$Config\CableB\aocableb.sys"
)
$cableAInf = Resolve-NewestExistingPath @(
    "$root\Source\Main\$Platform\$Config\CableA\aocablea.inf",
    "$root\$Platform\$Config\CableA\aocablea.inf"
)
$cableBInf = Resolve-NewestExistingPath @(
    "$root\Source\Main\$Platform\$Config\CableB\aocableb.inf",
    "$root\$Platform\$Config\CableB\aocableb.inf"
)
$cpExe = Resolve-NewestExistingPath @(
    "$root\Source\ControlPanel\$Platform\$Config\AOControlPanel.exe",
    "$root\$Platform\$Config\AOControlPanel.exe"
)

Write-Host "============================================" -ForegroundColor Cyan
Write-Host " AO Virtual Cable - Build Verification" -ForegroundColor Cyan
Write-Host " Config: $Config | Platform: $Platform" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

Write-Host "`n[1/6] File Existence" -ForegroundColor Cyan
Test-Check "Utilities.lib" ($null -ne $utilLib) | Out-Null
Test-Check "Filters.lib" ($null -ne $filtLib) | Out-Null
Test-Check "aocablea.sys" ($null -ne $cableASys) | Out-Null
Test-Check "aocableb.sys" ($null -ne $cableBSys) | Out-Null
Test-Check "aocablea.inf" ($null -ne $cableAInf) | Out-Null
Test-Check "aocableb.inf" ($null -ne $cableBInf) | Out-Null
Test-Check "AOControlPanel.exe" ($null -ne $cpExe) | Out-Null

Write-Host "`n[2/6] Build Order (lib before sys)" -ForegroundColor Cyan
foreach ($pair in @(
    @{Lib=$utilLib; LibName="Utilities.lib"; Sys=$cableASys; SysName="aocablea.sys"},
    @{Lib=$utilLib; LibName="Utilities.lib"; Sys=$cableBSys; SysName="aocableb.sys"},
    @{Lib=$filtLib; LibName="Filters.lib";   Sys=$cableASys; SysName="aocablea.sys"},
    @{Lib=$filtLib; LibName="Filters.lib";   Sys=$cableBSys; SysName="aocableb.sys"}
)) {
    if ($pair.Lib -and $pair.Sys) {
        $libTime = (Get-Item $pair.Lib).LastWriteTime
        $sysTime = (Get-Item $pair.Sys).LastWriteTime
        Test-Check "$($pair.LibName) <= $($pair.SysName)" ($libTime -le $sysTime) `
            "($($libTime.ToString('HH:mm:ss')) <= $($sysTime.ToString('HH:mm:ss')))" | Out-Null
    } else {
        Test-Check "$($pair.LibName) <= $($pair.SysName)" $false "(files missing)" | Out-Null
    }
}

Write-Host "`n[3/6] INF Stamping" -ForegroundColor Cyan
if ($cableAInf) {
    Test-Check "aocablea.inf stamped" (Test-InfStamped $cableAInf) "($cableAInf)" | Out-Null
}
if ($cableBInf) {
    Test-Check "aocableb.inf stamped" (Test-InfStamped $cableBInf) "($cableBInf)" | Out-Null
}

Write-Host "`n[4/6] Binary Distinction" -ForegroundColor Cyan
if ($cableASys -and $cableBSys) {
    $hashA = (Get-FileHash $cableASys -Algorithm SHA256).Hash
    $hashB = (Get-FileHash $cableBSys -Algorithm SHA256).Hash
    Test-Check "CableA != CableB binary" ($hashA -ne $hashB) | Out-Null
} else {
    Test-Check "CableA != CableB binary" $false "(files missing)" | Out-Null
}

Write-Host "`n[5/6] Build Manifest" -ForegroundColor Cyan
$manifest = @{
    timestamp = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
    config = $Config
    platform = $Platform
    artifacts = @{}
}

$files = @{
    "Utilities.lib"      = $utilLib
    "Filters.lib"        = $filtLib
    "aocablea.sys"       = $cableASys
    "aocableb.sys"       = $cableBSys
    "aocablea.inf"       = $cableAInf
    "aocableb.inf"       = $cableBInf
    "AOControlPanel.exe" = $cpExe
}

foreach ($name in $files.Keys) {
    $path = $files[$name]
    if ($path -and (Test-Path $path)) {
        $item = Get-Item $path
        $manifest.artifacts[$name] = @{
            path = $path
            sha256 = (Get-FileHash $path -Algorithm SHA256).Hash
            size = $item.Length
            modified = $item.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss")
        }
    }
}

$manifestPath = "$root\build-manifest.json"
$manifest | ConvertTo-Json -Depth 4 | Set-Content -Path $manifestPath -Encoding UTF8
Test-Check "build-manifest.json written" (Test-Path $manifestPath) | Out-Null
Write-Host "       Path: $manifestPath" -ForegroundColor Gray

Write-Host "`n[6/6] Size Sanity" -ForegroundColor Cyan
foreach ($sys in @($cableASys, $cableBSys)) {
    if ($sys -and (Test-Path $sys)) {
        $item = Get-Item $sys
        Test-Check "$($item.Name) size > 50KB" ($item.Length -gt 50000) "($([math]::Round($item.Length / 1KB))KB)" | Out-Null
    }
}

Write-Host "`n============================================" -ForegroundColor Cyan
Write-Host " RESULT: $pass PASS / $fail FAIL" -ForegroundColor $(if ($fail -eq 0) { "Green" } else { "Red" })
Write-Host "============================================" -ForegroundColor Cyan

exit $(if ($fail -eq 0) { 0 } else { 1 })
