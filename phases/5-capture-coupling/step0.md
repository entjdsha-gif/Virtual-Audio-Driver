# Phase 5 Step 0: Capture audible flip — helper writes audibly, legacy retires (atomic)

## Read First

- Phase 4 (render audible flip pattern; this step mirrors it for
  capture).
- `docs/AO_CABLE_V1_DESIGN.md` § 4 (helper capture branch),
  § 5.3 (destructor / `FreeAudioBuffer` ordering),
  § 5.4 (DMA buffer lifetime).
- `docs/ADR.md` ADR-005 (hysteretic underrun recovery — capture
  side is where this matters most).
- `docs/REVIEW_POLICY.md` § 2 (no transient parallel owner commit;
  no `MicSink` resurrection).

## Goal

Atomically flip capture audible ownership from the legacy
`WriteBytes` (capture-side) cable branch to `AoCableAdvanceByQpc`. In
a **single commit**:

1. The helper capture branch performs the real ring → scratch → DMA
   write with wrap handling and advances capture cursors.
2. The legacy `WriteBytes` capture-side cable branch becomes a no-op
   for cable streams.
3. Any remaining `MicSink`-related call sites are removed
   simultaneously (no `MicSink` survives this commit; ADR-002 +
   REVIEW_POLICY § 2).

This step has no in-between commit where two transport owners run
simultaneously. (Review #14 of 8afa59a; same atomicity rule as
Phase 4 Step 1.)

## Planned Files

Edit atomically (single commit):

- `Source/Utilities/transport_engine.cpp` — capture branch of
  `AoCableAdvanceByQpc`: implement real ring → scratch → DMA write.
- `Source/Main/minwavertstream.cpp` — capture-side `WriteBytes`
  cable branch becomes no-op; any remaining capture-side
  `UpdatePosition` cable side-effects retired.
- `Source/Main/minwavert.cpp` (or wherever `MicSink` is wired) —
  remove the `MicSink` dual-write call site entirely.

## Required Edits

### Helper capture branch (transport_engine.cpp)

```c
if (rt->IsCapture) {
    /* Guard: if the WaveRT-mapped buffer is being freed, skip this
     * tick — DESIGN § 5.4. */
    if (rt->DmaBuffer == NULL) {
        return;  /* helper still holds rt->PositionLock; caller releases */
    }

    ULONG bytes = advance * rt->BlockAlign;
    NTSTATUS s = AoRingReadToScratch(rt->Pipe,
                                     (BYTE*)rt->CableScratchBuffer,
                                     advance,
                                     rt->SampleRate,
                                     rt->Channels,
                                     rt->BitsPerSample);
    /* AoRingReadToScratch always returns STATUS_SUCCESS; on underrun
     * it silence-fills scratch and increments UnderrunCounter +
     * sets UnderrunFlag (Phase 1 Step 4 hysteresis). */
    CopyScratchToCaptureDmaWithWrap(rt, bytes);
    rt->DmaCursorFrames = (rt->DmaCursorFrames + advance)
                          % (rt->DmaBufferSize / rt->BlockAlign);
    rt->MonoFramesLow    += advance;
    rt->MonoFramesMirror += advance;

    /* V1 scope: shared-mode capture (Phone Link) never arms packet
     * notification. NotifyArmed stays 0; do not implement the
     * dispatch in Phase 5. Notification arming + dispatch is deferred
     * to Phase 7 if a future event-driven client demands it. */
}
```

`CopyScratchToCaptureDmaWithWrap` handles the circular buffer wrap.
It must not allocate.

### Legacy retirement (minwavertstream.cpp / minwavert.cpp)

In capture-side `WriteBytes` (or whichever symbol the existing code
uses for capture fill):

```c
VOID
CMiniportWaveRTStream::WriteBytes(ULONG ByteDisplacement)
{
    if (IsCableStream(this)) {
        /* Capture fill is owned by helper. No-op for cable. */
        return;
    }
    /* non-cable legacy path — strictly unchanged */
    ...
}
```

Remove the `MicSink` dual-write call site **in the same commit**. No
`MicSink`-related symbol may survive past this step. If a `MicSink`
field, function, or call site is left behind, this step's commit is
rejected by REVIEW_POLICY § 2.

## Rules

- **Single commit.** Helper flip and legacy retirement (including
  `MicSink` removal) land together or not at all. No interim
  "parallel still active" or "MicSink behind a flag" state.
- Underrun recovery is owned by `AoRingReadToScratch`. The helper
  does not second-guess: if scratch comes back silence-filled, copy
  it to DMA verbatim.
- No `MicSink`-style direct DMA push from the render side. Capture
  reads only from the ring, end of story.
- Non-cable behavior is **strictly unchanged**.

## Acceptance Criteria

- [ ] Build clean.
- [ ] Local sine loopback Cable A speaker → Cable A mic now uses the
      helper for both directions.
- [ ] `OverflowCounter` (render side) and `UnderrunCounter` (capture
      side) both 0 in steady state; `UnderrunFlag = 0`.
- [ ] Forced underrun on capture (e.g., pause render briefly) shows
      `UnderrunCounter` increments, `UnderrunFlag = 1`, capture goes
      silent until ring refills past `WrapBound / 2`, then
      `UnderrunFlag = 0`.
- [ ] `grep -r MicSink Source/` returns nothing in the cable path.
- [ ] No regression in non-cable streams.

## What This Step Does NOT Do

- Does not run the live-call validation (Step 1).
- Does not change non-cable streams.

## Completion

```powershell
python scripts/execute.py mark 5-capture-coupling 0 completed --message "Capture audible flip atomic: helper owns, legacy + MicSink retired in same commit."
```
