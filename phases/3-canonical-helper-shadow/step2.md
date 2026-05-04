# Phase 3 Step 2: Wire query path call source

## Read First

- `docs/AO_CABLE_V1_DESIGN.md` § 5.1.
- Current `Source/Main/minwavertstream.cpp` `GetPosition` /
  `GetPositions` cable branches.

## Goal

Make every cable `GetPosition` and `GetPositions` call invoke
`AoCableAdvanceByQpc(rt, KeQueryPerformanceCounter(),
AO_ADVANCE_QUERY, 0)` inside the position spinlock **before** reading
position values. The helper still runs in shadow mode (Step 1), so
audible behavior is unchanged. This step proves the wiring works.

## Planned Files

Edit only:

- `Source/Main/minwavertstream.cpp` — cable branches of `GetPosition`
  and `GetPositions`.

Do not touch the non-cable code path.

## Required Edits

In `GetPositions(...)`:

```c
NTSTATUS
CMiniportWaveRTStream::GetPositions(
    _Out_opt_ ULONGLONG*     pllLinearBufferPosition,
    _Out_opt_ ULONGLONG*     pllPresentationPosition,
    _Out_opt_ LARGE_INTEGER* pliQPCTime)
{
    KIRQL oldIrql;
    KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);

    if (IsCableStream(this) && m_pTransportRt) {
        LARGE_INTEGER now = KeQueryPerformanceCounter(NULL);
        AoCableAdvanceByQpc(m_pTransportRt, now.QuadPart,
                            AO_ADVANCE_QUERY, 0);
        if (pliQPCTime) pliQPCTime->QuadPart = now.QuadPart;
    }

    /* Existing legacy position publication stays — Phase 4 retires it
     * for cable streams. Until then, GetPositions returns the legacy
     * MonoFramesLow / Mirror values. */
    ...

    KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);
    return STATUS_SUCCESS;
}
```

`IsCableStream(this)` is a small helper: `m_pMiniport->m_DeviceType` is
`eCable*`.

Same pattern for `GetPosition`.

## Rules

- Tell the user before editing.
- Helper invocation must be **inside** `m_PositionSpinLock` to match
  the design (per § 5.1).
- Do not change the position values that `GetPositions` returns yet —
  legacy publication stays.
- Do not modify non-cable behavior.

## Acceptance Criteria

- [ ] Build clean.
- [ ] Live test: open a cable stream, run a normal capture/render pair,
      observe via `test_stream_monitor.py` that `DbgShadowQueryHits`
      increases steadily during the call.
- [ ] No audible regression vs the pre-Phase-3 baseline.
- [ ] No new BSOD / hang / deadlock under live use.

## What This Step Does NOT Do

- Does not change audible cable transport.
- Does not wire timer / packet call sources (Step 3).

## Completion

```powershell
python scripts/execute.py mark 3-canonical-helper-shadow 2 completed --message "GetPosition/GetPositions wired to helper (shadow mode)."
```
