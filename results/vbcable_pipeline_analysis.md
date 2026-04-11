# VB-Cable Full Pipeline Analysis (Ghidra Decompilation)

Based on complete Ghidra decompilation of `vbcable_a64.sys`: 297 functions, 12096 lines.

---

## 1. Data Flow Diagram

```
                        ExSetTimer (10000 ticks ~1ms)
                               |
                        FUN_140005cc0 (DPC Timer Callback)
                               |
       +-----------------------+-----------------------+
       |                                               |
  Speaker (render) stream               Mic (capture) stream
  (param_1+0xb8 == 0)                   (param_1+0xa4 != '\0')
       |                                               |
  FUN_1400068ac                                   FUN_1400068ac
  (time-based frame calc)                         (time-based frame calc)
       |                                               |
  FUN_140006320                                   FUN_140006320
  (position update + dispatch)                    (position update + dispatch)
       |                                               |
  FUN_140005634 (Speaker ReadBytes)          FUN_140006adc (Mic WriteBytes)
       |                                               |
  Copy DMA buf -> scratch buf              Ring read -> scratch -> DMA buf
       |                                               |
  FUN_1400022b0 (ring WRITE)               FUN_1400011d4 (ring READ)
  [same-rate fast path]                    [same-rate fast path]
       |                                               |
  FUN_1400026a0 (ring WRITE               FUN_1400017ac (ring READ
   with SRC when rates differ)             with SRC when rates differ)
       |                                               |
       +-----> INT32 Ring Buffer <-----+
               (frame-indexed, 4 bytes/sample)
               Init: FUN_140001008
               Reset: FUN_1400039ac
               Available: FUN_14000112c
```

**Key principle**: Speaker writes INTO the ring, Mic reads FROM the ring. Both use the same ring buffer. The ring stores INT32 (4 bytes per sample), not packed PCM bytes.

---

## 2. DPC Timer -- Tick Interval, Frame Calculation

### 2.1 Timer Creation (FUN_1400065b8)

```c
// ExAllocateTimer with FUN_140005cc0 as callback
DAT_140012fd8 = ExAllocateTimer(FUN_140005cc0, 0, 4);
if (DAT_140012fd8 != 0) {
    ExSetTimer(DAT_140012fd8, 0xffffffffffffd8f0, 10000, 0);
}
```

- **Due time**: `0xffffffffffffd8f0` = -10000 (relative, 100ns units) = **1 ms initial delay**
- **Period**: `10000` (100ns units) = **1 ms period**
- **Tolerance**: `0` (no coalescing tolerance)
- Uses `ExAllocateTimer`/`ExSetTimer` (high-resolution timer API, Windows 8.1+)

### 2.2 Timer Drift Correction

FUN_140005cc0 implements a **drift-corrected tick system**. The driver tracks:
- `puVar9+0x298`: last tick QPC timestamp
- `puVar9+0x2A0`: tick counter (wraps at 100)
- `puVar9+0x2A8`: next expected tick time

The magic constant `-0x5c28f5c28f5c28f5` in the 128-bit multiply is the magic number for dividing by ~64 (specifically `SEXT816(-0x5c28f5c28f5c28f5) * value` then `>> 6` yields `value * 63/64`). This implements **phase correction**: the next tick deadline is `base + (count * interval) * 63/64`, which subtly catches up if ticks are late, preventing cumulative drift.

Every 100 ticks, the base timestamp resets (`lVar8 = lVar8 + local_a8` where `local_a8` is the QPC frequency).

### 2.3 Frame Count Calculation (FUN_1400068ac / FUN_140006320)

FUN_1400068ac calculates how many bytes to process per tick:

```c
// param_1+0x1bc = bytes per tick (pre-calculated)
// param_1+0x1c0 = frames per tick (pre-calculated)
uVar1 = *(uint *)(param_1 + 0x1bc);  // bytes this tick
lVar8 = *(int *)(param_1 + 0x1c0);   // frames this tick
```

FUN_140006320 does the actual time-to-frames conversion using KeQueryPerformanceCounter:

```c
// Convert QPC to 10MHz time
uVar3 = (param_2 >> 0x20) * 10000000;
lVar1 = (uVar3 / uVar6 << 0x20) +
        ((param_2 & 0xffffffff) * 10000000 + (uVar3 % uVar6 << 0x20)) / uVar6;

// Elapsed time * sample_rate / 10000000
lVar4 = (lVar1 - *(longlong *)(param_1 + 0x180)) * *(uint *)(param_1 + 0x8c);
// Magic constant -0x29406b2a1a85bd43 with >> 23 = divide by 10000000
uVar6 = (lVar4 >> 0x17) - (lVar4 >> 0x3f);
```

The magic number `0x29406b2a1a85bd43` with `>> 23` implements division by 10,000,000 (converting 100ns time units to seconds, then multiplied by sample rate to get frames).

**Frame gating**: Only processes if `iVar7 > 7` (minimum 8 frames). This prevents processing noise-level tiny increments.

**Overflow protection**: If `uVar5 >= sampleRate * 128`, the driver resets the timing baseline rather than processing a massive burst:
```c
if (*(uint *)(param_1 + 0x8c) << 7 <= uVar5) {
    *(undefined8 *)(param_1 + 0x198) = 0;
    *(longlong *)(param_1 + 0x180) = lVar1;  // reset baseline
}
```

**DMA buffer overrun protection**: If computed bytes > half the DMA buffer, the driver increments an overrun counter and returns without processing (the half-buffer check):
```c
if (uVar5 >> 1 < uVar8) {
    // increment overrun counter at puVar2+0x158 or puVar2+0x180
    // skip processing
}
```

---

## 3. Speaker DMA -> Ring (FUN_140005634 = ReadBytes equivalent)

FUN_140005634 is called for **speaker (render)** streams. This reads from the WaveRT DMA buffer and writes into the ring.

### 3.1 DMA Buffer Copy

```c
// Copy from DMA circular buffer into scratch buffer (param_1+0x178)
uVar7 = *(int *)(param_1 + 0xa8) - param_3;  // space to end of DMA buffer
if (param_2 < uVar7) uVar7 = param_2;

// Copy first part
FUN_140007680(*(param_1 + 0x178),    // scratch buffer dest
              param_3 + *(param_1 + 0xb0),  // DMA buf + offset
              uVar7);

// Copy wrapped part if needed
if (uVar7 < param_2) {
    FUN_140007680(*(param_1 + 0x178) + uVar7,
                  *(param_1 + 0xb0),  // DMA buf start
                  param_2 - uVar7);
}
```

**Key insight**: VB-Cable first copies the DMA region to a linear scratch buffer, then processes from that scratch buffer. This avoids dealing with wrap-around during SRC.

### 3.2 Ring Write Path

Two paths based on `param_1+0xb8` (stream type flag):

**Path A (b8 == 0, no mixing)**:
```c
// Direct ring write from scratch buffer
FUN_1400022b0(*(param_1 + 0x170),     // ring buffer struct
              *(param_1 + 0x178),      // scratch buffer
              param_2,                  // byte count
              *(param_1 + 0x8c),       // sample rate
              *(ushort *)(param_1 + 0x88), // channel count
              *(ushort *)(param_1 + 0x86), // bits per sample
              1);                       // direction = write
```

**Path B (b8 != 0, mixing enabled)**:
1. Apply per-channel volume (FUN_1400051a8) if mixing flag set
2. Write to shared ring via FUN_1400022b0
3. Peak meter calculation via FUN_140004f2c

### 3.3 Same-Rate Ring Write (FUN_1400022b0)

When speaker rate == ring rate, FUN_1400022b0 does a **format-converting write**:

```c
// 8-bit: (byte - 0x80) * 0x800 -> INT32 in ring
// 16-bit: (short) << 3 -> INT32 in ring
// 24-bit: assemble 3 bytes, << 8, >> 13 -> INT32 (~19-bit)
// 32-bit (0x78c): direct INT32 copy (or memcpy for aligned)
```

The converted INT32 samples are written into the ring at `WritePos`, which wraps around:
```c
iVar10 = iVar10 + 1;  // advance WritePos
if (iVar10 >= iVar2)   // if past capacity
    iVar10 = 0;         // wrap to 0
```

**Overflow handling**: If `iVar14 <= (int)uVar13` (available space < frame count), the function increments `param_1[0x60]` (overflow counter) and returns an error code. **The write is rejected entirely** -- no partial write, no data corruption.

### 3.4 Different-Rate Ring Write (FUN_1400026a0)

When rates differ, FUN_1400026a0 handles SRC + ring write atomically:

1. **GCD ratio calculation**: Try dividing both rates by 300, then 100, then 75
2. **Capacity check before writing**: `if (iVar23 + -2 < (int)(local_130 * iVar2) / (param_4 / iVar24) + 1)` -- if not enough ring space, increment overflow counter at `+0x180` and return `-3`
3. **Linear interpolation** using `local_c8[]` / `local_88[]` accumulators (16-channel max)
4. **Multi-rate downsampling** path for extreme ratios (2x, 4x, 8x, 16x)

---

## 4. Ring -> Mic DMA (FUN_140006adc = WriteBytes equivalent)

FUN_140006adc is called for **mic (capture)** streams.

### 4.1 Ring Read Path

Two paths based on stream pairing:

**Path A (b8 == 0, direct pairing)**:
```c
// Check available data in ring
uVar3 = FUN_1400011d4(*(param_1+0x170), ..., param_2, rate, channels, bits, 1);
```

If ring has less data than requested bytes, the function **zeroes the scratch buffer**:
```c
if (...available < param_2) {
    iVar5 = 100;
    FUN_140007940(*(param_1+0x178), 0, param_2);  // fill with silence
}
```

**Path B (b8 != 0, mixing/monitoring)**:
1. Read from speaker-side ring (FUN_1400011d4 or FUN_140004068 ring)
2. Apply peak metering
3. Write result to scratch buffer

### 4.2 Same-Rate Ring Read (FUN_1400011d4)

Reverse of write -- converts INT32 back to target format:

```c
// INT32 -> 8-bit: (value >> 0xb) + 0x80
// INT32 -> 16-bit: value >> 3
// INT32 -> 24-bit: value << 5, write 3 bytes
// INT32 -> 32-bit: direct copy
```

ReadPos advances per frame consumed and wraps at capacity.

**Underrun handling**: `param_1[0x62]` flags underrun state. When ring data < half capacity (`iVar2 >> 1 > iVar14`), the function enters a "recovery" mode where it returns the underrun error `-12` and optionally fills output with silence/center (0x80 for 8-bit, 0 for 16/24-bit).

### 4.3 DMA Buffer Write-Back

After ring read, data is copied from scratch buffer into the DMA circular buffer:
```c
uVar6 = *(int *)(param_1 + 0xa8) - param_3;
if (param_2 < uVar6) uVar6 = param_2;
FUN_140007680(param_3 + *(param_1 + 0xb0),  // DMA buf + offset
              *(param_1 + 0x178),             // scratch buffer
              uVar6);
if (uVar6 < param_2) {
    FUN_140007680(*(param_1 + 0xb0),          // DMA buf start
                  *(param_1 + 0x178) + uVar6,
                  param_2 - uVar6);
}
```

---

## 5. Position Tracking

### 5.1 Position Update (FUN_140006320)

Each DPC tick updates these position fields:

```c
// Monotonic byte counters (never wrap):
*(param_1 + 0xe8) += uVar6;   // TotalBytesTransferred (for PlayPosition)
*(param_1 + 0xe0) += uVar6;   // TotalBytesProcessed (for WritePosition)
*(param_1 + 0x1b8) += uVar6;  // BytesThisTick (for event notification)

// DMA circular position (wraps at buffer size):
*(param_1 + 0xd8) = *(param_1 + 0xd0);   // PreviousPosition = CurrentPosition
*(param_1 + 0xd0) += uVar6;               // CurrentPosition += bytes
if (*(param_1 + 0xa8) <= *(param_1 + 0xd0)) {
    *(param_1 + 0xd0) -= *(param_1 + 0xa8);  // wrap at buffer size
}
```

### 5.2 Position Reporting (FUN_140004598 / FUN_140004664)

FUN_140004598 reports position under spinlock (`param_1+0x168`):

```c
uVar1 = KeAcquireSpinLockRaiseToDpc(param_1 + 0x168);
uVar2 = *(ulonglong *)(param_1 + 0x188);  // cached QPC from DPC
if (uVar2 == 0) {
    uVar2 = KeQueryPerformanceCounter(0);  // fallback
}
// If stream is running (state==3) and not paused (b8==0):
if (*(int *)(param_1 + 0xbc) == 3 && *(int *)(param_1 + 0xb8) == 0) {
    FUN_140006320(param_1, uVar2, 3);  // update position NOW
}
*param_2 = *(param_1 + 0xe0);  // PlayPosition (total bytes)
*param_3 = *(param_1 + 0xe8);  // WritePosition (total bytes)
*param_4 = uVar2;              // QPC timestamp
KeReleaseSpinLock(param_1 + 0x168, uVar1);
```

FUN_140004664 converts to buffer-relative position:
```c
// Convert total bytes to DMA-buffer-relative position
uVar3 = (*(param_1+0x148+4) * local_res8[0]) / *(param_1+0x148+8);
*param_2 = uVar3;  // position within DMA buffer
```

**Critical pattern**: When position is requested, VB-Cable **recalculates position to the current moment** (calls FUN_140006320 with current QPC). This means the position is always up-to-date, not stale from the last DPC tick.

### 5.3 Loopback Event Notification

For loopback endpoints (param_1+0x164 flag), VB-Cable detects when the write position has caught up to the notification position:
```c
if (uVar3 % uVar10 == *(uint *)(param_1 + 0x7c)) {
    *(param_1 + 0x165) = 1;  // mark event fired
    // Report position via portcls event
}
```

---

## 6. Format/Rate Handling

### 6.1 Supported Rates

From IOCTL handler FUN_140003aa0, IOCTL `0x222048`:
```c
// Validated rates:
32000, 44100, 48000, 88200, 176400, 192000
// If none match, defaults to 96000
```

### 6.2 Ring Buffer Initialization (FUN_140016380 -- adapter init)

```c
// Read registry for buffer size, default 0x1c00 (7168) frames
// Min: 0x400 (1024), Max: 0x8000 (32768) frames
// Read registry for latency
// Read registry for sample rate, default 96000

// Create ring buffers:
DAT_140012e60 = FUN_140001000(uVar7 + 8, 0x10, uVar9);
//                              ^frames+8  ^channels ^rate
```

FUN_140001008 (ring constructor):
```c
param_1[0] = param_1 * param_2;           // totalSamples = frames * channels
*(param_1 + 0xc) = param_1;                // frames
*(param_1 + 2) = param_2;                  // channels  
*(param_1 + 4) = param_3;                  // sampleRate
*(param_1 + 5) = totalSamples * 4;         // stride in bytes (INT32)
*(param_1 + 0x2c) = totalSamples * 8;      // double stride
*(param_1 + 1) = 400;                      // ring data offset from struct base
*(param_1 + 0x24) = totalSamples * 4 + 400; // allocated ring size
```

**Ring is embedded after struct at offset 400 bytes. Each sample is INT32 (4 bytes). Ring capacity = frames * channels * 4.**

### 6.3 Format Conversion Normalization

All paths normalize to ~19-bit signed integer:

| Input Format | Conversion to Ring INT32 |
|-------------|-------------------------|
| 8-bit unsigned | `(byte - 0x80) * 0x800` = shift left 11 |
| 16-bit signed | `short << 3` = shift left 3 |
| 24-bit signed | `(3bytes << 8) >> 13` = net shift left ~6 |
| 32-bit (0x78c) | Direct copy or native INT32 |

All formats produce values in approximately the same ~19-bit dynamic range (-262144 to +262143), preventing overflow during SRC interpolation.

### 6.4 SRC Rate Ratio

GCD-based ratio using divisors 300, 100, 75:
- 44100/48000: GCD=300, ratio=147:160
- 48000/96000: GCD=100, ratio=480:960
- 44100/96000: GCD=300, ratio=147:320

If no GCD matches, returns error `-486` (0xFFFFFE1A).

---

## 7. Stream State Management

### 7.1 State Machine (FUN_140005910)

```c
// State values (param_1+0xb4):
// 0 = STOP     2 = PAUSE     3 = RUN
```

**STOP (param_2 == 0)**:
```c
KeAcquireSpinLockRaiseToDpc();
*(param_1 + 0xf0) = 0;          // clear all position/timing state
*(param_1 + 0xe8) = -1;
*(param_1 + 0xec) = -1;
*(param_1 + 200) = 0;
*(param_1 + 0xd0) = 0;
*(param_1 + 0xd8) = 0;
*(param_1 + 0xe0) = 0;
*(param_1 + 0x1b0) = 0;
*(param_1 + 0x74) = 0;
*(param_1 + 0x15c) = 0;
KeReleaseSpinLock();
```

**PAUSE (param_2 == 2)**:
```c
if (previousState > 2) {  // was running
    // Remove from timer list
    FUN_14000669c(param_1);
    *(param_1 + 0x60) = 0;    // clear timer handle
    KeFlushQueuedDpcs();       // wait for any pending DPCs to complete
    // Reset ring buffer
    FUN_1400039ac(*(param_1 + 0x168));
    // Clear statistics
}
```

**RUN (param_2 == 3)**:
```c
uVar4 = KeQueryPerformanceCounter(&(param_1 + 0xf8));  // QPC + frequency
uVar1 = *(param_1 + 0xf8);  // QPC frequency
// Calculate timer period in 100ns units:
lVar6 = (uVar4 * 10000000 / uVar1);

*(param_1 + 0x108) = lVar6;   // period in 100ns
*(param_1 + 0x178) = lVar6;   // next tick time
*(param_1 + 0x1a0) = *(param_1 + 0x70);  // frames per tick = buffer size / tick

// Clear all position/timing state
*(param_1 + 400) = 0;
*(param_1 + 0x198) = 0;
*(param_1 + 0x110) = 0;
*(param_1 + 0x1a8) = 0;
*(param_1 + 0x180) = 0;
*(param_1 + 0x188) = 0;
*(param_1 + 0x1c8) = 0;

// Register with timer (adds to DPC list)
bVar2 = FUN_140004cf4(...);  // check if other side already running
if (!bVar2) {
    lVar6 = FUN_1400065b8(param_1);  // register with shared timer
    *(param_1 + 0x58) = lVar6;
}
```

### 7.2 Stream Registration

The shared timer (FUN_1400065b8) maintains a list of up to 9 active streams:
```c
// DAT_140012f90..DAT_140012fd0: array of 9 stream pointers
// DAT_140012f88: count of highest active slot
// DAT_140012f84: total active count
// DAT_140012fd8: timer handle (shared across all streams)
```

First stream to register creates the timer. Last stream to unregister deletes it with `ExDeleteTimer(handle, 1, ...)` (wait for completion).

---

## 8. IOCTL Interface (FUN_140003aa0)

| IOCTL Code | Function |
|-----------|----------|
| 0x222010 | Authentication challenge (random number generation) |
| 0x222014, 0x222018, 0x22201c | Authentication verify (anti-tamper) |
| 0x222024 | Get device status (0x70 or 0x1A0 bytes) |
| 0x222040 | Get version: returns 0x03030109 |
| 0x222044 | Get extended status (0x20 bytes from global state) |
| 0x222048 | Set/get sample rate (validated list) |
| 0x22204c | Set/get buffer size (min 0x30 frames) |
| 0x222050 | Get device info by index (0x70 or 0x1A0 bytes) |
| 0x222054 | Get per-device extended status |
| 0x222058 | Set mixing enable flag |
| 0x22205c | Set per-device buffer size |
| Other | Passed to PcDispatchIrp() |

---

## 9. Key Differences from AO Cable's Pipeline

| Aspect | VB-Cable | AO Cable |
|--------|----------|----------|
| **Ring sample format** | INT32 (4 bytes), ~19-bit precision | Packed 24-bit (3 bytes), full 24-bit range |
| **Ring indexing** | Frame-based (WritePos = frame index, wraps to 0) | Byte-based (ReadPos/WritePos in bytes) |
| **SRC algorithm** | Integer-ratio linear interpolation | 8-tap windowed sinc with 2048-entry coefficient table |
| **SRC location** | Unified: same function does read+SRC or write+SRC | Split: ConvertToInternal -> SrcConvert -> ConvertFromInternal -> LoopbackWrite |
| **Overflow handling** | Hard reject + counter, returns error code | Silent overwrite (advance ReadPos, log PushLossFrames) |
| **DMA buffer access** | Copy to linear scratch buffer first, then process | Process directly from circular DMA buffer |
| **Timer creation** | ExAllocateTimer/ExSetTimer (high-res, 1ms) | KeSetTimerEx with KeDpc (1ms) |
| **Drift correction** | 63/64 phase correction, rebase every 100 ticks | None (relies on timer accuracy) |
| **Frame minimum** | 8-frame minimum per tick (skip small increments) | Processes any nonzero amount |
| **DMA overrun guard** | Skip if computed bytes > half DMA buffer | No such guard |
| **Position freshness** | Recalculates to current QPC on every position query | Returns last DPC-calculated position |
| **MicSink dual-write** | None -- ring only | Speaker DPC pushes to both ring AND mic DMA |
| **Spinlock usage** | One lock per stream for position (`+0x168`) | Position lock + separate loopback lock (ordering risk) |
| **Mixing** | Built-in per-channel volume + metering in DPC | No mixing support |
| **Ring init** | Ring data embedded in struct at fixed offset (400) | Separate allocation, pointer-based |

---

## 10. Specific Code Patterns AO Cable Should Adopt

### 10.1 Scratch Buffer Before Processing (CRITICAL)

VB-Cable always copies the DMA region to a linear scratch buffer before any processing:
```
DMA circular buf --memcpy--> scratch buf --SRC/convert--> ring
```
AO Cable processes directly from the circular DMA buffer, which means wrap-around handling must be mixed into the conversion logic. This is error-prone. **Adopt the scratch buffer pattern.**

### 10.2 Hard Reject on Ring Overflow (CRITICAL)

VB-Cable checks capacity BEFORE writing and rejects entirely if insufficient space:
```c
if (iVar23 + -2 < required_frames) {
    *(param_1 + 0x180) += 1;  // overflow counter
    return -3;
}
```
AO Cable silently overwrites and advances ReadPos, causing discontinuity in the capture stream. **Switch to hard reject with counter.**

### 10.3 INT32 Ring Buffer (HIGH)

4-byte aligned samples eliminate all 3-byte alignment issues. The ~19-bit normalization means SRC interpolation never risks INT32 overflow. **Switch ring to INT32 samples.**

### 10.4 Frame Minimum Gate (MEDIUM)

VB-Cable only processes when >= 8 frames are pending. This prevents sub-sample timing noise from causing tiny irregular transfers:
```c
if (7 < iVar7) { // process }
```

### 10.5 Position-on-Query Freshness (HIGH)

When Windows queries position, VB-Cable recalculates to the current QPC timestamp *inside the position query handler*:
```c
if (state == RUN && !paused) {
    FUN_140006320(param_1, currentQPC, 3);  // update to NOW
}
*playPosition = ...; // return fresh value
```
AO Cable returns the position from the last DPC tick, which can be up to 1ms stale. For low-latency paths, this matters.

### 10.6 Timer Drift Correction (MEDIUM)

The 63/64 phase correction prevents cumulative drift. If a tick fires 0.1ms late, the next tick deadline compensates. AO Cable's timer relies purely on OS scheduler accuracy.

### 10.7 DMA Overrun Guard (MEDIUM)

VB-Cable skips processing if computed bytes exceed half the DMA buffer:
```c
if (uVar5 >> 1 < uVar8) {
    overrunCounter++;
    return;  // skip this tick
}
```
This prevents a single late tick from causing a massive burst that would wrap the DMA buffer.

### 10.8 KeFlushQueuedDpcs on Pause (LOW)

When pausing, VB-Cable calls `KeFlushQueuedDpcs()` to ensure no DPC is in-flight before resetting the ring buffer. This prevents a race where a DPC reads the ring after it has been zeroed.

### 10.9 Unified SRC Function (MEDIUM)

Using a single function for both write-SRC and read-SRC (with a direction parameter) guarantees symmetric behavior. AO Cable's split pipeline means bugs can exist in one direction but not the other.

---

## 11. Ring Buffer Struct Layout (Refined)

```
Offset  Size  Field
+0x00   4     SampleRate (actual, copied from init)
+0x04   4     SampleRate (requested, from param_3)
+0x08   4     RingDataOffset (signed, relative to struct base = 400)
+0x0C   4     Frames (capacity)
+0x10   4     ChannelCount * FrameCount (total INT32 slots)
+0x14   4     WrapBound (= FrameCapacity for modulo)
+0x18   4     WritePos (frame index, wrapping)
+0x1C   4     ReadPos (frame index, wrapping)
+0x20   4     InternalRate (for ratio computation)
+0x24   4     Valid flag / allocated size
+0x28   4     FrameStride (channels * 4)
+0x2C   4     DoubleStride (channels * 8)
+0x30   4     Allocated flag (1 = driver owns memory)
+0x34   4     SRC fractional accumulator
+0x38   64    SRC history per channel (16 * INT32)
+0xB8   4     SRC previous-sample accumulator
+0xBC   64    SRC previous samples per channel (16 * INT32)
+0x13C  64    SRC prev-sample state (extreme downsampling path)
+0x174  4     Feature flags (bit 1 = per-channel volume)
+0x178  4     Reserved
+0x17C  4     Reserved
+0x180  4     Overflow counter (write rejected)
+0x184  4     Underrun counter (SRC path)
+0x188  4     Underrun flag (same-rate read)
+0x190  400   Ring data starts here (INT32 array)
```

---

## 12. Summary: Why VB-Cable Sounds Clean

1. **No data corruption path**: overflow = reject, not overwrite
2. **Aligned ring**: INT32 (4 bytes) eliminates 3-byte alignment bugs
3. **Bounded precision**: ~19-bit normalized range prevents SRC overflow
4. **Linear scratch buffer**: DMA wrap-around isolated from processing
5. **Fresh position**: recalculated to current QPC on every query
6. **Drift-corrected timer**: 63/64 phase compensation
7. **Frame minimum gate**: prevents sub-sample jitter
8. **Single-path SRC**: one function, one direction flag, symmetric behavior
9. **No dual-write**: ring is the sole data path (no MicSink push race)
10. **Clean state transitions**: KeFlushQueuedDpcs on pause, full reset on stop
