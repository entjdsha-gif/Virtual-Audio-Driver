# Phase 6 Step 3: Remove MicSink remnants

## Goal

Delete any remaining `MicSink` symbol, field, or function in the cable
code path. Phase 5 retired the dual-write semantically; Step 3 removes
the dead code physically.

## Planned Files

Audit + delete:

- `Source/Utilities/loopback.h` — `MicSink*` fields/declarations.
- `Source/Utilities/loopback.cpp` — `MicSink*` implementations.
- `Source/Main/minwavertstream.cpp` — any `MicSink*` call sites.

## Rules

- `grep -r "MicSink" Source/` must return nothing under the cable
  compilation, no exceptions, no TODOs.
- If a non-cable code path uses a similarly named symbol that is not
  the cable `MicSink`, preserve it (rename if naming conflict matters).

## Acceptance Criteria

- [ ] Build clean.
- [ ] `grep -r "MicSink" Source/` is empty (or pre-existing
      explicitly-non-cable hits only).
- [ ] No regression.

## Completion

```powershell
python scripts/execute.py mark 6-cleanup 3 completed --message "MicSink remnants removed."
```
