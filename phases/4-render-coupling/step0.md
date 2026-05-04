# Phase 4 Step 0: Helper render-side does the real DMA → ring write

## Read First

- Phase 3 exit (divergence counter zero).
- `docs/AO_CABLE_V1_DESIGN.md` § 4.3 (helper render branch) and § 5.3
  (render path).

## Goal

Flip the render branch of `AoCableAdvanceByQpc` from shadow mode to
real audible owner: linearize DMA window into per-stream scratch,
call `AoRingWriteFromScratch`, advance `MonoFramesLow / Mirror` and
`DmaCursorFrames`. The legacy `UpdatePosition` cable-render branch
stays in place but becomes redundant — it will be retired in Step 2.

## Planned Files

Edit only:

- `Source/Utilities/transport_engine.cpp` — render branch of helper.

## Required Edits

Replace the shadow-only render branch with the real flow:

```c
if (!rt->IsCapture) {
    ULONG bytes = advance * rt->BlockAlign;
    LinearizeDmaWindowToScratch(rt, bytes);
    NTSTATUS s = AoRingWriteFromScratch(rt->Pipe,
                                        rt->CableScratchBuffer,
                                        advance,
                                        rt->SampleRate,
                                        rt->Channels,
                                        rt->BitsPerSample);
    if (NT_SUCCESS(s)) {
        AoApplyFadeEnvelope((LONG*)rt->CableScratchBuffer,
                            advance * rt->Channels,
                            &rt->FadeSampleCounter);
        rt->DmaCursorFrames = (rt->DmaCursorFrames + advance)
                              % (rt->DmaBufferSize / rt->BlockAlign);
        rt->MonoFramesLow    += advance;
        rt->MonoFramesMirror += advance;
    }
    /* Hard-reject overflow simply does not advance cursors —
     * the next tick will re-attempt with the same backing data
     * after the consumer drains. */
}
```

## Rules

- Tell the user before editing.
- Hard-reject overflow path **does not** retry, swap, or fall back.
  Counter increments and helper returns. Same-tick caller behavior
  is unchanged.
- Capture branch stays in shadow mode until Phase 5.

## Acceptance Criteria

- [ ] Build clean.
- [ ] Local sine loopback on Cable A render → capture preserves the
      sine without audible glitches in steady state.
- [ ] `OverflowCounter` stays 0 in steady state.
- [ ] `MonoFramesLow` advances at a rate matching the input frame
      flow (verify via `test_stream_monitor.py`).
- [ ] No regression in Cable B (independent ring).

## What This Step Does NOT Do

- Does not yet remove the legacy `UpdatePosition` cable-render
  branch. It runs in parallel and now is **redundant**, not
  authoritative.
- Does not flip capture audible.

## Completion

```powershell
python scripts/execute.py mark 4-render-coupling 0 completed --message "Render helper writes audibly; legacy parallel still active."
```
