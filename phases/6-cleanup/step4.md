# Phase 6 Step 4: Strip stale Phase 5 / Phase 6 / Y-attempt comments

## Goal

Sweep `Source/` for stale comments that reference retired patterns
(Phase 5 pump model, Step 3-4 timer-owned transport, Option Z, Option
Y, Y1A/B/C, Y2-1/Y2-2, Y3 attempts, etc.) and remove or rewrite them
to match the V1 final design.

## Planned Files

Audit-and-edit:

- `Source/Main/minwavertstream.h` — header comments around field
  blocks.
- `Source/Main/minwavertstream.cpp` — function-level comments.
- `Source/Main/adapter.cpp`
- `Source/Utilities/loopback.h`
- `Source/Utilities/loopback.cpp`
- `Source/Utilities/transport_engine.h`
- `Source/Utilities/transport_engine.cpp`

## Rules

- `grep -rn "Phase 5\|Phase 6 Step 3\|Step 3/4\|Option Y\|Option Z\|Y1A\|Y1B\|Y1C\|Y2-1\|Y2-2\|Y3\|MSVAD"
  Source/` and classify each hit:
  - **Stale** (refers to a retired pattern that no longer exists in
    code) → delete or rewrite.
  - **Historical** (refers to a verified-correct pattern that came
    from a Phase, useful for future readers) → keep but reword to
    cite the V1 source (e.g., `docs/ADR.md` ADR-NNN) instead of the
    abandoned Phase number.
  - **Active** (refers to something still present in code) → confirm
    the description still matches and update if it doesn't.
- Do not delete useful design rationale wholesale. The goal is
  **accuracy**, not minimalism.

## Acceptance Criteria

- [ ] Build clean.
- [ ] No comment in `Source/` refers to a retired Phase 5/6
      experiment by name without context, except for explicit
      historical citations in commit logs or `docs/archive/`.
- [ ] Spot-check 10 random comments — each accurately describes the
      surrounding code.

## Completion

```powershell
python scripts/execute.py mark 6-cleanup 4 completed --message "Stale Phase 5/6/Y comments rewritten or removed."
```
