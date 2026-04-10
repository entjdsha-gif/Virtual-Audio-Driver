# AO Virtual Cable - Release Candidate Summary

**Version:** Post-M4 (2026-04-10)
**Status:** Feature-complete for initial release evaluation

---

## Milestone Status

| Milestone | Status | Key Outcome |
|-----------|--------|-------------|
| M1: Build Reproducibility + Install | COMPLETE | Hash-verified build, scripted install/upgrade/uninstall |
| M2: 16-Channel Selectable | COMPLETE | 8/16ch runtime switch, 16ch isolation verified |
| M3: Control Panel | COMPLETE | Live status, apply, self-test, channel restart, defaults |
| M4: Quality Measurement | COMPLETE | Automated benchmark suite, AO vs VB-Cable comparison |
| M5: Release Packaging | IN PROGRESS | Documentation, demo checklist, known limitations |

---

## Verified Capabilities

### Audio Features
- **108 format combinations**: PCM 16/24-bit, Float32, mono/stereo/5.1/7.1, 8kHz-192kHz
- **8/16-channel selectable**: Registry-driven, runtime switch via Control Panel
- **16-channel isolation**: Sequential RMS + simultaneous FFT verification, crosstalk < -60dB
- **Sample Rate Conversion**: Built-in SRC across 12 sample rates
- **Multi-client**: 2+ concurrent render streams verified
- **Two independent cables**: Cable A and Cable B

### Build & Install
- Reproducible build with hash manifest and timestamp verification
- PnP-based install/upgrade/uninstall via PowerShell
- Reboot-resume recovery for upgrade scenarios
- Driver Store cleanup for stale packages

### Control Panel
- Live runtime status (rate, depth, channels, stream state)
- Runtime apply (sample rate, max latency)
- Channel mode switch (8/16) with device restart
- One-click self-test
- Defaults reset

### Quality (Measured)

| Metric | AO Cable A | VB-Cable A | Verdict |
|--------|-----------|-----------|---------|
| Latency | **121 ms** | 200 ms | **AO 39% faster** |
| Silence floor | -200 dBFS | -200 dBFS | Tie (perfect) |
| Dropouts (30s) | 0 | 0 | Tie |
| Drift (30s) | 0 ppm | 0 ppm | Tie |
| **Overall** | **4/4 PASS** | 3/4 PASS | **AO wins** |

---

## Test Evidence

| Test | Script | Result |
|------|--------|--------|
| 108 formats | `test_formats.py` | PASS |
| 16ch isolation | `test_16ch_isolation.py` | 16/16 PASS (both stages) |
| IOCTL roundtrip | `test_ioctl_diag.py` | PASS |
| Q02 silence | `test_bit_exact.py --test q02` | PASS (-200 dBFS) |
| L02 latency | `test_latency.py --test l02` | PASS (121ms, std 0.6ms) |
| Dropout | `test_dropout.py` | PASS (0 events) |
| Drift | `test_drift.py` | PASS (0 ppm) |
| AO vs VB | `test_compare_vb.py` | AO 4/4, VB 3/4 |
| Build verify | `build-verify.ps1` | PASS |
| Install verify | `verify-install.ps1` | PASS |

---

## Known Limitations

See `docs/KNOWN_LIMITATIONS.md` for full details. Key items:

1. **Q01 bit-exact**: Experimental - blocked by `sd.playrec()` sample-drop artifact, not a driver issue
2. **Upgrade reboot**: Usually required due to kernel service lifecycle
3. **No driver signing**: Requires test signing mode
4. **Long-run tests**: 30-60s verified; 1hr/24hr runs pending

---

## Benchmark Reproduction

```powershell
# Quick (30s)
.\run_benchmark_suite.ps1

# Formal (60s)
.\run_benchmark_suite.ps1 -Duration 60

# Extended (10 min)
.\run_benchmark_suite.ps1 -Duration 600 -Trials 20
```

See `docs/BENCHMARK_SUMMARY.md` for detailed results and per-test commands.

---

## VB-Cable Surpass Criteria Assessment

From `docs/VBCABLE_SURPASS_PLAN.md` M5 criteria:

| Criterion | Status | Evidence |
|-----------|--------|----------|
| Feature superset (16ch + SRC + multi-client + float32) | MET | `test_formats.py`, `test_16ch_isolation.py` |
| Install experience (clean, hash-verified) | MET | `install.ps1`, `verify-install.ps1` |
| Measured quality (latency, no dropout) | MET | `BENCHMARK_SUMMARY.md`: AO 121ms vs VB 200ms |
| Management tools (live status, self-test) | MET | Control Panel verified in M3 |
| No known regressions | MET | `VALIDATION_MATRIX.md` - no regressions, Q01 is experimental (not regression) |

**All 5 surpass criteria are met with documented evidence.**

---

## File Reference

| Document | Purpose |
|----------|---------|
| `docs/RELEASE_CANDIDATE.md` | This summary |
| `docs/BENCHMARK_SUMMARY.md` | AO vs VB detailed comparison |
| `docs/VALIDATION_MATRIX.md` | Full test matrix with pass/fail |
| `docs/VBCABLE_SURPASS_PLAN.md` | Milestone plan and outcomes |
| `docs/KNOWN_LIMITATIONS.md` | Known issues and workarounds |
| `docs/DEMO_CHECKLIST.md` | Step-by-step demo procedure |
| `run_benchmark_suite.ps1` | One-command benchmark runner |
