# AO Virtual Cable - Current State

**Last updated:** 2026-04-25
**Primary active branch:** `feature/ao-fixed-pipe-rewrite`
**Primary active worktree:** `D:/mywork/Virtual-Audio-Driver`
**Effective last-known-good baseline:** `439bbcd`

## Source of truth

| Topic | File |
|---|---|
| **Architecture (single source)** | `docs/AO_FIXED_PIPE_ARCHITECTURE.md` |
| Current state / next step (this file) | `docs/CURRENT_STATE.md` |
| VB-Cable evidence (Ghidra) | `results/vbcable_pipeline_analysis.md`, `results/vbcable_disasm_analysis.md`, `results/vbcable_capture_contract_answers.md`, `results/ghidra_decompile/`, `results/ghidra_logs/` |
| VB-Cable evidence (WinDbg) | `results/phase6_vb_verification.md`, `results/vb_session.log` |
| Live-call test harness | `tests/live_call/run_test_call.py` |
| Driver-level diagnostics | `test_stream_monitor.py` |
| Changelog | `docs/PIPELINE_V2_CHANGELOG.md` |
| Older planning (archived) | `docs/archive/`, `results/archive/` |

If two documents disagree, the architecture doc wins. If the architecture doc
is silent on a topic, runtime evidence wins. If runtime evidence is silent,
this file (CURRENT_STATE.md) provides the operational answer.

## Where we are

The cable-stream rewrite has its design frozen. The next step is implementation Stage 1.

### Confirmed

- VB-Cable Ghidra static analysis complete (12,096 lines decompile, 297 functions)
- VB-Cable WinDbg dynamic verification done under live TTS payload
- AO vs VB head-to-head live call: **VB clean, AO garbled** on the same path → driver-core problem confirmed
- All historical Phase 5 / Step 3-4 / Option Z / Option Y attempts are recorded and archived
- Branches consolidated to single `feature/ao-fixed-pipe-rewrite`
- Single architecture document `docs/AO_FIXED_PIPE_ARCHITECTURE.md` consolidates all prior planning

### Active branch state

`feature/ao-fixed-pipe-rewrite` currently contains the last stable Phase 6 Y2-2 implementation (render audible flag flipped to helper, capture remains on legacy path). Y3 attempts are reverted.

## Implementation stages

See `docs/AO_FIXED_PIPE_ARCHITECTURE.md` § 15 for full stage definitions and gates.

| Stage | Scope | Status |
|---|---|---|
| 0 | Baseline & evidence | ✓ done |
| 1 | INT32 frame-indexed ring + hard-reject overflow | next |
| 2 | Single-pass SRC (linear interpolation, GCD divisor) | pending |
| 3 | Canonical advance helper (shadow mode) | pending (partly present as Y1 scaffolding, needs reconciliation) |
| 4 | Render coupling (audible) | pending (currently Y2-2 on the branch — needs revisit per new principles) |
| 5 | Capture coupling | pending |
| 6 | Cleanup | pending |
| 7 | Quality polish (post-rewrite) | future |

Do not skip stages. Each gate is mandatory.

## Immediate next step

**Stage 1 — Ring rewrite (INT32, frame-indexed, hard-reject).**

Edit targets:

- `Source/Utilities/loopback.h` — `FRAME_PIPE` struct fields
- `Source/Utilities/loopback.cpp` — ring storage, format normalization, overflow handling

Before starting Stage 1, the implementer should:

1. Re-read `docs/AO_FIXED_PIPE_ARCHITECTURE.md` § 4 (principles), § 6 (data structures), § 10 (SRC), Appendix A (offset map).
2. Run `.\build-verify.ps1 -Config Release` on the current branch to confirm clean baseline.
3. Run `tests/live_call/run_test_call.py` once on AO (current state) and once on VB to refresh the quality reference.

## Key operational rules

These come from CLAUDE.md and remain in effect:

- **Pre-experiment Cable check** (default device must be AO Cable A/B, not VB)
- **Pre-experiment Phone Link connection check** (phone-side toggle ON)
- **Experiment commit rule** (commit each experiment with phase+shorthash; result files in repo)
- **Build process** (`build-verify.ps1`, no `-AutoReboot`)
- **Diagnostics rule** (`ioctl.h` + `adapter.cpp` + `test_stream_monitor.py` updated together)
- **Changelog rule** (every code change logged in `docs/PIPELINE_V2_CHANGELOG.md`)

## Bottom line

**Architecture is frozen. Stage 1 is the next implementation step. Single source of truth is `docs/AO_FIXED_PIPE_ARCHITECTURE.md`.**
