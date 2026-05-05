# Phase 4 Step 1: Render audible flip — helper writes audibly, legacy retires (atomic)

## Read First

- Phase 3 exit (divergence counter gate is ≤ 5 increments/minute
  during steady-state speech).
- Phase 4 Step 0 (fade envelope helpers exist).

## Precondition (re-check immediately before this commit)

Before the atomic flip lands, re-run a live call on the helper-shadow
build and confirm `<Cable>_<R/C>_ShadowDivergenceCount` is still
≤ 5/min. The Phase 3 exit threshold is the **same** threshold here;
a stale Phase 3 measurement does not satisfy this precondition. If
the rate has drifted upward since Phase 3 closed, fix the cause
before flipping. (Review #12 of 8afa59a unifies the threshold
language across Phase 3 exit and Phase 4 Step 1 precondition.)

- `docs/AO_CABLE_V1_DESIGN.md` § 4.3 (helper render branch),
  § 5.3 (destructor / `FreeAudioBuffer` ordering).
- `docs/REVIEW_POLICY.md` § 2 (no transient parallel owner commit).

## Goal

Atomically flip render audible ownership from the legacy
`UpdatePosition` cable-render branch to `AoCableAdvanceByQpc`. In a
**single commit**:

1. The helper render branch performs the real DMA → scratch → fade →
   ring write and advances render cursors.
2. The legacy `UpdatePosition` cable-render branch becomes a no-op
   for cable streams (still callable by PortCls, but does no audible
   work).
3. The legacy `WriteBytes` cable-render branch becomes a no-op for
   cable streams.

This step has no in-between commit where two transport owners run
simultaneously. (Review #13 of 8afa59a; REVIEW_POLICY § 2 forbids the
split.)

## Planned Files

Edit atomically (single commit):

- `Source/Utilities/transport_engine.cpp` — render branch of
  `AoCableAdvanceByQpc`: implement real DMA → scratch → fade → ring
  write.
- `Source/Main/minwavertstream.cpp` — `UpdatePosition` cable branch
  becomes no-op (or invokes the helper as a wake source);
  `WriteBytes` cable branch becomes no-op.

## Required Edits

### Helper render branch (transport_engine.cpp)

Replace the shadow-only render branch with:

```c
if (!rt->IsCapture) {
    /* Guard: if the WaveRT-mapped buffer is being freed, skip this
     * tick — DESIGN § 5.4. */
    if (rt->DmaBuffer == NULL) {
        return;  /* helper still holds rt->PositionLock; caller releases */
    }

    ULONG bytes = advance * rt->BlockAlign;
    LinearizeDmaWindowToScratch(rt, bytes);

    /* Fade envelope MUST be applied to scratch BEFORE the ring write,
     * otherwise the ring receives un-faded samples and the envelope
     * affects nothing audible (review #15). */
    AoApplyFadeEnvelope((LONG*)rt->CableScratchBuffer,
                        advance * rt->Channels,
                        &rt->FadeSampleCounter);

    NTSTATUS s = AoRingWriteFromScratch(rt->Pipe,
                                        rt->CableScratchBuffer,
                                        advance,
                                        rt->SampleRate,
                                        rt->Channels,
                                        rt->BitsPerSample);
    if (NT_SUCCESS(s)) {
        rt->DmaCursorFrames = (rt->DmaCursorFrames + advance)
                              % (rt->DmaBufferSize / rt->BlockAlign);
        rt->MonoFramesLow    += advance;
        rt->MonoFramesMirror += advance;
    }
    /* Hard-reject overflow: counter already incremented inside
     * AoRingWriteFromScratch; helper does not retry, swap, or fall
     * back. The next tick will re-attempt with the same backing data
     * after the consumer drains. */
}
```

### Legacy retirement (minwavertstream.cpp)

In `UpdatePosition(LARGE_INTEGER ilQPC)`:

```c
VOID
CMiniportWaveRTStream::UpdatePosition(LARGE_INTEGER ilQPC)
{
    if (IsCableStream(this) && m_pTransportRt) {
        /* Cable transport is owned by AoCableAdvanceByQpc.
         * UpdatePosition becomes a wake source only. */
        AoCableAdvanceByQpc(m_pTransportRt, ilQPC.QuadPart,
                            AO_ADVANCE_QUERY, 0);
        return;
    }
    /* non-cable legacy path — strictly unchanged */
    ...
}
```

In `WriteBytes(ULONG ByteDisplacement)`:

```c
VOID
CMiniportWaveRTStream::WriteBytes(ULONG ByteDisplacement)
{
    if (IsCableStream(this)) {
        /* Render fill is owned by helper. No-op for cable. */
        return;
    }
    /* non-cable legacy path — strictly unchanged */
    ...
}
```

## Rules

- **Single commit.** Helper flip and legacy retirement land together
  or not at all. No interim "parallel still active" state.
- Non-cable behavior is **strictly unchanged**.
- Hard-reject overflow path **does not** retry, swap, or fall back.
  Counter increments inside `AoRingWriteFromScratch`; helper just
  skips advancing cursors.
- Capture branch stays in shadow mode until Phase 5.

## Acceptance Criteria

- [ ] Build clean.
- [ ] Local sine loopback on Cable A render → capture preserves the
      sine without audible glitches in steady state.
- [ ] `OverflowCounter` stays 0 in steady state.
- [ ] `MonoFramesLow` advances at a rate matching the input frame
      flow (verify via `test_stream_monitor.py`).
- [ ] Legacy advance counters at the stream level
      (`m_ulPumpProcessedFrames` etc.) **stop incrementing** on cable
      streams from this commit forward.
- [ ] No regression in Cable B (independent ring).
- [ ] No regression in non-cable streams.

## What This Step Does NOT Do

- Does not flip capture audible ownership (Phase 5).
- Does not run the live-call validation (Step 2).

## Completion

```powershell
python scripts/execute.py mark 4-render-coupling 1 completed --message "Render audible flip atomic: helper owns, legacy retired in same commit."
```
