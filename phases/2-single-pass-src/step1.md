# Phase 2 Step 1: Ring write SRC path

## Read First

- `docs/ADR.md` ADR-004.
- `docs/AO_CABLE_V1_DESIGN.md` § 2.3 (write SRC algorithm).
- Phase 2 Step 0 (GCD divisor helper).

## Goal

Extend `AoRingWriteFromScratch` to handle the SRC path
(`srcRate != pipe->InternalRate`). Same-rate fast path stays. Algorithm
is single-pass linear interpolation with per-channel weighted accumulator,
using the `(SrcRatio, DstRatio)` from `PickGCDDivisor`.

## Planned Files

Edit only:

- `Source/Utilities/loopback.cpp` — replace the "if rate-mismatch return
  STATUS_NOT_SUPPORTED" stub with the SRC algorithm.

## Required Edits

Inside `AoRingWriteFromScratch` (after the available-space check):

```c
AO_GCD_RATIO ratio;
NTSTATUS st = PickGCDDivisor(srcRate, pipe->InternalRate, &ratio);
if (!NT_SUCCESS(st)) {
    KeReleaseSpinLock(&pipe->Lock, oldIrql);
    return st;  /* STATUS_NOT_SUPPORTED */
}

/* Per-channel accumulator state; lives on stack since SRC is one-shot
 * per call. The persistent SRC state lives on the pipe struct only if
 * the same-call-spans-multiple-frames pattern needs it (it does for
 * proper continuous SRC; allocate as needed). */
LONG accumulator[16] = {0};   /* internal channel slots */
LONG residual[16]    = {0};

/* per-frame interp loop — see vbcable_disasm_analysis.md FUN_1400026a0
 * and vbcable_pipeline_analysis.md § 6.3 for the VB pattern. */
for (ULONG f = 0; f < frames; ++f) {
    /* read input, normalize to 19-bit, weighted-blend into output slot,
     * advance per-channel residual carry, write to ring on each
     * dst-tick, advance WritePos. */
    ...
}
```

The exact accumulator math is implementation-level; specifically, the
residual carry pattern from `vbcable_disasm_analysis.md` § 3.3:

```c
output[ch] = (accumulator[ch] * dst_ratio + residual[ch] * src_ratio)
             / total_ratio;
```

## Rules

- Tell the user before editing.
- Keep the same-rate fast path (no SRC math when ratios are equal).
- Hard-reject overflow logic from Step 1 / Phase 1 stays intact.
- Do not add a sinc fallback. Linear interp only.

## Acceptance Criteria

- [ ] Build clean.
- [ ] Same-rate scenario still passes Phase 1 Step 1 acceptance.
- [ ] 48 → 96 kHz upsample preserves the input frame count proportionally
      and produces a smooth interpolated waveform (manual inspection on
      a sine input).
- [ ] 48 → 44.1 kHz downsample produces the expected reduced frame count
      and no aliasing artifacts beyond what linear interp predicts.
- [ ] Forced-overflow scenario at SRC rate still hard-rejects.
- [ ] No memory allocations on the hot path beyond stack accumulators.

## What This Step Does NOT Do

- Does not yet implement the read SRC path (Step 2).
- Does not change caller wiring.
- Does not change `AO_STREAM_RT`.

## Completion

```powershell
python scripts/execute.py mark 2-single-pass-src 1 completed --message "Write SRC path (linear interp + GCD)."
```
