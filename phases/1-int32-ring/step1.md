# Phase 1 Step 1: FRAME_PIPE struct/API rewrite

## Read First

- `docs/ADR.md` ADR-003 (INT32 frame-indexed ring) and ADR-005 (hard-
  reject overflow + hysteretic underrun)
- `docs/AO_CABLE_V1_ARCHITECTURE.md` § 6 (cable ring state)
- `docs/AO_CABLE_V1_DESIGN.md` § 2 (FRAME_PIPE detail)
- `results/vbcable_disasm_analysis.md` (VB ring layout reference)
- `Source/Utilities/loopback.h` (current FRAME_PIPE struct)
- `Source/Utilities/loopback.cpp` (current FRAME_PIPE init / reset)

## Goal

Replace the existing ad-hoc `FRAME_PIPE` shape (`RingBuffer / WriteFrame
/ ReadFrame / FillFrames / Pipe* / Speaker*/Mic*` etc.) with the
canonical INT32 frame-indexed shape from `docs/AO_CABLE_V1_DESIGN.md`
§ 2.1, and add the canonical API from § 2.2. **Struct shape and API
surface only — write/read algorithms are Steps 2 and 3.**

`LOOPBACK_BUFFER` is **not** touched in this step; it is a separate
legacy ring living alongside `FRAME_PIPE` and is retired in Phase 6.

The 12 legacy `FramePipe*` functions that external translation units
(`minwavertstream.cpp`, `transport_engine.cpp`) call are kept as a
**compile-preserving shim layer** inside `loopback.h` / `loopback.cpp`
— forward wrappers where the new API has the same semantics, no-op /
zero-return stubs where it does not. This is not architectural drift:
the shim exists only so untouched translation units link, and the new
canonical API is introduced clean. Step 2+ migrates callers off the
shims; Phase 6 cleanup deletes the shim layer.

## Planned Files

Edit only:

- `Source/Utilities/loopback.h` — replace `FRAME_PIPE` struct
  definition with the DESIGN § 2.1 shape; declare the new canonical
  API (DESIGN § 2.2); keep the existing legacy `FramePipe*`
  declarations for the compile-preserving shim layer.
- `Source/Utilities/loopback.cpp` — implement the new canonical API
  (Init/Free/Reset/AvailableFrames real; Read/Write
  `STATUS_NOT_IMPLEMENTED` stubs); rewrite the 12 legacy `FramePipe*`
  function bodies as forward wrappers (where new API has matching
  semantics) or no-op / zero-return stubs (where it does not).
- `Source/Main/adapter.cpp` — `IOCTL_AO_GET_STREAM_STATUS` handler:
  any read of an `AO_V2_DIAG` field that came from a removed
  `FRAME_PIPE` member (e.g. `RenderPumpDriveCount`,
  `RenderLegacyDriveCount`, pump-shadow counters, gated-skip /
  over-jump / frames-processed / pump-invocation /
  pump-shadow-divergence / pump-feature-flags counters per
  cable × {render, capture}) must be replaced with a literal `0`
  until Phase 6 cleanup retires those `AO_V2_DIAG` fields entirely.
  This keeps the IOCTL ABI stable for V1/V2 callers during the
  rewrite. **Do not** delete the `AO_V2_DIAG` fields here; that is
  Phase 6 Step 0. **Do not** touch top-level `AO_STREAM_STATUS`
  fields (Speaker/MicFormat / Speaker/MicActive — those source from
  `LOOPBACK_BUFFER`, untouched in this step). **Do not** touch
  `AO_CONFIG.MaxLatencyMs` — it remains the UI-facing value per
  ADR-011.
- `test_stream_monitor.py` — if any displayed counter now reads as
  literal `0` permanently, mark it `[deprecated, see Phase 6]` so
  operators do not chase ghost values.

Per `docs/REVIEW_POLICY.md` § 7, the three files
(`Source/Main/ioctl.h`, `Source/Main/adapter.cpp`,
`test_stream_monitor.py`) are updated together as one atomic change
when the schema or its sources move. In this step, `ioctl.h` does
not change (the field set stays — just sourced as `0`); the other two
do change.

Do not touch in this step:

- `Source/Main/minwavertstream.cpp`
- `Source/Main/ioctl.h` (no schema change)
- `Source/Utilities/transport_engine.cpp`

## Required Edits

1. In `Source/Utilities/loopback.h`, replace the existing `FRAME_PIPE`
   struct with the shape defined in `docs/AO_CABLE_V1_DESIGN.md` § 2.1:

   ```c
   typedef struct _FRAME_PIPE {
       KSPIN_LOCK  Lock;

       ULONG       InternalRate;
       USHORT      InternalBitsPerSample;  // always 32 (INT32 ring)
       LONG        InternalBlockAlign;

       LONG        TargetLatencyFrames;
       LONG        WrapBound;
       LONG        FrameCapacityMax;
       LONG        Channels;

       LONG        WritePos;
       LONG        ReadPos;

       LONG        OverflowCounter;
       LONG        UnderrunCounter;
       UCHAR       UnderrunFlag;

       /* Persistent SRC state — survives across calls (VB parity). */
       LONG        WriteSrcPhase;
       LONG        WriteSrcResidual[16];
       LONG        ReadSrcPhase;
       LONG        ReadSrcResidual[16];

       LONG*       Data;
       SIZE_T      DataAllocBytes;
   } FRAME_PIPE, *PFRAME_PIPE;
   ```

2. Add public API declarations matching § 2.2 of the design doc.
   Implementation of `AoRingWriteFromScratch` / `AoRingReadToScratch` is
   in Steps 2 / 3 — for Step 1, declare them but stub the body to
   return `STATUS_NOT_IMPLEMENTED`.

3. In `Source/Utilities/loopback.cpp`:
   - `FramePipeInitCable(pipe, internalRate, channels, initialFrames)`
     allocates `Data` as `initialFrames * channels * sizeof(LONG)`
     from non-paged pool, sets `InternalRate = internalRate`,
     `InternalBitsPerSample = 32`, `InternalBlockAlign = 4 * channels`,
     `TargetLatencyFrames = WrapBound = initialFrames`,
     `FrameCapacityMax = max(initialFrames, registry-driven max)`,
     zeros `WritePos/ReadPos/OverflowCounter/UnderrunCounter/
     UnderrunFlag/WriteSrcPhase/WriteSrcResidual/ReadSrcPhase/
     ReadSrcResidual`, initializes the spinlock.
   - `FramePipeFree(pipe)` releases `Data` and zeros the struct.
   - `FramePipeResetCable(pipe)` zeros `WritePos/ReadPos/UnderrunFlag/
     WriteSrcPhase/WriteSrcResidual/ReadSrcPhase/ReadSrcResidual` (SRC
     state belongs to the *current* stream session — Stop/Start must
     start fresh phase). Does **not** zero counters — those persist
     across pause cycles for diagnostics.

4. Rewrite the existing 12 legacy `FramePipe*` function bodies as the
   compile-preserving shim layer:

   - **Forward wrappers** (semantics carry over to the new API):
     - `FramePipeInit(pipe, sampleRate, channels, targetFillFrames)`
       → `FramePipeInitCable(pipe, sampleRate, channels,
       targetFillFrames)`
     - `FramePipeCleanup(pipe)` → `FramePipeFree(pipe)`
     - `FramePipeReset(pipe)` → `FramePipeResetCable(pipe)`
     - `FramePipeGetFillFrames(pipe)` → `AoRingAvailableFrames(pipe)`
   - **Stubs** (behavior intentionally absent; Step 2+ migrates
     callers to the new canonical API instead):
     - `FramePipeWriteFrames(...)` → `return 0;`
     - `FramePipeReadFrames(...)` → `return 0;`
     - `FramePipePrefillSilence(pipe)` → no-op (VOID)
     - `FramePipeRegisterFormat(...)` → no-op (VOID)
     - `FramePipeUnregisterFormat(...)` → no-op (VOID)
     - `FramePipeWriteFromDma(...)` → `return 0;`
     - `FramePipeWriteFromDmaEx(...)` → `return 0;`
     - `FramePipeReadToDma(...)` → no-op (VOID)

   The shim layer is intentional and scoped: it exists only so that
   `minwavertstream.cpp` / `transport_engine.cpp` (untouched in this
   step) link without modification. Step 2+ migrates each caller to
   the new canonical API; Phase 6 cleanup deletes the shim layer.

5. STOP condition: if the build requires touching
   `Source/Main/minwavertstream.cpp`,
   `Source/Utilities/transport_engine.cpp`,
   `Source/Utilities/transport_engine.h`,
   `Source/Main/ioctl.h`, or any `LOOPBACK_BUFFER` field /
   `Loopback*` function — stop and report the exact file / field /
   error before changing anything. Compile-preserving is the contract;
   widening into legacy transport semantics is out of scope.

## Rules

- Tell the user before editing each file.
- Do not delete the existing ring read/write functions yet — they will
  be replaced in Steps 2 / 3.
- Do not modify behavior of `Source/Main/*` in this step.
- If a Phase 5 / pump-flag artifact is in the way of compiling the new
  struct shape (e.g., a field that is now removed), stop and report
  before touching it. It may belong to Phase 6 cleanup.

## Acceptance Criteria

- [ ] `build-verify.ps1 -Config Release` succeeds.
- [ ] `Source/Utilities/loopback.h` contains the new `FRAME_PIPE` struct
      shape exactly as in `docs/AO_CABLE_V1_DESIGN.md` § 2.1.
- [ ] `FramePipeInitCable` / `FramePipeFree` / `FramePipeResetCable` /
      `AoRingAvailableFrames` are real implementations against the new
      struct fields.
- [ ] `AoRingWriteFromScratch` / `AoRingReadToScratch` are declared
      with the signatures in § 2.2 (bodies are stubs returning
      `STATUS_NOT_IMPLEMENTED`).
- [ ] All 12 legacy `FramePipe*` functions still link from their
      external callers (`minwavertstream.cpp`, `transport_engine.cpp`,
      `adapter.cpp`) with **no edits to those translation units**.
      The legacy bodies in `loopback.cpp` are forward wrappers (Init /
      Cleanup / Reset / GetFillFrames) or behavior-absent stubs (the
      other 8) per § 4 of Required Edits.
- [ ] `LOOPBACK_BUFFER` is unchanged. `Loopback*` functions are
      unchanged. Phase 6 owns retiring the legacy ring.
- [ ] `adapter.cpp` AO_V2_DIAG fields whose source was a removed
      `FRAME_PIPE` member read literal `0`. Top-level
      `AO_STREAM_STATUS` Speaker/MicFormat / Speaker/MicActive and
      `AO_CONFIG.MaxLatencyMs` are unchanged.
- [ ] No edits to `minwavertstream.cpp`, `transport_engine.cpp`,
      `transport_engine.h`, `ioctl.h`. If the build forces an edit
      there, the contract is violated and step 1 must stop and report.

### What this commit claims (and does not)

- **Claims:** compile-preserving struct + canonical API rewrite. The
  driver builds and links with the new `FRAME_PIPE` shape and the new
  canonical API in place. Legacy callers continue to link via the
  shim layer.
- **Does NOT claim:** runtime cable-transport correctness. The shim
  stubs (Write/Read/etc.) intentionally do not move audio. Step 2
  (write same-rate) and Step 3 (read same-rate) restore behavior on
  the new canonical API; Step 4-6 migrate callers off the shim.

## What This Step Does NOT Do

- Does not change cable transport behavior (the ring write/read are
  stubs).
- Does not change `AO_STREAM_RT` field set.
- Does not flip `phases/1-int32-ring/index.json` Step 1 to `completed`
  until review passes and the commit lands.

## Completion

```powershell
python scripts/execute.py mark 1-int32-ring 1 completed --message "FRAME_PIPE struct shape replaced (INT32, frame-indexed)."
```

Do not run the `mark` command before review and commit per `docs/GIT_POLICY.md`.
