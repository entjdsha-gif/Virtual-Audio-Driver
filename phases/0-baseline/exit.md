# Phase 0 Exit: baseline ready for Phase 1

Status: completed (2026-04-25)

## Exit Gate

Phase 0 exits when all of the following are true. All passed.

- [x] VB-Cable RE evidence committed and reviewed (Step 0).
- [x] Single branch / single worktree in effect (Step 1).
- [x] V1 planning files in place (Step 2): PRD, ADR (12), ARCHITECTURE,
      DESIGN, REVIEW_POLICY, GIT_POLICY, CLAUDE.md, AGENTS.md, slash
      commands, phase scaffolding, execute.py.
- [x] Build sanity on `feature/ao-fixed-pipe-rewrite` HEAD: `.\build-verify.ps1
      -Config Release` succeeds (verify before starting Phase 1).
- [x] No stale references to Phase 5 / Step 3-4 / Option Y/Z in the live
      doc set (archived).

## What Phase 1 Inherits

- Confirmed VB target shape for the cable transport core.
- Stable code baseline at the merge of phase6-core into
  `feature/ao-fixed-pipe-rewrite`.
- Clean planning structure: PRD locks goal, ADR-001..012 lock decisions,
  ARCHITECTURE locks system shape, DESIGN locks file/struct/function
  shape.
- Working build / install / signing pipeline.
- Live-call test harness (`tests/live_call/run_test_call.py`) for Phase
  1+ validation gates.

## Forbidden Work In Phase 1

(Lifted from `CLAUDE.md` / `AGENTS.md` "Forbidden Compromises". Listed
here so the Phase 1 implementer does not need to re-read every doc.)

- No re-introduction of packed 24-bit ring storage.
- No re-introduction of the 4-stage cable conversion pipeline.
- No re-introduction of 8-tap sinc SRC for cable streams.
- No re-introduction of `MicSink` dual-write.
- No FormatMatch enforcement.
- No second cable transport owner outside the canonical helper.
- No silent ring overflow (must hard-reject + counter).
- No `ms` in cable transport runtime state.

## Next

Phase 1 starts: `phases/1-int32-ring/step0.md`.
