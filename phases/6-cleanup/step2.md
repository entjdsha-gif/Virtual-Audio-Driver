# Phase 6 Step 2: Remove packed 24-bit cable paths

## Goal

Delete any cable-related packed-24-bit-byte-indexed code or fields
that survived Phase 1's struct rewrite. Phase 1 changed the struct
shape; if any callers still computed positions or sizes assuming
3-byte-per-sample, those are now dead-code candidates.

## Planned Files

- `Source/Utilities/loopback.cpp` — packed-24 helpers / size math.
- `Source/Utilities/loopback.h` — packed-24-related declarations.
- `Source/Main/minwavertstream.cpp` — any remaining `byte * 3` size
  arithmetic that referenced cable storage.

## Rules

- Audit `grep -r "PackedBytes\|3 \\* sizeof\|BytesPerSample.*3" Source/`
  and classify each hit.
- Cable hits → delete.
- Non-cable hits (e.g., the legacy `LoopbackWrite` path for sample
  miniport) → preserve.

## Acceptance Criteria

- [ ] Build clean.
- [ ] No cable-side packed-24 logic survives.
- [ ] Non-cable behavior unchanged.

## Completion

```powershell
python scripts/execute.py mark 6-cleanup 2 completed --message "Packed 24-bit cable paths removed."
```
