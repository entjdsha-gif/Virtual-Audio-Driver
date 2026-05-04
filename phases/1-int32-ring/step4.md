# Phase 1 Step 4: Underrun hysteresis end-to-end test

## Read First

- `docs/ADR.md` ADR-005 (hysteretic underrun recovery, 50% threshold).
- Phase 1 Steps 1, 2 (read/write same-rate paths).

## Goal

End-to-end smoke test of the underrun hysteresis path under realistic
producer/consumer rates. Drive a fresh `FRAME_PIPE` from a controlled
producer/consumer and confirm:

1. Steady-state with producer rate ≥ consumer rate: no underrun.
2. Producer underruns once: consumer goes silent until ring fill
   recovers to ≥ `WrapBound / 2`.
3. After recovery, consumer delivers real data again.
4. `UnderrunCounter` increments by exactly 1 per underrun event.

## Planned Files

Edit only:

- Add `tests/phase1-runtime/underrun_hysteresis_test.py` (untracked
  per `docs/GIT_POLICY.md` § 8) — Python harness using `ctypes` to
  drive a thin user-mode helper that exposes `AoRingWriteFromScratch` /
  `AoRingReadToScratch` for testing. The helper itself remains
  internal to the test; do not promote unless explicitly approved.

If a user-mode test harness for the ring does not exist yet, this step
documents that as a blocker and proposes the minimum helper surface
needed.

## Rules

- Do not modify `Source/Utilities/loopback.cpp` in this step (it is
  already correct from Step 2). If the test exposes a bug, capture it
  as a Step 2 retroactive finding and fix in a Step 2 fix-up commit.
- Do not promote test artifacts to tracked locations.

## Acceptance Criteria

- [ ] Test runs deterministically and prints PASS / FAIL.
- [ ] PASS condition: above 4 invariants all hold.
- [ ] FAIL leaves a captured trace in `tests/phase1-runtime/` for
      diagnosis.

## What This Step Does NOT Do

- Does not test SRC.
- Does not test the canonical helper.
- Does not flip cable transport ownership.

## Completion

```powershell
python scripts/execute.py mark 1-int32-ring 4 completed --message "Underrun hysteresis end-to-end smoke PASS."
```
