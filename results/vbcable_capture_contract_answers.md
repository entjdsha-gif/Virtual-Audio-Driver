# VB-Cable Capture-Side Pacing/Latency Contract — Deep Decompile Verification

Source: `results/ghidra_decompile/vbcable_all_functions.c` (12096 lines). All line numbers
below refer to that file. All struct offsets are verified against the decompiled
reads/writes, not copied from the prior analysis documents. Where a prior doc
(`vbcable_pipeline_analysis.md`, `vbcable_disasm_analysis.md`) contradicts what the
decompile shows, it is flagged.

---

## 0. Ring struct layout — CORRECTION to prior docs

Prior docs claim:
`+0x00 SR actual, +0x04 SR requested, +0x08 RingDataOffset, +0x0C Frames,
+0x10 TotalSlots, +0x14 WrapBound, +0x18 WritePos, +0x1C ReadPos,
+0x20 Stride, +0x24 Valid, +0x180 Overflow, +0x188 Underrun`.

**This is wrong in several places.** Verified from `FUN_140001008` (constructor,
lines 14-42):

```c
iVar1 = param_1 * param_2;                        // totalSamples = frames × channels
*(int *)((longlong)param_4 + 0xc) = param_1;      // +0x0C = frames
*(int *)(param_4 + 2)           = param_2;        // +0x10 = channels  (param_4 is longlong*, +2→+0x10)
*(undefined4 *)(param_4 + 4)    = param_3;        // +0x20 = SR (copy A)
*(int *)(param_4 + 5)           = iVar1 * 4;      // +0x28 = samples × 4 (one ring in bytes)
*(int *)((longlong)param_4 + 0x2c) = iVar1 * 8;   // +0x2C = samples × 8 (2× mirror wrap)
*(undefined4 *)(param_4 + 1)    = 400;            // +0x08 = data header offset (const 400)
*(int *)((longlong)param_4 + 0x24) = iVar1*4+400; // +0x24 = 2nd-copy / mirror offset
*(undefined4 *)((longlong)param_4 + 4) = param_3; // +0x04 = SR (copy B)
*(int *)param_4                 = param_1;        // +0x00 = frames (later overwritten by Latency reg)
*(int *)((longlong)param_4 + 0x14) = param_1;     // +0x14 = frames (redundant)
```

**Corrected map** (verified by tracing reader/writer uses):

| Offset | Size | Meaning |
|--------|------|---------|
| +0x00 | int | **Effective latency frames** — initialized to capacity, then OVERWRITTEN in `FUN_140016380:7704` with the registry-clamped Latency value |
| +0x04 | int | SR (copy B) |
| +0x08 | int | Data header offset (const 400) |
| +0x0C | int | Frames (capacity), immutable once allocated |
| +0x10 | int | Channels (16 for VB adapter ring) |
| +0x14 | int | Frames (redundant copy; used as wrap modulus by readers/writers) |
| +0x18 | int | **WritePos** (frame index, range [0, frames)) |
| +0x1C | int | **ReadPos** (frame index) |
| +0x20 | int | SR (copy A) |
| +0x24 | int | 2nd-copy / mirror offset = samples × 4 + 400 |
| +0x28 | int | Stride bytes = samples × 4 |
| +0x2C | int | Wrap bound = samples × 8 |
| +0x30 | int | owned-flag (1 if allocated internally) |
| +0xB8 | int | SRC accumulator phase (FUN_1400017ac:588) |
| +0xBC..+0xFB | int[N] | Per-channel SRC residual state (FUN_1400017ac:1013) |
| +0x17C | int | cleared in ring-reset; purpose not fully traced |
| +0x180 | int | **Write-overflow counter** — writer increments when ring is full (FUN_1400022b0:1251, FUN_1400026a0:1413) |
| +0x184 | int | **Read-underrun counter** — reader increments when insufficient data (FUN_1400011d4 `param_1[0x61]++` lines 294,331,373,443; FUN_1400017ac `+0x184 += 1` lines 640,712,770,842,908,997) |
| +0x188 | int | **"Drained" boolean flag** (0/1) — set when a read fails; cleared when ring fills past half (FUN_1400011d4:450/477, FUN_1400017ac:1003/1045) |

Key disagreement with prior docs: **the "Underrun" counter is at +0x184, NOT +0x188**.
+0x188 is a hysteresis flag, not a counter. `test_stream_monitor.py` and the AO driver
diagnostic IOCTL code that mirrors VB's layout must use +0x184 for underrun counts.

---

## 1. Latency=7168 unit

**Question**: What unit is `VBAudioCableAWDM_Latency = 7168`?

**Verified from code** — `FUN_140001008:19,31` proves ring cells are
`frames × channels`, 4 bytes each. `FUN_140016380` registry flow (7682, 7696-7704):

```c
// 7682: allocate ring with frames=uVar7+8, channels=0x10, SR=uVar9
DAT_140012e60 = (uint *)FUN_140001000(uVar7 + 8, 0x10, uVar9);
// 7696: read VBAudioCableAWDM_Latency → local_158
swprintf(local_148, ..., L"VBAudioCableAWDM_Latency", 1);
FUN_14000436c(0, 0x14001b310, (longlong)local_148, &local_158);
// 7700: clamp local_158 into [0x30, uVar7]
if (0x2f < local_158 && uVar7 < local_158) uVar6 = uVar7;  else uVar6 = local_158;
// 7704: store clamped value at ring +0x00
if (DAT_140012e60 != 0) *DAT_140012e60 = uVar6;
```

`uVar7` default at line 7645 is `0x1c00 = 7168` (registered under a separate "max" key
at `0x14001b290`). So if Latency reg value is 7168 and the max is also 7168, after clamp
`uVar6 = 7168`, and the ring's effective latency frame count at +0x00 is 7168.

**Answer** (high confidence):

- Latency unit is **frames** (not bytes, not 100ns).
- `7168 frames @ 48 kHz = 149.33 ms` — this is the MAX buffering window, not the target.
- Stride = 4 bytes per sample (INT32). Verified.
- Ring data region is allocated for `(7176 frames × 16 channels × 12) + 400 ≈ 1.37 MB`
  (12× the per-sample size — likely ring + mirror + SRC scratch).

**Uncertain**: I did not trace whether the float-path ring write uses a different stride.
The 0x78c (INT32/float-bit) write at FUN_1400022b0:1227 copies 4 bytes per sample same as
the INT32 path, so stride is still 4.

---

## 2. Latency=7168 maps to what

**Question**: (a) loopback ring depth, (b) capture delivery latency, (c) endpoint engine
latency reported to Windows? Is it passed to portcls or only used internally?

**Verified from code** — after `FUN_140016380` writes the value to ring +0x00, the only
immediate downstream caller is `FUN_1400040b0` (line 7742). That function (2731-2780) just
writes to a global diagnostics table (`DAT_140012cc0` indexed by param_1), storing
`SR, max_latency, effective_latency, flags`. It makes **no PortCls call** — no
`PcSetDeviceFormat`, no `PropertyHandlerAudio`, no `IMiniportAudioEngine`.

The Latency value is used only by:
- `FUN_140001180` (live-resize clamp): if stored `*param_1` (+0x00) changes, update
  `param_1[5]` (+0x14 wrap modulus) accordingly.
- Writer's "enough room" check (FUN_1400022b0:1137-1143, FUN_1400026a0:1412) — based on
  the +0x14 modulus, which reflects the Latency-overwritten value.

**Answer** (high confidence):

- (a) **YES** — loopback ring depth (render→capture via the global adapter ring).
- (b) **Indirectly** — capture delivery latency is *bounded* by this. You cannot queue
  more than 7168 frames between render and capture.
- (c) **NO** — it is NOT reported to Windows as an endpoint/engine latency. Windows derives
  endpoint latency from the WaveRT buffer parameters the miniport advertises, not from this
  registry key. The key shows up nowhere in the code path leading to a PortCls or
  IMiniport call that I traced.

So the 7168 is a **maximum speaker-to-mic buffering window**, not a target latency.

**Uncertain**: A secondary function could use +0x00 as a KS property reply. But the flow
`registry → *ring_base = latency → FUN_1400040b0(diag table)` does not pass through
any PcFunction or IMiniport callback in the traced path.

---

## 3. Pacing policy (same-tick repeated reads)

**Question**: If WASAPI calls capture-read repeatedly within 1ms, does each call get new
data or the same data?

**Verified from code** — `FUN_1400011d4` (same-rate READ, 189-509):

```c
// Available frames: modular (WritePos - ReadPos)
iVar4  = param_1[6] - iVar5;              // WritePos - ReadPos
iVar14 = iVar4 + iVar2;                   // +frames if negative
if (-1 < iVar4) iVar14 = iVar4;
iVar2  = param_1[2];                      // header offset 400 (base of ring data)

// Required frames: request_bytes / (channels × bytes_per_sample)
// Bytes-per-sample from param_6: 8=1, 0x10=2, 0x18=3, 0x78c=4

// If required > available:
  param_1[0x61] = param_1[0x61] + 1;      // +0x184 underrun counter
  if (0x100000 bit of param_7 set)
    memset(output, silence, required_bytes);
  return -5 to -9;                         // error, do NOT advance ReadPos

// Success path (line 370):
LAB_14000178b: param_1[7] = iVar8; return 0;  // ReadPos advanced by frames consumed
```

`FUN_1400017ac` (SRC READ, 513-1077): same pattern, line 1017:
`*(int *)(local_110 + 0x1c) = iVar14` on success.

**Head-gap / hysteresis**: both functions have a drain-state gate (lines 454-478 / 1020-1046):

```c
uVar24 = 0xfffffff4;
if (iVar2 >> 1 <= iVar14) {               // available > frames/2
    uVar24 = 0;                           // OK flag
}
...
if (iVar2 >> 1 <= iVar14) {               // refill past half
    param_1[0x62] = 0;                    // clear drain flag (+0x188 = 0)
    goto LAB_1400013e4;                   // normal read path
}
// Else: return error and keep drain flag set
```

Effect: after an underrun sets +0x188 = 1, subsequent reads all fail (returning silence)
UNTIL the ring fills past 50% capacity. **This is a software hysteresis gate.**

**Answer** (high confidence):

- Two reads within 1ms return **different (advanced) data**. ReadPos moves per successful
  call. **Pull-driven consumption.**
- Once an underrun fires, the drain flag forces the ring to refill to >= frames/2 before
  reads resume returning audio. For Latency=7168 that's 3584 frames = ~74.6 ms of forced
  silence after any starvation event.
- There is NO "don't read within N frames of WritePos" guard on the read side. The only
  guard is on the WRITE side (`-2` at FUN_1400022b0:1143) to prevent WritePos overtaking
  ReadPos at exact boundary.

**Uncertain**: how often the 50% hysteresis actually fires in practice. Under normal
steady-state it shouldn't — but under scheduling jitter it can, and it's a plausible
"hundred-ms-silence" glitch source.

---

## 4. Pull-driven vs timer-paced

**Question**: Is ReadPos advanced by the DPC timer or by capture-read calls?

**Verified from code** — exhaustive grep of writes to ring +0x1c (`param_1[7]` or
`+ 0x1c\) =`):

| Line | Function | Context |
|------|----------|---------|
| 97   | FUN_1400010ec | utility (test helper, not in data path) |
| 177  | FUN_140001180 | format-resize clamp (edge case at start of read/write) |
| 370  | FUN_1400011d4 | **same-rate READ success** |
| 1017 | FUN_1400017ac | **SRC READ success** |
| 2369 | (format init) | one-time setup |
| 6511 | XMM context | unrelated (FP register save) |
| 12016 | (init path) | adapter setup |

`FUN_140005cc0` (DPC at 1ms) — **no write to +0x1c**.
`FUN_140006320` (position update at QPC delta) — **no write to +0x1c**.
`FUN_1400068ac` (per-tick frame calc) — **no write to +0x1c**.

DPC period is 1ms, from `FUN_1400065b8:5072`:
```c
ExSetTimer(DAT_140012fd8, 0xffffffffffffd8f0, 10000, 0);
// period = 10000 × 100ns = 1 ms
```

Inside `FUN_140005cc0` there's also a 10ms "slow tick" gate via magic constant
`-0x5c28f5c28f5c28f5` (compiler-generated ÷ 100). Once per 10 DPC fires (=10ms),
certain bulk loopback work runs (lines 4835-4856 loopback handoff, 4860-4934
packet-sized 10ms operation). So DPC has two cadences inside: **1ms (every call)**
+ **10ms (every 10th call, for loopback bulk transfer)**.

**Answer** (high confidence):

- **Pull-driven.** ReadPos is ONLY advanced by the reader functions.
- The DPC TRIGGERS reads (via `FUN_140006adc` → `FUN_1400011d4`) as part of moving data
  from ring to DMA shadow, but the amount advanced per tick equals the bytes the reader
  actually consumed, not a fixed playout quota.
- Net: pacing is "consume at the rate the OS-visible DMA cursor demands".
- Within a single DPC tick, multiple reads DO each get fresh data (no minimum-interval
  gate on the reader). But in `FUN_140006320` there IS an 8-frame minimum delta
  (line 4968: `if (7 < iVar7)`) before the position update runs — that's a
  lower-bound on how often the position-update path fires, not on how often reads
  happen.

**Uncertain**: the exact trigger topology of WASAPI capture read → kernel callback →
FUN_140006adc is a PortCls-layer detail I did not fully unwind. The observation that
FUN_140006adc is called from BOTH `FUN_140006320` (on GetPosition delta) AND
`FUN_1400068ac` (per-tick) is what I can verify.

---

## 5. Actual lag

**Question**: With Latency=7168, when speaker writes at T, when does mic read it?

**Verified from code** — `FUN_140005910` SetState, lines 4510-4600:

- `param_2 == 3` (RUN): QPC baseline captured (+0xF8), counters cleared, timer started.
  **No pre-fill. No initial gate.** Ring is whatever FUN_1400039ac (ring-reset in
  FUN_140005910:4548 during Pause→Run) left it in: zeroed data, WritePos=0, ReadPos=0.
- `FUN_1400039ac` (2254-2272): zeros data region + resets counters. Then calls
  `FUN_140003968` (2227-2250), which at line 2246 zeroes both WritePos (+0x18) AND
  ReadPos (+0x1C) in one 8-byte write.

No code enforces "ReadPos must stay N frames behind WritePos". The only enforcement is
the writer's `-2` guard against overtaking, and the reader's hysteresis gate
(§3) that kicks in only after an underrun.

**Answer** (high confidence):

- **Actual lag is NOT pinned to 7168**. 7168 is **capacity**, not target.
- On RUN, both pointers start at 0. They chase each other per tick.
- **Typical steady-state lag**: whatever bulk the DPC loopback transfer dumps per 10ms
  cycle (~480 frames at 48k) plus small scheduling jitter — on the order of **10-20 ms**.
- **Post-underrun lag**: the hysteresis gate forces lag to jump to ~74.6 ms (frames/2)
  temporarily.
- **Worst-case lag**: 7168 - 2 frames = ~149 ms. At that point writes get rejected
  with +0x180 overflow counter incremented.

So the mental model "Latency = 7168 ≈ 149 ms between speaker and mic" is **wrong by
~10×** for typical operation.

**Uncertain**: whether VB maintains an explicit "fill level" target elsewhere (e.g.,
via the 10ms loopback bulk transfer always writing a fixed 480-frame chunk even when
the ring is fuller). I did not trace the exact chunk size of the loopback transfer at
FUN_140005cc0:4902 / FUN_1400022b0 call; it is `iVar16 = iVar20 * 0x40` where `iVar20 =
SR / 100`, so 480 at 48k — yes, 10ms chunks.

---

## 6. Counter / cursor semantics

**Verified from code** — `FUN_140006320:4942-5044`, `FUN_140005910:4527-4600`,
`FUN_1400039ac:2254-2272`:

**Stream struct** (base = adapter/stream object, not ring):

| Offset | Purpose | Evidence |
|---|---|---|
| +0xB0 | Shadow DMA buffer pointer | FUN_140005634:4412, FUN_140006adc:5399 |
| +0xB8 | Paused/stopped flag | FUN_140006adc:5298 (`== 0` = running) |
| +0xD0 | DMA circular position (bytes, wraps at +0xA8 = DMA size) | FUN_140006320:5014-5018 |
| +0xD8 | Previous DMA position (for delta / hw-pos query) | 5013 |
| +0xE0 | **Monotonic byte counter A** (reported via GetPosition) | 5038 |
| +0xE8 | **Monotonic byte counter B** (presentation position) | 5037 |
| +0xF8 | QPC at RUN start | FUN_140005910:4577 |
| +0x100 | QPC frequency (performance counter denom) | 4959 |
| +0x168 | Spinlock (`KeAcquireSpinLockRaiseToDpc`) | 3199, 4740 |
| +0x180 | QPC reference for elapsed-time calc | FUN_140006320:4963 |
| +0x188 | Saved QPC from last DPC (read by GetPosition) | FUN_140005cc0:4741, FUN_140004598:3200 |
| +0x198 | Last elapsed-frame snapshot (for 8-frame gate delta) | 4967 |
| +0x1A0 | ...link field, reset to +0x70 at RUN | FUN_140005910:4589 |
| +0x1B0 | Next-due QPC deadline for FUN_1400068ac per-tick | 5213-5214 |
| +0x1B8 | Per-tick frame advance accumulator | 5039 |
| +0x1BC | Per-tick **byte count** (fed to FUN_140006adc/FUN_140005634 as param_2) | 5208 |
| +0x1C0 | Per-tick **frame count** (48 @ 48k) | 5207 |

**Ring struct** (separate base, global):

| Ring offset | Purpose |
|---|---|
| +0x180 | Write-overflow counter (incremented on reject, never decremented) |
| +0x184 | Read-underrun counter |
| +0x188 | Drain boolean flag (post-underrun hysteresis state) |

**Answer** (high confidence):

- **Overflow** at ring+0x180 = **write rejected** (data not written, error returned). No
  silent overwrite. The writer returns `0xfffffffa`/`0xfffffffd` and the caller (speaker
  DMA handler at FUN_140005634:4469-4472) just increments its own error counter
  (+0x24 of puVar3) and drops the input.
- **Underrun** at ring+0x184 = **read failed**. If the mic-path flag `0x100000` is set in
  `param_7` (it always is in FUN_140006adc: 5308,5350,5391), the output buffer is
  zero-filled. Otherwise caller gets an error and no data.
- **Drain flag** at ring+0x188 = hysteresis state. `1` = drained; stays drained until
  ring refills past 50%.

Stream-struct +0xE0, +0xE8 = monotonic counters returned via GetPosition
(FUN_140004598:3208-3211). These never decrement — they are the "total bytes consumed /
presented since RUN".

---

## 7. Anti-glitch mechanism ranking

| Rank | Mechanism | Weight | Evidence |
|------|-----------|--------|----------|
| **A** | Large internal ring (7168-frame / ~149ms capacity) | **Primary** | Absorbs OS scheduling jitter. Without this, any 10ms DPC slip = underrun. |
| **B** | Post-underrun 50% hysteresis gate | Major | Prevents rapid glitch train. Pays back as ~74ms silence stretch on recovery. Unique to VB — not in typical WDM drivers. |
| **C** | Hard-reject write on overflow | Major | No silent overwrite; preserves already-queued audio. Glitches manifest as gaps, not pops. |
| **D** | Silence-fill on underrun (mic path) | Moderate | Consumer never gets stale data. |
| **E** | 1ms DPC cadence + 10ms slow-tick gate | Moderate | Fast response to state changes; bulk loopback at 10ms chunks aligned to WASAPI's preferred period. |
| **F** | INT32 ~19-bit normalized format | Moderate | 13-bit headroom prevents SRC accumulator overflow. Silent corruption if absent. |
| **G** | WDM/PortCls DMA cursor model | Structural | Enables arbitrary-frame pulls, which ACX 2-slot does NOT natively offer. |
| **H** | KeFlushQueuedDpcs on Pause | Structural | FUN_140005910:4546 — prevents torn-state on state transition. |
| **I** | Position recalculated on GetPosition query | Structural | FUN_140004598:3204-3206 ensures OS-visible position is fresh, not stale. |
| **J** | Single global ring (not per-stream) | Structural | Eliminates cross-stream sync complexity. |

I rank A as the single biggest factor: with 149ms of capacity, almost any reasonable jitter
is absorbed. B is the second differentiator vs. naive ring designs — it explicitly
converts "N small glitches" into "one larger silence", which users perceive as cleaner.
C is the correctness foundation (no data corruption).

---

## 8. Drop policy

**Question**: If capture is not read in time, what happens?

**Verified from code** — writers in FUN_1400022b0:1137-1143,1251 and FUN_1400026a0:1412-1415:

```c
// FUN_1400022b0 (same-rate write):
iVar5  = param_1[7] - iVar10;                // ReadPos - WritePos
iVar14 = iVar5 + iVar2;                      // wrap
if (0 < iVar5) iVar14 = iVar5;
iVar14 = iVar14 + -2;                        // 2-frame guard
...
if (iVar14 <= (int)uVar17) {                 // required > free
    uVar6 = 0xfffffffa;
    goto LAB_1400025c6;                      // param_1[0x60]++ (=+0x180), return error
}

// FUN_1400026a0 (SRC write):
if (iVar23 + -2 < (int)(local_130 * iVar2) / (param_4 / iVar24) + 1) {
    *(int *)(param_1 + 0x180) = *(int *)(param_1 + 0x180) + 1;
    return 0xfffffffd;                       // hard reject
}
```

**Answer** (high confidence):

- The writer BLOCKS (back-pressures), it does NOT overwrite. Incoming producer data is
  DROPPED, overflow counter increments, error returned.
- **Old (unread) audio is preserved**. New render-side audio is lost. Caller
  (FUN_140005634:4469) discards the return value — the render client does NOT find out.
- So: "drop NEWEST producer data" policy, not "drop oldest unread". From a call-quality
  standpoint this produces a capture-side **silence/gap** rather than a **pop/crackle**.
  Empirically cleaner.

**Uncertain**: whether the speaker miniport's WaveRT position-reporting path lies to the
render client (claiming bytes were consumed) when the underlying ring-write was rejected.
In FUN_140005634:4469-4478 the return of FUN_1400022b0 is discarded; only a local counter
(+0x24) is bumped. So the render client very likely thinks its data was accepted, and
continues queuing — which is correct behavior for not stalling the render pipeline.

---

## 9. Internal format: 24-bit → ~19-bit INT32

**Question**: In FUN_1400022b0, how is 24-bit input stored?

**Verified** — FUN_1400022b0:1184-1189 (24-bit / 0x18 case):

```c
puVar9 = (undefined2 *)((longlong)param_2 + 1);
*(int *)(ring_slot) =
    (int)((uint)CONCAT21(*puVar9, *(undefined1 *)((longlong)puVar9 + -1)) << 8) >> 0xd;
```

Decoding the 3-step conversion:
1. `CONCAT21(*puVar9, byte)` assembles 3 bytes LE into a 24-bit value in a 32-bit int
   (high byte = 0).
2. `<< 8`: places the 24-bit value in bits 31..8 of a 32-bit int. Bit 31 = original bit
   23 = sign bit.
3. `>> 0xd` (arithmetic right-shift by 13): brings the value down by 13 bits, preserving
   sign. Net range: `[-2^(24+8-13-1), +2^(24+8-13-1) - 1] = [-262144, 262143]` — **19-bit
   signed range**.

Other input widths:
- 8-bit (FUN_1400022b0:1159): `(byte - 0x80) * 0x800` → `[-128,127] × 2048 = [-262144,260096]`
  = 19-bit signed.
- 16-bit (FUN_1400022b0:1268): `(short)x << 3` → `[-32768,32767] × 8 = [-262144,262136]`
  = 19-bit signed.
- INT32/float 0x78c (FUN_1400022b0:1227): direct copy, no scaling. Ring holds raw 32-bit
  value.

**Why 19-bit?** FUN_1400017ac SRC math (e.g., line 612 / 753):
```c
iVar7 = (iVar6 * iVar5) / local_128;
*piVar1 = *piVar1 + iVar7;
```
With `iVar5` in 19-bit range and `iVar6` up to the SR ratio numerator (≤ 300 for /100
ratios), `iVar6 * iVar5` stays within `2^18 × 300 ≈ 2^26.2`. Multiple SRC stages can
accumulate, but stay well under INT32 overflow (`2^31`). If samples were stored as full
24-bit (2^23), intermediate `2^23 × 300 ≈ 2^31.2` would overflow. So **19-bit
normalization is deliberate accumulator headroom**.

**Answer** (high confidence):

- All integer input formats (8/16/24-bit) are normalized to **~19-bit signed INT32**
  before ring storage.
- This is a deliberate design choice giving SRC 13 bits of headroom.
- The float/INT32 path (0x78c) bypasses this normalization — raw bits stored. Caller
  is expected not to mix.

**Uncertain**: whether the 0x78c path is ever used for the adapter ring in production
(vs. test). The INF advertises 24-bit as default, so most real streams go through the
<< 8 >> 13 path. But if a WASAPI exclusive client requests float, it would store
raw-float bits, and the SRC math on those would be incorrect (interpreting float bit
patterns as INT32 math). This looks like a latent bug, but likely unreachable in
practice because the format negotiation forces 24-bit.

---

## 10. Design for AO Cable V2 (ACX 2-slot)

### What VB does that's WDM/PortCls-specific — do NOT copy verbatim

1. **Arbitrary-frame DMA cursor polling**. VB reads the DMA circular position at
   every tick and pulls "whatever new bytes appeared since last tick". ACX uses
   notification-based 2-slot semantics — you're told when half A or half B is ready,
   not a continuous cursor. Trying to fake a 1ms cursor over ACX's 10ms notification
   will just re-sample the same half.

2. **1ms DPC timer**. VB runs ExAllocateTimer at 1ms for responsiveness. ACX circuits
   have built-in notification cadence; an additional 1ms timer on top is redundant and
   risks double-firing data moves.

3. **Shadow DMA region at +0xB0**. VB copies data into both the ring AND a separate DMA
   region for a second consumer. ACX exposes `ACX_STREAM_BUFFER` (KsLinearBuffer under
   the hood), not raw DMA regions. No equivalent shadow needed.

### What VB does that AO V2 SHOULD mirror

1. **INT32 ~19-bit normalized ring format**. Identical semantics work on ACX: 4 bytes per
   sample, left-shift on write, right-shift on read, 13-bit SRC headroom.

2. **Hard-reject on writer overflow + counter at struct+0x180 equivalent**. Increment,
   return error, drop incoming producer data. Do NOT overwrite existing queued audio.
   This is the single most important glitch-prevention invariant from VB.

3. **Post-underrun hysteresis gate (50% refill required)**. Trade one long silence
   stretch (~75ms at 7168-frame ring) for avoiding glitch trains. Mirror in AO as
   ring-state machine: RUNNING → DRAINED (underrun fired) → RUNNING (ring ≥ 50%).

4. **ReadPos/WritePos in frames, single wrap modulus, -2 writer guard**. 2-frame guard
   prevents writer catching reader at exact boundary. Essential for correctness.

5. **KeFlushQueuedDpcs on Pause** (or ACX equivalent: AcxStreamDispatch finalization or
   wait-for-outstanding-callbacks) before ring reset. Prevents use-after-free on
   state transitions.

6. **Overflow / Underrun / Drained counters exposed via IOCTL**. These ARE the diagnostic
   signal for call-quality regression. Without them you cannot distinguish "ring is too
   small" from "SRC is broken" from "consumer is slow". `test_stream_monitor.py` should
   mirror all three.

7. **Position recalculated on GetPosition query** (FUN_140004598 → FUN_140006320 pattern).
   ACX's position-reporter callback should extrapolate position from QPC-delta × SR at
   query time. Do NOT return a cached value updated only once per notification.

8. **8-frame minimum delta gate** (FUN_140006320:4968 `if (7 < iVar7)`). Skip sub-sample
   noise on position updates. Easy to mirror.

### Corrections to the "private FIFO + target latency + 10ms paced return" shorthand

Two misleading terms:

- **"Target latency"** implies VB actively pins buffering at ~7168 frames. It does NOT.
  Steady-state buffering is ~480-1000 frames (10-20 ms). 7168 is a **capacity gate**.
  Only post-underrun does the hysteresis gate push lag to ~3584 frames (74ms) temporarily.

- **"10ms paced return"** is partially correct. The DPC slow-tick is 10ms and paces
  the speaker→mic loopback bulk transfer (FUN_140005cc0:4902-4928 writes 480-frame
  chunks). But the mic-side WASAPI read is PULL-DRIVEN, not 10ms-paced. An app calling
  IAudioCaptureClient::GetBuffer gets whatever is ready at that instant.

Better VB characterization:

> **Capacity-gated pull FIFO** (max 149ms @ 48k/16ch). Producer fed by 10ms-paced
> loopback DPC; consumer pull-driven at arbitrary cadence. Overflow = hard-reject new
> producer data; underrun = silence-fill output + require 50% refill before resuming
> reads. Format normalized to 19-bit INT32 for SRC headroom. Position recomputed from
> QPC on each GetPosition query.

### Single-SRC-function claim — CORRECTION

AO V2 design principle #5 says "Single SRC function — direction flag for write/read,
symmetric behavior". This does NOT match VB: VB has **two separate SRC functions**,
FUN_1400026a0 (WRITE SRC, 4808 bytes) and FUN_1400017ac (READ SRC, 2817 bytes). They
share ring struct layout but not code. AO can still unify into one function as a
code-reuse choice, but don't claim it matches VB structure.

### AO V2 translation summary

| VB mechanism | Keep | Translate | Drop |
|---|---|---|---|
| 19-bit INT32 format | ✓ | — | — |
| Hard-reject overflow | ✓ | — | — |
| Post-underrun 50% hysteresis | ✓ | — | — |
| 2-frame writer guard | ✓ | — | — |
| Frame-indexed positions | ✓ | — | — |
| Position recalc on query | ✓ | — | — |
| 8-frame delta gate | ✓ | — | — |
| Counters at struct offset | ✓ | — | — |
| KeFlushQueuedDpcs on Pause | ✓ (use ACX equivalent) | — | — |
| 10ms loopback bulk | — | Use ACX notification period | — |
| 1ms DPC timer | — | Not needed | ✓ drop |
| Shadow DMA +0xB0 | — | Not applicable | ✓ drop |
| Separate Latency reg + max reg | — | Single "BufferFrames" param | ✓ drop extras |

**Uncertain**: my ACX characterization comes from the driver model as I recall it;
I did not cross-verify against the ACX sample drivers in this analysis. If AO V2 uses
non-standard ACX buffering (e.g., a ring of small notifications), some of the above
may not apply.

---

## Summary of corrections to prior docs

| Claim in prior doc | My verification | Verdict |
|---|---|---|
| Latency unit = frames | frames | **Agree** |
| Stride = 4 bytes INT32 | 4 bytes INT32 | **Agree** |
| Ring indexed in frames | frames (channels as inner index) | **Agree** |
| Overflow at ring+0x180 | +0x180 | **Agree** |
| **Underrun at ring+0x188** | **+0x184; +0x188 is drain FLAG** | **Corrected** |
| Ring struct offsets (+0x00..+0x24) | See §0 corrected map | **Several fields renamed/reordered** |
| DPC = 1ms | 1ms (plus 10ms slow-tick gate inside) | **Refined** |
| Timer-paced playout cursor | **Pull-driven; DPC never writes ReadPos** | **Corrected** |
| 7168 = target latency | **7168 = capacity max; typical lag 10-75ms** | **Corrected** |
| Post-underrun hysteresis (50% refill) | **Verified at FUN_1400011d4:454-478, FUN_1400017ac:1020-1046** | **New finding not in prior docs** |
| 19-bit normalization | deliberate SRC headroom | **Verified** |
| Drop policy (overflow) | hard-reject NEW producer data | **Verified; made explicit** |
| KeFlushQueuedDpcs on Pause | verified | **Verified** |
| Position-on-query | verified at FUN_140004598:3204-3206 | **Verified** |
| 8-frame minimum gate | verified at FUN_140006320:4968 | **Verified** |
| Single SRC with direction flag | **Two separate SRC functions** (FUN_1400026a0 WRITE 4808B; FUN_1400017ac READ 2817B) | **Corrected** |

**Highest-impact corrections for the AO V2 plan:**

1. The anti-glitch guarantee comes primarily from **capacity + hard-reject + post-underrun
   hysteresis**, NOT from "target latency = 7168". Mirroring VB means implementing all
   three; just copying the 7168 into a BufferFrames field without the hysteresis gate
   will produce a driver that passes one-shot tests but degrades on first glitch.

2. The DPC is 1ms, not 10ms, and it is NOT a playout cursor. Implementing AO V2 with a
   fixed "advance ReadPos every 10ms" tick will NOT match VB behavior and will cause
   identical-data-twice bugs on fast WASAPI polls.

3. The ring layout has the "Underrun" counter at +0x184, not +0x188 as prior docs claim.
   Any IOCTL diagnostic that was reading +0x188 as an underrun count has been reading
   a 0/1 drain flag instead.

---

## Files cited

- `results/ghidra_decompile/vbcable_all_functions.c` — all line numbers above
- `results/ghidra_decompile/vbcable_function_index.txt` — function map
- `results/vbcable_pipeline_analysis.md` — prior doc; several claims corrected above
- `results/vbcable_disasm_analysis.md` — prior doc; ring struct layout corrected above
