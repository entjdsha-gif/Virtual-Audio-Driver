# G4 B1 — Render Prefill Proposal (Diagnostic Gate)

**Branch:** `feature/ao-fixed-pipe-rewrite` @ `2c733f1` (Phase 5 CLOSED, G3 ✅)
**Date:** 2026-04-14
**Status:** PROPOSAL — not yet implemented, not yet committed
**Phase 6:** still BLOCKED until this gate resolves

---

## § 1. Framing — B1 is a diagnostic gate, not a fix attempt

Per G3 §12 resolution, the leading hypothesis is:

> pump-owned render path fires at client query cadence (~7–8 Hz under this workload),
> leaving near-zero fill headroom against a 1 ms reader drain; strongly supported, structural

Numeric reality check for B1's viability as a standalone fix:

- Pump inter-fire interval ≈ **130 ms** (order-of-magnitude from G3 §12)
- Reader drain = **48 frames/ms** → between two pump fires, ~**6240 frames** leave the pipe
- `FP_DEFAULT_TARGET_FILL = 3584` frames ([loopback.h:204](../Source/Utilities/loopback.h#L204))
- Candidate prefill `max(TargetFillFrames / 2, SampleRate / 8)`
  at 48 kHz default: `max(1792, 6000) = 6000` frames ≈ **125 ms of headroom**

**6000 frames ≈ one pump inter-fire interval.** One-shot prefill covers roughly one
cycle. After that, with no change to pump cadence, steady-state trough could still
re-converge toward the original near-zero level — the prefill is a **one-time cushion**,
not a standing cushion. Whether steady-state trough recovers or not is exactly the
signal B1 is designed to reveal.

Therefore B1 is **not positioned as a fix**. B1 is positioned as a **decision gate**:

| Observed result after B1 | Interpretation | Next branch |
|---|---|---|
| Trough Fill still approaches 0, dUnderrun ≥ 1 | Prefill alone insufficient; writer/reader period mismatch dominates as predicted | **B2 mandatory** — raise `FP_DEFAULT_TARGET_FILL` above `pump_interval × reader_rate` |
| Trough Fill stays elevated (> ~2000 frames), dUnderrun = 0 over 30+ s sine | Pump fire rate is faster/steadier than G3 §12 estimated, OR our rate estimate was wrong | Revisit G3 assumptions; B2 may be deferred |
| dUnderrun = 0 but trough oscillates near the prefill level | Prefill cushion holds because pump catches up within one reader window; unexpected | Capture per-fire delta with finer instrumentation before committing to B2 |

B1's job is to turn the G3 §12 order-of-magnitude estimate into a **quantitative yes/no
signal** on whether steady-state fill headroom is recoverable without raising
`TargetFillFrames`.

---

## § 2. Injection point — three candidates, one recommended

| Candidate | Where | Pros | Cons |
|---|---|---|---|
| A. `FramePipeInit` | [loopback.cpp:1256](../Source/Utilities/loopback.cpp#L1256) | Runs once per driver load; simplest | Prefill stays from boot to first format register; by the time a client runs, the pipe has been idle and the prefill is meaningless |
| B. `FramePipeRegisterFormat(isSpeaker=TRUE)` | [loopback.cpp:1727](../Source/Utilities/loopback.cpp#L1727) | Fires on every Speaker RUN (through `SetState`); aligns with the exact moment the cable speaker becomes active | Ring persists across STOP/RUN ([loopback.cpp:1755-1756](../Source/Utilities/loopback.cpp#L1755-L1756)); must guard against stacking prefill on existing data |
| **C. `minwavertstream.cpp SetState(KSSTATE_RUN)` cable-speaker branch** | [minwavertstream.cpp:1507-1538](../Source/Main/minwavertstream.cpp#L1507-L1538) | Fires on exactly the RUN transition for cable speakers; sees `m_pMiniport->m_DeviceType` directly; can sit immediately after `FramePipeRegisterFormat` | Caller must hold the correct lock discipline; new code lives in stream file instead of pipe file |

**Recommended: C** — call a new helper `FramePipePrefillIfEmpty(pFP, frames)` from
`SetState(KSSTATE_RUN)` right after the existing `FramePipeRegisterFormat` call at
[minwavertstream.cpp:1531-1536](../Source/Main/minwavertstream.cpp#L1531-L1536), and
only for the cable speaker direction (`!m_bCapture && isCableSpeakerDeviceType`).

Rationale:
- RUN is the only moment where a fresh cable speaker stream actually begins producing
  frames, which is when a cushion matters.
- Phase 5 pump ownership arms here too ([minwavertstream.cpp:1577-](../Source/Main/minwavertstream.cpp#L1577)),
  so the prefill gets exercised by the exact transport we're diagnosing.
- The helper lives in `loopback.cpp` and owns the pipe lock internally; the call site
  only needs to pass the pipe pointer, matching the pattern already used by
  `FramePipeRegisterFormat`.

---

## § 3. Prefill amount

**Formula:** `prefillFrames = max(pPipe->TargetFillFrames / 2, pPipe->PipeSampleRate / 8)`

At 48 kHz default (`TargetFillFrames = 3584`): `max(1792, 6000) = 6000` frames
≈ 125 ms of silence cushion.

Rationale:
- The fixed `TargetFillFrames / 2` divisor (1792 frames ≈ 37 ms) is **smaller than one
  observed pump inter-fire interval (~130 ms)** and would be drained before the first
  real pump fire lands. B1 would then produce a near-zero result that doesn't
  distinguish "prefill too small" from "structural period mismatch".
- `SampleRate / 8` (= 6000 frames at 48 kHz = 125 ms) is sized to cover approximately
  one pump inter-fire interval per G3 §12 order-of-magnitude. This is the minimum
  cushion that makes B1's result meaningful.
- `max(...)` preserves the proportional behavior when a future B2 decision raises
  `TargetFillFrames` — once `TargetFillFrames / 2 > SampleRate / 8`, the formula
  tracks the target fill instead of the fixed ~125 ms floor.
- Bounded above by `CapacityFrames - 1` as a safety clamp. `CapacityFrames =
  TargetFillFrames * 2 = 7168` frames at default, so `6000 < 7168` leaves headroom
  for the first pump fire without triggering `DropCount++`.
- Frame count is block-aligned by construction (integer frames, written in whole-frame
  units through the same `PipeChannels * sizeof(INT32)` stride the write path uses).

**Not used:**
- Fixed `TargetFillFrames / 2` — too small (see above).
- Full `TargetFillFrames` — leaves only `TargetFillFrames` headroom before
  `DropCount++`; marginal against the first pump fire.
- Fixed constant (e.g. 6000 literal) — decouples from sample-rate scaling; at a
  hypothetical 96 kHz session the cushion would be half as long in wall-clock terms.

---

## § 4. Prefill data — zero

`RtlZeroMemory` the prefill region. INT32 zeros are valid silence in the pipe's native
format, which means:
- No audible artifact when the reader drains them before any real write arrives
- No format-conversion code needed
- Matches the existing `RtlZeroMemory(m_pDmaBuffer, ...)` at
  [minwavertstream.cpp:1544](../Source/Main/minwavertstream.cpp#L1544) in spirit

---

## § 5. Guard — `FillFrames == 0` (empty ring), not `StartPhaseComplete`

**Critical correction.** `StartPhaseComplete` is set to `TRUE` unconditionally at
[loopback.cpp:1342](../Source/Utilities/loopback.cpp#L1342) and therefore cannot be used
as an "is this the first RUN" flag.

Valid guard is **`pPipe->FillFrames == 0`** under `PipeLock`. Semantics:
- First RUN after `FramePipeInit` → ring empty → prefill fires.
- RUN after Speaker STOP that left data in the ring → ring non-empty → prefill skipped,
  persistent ring data honored as the existing `FramePipeRegisterFormat` comment demands.
- RUN after Speaker STOP that drained the ring completely → ring empty → prefill fires
  again. This is arguably desirable (equal starting conditions) but also a behavior
  change; acceptable for B1 because it's diagnostic-only, uncommitted.

Lock discipline: acquire `PipeLock`, check `FillFrames`, write prefill bytes into
`RingBuffer[WriteFrame..]`, advance `WriteFrame`, bump `FillFrames`, release.
This is the same pattern the existing write path uses.

---

## § 6. Physical separation from G2 instrumentation

The current working tree has G2 1-second-window DbgPrint instrumentation in
`loopback.{h,cpp}`, uncommitted. B1's new helper must be:

- Added as a **new function block** `FramePipePrefillIfEmpty` placed *after*
  `FramePipeRegisterFormat` in `loopback.cpp`, with its own `#pragma code_seg("PAGE")`
  guard and its own header declaration block.
- Kept separate from the G2 DbgPrint stanzas so that a future `git restore` of G2
  does not touch B1 code and vice versa.
- Declared in `loopback.h` in a dedicated **"G4 B1 prefill"** comment section, not
  mixed into the existing Phase 1 / Phase 3 / Phase 5 sections.

Commit posture for B1:
- G2 instrumentation stays uncommitted (unchanged rule from memory).
- B1 code also stays uncommitted until the measurement outcome in §8 is confirmed.
- If B1 proves it justifies B2, commit B1 + B2 together as a single Phase-6-gating
  change. If B1 proves nothing is needed, `git restore` both.

---

## § 7. Build and install

Standard path from memory:

```powershell
# Build
msbuild VirtualAudioDriver.sln /p:Configuration=Release /p:Platform=x64

# Verify
.\build-verify.ps1 -Config Release

# Install (no reboot)
.\install.ps1 -Action upgrade
```

No `-AutoReboot`. If TESTSIGNING re-arms, accept the reboot per
`feedback_test_signing_reboot` guidance.

---

## § 8. Measurement protocol

Reuse existing G2/G3 tooling. **No new scripts.**

1. Start DebugView with file logging:
   `.\DebugView64.exe /k /g /l runtime_logs\g4_b1_sine.log`
2. Run the sine feeder on Cable A first, then Cable B:
   `python tests\g2_sine_48_16_2.py` (full 15 s run each)
3. Immediately after each run, snapshot the stream monitor:
   `python test_stream_monitor.py --once`
4. Optional: during the sine run, take a mid-run snapshot to catch
   `PumpDrv` / `LegacyDrv` deltas inside the active window.

Parse from the DebugView log for each 1-second window:
- `WrF/s`, `RdF/s` — confirm reader still ≈ 48000
- `Fill` — **trough over the full 15 s run**, not just the last sample
- `dUnderrun` — per-window delta, target = 0 across all windows
- `dDrop` — must remain 0 (prefill should not tip writes over capacity)

From the `--once` snapshot:
- `Flags` must still be `0x00000007` (Phase 5 pump-owned)
- `PumpDrv` delta > 0 across the run window
- `LegacyDrv` delta = 0 (ownership unchanged)

Then run the live-call test per
[tests/live_call/run_test_call.py](../tests/live_call/run_test_call.py) and record
the user-reported audio quality label (clean / garbled / silent) for both cables.
Remember the live-call PASS trap from memory — lab PASS is not the same as real
Phone Link audio quality.

---

## § 9. Success / escalation table

| Signal | Result | Next move |
|---|---|---|
| Cable A trough Fill > ~2000, Cable B trough Fill > ~2000, dUnderrun = 0 over full 15 s, live call clean on both | Unexpected win — pump catches up faster than G3 §12 estimated | Write a short explanation note, consider committing B1, **do not enter Phase 6 yet** — still need to explain the gap with the §12 rate estimate |
| Trough Fill collapses to < 100 on either cable, dUnderrun ≥ 1 | Expected per §1 table | **Advance to B2**: raise `FP_DEFAULT_TARGET_FILL` to cover at least 2× worst observed pump interval (~300 ms × 48 fps/ms = 14400 frames, round to 16384) and re-measure |
| dDrop ≥ 1 | Prefill pushes writer over capacity — pump fire arrives before reader drains the prefill | Reduce the `SampleRate / 8` floor to `SampleRate / 16` (3000 frames @ 48 kHz) and re-run once; if still dropping, abandon B1 and jump to B2 directly |
| `LegacyDrv` delta > 0 during the run | Phase 5 ownership regressed — not a B1 problem | Stop B1 work, open a Phase 5 regression note, do not proceed |
| `Flags` ≠ `0x00000007` during the run | Same as above — ownership regression | Same handling |

---

## § 10. Scope guardrails

**In scope for B1:**
- One new helper function in `loopback.cpp`
- One declaration in `loopback.h`
- One call site in `minwavertstream.cpp SetState(KSSTATE_RUN)` cable-speaker branch
- Changelog entry in `docs/PIPELINE_V2_CHANGELOG.md`
- Measurement run + results appended to this file as § 11

**Out of scope for B1:**
- Any change to `FP_DEFAULT_TARGET_FILL` — that belongs to B2 and must not be mixed in
- Any change to pump cadence, `PumpToCurrentPositionFromQuery`, or `GetPositions`
- Any change to capture-side logic
- Any new IOCTL, new diagnostic counter, or new DbgPrint (reuse G2 instrumentation as-is)
- `git restore` of the G2 instrumentation — still deferred per current operating rules

**Non-negotiables:**
- Expression of results stays at "strongly supported, structural" — no "confirmed".
- Phase 6 stays BLOCKED regardless of B1 outcome.
- Nothing in B1 gets committed until B2 decision is made.

---

## § 11. Measurement results

*To be filled in after the measurement run. Leave empty until then.*

### § 11.1 Cable A sine 15 s

*(pending)*

### § 11.2 Cable B sine 15 s

*(pending)*

### § 11.3 `--once` snapshots

*(pending)*

### § 11.4 Live call subjective report

*(pending)*

### § 11.5 Decision

*(pending — apply § 9 table)*
