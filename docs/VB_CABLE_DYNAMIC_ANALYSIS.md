# VB-Cable Dynamic Analysis Notes (2026-04-12)

## Driver Base Addresses (리부트 후)

| Driver | Base | End |
|--------|------|-----|
| vbaudio_cablea64_win10.sys | `fffff807'1c750000` | `fffff807'1c772000` |
| vbaudio_cableb64_win10.sys | `fffff807'1c780000` | `fffff807'1c7a2000` |
| aocablea.sys | `fffff807'1c6f0000` | `fffff807'1c70e000` |
| aocableb.sys | `fffff807'07b00000` | `fffff807'07b1e000` |

## PE Sections (vbaudio_cablea64_win10)

| Section | RVA | Virtual Size |
|---------|-----|-------------|
| .text | 0x1000 | 0x6b92 |
| .rdata | 0x8000 | 0x2154 |
| .data | 0xb000 | 0x8228 |
| .pdata | 0x14000 | 0x07c8 |
| PAGE | 0x15000 | 0x647a |
| INIT | 0x1c000 | 0x0712 |

## Key Function Offsets (from Ghidra, confirmed with WinDbg `u`)

| Ghidra Addr | Offset | Function | Purpose |
|-------------|--------|----------|---------|
| 0x140005910 | +0x5910 | SetState | Stream state change (STOP/PAUSE/RUN) |
| 0x1400039ac | +0x39ac | Ring Reset | Zero ring buffer + counters |
| 0x1400010ec | +0x10ec | Ring Write (overwrite-oldest) | Advance ReadPos to make room |
| 0x140001144 | +0x1144 | Available Frames | (WritePos - ReadPos) % WrapBound |
| 0x1400011d4 | +0x11d4 | Ring Read | Read + denormalize |
| 0x1400022b0 | +0x22b0 | Ring Write (main) | Normalize + write to ring |
| 0x140005634 | +0x5634 | DMA→Scratch→Ring | DMA linearization + ring write |
| 0x140005cc0 | +0x5cc0 | DPC Timer Callback | Shared timer iterates all streams |
| 0x140006320 | +0x6320 | Frame Calculation | QPC→frame count + position update |
| 0x1400065b8 | +0x65b8 | Timer Start | ExAllocateTimer + ExSetTimer |
| 0x140006adc | +0x6adc | Capture DPC Path | Mic DPC ring read + DMA write |
| 0x140004cf4 | +0x4cf4 | Check Other Stream | Check if other stream is RUN |

## Global Data Addresses (VB-Cable A, base+offset)

| Offset | Absolute Addr | Purpose |
|--------|--------------|---------|
| +0x12f90 | `fffff807'1c762f90` | Stream pointer array (9 slots, 8 bytes each) |

## Live Stream Pointers (captured during VB-Cable call)

| Slot | Address | Notes |
|------|---------|-------|
| [0] | `ffffb68f'abd4e070` | Active stream #0 (Speaker or Mic) |
| [8] (at +0x40) | `ffffb68f'ae529120` | Active stream #8 (Speaker or Mic) |

## Stream Structure Offsets (from decompile)

| Offset | Purpose |
|--------|---------|
| +0x70 | buffer_size |
| +0x74 | current position |
| +0x90 | port class object pointer |
| +0x98 | other stream index |
| +0xA0 | stream type/config |
| +0xA4 | isMic flag |
| +0xA8 | DMA buffer size |
| +0xB4 | previous state |
| +0xB8 | active state (0=paused/stopped, nonzero=running) |
| +0xD0 | DMA position 1 |
| +0xD8 | DMA position 2 |
| +0xE0 | TotalBytesProcessed |
| +0xE8 | TotalBytesTransferred |
| +0xF0 | linear position |
| +0x108 | timer period (100ns units) |
| +0x110 | bytes this tick |
| +0x158 | DMA overrun counter (speaker) |
| +0x160 | spinlock (device-level) |
| +0x168 | **ring buffer pointer** |
| +0x178 | next tick deadline (QPC) |
| +0x180 | DMA overrun counter (mic) / timing baseline |
| +0x188 | QPC timestamp |
| +0x190 | zero on RUN |
| +0x198 | zero on RUN |
| +0x1A0 | frames per tick |
| +0x1A8 | zero on RUN |
| +0x1B0 | tick counter / timing |
| +0x1C8 | zero on RUN |

## Ring Structure Offsets (from decompile)

| Offset | Purpose |
|--------|---------|
| +0x00 | buffer pointer (relative to struct base) |
| +0x08 | buffer displacement |
| +0x0C | channels per frame |
| +0x10 | frame capacity |
| +0x14 | WrapBound (wrap threshold) |
| +0x18 | WritePos (frame index) |
| +0x1C | ReadPos (frame index) |
| +0x20 | stride / internal rate |
| +0x24 | valid flag |
| +0x60 | underrun flag |
| +0x17C | counter 1 (cleared on reset) |
| +0x180 | overflow counter (cleared on reset) |
| +0x184 | counter 3 (cleared on reset) |
| +0x188 | counter 4 / status flag (cleared on reset) |

## Next Steps

1. Read stream+0x168 to get ring pointer for both active streams
2. Read ring+0x14 (WrapBound), +0x18 (WritePos), +0x1C (ReadPos)
3. Read stream+0xA4 (isMic) and +0xB8 (active state) to identify Speaker vs Mic
4. Poll WritePos/ReadPos during active playback to see fill level dynamics
5. Poll during Speaker STOP gap to see if ring empties or stays filled

## WinDbg Commands (to run during active VB-Cable call)

```
REM Identify streams (run once):
dd ffffb68f`abd4e070+a0 L10
dq ffffb68f`abd4e070+168 L1
dd ffffb68f`ae529120+a0 L10
dq ffffb68f`ae529120+168 L1

REM After finding ring pointer (RING_ADDR):
dd RING_ADDR+14 L3    ; WrapBound, WritePos, ReadPos
dd RING_ADDR+17c L4   ; counters
```
