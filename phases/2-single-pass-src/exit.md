# Phase 2 Exit: SRC ready

## Exit Gate

- [ ] Steps 0, 1, 2, 3 all marked completed.
- [ ] Round-trip rate matrix passes.
- [ ] Phase 1 acceptance criteria still pass (no regression in
      same-rate path).
- [ ] Build clean, no new warnings.
- [ ] No allocations on the SRC hot path.

## Outcome

After Phase 2, the cable ring supports the full V1 KSDATARANGE rate set
via single-pass linear-interp SRC with GCD divisor 300/100/75. The
write path hard-rejects on overflow; the read path silence-fills with
hysteretic recovery on underrun.

The cable transport ownership has not yet flipped — Phases 1 and 2 are
**under the hood**. Cable streams still flow through the legacy
`UpdatePosition`-driven path. Phase 3 introduces the canonical helper
in shadow mode; Phases 4 and 5 flip ownership.

## Forbidden Carry-Over Into Phase 3

- Do not let Phase 3 add a second SRC path or call site outside
  `AoRingWriteFromScratch` / `AoRingReadToScratch`.
- Do not let Phase 3 weaken hard-reject to "soft skip" because the
  helper makes overflow easier to mitigate. Hard-reject stays.

## Phase 2 → Phase 3 Handoff

Phase 3 implements `AoCableAdvanceByQpc` in `transport_engine.cpp`,
hooks every cable call source (query path, shared timer, packet
surface) to it in shadow mode (helper computes everything but the
audible cable transport stays on legacy `UpdatePosition`-driven path).
