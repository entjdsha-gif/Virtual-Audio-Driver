# AO Virtual Cable — Fixed Pipe Architecture

> **Status:** authoritative design document for the cable-stream rewrite.
> Replaces all earlier Phase 5/6 plans, Option Y/Z naming, and scattered VB
> parity drafts. If anything else disagrees with this file, this file wins.
>
> **Last updated:** 2026-04-25
> **Scope:** cable-stream transport core (`eCableASpeaker`, `eCableBSpeaker`,
> `eCableAMic`, `eCableBMic`). PortCls/WaveRT shell, install, INF, signing,
> non-cable streams are out of scope.

---

## Table of contents

1. [Goal & non-goals](#1-goal--non-goals)
2. [Why we are starting over](#2-why-we-are-starting-over)
3. [Reference & evidence](#3-reference--evidence)
4. [Design principles](#4-design-principles)
5. [External contract](#5-external-contract)
6. [Internal data structures](#6-internal-data-structures)
7. [Canonical pipeline](#7-canonical-pipeline)
8. [State machine](#8-state-machine)
9. [Threading & locking](#9-threading--locking)
10. [Format conversion / SRC](#10-format-conversion--src)
11. [Position & notification](#11-position--notification)
12. [Diagnostics](#12-diagnostics)
13. [Code organization](#13-code-organization)
14. [Migration: keep / retire / rewrite](#14-migration-keep--retire--rewrite)
15. [Implementation stages](#15-implementation-stages)
16. [Validation](#16-validation)
17. [Failure modes & lessons](#17-failure-modes--lessons)
18. [Risk table](#18-risk-table)
19. [Appendix A — VB struct offset map](#appendix-a--vb-struct-offset-map)

---

## 1. Goal & non-goals

### Goal

Rebuild the **cable-stream transport core** to match VB-Cable's behavior closely enough that the AO live-call quality reaches or exceeds VB-Cable on the Phone Link / OpenAI Realtime path. Achieve this without rewriting the PortCls/WaveRT shell.

Concretely:

- one canonical cable advance path that owns transport + accounting + position freshness together
- INT32 frame-indexed ring with hard-reject overflow
- single SRC function with linear interpolation
- DMA → scratch linearization before processing
- no MicSink / dual-write
- position recalculated to current QPC on every WaveRT query
- shared timer + WaveRT queries are both first-class call sources, but neither owns separate truth

### Non-goals

- not rewriting `adapter.cpp` PortCls registration
- not rewriting filter/pin/topology/INF/install/service
- not rewriting non-cable stream behavior
- not pursuing exact byte-for-byte parity with VB beyond what materially affects audio quality
- not chasing exclusive-mode / packet-event-driven parity as a blocker for shared-mode phone path

### Definition of done

The shared-mode Phone Link call path on AO Cable is **at least as clean as VB-Cable** on the same hardware, measured by:

- live call quality judgment (clean / garbled / silent) → clean
- transcript accuracy on OpenAI Realtime (not garbled hallucinations)
- driver-level diagnostics (no underrun/overflow during steady-state speech)

---

## 2. Why we are starting over

Confirmed via Ghidra static analysis of VB-Cable + WinDbg dynamic analysis on live runs:

| Concern | AO (current state) | VB-Cable (target) |
|---|---|---|
| Ring sample format | packed 24-bit (3 bytes, unaligned) | INT32 (4 bytes, ~19-bit normalized) |
| Ring indexing | byte-based, 3-byte stride | frame-indexed, fixed 4-byte stride |
| Overflow handling | silent overwrite (ReadPos jumps) | hard reject + counter, ring never corrupts |
| Format conversion | 4-stage: ConvertToInternal → SrcConvert → ConvertFromInternal → LoopbackWrite | single atomic SRC+ring write per direction |
| SRC algorithm | 8-tap windowed sinc, 2048-coef table | GCD-based integer-ratio linear interpolation |
| FormatMatch | required: Speaker == Mic == 48k/24/8ch | none — internal SR is registry-driven, accepts any client format |
| MicSink dual-write | ring + Mic DMA push (race-prone) | none — ring is sole data path |
| DMA wrap handling | inline in conversion (error-prone) | DMA → linear scratch → process |
| Position freshness | last DPC's value (up to 1ms stale) | recalculated to NOW QPC per query |
| Timer drift | none | 63/64 phase correction, rebase every 100 ticks |
| Frame gate | none | 8-frame minimum |

The Phase 5/6 attempts that tried to **patch** AO's existing core failed because the core's invariants (FormatMatch, packed 24-bit, 4-stage pipeline, MicSink dual-write) are wrong, not the constants on top of them. We reset and rebuild against VB's verified shape.

---

## 3. Reference & evidence

All design choices below trace back to one of these. When this doc states a behavior, that behavior is verified in the cited evidence (not invented).

### Ghidra static analysis (decompile dump)

- `results/ghidra_decompile/vbcable_all_functions.c` — full decompile, 297 functions, 12,096 lines
- `results/ghidra_decompile/vbcable_function_index.txt` — function map with addresses/sizes
- `results/ghidra_logs/vb_re_headless*.txt` — headless run logs (FindVbLatency, registry keys, init function)

### Synthesized analyses

- `results/vbcable_pipeline_analysis.md` — pipeline trace (DPC, render, capture, position)
- `results/vbcable_disasm_analysis.md` — `FUN_1400026a0` (write SRC) deep dive, ring struct layout
- `results/vbcable_func26a0.asm` — raw assembly of the largest function (4808 bytes)
- `results/vbcable_capture_contract_answers.md` — capture-side contract Q&A (10 verified answers)

### WinDbg dynamic analysis (live runs)

- `results/vb_session.log` — full WinDbg/KDNET session, 6,336 lines
- `results/phase6_vb_verification.md` — verification summary, breakpoint counts under TTS payload
- (older raw outputs: `livekd_*.txt`, `kd_out.txt`)

### Live call confirmation

- VB-Cable: clean (TTS + AI Realtime both clear, transcription accurate)
- AO Cable: garbled (same path, same harness, driver core problem)
- harness: `tests/live_call/run_test_call.py`

---

## 4. Design principles

These are **non-negotiable**. Every implementation choice must respect all of them or get explicit written exception.

### 4.1 INT32 frame-indexed ring

The ring stores samples as 4-byte signed integers, indexed by **frame** (not byte). Each format on input is normalized to ~19-bit signed range during ring write so SRC headroom never overflows.

| Input format | Conversion to ring INT32 |
|---|---|
| 8-bit unsigned | `(byte - 0x80) << 11` |
| 16-bit signed | `(int)short << 3` |
| 24-bit packed signed | `(3-byte assembled << 8) >> 13` |
| 32-bit float / native INT32 | direct copy |

### 4.2 Hard-reject on ring overflow

If a write would exceed available space, the write is **rejected entirely** (no partial write, no advance of WritePos), and a per-ring counter is incremented. Old data in the ring is never overwritten.

### 4.3 Single SRC function per direction (write/read)

Two functions:

- **write SRC**: client format → ring INT32 (called by render path)
- **read SRC**: ring INT32 → client format (called by capture path)

Each function does GCD-based integer-ratio linear interpolation in one pass. There is **no** "convert to internal then SRC then convert from internal" pipeline.

GCD divisors tried in order: 300, 100, 75. If none work, return error.

### 4.4 DMA → scratch → ring (linearization)

Before any conversion or pipe write, the DMA circular region is copied to a per-stream linear scratch buffer. Wrap-around is handled here, **not** inside conversion logic.

### 4.5 No MicSink, no dual-write

The ring is the sole data path between speaker and mic. Speaker side writes to ring. Mic side reads from ring. There is no second push that bypasses the ring.

### 4.6 Position recalculated on query

When WaveRT calls `GetPosition` / `GetPositions`, the position handler invokes the canonical advance helper with the current QPC **before** returning the value. Returned positions reflect "now" within sub-DPC precision, not the last DPC tick.

### 4.7 Canonical cable advance helper

All cable transport entry points (query path, shared timer, packet surface) funnel into a single function:

```c
AoCableAdvanceByQpc(rt, nowQpcRaw, reason, flags);
```

This function owns, together:

1. QPC delta → elapsed frames
2. minimum-frame gate (8 frames)
3. monotonic cursor / accounting updates
4. DMA wrap / linearization
5. ring read/write
6. position freshness before WaveRT reporting
7. packet-ready accounting (when arming exists)

Plus three mandatory invariants:

8. drift-corrected scheduling (63/64 phase)
9. scratch linearization
10. DMA overrun guard (skip if computed move > sampleRate / 2 frames)

**No second path is allowed to "just move audio later" using state published by the first path.** That split was the Step 3/4 regression source.

### 4.8 8-frame minimum gate

The helper computes `advance = elapsedFrames - publishedFrames`. If `advance < 8`, the helper returns without writing/reading. This prevents sub-sample jitter from causing irregular tiny transfers.

### 4.9 Frame-only units

Authoritative unit is **frames**. Bytes and QPC are derived. **`ms` may not appear in runtime state or transport math**, only in:

- comments
- external UI / control panel
- logs for human readability

### 4.10 KeFlushQueuedDpcs on Pause/Stop

Before resetting any per-stream runtime state, all in-flight DPCs that could touch that state must be drained. Destructor ordering:

1. `KeFlushQueuedDpcs()`
2. `AoTransportOnStopEx(rt)` (zero monotonic/cursor state)
3. `AoTransportFreeStreamRt(rt)`

---

## 5. External contract

What the driver advertises to Windows. This is what the audio engine and WASAPI clients negotiate against.

### 5.1 INF declarations

Each cable endpoint advertises:

- `KSCATEGORY_AUDIO`
- `KSCATEGORY_REALTIME`
- `KSCATEGORY_RENDER` (speaker side) / `KSCATEGORY_CAPTURE` (mic side)
- `KSCATEGORY_TOPOLOGY` (topology filter)

Endpoint association: `KSNODETYPE_ANY` (matches VB convention; Windows shows it as a generic capture/render endpoint).

### 5.2 OEM Default Format

Advertised as `48000 Hz / 24-bit / Stereo` (`WAVEFORMATEXTENSIBLE`, subformat = `KSDATAFORMAT_SUBTYPE_PCM`). This is the format Windows Audio Engine adopts as the mix format in shared mode by default.

### 5.3 KSDATARANGE

Accept any rate / bit-depth / channel combination that the SRC function can handle:

- rates: 8000, 16000, 22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000 (all that are GCD-divisible by 300/100/75 against the registry SR)
- bits: 8, 16, 24, 32 (PCM int) and 32 (IEEE float)
- channels: 1, 2 (more later if needed)

Driver internal SRC handles whatever the client requests. Do not fall into the V1 "FormatMatch" trap.

### 5.4 Position contract (`GetPosition` / `GetPositions`)

Both return monotonic byte counters (`PlayPosition`, `WritePosition`) recalculated to the current QPC on every call, plus the QPC time. Implementation is `AoCableAdvanceByQpc(..., AO_ADVANCE_QUERY, 0)` followed by a read of the per-stream monotonic counters.

### 5.5 Packet contract (`GetReadPacket` / `SetWritePacket` / `GetPacketCount`)

Preserved for event-driven (notification-armed) clients only. Returns `STATUS_NOT_SUPPORTED` when `m_ulNotificationsPerBuffer == 0` (which is the shared-mode phone path). When packet mode is armed, packet bookkeeping is owned by the canonical helper, not by a separate timer.

---

## 6. Internal data structures

### 6.1 Ring buffer (`FRAME_PIPE`)

Owned by `Source/Utilities/loopback.cpp`. One per cable (A, B).

Key fields (target shape, mirroring VB layout):

| Field | Type | Meaning |
|---|---|---|
| `TargetLatencyFrames` | int | frames the ring tries to maintain (registry-driven; default ~7168 at 48k) |
| `WrapBound` | int | current ring depth in frames (reconciled toward TargetLatencyFrames per op) |
| `FrameCapacityMax` | int | hard upper bound (registry max, e.g. 32768) |
| `Channels` | int | internal channel slot count (16 to match VB) |
| `WritePos` | int | render-side fill cursor, frame index, wraps at WrapBound |
| `ReadPos` | int | capture-side drain cursor, frame index, wraps at WrapBound |
| `OverflowCounter` | int | write rejected (writes refused) — incremented, never resets except on Stop |
| `UnderrunCounter` | int | read requested more than available |
| `UnderrunFlag` | bool | recovery state (silence delivered until refill ≥ WrapBound/2) |
| `Data` | INT32[] | sample storage, frames × channels × 4 bytes |

### 6.2 Per-stream runtime (`AO_STREAM_RT`)

Owned by `Source/Utilities/transport_engine.h`. Allocated on first RUN, freed in stream destructor.

Key fields (cable subset; VB offset reference in parentheses):

| Field | Meaning |
|---|---|
| `IsCapture`, `IsCable`, `IsSpeakerSide` | classification flags |
| `Active` | RUN / PAUSE / STOP scheduling state |
| `SampleRate`, `Channels`, `BlockAlign`, `BitsPerSample` | client format snapshot |
| `Pipe` | non-owning pointer to the cable's `FRAME_PIPE` |
| `DmaBuffer`, `DmaBufferSize` | non-owning DMA window |
| `RingSizeFrames` | ring frame capacity for cursor wrap math |
| `AnchorQpc100ns` (VB +0x180) | reference time for elapsed-frames calc |
| `PublishedFramesSinceAnchor` (+0x198) | already-reported frames since anchor; advance = elapsed - this |
| `DmaCursorFrames` (+0xD0) | current position inside DMA, frame-denominated |
| `MonoFramesLow` (+0xE0) | monotonic frames produced/consumed (PlayPos in frames) |
| `MonoFramesMirror` (+0xE8) | mirror of Low (WritePos in frames) |
| `LastAdvanceDelta` (+0x1B8) | most recent advance count, for diagnostics |
| `StatOverrunCounter` | helper-level reject count |
| `NotifyBoundaryBytes` (+0x7C) | armed packet boundary |
| `NotifyArmed` (+0x164), `NotifyFired` (+0x165) | packet notification state |
| `CableScratchBuffer`, `CableScratchSize` | per-stream linear scratch for DMA staging |
| `FadeSampleCounter` | -96..0 fade-in envelope state for click suppression |

### 6.3 Engine (`AO_TRANSPORT_ENGINE`)

Owned by `Source/Utilities/transport_engine.cpp`. Single global instance.

Responsibilities:

- shared timer creation/destruction (`ExAllocateTimer` / `ExSetTimer`, period 1 ms)
- active-stream registry (snapshot list under `EngineLock`)
- per-tick dispatch into `AoCableAdvanceByQpc(..., AO_ADVANCE_TIMER_*, ...)` for each active cable stream
- 63/64 drift correction state

Engine **must not** become a second transport owner. It is one of the call sources that funnels into the canonical helper.

### 6.4 What stays on `CMiniportWaveRTStream`

- KS state (`m_KsState`)
- WaveRT-visible packet contract fields (kept physically but logically owned by the helper):
  - `m_llPacketCounter`
  - `m_ulLastOsReadPacket`
  - `m_ulLastOsWritePacket`
  - `m_ulCurrentWritePosition`
- format / block-align / sample-rate already required by the miniport contract
- non-cable legacy fields (untouched)
- `m_pTransportRt` — the runtime allocation

What **leaves** `CMiniportWaveRTStream` (cable streams):

- per-stream `m_pNotificationTimer` ownership of cable transport
- per-stream `m_pDpc` ownership of cable transport
- precomputed `m_ullLinearPosition += ByteDisplacement` cable truth

---

## 7. Canonical pipeline

### 7.1 Helper signature & reasons

```c
typedef enum _AO_ADVANCE_REASON {
    AO_ADVANCE_QUERY         = 0,  // GetPosition / GetPositions
    AO_ADVANCE_TIMER_RENDER  = 1,  // shared timer DPC, render stream
    AO_ADVANCE_TIMER_CAPTURE = 2,  // shared timer DPC, capture stream
    AO_ADVANCE_PACKET        = 3,  // event-driven WaveRT packet surface
} AO_ADVANCE_REASON;

VOID AoCableAdvanceByQpc(AO_STREAM_RT* rt,
                         ULONGLONG nowQpcRaw,
                         AO_ADVANCE_REASON reason,
                         ULONG flags);
```

### 7.2 Helper body (target shape)

```c
AoCableAdvanceByQpc(rt, nowQpcRaw, reason, flags) {
    lock(rt->PositionLock);

    apply_drift_correction(rt, nowQpcRaw);
    nowQpc100ns = AoQpcTo100ns(nowQpcRaw);

    elapsedFrames =
        ((nowQpc100ns - rt->AnchorQpc100ns) * rt->SampleRate) / 10000000ULL;
    advance = elapsedFrames - rt->PublishedFramesSinceAnchor;

    if (advance < 8) { unlock; return; }   // 8-frame gate

    if (elapsedFrames >= ((uint64_t)rt->SampleRate << 7)) {  // 127s rebase
        rt->PublishedFramesSinceAnchor = 0;
        rt->AnchorQpc100ns = nowQpc100ns;
    }

    if ((uint32_t)advance > (rt->SampleRate / 2)) {  // overrun guard
        AoHandleAdvanceOverrun(rt, reason, advance);
        unlock; return;
    }

    if (!rt->IsCapture) {
        bytes = elapsedFrames * rt->BlockAlign;
        linearize_dma_window_to_scratch(rt, bytes);
        FramePipeWriteFromScratch(rt->Pipe, rt->ScratchBuffer, bytes);
        AoApplyFadeEnvelope(rt->ScratchBuffer, elapsedFrames, &rt->FadeSampleCounter);
        advance_render_cursors(rt, elapsedFrames, bytes);
    } else {
        bytes = elapsedFrames * rt->BlockAlign;
        apply_startup_gate_if_needed(rt, elapsedFrames);
        apply_capture_boundary_alignment_if_needed(rt, &bytes);
        FramePipeReadToDma(rt->Pipe, rt->DmaBuffer, ..., bytes);
        apply_partial_zero_tail_if_needed(rt, ...);
        advance_capture_cursors(rt, elapsedFrames, bytes);

        if (rt->NotifyArmed && !rt->NotifyFired &&
            ((rt->DmaCursor % rt->RingSizeFrames) == rt->NotifyBoundaryBytes)) {
            rt->NotifyFired = 1;
            AoInvokePortclsNotify(rt->PortNotifyCtx, 8);
        }
    }

    rt->MonoFramesLow    += elapsedFrames;
    rt->MonoFramesMirror += elapsedFrames;
    rt->LastAdvanceDelta = elapsedFrames;
    rt->PublishedFramesSinceAnchor = (uint32_t)elapsedFrames;

    unlock(rt->PositionLock);
}
```

### 7.3 Render path (speaker → ring)

1. WASAPI writes into the WaveRT-mapped DMA buffer (driver does not see this directly; the buffer is shared memory).
2. Helper invocation (timer tick or query):
   - compute `bytes = advance * BlockAlign`
   - copy `bytes` from `DmaBuffer + DmaCursor` to `CableScratchBuffer` (handle wrap)
   - call ring write SRC: scratch → ring (one atomic call per advance)
   - apply fade envelope on the freshly written ring samples (click suppression at packet boundaries)
   - advance `DmaCursor`, monotonic counters

Single producer of WritePos: this helper.

### 7.4 Capture path (ring → mic)

1. Helper invocation:
   - compute `bytes = advance * BlockAlign`
   - call ring read SRC: ring → scratch
   - if available < requested → silence-fill, set UnderrunFlag
   - if recovery (UnderrunFlag == 1 && available < WrapBound/2) → keep silence-filling
   - copy scratch → DMA buffer at `DmaCursor` (handle wrap)
   - advance `DmaCursor`, monotonic counters
2. WASAPI reads directly from the WaveRT-mapped DMA buffer (driver doesn't see the consumer).

Single owner of ReadPos: the ring read function called from this helper.

### 7.5 Cable A vs Cable B

Independent rings, independent runtime. Same code path. Cable A's render writes Cable A's ring; Cable A's capture reads Cable A's ring. Cable B is symmetric and isolated.

---

## 8. State machine

KS states the cable streams transition through: `KSSTATE_STOP`, `KSSTATE_ACQUIRE`, `KSSTATE_PAUSE`, `KSSTATE_RUN`.

### 8.1 RUN (entry)

- allocate `AO_STREAM_RT` if null
- snapshot format → fill `SampleRate`/`Channels`/`BlockAlign`/`BitsPerSample`
- snapshot DMA buffer pointer/size
- snapshot pipe pointer (pick correct `FRAME_PIPE` based on `eDeviceType`)
- snapshot `RingSizeFrames`
- allocate `CableScratchBuffer` (size = `(SampleRate/2) * BlockAlign` worst case)
- record `AnchorQpc100ns = KeQueryPerformanceCounter`
- zero `PublishedFramesSinceAnchor`, `MonoFramesLow/Mirror`, `DmaCursorFrames`
- mark `Active = TRUE`
- register with engine (engine `RefCount++`, add to active list, arm shared timer if first)

### 8.2 PAUSE

- mark `Active = FALSE`
- unregister from engine active list (engine timer may auto-stop if last)
- runtime allocation **retained** for resume
- ring is **not** cleared (next RUN can resume)

### 8.3 STOP

- mark `Active = FALSE`
- unregister from engine active list
- `AoCableResetRuntimeFields(rt)` — zero monotonic counters, anchor QPC, DMA cursor, fade state
- ring is **reset** via `FramePipeResetCable(...)` (matches VB lifecycle clearing on stop/close)

### 8.4 Destructor

```
KeFlushQueuedDpcs();              // drain any in-flight engine DPC touching this stream
AoTransportOnStopEx(rt);          // idempotent stop (safe even if never RUN)
AoTransportFreeStreamRt(rt);      // free runtime (refcount-aware; engine may hold ref briefly)
```

If the engine timer DPC has a temporary ref to this RT, the free is deferred until the DPC releases; the destructor returns immediately because `KeFlushQueuedDpcs` already ensured the DPC has finished.

---

## 9. Threading & locking

### 9.1 Lock hierarchy (top → bottom; never invert)

1. **Engine lock** (`AO_TRANSPORT_ENGINE::Lock`) — protects active-stream list, refcount discipline, timer state
2. **Per-stream position lock** (`m_PositionSpinLock` or future `AO_STREAM_RT::PositionLock`) — protects per-stream cursors, anchor QPC, fade state
3. **`FRAME_PIPE` lock** — protects ring read/write

Acquire in this order. Release in reverse. Never hold engine lock across long data movement.

### 9.2 Concurrency rules

- **DPC-only writers of `ReadPos`**: ring read functions are called from `AoCableAdvanceByQpc` only, which runs at `DISPATCH_LEVEL` from either the engine timer DPC or from within `m_PositionSpinLock` on the query path. WASAPI never advances `ReadPos`.
- **Engine timer DPC** snapshots active streams under engine lock, takes per-stream refs, drops engine lock, then calls the helper per stream under per-stream lock.
- **Query path** (`GetPosition` from PortCls) takes per-stream lock, calls helper, releases.
- The helper never blocks — no waits, no allocations, no paged-memory access at `DISPATCH_LEVEL`.

### 9.3 Lifecycle race avoidance

The engine timer DPC may be running with a stream ref while the destructor wants to free that stream. Discipline:

- engine takes `RefCount++` on each stream when snapshotting
- engine releases ref after the helper call returns
- destructor calls `KeFlushQueuedDpcs` → guarantees no DPC is in flight before `AoTransportFreeStreamRt`

---

## 10. Format conversion / SRC

### 10.1 GCD divisor selection

```
try iVar = 300:
    if (srcRate % 300 == 0) and (dstRate % 300 == 0): use 300
try iVar = 100: same
try iVar = 75:  same
else: return error -486 (unsupported rate pair)
```

Then `srcRatio = srcRate / iVar`, `dstRatio = dstRate / iVar`. Linear interpolation over weighted accumulator with these ratios.

### 10.2 Same-rate fast path

When `srcRate == dstRate`, ratio is 1:1. The helper falls through to direct copy (no interpolation arithmetic), still doing per-sample format normalization (8/16/24 → INT32, or direct INT32).

### 10.3 4-way bit-depth dispatch

In one place — the ring write SRC. No layered conversion. Branch on `BitsPerSample`:

| Branch | Operation |
|---|---|
| `8`  | `(byte - 0x80) << 11` → INT32 |
| `16` | `(int)short << 3`     → INT32 |
| `24` | `(asm 3 bytes << 8) >> 13` → INT32 (~19-bit) |
| `32` | direct copy (PCM int or IEEE float reinterpret) |

Read path (ring → client format) inverts:

| Branch | Operation |
|---|---|
| `8`  | `(int >> 11) + 0x80` → byte |
| `16` | `int >> 3`           → short |
| `24` | `int << 5` → write 3 packed bytes |
| `32` | direct copy |

### 10.4 Single SRC function per direction

Two functions, not one (shared decompile evidence shows VB has **two**, not a unified directional flag):

- `AoRingWriteFromScratch(ring, scratch, frames, srcRate, srcChannels, srcBits)` — write SRC
- `AoRingReadToScratch(ring, scratch, frames, dstRate, dstChannels, dstBits)`   — read SRC

Each is a single atomic call from the caller's point of view. The read function adds underrun handling (silence fill + recovery flag).

### 10.5 Channel handling

Internal ring is 16-channel (matches VB capacity for the multi-channel pin). Mono input → write to first slot, others zero. Stereo → fill slots 0 and 1, others zero. Read path takes the configured client channel count and packs the planar slots.

(Detail: VB stores channel-planar internally. AO will mirror this. If we pick interleaved instead, document the deviation here.)

---

## 11. Position & notification

### 11.1 Position freshness

Every cable `GetPosition` and `GetPositions` call invokes `AoCableAdvanceByQpc(rt, KeQueryPerformanceCounter(), AO_ADVANCE_QUERY, 0)` under the position spinlock **before** returning. After this call, `MonoFramesLow * BlockAlign` is the up-to-date `PlayPosition` (in bytes), and `MonoFramesMirror * BlockAlign` is the `WritePosition`.

This means position values are accurate to within the time it takes to run the helper (sub-microsecond), not within 1ms (the timer tick).

### 11.2 Packet notification (provisional, shared-mode bypass)

VB-Cable's notification dispatch (`+0x8188 edx=8`) is **gated** behind `NotifyArmed`. The shared-mode Phone Link path never arms notification, so the dispatch never fires, and position polling is sufficient.

For AO:

- shared-mode cable streams: `m_ulNotificationsPerBuffer == 0` → packet APIs return `STATUS_NOT_SUPPORTED` (current AO behavior, kept)
- event-driven cable streams (when arming exists): the helper sets `NotifyFired` and invokes the PortCls notify when the DMA cursor crosses `NotifyBoundaryBytes`

### 11.3 Click suppression (fade-in envelope)

VB applies a 95-entry fade-in coefficient table at packet-boundary transitions. AO mirrors this:

- 96-entry table (max usable index 95) in `transport_engine.cpp`
- `FadeSampleCounter` reset to `-96` at packet boundary
- per-sample: `sample = (sample * coef[clamped_index]) >> 7`
- counter advances per sample until saturated at 0

Applied on freshly written render samples in scratch before pipe write.

---

## 12. Diagnostics

### 12.1 IOCTL_AO_GET_STREAM_STATUS payload (V2/V3 plan)

Today: `AO_STREAM_STATUS` (V1, always present) + `AO_V2_DIAG` (V2, present if buffer large enough).

After the rewrite, evolve `AO_V2_DIAG` to expose:

- `<Cable>_<R/C>_OverflowCount` — ring write rejected (helper or pipe write)
- `<Cable>_<R/C>_UnderrunCount` — ring read insufficient (silence delivered)
- `<Cable>_<R/C>_DropEvents` — DMA overrun guard hits
- `<Cable>_<R/C>_LateEventCount` — engine tick fired late
- `<Cable>_<R/C>_PipeFillFrames` — current ring fill (writePos − readPos, wrap-corrected)
- `<Cable>_<R/C>_PublishedFrames` — monotonic frame counter
- `<Cable>_<R/C>_AdvanceMin/Max/Avg` — per-tick advance distribution

Bump the C_ASSERT, version field at struct head.

### 12.2 KdPrint discipline

Per-call DbgPrint is too intrusive (drowns serial KDNET). Use:

- 1 Hz throttled prints in the engine timer DPC
- one-shot prints on RUN/PAUSE/STOP transitions
- counter snapshots inside the helper, formatted occasionally

### 12.3 `test_stream_monitor.py`

User-mode IOCTL consumer. Polls `IOCTL_AO_GET_STREAM_STATUS` at ~10 Hz, prints differences. Must be updated together with `Source/Main/ioctl.h` whenever the V2_DIAG schema changes (Diagnostics Rule, see CLAUDE.md).

---

## 13. Code organization

### 13.1 File ownership

| File | Owns | Does NOT own |
|---|---|---|
| `Source/Main/adapter.cpp` | engine init/teardown, IOCTL dispatch | transport policy |
| `Source/Main/minwavertstream.cpp` | WaveRT-facing entrypoints (Init, SetState, GetPosition, GetReadPacket, SetWritePacket), call-site bridging into helper | transport math, ring access |
| `Source/Main/minwavertstream.h` | stream class + Phase 6 `m_pTransportRt` | runtime fields beyond minimum |
| `Source/Main/ioctl.h` | IOCTL codes, `AO_V2_DIAG` schema | implementation |
| `Source/Utilities/transport_engine.h` | `AO_STREAM_RT`, `AO_TRANSPORT_ENGINE`, helper API | WaveRT semantics |
| `Source/Utilities/transport_engine.cpp` | helper implementation, fade envelope, engine timer DPC, drift correction | ring internals |
| `Source/Utilities/loopback.cpp` | `FRAME_PIPE`, ring write/read SRC, format dispatch | transport ownership, lifecycle |
| `Source/Utilities/loopback.h` | `FRAME_PIPE` API | per-stream state |

### 13.2 New / modified key APIs

```c
// transport_engine.h
VOID AoCableAdvanceByQpc(AO_STREAM_RT* rt, ULONGLONG nowQpcRaw,
                         AO_ADVANCE_REASON reason, ULONG flags);
VOID AoApplyFadeEnvelope(LONG* samples, ULONG count, LONG* fadeCounter);
VOID AoResetFadeCounter(AO_STREAM_RT* rt);
VOID AoCableResetRuntimeFields(AO_STREAM_RT* rt);

// loopback.h
NTSTATUS AoRingWriteFromScratch(PFRAME_PIPE pipe, const BYTE* scratch,
                                ULONG frames, ULONG srcRate, ULONG srcChannels,
                                ULONG srcBits);
NTSTATUS AoRingReadToScratch(PFRAME_PIPE pipe, BYTE* scratch,
                             ULONG frames, ULONG dstRate, ULONG dstChannels,
                             ULONG dstBits);
ULONG    AoRingAvailableFrames(PFRAME_PIPE pipe);
VOID     FramePipeResetCable(PFRAME_PIPE pipe);
```

---

## 14. Migration: keep / retire / rewrite

### 14.1 Keep

- PortCls / WaveRT framework usage (`PcAddAdapterDevice`, miniport interface implementations)
- subdevice / filter / pin / topology registration
- INF / install / signing / service scaffolding
- non-cable stream behavior (untouched)
- `FRAME_PIPE` infrastructure (will be heavily modified for INT32, not removed)
- `AO_TRANSPORT_ENGINE` / `AO_STREAM_RT` skeleton + lifecycle (Step 1 work)
- `m_pTransportRt` member on stream
- destructor discipline (`KeFlushQueuedDpcs` + lifecycle)
- IOCTL surface (extend `AO_V2_DIAG`, do not change codes)

### 14.2 Retire (cable streams only)

Existing AO code that must stop owning cable transport:

- `CMiniportWaveRTStream::UpdatePosition` cable branch — becomes thin shim or no-op for cable (other streams keep it)
- `CMiniportWaveRTStream::ReadBytes` cable usage — replaced by `AoCableAdvanceByQpc` render branch
- `CMiniportWaveRTStream::WriteBytes` cable usage — replaced by `AoCableAdvanceByQpc` capture branch
- `m_pNotificationTimer` cable transport ownership — engine shared timer is the wake source
- `m_pDpc` cable transport ownership — engine DPC handles it
- `m_ullLinearPosition += ByteDisplacement` cable truth — replaced by `MonoFramesLow * BlockAlign`
- `MicSink` dual-write (if still present in `loopback.cpp`) — removed
- `m_ulPump*` Phase 5 instrumentation fields — removed (not the pump model anymore)
- `AO_PUMP_FLAG_*` runtime flags — removed (pump model retired)
- 8-tap sinc SRC + 2048 coefficient table — replaced by linear interpolation
- packed 24-bit ring storage — replaced by INT32 frame-indexed
- silent overflow handling — replaced by hard reject + counter

### 14.3 Rewrite

| Component | Current | Target |
|---|---|---|
| `FRAME_PIPE` storage | packed 24-bit, byte-indexed | INT32, frame-indexed |
| `FRAME_PIPE` overflow | silent overwrite | hard reject + counter |
| Format conversion | 4-stage pipeline | single ring-write SRC |
| Cable transport ownership | `UpdatePosition` periodic | `AoCableAdvanceByQpc` query+timer hybrid |
| Position freshness | last-DPC value | recalculated per query |
| Timer | `KeSetTimerEx` + per-stream DPC | `ExAllocateTimer` shared, 1ms |

---

## 15. Implementation stages

> This replaces all previous Phase 5 / Step 3-4 / Option Z / Y1A-Y4 naming. Stages are scope-defined, not phase-numbered, so re-attempts within a stage don't bump nomenclature.

### Stage 0 — Baseline & evidence

**Status: complete.**

- VB Ghidra decompile dumped (12096 lines)
- VB WinDbg dynamic analysis recorded
- Capture contract verified
- AO vs VB live-call comparison: VB clean, AO garbled
- Branch consolidation: single branch `feature/ao-fixed-pipe-rewrite`, single worktree
- This document.

### Stage 1 — Ring rewrite (INT32, frame-indexed, hard-reject)

**Goal:** replace `FRAME_PIPE` internals to match VB's INT32 frame-indexed model, including hard-reject overflow.

**Edits:**

- `Source/Utilities/loopback.h` — `FRAME_PIPE` struct: change sample storage to INT32 array, swap byte indexing for frame indexing, add `OverflowCounter`, `UnderrunCounter`, `UnderrunFlag`, `WrapBound`, `TargetLatencyFrames` fields
- `Source/Utilities/loopback.cpp` — rewrite ring write/read entry points; rewrite format normalization (8/16/24/32 → INT32, 19-bit normalized); replace silent overwrite with hard reject; expose `AoRingAvailableFrames`, `FramePipeResetCable`

**Gate:**

- builds cleanly
- existing transport callers still work (they call write/read APIs whose signatures are stable)
- local loopback (sine tone) survives one minute without crash
- diagnostic counters expose ring fill / overflow / underrun

**No audible regression** — this stage doesn't change cadence/ownership, only ring internals.

### Stage 2 — Single-pass SRC

**Goal:** replace 4-stage conversion pipeline with one ring-write-SRC and one ring-read-SRC, using GCD linear interpolation.

**Edits:**

- `Source/Utilities/loopback.cpp` — implement `AoRingWriteFromScratch` (linear interpolation + 4-way bpp dispatch) and `AoRingReadToScratch` (inverse + underrun handling)
- remove old `ConvertToInternal`, `SrcConvert`, `ConvertFromInternal`, sinc table

**Gate:**

- 48k/16/2 → 96k internal → 48k/16/2 round-trip is bit-stable on a tone
- 8k/16/1 → 96k internal → 48k/16/2 round-trip is intelligible
- no static / glitches in local loopback

### Stage 3 — Canonical advance helper (shadow)

**Goal:** implement `AoCableAdvanceByQpc` body fully, hook all cable call sources to it in shadow mode (helper computes everything but the audible truth still comes from legacy `UpdatePosition` for cable).

**Edits:**

- `Source/Utilities/transport_engine.cpp` — full body of `AoCableAdvanceByQpc`, including drift correction, gate, overrun guard, scratch linearization, fade envelope helpers
- `Source/Main/minwavertstream.cpp` — `GetPosition`, `GetPositions`, engine timer DPC: each invokes the helper with appropriate `AO_ADVANCE_REASON`
- shadow-mode flag (helper computes shadow state but does not write to ring or DMA yet)

**Gate:**

- local loopback unchanged (no audible regression)
- diagnostic counters show helper hits from query + timer paths
- helper-computed advance ≈ legacy advance within 8-frame gate tolerance

### Stage 4 — Render coupling

**Goal:** flip cable render audible ownership from legacy `UpdatePosition` → `ReadBytes` to the helper's render branch (DMA → scratch → ring + fade).

**Edits:**

- `Source/Utilities/transport_engine.cpp` — wire render branch to do the actual DMA → scratch → ring write (was shadow-only)
- `Source/Main/minwavertstream.cpp` — cable render branch in `UpdatePosition` becomes no-op (`!IsCable` keeps legacy)

**Gate:**

- local loopback no worse than Stage 3
- live call render path (TTS to phone) audible without static
- ring overflow counter remains 0 during steady-state (any non-zero is a warning)

### Stage 5 — Capture coupling

**Goal:** flip cable capture audible ownership to the helper's capture branch (ring → scratch → DMA + underrun handling + startup gate).

**Edits:**

- `Source/Utilities/transport_engine.cpp` — wire capture branch
- `Source/Main/minwavertstream.cpp` — cable capture branch in `UpdatePosition` becomes no-op

**Gate:**

- live call capture path (phone audio to AI) — transcription accuracy returns to VB level
- mid-call chopping reduced
- startup clipping not worse than Stage 4

### Stage 6 — Cleanup

**Goal:** remove retired cable code (Section 14.2). Delete dead Phase 5 / Step 3-4 scaffolding. Strip stale comments.

**Edits:**

- delete `m_ulPump*` fields from `minwavertstream.h`
- delete `AO_PUMP_FLAG_*` constants
- delete `IOCTL_AO_SET_PUMP_FEATURE_FLAGS` (or downgrade to no-op for compatibility)
- delete legacy 8-tap sinc remnants
- update CLAUDE.md, CURRENT_STATE.md, V2_RESEARCH_INDEX.md

**Gate:**

- driver functionality identical to Stage 5
- code reads cleanly without "Phase X" comments scattered everywhere
- diagnostic surface stable

### Stage 7 — Quality polish (post-rewrite)

After Stages 1–6 land, work on:

- broader format/channel acceptance (more SR pairs, multi-channel)
- AMD / telephony category metadata (KSNODETYPE_MICROPHONE etc.) for Phone Link auto-selection
- Control Panel UX
- benchmark suite improvements

These were already deferred behind "Phase 6 lands cleanly" in `CURRENT_STATE.md` and remain so.

---

## 16. Validation

### 16.1 Per-stage validation

Each stage above has a gate. Don't advance to the next stage until the gate passes.

### 16.2 Standard test sequence

1. **build** — `.\build-verify.ps1 -Config Release` succeeds, both `.sys` files generated and signed
2. **install** — `.\install.ps1 -Action upgrade` succeeds, no BSOD on driver load
3. **smoke** — Cable A/B endpoints visible in Sound Settings, default-device switching works
4. **local loopback** — sine tone written to Cable A speaker, captured from Cable A mic, no static, position counters monotonic
5. **diagnostics** — `python test_stream_monitor.py` shows expected fields, no overflow/underrun during steady-state
6. **live call** — `tests/live_call/run_test_call.py` with `AUDIO_CABLE_PROFILE=ao` then `=vb`. Compare quality. AO must reach VB's level by Stage 5.

### 16.3 Pre-experiment regression checks (CLAUDE.md "엄수" rules)

- **Phone Link connection check** — actual phone connection confirmed
- **Cable check** — system default = AO Cable A/B (not VB)
- if either fails, the run is invalid

---

## 17. Failure modes & lessons

What went wrong before, and how this design avoids it.

### 17.1 Phase 5 (timer-owned transport) failure

**What:** moved cable transport ownership to a separate timer engine, leaving query path advancing accounting and timer path moving audio independently.

**Why it failed:** two independent ownership paths created stale-cursor reads and race-like artifacts. Audio became chopped/screechy.

**This design avoids:** Section 4.7 (canonical helper) — every active call source funnels into the same function. Engine timer is a wake source, not a transport owner.

### 17.2 Phase 6 Step 3/4 (publish-cursor pattern) failure

**What:** UpdatePosition published a cursor, the engine event runner moved data later based on that cursor.

**Why it failed:** "publish state for another cadence to move later" splits ownership. Same root cause as Phase 5, different shape.

**This design avoids:** the helper does QPC delta + frame gate + cursor advance + transport in one critical section. There is no "publish then move later" pattern.

### 17.3 Y3 capture migration attempts (rolled back to Y2-2)

**What:** Y3-v1..v8 each tried different cadence/cursor models for capture transfer. All produced degraded audio.

**Why it failed:** capture path needs the same coupled-helper model as render, but the attempts kept introducing partial decoupling (e.g., capture tick accumulator, query-driven only, timer-only).

**This design avoids:** Section 7 — capture path goes through the exact same helper, with the same advance math, just a different branch internally. No special capture cadence.

### 17.4 Front-chopping / mid-call chopping (Phase 5c)

**What:** 40ms / 300ms startup headroom prefill experiments. Front clipping fixed but mid-call chopping persisted.

**Why partial fix:** the headroom prefill papered over startup but did not address writer-reader rate mismatch in steady state. The deeper issue was the 4-stage conversion pipeline corrupting samples in flight.

**This design avoids:** Section 4.3 (single SRC), Section 4.1 (INT32 ring) — corruption-free path means rate matching becomes an arithmetic problem, not a quality problem.

### 17.5 Forced FormatMatch (V1)

**What:** required Speaker == Mic == Internal at 48k/24/8ch. Phone Link's default 48k/16/2 didn't match → Windows SRC'd it to driver format → driver did internal SRC → double conversion.

**Why it failed:** every shared-mode client paid a double SRC cost. Quality lost.

**This design avoids:** Section 5.3 — KSDATARANGE accepts client-native formats. Driver's internal SRC handles the difference once.

---

## 18. Risk table

| Risk | Why it matters | Mitigation |
|---|---|---|
| Helper becomes long, holds per-stream lock too long | DPC latency suffers, other clients see late position | Keep helper body tight; do conversions outside lock when possible |
| Engine timer becomes second owner again | Recreates Phase 5 / Step 3-4 regression | Section 4.7 hard rule: every reason value funnels into the helper, no separate transport function |
| Wrong INT32 normalization (sign extension off) | Audible distortion / DC bias | Stage 2 round-trip bit-stability gate |
| GCD divisor table misses a rate pair | Stream open fails at unusual rates | Document supported rates in Section 5.3; reject cleanly with `STATUS_NOT_SUPPORTED` |
| Fade envelope misapplied across packet boundary | Click suppression doesn't work | Stage 4 gate: forced packet-boundary discontinuity test |
| Packet notification regresses for event-driven clients | Some apps depend on `KeSetEvent` notify | Section 11.2: keep packet path armed-only; helper sets `NotifyFired` on boundary cross |
| `KeFlushQueuedDpcs` skipped in destructor | UAF if engine DPC holds stale stream pointer | Discipline in Section 8.4; `RefCount` discipline as second line of defense |
| `m_pNotificationTimer` deletion breaks non-cable streams | Non-cable still uses the timer | Section 14.2: "cable streams only" — non-cable code path untouched |
| Sample-rate change at runtime races against active stream | New rate vs cached `SampleRate` in `AO_STREAM_RT` | Re-snapshot on every RUN; reject mid-run rate changes (PortCls already handles via re-Init) |
| Diagnostics fields read with wrong size | Old `test_stream_monitor.py` reads new layout | Diagnostics Rule (CLAUDE.md): update `ioctl.h`/`adapter.cpp`/`test_stream_monitor.py` together; bump `StructSize` field |
| Wrong WrapBound vs FrameCapacityMax | Ring writes go OOB or starve | `FUN_140001180`-style reconciliation: clamp `WrapBound` to `[min(48), FrameCapacityMax]` on every op |
| Stage skipped to chase a fix | Half-implemented helper inside legacy code = worst of both worlds | Phase gates are mandatory; rollback within stage, never advance past gate |

---

## Appendix A — VB struct offset map

For implementers translating decompiled VB code references to AO `AO_STREAM_RT` fields. Source: `results/vbcable_pipeline_analysis.md` Section 11, cross-checked with `results/vbcable_capture_contract_answers.md`.

### Per-stream (CMiniportWaveRTStream-equivalent)

| VB offset | Field | AO equivalent |
|---|---|---|
| `+0x7C`  | NotifyBoundaryBytes      | `AO_STREAM_RT::NotifyBoundaryBytes` |
| `+0x86`  | BitsPerSample container | `AO_STREAM_RT::BitsPerSample` |
| `+0x8C`  | StreamSampleRate         | `AO_STREAM_RT::SampleRate` |
| `+0xA4`  | IsCapture flag           | `AO_STREAM_RT::IsCapture` |
| `+0xA8`  | DmaBufferSize            | `AO_STREAM_RT::DmaBufferSize` (bytes) |
| `+0xB0`  | DmaBuffer ptr            | `AO_STREAM_RT::DmaBuffer` |
| `+0xB8`  | RoutingType (mixing flag)| (not used; AO has only direct routing) |
| `+0xBC`  | KS state                 | `m_KsState` |
| `+0xD0`  | DmaCurrentPos (bytes)    | `AO_STREAM_RT::DmaCursorFrames` (note: AO uses frames) |
| `+0xD8`  | DmaPreviousPos           | `AO_STREAM_RT::DmaCursorFramesPrev` |
| `+0xE0`  | TotalBytesRead (PlayPos) | `AO_STREAM_RT::MonoFramesLow` (× BlockAlign for bytes) |
| `+0xE8`  | TotalBytesWritten (WritePos) | `AO_STREAM_RT::MonoFramesMirror` |
| `+0x164` | NotifyArmed              | `AO_STREAM_RT::NotifyArmed` |
| `+0x165` | NotifyFired              | `AO_STREAM_RT::NotifyFired` |
| `+0x178` | ScratchBuffer ptr        | `AO_STREAM_RT::CableScratchBuffer` |
| `+0x180` | BaselineQpc100ns         | `AO_STREAM_RT::AnchorQpc100ns` |
| `+0x198` | FramesReportedSinceBaseline | `AO_STREAM_RT::PublishedFramesSinceAnchor` |
| `+0x1B8` | LastAdvanceDelta / BytesThisTick | `AO_STREAM_RT::LastAdvanceDelta` |

### Per-ring (FRAME_PIPE-equivalent)

| VB offset | Field | AO equivalent |
|---|---|---|
| `+0x00`  | TargetLatencyFrames | `FRAME_PIPE::TargetLatencyFrames` |
| `+0x04`  | RequestedRate       | (kept as comment; AO drives from Channels/SR registry) |
| `+0x08`  | RingDataOffset      | (implicit in AO struct layout) |
| `+0x0C`  | FrameCapacityMax    | `FRAME_PIPE::FrameCapacityMax` |
| `+0x10`  | ChannelCount        | `FRAME_PIPE::Channels` |
| `+0x14`  | WrapBound           | `FRAME_PIPE::WrapBound` |
| `+0x18`  | WritePos (frames)   | `FRAME_PIPE::WritePos` |
| `+0x1C`  | ReadPos (frames)    | `FRAME_PIPE::ReadPos` |
| `+0x180` | OverflowCounter     | `FRAME_PIPE::OverflowCounter` |
| `+0x184` | UnderrunCounter     | `FRAME_PIPE::UnderrunCounter` |
| `+0x188` | UnderrunFlag        | `FRAME_PIPE::UnderrunFlag` |

### Engine-equivalent (`FUN_1400065b8` / `FUN_14000669c` register/unregister)

VB has a global 8-slot active-stream table + shared `ExAllocateTimer`. AO mirrors with `AO_TRANSPORT_ENGINE::ActiveStreams` list + `Timer` member. First-stream registration arms the timer, last-stream unregister destroys it.

---

## One-line summary

**Replace the cable-stream core: INT32 frame-indexed ring + single linear-interp SRC + canonical `AoCableAdvanceByQpc` helper that owns transport, accounting, and position freshness together. Keep the PortCls/WaveRT shell. Six implementation stages, each with a gate.**
