# Phase 1 Step 4: Hard-reject overflow + counter audit

## Read First

- `docs/ADR.md` ADR-005 (hard-reject overflow + hysteretic underrun)
- `docs/REVIEW_POLICY.md` § 2 (forbidden drift) — silent overflow is
  forbidden.
- Phase 1 Step 2 already implements hard-reject in the write path.
  This step is the **audit** that verifies no other path silently
  overwrites the ring.

## Goal

Audit `Source/Utilities/loopback.cpp` and confirm that no remaining code
path silently overwrites cable ring data on overflow. All paths that
could touch `pipe->Data` / `pipe->WritePos` must:

1. Go through `AoRingWriteFromScratch` (which hard-rejects), OR
2. Be deleted (legacy paths that contradict ADR-005).

## Planned Edits

This step is mostly read-and-classify, with targeted deletions:

- Inspect every `loopback.cpp` function that writes to the cable ring.
- For each function, decide:
  - **Keep** if it's `AoRingWriteFromScratch` or its helpers.
  - **Delete** if it's a legacy write path (e.g., `LoopbackWriteRaw`
    or similar pre-rewrite functions that bypassed overflow checks).
  - **Refactor** if it's a path that is still called from somewhere
    valid but uses the wrong overflow semantics (rare; should be
    captured during pre-step inspection).

## Rules

- Tell the user before deleting any function.
- If a function is called from a non-cable path that still relies on
  silent overwrite, **stop and report** — do not delete or refactor
  without approval. (Non-cable streams are out of scope for V1; their
  legacy behavior may be preserved.)
- Cite the call sites explicitly in the report.

## Acceptance Criteria

- [ ] Audit report committed to `phases/1-int32-ring/step4-audit.md`
      listing every cable-ring write path and its disposition (keep /
      delete / refactor).
- [ ] No remaining cable-ring write path increments `WritePos` without
      incrementing `OverflowCounter` on overflow.
- [ ] Build clean.
- [ ] Step 2's overflow scenario test still passes.
- [ ] No non-cable (mic-array / speaker / save-data / tone-generator)
      regression — sample test on those paths still works (manual
      inspection if no automated coverage exists).

## What This Step Does NOT Do

- No new functionality.
- No caller swap.

## Completion

```powershell
python scripts/execute.py mark 1-int32-ring 4 completed --message "Audit complete; only AoRingWriteFromScratch writes the cable ring."
```
