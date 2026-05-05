# Phase 5 Exit: V1 ships-ready audibly

## Exit Gate

- [ ] Steps 0..1 marked completed.
- [ ] Live-call parity with VB-Cable confirmed (Step 1 acceptance).
- [ ] All forbidden-compromises (CLAUDE.md / AGENTS.md) are absent
      from cable code path:
      - no packed 24-bit ring;
      - no 4-stage cable conversion pipeline;
      - no 8-tap sinc cable SRC;
      - no `MicSink` dual-write;
      - no FormatMatch enforcement;
      - no second cable transport owner.
- [ ] Build clean. Install clean. No BSOD under stress.
- [ ] Cable A and Cable B are independent (heavy traffic on one does
      not affect the other).

## Outcome

Phase 5 is the V1 ships-ready milestone for cable transport quality.
After Phase 5 exits, AO Cable V1 produces clean live-call audio
parity with VB-Cable on the same harness. Render and capture are
both owned by the canonical helper. Legacy MSVAD-derived cable
transport is no-op or removed.

The **product** still has cleanup work in Phase 6 and quality polish
in Phase 7 before the M6 shipping milestone, but the **cable
transport rewrite goal** of V1 is achieved here.

## Forbidden Carry-Over Into Phase 6

- Phase 6 cleanup must not delete anything that Phase 5 still relies
  on (e.g., the helper, fade table, scratch buffer allocation).
- Phase 6 must not reintroduce any retired pattern under a new name.

## Phase 5 → Phase 6 Handoff

Phase 6 removes retired Phase 5 / Step 3-4 / Y-attempt scaffolding
(pump flags, sinc table, packed-24 conversion, MicSink remnants),
strips stale comments, and prepares the codebase for Phase 7 quality
polish.
