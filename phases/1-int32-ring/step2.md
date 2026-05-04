# Phase 1 Step 2: Ring read — INT32 conversion (no SRC yet)

## Read First

- `docs/ADR.md` ADR-003, ADR-005
- `docs/AO_CABLE_V1_DESIGN.md` § 2.4 (read SRC algorithm), § 2.5
  (bit-depth dispatch table — read direction).
- Phase 1 Step 1 (write side).

## Goal

Implement the **same-rate** ring read path: INT32 ring → client format,
with 4-way bit-depth dispatch (denormalize from 19-bit). SRC out of scope
(Phase 2). Underrun handling is in this step (Step 1 deferred it).

## Planned Files

Edit only:

- `Source/Utilities/loopback.cpp` — implement `AoRingReadToScratch`
  same-rate path with underrun + hysteretic recovery (per ADR-005).
  Stub the rate-mismatch path with `STATUS_NOT_SUPPORTED`.

## Required Edits

```c
NTSTATUS
AoRingReadToScratch(PFRAME_PIPE pipe, BYTE* scratch, ULONG frames,
                    ULONG dstRate, ULONG dstChannels, ULONG dstBits)
{
    KIRQL oldIrql;
    KeAcquireSpinLock(&pipe->Lock, &oldIrql);

    if (dstRate != pipe->InternalRate /* same-rate only in Step 2 */) {
        KeReleaseSpinLock(&pipe->Lock, oldIrql);
        return STATUS_NOT_SUPPORTED;
    }

    LONG available = AoRingAvailableFrames_Locked(pipe);

    /* hysteretic underrun recovery */
    if (pipe->UnderrunFlag) {
        if (available < pipe->WrapBound / 2) {
            ZeroFillScratch(scratch, frames, dstBits, dstChannels);
            KeReleaseSpinLock(&pipe->Lock, oldIrql);
            return STATUS_SUCCESS;
        }
        pipe->UnderrunFlag = 0;  /* exit recovery */
    }

    if ((LONG)frames > available) {
        pipe->UnderrunCounter++;
        pipe->UnderrunFlag = 1;
        ZeroFillScratch(scratch, frames, dstBits, dstChannels);
        KeReleaseSpinLock(&pipe->Lock, oldIrql);
        return STATUS_SUCCESS;
    }

    for (ULONG f = 0; f < frames; ++f) {
        for (ULONG ch = 0; ch < dstChannels; ++ch) {
            LONG slot = (pipe->ReadPos * pipe->Channels) + ch;
            LONG sample = (ch < pipe->Channels) ? pipe->Data[slot] : 0;
            DenormalizeFromInt19(scratch, dstBits, f, ch, dstChannels, sample);
        }
        pipe->ReadPos++;
        if (pipe->ReadPos >= pipe->WrapBound) pipe->ReadPos = 0;
    }

    KeReleaseSpinLock(&pipe->Lock, oldIrql);
    return STATUS_SUCCESS;
}
```

## Rules

- Tell the user before editing.
- Same-rate only.
- Caller-side behavior still unchanged.

## Acceptance Criteria

- [ ] Build clean.
- [ ] Round-trip test (Step 1 write → Step 2 read at same rate) is
      bit-exact for 16-bit and 32-bit input. 8-bit and 24-bit may show
      ≤1 LSB delta from the lossy 19-bit normalization (intentional).
- [ ] Forced underrun (read more frames than available) returns
      `STATUS_SUCCESS`, scratch is silence-filled, `UnderrunCounter`
      incremented, `UnderrunFlag = 1`.
- [ ] Recovery: continued reads while `available < WrapBound / 2` keep
      delivering silence. When fill recovers ≥ `WrapBound / 2`,
      `UnderrunFlag = 0` and real data resumes.

## What This Step Does NOT Do

- No SRC.
- No caller swap.
- No changes to `AO_STREAM_RT` or the canonical helper.

## Completion

```powershell
python scripts/execute.py mark 1-int32-ring 2 completed --message "Same-rate INT32 ring read + hysteretic underrun."
```
