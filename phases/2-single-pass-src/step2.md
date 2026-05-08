# Phase 2 Step 2: Ring read SRC path

## Read First

- `docs/ADR.md` ADR-004 (single-pass linear-interp SRC, GCD divisor
  first-match), ADR-005 (hysteretic underrun + monotone counters),
  ADR-008 (KSDATARANGE / PCM-only).
- `docs/AO_CABLE_V1_DESIGN.md` section 2.4 (read SRC algorithm) and section 2.5
  (bit-depth dispatch / 19-bit headroom).
- `results/vbcable_pipeline_analysis.md` section 3.4 (VB capacity check uses
  output ring frames; AO Cable V1 adopts a phase-aware EXACT form, see
  Required Edits below).
- Phase 2 Step 1 (`AoRingWriteFromScratch` write SRC). The read SRC
  is the inverted-direction mirror with two extra invariants
  (multi-consume reload and zero-consume lookahead readable
  requirement) covered below.

## Goal

Extend `AoRingReadToScratch` to handle the SRC path
(`dstRate != pipe->InternalRate`). The same-rate fast path stays
**byte-identical** to Phase 1 Step 3 -- the diff for that branch is
zero. Algorithm is single-pass linear interpolation with output-driven
loop and per-channel `ReadSrcPhase` / `ReadSrcResidual[]` carried
across calls. Phase 1 Step 3 hysteretic underrun contract
(`UnderrunFlag` / `UnderrunCounter` / `WrapBound/2` recovery threshold)
is preserved on the SRC branch with capacity comparison adjusted to
match the SRC framing.

## Planned Files

Edit only:

- `Source/Utilities/loopback.cpp`:
  - Replace the same-rate-only rate-mismatch stub in
    `AoRingReadToScratch` with the SRC branch.
  - Update the function-level docstring to document the SRC path
    alongside the Phase 1 Step 3 same-rate path.

`loopback.h` is **not** edited. `AO_STREAM_RT` is not edited. No
other translation unit is touched.

## Required Edits

### Function structure (branch-first, per-branch capacity)

The SRC branch is added BEFORE the same-rate body and returns early.
The same-rate code (lines 2010-2076 of current `loopback.cpp`) stays
byte-identical.

```c
KeAcquireSpinLock(&pipe->Lock, &oldIrql);

// SRC branch -- rate mismatch.
if (dstRate != pipe->InternalRate)
{
    AO_GCD_RATIO ratio;
    NTSTATUS gcdSt = PickGCDDivisor(pipe->InternalRate, dstRate, &ratio);
    if (!NT_SUCCESS(gcdSt)) {
        // Helper status propagated as-is. No state mutation.
        KeReleaseSpinLock(&pipe->Lock, oldIrql);
        return gcdSt;
    }

    // Hostile dstChannels reject -- option B (hybrid). Bound only by
    // the residual array size. dstChannels > pipe->Channels is
    // legitimate (mono ring -> stereo client, etc.) and is silence-
    // filled per Phase 1 Step 3 same-rate read (loopback.cpp:2052-2061
    // / DESIGN section 2.3 "no hidden upmix"). The reject covers only
    // dstChannels values that would let DenormalizeFromInt19 / silence
    // fill stride a hostile width into scratch (e.g. ULONG_MAX).
    if (dstChannels > (ULONG)ARRAYSIZE(pipe->ReadSrcResidual)) {
        KeReleaseSpinLock(&pipe->Lock, oldIrql);
        return STATUS_INVALID_PARAMETER;
    }

    // Phase-aware EXACT ReadPos advance count for this call.
    //
    // VB-Cable's evidence (results/vbcable_pipeline_analysis.md section 3.4)
    // uses a `floor + 1` safety idiom on the write direction. AO Cable
    // V1 chooses the symmetric phase-aware EXACT form on read:
    // ReadSrcPhase carries the precise common-tick offset since the
    // last consumed ring frame, so the actual ReadPos advance count
    // for this call is
    //   ringFramesNeeded64 =
    //     floor((ReadSrcPhase + frames*SrcRatio) / DstRatio)
    // This drives the SRC loop's invariant -- emits == ReadPos
    // advances == ringFramesNeeded64, bit-for-bit (proof in section "SRC
    // math invariants" below).
    ULONGLONG totalSrc = (ULONGLONG)(ULONG)pipe->ReadSrcPhase
                       + (ULONGLONG)frames * (ULONGLONG)ratio.SrcRatio;
    ULONGLONG ringFramesNeeded64 = totalSrc / (ULONGLONG)ratio.DstRatio;

    // Lookahead-aware readable-frame requirement (BLOCKER 2 fix).
    //
    // The output-driven SRC loop loads curr = ring[ReadPos] BEFORE the
    // first emit and uses it as the upper interpolation bound. When
    // ringFramesNeeded64 == 0 (small upsample chunk: total time
    // advance < one ring period), no ReadPos++ happens, but the
    // initial curr load still touches ring[ReadPos]. Treating
    // ringFramesNeeded64 alone as the readable requirement would let
    // (0 > available) accept available == 0 and read stale ring data.
    // Require >= 1 always; otherwise == ringFramesNeeded64.
    //
    // ReadPos advance count is ALWAYS == ringFramesNeeded64 (not
    // requiredReadableFrames64) -- the +1 only covers the lookahead
    // load, not actual consumption.
    ULONGLONG requiredReadableFrames64 =
        (ringFramesNeeded64 == 0) ? 1ULL : ringFramesNeeded64;

    LONG available = AoRingAvailableFrames_Locked(pipe);

    // ADR-005 hysteretic underrun -- Phase 1 Step 3 semantics on the
    // SRC branch. State invariants on each non-success path are in the
    // "State persistence contract" table below.
    if (pipe->UnderrunFlag) {
        if (available < pipe->WrapBound / 2) {
            // Stay in recovery -- silence delivered, no SRC state
            // mutation, UnderrunCounter unchanged.
            ZeroFillScratch(scratch, frames, dstBits, dstChannels);
            KeReleaseSpinLock(&pipe->Lock, oldIrql);
            return STATUS_SUCCESS;
        }
        // Refill crossed the 50% threshold -- exit recovery, fall
        // through to the capacity check.
        pipe->UnderrunFlag = 0;
    }

    if (requiredReadableFrames64 > (ULONGLONG)available) {
        // Hard underrun -- enter recovery, deliver silence. ReadPos /
        // ReadSrcPhase / ReadSrcResidual[] all unchanged so the caller
        // may retry on the next tick once more frames arrive.
        pipe->UnderrunCounter++;
        pipe->UnderrunFlag = 1;
        ZeroFillScratch(scratch, frames, dstBits, dstChannels);
        KeReleaseSpinLock(&pipe->Lock, oldIrql);
        return STATUS_SUCCESS;
    }

    // SRC channel mapping. dstChannels was already bounded by the
    // residual-array reject above (dstChannels <=
    // ARRAYSIZE(ReadSrcResidual)). copyChannels is the count of
    // channels the SRC actually computes for; surplus client channels
    // (copyChannels..dstChannels-1) get inline silence per Phase 1
    // Step 3 policy.
    LONG  ringChannels  = pipe->Channels;
    ULONG copyChannelsU = dstChannels;
    if (copyChannelsU > (ULONG)ringChannels) copyChannelsU = (ULONG)ringChannels;
    LONG  copyChannels  = (LONG)copyChannelsU;

    // SRC loop -- output-driven (see "SRC loop algorithm" below).
    /* ... */

    KeReleaseSpinLock(&pipe->Lock, oldIrql);
    return STATUS_SUCCESS;
}

// Same-rate fast path -- Phase 1 Step 3 logic, BYTE-IDENTICAL.
LONG available = AoRingAvailableFrames_Locked(pipe);
if (pipe->UnderrunFlag) { /* stay or exit -- existing logic */ }
if ((LONG)frames > available) { /* hard underrun -- existing logic */ }
/* ... existing per-frame DenormalizeFromInt19 loop ... */
```

### SRC loop algorithm (inline, per ADR-004 / DESIGN section 2.4)

Time model: with `PickGCDDivisor(pipe->InternalRate, dstRate, &ratio)`,
one ring frame spans `ratio.DstRatio` common ticks and one client
(output) frame spans `ratio.SrcRatio` common ticks. `ReadSrcPhase` is
in `[0, DstRatio)` and represents the common-tick distance since the
most recently consumed ring frame; it carries across calls.

`prev[ch]` is the value of the last consumed ring frame (carried via
`ReadSrcResidual[ch]`). `curr[ch]` is the value of the next ring frame
(loaded fresh from `ring[ReadPos]`). Linear interpolation produces the
output sample at fractional position `accum / DstRatio` between `prev`
and `curr`.

```c
LONG accum = pipe->ReadSrcPhase;
LONG prev[FP_MAX_CHANNELS];
LONG curr[FP_MAX_CHANNELS];

// prev[] carries from the prior call. curr[] is loaded fresh at the
// start of every outer iteration (and conditionally inside the inner
// while; see comments below). Capacity check above guarantees both
// the iteration-start ReadPos and every consumed ReadPos++ position
// are readable.
for (LONG ch = 0; ch < copyChannels; ++ch) {
    prev[ch] = pipe->ReadSrcResidual[ch];
}

for (ULONG f = 0; f < frames; ++f)
{
    // Per-iteration curr reload from the current ReadPos. The position
    // read here is always inside the readable window:
    //   f == 0: ReadPos is the entry value, covered by the
    //           requiredReadableFrames64 >= 1 lookahead.
    //   f >= 1: ReadPos is whatever the previous iteration's last
    //           consume left behind (no reload happened after that
    //           consume -- this iteration's start performs the read
    //           on the now-current outer loop). The position is one
    //           that ReadPos++ has just landed on, so it is inside
    //           the consumed-frame portion of requiredReadableFrames64.
    {
        LONG slotBase = pipe->ReadPos * ringChannels;
        for (LONG ch = 0; ch < copyChannels; ++ch) {
            curr[ch] = pipe->Data[slotBase + ch];
        }
    }

    // Emit at fractional position accum/DstRatio between prev and curr.
    //   sample = (prev*(DstRatio-accum) + curr*accum) / DstRatio
    // Range invariant (DESIGN section 2.5 19-bit headroom):
    //   prev, curr in [-2^18, 2^18]; accum, (DstRatio-accum) in
    //   [0, DstRatio-1]; DstRatio <= 2560. LONGLONG intermediate
    //   matches DenormalizeFromInt19's defensive style and removes
    //   any future doubt about overflow at the two-product sum.
    for (LONG ch = 0; ch < copyChannels; ++ch) {
        LONGLONG mixed =
            ((LONGLONG)prev[ch] * (LONGLONG)((LONG)ratio.DstRatio - accum))
          + ((LONGLONG)curr[ch] * (LONGLONG)accum);
        LONG sample = (LONG)(mixed / (LONGLONG)ratio.DstRatio);
        DenormalizeFromInt19(scratch, dstBits, f, (ULONG)ch,
                             dstChannels, sample);
    }
    // Surplus client channels (ch >= ringChannels): inline silence
    // (Phase 1 Step 3 section 4 / DESIGN section 2.3 "no hidden upmix").
    for (ULONG ch = (ULONG)ringChannels; ch < dstChannels; ++ch) {
        SIZE_T off = ((SIZE_T)f * (SIZE_T)dstChannels + (SIZE_T)ch)
                   * (SIZE_T)(dstBits / 8);
        BYTE* p = scratch + off;
        if (dstBits == 8) *p = 0x80;
        else { /* 16/24/32: 0x00 fill */ }
    }

    accum += (LONG)ratio.SrcRatio;

    // Multi-consume reload (BLOCKER 1 fix), CONDITIONAL on more
    // consumes ahead in this iteration.
    //
    // For downsample (DstRatio < SrcRatio) the inner while can advance
    // ReadPos multiple times per output frame. Two distinct cases:
    //
    // (a) MID-consume advance: another consume is still pending in
    //     this iteration (`accum >= DstRatio` is true after the
    //     subtract). curr MUST be reloaded from the new ReadPos so
    //     the next prev = curr promotion captures the actual
    //     intermediate ring sample. Without this reload, prev = curr
    //     would store the SAME value as the previous promotion and
    //     ring[R+1..R+K-1] would never enter the prev sequence at
    //     all. Concretely, 96k -> 48k (SrcRatio=320, DstRatio=160)
    //     emits once, accum becomes 320, the while body runs twice;
    //     the FIRST consume MUST reload curr = ring[R+1].
    //
    // (b) FINAL consume of this iteration (`accum < DstRatio` after
    //     the subtract). curr MUST NOT be reloaded -- the new
    //     ReadPos may point at a frame outside the readable window
    //     when `available == ringFramesNeeded64` exactly. The next
    //     outer iteration's start handles the reload (when there is
    //     one), and a final outer iteration leaves the post-final-
    //     consume position untouched until the next call.
    //
    // The conditional reload below ties memory reads exactly to
    // requiredReadableFrames64 = max(ringFramesNeeded64, 1) -- 1 for
    // the initial lookahead and ringFramesNeeded64 for the consumed
    // frames. No stale read can occur on the success path.
    while (accum >= (LONG)ratio.DstRatio)
    {
        accum -= (LONG)ratio.DstRatio;
        for (LONG ch = 0; ch < copyChannels; ++ch) {
            prev[ch] = curr[ch];
        }
        pipe->ReadPos++;
        if (pipe->ReadPos >= pipe->WrapBound) pipe->ReadPos = 0;
        // Conditional reload: only when more consumes remain in this
        // iteration. Final consume leaves curr unread (next outer
        // iteration's start handles the reload, or the call ends).
        if (accum >= (LONG)ratio.DstRatio) {
            LONG slotBase = pipe->ReadPos * ringChannels;
            for (LONG ch = 0; ch < copyChannels; ++ch) {
                curr[ch] = pipe->Data[slotBase + ch];
            }
        }
    }
}

// Persist phase + residuals across the call boundary.
for (LONG ch = 0; ch < copyChannels; ++ch) {
    pipe->ReadSrcResidual[ch] = prev[ch];
}
pipe->ReadSrcPhase = accum;
```

### SRC math invariants

**emit count == ReadPos advance count == `ringFramesNeeded64`**:
the loop runs `accum += SrcRatio` exactly once per output frame
(`frames` total) and `accum -= DstRatio` exactly once per ReadPos
advance. Total accumulator delta = `frames * SrcRatio - emits *
DstRatio`. The loop terminates with `accum` in `[0, DstRatio)`.
Solving:

```text
emits = floor((ReadSrcPhase + frames * SrcRatio) / DstRatio)
      = ringFramesNeeded64.
```

**Capacity check guarantees** (BLOCKER reload-fix invariant):

- the **initial `ring[ReadPos]` lookahead** read at the start of the
  first outer iteration is readable (covered by the `>= 1` floor of
  `requiredReadableFrames64`),
- **every ring frame actually consumed by `ReadPos++`** is readable
  (covered by the `ringFramesNeeded64` portion of
  `requiredReadableFrames64`).

The loop **must not reload the post-final-consume `ReadPos`** inside
the same call. When `available == ringFramesNeeded64` exactly, the
position one past the last consume is outside the readable window;
the conditional reload pattern above (mid-consume only, final-consume
skipped) ties memory reads exactly to `max(ringFramesNeeded64, 1)`.
A future outer iteration of the same call (if any) reloads from that
position, but capacity has been verified to cover that case too -- it
is the next consume's pre-position, not a "post-final-consume"
artifact.

**ReadPos advance count != requiredReadableFrames64** (subtle but
important): the `>= 1` floor only covers the lookahead read; ReadPos
advance count stays exactly `ringFramesNeeded64`. The ring's "position
one past the last consumed frame" is allowed to point at a frame the
producer hasn't filled yet only after the call returns -- the next
call's capacity check re-evaluates `available` and either rejects or
proceeds.

### State persistence contract

| Path | `ReadPos` | `ReadSrcPhase` | `ReadSrcResidual[]` | `UnderrunFlag` | `UnderrunCounter` |
|---|---|---|---|---|---|
| `PickGCDDivisor` failure | unchanged | unchanged | unchanged | unchanged | unchanged |
| hostile-`dstChannels` reject (`STATUS_INVALID_PARAMETER`) | **unchanged** | **unchanged** | **unchanged** | **unchanged** | **unchanged** |
| hysteresis stay (`UnderrunFlag=1` AND `available < WrapBound/2`) -- silence delivered | **unchanged** | **unchanged** | **unchanged** | unchanged (stays 1) | unchanged |
| hysteresis exit (`UnderrunFlag=1` AND `available >= WrapBound/2`) -> falls through | (depends on subsequent path) | (depends) | (depends) | cleared to 0 | unchanged |
| hard underrun (`requiredReadableFrames64 > available`) -- silence delivered | **unchanged** | **unchanged** | **unchanged** | set to 1 | `++` |
| SRC success (any `ringFramesNeeded64 >= 0`, including 0) | advanced by `ringFramesNeeded64` (modulo `WrapBound`) | updated to post-loop `accum` | updated for `ch < copyChannels` | unchanged (already 0) | unchanged |
| Same-rate path | (Phase 1 Step 3, unchanged) | n/a (not used in same-rate) | n/a | per existing Phase 1 contract | per Phase 1 |

Channel bound (option B -- hybrid):

```text
if (dstChannels > ARRAYSIZE(pipe->ReadSrcResidual))
    return STATUS_INVALID_PARAMETER;     // before any state mutation
copyChannels = min(dstChannels, pipe->Channels);
                                         // SRC computes for ring-bound
                                         // channels only; surplus
                                         // client slots receive
                                         // silence inline per Phase 1
                                         // Step 3 read policy.
```

The reject runs AFTER `PickGCDDivisor` (so unsupported rate pairs
still surface `STATUS_NOT_SUPPORTED` first) and BEFORE the capacity
check (so a hostile input never increments `UnderrunCounter` or
clears `UnderrunFlag`).

## Rules

- Tell the user before editing (CLAUDE.md Edit Protocol).
- Same-rate fast path stays **byte-identical** to Phase 1 Step 3 --
  the diff for that branch is zero. The SRC branch is placed first
  with early return; the same-rate body sits immediately after.
- ADR-005 hard-underrun contract is binding: silence delivered, NO
  partial read, NO stale-ring read, NO `ReadPos` / `ReadSrcPhase` /
  `ReadSrcResidual[]` mutation on any non-success path.
- Linear interpolation only. No sinc fallback. No second SRC path.
- Multi-consume reload (BLOCKER 1) is mandatory for downsample
  correctness; do not reorder/remove it under "optimization".
- The reload after `ReadPos++` is **conditional** on `accum >=
  DstRatio` (i.e., another consume is pending in the same iteration).
  The post-final-consume position must NOT be reloaded -- that
  position can sit outside the readable window when
  `available == ringFramesNeeded64` exactly, and an unconditional
  reload there would re-introduce the BLOCKER stale-read.
- Lookahead-aware capacity (BLOCKER 2) is mandatory for stale-read
  prevention; the `requiredReadableFrames64` distinction must persist
  even if the SRC loop is later refactored. The `>= 1` floor only
  covers the lookahead, NOT extra slack on consumed frames.

## Acceptance Criteria

- [ ] Build clean (Utilities -> CableA -> CableB), no new warnings.
- [ ] Same-rate scenario still passes Phase 1 Step 3 acceptance --
      no regression in the same-rate path.
- [ ] **Rate-pair output counts** at `ReadSrcPhase=0`:
      - 96k ring -> 48k client (downsample, `SrcRatio=320`,
        `DstRatio=160`): 1 client frame consumes exactly 2 ring
        frames in steady state.
      - 48k ring -> 96k client (upsample, `SrcRatio=160`,
        `DstRatio=320`): 2 client frames consume exactly 1 ring
        frame in steady state.
      - 48k ring -> 8k client (extreme downsample, `SrcRatio=480`,
        `DstRatio=80`): 1 client frame consumes exactly 6 ring
        frames.
      - 48k ring -> 22.05k client (`SrcRatio=640`, `DstRatio=294`):
        per-output ring consume averages `SrcRatio/DstRatio`.
- [ ] **Mid-consume reload regression** (BLOCKER 1): in 96k -> 48k
      downsample, every intermediate ring sample appears in the
      `prev` sequence as the algorithm walks past it. A test that
      writes a unique tag value to each ring slot and inspects the
      emitted `ReadSrcResidual[]` history must show no skipped tags.
- [ ] **No post-final-consume read** (BLOCKER reload-fix invariant):
      with `available == ringFramesNeeded64` exactly, the SRC loop
      touches exactly `requiredReadableFrames64` distinct ring
      positions across the call and does NOT read the position one
      past the last consume. A test that places a sentinel beyond
      the readable window and verifies the sentinel never enters the
      `prev` sequence demonstrates this invariant. Specifically:
      96k -> 48k with frames=2, phase=0, available=4 must read ring
      slots `[R, R+1, R+2, R+3]` and NOT `R+4`.
- [ ] **`ringFramesNeeded64 == 0` cases** (BLOCKER 2):
      - `available == 0` -> hard underrun (silence delivered,
        `UnderrunCounter += 1`, `UnderrunFlag = 1`, `ReadPos`
        unchanged, `ReadSrcPhase` unchanged, `ReadSrcResidual[]`
        unchanged).
      - `available >= 1` -> success (one client frame emitted),
        `ReadPos` unchanged, `ReadSrcPhase` updated, optionally
        `ReadSrcResidual[]` unchanged (no advance happened).
- [ ] **Forced hard underrun at SRC rate**: pre-drain ring so
      `available` is exactly `requiredReadableFrames64 - 1`, send
      the chunk -> `UnderrunCounter` increments by exactly 1,
      `UnderrunFlag` set to 1, `ReadPos` / `ReadSrcPhase` /
      `ReadSrcResidual[]` all unchanged, return value
      `STATUS_SUCCESS` (silence delivered).
- [ ] **Hysteresis stay**: pre-set `UnderrunFlag=1` and
      `available < WrapBound/2`. Send a chunk -> silence delivered,
      ALL SRC state unchanged (including `UnderrunCounter`, since the
      stay path does not increment it).
- [ ] **Hysteresis exit**: pre-set `UnderrunFlag=1` and
      `available >= WrapBound/2`. Send a chunk -> `UnderrunFlag`
      cleared to 0, then either SRC success or hard underrun depending
      on the subsequent capacity check.
- [ ] **Phase carry parity**: split a long-frame call into N chunks;
      total emit count and final `ReadPos` / `ReadSrcPhase` /
      `ReadSrcResidual[]` / scratch byte-identical to single-call.
- [ ] **Hostile `dstChannels` (option B hybrid)**:
      - `dstChannels > ARRAYSIZE(pipe->ReadSrcResidual)` -> reject
        with `STATUS_INVALID_PARAMETER`, all state unchanged
        (including `UnderrunCounter`).
      - `dstChannels > pipe->Channels` but `<=
        ARRAYSIZE(ReadSrcResidual)` -> success; channels in
        `[copyChannels, dstChannels)` filled with silence (8-bit
        `0x80` / 16/24/32-bit `0x00`).
      - Boundary `dstChannels == ARRAYSIZE(ReadSrcResidual)` (= 16):
        accept.
- [ ] **Huge frames cast wrap regression**: `frames * SrcRatio`
      large enough that `ringFramesNeeded64 > (1 << 32)` -- capacity
      check rejects via the `ULONGLONG` comparison; truncating to
      `ULONG` (or `LONG`) would let a wrapped value pass.
- [ ] **Phase-aware exact boundary**: the same chunk that fits at
      `ReadSrcPhase=0` must legitimately reject at
      `ReadSrcPhase=DstRatio-1` (the +1 boundary case). The exact-
      form check reports underrun only when actually warranted.
- [ ] **No memory allocations on the hot path** beyond stack
      accumulators (`prev` / `curr` arrays of size `FP_MAX_CHANNELS`).

### Startup transient

The first SRC interval after init / `FramePipeResetCable` blends the
zero `ReadSrcResidual` against the first ring frame. Bit-exact sample
comparisons against a reference resampler must therefore:

- skip the first `ceil(DstRatio / SrcRatio)` output frames, **or**
- pre-fill `ReadSrcResidual[]` via a dummy emit before bit-exact
  assertion,

before asserting bit-equality. Steady-state assertions apply once the
residual has been overwritten by real ring data. This is a property
of the algorithm, not a defect; do not extend bit-exact expectations
into the leading interval.

## What This Step Does NOT Do

- Does not change the same-rate fast-path semantics.
- Does not change caller wiring -- all external callers still go
  through legacy shims until Phases 4-6.
- Does not change `AO_STREAM_RT`.
- Does not change `loopback.h`.
- Does not introduce a sinc fallback, a 4-stage pipeline, or any
  second SRC path (CLAUDE.md Forbidden Compromises).

## Completion

```powershell
python scripts/execute.py mark 2-single-pass-src 2 completed --message "Read SRC path (linear interp + GCD + phase-aware capacity + multi-consume reload)."
```
