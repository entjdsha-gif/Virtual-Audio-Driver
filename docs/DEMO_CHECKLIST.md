# AO Virtual Cable - Demo Checklist

Minimum steps to install, verify, and demonstrate the driver.

---

## Prerequisites

- Windows 10/11 (x64), test signing enabled or secure boot off
- Python 3.10+ with `pip install sounddevice numpy scipy`
- PowerShell 5.1+
- Administrator privileges for install/uninstall

---

## 1. Build

```powershell
# Open VS Developer Command Prompt or use MSBuild directly
msbuild /p:Configuration=Release /p:Platform=x64
.\build-verify.ps1    # verify hashes and timestamps
```

---

## 2. Install

```powershell
# Install driver (requires elevation)
.\install.ps1 -Action install

# Verify installation
.\verify-install.ps1
```

Expected: Cable A and Cable B devices appear in Sound Settings. `verify-install.ps1` reports all checks PASS.

---

## 3. Quick Smoke Test

```powershell
# List devices (confirm AO Cable A/B visible)
python test_quality_common.py --list-devices --api wdmks

# Q02 Silence (should PASS in ~5s)
python test_bit_exact.py --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks --test q02

# L02 Latency (should PASS, ~121ms mean)
python test_latency.py --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks --test l02
```

---

## 4. Full Benchmark

```powershell
# 30s quick benchmark (AO + VB-Cable comparison)
.\run_benchmark_suite.ps1

# 60s formal benchmark
.\run_benchmark_suite.ps1 -Duration 60

# AO-only (no VB-Cable needed)
.\run_benchmark_suite.ps1 -SkipVB
```

Results saved to `results/benchmark_YYYYMMDD_HHMMSS/`.

---

## 5. Control Panel

1. Launch `ControlPanel.exe` from the build output
2. Verify:
   - **Runtime Status** shows current sample rate, bit depth, channel count for Cable A and Cable B
   - **Apply** changes sample rate and max latency at runtime
   - **Self-Test** passes (connectivity + config sanity)
   - **Defaults** resets to 48000 Hz / 20ms / 8ch

---

## 6. 16-Channel Mode

```powershell
# Switch Cable A to 16ch via Control Panel "Set & Restart"
# Or manually:
Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Services\AOCableA\Parameters" -Name "MaxChannelCount" -Value 16
# Restart device via Device Manager or Control Panel

# Verify 16ch isolation
python test_16ch_isolation.py
```

Expected: Stage 1 (sequential RMS) 16/16 PASS, Stage 2 (FFT crosstalk matrix) 16/16 PASS.

---

## 7. Format Coverage

```powershell
# Test all 108 formats (PCM 16/24, Float32, mono/stereo/5.1/7.1, 8k-192k)
python test_formats.py

# Quick subset
python test_formats.py --quick
```

---

## 8. Uninstall

```powershell
.\install.ps1 -Action uninstall
```

---

## Demo Talking Points

1. **Feature superset**: 8/16ch selectable, SRC 8k-192k, float32, multi-client
2. **Measured quality**: 121ms latency (39% faster than VB-Cable), zero dropout, zero drift
3. **Operational tooling**: Control Panel with live status, self-test, channel restart
4. **Reproducible**: Hash-verified build/install, automated benchmark suite
5. **108 format support**: PCM 16/24, float32, mono through 7.1, 12 sample rates
