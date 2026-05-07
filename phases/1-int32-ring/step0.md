# Phase 1 Step 0: Cross-TU FRAME_PIPE access removal

## Read First

- `docs/ADR.md` ADR-003 (INT32 frame-indexed ring), ADR-005
  (hard-reject overflow + hysteretic underrun)
- `docs/AO_CABLE_V1_DESIGN.md` section 2 (FRAME_PIPE detail)
- `docs/REVIEW_POLICY.md` section 2 (forbidden compromises)
- `Source/Utilities/loopback.h` and `Source/Utilities/loopback.cpp`
- `Source/Main/minwavertstream.cpp`

## Goal

Prepare for the `FRAME_PIPE` struct/API rewrite by removing direct
cross-translation-unit member access to `FRAME_PIPE` from
`Source/Main/minwavertstream.cpp`.

This step exists because the old Step 0 contract was self-contradictory:
it required replacing the `FRAME_PIPE` struct while also requiring no
edits to `minwavertstream.cpp`, but `minwavertstream.cpp` directly reads
or writes old `FRAME_PIPE` fields. Both requirements cannot be true at
the same time.

After this step, `minwavertstream.cpp` may keep using the legacy
`FramePipe*` function API, but it must not access `FRAME_PIPE` members
directly. The legacy struct shape remains unchanged until Step 1.

## Planned Files

Edit only:

- `Source/Utilities/loopback.h` - declare narrow helper functions for
  the old fields currently read/written outside `loopback.cpp`.
- `Source/Utilities/loopback.cpp` - implement those helpers against the
  current legacy `FRAME_PIPE` shape.
- `Source/Main/minwavertstream.cpp` - replace direct `pFP->...` member
  access with the new helpers.

Do not touch:

- `Source/Main/adapter.cpp`
- `Source/Main/ioctl.h`
- `Source/Utilities/transport_engine.cpp`
- `Source/Utilities/transport_engine.h`
- `LOOPBACK_BUFFER` fields or `Loopback*` functions

## Required Edits

1. Audit all `FRAME_PIPE` direct member access outside
   `loopback.h` / `loopback.cpp`:

   ```powershell
   rg -n "pFP->|pPipe->|FRAME_PIPE" Source/Main Source/Utilities
   ```

   Classify each hit as one of:

   - Legacy API call or pointer plumbing: allowed.
   - Direct status read, such as `SpeakerActive` / `MicActive`:
     replace with a helper.
   - Direct pump flag write/reset:
     replace with a helper.
   - Direct pump diagnostic counter mirror:
     replace with a helper.
   - Direct ring transport state access:
     stop and report before changing; Step 0 must not invent a second
     transport path.

2. Add only narrow helper APIs. The helper names may differ from this
   sketch if the local style suggests better names, but the ownership
   boundary must be the same:

   ```c
   BOOLEAN FramePipeIsDirectionActive(PFRAME_PIPE pipe, BOOLEAN isSpeaker);
   VOID FramePipeSetPumpFeatureFlags(PFRAME_PIPE pipe, BOOLEAN isRenderSide, ULONG flags);
   VOID FramePipeResetPumpFeatureFlags(PFRAME_PIPE pipe, BOOLEAN isRenderSide);
   VOID FramePipePublishPumpCounters(PFRAME_PIPE pipe,
                                     BOOLEAN isRenderSide,
                                     ULONG gatedSkipCount,
                                     ULONG overJumpCount,
                                     ULONGLONG framesProcessedTotal,
                                     ULONG invocationCount,
                                     ULONG shadowDivergenceCount,
                                     ULONG featureFlags);
   ```

3. Replace all direct `FRAME_PIPE` member access in
   `minwavertstream.cpp` with helper calls. Existing calls such as
   `FramePipeRegisterFormat`, `FramePipeUnregisterFormat`,
   `FramePipeReset`, `FramePipeReadToDma`, and `FramePipeWriteFromDma`
   may remain.

4. Do not change behavior. This is a boundary cleanup step only:

   - No struct layout change.
   - No ring allocation change.
   - No read/write algorithm change.
   - No SRC change.
   - No diagnostics schema change.

5. STOP condition: if removing direct access requires changing stream
   timing, WaveRT position behavior, DMA buffer behavior, INF behavior,
   `AO_STREAM_RT`, or the cable transport design, stop and report the
   exact blocker before editing further.

## Rules

- Tell the user before editing each file.
- Helpers must live in `loopback.h` / `loopback.cpp`; do not expose the
  old struct fields through new macros.
- Do not add a second cable transport owner.
- Do not weaken overflow, underrun, or stale-data semantics.
- Do not delete diagnostics counters in this step.

## Acceptance Criteria

- [ ] `build-verify.ps1 -Config Release` succeeds.
- [ ] `Source/Main/minwavertstream.cpp` contains no direct `FRAME_PIPE`
      member access (`pFP->...` / `pPipe->...`) except pointer passing to
      `FramePipe*` helper functions.
- [ ] `Source/Utilities/loopback.h` / `.cpp` own all legacy
      `FRAME_PIPE` field access needed by external translation units.
- [ ] `FRAME_PIPE` struct layout is unchanged in this step.
- [ ] `adapter.cpp`, `ioctl.h`, `transport_engine.cpp`, and
      `transport_engine.h` are unchanged.
- [ ] No `LOOPBACK_BUFFER` field or `Loopback*` function is changed.

### What this commit claims (and does not)

- **Claims:** `minwavertstream.cpp` no longer depends on the legacy
  `FRAME_PIPE` field layout, so Step 1 can replace the struct/API
  shape without a cross-TU contradiction.
- **Does NOT claim:** INT32 ring behavior, hard-reject overflow,
  underrun hysteresis, SRC, or audible cable coupling.

## What This Step Does NOT Do

- Does not replace the `FRAME_PIPE` struct.
- Does not implement INT32 write/read paths.
- Does not change `AO_STREAM_RT`.
- Does not flip `phases/1-int32-ring/index.json` Step 0 to
  `completed` until review passes and the commit lands.

## Completion

```powershell
python scripts/execute.py mark 1-int32-ring 0 completed --message "Removed cross-TU FRAME_PIPE field access before struct rewrite."
```

Do not run the `mark` command before review and commit per
`docs/GIT_POLICY.md`.
