# Phase 3 - Edit Proposal (pre-implementation)

Date: 2026-04-13
Author: Codex
Status: PROPOSAL - revised after first bring-up false-green finding
Scope: Phase 3 of the `feature/ao-fixed-pipe-rewrite` parity-first rewrite.

## Approval record

Locked 2026-04-13, then revised after Phase 3 first bring-up produced a
false-green shadow result.

Revision reason:
- first active polling run proved that helper wiring, flags, and hot-path hook
  were correct,
- but almost every call hit the over-jump guard,
- which prevented the rolling shadow window from ever flushing,
- so `PumpShadowDivergenceCount == 0` was not yet a meaningful parity signal.

Revision principle:
- in Phase 3 SHADOW_ONLY mode, over-jump remains a diagnostic counter, not a
  compare-suppressing stop condition.
- real skip/clamp semantics belong to Phase 5/6, where the pump starts owning
  transport.

Scope lock:
- Hook target: `CMiniportWaveRTStream::GetPositions()` ONLY.
- Mode: SHADOW_ONLY. `AO_PUMP_FLAG_ENABLE | AO_PUMP_FLAG_SHADOW_ONLY` set on cable RUN.
- No ownership move. `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` /
  `AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE` remain clear in Phase 3.
- Helper is invoked from `GetPositions()` in the same call, immediately after
  `UpdatePosition(ilQPC)`, still under `m_PositionSpinLock`.
- `TimerNotifyRT` is untouched in Phase 3.
- `GetPosition()` is untouched in Phase 3.
- No IOCTL shape change. No `test_stream_monitor.py` schema change.
- Phase 3 closure metric: windowed divergence counter stays `0` under normal,
  aggressive, and sparse polling.

Default lock:
1. `AO_PUMP_SHADOW_WINDOW_CALLS = 128`
2. Divergence tolerance = `max(16 frames, 2% of the larger total)`
3. Over-jump threshold = `max(framesPerDmaBuffer * 2, sampleRate / 4)`
4. Cable-endpoint guard lives INSIDE `PumpToCurrentPositionFromQuery()`
   (single source of truth; call sites stay simple).

Semantic lock:
- Phase 3 over-jump is count-only diagnostic.
- Over-jump increments `m_ulPumpOverJumpCount` but does NOT block window
  accumulation or shadow compare.
- Phase 3 closure requires at least one real shadow-window flush during active
  validation; `divergence == 0` without a flush is not considered green.

Any deviation from the above requires a new proposal revision before edit.

This document is intended to:
- reload the authoritative Codex Phase 3 scope before any edits,
- reconcile current source state against both the Codex and Claude plans,
- freeze the non-negotiable Phase 3 guardrails,
- identify the smallest file touch set that lands shadow-mode pump logic only,
- surface the few remaining scope decisions that should be locked before code edit,
- keep Phase 5/6 ownership work completely out of this phase.

Plan sources (operating-rules order):
- Codex baseline: `docs/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CODEX.md`, Phase 3
- Claude execution notebook: `results/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CLAUDE.md`, Phase 3
- Runtime evidence: Phase 0 closed at commit `852ca16`
- Current source baseline: Phase 2 closed at commit `4878d92`
- Operating rules: `docs/VB_CABLE_DUAL_PLAN_OPERATING_RULES.md`

---

## 1. Scope and non-goals

### 1.1 Goal

Introduce a query-driven pump helper on the confirmed hot path,
`CMiniportWaveRTStream::GetPositions(...)`, in SHADOW_ONLY mode:

- compute frame delta from current QPC,
- apply 8-frame gate,
- apply over-jump guard,
- compare pump math against same-call `UpdatePosition()` byte math over a rolling
  window,
- publish counters through the Phase 1 diagnostic surface,
- make no transport change.

Phase 3 exists to prove timing/accounting parity before render/capture ownership
moves in Phase 5 and Phase 6.

### 1.2 In scope

- Add `PumpToCurrentPositionFromQuery(LARGE_INTEGER ilQPC)` as a private helper.
- Call it from `GetPositions()` only, after `UpdatePosition(ilQPC)` and while
  still under `m_PositionSpinLock`.
- Keep helper behavior cable-only and feature-flag gated.
- Re-arm helper baseline/latch state in `SetState(KSSTATE_RUN)`.
- Clear helper runtime state in `SetState(KSSTATE_STOP)`.
- Stash same-call `ByteDisplacement` from `UpdatePosition()` into the existing
  `m_ulLastUpdatePositionByteDisplacement` field.
- Increment stream counters and mirror them into the existing per-direction
  `FRAME_PIPE` counters that Phase 1 already exposed through V2 diag.
- Add a changelog entry documenting the Phase 3 shadow-only landing.

### 1.3 Out of scope

- No transport ownership move.
- No calls to `FramePipeWriteFromDma()`, `FramePipeReadToDma()`, `WriteBytes()`,
  or `ReadBytes()` from the new helper.
- No edits to `GetPosition()`; Phase 0 runtime evidence showed it as a cold path
  for the target scenario.
- No timer-path hook; `TimerNotifyRT` remains legacy and unchanged in Phase 3.
- No IOCTL shape change.
- No `test_stream_monitor.py` schema change.
- No state-semantics rewrite beyond the minimum flag/reset wiring needed for
  shadow-mode bring-up.
- No Phase 5/6 `AO_PUMP_FLAG_DISABLE_LEGACY_*` enablement.

### 1.4 Non-negotiable guardrails

- Phase 3 is SHADOW_ONLY.
- `AO_PUMP_FLAG_ENABLE | AO_PUMP_FLAG_SHADOW_ONLY` may be set.
- `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` and
  `AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE` must remain clear.
- `UpdatePosition()` remains the only transport owner in this phase.
- Pump helper performs compute, compare, and counter publication only.
- Over-jump in Phase 3 is diagnostic only; it must not suppress shadow compare.
- Runtime rollback must still be a simple feature-flag clear, not a reinstall.

### 1.5 Exit criteria

Authoritative Phase 3 exit criteria remain the Codex rules:

1. AO builds green.
2. No regression in basic playback/capture open/close.
3. Helper shows sane frame deltas under position polling.
4. Shadow mode proves gate/accounting stability before any transport ownership
   move.
5. Under a normal 5-minute Phone Link-like run, the windowed divergence counter
   stays at `0`.
6. The same zero-divergence result holds under at least three polling regimes:
   normal, aggressive, and sparse.
7. At least one real rolling-window compare executes during active validation;
   `0` divergence with zero flushes does not pass.
8. Clearing the Phase 3 feature flag at runtime immediately turns the helper
   into an accounting-only no-op.

### 1.6 Idle expectations

At idle, after Phase 3 lands:

- `PumpInvocationCount == 0`
- `GatedSkipCount == 0`
- `OverJumpCount == 0`
- `PumpShadowDivergenceCount == 0`
- `FramesProcessedTotal == 0`
- `PumpFeatureFlags == 0x00000000` unless the stream is actively transitioned
  into RUN

The Phase 1/2 expectation remains intact: idle counters stay zero.

---

## 2. Plan versus current code drift

### 2.1 Hook target is `GetPositions()` only

Current runtime evidence is no longer hypothetical:

- Phase 0 closed with `aocablea!CMiniportWaveRTStream::GetPositions` firing
  120 times over 5 seconds during a real Phone Link call.
- `GetPosition()` was cold in that same run.

Consequence:
- Phase 3 should hook `GetPositions()` only.
- `GetPosition()` remains untouched in this phase.

This overrides stale Claude-plan wording that still discusses wiring both
`GetPosition()` and `GetPositions()`.

### 2.2 The same-call handoff field already exists

Phase 1 already landed:

- `m_ulLastUpdatePositionByteDisplacement`

in `Source/Main/minwavertstream.h`.

Consequence:
- Phase 3 must use the existing field.
- Do not add it again.
- Proposal/code should only define when it is written, consumed, reset, and
  validated.

### 2.3 Phase 1 already landed the required diagnostic contract

Current source already contains:

- stream-level pump counters in `CMiniportWaveRTStream`,
- per-direction pipe counters in `FRAME_PIPE`,
- V2 IOCTL exposure in `adapter.cpp`,
- display support in `test_stream_monitor.py`.

Consequence:
- Phase 3 should not change IOCTL shape.
- Phase 3 should not change `test_stream_monitor.py` schema.
- Phase 3 should populate the existing counters only.

### 2.4 The real closure metric is zero windowed divergence, not a ratio threshold

Some earlier notes talked about a divergence ratio threshold. Codex Phase 3 is
stricter:

- windowed divergence counter stays at `0`

under normal/aggressive/sparse polling.

Consequence:
- `test_stream_monitor.py` may continue printing a ratio for convenience,
  but proposal and closure criteria should treat any non-zero divergence window
  as a failure unless later evidence forces a narrower exception.

### 2.5 Timer path still owns its legacy work and must remain untouched

Current `TimerNotifyRT` still calls `UpdatePosition()` every tick for cable
endpoints. Phase 3 does not change that.

Consequence:
- no helper call from timer path,
- no timer-path branching on `AO_PUMP_FLAG_*`,
- no attempt to "shadow compare" inside the timer callback.

Phase 3 is strictly the public query-path bring-up.

### 2.6 First bring-up exposed a false-green over-jump policy

Observed active validation pattern:

- helper invocation counts increased,
- feature flags were correct,
- divergence stayed `0`,
- but `OverJumpCount` increased on nearly every call,
- and the rolling window never flushed.

Root cause:
- the earlier threshold (`framesPerDmaBuffer / 2`) was far below real
  query-to-query frame deltas under normal polling,
- so the helper counted almost every call as an over-jump,
- and the original skip/rebase behavior prevented window compare from running.

Consequence:
- Phase 3 cannot treat over-jump as a compare-blocking condition.
- In SHADOW_ONLY mode, over-jump must remain visible for diagnostics while
  allowing the rolling compare to continue.
- True clamp/skip semantics move to Phase 5/6 when transport ownership exists.

---

## 3. File-by-file edit plan

Expected code touch set is intentionally small:

1. `Source/Main/minwavertstream.h`
2. `Source/Main/minwavertstream.cpp`
3. `docs/PIPELINE_V2_CHANGELOG.md`

No Phase 3 code edits are expected in:

- `Source/Main/ioctl.h`
- `Source/Main/adapter.cpp`
- `Source/Utilities/loopback.h`
- `Source/Utilities/loopback.cpp`
- `test_stream_monitor.py`

### 3.1 `Source/Main/minwavertstream.h`

Anchor:
- helper declarations block around lines 212-237

Add:
- private helper declaration for `PumpToCurrentPositionFromQuery(...)`

Recommended insertion point:
- immediately after `UpdatePosition(...)` and before
  `SetCurrentWritePositionInternal(...)`

No new data fields are required here for Phase 3. Phase 1 already landed the
state needed for:

- baseline/latch,
- invocation/gate/over-jump/divergence counters,
- rolling-window accumulators,
- same-call byte-displacement stash.

### 3.2 `Source/Main/minwavertstream.cpp` - `GetPositions()`

Anchor:
- current `GetPositions()` around lines 1074-1115

Current flow:

```cpp
KeAcquireSpinLock(...)
ilQPC = KeQueryPerformanceCounter(NULL);
if (m_KsState == KSSTATE_RUN)
{
    UpdatePosition(ilQPC);
}
... copy linear/presentation positions ...
KeReleaseSpinLock(...)
```

Planned Phase 3 flow:

```cpp
KeAcquireSpinLock(...)
ilQPC = KeQueryPerformanceCounter(NULL);
if (m_KsState == KSSTATE_RUN)
{
    UpdatePosition(ilQPC);
    PumpToCurrentPositionFromQuery(ilQPC);
}
... copy linear/presentation positions ...
KeReleaseSpinLock(...)
```

Why here:
- same-call `UpdatePosition()` byte displacement is still fresh,
- helper sees current-call data, not previous-call leftovers,
- helper remains under the same spinlock as the legacy accounting.

### 3.3 `Source/Main/minwavertstream.cpp` - new helper body

Anchor:
- add helper implementation immediately before `UpdatePosition()` so review can
  read both side-by-side

Required helper responsibilities:

1. Master gate:
   - return unless `m_ulPumpFeatureFlags & AO_PUMP_FLAG_ENABLE`
2. Cable-only gate:
   - return for non-cable endpoints
3. Format sanity:
   - require valid `m_pWfExt`, non-zero sample rate, non-zero block align
4. Invocation count:
   - increment `m_ulPumpInvocationCount`
5. First-call latch:
   - if `!m_bPumpInitialized`, capture baseline time, reset per-run working
     state, set `m_bPumpInitialized = TRUE`, mirror state, return
6. Elapsed frame math:
   - compute total frames from QPC baseline
   - compute `newFrames = totalFrames - m_ulPumpProcessedFrames`
7. 8-frame gate:
   - if `newFrames < FP_MIN_GATE_FRAMES`, increment `m_ulPumpGatedSkipCount`,
     mirror state, return
8. Over-jump guard:
   - derive `framesPerDmaBuffer = m_ulDmaBufferSize / nBlockAlign`
   - compare `newFrames` against the chosen threshold
   - if exceeded, increment `m_ulPumpOverJumpCount`
   - do NOT return
   - do NOT rebase
   - continue into rolling-window accumulation and compare
9. Accepted-frame accounting:
   - advance `m_ulPumpProcessedFrames`
   - increment `m_ullPumpFramesProcessed`
10. Shadow compare:
   - convert same-call legacy bytes to frames using `nBlockAlign`
   - accumulate pump frames and legacy bytes into rolling window totals
   - every `AO_PUMP_SHADOW_WINDOW_CALLS`, compare totals against tolerance
   - if `diff > tolerance`, increment `m_ulPumpShadowDivergenceCount`
   - reset rolling window
11. Cleanup:
   - clear `m_ulLastUpdatePositionByteDisplacement`
   - mirror current stream counters/flags into the correct pipe direction slot

Explicit non-responsibilities:
- no transport mutation
- no `m_ulPumpLastBufferOffset` ownership math yet
- no DMA write/read calls
- no legacy path disable

### 3.4 `Source/Main/minwavertstream.cpp` - `UpdatePosition()`

Anchor:
- current `UpdatePosition()` around lines 1425-1529

Phase 3 change:
- stash the finalized same-call `ByteDisplacement` into
  `m_ulLastUpdatePositionByteDisplacement`

Important placement rule:
- write the stash after `ByteDisplacement` has been fully finalized
  (including EOS clamp for render path),
- after any legacy `WriteBytes()` / `ReadBytes()` calls that consume it,
- before `UpdatePosition()` returns.

This keeps the stash aligned with what legacy transport actually used, not an
earlier pre-clamp intermediate.

### 3.5 `Source/Main/minwavertstream.cpp` - `SetState()`

Anchor:
- current `SetState()` around lines 1199-1394

Phase 3 RUN behavior for cable endpoints:
- set `m_ulPumpFeatureFlags = AO_PUMP_FLAG_ENABLE | AO_PUMP_FLAG_SHADOW_ONLY`
- re-arm per-run working state:
  - `m_bPumpInitialized = FALSE`
  - `m_ullPumpBaselineHns = 0`
  - `m_ulPumpProcessedFrames = 0`
  - `m_ulPumpLastBufferOffset = 0`
  - `m_ullPumpShadowWindowPumpFrames = 0`
  - `m_ullPumpShadowWindowLegacyBytes = 0`
  - `m_ulPumpShadowWindowCallCount = 0`
  - `m_ulLastUpdatePositionByteDisplacement = 0`
- clear per-session stream counters:
  - `m_ulPumpGatedSkipCount = 0`
  - `m_ulPumpOverJumpCount = 0`
- preserve monotonic stream counters across PAUSE -> RUN:
  - `m_ulPumpInvocationCount`
  - `m_ulPumpShadowDivergenceCount`
  - `m_ullPumpFramesProcessed`
- snapshot feature flags into the corresponding `FRAME_PIPE` direction slot

Phase 3 STOP behavior for cable endpoints:
- clear all pump flags
- clear baseline/latch/window state
- clear stream counters so a new STOP -> RUN session starts clean
- snapshot zeroed feature flags into the corresponding pipe slot

Phase 3 PAUSE behavior:
- do not move ownership
- do not clear monotonic counters
- do not disable flags here
- rely on RUN re-arm to create a fresh timing baseline when the stream resumes

### 3.6 `Source/Main/minwavertstream.cpp` - pipe mirroring

No new `FRAME_PIPE` fields are needed.

The helper should update exactly one direction slot:

- render speaker endpoints -> `Render*`
- capture mic endpoints -> `Capture*`

Recommended mirror fields:
- `GatedSkipCount`
- `OverJumpCount`
- `FramesProcessedTotal`
- `PumpInvocationCount`
- `PumpShadowDivergenceCount`
- `PumpFeatureFlags`

Why direct mirroring is safe:
- Phase 1 split the slots by direction,
- only one stream direction writes a given slot,
- no render/capture race exists on the same slot.

### 3.7 `Source/Main/minwavertstream.cpp` - explicitly no `GetPosition()` edit

Anchor:
- current `GetPosition()` around lines 808-835

Phase 3 should leave it untouched.

Rationale:
- Phase 0 proved it is not the hot path for the target live-call scenario
- hooking both risks double-accounting or misleading counters
- Phase 3 is about the confirmed runtime path, not broad speculative coverage

### 3.8 `docs/PIPELINE_V2_CHANGELOG.md`

Add Phase 3 entry recording:
- helper landed on `GetPositions()` only
- shadow-mode only
- 8-frame gate enabled
- over-jump guard enabled
- rolling-window divergence compare enabled
- no ownership move in this phase

---

## 4. Recommended defaults to lock before editing

These are the remaining scope decisions that should be frozen before code edit.

### 4.1 `AO_PUMP_SHADOW_WINDOW_CALLS`

Recommended default: `128`

Reason:
- matches Codex suggested starting point,
- matches Claude Phase 3 notebook,
- smooths out `UpdatePosition()` carry noise without making the window too long
  for live debugging.

### 4.2 Divergence tolerance formula

Recommended default:

```text
max(16 frames, 2% of the larger total)
```

Reason:
- exactly matches Codex Phase 3 guidance,
- already aligned with the corrected Claude notebook,
- strict enough that any non-zero divergence window still means meaningful
  drift, not per-call carry noise.

### 4.3 Over-jump threshold

Recommended default:

```text
max(framesPerDmaBuffer * 2, sampleRate / 4)
```

Reason:
- avoids a magic absolute frame constant,
- scales with actual DMA buffer geometry,
- stays above normal query-to-query deltas seen in practice,
- keeps over-jump meaningful as an anomaly counter instead of a near-always-hit
  false alarm,
- still leaves Phase 5/6 free to impose a stricter transport-owning guard if
  runtime evidence later supports it.

### 4.4 Cable-endpoint guard location

Recommended default:
- keep the cable guard inside `PumpToCurrentPositionFromQuery()`

Reason:
- single source of truth
- easier future reuse if `GetPosition()` or another query surface is revisited
- keeps call sites simple: `UpdatePosition()` then `PumpToCurrentPositionFromQuery()`

---

## 5. Verification plan

### 5.1 First-pass bring-up

1. `build-verify.ps1 -Config Release`
2. `install.ps1 -Action upgrade`
3. `python test_ioctl_diag.py`
4. `python test_stream_monitor.py --once`
   - idle counters remain zero
5. active Phone Link call
   - `PumpInvocationCount > 0` on `CableA_Render`
   - `PumpFeatureFlags == ENABLE | SHADOW_ONLY`
   - no visible transport regression relative to Phase 2 baseline

### 5.2 Phase 3 closure validation

1. normal polling run
   - 5-minute run
   - at least one rolling shadow window flush
   - divergence counter stays `0`
2. aggressive polling run
   - at least one rolling shadow window flush
   - divergence counter stays `0`
3. sparse polling run
   - at least one rolling shadow window flush
   - divergence counter stays `0`
4. runtime rollback proof
   - clearing `AO_PUMP_FLAG_ENABLE` turns helper into a no-op immediately
   - no reinstall/reboot required

### 5.3 KDNET role in Phase 3

KDNET is now optional but useful:

- not required to rediscover the hook target; Phase 0 already closed that
- useful if `PumpInvocationCount` stays zero unexpectedly
- useful if a non-zero divergence window appears and we need to inspect
  call ordering or current-call stash correctness

---

## 6. Rollback

Expected rollback surface is small:

- `Source/Main/minwavertstream.h`
- `Source/Main/minwavertstream.cpp`
- `docs/PIPELINE_V2_CHANGELOG.md`

Rollback plan:
1. `git revert <phase3-commit>`
2. rebuild
3. reinstall
4. verify idle counters and V1 IOCTL path again

No INF, IOCTL layout, or test-tool schema change is expected in this phase.

---

## 7. What this document is not

This document is not:

- a Phase 5 render-ownership move
- a Phase 6 capture-ownership move
- a state-semantics rewrite
- a timer-path redesign
- a new diagnostic contract
- a change to `GetPosition()` hot-path assumptions

If any proposed code edit would:

- disable legacy transport,
- call transport helpers from the pump,
- alter IOCTL layout,
- or require new test-tool parsing,

then the edit has escaped Phase 3 and must be split or deferred.

---

## 8. Next actions

1. Lock the 4 recommended defaults in Section 4.
2. Record this proposal as a checkpoint commit if desired.
3. Edit in this order:
   - `Source/Main/minwavertstream.h`
   - `Source/Main/minwavertstream.cpp`
   - `docs/PIPELINE_V2_CHANGELOG.md`
4. Run build/install/IOCTL/monitor verification.
5. Run the Phase 3 live-call validation pass.
6. Only if all exit criteria are green, commit Phase 3.

---

## 9. Proposed Phase 3 commit message

```text
Phase 3: add shadow-mode query pump on GetPositions (no ownership move)
```
