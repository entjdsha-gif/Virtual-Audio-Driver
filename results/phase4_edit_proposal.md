# Phase 4 - Edit Proposal (pre-implementation)

Date: 2026-04-13
Author: Codex
Status: PROPOSAL - approved, ready for checkpoint
Scope: Phase 4 of the `feature/ao-fixed-pipe-rewrite` parity-first rewrite.

## Approval record

Locked 2026-04-13 after Codex/Claude drift check against current Phase 3 source.

Accepted as-is:
- Section 4.1 teardown unregister location = destructor
- Section 4.2 teardown ordering = timer delete / `KeFlushQueuedDpcs()` ->
  unregister -> miniport release
- Section 4.3 PAUSE reset gate = conservative `!otherSideActive`
- Section 4.4 fade-in extraction deferred out of the base Phase 4 commit
- Section 7 recommended answers accepted without modification

Implementation note:
- destructor reorder is the highest-risk edit in this phase because it changes
  lifetime ordering for both cable and non-cable streams.
- that risk is acceptable inside Phase 4 because STOP unregister removal is not
  safe unless unregister ownership moves to a real teardown edge in the same
  commit.

This document is intended to:
- reload the authoritative Codex Phase 4 scope before any edits,
- reconcile current `SetState()` / teardown code against the Codex and Claude
  plans,
- separate true state-semantics work from later transport-ownership work,
- identify the smallest safe edit set that removes STOP-era pipe collapse
  without masking Phase 3 pump regressions,
- surface the few remaining lifecycle decisions that should be frozen before
  code edit.

Plan sources (operating-rules order):
- Codex baseline: `docs/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CODEX.md`, Phase 4
- Claude execution notebook: `results/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CLAUDE.md`, Phase 4
- Runtime baseline: Phase 3 closed at commit `b4d5e71`
- Operating rules: `docs/VB_CABLE_DUAL_PLAN_OPERATING_RULES.md`

---

## 1. Scope and non-goals

### 1.1 Goal

Make AO's STOP/PAUSE/RUN behavior match VB more closely before any transport
ownership move:

- STOP must no longer be the accidental pipe-collapse trigger,
- PAUSE must own the conditional reset behavior,
- teardown/unregister responsibility must move to a real stream-lifetime edge,
- Phase 3 shadow math must remain green after the state change.

### 1.2 In scope

- Remove `FramePipeUnregisterFormat(...)` from `SetState(KSSTATE_STOP)`.
- Add VB-like conditional `FramePipeReset(...)` to the PAUSE transition from
  RUN, after timer cancel and `KeFlushQueuedDpcs()`.
- Clear per-run pump baseline/latch state on PAUSE so the next RUN starts with a
  fresh timing baseline, while preserving monotonic Phase 3 evidence counters.
- Move `FramePipeUnregisterFormat(...)` to a true teardown path.
- Add changelog entry documenting the Phase 4 state-semantics landing.

### 1.3 Out of scope

- No transport ownership move.
- No Phase 5 render path changes.
- No Phase 6 capture path changes.
- No new IOCTL layout or `test_stream_monitor.py` schema change.
- No timer-role redesign.
- No change to Phase 3 hook target (`GetPositions()` remains the query hook).
- No change to Phase 3 over-jump/divergence semantics except preserving them
  across the new state flow.
- No fade-in extraction in this proposal by default; keep Phase 4 focused on
  state semantics only.

### 1.4 Non-negotiable guardrails

- Phase 3 stays SHADOW_ONLY after Phase 4 lands.
- `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` and
  `AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE` must remain clear.
- `KeFlushQueuedDpcs()` ordering in PAUSE must remain intact.
- STOP must not call `FramePipeUnregisterFormat(...)`.
- Any teardown-time unregister must happen only after queued DPC/timer activity
  can no longer race the pipe state.
- Phase 3 divergence must not regress due to Phase 4 state changes.

### 1.5 Exit criteria

Authoritative Phase 4 exit criteria remain the Codex rules:

1. State churn no longer causes the old AO-style continuity failures.
2. STOP is no longer the accidental hard-reset trigger for cable flow.
3. Phase 3 shadow/divergence counters do not newly regress after STOP/PAUSE
   changes land.

Operationally, this should also mean:

- build/install green,
- `test_ioctl_diag.py` still green,
- idle monitor still sane,
- live call or equivalent churn scenario is no worse than Phase 3 and ideally
  better during STOP/PAUSE/RUN transitions.

---

## 2. Plan versus current code drift

### 2.1 STOP still unregisters and collapses the pipe

Current code in `SetState(KSSTATE_STOP)` still does:

- resolve `g_CableAPipe` / `g_CableBPipe`
- call `FramePipeUnregisterFormat(...)`
- which clears `SpeakerActive` / `MicActive`
- and if both sides are inactive, immediately calls `FramePipeReset(...)`

That is the opposite of the desired Phase 4 direction.

Consequence:
- STOP currently remains the accidental hard-reset trigger,
- so Phase 4 must start by removing STOP-time unregister.

### 2.2 PAUSE still has no conditional pipe reset

Current PAUSE path already has:

- `if (m_KsState > KSSTATE_PAUSE)` guard,
- timer cancel,
- `KeFlushQueuedDpcs()`,
- carry-forward bookkeeping,
- `GetPositions(NULL, NULL, NULL)`

But it does not touch the pipe.

Consequence:
- the VB-like conditional reset belongs here, not in STOP.

### 2.3 Destructor currently does not own format unregistration

`CMiniportWaveRTStream::~CMiniportWaveRTStream()` currently:

- calls `StreamClosed(...)`,
- releases `m_pMiniport`,
- deletes timer/DPC resources,
- flushes queued DPCs,
- exits

It does not call `FramePipeUnregisterFormat(...)`.

Consequence:
- if STOP no longer unregisters, Phase 4 must introduce a real teardown
  unregister path.
- destructor ordering matters because the current destructor releases
  `m_pMiniport` too early for a later device-type-based pipe lookup.

### 2.4 Active-flag ownership is the main lifecycle risk

`FramePipeUnregisterFormat(...)` currently owns:

- `SpeakerActive = FALSE` or `MicActive = FALSE`
- `ActiveRenderCount--` for speaker
- conditional pipe reset when both directions are inactive

If unregister moves out of STOP, then between STOP and actual destruction:

- active flags may remain TRUE,
- PAUSE's future `otherSideActive` check may see stale TRUE,
- which may suppress a legitimate reset.

Consequence:
- this is the primary blocker to document and verify in Phase 4.
- safer default: accept conservative "stale active means skip reset" behavior
  over any aggressive reset that could wipe a still-running peer.

### 2.5 Phase 3 already added STOP-time pump resets that are broader than needed

Current STOP branch clears:

- flags,
- latch/baseline/window state,
- per-session counters,
- monotonic counters,
- and snapshots zeroed feature flags to the pipe.

Current PAUSE branch does not reset the pump baseline/latch at all.

Consequence:
- Phase 4 should rebalance this:
  - PAUSE should clear baseline/latch working state,
  - monotonic evidence counters should survive PAUSE -> RUN,
  - STOP may still clear stream-local state, but should not own the pipe reset.

### 2.6 `FramePipeUnregisterFormat(...)` is already close to idempotent

Current implementation:

- safely checks `ActiveRenderCount > 0` before decrement,
- sets active flags directly,
- conditionally resets only when both inactive.

Repeated calls would mostly collapse to:

- active flags already false,
- count not decremented below zero,
- optional extra `FramePipeReset(...)` when both inactive.

Consequence:
- a Phase 4 move to teardown does not appear blocked on a major loopback API
  rewrite.
- we should still treat repeated unregister as "expected safe," not formally
  proven, and validate it.

---

## 3. File-by-file edit plan

Expected code touch set:

1. `Source/Main/minwavertstream.cpp`
2. `docs/PIPELINE_V2_CHANGELOG.md`

No header or IOCTL changes are expected in the base Phase 4 proposal.

### 3.1 `Source/Main/minwavertstream.cpp` - destructor ordering and teardown unregister

Anchor:
- `CMiniportWaveRTStream::~CMiniportWaveRTStream()` near file top

Required change:
- move cable-pipe unregister responsibility into destructor teardown

Recommended teardown order:

1. determine whether this stream is a cable endpoint and which pipe/direction it
   maps to
2. cancel/delete notification timer if present
3. `KeFlushQueuedDpcs()`
4. call `FramePipeUnregisterFormat(pFP, isSpeaker)` for cable streams
5. then call `StreamClosed(...)` / release miniport / free other resources

Why this order:
- avoids using `m_pMiniport` after release,
- ensures queued DPC/timer work is not still advancing the pipe while unregister
  flips active flags or resets ring state,
- keeps unregister at a true lifetime edge rather than a state-transition edge.

### 3.2 `Source/Main/minwavertstream.cpp` - STOP branch

Anchor:
- `SetState(KSSTATE_STOP)` around current lines 1223-1318

Required change:
- remove the `FramePipeUnregisterFormat(...)` block entirely

STOP should still own:

- stream-local DMA/timing reset,
- WaveRT packet position reset,
- per-stream end-of-stream flags reset,
- pump feature flags reset,
- stream-local pump working-state clear

STOP should no longer own:

- pipe unregister,
- pipe active-flag transitions,
- pipe reset.

Recommendation on stream counters in STOP:
- keep current STOP clearing of stream-local pump counters as-is for now,
  unless Phase 4 edit proves it interferes with validation.
- Phase 4 is about pipe/state semantics first, not stream-counter policy.

### 3.3 `Source/Main/minwavertstream.cpp` - PAUSE branch

Anchor:
- `SetState(KSSTATE_PAUSE)` around current lines 1336-1373

Required change:
- after `KeFlushQueuedDpcs()` and within the `m_KsState > KSSTATE_PAUSE` path,
  resolve the cable pipe and conditionally call `FramePipeReset(pFP)`

Recommended conditional reset rule:

1. only if this transition came from RUN (`m_KsState > KSSTATE_PAUSE`)
2. only if timer path had been active (`m_ulNotificationIntervalMs > 0`)
3. only for cable endpoints
4. only if `pFP` exists and `pFP->Initialized`
5. only if the opposite direction is not active:
   - current render stream pausing -> inspect `pFP->MicActive`
   - current capture stream pausing -> inspect `pFP->SpeakerActive`

This gives the conservative Phase 4 rule:
- if the other side looks active, do not reset the pipe
- if the other side is inactive, reset is allowed

Also clear pump working state on PAUSE:

- `m_ulPumpFeatureFlags = 0`
- `m_bPumpInitialized = FALSE`
- `m_ullPumpBaselineHns = 0`
- `m_ulPumpProcessedFrames = 0`
- `m_ulPumpLastBufferOffset = 0`
- `m_ullPumpShadowWindowPumpFrames = 0`
- `m_ullPumpShadowWindowLegacyBytes = 0`
- `m_ulPumpShadowWindowCallCount = 0`
- `m_ulLastUpdatePositionByteDisplacement = 0`

Recommended preservation on PAUSE:
- keep monotonic counters intact:
  - `m_ulPumpInvocationCount`
  - `m_ulPumpShadowDivergenceCount`
  - `m_ullPumpFramesProcessed`
- keep per-pipe monotonic totals intact

Why:
- Phase 3 evidence should survive PAUSE -> RUN churn,
- the next RUN should still start from a fresh timing baseline.

### 3.4 `Source/Main/minwavertstream.cpp` - RUN branch

Anchor:
- `SetState(KSSTATE_RUN)` around current lines 1375 onward

Base Phase 4 plan:
- leave registration-on-RUN intact
- leave Phase 3 shadow-only flag arming intact
- no ownership-bit changes

Only expected adjustment here:
- make sure PAUSE-cleared pump state is correctly re-armed on RUN,
- keep feature flag snapshot logic intact.

### 3.5 `Source/Main/minwavertstream.cpp` - explicitly no transport edits

Do not touch in Phase 4:

- `UpdatePosition()` transport behavior
- `WriteBytes()`
- `ReadBytes()`
- query-pump helper transport ownership

If an edit proposal needs to touch any of these for actual data movement, it has
escaped into Phase 5/6 and should stop.

### 3.6 `docs/PIPELINE_V2_CHANGELOG.md`

Add Phase 4 entry recording:

- STOP no longer unregisters format
- PAUSE now conditionally resets pipe after `KeFlushQueuedDpcs()`
- cable teardown owns unregister
- Phase 3 shadow-only pump remains unchanged

---

## 4. Recommended defaults to lock before editing

### 4.1 Teardown unregister location

Recommended default:
- destructor teardown path

Reason:
- current code already has a concrete stream-lifetime edge there,
- no new public lifecycle hook is needed,
- the destructor already flushes queued DPCs, which is the right safety barrier
  for pipe unregister.

### 4.2 Teardown unregister ordering

Recommended default:
- timer delete / DPC flush before unregister, unregister before miniport release

Reason:
- avoids stale in-flight callback races,
- still has access to `m_pMiniport->m_DeviceType` while resolving pipe.

### 4.3 PAUSE reset gate

Recommended default:
- use the conservative `!otherSideActive` rule described in Section 3.3

Reason:
- under-reset is safer than wiping a peer that may still be running,
- stale TRUE blocks reset but does not destroy in-flight peer state,
- this matches Codex's caution around active-flag ownership.

### 4.4 Fade-in extraction

Recommended default:
- defer to a later pure-refactor checkpoint, not the base Phase 4 state
  semantics commit

Reason:
- keeps this phase tightly focused on STOP/PAUSE semantics,
- avoids mixing a refactor into the same commit as the state-machine change,
- can still be done before Phase 6 if we want a separate `Phase 4a`.

---

## 5. Verification plan

### 5.1 Bring-up checks

1. `build-verify.ps1 -Config Release`
2. `install.ps1 -Action upgrade`
3. `python test_ioctl_diag.py`
4. `python test_stream_monitor.py --once`

Expected:
- build green
- install green
- V1/V2 diagnostics intact
- idle counters sane

### 5.2 State-semantics checks

1. STOP path no longer calls `FramePipeUnregisterFormat(...)`
   - log or WinDbg confirmation
2. PAUSE from RUN can trigger `FramePipeReset(...)` when the opposite direction
   is not active
3. PAUSE from RUN does not collapse a still-active peer direction
4. Phase 3 divergence does not newly tick during state churn
5. pause/resume churn remains no worse than Phase 3 baseline

### 5.3 Live validation

Recommended active validation sequence:

1. active stream / live-call scenario
2. repeated PAUSE -> RUN transitions
3. STOP -> RUN transition
4. monitor:
   - `PumpShadowDivergenceCount`
   - `DropCount`
   - `UnderrunCount`
   - feature flags / helper activity

Expected:
- no new divergence growth caused by state ordering,
- fewer or no STOP-era pipe-collapse symptoms,
- transport quality still broadly Phase 3-like because ownership has not moved.

---

## 6. Rollback

Expected rollback surface:

- `Source/Main/minwavertstream.cpp`
- `docs/PIPELINE_V2_CHANGELOG.md`

Rollback plan:
1. `git revert <phase4-commit>`
2. rebuild
3. reinstall
4. verify Phase 3 counters and live-call baseline return to pre-Phase-4 state

Phase 4 is not flag-gated, so rollback is source-level, not runtime-flag-based.

---

## 7. Resolved items

### 7.1 Final teardown hook

Decision:
- use destructor first
- validate with logs/WinDbg
- only escalate to an alternate teardown point if destructor clearly does not
  run in the target lifecycle

### 7.2 STOP counter-clearing breadth

Decision:
- do not change this in Phase 4 unless validation proves it blocks useful
  observability
- keep Phase 4 focused on pipe semantics first

### 7.3 Fade-in extraction

Decision:
- keep it out of the base Phase 4 state-semantics commit
- revisit as a tiny follow-up `Phase 4a` only if we want the refactor before
  Phase 6

---

## 8. What this document is not

This document is not:

- a render ownership proposal
- a capture ownership proposal
- a timer-role redesign
- a new pump-math proposal
- a format parity proposal
- a generic cleanup pass

If any edit requires:

- transport ownership flags to change,
- `FramePipeWriteFromDma()` / `FramePipeReadToDma()` from the pump,
- new IOCTL fields,
- or new monitor schema,

then it has escaped Phase 4 and should stop.

---

## 9. Next actions

1. Lock the recommended defaults in Section 4 and answers in Section 7.
2. If desired, checkpoint this proposal in its own commit.
3. Edit in this order:
   - `Source/Main/minwavertstream.cpp`
   - `docs/PIPELINE_V2_CHANGELOG.md`
4. Run build/install/IOCTL/monitor verification.
5. Run active STOP/PAUSE/RUN churn validation.
6. Only if Phase 3 remains green and state-churn behavior improves, commit
   Phase 4.

---

## 10. Proposed Phase 4 commit message

```text
Phase 4: align STOP/PAUSE state semantics with VB before ownership move
```
