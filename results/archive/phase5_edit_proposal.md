# Phase 5 - Edit Proposal (pre-implementation)

Date: 2026-04-14
Author: Claude
Status: PROPOSAL - pending lock before edit
Scope: Phase 5 of the `feature/ao-fixed-pipe-rewrite` parity-first rewrite.

## Approval record

Locked 2026-04-14 after user/Claude review of initial proposal draft.

Decisions:
- **§4.4 Runtime rollback knob** = **Option B (new `SET_PUMP_FEATURE_FLAGS`
  IOCTL)**. The user rejected Option A (registry + STOP->RUN cycle) as too
  weak for Codex exit criterion #5's literal "within one or two position
  queries" requirement. The new IOCTL is write-only, Phase 5 restricts
  accepted mask bits to `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` only, not
  persistent, not UI-exposed.
- **§4.5 One-owner verification** = **direct owner-side evidence via new
  per-side drive counters**. The user rejected "structural + rollback
  smoke test" alone as insufficient. Phase 5 must add minimal
  instrumentation so one-owner proof becomes a counter assertion, not a
  review argument: `RenderPumpDriveCount` grows only when the pump
  transport block fires, `RenderLegacyDriveCount` grows only when the
  `ReadBytes()` cable render path fires. Pump-owned state proves pump
  counter grows and legacy counter freezes; rollback toggle proves
  grow-stop crossover within 1-2 position queries.
- **§4.1 Helper transport block position** = **between step 9 (accepted
  frame accounting) and step 10 (rolling-window shadow compare)**, with
  cable-pipe resolution hoisted out of the Mirror block to just after
  the cable-only gate.
- **Phase 3 divergence must stay green** across the entire Phase 5
  validation pass. Any new divergence tick blocks commit.
- **Rollback smoke test is mandatory** in the same closure commit (not
  deferred).

Scope expansion granted for §4.4 + §4.5:
- new IOCTL `IOCTL_AO_SET_PUMP_FEATURE_FLAGS`,
- two new `FRAME_PIPE` fields per cable (`RenderPumpDriveCount`,
  `RenderLegacyDriveCount`),
- V2 diag struct extension at the tail to expose the new counters,
- `test_stream_monitor.py` parser update for the new tail,
- a new rollback smoke test script `tests/phase5_rollback.py`.

Parity-first rationale for scope expansion:
- This is rollout/rollback control infrastructure, not a product feature
  addition. It does not introduce new user-visible functionality.
- The IOCTL is Phase 5's safety net; without it, Codex exit criterion #5
  cannot be proved.
- The counters are Phase 5's proof obligation; without them, Codex exit
  criterion #4 falls back to a review argument.

Any deviation from the above requires a new proposal revision before edit.

This document is intended to:
- reload the authoritative Codex Phase 5 scope before any edits,
- reconcile current render-path source state (after Phase 3 + Phase 4) against
  both the Codex and Claude plans,
- strictly isolate render ownership move from capture ownership move,
- lock the dual-gate pattern and rollback mechanism before code edit,
- decide the one-owner verification method given the current debug
  infrastructure constraints.

Plan sources (operating-rules order):
- Codex baseline: `docs/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CODEX.md`, Phase 5
  (lines 920-947) and Checkpoint D (lines 1072-1083)
- Claude execution notebook:
  `results/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CLAUDE.md`, Phase 5
  (lines 2191-2352)
- Runtime baseline: Phase 4 closed at commit `439bbcd`
- Operating rules: `docs/VB_CABLE_DUAL_PLAN_OPERATING_RULES.md`
- Phase 3 proposal (reference for shadow helper body):
  `results/phase3_edit_proposal.md`
- Phase 4 proposal (reference for SetState/destructor structure):
  `results/phase4_edit_proposal.md`

---

## 1. Scope and non-goals

### 1.1 Goal

Move cable **render** transport ownership from the legacy
`UpdatePosition() -> ReadBytes() -> FramePipeWriteFromDma()` path to the Phase 3
query-driven pump helper. Cable **capture** transport stays on the legacy
`UpdatePosition() -> WriteBytes() -> FramePipeReadToDma()` path in this phase.

Ownership move is controlled at RUN time by a new use of the existing
`AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` bit (Phase 1 already defined it). The bit
is set on cable speaker RUN in this phase. The helper body adds a render
transport block gated on that bit. `UpdatePosition()`'s existing render call
site adds a symmetric gate so legacy transport does not fire when the pump
owns render.

### 1.2 In scope

- New IOCTL `IOCTL_AO_SET_PUMP_FEATURE_FLAGS`, write-only, accepts a
  single-DWORD input interpreted as a mask-constrained update. Phase 5
  restricts accepted mask bits to `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER`
  only; all other bits are rejected.
- Two new `FRAME_PIPE` fields per cable:
  - `volatile ULONG RenderPumpDriveCount` — incremented once per pump
    helper invocation that actually executes the render transport block
    (not per DMA chunk).
  - `volatile ULONG RenderLegacyDriveCount` — incremented once per
    `ReadBytes()` cable render invocation (not per DMA chunk).
- V2 diag struct extension at the tail: add 4 ULONGs (2 cables x 2
  counters) so the rollback smoke test and regression scripts can read
  direct owner-side evidence via the existing V1 IOCTL path.
  `V2_DIAG_SIZE` bumps from 116 to 132.
- Extend `PumpToCurrentPositionFromQuery()` with a render transport block that:
  - runs only for cable **speaker** endpoints (`!m_bCapture` + cable device type),
  - runs only when `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` is set,
  - computes `bytesToMove = newFrames * nBlockAlign`,
  - uses a pump-owned buffer offset (`m_ulPumpLastBufferOffset`) that is
    synced to the current WaveRT view on first call after RUN, then
    advanced independently,
  - drives `FramePipeWriteFromDma()` in wrap-safe chunks until `bytesToMove`
    is consumed,
  - does NOT touch `m_ullLinearPosition`, `m_ullWritePosition`,
    `m_ullPresentationPosition`, `m_ullDmaTimeStamp`, or any other WaveRT
    contract field.
- Modify `UpdatePosition()` render branch so the existing cable
  `ReadBytes(ByteDisplacement)` call is gated `!pumpOwnsRender`. Non-cable
  render continues through `ReadBytes()` → `SaveData.WriteData()` exactly as
  before.
- Set `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` on cable speaker RUN, alongside the
  Phase 3 `AO_PUMP_FLAG_ENABLE | AO_PUMP_FLAG_SHADOW_ONLY` arming.
- Preserve Phase 3 shadow compare semantics. See §1.7 for what "shadow" means
  once legacy transport is disowned.
- Clear the render ownership flag on PAUSE (from RUN) and on STOP, alongside
  the existing Phase 4 pump state clears.
- Add a changelog entry documenting the Phase 5 render ownership landing.

### 1.3 Out of scope

- **Capture ownership move**. `AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE` stays clear.
  `UpdatePosition()`'s `WriteBytes()` call site is not touched. The pump helper
  body gains no capture transport block in this phase. This is strictly
  deferred to Phase 6.
- No transport math changes to `WriteBytes()` / `ReadBytes()` themselves
  (Phase 5 adds a gate at the call site; the bodies are unchanged).
- No `UpdatePosition()` byte-math changes (`ByteDisplacement` is still
  computed the same way and still advances WaveRT contract fields).
- No timer path redesign.
- No Phase 3 helper rewiring (hook target, gate, over-jump, shadow compare).
- No new `FRAME_PIPE` fields.
- No `test_stream_monitor.py` schema change **except** the V2 diag tail
  extension for the new render-side drive counters (see §1.2). The parser
  change is additive: old consumers that read only the first 116 bytes
  keep working.
- No further IOCTL additions beyond `IOCTL_AO_SET_PUMP_FEATURE_FLAGS`.
  The new IOCTL is write-only, mask-constrained to
  `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` in Phase 5, not persistent, not
  UI-exposed.
- No fade-in extraction.
- No Phase 7 timer role narrowing.

### 1.4 Non-negotiable guardrails

- Render ownership move is strictly one direction in this phase: cable speaker
  only. `AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE` must remain clear.
- `UpdatePosition()` still owns `m_ullLinearPosition`, `m_ullWritePosition`,
  `m_ullPresentationPosition`, `m_ullDmaTimeStamp`, and the
  `m_ulBlockAlignCarryForward` / `m_byteDisplacementCarryForward` /
  `m_hnsElapsedTimeCarryForward` state machinery.
- The pump render block must not double-advance transport: either the pump
  path runs OR the legacy path runs, never both.
- Overflow handling stays reject, not overwrite. `FramePipeWriteFromDma()`
  already returns 0 on overflow (entire batch rejected) — we accept that
  behavior and count it as a `DropCount` in the pipe counters.
- `KeFlushQueuedDpcs()` ordering in PAUSE stays intact.
- Destructor teardown order stays intact (Phase 4 established it).
- Phase 4 STOP behavior stays intact: STOP no longer calls
  `FramePipeUnregisterFormat()`.
- No rollback mechanism may require rebuild/reinstall. Runtime rollback knob
  is required (see §4.5 for the decision).

### 1.5 Exit criteria

Authoritative Codex Phase 5 exit criteria, verbatim:

1. Render-side data progression works while query path is active.
2. No byte-ring semantics reintroduced.
3. No direct old-path overwrite behavior remains.
4. One-owner confirmation that render transport fires from exactly one owner
   on the query path.
5. Runtime rollback smoke test passes:
   - clearing the render-ownership flag returns render transport to the legacy
     path within one or two position queries.

Operationally, this also means:

- build green,
- install upgrade green,
- `test_ioctl_diag.py` still green (V1 path),
- idle monitor still sane,
- Phase 3 A-step regression green (no new shadow divergence),
- Phase 3 B-step regression green (real Phone Link call, no new shadow
  divergence, render audio audible on receiver),
- `FrameWriteCount` (pipe write total) should continue to grow during a live
  render scenario,
- `DropCount` should stay at 0 under normal live-call load.

### 1.6 Dual-gate pattern (the rollback safety net)

Phase 5 is built around a symmetric pair of gates on a single feature flag:

- **pump transport block** (new, inside `PumpToCurrentPositionFromQuery`):
  runs iff `pumpOwnsRender == TRUE`.
- **legacy transport call** (existing, inside `UpdatePosition`):
  runs iff `pumpOwnsRender == FALSE` (for cable render streams).

Both gates read the same `m_ulPumpFeatureFlags & AO_PUMP_FLAG_DISABLE_LEGACY_RENDER`
bit. They are therefore mutually exclusive by construction. Clearing the bit
at runtime immediately returns render transport to the legacy `ReadBytes`
path on the next `UpdatePosition()` call. Setting the bit at runtime
immediately moves transport to the pump on the next pump helper call.

This pattern eliminates the common "two owners for a moment" failure mode
seen in naive phase migrations and also gives us the runtime rollback
mechanism required by exit criterion #5.

### 1.7 Phase 3 shadow semantics after Phase 5 render ownership

Once `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` is set on a cable render stream:

- `UpdatePosition()` still computes `ByteDisplacement` and still stashes it
  into `m_ulLastUpdatePositionByteDisplacement` via the Phase 3 Revision 2
  `+=` accumulate, but that stash now represents "what legacy transport
  **would have** processed if it still owned the path".
- The pump helper still reads the stash, still runs the rolling-window
  compare against its own frame math, and still publishes
  `PumpShadowDivergenceCount`.
- **What the divergence counter means now**: it is no longer "live parity
  proof against the active transport owner" (because legacy render transport
  no longer runs). It is "parity proof that the pump's frame-delta math still
  agrees with `UpdatePosition()`'s byte-delta math" — i.e., it continues to
  validate the pump's internal accounting against `UpdatePosition()`'s
  wall-clock accounting. This is still useful as a drift detector.
- If divergence does begin to tick after Phase 5, that is evidence that
  either (a) the pump transport block is somehow feeding back into
  `UpdatePosition()`'s math, or (b) a clock/carry-forward corner case is
  appearing. Either case is a Phase 5 red flag and should block commit.

### 1.8 Idle expectations

Idle behavior after Phase 5 lands should match Phase 3/4 idle behavior:

- `PumpInvocationCount == 0`
- `GatedSkipCount == 0`
- `OverJumpCount == 0`
- `PumpShadowDivergenceCount == 0`
- `FramesProcessedTotal == 0`
- `PumpFeatureFlags == 0x00000000`
- `DropCount == 0` (pipe-side)
- `FrameWriteCount == 0` (pipe-side)

---

## 2. Plan versus current code drift

### 2.1 Current render transport path (post-Phase-4)

Post-Phase-4 cable render transport is:

1. `GetPositions()` or `TimerNotifyRT` acquires `m_PositionSpinLock` and
   calls `UpdatePosition(ilQPC)`.
2. `UpdatePosition()` computes `ByteDisplacement` from wall-clock math
   (with three carry-forward fields), applies EOS clamp for render,
   updates `m_ullPresentationPosition`, calls `ReadBytes(ByteDisplacement)`
   for the cable render branch, then updates `m_ullPlayPosition`,
   `m_ullWritePosition`, `m_ullLinearPosition`, and `m_ullDmaTimeStamp`.
3. `ReadBytes()` at [minwavertstream.cpp:2086] computes
   `bufferOffset = m_ullLinearPosition % m_ulDmaBufferSize`, then loops in
   wrap-safe chunks calling `FramePipeWriteFromDma(pPipe, dmaBuffer + offset, runWrite)`
   until `ByteDisplacement` is consumed.
4. For render, after `ReadBytes()` returns, `UpdatePosition()` advances the
   WaveRT contract fields to catch up.

The Phase 3 pump helper runs right after `UpdatePosition()` in the
same-call, same-spinlock pattern, but currently only counts and compares.
It has no transport call.

### 2.2 `PumpToCurrentPositionFromQuery` shape (post-Phase-3 Revision 2)

Current helper structure:

1. master gate (`AO_PUMP_FLAG_ENABLE`),
2. cable-only gate,
3. format sanity,
4. invocation count++,
5. first-call latch (goto Mirror),
6. elapsed frame math from baseline,
7. 8-frame gate (Phase 3),
8. over-jump diagnostic (Phase 3 Revision 1, count-only),
9. accepted-frame accounting (`m_ulPumpProcessedFrames += newFrames`,
   `m_ullPumpFramesProcessed += newFrames`),
10. rolling-window shadow compare (Phase 3 Revision 2 with `+=` stash
    semantics),
11. cleanup (`m_ulLastUpdatePositionByteDisplacement = 0`),
12. Mirror label (snapshot counters into pipe).

Phase 5 inserts a **new step between 9 and 10**: if
`pumpOwnsRender && !m_bCapture && cable && m_pDmaBuffer && m_ulDmaBufferSize > 0`,
call `FramePipeWriteFromDma` in wrap-safe chunks using
`m_ulPumpLastBufferOffset`. Shadow compare (step 10) still runs after the
transport call, with the understanding from §1.7 that divergence is now
validating internal math rather than external legacy transport.

Decision point: insertion order matters. The Claude plan inserts transport
**before** `m_ullPumpFramesProcessed += newFrames`. We follow that order
because `m_ullPumpFramesProcessed` is the monotonic evidence counter that
should increment on every accepted call regardless of whether a transport
side-effect fires.

### 2.3 `UpdatePosition()` render branch (current state)

`UpdatePosition()` render branch at [minwavertstream.cpp:1900-1907]:

```cpp
{
    CMiniportWaveRT* pMp = m_pMiniport;
    BOOL isCable = (pMp && (pMp->m_DeviceType == eCableASpeaker ||
                             pMp->m_DeviceType == eCableBSpeaker));
    if (isCable || !g_DoNotCreateDataFiles)
    {
        ReadBytes(ByteDisplacement);
    }
}
```

Phase 5 changes the gate so cable render skips `ReadBytes` when
`pumpOwnsRender`:

```cpp
{
    CMiniportWaveRT* pMp = m_pMiniport;
    BOOL isCable = (pMp && (pMp->m_DeviceType == eCableASpeaker ||
                             pMp->m_DeviceType == eCableBSpeaker));
    BOOL pumpOwnsRender =
        isCable &&
        (m_ulPumpFeatureFlags & AO_PUMP_FLAG_DISABLE_LEGACY_RENDER);
    if ((isCable && !pumpOwnsRender) || !g_DoNotCreateDataFiles)
    {
        ReadBytes(ByteDisplacement);
    }
}
```

This preserves:
- non-cable render → `ReadBytes` when savedata is enabled,
- cable render with pump disabled → `ReadBytes` (rollback path),
- cable render with pump enabled → `ReadBytes` skipped, pump owns transport.

### 2.4 SetState RUN arm for cable speakers

Phase 3 RUN arm currently sets only `ENABLE | SHADOW_ONLY` for all cable
endpoints. Phase 5 additionally sets `DISABLE_LEGACY_RENDER` for cable
**speaker** only:

```cpp
ULONG newFlags = AO_PUMP_FLAG_ENABLE | AO_PUMP_FLAG_SHADOW_ONLY;
if (!m_bCapture &&
    (m_pMiniport->m_DeviceType == eCableASpeaker ||
     m_pMiniport->m_DeviceType == eCableBSpeaker))
{
    newFlags |= AO_PUMP_FLAG_DISABLE_LEGACY_RENDER;
}
m_ulPumpFeatureFlags = newFlags;
```

Capture streams (`m_bCapture == TRUE`) never get the render flag. Phase 5
does not touch `AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE` at all.

### 2.5 SetState PAUSE and STOP pump clear

Both Phase 4 PAUSE clear and Phase 3 STOP clear already zero
`m_ulPumpFeatureFlags`. Nothing to change — clearing all flag bits
automatically clears the render ownership bit, so PAUSE and STOP naturally
return to legacy render transport for the next RUN (until RUN rearms the
flag again per §2.4).

### 2.6 `m_ulPumpLastBufferOffset` field

Phase 1 already added this field and Phase 3/4 zero it on RUN arm, STOP clear,
and PAUSE clear. Phase 5 is its first read/write site in the helper body.
The field is `ULONG` and lives on `CMiniportWaveRTStream`.

First-call sync: if `m_ulPumpLastBufferOffset == 0` at the time of the first
accepted accounting call AND `m_ullLinearPosition > 0`, we interpret that
as "stream has been running for a tick or two before the pump's first
accepted call; catch up to the current WaveRT view". Subsequent calls
advance the pump's own offset independently.

### 2.7 FramePipeWriteFromDma signature and behavior

From `Source/Utilities/loopback.h`:

```cpp
ULONG FramePipeWriteFromDma(
    PFRAME_PIPE     pPipe,
    const BYTE*     dmaData,
    ULONG           byteCount
);
// Returns frames written (0 = overflow, entire batch rejected)
```

The helper must be called at DISPATCH_LEVEL. The Phase 3 pump helper runs
inside `GetPositions()` under `m_PositionSpinLock`, and spinlock acquisition
raises IRQL to DISPATCH_LEVEL — so we are already at DISPATCH_LEVEL inside
the pump helper body. The call site is therefore valid.

Overflow handling: return value of 0 means the pipe rejected the entire
batch. In Phase 5 we do NOT retry the write and we do NOT advance
`m_ulPumpLastBufferOffset` on a rejected write — we simply accept the drop
(the pipe's internal `DropCount` will increment) and move on. This matches
the Codex exit criterion "keep overflow as reject, not overwrite".

### 2.8 Current code has no double-owner risk because ReadBytes remains the single cable render driver

Post-Phase-4, `FramePipeWriteFromDma()` is called from exactly one site for
cable render: `ReadBytes()` at [minwavertstream.cpp:2127]. There is no other
caller. Phase 5 adds a second call site (the pump helper body). The gates
in §2.3 + §2.4 ensure that exactly one of the two call sites is active at
any given moment for any given cable render stream.

---

## 3. File-by-file edit plan

Expected code touch set (after §4.4/§4.5 scope expansion):

1. `Source/Utilities/loopback.h` — add 2 new `FRAME_PIPE` fields.
2. `Source/Utilities/loopback.cpp` — zero new fields in `FramePipeInit()`.
3. `Source/Main/ioctl.h` — new `IOCTL_AO_SET_PUMP_FEATURE_FLAGS` code,
   new `AO_PUMP_FLAGS_REQ` input struct, extended `AO_V2_DIAG` with 4
   tail ULONGs, `V2_DIAG_SIZE` bumped to 132.
4. `Source/Main/minwavert.h` — new `CMiniportWaveRT::m_pActiveRenderStream`
   pointer and `m_ActiveStreamLock` spinlock.
5. `Source/Main/minwavert.cpp` — initialize the new members; provide
   `RegisterActiveRenderStream(stream)` / `UnregisterActiveRenderStream(stream)`
   helpers that do the lock-and-check-identity protocol from §4.4.
6. `Source/Main/minwavertstream.cpp` —
   - pump helper body: hoist `pFP` resolution, insert render transport
     block between step 9 and step 10, increment
     `pFP->RenderPumpDriveCount` at the end of a successful transport
     execution,
   - `UpdatePosition()` render branch: gate existing `ReadBytes()` on
     `!pumpOwnsRender`,
   - `ReadBytes()` cable branch: increment `pFP->RenderLegacyDriveCount`
     once per invocation that enters the pipe branch,
   - `SetState(KSSTATE_RUN)` cable speaker arm: add
     `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER`, call
     `RegisterActiveRenderStream(this)`,
   - `SetState(KSSTATE_PAUSE)` cable speaker from RUN: call
     `UnregisterActiveRenderStream(this)` **before** the existing pump
     working-state clear,
   - `SetState(KSSTATE_STOP)` cable speaker: call
     `UnregisterActiveRenderStream(this)` inside the existing Phase 3
     STOP clear block,
   - destructor: call `UnregisterActiveRenderStream(this)` unconditionally
     before the Phase 4 cable unregister, so a stream that skipped
     STOP/PAUSE still drops its registration.
7. `Source/Main/adapter.cpp` —
   - V2 diag fill: expose the 4 new tail ULONGs,
   - IOCTL dispatch: handle `IOCTL_AO_SET_PUMP_FEATURE_FLAGS` with
     input validation, mask constraint, active-stream lookup, and the
     double-lock write protocol (§4.4).
8. `test_stream_monitor.py` — parse the new 132-byte tail, display the
   new counters. Backward-compatible: if the driver returns only 116
   bytes, the new counters show as "-".
9. `tests/phase3_shadow_active.py` — parse the new tail and make sure
   the Phase 3 regression still works unchanged.
10. `tests/phase3_live_call_shadow.py` — parse the new tail.
11. `tests/phase5_rollback.py` — NEW. Drives the rollback IOCTL during an
    active cable speaker stream and asserts the one-owner counter
    signature from §4.5.
12. `docs/PIPELINE_V2_CHANGELOG.md` — Phase 5 entry.

No `loopback.cpp` transport math changes beyond zeroing the new fields in
`FramePipeInit()`. `FramePipeReset()` does NOT touch the new fields
(they are monotonic evidence counters, like the Phase 1 pump counters).

### 3.1 `Source/Main/minwavertstream.cpp` - `PumpToCurrentPositionFromQuery`

Anchor:
- helper body is at the Phase 3 location, just before `UpdatePosition()`.

Required changes:

1. Add a transport block between the current "9. Accepted-frame accounting"
   step and the current "10. Rolling-window shadow compare" step.
2. The block's precondition is:
   - `!m_bCapture`,
   - `m_pMiniport->m_DeviceType` is `eCableASpeaker` or `eCableBSpeaker`,
   - `m_ulPumpFeatureFlags & AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` is set,
   - `pFP != NULL` and `pFP->Initialized == TRUE`,
   - `m_pDmaBuffer != NULL` and `m_ulDmaBufferSize > 0`,
   - `newFrames > 0`,
   - `nBlockAlign > 0`.
3. Compute `bytesToMove = newFrames * nBlockAlign` in ULONG space.
4. First-call sync: if `m_ulPumpLastBufferOffset == 0` and
   `m_ullLinearPosition > 0`, set
   `m_ulPumpLastBufferOffset = (ULONG)(m_ullLinearPosition % m_ulDmaBufferSize)`.
5. Wrap-safe loop:
   - `bufferOffset = m_ulPumpLastBufferOffset`,
   - `remaining = bytesToMove`,
   - while `remaining > 0`:
     - `chunk = min(remaining, m_ulDmaBufferSize - bufferOffset)`,
     - `FramePipeWriteFromDma(pFP, m_pDmaBuffer + bufferOffset, chunk)`
       (return value intentionally unused; pipe owns drop counters),
     - `bufferOffset = (bufferOffset + chunk) % m_ulDmaBufferSize`,
     - `remaining -= chunk`.
6. Save the new offset: `m_ulPumpLastBufferOffset = bufferOffset`.
7. Do NOT touch `m_ullLinearPosition`, `m_ullWritePosition`,
   `m_ullPresentationPosition`, `m_ullDmaTimeStamp`.
8. The existing rolling-window shadow compare (step 10 of the Phase 3
   helper) runs after the transport block exactly as today. No changes to
   shadow compare logic in this phase.

Resolving `pFP`: the helper already resolves the cable pipe in the Mirror
block at the end of the function. The transport block needs `pFP` earlier,
so we hoist the cable-pipe lookup out of Mirror to just after the
cable-only gate (step 2). The Mirror block then reuses the hoisted `pFP`
variable rather than recomputing it.

### 3.2 `Source/Main/minwavertstream.cpp` - `UpdatePosition()` render branch

Anchor:
- `UpdatePosition()` render branch at
  [minwavertstream.cpp:1900-1907] (post-Phase-4 line numbering).

Required change:
- as shown in §2.3: add `pumpOwnsRender` computation and use it to gate the
  `ReadBytes(ByteDisplacement)` call on the cable side.

Non-cable render behavior (the `!g_DoNotCreateDataFiles` path) is unchanged.

### 3.3 `Source/Main/minwavertstream.cpp` - `SetState(KSSTATE_RUN)` flag arming

Anchor:
- Phase 3 RUN arm block (the part that computes `newFlags` and assigns
  `m_ulPumpFeatureFlags = newFlags`).

Required change:
- add the render ownership bit for cable speaker endpoints, as shown in §2.4.

### 3.4 `Source/Main/minwavertstream.cpp` - explicitly no capture-side edits

Do not touch in Phase 5:

- `UpdatePosition()`'s `WriteBytes()` call site for capture,
- `WriteBytes()` body,
- `AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE`,
- any `m_bCapture == TRUE` path in the pump helper body.

If any edit in Phase 5 needs to touch any of the above, it has escaped into
Phase 6 and should stop.

### 3.5 `docs/PIPELINE_V2_CHANGELOG.md`

Add Phase 5 entry recording:
- new `FramePipeWriteFromDma` call site in the pump helper body (cable
  speaker only, gated on `DISABLE_LEGACY_RENDER`),
- `UpdatePosition()` render branch now gates `ReadBytes` on
  `!pumpOwnsRender` for cable streams,
- `SetState(KSSTATE_RUN)` now arms `DISABLE_LEGACY_RENDER` for cable
  speaker endpoints,
- dual-gate pattern documentation,
- note on Phase 3 shadow semantics shift (drift detector, not live parity
  proof),
- rollback mechanism documentation (see §4.5 decision).

---

## 4. Recommended defaults to lock before editing

### 4.1 Transport block position in the helper body

Recommended default:
- between "9. Accepted-frame accounting" and "10. Rolling-window shadow
  compare".

Reason:
- keeps `m_ullPumpFramesProcessed` as the single source of truth for
  accepted frames,
- runs transport right after the frame math that drove the decision,
- shadow compare continues to see both the pump's frame math and the
  stashed legacy bytes, so the drift detector (§1.7) still works.

### 4.2 First-call buffer-offset sync rule

Recommended default:
- if `m_ulPumpLastBufferOffset == 0` and `m_ullLinearPosition > 0` at the
  first call that passes the accepted-frame gate, set
  `m_ulPumpLastBufferOffset = (ULONG)(m_ullLinearPosition % m_ulDmaBufferSize)`.

Reason:
- guarantees the pump's first render write lands at the same DMA offset
  the legacy path would have written to,
- avoids a phase-shift discontinuity at Phase 5 bring-up,
- Claude plan already documents this as R9/OQ7 mitigation.

### 4.3 Overflow behavior

Recommended default:
- call `FramePipeWriteFromDma` unconditionally on the wrap-safe chunk,
- ignore the return value,
- let the pipe's internal `DropCount` track overflow,
- do NOT retry,
- do advance `m_ulPumpLastBufferOffset` by the chunk size regardless of
  whether the write succeeded (so the pump stays in sync with wall-clock,
  even if a write was dropped).

Reason:
- matches Codex's "overflow as reject, not overwrite",
- keeps the pump's offset monotonically advancing with wall-clock time,
- avoids introducing any retry/backpressure loop at DISPATCH_LEVEL,
- any drop will be visible via pipe `DropCount` and via the Phase 3
  shadow divergence counter if it grows.

### 4.4 Runtime rollback knob — LOCKED as Option B

**Locked decision (approval record): Option B — new IOCTL
`IOCTL_AO_SET_PUMP_FEATURE_FLAGS`, write-only, mask-constrained.**

The user rejected Option A (registry + STOP->RUN cycle) as insufficient
for Codex exit criterion #5's literal "within one or two position queries"
requirement. The runtime rollback knob must be able to flip the flag on a
running stream and have the very next pump helper call route differently.

#### IOCTL specification

Code: `IOCTL_AO_SET_PUMP_FEATURE_FLAGS = CTL_CODE(0x805, FILE_WRITE_ACCESS)`

Method: `METHOD_BUFFERED`, single input buffer, no output.

Input layout (8 bytes):

```cpp
typedef struct _AO_PUMP_FLAGS_REQ {
    ULONG   SetMask;    // bits to set
    ULONG   ClearMask;  // bits to clear
} AO_PUMP_FLAGS_REQ;
```

Handler behavior (inside `adapter.cpp` IOCTL dispatch):

1. Validate `InputBufferLength >= sizeof(AO_PUMP_FLAGS_REQ)`.
2. Mask-constrain both `SetMask` and `ClearMask` to
   `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` only. Bits outside this mask are
   silently dropped in Phase 5. (Phase 6 will expand the allowed mask to
   include `AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE`.)
3. Find the active cable speaker stream instance on this miniport via a
   new `CMiniportWaveRT::m_pActiveRenderStream` pointer (set on cable
   speaker RUN, cleared on PAUSE/STOP). Must be protected by a
   miniport-level spinlock because RUN/STOP transitions and the IOCTL
   handler can both touch it.
4. If there is no active render stream, the IOCTL succeeds but has no
   visible effect. Return `STATUS_SUCCESS`.
5. If there is an active render stream:
   - acquire that stream's `m_PositionSpinLock`,
   - apply `m_ulPumpFeatureFlags |= SetMask`,
   - then apply `m_ulPumpFeatureFlags &= ~ClearMask`,
   - release `m_PositionSpinLock`,
   - return `STATUS_SUCCESS`.

Effect latency:
- The next call to `PumpToCurrentPositionFromQuery()` (triggered by the
  next `GetPositions()` from Windows audio engine, roughly 100 ms under
  WASAPI exclusive, roughly 42 ms under Phone Link) reads the updated
  flag and routes transport to whichever path is now enabled.
- The next call to `UpdatePosition()` (same-call or timer tick, whichever
  comes first) reads the updated flag at its render branch and either
  calls or skips `ReadBytes()` accordingly.
- Both gates therefore flip within 1-2 position queries, matching Codex
  exit criterion #5 literally.

Scope restrictions on the IOCTL (Phase 5):
- **Write-only.** No output buffer, no read-back of current flags.
- **Mask-constrained to render ownership bit only.** `AO_PUMP_FLAG_ENABLE`,
  `AO_PUMP_FLAG_SHADOW_ONLY`, `AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE` are
  NOT writable via this IOCTL in Phase 5.
- **Not persistent.** Values are not written to registry and do not
  survive STOP->RUN cycles. Each RUN re-reads the default (cable speaker
  RUN always arms the render bit). The IOCTL override lives only until
  the next RUN transition.
- **Not UI-exposed.** No control panel surface, no user-facing API.
  Diagnostic and test tools only.

Client side:
- A new validation script `tests/phase5_rollback.py` drives the IOCTL
  during an active render stream and asserts the evidence rules in §4.5.
- `test_stream_monitor.py` does NOT issue this IOCTL; its role remains
  observer only.

New miniport member `m_pActiveRenderStream`:
- Declared in `minwavert.h` as `CMiniportWaveRTStream*`, initialized to
  `NULL`, synchronized by a new `m_ActiveStreamLock` (`KSPIN_LOCK`).
- Set by `SetState(KSSTATE_RUN)` for cable speaker streams immediately
  after flag arming.
- Cleared by `SetState(KSSTATE_STOP)` (and `SetState(KSSTATE_PAUSE)` from
  RUN) for the same stream — but only if `m_pActiveRenderStream == this`,
  to avoid clobbering a newer stream that may have already registered.
- Cleared unconditionally in the destructor teardown path, before
  `FramePipeUnregisterFormat()` (still respecting the Phase 4 ordering).

Register/unregister must not race the Phase 3 pump helper. The pump
helper runs under `m_PositionSpinLock` (per-stream). The IOCTL handler
acquires `m_ActiveStreamLock` to read `m_pActiveRenderStream`, then
acquires the stream's `m_PositionSpinLock` to write the flag. Lock order
is therefore: `m_ActiveStreamLock` before `m_PositionSpinLock`. Anyone
else that needs both locks must use the same order.

### 4.5 One-owner verification — LOCKED with direct counter evidence

**Locked decision (approval record): direct owner-side counter evidence.**
Structural-guarantee-only is not sufficient. Phase 5 must land the
minimum instrumentation needed to turn one-owner proof into a counter
assertion instead of a review argument.

#### New counters on `FRAME_PIPE`

Two new fields per cable pipe (2 fields x 2 cables = 4 new counters in
total):

```cpp
// In FRAME_PIPE struct:
volatile ULONG      RenderPumpDriveCount;    // Phase 5: pump transport fires
volatile ULONG      RenderLegacyDriveCount;  // Phase 5: ReadBytes cable render fires
```

`FramePipeInit()` zeros both. `FramePipeReset()` does NOT reset them —
they are monotonic evidence counters, like the Phase 1 pump counters.

Increment points:
- `RenderPumpDriveCount`: incremented once per helper invocation that
  actually enters the render transport block and calls
  `FramePipeWriteFromDma()` at least once. The increment is the last
  action of the transport block before exit. Not incremented per DMA
  chunk, not incremented on early-exit from any gate.
- `RenderLegacyDriveCount`: incremented once per `ReadBytes()`
  invocation that enters the cable-pipe branch and calls
  `FramePipeWriteFromDma()` at least once. Like the pump counter,
  not incremented per DMA chunk.

Both increments happen under `m_PositionSpinLock` because both
call sites run inside that lock already (pump helper → inside
GetPositions spinlock; `ReadBytes()` → inside UpdatePosition called
under spinlock from GetPositions or TimerNotifyRT, both already spinlock
holders).

#### V2 diag exposure

V2 diag struct grows by 4 ULONGs at the tail (16 bytes). New layout:

```
0    .. 3    StructSize (now = 132)
4    .. 31   A_Render block  (7 ULONGs, unchanged)
32   .. 59   A_Capture block (7 ULONGs, unchanged)
60   .. 87   B_Render block  (7 ULONGs, unchanged)
88   .. 115  B_Capture block (7 ULONGs, unchanged)
116  .. 119  A_R_PumpDriveCount   (NEW)
120  .. 123  A_R_LegacyDriveCount (NEW)
124  .. 127  B_R_PumpDriveCount   (NEW)
128  .. 131  B_R_LegacyDriveCount (NEW)
```

`V2_DIAG_SIZE` bumps from 116 to 132. `test_stream_monitor.py` parses the
new tail when `ret.value >= V1_STATUS_SIZE + 132`. Old consumers that
only read 116 bytes still get the Phase 1 blocks and ignore the tail.

#### One-owner assertion rules (used by `tests/phase5_rollback.py`)

During active render at state X, the drive counters must satisfy:

| State                              | `RenderPumpDriveCount` | `RenderLegacyDriveCount` |
|------------------------------------|:----------------------:|:------------------------:|
| Pump-owned (flag set, default)     | **grows**              | frozen                   |
| Legacy-owned (flag cleared)        | frozen                 | **grows**                |

Phase 5 validation MUST show both quadrants. Evidence:
1. Start with default (flag set). Observe
   `RenderPumpDriveCount` growing and `RenderLegacyDriveCount` NOT
   growing over a 3-5 second window while audio is live.
2. Issue the rollback IOCTL with
   `ClearMask = AO_PUMP_FLAG_DISABLE_LEGACY_RENDER`.
3. Within 1-2 `GetPositions` calls (≤ 250 ms wall-clock under WASAPI
   exclusive), `RenderLegacyDriveCount` starts growing and
   `RenderPumpDriveCount` freezes.
4. Issue the flip-back IOCTL with
   `SetMask = AO_PUMP_FLAG_DISABLE_LEGACY_RENDER`.
5. Within 1-2 calls, the crossover reverses.
6. Audio stays audible across both transitions (no pop, no stall, no
   underrun spike on `DropCount`).

If any of the four grow/freeze assertions fail, Phase 5 commit is
blocked.

#### Why this is stronger than WinDbg BP

- It is automatable: the rollback test script drives the transitions and
  asserts the counters, no human interaction required.
- It runs inside normal operation, not under debugger interference.
- It proves **both** directions of the rollback, not just the default
  state.
- It pins the one-owner property to a machine-checkable counter
  signature instead of a code review argument.

Full WinDbg breakpoint-based one-owner confirmation remains available as
a follow-up if counter evidence ever gets ambiguous.

### 4.6 What to do if divergence starts ticking

Recommended default:
- Phase 5 commit blocks on `PumpShadowDivergenceCount == 0` during
  validation.
- If divergence appears:
  - do NOT commit Phase 5,
  - do NOT disable the shadow compare to "fix" it,
  - treat it as evidence that the pump transport block is interfering with
    `UpdatePosition()`'s math (possible causes: re-entering the spinlock,
    advancing a WaveRT contract field that we said we wouldn't touch, or
    an offset calculation mixing the pump's view and the WaveRT view),
  - roll back the in-tree edit and re-propose with a revised helper
    structure.

### 4.7 Fade-in extraction

Recommended default:
- still deferred, same as Phase 4.

Reason:
- Phase 5 is render only, and fade-in lives in capture (`WriteBytes()`),
- Phase 6 can deal with fade-in extraction at the same time it adds the
  capture transport block, OR we can land a pure Phase 5a refactor between
  them.

---

## 5. Verification plan

### 5.1 Bring-up checks

1. `build-verify.ps1 -Config Release`
2. `install.ps1 -Action upgrade`
3. `python test_ioctl_diag.py`
4. `python test_stream_monitor.py --once`

Expected:
- build green,
- install green (manifest match),
- V1 IOCTL path intact,
- V2 diag struct unchanged,
- idle counters all zero, flags `0x00000000`.

### 5.2 Phase 3 regression under Phase 5 (the "shadow drift detector" pass)

1. `python tests/phase3_shadow_active.py`
   - 20s x 3 regimes,
   - expect `Inv > 128`, `Div == 0`, `OverJump == 0`, `Flags == 0x00000007`
     (ENABLE | SHADOW_ONLY | DISABLE_LEGACY_RENDER),
   - Phase 3 shadow divergence stays zero — this is the Phase 5 "no math
     regression" gate.
2. `python tests/phase4_churn.py`
   - 20 STOP/RUN cycles x 1s,
   - expect `Div == 0`, helper fires each cycle, symbolic link stable.
3. `python tests/phase3_live_call_shadow.py`
   - real Phone Link call,
   - expect `A_Render peak Inv > 128`, `peak Div == 0`,
     `flags_seen == 0x00000007`,
   - user reports render audio audible on receiver (call quality side
     note: the existing AO cable distortion regression is tracked
     separately and NOT a Phase 5 blocker; Phase 5 passes if audio is
     audible and there is no new regression vs the Phase 4 baseline the
     user heard in the same session).

### 5.3 Phase 5 specific: rollback smoke test (IOCTL-driven, §4.4 Option B)

Executed by `tests/phase5_rollback.py`:

1. Open `\\.\AOCableA`.
2. Start a continuous WASAPI exclusive sine tone into the AO Cable A
   speaker device (same pattern as `phase3_shadow_active.py`'s
   `ContinuousSine`).
3. Wait 1 second so the stream is in RUN and the pump helper has run
   at least 2-3 times (first-call latch + one accepted accounting call).
4. Snapshot V2 diag: capture `A_R_PumpDriveCount`,
   `A_R_LegacyDriveCount`, `A_Render.PumpFeatureFlags`,
   `A_Render.FramesProcessedTotal`.
5. Wait 3 seconds, snapshot again. Assert:
   - `A_Render.PumpFeatureFlags == (ENABLE|SHADOW_ONLY|DISABLE_LEGACY_RENDER) == 0x00000007`
   - `A_R_PumpDriveCount` **grew**
   - `A_R_LegacyDriveCount` **frozen** (delta == 0)
6. Issue `IOCTL_AO_SET_PUMP_FEATURE_FLAGS` with
   `SetMask = 0, ClearMask = AO_PUMP_FLAG_DISABLE_LEGACY_RENDER`.
7. Wait 250 ms (generous margin vs the 1-2 position query target).
   Snapshot.
8. Wait 3 seconds, snapshot again. Assert:
   - `A_Render.PumpFeatureFlags == 0x00000003` (render bit cleared,
     shadow + enable intact)
   - `A_R_PumpDriveCount` **frozen** (delta == 0 across the 3-second
     window after the flip)
   - `A_R_LegacyDriveCount` **grew**
9. Issue `IOCTL_AO_SET_PUMP_FEATURE_FLAGS` with
   `SetMask = AO_PUMP_FLAG_DISABLE_LEGACY_RENDER, ClearMask = 0`.
10. Wait 250 ms, snapshot.
11. Wait 3 seconds, snapshot again. Assert:
    - `A_Render.PumpFeatureFlags == 0x00000007` again
    - `A_R_PumpDriveCount` **grew** again
    - `A_R_LegacyDriveCount` **frozen** again
12. Stop the sine tone, close handles.
13. Assert: audio bytes flowed continuously (pipe total advanced
    monotonically across the full run), and no transition caused
    `DropCount` to spike.

All four grow/freeze assertions (two states × two counters each) must
pass. A single failed assertion blocks Phase 5 commit.

### 5.4 Phase 5 specific: no double-advance check

Direct evidence from §5.3: at any moment, exactly one of
`RenderPumpDriveCount` and `RenderLegacyDriveCount` is growing. If both
were growing simultaneously, the pipe would receive double bytes per
unit time and one of the following would happen:
- `DropCount` would spike (pipe overflow from the second writer),
- `PumpShadowDivergenceCount` would tick (pump internal math diverges
  from `UpdatePosition()`'s byte-delta math because legacy is still
  writing).

Both are Phase 5 commit blockers. This is the double-advance guard.

---

## 6. Rollback

Expected rollback surface:

- `Source/Utilities/loopback.h`
- `Source/Utilities/loopback.cpp`
- `Source/Main/ioctl.h`
- `Source/Main/minwavert.h`
- `Source/Main/minwavert.cpp`
- `Source/Main/minwavertstream.cpp`
- `Source/Main/adapter.cpp`
- `test_stream_monitor.py`
- `tests/phase3_shadow_active.py`
- `tests/phase3_live_call_shadow.py`
- `tests/phase5_rollback.py` (new)
- `docs/PIPELINE_V2_CHANGELOG.md`

Rollback plan:
1. **Runtime (preferred, no rebuild)**: issue
   `IOCTL_AO_SET_PUMP_FEATURE_FLAGS` with
   `ClearMask = AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` on the affected
   cable device. Render transport reverts to the legacy `ReadBytes` path
   on the very next `GetPositions` call. No reboot, no reinstall, no
   stream restart.
2. **Full source rollback**: `git revert <phase5-commit>`, rebuild,
   reinstall. Phase 3 and Phase 4 remain intact because Phase 5 is a
   strictly additive patch on top of those phases.

---

## 7. Open items to lock before editing

### 7.1 Rollback knob mechanism

Question: Option A (registry) vs Option B (new IOCTL) vs Option C (WinDbg)
vs Option D (defer).

**Locked answer: Option B (new IOCTL)**. See §4.4 approval record.

### 7.2 One-owner verification method

Question: structural + rollback smoke test vs WinDbg BP vs direct
per-side drive counters.

**Locked answer: direct per-side drive counter evidence.** See §4.5
approval record. Structural + rollback smoke test alone was rejected as
insufficient; Phase 5 lands minimal per-side instrumentation so
one-owner proof becomes a four-quadrant counter assertion.

### 7.3 Transport block ordering inside the helper body

Question: run transport before or after step 10 (rolling window shadow
compare)?

Recommended answer: **before step 10**, i.e., insert between step 9 and
step 10. See §4.1.

### 7.4 What to do if divergence ticks during Phase 5 validation

Question: commit anyway (because legacy is disowned so divergence is no
longer authoritative), or block commit?

Recommended answer: **block commit**. See §4.6.

---

## 8. What this document is not

This document is not:

- a capture ownership move proposal,
- a timer role redesign,
- a new pump-math proposal,
- a loopback API rewrite,
- a new IOCTL schema proposal (unless §4.4 Option B is picked, in which
  case it expands to include one new IOCTL only),
- a fade-in refactor proposal.

If any edit requires:
- capture transport ownership to change,
- `FramePipeReadToDma()` to be called from the pump,
- `m_ullLinearPosition` / `m_ullWritePosition` /
  `m_ullPresentationPosition` / `m_ullDmaTimeStamp` to be written from
  the pump,
- `WriteBytes()` to be gated,
- new `FRAME_PIPE` fields,
- new `test_stream_monitor.py` fields beyond what Phase 1 already landed,

then it has escaped Phase 5 and should stop.

---

## 9. Next actions

1. Approval record is already locked (see top of document).
2. Checkpoint this proposal in its own commit.
3. Edit in this order (small-to-large, infrastructure first):
   - `Source/Utilities/loopback.h` — add 2 `FRAME_PIPE` fields.
   - `Source/Utilities/loopback.cpp` — zero them in `FramePipeInit()`.
   - `Source/Main/ioctl.h` — new IOCTL code, new request struct, V2 diag
     tail extension, `V2_DIAG_SIZE` = 132.
   - `Source/Main/minwavert.h` — new miniport members.
   - `Source/Main/minwavert.cpp` — init + register/unregister helpers.
   - `Source/Main/minwavertstream.cpp` — pump helper transport block,
     `UpdatePosition` render gate, `ReadBytes` legacy drive counter,
     `SetState` RUN/PAUSE/STOP active-render-stream registration,
     destructor unregister.
   - `Source/Main/adapter.cpp` — V2 diag tail fill, IOCTL handler.
   - `test_stream_monitor.py` — tail parser, display.
   - `tests/phase3_shadow_active.py` — tail parser.
   - `tests/phase3_live_call_shadow.py` — tail parser.
   - `tests/phase5_rollback.py` — NEW.
   - `docs/PIPELINE_V2_CHANGELOG.md` — Phase 5 entry.
4. Build/install/IOCTL/monitor bring-up (§5.1).
5. Phase 3 regression under Phase 5: A-step, churn, B-step (§5.2).
6. Phase 5 rollback smoke test (§5.3) — four counter assertions, audio
   continuity, no double-advance.
7. Only if all checks green, commit Phase 5 CLOSED. Manifests and
   `install_elev.exit` excluded from the commit per repo rule.

---

## 10. Proposed Phase 5 commit message

```text
Phase 5: pump owns cable render transport (IOCTL-gated runtime rollback)
```
