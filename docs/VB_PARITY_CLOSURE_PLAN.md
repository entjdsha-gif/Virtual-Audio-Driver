# VB Parity Closure Plan

**Last updated:** 2026-04-16  
**Purpose:** define exactly how to close the remaining VB-parity gaps before freezing the final Option Y implementation spec

Related documents:

- `docs/PHASE6_PLAN.md`
- `docs/PHASE6_OPTION_Y_CABLE_REWRITE.md`
- `docs/VB_PARITY_DEBUG_RESULTS.md`
- `results/phase6_vb_verification.md`
- `results/vbcable_runtime_claude.md`
- `results/vbcable_pipeline_analysis.md`
- `results/ghidra_decompile/vbcable_all_functions.c`

## Current stance

Phase 6 Option Y architecture is already decided strongly enough:

- hybrid call sources
- one canonical owner
- no timer-only model
- no query-only model

What is **not** frozen yet is the final VB-parity behavior spec.

The remaining gaps are:

1. entrypoint ownership map
2. reset semantics
3. byte-exact data-path details
4. packet-mode/event-driven parity details beyond the shared-mode phone path

This document defines how each one gets closed.

## Evidence classes

To avoid overclaiming, every closure result should be tagged with one of these:

- **Confirmed**
  - supported by both static RE and runtime evidence
- **Provisional**
  - strongly supported by static RE, but live precedence/order not fully observed
- **Open**
  - conflicting or insufficient evidence remains

The goal is not to pretend everything is `Confirmed`.
The goal is to reduce the spec to the smallest possible set of `Provisional` items and remove avoidable `Open` items.

## 1. Packet notification exact rule

### Current state

- static RE strongly suggests notification state tied to:
  - `+0x164`
  - `+0x165`
  - `+0x7C`
  - `+0x8188`
  - `KeSetEvent`
- live probing did **not** close exact precedence
- `+0x68ac` did not move in the tested payload scenario, but static RE still implicates it
- static reconciliation now strongly suggests this path is **not** required for the shared-mode phone path

### How we close it

We do **not** use more broad WinDbg probing as the primary method.

Instead:

1. reconcile static sources:
   - `results/phase6_vb_verification.md`
   - `results/vbcable_runtime_claude.md`
   - `results/vbcable_pipeline_analysis.md`
   - `results/ghidra_decompile/vbcable_all_functions.c`
2. extract every notification-related condition and call site
3. split them into:
   - query-side notification path
   - timer/callback-list signalling path
   - capture/helper-side notification path
4. write one frozen `packet notification rule` section with:
   - trigger condition
   - single-fire / re-arm semantics
   - callback/event side effects
   - fields affected

### Closure target

The packet section is considered closed enough when we can write:

- which fields are authoritative
- when `notifyFired` flips
- how re-arming happens
- what the callback path must emit
- what AO must preserve for WaveRT clients

### Expected outcome

- **Confirmed enough for shared-mode phone-path Y**
- still only **Provisional** for full packet-mode/event-driven parity

### Implementation rule if ambiguity remains

Be conservative:

- do not make shared-mode Y depend on packet notifications
- preserve packet-ready behavior only for event-driven / packet-mode streams that actually arm it
- preserve single-fire edge semantics where packet mode remains supported
- let both query and timer call sources funnel into the same canonical helper
- do not hardcode one source as the only legal notifier unless the static evidence is unanimous

## 2. Entrypoint ownership map

### Current state

We already know:

- `+0x6320` is active
- `+0x22b0` is hot payload primitive
- `+0x6adc` and `+0x5634` are both active
- A is query-heavier
- B is timer-dominant hybrid

What is still not frozen is the exact ownership statement:

- whether we should say `render=query owner, capture=timer owner`
- or keep the safer `hybrid source model` statement

### How we close it

This is a reconciliation task, not primarily a live-debug task.

1. collect all entrypoint claims from:
   - `docs/VB_PARITY_DEBUG_RESULTS.md`
   - `results/phase6_vb_verification.md`
   - `results/vbcable_runtime_claude.md`
   - `docs/VB_CABLE_DYNAMIC_ANALYSIS.md`
2. map them into one table:
   - `+0x5420`
   - `+0x5cc0`
   - `+0x6320`
   - `+0x6778`
   - `+0x68ac`
   - `+0x6adc`
   - `+0x5634`
   - `+0x22b0`
3. for each entrypoint, record:
   - evidence source
   - role claim
   - confidence
4. freeze only the strongest common denominator

### Closure target

We must be able to say one of these, explicitly:

- **strong split:** render=query owner, capture=timer owner
- **safe hybrid split:** render=query-dominant, capture=timer-dominant hybrid

### Expected outcome

Based on current evidence, the likely closure is:

- **Confirmed**: one canonical owner, multiple active sources
- **Provisional**: render=query-dominant, capture=timer-dominant hybrid

Not:

- fully `Confirmed` strict ownership split

### Implementation rule if ambiguity remains

Do not encode a stronger ownership split than the evidence justifies.

That means:

- one helper
- multiple reasons / entrypoint kinds
- reason enum may distinguish `query`, `timer_render`, `timer_capture`
- but state ownership remains singular

## 3. Reset semantics

### Current state

Runtime evidence already shows:

- register entry zero-inits cursor/counter-looking fields
- unregister entry still carries accumulated state

What remains open:

- PAUSE semantics
- STOP semantics
- final teardown / free semantics

### How we close it

This one is a mixed static/runtime closure.

1. use the existing lifecycle runtime traces as the baseline
2. reconcile them with static decompile around:
   - `+0x65b8`
   - `+0x669c`
   - any related finalize/helper path referenced in decompile notes
3. write a transition table:
   - RUN / REGISTER
   - PAUSE
   - STOP
   - UNREGISTER
   - FINAL FREE
4. for each state, mark:
   - zeroed fields
   - preserved fields
   - re-armed fields

### Closure target

We must be able to specify, for the AO rewrite:

- what gets zeroed on start
- what remains monotonic across pause/resume
- what is preserved until unregister
- what only dies on final free

### Expected outcome

- RUN / UNREGISTER likely `Confirmed`
- PAUSE / STOP / FINAL FREE may remain partly `Provisional`

### Implementation rule if ambiguity remains

Choose the safer lifetime:

- zero less aggressively
- preserve monotonic/accounting state until explicit stop/unregister
- do not eagerly clear packet/cursor state earlier than the evidence requires

## 4. Byte-exact data-path details

### Current state

This is the most static-RE-heavy area and is already close to closure.

Strong candidates already exist for:

- INT32 ring shape
- 4-way bpp dispatch
- scratch linearization
- fade envelope table
- no-real-SRC conclusion
- field layout candidates for `AO_STREAM_RT`

### How we close it

This is a spec-writing closure, not a new-debug closure.

1. extract the agreed data-path claims from:
   - `results/phase6_vb_verification.md`
   - `results/vbcable_pipeline_analysis.md`
   - `results/ghidra_decompile/vbcable_all_functions.c`
2. split into:
   - definitely mandatory for Y1
   - likely mandatory but still provisional
   - optional / Y5 polish
3. map each item to AO implementation surface:
   - `AO_STREAM_RT`
   - `FRAME_PIPE`
   - `AoCableAdvanceByQpc`
   - conversion/write helpers

### Closure target

We need a frozen table that says:

- ring container format
- dispatch formats
- whether real SRC exists
- whether envelope application is mandatory
- which AO structure owns each state field

### Expected outcome

- most of this should be closable as **Provisional** or better from existing static RE
- no more WinDbg is required for this section

### Implementation rule if ambiguity remains

Prefer the more conservative parity-preserving choice:

- keep scratch staging
- keep envelope support
- keep 4-way dispatch
- do not add SRC if the best static evidence says VB does not really use it

## What we will actually do next

The closure work is now:

1. write a **frozen entrypoint ownership map** with confidence labels
2. write a **state transition/reset table**
3. write a **byte-exact data-path parity table** mapped to AO structures/helpers
4. optionally append packet-mode/event-driven parity notes, if we decide to preserve that path beyond semantic equivalence

That is the point where Option Y can move from:

- "architecturally correct"

to:

- "as close to VB as current evidence can justify"

## Practical closure order

The most realistic next-pass order is:

1. **real unregister / stop path**
   - method:
     - static xref walk from `FUN_14000669c`
     - compare with `FUN_1400065b8`
   - goal:
     - distinguish register, unregister, stop/finalize, and pending-finalize paths
   - expected cost:
     - low

2. **scratch buffer ownership and size**
   - method:
     - inspect `FUN_140007680`
     - inspect its immediate callers around the ring/write helpers
   - note:
     - `FUN_140007680` by itself only proves "overlap-safe copy / memmove-like primitive"
     - ownership and effective scratch size come from the callers, not from `FUN_140007680` alone

3. **ring allocation path and size**
   - method:
     - static walk around `FUN_1400065b8`
     - inspect allocator/init helpers and instance/stream field writes
   - goal:
     - freeze who allocates the ring
     - freeze which field carries the authoritative ring size

4. **`+0x7C` boundary meaning**
   - method:
     - collect every xref using `param_1 + 0x7c`
     - reconcile the `FUN_140006320` and `FUN_1400068ac` branches
   - goal:
     - freeze whether `+0x7C` is packet boundary, notify threshold, or both

5. **packet notify behavior in shared-mode**
   - method:
     - only if the static pass still leaves ambiguity
     - use one low-frequency breakpoint or one narrow live check
   - note:
     - this is now optional, not the primary closure tool
     - if static reconciliation is clean enough, skip this step
     - current evidence says shared-mode phone-path closure is already good enough

## Current assessment of that plan

This means:

- the proposed "mostly Ghidra/static" direction is correct
- items 1, 3, and 4 are strong static-analysis candidates
- item 2 needs caller analysis, not just `uf 7680`
- item 5 should be treated as a last-mile confirmation step, not as a required main path

## Bottom line

We do **not** need more broad live WinDbg probing to close these four.

We need to turn the existing evidence into four frozen parity sections:

- ownership map
- reset table
- byte-exact data-path table
- optional packet-mode/event-driven appendix

Once those four are written, Option Y can be specified at the strongest level the current evidence supports.
