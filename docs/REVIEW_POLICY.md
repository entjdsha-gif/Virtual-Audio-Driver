# AO Cable V1 Review Policy

Status: active
Date: 2026-04-25

This policy applies to every AO Cable V1 phase and step review.

Build success and design-value matching are not enough to pass review. PortCls,
WaveRT, KS, INF, ring/transport changes, and diagnostics changes must also
prove that the created objects are registered, published, observable, and
correctly unwound.

## 1. Review Standard

Every step review must include:

```text
1. Build/static validation.
2. Forbidden-symbol validation.
3. Design value validation.
4. PortCls / WaveRT / KS API sequence validation against installed WDK headers
   and Microsoft samples.
5. Create / register / unregister pairing validation.
6. Runtime observable proof for the phase goal.
7. Failure-path, lifetime, and unwind validation.
8. INF / registry / interface state validation when applicable.
9. Phase-specific negative tests when applicable.
10. Requirement trace matrix.
```

Do not mark a step complete only because the code compiles.

Do not mark a phase complete until the phase exit behavior is observable or a
documented blocker explicitly prevents runtime validation.

## 2. Forbidden Drift (canonical list)

This list is the single source of truth for V1 forbidden-compromises.
`CLAUDE.md`, `AGENTS.md`, `.claude/commands/review.md`, and per-phase
`exit.md` files reference this section instead of restating the list.

Reject any change that:

- Re-introduces packed 24-bit ring sample storage.
- Re-introduces the 4-stage `ConvertToInternal -> SrcConvert ->
  ConvertFromInternal -> LoopbackWrite` cable pipeline.
- Re-introduces sinc SRC with a 2048-coefficient table for cable streams.
- Re-introduces `MicSink` dual-write (ring + DMA push).
- Re-enables FormatMatch enforcement that requires Speaker == Mic == Internal.
- Adds a second cable transport owner outside `AoCableAdvanceByQpc`.
- Lets `GetPosition`/`GetPositions` advance audio without going through the
  canonical helper.
- Lets the shared timer advance audio independently of the canonical helper.
- Pumps audio from query callbacks without going through the canonical
  helper.
- Silently overwrites the ring on overflow (must be hard-reject + counter).
- Hides underrun, overflow, drop, zero-fill, or DMA overrun-guard hits as
  success.
- Adds hidden mixing, volume, mute, APO, DSP, AGC, EQ, limiter, noise
  suppression, or echo cancellation to the cable path.
- Stores `ms` as a runtime-state field in cable transport math (frames are
  authoritative; `ms` only in comments / UI / logs).
- Returns stale ring data into a fresh capture session after Stop/Start.
- Treats Phone Link end-to-end audio quality as proof of driver-internal
  bit-perfect behavior.
- Copies fresh code from `feature/ao-pipeline-v2` (frozen reference) without
  cherry-pick discipline.
- Changes architecture only to make a build error disappear.
- Treats `TargetLatencyFrames` (default 7168 @ 48 kHz) as a steady-state
  *fill* target. `TargetLatencyFrames` is the ring **capacity** (the value
  `WrapBound` reaches). Steady-state ring **fill** (writer-vs-reader
  in-flight) is a small live-latency band (typically a few ms). Any
  diagnostic / acceptance criterion that asserts fill ≈ `TargetLatencyFrames`
  is rejecting healthy state and accepting buffer-full / overflow risk.
- Hides hysteresis state (`UnderrunFlag` / drained boolean) from the
  diagnostics IOCTL. Counters alone cannot prove the 50%-`WrapBound`
  underrun-recovery path is operating correctly; the flag state must be
  observable through `AO_V2_DIAG`.
- Splits an audible-ownership transition across two commits. A render or
  capture flip from legacy → helper must atomically retire the legacy
  owner in the same commit; no commit may leave two transport owners
  simultaneously active for the same audible direction. (See ADR-006:
  one canonical helper.)
- Resizes `WrapBound` past `FrameCapacityMax`, or in a way that places
  `WritePos` / `ReadPos` outside the new bound. Latency reconciliation
  must clamp to `[currentFill + guard, FrameCapacityMax]` and must run
  while holding `pipe->Lock`.

## 3. API Sequence Validation

For every PortCls / WaveRT / KMDF object the change touches, review the full
lifecycle:

```text
create -> configure -> attach/add/register -> runtime observable -> remove/unwind
```

The key review question is:

```text
After this object is created, is it visible to the next owner that must use it?
```

Examples:

```text
PcAddAdapterDevice
-> miniport / port pair created and registered

IPortClsRegistration::RegisterSubdevice
-> KS device interfaces enabled (KSCATEGORY_AUDIO / RENDER / CAPTURE / REALTIME / TOPOLOGY)

KsCreateFilterFactory + KsAddItemToObjectBag
-> filter discoverable from user mode

IMiniportWaveRTStream::AllocateAudioBuffer
-> WaveRT-mapped DMA buffer addressable, bound to the right pin

ExAllocateTimer + ExSetTimer
-> shared cable transport timer arm/disarm symmetric across RUN/STOP

KeFlushQueuedDpcs
-> required before resetting per-stream runtime fields on Pause/Stop and in
   the destructor
```

If a Microsoft sample or installed WDK header sequence performs a required
add/register/unregister call and AO Cable V1 omits it, that omission is a
review finding unless an ADR explicitly justifies the omission with an
authoritative reference.

## 4. PortCls / WaveRT Sample Comparison

For PortCls/WaveRT API usage, compare call order and ownership against:

- Installed WDK PortCls headers (`portcls.h`, `ksmedia.h`, `wdmaudio.h`).
- Microsoft official samples (`MSVAD`-derived, but only as API reference, not
  as architecture template — see ADR on cable transport).
- Existing AO Cable V1 code paths that are already considered correct.

Distinguish:

```text
Required sample sequence:
- Calls necessary for the object to participate in the Windows audio stack.

AO Cable V1 sequence:
- Calls present in current code.

Missing:
- Calls absent from current code.

Intentionally omitted:
- Calls omitted by AO Cable V1 design, with WDK / sample / ADR evidence.
```

Do not compare only enum values, GUIDs, constants, or signatures. Compare the
whole API sequence.

## 5. Runtime Observable Proof

Each phase must define the runtime evidence that proves its goal.

Examples:

```text
Endpoint enumeration:
- Cable A and Cable B render endpoints visible in Sound Settings.
- Cable A and Cable B capture endpoints visible.
- Status OK and ProblemCode 0 for all four.
- Default device switching to AO Cable A/B works.
- setupapi.dev.log has no install/start failure for the target instance.

Stream creation / format:
- WASAPI shared-mode open at 48 kHz / 24-bit / Stereo succeeds.
- KSDATARANGE intersection accepts at least the formats listed in
  docs/AO_CABLE_V1_ARCHITECTURE.md § "External contract".
- Unsupported formats are rejected cleanly with the expected status code.

Ring / transport (per phase scope):
- Render input frame count == accepted ring write count + explicit
  hard-reject counter (no silent loss).
- Capture output frame count == ring read count + explicit zero-fill /
  underrun counter (no stale data).
- Frame order proven by deterministic payload tests (sine sweep / PRBS).

Position / cadence:
- GetPosition / GetPositions invoke the canonical helper before returning.
- PlayPosition and WritePosition reflect "now" with sub-DPC precision.
- Shared timer is one of multiple call sources; not the only owner.
- QPC timestamps are monotonic.

Lifecycle (per ADR-009):
- RUN -> PAUSE -> RUN keeps the ring usable.
  PAUSE handler runs KeFlushQueuedDpcs + AoTransportOnPauseEx.
  PAUSE must NOT call AoTransportOnStopEx (would reset state).
- RUN -> STOP -> RUN resets monotonic counters, cursor, fade, and ring.
  STOP handler runs KeFlushQueuedDpcs + AoTransportOnStopEx.
- Destructor runs KeFlushQueuedDpcs + AoTransportOnStopEx +
  AoTransportFreeStreamRt (owner ref drops).
- FreeAudioBuffer ordering — Step A through Step D from DESIGN § 5.4
  (no exceptions):
  Step A. exclude query path:
    acquire(m_PositionSpinLock) -> m_pTransportRt = NULL ->
    release(m_PositionSpinLock).
  Step B. exclude DPC path:
    AoTransportUnregister(rt) -> KeFlushQueuedDpcs().
    After Step B the contract requires RefCount == 1 (owner-only);
    assert, do not wait.
  Step C. tear down DMA-buffer state:
    rt->DmaBuffer = NULL -> FreeAudioBuffer().
  Step D. destructor only:
    AoTransportFreeStreamRt(rt) (owner ref drops 1 -> 0, frees rt).
- Query path is NOT in RefCount. m_pTransportRt publish under
  m_PositionSpinLock is what excludes it; do NOT review for a
  "RefCount++ in GetPosition" — that would be wrong.
```

If runtime validation is blocked by signing, package verification, hardware,
or environment constraints, record the exact blocker and do not claim runtime
proof.

## 6. Failure Path And Lifetime

Each review must check the unsuccessful paths, not only the success path.

Questions to answer:

```text
If a later step fails, who owns every object that was already created?
Was ownership transferred to PortCls, KMDF, Windows, or still held by AO Cable?
Is the cleanup API valid for the current ownership state?
If unregister fails, are handles and flags preserved?
Can device PnP start again without stale state corruption?
Does ReleaseHardware attempt all required cleanup and return the first failure?
Are IRQL and pageable/nonpageable assumptions valid for callbacks?
Does the destructor flush DPCs before freeing AO_STREAM_RT?
Does the engine ref-count discipline prevent UAF when timer DPC drops the lock?
```

Do not assume object lifetime behavior. Verify with installed WDK headers,
Microsoft Learn, or existing-correct AO code.

## 7. Diagnostics Coupling

When `AO_V2_DIAG` schema (in `Source/Main/ioctl.h`) changes, the review must
verify that all three of the following are updated together:

- `Source/Main/ioctl.h`
- `Source/Main/adapter.cpp` (`IOCTL_AO_GET_STREAM_STATUS` writer)
- `test_stream_monitor.py` (consumer)

A schema change without all three updates is a BLOCKER.

## 8. Requirement Trace Matrix

Every non-trivial review should include a compact trace matrix:

```text
Requirement | Code location | Official/design basis | Runtime proof | Status
```

Example:

```text
Ring overflow rejects the write and increments a counter
Code: Source/Utilities/loopback.cpp AoRingWriteFromScratch
Basis: docs/ADR.md ADR-005 + docs/AO_CABLE_V1_ARCHITECTURE.md § 4.2
Runtime proof: forced overflow scenario shows counter++ and no ring overwrite
Status: pass
```

For a code review final answer, findings still come first. The trace matrix
may be summarized when there are no blockers.

## 9. Phase Exit Rule

A phase may exit only when:

```text
1. All step reviews pass under this policy.
2. Phase exit runtime proof passes, or the exact blocker is documented.
3. No forbidden compromise is present.
4. The reviewer (Codex / user) has no BLOCKER finding.
5. The phase exit document records what was proven and what remains unproven.
```

Do not flip `phases/<phase>/index.json` status from `in_progress` to
`completed` while a phase-exit runtime blocker is unresolved, unless the user
explicitly approves a documented blocked exit.

## 10. PASS / MINOR / BLOCKER Vocabulary

A review finding uses exactly one of:

- `BLOCKER`: must be fixed before commit and before status flips to
  `completed`.
- `MINOR`: should be fixed but does not block commit if explicitly accepted in
  the phase exit document.
- `RESIDUAL RISK`: a known risk that this review does not eliminate; recorded
  for future phases.

A `helper PASS`, `hygiene PASS`, `compile PASS`, or `re-parse PASS` is **not**
a step PASS unless the relevant acceptance criteria in the step's `step<N>.md`
also pass.
