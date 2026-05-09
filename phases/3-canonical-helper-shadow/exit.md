# Phase 3 Exit: helper ready, audible flip blocked on agreement

## Exit Gate

- [x] All steps in `phases/3-canonical-helper-shadow/index.json`
      marked completed.
      *Evidence:* index.json shows steps 0..5 all `completed`; last
      Step 5 commit `24fb623` + mark `0822001`.
- [x] 63/64 phase correction (ADR-007 Decision 2) implemented in
      timer DPC scheduling. `AO_TRANSPORT_ENGINE` has
      `BaselineQpc` / `TickCounter` / `LastTickQpc` fields and
      `AoTransportTimerCallback` advances them. (Pre-Phase-4
      precondition because long-call drift would otherwise become
      observable after audible flip.)
      *Evidence:* commit `24fb623`; `apply_drift_correction` static
      helper in `Source/Utilities/transport_engine.cpp` called once
      per firing from `AoTransportTimerCallback` under EngineLock;
      engine struct fields in `Source/Utilities/transport_engine.h`.
- [x] Final shadow-divergence evidence is collected after the
      63/64 timer scheduling step, not before it.
      *Evidence:* `tests/phase3-runtime/step5_v3_monitor_drive_a.log`
      (Cable A 60 s sine drive, captured AFTER Step 5 commit
      `24fb623` install). Render `Adv 59877 = Q 552 + T 59325`,
      `ShDiv 0 / 552 Q`.
- [x] Build clean.
      *Evidence:* `tests/phase3-runtime/step5_build_verify_v2.log`
      reports 17 PASS / 0 FAIL; `aocablea.sys` and `aocableb.sys`
      both signed (InfVerif env caveat — see Non-claims).
- [x] Phase 3 proxy reachability test: `DbgShadowQueryHits` and
      `DbgShadowTimerHits` both increase steadily during a 60 s
      sine-drive call. Helper is reached. Real Phone Link live-call
      re-check is deferred to Phase 4 Step 1 and is mandatory before
      audible render flip.
      *Evidence:* Step 5 v3 60 s drive — `DbgShadowQueryHits` grew
      0 → 552 (~9.2 Hz, natural OS query cadence),
      `DbgShadowTimerHits` grew 0 → 59325 (~989 Hz, ADR-013 1 ms
      cadence). Sine-drive proxy at 48 kHz / 16-bit / 2-ch; helper
      code path is identical to a Phone Link live call but
      Phone Link traffic is not exercised here (see Non-claims).
- [x] `<Cable>_R_ShadowDivergenceCount` is **<= 5 increments/minute**
      during the Phase 3 post-Step5 60 s sine-drive proxy test
      (RENDER side only — see capture caveat below). Any single
      increment indicates a QUERY-time helper-vs-legacy frame-anchor
      cumulative delta that exceeded the rate-aware tolerance
      `ceil(SampleRate / 1000) + AO_CABLE_MIN_FRAMES_GATE` defined in
      `phases/3-canonical-helper-shadow/step4.md`. <= 5/minute is
      consistent with normal scheduler jitter inside the tolerance
      band; > 5/minute indicates a cursor / drift bug that must be
      fixed before Phase 4. Real steady-state speech / Phone Link
      live-call evidence is not claimed here and must be re-collected
      immediately before Phase 4 audible flip.
      *Evidence:* `step5_v3_monitor_drive_a.log` Cable A 60 s
      sine-drive proxy, `<Cable>_R_ShadowDivergenceCount = 0 / 552 Q`
      = **0/min** (well under the 5/min gate).
- [x] `<Cable>_C_ShadowQueryHits` is reported. If it stays at 0 the
      capture-side `<Cable>_C_ShadowDivergenceCount == 0` reading is
      **NOT EXERCISED**, not PASS. Phase 5 must add its own
      capture-side evidence gate before flipping audible ownership.
      *Evidence:* `step5_v3_monitor_drive_a.log` Cable A capture
      side reports `Q = 0 / ShDiv = 0` throughout the 60 s drive →
      explicitly classified as **NOT EXERCISED**.
- [x] No audible regression vs Phase 2 exit baseline. Helper is in
      shadow mode and must not touch audible behavior.
      *Evidence:* Phase 3 baseline `RenderAudibleActive = FALSE` is
      enforced in `AoTransportOnRunEx` for every cable stream;
      `AoCableWriteRenderFromDma` gate is unreachable (audible
      ownership remains on legacy
      `CMiniportWaveRTStream::UpdatePosition` / `ReadBytes` /
      `WriteBytes`). All sine drives across Steps 3/4/5 completed
      without audible regression flagged. Audible ownership flip
      is explicitly Phase 4 (render) / Phase 5 (capture) work.
- [x] No BSOD / hang / deadlock under stream open/close stress.
      *Evidence:* `tests/phase3-runtime/step5_v2_stress_a.log` and
      `step4_v6_stress_a.log` — 8-cycle rapid open/close stress
      (Cable A render + capture, 0.4 s hold, 0.1 s gap, 8/8 OK).
      Pause/resume 4-cycle (`step4_v6_pause_resume_a.log`) likewise
      completes cleanly. Monitor responds normally after each
      stress.
- [x] `KeFlushQueuedDpcs` on stream destructor confirmed correct.
      *Evidence:* `Source/Main/minwavertstream.cpp` — destructor
      flushes via `KeFlushQueuedDpcs()` before tearing down
      transport runtime; PAUSE handler also flushes before timer
      cancel + carry-forward save. ADR-009 contract preserved.

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
> the legacy carry residual sitting near the 1 ms boundary. The
> Phase 3 closeout gate is "<= 5/min on the post-Step5 sine-drive
> proxy." Phase 4 Step 1 must re-check the live-call threshold
> immediately before flipping audible ownership; do not rely on the
> Phase 3 proxy measurement.

> Capture caveat: capture-side `ShadowDivergenceCount` will read 0 in
> Phase 3 because no QUERY call source exists for capture streams
> (`ShadowQueryHits == 0`). That 0 is "NOT EXERCISED", not "agreement
> proven". Phase 5 capture flip needs its own capture-side evidence
> gate (e.g., a capture-equivalent UpdatePosition+helper reorder, or a
> different metric entirely) before it can match the render-side
> precondition.

## Non-claims

- **Audible ownership flip is NOT performed in Phase 3.** Cable
  render audible ownership stays on legacy
  `CMiniportWaveRTStream::UpdatePosition` / `ReadBytes`; cable
  capture audible ownership stays on legacy `WriteBytes`. Phase 4
  flips render. Phase 5 flips capture. The helper
  (`AoCableAdvanceByQpc`) is in shadow mode end-to-end through
  Phase 3.
- **Capture-side helper-vs-legacy agreement is NOT proven.**
  Capture has no QUERY call source, so
  `<Cable>_C_ShadowDivergenceCount = 0` reads as **NOT EXERCISED**.
  Phase 5 must add a capture-side evidence gate (e.g.,
  capture-equivalent `UpdatePosition` + helper reorder, or a
  different metric entirely) before flipping capture audible
  ownership.
- **Runtime `StatOverrunCounter` rising is NOT observed or proven** in
  Phase 3 evidence. Under the v6 lifecycle (`AoTransportOnRunEx`
  resets the helper anchor quartet on every RUN entry to align with
  legacy's "active-time since most recent RUN" semantics; see
  `step4.md` § Approach), the overrun threshold (advance >
  `SampleRate / 2` in a single helper call) is practically
  unreachable in PAUSE/RUN/normal-drive scenarios. The
  "overrun does not bump `DbgShadowDivergenceHits`" property is
  guaranteed by **static branch ordering** instead: inside
  `AoCableAdvanceByQpc`, the overrun bail-out, the first-call seed
  branch, the long-window QPC rebase, the 8-frame gate-fail, and
  the helper-side backwards-baseline guard each
  `KeReleaseSpinLock + return` BEFORE the QUERY compare block at
  the function tail. The `rt->Active == FALSE` gate on the compare
  block likewise excludes non-RUN queries. Operational corroboration
  is the v6 stress evidence (rapid open/close 8-cycle + A/B 30 s
  sine drive + pause/resume 4-cycle) showing zero spurious
  `ShadowDivergenceCount` bumps in any observed scenario.
- **Live-call (Phone Link) ShadowDivergenceCount evidence is NOT
  collected** in Phase 3. Step 5 v3 evidence is sine-drive proxy
  at 48 kHz / 16-bit / 2-ch, which exercises the identical helper
  code path (QUERY via WASAPI `GetPosition` / `GetPositions`,
  TIMER via engine DPC, 63/64 correction in
  `apply_drift_correction`). Phase 4 Step 1 must re-check
  `<Cable>_R_ShadowDivergenceCount <= 5/min` on a real Phone Link
  live call IMMEDIATELY BEFORE flipping audible ownership per the
  Forbidden Carry-Over rule below.
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
