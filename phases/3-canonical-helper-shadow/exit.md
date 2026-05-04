# Phase 3 Exit: helper ready, audible flip blocked on agreement

## Exit Gate

- [ ] Steps 0..4 marked completed.
- [ ] Build clean.
- [ ] Live call test: `DbgShadowQueryHits` and `DbgShadowTimerHits`
      both increase steadily during a live call. Helper is reached.
- [ ] `<Cable>_<R/C>_ShadowDivergenceCount` is 0 (or near 0) in
      steady-state speech for at least one full live call.
- [ ] No audible regression vs Phase 2 exit baseline. Helper is in
      shadow mode and must not touch audible behavior.
- [ ] No BSOD / hang / deadlock under stream open/close stress.
- [ ] `KeFlushQueuedDpcs` on stream destructor confirmed correct.

## Outcome

After Phase 3, every cable transport call source funnels into the
helper. The helper computes everything (advance, gate, overrun,
drift correction) and writes shadow state. The legacy
`UpdatePosition`-driven path still owns audible cable transport.

The shadow divergence counter being zero in steady state proves the
helper's advance math agrees with the legacy advance math within
the 8-frame gate tolerance. This unlocks Phase 4 audible flip on the
render side.

## Forbidden Carry-Over Into Phase 4

- Phase 4 must not introduce a third "publish state for the timer
  to consume" pattern. Audible ownership flips **directly** from
  legacy to helper.
- Phase 4 must not flip audible ownership without first confirming
  the divergence counter is zero on a live call.

## Phase 3 → Phase 4 Handoff

Phase 4 retires the legacy `UpdatePosition`-driven cable render
path and lets the helper own render audibly. Capture stays on
legacy until Phase 5.
