<#
.SYNOPSIS
    AO Virtual Cable - Full Benchmark Suite Runner

.DESCRIPTION
    Runs the complete M4 quality measurement suite against AO Cable A
    and VB-Cable A, saving all results to a timestamped folder.

    Tests executed (in order):
      1. Q02 Silence/null (AO)
      2. L02 Chirp latency (AO, 10 chirps)
      3. Dropout detection (AO)
      4. Clock drift (AO)
      5. AO vs VB-Cable comparison

.PARAMETER Duration
    Duration in seconds for dropout/drift tests (default: 30)

.PARAMETER Trials
    Number of latency chirp probes (default: 10)

.PARAMETER Api
    Host API filter (default: wdmks)

.PARAMETER OutDir
    Base output directory (default: results)

.PARAMETER SkipVB
    Skip VB-Cable comparison (run AO-only tests)

.EXAMPLE
    .\run_benchmark_suite.ps1
    .\run_benchmark_suite.ps1 -Duration 60
    .\run_benchmark_suite.ps1 -Duration 600 -Trials 20
    .\run_benchmark_suite.ps1 -SkipVB
#>

param(
    [int]$Duration = 30,
    [int]$Trials = 10,
    [string]$Api = "wdmks",
    [string]$OutDir = "results",
    [switch]$SkipVB
)

$ErrorActionPreference = "Continue"

# Timestamp for this run
$ts = Get-Date -Format "yyyyMMdd_HHmmss"
$runDir = Join-Path $OutDir "benchmark_$ts"
New-Item -ItemType Directory -Path $runDir -Force | Out-Null

# Device names
$aoPlay = "AO Cable A Output"
$aoRec  = "AO Cable A Input"

Write-Host ""
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host " AO Virtual Cable - Benchmark Suite" -ForegroundColor Cyan
Write-Host " Time:     $ts" -ForegroundColor Cyan
Write-Host " API:      $Api" -ForegroundColor Cyan
Write-Host " Duration: ${Duration}s (dropout/drift)" -ForegroundColor Cyan
Write-Host " Trials:   $Trials (latency chirps)" -ForegroundColor Cyan
Write-Host " Output:   $runDir" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

$results = @()
$failed = 0

function Run-Test {
    param([string]$Name, [string]$Cmd)
    Write-Host "--- $Name ---" -ForegroundColor Yellow
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    Invoke-Expression $Cmd
    $code = $LASTEXITCODE
    $sw.Stop()
    $status = if ($code -eq 0) { "PASS" } else { "FAIL" }
    $color = if ($code -eq 0) { "Green" } else { "Red" }
    Write-Host "  [$status] $Name (${($sw.Elapsed.TotalSeconds.ToString('F1'))}s)" -ForegroundColor $color
    Write-Host ""
    $script:results += [PSCustomObject]@{
        Test = $Name
        Status = $status
        ExitCode = $code
        Seconds = [math]::Round($sw.Elapsed.TotalSeconds, 1)
    }
    if ($code -ne 0) { $script:failed++ }
}

# ── 1. Q02 Silence ──
Run-Test "Q02 Silence (AO)" "python test_bit_exact.py --play-device `"$aoPlay`" --record-device `"$aoRec`" --api $Api --test q02 --out-dir `"$runDir\ao_silence`""

# ── 2. L02 Latency ──
Run-Test "L02 Latency (AO)" "python test_latency.py --play-device `"$aoPlay`" --record-device `"$aoRec`" --api $Api --test l02 --trials $Trials --out-dir `"$runDir\ao_latency`""

# ── 3. Dropout ──
Run-Test "Dropout (AO)" "python test_dropout.py --play-device `"$aoPlay`" --record-device `"$aoRec`" --api $Api --duration $Duration --out-dir `"$runDir\ao_dropout`""

# ── 4. Drift ──
Run-Test "Drift (AO)" "python test_drift.py --play-device `"$aoPlay`" --record-device `"$aoRec`" --api $Api --duration $Duration --out-dir `"$runDir\ao_drift`""

# ── 5. AO vs VB Comparison ──
if (-not $SkipVB) {
    Run-Test "AO vs VB Comparison" "python test_compare_vb.py --api $Api --duration $Duration --latency-trials $Trials --out-dir `"$runDir\compare`""
}

# ── Summary ──
Write-Host ""
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host " Benchmark Suite Summary" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host ("  {0,-30} {1,-8} {2,8}" -f "Test", "Status", "Time(s)")
Write-Host ("  {0,-30} {1,-8} {2,8}" -f "----", "------", "-------")
foreach ($r in $results) {
    $color = if ($r.Status -eq "PASS") { "Green" } else { "Red" }
    Write-Host ("  {0,-30} {1,-8} {2,8}" -f $r.Test, $r.Status, $r.Seconds) -ForegroundColor $color
}
Write-Host ""

$total = $results.Count
$passed = $total - $failed
Write-Host "  Result: $passed/$total PASS" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Yellow" })
Write-Host "  Output: $runDir"
Write-Host ""

# Save summary
$summaryPath = Join-Path $runDir "suite_summary.txt"
$results | Format-Table -AutoSize | Out-String | Set-Content -Path $summaryPath -Encoding UTF8
"Run: $ts  Duration: ${Duration}s  Trials: $Trials  API: $Api  Result: $passed/$total PASS" | Add-Content -Path $summaryPath

Write-Host "  Summary saved: $summaryPath"
Write-Host ""

exit $failed
