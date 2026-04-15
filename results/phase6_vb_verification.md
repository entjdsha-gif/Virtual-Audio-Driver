# Phase 6 Option Y — VB-Cable WinDbg Verification

**Date**: 2026-04-16
**Target**: DESKTOP-0GRU07E (Win11, VB-Cable installed)
**Host**: DESKTOP-AA5B33E (WinDbg kernel net debug)
**Module**: `vbaudio_cableb64_win10` (Cable B, capture side) — A side symmetric
**Purpose**: Empirically verify 5 critical design assumptions for Option Y cable stream rewrite before Y1 implementation

## 1. Verified Facts (five-item checklist)

| # | Item | Verdict | Evidence |
|---|---|---|---|
| 1 | Hot path owner | render=pos-query, capture=timer | counter ratio 5420≈6320 (1:1), 5cc0 asymmetric A vs B |
| 2 | Shared timer scope | capture data-path owner (not auxiliary) | 5cc0 stack = KiExpireTimer2 → ExTimer2 DPC; 5cc0 dispatches 6778/68ac per stream |
| 3 | Packet notification | KeSetEvent inside 5cc0 callback loop AND direct [+0x8188] inside 6320/68ac when cursor crosses boundary | disasm + `ln` resolution |
| 4 | Render/capture asymmetry | yes, 3 distinct advance entry points | 6320 (query), 6778 (timer render), 68ac (timer capture) |
| 5 | RUN/PAUSE/STOP reset scope | only tick anchor (+0x190/+0x1A0/+0x1B0) re-initialized on first tick; cursor/monotonic never reset inside driver | 68ac init branch; 65b8 zero-init only |

## 2. Canonical Advance Pseudocode (extracted from 6320 disasm)

This is the direct specification for `AoCableAdvanceByQpc`:

```c
// rcx = stream, rdx = nowCounter, r8 = flags
void AdvanceByQpc(Stream *s, uint64_t nowCtr, uint32_t flags)
{
    KeQueryPerformanceCounter(&freq);           // [+0x8000]

    // QPC → frames since anchor
    uint64_t nowQpc100ns = (arg_r8 * 10000000 / freq) + (nowCtr * 10000000 / freq);
    int64_t elapsedFrames = ((nowQpc100ns - s->anchorQpc /*+0x180*/) * s->sampleRate /*+0x8C*/) / 10000000;
    int32_t advance = (int32_t)(elapsedFrames - s->publishedFrames /*+0x198*/);

    // 8-frame minimum gate (source of CLAUDE.md principle #9)
    if (advance < 8) return;

    // 127-frame rebase: if accumulated frames exceed sampleRate*128, reset anchor
    if (elapsedFrames >= (s->sampleRate << 7)) {
        s->publishedFrames = 0;
        s->anchorQpc = nowQpc100ns;
    }

    // Overrun: elapsed > half a second of frames = stall
    if (advance > s->sampleRate / 2) {
        if (s->isCapture)  s->statOverrunCap++;
        else               s->statOverrunRen++;
        return;
    }

    // Dispatch
    if (s->isCapture == 0) {   // RENDER (+0xA4 == 0)
        PreprocessDma(s);                       // call 6adc
        s->prevCursor = s->cursor;              // +0xD8 = +0xD0
        s->cursor = (s->cursor + advance) % s->bufferSize;
    } else {                   // CAPTURE
        // Align to +0x7C boundary if needed (wrap case)
        FillRingWithSrc(s, advance, s->cursor); // call 5634
        s->prevCursor = s->cursor;
        s->cursor = (s->cursor + advance) % s->bufferSize;

        // Direct packet notification on boundary crossing
        if (s->notifyArmed /*+0x164*/ && !s->notifyFired /*+0x165*/
            && (s->cursor % s->bufferSize) == s->notifyBoundary /*+0x7C*/) {
            s->notifyFired = 1;
            CallPortclsNotify(s->notifyCtx, 8);  // [+0x8188] thunk, edx=8
        }
    }

    // Monotonic counters (both mirrored)
    s->monoHigh /*+0x0E8*/ += advance;
    s->monoLow  /*+0x0E0*/ += advance;
    s->deltaStat /*+0x1B8*/ += advance;
}
```

## 3. Shared Timer Confirmation (65b8 disasm)

On first stream registration:

```c
if (g_streamGen == 0) {
    RtlZeroMemory(&g_streamTable, 64);      // 8 slots × 8 bytes
    g_timer = ExAllocateTimer2(              // [+0x8110]
        AoTimerDpc /* 5cc0 */, NULL, 0);
    if (g_timer) {
        ExSetTimer(g_timer,                  // [+0x8118]
            -10000,     // due = 1ms
            10000,      // period = 1ms
            NULL);
    }
}
// Add stream to first empty slot
for (i = 0; i < 8 && g_streamTable[i]; i++);
g_streamTable[i] = newStream;
if (i >= g_streamCount) g_streamCount = i + 1;
g_streamGen++;
```

**Key facts**:
- Timer type: `ExAllocateTimer2` (equivalent to ExTimer2 / KeTimer2) — HAL-backed, DPC on expiration
- Period: **1 millisecond fixed** (both due and period = 10000 × 100ns)
- Max streams: 8 (hard-coded array size at +0x12F90)
- Generation counter at +0x12F84 (used to detect re-init)
- **No per-stream field reset here** — zero-init at ExAllocatePool is assumed

## 4. 5cc0 Timer DPC Structure

```c
void AoTimerDpc(PEX_TIMER, void *, void *, void *)  // called by KiExpireTimer2
{
    KeQueryPerformanceCounter(&freq);               // [rsp+50h] = freq

    // 63/64 drift correction accumulator
    ctx = GetGlobalCtx();
    ctx->tickCount /*+0x2A0*/++;
    if (ctx->tickCount > 100) {
        ctx->baselineQpc /*+0x298*/ += freq;        // one extra period every 100 ticks
        ctx->tickCount = 1;
    }
    ctx->targetQpc /*+0x2A8*/ =
        ctx->baselineQpc + ((ctx->tickCount * freq * 0xA3D70A3D70A3D70B) >> 6);
        // ≈ baseline + (tickCount * freq / 100)

    // Dispatch all active streams
    for (i = 0; i < g_streamCount; i++) {
        s = g_streamTable[i];
        if (!s) continue;
        AcquireSpinLock(&s->lock /*+0x168*/);

        if (s->isCapture == 0)  RenderAdvance(s, tick);  // 6778
        else                    CaptureAdvance(s, tick, freq);  // 68ac

        ReleaseSpinLock(&s->lock);

        if (s->state /*+0xBC*/ == 3) {               // active → run callback list
            for (cb = s->callbackList /*+0x50*/; cb != &s->callbackList; cb = cb->next) {
                [+0x8188](cb->vtable[0xB0]);          // notify
                KeSetEvent(cb->event /*+0x10*/, 0, FALSE);   // [+0x8108]
            }
        }

        if (s->pendingFlag /*+0x165*/ && s->pendingCookie /*+0x60*/) {
            FinalizePending(s);                       // 669c
            s->pendingCookie = 0;
        }
    }
}
```

## 5. Field Layout (must mirror in AO_STREAM_RT)

Per-stream offsets observed on Cable B (capture). Render (Cable A) shares most:

| offset | type | meaning |
|---|---|---|
| `+0x50` | LIST_ENTRY | callback list head (doubly-linked) |
| `+0x60` | ptr/flag | pending finalize cookie |
| `+0x7C` | uint32_t | notify boundary (ring byte/frame offset) |
| `+0x86` | uint16_t | bitsPerSample (or container bits) |
| `+0x88` | uint16_t | channels |
| `+0x8C` | uint32_t | sample rate (Hz) |
| `+0x98` | ptr | port context / adapter pointer |
| `+0xA4` | uint8_t | **render=0 / capture=1** flag |
| `+0xA8` | uint32_t | ring buffer size in frames |
| `+0xB0` | uint32_t | state sub-flag (0 = normal) |
| `+0xB4` | uint32_t | stream state (3 = active RUN) |
| `+0xB8` | uint32_t | **alt render/capture flag** (0=render) used by timer dispatcher |
| `+0xBC` | uint32_t | notification state (3 = callback-enabled) |
| `+0xC8` | uint64_t | **linear position** (returned by GetPosition) |
| `+0xD0` | uint64_t | **DMA cursor** (current, byte or frame offset) |
| `+0xD8` | uint64_t | DMA cursor (previous snapshot) |
| `+0xE0` | uint64_t | **monotonic frame counter (low)** — observed 0x861e5e |
| `+0xE8` | uint64_t | monotonic frame counter (mirror) |
| `+0x160` | SPIN_LOCK | stream spinlock |
| `+0x164` | uint8_t | notify-armed flag |
| `+0x165` | uint8_t | notify-fired flag |
| `+0x180` | uint64_t | anchor QPC (100ns units) |
| `+0x198` | uint32_t | published frames since anchor |
| `+0x1A0` | ptr | tick accumulator context |
| `+0x1B0` | uint64_t | next-tick target frames |
| `+0x1B8` | uint32_t | last advance delta (stat) |
| `+0x1BC` | uint32_t | frame-per-tick base (ebp) |
| `+0x1C0` | uint32_t | frame-per-tick num (r14) |
| `+0x298` | uint64_t | (global ctx) baseline QPC |
| `+0x2A0` | uint64_t | (global ctx) tick count mod 100 |
| `+0x2A8` | uint64_t | (global ctx) target QPC |

## 6. Packet Notification Path

`[+0x8188]` is a `jmp rax` thunk (at vbaudio+0x7650). Real target resolved at call site via:
```
rcx = stream[+0x98]             // port ctx
rcx = [rcx + 0x128]             // registered notify object
rax = [rcx]                     // vtable
rax = [rax + 0xB0]              // method slot
call [+0x8188]                  // → jmp rax
```

This is a **COM-style vtable dispatch**. In AO we equate it to the PortCls notification path we already use (IPortWaveRTStream notification / KSEVENT completion).

`[+0x8108]` = `nt!KeSetEvent` (raw event signalling, inside callback loop — for WaveRT packet-complete events registered by clients).

## 7. Ring Write & Format Conversion (verified from disasm)

### 7.1 Ring format (22b0, 4f2c, 51a8 cross-verified)

**INT32 ring, 19-bit normalized, sign-extended.** All input formats convert to this canonical container:

| Input bpp | Conversion (to INT32 ring cell) | Source offset in 22b0 |
|---|---|---|
| 8 (u8) | `(sample - 0x80) << 11` — centered, shifted to 19-bit | 0x25eb |
| 16 (i16) | `sample << 3` — shifted to 19-bit | 0x2585 |
| 24 (i24 packed LE) | `sample << 8 >> 13` — sign-extend 24→32 then /32 | 0x24fb |
| 0x78C (1932) | linear memcpy via call 7680 | 0x23ea |

**Bpp 0x78C is float/INT32 passthrough.** 22b0 at 0x23ea uses `idiv eax, [rdi*4]` (divide by 4-byte stride) and calls 7680 (memcpy), which is why this path goes through scratch linearization. So INT32/float is copied as-is without bit conversion.

### 7.2 22b0 argument contract

```c
// rcx = ringCtx, rdx = srcBuffer, r8d = srcBytes, r9d = srcRate
// [rsp+0A0h] = numChannels (int)
// [rsp+0A8h] = bpp (8/16/24/1932)
// [rsp+0B0h] = flags (0 = simple, !=0 = post-dither via 7940)
int32_t RingWriteFast(RingCtx *ctx, void *src, uint32_t srcBytes, uint32_t srcRate,
                      int numChannels, int bpp, int flags);
```

Return codes (negative = error):
- `-1`: ringCtx invalid (ctx[+8] == 0)
- `-2`: srcBytes == 0
- `-3`: numChannels < 1
- `-4`: srcRate < 0x1F40 (8000)
- `-5`: srcRate > 0x30D40 (200000)
- `-6`: overrun (frame count would exceed ring capacity - 2)
- `-7`: unknown bpp (8/16/24/1932 not matched)
- `-9`: bpp=24 overrun
- `-0xA`: bpp=16 overrun
- `-0xB`: bpp=8 overrun
- `0`: success

On overrun, increments ctx[+0x180] (overrun counter).

### 7.3 Channel ring layout

Each channel writes to `ring[channelStride * channelIdx + frameIdx]` where:
- `channelStride = ctx[+0x10]` (per-channel frame count)
- `baseRing = ctx + ctx[+8]` (ring base offset from ctx)
- `writeCursor = ctx[+0x18]` (wraps on `ctx[+0x14]` limit)

This is **channel-planar** (not interleaved in the ring), which matters for AO's `FRAME_PIPE` layout decision.

### 7.4 4f2c (peak-hold / VU calculation)

After 22b0 writes raw samples, 4f2c walks the same samples and updates **abs-peak values** per frame slot:

```c
// Two modes based on [rsp+0F8h] (arg 5):
// mode 0: peak table at ctx[+0x50]
// mode !=0: peak table at ctx[+0xD0]
for (each sample s in block) {
    abs_s = abs(s * scale);   // scale varies per bpp: <<5 for 32, <<3 for 16, <<11 for 8, etc.
    if (abs_s > peak[i]) {
        peak[i] = abs_s;                 // new peak
    } else {
        peak[i] = (peak[i] * 0x7F) >> 7; // decay by 127/128 per frame
    }
}
```

This is **VU meter / peak-hold state** — not audio data. **AO can skip this entirely** since we don't expose peak meters through the cable. Not an audio-quality contributor.

### 7.5 51a8 (gain/envelope application)

Called from 5634 (capture ring fill) with a **coefficient array** as arg4:
```c
// rcx = ringCtx, edx = srcBytes, r8d = srcRate, r9d = numChannels
// [rsp+28h] = coefArray (ptr to int32[frameCount])
// Applies: sample[i] = (sample[i] * coef[i]) >> 7
```

The coefficient array comes from the **fade-in envelope table at +0x12a60** — see §7.6. Each sample gets multiplied by a per-sample gain, then `>> 7` (divide by 128).

### 7.6 Envelope table at +0x12a60 (resolved — corrected 2026-04-16)

**96-entry fade-in ramp, 4 bytes per entry (int32), valid indexes 0..95**:

Previously documented as "95-entry" — that was imprecise. The VB code uses
`add edx, 0x60` (0x60=96) to offset the counter, then clamps via `cmovg edx, r11d`
where `r11d = 0x5F = 95` (max valid index). Array size is therefore 96 entries,
max legal index 95. AO Y1B code (`g_aoFadeEnvelope[96]` in transport_engine.cpp)
matches the VB memory layout byte-for-byte as captured via WinDbg `dps` on
2026-04-16:

```
index  0..51: 0           (silence prefix — 52 samples, not 28)
index 52..55: 0, 1, 1, 1
index 56..59: 1, 1, 1, 2
index 60..63: 2, 2, 2, 3
index 64..67: 3, 4, 4, 5
index 68..71: 5, 6, 7, 8
index 72..75: 9, 10, 11, 12
index 76..79: 14, 16, 18, 20
index 80..83: 22, 25, 28, 32
index 84..87: 36, 40, 45, 50
index 88..91: 57, 64, 71, 80
index 92..95: 90, 101, 114, 128  (saturated)
```

Rising from 0 to 128 (max gain in 7-bit fixed point → unity). Indexed by
`(sampleCounter + 96)` clamped to `[0, 95]`.

**Purpose**: Smooth fade-in at packet boundaries to prevent click/pop during cursor discontinuities. Applied per-sample in 51a8's `imul eax, coef; sar eax, 7`.

**AO implication**: This is the **click suppression mechanism** that makes VB sound clean. If AO writes raw samples to ring without this envelope, packet boundary transitions may click. **Y1 must replicate this envelope table.**

### 7.7 4080 global ctx accessor (resolved)

```c
// idx is 1-based; idx=0 returns NULL
void* GetInstanceCtx(uint32_t idx) {
    if (idx == 0) return NULL;
    return &g_instances[idx] where each entry is 0x2B0 bytes, base at +0x12CC0
}
```

- Base: `vbaudio_cableb64_win10 + 0x12CC0`
- Stride: **0x2B0 bytes per instance**
- Indexing: 1-based (idx=1 → +0x12F70, idx=2 → +0x13220, ...)

**This is NOT per-stream** — it's per-cable-instance (one Cable B endpoint = one ctx). The `+0x298/+0x2A0/+0x2A8` fields observed in 5cc0's drift correction live in this instance ctx, not per-stream.

Observed instance[0] data at +0x12CC0 (matches **48k/16bit/2ch** default format):
```
+0x00: 0x0000bb80 0x00020001   // rate=48000, ch=2, bpp=16?
+0x08: 0x00000002 0x00001c00   // framesPerChunk=7168
+0x10: 0x00000018 0x0000bb80   // alt bpp=24, rate=48000
+0x20: 0xad/039d 0x2fcb/0       // stat counters
```

### 7.8 Format dispatch (4f2c / 51a8 mirror 22b0 exactly)

All three ring functions check bpp via the same 4-way switch:
- `cmp eax, 8` → u8 path
- `cmp eax, 0x10` → i16 path
- `cmp eax, 0x18` → i24 path
- `cmp eax, 0x78C` → float/INT32 path
- else → error

So **bpp 0x78C (1932) = WAVE_FORMAT_EXTENSIBLE float variant** or a VB-internal "no conversion needed" marker. Either way, AO's FRAME_PIPE INT32 ring can use 0x78C as the "native format pass-through" sentinel.

## 8. Remaining Gaps (low priority)

1. **5634 SRC inner loop normalization** — captured in §2 but the per-sample coefficient math at 0x578f–0x57e1 uses the envelope table lookup, not a sinc filter. So **VB does not do actual sample-rate conversion** — it uses linear copy + envelope smoothing. Same-rate paths are passthrough. This means the "SRC" concern from earlier notes is moot — VB doesn't have a real SRC, just a fade envelope.
2. **6adc / 4cf4 format re-negotiation** — transient during format change. Edge case, Y5 polish.
3. **Exact 7680 memcpy semantics** — it's a scratch linearization (wrap-handling memcpy), low risk.
4. **Ring wrap alignment (+0x7C boundary)** — still partially opaque; looks like `ctx[+0x14]` (ring size limit) vs `ctx[+0x18]` (write cursor). Not a blocker.

## 9. Design Impact on Option Y

### Must change in `docs/PHASE6_PLAN.md`
- Rule: "shared timer is auxiliary only" → **Split rule**:
  - Render: pos-query driven, timer only for notification fallback
  - Capture: timer is data-path owner, pos-query is informational
- Rule: "canonical cable advance path" → clarify **3 entry points** (query, timer-render, timer-capture) but **one helper** `AoCableAdvanceByQpc` with a `reason` enum

### Must add to `docs/PHASE6_OPTION_Y_CABLE_REWRITE.md`
- Verbatim pseudocode from §2 as the spec for `AoCableAdvanceByQpc`
- Field layout table (§5) as target for `AO_STREAM_RT`
- 1ms `ExAllocateTimer2` as the transport timer primitive (not ExSetTimer with KeTimer)
- 8-frame minimum gate, 127-frame rebase, 0.5s overrun — all mandatory
- Monotonic counter mirror field rule (write both +0xE0 and +0xE8 equivalents)
- 63/64 drift correction formula from §4

## 9.5. Ghidra Static Closure (2026-04-16 follow-up)

After §9 open items were raised, three parallel Ghidra passes on the existing decompile (`results/ghidra_decompile/vbcable_all_functions.c`) closed all remaining blockers.

### 9.5.1. Unregister / STOP entry confirmed — `FUN_14000669c`

669c is **both** the in-stream cleanup path (called from 5cc0's `+0x165 && +0x60` branch) **and** the stream destructor/unregister. It has 3 caller sites in addition to 5cc0:
- Line 4544, 4798 — stream STOP/close paths
- Line 11464 — miniport teardown

Behavior:
1. Linear search of `g_streamTable[8]` for the stream ptr, clears the slot
2. Decrements `g_streamGen`; updates `g_streamCount` to highest-populated index
3. **When g_streamCount reaches 0**: calls `ExDeleteTimer(DAT_140012fd8, 1, ...)` (synchronized) then clears the handle and zeros instance ctx fields `+0x298/+0x2A0/+0x2A8` (the drift correction accumulator)
4. Per-stream field reset happens at the **caller site**, not inside 669c itself:
   - Capture streams: zero `+0xD0..+0x108` (cursor, prev cursor, monotonic counters, spare)
   - Render streams: zero `+0x50..+0x88` (different layout due to render/capture struct divergence)
   - Both: anchor QPC `+0x180` cleared at line ~4590

**Implication for Y**: monotonic counters `+0xE0/+0xE8` **ARE cleared on STOP** (correction to earlier §5 note). AO's `AoTransportOnStopEx` must zero the equivalent fields. Shared timer is ref-counted globally: last stream out destroys it.

### 9.5.2. Ring allocation + scratch buffer

- **Ring allocator**: `FUN_140001008 → FUN_140003a04(tag, size)` at miniport init (line 11541)
- **Ring stored at**: stream `+0x170` (base ptr)
- **Size formula** (agent finding, approximate): `(channels * 16) * 12 + 400` bytes with channels derived from `max(0xF00, framesPerChunk * 0x2ee00 / rate)` — **need to re-verify**, but bounded to a small (<few KB) per-channel allocation. Not literally `frames × channels × 4`.
- **Scratch buffer**: per-stream, stored at `+0x178`. Passed as dst to `FUN_140007680` for DMA wrap linearization.
- **FUN_140007680**: plain SSE-optimized memcpy with overlap handling (NOT a conversion) — small-copy byte-wise, medium 8-byte aligned, large 64-byte SSE, backward-copy for overlap.

**Implication for Y**: AO's FRAME_PIPE can use a **per-stream scratch** of bounded size (enough for one DMA wrap's worth) and reuse `RtlCopyMemory` / `memcpy`. Size bound = max burst size = `sampleRate/2 * bytesPerFrame` (from 6320's overrun guard).

### 9.5.3. ★ Packet notification — VB does NOT notify shared-mode clients

**Critical finding that closes the "packet notification OPEN" question**:

- **`+0x7C` is NOT a derived boundary — it is set by the client via `SetNotificationCount` API** (`FUN_140004700 → FUN_140004764`)
- **`+0x164` is a "notify enabled" gate flag, NOT a count**. Initialized to 0 at stream alloc. Only becomes non-zero when client explicitly calls `SetNotificationCount` with a valid non-zero byte position.
- **`SetNotificationCount(0)` is explicitly rejected with `STATUS_INVALID_PARAMETER` (0xC0000010)** at line 3296.
- **Shared-mode clients that never call `SetNotificationCount` get ZERO packet notifications.** The check at 6320:0x6544 (`cmp byte ptr [rdi+0x164], 0; je skip`) bypasses the entire [+0x8188] call path.

**Why this matters**:
- Phone Link, Realtek-class audio apps, and most shared-mode consumers never set a notification count. They poll via `GetPosition`.
- VB's "packet notify via [+0x8188]" path is **event-driven / WaveRT exclusive-mode only**.
- AO's existing behavior — returning `STATUS_NOT_SUPPORTED` for `GetReadPacket`/`SetWritePacket`/`GetPacketCount` when `m_ulNotificationsPerBuffer == 0` — is **already semantically equivalent to VB** for shared-mode clients.

**Implication for Y1**:
- **Y1 does NOT need to replicate the [+0x8188] notification call path.** It only matters for event-driven clients which AO already handles through the existing PortCls contract.
- The **entire "packet notification contract is OPEN"** item from Codex's `VB_PARITY_DEBUG_RESULTS.md` §7 can be marked **CLOSED** — shared-mode clients (the only ones AO currently cares about for live call) don't take this path at all.
- Bug B's "지지직 overlay" was never about missing packet notifications. The root cause (now confirmed) is the structural decoupling of update chain from transport cadence in the MSVAD-pattern code, which Y's canonical advance helper directly fixes.

**Callback target identity** (for completeness, even though shared-mode skips it):
```
thunk @ +0x8188  →  jmp rax
                    where rax = stream[+0x98][+0x128][0][+0xB0]
                    i.e., IPortWaveRTStream-like vtable method
args: (notifyCtx, 8=eventCode, position, boundaryByteOffset)
```

### 9.5.4. Updated open-item status

| Item | Status | Note |
|---|---|---|
| Unregister/STOP entry | **CLOSED** | 669c confirmed multi-role, caller-side reset |
| Scratch buffer | **CLOSED** | per-stream at +0x178, memcpy via 7680 |
| Ring allocation/size | **MOSTLY CLOSED** | per-stream at +0x170, formula approximate — re-verify if byte-exact parity demanded |
| +0x7C boundary semantics | **CLOSED** | client-set via SetNotificationCount, bytes |
| Packet notify shared-mode | **CLOSED** | VB doesn't notify shared-mode clients; AO already matches |
| NotifyArmed STOP preservation | **PROVISIONAL** | AO Y1B preserves across STOP — NOT directly verified in VB. Must be confirmed or corrected before Y2 (see §9.6) |

### 9.6. Provisional items tracked for Y2 pre-flight closure

These items were noted during Y1B implementation review (Codex validation,
2026-04-16) as NOT fully verified against VB. They are acceptable to carry
through Y1C shadow mode, but must be resolved before Y2 promotes the helper
to audible ownership.

**P-1. NotifyArmed preservation across STOP**

Current AO Y1B behavior (`AoCableResetRuntimeFields`): preserves
`NotifyArmed` + `NotifyBoundaryBytes` across STOP, clears only `NotifyFired`.
Rationale at the time: event-driven clients that called `SetNotificationCount`
should not lose their arm state across a pause/resume pair.

**Gap**: VB's actual STOP reset scope for the notify-related fields
(+0x164 / +0x165 / +0x7C) was not directly captured. The Ghidra pass on
`FUN_14000669c` only dumped the cursor/counter caller-side reset pattern
(+0xD0..+0x108 for capture, +0x50..+0x88 for render, +0x180 anchor).

**Closure plan before Y2**: one of —
  (a) dump `FUN_14000669c`'s caller sites in Ghidra and check whether they
      also zero `[rcx+0x164]` / `[rcx+0x7C]`; or
  (b) WinDbg BP on `FUN_140004700` (`SetNotificationCount`) across a
      stream stop/resume cycle to observe client re-arm behavior; or
  (c) if unresolved, change AO Y1B to clear `NotifyArmed` on STOP and
      require clients to re-arm via `SetNotificationCount` after resume.

Until closed, treat the current behavior as a design choice, not a VB
parity claim. The shared-mode live-call path is unaffected either way
(NotifyArmed stays 0 for shared-mode clients regardless).

## 10. Verdict (updated after §7 closure)

**All major gaps closed.** VB-equivalence is now achievable at byte-exact level for standard paths:

- Structural (§1–§6): advance logic, timer, field layout, notification — fully specified
- Data path (§7): ring format (INT32 19-bit), 4-way bpp dispatch, fade envelope table, channel-planar layout — fully specified
- **Critical discovery**: VB has **no real SRC**. The "SRC-like" loop in 5634 is a 95-sample fade-in envelope (table at +0x12a60) used to suppress packet-boundary clicks. Same-rate paths are linear copy. This means AO does not need any resampling infrastructure to match VB.

**Y1 implementation checklist** (minimum for audible parity):
1. ✅ `AoCableAdvanceByQpc` helper (§2 pseudocode)
2. ✅ `ExAllocateTimer2` 1ms shared timer (§3)
3. ✅ INT32 ring with 4-way format dispatch (§7.1)
4. ✅ Fade envelope table (95 entries) and `51a8`-equivalent application (§7.5, §7.6)
5. ✅ Field layout for `AO_STREAM_RT` (§5)
6. ⏭️ VU peak-hold: skip (AO doesn't expose meters)
7. ⏭️ Format renegotiation: Y5 polish

**Byte-exact parity** is now a realistic Y1 target, not a Y5 stretch goal.

---

**Generated from**: WinDbg kernel session 2026-04-16, Cable B module at `fffff803`5ce20000`, verified against live YouTube render + capture traffic.
