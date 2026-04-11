# VB-Cable FUNC_26A0 Disassembly Analysis

## 1. Function Signature

    FUNC_26A0(rcx: struct*, rdx: source_data_ptr, r8d: byte_count, r9d: bits_per_sample)
    5th param [rbp+0x80]: channel_count (signed, movsxd r12)
    6th param [rbp+0x88]: target_bits_per_sample
    7th param [rbp+0x90]: direction_flag (0 = same-format copy path)

This is a **sample rate converter with integrated ring buffer write** -- the single largest function in VB-Cable driver. It handles reading PCM data from a source buffer, resampling it, and writing the result into an internal circular buffer.

---

## 2. Struct Layout at [rcx] (r15)

| Offset | Size | Name (inferred) | Evidence |
|--------|------|-----------------|----------|
| +0x08  | 4    | RingBufferOffset (signed, relative to struct base) | movsxd rax,[rcx+8]; add rax,r15 -> ring is embedded/adjacent to struct |
| +0x10  | 4    | FrameCapacity (max frames in ring) | Used as upper bound: cmp r12d,eax; cmovg ecx,eax |
| +0x14  | 4    | WrapBound / BufferFrameCount | mov ecx,[r15+0x14] used for write-pos wrap check |
| +0x18  | 4    | WritePos (frame index, wrapping) | lea rax,[r15+0x18] -> pointer stored, written back at end |
| +0x1C  | 4    | Available space / free frames | [r15+0x1c] - ebx = remaining space calculation |
| +0x20  | 4    | StridePerFrame (in int32 units) | Used as multiplier: imul eax,[rsp+0x20] to index ring |
| +0x24  | 4    | Valid flag or format identifier | Tested early: test eax,eax; jne (zero = error, return -1) |
| +0x34  | 4    | SRC fractional accumulator (persistent) | Saved/restored at start and end of function |
| +0x38  | N*4  | SRC history buffer (per-channel INT32) | lea rdx,[rax+0x38] used as source for memcpy to local stack |
| +0x13C | N*4  | SRC prev-sample state (per-channel INT32) | Used for per-channel previous sample persistence |
| +0x180 | 4    | Overflow counter | Incremented on buffer-full: inc dword ptr [r15+0x180] |

### Key insight: The ring buffer is located at a **signed offset from the struct base** (offset +0x08 holds that signed displacement). The ring stores INT32 samples (4 bytes each), not raw PCM bytes.

---

## 3. Sample Rate Conversion Algorithm

### 3.1 Rate Detection and GCD-based Ratio

The function performs **integer ratio resampling** using a GCD (Greatest Common Divisor) approach.

**Magic number analysis:**
- 0x1b4e81b5 with sar edx,5 = division by 44100
- 0x51eb851f with sar edx,5 = division by 100
- 0x1b4e81b5 with sar edx,3 = division by 75

The algorithm tries GCD candidates in order:
1. **300 (0x12C)**: both rates divisible by 300 -> use ratio = rate/300
2. **100 (0x64)**: both rates divisible by 100 -> use ratio = rate/100
3. **75 (0x4B)**: both rates divisible by 75 -> use ratio = rate/75
4. If none match -> return error -486

Supported rate pairs: 44100/48000 (via 300 -> 147:160), 48000/96000 (via 100), etc.

### 3.2 Overflow Check

The driver **refuses to process** if the output would overflow the ring buffer, incrementing a counter at +0x180 and returning -3. This is a **hard reject with counter** -- no silent data corruption.

### 3.3 Resampling Core: Linear Interpolation

At 0x1400028B0, the function checks source_rate vs 44100 for up/downsampling path selection.

For significant downsampling (source rate < half of destination), it uses multi-stage interpolation with shift factors (r13d = 2, 4, 8, or 16).

For same-rate or modest resampling (common 48kHz path), it uses **weighted linear interpolation** with two local stack arrays:
- [rbp-0x70]: accumulator per channel
- [rbp-0x30]: residual/previous sample per channel

    output[ch] = (accumulator[ch] * dst_ratio + residual[ch] * src_ratio) / total_ratio

### 3.4 Three Format Paths (8/16/24 bit)

**8-bit**: read unsigned byte, subtract 0x80, shift left 11 -> ~19-bit range
**16-bit**: read signed word, shift left 3 -> ~19-bit range
**24-bit**: read 3 bytes, assemble, shift left 8 then sar 13 -> ~19 bits

All paths normalize to **~19-bit signed integer** range before interpolation.

---

## 4. Ring Buffer Write

Ring stores **INT32 (4-byte) samples**, indexed by frame position. Write position wraps with simple compare-and-reset (no modulo). Frame-indexed circular buffer.

## 5. Multi-Channel Support

Channel count drives per-channel loops. Stack arrays hold per-channel accumulator/residual state (up to ~16 channels). SRC state at +0x38 and +0x13C persists across calls.

## 6. Direction-Dependent Path

7th parameter controls direction: write (speaker->ring) vs read (ring->mic). Same function handles both, ensuring identical SRC algorithm. Non-zero direction also zero-fills unfilled frames.

## 7. Return Values

| Value | Meaning |
|-------|---------|
| 0     | Success |
| -1    | Invalid struct |
| -2    | Unsupported bit depth |
| -3    | Ring buffer overflow |
| -9    | Format error in output |
| -50   | Unsupported output bit depth |
| -486  | Incompatible sample rates |

---

## 8. Critical Comparison: VB-Cable vs AO Cable

### 8.1 Architecture

| Aspect | VB-Cable | AO Cable |
|--------|----------|----------|
| **Ring buffer format** | INT32 (4 bytes/sample), any rate | 24-bit packed (3 bytes/sample), fixed internal rate |
| **SRC algorithm** | Integer-ratio linear interpolation with GCD | 8-tap windowed sinc interpolation |
| **SRC location** | Unified in single function (write+read) | Split: Speaker DPC writes converted, Mic DPC reads converted |
| **Conversion** | All formats -> ~19-bit int -> ring (INT32) | Format -> 24-bit INT32 -> sinc SRC -> pack 24-bit -> ring |
| **Data path** | One function does read + resample + write | LoopbackWriteConverted + ConvertToInternal + SrcConvert + ConvertFromInternal + LoopbackWrite |
| **Overflow handling** | Hard reject + counter (return -3, no data written) | Silent overwrite (advance ReadPos, log PushLossFrames) |
| **Ring indexing** | Frame-based (INT32 array, 4 bytes/sample) | Byte-based (packed 24-bit, 3 bytes/sample) |

### 8.2 Why VB-Cable Produces Clean Audio

**Root cause #1: VB-Cable never silently corrupts data.**
When the ring buffer would overflow, VB-Cable **refuses the write entirely** and returns -3. AO Cable silently overwrites the oldest data and advances ReadPos, causing a **discontinuous jump** in the audio stream mid-read.

**Root cause #2: VB-Cable ring uses INT32 (power-of-2 aligned) samples.**
AO Cable uses packed 24-bit (3 bytes/sample) in the ring buffer:
- 3-byte unaligned memory operations on every read/write
- Wrap-around calculations with 3-byte alignment are error-prone
- PipeBlockAlign varies between passthrough and converted modes, making DataCount math fragile

VB-Cable avoids this with uniform INT32 per sample and frame-based indexing.

**Root cause #3: VB-Cable does SRC and ring write atomically in one function.**
AO Cable pipeline: ConvertToInternal -> SrcConvert -> ConvertFromInternal -> LoopbackWrite. Each step uses separate scratch buffers, spinlock acquired separately:
- SRC state (Accumulator, PrevSamples) read/written without spinlock protection
- Format can change between conversion and ring write, causing data dimension mismatch
- Multiple memory copies vs VB-Cable single-pass approach

**Root cause #4: VB-Cable linear interpolation is simpler and more robust than sinc.**
AO Cable 8-tap sinc uses 2048-entry coefficient table, 64-bit accumulators, Q23 fixed-point. Any sinc bug produces severe artifacts. VB-Cable first-order linear interpolation is 10x simpler and virtually impossible to get wrong.

**Root cause #5: VB-Cable normalizes ALL input to ~19-bit precision immediately.**
AO Cable converts to full 24-bit range requiring careful clamping. Sinc accumulator overflow (sum >> 23 exceeding INT32 range before Clamp24) is undefined behavior in C.

### 8.3 The MicSink Push Problem (AO Cable specific)

AO Cable has a **dual-write architecture**: LoopbackWrite writes to both ring buffer AND Mic DMA (MicSink):
1. Speaker DPC writes to ring (for async Mic reads)
2. Speaker DPC ALSO writes directly to Mic DMA buffer

Mic DPC uses TotalBytesWritten to calculate position. Any timing mismatch -> stale or partially-written data read. VB-Cable has **no equivalent** -- single ring buffer only.

### 8.4 Summary of Likely Garbling Causes in AO Cable

1. **Packed 24-bit ring buffer alignment issues** -- 3-byte samples misaligned with CPU word boundaries
2. **MicSink dual-write race** -- Speaker pushes to Mic DMA while Mic DPC reads position independently
3. **Multi-stage conversion pipeline** -- format can change between stages (no lock across entire pipeline)
4. **SRC accumulator precision** -- 8-tap sinc with full 24-bit range risks overflow
5. **Silent overflow handling** -- ReadPos jump on overflow causes mid-stream discontinuity

### 8.5 Recommended Fix Strategy

Based on VB-Cable approach:
1. **Switch ring buffer to INT32 samples** (4 bytes each) -- eliminate 3-byte alignment issues
2. **Remove MicSink dual-write** -- use ring-read only (V2 SessionPassthrough already does this)
3. **Replace sinc SRC with linear interpolation** for initial stability, optimize later
4. **Implement hard-reject on overflow** with counter, instead of silent overwrite
5. **Do SRC + ring write in a single atomic operation** (one spinlock acquisition covers both)
6. **Normalize input precision immediately** (19-bit like VB-Cable, not full 24-bit range)

---

## 9. Additional Observations

### 9.1 No Floating Point
The entire 4808-byte function contains zero SSE/AVX/FPU instructions. All integer math. Safe for kernel-mode DPC at DISPATCH_LEVEL.

### 9.2 Bidirectional Design
Same function handles both speaker->ring (write) and ring->mic (read) via direction flag. Ensures identical SRC algorithm with consistent state management.

### 9.3 Extreme Downsampling Path
For rates < 1/16th of destination, VB-Cable uses r13d = 16 (16x oversampling) with weighted multi-sample blending to prevent aliasing.

### 9.4 Same-Rate Fast Path
When rates match, ratio = 1:1, resampling becomes direct memcpy. This is VB-Cable passthrough equivalent.
