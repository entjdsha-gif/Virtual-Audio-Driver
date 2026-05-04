# Phase 3 Step 4: Shadow divergence counter

## Read First

- `docs/AO_CABLE_V1_DESIGN.md` § 7 (diagnostics IOCTL).
- Phase 1 Step 5 (existing `AO_V2_DIAG` extension pattern).

## Goal

Compare the helper's shadow-state advance computation against the
legacy `UpdatePosition`-driven advance, on each helper invocation,
and increment a divergence counter when the two disagree. This
provides quantitative evidence that Phase 4 / 5 is safe to flip
audible ownership.

## Planned Files

Edit atomically:

- `Source/Utilities/transport_engine.cpp` — inside helper, capture
  the legacy advance (computed by `UpdatePosition` for the same time
  delta) and compute the per-call diff. Bump
  `DbgY2RenderMismatchHits` (renamed in V1: `DbgShadowDivergenceHits`)
  when diff exceeds tolerance.
- `Source/Main/ioctl.h` — add `<Cable>_<R/C>_ShadowDivergenceCount`
  fields to `AO_V2_DIAG`.
- `Source/Main/adapter.cpp` — `IOCTL_AO_GET_STREAM_STATUS` writes the
  new field.
- `test_stream_monitor.py` — reads and prints.

## Required Edits

Inside `AoCableAdvanceByQpc` (after computing `advance`):

```c
/* Shadow divergence: compare helper-computed advance against the
 * legacy ms-based advance recorded by UpdatePosition. Tolerance is
 * 8 frames (1 gate width) per call. */
if (rt->LegacyLastAdvance != 0 && advance != 0) {
    LONG diff = advance - rt->LegacyLastAdvance;
    if (diff < 0) diff = -diff;
    if ((ULONG)diff > 8) {
        rt->DbgShadowDivergenceHits++;
    }
}
```

`rt->LegacyLastAdvance` is populated by `UpdatePosition` cable branch
each tick (per ADR-011 unit policy: in frames, not ms).

## Rules

- Three-file atomic edit per `docs/REVIEW_POLICY.md` § 7.
- Tolerance is 8 frames; values outside tolerance count as
  divergence. The 100-tick rebase will reset the comparison; that's
  expected.
- Do not act on divergence counter in the helper (do not skip the
  helper if it diverges). It is observability only.

## Acceptance Criteria

- [ ] Build clean.
- [ ] Live cable stream shows
      `<Cable>_<R/C>_ShadowDivergenceCount == 0` (or near zero) in
      steady state — proving helper and legacy agree, which is the
      precondition for Phase 4 to flip audible ownership safely.
- [ ] Forced overrun scenario does not spuriously bump divergence.

## What This Step Does NOT Do

- Does not flip audible ownership.

## Completion

```powershell
python scripts/execute.py mark 3-canonical-helper-shadow 4 completed --message "Shadow divergence counter; helper agrees with legacy in steady state."
```
