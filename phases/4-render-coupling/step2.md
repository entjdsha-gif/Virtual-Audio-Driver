# Phase 4 Step 2: Retire legacy cable-render path

## Read First

- Phase 4 Step 0 (helper is audible owner now; legacy is redundant).
- `docs/AO_CABLE_V1_DESIGN.md` § 5.2 (SetState cable branch),
  § 5.3 (destructor).

## Goal

Make the legacy `CMiniportWaveRTStream::UpdatePosition` cable-render
branch a thin shim or no-op for cable streams. Non-cable streams keep
their existing behavior unchanged.

## Planned Files

Edit only:

- `Source/Main/minwavertstream.cpp` — `UpdatePosition` cable branch,
  `WriteBytes` cable branch, related cable transport call sites that
  currently double as transport owners.

## Required Edits

In `UpdatePosition(LARGE_INTEGER ilQPC)`:

```c
VOID
CMiniportWaveRTStream::UpdatePosition(LARGE_INTEGER ilQPC)
{
    if (IsCableStream(this) && m_pTransportRt) {
        /* Cable transport is owned by AoCableAdvanceByQpc.
         * UpdatePosition becomes a no-op for cable streams; it is
         * kept in place because PortCls / WaveRT may still call it.
         * The shared timer DPC and the GetPositions query path both
         * already advance the helper.
         *
         * If we want UpdatePosition itself to be a wake source too,
         * we may invoke the helper here with reason QUERY:
         */
        AoCableAdvanceByQpc(m_pTransportRt, ilQPC.QuadPart,
                            AO_ADVANCE_QUERY, 0);
        return;
    }
    /* non-cable legacy path — unchanged */
    ...
}
```

In `WriteBytes(ULONG ByteDisplacement)`:

```c
VOID
CMiniportWaveRTStream::WriteBytes(ULONG ByteDisplacement)
{
    if (IsCableStream(this)) {
        /* Cable render fill is owned by AoCableAdvanceByQpc render
         * branch. WriteBytes is a no-op for cable streams. */
        return;
    }
    /* non-cable legacy path — unchanged */
    ...
}
```

(Mirror for `ReadBytes` if it has cable branches that should be
retired here. Capture-side `WriteBytes` retirement is Phase 5.)

## Rules

- Tell the user before editing each function.
- Non-cable behavior is **strictly unchanged**.
- Phase 5 capture coupling depends on retiring legacy capture write,
  so Step 2 here only retires the **render** legacy paths, not
  capture.
- If a Phase 5 / pump-flag artifact still depends on
  `UpdatePosition`'s cable side-effects, capture the dependency in
  the commit message and address it in Phase 6 cleanup.

## Acceptance Criteria

- [ ] Build clean.
- [ ] Live test: cable render path still works correctly with the
      legacy branch retired (helper is now sole audible owner).
- [ ] Legacy advance counters at the stream level
      (`m_ulPumpProcessedFrames` etc.) stop incrementing on cable
      streams (only helper's `MonoFramesLow` does).
- [ ] No regression in non-cable streams.
- [ ] Cable render `OverflowCounter` stays 0 in steady state.

## Completion

```powershell
python scripts/execute.py mark 4-render-coupling 2 completed --message "Legacy cable-render path retired; helper is sole owner."
```
