# Phase 7 Step 0: Rate-aware frame gate (DEFERRED)

## Status

**DEFERRED out of V1.** This step is retained as a placeholder so
phase numbering does not shift; it must be marked `completed` with a
no-op message before Phase 7 exit.

## Why deferred

V1 explicitly preserves VB-Cable parity, including the **fixed 8-frame
minimum gate** in `AoCableAdvanceByQpc` (ADR-007). The earlier draft
of this step proposed a rate-aware gate:

```c
gate_frames = max(8, (rt->SampleRate * 167) / 1000000);  /* 167 µs */
```

The proposed formula has two problems that make it unfit for V1:

1. **Stated goal not achieved at 8 kHz.** At 8 kHz mono the formula
   evaluates to `max(8, 1) = 8`, identical to the fixed gate. The
   "low-rate streams no longer spend most ticks gated out" objective
   does not apply.
2. **VB parity lost above 48 kHz.** At 96 kHz the formula yields 16,
   at 192 kHz it yields 32. VB-Cable uses a fixed 8-frame gate at
   every supported rate. Diverging here makes any future "is this a
   driver-internal regression vs a VB-parity regression" diagnosis
   harder.

(Review #16 of 8afa59a.)

## Future work

A rate-aware gate (or a configurable gate) may still be worth
introducing post-V1 if:

- live-call evidence shows the fixed 8-frame gate causes audible
  artifacts at a specific rate, **and**
- the mechanism is re-described in a new ADR that supersedes
  ADR-007's "fixed 8-frame gate" choice with explicit rationale and
  parity exception language.

That work belongs in V1.next, not V1.

## Required Edits

None. This step is a documentation-only placeholder.

## Acceptance Criteria

- [ ] No code change.
- [ ] V1 keeps the fixed 8-frame gate from ADR-007.
- [ ] No rate-aware-gate ADR has been added behind anyone's back.

## Completion

```powershell
python scripts/execute.py mark 7-quality-polish 0 completed --message "Rate-aware frame gate deferred out of V1; VB-parity 8-frame gate retained."
```
