# VB-Cable AO Dual-Plan Operating Rules

Date: 2026-04-13  
Applies to:

- `docs/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CODEX.md`
- `results/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CLAUDE.md`

Purpose:

- keep the two-document workflow stable during implementation
- prevent silent plan drift while coding
- make conflict resolution explicit before code changes begin

---

## Why this file exists

We intentionally keep two plan documents:

- Codex plan = architecture baseline and merge gate
- Claude plan = execution notebook and implementation detail map

That only works if the decision hierarchy is written down in one shared place.
This file is that shared place.

---

## Rule 1. Truth hierarchy

When these sources disagree, resolve them in this order:

1. live runtime evidence and current source code
2. Codex plan
3. Claude plan

Meaning:

- if WinDbg evidence or the current AO source disproves both documents, update
  the documents
- if Codex and Claude disagree on architecture, phase order, threshold,
  ownership, or exit criteria, Codex wins
- if both documents agree but the code says otherwise, the code/runtime evidence
  wins and the documents must be corrected

This rule prevents the team from treating either document as more real than the
codebase or the measured runtime behavior.

---

## Rule 2. Scope split between the two plans

Use the documents for different jobs.

Codex owns:

- architecture interpretation
- phase ordering
- transport ownership boundaries
- thresholds and acceptance criteria
- rollback rules
- merge/ship gates

Claude owns:

- code-location detail
- file/function editing checklist
- WinDbg breakpoints and inspection flow
- per-phase implementation notes
- richer scenario matrices and local bring-up notes

Short version:

- decide with Codex
- execute with Claude

---

## Rule 3. Mandatory phase workflow

Every implementation phase follows this sequence.

### Before starting a phase

Read the matching phase in the Codex plan first.

Goal:

- reload the intended architecture
- reload the exact exit criteria
- avoid drifting into implementation-led redesign

### While editing code

Keep the matching phase in the Claude plan open.

Use it for:

- edit locations
- helper breakdown
- diagnostics
- WinDbg commands
- test checklist

### Before closing a phase

Return to the Codex plan and verify:

- the phase goal was actually met
- the intended owner changed only where the plan allowed it
- the acceptance criteria were met as written
- the rollback path still works

If Claude suggests a local shortcut that violates the Codex phase boundary, the
shortcut is rejected.

---

## Rule 4. Rule promotion path

New architectural rules are promoted in one direction only:

1. write the rule in Codex first
2. then mirror or absorb it into Claude if execution notes need it

Do not introduce a new architecture rule only inside the Claude plan.

Examples of "rule-level" content:

- new phase gate
- new threshold
- new rollback requirement
- new ownership constraint
- new architecture claim

Examples of Claude-only content that may remain local until promoted:

- exact edit location
- temporary code sketch
- breakpoint command
- implementation TODO ordering inside a phase

This prevents important rules from being buried inside long code-oriented text.

---

## Rule 5. Conflict handling

If Codex and Claude disagree in a way that affects code changes:

1. stop the implementation step
2. identify whether the conflict is about architecture or execution detail
3. if architecture: follow Codex
4. if execution detail only: Claude may lead
5. if runtime evidence contradicts both: update both documents before
   continuing

Never "split the difference" by coding a hybrid interpretation without updating
the documents.

---

## Rule 6. One-owner rule during transport migration

During any phase that changes transport ownership:

- each cable direction must have exactly one transport owner
- if ownership is moving, rollback must be proven in a live session
- if counters suggest double-advance, pause implementation and resolve the
  ownership map before continuing

This rule is derived from the Codex phase model and must be enforced even if a
local Claude snippet looks convenient.

---

## Rule 7. Documentation update cadence

Documentation must be updated at these moments:

- when a new architecture rule is discovered
- when a phase gate changes
- when a threshold changes
- when runtime evidence invalidates a previous claim
- when a rollback mechanism changes

Preferred update pattern:

1. update Codex if the change affects rules or architecture
2. update Claude if the change affects implementation notes or local execution
3. update both if the change affects both

---

## Rule 8. Recommended daily operating rhythm

For active implementation periods:

- read this file once before starting work
- read the active Codex phase at phase entry
- keep the matching Claude phase open during coding
- re-check Codex exit criteria before commit or phase close

Practical shorthand:

- Codex at phase start and phase end
- Claude during the middle

---

## Example workflow

### Example: starting Phase 3

1. Read Codex Phase 3
2. Confirm the intended meaning:
   - query-driven helper exists
   - transport still owned by legacy path
   - comparison is windowed
3. Open Claude Phase 3
4. Edit the exact fields, helper shell, diagnostics, and WinDbg probes
5. Run build/test/debug
6. Re-open Codex Phase 3 exit criteria
7. Confirm:
   - divergence behavior matches plan
   - rollback works
   - no ownership moved early

If Phase 3 code starts to "almost" move transport ownership, stop and push that
work back to the proper later phase.

---

## Bottom line

The two-plan system only works if we keep their roles strict:

- Codex defines what is allowed
- Claude helps us carry it out
- source code and runtime evidence can overrule both

If this discipline breaks, the documents stop being a safety net and become two
competing stories. This file exists to prevent that.
