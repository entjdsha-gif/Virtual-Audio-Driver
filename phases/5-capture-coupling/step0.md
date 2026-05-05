# Phase 5 Step 0: Helper capture-side does the real ring → DMA write

## Read First

- Phase 4 (render flip patterns).
- `docs/AO_CABLE_V1_DESIGN.md` § 4 (helper capture branch),
  § 5.3 (capture path).
- `docs/ADR.md` ADR-005 (hysteretic underrun recovery — capture side
  is where this matters most).

## Goal

Flip the capture branch of `AoCableAdvanceByQpc` to real audible owner:
ring → per-stream scratch → capture DMA buffer with wrap handling.
Underrun recovery (`UnderrunFlag` + `WrapBound / 2` threshold) is
already implemented in `AoRingReadToScratch` (Phase 1 Step 2); the
helper just consumes the silence-filled output and copies it to DMA
unchanged.

## Planned Files

Edit only:

- `Source/Utilities/transport_engine.cpp` — capture branch of helper.

## Required Edits

```c
if (rt->IsCapture) {
    ULONG bytes = advance * rt->BlockAlign;
    NTSTATUS s = AoRingReadToScratch(rt->Pipe,
                                     (BYTE*)rt->CableScratchBuffer,
                                     advance,
                                     rt->SampleRate,
                                     rt->Channels,
                                     rt->BitsPerSample);
    /* AoRingReadToScratch always returns STATUS_SUCCESS; on underrun
     * it silence-fills scratch and increments UnderrunCounter. */
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

`CopyScratchToCaptureDmaWithWrap` handles the circular buffer wrap. It
must not allocate.

## Rules

- Tell the user before editing.
- Underrun recovery is owned by `AoRingReadToScratch`. The helper does
  not second-guess: if scratch comes back silence-filled, copy it to
  DMA verbatim.
- No `MicSink`-style direct DMA push from the render side. Capture
  reads only from the ring, end of story.

## Acceptance Criteria

- [ ] Build clean.
- [ ] Local sine loopback Cable A speaker → Cable A mic now uses the
      helper for both directions.
- [ ] `OverflowCounter` (render side) and `UnderrunCounter` (capture
      side) both 0 in steady state.
- [ ] Forced underrun on capture (e.g., pause render briefly) shows
      `UnderrunCounter` increments and capture goes silent until ring
      refills past `WrapBound / 2`.

## What This Step Does NOT Do

- Does not yet retire the legacy capture write path.
- Does not change non-cable streams.

## Completion

```powershell
python scripts/execute.py mark 5-capture-coupling 0 completed --message "Capture helper writes audibly; legacy parallel still active."
```
