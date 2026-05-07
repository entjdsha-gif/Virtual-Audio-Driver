# Phase 1 Exit: INT32 frame-indexed ring ready

## Exit Gate

All of the following are true:

- [ ] Step 0 (cross-TU access removal), Step 1 (struct/API shape),
      Step 2 (write same-rate), Step 3 (read same-rate), Step 4
      (overflow audit), Step 5 (underrun hysteresis), Step 6
      (diagnostics) all marked completed.
- [ ] Each step's commit has a passing review per
      `docs/REVIEW_POLICY.md`.
- [ ] `build-verify.ps1 -Config Release` succeeds clean (no new
      warnings).
- [ ] `tests/phase1-runtime/` contains the underrun hysteresis test
      and its passing trace (untracked).
- [ ] Forced-overflow and forced-underrun scenarios show counters via
      `test_stream_monitor.py`.
- [ ] No regression in non-cable paths (mic-array / speaker / save-
      data / tone-generator) — manual sanity confirmed.

## Outcome

After Phase 1, the cable `FRAME_PIPE` matches `docs/AO_CABLE_V1_DESIGN.md`
§ 2 exactly. Same-rate write/read works correctly with hard-reject
overflow and hysteretic underrun. SRC is still missing (Phase 2). The
canonical advance helper is still missing (Phase 3). Cable transport
ownership has not yet flipped (Phases 4 / 5).

## Forbidden Carry-Over Into Phase 2

- Do not let Phase 2 weaken the hard-reject by adding a "skip if
  overflow imminent" path on the caller side. Hard-reject is the
  single source of truth.
- Do not let Phase 2 introduce a second SRC path. Single function per
  direction (per ADR-004).

## Phase 1 → Phase 2 Handoff

Phase 2 implements the SRC path: GCD divisor 300/100/75, linear-
interpolation accumulator, per-channel state. Same atomic call,
same hard-reject on the write side, same hysteretic recovery on the
read side.
