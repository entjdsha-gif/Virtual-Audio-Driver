$recDev = Get-AudioDevice -Recording
Write-Host "Input device: $($recDev.Name)"
Write-Host "Checking peak levels (play YouTube now)..."
Write-Host ""
for ($i = 1; $i -le 10; $i++) {
    $peak = Get-AudioDevice -RecordingPeak -ID $recDev.ID
    $bar = "#" * [math]::Floor($peak)
    Write-Host "[$i/10] Peak: $peak % $bar"
    Start-Sleep -Milliseconds 500
}
if ($peak -gt 0) {
    Write-Host "`n[OK] Audio signal detected! Loopback is working."
} else {
    Write-Host "`n[WARN] No signal. Make sure YouTube is playing and output is set to AO Cable A."
}
