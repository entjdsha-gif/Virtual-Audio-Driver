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

Never:

- Re-introduce the old packed 24-bit cable ring storage.
- Re-introduce the 4-stage `ConvertToInternal -> SrcConvert ->
  ConvertFromInternal -> LoopbackWrite` cable pipeline.
- Re-introduce 8-tap sinc SRC with 2048-coefficient table for cable
  streams.
- Re-introduce `MicSink` dual-write.
- Re-enable FormatMatch enforcement requiring Speaker == Mic == Internal.
- Add a second cable transport owner outside `AoCableAdvanceByQpc`.
- Let `GetPosition`/`GetPositions` advance audio without going through the
  canonical helper.
- Let the shared timer advance audio independently of the canonical helper.
- Silently overwrite the ring on overflow (must hard-reject + counter).
- Add hidden mixing, volume, mute, APO, DSP, AGC, EQ, limiter, or noise
  suppression.
- Hide underrun, overflow, zero-fill, drop, or DMA-overrun-guard hits as
  success.
- Pump audio from query callbacks without going through the canonical
  helper.
- Store `ms` as runtime state in cable transport math (frames are
  authoritative).
- Return stale ring data into a fresh capture session after STOP/RUN.
- Change architecture only to fix a build error.

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

AO Cable V1 uses **single-branch + commit-prefix** (per `docs/ADR.md`
ADR-012). The active branch is `feature/ao-fixed-pipe-rewrite`. Detail in
`docs/GIT_POLICY.md`.

Summary:

- Do not commit directly to `main`. Shipping merges are a separate event.
- Phase identity comes from `phases/<N>-name/` directory + commit prefix
  (`phase1/step0`, `phase1/exit`, etc.).
- Workflow per step: implement → review → fix BLOCKERs → re-review →
  commit → mark `completed`.
- Do not commit before review passes. Do not mark step `completed` before
  commit.
- Cross-verify reviewer findings against WDK headers, design documents, and
  existing-correct AO code before fixing. If a finding is incorrect, report
  the disagreement with evidence — do not blindly apply.
- One-test-one-commit for runtime / helper validation. A failed test may be
  committed (failure recorded), but commit message and step file must say
  `FAIL` clearly.
- Runtime artifacts under `tests/` remain untracked unless explicitly
  promoted.
- Never commit build artifacts, `.env`, secrets, or local WDK signing
  bypass files.

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
