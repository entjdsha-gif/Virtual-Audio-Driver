# VB-Cable -> AO Reimplementation Plan (Codex)

> **⚠️ CURRENT STATUS — Phases 1–4 complete, Phase 5 reverted, Phase 5c in place.
> See `docs/CURRENT_STATE.md` for the live roadmap including Phase 6 redesign.**
>
> Phase 5 original (GetPositions-driven render ownership, commit `2c733f1`) was
> **reverted** in commit `ed23271` (Phase 5c) because measurement proved GetPositions
> fires at ~130 ms while Phone Link reads Cable B mic at ~1 ms, producing burst+gap
> mid-call chopping. Phase 5 scaffold (ownership flags, counters, IOCTL) is retained
> but the transport call site is back on UpdatePosition packet cadence.
>
> Phase 6 now targets an **independent high-resolution timer** (1 ms ExAllocateTimer)
> as the cadence owner — not GetPositions. The Phase 6 plan in this document
> (capture ownership via query pump) is therefore **architecturally revised**; the
> goal of "driver owns transport cadence" stands, but the mechanism has changed.
> See `CURRENT_STATE.md` § Phase 6 for the authoritative step list.

Date: 2026-04-13  
Owner: Codex  
Status: Planning baseline fixed, ready to implement in phases

Shared operating rules:

- `docs/VB_CABLE_DUAL_PLAN_OPERATING_RULES.md`

This Codex plan remains the authority for architecture, phase order,
acceptance criteria, ownership boundaries, and rollback rules. For the
two-document workflow that coordinates this file with Claude's counterpart,
follow the shared operating-rules file above.

## Document ownership and naming contract

This document name is now fixed:

- Codex-owned plan: `docs/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CODEX.md`

The reserved non-overlapping counterpart name for Claude is:

- Claude-owned plan: `results/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CLAUDE.md`

Do not merge the two into one file. The point is to keep two independent
designs that can later be compared.

---

## Why this document exists

We now have enough material that failure should no longer come from "not
knowing VB". The risk has moved from reverse engineering into design judgment,
translation into AO, and validation discipline.

This plan is therefore not a loose brainstorming note. It is intended to be:

- the Codex-side implementation blueprint
- the decision log for what we will copy behaviorally from VB
- the boundary line for what we will deliberately not copy
- the checklist for implementation order and validation

The goal is:

- reproduce VB-Cable's effective behavior model inside AO
- keep AO's existing strengths when they do not conflict with the verified VB
  behavior
- avoid cargo-culting every VB helper when the helper is only auxiliary

The target is not binary equivalence. The target is behavioral equivalence of
the transport model.

---

## Source corpus used for this plan

This plan is based on all of the material below, not on any single session.

### Static reverse engineering corpus

- `results/ghidra_decompile/vbcable_all_functions.c`
- `results/ghidra_decompile/vbcable_function_index.txt`
- `results/vbcable_disasm_analysis.md`
- `results/vbcable_pipeline_analysis.md`
- `results/vbcable_func26a0.asm`

### Dynamic debugging corpus

- `results/vb_session.log`
- `results/vbcable_runtime_claude.md`
- `docs/VB_CABLE_DYNAMIC_ANALYSIS.md`
- `docs/VB_CABLE_AO_COMPARISON_CODEX_NOTES.md`

### AO current-state corpus

- `docs/AO_V2_ARCHITECTURE_PLAN.md`
- `docs/VBCABLE_SURPASS_PLAN.md`
- `Source/Main/minwavertstream.cpp`
- `Source/Main/minwavertstream.h`
- `Source/Utilities/loopback.cpp`
- `Source/Utilities/loopback.h`

### Confidence ranking

Use this confidence order when conflicts appear:

1. live stack and breakpoint evidence
2. direct static disassembly of the exact function body
3. decompilation summary
4. older notes written before the late-session query-path discovery

---

## Final synthesis of the current VB architecture

This section is the most important part of the document. Everything else follows
from these claims.

### Claim 1. VB is not a simple timer-driven ring pump

This was the early model, and it is no longer sufficient.

What is still true:

- VB has a real shared timer subsystem
- `+0x65b8` registers streams into a shared timer set
- `+0x669c` unregisters them
- `+0x5cc0` is a real shared timer callback

What changed:

- the main observed paired path is not best modeled as
  `timer -> top helper -> write`
- dynamic and static evidence now show that `GetKsAudioPosition` can trigger
  internal elapsed-time update work directly

The safest current model is hybrid:

- main paired path can be lazily advanced by position-query polling
- shared timer subsystem still exists and appears to service auxiliary/shared
  work

### Claim 2. Position query is not passive reporting

This is the biggest architectural update.

Observed stack:

- `portcls!CPortPinWaveRT::GetKsAudioPosition`
- internal VB helper around `+0x5420`
- internal VB helper around `+0x6320`
- `+0x5634`
- `+0x22b0`

Important interpretation note:

- stack return sites such as `+0x54bb` and `+0x64f6` are not separate logical
  stages by themselves
- they are offsets inside the larger helpers `+0x5420` and `+0x6320`
- AO translation must therefore follow whole-function responsibility, not
  cargo-cult a return address as if it were its own subsystem

Static meaning:

- `+0x5420` is the position-query-side helper
- it locks a per-stream position lock
- in RUN state, it obtains timing and calls `+0x6320`
- after the call, it returns `[stream+0xC8]` and `[stream+0xD0]`

Static meaning of `+0x6320`:

- derive elapsed time from current timing fields
- convert elapsed time to frame delta
- subtract previously-accounted frames
- apply the 8-frame minimum gate
- dispatch to `+0x6adc` on one branch or `+0x5634` on the other
- advance internal cursors/accounting fields

Translation caution:

- the observed call convention into `+0x6320` includes an interior-pointer style
  adjustment in some paths
- that means raw offsets seen inside `+0x6320` should not be promoted into an AO
  "VB struct map" without first normalizing them against the true stream base
- the plan should copy verified behavior first and only copy field layouts when
  they are corroborated by whole-function context

Therefore:

- `GetKsAudioPosition` is not merely returning counters that some timer updated
  earlier
- it can actively drive the main path forward

### Claim 3. VB has an explicit 8-frame minimum gate

This is directly visible in `+0x6320`:

- compute frame delta
- subtract previously-accounted amount
- `cmp ebx, 8`
- skip processing when the delta is below 8

Meaning:

- VB deliberately refuses to perform tiny sub-threshold work on every poll
- position-driven progression is made efficient by this gate

This is not optional if we want VB-like behavior.

### Claim 4. The main write path is layered

The observed main write chain is:

- query/update helper
- `+0x5634`
- `+0x22b0`

Roles:

- `+0x5634` is the upper write-side service/helper
- `+0x22b0` is the core write primitive
- `+0x26a0` is the heavy slow/adaptation path under `+0x22b0`
- `+0x4f2c` is side metering/peak logic, not the transport core

Implication:

- AO must not reduce write transport to one monolithic function
- the plan must separate:
  - upper update/service layer
  - core write primitive
  - slow/adaptation path
  - side metering/statistics

### Claim 5. VB state semantics matter

Confirmed semantics:

- `STOP` does not reset the ring
- `PAUSE` conditionally resets
- `RUN` initializes timing and registers the stream into shared timer tracking
- `ACQUIRE` is lightweight

Implication:

- AO should stop treating STOP as the unconditional reset point
- AO should move closer to VB's pause-only conditional reset semantics

### Claim 6. Timer subsystem should remain in AO, but not as the primary engine

We should not swing from one extreme to the other.

Wrong conclusion:

- "timer model was fake, delete the timer"

Safer conclusion:

- keep timer support for auxiliary/shared duties
- do not use timer tick as the primary steady-state engine for the main paired
  transport path

The timer can still be useful for:

- notification/watchdog style duties
- auxiliary stream handling
- envelope/metering side work
- long-idle maintenance

---

## What AO already has that we should keep

This matters because the right plan is not "replace AO with a new toy". The
right plan is "redirect AO into the verified VB behavior with the smallest
dangerous surface".

### Keep as-is or keep conceptually

- on-query `UpdatePosition()` pattern already exists in
  `Source/Main/minwavertstream.cpp`
- `GetPosition()` and `GetPositions()` already call `UpdatePosition()` during
  RUN
- `KeFlushQueuedDpcs` on pause already exists in AO and is directionally
  correct
- fixed frame pipe work in `loopback.cpp` is already much closer to VB than the
  old byte-ring path was
- AO already distinguishes render write and capture read operations
- AO already has format registration and unregistration hooks
- AO already has diagnostics counters and validation infrastructure

### Do not discard these wins

- do not throw away AO's frame-pipe infrastructure
- do not throw away AO's current on-query position recalculation
- do not regress to a byte-ring
- do not reintroduce MicSink-style direct push paths

---

## Where AO is still wrong relative to the new evidence

This section is the translation gap.

### Gap 1. AO still treats periodic progression as more timer-centric than VB's observed main path

Current AO still has cable endpoint progression happening through:

- periodic update flow
- DPC tick assumptions
- read/write path invocation from update logic that is not explicitly modeled as
  a position-query-driven pump

Target:

- query path should become the canonical main progression trigger for the main
  paired transport path

### Gap 2. AO has no explicit 8-frame gate in the VB sense

We need the equivalent of:

- elapsed frame delta accumulation
- subtract already-accounted work
- skip if `< 8`

Without that, aggressive polling will overwork AO and produce different timing
behavior than VB.

### Gap 3. AO still does not cleanly separate transport, adaptation, and metering roles

The VB model is structurally layered.

AO needs a clearer separation between:

- position-driven pump/update logic
- fast transport primitive
- slow/adaptation path
- auxiliary metering/statistics

### Gap 4. AO STOP/PAUSE semantics still differ from VB

If STOP still collapses the pipe more aggressively than VB, we will keep seeing
behavioral mismatches around churn and continuity.

### Gap 5. AO currently risks conflating interface-level position freshness with full transport equivalence

It was true that AO already recalculates position on query.

But the new finding is stronger:

- VB appears to use the query as the place where progression itself is driven

So the new target is:

- not just "fresher position values"
- but "query-time progression and accounting updates"

---

## Target AO architecture

This is the design Codex intends to build toward.

### Layer 1. PortCls query entry remains the public trigger

AO already has:

- `GetPosition()`
- `GetPositions()`

These must remain the externally visible position-query entrypoints.

New rule:

- for cable endpoints in RUN, these functions must call a dedicated internal
  pump helper before returning position

### Layer 2. New internal helper: query-driven pump/update

Codex implementation target:

- add a single helper that becomes the canonical "advance this stream to current
  QPC" function
- implement it as a sibling helper beside the existing WaveRT timing path, not
  as an immediate destructive rewrite of `UpdatePosition()`

Suggested name:

- `CMiniportWaveRTStream::PumpToCurrentPositionFromQuery(...)`

Responsibilities:

- run only for cable endpoints
- run only when state is effectively RUN
- compute elapsed time from current QPC and stored baseline
- convert elapsed time to frame delta
- subtract previously-accounted frames
- apply the 8-frame gate
- reject pathological over-jump deltas
- dispatch to the correct transport branch
- update buffer/sample positions
- update accounting counters

This is the AO equivalent of the observed `+0x5420 -> +0x6320` responsibility
split.

Implementation preference:

- keep the existing public call shape
- keep `UpdatePosition()` alive as the generic WaveRT timing/presentation helper
- add the new transport-driving logic in a dedicated sibling helper
- only retire old cable-endpoint transport from `UpdatePosition()` after the new
  helper has proven parity in shadow mode

### Layer 3. Branch dispatch: render-side vs capture-side

The new pump helper should not do all transport work inline.

It should branch into small internal helpers:

- render/query branch
- capture/query branch

Suggested names:

- `PumpRenderFramesFromQuery(...)`
- `PumpCaptureFramesFromQuery(...)`

These are conceptual names. The exact names can change, but the separation
should remain.

### Layer 4. Transport primitive remains frame-pipe based

Use existing AO frame-pipe core rather than inventing a second transport system.

Mapping to AO:

- upper VB write helper `+0x5634`
  -> AO speaker-side upper helper over DMA buffer and frame pipe
- VB core write `+0x22b0`
  -> AO `FramePipeWriteFromDma`
- VB read path `+0x11d4` / read-side helper
  -> AO `FramePipeReadToDma`

This means AO should reuse its frame-pipe core but change **when** and **why**
it is invoked.

### Layer 5. Timer path becomes auxiliary/shared path

Keep AO timer support, but redefine its role.

Timer should be allowed to handle:

- watchdog-style maintenance
- notification/event cadence if needed
- auxiliary activity
- long idle aging
- optional metering/statistics

Timer should not be treated as the primary source of truth for the main paired
path's frame progression.

---

## Proposed file-level implementation plan

This section is the actionable map.

### 1. `Source/Main/minwavertstream.h`

Add or repurpose per-stream state for the query-driven pump.

Needed concepts:

- QPC/time baseline for the query-driven pump
- accumulated already-accounted frame count
- last returned sample position
- last returned buffer position
- a guard/state bit for whether the query-driven pump baseline is initialized
- optional over-jump/error counters

If existing fields already express part of this state, prefer reuse over adding
new members. Do not create duplicate timing truths unless necessary.

Suggested conceptual fields:

- `m_QueryPumpInitialized`
- `m_QueryPumpBaselineHns` or `m_QueryPumpBaselineQpc`
- `m_QueryPumpAccountedFrames`
- `m_QueryPumpLastBufferPosition`
- `m_QueryPumpLastDmaBufferOffset`
- `m_QueryPumpGatedSkipCount`
- `m_QueryPumpOverJumpCount`
- `m_QueryPumpInvocationCount`
- `m_QueryPumpFramesProcessedTotal`
- `m_QueryPumpShadowDivergenceCount`
- `m_QueryPumpLegacyShadowBytes`
- `m_QueryPumpFeatureFlags`

Whether these become new members or aliases over existing members should be
decided during implementation review.

Recommended feature flags:

- `AO_QUERY_PUMP_ENABLE`
- `AO_QUERY_PUMP_SHADOW_ONLY`
- `AO_QUERY_PUMP_DISABLE_LEGACY_MAIN_PROGRESS`

The goal is not permanent flag sprawl. The goal is to keep rollout and rollback
cheap while we are still proving parity.

Diagnostic discipline:

- if we add or change query-pump diagnostics, update the diagnostic contract in
  one logical change-set
- at minimum this usually means the stream/pipe data structure, the IOCTL
  population path, and the user-facing monitor/test surface must move together
- do not let diagnostic field definitions drift apart across those surfaces

### 2. `Source/Main/minwavertstream.cpp`

This file becomes the heart of the VB-style behavioral shift.

Entry points to modify:

- `GetPosition()` around current on-query `UpdatePosition()` call
- `GetPositions()` around current on-query `UpdatePosition()` call
- `SetState()`
- `UpdatePosition()`

#### `GetPosition()` and `GetPositions()`

New rule:

- when the stream is a cable endpoint and state is RUN, call the new internal
  query-driven pump helper before publishing position

Important:

- keep normal PortCls/WaveRT contract behavior intact
- do not break non-cable endpoints
- do not remove current timestamp/position freshness logic until the new helper
  fully subsumes the needed path

Phase-3 shadow/compare preference:

- during the compare-only phase, call ordering should allow the pump helper to
  compare against the same query's legacy displacement math
- the safest implementation is usually:
  - take the existing position lock
  - run `UpdatePosition()` first
  - stash the same-call legacy byte displacement in a non-contract temporary
    field
  - run the pump helper second in shadow mode
- do not treat single-call divergence as strong evidence by default
  - AO's current `UpdatePosition()` uses time and byte carry-forward, so
    per-call comparison can false-positive on perfectly legal rounding behavior
- prefer a windowed compare in Phase 3
  - accumulate pump-frame totals and legacy-byte totals over a fixed window
  - start with `128` queries per window
  - convert legacy bytes to legacy frames using current `nBlockAlign`
  - compare totals with a starting tolerance of `max(16 frames, 2% of larger)`
  - if the window trips under normal polling, stop and debug before moving any
    transport ownership
- once transport ownership moves to the pump for a cable direction, re-evaluate
  the ordering and keep only one real transport owner active

#### `SetState()`

Target behavior:

- `ACQUIRE`
  - light preparation only
- `RUN`
  - initialize query-pump baseline
  - initialize accounted-frame state
  - register any needed auxiliary timer/watchdog participation
  - register stream format in the frame pipe
- `PAUSE`
  - cancel/quiet auxiliary timer participation
  - flush queued DPCs
  - apply conditional reset only when the VB-equivalent conditions are met
  - do not blindly collapse the whole pipe just because state moved away from
    RUN
- `STOP`
  - clear state ownership and unregister format as needed
  - do not make STOP the automatic hard reset point unless no active paired
    state remains and the higher-level policy explicitly allows it

#### `UpdatePosition()`

Do not delete it outright.

Instead:

- narrow it so it does not remain a competing source of truth for cable endpoint
  progression
- preserve it for the cases where PortCls/WaveRT still expects linear/presentation
  maintenance
- treat the new query-driven pump helper as the owner of "how much real transport
  work must happen now"

Specific preservation rule:

- do not eagerly remove AO's existing carry/timestamp fields just because the
  new pump no longer depends on them for cable transport
- this includes the current `m_ullDmaTimeStamp`,
  `m_hnsElapsedTimeCarryForward`, `m_byteDisplacementCarryForward`,
  `m_ulBlockAlignCarryForward`, and `m_ulDmaMovementRate` model
- those fields still support the legacy `UpdatePosition()` contract, non-cable
  endpoints, and rollback safety during bring-up
- delete or merge them only after the new pump path is proven stable and their
  remaining ownership is explicitly reviewed

Pump-local tracking rule:

- do not permanently derive the pump's DMA buffer offset only from
  `m_ullLinearPosition % buffer_size`
- once the pump owns real transport for a cable direction, let it maintain its
  own DMA-buffer offset/accounting state so long-run drift in WaveRT
  presentation math does not silently steer cable transport to stale data

This is an important distinction:

- `UpdatePosition()` can remain the generic timing/accounting tool
- the new query-driven pump helper becomes the VB-style transport advancement
  tool

Before capture-side ownership moves:

- isolate AO's existing capture fade-in/pop-suppression logic into a reusable
  helper instead of duplicating the loop in multiple places
- the capture pump branch may then call that helper after
  `FramePipeReadToDma()`
- this preserves an AO quality win without keeping cable capture transport tied
  to the old byte-displacement path

### 3. `Source/Utilities/loopback.h`

Review the current `FRAME_PIPE` and add only what is missing for the VB-like
 model.

Potential additions:

- explicit minimum gate configuration constant
- explicit statistics for query-driven drops/over-jumps
- optional persistent carry state for adaptation
- optional fields separating main-paired-path vs auxiliary-timer-path accounting
- optional per-pipe counters for:
  - query-pump writes
  - query-pump reads
  - auxiliary-timer writes
  - gated skips
  - over-jump rejects

Do not bloat the structure unless the information is actually used.

### 4. `Source/Utilities/loopback.cpp`

Current core functions:

- `FramePipeRegisterFormat()` at line 1666
- `FramePipeUnregisterFormat()` at line 1728
- `FramePipeWriteFromDma()` at line 1775
- `FramePipeReadToDma()` at line 1900

Required changes:

#### `FramePipeRegisterFormat()` / `FramePipeUnregisterFormat()`

Adjust semantics so they match the new state model:

- registration belongs to RUN ownership
- unregistration should not imply eager destructive reset unless the higher
  state logic says it is safe

#### `FramePipeWriteFromDma()`

Treat this as AO's core write primitive.

Requirements:

- preserve frame-pipe discipline
- preserve same-rate fast path
- keep adaptation/SRC path separate
- add a clear overflow reject policy matching the VB model direction
- do not use overwrite-oldest

#### `FramePipeReadToDma()`

Treat this as AO's core read primitive.

Requirements:

- preserve silence fill on underrun
- keep format conversion logic separate from outer pump logic
- remain usable from the new query-driven pump branch

### 5. Optional auxiliary timer/watchdog location

If existing timer code lives elsewhere in `minwavertstream.cpp` or adjacent
helpers, do not remove it first.

Instead:

- demote it
- gate it
- narrow it to auxiliary duties

Deletion should come only after equivalent behavior is proven.

---

## Transport ownership matrix

At every phase, we should be able to answer one simple question for each cable
direction: who owns real transport work right now?

If the answer is "more than one place", the phase is unsafe.

### Ownership target by phase

- Phases 0, 0.5, 1, 2:
  - cable render transport owner: legacy AO path
  - cable capture transport owner: legacy AO path
  - query pump: none
  - timer: current AO role
- Phase 3:
  - cable render transport owner: legacy AO path
  - cable capture transport owner: legacy AO path
  - query pump: observer/accounting only
  - timer: current AO role
- Phase 4:
  - cable render transport owner: legacy AO path
  - cable capture transport owner: legacy AO path
  - query pump: observer/accounting only
  - timer: current AO role, but state semantics closer to VB
- Phase 5:
  - cable render transport owner: query pump
  - cable capture transport owner: legacy AO path
  - `UpdatePosition()`: non-cable timing plus non-owned transport only
  - timer: auxiliary/current mixed role
- Phase 6:
  - cable render transport owner: query pump
  - cable capture transport owner: query pump
  - `UpdatePosition()`: non-cable timing plus presentation/accounting only
  - timer: auxiliary/current mixed role
- Phase 7 and later:
  - cable render transport owner: query pump
  - cable capture transport owner: query pump
  - timer: auxiliary/shared duties only

Ownership review rule:

- every phase review must explicitly confirm that a given cable direction is
  advanced by exactly one transport owner

---

## Concrete implementation phases

These phases are ordered to minimize risk.

### Phase 0. Freeze evidence and naming

Deliverables:

- keep this document as Codex-owned source of truth
- keep Claude document separate
- do not overwrite old notes; only supersede them
- confirm via one short WinDbg pre-flight that AO's target scenario really hits
  `GetPosition()` and/or `GetPositions()` on the live call path before wiring
  the pump there

Exit criteria:

- all future implementation decisions can cite a single current Codex plan
- we are not guessing about the AO entrypoint the new helper will hook into

### Phase 0.5. Create a normalized evidence sheet for risky field mappings

Goal:

- prevent implementation bugs caused by copying raw `+0x6320` offsets without
  accounting for interior-pointer adjustment

Tasks:

- create a short side note or appendix table of only the fields that are safe to
  treat as behaviorally confirmed
- mark each offset as one of:
  - `verified direct`
  - `verified after normalization`
  - `behavior only, offset still provisional`
- do not let provisional offsets harden into AO member design unless another
  function body or dynamic trace confirms them

Exit criteria:

- there is a small "safe field map" the implementation can trust
- questionable fields remain explicitly questionable

### Phase 1. Add diagnostics and rollout scaffolding without behavior change

Goal:

- create the visibility and rollback controls needed for a safe rewrite

Tasks:

- add stream-level query-pump counters
- add pipe-level counters where they materially help transport debugging
- add feature flags / ownership switches needed for staged rollout
- initialize and reset those counters without changing transport ownership
- expose counters through existing diagnostic paths where practical
- update the diagnostic producer/consumer surfaces together so the data contract
  stays synchronized

Why this phase exists:

- we should not start moving transport without first making it observable

Exit criteria:

- AO builds
- diagnostics remain sane and zero/idle when the new path is not active
- no transport behavior change is observable

### Phase 2. Land low-risk format parity fixes that do not change transport ownership

Goal:

- apply independently-understood format behavior fixes before the architectural
  rewrite starts muddying root-cause analysis

Tasks:

- land low-risk normalization / denormalization corrections that are already
  supported by the evidence corpus
- keep these fixes tightly scoped to sample-format behavior
- avoid mixing them with state-machine or pump-ownership changes

Examples of suitable work for this phase:

- 32-bit direct-copy parity fixes where already verified
- unsigned 8-bit silence / center-fill parity fixes where already verified

Exit criteria:

- AO builds
- format-focused tests stay green
- if a regression appears, it is clearly attributable to format logic rather
  than timing/state ownership changes

### Phase 3. Introduce query-driven pump skeleton without changing transport

Tasks:

- add helper stub
- initialize/reset its state in `SetState()`
- guard to cable endpoints only
- compute elapsed frame delta
- add 8-frame gate
- publish counters for how often the helper would have pumped
- keep legacy progression path intact
- keep `UpdatePosition()` behavior intact for non-cable and generic WaveRT
  timing responsibilities
- add a shadow-mode comparison path where the new helper computes/account only
  and does not mutate transport
- use an explicit same-call handoff for legacy byte displacement if the shadow
  comparison needs to compare pump frames against legacy byte math
- accumulate comparison results across a shadow window rather than judging every
  single query in isolation
  - suggested starting point: `128` calls per window
  - suggested starting tolerance: `max(16 frames, 2% of the larger total)`
- record divergence counters when new-frame math and old-frame math disagree in
  materially important ways
- choose and document the Phase-3 call order explicitly so shadow comparison
  reads current-call data, not stale previous-call data
- prove that runtime feature-flag rollback turns the helper back into an
  accounting-only no-op without reinstall/reboot
- define the meaning window of divergence counters up front
  - they are parity signals while legacy cable transport still owns the path
  - once a direction becomes pump-owned, freeze or retire live comparison for
    that direction instead of pretending it still measures current parity

Why this phase exists:

- we separate timing-model correctness from transport mutation correctness

Exit criteria:

- AO builds
- no regressions in basic playback/capture open/close
- helper shows sane frame deltas under position polling
- shadow mode proves that gate/accounting logic is stable before it owns real
  transport work
- under a normal 5-minute Phone Link-like run, the windowed divergence counter
  stays at `0`
- the same zero-divergence result holds under at least three polling regimes
  - normal client cadence
  - aggressive stress polling
  - sparse polling
- clearing the Phase-3 feature flag at runtime immediately disables the new
  helper behavior without rebuild/reinstall

### Phase 4. Align state semantics with VB

Goal:

- make STOP/PAUSE/RUN behavior match VB more closely before transport ownership
  moves

Tasks:

- review STOP no-reset semantics
- review PAUSE conditional reset semantics
- keep `KeFlushQueuedDpcs` ordering on pause
- ensure timer/register/unregister ownership is consistent
- before moving unregister out of STOP, trace exactly how `SpeakerActive` /
  `MicActive` are cleared today and ensure the teardown path will not leave stale
  active-state behind
- if needed, make unregister/reset operations idempotent before relocating them
- explicitly test resume after long pause and pause/resume churn under active
  query polling

Why this phase must come before transport ownership migration:

- otherwise state-reset bugs and transport-ownership bugs get entangled and are
  much harder to isolate

Exit criteria:

- state churn no longer causes the old AO-style continuity failures
- STOP is no longer the accidental hard-reset trigger for cable flow
- Phase-3 shadow/divergence counters do not newly regress after the STOP/PAUSE
  changes land

### Phase 5. Move render-side progression behind the query-driven helper

Goal:

- make speaker-side transport progression happen from the query-driven helper

Tasks:

- call into upper render helper from the query-driven pump
- route actual data movement through `FramePipeWriteFromDma()`
- keep overflow as reject, not overwrite
- preserve current save/diagnostic hooks where useful
- leave rollback flag available so render transport can fall back to legacy path
  during bring-up if needed
- explicitly gate off legacy cable-render transport so render does not
  double-advance

Exit criteria:

- render-side data progression works while query path is active
- no byte-ring semantics reintroduced
- no direct old-path overwrite behavior remains
- WinDbg or equivalent confirmation shows render transport fires from exactly
  one owner on the query path
- runtime rollback smoke test passes
  - clearing the render-ownership flag returns render transport to the legacy
    path within one or two position queries

### Phase 6. Move capture-side progression behind the query-driven helper

Goal:

- make mic/capture side progression consistent with the same timing model

Tasks:

- query-driven pump dispatches to read-side helper
- route data movement through `FramePipeReadToDma()`
- preserve silence fill on underrun
- preserve notification behavior only after transport correctness is stable
- keep render/capture branch accounting separate so we can prove direction
  semantics instead of assuming them
- preserve AO's fade-in/pop-suppression behavior by calling the extracted
  fade-in helper from the new capture-owned path
- use the pump-owned DMA buffer offset/accounting path rather than relying only
  on WaveRT linear-position modulo math
- explicitly gate off legacy cable-capture transport so capture does not
  double-advance

Exit criteria:

- capture path produces stable audio under live call scenario
- no stale old-path progression dominates the stream
- fade-in/pop-suppression still audibly works at capture start
- WinDbg or equivalent confirmation shows capture transport fires from exactly
  one owner on the query path
- runtime rollback smoke test passes
  - clearing the capture-ownership flag returns capture transport to the legacy
    path within one or two position queries

### Phase 7. Narrow timer to auxiliary/shared duties and optionally move toward shared-timer parity

Goal:

- preserve timer only where it still provides value

Tasks:

- identify remaining timer-owned responsibilities
- remove timer from main paired progression if still present
- keep auxiliary maintenance/notification/meters on timer only if needed
- publish separate counters for main-query-pump activity vs auxiliary-timer
  activity so we can prove the role split in live runs
- if worth the risk, move from per-stream timer ownership toward a shared-timer
  model only after Phases 5 and 6 are green

Exit criteria:

- main paired path works correctly even when the timer is conceptually auxiliary

### Phase 8. Hardening and validation

Goal:

- prove that the new architecture behaves better, not just differently

Tasks:

- short live calls
- repeated call start/stop churn
- speech/no-speech transitions
- same-rate and mismatched-rate scenarios
- long-run 30min and 60min stability
- monitor drop/underrun/query-pump counters

Exit criteria:

- quality no worse than VB in the target scenario
- no structural regressions against AO's existing strengths

---

## Rollback checkpoints and expected failure modes

This section exists to keep bring-up disciplined. If a phase fails, revert to
the last green checkpoint instead of stacking speculation on top of a broken
state.

### Checkpoint A. After Phase 1

Expected state:

- diagnostics, feature flags, and rollout scaffolding exist
- no cable transport ownership has moved yet

If broken:

- suspect diagnostics plumbing or initialization/reset ownership
- rollback should be cheap because transport ownership is still legacy

### Checkpoint B. After Phase 3

Expected state:

- helper is present
- helper computes/query-accounts only
- no cable transport ownership has moved yet
- windowed shadow comparison is green under at least the baseline polling
  scenario
- runtime feature-flag rollback is still cheap and immediate

If broken:

- suspect helper wiring, state initialization, lock/context assumptions, or bad
  frame math
- suspect per-call compare noise if divergence is rising without a whole-window
  pattern
- rollback should still be cheap because transport ownership is still legacy

### Checkpoint C. After Phase 4

Expected state:

- STOP/PAUSE/RUN churn behaves closer to VB
- STOP no longer acts like the accidental hard-reset trigger

If broken:

- first suspect ownership/lifetime mistakes around unregister/reset timing
- specifically review STOP-vs-destroy responsibilities before changing transport
  math again

### Checkpoint D. After Phase 5

Expected state:

- render-side cable transport ownership has moved behind the query-driven helper
- no double-transport on render path

If broken:

- first suspect duplicated render transport work
- verify legacy render-side calls are truly gated off for cable endpoints
- verify overflow/drop counters before assuming SRC or state-machine faults

### Checkpoint E. After Phase 6

Expected state:

- capture-side cable transport ownership has moved behind the query-driven helper
- fade-in, underrun silence, and wrap handling still behave correctly

If broken:

- first suspect capture wrap math, bypassed fade-in handling, or double-advance
- verify capture is not simultaneously advanced by old and new paths

### Checkpoint F. After Phase 7

Expected state:

- timer is auxiliary/shared
- main paired path is still healthy without timer-owned main progression

If broken:

- first suspect that timer still owns a hidden main-path responsibility that has
  not yet been rehomed
- restore the previous checkpoint and re-classify the timer duty instead of
  forcing deletion

---

## Validation matrix

This is the minimum matrix before declaring the architecture change successful.

### Functional validation

- cable endpoints enumerate and open cleanly
- `GetPosition()` and `GetPositions()` remain valid
- live Phone Link call starts and stays on PC path
- no crash during pause/resume/stop/run churn

### Transport validation

- speaker speech appears at mic side with no periodic tearing
- same-rate path works without SRC dependence
- mismatched-rate path still functions through fallback path
- underrun produces silence, not stale or garbage data
- overflow rejects cleanly and increments counters
- ownership rollback works at runtime for any direction already moved to the
  pump

### Timing validation

- 8-frame gate visibly suppresses tiny-query overprocessing
- query-driven frame advancement follows polling cadence without drift explosions
- long run does not accumulate obvious position divergence
- over a long run, pump-owned DMA offset stays within a small bounded distance
  from WaveRT presentation math rather than drifting monotonically
  - recommended first check: sample every 60 seconds over a 30-minute run
  - if drift grows in one direction only, debug conversion/accounting before
    adding "drift nudge" logic

### Polling-regime validation

- aggressive polling:
  - stress `GetPosition()` / `GetPositions()` frequency and confirm gated-skip
    counters rise while audio stays stable
  - in shadow mode, the windowed divergence counter should still stay at `0`
    under healthy math; if it only rises under hammer polling, suspect handoff
    or conversion bugs first
- normal polling:
  - validate expected live-call behavior under Phone Link or equivalent real
    client cadence
- sparse polling:
  - confirm main paired path still behaves acceptably when position queries are
    less frequent than ideal
  - sparse cadence should not create unexplained over-jump noise in a healthy
    build
- near-no polling:
  - confirm auxiliary timer/watchdog duties do not accidentally disappear if the
    public client goes quiet for a while
- mixed polling:
  - multiple clients querying position at different cadences should not create
    double-advance bugs

### Regression validation

- 8ch mode
- 16ch mode
- multi-client
- existing IOCTL/config workflows
- install/upgrade/uninstall flows

---

## Non-negotiable implementation rules

These are the rules that should not be compromised without a strong reason.

### Rule 1. No overwrite-oldest write policy

If AO needs to mimic VB, overflow must reject or skip, not silently advance the
 read pointer to make room.

### Rule 2. No MicSink-style direct transport bypass

All real transport should flow through the frame-pipe model.

### Rule 3. Position polling must become behaviorally meaningful

It is not enough to return fresher timestamps. Query-time progression must
 actually matter for cable endpoints.

### Rule 4. 8-frame gate must exist

This is now directly supported by evidence and should be treated as part of the
 architecture, not an optional micro-optimization.

### Rule 5. Timer remains subordinate to the main paired path

Keep it as auxiliary/shared support, not the primary observed engine.

### Rule 6. State semantics must move toward VB, not away from it

Especially STOP vs PAUSE.

### Rule 7. Rollout must preserve a rollback path until parity is proven

Do not force the driver onto the new query-pump transport path without:

- a feature switch
- shadow-mode accounting
- divergence counters
- a way to temporarily fall back during bring-up

### Rule 8. Behavior beats offsets when the two disagree

If a raw offset interpretation from a tricky helper conflicts with the
whole-function behavior model, trust the behavior model until the offset is
revalidated.

### Rule 9. Preserve existing WaveRT timing semantics until replacement is proven

The new query-driven pump is allowed to take ownership of cable transport.

It is not allowed to casually break:

- generic `UpdatePosition()` timing behavior
- non-cable endpoints
- packet/presentation bookkeeping that still depends on the old carry model

### Rule 10. Preserve AO wins unless they directly conflict with verified VB behavior

Examples:

- capture fade-in/pop suppression
- existing diagnostics discipline
- non-cable fallback behavior

If AO has a quality or safety improvement that does not conflict with verified
VB transport behavior, keep it.

### Rule 11. Each cable direction must have exactly one transport owner per phase

At no point should the same cable direction be advanced by both:

- the new query-driven pump
- and the legacy cable transport path

If ownership is ambiguous, the phase is not ready.

### Rule 12. Diagnostic contract changes must be atomic

If we add or rename query-pump diagnostics, update the producer and every
consumer surface together.

Do not land half-updated diagnostics that make monitoring lie during bring-up.

### Rule 13. Pump transport must not depend forever on WaveRT presentation offset

During shadow mode, borrowing legacy position/accounting is acceptable.

After a cable direction is pump-owned, the pump should maintain its own transport
offset truth for that direction.

### Rule 14. Shadow comparison should be windowed, not naively per-call

`UpdatePosition()` currently uses carry-forward math in both time and bytes.

That makes single-query comparison too noisy to serve as a bring-up truth source.

Use a rolling window in Phase 3 so we catch systematic drift without mistaking
rounding noise for a real architecture bug.

### Rule 15. Ownership-moving phases must prove runtime rollback

For Phase 5 and Phase 6, a feature flag is not enough on paper.

The plan is only valid if we actually prove that clearing the ownership flag
returns the affected direction to the legacy path during a live session without
reinstall/reboot.

---

## Things we should explicitly not do

These are common failure modes for a rewrite like this.

- do not rewrite the entire driver in one patch
- do not delete timer code first and ask questions later
- do not conflate metering/statistics helpers with transport primitives
- do not assume every helper that appears on a stack owns the whole path
- do not rewrite `UpdatePosition()` into a cable-only transport god-function
- do not force all transport into `UpdatePosition()` without preserving API
  expectations of `GetPosition()` / `GetPositions()`
- do not let both the pump and the legacy cable path advance the same direction
  at once
- do not treat the presence of on-query position recalculation in AO as proof
  that AO already matches VB
- do not let STOP become the accidental pipe reset trigger
- do not delete existing carry/timestamp fields before proving they are dead
- do not regress AO-only quality wins just to make the code look more like VB

---

## Open questions that should not block phase 1

These are still worth answering, but they should not stop the rewrite.

- exact extent of timer-owned auxiliary duties in active steady-state
- exact render/capture flag semantics around some stream fields
- whether AO needs a dedicated second lock for query-pump semantics beyond the
  current position lock discipline
- whether certain notification details should be aligned now or only after
  transport stability is proven
- whether a temporary "legacy progression fallback" switch should remain in
  checked builds even after release builds stop using it
- whether the final pump-local DMA offset can reuse an existing AO field or
  should remain a dedicated member for clarity

None of these questions invalidate the main architecture direction.

---

## Codex implementation order recommendation

If we start coding immediately, do it in this order:

1. add diagnostics and rollout/ownership flags before moving transport
2. verify the live AO entrypoint with WinDbg before hooking the pump there
3. prepare query-pump state and helper shell in `minwavertstream.*`
4. land low-risk format parity corrections that are independent of timing/state
5. wire helper into `GetPosition()` and `GetPositions()` in observer/shadow mode
6. implement frame-delta + 8-frame gate + counters
7. make the Phase-3 shadow compare windowed instead of single-call
8. prove Phase-3 flag rollback and divergence stability before touching
   ownership
9. align STOP/PAUSE/RUN semantics and resolve unregister/active-flag ownership
10. extract and preserve capture fade-in/smoothing helper for later reuse
11. move render-side progression into the helper using `FramePipeWriteFromDma()`
12. prove render runtime rollback and single-owner behavior
13. move capture-side progression into the helper using pump-owned DMA offseting
    plus `FramePipeReadToDma()`
14. prove capture runtime rollback and single-owner behavior
15. demote timer to auxiliary/shared role
16. run live-call validation and counter review

This order reduces the chance that we lose both transport correctness and
timing correctness at the same time.

---

## Bottom line

The old simplified belief was:

- VB has a timer
- therefore VB is timer-driven

The stronger current model is:

- VB has a timer
- but the main observed paired transport path appears to be lazily advanced by
  position-query polling
- and VB protects that model with an explicit 8-frame gate

Therefore the AO rewrite should be built around:

- query-driven pump/update for the main paired path
- frame-pipe transport primitives as the actual data movers
- timer as auxiliary/shared support
- VB-like STOP/PAUSE semantics

If we follow this plan carefully, we are no longer guessing about the core
transport architecture. We are translating a behaviorally grounded model into AO.
