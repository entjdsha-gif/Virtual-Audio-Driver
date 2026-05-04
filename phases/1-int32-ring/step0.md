# Phase 1 Step 0: FRAME_PIPE struct rewrite

## Read First

- `docs/ADR.md` ADR-003 (INT32 frame-indexed ring) and ADR-005 (hard-
  reject overflow + hysteretic underrun)
- `docs/AO_CABLE_V1_ARCHITECTURE.md` § 6 (cable ring state)
- `docs/AO_CABLE_V1_DESIGN.md` § 2 (FRAME_PIPE detail)
- `results/vbcable_disasm_analysis.md` (VB ring layout reference)
- `Source/Utilities/loopback.h` (current FRAME_PIPE struct)
- `Source/Utilities/loopback.cpp` (current FRAME_PIPE init / reset)

## Goal

Replace the existing packed-24-bit byte-indexed `FRAME_PIPE` struct with
an INT32 frame-indexed shape that matches `docs/AO_CABLE_V1_DESIGN.md`
§ 2.1. **Struct shape only — read/write algorithms are Steps 1 and 2.**

## Planned Files

Edit only:

- `Source/Utilities/loopback.h` — replace `FRAME_PIPE` struct definition.
- `Source/Utilities/loopback.cpp` — update `FramePipeInitCable`,
  `FramePipeFree`, `FramePipeResetCable` to allocate / free / zero the
  new struct shape. Existing ring read/write entry points remain wired
  to the old code path for now (Step 1 swaps them).

Do not touch in this step:

- `Source/Main/minwavertstream.cpp`
- `Source/Main/adapter.cpp`
- `Source/Utilities/transport_engine.cpp`

## Required Edits

1. In `Source/Utilities/loopback.h`, replace the existing `FRAME_PIPE`
   struct with the shape defined in `docs/AO_CABLE_V1_DESIGN.md` § 2.1:

   ```c
   typedef struct _FRAME_PIPE {
       KSPIN_LOCK  Lock;

       LONG        TargetLatencyFrames;
       LONG        WrapBound;
       LONG        FrameCapacityMax;
       LONG        Channels;

       LONG        WritePos;
       LONG        ReadPos;

       LONG        OverflowCounter;
       LONG        UnderrunCounter;
       UCHAR       UnderrunFlag;

       LONG*       Data;
       SIZE_T      DataAllocBytes;
   } FRAME_PIPE, *PFRAME_PIPE;
   ```

2. Add public API declarations matching § 2.2 of the design doc.
   Implementation of `AoRingWriteFromScratch` / `AoRingReadToScratch` is
   in Steps 1 / 2 — for Step 0, declare them but stub the body to
   return `STATUS_NOT_IMPLEMENTED`.

3. In `Source/Utilities/loopback.cpp`:
   - `FramePipeInitCable(pipe, initialFrames, channels)` allocates
     `Data` as `initialFrames * channels * sizeof(LONG)` from non-paged
     pool, sets `TargetLatencyFrames = WrapBound = initialFrames`,
     `FrameCapacityMax = max(initialFrames, registry-driven max)`,
     zeros `WritePos/ReadPos/OverflowCounter/UnderrunCounter/
     UnderrunFlag`, initializes the spinlock.
   - `FramePipeFree(pipe)` releases `Data` and zeros the struct.
   - `FramePipeResetCable(pipe)` zeros `WritePos/ReadPos/UnderrunFlag`
     (does **not** zero counters — those persist across pause cycles for
     diagnostics).

4. Confirm existing callers in `Source/Utilities/loopback.cpp` (any
   helper that touches the old packed-24 fields) compile with the new
   struct shape, even if their behavior is now incorrect — they will be
   fixed in Steps 1 / 2.

## Rules

- Tell the user before editing each file.
- Do not delete the existing ring read/write functions yet — they will
  be replaced in Steps 1 / 2.
- Do not modify behavior of `Source/Main/*` in this step.
- If a Phase 5 / pump-flag artifact is in the way of compiling the new
  struct shape (e.g., a field that is now removed), stop and report
  before touching it. It may belong to Phase 6 cleanup.

## Acceptance Criteria

- [ ] `build-verify.ps1 -Config Release` succeeds.
- [ ] `Source/Utilities/loopback.h` contains the new `FRAME_PIPE` struct
      shape exactly as in `docs/AO_CABLE_V1_DESIGN.md` § 2.1.
- [ ] `FramePipeInitCable` / `FramePipeFree` / `FramePipeResetCable`
      use the new fields correctly.
- [ ] `AoRingWriteFromScratch` / `AoRingReadToScratch` are declared
      with the signatures in § 2.2 (bodies are stubs returning
      `STATUS_NOT_IMPLEMENTED`).
- [ ] Existing transport callers (legacy `LoopbackWrite` /
      `LoopbackRead` if still present) compile, even if they no longer
      function correctly. Their failure to function is expected and is
      fixed in Steps 1 / 2.

## What This Step Does NOT Do

- Does not change cable transport behavior (the ring write/read are
  stubs).
- Does not change `AO_STREAM_RT` field set.
- Does not flip `phases/1-int32-ring/index.json` Step 0 to `completed`
  until review passes and the commit lands.

## Completion

```powershell
python scripts/execute.py mark 1-int32-ring 0 completed --message "FRAME_PIPE struct shape replaced (INT32, frame-indexed)."
```

Do not run the `mark` command before review and commit per `docs/GIT_POLICY.md`.
