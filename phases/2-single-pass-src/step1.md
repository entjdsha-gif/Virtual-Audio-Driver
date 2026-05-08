# Phase 2 Step 1: Ring write SRC path

## Read First

- `docs/ADR.md` ADR-004 (single-pass linear-interp SRC, GCD divisor
  first-match), ADR-005 (hard-reject overflow + monotone counters),
  ADR-008 (KSDATARANGE / PCM-only).
- `docs/AO_CABLE_V1_DESIGN.md` § 2.3 (write SRC algorithm) and § 2.5
  (bit-depth dispatch / 19-bit headroom).
- `results/vbcable_pipeline_analysis.md` § 3.4 (VB `FUN_1400026a0`
  different-rate write — capacity check uses output ring frames; VB
  uses `floor(frames * DstRatio / SrcRatio) + 1` safety. AO Cable V1
  deliberately diverges to a phase-aware EXACT capacity check; see
  Required Edits below for rationale).
- Phase 2 Step 0 (`PickGCDDivisor` and `AO_GCD_RATIO`) in
  `Source/Utilities/loopback.cpp`.

## Goal

Extend `AoRingWriteFromScratch` to handle the SRC path
(`srcRate != pipe->InternalRate`). The same-rate fast path stays
**byte-identical** to Phase 1 Step 2 — the diff for that branch is
zero. Algorithm is single-pass linear interpolation with input-driven
loop and per-channel weighted accumulator, using the
`(SrcRatio, DstRatio)` from `PickGCDDivisor` plus
`pipe->WriteSrcPhase` / `pipe->WriteSrcResidual[]` carried across
calls.

## Planned Files

Edit only:

- `Source/Utilities/loopback.cpp`:
  - Replace the same-rate-only rate-mismatch stub with the SRC branch.
  - Remove the Step 0 `#pragma warning(push) / disable: 4505 / pop`
    block around `PickGCDDivisor`. The helper has a real caller now,
    so the C4505 suppression is no longer needed (and leaving it
    masks future genuine-unused regressions).

`loopback.h` is **not** edited. `AO_STREAM_RT` is not edited. No
other translation unit is touched.

## Required Edits

### Function structure (branch-first, per-branch capacity check)

The SRC branch is added BEFORE the same-rate body and returns early.
The same-rate code (lines 1644-1677 of current `loopback.cpp`) stays
byte-identical.

```c
KeAcquireSpinLock(&pipe->Lock, &oldIrql);

// SRC branch -- rate mismatch.
if (srcRate != pipe->InternalRate)
{
    AO_GCD_RATIO ratio;
    NTSTATUS gcdSt = PickGCDDivisor(srcRate, pipe->InternalRate, &ratio);
    if (!NT_SUCCESS(gcdSt)) {
        KeReleaseSpinLock(&pipe->Lock, oldIrql);
        return gcdSt;   // STATUS_NOT_SUPPORTED or STATUS_INVALID_PARAMETER
    }

    // Phase-aware EXACT output frame count for this call.
    //
    // VB-Cable's evidence (results/vbcable_pipeline_analysis.md § 3.4
    // line 193) uses a `floor(frames * DstRatio / SrcRatio) + 1`
    // safety idiom. AO Cable V1 deliberately diverges to a phase-
    // aware EXACT form: WriteSrcPhase carries the precise common-
    // tick offset since the last emit, so the actual emit count for
    // this call is computable without slack:
    //
    //   outputFrames = floor((WriteSrcPhase + frames * DstRatio) / SrcRatio)
    //
    // Both forms satisfy ADR-005 hard-reject; the exact form keeps
    // OverflowCounter free of false positives in cases where the
    // emit count would have fit. WriteSrcPhase is a single LONG
    // already in the cache line we touched on Lock acquire, so the
    // accuracy gain has zero observable cost.
    ULONGLONG totalDst = (ULONGLONG)(ULONG)pipe->WriteSrcPhase
                       + (ULONGLONG)frames * (ULONGLONG)ratio.DstRatio;
    ULONG outputFrames = (ULONG)(totalDst / (ULONGLONG)ratio.SrcRatio);

    LONG writable = AoRingAvailableSpaceFrames_Locked(pipe);
    if ((LONG)outputFrames > writable) {
        // ADR-005 hard-reject. WritePos / WriteSrcPhase /
        // WriteSrcResidual[] all unchanged so the caller may retry
        // after the consumer drains the ring.
        pipe->OverflowCounter++;
        KeReleaseSpinLock(&pipe->Lock, oldIrql);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Channel mapping bound by the residual array size. The per-
    // channel residual is the only state that survives the call, so
    // we MUST NOT index past ARRAYSIZE(pipe->WriteSrcResidual).
    LONG ringChannels = pipe->Channels;
    LONG copyChannels = (LONG)srcChannels;
    if (copyChannels > ringChannels) copyChannels = ringChannels;
    if ((SIZE_T)copyChannels > ARRAYSIZE(pipe->WriteSrcResidual))
        copyChannels = (LONG)ARRAYSIZE(pipe->WriteSrcResidual);

    // SRC loop -- input-driven (see "SRC loop algorithm" below).
    // Updates WritePos, WriteSrcPhase, WriteSrcResidual[] in place.
    // outputFrames == 0 (small downsample chunk) is a valid SUCCESS
    // outcome: no ring slot is written, but phase + residuals still
    // advance so the next call continues the interpolation.
    /* ... see SRC loop algorithm below ... */

    KeReleaseSpinLock(&pipe->Lock, oldIrql);
    return STATUS_SUCCESS;
}

// Same-rate fast path -- Phase 1 Step 2 logic, BYTE-IDENTICAL.
LONG writable = AoRingAvailableSpaceFrames_Locked(pipe);
if ((LONG)frames > writable) { /* hard-reject as before */ }
/* ... existing per-frame normalize loop ... */
```

### SRC loop algorithm (inline, per ADR-004)

Time model: with `PickGCDDivisor(srcRate, InternalRate)`, one input
frame spans `DstRatio` common ticks and one output (ring) frame spans
`SrcRatio` common ticks. `WriteSrcPhase` is in `[0, SrcRatio)` and
represents the common-tick distance since the last emit, carried
across calls.

```c
LONG accum = pipe->WriteSrcPhase;

for (ULONG f = 0; f < frames; ++f) {
    LONG curr[FP_MAX_CHANNELS];
    for (LONG ch = 0; ch < copyChannels; ++ch) {
        curr[ch] = NormalizeToInt19(scratch, srcBits, f,
                                    (ULONG)ch, srcChannels);
    }

    accum += (LONG)ratio.DstRatio;

    while (accum >= (LONG)ratio.SrcRatio) {
        accum -= (LONG)ratio.SrcRatio;
        // After the subtract, `accum` is in [0, SrcRatio); the emit
        // point sits at fractional position
        // (DstRatio - accum)/DstRatio between prev and curr.
        //
        //   sample = (prev*accum + curr*(DstRatio - accum)) / DstRatio
        //
        // Range check (DESIGN § 2.5 19-bit headroom preserved):
        //   prev, curr in [-2^18, 2^18]
        //   accum, (DstRatio - accum) in [0, DstRatio-1]; DstRatio <= 2560
        //   single product <= 2^18 * 2560 ~= 6.7e8 (fits LONG)
        //   sum of two products ~= 1.3e9 (still fits LONG, but
        //   LONGLONG intermediate matches DenormalizeFromInt19's
        //   defensive style and removes any future doubt).
        LONG slotBase = pipe->WritePos * ringChannels;
        for (LONG ch = 0; ch < copyChannels; ++ch) {
            LONGLONG mixed =
                ((LONGLONG)pipe->WriteSrcResidual[ch] * (LONGLONG)accum)
              + ((LONGLONG)curr[ch] *
                 (LONGLONG)((LONG)ratio.DstRatio - accum));
            pipe->Data[slotBase + ch] =
                (LONG)(mixed / (LONGLONG)ratio.DstRatio);
        }
        // Ring slots beyond copyChannels keep their prior contents
        // (Phase 1 Step 2 channel policy).

        pipe->WritePos++;
        if (pipe->WritePos >= pipe->WrapBound) pipe->WritePos = 0;
    }

    for (LONG ch = 0; ch < copyChannels; ++ch) {
        pipe->WriteSrcResidual[ch] = curr[ch];
    }
}

pipe->WriteSrcPhase = accum;
```

### State persistence contract

| Path | `WritePos` | `WriteSrcPhase` | `WriteSrcResidual[]` | `OverflowCounter` |
|---|---|---|---|---|
| SRC success (any `outputFrames`, including 0) | advanced by `outputFrames` (modulo `WrapBound`) | updated to post-loop `accum` | updated for `ch < copyChannels` | unchanged |
| SRC hard-reject (`outputFrames > writable`) | **unchanged** | **unchanged** | **unchanged** | `++` |
| `PickGCDDivisor` failure | unchanged | unchanged | unchanged | unchanged |
| Same-rate path | (unchanged from Phase 1 Step 2) | n/a (not used in same-rate) | n/a | per existing Phase 1 contract |

Channel bound:

```text
copyChannels = min(srcChannels, pipe->Channels, ARRAYSIZE(pipe->WriteSrcResidual))
```

Indexing past `WriteSrcResidual[]` is forbidden -- the residual array
is the only state that survives the call boundary, and a write past
its end would corrupt adjacent FRAME_PIPE fields.

## Rules

- Tell the user before editing (CLAUDE.md Edit Protocol).
- Same-rate fast path stays **byte-identical** to Phase 1 Step 2 --
  the diff for that branch is zero. Wrap it with the SRC branch
  placed first (early return) as shown above; do not refactor the
  same-rate normalize loop.
- ADR-005 hard-reject contract is binding: the overflow path mutates
  `OverflowCounter` only. No partial write, no soft-drop, no
  WritePos/Phase/Residual mutation on the reject path.
- Linear interpolation only. No sinc fallback. No second SRC path.
- The Step 0 `#pragma warning(push) / disable: 4505 / pop` block
  around `PickGCDDivisor` is removed once the SRC branch wires the
  call site -- the helper is no longer a transitional unused symbol,
  and leaving the suppression in place would mask any future
  genuine-unused regression.

## Acceptance Criteria

- [ ] Build clean (Utilities -> CableA -> CableB), no new warnings.
- [ ] Same-rate scenario still passes Phase 1 Step 1 / Step 2
      acceptance criteria -- no regression in the same-rate path.
- [ ] 44.1k -> 48k upsample at `WriteSrcPhase=0`: 441 input frames
      produce exactly 480 output frames; phase + residual carry
      across consecutive chunks gives the same total output count
      and sample values as a single-call equivalent (for sample
      values, evaluate after the leading SRC interval -- see
      "Startup transient" below).
- [ ] 48k -> 44.1k downsample at `WriteSrcPhase=0`: 480 input
      frames produce exactly 441 output frames.
- [ ] 48k -> 96k upsample (first-match per ADR-004:
      `Divisor=300`, `SrcRatio=160`, `DstRatio=320`): in steady
      state, 1 input frame yields 2 output frames.
- [ ] 8k -> 48k extreme upsample (`Divisor=100`, `SrcRatio=80`,
      `DstRatio=480`): in steady state, 1 input frame yields 6
      output frames.
- [ ] 96k -> 8k extreme downsample (`Divisor=100`,
      `SrcRatio=960`, `DstRatio=80`): in steady state, 12 input
      frames yield 1 output frame.
- [ ] Small-chunk downsample with `outputFrames == 0`: returns
      `STATUS_SUCCESS`, `WritePos` unchanged, `WriteSrcPhase` and
      `WriteSrcResidual[]` updated for the next call.
- [ ] Forced overflow at SRC rate: pre-fill ring so `writable` is
      exactly `outputFrames - 1`, send the chunk, observe
      `OverflowCounter` increments by exactly 1; `WritePos`,
      `WriteSrcPhase`, and `WriteSrcResidual[]` are all unchanged;
      return value is `STATUS_INSUFFICIENT_RESOURCES`.
- [ ] Phase-aware exact capacity check: the same chunk that fits
      at `WriteSrcPhase=0` may legitimately reject at
      `WriteSrcPhase=SrcRatio-1` (the +1 boundary case). The
      exact-form check reports overflow only when actually
      warranted, not on VB's `floor + 1` slack.
- [ ] Channel bound: a hostile input with `srcChannels` larger
      than `ARRAYSIZE(pipe->WriteSrcResidual)` is clamped (no
      out-of-bounds residual write); `copyChannels` follows the
      `min(srcChannels, pipe->Channels, ARRAYSIZE(WriteSrcResidual))`
      contract.
- [ ] No memory allocations on the hot path beyond stack
      accumulators (`curr[FP_MAX_CHANNELS]`).

### Startup transient

The first SRC interval after init / `FramePipeResetCable` blends the
zero `WriteSrcResidual` against the first real input. Bit-exact
sample comparisons against a reference resampler must therefore:

- skip the first `ceil(SrcRatio / DstRatio)` output frames, **or**
- prepend leading-zero frames to the input so the residual = 0
  matches the synthetic prepended sample,

before asserting bit-equality. Steady-state assertions apply once
the residual has been overwritten by real input data. This is a
property of the algorithm, not a defect; do not extend bit-exact
expectations into the leading interval.

## What This Step Does NOT Do

- Does not implement the read SRC path (Step 2 mirrors this on the
  read direction).
- Does not change caller wiring -- all external callers still go
  through legacy shims until Phases 4-6.
- Does not change `AO_STREAM_RT`.
- Does not change `loopback.h`.
- Does not introduce a sinc fallback, a 4-stage pipeline, or any
  second SRC path (CLAUDE.md Forbidden Compromises).

## Completion

```powershell
python scripts/execute.py mark 2-single-pass-src 1 completed --message "Write SRC path (linear interp + GCD + phase-aware capacity)."
```
