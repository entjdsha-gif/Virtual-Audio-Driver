# Phase 3 Step 0: AO_STREAM_RT field freeze

## Read First

- `docs/AO_CABLE_V1_DESIGN.md` section 3 (per-stream runtime).
- `docs/ADR.md` ADR-006 (canonical helper).
- Current `Source/Utilities/transport_engine.h` `AO_STREAM_RT` shape.

## Goal

Freeze the `AO_STREAM_RT` field set so Phases 3-6 do not have to keep
extending the struct. Add Stage-3 fields that the helper body needs;
remove any retired fields from earlier Phase 6 Y attempts.

## Planned Files

Edit only:

- `Source/Utilities/transport_engine.h` -- `AO_STREAM_RT` field set
  matching `docs/AO_CABLE_V1_DESIGN.md` section 3.1.
- `Source/Utilities/transport_engine.cpp` -- adjust
  `AoTransportAllocStreamRt` / `AoTransportFreeStreamRt` /
  `AoCableResetRuntimeFields` to match the new field set.

Do not touch:

- `Source/Main/minwavertstream.*` -- call sites are wired in Steps 2/3.
- `Source/Utilities/loopback.*` -- Phase 1/2 fields stay.

## Required Edits

Reconcile current `AO_STREAM_RT` against section 3.1:

- Keep: `Link`, `RefCount`, `Stream`, `IsCable`, `IsCapture`,
  `IsSpeakerSide`, `Active`, `SampleRate`, `Channels`, `BlockAlign`,
  `Pipe`, `DmaBuffer`, `DmaBufferSize`, `RingSizeFrames`,
  `AnchorQpc100ns`, `PublishedFramesSinceAnchor`, `DmaCursorFrames`,
  `DmaCursorFramesPrev`, `MonoFramesLow`, `MonoFramesMirror`,
  `LastAdvanceDelta`, `BitsPerSample`, `NotifyBoundaryBytes`,
  `NotifyArmed`, `NotifyFired`, `CableScratchBuffer`, `CableScratchSize`,
  `FadeSampleCounter`, `FramesPerEvent`, `BytesPerEvent`,
  `EventPeriodQpc`, `NextEventQpc`, `StatOverrunCounter`,
  `DbgShadowAdvanceHits`, `DbgShadowQueryHits`, `DbgShadowTimerHits`.

- Add `PositionLock` (`KSPIN_LOCK`) -- protects all advance state when
  acquired by the helper.

- Remove (Phase 6 Y2-2 leftovers no longer needed):
  - `RenderAudibleActive`, `DbgY2*` fields (these belonged to the
    intermediate Y2 audible-flip switch; Phase 4 in V1 design uses a
    different mechanism).
  - Any other field present in current code but not in section 3.1.

If a field cannot be removed without breaking compilation of legacy
code paths that haven't been retired yet, leave it temporarily and
record the dependency in a TODO comment naming the legacy caller and
the future phase that retires it.

## Rules

- Tell the user before each edit.
- Document every removed field in the commit message with the legacy
  caller (if any) and which future phase retires that caller.
- If `RefCount` discipline (engine `RefCount++` on snapshot,
  `RefCount--` after) is not yet in the engine timer DPC, that is
  acceptable -- Steps 3 / 4 wire it. Step 0 only freezes the field
  shape.

## Acceptance Criteria

- [ ] Build clean.
- [ ] `AO_STREAM_RT` matches `docs/AO_CABLE_V1_DESIGN.md` section 3.1
      exactly. Any deviation is a TODO with a documented caller and
      retirement-phase target.
- [ ] No regression in non-cable streams.

## What This Step Does NOT Do

- Does not implement the helper body (Step 1).
- Does not change call sites.
- Does not flip audible ownership.

## Completion

```powershell
python scripts/execute.py mark 3-canonical-helper-shadow 0 completed --message "AO_STREAM_RT field freeze."
```
