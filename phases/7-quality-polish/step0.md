# Phase 7 Step 0: Rate-aware frame gate

## Goal

The 8-frame minimum gate (ADR-007) is rate-aware in spirit but
hard-coded as 8 frames. At 8 kHz mono it is 1 ms, exactly one tick
— marginal. Phase 7 introduces a rate-aware floor:

```c
gate_frames = max(8, (rt->SampleRate * 167) / 1000000);  /* 167 µs */
```

Step 0 implements this and validates that low-rate streams (8 kHz
mono) no longer spend most ticks gated out.

## Planned Files

- `Source/Utilities/transport_engine.cpp` — gate computation in
  helper.

## Acceptance Criteria

- [ ] Build clean.
- [ ] Local sine loopback at 8 kHz mono is now smooth (gate fires
      only on jitter, not on the normal cadence).
- [ ] No regression at 48 kHz / higher rates (gate stays at 8 frames
      effective).

## Completion

```powershell
python scripts/execute.py mark 7-quality-polish 0 completed --message "Rate-aware frame gate (≥167 µs)."
```
