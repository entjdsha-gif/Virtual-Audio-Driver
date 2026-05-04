# VB Parity Next Debug Checklist

**Last updated:** 2026-04-16  
**Purpose:** close the remaining VB parity gaps with the least intrusive WinDbg method  
**Companion docs:**

- `docs/VB_PARITY_DEBUG_RESULTS.md`
- `docs/PHASE6_OPTION_Y_CABLE_REWRITE.md`

## What still remains open

Only three areas still need direct runtime closure:

1. packet notification contract
2. capture branch parity
3. full lifecycle reset semantics

The architecture choice is already supported.
This checklist is only for pushing from "near-equivalent" toward "as close to VB as we can prove."

## Current status before the next run

Already closed enough:

- B-side `+0x22b0` is a real hot payload primitive
- B side is timer-dominant hybrid
- both `+0x6adc` and `+0x5634` are active, with `+0x6adc > +0x5634` in the tested payload/TTS scenario
- the tested live Phone Link environment treats **Chrome output -> `CABLE-B Input`** as a practical payload-present condition

Still open:

- packet notification contract
- deeper lifecycle/reset semantics

## Hard rule for all remaining sessions

Hot-path breakpoints are intrusive enough to ruin live audio.

So every remaining session must follow this rule:

1. **no hot-path breakpoints while opening Phone Link or establishing the call**
2. connect the call first
3. run the test script with a wait window
4. set at most **one hot breakpoint** during that wait window
5. resume
6. break once after playback/TTS finishes

Recommended script window:

```powershell
Set-Location C:\livecall
py -3 run_test_call_tts_only.py --wait 15
```

That 15-second wait gives enough time to:

- connect or answer the call manually
- set one breakpoint
- resume before TTS starts

## Common session skeleton

1. open Phone Link with **no** hot breakpoints
2. connect or answer the call manually
3. on the target:

```powershell
Set-Location C:\livecall
py -3 run_test_call_tts_only.py --wait 15
```

4. while the script is waiting, set exactly one breakpoint in WinDbg
5. `g`
6. wait for TTS/playback to finish
7. `Ctrl+Break`
8. inspect the one counter or one stack you asked for

## 1. Packet notification contract

### Goal

Close the remaining packet-notification gap without pretending that `+0x68ac = 0` made the static RE disappear.

### Status of the old candidate

`+0x68ac` was tested and stayed at `0`.

Interpret that carefully:

- `+0x68ac` was not hot in the tested payload scenario
- static RE still implicates `+0x68ac` in periodic/capture-side helper logic and notification-related checks

So do **not** spend another run on `+0x68ac` unless the scenario changes in a way that should activate capture-side helper ownership more directly.

### Next safe move

Do the low-frequency lifecycle stack pass first, because it is much less intrusive and may reveal the transition/helper path that also owns packet-related state.

### After lifecycle stack pass

If packet notification still needs a direct probe, move to a **new candidate** rather than blindly repeating the same `+0x68ac` scenario.

The first acceptable next packet probe is:

- a targeted breakpoint on a newly identified packet-related VB helper from the next RE pass
- or a carefully filtered `nt!KeSetEvent` stack sample only if a quieter candidate cannot be found

Do **not** jump straight to broad `KeSetEvent` tracing on a sluggish target unless absolutely necessary.
On the current target, that approach has already proven too intrusive for practical live use.

## 2. Capture branch parity

This is now largely classified for the tested scenario.

### 2-A. `+0x6adc`

```txt
bc *
r? @$t0 = 0
bp vbaudio_cableb64_win10+0x6adc "r? @$t0 = @$t0 + 1; gc"
bl
g
```

After playback:

```txt
? @$t0
```

#### Interpretation

- `0` means `+0x6adc` is not active in that payload scenario
- non-zero means the capture/other branch is genuinely participating

### 2-B. `+0x5634`

```txt
bc *
r? @$t0 = 0
bp vbaudio_cableb64_win10+0x5634 "r? @$t0 = @$t0 + 1; gc"
bl
g
```

After playback:

```txt
? @$t0
```

#### Interpretation

- high `+0x5634` with low/zero `+0x6adc` means the render-side branch dominates that scenario
- both non-zero means both branches are active and the split is more mixed than currently modeled

### Current classification

Observed:

- `+0x6adc = 636`
- `+0x5634 = 278`

Interpretation:

- both branches are active
- `+0x6adc` had higher participation in the tested payload/TTS run

Nothing urgent remains here unless we want:

- a second scenario to see whether the branch balance changes
- or a stack sample on one of the branches

## 3. Full lifecycle reset semantics

We already know:

- register entry starts key cursor/counter-looking fields at zero
- unregister entry still contains accumulated state

What is still missing:

- whether PAUSE/STOP perform partial reset
- whether final teardown clears fields after unregister entry
- which state helper actually drives the unregister path

### 3-A. Register/unregister with caller stack

Use the low-frequency lifecycle breakpoints again, but include stack output.

```txt
bc *
bp vbaudio_cableb64_win10+0x65b8 ".printf \"[B REGISTER]\\n\"; kb 8; dq @rcx+0xd0 L4; dq @rcx+0xe0 L4; dq @rcx+0x180 L4; gc"
bp vbaudio_cableb64_win10+0x669c ".printf \"[B UNREGISTER]\\n\"; kb 8; dq @rcx+0xd0 L4; dq @rcx+0xe0 L4; dq @rcx+0x180 L4; gc"
bl
g
```

### What to record

- stack on register
- stack on unregister
- whether the same caller always reaches unregister
- whether unregister is tied to STOP, stream close, or another helper

### Why this matters

The field dumps already told us **when** state is alive.
The stack will tell us **which transition path** is responsible.

That is the shortest route to finding the PAUSE/STOP helper without guessing another hot breakpoint address too early.

## Recommended order

Use this order:

1. lifecycle with stacks
2. packet/notification follow-up using a new candidate

Why:

- `+0x6adc` / `+0x5634` are already classified enough for design
- `+0x68ac` is not hot in the tested scenario, but static RE still keeps it relevant
- lifecycle stacks are low-frequency and safe, so they are the best next step on a sluggish target

## When to stop

Stop further runtime probing when any of these becomes true:

- `+0x6adc` / `+0x5634` split is clear enough to write the capture/render branch notes
- packet candidate still has no clean signal after one careful pass
- the target becomes too sluggish for meaningful live-call testing

At that point, the remaining uncertainty should be documented rather than chased with progressively more intrusive breakpoints.

## Bottom line

The remaining work is no longer "find the architecture."

The architecture is already supported.
The remaining work is:

**close packet, capture-branch, and lifecycle details without destroying the live path while measuring it.**
