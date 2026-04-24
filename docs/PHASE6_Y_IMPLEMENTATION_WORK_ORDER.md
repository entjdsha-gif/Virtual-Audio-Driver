# Phase 6 Y Implementation Work Order

**Last updated:** 2026-04-16  
**Audience:** implementer starting Option Y from the validated Z baseline  
**Purpose:** provide a concrete execution order that can be followed without re-deciding Phase 6 architecture

Read in this order:

1. `docs/PHASE6_PLAN.md`
2. `docs/PHASE6_OPTION_Y_CABLE_REWRITE.md`
3. `docs/VB_PARITY_DEBUG_RESULTS.md`
4. `results/phase6_vb_verification.md`

## Preconditions

Start only after the Z baseline is in place and validated:

- Step 3/4 timer-owned transport reverted
- build/install/load works
- no BSOD / hang / deadlock
- local loopback and live call are back to baseline quality

Do not start Option Y from a half-broken timer-owned branch.

## Scope

This work order is for the **shared-mode phone path first**.

That means:

- packet-ready notification parity is **not** a Y1 blocker
- event-driven/packet-mode support should be preserved conservatively if present
- but the implementation must not be stalled on packet-mode perfection

Do not touch:

- `adapter.cpp` PortCls init
- filter/pin/topology registration
- INF / install / signing / service
- non-cable stream behavior

## Files to touch first

Primary edit set:

- `D:/mywork/ao-phase6/Source/Utilities/transport_engine.h`
- `D:/mywork/ao-phase6/Source/Utilities/transport_engine.cpp`
- `D:/mywork/ao-phase6/Source/Main/minwavertstream.h`
- `D:/mywork/ao-phase6/Source/Main/minwavertstream.cpp`

Secondary only if needed later:

- `D:/mywork/ao-phase6/Source/Utilities/loopback.cpp`

## Non-negotiable design rules

1. One canonical helper:
   - `AoCableAdvanceByQpc(rt, nowQpcRaw, reason, flags)`
2. Multiple active call sources are allowed:
   - query
   - shared timer render
   - shared timer capture
   - packet/event-driven only when armed
3. No second owner:
   - no timer-owned cursor truth separate from query truth
4. Shared-mode phone path does not depend on packet-ready notification
5. STOP/close must clear monotonic/cursor-like runtime state
6. Fade envelope is Y1/Y2 mandatory for audible parity

## Phase layout

The implementation order is:

1. `Y1A` runtime structure freeze
2. `Y1B` canonical helper skeleton
3. `Y1C` shadow-mode hook-up
4. `Y2` render migration
5. `Y3` capture migration
6. `Y4` cleanup

Each phase has a stop gate.
Do not skip gates.

## Y1A - Runtime structure freeze

### Goal

Freeze the runtime structure and helper API so later phases stop debating field ownership.

### Edit targets

- `Source/Utilities/transport_engine.h`
- `Source/Main/minwavertstream.h`

### Required edits

Add `AO_ADVANCE_REASON`:

```c
typedef enum _AO_ADVANCE_REASON {
    AO_ADVANCE_QUERY,
    AO_ADVANCE_TIMER_RENDER,
    AO_ADVANCE_TIMER_CAPTURE,
    AO_ADVANCE_PACKET,
} AO_ADVANCE_REASON;
```

Extend `AO_STREAM_RT` with at least:

- anchor QPC in 100ns
- published frames since anchor
- DMA cursor
- previous DMA cursor
- monotonic low / mirror counters
- last advance delta
- capture/render flag
- notify armed / fired
- notify boundary
- sample rate / block align / bits per sample / channels
- ring size frames
- scratch pointer / scratch size
- fade sample counter

Also define helpers/prototypes for:

- `AoCableAdvanceByQpc(...)`
- `AoApplyFadeEnvelope(...)`
- `AoResetFadeCounter(...)`
- stop/reset helper for cable runtime fields

### Must not do in Y1A

- no transport movement changes
- no call-site behavior changes yet

### Y1A gate

- builds cleanly
- structure ownership is frozen in code comments / declarations

## Y1B - Canonical helper skeleton

### Goal

Implement the helper body and all mandatory parity invariants, but keep audible transport under explicit migration control.

### Edit targets

- `Source/Utilities/transport_engine.cpp`

### Required edits

Implement:

- QPC-to-100ns conversion
- elapsed frame calculation from anchor
- 8-frame minimum gate
- 127-frame rebase
- overrun reject at `advance > sampleRate / 2`
- mirrored monotonic increments
- reason-based dispatch surface

Add fade support:

- `static const int32_t g_aoFadeEnvelope[95] = {...}`
- `AoApplyFadeEnvelope(...)`
- `AoResetFadeCounter(...)`

Add placeholders or internal helpers for:

- render scratch linearization
- render frame-pipe write
- capture frame-pipe read
- capture boundary alignment
- startup gate / partial-zero tail

### Y1B rule

The helper must be able to run in **shadow mode**.

That means:

- it may compute advance / update shadow bookkeeping
- it may record would-move counts
- but it must not yet become the audible owner in Y1

If you need a temporary internal flag to suppress audible movement in Y1, add it.

### Must not do in Y1B

- do not retire old cable transport ownership yet

### Y1B gate

- helper compiles
- parity constants are in code
- no behavior regression yet

## Y1C - Shadow-mode hook-up

### Goal

Route all important cable call sources into the helper without changing audible transport ownership.

### Edit targets

- `Source/Main/minwavertstream.cpp`

### Required edits

Cable-only call sites:

1. `GetPosition`
   - invoke `AoCableAdvanceByQpc(..., AO_ADVANCE_QUERY, ...)`
2. `GetPositions`
   - invoke `AoCableAdvanceByQpc(..., AO_ADVANCE_QUERY, ...)`
3. shared timer wake
   - invoke helper with:
     - `AO_ADVANCE_TIMER_RENDER`
     - `AO_ADVANCE_TIMER_CAPTURE`
4. `UpdatePosition`
   - cable streams become:
     - thin shim forwarding to helper
     - or cable-specific no-op wrapper
5. packet surfaces
   - keep current semantics when `m_ulNotificationsPerBuffer == 0`
   - do not force packet-mode ownership into shared-mode flow

### Must not do in Y1C

- do not move audible render/capture yet
- do not remove legacy cable `ReadBytes` / `WriteBytes` ownership yet

### Y1C gate

- build/install succeeds
- no BSOD / hang / deadlock
- no audible regression against Z baseline
- helper is visibly hit from query + timer paths

## Y2 - Render migration

### Goal

Move cable render transport into the canonical helper.

### Edit targets

- `Source/Utilities/transport_engine.cpp`
- `Source/Main/minwavertstream.cpp`
- `Source/Utilities/loopback.cpp` only if strictly necessary

### Required edits

On render path:

- `NotifyBoundaryBytes` / `NotifyArmed` / `NotifyFired` are preserved across STOP
  - this was re-checked in the VB decompile follow-up
  - AO should treat fresh stream allocation / zero-init as the once-only initialization point
- enable `DMA -> scratch -> FRAME_PIPE` inside the helper
- apply fade envelope on freshly written render samples
- update render cursor/accounting in the same helper call
- retire old cable `UpdatePosition -> ReadBytes` ownership

Conversion/ring behavior must follow the VB-parity target:

- `int32_t` container
- channel-planar interpretation
- 4-way bpp dispatch
- scratch linearization before processing

### Must not do in Y2

- do not move capture transport yet
- do not break packet/event-driven semantics if they already exist

### Y2 gate

- local loopback no worse than Z
- no static/buzz regression
- packet-free shared-mode phone path still works

## Y3 - Capture migration

### Goal

Move cable capture transport into the canonical helper.

### Edit targets

- `Source/Utilities/transport_engine.cpp`
- `Source/Main/minwavertstream.cpp`

### Required edits

On capture path:

- enable boundary-aware capture fill inside helper
- integrate startup/headroom/recovery behavior
- update capture cursor/accounting in the same helper call
- retire old cable `UpdatePosition -> WriteBytes` ownership

Shared-mode rule:

- do not make direct packet notification dispatch a blocker
- shared-mode phone path may rely on polling

Packet-mode rule:

- if packet mode remains supported, keep boundary/armed/fired bookkeeping logically owned by the helper

### Must not do in Y3

- do not reintroduce timer-only ownership
- do not split cursor truth between timer and query

### Y3 gate

- live call no worse than Z baseline
- startup clipping not worse
- no new burst/static artifacts
- Phone Link shared-mode path works

## Y4 - Cleanup

### Goal

Remove stale MSVAD cable ownership remnants after Y2/Y3 pass.

### Edit targets

- `Source/Main/minwavertstream.cpp`
- `Source/Main/minwavertstream.h`
- `Source/Utilities/transport_engine.cpp`
- `Source/Utilities/transport_engine.h`

### Required edits

Remove or cable-no-op:

- per-stream cable notification timer ownership
- per-stream cable DPC ownership
- old cable transport comments that assume periodic MSVAD ownership
- dead Step 3/4 timer-owned transport helpers/fields

### Y4 gate

- cable path no longer depends on old MSVAD ownership
- non-cable behavior untouched

## Stop / reset rules

These rules are now fixed:

- STOP/close must clear monotonic/cursor-like cable runtime state
- unregister removes the stream from the global table
- last-unregister destroys the shared timer and clears global timer state
- do not preserve cable monotonic counters across stop/close

Implement this through:

- `AoTransportOnStopEx`
- unregister path
- destructor ordering:
  - `KeFlushQueuedDpcs`
  - `AoTransportOnStopEx`
  - `AoTransportFreeStreamRt`

## Validation order

### After Y1

- build clean
- install clean
- no BSOD
- live call baseline unchanged

### After Y2

- local sine / TTS / loopback
- no click/static regression
- fade envelope measurable on forced packet-boundary discontinuity

### After Y3

- live Phone Link call
- Chrome output routed to `CABLE-B Input`
- startup / steady-state / stop all acceptable

## Commit boundaries

Use these boundaries:

1. Y1A/Y1B runtime structure + helper skeleton
2. Y1C shadow hook-up
3. Y2 render migration
4. Y3 capture migration
5. Y4 cleanup

Do not batch Y2 and Y3 together.

## Immediate next action

If Z is already validated, the implementer should start with:

1. `transport_engine.h`
2. `transport_engine.cpp`
3. `minwavertstream.cpp` query/timer shadow hook-up

That is the first patch.
