# Dual-Plan Operating Rules Pointer

This folder's execution plan is:

- `results/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CLAUDE.md`

The shared operating rules for the Codex/Claude two-document workflow are:

- `docs/VB_CABLE_DUAL_PLAN_OPERATING_RULES.md`

Workflow summary:

- Codex decides architecture, phase order, thresholds, acceptance criteria,
  ownership boundaries, and rollback rules
- Claude drives execution detail, code-location notes, WinDbg probes, and
  implementation checklists
- if the two documents disagree, follow the shared operating-rules file and
  prefer Codex unless current source/runtime evidence disproves both

This pointer file exists so the Claude-side folder also visibly carries the
same operating contract.
