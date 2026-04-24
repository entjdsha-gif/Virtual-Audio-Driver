# AO Virtual Cable - Current State

**Last updated:** 2026-04-16
**Primary active design branch:** `feature/ao-phase6-core`
**Primary active worktree:** `D:/mywork/ao-phase6`
**Historical branch:** `feature/ao-fixed-pipe-rewrite`
**Effective last-known-good baseline:** `439bbcd`

## Canonical docs

- **Design source of truth:** `docs/PHASE6_PLAN.md`
- **Detailed Option Y rewrite spec:** `docs/PHASE6_OPTION_Y_CABLE_REWRITE.md`
- **VB runtime parity findings:** `docs/VB_PARITY_DEBUG_RESULTS.md`
- **Current status / roadmap:** this file
- **Historical commit-by-commit record:** `docs/PIPELINE_V2_CHANGELOG.md`
- **Older architecture history:** `docs/AO_V2_ARCHITECTURE_PLAN.md`

If two documents disagree:

1. runtime evidence
2. `docs/PHASE6_PLAN.md` for architecture/design intent
3. `docs/CURRENT_STATE.md` for current execution state and roadmap
4. `docs/PIPELINE_V2_CHANGELOG.md` for historical record

## Where we are

### Known-good baseline

`439bbcd` remains the effective last-known-good baseline for the phone path.

That does not mean it is perfect. It means:

- it is the current safe behavioral baseline
- later Step 3/4 timer-owned transport experiments regressed from it
- all new design work must compare back to it

### Phase 5 status

`2c733f1` is the archived failed Phase 5 attempt.

What failed:

- moving transport ownership to a query/timer-owned path
- treating transport cadence as something external to the stream update chain

What remains useful:

- historical evidence
- some scaffolding ideas
- lessons learned about what not to do

But Phase 5 is not a design baseline anymore.

### Phase 6 status

Phase 6 Step 1 skeleton succeeded.

What is already proven:

- engine lifecycle scaffolding can exist safely
- stream register/unregister works
- shared timer skeleton can be loaded without BSOD

What failed:

- Phase 6 Step 3/4 timer-owned transport

Current understanding:

- the Step 3/4 regression was not just one bad constant
- it was not solved by "better publish cursor" alone
- the core mistake was **decoupling transport from the update chain**

In other words:

- one path advanced accounting/cursor state
- another path later moved audio on its own cadence

That separation is now considered the structural regression source.

## Current decision

The project is now on:

### `Z`

Revert the failed Step 3/4 data movement and recover Step 1 / Phase 4 quality while keeping the reusable engine skeleton.

### `Y`

Rebuild Phase 6 as **update-chain-coupled transport**, not timer-owned transport.

This means:

- main transport owner is the canonical cable advance path
- query path and shared timer both remain active call sources
- frame delta, gate, cursor/accounting, and transport move together
- shared timer must not become a second owner

It explicitly does **not** mean:

- "just lower the timer to 1 ms"
- "make the shared timer own everything"
- "keep timer-owned transport and tune constants"

## Immediate next steps

1. Finish `Z`
   - restore legacy cable speaker `ReadBytes` from `UpdatePosition`
   - restore legacy cable mic `WriteBytes` from `UpdatePosition`
   - stop timer callback from dispatching render/capture movement
   - keep Step 1 skeleton

2. Validate `Z`
   - build/install
   - local loopback
   - live call
   - confirm Step 3/4 regression disappears

3. Start `Y1`
   - add update-chain shadow hook only
   - no audio movement change yet
   - follow `docs/PHASE6_OPTION_Y_CABLE_REWRITE.md` for exact cable-only removal scope and phase gates

4. Start `Y2`
   - move render transport into update-coupled helper
   - validate before touching capture

5. Start `Y3`
   - move capture transport into update-coupled helper
   - reuse good startup/headroom/recovery ideas only inside the new coupled design

## Current quality gate

We are **not** in cleanup/tuning-only territory yet.

The active quality gate is:

- recover from the failed timer-owned Step 3/4 experiment
- prove that update-chain-coupled migration can match or beat the `439bbcd` baseline

Until that happens:

- Step 5/6 style cleanup is not the main work
- timer-owned transport is not the design baseline
- "VB-equivalent" means structural coupling with hybrid call sources, not simply a faster timer

## Runtime evidence snapshot

The current VB runtime evidence is recorded in `docs/VB_PARITY_DEBUG_RESULTS.md`.

What is already strong enough to act on:

- A side looks query-heavier
- B side is timer-dominant hybrid
- `+0x22b0` is a real hot payload primitive
- hot-path WinDbg breakpoints are intrusive enough to distort quality judgment

What still remains open before claiming full VB identity:

- packet notification contract
- capture branch parity
- full lifecycle reset semantics beyond register/unregister entry

## Stage B and later

After Phase 6 lands cleanly:

1. broader channel acceptance
2. broader format acceptance
3. telephony features such as AMD
4. ControlPanel runtime configuration polish

These are intentionally downstream of Phase 6.

## One-line summary

Current project direction is:

**recover from the failed timer-owned experiment, then rebuild Phase 6 so query and timer both funnel into one canonical cable advance path instead of maintaining separate ownership models.**
