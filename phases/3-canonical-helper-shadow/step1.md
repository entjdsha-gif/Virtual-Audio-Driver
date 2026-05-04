# Phase 3 Step 1: AoCableAdvanceByQpc body — shadow mode

## Read First

- `docs/ADR.md` ADR-006, ADR-007.
- `docs/AO_CABLE_V1_ARCHITECTURE.md` § 4 (helper signature + body).
- `docs/AO_CABLE_V1_DESIGN.md` § 3, § 4.

## Goal

Implement the `AoCableAdvanceByQpc` body fully — drift correction,
QPC → frames math, 8-frame gate, 100-tick rebase, DMA overrun guard,
scratch linearization, ring write/read, position update — but keep
**audible** cable transport on the legacy path. The helper writes
**shadow** state (its own bookkeeping fields) and bumps debug counters
to prove it's reached, but does not yet replace the legacy ring
write/read or position publication.

## Planned Files

Edit only:

- `Source/Utilities/transport_engine.cpp` — implement the helper body
  per `docs/AO_CABLE_V1_ARCHITECTURE.md` § 4.2 pseudocode and
  `docs/AO_CABLE_V1_DESIGN.md` § 4.

## Required Edits

Implement:

```c
VOID
AoCableAdvanceByQpc(PAO_STREAM_RT rt,
                    ULONGLONG     nowQpcRaw,
                    AO_ADVANCE_REASON reason,
                    ULONG         flags)
{
    KIRQL oldIrql;
    KeAcquireSpinLockRaiseToDpc(&rt->PositionLock);

    /* drift correction (ADR-007) */
    apply_drift_correction(rt, nowQpcRaw);

    ULONGLONG nowQpc100ns = AoQpcTo100ns(nowQpcRaw);
    ULONGLONG elapsed = ((nowQpc100ns - rt->AnchorQpc100ns)
                         * rt->SampleRate) / 10000000ULL;
    LONG advance = (LONG)(elapsed - rt->PublishedFramesSinceAnchor);

    /* 8-frame minimum gate */
    if (advance < 8) {
        rt->DbgShadowAdvanceHits++;
        switch (reason) {
            case AO_ADVANCE_QUERY:         rt->DbgShadowQueryHits++; break;
            case AO_ADVANCE_TIMER_RENDER:
            case AO_ADVANCE_TIMER_CAPTURE: rt->DbgShadowTimerHits++; break;
            default: break;
        }
        KeReleaseSpinLock(&rt->PositionLock, oldIrql);
        return;
    }

    /* 100-tick rebase */
    if (elapsed >= ((ULONGLONG)rt->SampleRate << 7)) {
        rt->PublishedFramesSinceAnchor = 0;
        rt->AnchorQpc100ns = nowQpc100ns;
    }

    /* overrun guard */
    if ((ULONG)advance > (rt->SampleRate / 2)) {
        rt->StatOverrunCounter++;
        KeReleaseSpinLock(&rt->PositionLock, oldIrql);
        return;
    }

    /* SHADOW MODE: compute everything but do NOT write FRAME_PIPE,
     * do NOT write DMA, do NOT update MonoFramesLow/Mirror that
     * GetPosition reads. Use shadow fields only. */

    rt->LastAdvanceDelta = advance;
    rt->PublishedFramesSinceAnchor = (ULONG)elapsed;
    rt->DbgShadowAdvanceHits++;
    switch (reason) {
        case AO_ADVANCE_QUERY:         rt->DbgShadowQueryHits++; break;
        case AO_ADVANCE_TIMER_RENDER:
        case AO_ADVANCE_TIMER_CAPTURE: rt->DbgShadowTimerHits++; break;
        default: break;
    }

    KeReleaseSpinLock(&rt->PositionLock, oldIrql);
}
```

`apply_drift_correction` and `AoQpcTo100ns` are static helpers. The
63/64 phase correction is conservative on the first call (no
correction yet); subsequent ticks accumulate the correction state.

## Rules

- Tell the user before editing.
- Helper must run at `DISPATCH_LEVEL` safe (no allocations, no paged
  memory, no waits).
- Helper must be re-entrant safe via the per-stream `PositionLock`.
- Do not yet wire any caller — Steps 2 / 3 do that.
- Do not perform any FRAME_PIPE write or DMA write in this step.

## Acceptance Criteria

- [ ] Build clean.
- [ ] Calling the helper directly from a unit test (synthetic
      `AO_STREAM_RT` instance) increments `DbgShadowAdvanceHits` and
      the appropriate per-reason counter.
- [ ] Calling the helper twice with the same `nowQpcRaw` results in
      one increment and one no-op (8-frame gate fires the second
      time because `elapsed - PublishedFramesSinceAnchor < 8`).
- [ ] Calling with a synthesized future `nowQpcRaw` such that
      `advance > sampleRate / 2` returns without updating
      `PublishedFramesSinceAnchor` and increments `StatOverrunCounter`.

## What This Step Does NOT Do

- Does not flip audible ownership.
- Does not change `MonoFramesLow` / `MonoFramesMirror` (those stay
  driven by legacy `UpdatePosition` until Phase 4 / 5).
- Does not write FRAME_PIPE.

## Completion

```powershell
python scripts/execute.py mark 3-canonical-helper-shadow 1 completed --message "Helper body in shadow mode."
```
