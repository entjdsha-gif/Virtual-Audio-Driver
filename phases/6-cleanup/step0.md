# Phase 6 Step 0: Remove pump feature flags

## Read First

- Phase 5 exit (V1 audible parity with VB).
- Current `Source/Main/minwavertstream.h` (`AO_PUMP_FLAG_*`,
  `m_ulPump*` fields).
- `Source/Main/ioctl.h` (`IOCTL_AO_SET_PUMP_FEATURE_FLAGS`,
  `AO_PUMP_FLAGS_REQ`).

## Goal

Delete the Phase 5 pump-feature-flag scaffolding now that the canonical
helper is the single owner. This was a Phase 5-era runtime rollback
mechanism that became obsolete when ownership flipped definitively in
Phases 4 / 5.

## Planned Files

Edit only:

- `Source/Main/minwavertstream.h` — delete `AO_PUMP_FLAG_*` constants
  and `m_ulPump*`, `m_ullPump*`, `m_bPumpInitialized` fields.
- `Source/Main/minwavertstream.cpp` — delete `PumpToCurrentPositionFromQuery`,
  `ApplyPumpFlagMaskUnderLock`, and any pump-flag branch.
- `Source/Main/ioctl.h` — delete `IOCTL_AO_SET_PUMP_FEATURE_FLAGS`
  and `AO_PUMP_FLAGS_REQ`. **Do not delete `IOCTL_AO_GET_STREAM_STATUS`,
  `IOCTL_AO_GET_CONFIG`, or other live IOCTLs.**
- `Source/Main/adapter.cpp` — delete the
  `IOCTL_AO_SET_PUMP_FEATURE_FLAGS` handler.
- `Source/Utilities/loopback.h` — delete pump-flag helper API
  (`AoPumpApplyRenderFlagMask`, `AoPumpRegisterActiveRenderStream`,
  `AoPumpUnregisterActiveRenderStream`) if still present.
- `Source/Utilities/loopback.cpp` — delete the implementations.

## Rules

- Tell the user before deleting each file's content.
- This is a multi-file atomic-ish change. If any non-cable code still
  uses a pump flag, **stop and report** — do not blanket-delete.
- Diagnostic counters tied to pump flags (`A_R_PumpFeatureFlags` etc.
  in `AO_V2_DIAG`) — leave the field for now (don't break IOCTL
  schema mid-cleanup); set their value to constant 0. Step 4 of
  Phase 7 may rename or repurpose them later.

## Acceptance Criteria

- [ ] Build clean.
- [ ] `grep -r "AO_PUMP_FLAG\|PumpFeatureFlag\|m_ulPump\|PumpToCurrentPosition"
      Source/` returns nothing (or only TODO-tagged stubs scheduled
      for further cleanup).
- [ ] No regression in non-cable streams.
- [ ] No regression in cable streams (helper is sole owner).

## Completion

```powershell
python scripts/execute.py mark 6-cleanup 0 completed --message "Pump feature flags removed; helper is sole owner; no fallback path."
```
