# Phase 6 Plan

> Authoritative Phase 6 plan as of 2026-04-16.
> If this file conflicts with older notes, Phase 5 experiments, or timer-owned Phase 6 drafts, this file wins.

Detailed cable-stream rewrite spec:

- `docs/PHASE6_OPTION_Y_CABLE_REWRITE.md`
- `docs/VB_PARITY_DEBUG_RESULTS.md`
- `docs/PHASE6_Y_IMPLEMENTATION_WORK_ORDER.md`

## Current position

We now have three distinct states:

1. `439bbcd`
   Effective last-known-good baseline for the phone path.

2. `2c733f1`
   Archived failed Phase 5 attempt. Keep for history, not as a design baseline.

3. Phase 6 Step 3/4 timer-owned transport experiment
   Also failed. The problem was not just a constant or one bad cursor choice. The structural mistake was **decoupling transport from the update chain** and moving audio transfer into an external engine cadence.

The next move is therefore:

- `Z`: revert Step 3/4 data movement and recover the known-good baseline behavior while keeping the Step 1 skeleton
- `Y`: reimplement Phase 6 as a **VB-equivalent update-chain-coupled transport**, not as a timer-owned transport core

## What Phase 6 is now

Phase 6 is now a **cable-stream core rewrite**.

That means:

- we keep the PortCls / WaveRT shell
- we keep device registration, INF, install, service, adapter, filter, and non-cable paths
- we discard the MSVAD-derived cable-stream transport/update pattern

Phase 6 is no longer:

- a shared-timer-owned render/capture transport experiment
- a "1 ms timer will fix it" experiment
- a GetPositions-only ownership model
- a tuning exercise on top of the existing MSVAD cable path

Phase 6 is now:

- a new cable-stream core where accounting and transport stay coupled
- a design where frame delta calculation, gating, cursor/accounting, DMA<->pipe movement, and position freshness are owned by one canonical cable advance path
- a design where query path and shared timer may both be active call sources, but neither may become a second owner
- a design that uses the existing engine skeleton only as supporting infrastructure
- a design where shared-mode phone paths do not depend on packet-ready notification as a Y1 blocker

## Rewrite boundary

### Keep

- PortCls / WaveRT framework usage
- `PcAddAdapterDevice`, subdevice registration, filter/pin/topology setup
- install / INF / signing / service scaffolding
- `FRAME_PIPE` infrastructure, unless a later implementation detail forces a local rewrite
- non-cable stream behavior

### Rewrite

Cable-stream-specific MSVAD-derived transport/update behavior is not a baseline anymore.

For cable streams, Phase 6 is allowed to replace:

- `UpdatePosition` as the authoritative transport owner
- cable-side `ReadBytes` / `WriteBytes` ownership
- `m_pNotificationTimer`
- `m_pDpc`
- precomputed `m_ullLinearPosition += ByteDisplacement` as the cable truth source
- any cable-only dependence on the old periodic simulated-hardware pattern

## Design rules

### 1. Canonical cable advance path

Cable streams must have exactly one canonical advancement routine.

Suggested shape:

```c
AoCableAdvanceByQpc(rt, nowQpc, reason, flags);
```

This routine owns, together, for cable streams:

1. QPC -> frame delta calculation
2. minimum gate logic
3. monotonic cursor/accounting updates
4. DMA linearization / wrap handling
5. `FRAME_PIPE` read/write
6. position freshness before reporting to WaveRT clients

This is the key rewrite rule:

**No second path is allowed to "just move audio later" using state published by the first path.**

That split was the Step 3/4 regression source.

### 2. Call sources

The call source may be more than one entrypoint, but they must all funnel into the same canonical cable advance path.

Possible call sources include:

- `GetPosition`
- `GetPositions`
- `GetReadPacket`
- `SetWritePacket`
- a shared engine tick

Runtime evidence in `docs/VB_PARITY_DEBUG_RESULTS.md` currently supports:

- A-side query-heavier behavior
- B-side timer-dominant hybrid behavior
- therefore neither `query-only` nor `timer-only` wording is safe

Do not assume `GetLinearBufferPosition` exists unless it is found in the current codebase.

The important rule is not the name of the entrypoint.
The important rule is:

**all cable transport/accounting work must go through the same advancement function.**

Loud implementation rule:

- every cable `GetPosition` call must invoke the canonical helper before reporting offsets
- every cable `GetPositions` call must invoke the canonical helper before reporting positions
- packet-mode surfaces may also invoke the helper, but they must not become a second ownership path

### 3. Shared timer role

The shared timer remains in the codebase as an **active first-class call source**, but it is not allowed to become a second owner.

Allowed uses:

- shared wakeups that enter the canonical cable advance path
- lifecycle scaffolding
- idle/background bookkeeping
- optional diagnostics
- future helper work that ultimately funnels into the canonical cable advance path

Explicitly forbidden for cable streams:

- per-stream `m_pNotificationTimer` ownership as the old MSVAD transport owner
- per-stream DPC ownership as the old MSVAD transport owner
- a second transport path outside the canonical cable advance path

Forbidden use:

- a second independent transport owner that bypasses the canonical cable advance path
- a timer-owned cursor/accounting truth separate from the query/packet truth

### 4. Units

All runtime transport math is frame-count-driven.

- authoritative unit: `frames`
- derived units: `bytes`, `QPC`
- forbidden runtime unit: `ms`

`ms` may appear only in:

- comments
- external UI
- logs for human readability

It must not be stored as runtime state and must not drive transport math.

### 5. Required VB-parity implementation details

The following are not optional polish items.
They are required for Option Y to remain meaningfully VB-equivalent:

1. **63/64 drift correction**
   - the canonical cable advance path must include drift-corrected scheduling/accounting behavior
   - this is not a later optimization item
   - long-running calls must not depend on raw timer regularity alone

2. **Scratch linearization before processing**
   - the canonical helper must support `DMA circular -> linear scratch -> FRAME_PIPE`
   - do not make wrap handling an incidental side effect scattered across conversion code

3. **DMA overrun guard**
   - the canonical helper must reject or skip pathological late bursts before they exceed the safe DMA-window bound
   - this guard sits next to the frame gate and is part of the authoritative helper contract

If these three are absent, the design may still compile and sound close, but it is not considered sufficiently VB-equivalent.

### 6. Keep transport and accounting coupled

Within the authoritative update path, the following belong together:

1. QPC/frame delta calculation
2. minimum gate logic
3. monotonic cursor/accounting update
4. DMA linearization or wrapping logic
5. `FRAME_PIPE` read/write

If these steps are split across independent clocks, stale gaps and race-like behavior return.

### 7. Render and capture may remain asymmetric

Render and capture do not have to use the exact same helper internals, but they must obey the same rule:

**the producer/consumer accounting used by transport must be advanced in the same chain that triggers transport.**

Do not force symmetry if the real producer differs by direction.

## What is abandoned

The following are explicitly abandoned and must not come back as the primary design:

- timer-owned Step 3/4 data movement
- fixed 20 ms engine-owned burst transport
- "just lower the timer to 1 ms" as the main fix
- "publish a better cursor to the timer engine and keep the rest the same"

Those may be useful diagnostics, but they are not the Phase 6 architecture.

## Z: immediate recovery step

### Goal

Recover Step 1 / Phase 4 quality while preserving the reusable Phase 6 skeleton.

### Code changes

1. Restore legacy cable speaker `ReadBytes` from `UpdatePosition`
2. Restore legacy cable mic `WriteBytes` from `UpdatePosition`
3. Stop the timer callback from dispatching `AoRunRenderEvent` / `AoRunCaptureEvent`
4. Keep:
   - `AO_TRANSPORT_ENGINE`
   - `AO_STREAM_RT`
   - engine init/cleanup
   - stream register/unregister
   - shared timer creation/destruction
   - helper scaffolding that does not move audio
   - the future hook points needed for the cable-stream rewrite

### Validation gate

Do not start `Y` until `Z` passes:

1. build/install succeeds
2. no BSOD / hang / deadlock
3. local loopback returns to Step 1 / Phase 4 level
4. live call regression from Step 3/4 disappears

## Y: redefined Phase 6

### Definition

`Y` means:

**VB-equivalent cable-stream core rewrite**

It does **not** mean:

- "1 ms timer-only engine"
- "shared timer now owns everything"
- "keep MSVAD cable transport and just tune it"
- "port only one helper and leave the rest of the cable path MSVAD-owned"

### Y1: canonical cable runtime + shadow hook

Build the cable-stream runtime and canonical cable advance helper without changing audible movement yet.

Example shape:

```c
AoCableAdvanceByQpc(rt, nowQpc, reason, flags);
```

The helper may compute and record:

- frame delta
- gating result
- would-move frame count
- monotonic accounting state
- diagnostics

But audible cable transport remains legacy during this shadow step.

#### Y1 pass criteria

- no quality change
- no new timing instability
- cable call sources successfully funnel into the new canonical helper
- we can observe one source of truth for cable accounting
- 63/64 drift correction is present in the helper, not deferred
- scratch linearization path exists in the helper design and implementation surface
- overrun guard exists next to the frame gate before audible migration begins

### Y2: render migration

Move cable render transport into the canonical cable advance path.

Rules:

- cable render no longer depends on old MSVAD transport ownership
- helper performs accounting and transport together
- any call source still funnels through the same helper
- old cable-render `UpdatePosition` transport ownership is retired

#### Y2 pass criteria

- local loopback is not worse than `Z`
- no new buzz/static regression
- render path is at least Phase 4 level before moving capture

### Y3: capture migration

Move cable capture transport into the canonical cable advance path.

Reuse the good parts learned from the Step 4 experiment:

- startup arming
- threshold/headroom concepts
- partial-read + tail zero-fill behavior

But keep them inside the update-coupled transport design.

#### Y3 pass criteria

- Phone Link call path improves or at minimum does not regress from `Z`
- startup clipping is reduced
- mid-call chopping is reduced

### Y4: cable MSVAD-remnant cleanup

After Y2/Y3 pass, remove stale remnants from the failed timer-owned attempt.

Cleanup targets:

- timer-owned transport-only assumptions
- stale comments about transport ownership
- unused Step 3/4-only fields/helpers
- old scaffolding that no longer serves the update-coupled design
- cable-stream `m_pNotificationTimer` / `m_pDpc` remnants if no longer required
- cable-stream dependence on old MSVAD periodic position-simulation patterns

## File-level ownership

### `Source/Main/minwavertstream.cpp`

This is now the most important Phase 6 rewrite file.

It owns:

- the authoritative cable-stream update path
- the WaveRT-facing cable entrypoints that will feed the canonical cable advance path
- stream-specific bridging from WaveRT state into the rewritten cable core

This file is where `Y1`, `Y2`, and `Y3` are anchored.
The cable-stream rewrite details, current AO file:line references, and the implementation-phase risk table live in `docs/PHASE6_OPTION_Y_CABLE_REWRITE.md`.

### `Source/Utilities/transport_engine.h`

Owns shared Phase 6 runtime structures and helper APIs:

- `AO_TRANSPORT_ENGINE`
- `AO_STREAM_RT`
- helper declarations for update-coupled transport

Keep the API surface narrow. This file should describe runtime structures, not re-own WaveRT semantics.

### `Source/Utilities/transport_engine.cpp`

Owns reusable helper logic and auxiliary engine lifecycle:

- engine init/cleanup
- stream registration bookkeeping
- helper routines used by the rewritten cable core, including the implementation of `AoCableAdvanceByQpc(...)`
- optional auxiliary timer behavior

This file must not silently become the main transport owner again.

`Source/Main/minwavertstream.cpp` remains the WaveRT-facing callsite owner.
`Source/Utilities/transport_engine.cpp` owns the canonical helper implementation.

### `Source/Utilities/loopback.cpp`

Owns `FRAME_PIPE` and conversion/ring behavior only.

It should not decide transport ownership.

### `Source/Main/adapter.cpp`

Owns engine lifetime at device level:

- create after `FramePipeInit`
- destroy on teardown

No main transport policy belongs here.

## Locking rules

The locking hierarchy remains strict:

1. engine/global bookkeeping lock
2. stream-local position/accounting lock
3. `FRAME_PIPE` lock

Avoid nested ad hoc locking.

The update-coupled helper should do the minimum necessary under each lock and not hold unrelated locks across large data movement.

## Success criteria

Phase 6 is only considered successful if all of the following become true:

1. `Z` restores the Step 1 / Phase 4 baseline behavior
2. `Y2` render migration does not worsen local loopback or reintroduce static
3. `Y3` reduces phone chopping on live call
4. startup clipping is improved, not worsened
5. cable streams no longer depend on the old MSVAD periodic transport pattern
6. transport quality no longer depends on the failed timer-owned Step 3/4 experiment
7. the failed timer-owned Step 3/4 path can be cleanly removed afterward
8. shared-mode phone paths work without making packet-notification parity a Y1 blocker

## One-line summary

Phase 6 is now:

**revert the failed timer-owned transport, then rewrite the cable-stream core so timer and query both funnel into one canonical cable advance path that owns accounting, transport, and position freshness instead of the old MSVAD-derived transport pattern.**
