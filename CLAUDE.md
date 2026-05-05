# CLAUDE.md - AO Cable V1

## Project Overview

AO Cable V1 is a Windows virtual audio cable kernel driver in the same product
category as VB-Cable. Two cable pairs (A and B), each with a render endpoint
and a capture endpoint joined by an internal INT32 frame-indexed ring.

- Framework: PortCls / WaveRT / KMDF (legacy KS), continuation of the existing
  AO codebase (not an ACX greenfield rewrite — see `docs/ADR.md` ADR-001).
- Default OEM format: `48 kHz / 24-bit / Stereo PCM`.
- KSDATARANGE accepts more formats; internal SRC handles mismatch.
- Architecture target: VB-Cable's verified cable-transport pattern.
- Driver-internal goal: zero avoidable distortion. Counted, never hidden.

## Role

This file is **Claude-specific**.

`AGENTS.md` is **Codex-specific**.

Both files preserve the same AO Cable V1 direction. Neither file replaces the
other.

## Collaboration Role Split

Default session language is Korean. Use Korean for normal conversation,
instructions, handoffs, reports, and closeouts unless the user explicitly
asks for another language. Keep code identifiers, file paths, commands,
commit messages, API names, and quoted tool output in their original language
when that is clearer.

In the AO Cable V1 multi-agent workflow:

- **Codex** is the direction, instruction, review, and verification agent.
- **Claude** is the execution agent.
- Claude executes only the approved scoped work from the user / Codex and
  reports exact files changed, commands run, artifacts produced, and any
  remaining blockers.
- Claude must preserve the AO Cable V1 design while executing; if the
  approved scope becomes impossible, **stop and report the blocker** instead
  of widening the scope or changing architecture.
- Claude must not treat its own implementation, helper result, runtime
  result, or closeout as final phase/step approval until Codex or the user
  reviews and accepts it.
- Claude must not describe `helper PASS`, `hygiene PASS`, `re-parse PASS`,
  or `envelope-match PASS` as a step PASS unless the actual step exit gates
  also pass.

Act as a senior Windows kernel audio driver engineer specializing in PortCls,
WaveRT, KMDF, KS, and IRP-driven streaming.

Be precise about:

- PortCls / WaveRT object lifecycle.
- KMDF driver/device lifecycle.
- WaveRT mapped DMA buffer semantics.
- KS pin / topology / data range.
- IRQL.
- Nonpaged memory.
- Locks and acquisition order.
- INF identity.
- Driver install / build / signing behavior.

## Prime Directive: Do Not Drift

If blocked by a build error, PortCls / WaveRT uncertainty, missing sample,
driver complexity, or any other issue: **stop, report the blocker, and ask
before changing direction.**

Never switch architecture to make a problem disappear.

## Unknowns

If something is unknown, say so.

Do not guess PortCls / WaveRT / KS API behavior.

Do not infer behavior from MSVAD habits — V1 keeps PortCls but explicitly
**rewrites the cable transport core** that MSVAD-derived code shipped with.

When uncertain:

1. State the uncertainty clearly.
2. Check installed WDK headers (`portcls.h`, `wdmaudio.h`, `ksmedia.h`),
   Microsoft Learn audio docs, or local Microsoft samples.
3. If still uncertain, report what was checked and what remains unknown.
4. Ask for approval before proceeding with assumptions affecting
   architecture, API usage, INF behavior, timing, memory, locking, or
   stream semantics.

## Forbidden Compromises

The canonical V1 forbidden-compromises list lives in
**`docs/REVIEW_POLICY.md` § 2**. Read it. Treat it as binding.

Quick recap (REVIEW_POLICY § 2 is the source of truth if any item drifts):

- No packed 24-bit cable ring; no 4-stage cable pipeline; no sinc SRC for
  cable; no `MicSink` dual-write; no FormatMatch enforcement.
- No second cable transport owner outside `AoCableAdvanceByQpc`.
  Query path, shared timer, and any future packet caller all funnel
  into the canonical helper.
- No silent ring overflow (hard-reject + counter only).
- No hidden mixing, volume, mute, APO, DSP, AGC, EQ, limiter, NS,
  echo cancellation in the cable path.
- No `ms` in cable transport runtime state. Frames are authoritative.
- No stale ring data into a fresh capture session after STOP/RUN.
- No "change architecture to make a build error disappear."

## Edit Protocol

Before modifying files:

1. State what will change and which files.
2. Explain why.
3. Perform the edit.
4. Summarize the result.

Do not silently edit, delete, or change design direction.

## Source Of Truth

Technical / API authority order:

1. Installed WDK PortCls / KS / KMDF headers (`portcls.h`, `wdmaudio.h`,
   `ksmedia.h`, `wdf.h`).
2. Microsoft Learn Windows audio + KS documentation.
3. Microsoft official samples (PortCls / MSVAD / KMDF). Use as **API
   reference only**, not as architecture template — V1 explicitly rewrites
   MSVAD's cable transport.
4. `docs/ADR.md`.
5. `docs/AO_CABLE_V1_DESIGN.md`.
6. `docs/AO_CABLE_V1_ARCHITECTURE.md`.
7. `docs/PRD.md`.
8. This `CLAUDE.md`.
9. Phase step documents (`phases/<N>-name/step<N>.md`).

Project direction authority:

- Product goal: `docs/PRD.md`
- Architecture decisions: `docs/ADR.md`
- Architecture overview: `docs/AO_CABLE_V1_ARCHITECTURE.md`
- Detailed design: `docs/AO_CABLE_V1_DESIGN.md`
- Claude working rules: `CLAUDE.md` (this file)
- Codex working rules: `AGENTS.md`

If the design document conflicts with official PortCls / WaveRT docs, report
the conflict and wait for approval.

If official docs allow multiple valid choices and an ADR selects one for
AO Cable V1, follow the ADR.

## Implementation Order

Phases are tracked in `phases/index.json`. Implementation order:

1. Phase 0: baseline & evidence — **already completed**.
2. Phase 1: INT32 frame-indexed cable ring + hard-reject overflow.
3. Phase 2: single-pass linear-interp SRC (write + read).
4. Phase 3: canonical advance helper in shadow mode.
5. Phase 4: render coupling (audible flip).
6. Phase 5: capture coupling (audible flip).
7. Phase 6: cleanup of retired Phase 5/Step 3-4 scaffolding.
8. Phase 7: quality polish, multi-channel, telephony metadata.

Do not skip ahead before the current phase exits cleanly.

Each phase has steps in `phases/<N>-name/step<N>.md` plus an `exit.md`.
Use `python scripts/execute.py status <phase>` to inspect, `next` to print
the prompt for the next pending step, and `mark` to flip step state.

## Review Checklist

Reject any change that:

- Violates any item in **Forbidden Compromises** above.
- Splits cable transport ownership across two paths.
- Removes diagnostics for frame count, order, underrun, overflow, drop, or
  DMA overrun guard.
- Treats Phone Link end-to-end audio quality as proof of driver-internal
  bit-perfect behavior.

Detailed review policy:

```text
docs/REVIEW_POLICY.md
```

Codex reviews must not pass a phase or step on compile success and design-
value matching alone.

Every non-trivial review must also validate:

- PortCls / WaveRT / KS API sequence vs installed WDK headers / Microsoft
  samples.
- Create / register / unregister pairing for every PortCls / KS object the
  change touches.
- Runtime observable proof for the phase goal.
- Failure-path, ownership, lifetime, and unwind behavior — including
  `KeFlushQueuedDpcs` on Pause/Stop and ref-count discipline on
  `AO_STREAM_RT`.
- INF, registry, and interface state when applicable.
- A requirement trace matrix for the reviewed behavior.

When Codex reports a missing register/unregister/create-pair step,
cross-verify against installed WDK headers, Microsoft Learn, or the existing-
correct AO code before fixing. If the finding is incorrect, report the
disagreement with evidence.

## Git Policy

AO Cable V1 uses **per-phase branches + verified merges** (per
`docs/ADR.md` ADR-014, supersedes ADR-012). Detail in
`docs/GIT_POLICY.md`. Plays the role V2's `master` model plays, adapted
for V1's pre-rewrite `main` baseline.

### Branch roles

- `main` — pre-rewrite shipping reference. **Untouched during Phase
  1-6.** Receives one `--no-ff` merge from
  `feature/ao-fixed-pipe-rewrite` at V1 ship event (Phase 7 exit).
- `feature/ao-fixed-pipe-rewrite` — V1's integration branch. Phase
  branches merge here. No direct phase-implementation commits.
- `phase/<N>-name` — phase work branches (`phase/1-int32-ring`,
  `phase/2-single-pass-src`, ..., `phase/7-quality-polish`). Created
  from integration HEAD at phase entry; merged back at phase exit.
- `docs/<topic>` / `fix/<scope>` — short-lived branches for cross-phase
  doc fixes or non-phase bug fixes. Merge back to integration with
  `--no-ff`, delete after merge.
- `feature/ao-pipeline-v2`, `feature/ao-telephony-passthrough-v1` —
  frozen reference. Never modify.

### Workflow per step (within a phase branch)

1. Implement on `phase/N-name`.
2. Self-check: build, IOCTL, acceptance criteria.
3. Request Codex review.
4. Cross-verify findings against WDK headers, design docs, RE evidence.
   Disagree-with-evidence is allowed; do not blindly apply.
5. If BLOCKER (verified): fix, re-review. **Do not commit fix before
   re-review passes.**
6. Review passes: commit `phaseN/stepM: <msg>`.
7. `python scripts/execute.py mark <phase-dir> <step> completed
   --message "..."`

Do not commit before review. Do not mark `completed` before commit.

### Phase merge

At phase exit, after the closeout commit
(`phaseN: close <CLASSIFICATION>`) on the phase branch:

```powershell
git checkout feature/ao-fixed-pipe-rewrite
git merge --no-ff phase/N-name
```

Merge commit message **must** include `Phase N classification:`,
`Verified:` (build, install, IOCTL, runtime, exit.md acceptance —
each line specific enough to re-run), `Known blockers:`, `Non-claims:`.
Squash and fast-forward are forbidden. See `docs/GIT_POLICY.md` § 5
for the full template.

### V1 ship merge

Only at Phase 7 exit, user-approved. `feature/ao-fixed-pipe-rewrite`
→ `main` with the same `Verified` / `Known blockers` / `Non-claims`
block scoped to the M6 ship gate.

### Forbidden

- Direct commits to `main` during Phase 1-6.
- Direct commits to `feature/ao-fixed-pipe-rewrite` for phase
  implementation work.
- Squash or fast-forward merge of phase branches.
- Force-push to `main` or `feature/ao-fixed-pipe-rewrite`.
- Skipping git hooks (`--no-verify`) or signing (`--no-gpg-sign`)
  unless user explicitly authorizes a specific commit.
- Committing build artifacts, `.env`, secrets, local WDK signing
  bypass files.

### Other rules

- One-test-one-commit for runtime validation. A failed test may be
  committed (failure recorded); commit message and step file must
  say `FAIL` clearly.
- Runtime artifacts under `tests/` remain untracked unless explicitly
  promoted.

## Session Continuity

Continue in the same session while the current phase/step context is clear.

Use a new session when:

- Conversation context becomes long enough that phase goals, blockers, or
  file ownership may be confused.
- Moving from phase planning/review into substantial implementation.
- Moving from one phase step to another after a completed review.
- PortCls/WaveRT API investigation becomes deep enough that focused fresh
  context would reduce risk.

When starting a new session, include a short handoff summary:

- Current branch.
- Current phase and step (`phases/<N>-name/step<N>.md`).
- Last verified commit.
- Required source-of-truth documents.
- Active blockers.
- Explicit forbidden work for the next step (cite from this file).
