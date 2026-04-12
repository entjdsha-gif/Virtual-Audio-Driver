# AO Virtual Cable - Codex Instructions

This file exists to give future Codex sessions a visible, repo-local starting
point.

It does **not** guarantee automatic loading by every future session, but it is
the intended first file to open for AO/VB reimplementation work.

---

## Read This First

For any AO vs VB planning or implementation session, read these in order:

1. `docs/VB_CABLE_DUAL_PLAN_OPERATING_RULES.md`
2. `docs/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CODEX.md`
3. `results/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CLAUDE.md`

Short version:

- operating rules first
- Codex architecture plan second
- Claude execution plan third

---

## Truth Hierarchy

When sources disagree, resolve them in this order:

1. live runtime evidence and current source code
2. Codex plan
3. Claude plan

Meaning:

- measured behavior and current AO source can overrule both documents
- Codex wins on architecture, phase order, thresholds, ownership, and rollback
- Claude wins on local execution detail, code-location notes, and bring-up
  workflow

---

## Two-Plan Workflow

Use the documents for different purposes.

Codex plan:

- architecture baseline
- implementation order
- ownership boundaries
- acceptance criteria
- rollback rules

Claude plan:

- code editing checklist
- breakpoint recipes
- scenario verification detail
- file/function implementation notes

Daily operating rhythm:

1. read Codex before a phase starts
2. keep Claude open while editing
3. return to Codex before closing the phase

---

## Current Shared Rule

If Codex and Claude disagree:

- do not improvise a hybrid implementation in code
- resolve the disagreement against the operating-rules document
- prefer Codex unless current source/runtime evidence disproves both

---

## Key Files

- shared operating rules:
  - `docs/VB_CABLE_DUAL_PLAN_OPERATING_RULES.md`
- Codex implementation baseline:
  - `docs/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CODEX.md`
- Claude execution notebook:
  - `results/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CLAUDE.md`
- AO stream implementation:
  - `Source/Main/minwavertstream.cpp`
  - `Source/Main/minwavertstream.h`
- frame-pipe primitives:
  - `Source/Utilities/loopback.cpp`
  - `Source/Utilities/loopback.h`

---

## Important Reminder

The goal is not to keep two competing stories alive.

The goal is:

- one architecture baseline
- one execution notebook
- one truth hierarchy

If a future session changes rules, thresholds, ownership, or phase gates,
promote that change into the Codex plan and keep the operating-rules document
consistent with it.
