# Phase 0 Step 2: Architecture frozen + planning files restarted

Status: completed (2026-04-25)

## Read First

- `docs/PRD.md`
- `docs/ADR.md`
- `docs/AO_CABLE_V1_ARCHITECTURE.md`
- `docs/AO_CABLE_V1_DESIGN.md`
- `docs/REVIEW_POLICY.md`
- `docs/GIT_POLICY.md`
- `CLAUDE.md`
- `AGENTS.md`

## Goal

Lock the architecture and adopt a clean planning-file structure modeled on
the parallel `ao-cable-v2-step2b-merge` project, before starting any
implementation work in Phase 1.

## What Was Done

1. Discarded prior planning artifacts that were structurally messy:
   - removed `docs/AO_FIXED_PIPE_ARCHITECTURE.md`,
     `docs/CURRENT_STATE.md`, `docs/V2_RESEARCH_INDEX.md`,
     `docs/PIPELINE_V2_CHANGELOG.md`, `docs/EVIDENCE_INVENTORY.md`.
2. Adopted the V2-project planning layout (V1 context):
   - `docs/PRD.md` — product identity / scope / success criteria.
   - `docs/ADR.md` — 12 architecture decisions (at the time of this
     step; later extended to 14 — ADR-013 timer period, ADR-014
     per-phase branch model superseding ADR-012).
   - `docs/AO_CABLE_V1_ARCHITECTURE.md` — system overview.
   - `docs/AO_CABLE_V1_DESIGN.md` — file-level + struct-level detail.
   - `docs/REVIEW_POLICY.md` — review standard.
   - `docs/GIT_POLICY.md` — single-branch + commit-prefix at this
     step's time. Switched to per-phase-branch + verified-merge by
     ADR-014 between Phase 0 exit and Phase 1 entry; the historical
     single-branch model recorded here is the policy under which
     Phase 0 actually ran.
3. Replaced agent guidance:
   - `CLAUDE.md` (V1 context, V1 forbidden-drift list).
   - `AGENTS.md` (Codex counterpart, same shared rules).
4. Added slash-command harness:
   - `.claude/commands/harness.md`.
   - `.claude/commands/review.md`.
5. Added phase scaffolding:
   - `phases/index.json` (phases 0..7).
   - `phases/<N>-name/index.json` per phase.
   - `phases/<N>-name/step<N>.md` + `exit.md` per phase.
6. Added phase execution helper `scripts/execute.py` (status / next /
   mark).

## Acceptance Criteria

- [x] `python scripts/execute.py status 0-baseline` runs without error and
      lists steps 0/1/2.
- [x] `python scripts/execute.py status 1-int32-ring` runs and shows
      planned status.
- [x] `docs/PRD.md` exists and references `docs/ADR.md` /
      `docs/AO_CABLE_V1_DESIGN.md` correctly.
- [x] `docs/ADR.md` contains ADR-001 through ADR-012.
- [x] `CLAUDE.md` and `AGENTS.md` agree on collaboration role split,
      forbidden-compromises list, source-of-truth order, and git policy.
- [x] `.claude/commands/harness.md` and `review.md` reference the V1 doc
      set, not V2.

## Completion

```powershell
python scripts/execute.py mark 0-baseline 2 completed --message "Planning files restarted; structure ready for Phase 1."
```

(Already marked completed.)
