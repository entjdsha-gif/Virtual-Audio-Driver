# Phase 0 Step 1: Branch consolidation

Status: completed (2026-04-25)

## Read First

- `docs/GIT_POLICY.md`
- `docs/ADR.md` ADR-012 (single-branch + commit-prefix model)

## Goal

Consolidate the AO Cable codebase back to a single active branch and a single
worktree before starting V1 implementation. Multiple parallel branches /
worktrees during Phase 5 / Phase 6 created coordination overhead and led to
the same regression being investigated in two different copies.

## What Was Done

1. Merged `feature/ao-phase6-core` (Phase 6 implementation iterations,
   including Y1A/B/C, Y2-1, Y2-2, Y3 attempts and rollback to Y2-2) into
   `feature/ao-fixed-pipe-rewrite` with `--no-ff`.
   - Source/* conflicts resolved in favor of phase6-core (Phase 5 pump
     model retired in favor of Phase 6 transport_engine).
   - merge commit: `30fb344`.
2. Deleted `feature/ao-phase6-core` and the experimental
   `archive/simple-loop-byte-ring-experiment` branches.
3. Removed the `D:/mywork/ao-phase6` worktree.
4. Archived superseded planning docs to `docs/archive/` and
   `results/archive/`.
5. Swept all stale references to `ao-phase6` / `phase6-core` in the
   remaining live docs.

## Final State

- Single active branch: `feature/ao-fixed-pipe-rewrite`.
- Single worktree: `D:/mywork/Virtual-Audio-Driver`.
- Frozen reference branches (untouched): `feature/ao-pipeline-v2`,
  `feature/ao-telephony-passthrough-v1`.

## Completion

```powershell
python scripts/execute.py mark 0-baseline 1 completed --message "Single branch + single worktree."
```

(Already marked completed.)
