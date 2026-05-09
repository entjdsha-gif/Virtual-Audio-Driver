# Phase 3 Exit: helper ready, audible flip blocked on agreement

## Exit Gate

- [ ] All steps in `phases/3-canonical-helper-shadow/index.json`
      marked completed.
- [ ] 63/64 phase correction (ADR-007 Decision 2) implemented in
      timer DPC scheduling. `AO_TRANSPORT_ENGINE` has
      `BaselineQpc` / `TickCounter` / `LastTickQpc` fields and
      `AoTransportTimerCallback` advances them. (Pre-Phase-4
      precondition because long-call drift would otherwise become
      observable after audible flip.)
- [ ] Final shadow-divergence evidence is collected after the
      63/64 timer scheduling step, not before it.
- [ ] Build clean.
- [ ] Live call test: `DbgShadowQueryHits` and `DbgShadowTimerHits`
      both increase steadily during a live call. Helper is reached.
- [ ] `<Cable>_R_ShadowDivergenceCount` is **<= 5 increments per
      minute** during steady-state speech for at least one full live
      call (RENDER side only — see capture caveat below). Any single
      increment indicates a QUERY-time helper-vs-legacy frame-anchor
      cumulative delta that exceeded the rate-aware tolerance
      `ceil(SampleRate / 1000) + AO_CABLE_MIN_FRAMES_GATE` defined in
      `phases/3-canonical-helper-shadow/step4.md`. <= 5/minute is
      consistent with normal scheduler jitter inside the tolerance
      band; > 5/minute indicates a cursor / drift bug that must be
      fixed before Phase 4.
- [ ] `<Cable>_C_ShadowQueryHits` is reported. If it stays at 0 the
      capture-side `<Cable>_C_ShadowDivergenceCount == 0` reading is
      **NOT EXERCISED**, not PASS. Phase 5 must add its own
      capture-side evidence gate before flipping audible ownership.
- [ ] No audible regression vs Phase 2 exit baseline. Helper is in
      shadow mode and must not touch audible behavior.
- [ ] No BSOD / hang / deadlock under stream open/close stress.
- [ ] `KeFlushQueuedDpcs` on stream destructor confirmed correct.

## Outcome

After Phase 3, every cable transport call source funnels into the
helper. The helper computes everything (advance, gate, overrun,
drift correction) and writes shadow state. The legacy
`UpdatePosition`-driven path still owns audible cable transport.

The render-side shadow divergence counter staying within the
steady-state gate (**<= 5 increments per minute**) proves the helper's
advance math agrees with the legacy advance math within the rate-aware
tolerance band `ceil(SampleRate / 1000) + AO_CABLE_MIN_FRAMES_GATE`
(legacy `UpdatePosition`'s ms-quantization envelope plus the helper's
8-frame floor). This is the single numeric precondition that unlocks
Phase 4 audible flip on the **render** side.

> Note: a strictly *zero* divergence count is not realistic even
> within the tolerance band given normal Windows scheduler jitter and
> the legacy carry residual sitting near the 1 ms boundary. The exit
> gate is "<= 5/min for at least one full live call." Phase 4 Step 1
> must re-check the same threshold immediately before flipping audible
> ownership; do not rely on a stale Phase 3 measurement.

> Capture caveat: capture-side `ShadowDivergenceCount` will read 0 in
> Phase 3 because no QUERY call source exists for capture streams
> (`ShadowQueryHits == 0`). That 0 is "NOT EXERCISED", not "agreement
> proven". Phase 5 capture flip needs its own capture-side evidence
> gate (e.g., a capture-equivalent UpdatePosition+helper reorder, or a
> different metric entirely) before it can match the render-side
> precondition.

## Non-claims

- **Runtime `StatOverrunCounter` rising is NOT observed or proven** in
  Phase 3 evidence. Under the v6 lifecycle (`AoTransportOnRunEx`
  resets the helper anchor quartet on every RUN entry to align with
  legacy's "active-time since most recent RUN" semantics; see
  `step4.md` § Approach), the overrun threshold (advance >
  `SampleRate / 2` in a single helper call) is practically
  unreachable in PAUSE/RUN/normal-drive scenarios. The
  "overrun does not bump `DbgShadowDivergenceHits`" property is
  guaranteed by **static branch ordering** instead: the overrun
  bail-out (`AoCableAdvanceByQpc` ~ transport_engine.cpp:1374),
  along with the seed branch, long-window QPC rebase, 8-frame gate-
  fail, and helper-side backwards-baseline guard, each
  `KeReleaseSpinLock + return` BEFORE the QUERY compare block (~
  transport_engine.cpp:1547). The `Active=FALSE` gate on the compare
  block likewise excludes non-RUN queries. Operational corroboration
  is the v6 stress evidence (rapid open/close 8-cycle + A/B 30 s
  sine drive + pause/resume 4-cycle) showing zero spurious
  `ShadowDivergenceCount` bumps in any observed scenario.
- **InfVerif env-side warning is NOT a code result**. The
  `x86\InfVerif.dll` missing in the local WDK install is a build-
  environment caveat that prevents a green InfVerif step output;
  the .sys binaries are still produced and signed, and `install.ps1`
  uses its own staging path. InfVerif PASS is not claimed.

## Forbidden Carry-Over Into Phase 4

- Phase 4 must not introduce a third "publish state for the timer
  to consume" pattern. Audible ownership flips **directly** from
  legacy to helper.
- Phase 4 must not flip audible ownership without re-confirming
  RENDER-side divergence counter <= 5/min on a live call
  **immediately before** the flip commit (not on a stale Phase 3
  measurement). Capture-side ShadowDivergenceCount is NOT EXERCISED
  in Phase 3 and is NOT a Phase 4 precondition; it is a Phase 5
  precondition under a separate capture-side evidence gate.
- Phase 4 must not flip audible ownership before the 63/64 phase
  correction step is committed and verified. Long-call drift would
  otherwise become audible.

## Phase 3 -> Phase 4 Handoff

Phase 4 retires the legacy `UpdatePosition`-driven cable render
path and lets the helper own render audibly. Capture stays on
legacy until Phase 5.
