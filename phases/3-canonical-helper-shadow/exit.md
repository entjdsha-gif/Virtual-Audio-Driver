# Phase 3 Exit: helper ready, audible flip blocked on agreement

## Exit Gate

- [ ] Steps 0..4 marked completed.
- [ ] Build clean.
- [ ] Live call test: `DbgShadowQueryHits` and `DbgShadowTimerHits`
      both increase steadily during a live call. Helper is reached.
- [ ] `<Cable>_<R/C>_ShadowDivergenceCount` is **<= 5 increments per
      minute** during steady-state speech for at least one full live
      call. Any single increment indicates a per-tick advance delta
      that exceeded the 8-frame gate tolerance defined in
      `phases/3-canonical-helper-shadow/step4.md`. <= 5/minute is
      consistent with normal scheduler jitter; > 5/minute indicates a
      cursor / drift bug that must be fixed before Phase 4.
- [ ] No audible regression vs Phase 2 exit baseline. Helper is in
      shadow mode and must not touch audible behavior.
- [ ] No BSOD / hang / deadlock under stream open/close stress.
- [ ] `KeFlushQueuedDpcs` on stream destructor confirmed correct.

## Outcome

After Phase 3, every cable transport call source funnels into the
helper. The helper computes everything (advance, gate, overrun,
drift correction) and writes shadow state. The legacy
`UpdatePosition`-driven path still owns audible cable transport.

The shadow divergence counter staying within the steady-state gate
(**<= 5 increments per minute**) proves the helper's advance math
agrees with the legacy advance math within the 8-frame per-call
tolerance. This is the single numeric precondition that unlocks
Phase 4 audible flip on the render side.

> Note: a strictly *zero* divergence count is not realistic given
> normal Windows scheduler jitter. The exit gate is "<= 5/min for at
> least one full live call." Phase 4 Step 1 must re-check the same
> threshold immediately before flipping audible ownership; do not
> rely on a stale Phase 3 measurement.

## Forbidden Carry-Over Into Phase 4

- Phase 4 must not introduce a third "publish state for the timer
  to consume" pattern. Audible ownership flips **directly** from
  legacy to helper.
- Phase 4 must not flip audible ownership without re-confirming
  divergence counter <= 5/min on a live call **immediately before**
  the flip commit (not on a stale Phase 3 measurement).

## Phase 3 -> Phase 4 Handoff

Phase 4 retires the legacy `UpdatePosition`-driven cable render
path and lets the helper own render audibly. Capture stays on
legacy until Phase 5.
