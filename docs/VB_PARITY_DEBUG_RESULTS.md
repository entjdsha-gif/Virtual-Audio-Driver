# VB Parity Debug Results

**Last updated:** 2026-04-16  
**Scope:** VB-Cable runtime parity investigation for Phase 6 Option Y  
**Status:** enough evidence to freeze the Phase 6 Y1/Y2/Y3 architecture for the shared-mode phone path; not enough yet to claim full VB identity across every mode

## Purpose

This document records the runtime evidence gathered after the failed Step 3/4 timer-owned transport experiment.

It exists to:

- preserve the concrete WinDbg/runtime results that changed the Option Y design
- separate confirmed behavior from still-open parity questions
- prevent the design docs from drifting back toward `timer-only`, `query-only`, or `shared timer = auxiliary only`
- hand off the remaining parity closure work to `docs/VB_PARITY_NEXT_DEBUG_CHECKLIST.md`

## Measurement caveat

Hot-path WinDbg breakpoints materially distort live-call audio quality.

Confirmed during testing:

- with no hot-path breakpoints, live-call/TTS audio sounded normal
- with even one breakpoint on the B-side hot payload primitive, voice became chopped/stuttered
- a broad or system-hot breakpoint such as `nt!KeSetEvent` was too intrusive to use as a practical live-call probe

Therefore:

- hot-path breakpoint sessions are valid for **path classification**
- hot-path breakpoint sessions are **not** valid for judging perceived quality

## Confirmed runtime findings

### 1. Early live-call control-path counts

Observed counts:

- Cable A
  - `+0x5420 = 821`
  - `+0x5cc0 = 0`
  - `+0x6320 = 821`
  - `+0x22b0 = 0`
- Cable B
  - `+0x5420 = 208`
  - `+0x5cc0 = 289`
  - `+0x6320 = 200`
  - `+0x22b0 = 0`

Interpretation:

- A already looked query-heavy
- B already looked hybrid
- but there was no confirmed payload primitive activity yet, so this was not enough for a payload verdict

### 2. B-side payload primitive confirmed

Using explicit payload/TTS injection:

- Cable B `+0x6320 = 10531`
- Cable B `+0x22b0 = 876`

Interpretation:

- `+0x6320` is an active core/advance path
- `+0x22b0` is a real hot payload primitive
- payload-bearing runs do reach `+0x22b0`

Operational note from the live Phone Link environment:

- for the tested target setup, a normal user-space payload reached the phone uplink when **Chrome output was explicitly routed to `CABLE-B Input`**
- this is treated as the practical "real payload present" condition for later B-side live checks

### 3. B-side source bias: timer-dominant hybrid

Additional B-side counts:

- `+0x5420 = 4785`
- `+0x5cc0 = 8770`

Interpretation:

- B is **not** query-only
- B is **not** timer-only
- B is a **timer-dominant hybrid**

### 4. Hot-path breakpoint intrusiveness confirmed

Single-breakpoint run on the B-side payload primitive:

- B `+0x22b0 = 1133`

Observed effect:

- audio became chopped/stuttered even with only that one breakpoint active

Interpretation:

- `+0x22b0` is hot enough that KD breakpoint instrumentation itself perturbs the live path
- do not use hot-path breakpoints as an audio-quality oracle

### 5. Lifecycle: register vs stop/unregister

Observed on B-side lifecycle tracing:

- at `+0x65b8` (`REGISTER`)
  - cursor/counter-looking fields near `+0xd0/+0xd8` and `+0xe0/+0xe8` started at zero
  - a baseline/QPC-anchor-looking field near `+0x180` was already seeded
- at `+0x669c` (`UNREGISTER`)
  - cursor/counter-looking fields were non-zero
  - the baseline-looking field remained stable
  - accumulated runtime state was still present at unregister entry

Static reconciliation with the decompile adds:

- `FUN_14000669c` is the real unregister helper and also participates in stop/cleanup call chains
- callers perform the aggressive field zeroing, not `FUN_14000669c` itself
- stop/close paths zero monotonic/cursor fields before or around unregister
- if the shared stream count reaches zero, `FUN_14000669c` also destroys the shared timer and clears global timer state

Interpretation:

- register appears to initialize key runtime counters/cursors to zero
- unregister entry is **not** an eager zero-everything point
- stop/close does clear monotonic/cursor state
- accumulated state survives until unregister entry, but not indefinitely across stop/close

### 6. B-side branch split under payload

Observed one-breakpoint runs:

- `+0x6adc = 636`
- `+0x5634 = 278`

Interpretation:

- both branches are genuinely active
- this is not a single-branch model
- in the tested payload/TTS scenario, `+0x6adc` had higher participation than `+0x5634`

### 7. `+0x68ac` did not move in the tested payload scenario

Observed one-breakpoint run:

- `+0x68ac = 0`

Interpretation:

- this helper did not participate in the tested payload/TTS scenario
- that result is scenario-local, not a global proof that `+0x68ac` is irrelevant
- local static RE still ties `+0x68ac` to periodic/capture-side helper logic and notification-related checks involving `+0x164`, `+0x165`, `+0x7C`, and indirect dispatch through `+0x8188`
- therefore `+0x68ac` should be treated as **not hot in the tested scenario**, not as globally dead

### 8. Packet notification is closed enough for the shared-mode phone path

Local static notes consistently point to a provisional notification rule:

- notification-armed / fired flags near `+0x164` / `+0x165`
- boundary/threshold state near `+0x7C`
- indirect notification dispatch through `+0x8188` with notification-like `edx=8`
- callback/event signalling also appears in the shared-timer callback path via `KeSetEvent`
- `FUN_140004764` behaves like the boundary-setting packet API path and writes `+0x7C`

Interpretation:

- the packet-notification path is gated and is not the normal shared-mode phone path
- without the packet/boundary setup path, `+0x164` remains disabled and the direct `+0x8188` boundary-crossing dispatch in `FUN_140006320` / `FUN_1400068ac` is skipped
- this means shared-mode clients such as the tested Phone Link path are consistent with `GetPosition` polling and do not require packet-notification parity as a Y1 blocker
- AO's current behavior, where packet APIs are unsupported when `m_ulNotificationsPerBuffer == 0`, is meaningfully aligned with that shared-mode behavior
- exact event-driven / packet-mode parity is still worth preserving, but it is no longer a blocker for the shared-mode Y design

### 9. Broad `KeSetEvent` probing is not practical on the live target

Observed:

- a caller-filtered `nt!KeSetEvent` attempt caused severe lag on the target

Interpretation:

- even a filtered `KeSetEvent` breakpoint is too invasive for the current target
- packet notification exact-rule closure should not depend on repeating that live method

## Architectural conclusion

Current runtime evidence supports this model:

- Cable A appears query-heavier
- Cable B is timer-dominant hybrid
- shared timer is an **active first-class call source**
- query path is also an **active first-class call source**
- neither source may become a second owner
- all active call sources must funnel into **one canonical cable advance path**

The safe summary is:

**multiple active call sources, one canonical owner**

This replaces all of the following older ideas:

- `shared timer is auxiliary only`
- `VB is timer-only`
- `VB is query-only`

## What this is enough to decide

This evidence is enough to choose the Phase 6 Option Y architecture:

- do **not** build a timer-only model
- do **not** build a query-only model
- do build a hybrid model where query and timer both enter the same `AoCableAdvanceByQpc(...)`

## What is still not closed

These parity questions remain open:

1. **Packet notification contract**
   - shared-mode phone-path behavior is closed enough for Y
   - exact event-driven / packet-mode parity is still partly provisional
   - callback/event-side details remain lower-priority parity work, not a Y1 blocker

2. **Capture branch parity**
   - exact role of the `+0x6adc` branch
   - exact render-vs-capture branch split under real payload

3. **Full lifecycle reset semantics**
   - STOP/close semantics are now much clearer
   - PAUSE/final free still remain less explicit than RUN/STOP/unregister

## Recommended next inspection order

To finish parity closure with minimal breakpoint-induced distortion:

1. lifecycle/transition semantics with low-frequency breakpoints
2. only lower-priority packet-mode/event-driven confirmation, if we choose to preserve byte-closer packet semantics beyond the shared-mode phone path

One breakpoint at a time is preferred for the remaining work.

## Bottom line

We do **not** yet have enough evidence to say AO Option Y will be fully identical to VB in every mode.

We **do** have enough evidence to say the shared-mode phone-path Option Y structure must be:

**hybrid call sources (timer + query), one canonical cable advance path, no second owner**
