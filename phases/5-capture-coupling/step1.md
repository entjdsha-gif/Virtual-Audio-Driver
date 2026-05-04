# Phase 5 Step 1: Retire legacy capture write path

## Read First

- Phase 4 Step 2 (legacy render retirement pattern).
- `docs/AO_CABLE_V1_DESIGN.md` § 5.2 (SetState cable branch).

## Goal

Make the legacy `CMiniportWaveRTStream::WriteBytes` (capture side) and
related cable-capture transport call sites no-op for cable streams.

## Planned Files

Edit only:

- `Source/Main/minwavertstream.cpp` — capture-side `WriteBytes`,
  `UpdatePosition` capture-side branch (if it has one), and any
  remaining cable-capture-specific transport call site.

## Required Edits

In `WriteBytes(ULONG ByteDisplacement)`:

```c
VOID
CMiniportWaveRTStream::WriteBytes(ULONG ByteDisplacement)
{
    if (IsCableStream(this)) {
        /* Cable capture fill is owned by AoCableAdvanceByQpc capture
         * branch. WriteBytes is a no-op for cable streams. */
        return;
    }
    /* non-cable legacy path — unchanged */
    ...
}
```

Also retire any remaining `MicSink`-related call sites if they still
exist. Per ADR-002 / forbidden-compromises in CLAUDE.md / AGENTS.md,
`MicSink` dual-write must be entirely gone after Phase 5.

## Rules

- Tell the user before editing.
- Non-cable behavior stays unchanged.
- If a `MicSink` field or function is still wired into anything,
  remove it (or mark it with a TODO + explicit retirement deadline in
  Phase 6 cleanup).

## Acceptance Criteria

- [ ] Build clean.
- [ ] Live cable capture path works correctly with legacy retired.
- [ ] No `MicSink`-related symbol survives in the cable path
      (`grep MicSink` in `Source/` returns nothing under cable
      compilation, or only TODO-tagged stubs scheduled for Phase 6).
- [ ] No regression in non-cable streams.

## Completion

```powershell
python scripts/execute.py mark 5-capture-coupling 1 completed --message "Legacy cable-capture path retired; helper is sole owner."
```
