# Phase 1 Step 6: Diagnostics counter exposure

## Read First

- `docs/AO_CABLE_V1_DESIGN.md` § 7 (diagnostics IOCTL detail).
- `docs/REVIEW_POLICY.md` § 7 (diagnostics coupling rule).
- `Source/Main/ioctl.h` (`AO_V2_DIAG` schema).
- `test_stream_monitor.py`.

## Goal

Expose Phase 1 ring counters and the underrun-recovery flag state
through `IOCTL_AO_GET_STREAM_STATUS` so user-mode can observe them:
`OverflowCounter`, `UnderrunCounter`, `UnderrunFlag` (drained
boolean), current ring fill in frames, and current `WrapBound`.

The flag is exposed because counters alone cannot prove the
50%-`WrapBound` recovery path is operating correctly — a stream that
sits in `UnderrunFlag = 1` for an extended period is broken even if
`UnderrunCount` is small. (Review #4 of 8afa59a.)

Since the diagnostics rule (REVIEW_POLICY § 7) requires that
`ioctl.h`, `adapter.cpp`, and `test_stream_monitor.py` are updated
together, this is one atomic step.

## Planned Files

Edit only:

- `Source/Main/ioctl.h` — extend `AO_V2_DIAG` with per-cable
  ring counters and state: `<Cable>_OverflowCount`,
  `<Cable>_UnderrunCount`, `<Cable>_UnderrunFlag` (UCHAR; 0 = normal,
  1 = drained-recovery), `<Cable>_RingFillFrames`,
  `<Cable>_WrapBoundFrames`. Bump `StructSize` and the `C_ASSERT`
  shape check.
- `Source/Main/adapter.cpp` — extend `IOCTL_AO_GET_STREAM_STATUS`
  handler to fill the new fields from the cable `FRAME_PIPE`s.
- `test_stream_monitor.py` — read the new fields and print per-cable
  ring health, including the underrun flag state.

## Rules

- Tell the user before editing. Three files are edited atomically.
- Do not change existing field offsets in `AO_V2_DIAG` (only append).
- The existing `C_ASSERT` for `AO_V2_DIAG` shape **must** be updated
  to reflect the new size — this is the test that catches schema
  drift.

## Acceptance Criteria

- [ ] Build clean.
- [ ] `test_stream_monitor.py` shows 0/0 for `OverflowCount` /
      `UnderrunCount` on both cables in steady state.
- [ ] `WrapBoundFrames == TargetLatencyFrames`. This acceptance is
      the **equality**, not a specific frame count. The actual value
      is whatever the live `targetFill` policy (set in
      `Source/Main/adapter.cpp` at `FramePipeInit` time) chooses;
      both fields must agree at the live driver. The number is the
      ring **capacity**, not the steady-state fill.

      Phase 1 live evidence (Step 6.2 against the installed driver
      from `phase/1-int32-ring`): `WrapBound=96000` per cable
      (~2 s at 48 kHz, set by `adapter.cpp` `targetFill = 96000`).
      Earlier drafts of this step referenced `7168` as a default;
      that value does not match the current `adapter.cpp` policy
      and was retired here. Future phases that change the policy
      should update this evidence note rather than the equality
      acceptance.
- [ ] `RingFillFrames` is in a small live-latency band — typically
      well below `TargetLatencyFrames` (a few hundred frames at
      48 kHz, depending on writer/reader cadence). Fill ≈ capacity
      means the ring is full and overflow is imminent; fill ≈ 0 with
      `UnderrunFlag = 0` means writer is keeping up with reader. Both
      are healthy. Fill drifting upward toward capacity over time is
      a leak.
- [ ] `UnderrunFlag` is `0` in steady state. It is allowed to flicker
      to `1` at startup or under transient stalls, but must clear
      back to `0` within one fill cycle (≤ `WrapBound / 2` frames of
      writer activity).
- [ ] Forced overflow scenario from Step 2 increments
      `<Cable>_OverflowCount` visible in `test_stream_monitor.py`.
- [ ] Forced underrun scenario from Step 5 increments
      `<Cable>_UnderrunCount` AND sets `<Cable>_UnderrunFlag` to `1`,
      visible in `test_stream_monitor.py`. Flag clears to `0` after
      ring refills past `WrapBound / 2`.

## What This Step Does NOT Do

- Does not change the canonical helper (Phase 3).
- Does not flip render/capture coupling (Phases 4/5).

## Completion

```powershell
python scripts/execute.py mark 1-int32-ring 6 completed --message "AO_V2_DIAG extended with cable ring counters; test_stream_monitor.py reads them."
```
