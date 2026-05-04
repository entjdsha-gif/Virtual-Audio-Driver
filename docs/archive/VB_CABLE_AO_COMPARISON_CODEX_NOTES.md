# Codex Notes: VB-Cable vs AO Comparison

Date: 2026-04-13
Source quality: mixed static + dynamic
Rule: this file records only clearly scoped notes and keeps confirmed facts
separate from interpretation.

## Dynamic-confirmed VB state model

Live WinDbg / KDNET session confirmed `vbaudio_cablea64_win10+0x5910` uses:

- `0 = STOP`
- `1 = ACQUIRE`
- `2 = PAUSE`
- `3 = RUN`

This matters because earlier notes sometimes simplified VB into only
STOP / PAUSE / RUN. That is not complete.

## Dynamic-confirmed VB reset behavior

Direct disassembly of `+0x5910` confirms:

- `STOP` path does not call `+0x39ac`
- `PAUSE` path conditionally calls `+0x39ac`
- `RUN` path does not call `+0x39ac`

The PAUSE-side reset is conditional:

- previous state must be `> 2`
- `[stream+0x58]` must be non-null
- `[stream+0x168]` ring pointer must be non-null

Safe wording:

- `VB STOP does not reset the ring`
- `VB PAUSE conditionally resets the ring`

Unsafe wording to avoid:

- `VB always resets on pause`
- `VB resets on stop`

## Dynamic-confirmed call-stack caveat for `+0x39ac`

Live stack capture showed `vbaudio_cablea64_win10+0x39ac` can be reached from:

- `portcls!CPortPinWaveRT::DistributeDeviceState`
- `portcls!PinPropertyDeviceState`
- `ks!KsPropertyHandler`

Therefore:

- a raw `+0x39ac` hit is not enough by itself
- stack context must be checked before labeling the event as STOP/PAUSE reset

## Dynamic-confirmed live ring churn

Observed ring pointer examples:

- `ffff968f`b0630000`
- `ffff968f`ca9e0000`

Observed live state sequences:

- same ring: `ACQUIRE -> PAUSE`
- another live ring: `ACQUIRE -> PAUSE -> RUN -> PAUSE -> ACQUIRE -> STOP`

Interpretation:

- VB live call flow does churn
- the differentiator vs AO is not "VB never churns"
- the differentiator is likely how VB handles churn, reset timing, and
  transport continuity

## Correction for `+0x10ec`

Current safest wording for `vbaudio_cablea64_win10+0x10ec`:

- `ReadPos trim / backlog reduction helper`

Avoid overclaiming:

- do not call it the generic normal write path
- do not treat it as full proof that VB always uses overwrite-oldest in the
  hot path

Reason:

- the body clearly advances `ReadPos`
- but the evidence still does not prove that every normal write goes through
  this exact behavior

## AO comparison notes that should remain visible

When comparing current AO code to VB, keep these AO-side facts in view:

- current AO `FramePipeInit()` uses immediate-read semantics
  - `StartThresholdFrames = 0`
  - `StartPhaseComplete = TRUE`
- current AO reset behavior is still not equivalent to VB's PAUSE-only
  conditional reset
- AO comparisons should distinguish:
  - property / device-state path
  - stream `SetState` path

## Shared timer model: stronger confirmation

Re-review of `vbaudio_cablea64_win10+0x65b8` confirms that VB uses a shared
timer model:

- if global active-stream count is zero, it allocates the timer
- it stores active stream pointers in the global table at `+0x12f90`
- it increments the global active-stream count at `+0x12f84`
- it arms the timer through `ExSetTimer`

This strengthens an earlier comparison point:

- VB is not simply "per-stream timer like AO"
- VB registers streams into a shared timer set and tracks them globally

## Live hot-path result: Cable A is the active steady-state path

Auto-log breakpoints on:

- `A +0x22b0` (write)
- `A +0x11d4` (read)
- `B +0x22b0` (write)
- `B +0x11d4` (read)

produced:

- repeated `A Write` on:
  - `ffff968f`b0320000`
  - `ffff968f`b2aa0000`
  - `ffff968f`b27e0000`
- repeated `A Read` on:
  - `ffff968f`b0320000`
- no `B Write`
- no `B Read`

Safe interpretation:

- the traced live call was actually using Cable A as the active steady-state
  transport
- ring `b0320000` is part of the main paired write/read path
- `b2aa0000` and `b27e0000` look like additional A-side write paths or helper
  stream objects, not the main capture readback ring

## Shared timer unregister path: `+0x669c`

Re-review of `vbaudio_cablea64_win10+0x669c` confirms it is the opposite side
of `+0x65b8`:

- walks the global stream table at `+0x12f90`
- clears the matching stream pointer
- decrements the active-stream count at `+0x12f84`
- recomputes the high-water slot count at `+0x12f88`
- if the active-stream count reaches zero:
  - calls `ExDeleteTimer`
  - clears the shared timer handle at `+0x12fd8`
  - zeroes related global timing fields

Comparison meaning:

- VB uses explicit shared timer register/unregister helpers
- this is more structured than AO's current pipe/runtime scheduling model

## Live caller mapping for the three A-side write rings

Live caller addresses from the steady-state trace now map as follows:

- `caller=...573a`
  - inside `FUN_140005634`
  - main speaker-side DMA/scratch to ring write path
- `caller=...6884`
  - inside `FUN_140006778`
  - helper path that can call `+0x11d4` / `+0x22b0` outside the main
    `FUN_140006adc` path
- `caller=...6298`
  - inside `FUN_140005cc0` shared timer callback
  - writes one auxiliary A-side ring path
- `caller=...62e6`
  - inside `FUN_140005cc0` shared timer callback
  - writes another auxiliary A-side ring path

Comparison meaning:

- ring `b0320000` is the main paired write/read transport
- rings `b2aa0000` and `b27e0000` are auxiliary shared-timer-driven write
  paths
- VB therefore does more than a single simple "one writer / one reader" ring
  model during steady-state call handling

## Caller/function closure from `.fnent`

WinDbg `.fnent` confirmed the enclosing functions for the important caller
addresses:

- `...573a` sits inside `+0x5634 .. +0x5904`
- `...6884` sits inside `+0x6778 .. +0x68ac`
- `...6298` and `...62e6` both sit inside `+0x5cc0 .. +0x631e`

Comparison meaning:

- the main paired ring path and the auxiliary write-only paths are now anchored
  to concrete function ranges, not just loose offsets

## Helper-layer note: `+0x6778`, `+0x68ac`, `+0x11d4`

Current safest comparison wording:

- `+0x6778` is a helper dispatcher that can call either `+0x22b0` or `+0x11d4`
- `+0x68ac` is a higher-level periodic service helper that can dispatch to
  `+0x6adc` or `+0x5634`
- `+0x11d4` is a substantial read / convert path with validation, availability
  checks, and fill handling
- `+0x22b0` is the corresponding core write / convert / capacity-check
  primitive
- `+0x5634` is the main speaker-side service wrapper that stages data and then
  calls `+0x22b0`

Comparison meaning:

- VB has visible orchestration layers above the core ring primitives
- AO comparison should therefore not assume a flat one-function transport path
- the VB transport model appears to include:
  - shared timer coordination
  - orchestration helpers
  - core ring read/write primitives
  - conversion/fill logic inside the read path
  - conversion/capacity logic inside the write path

## Slow-path and side-helper note: `+0x26a0`, `+0x4f2c`

Current safest comparison wording:

- `+0x26a0` is the large slow-path write/adaptation routine reached from
  `+0x22b0` when fast-path assumptions do not hold
- it uses substantial scratch state and persistent carry/residual-style fields,
  so it should be modeled as a real adaptation path rather than a trivial
  fallback
- `+0x4f2c` is a smoothed per-channel level / peak helper, not the core ring
  transport path

Comparison meaning:

- AO should not collapse VB's write side into a single primitive
- AO should keep transport/adaptation logic separate from metering/statistics
  helpers
- if AO aims to behave like VB, the minimal mental model is:
  - fast write path
  - heavy slow/adaptation path
  - separate side metering helper

## Negative live result worth keeping

During a short live call utterance, auto-log breakpoints on:

- `+0x6adc`
- `+0x10ec`

did not fire.

Comparison meaning:

- the captured steady-state path was already sufficiently explained by the
  previously observed `+0x5634 -> +0x22b0` and `+0x6778 -> +0x11d4` style flow
- `+0x6adc` and `+0x10ec` should remain in the model, but they do not need to
  be treated as mandatory hot-path steps for every short live call window

## Position-query path note

Live breakpoint hits on `portcls!CPortPinWaveRT::GetKsAudioPosition` produced a
stable stack through:

- `portcls!PinPropertyPositionEx`
- `ks!KspPropertyHandler`
- `ks!KsPropertyHandler`
- `portcls!CPortPinWaveRT::DeviceIoControl`

Comparison meaning:

- VB position reporting is exposed through the KS property / IOCTL path
- AO should keep position-query handling conceptually separate from the core
  data-movement transport path at the interface level
- the query stack remained the same both without speech and while speaking, so
  this path should be treated as steady polling rather than speech-triggered
  transport work

## Reconfirmed main write-side live path

During a short spoken call segment, live breakpoints showed repeated:

- `+0x5634` hits with varying frame-like `edx` values in roughly the
  `2600..3100` range
- several immediate `+0x22b0` hits under each `+0x5634`
- no `+0x68ac` hits in the same capture window

Comparison meaning:

- `+0x5634 -> +0x22b0` is the safest current model for the observed main
  write-side steady-state path
- `+0x68ac` should remain modeled as a conditional/helper path rather than an
  always-on write-side top-level service

## Refinement: `+0x5634` is also reachable from position polling

An entry breakpoint on `+0x5634` later showed a repeated stack through:

- `portcls!CPortPinWaveRT::GetKsAudioPosition`
- internal VB helpers around `+0x54bb` and `+0x64f6`
- `+0x5634`

Comparison meaning:

- `+0x5634` should not be treated as an exclusively owned background service
  entry
- it is safer to model it as a shared internal helper that participates in the
  write-side path and can also be reused from position/accounting refresh work

## Stronger refinement: position polling can trigger internal pump/update work

Static disassembly of the internal helpers around the observed stack shows:

- `+0x5420` is consistent with a `GetKsAudioPosition`-side helper that locks,
  conditionally calls `+0x6320`, and then returns position values
- `+0x6320` derives an elapsed-frame delta from timing fields and can dispatch
  to `+0x6adc` or `+0x5634` before advancing internal cursor/accounting state

Comparison meaning:

- the safer model is no longer "position query is fully separate from
  transport"
- instead, position polling appears able to lazily trigger internal
  update/pump work for at least part of the main path
- the best current VB model is hybrid:
  - shared timer coordination exists
  - position polling can drive main-path progression

## 8-frame gate is now directly confirmed

Static disassembly of `+0x6320` shows:

- a frame delta is computed from elapsed timing
- the already-accounted amount is subtracted
- the remaining delta is compared against `8`
- sub-8 work skips the main processing body

Comparison meaning:

- VB really does implement a minimum work threshold before advancing the path
- if AO adopts the same position-query-driven model, it should preserve the
  same kind of minimum gate to avoid noisy over-processing under aggressive
  polling

## Safe final wording after the new evidence

Use this wording rather than the more extreme "timer model was completely
wrong" phrasing:

`VB's main observed paired path appears to be lazily advanced by position-query polling, while the shared timer subsystem still exists and appears to service auxiliary/shared activity.`

## Safe summary

Use this sentence if a short summary is needed elsewhere:

`VB live call flow does churn through ACQUIRE/PAUSE/RUN/STOP, but STOP does not reset the ring; PAUSE conditionally resets it, and reset/helper hits must be interpreted with stack context rather than by offset alone.`

## Suggested tagging convention for future notes

- `STATIC-CONFIRMED`: direct disassembly / decompile evidence
- `DYNAMIC-CONFIRMED`: live breakpoint / register / stack evidence
- `INFERENCE`: interpretation from confirmed facts
