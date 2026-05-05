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

/* Compute output ring frames first; capacity check uses the OUTPUT
 * count, not the input count (44.1k → 48k expansion writes more
 * frames than it consumes — see #6 from review). */
ULONG ringFramesToWrite = (ULONG)(((ULONGLONG)frames * ratio.DstRatio)
                                  / ratio.SrcRatio);
LONG  available = AoRingAvailableSpaceFrames_Locked(pipe);
if ((LONG)ringFramesToWrite > available) {
    pipe->OverflowCounter++;
    KeReleaseSpinLock(&pipe->Lock, oldIrql);
    return STATUS_INSUFFICIENT_RESOURCES;
}

/* Per-channel accumulator state lives on FRAME_PIPE
 * (pipe->WriteSrcPhase + pipe->WriteSrcResidual[]) and persists
 * across calls so the linear-interp phase does not reset at every
 * chunk boundary. VB parity: vbcable_capture_contract_answers.md § 0
 * (+0xB8 phase, +0xBC.. residual) and FUN_1400017ac:588/1013. */

/* per-frame interp loop — see vbcable_pipeline_analysis.md § 6.3 and
 * the FUN_1400026a0 / FUN_1400017ac decompile in
 * results/ghidra_decompile/vbcable_all_functions.c for the VB pattern. */
for (ULONG f = 0; f < frames; ++f) {
    /* read input, normalize to 19-bit per § 2.5 of DESIGN,
     * weighted-blend into output slot using pipe->WriteSrcResidual[ch]
     * as carry, advance per-channel residual, advance pipe->WriteSrcPhase,
     * write to ring on each dst-tick, advance WritePos. */
    ...
}
```

The exact accumulator math is implementation-level; specifically, the
residual carry pattern from VB:

```c
output[ch] = (input_sample * ratio.DstRatio +
              pipe->WriteSrcResidual[ch] * ratio.SrcRatio)
             / (ratio.SrcRatio + ratio.DstRatio);
pipe->WriteSrcResidual[ch] = output[ch];   /* carry into next frame */
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
