# Phase 4 Exit: render audible owner is the canonical helper

## Exit Gate

- [ ] Steps 0..2 marked completed.
- [ ] Build clean.
- [ ] Live AO call render quality matches VB on the same path.
- [ ] Cable A and Cable B render both stable.
- [ ] Legacy `UpdatePosition` and `WriteBytes` cable-render branches
      are no-op for cable streams (retired atomically in Step 1).
- [ ] No regression in non-cable streams.

## Outcome

Phase 4 closes the cable render-side ownership flip. The helper writes
DMA → scratch → ring with the fade envelope, advances render cursors
and monotonic counters, and runs from query / timer / packet sources.
Legacy render-side paths are no-op for cable. Capture is still on
legacy until Phase 5.

## Forbidden Carry-Over Into Phase 5

- Phase 5 must not split capture ownership. Helper capture branch is
  the single owner.
- Phase 5 must not keep `MicSink` dual-write under any flag.

## Phase 4 → Phase 5 Handoff

Phase 5 implements the helper capture branch (real ring → scratch →
DMA), retires the legacy capture write, and validates capture quality
on a live call.
