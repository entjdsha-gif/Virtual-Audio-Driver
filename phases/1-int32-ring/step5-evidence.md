# Phase 1 Step 5 — Underrun Hysteresis End-to-End Evidence

Status: complete
Date: 2026-05-08
Branch: `phase/1-int32-ring`
Auditor: Claude (execution agent)
Review: pending Codex

## 1. Scope

Per [step5.md](step5.md), drive a fresh `FRAME_PIPE` from a controlled
producer/consumer cadence and confirm the four ADR-005 invariants:

1. Steady-state with producer rate ≥ consumer rate: no underrun.
2. Producer underruns once: consumer goes silent until ring fill
   recovers to ≥ `WrapBound / 2`.
3. After recovery, consumer delivers real data again.
4. `UnderrunCounter` increments by **exactly 1 per underrun event**
   (not per silent recovering read).

Sources of truth:

- [docs/ADR.md § ADR-005](../../docs/ADR.md) — hard-reject overflow +
  hysteretic underrun recovery, 50%-of-`WrapBound` recovery threshold.
- [docs/AO_CABLE_V1_DESIGN.md § 2.4](../../docs/AO_CABLE_V1_DESIGN.md) —
  read-direction pseudocode showing hysteresis gate.

Out of scope: SRC, canonical-helper migration, audible flip — all are
later-phase work and explicitly excluded by [step5.md § "What This Step
Does NOT Do"](step5.md).

## 2. Harness Choice

step5.md anticipated a `ctypes`-on-DLL pattern that would call into the
real kernel C `AoRingWriteFromScratch` / `AoRingReadToScratch`. The
realized harness is the existing **Python equivalent-logic** pattern
established at Step 2 / Step 3 (`tests/phase1-runtime/ring_helpers.py`).

Rationale:

- `AoRingWriteFromScratch` / `AoRingReadToScratch` rely on kernel-only
  surface (`ExAllocatePool2`, `KeAcquireSpinLock`, `KIRQL`,
  `RtlZeroMemory`, `RtlFillMemory`, `STATUS_*`). Building a user-mode
  DLL requires a kernel-API shim layer that is itself substantial work
  — far beyond Step 5 scope.
- The Python re-implementation is **byte-equivalent** to the C path
  for every input the C function will see. Equivalence is established
  by:
  - `ring_round_trip_test.py` RT1..RT5 — Step 2 write + Step 3 read
    paired round-trips (8/16-bit bit-exact, 24/32-bit top-19
    preserved, defensive clamp boundary).
  - `ring_write_test.py` 6/6 — Step 2 write semantics (overflow,
    rate-mismatch, invalid-param, sign-extension).
  - `ring_read_test.py` 8/8 — Step 3 read semantics
    (R4/R5/R6 unit-level coverage of the same hysteresis branches
    Step 5 exercises end-to-end).
- The harness choice was approved by the user prior to Step 5
  implementation.

Trade-off: Step 5 is **behavioral** evidence, not kernel-binding
evidence. A future ctypes-on-DLL harness (or a separately-built
Windows test driver with an IOCTL surface) would close that gap; it
remains documented residual work (see § 6).

## 3. Test Scenarios

`tests/phase1-runtime/underrun_hysteresis_test.py` (untracked per
[GIT_POLICY.md § 9](../../docs/GIT_POLICY.md)) drives a `FRAME_PIPE`
configured as 48 kHz / 16-bit / stereo, `WrapBound = 7168` (V1 default),
`PKT_FRAMES = 480` (10 ms WaveRT-style packet). Recovery threshold =
`WrapBound / 2 = 3584`.

Cadence is "tick / frame batch", not wall-clock — reproducible across
machines and CI loads.

### 3.1 H1 — Steady-state, producer == consumer rate, no underrun

Pre-fill 4000 frames (above threshold), then 100 1:1 cycles of
`(write 480, read 480)`. Each write packet uses
`make_distinguishable_packet(seed=tick)` so silence vs real-data
detection is unambiguous.

Asserts: `UnderrunCounter == 0`, `UnderrunFlag == 0`,
`OverflowCounter == 0`, every read is non-silence.

→ Invariant #1.

### 3.2 H2 — Single underrun lifecycle

Five-phase end-to-end transition through the entire ADR-005 state
machine:

| Phase | Action | Expected state at end of phase |
|---|---|---|
| A — drain | pre-fill 1000, request 2000-frame read | counter 0→1, flag 0→1, ReadPos unchanged, scratch silence |
| B — silent recovery | producer paused; 5 consecutive 480-frame reads while available (1000) < threshold (3584) | counter stays at 1, flag stays at 1, every read silence, ReadPos unchanged |
| C — refill | producer writes 4000 frames in one shot | fill ≥ 3584, flag still 1 (only a read can clear it) |
| D — exit read | next 480-frame read | flag 1→0, ReadPos += 480, scratch is real data, counter still 1 |
| E — steady cycles | 10 normal `(write 480, read 480)` cycles | counter stays at 1, no further events |

→ Invariants #2, #3, #4 (single-event variant).

### 3.3 H3 — Multi-event count accuracy

Three discrete drain → recover → exit cycles back-to-back, with a
drain-down loop between events to return the pipe to a near-empty
state where the next underrun can be triggered cleanly. Each event:

1. Pre-fill 800 frames (writer).
2. Request 1500-frame read → underrun.
3. Refill 4000 frames.
4. Single 480-frame exit read.
5. Drain-down to ≤ `PKT_FRAMES` available so the next event starts
   in the same regime.

After 3 events: `UnderrunCounter == 3` (hard equality, not ≥),
`OverflowCounter == 0`. Per-event counter delta is asserted as exactly
+1.

→ Invariant #4 strong form: counter is **per-event**, not
**per-silent-read**.

## 4. Run Results

Run on 2026-05-08 from `phase/1-int32-ring` HEAD (Step 4 mark commit
`d9c7113`):

```
============================================================
 Phase 1 Step 5 -- end-to-end underrun hysteresis smoke
============================================================
 WRAP_BOUND=7168, THRESHOLD=3584, PKT=480 frames, 48000 Hz / 16-bit / 2ch
 NOTE: equivalent-logic harness (Python re-impl), not a kernel call test

  [PASS] [H1] steady-state  100 1:1 cycles, UnderrunCounter=0, OverflowCounter=0
  [PASS] [H2] single underrun lifecycle  drain→silence→refill→exit→steady; UnderrunCounter=1, flag=0
  [PASS] [H3] multi-event count  3 discrete events → UnderrunCounter=3

============================================================
 RESULT: 3 PASS / 0 FAIL
============================================================
```

No trace dump occurred (PASS path skips the dump per design).

### 4.1 Regression — Step 2/3 tests still pass

| Test | Result |
|---|---|
| `ring_write_test.py` | 6 PASS / 0 FAIL |
| `ring_read_test.py` | 8 PASS / 0 FAIL |
| `ring_round_trip_test.py` | 5 PASS / 0 FAIL |

Combined Phase 1 user-mode harness: **22 PASS / 0 FAIL**.

### 4.2 Build verification

`.\build-verify.ps1 -Config Release` → **17 PASS / 0 FAIL** (no-change
baseline; Step 5 introduces zero source-code edits).

### 4.3 Working tree

```
 M build-manifest.json                       # build artifact, not committed
?? tests/phase1-runtime/                     # all test files untracked (§ 9)
```

`git diff --check` clean.

## 5. Acceptance Criteria Trace

step5.md acceptance:

| # | Criterion | Status | Evidence |
|---|---|---|---|
| 1 | Test runs deterministically and prints PASS / FAIL | PASS | § 4 transcript; tick/frame-batch cadence (no wall-clock dependency) means re-runs produce identical traces. |
| 2 | PASS condition: above 4 invariants all hold | PASS | H1 → invariant #1. H2 phases A/B/C/D/E → invariants #2, #3, #4 (single-event). H3 → invariant #4 (multi-event hard equality). |
| 3 | FAIL leaves a captured trace in `tests/phase1-runtime/` for diagnosis | INFRA in place | `Trace` class in `underrun_hysteresis_test.py` accumulates per-tick `(tick, phase, event, write_pos, read_pos, fill, flag, overflow, underrun)` rows; `dump_on_fail` writes `underrun_hysteresis_trace_<UTC>.txt` only on FAIL. PASS run produces no trace file (§ 4 confirms). |

## 6. Residual Risks

### 6.1 RR-1 — Behavioral coverage, not kernel-binding

The harness validates algorithmic equivalence, not the actual kernel
binary's runtime behavior. A future ctypes-on-DLL or Windows test
driver harness would prove the C compiler emits the same control flow
the Python re-implementation models. This gap is the same one
established and acknowledged at Step 2 / Step 3; it is not specific to
Step 5.

Mitigation: cross-checked round-trip equivalence (RT1..RT5) +
`build-verify.ps1` 17/17 (the C code compiles and links into
`aocablea.sys` / `aocableb.sys`).

### 6.2 RR-2 — `step5.md` GIT_POLICY section reference

[step5.md line 25](step5.md) cites `docs/GIT_POLICY.md § 8` for the
untracked rule. The current GIT_POLICY structure puts runtime artifact
policy in **§ 9** ("Runtime artifacts under `tests/`"); § 8 covers
"Local WDK / signing workarounds". This evidence and the
`underrun_hysteresis_test.py` header use § 9. Recommend a follow-up
`docs/<topic>` patch updates step5.md's section reference; not a Step 5
BLOCKER.

## 7. Files Touched by Step 5

| Path | Tracked? | Change |
|---|---|---|
| `Source/Utilities/loopback.cpp` | tracked | **none** (per step5.md rule § "Rules") |
| `tests/phase1-runtime/underrun_hysteresis_test.py` | untracked (§ 9) | **new** (this is the test) |
| `tests/phase1-runtime/README.md` | untracked (§ 9) | new section, expected combined total updated |
| `phases/1-int32-ring/step5-evidence.md` | tracked | **new** (this document) |
| `phases/1-int32-ring/index.json` | tracked | mark Step 5 completed (separate commit) |

Source-code edits: 0.

## 8. Auditor Self-Assessment

The four ADR-005 invariants hold under the Python equivalent-logic
harness across H1/H2/H3 scenarios. Step 5 PASS, no MINOR. Two recorded
RESIDUAL RISKs that belong to follow-up work (RR-1 future kernel-binding
harness; RR-2 step5.md doc patch).

Awaiting Codex review.
