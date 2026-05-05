# Phase 3 Step 3: Wire shared timer call source

## Read First

- `docs/AO_CABLE_V1_DESIGN.md` § 4.3 (timer DPC body).
- `docs/ADR.md` ADR-006 (no second owner), ADR-009 (KeFlushQueuedDpcs).
- Current `Source/Utilities/transport_engine.cpp` engine timer DPC.

## Goal

The shared transport engine timer DPC, on every tick, invokes
`AoCableAdvanceByQpc` for each active cable stream with the appropriate
`AO_ADVANCE_TIMER_RENDER` / `AO_ADVANCE_TIMER_CAPTURE` reason. Helper
still in shadow mode.

## Planned Files

Edit only:

- `Source/Utilities/transport_engine.cpp` — timer DPC body.

## Required Edits

Match the target body from `docs/AO_CABLE_V1_DESIGN.md` § 4.3:

```c
EVT_EX_TIMER AoTransportTimerCallback;

VOID AoTransportTimerCallback(PEX_TIMER timer)
{
    LARGE_INTEGER nowQpcRaw = KeQueryPerformanceCounter(NULL);

    PAO_STREAM_RT snap[16];
    ULONG count = 0;

    KIRQL oldIrql;
    KeAcquireSpinLock(&g_engine.EngineLock, &oldIrql);
    /* snapshot active cable streams; RefCount++ each */
    for_each_active_cable_stream(rt) {
        if (count >= ARRAYSIZE(snap)) break;
        InterlockedIncrement(&rt->RefCount);
        snap[count++] = rt;
    }
    KeReleaseSpinLock(&g_engine.EngineLock, oldIrql);

    for (ULONG i = 0; i < count; ++i) {
        PAO_STREAM_RT rt = snap[i];
        AO_ADVANCE_REASON reason = rt->IsCapture
            ? AO_ADVANCE_TIMER_CAPTURE
            : AO_ADVANCE_TIMER_RENDER;
        AoCableAdvanceByQpc(rt, nowQpcRaw.QuadPart, reason, 0);
        AoTransportFreeStreamRt(rt);   /* drops the transient DPC ref;
                                        * frees if last ref (matches the
                                        * single-deallocator contract in
                                        * DESIGN § 4.2). */
    }
}
```

## Rules

- Tell the user before editing.
- DPC body must not hold `EngineLock` across helper invocation. Snapshot
  + drop lock + run helper + release ref. Pattern matches what was
  proven correct in earlier Phase 6 work and matches VB.
- Existing legacy timer-driven cable transport (any leftover Y2-2-style
  helper-render-bytes code) stays operational alongside, **as the
  audible owner**. Step 3 only adds the shadow-mode helper invocation.
  Phase 4 retires the legacy audible path.

## Acceptance Criteria

- [ ] Build clean.
- [ ] Live test: open cable stream, observe `DbgShadowTimerHits`
      increases steadily at 1 ms cadence during the call.
- [ ] No regression vs Step 2 baseline.
- [ ] `KeFlushQueuedDpcs` on stream destructor works correctly — no
      UAF when a stream stops while DPC is in flight (verified by
      stress test that opens / closes streams rapidly).

## What This Step Does NOT Do

- Does not flip audible ownership (Phase 4 / 5).
- Does not retire the legacy timer-driven path.

## Completion

```powershell
python scripts/execute.py mark 3-canonical-helper-shadow 3 completed --message "Timer DPC wired to helper (shadow mode)."
```
