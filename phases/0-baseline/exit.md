# Phase 0 Exit: baseline ready for Phase 1

Status: completed (2026-04-25)

## Exit Gate

Phase 0 exits when all of the following are true. All passed.

- [x] VB-Cable RE evidence committed and reviewed (Step 0).
- [x] Single branch / single worktree in effect (Step 1).
- [x] V1 planning files in place (Step 2): PRD, ADR (12 at exit;
      ADR-013 added during Phase 0 review fix bundle), ARCHITECTURE,
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
- Clean planning structure: PRD locks goal, ADR-001..013 lock decisions,
  ARCHITECTURE locks system shape, DESIGN locks file/struct/function
  shape.
- Working build / install / signing pipeline.
- Live-call test harness (`tests/live_call/run_test_call.py`) for Phase
  1+ validation gates.

## Forbidden Work In Phase 1

The full canonical forbidden-compromises list lives in
`docs/REVIEW_POLICY.md` § 2. Phase 1 implementers and reviewers must
read that section before starting; this exit document does not
duplicate it.

## Next

Phase 1 starts: `phases/1-int32-ring/step0.md`.
