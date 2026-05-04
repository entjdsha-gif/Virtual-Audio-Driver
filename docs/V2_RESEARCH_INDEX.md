# V2 Research Baseline — Asset Index

> **Architecture: `docs/AO_FIXED_PIPE_ARCHITECTURE.md`** (single source of truth).
> **Current state / next step: `docs/CURRENT_STATE.md`**.
> This file indexes the research assets (VB-Cable RE, prior design artifacts) that the
> architecture doc cites as evidence.

Branch: `feature/ao-pipeline-v2` @ ef50902 (frozen)
Date: 2026-04-12

## Purpose

This branch contains the V2 pipeline research: SessionPassthrough experiment,
VB-Cable reverse engineering, live call test harness, and diagnostic tooling.
The audio core design (packed 24-bit ring, sinc SRC) is superseded by
VB-style fixed-frame INT32 pipe in the new implementation branch.

All research assets here are reference material for the rewrite.

---

## 1. VB-Cable Reverse Engineering

### Ghidra Decompilation
| File | Description |
|------|-------------|
| `results/ghidra_decompile/vbcable_all_functions.c` | Full Ghidra decompile of VB-Cable A (297 functions, 12096 lines) |
| `results/ghidra_decompile/vbcable_function_index.txt` | Function list with addresses and sizes |

### Analysis Documents
| File | Description |
|------|-------------|
| `results/vbcable_pipeline_analysis.md` | Complete pipeline trace: DPC timer, Speaker→Ring, Ring→Mic, position tracking, format handling |
| `results/vbcable_disasm_analysis.md` | FUN_1400026a0 deep dive: SRC algorithm, ring buffer struct layout, overflow handling |
| `results/vbcable_func26a0.asm` | Raw disassembly of largest function (4808 bytes) |

### Ghidra Scripts & Logs
| File | Description |
|------|-------------|
| `tools/ghidra_scripts/FindVbLatency.java` | Ghidra script: find 7168/48000/96000 constants, decompile init function |
| `tools/ghidra_scripts/find_vb_latency.py` | Python variant (requires PyGhidra) |
| `tools/ghidra_scripts/ExportDecompile.java` | Ghidra headless: decompile all functions to C |
| `results/ghidra_logs/vb_re_headless.txt` | Headless analysis log #1 |
| `results/ghidra_logs/vb_re_headless_2.txt` | Log #2 |
| `results/ghidra_logs/vb_re_headless_3.txt` | Log #3 |
| `results/ghidra_logs/vb_re_headless_4.txt` | Log #4 |
| `results/ghidra_logs/vb_re_headless_5.txt` | Log #5 |
| `results/ghidra_logs/vb_re_headless_6.txt` | Log #6 |
| `results/ghidra_logs/vb_re_headless_7.txt` | Log #7 — FindVbLatency output, registry keys, init decompile |

### Key Findings (carry to rewrite)
- INT32 ring buffer (4 bytes/sample, ~19-bit normalized)
- GCD 300/100/75 integer-ratio SRC with linear interpolation
- Overflow = hard reject + counter (return -3, never silent overwrite)
- DMA → scratch buffer linearization before processing
- Position recalculated to current QPC on every query
- 63/64 timer drift compensation, 8-frame minimum gate
- ExAllocateTimer 1ms, KeFlushQueuedDpcs on Pause

---

## 2. Live Call Test Harness

| File | Description |
|------|-------------|
| `tests/live_call/run_test_call.py` | One-command test: route→dial→TTS→AI conversation→restore |
| `tests/live_call/audio_bridge.py` | PyAudio capture/playback (48kHz mono) |
| `tests/live_call/audio_router.py` | System default device switching + per-app reverse routing |
| `tests/live_call/realtime_engine.py` | OpenAI Realtime API engine |
| `tests/live_call/phone_link_dialer.py` | Phone Link hidden URI dial |
| `tests/live_call/phone_link_worker.py` | Phone Link UI worker |
| `tests/live_call/tools/svcl/svcl.exe` | SoundVolumeView CLI for per-app routing |
| `tests/live_call/.env.example` | Config template |

### Confirmed Results (2026-04-11)
- VB-Cable: clean (TTS + AI Realtime both clear, transcription accurate)
- AO Cable: garbled (same path, driver quality issue confirmed)

---

## 3. Driver Diagnostics

| File | Description |
|------|-------------|
| `test_stream_monitor.py` | V2 IOCTL consumer: PipeFill, PullLoss delta, WRITE/IDLE state |
| `Source/Main/ioctl.h` | AO_V2_DIAG struct (SessionPassthrough, loss counters, PipeFillFrames) |
| `Source/Main/adapter.cpp` | GET_STREAM_STATUS V2 extension + RESET_COUNTERS handler |

---

## 4. Documents

| File | Description |
|------|-------------|
| `docs/AO_V2_ARCHITECTURE_PLAN.md` | V2 SessionPassthrough design (superseded by fixed-pipe rewrite) |
| `docs/VB_CABLE_PATH_ANALYSIS.md` | VB-Cable reference analysis |
| `docs/VBCABLE_ReferenceManual.pdf` | VB-Cable official manual |
| `docs/PIPELINE_V2_CHANGELOG.md` | V2 change log (Phase 1-4 + test harness) |
| `docs/TELEPHONY_RAW_CHANGELOG.md` | V1 history (reference only) |

---

## 5. Benchmark Data

| Directory | Description |
|-----------|-------------|
| `results/benchmark_20260410_143156/` | AO vs VB-Cable automated comparison (silence, latency, dropout, drift) |
| `results/AO Cable A/`, `results/VB-Cable A/` | Individual test runs |

Note: `.wav` and `.npy` files are large captures — not carried to rewrite branch.
Only `compare_*.json`, `results.csv`, and `suite_summary.txt` have reference value.

---

## 6. V2 Audio Core (superseded)

These commits contain the experimental audio core that is NOT carried forward:
- `4c6fc75` Phase 1: SessionPassthrough + loss counters
- `5455a23` Phase 2: IOCTL diagnostics
- `3ee8332` Phase 3: timing + generalization
- `ef50902` Phase 4: quality gap isolation

The code is preserved here for reference but the ring buffer design
(packed 24-bit, sinc SRC, MicSink dual-write) is replaced in the rewrite.

### Independent commit to cherry-pick:
- `eb699f2` Fix no-reboot upgrade (applies cleanly to main)
