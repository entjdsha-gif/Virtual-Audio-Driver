# Phase 3 Step 4: Shadow divergence counter

## Read First

- `docs/AO_CABLE_V1_DESIGN.md` § 3.1 (AO_STREAM_RT runtime fields), § 7
  (diagnostics IOCTL).
- `docs/ADR.md` ADR-006 (one canonical advance owner), ADR-007
  (position on query, 8-frame gate, long-window QPC rebase).
- Phase 1 Step 5 (existing `AO_V2_DIAG` extension pattern).

## Goal

Compare the helper's shadow-state cumulative advance (frames since
anchor) against the legacy `UpdatePosition`-driven cumulative advance
on each `AO_ADVANCE_QUERY` helper invocation, and increment a
per-stream divergence counter when the two disagree by more than the
rate-aware tolerance
`((SampleRate + 999) / 1000) + AO_CABLE_MIN_FRAMES_GATE` (legacy 1 ms
quantization envelope + helper 8-frame gate; 56 frames at 48 kHz).
This provides quantitative evidence that Phase 4 / 5 is safe to flip
audible ownership.

## Approach

**QUERY-after-legacy cumulative compare** (Approach C).

`GetPosition` / `GetPositions` reorder so `UpdatePosition(ilQPC)` runs
**before** `AoCableAdvanceByQpc(... AO_ADVANCE_QUERY ...)` for cable
streams. Both calls share the same `m_PositionSpinLock` acquisition and
the same `ilQPC`. Step 2's invariant — "helper runs before any legacy
position value is RETURNED to the caller" — is preserved because the
output writes (`Position_->{Play,Write}Offset`,
`*_pullLinearBufferPosition`) sit after both calls under the same lock.

The helper's `AO_ADVANCE_QUERY` branch then runs the compare as the
last step before releasing `PositionLock`:

```c
if (reason == AO_ADVANCE_QUERY && rt->IsCable && rt->Active &&
    rt->BlockAlign > 0) {
    ULONGLONG legacyFramesAbs   = rt->DmaProducedMono / rt->BlockAlign;
    ULONGLONG legacySinceAnchor = legacyFramesAbs - rt->LegacyAnchorFrames;
    ULONGLONG helperSinceAnchor = rt->PublishedFramesSinceAnchor;
    ULONGLONG absDiff = (helperSinceAnchor > legacySinceAnchor)
        ? helperSinceAnchor - legacySinceAnchor
        : legacySinceAnchor - helperSinceAnchor;
    ULONG toleranceFrames =
        ((rt->SampleRate + 999U) / 1000U) + AO_CABLE_MIN_FRAMES_GATE;
    if (absDiff > toleranceFrames) {
        InterlockedIncrement(&rt->DbgShadowDivergenceHits);
    }
}
```

`LegacyAnchorFrames` is `ULONGLONG` so a multi-hour live session does
not 32-bit wrap (4 G frames at 48 kHz ≈ 25 hours).

**Tolerance policy.** `toleranceFrames = ceil(SampleRate / 1000) +
AO_CABLE_MIN_FRAMES_GATE`. At 48 kHz this is 48 + 8 = 56, at 44.1 kHz
45 + 8 = 53, at 96 kHz 96 + 8 = 104.

The two terms have distinct meaning:

- `ceil(SampleRate / 1000)` is one millisecond's worth of frames at the
  stream rate. It bounds legacy `CMiniportWaveRTStream::UpdatePosition`'s
  sub-ms residual: legacy accumulates `m_ullLinearPosition` via ms-
  quantized arithmetic with `m_hnsElapsedTimeCarryForward` carrying the
  not-yet-applied < 1 ms residual. The published `DmaProducedMono`
  therefore lags an ideal QPC-based cumulative by at most one cadence
  tick.
- `AO_CABLE_MIN_FRAMES_GATE` is the helper's 8-frame minimum gate
  (ADR-007), the per-call helper noise floor.

The tolerance is the sum: legacy's 1 ms quantization envelope plus the
helper's gate. Within this band the two cumulatives are considered to
agree. **Helper is NOT pulled down to legacy's ms math**; this is a
measurement allowance for comparing two different time-quantization
models. The Phase 3 Step 4 self-check measurement on 2026-05-09
confirmed that an 8-frame-only gate produces ~57 % divergence per
QUERY at 48 kHz purely from this quantization difference, with no
underlying drift.

`Active` gate: helper-only field flipped by `OnRunEx` / `OnPauseEx` /
`OnStopEx`. Skipping when not Active prevents a non-RUN query from
bumping on a stale legacy publish (helper still computes a fresh
QPC-anchored value, legacy stops advancing under PAUSE/STOP).

**Lifecycle / lock-graph asymmetry between helper and legacy.** Legacy
`CMiniportWaveRTStream::SetState` resets `m_ullDmaTimeStamp` to the
current QPC on every `KSSTATE_RUN` entry (see
[minwavertstream.cpp:1621](../../Source/Main/minwavertstream.cpp#L1621)).
This makes `m_ullLinearPosition` count only "active time since the
most recent RUN" — pause time is dropped, not accumulated. Helper QPC
math, by contrast, would carry pause time if the original anchor were
preserved across a `PAUSE → RUN` cycle. Without alignment, every
PAUSE→RUN cycle injects `pause_duration × rate` of fictional drift on
the helper side relative to the legacy producer; every post-resume
QUERY then bumps `ShadowDivergenceCount`.

The fix is a **narrow OnRunEx anchor reset**: `AoTransportOnRunEx`
zeros the anchor quartet (`AnchorQpc100ns`,
`PublishedFramesSinceAnchor`, `LastAdvanceDelta`,
`LegacyAnchorFrames`) on every RUN entry, under `rt->PositionLock`,
BEFORE acquiring `g_AoTransportEngine.Lock` and BEFORE `rt->Active`
flips TRUE. The legacy producer baseline (`DmaProducedMono`,
`DmaConsumedMono`) is **preserved** across PAUSE→RUN — it is STOP /
destructor scope only via `AoCableResetRuntimeFields`. The helper's
next call (TIMER or QUERY) hits the seed branch
(`AnchorQpc100ns == 0`), captures
`LegacyAnchorFrames = DmaProducedMono / BlockAlign` from the
preserved post-pause `DmaProducedMono`, anchors `AnchorQpc100ns` to
the post-resume QPC, and returns without compare. Subsequent QUERYs
measure the same post-resume sliver on both sides.

Lock ordering is PositionLock-then-EngineLock; the reverse ordering
does not exist in this TU, so this acquisition is safe. Releasing
PositionLock before taking EngineLock keeps the rule "do not hold
EngineLock across PositionLock" intact.

`LegacyAnchorFrames` is reset / re-seeded at four lifecycle points,
all of which leave the anchor quartet mutually consistent:

1. **`AoTransportOnRunEx` anchor reset (PAUSE→RUN alignment).** Above.
2. **First-call anchor seed (`AnchorQpc100ns == 0` branch).** Seeds
   `LegacyAnchorFrames` from current `DmaProducedMono / BlockAlign`
   and returns without compare. Reached on every RUN entry thanks to
   step 1, plus on the initial allocation (RtlZeroMemory).
3. **Long-window QPC rebase** (`elapsedFrames >= SampleRate <<
   REBASE_SHIFT`). ADR-007 helper-owned drift correction: re-anchors
   the QPC and both frame baselines once per ~128 s of stream time.
4. **Helper-side backwards-baseline guard (defense-in-depth).**
   Detects `currentLegacyFrames < LegacyAnchorFrames` at the top of
   the helper and re-seeds the entire anchor pair without bump. Not
   exercised by the current PAUSE→RUN lifecycle (OnRunEx anchor
   reset handles that path); kept as an "unexpected backwards
   baseline" safety net for any future cleanup path or out-of-band
   producer reorder that briefly publishes a smaller value.

`AoCableResetRuntimeFields` on STOP / destructor zeros the anchor
quartet alongside `DmaProducedMono` and `DmaConsumedMono`, so a fresh
RUN-after-STOP starts cleanly via the first-call seed.

All return BEFORE the compare runs, so housekeeping ticks never bump
the divergence counter. Overrun bail-out and the helper's 8-frame
gate-fail likewise return BEFORE the compare. `DbgShadowDivergenceHits`
is monotonic across PAUSE/RUN/STOP cycles (matches
`DbgShadow{Advance,Query,Timer}Hits` policy), zeroed only at
`AoTransportAllocStreamRt` via `RtlZeroMemory`.

`AO_ADVANCE_TIMER_RENDER` / `AO_ADVANCE_TIMER_CAPTURE` / `AO_ADVANCE_PACKET`
**skip the compare**: legacy publish cadence is decoupled from the 1 ms
transport timer, so per-call helper-vs-legacy comparison would explode
under cadence skew. Timer cadence agreement is proven separately by
Step 3 evidence.

## Planned Files

Edit atomically:

- `Source/Utilities/transport_engine.h` — add
  `LegacyAnchorFrames` + `DbgShadowDivergenceHits` to `AO_STREAM_RT`;
  add `ShadowDivergenceHits` to `AO_SHADOW_COUNTERS_PER_STREAM`.
- `Source/Utilities/transport_engine.cpp` — seed/rebase `LegacyAnchorFrames`,
  QUERY-only compare at the success-path tail, reset
  `LegacyAnchorFrames` in `AoCableResetRuntimeFields`, export
  `DbgShadowDivergenceHits` via `AoTransportSnapshotShadowCounters`.
  Suppress the per-call `DbgY2RenderMismatchHits++` (Y2-1.5 byte-cumulative
  diff retained for Phase 4 audible-flip rollback evidence; the public
  Step 4 metric is `DbgShadowDivergenceHits` alone).
- `Source/Main/minwavertstream.cpp` — reorder `UpdatePosition(ilQPC)`
  BEFORE `AoCableAdvanceByQpc(... AO_ADVANCE_QUERY ...)` in
  `GetPosition` and `GetPositions`. The `PumpToCurrentPositionFromQuery`
  call stays adjacent to `UpdatePosition` for stash freshness.
- `Source/Main/ioctl.h` — append 4 `<Cable>_<R/C>_ShadowDivergenceCount`
  ULONGs at the V2 tail (P8 layout, sizeof grows 220 → 236). Update
  C_ASSERT.
- `Source/Main/adapter.cpp` — add `kP8Size`, partial-write tier guard,
  fold P8 fill into the existing P7 snapshot block (single
  `AoTransportSnapshotShadowCounters` call covers both tiers).
- `test_stream_monitor.py` — add `V2_DIAG_SIZE_P8`, parse the new tail,
  print `ShDiv=N` on each Render / Capture line. Keep the existing
  `Div=` token (Phase 1 PumpShadowDivergence; force-zero) so reviewers
  can distinguish the two metrics.
- `phases/3-canonical-helper-shadow/step4.md` — this file: reflect
  Approach C and the corrected file list.
- `docs/AO_CABLE_V1_DESIGN.md` — update § 3.1 runtime field map and the
  diagnostics IOCTL section to include the P8 tail.

## Rules

- Eight-file atomic edit (per `docs/REVIEW_POLICY.md` § 7 atomicity
  rule extended to the doc/step set in this case because the runtime
  field freeze in Step 0 needs reconciling here).
- Tolerance is `((SampleRate + 999) / 1000) + AO_CABLE_MIN_FRAMES_GATE`
  — see "Tolerance policy" above. Values within the band are not
  divergence.
- `LegacyAnchorFrames` is the only field besides
  `PublishedFramesSinceAnchor` that re-seeds at long-window rebase.
  These two fields move together; they are the matched anchor pair.
- Do not act on the divergence counter inside the helper (do not skip
  the helper if it diverges). Observability only.
- Do not bump the counter on seed / rebase / overrun-bail / gate-fail
  ticks (the helper returns before the compare in every one of those
  branches), nor on non-RUN queries (`rt->Active == FALSE`).
- Do not reuse the Phase 1 `PumpShadowDivergenceCount` ABI slot or
  monitor token. P8 tail and `ShDiv=` are the Step 4 surface; the P1
  slot stays force-zeroed.
- Reorder is QUERY-only. No timer / packet / write-bytes paths gain a
  helper compare.

## Capture-side caveat (NOT EXERCISED, not PASS)

WASAPI capture clients do not call `GetPosition` / `GetPositions` on
the capture stream, so the helper `AO_ADVANCE_QUERY` call source never
fires for capture. `<Cable>_C_ShadowDivergenceCount == 0` therefore
means "the metric was not exercised", **not** that helper and legacy
agree on the capture side. `<Cable>_C_ShadowQueryHits == 0` is the
direct evidence for "not exercised".

Phase 5 (capture audible flip) requires its own capture-side evidence
gate before flipping ownership. Step 4 does not satisfy that gate.

## Acceptance Criteria

- [ ] Build clean.
- [ ] Live cable RENDER stream (Cable A and Cable B) shows
      `<Cable>_R_ShadowDivergenceCount == 0` or single-digit over a
      30 s sine drive in steady state — proving helper and legacy
      agree within the tolerance band, the precondition for Phase 4
      to flip audible ownership safely on render.
- [ ] `<Cable>_C_ShadowQueryHits == 0` and
      `<Cable>_C_ShadowDivergenceCount == 0` reported as **NOT
      EXERCISED** (not PASS).
- [ ] `Adv == Q + T` invariant holds across the same drive (Step 3
      regression check).
- [ ] Overrun / gate-fail / non-Active queries do not spuriously bump
      `ShadowDivergenceCount`. **Static branch-order proof**: the
      overrun bail-out
      (`AoCableAdvanceByQpc`, transport_engine.cpp ~ line 1510),
      8-frame gate-fail (~ line 1497), seed (~ line 1382), long-window
      QPC rebase (~ line 1472), and helper-side backwards-baseline
      guard each `KeReleaseSpinLock + return` BEFORE the QUERY compare
      block (~ line 1654). By branch ordering it is therefore
      impossible for any of those ticks to bump
      `DbgShadowDivergenceHits` in the same helper invocation. The
      `Active=FALSE` gate on the compare block likewise excludes
      non-RUN queries. **Operational corroboration**: v6 stress
      evidence (rapid open/close 8-cycle + A/B 30 s sine drive +
      pause/resume 4-cycle) shows zero spurious ShadowDivergenceCount
      bumps in any observed scenario. Direct runtime observation of
      `StatOverrunCounter` rising while ShDiv stays flat is **not a
      gate** under the v6 lifecycle, because OnRunEx anchor reset
      makes the overrun threshold (advance > sampleRate/2 in one
      helper call) practically unreachable in PAUSE/RUN/normal-drive
      scenarios.
- [ ] Pre-P8 monitor clients still parse the diag (P1 / P5 / P6 / P7
      consumers see `ShadowDivergenceCount` as `None`, not a parse
      error).

## What This Step Does NOT Do

- Does not flip audible ownership.
- Does not retire the legacy `UpdatePosition` cable advance path.
- Does not change `DbgY2HelperRenderBytes` / `DbgY2LegacyRenderBytes`
  / `DbgY2RenderByteDiffMax` updates — Phase 4 audible-flip rollback
  uses those byte-cumulative totals.
- Does not change the Phase 1 `PumpShadowDivergenceCount` ABI slot or
  its `Div=` representation in the monitor.

## Completion

```powershell
python scripts/execute.py mark 3-canonical-helper-shadow 4 completed --message "Shadow divergence counter (Approach C, QUERY-after-legacy cumulative compare); helper agrees with legacy in steady state."
```
