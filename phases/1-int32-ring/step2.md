# Phase 1 Step 2: Ring write - INT32 conversion (no SRC yet)

## Read First

- `docs/ADR.md` ADR-003, ADR-005
- `docs/AO_CABLE_V1_DESIGN.md` § 2.3 (write SRC algorithm), § 2.5
  (bit-depth dispatch table)
- Phase 1 Step 1 (struct shape).

## Goal

Implement the **same-rate** ring write path: client format → INT32 ring,
with 4-way bit-depth dispatch (8 / 16 / 24 / 32) and ~19-bit
normalization. SRC (rate conversion) is **out of scope** for this step
and added in Phase 2. Step 2 only handles `srcRate == pipe->InternalRate`.

## Planned Files

Edit only:

- `Source/Utilities/loopback.cpp` — implement `AoRingWriteFromScratch`
  same-rate path. Stub the rate-mismatch path with `STATUS_NOT_SUPPORTED`
  (returns clean error, no fall-back to old code).

Do not touch:

- `Source/Utilities/loopback.h` (signature stable from Step 1).
- Any caller — `AoRingWriteFromScratch` is still called by stubs only.

## Required Edits

Implement same-rate path in `AoRingWriteFromScratch`:

```c
NTSTATUS
AoRingWriteFromScratch(PFRAME_PIPE pipe, const BYTE* scratch, ULONG frames,
                       ULONG srcRate, ULONG srcChannels, ULONG srcBits)
{
    KIRQL oldIrql;
    KeAcquireSpinLock(&pipe->Lock, &oldIrql);

    if (srcRate != pipe->InternalRate /* same-rate only in Step 2 */) {
        KeReleaseSpinLock(&pipe->Lock, oldIrql);
        return STATUS_NOT_SUPPORTED;
    }

    LONG writable = AoRingAvailableSpaceFrames_Locked(pipe);
    if ((LONG)frames > writable) {
        pipe->OverflowCounter++;
        KeReleaseSpinLock(&pipe->Lock, oldIrql);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    /* per-frame, per-channel write with bit-depth dispatch */
    for (ULONG f = 0; f < frames; ++f) {
        for (ULONG ch = 0; ch < min(srcChannels, pipe->Channels); ++ch) {
            LONG sample = NormalizeToInt19(scratch, srcBits, f, ch, srcChannels);
            LONG slot = (pipe->WritePos * pipe->Channels) + ch;
            pipe->Data[slot] = sample;
        }
        pipe->WritePos++;
        if (pipe->WritePos >= pipe->WrapBound) pipe->WritePos = 0;
    }

    KeReleaseSpinLock(&pipe->Lock, oldIrql);
    return STATUS_SUCCESS;
}
```

`NormalizeToInt19` is a static inline helper covering the bit-depth
dispatch table (`docs/AO_CABLE_V1_DESIGN.md` § 2.5). 4 branches, no
indirection.

`AoRingAvailableSpaceFrames_Locked` is the **single source** of the
guard subtraction: it returns `WrapBound - currentFill - 2` (writer-vs-
reader collision guard, 2-frame band) computed without re-acquiring the
lock. Callers compare `framesNeeded > available` directly — they must
**not** subtract another 2, otherwise the guard band doubles and the
ring loses 2 frames of usable capacity. (Review #2 of 8afa59a.)

## Rules

- Tell the user before editing.
- Do not implement the SRC (rate conversion) path. That is Phase 2.
- Do not modify caller behavior — all callers still go through the
  stub-or-pre-rewrite path until Phase 3 wires the canonical helper.
- If the existing `LoopbackWrite` / `WriteConverted` legacy code still
  has callers, leave them in place; their behavior may now be wrong but
  this is expected until Phase 4 flips ownership.

## Acceptance Criteria

- [ ] `build-verify.ps1 -Config Release` succeeds.
- [ ] Unit-style test in `tests/phase1-runtime/` (untracked) writes
      a 1 kHz sine at 48 kHz / 24 / Stereo into a fresh `FRAME_PIPE`,
      then reads back via the existing legacy read path (or an inspector
      helper) and verifies:
      - WritePos advanced by `frames`.
      - Ring sample values match the 19-bit normalized expected curve.
      - `OverflowCounter == 0` for non-overflow scenarios.
- [ ] Forced overflow scenario (write more frames than available)
      returns `STATUS_INSUFFICIENT_RESOURCES`, increments
      `OverflowCounter`, and does **not** advance WritePos.
- [ ] Different-rate scenario returns `STATUS_NOT_SUPPORTED` cleanly.

## What This Step Does NOT Do

- Does not implement SRC.
- Does not change the audible cable behavior (no caller switched over
  yet).
- Does not modify `AO_STREAM_RT`.

## Completion

```powershell
python scripts/execute.py mark 1-int32-ring 2 completed --message "Same-rate INT32 ring write + 4-way bit-depth dispatch."
```
