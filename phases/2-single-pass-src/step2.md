# Phase 2 Step 2: Ring read SRC path

## Read First

- Phase 2 Step 1 (write SRC).
- `docs/AO_CABLE_V1_DESIGN.md` § 2.4 (read SRC algorithm).

## Goal

Extend `AoRingReadToScratch` to handle the SRC path
(`dstRate != pipe->InternalRate`). Same-rate fast path stays. Underrun
hysteresis from Phase 1 Step 2 stays unchanged.

## Planned Files

Edit only:

- `Source/Utilities/loopback.cpp` — replace the "if rate-mismatch return
  STATUS_NOT_SUPPORTED" stub in `AoRingReadToScratch` with the SRC
  algorithm.

## Required Edits

Mirror image of Step 1, but operating on the read direction. Per-channel
accumulator + residual carry. Sample read from ring at internal rate is
already INT32 normalized; linear-interpolate to the destination rate
ticks; format-denormalize at write-to-scratch.

Specifically, where Step 1 normalizes input to ring INT32, Step 2
denormalizes ring INT32 to output format **only at output ticks**, not
per source tick.

## Rules

- Same as Step 1.
- Underrun handling stays atop the SRC computation: if available frames
  in ring are insufficient for the requested output frame count after
  ratio conversion, set `UnderrunFlag` and zero-fill output.

## Acceptance Criteria

- [ ] Build clean.
- [ ] Same-rate read scenario still passes Phase 1 Step 2 acceptance.
- [ ] 96 ring → 48 client downsample produces the expected reduced output
      frame count and intelligible audio on a sine input.
- [ ] 48 ring → 96 client upsample produces the expected increased output
      frame count.
- [ ] Forced underrun at SRC rate still triggers hysteretic recovery
      semantics: zero-fill until ring fill ≥ `WrapBound / 2` again.

## What This Step Does NOT Do

- Does not change caller wiring.
- Does not modify the canonical helper (Phase 3).

## Completion

```powershell
python scripts/execute.py mark 2-single-pass-src 2 completed --message "Read SRC path (linear interp + GCD)."
```
