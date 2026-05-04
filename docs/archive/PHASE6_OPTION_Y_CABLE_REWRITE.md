# Phase 6 Option Y - Cable Stream Core Rewrite

> Detailed implementation spec for Option Y.
> This document is subordinate to `docs/PHASE6_PLAN.md`, and makes the cable-stream rewrite concrete enough to implement without touching the PortCls/WaveRT shell.

Related runtime evidence:

- `docs/VB_PARITY_DEBUG_RESULTS.md`
- `docs/PHASE6_Y_IMPLEMENTATION_WORK_ORDER.md`

## Purpose

Option Y is not a timer-tuning pass.
Option Y is not a "better cursor publish" patch on top of the old MSVAD cable path.

Option Y is a **cable-stream core rewrite**:

- keep the PortCls / WaveRT shell
- keep adapter / filter / pin / topology / install / INF / service
- keep non-cable streams on legacy behavior
- keep `FRAME_PIPE`
- replace the cable-stream transport/update ownership model

The rewrite target is the **cable stream path only**:

- `eCableASpeaker`
- `eCableBSpeaker`
- `eCableAMic`
- `eCableBMic`

Everything else stays out of scope unless a later validation proves otherwise.

## Design baseline

Current safety baseline:

- worktree: `D:/mywork/Virtual-Audio-Driver`
- branch: `feature/ao-fixed-pipe-rewrite`
- commit: `6d9e083`
- immediate prerequisite: `Z` revert and validation

Current architectural conclusion:

- old MSVAD cable ownership pattern is not recoverable by tuning
- failed Step 3/4 showed that moving transport onto a second independent cadence regresses badly
- cable transport, cursor/accounting, and position freshness must be advanced through **one canonical cable advance path**
- current VB runtime evidence supports a hybrid source model:
  - A side query-heavier
  - B side timer-dominant hybrid
  - shared timer active, but not second owner

This document therefore avoids two common traps:

1. it does **not** assume a timer-only transport owner
2. it does **not** assume query-only transport ownership

Instead, it defines one canonical helper and requires every active call source to funnel into it.

## Rewrite boundary

### Keep

- PortCls / `IMiniportWaveRT` framework
- adapter / filter / pin / topology registration
- service / INF / install / signing
- `FRAME_PIPE`
- existing Step 1 transport scaffold:
  - `AO_TRANSPORT_ENGINE`
  - `AO_STREAM_RT`
  - alloc/free
  - run/pause/stop registration
  - refcount discipline
- non-cable stream behavior

### Rewrite

For cable streams only, rewrite:

- MSVAD-style periodic simulated-hardware position ownership
- cable `UpdatePosition` transport ownership
- cable `ReadBytes` / `WriteBytes` ownership
- per-stream notification timer ownership
- per-stream DPC ownership
- cable truth sourced from `m_ullLinearPosition += ByteDisplacement`

## Current MSVAD-derived cable code to retire or cable-no-op

The references below are from the current Phase 6 worktree in `D:/mywork/Virtual-Audio-Driver`.

### Fields in `CMiniportWaveRTStream`

Current MSVAD-pattern fields still present:

- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.h:74`
  - `m_pNotificationTimer`
- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.h:114`
  - `m_pDpc`
- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.h:145`
  - `m_pTransportRt`

Keep:

- `m_pTransportRt`

Retire or cable-no-op:

- `m_pNotificationTimer`
- `m_pDpc`

### Lifetime / allocation sites

Current MSVAD-pattern timer/DPC lifetime:

- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:63`
  - notification timer teardown path
- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:125`
  - DPC free path
- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:329`
  - notification timer allocation
- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:359`
  - DPC allocation
- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:1593`
  - notification timer set/arm path
- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:2480`
  - notification timer cancel path

For cable streams, these paths should stop owning transport.

### Cable transport/update ownership sites

Current transport/update ownership sites that are not allowed to remain authoritative for cable streams:

- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:882`
  - `UpdatePosition(ilQPC)` in packet/query path
- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:1121`
  - `UpdatePosition(ilQPC)` before position/query return
- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:1155`
  - `UpdatePosition(ilQPC)` inside `GetPositions`
- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:1907`
  - `CMiniportWaveRTStream::UpdatePosition` body itself
- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:2026`
  - cable publish-from-`m_ullLinearPosition`

These are the core MSVAD-derived ownership points that must be removed, replaced, or turned into cable-specific wrappers around the new canonical helper.

### Packet/query surfaces that remain relevant

These surfaces are still important, but must be rewritten to funnel into one canonical cable advance helper:

- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:866`
  - `GetPosition`
- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:913`
  - `GetReadPacket`
- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:998`
  - `SetWritePacket`
- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:1101`
  - `GetPacketCount`
- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:1132`
  - `GetPositions`

Important note:

- `GetLinearBufferPosition` does **not** currently appear in the AO code search and must not be assumed as a real entrypoint until found in code.
- `GetReadPacket`, `SetWritePacket`, and `GetPacketCount` are currently **event-driven-only** surfaces in AO. In the current code they return `STATUS_NOT_SUPPORTED` when `m_ulNotificationsPerBuffer == 0`, so they cannot be the only authoritative cable advance entrypoints for shared-mode clients.

## Canonical cable ownership model

Cable streams must funnel all active transport/accounting through one helper:

```c
AoCableAdvanceByQpc(rt, nowQpc, reason, flags);
```

This function is the new cable-stream truth source.

Implementation ownership:

- declaration: `Source/Utilities/transport_engine.h`
- implementation: `Source/Utilities/transport_engine.cpp`
- WaveRT-facing callsites: `Source/Main/minwavertstream.cpp`

It owns, together:

1. QPC delta -> elapsed frames
2. minimum-frame gate
3. monotonic cursor/accounting updates
4. DMA wrap/linearization
5. `FRAME_PIPE` read/write
6. position freshness before WaveRT reporting
7. packet-ready accounting for cable clients

Non-optional VB-parity details inside the canonical helper:

8. 63/64-style drift correction / periodic rebase discipline
9. DMA circular -> linear scratch staging before conversion / pipe write
10. DMA overrun guard before processing pathological late bursts

What it must not do:

- depend on a second independent transport owner
- publish state for another cadence to move later
- let timer path and query path own different cursor truth

## AO_TRANSPORT_ENGINE role after rewrite

`AO_TRANSPORT_ENGINE` stays, but its role changes.

It remains responsible for:

- active cable stream registry
- shared timing infrastructure
- global wake source
- refcount-safe enumeration
- helper dispatch into the canonical cable advance path
- auxiliary background maintenance

It must stop being:

- a second transport owner separate from the cable update/query path
- the place where cable transport moves on state prepared elsewhere

The safe rule is:

**all active call sources must funnel into `AoCableAdvanceByQpc`, whether they originate from query, packet, or shared timer wakeup.**

Current runtime evidence says that the shared timer is not merely passive bookkeeping.
It is an active first-class call source, especially on the B side, but still not the only source.

## State ownership split

### Stays in `CMiniportWaveRTStream`

WaveRT-facing stream contract state:

- KS state
- format/block-align/sample-rate data already owned by the stream
- buffer pointers and sizes required by the miniport contract
- packet-mode contract fields required by WaveRT clients
- any non-cable legacy fields still needed by non-cable streams

Important rule:

- if a WaveRT-visible packet field must remain physically stored on `CMiniportWaveRTStream`, it may still be **logically owned** by the canonical cable advance path
- do not split ownership so the stream updates one subset and `AO_STREAM_RT` updates another subset on a different cadence

### Moves into `AO_STREAM_RT`

Cable-runtime-owned state:

- last advance QPC
- elapsed-frame carry/remainder
- canonical monotonic transport/accounting counters
- startup/headroom/gate state
- packet notification bookkeeping for cable clients
- cable render/capture activity flags
- any cable-only position freshness state
- per-stream engine registration state already introduced in Step 1

At minimum, the rewrite must explicitly decide ownership for the current packet-mode fields that exist today in `CMiniportWaveRTStream`, including:

- `m_llPacketCounter`
- `m_ulLastOsReadPacket`
- `m_ulLastOsWritePacket`
- `m_ulCurrentWritePosition`
- `m_IsCurrentWritePositionUpdated`

These fields are dangerous if left half-owned by legacy stream logic and half-owned by the new helper.

### Rule

If a field exists only to support MSVAD-style periodic cable transport ownership, move it out of the stream or make it cable-no-op.

## Active call sources and how they should behave

Option Y uses one canonical helper, but more than one entrypoint may invoke it.

Loud rule:

- every cable `GetPosition` call invokes `AoCableAdvanceByQpc(...)` before reporting offsets
- every cable `GetPositions` call invokes `AoCableAdvanceByQpc(...)` before reporting positions
- packet-mode surfaces may invoke the same helper too, but must not create a second cursor/accounting truth

### 1. `SetWritePacket`

Current location:

- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:998`

Role in Y:

- authoritative packet-boundary input for cable render streams
- update stream-side packet contract state
- call `AoCableAdvanceByQpc(...)` before returning if doing so is needed to keep cable transport and position freshness current

### 2. `GetReadPacket`

Current location:

- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:913`

Role in Y:

- cable capture-side packet handoff surface
- call `AoCableAdvanceByQpc(...)` before answering packet availability
- report current state produced by the canonical helper, not by a second ownership path

### 3. `GetPositions`

Current location:

- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:1132`

Role in Y:

- refresh cable transport/accounting on demand before reporting positions
- report already-coupled state, not stale precomputed position

### 4. Shared timer wake

Role in Y:

- allowed as a global wake source for active cable streams
- treated as an active first-class source, not just optional background bookkeeping
- must enter `AoCableAdvanceByQpc(...)`
- must not own a separate transport/accounting truth model

### 5. `UpdatePosition`

Role in Y:

- cable streams: no longer the old MSVAD transport owner
- acceptable end states:
  - cable no-op wrapper
  - cable shim that only forwards into `AoCableAdvanceByQpc(...)`
  - cable removal from authoritative ownership while leaving non-cable behavior intact

The important rule is:

**Do not let cable transport continue to depend on the old `UpdatePosition -> ByteDisplacement -> ReadBytes/WriteBytes` pattern.**

### 6. `GetPosition`

Current location:

- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:866`

Role in Y:

- treat as a first-class cable query surface, not a secondary detail
- call `AoCableAdvanceByQpc(...)` before returning current play/write offsets for cable streams
- this matters because `GetPosition` is a universal WaveRT surface, unlike event-driven-only packet APIs

### 7. `GetPacketCount`

Current location:

- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp:1101`

Role in Y:

- if packet mode remains supported for cable streams, packet count freshness must come from the same canonical helper
- do not let `GetPacketCount` remain on legacy `UpdatePosition` ownership while other packet/query paths move to the new helper

## Packet notification requirement

Safe assumption for Y1/Y2:

- do **not** blindly delete cable packet notification semantics
- do stop using the old per-stream MSVAD notification timer as transport owner

Working rule:

1. preserve WaveRT-visible packet notification behavior until validation proves it can be simplified
2. detach it from old per-stream timer ownership
3. drive packet-ready state from the canonical cable advance path instead of the old simulated-hardware timer path

That gives a safe migration path:

- keep client contract first
- replace internal ownership second

Concrete validation rule:

- after any phase that changes cable packet ownership, event-driven cable streams must still show correct packet progression via `GetReadPacket`, `SetWritePacket`, and `GetPacketCount`
- WaveRT notification events must still be emitted at packet-boundary progression even if the old per-stream notification timer is no longer the owner
- do not remove cable packet-ready behavior in a phase that does not also provide its replacement source of truth

Current provisional VB rule from local static RE:

- notification state appears to involve `+0x164` (armed), `+0x165` (fired), and `+0x7C` (boundary/threshold)
- indirect notification dispatch through `+0x8188` with notification-like `edx=8` is described in local notes for both the query-side `+0x6320` path and the periodic/helper-side `+0x68ac` path
- shared-timer callback notes also show callback-list signalling plus `KeSetEvent` for registered clients

Safe interpretation for Option Y:

- preserve packet-ready behavior conservatively
- allow both query-side and timer-side call sources to funnel into the same canonical helper
- do **not** claim that live precedence between those notification sources is fully proven

Shared-mode clarification from the current VB parity pass:

- the tested Phone Link-style shared-mode path appears to rely on position polling, not packet-ready notification
- packet notification state is gated behind the boundary/notification setup path, and the direct `+0x8188` boundary-crossing dispatch is skipped when that setup is absent
- therefore Y1/Y2/Y3 do **not** need to reproduce direct packet notification dispatch as a blocker for the shared-mode phone path
- if AO keeps event-driven / packet-mode support for cable streams, preserve that path conservatively, but do not let it dominate the shared-mode rewrite

## Required VB-parity details

These are not optional follow-up polish items.
They must land as part of the Option Y implementation.

### 1. Drift correction is mandatory

The helper must include drift-corrected scheduling/accounting behavior comparable to the 63/64 phase-correction described in local VB notes.

Rule:

- do not leave long-call timing quality dependent on raw timer punctuality or naive periodic accumulation
- if the helper uses a shared wake source, it still must apply the correction logic inside the canonical advance path

This is a Y1 requirement, not a later optimization.

### 2. Scratch linearization is mandatory

Before speaker-side processing/write, the canonical helper must support:

```text
DMA circular region -> linear scratch -> FRAME_PIPE
```

Rule:

- do not rely on ad hoc wrap handling spread across conversion or write logic
- wrap handling must be explicit and centralized in the canonical helper path

This is a Y1/Y2 requirement.

### 3. DMA overrun guard is mandatory

The canonical helper must include a guard that rejects or skips pathological late bursts before they exceed the safe DMA-window bound.

Rule:

- this guard sits beside the minimum-frame gate
- if a computed move would exceed the safe DMA half-window bound, the helper must not process it as a normal transport interval

This is a Y1/Y2 requirement.

## Canonical helper pseudocode

This is the design target, not final code.

The canonical helper should distinguish entry reason explicitly:

```c
typedef enum _AO_ADVANCE_REASON {
    AO_ADVANCE_QUERY,
    AO_ADVANCE_TIMER_RENDER,
    AO_ADVANCE_TIMER_CAPTURE,
    AO_ADVANCE_PACKET,
} AO_ADVANCE_REASON;
```

Important clarification:

- `AO_ADVANCE_PACKET` is event-driven-only
- shared-mode phone paths do not depend on packet-ready notification as a Y1 blocker
- the helper still owns packet bookkeeping if packet mode remains supported

The following invariants come directly from the reconciled VB static/runtime evidence:

- 8-frame minimum gate
- 127-frame rebase
- overrun reject at `advance > sampleRate / 2`
- mirrored monotonic increments
- notification gate checked through boundary/armed/fired state rather than as an always-on shared-mode path

```c
AoCableAdvanceByQpc(rt, nowQpcRaw, reason, flags)
{
    lock(rt->PositionLock);

    apply_drift_correction(rt, nowQpcRaw);
    nowQpc100ns = AoQpcTo100ns(nowQpcRaw);
    elapsedFrames =
        ((nowQpc100ns - rt->AnchorQpc100ns) * rt->SampleRate) / 10000000ULL;
    advance = elapsedFrames - rt->PublishedFramesSinceAnchor;

    if (advance < 8)
    {
        unlock(rt->PositionLock);
        return;
    }

    if (elapsedFrames >= ((uint64_t)rt->SampleRate << 7))
    {
        rt->PublishedFramesSinceAnchor = 0;
        rt->AnchorQpc100ns = nowQpc100ns;
    }

    if ((uint32_t)advance > (rt->SampleRate / 2))
    {
        AoHandleAdvanceOverrun(rt, reason, advance);
        unlock(rt->PositionLock);
        return;
    }

    if (!rt->IsCapture)
    {
        bytes = elapsedFrames * rt->BlockAlign;
        linearize_dma_window_to_scratch(rt, bytes);
        FramePipeWriteFromDma(rt->Pipe, rt->ScratchBuffer, bytes);
        AoApplyFadeEnvelope(rt->ScratchBuffer, elapsedFrames, &rt->FadeSampleCounter);
        advance_render_cursors(rt, elapsedFrames, bytes);
    }

    if (rt->IsCapture)
    {
        bytes = elapsedFrames * rt->BlockAlign;
        apply_startup_gate_if_needed(rt, elapsedFrames);
        apply_capture_boundary_alignment_if_needed(rt, &bytes);
        FramePipeReadToDmaPartial(rt->Pipe, ...);
        apply_partial_zero_tail_if_needed(rt, ...);
        advance_capture_cursors(rt, elapsedFrames, bytes);

        if (rt->NotifyArmed && !rt->NotifyFired &&
            ((rt->DmaCursor % rt->RingSizeFrames) == rt->NotifyBoundaryBytes))
        {
            rt->NotifyFired = 1;
            AoInvokePortclsNotify(rt->PortNotifyCtx, 8);
        }
    }

    rt->MonoFramesLow += elapsedFrames;
    rt->MonoFramesMirror += elapsedFrames;
    rt->LastAdvanceDelta = elapsedFrames;
    rt->PublishedFramesSinceAnchor = (uint32_t)elapsedFrames;

    unlock(rt->PositionLock);
}
```

## Shared timer primitive

Current reconciled static evidence around `FUN_1400065b8` / `FUN_14000669c` supports:

- one global shared timer
- one 8-slot active-stream table
- timer created on first active stream registration
- timer destroyed when the last stream unregisters

The current local decompile shows:

- `ExAllocateTimer(...)`
- `ExSetTimer(...)`

not `ExAllocateTimer2`.

For Option Y parity notes, treat the observed pair as the current reference unless later evidence overrides it.

## Click suppression - fade-in envelope

VB appears to use a fade-in envelope at packet boundary transitions rather than a real SRC path for audible cleanup.

This is a Y1 mandatory parity detail.

The verified shape from local notes is:

- 96-entry coefficient table with maximum usable index `95`
- silence prefix at the front
- rising ramp toward `128`
- per-sample application equivalent to:

```c
sample = (sample * coef[i]) >> 7;
```

Y1 requirements:

- define the fade-envelope table in `Source/Utilities/transport_engine.cpp`
- reset the per-stream fade counter at packet boundary transitions
- apply the envelope on the ring-write / freshly-written sample path so packet-boundary clicks are suppressed

Practical note:

- the current Y1B code path already uses the corrected interpretation:
  - 96 stored entries
  - index clamped to `0..95`
- do not "fix" that back to a 95-sized table unless the VB source notes are re-verified again

If this is omitted, Y1 may compile and run but is not considered audibly VB-equivalent enough.

## Ring format - INT32 19-bit, 4-way dispatch

Current static parity notes support the following ring/dispatch model:

- container: `int32_t`
- layout: channel-planar
- effective range: 19-bit normalized/sign-extended

Recommended Y1 conversion table:

- 8-bit `u8`: `(sample - 0x80) << 11`
- 16-bit `i16`: `sample << 3`
- 24-bit packed `i24`: sign-extend to 32, then shift down to the VB-equivalent 19-bit range
- 32-bit / VB passthrough-style path: linear copy

For Y1, treat this as the parity target for `FRAME_PIPE` ingestion and internal scratch/ring movement.

## Y1 implementation manifest

The minimum Y1 patch should contain:

### `Source/Utilities/transport_engine.h`

- `AO_ADVANCE_REASON`
- `AoCableAdvanceByQpc(...)` declaration
- cable-runtime fields for:
  - anchor QPC
  - published frames
  - DMA cursor
  - previous DMA cursor
  - mirrored monotonic counters
  - capture/render direction flag
  - scratch pointer / scratch size
  - fade counter

### `Source/Utilities/transport_engine.cpp`

- `AoCableAdvanceByQpc(...)`
- fade envelope table
- fade application helper
- drift/rebase / overrun behavior matching the reconciled VB rule
- shared timer dispatch through the reason enum

### `Source/Main/minwavertstream.cpp`

- cable `GetPosition` / `GetPositions` call the canonical helper before returning state
- cable `UpdatePosition` becomes a shim or cable-no-op wrapper, not the old MSVAD owner
- shared timer wake enters the helper with the correct reason
- per-stream notification timer / DPC cease to be cable transport owners

### Stop / teardown rule

- `AoTransportOnStopEx` must clear the monotonic/cursor-like runtime state that VB stop/close paths clear before or around unregister
- destructor order remains:
  - `KeFlushQueuedDpcs`
  - `AoTransportOnStopEx`
  - `AoTransportFreeStreamRt`

Key property:

- frame delta
- gate
- transport
- cursor advance
- WaveRT-visible freshness

all happen in one path.

## Stream lifecycle in Option Y

### RUN

Keep from Step 1:

- `AoTransportAllocStreamRt`
- `AoTransportOnRunEx`

But add Y-specific meaning:

- mark cable stream active in shared engine
- initialize cable runtime state
- do **not** arm per-stream cable notification timer as transport owner

### PAUSE

Keep from Step 1:

- `AoTransportOnPauseEx`

Y-specific behavior:

- pause cable activity in shared engine
- retain runtime object if pause/resume reuse is desired
- stop any cable-only ownership that still depends on old per-stream MSVAD timer/DPC

### STOP

Keep from Step 1:

- `AoTransportOnStopEx`

Y-specific behavior:

- detach cable stream from active engine set
- drain outstanding shared-engine references
- ensure no further cable advancement occurs after stop
- zero monotonic/cursor-like runtime state that VB stop/close paths clear before or around unregister

### Destructor

Keep Step 1 safety discipline:

- `KeFlushQueuedDpcs`
- `AoTransportOnStopEx`
- `AoTransportFreeStreamRt`

Additional Y rule:

- old cable timer/DPC ownership must already be gone or cable-no-op before destructor safety can be trusted

## Locking and teardown safety

### Lock order

Preferred order:

1. engine/global lock
2. per-stream position/accounting lock
3. `FRAME_PIPE` lock

Do not invert this order in shared-engine call sites.

### Execution rule

- engine lock is for active-list snapshot / registration
- canonical cable advance runs on per-stream lock, outside long global lock ownership

### Teardown rule

- shared-engine references must be refcounted
- destructor must flush DPC work before freeing runtime
- timer destruction must happen after active work is drained

### Current Step 1 scaffolding that stays useful

The following remain valid and should be reused:

- `AoTransportAllocStreamRt`
- `AoTransportFreeStreamRt`
- `AoTransportOnRunEx`
- `AoTransportOnPauseEx`
- `AoTransportOnStopEx`
- engine active-list refcount discipline

## Engine timer handoff from Step 1 scaffold

Current state after Step 1 / Z:

- shared engine lifecycle exists safely
- shared timer exists safely
- timer-owned audio movement is disabled by Z

Rewrite rule:

- Y1 may use the shared timer as an active wake source into the canonical helper in shadow mode
- once render/capture migration starts, the shared timer must never own a second independent cursor/accounting model
- by the end of Y3, cable transport/accounting truth must come from the canonical helper only, regardless of whether entry came from query, packet, or timer wake

## Implementation phases

### Y1 - canonical runtime scaffold, no audible transport change

Scope:

- define final `AO_STREAM_RT` cable-owned state
- add `AoCableAdvanceByQpc(...)`
- make cable entrypoints call it in shadow mode only
- do not change audible cable transport yet

Files:

- `D:/mywork/Virtual-Audio-Driver/Source/Utilities/transport_engine.h`
- `D:/mywork/Virtual-Audio-Driver/Source/Utilities/transport_engine.cpp`
- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.h`
- `D:/mywork/Virtual-Audio-Driver/Source/Main/minwavertstream.cpp`

Gate:

- build/install succeeds
- no BSOD / hang / deadlock
- non-cable behavior unchanged
- cable runtime counters/logs show canonical helper being hit from expected entrypoints
- helper design surface includes drift correction, scratch linearization, and DMA overrun guard before audible migration

### Y2 - cable render migration

Scope:

- move cable speaker transport into canonical helper
- old cable render `UpdatePosition -> ReadBytes` ownership removed
- any render-side timer/DPC ownership that exists only to support old MSVAD transport must be retired or made cable-no-op by the end of this phase
- if render-side packet semantics are touched, their replacement source of truth must land in the same phase

Gate:

- local loopback does not regress
- no static overlay introduced
- Y2 can be reverted cleanly back to Y1 without leaving packet/timer ownership half-retired
- any render-side packet-ready behavior that existed before still works after the phase
- speaker-side path uses explicit scratch linearization and overrun guard, not implicit wrap handling

### Y3 - cable capture migration

Scope:

- move cable mic transport into canonical helper
- old cable capture `UpdatePosition -> WriteBytes` ownership removed
- startup/headroom/recovery integrated inside the same helper
- by the end of Y3, cable streams stop using `m_pNotificationTimer` and `m_pDpc` as transport owners
- packet counter / notification semantics are fully re-homed into the canonical cable path

Gate:

- live call improves or matches baseline
- no front-chop regression
- no new static / burst artifacts
- packet-ready behavior still works after cable timer/DPC ownership removal

### Y4 - cable-only cleanup

Scope:

- remove stale cable-only MSVAD remnants
- remove dead comments and stale assumptions
- leave non-cable legacy paths alone

Gate:

- cable path no longer depends on the retired MSVAD ownership pattern

## Risk table

| Risk | Why it matters | What to do |
| --- | --- | --- |
| Wrong authoritative entrypoint set | If the real hot path is wrong, the rewrite will still split ownership | Validate actual cable call frequency in `GetPosition`, `GetPositions`, `SetWritePacket`, `GetReadPacket`, `GetPacketCount`, and shared timer wake before Y2 |
| Packet notification behavior misunderstood | A client may depend on WaveRT-visible packet readiness even if old timer ownership is wrong | Preserve packet semantics first; replace ownership second |
| Event-driven-only APIs over-weighted | `GetReadPacket` / `SetWritePacket` / `GetPacketCount` are not universal shared-mode entrypoints in the current AO code | Treat `GetPosition` and `GetPositions` as first-class surfaces in the rewrite and prove which paths are actually hot on cable clients |
| Shared timer becomes second owner again | Recreates Step 3/4 regression in a different form | Require every source to funnel into `AoCableAdvanceByQpc(...)` |
| Render/capture asymmetry mishandled | Speaker and mic do not have identical producer truth | Keep shared design rule but allow different internal subpaths |
| Packet fields split between stream and RT | `m_llPacketCounter`, current write position, and OS packet bookkeeping can become dual-owned and drift | Explicitly assign single-writer ownership through the canonical helper even if storage remains on `CMiniportWaveRTStream` |
| Missing drift correction | Long calls can accumulate timing slip even if short tests sound fine | Make 63/64-style correction mandatory in Y1 |
| Missing scratch staging | DMA wrap handling can leak into conversion/write logic and reintroduce subtle distortion | Make scratch linearization explicit in Y1/Y2 |
| Missing DMA overrun guard | Late wakeups can turn into oversized bursts that cross unsafe DMA windows | Add the guard beside the frame gate, not as a later optimization |
| Phase numbering drift | If `Y1`/`Y2`/`Y3`/`Y4` mean different things across docs, rollback and validation become ambiguous | Keep this file aligned with `PHASE6_PLAN.md` and define rollback expectation at each migration gate |
| Teardown race / UAF | Shared engine + DPC + stream lifetime is still kernel code | Keep Step 1 refcount/flush discipline and do not shortcut destructor ordering |
| Too much removal too early | Deleting legacy cable paths before Y1/Y2 proves the new helper is live raises BSOD risk | Phase gates are mandatory; do not skip them |

## Bottom line

Option Y is not "make the timer faster."

Option Y is:

**remove MSVAD-derived cable transport/update ownership, keep the PortCls/WaveRT shell, and rebuild cable streams around one canonical cable advance path so transport, accounting, and position freshness stay coupled.**
