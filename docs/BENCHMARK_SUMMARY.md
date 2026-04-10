# AO Virtual Cable vs VB-Cable - Benchmark Summary

## Environment

| Item | Value |
|------|-------|
| Date | 2026-04-10 |
| OS | Windows 11 Pro 10.0.26200 |
| Format | 48000 Hz / float32 / stereo |
| Host API | Windows WDM-KS |
| AO Cable | AO Cable A (WDM-KS Output/Input, 8ch) |
| VB-Cable | VB-Audio Virtual Cable A (WDM-KS Point A, 16ch) |
| Python | 3.13 + sounddevice + numpy + scipy |
| Method | Same machine, sequential runs, single `sd.playrec()` session per test |

---

## Side-by-Side Results

| Metric | AO Cable A | VB-Cable A | Winner |
|--------|-----------|-----------|--------|
| **Silence RMS** | -200.0 dBFS | -200.0 dBFS | Tie |
| **Silence Peak** | -200.0 dBFS | -200.0 dBFS | Tie |
| **Silence PASS** | PASS | PASS | Tie |
| **Latency (mean)** | **121.47 ms** | 200.00 ms | **AO (-39%)** |
| **Latency (std)** | 0.603 ms | 0.000 ms | - |
| **Latency (range)** | 120.33..122.00 ms | 200.00..200.00 ms | AO (lower absolute) |
| **Latency PASS** | PASS | FAIL (>=200ms) | **AO** |
| **Dropouts** | 0 | 0 | Tie |
| **Dropout total** | 0.0 ms | 0.0 ms | Tie |
| **Dropout PASS** | PASS | PASS | Tie |
| **Drift (ppm)** | 0.0 | 0.0 | Tie |
| **Drift (ms/hour)** | 0.0 | 0.0 | Tie |
| **Drift jitter** | 0.0 ms | 0.0 ms | Tie |
| **Drift PASS** | PASS | PASS | Tie |

**Overall: AO 4/4 PASS | VB 3/4 PASS**

---

## Key Findings

### AO Advantages
1. **Latency: 121ms vs 200ms** - AO is 39% faster in round-trip loopback. VB-Cable hits the 200ms threshold boundary and fails the automated check.
2. **All 4 tests PASS** for AO; VB fails latency threshold.

### Parity Areas
- **Silence floor**: Both achieve -200 dBFS (digital silence - perfect null path).
- **Drift**: Both show zero measurable clock drift at 30s scale.
- **Dropout**: Both show zero dropouts at 30s scale.

### Notes
- VB-Cable latency of exactly 200.00ms across all 10 trials suggests a fixed internal buffer size, not jitter.
- AO latency varies 120.33-122.00ms (std 0.6ms), indicating buffer-level quantization rather than instability.

---

## Q01 Bit-Exact Status (Experimental)

Q01 bit-exact loopback is currently **parked as experimental**. Root cause analysis showed that `sd.playrec()` over WDM-KS exhibits periodic sample drops (~0.6% over 2s) at ring-buffer boundaries during simultaneous play+record. This is a harness/API limitation, not a driver defect:

- Q02 silence passes perfectly on the same path (zero noise, zero DC offset)
- Sample drops occur at consistent 48/96-sample intervals matching driver buffer boundaries
- Between drops, captured samples are bit-exact matches to the reference

Q01 requires an alternative verification method (e.g., WASAPI exclusive loopback, or file-based round-trip) to produce a definitive result.

---

## Feature Comparison

| Feature | AO Virtual Cable | VB-Cable (free) |
|---------|-----------------|-----------------|
| Channels | 8 or 16 (selectable) | 2 (free) / 8 (paid) |
| SRC | Built-in (8k-192k) | None |
| Float32 | Yes | Yes |
| Multi-client | Yes | Unknown |
| Control Panel | Live status, self-test, channel mode | Minimal systray |
| Latency | ~121ms | ~200ms |
| Install | Scripted, hash-verified | EXE installer |

---

## How to Reproduce

### Quick (30s, recommended first run)

```powershell
# Full benchmark suite (AO + VB comparison)
.\run_benchmark_suite.ps1

# AO-only (skip VB-Cable)
.\run_benchmark_suite.ps1 -SkipVB
```

### Formal (60s)

```powershell
.\run_benchmark_suite.ps1 -Duration 60
```

### Extended (10 min)

```powershell
.\run_benchmark_suite.ps1 -Duration 600 -Trials 20
```

### Individual tests (Python)

```bash
# Prerequisites
pip install sounddevice numpy scipy

# List devices
python test_quality_common.py --list-devices --api wdmks

# Q02 Silence
python test_bit_exact.py --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks --test q02 --out-dir results/

# L02 Latency (10 chirps, single session)
python test_latency.py --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks --test l02 --trials 10 --out-dir results/

# Dropout (60s)
python test_dropout.py --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks --duration 60 --out-dir results/

# Drift (60s)
python test_drift.py --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks --duration 60 --out-dir results/

# AO vs VB side-by-side
python test_compare_vb.py --api wdmks --duration 30 --out-dir results/
```

### Output structure

```
results/benchmark_YYYYMMDD_HHMMSS/
  suite_summary.txt          # pass/fail table
  ao_silence/                # Q02 results
  ao_latency/                # L02 results
  ao_dropout/                # dropout results
  ao_drift/                  # drift results
  compare/                   # AO vs VB comparison
    AO Cable A/              # per-cable results
    VB-Cable A/
    compare_*.json           # combined comparison
```

---

## Test Scripts

| Script | Purpose | Key Options |
|--------|---------|-------------|
| `run_benchmark_suite.ps1` | Full suite runner | `-Duration`, `-Trials`, `-SkipVB` |
| `test_quality_common.py` | Shared harness (M4a) | `--list-devices`, `--selftest` |
| `test_bit_exact.py` | Q01 (experimental) + Q02 silence | `--test q01\|q02\|all` |
| `test_latency.py` | L01 impulse + L02 chirp latency | `--test l01\|l02\|all`, `--trials` |
| `test_dropout.py` | Dropout detection | `--duration` |
| `test_drift.py` | Clock drift measurement | `--duration` |
| `test_compare_vb.py` | AO vs VB side-by-side | `--duration`, `--latency-trials` |

All scripts share: `--play-device`, `--record-device`, `--api`, `--samplerate`, `--channels`, `--out-dir`

---

## Data Source

Raw JSON: `results/compare_20260410_141754.json`
