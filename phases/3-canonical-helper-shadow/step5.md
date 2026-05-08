# Phase 3 Step 5: Timer-DPC-owned 63/64 phase correction

## Read First

- `docs/ADR.md` ADR-007 Decision 2 (corrected in commit 7147c2c).
- `docs/AO_CABLE_V1_DESIGN.md` section 4.1 (engine struct fields)
  and section 4.3 (timer DPC body 63/64 call site).
- `docs/AO_CABLE_V1_ARCHITECTURE.md` section 4 (helper pseudocode --
  helper does NOT own 63/64).
- `phases/3-canonical-helper-shadow/step1.md` (helper body shadow
  scope -- 63/64 explicitly out of helper scope).
- `phases/3-canonical-helper-shadow/exit.md` (Phase 3 exit gate --
  63/64 implementation is a Phase 4 flip precondition; final
  shadow-divergence evidence is collected after this step).
- Current `Source/Utilities/transport_engine.cpp`
  `AoTransportTimerCallback` body and `transport_engine.h`
  `AO_TRANSPORT_ENGINE` struct.
- `docs/REVIEW_POLICY.md` section 7 (schema coupling). Relevant
  only as a constraint that this step explicitly avoids -- see
  Resolved Design D6.

## Goal

Implement the ADR-007 63/64 per-tick phase correction in the engine
timer DPC. State (`BaselineQpc`, `TickCounter`, `LastTickQpc`,
`NextTickQpc`) is engine-global on `AO_TRANSPORT_ENGINE`. The helper
(`AoCableAdvanceByQpc`) is not modified. The query path remains
timer-cadence-neutral.

This step is a Phase 3 exit-gate item and a Phase 4 audible-flip
precondition (`phases/3-canonical-helper-shadow/exit.md`).

## Resolved Design

The following are not options. They are the contract Step 5
implements.

### D1. Deadline binding

Use engine-corrected `g_engine.NextTickQpc` as the timer-DPC phase
reference while preserving per-stream `rt->NextEventQpc`
virtual-tick replay.

- `BaselineQpc`, `TickCounter`, `LastTickQpc`, and `NextTickQpc` are
  engine-global state on `AO_TRANSPORT_ENGINE`. Only
  `AoTransportTimerCallback` mutates them.
- `TickCounter` represents the **next corrected tick index** within
  the current 100-tick baseline window. On each timer firing:
  - if `TickCounter == 0`, arm `BaselineQpc = nowQpcRaw`;
  - compute
    `NextTickQpc = BaselineQpc + ((TickCounter + 1) * PeriodQpc * 63 / 64)`;
  - record `LastTickQpc = nowQpcRaw`;
  - advance `TickCounter = (TickCounter + 1) % 100`;
  - when the advance wraps to 0, the next firing re-baselines.

  Observed `TickCounter` values are 0..99. `NextTickQpc` always
  points at the next corrected tick. This is the binding contract;
  alternative index conventions are not allowed unless explicitly
  documented and re-spec'd.

- The per-stream virtual-tick replay model in
  `transport_engine.cpp:700-848` is preserved. Its overdue
  computation reference (currently raw `now.QuadPart` at ~line 769)
  is rebound to use `engine->NextTickQpc` as the corrected base,
  so per-stream tick budgeting tracks the corrected engine cadence
  rather than raw scheduler jitter.
- `g_engine.NextTickQpc` exits its current declared-but-unused
  state. After this step it is a live snapshot of the corrected
  engine deadline.

Per-stream `rt->NextEventQpc` semantics are not redesigned. Per-
stream scheduler rewrite is out of scope (would not fit in Phase 3).

### D2. ExSetTimer arming rule

The 63/64 formula naturally produces an absolute QPC. Before
editing the deadline expression, the implementer must verify the
current `ExSetTimer` arming mode in `transport_engine.cpp`
(relative interval vs absolute target, units, sign convention) and
re-express the corrected deadline in whatever form `ExSetTimer`
already consumes today. This is an implementation rule, not a
design decision: the existing arming code is the source of truth.
The 63/64 ratio is fixed and does not change.

### D3. Counter reset semantics

At the firing where `TickCounter` advances to 0 via the
`(TickCounter + 1) % 100` modulo, the next firing observes
`TickCounter == 0` and re-baselines (`BaselineQpc = nowQpcRaw`).
Matches ADR-007 Decision 2.

### D4. First-tick semantics

On the first timer firing of a session, `BaselineQpc` and
`TickCounter` are zero from `RtlZeroMemory` in
`AoTransportEngineInit`. Treat `TickCounter == 0` uniformly as the
baseline-arm condition: `BaselineQpc = nowQpcRaw`, then advance.
First firing is functionally identical to the 100-tick reset path;
no special-case branch.

### D5. LastTickQpc role

Diagnostic only. Records the QPC of the previous firing. Not used
as input to the correction formula.

### D6. Diagnostics scope

Step 5 uses **DbgPrint evidence** for acceptance, not formal
monitor diagnostic fields. Reasons:

- Adding `<Cable>_<R/C>_TickCounter` (or similar) would trigger
  REVIEW_POLICY section 7 schema coupling: atomic update of
  `Source/Main/ioctl.h` + `Source/Main/adapter.cpp` +
  `test_stream_monitor.py`. That work is orthogonal to 63/64
  scheduling correctness and would expand this step's blast radius.
- DbgPrint evidence captured under `tests/phase3-runtime/` (an
  untracked runtime artifact unless explicitly promoted) with
  timestamps is sufficient to demonstrate counter cycling and
  re-baseline events for Phase 3 exit and Phase 4 precondition.
  If a tracked summary of evidence is wanted, it goes in
  `phases/3-canonical-helper-shadow/step5-evidence.md` (mirroring
  the `phases/2-single-pass-src/step3-evidence.md` pattern), not
  the raw DbgPrint capture.
- A formal diagnostic counter, if desired later, is a follow-up
  step (a separate Phase 3 step after Step 5 lands, or Phase 7
  polish). Step 5 does not block on it.

### Forbidden in Step 5

- `AoCableAdvanceByQpc` (helper) and `GetPosition` /
  `GetPositions` (query) must not advance `TickCounter`,
  `BaselineQpc`, `LastTickQpc`, or `NextTickQpc`. Only
  `AoTransportTimerCallback` mutates them.
- The 63/64 ratio is not adjustable.
- If the implementation cannot wire `NextTickQpc` per D1, the step
  is a BLOCKER, not a partial pass. Do not pretend `NextTickQpc`
  is the active deadline source while leaving the wiring half-done.

## Conflict with current implementation (context)

Records the asymmetry that Step 5 resolves. Resolution is in
Resolved Design above.

- `AO_TRANSPORT_ENGINE.NextTickQpc` is **declared but unused** in
  current code. Current scheduling is per-stream
  `rt->NextEventQpc` virtual-tick replay
  (`transport_engine.cpp:700-848`). Each timer firing computes
  per-stream overdue counts and replays virtual ticks in
  render-then-capture order with logical times
  `BaseTickQpc + tickIdx * PeriodQpc`.
- The 63/64 state model from DESIGN section 4.1 is not bound to
  any deadline source today.
- `AoTransportTimerCallback` issues `apply_drift_correction(&g_engine,
  nowQpcRaw.QuadPart)` per DESIGN section 4.3 (commit 7147c2c) but
  the helper body is currently absent.

`BaselineQpc`, `TickCounter`, `LastTickQpc` are already specified in
DESIGN section 4.1; this step adds them to **current code**, not
to the design. `NextTickQpc` similarly already exists in code
(declared) and moves from declared-but-unused to actively
populated.

## Planned Files

Edit only:

- `Source/Utilities/transport_engine.h` -- add `BaselineQpc`
  (`LONGLONG`), `TickCounter` (`ULONG`), `LastTickQpc` (`LONGLONG`)
  to `AO_TRANSPORT_ENGINE`. (`NextTickQpc` is already present.)
  Update header comment block to describe the engine 63/64 state
  set and the timer-DPC-only mutation rule.
- `Source/Utilities/transport_engine.cpp` -- add static helper
  `apply_drift_correction(PAO_TRANSPORT_ENGINE engine, LONGLONG
  nowQpcRaw)`. Wire into `AoTransportTimerCallback` per Resolved
  Design D1. Rebind the per-stream overdue computation reference
  to `engine->NextTickQpc`.

Do not touch:

- `Source/Main/minwavertstream.*` -- query path must remain
  timer-cadence-neutral.
- `Source/Utilities/loopback.*`.
- `Source/Main/ioctl.h`, `Source/Main/adapter.cpp`,
  `test_stream_monitor.py` -- no monitor schema change in this
  step (D6).
- `AoCableAdvanceByQpc` body (locked by step1.md).
- `AO_STREAM_RT` -- engine fields only.

## Required Edits

1. Add `BaselineQpc`, `TickCounter`, `LastTickQpc` to
   `AO_TRANSPORT_ENGINE`. `AO_STREAM_RT` field freeze (Step 0)
   unaffected: this step adds engine fields, not stream fields.
2. Implement
   `static VOID apply_drift_correction(PAO_TRANSPORT_ENGINE engine,
   LONGLONG nowQpcRaw)` per Resolved Design D1:
   - if `engine->TickCounter == 0`: arm baseline
     (`BaselineQpc = nowQpcRaw`);
   - compute `NextTickQpc = BaselineQpc +
     ((TickCounter + 1) * PeriodQpc * 63 / 64)` (re-expressed in
     the unit `ExSetTimer` consumes per D2);
   - set `LastTickQpc = nowQpcRaw`;
   - advance `TickCounter = (TickCounter + 1) % 100`;
   - store the corrected deadline in `engine->NextTickQpc`.
3. Wire the call into `AoTransportTimerCallback` at the position
   already established by DESIGN section 4.3 (before snapshot).
   `g_engine.NextTickQpc` exits declared-but-unused state and is
   actively populated each firing.
4. Rebind the per-stream overdue computation reference. Currently
   `transport_engine.cpp` ~line 769 reads
   `behindQpc = now.QuadPart - rt->NextEventQpc`. After this step
   the reference becomes the corrected engine deadline (i.e.
   `engine->NextTickQpc`) so per-stream cadence tracks the
   corrected engine cadence rather than raw scheduler jitter.
   Document the relationship between `NextTickQpc` and
   `NextEventQpc` in a comment block at the call site.

## Rules

- Tell the user before editing.
- Helper must run at `DISPATCH_LEVEL` safe (no allocations, no
  paged memory, no waits).
- The 63/64 magic ratio is fixed by ADR-007.
- `AO_STREAM_RT` field freeze (Step 0) is not affected.
- Verify `ExSetTimer` arming convention before editing the
  deadline expression (D2). Do not change the arming convention.

## Acceptance Criteria

- [ ] Build clean.
- [ ] `AO_TRANSPORT_ENGINE` has `BaselineQpc` / `TickCounter` /
      `LastTickQpc`. `NextTickQpc` is no longer
      declared-but-unused.
- [ ] `AoTransportTimerCallback` advances `TickCounter` per D1
      (`(TickCounter + 1) % 100`), records `LastTickQpc`,
      re-baselines on the firing where `TickCounter == 0`, and
      stores the corrected deadline in `NextTickQpc`.
- [ ] Per-stream overdue computation uses `engine->NextTickQpc`
      as the cadence reference. Steady-state behavior remains
      regular (no spurious overdue spikes from the binding
      change).
- [ ] **DbgPrint evidence** captured during a live call under
      `tests/phase3-runtime/` (an untracked runtime artifact
      unless explicitly promoted), containing timestamped samples
      of `TickCounter`, `BaselineQpc`, `LastTickQpc`,
      `NextTickQpc`. Evidence demonstrates at least one full
      `TickCounter` 0..99 cycle and at least one observed
      re-baseline event under steady-state operation. A tracked
      summary, if needed, lives in
      `phases/3-canonical-helper-shadow/step5-evidence.md`.
- [ ] Final shadow-divergence evidence
      (`<Cable>_<R/C>_ShadowDivergenceCount` <= 5 increments per
      minute during steady-state speech) is collected on a live
      call **after this step is committed**, satisfying the
      `phases/3-canonical-helper-shadow/exit.md` gate added by
      commit 7147c2c.
- [ ] No audible regression vs Phase 2 exit baseline. Phase 3
      shadow invariant remains intact.
- [ ] No BSOD / hang / deadlock under stream open/close stress.
- [ ] Query path verified to **not** advance any 63/64 state.
      Code-review trace confirms `apply_drift_correction` is
      called only from `AoTransportTimerCallback` (single call
      site).

## What This Step Does NOT Do

- Does not flip audible ownership for either render or capture.
- Does not advance any 63/64 state (`TickCounter`, `BaselineQpc`,
  `LastTickQpc`, `NextTickQpc`) from `AoCableAdvanceByQpc`
  (helper) or from `GetPosition` / `GetPositions` (query).
- Does not modify the 63/64 ratio.
- Does not eliminate or redesign per-stream `rt->NextEventQpc`
  virtual-tick replay. Per-stream scheduler rewrite is out of
  scope.
- Does not add formal monitor diagnostic counters per D6. Adding
  monitor fields would require atomic update of
  `Source/Main/ioctl.h` + `Source/Main/adapter.cpp` +
  `test_stream_monitor.py` per REVIEW_POLICY section 7, scoped
  to a follow-up step if needed.
- Does not change the `ExSetTimer` arming convention.
- Does not implement event-driven packet notification,
  multi-channel widening, or any Phase 7 quality polish.
- Does not change the helper body, long-window rebase, 8-frame
  gate, or overrun guard.

## Forbidden carry-over into Phase 4

- Phase 4 must not flip audible ownership before this step is
  committed and verified (already enforced by
  `phases/3-canonical-helper-shadow/exit.md`).

## Completion

    python scripts/execute.py mark 3-canonical-helper-shadow 5 completed --message "Timer-DPC-owned 63/64 phase correction."
