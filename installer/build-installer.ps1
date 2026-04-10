<#
.SYNOPSIS
    Build the AO Virtual Cable installer package.

.DESCRIPTION
    Collects pre-built, pre-signed, pre-stamped driver files and the
    Control Panel into a self-contained installer directory that can be
    zipped or wrapped into an EXE. No WDK/SDK required on the target PC.

    Inputs (from build output):
      - aocablea.sys + aocablea.inf (pre-stamped from .inx)
      - aocableb.sys + aocableb.inf
      - aocablea.cat + aocableb.cat (if signed)
      - AOControlPanel.exe
      - install-core.ps1 (embedded installer logic)

.PARAMETER Config
    Debug or Release (default: Release)

.PARAMETER OutDir
    Output directory for the installer package (default: installer\out)
#>
param(
    [ValidateSet('Debug','Release')]
    [string]$Config = 'Release',

    [string]$OutDir = (Join-Path $PSScriptRoot "out")
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

# Source paths
$buildA = Join-Path $root "Source\Main\x64\$Config\CableA"
$buildB = Join-Path $root "Source\Main\x64\$Config\CableB"
$cpBuild = Join-Path $root "Source\ControlPanel\x64\$Config"

# Staging directories from install.ps1 (pre-stamped .inf files)
# If .tmp-stage-a/b exist with stamped .inf, use those; otherwise use build output
$stageA = Join-Path $root ".tmp-stage-a"
$stageB = Join-Path $root ".tmp-stage-b"

# Validate required files exist
$required = @(
    @{Name="aocablea.sys"; Path=(Join-Path $buildA "aocablea.sys")},
    @{Name="aocableb.sys"; Path=(Join-Path $buildB "aocableb.sys")}
)

foreach ($file in $required) {
    if (-not (Test-Path $file.Path)) {
        Write-Error "$($file.Name) not found at $($file.Path). Run build first."
        exit 1
    }
}

# Create output directory
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

$pkgDir = Join-Path $OutDir "AOVirtualCable"
if (Test-Path $pkgDir) { Remove-Item $pkgDir -Recurse -Force }
New-Item -ItemType Directory -Path $pkgDir -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $pkgDir "drivers\CableA") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $pkgDir "drivers\CableB") -Force | Out-Null

# Copy driver files
foreach ($cable in @(
    @{Name="CableA"; Build=$buildA; Stage=$stageA; Prefix="aocablea"},
    @{Name="CableB"; Build=$buildB; Stage=$stageB; Prefix="aocableb"}
)) {
    $dest = Join-Path $pkgDir "drivers\$($cable.Name)"

    # .sys from build
    Copy-Item (Join-Path $cable.Build "$($cable.Prefix).sys") $dest -Force

    # .inf: prefer pre-stamped from stage dir, fallback to build output
    $infStage = Join-Path $cable.Stage "$($cable.Prefix).inf"
    $infBuild = Join-Path $cable.Build "$($cable.Prefix).inf"
    if (Test-Path $infStage) {
        Copy-Item $infStage $dest -Force
        Write-Host "  $($cable.Prefix).inf: from staging (pre-stamped)"
    } elseif (Test-Path $infBuild) {
        Copy-Item $infBuild $dest -Force
        Write-Host "  $($cable.Prefix).inf: from build output"
    } else {
        Write-Error "$($cable.Prefix).inf not found in staging or build"
        exit 1
    }

    # .cat (optional)
    $catStage = Join-Path $cable.Stage "$($cable.Prefix).cat"
    $catBuild = Join-Path $cable.Build "$($cable.Prefix).cat"
    if (Test-Path $catStage) {
        Copy-Item $catStage $dest -Force
    } elseif (Test-Path $catBuild) {
        Copy-Item $catBuild $dest -Force
    }
}

# Control Panel
$cpExe = Join-Path $cpBuild "AOControlPanel.exe"
if (Test-Path $cpExe) {
    Copy-Item $cpExe $pkgDir -Force
    Write-Host "  AOControlPanel.exe: included"
} else {
    Write-Host "  AOControlPanel.exe: not found (skipping)"
}

# Copy installer scripts
Copy-Item (Join-Path $PSScriptRoot "install-core.ps1") $pkgDir -Force
Copy-Item (Join-Path $PSScriptRoot "Setup.bat") $pkgDir -Force
Copy-Item (Join-Path $PSScriptRoot "Uninstall.bat") $pkgDir -Force

# Generate manifest
$manifest = @{
    version = Get-Date -Format "yyyy.MM.dd.HHmm"
    files = @{}
}
Get-ChildItem $pkgDir -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring($pkgDir.Length + 1)
    $hash = (Get-FileHash $_.FullName -Algorithm SHA256).Hash
    $manifest.files[$rel] = $hash
}
$manifest | ConvertTo-Json -Depth 3 | Set-Content (Join-Path $pkgDir "manifest.json") -Encoding UTF8

Write-Host ""
Write-Host "Installer package built: $pkgDir" -ForegroundColor Green
Write-Host "Files:"
Get-ChildItem $pkgDir -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring($pkgDir.Length + 1)
    Write-Host "  $rel ($([math]::Round($_.Length/1KB, 1)) KB)"
}
Write-Host ""
Write-Host "To create distributable: zip $pkgDir or wrap with SFX tool"
