# Phase 6 Step 1: Remove 8-tap sinc SRC remnants

## Read First

- Phase 2 (single-pass linear-interp SRC is the cable SRC).
- `docs/ADR.md` ADR-004.

## Goal

Delete any remaining 8-tap sinc SRC code or 2048-coefficient table
in the cable code path. Phase 2 replaced it functionally; Step 1 here
removes the dead code physically.

## Planned Files

Edit only:

- `Source/Utilities/loopback.cpp` — delete sinc tables, sinc kernels,
  multi-stage `ConvertToInternal` / `SrcConvert` /
  `ConvertFromInternal` chain that the cable path no longer uses.
- `Source/Utilities/loopback.h` — delete corresponding declarations.
- `Source/Utilities/sinc_table.h` (if present) — delete entirely.
- `Source/Utilities/Utilities.vcxproj` — remove the deleted file
  references.

## Rules

- Tell the user before each deletion.
- If a non-cable path still uses sinc (e.g., the mic-array sample
  generator), preserve only that exact code path. Cable code is the
  cleanup target.
- Run a grep audit and cite the surviving callers in the commit
  message.

## Acceptance Criteria

- [ ] Build clean.
- [ ] `grep -r "sinc\|Sinc" Source/` returns no cable-related hits.
- [ ] No regression.

## Completion

```powershell
python scripts/execute.py mark 6-cleanup 1 completed --message "8-tap sinc cable SRC remnants removed."
```
