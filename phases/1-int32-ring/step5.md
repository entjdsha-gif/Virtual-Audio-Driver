# Phase 1 Step 5: Diagnostics counter exposure

## Read First

- `docs/AO_CABLE_V1_DESIGN.md` § 7 (diagnostics IOCTL detail).
- `docs/REVIEW_POLICY.md` § 7 (diagnostics coupling rule).
- `Source/Main/ioctl.h` (`AO_V2_DIAG` schema).
- `test_stream_monitor.py`.

## Goal

Expose Phase 1 ring counters (`OverflowCounter`, `UnderrunCounter`,
`UnderrunFlag`, current ring fill in frames) through
`IOCTL_AO_GET_STREAM_STATUS` so user-mode can observe them.

Since the diagnostics rule (REVIEW_POLICY § 7) requires that
`ioctl.h`, `adapter.cpp`, and `test_stream_monitor.py` are updated
together, this is one atomic step.

## Planned Files

Edit only:

- `Source/Main/ioctl.h` — extend `AO_V2_DIAG` with per-cable
  ring counters: `<Cable>_OverflowCount`, `<Cable>_UnderrunCount`,
  `<Cable>_RingFillFrames`, `<Cable>_WrapBoundFrames`. Bump
  `StructSize` and the `C_ASSERT` shape check.
- `Source/Main/adapter.cpp` — extend `IOCTL_AO_GET_STREAM_STATUS`
  handler to fill the new fields from the cable `FRAME_PIPE`s.
- `test_stream_monitor.py` — read the new fields and print per-cable
  ring health.

## Rules

- Tell the user before editing. Three files are edited atomically.
- Do not change existing field offsets in `AO_V2_DIAG` (only append).
- The existing `C_ASSERT` for `AO_V2_DIAG` shape **must** be updated
  to reflect the new size — this is the test that catches schema
  drift.

## Acceptance Criteria

- [ ] Build clean.
- [ ] `test_stream_monitor.py` shows 0/0 for both cables in steady
      state, with `RingFillFrames` oscillating in a sane band around
      `TargetLatencyFrames`.
- [ ] Forced overflow scenario from Step 1 increments
      `<Cable>_OverflowCount` visible in `test_stream_monitor.py`.
- [ ] Forced underrun scenario from Step 4 increments
      `<Cable>_UnderrunCount` visible in `test_stream_monitor.py`.

## What This Step Does NOT Do

- Does not change the canonical helper (Phase 3).
- Does not flip render/capture coupling (Phases 4/5).

## Completion

```powershell
python scripts/execute.py mark 1-int32-ring 5 completed --message "AO_V2_DIAG extended with cable ring counters; test_stream_monitor.py reads them."
```
