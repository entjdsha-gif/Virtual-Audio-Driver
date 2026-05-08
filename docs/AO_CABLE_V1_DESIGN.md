# AO Cable V1 Design

Status: active
Date: 2026-04-25

This file is the **how** companion to `docs/AO_CABLE_V1_ARCHITECTURE.md`.
Architecture explains the system shape and decisions; this file provides
implementation-ready specifics: file layout, struct fields, function
signatures, control flow.

If anything here disagrees with `docs/AO_CABLE_V1_ARCHITECTURE.md` or
`docs/ADR.md`, the higher-level doc wins; this file is updated to match.

---

## 1. File-level layout

### 1.1 Tracked source

```text
Source/
├── Main/
│   ├── adapter.cpp          PortCls init, IOCTL dispatch, engine bring-up
│   ├── adapter.h
│   ├── minwavert.cpp        IMiniportWaveRT + IPortClsRegistration glue
│   ├── minwavert.h
│   ├── minwavertstream.cpp  Per-stream WaveRT entrypoints
│   ├── minwavertstream.h
│   ├── basetopo.cpp         Topology helpers
│   ├── mintopo.cpp          IMiniportTopology
│   ├── common.cpp           Shared helpers
│   ├── ioctl.h              IOCTL codes + AO_V2_DIAG schema
│   ├── aocablea.inx         INF source for Cable A
│   ├── aocableb.inx         INF source for Cable B
│   ├── postbuild.bat        Post-build steps (signing etc.)
│   └── CableA.vcxproj       MSBuild project for Cable A (CABLE_A define)
│   └── CableB.vcxproj       MSBuild project for Cable B (CABLE_B define)
│
├── Filters/
│   ├── cablewavtable.h
│   ├── speakerwavtable.h
│   ├── micarraywavtable.h
│   ├── micarray1toptable.h
│   ├── speakertoptable.h
│   ├── micarraytopo.cpp
│   ├── speakertopo.cpp
│   ├── minipairs.h          Pin pair definitions
│
├── Utilities/
│   ├── transport_engine.cpp Canonical helper, fade, engine timer DPC
│   ├── transport_engine.h   AO_STREAM_RT, AO_TRANSPORT_ENGINE, helper API
│   ├── loopback.cpp         FRAME_PIPE, ring write/read SRC
│   ├── loopback.h           FRAME_PIPE struct, public API
│   ├── savedata.cpp         Audio capture-to-disk helper (not for cable)
│   ├── ToneGenerator.cpp    Tone generator (not for cable)
│   ├── kshelper.cpp         KS helpers
│   └── hw.cpp               Hardware abstraction layer
│
└── ControlPanel/             User-mode app reading IOCTL_AO_GET_STREAM_STATUS
```

### 1.2 Build outputs

`CABLE_A` define → `aocablea.sys` (service `AOCableA`, hardware id `ROOT\AOCableA`).
`CABLE_B` define → `aocableb.sys` (service `AOCableB`, hardware id `ROOT\AOCableB`).

Same source tree; conditional compilation paths in `minwavert.cpp` /
`minwavertstream.cpp` produce the per-cable identity.

---

## 2. Cable ring (`FRAME_PIPE`) detail

Defined in `Source/Utilities/loopback.h`. One per cable.

### 2.1 Struct shape (target after Stage 1 rewrite)

```c
typedef struct _FRAME_PIPE {
    // Lifetime
    KSPIN_LOCK  Lock;                  // protects all fields below

    // Internal pipe format — fixed at FramePipeInitCable time, never
    // changes for the life of the pipe. Both write SRC and read SRC
    // use this rate as their reference; client-side rates are converted
    // against it.
    ULONG       InternalRate;          // ring sample rate, Hz (registry-driven)
    USHORT      InternalBitsPerSample; // ring sample width — always 32 (INT32)
    LONG        InternalBlockAlign;    // = (InternalBitsPerSample/8) * Channels

    // Latency / capacity (frames)
    LONG        TargetLatencyFrames;   // registry-driven (typ. 7168 @ 48k)
    LONG        WrapBound;             // current ring depth, frames
    LONG        FrameCapacityMax;      // hard upper bound, frames
    LONG        Channels;              // typically 16 (planar slots)

    // Cursors (frames; wrap at WrapBound)
    LONG        WritePos;              // render-side fill cursor
    LONG        ReadPos;               // capture-side drain cursor

    // Statistics
    LONG        OverflowCounter;       // hard-reject hits
    LONG        UnderrunCounter;       // read-insufficient hits
    UCHAR       UnderrunFlag;          // 1 = in recovery (silence until WrapBound/2)

    // Persistent SRC state — must survive across calls so the linear-interp
    // accumulator does not reset at every chunk boundary. VB-Cable carries
    // the same kind of state per ring (RE: vbcable_capture_contract_answers.md
    // § 0, +0xB8 phase + +0xBC..+0xFB per-channel residual). 16 channel slots
    // mirror `Channels`.
    LONG        WriteSrcPhase;         // accumulator phase (write direction)
    LONG        WriteSrcResidual[16];  // per-channel residual carry
    LONG        ReadSrcPhase;          // accumulator phase (read direction)
    LONG        ReadSrcResidual[16];   // per-channel residual carry

    // Backing storage — INT32 array of size WrapBound * Channels
    LONG*       Data;
    SIZE_T      DataAllocBytes;
} FRAME_PIPE, *PFRAME_PIPE;
```

`InternalRate` is the single field used everywhere as the pipe's "internal
sample rate." Phase 1 Step 0 sets `InternalRate` and `InternalBitsPerSample`
at `FramePipeInitCable` time; subsequent code reads them as constants.

### 2.2 Public API (target)

```c
NTSTATUS FramePipeInitCable(PFRAME_PIPE pipe,
                            ULONG       internalRate,
                            LONG        channels,
                            LONG        initialFrames);

VOID     FramePipeFree(PFRAME_PIPE pipe);

VOID     FramePipeResetCable(PFRAME_PIPE pipe);   // STOP path: zero pos + flag

ULONG    AoRingAvailableFrames(PFRAME_PIPE pipe); // (WritePos - ReadPos) wrap-corrected

NTSTATUS AoRingWriteFromScratch(PFRAME_PIPE  pipe,
                                const BYTE*  scratch,
                                ULONG        frames,
                                ULONG        srcRate,
                                ULONG        srcChannels,
                                ULONG        srcBits);

NTSTATUS AoRingReadToScratch (PFRAME_PIPE pipe,
                              BYTE*       scratch,
                              ULONG       frames,
                              ULONG       dstRate,
                              ULONG       dstChannels,
                              ULONG       dstBits);
```

### 2.3 Write SRC algorithm (`AoRingWriteFromScratch`)

```c
acquire(pipe->Lock);

reconcile_wrapbound_to_target(pipe);  // grow/shrink WrapBound toward TargetLatencyFrames

// GCD divisor selection FIRST (so capacity check uses output ring frames).
// PickGCDDivisor takes the first candidate from the priority list
// [300, 100, 75] that divides both rates evenly (ADR-004 Decision
// step 1 — first-match, NOT "smallest divisor"). Returns:
//   STATUS_INVALID_PARAMETER — caller bug (zero rate, NULL out)
//   STATUS_NOT_SUPPORTED     — in-range rate pair with no matching
//                              divisor (ADR-008 § Consequences)
AO_GCD_RATIO ratio;
NTSTATUS gcdStatus = PickGCDDivisor(srcRate, pipe->InternalRate, &ratio);
if (!NT_SUCCESS(gcdStatus)) {
    release(pipe->Lock);
    return gcdStatus;
}
srcRatio = ratio.SrcRatio;
dstRatio = ratio.DstRatio;

// Capacity check uses OUTPUT ring frames — for 44.1k → 48k expansion,
// the actual ring write count is larger than the input frame count.
ringFramesToWrite = ((uint64_t)frames * dstRatio) / srcRatio;
available = AoRingAvailableSpaceFrames(pipe);  // WrapBound - currentFill - 2
                                               // (single source of guard)

if (ringFramesToWrite > available) {
    pipe->OverflowCounter += 1;
    release(pipe->Lock);
    return STATUS_INSUFFICIENT_RESOURCES;
}

// Per-channel linear-interp accumulator — uses pipe->WriteSrcPhase and
// pipe->WriteSrcResidual[] which persist across calls (VB parity).
for (i = 0; i < frames; ++i) {
    for (ch = 0; ch < min(srcChannels, pipe->Channels); ++ch) {
        sample = read_and_normalize_to_int19(scratch, srcBits, ch);  // 8/16/24/32 dispatch per § 2.5
        // weighted accumulator linear interp using pipe->WriteSrcResidual[ch]...
        write_int32_to_ring(pipe, slot(ch, frame), interpolated);
    }
    pipe->WritePos = (pipe->WritePos + 1) % pipe->WrapBound;
}

release(pipe->Lock);
return STATUS_SUCCESS;
```

### 2.4 Read SRC algorithm (`AoRingReadToScratch`)

Same structure, inverted direction.

```c
acquire(pipe->Lock);
reconcile_wrapbound_to_target(pipe);

// GCD divisor selection FIRST. Read direction maps pipe internal rate
// (src) onto client (dst) rate. Same first-match contract as the write
// path (ADR-004 Decision step 1 — first candidate from [300, 100, 75]
// that divides both rates wins; this is NOT "smallest divisor").
// PickGCDDivisor returns:
//   STATUS_INVALID_PARAMETER — caller bug (zero rate, NULL out)
//   STATUS_NOT_SUPPORTED     — in-range rate pair with no matching
//                              divisor (ADR-008 § Consequences)
AO_GCD_RATIO ratio;
NTSTATUS gcdStatus = PickGCDDivisor(pipe->InternalRate, dstRate, &ratio);
if (!NT_SUCCESS(gcdStatus)) {
    release(pipe->Lock);
    return gcdStatus;
}
srcRatio = ratio.SrcRatio;   // ring side  (= pipe->InternalRate / Divisor)
dstRatio = ratio.DstRatio;   // client side (= dstRate            / Divisor)

// `frames` is the OUTPUT (client) frame count requested. Convert to
// the INPUT (ring) frame count needed before any availability check —
// for 48k ring → 96k client (upsample read), 96 client frames need
// 48 ring frames (#2 from re-review of a038ad6). The naive
// `frames > AoRingAvailableFrames` check would falsely trip underrun
// when the ring has plenty.
ringFramesNeeded = ((uint64_t)frames * srcRatio) / dstRatio;

if (pipe->UnderrunFlag) {
    if (AoRingAvailableFrames(pipe) < pipe->WrapBound / 2) {
        zero_fill(scratch, frames * dstBlockAlign);
        release(pipe->Lock);
        return STATUS_SUCCESS;  // silence delivered
    }
    pipe->UnderrunFlag = 0;     // exit recovery
}

if (ringFramesNeeded > AoRingAvailableFrames(pipe)) {
    pipe->UnderrunCounter += 1;
    pipe->UnderrunFlag = 1;
    zero_fill(scratch, frames * dstBlockAlign);
    release(pipe->Lock);
    return STATUS_SUCCESS;
}

// SRC + format-specific denormalization per § 2.5
//   8 PCM:  (int >> 11) + 0x80
//   16 PCM: int >> 3
//   24 PCM: int << 5 then 3-byte pack
//   32 PCM: int << 13   (NOT direct copy — restores the 19-bit
//                        normalization shift applied on write)
// Uses pipe->ReadSrcPhase + pipe->ReadSrcResidual[] persistent state.
read_and_advance_readpos(pipe, scratch, frames, dstRate, dstChannels, dstBits);

release(pipe->Lock);
return STATUS_SUCCESS;
```

### 2.5 Bit-depth dispatch

The ring stores values in a **19-bit signed-magnitude** representation
(range ≈ `[-2^18, +2^18]`) regardless of client bit depth. This gives
~13 bits of headroom for the linear-interp accumulator and avoids
overflow across channel/rate combinations.

V1 supports the `KSDATAFORMAT_SUBTYPE_PCM` subtype only (see ADR-008
and § 8 below). `KSDATAFORMAT_SUBTYPE_IEEE_FLOAT` is **not** advertised
in `KSDATARANGE` and **must** be rejected at intersection time. The
dispatch table therefore covers PCM widths only:

| Bits | Read (scratch → INT32 ring) | Write (INT32 ring → scratch) |
|---|---|---|
| 8 PCM (unsigned) | `(byte - 0x80) << 11` | `(int >> 11) + 0x80` |
| 16 PCM (signed) | `(int)int16 << 3` | `int >> 3` |
| 24 PCM (signed, packed) | `((b0 \| (b1<<8) \| (b2<<16)) << 8) >> 13` | `int << 5` then pack 3 bytes |
| 32 PCM (signed) | `(int)int32 >> 13` | `int << 13` |

**32-bit PCM is *not* a direct copy** — full-range INT32 input must be
right-shifted by 13 to fit the 19-bit ring representation, and ring
samples must be left-shifted by 13 on read to restore full INT32 range.
Direct copy would let 32-bit PCM clients bypass the headroom invariant
and overflow the SRC accumulator (#7 from review of 8afa59a).

Branch on `bits` once, outside the per-frame loop, for performance.

---

## 3. Per-stream runtime (`AO_STREAM_RT`) detail

Defined in `Source/Utilities/transport_engine.h`. Allocated by
`AoTransportAllocStreamRt`; freed by `AoTransportFreeStreamRt`.

### 3.1 Field map

```c
typedef struct _AO_STREAM_RT {
    // Lifetime
    LIST_ENTRY              Link;                   // engine active list
    LONG                    RefCount;               // engine + owner
    PCMiniportWaveRTStream  Stream;                 // back-pointer (non-owning)
    KSPIN_LOCK              PositionLock;           // protects all advance state

    // Classification (set at first RUN; stable thereafter)
    BOOLEAN                 IsCable;
    BOOLEAN                 IsCapture;
    BOOLEAN                 IsSpeakerSide;
    BOOLEAN                 Active;                 // RUN/PAUSE/STOP

    // Format (snapshot at RUN)
    ULONG                   SampleRate;
    ULONG                   Channels;
    ULONG                   BlockAlign;             // bytes per frame, client side
    USHORT                  BitsPerSample;

    // DMA window (non-owning; owned by WaveRT)
    BYTE*                   DmaBuffer;
    ULONG                   DmaBufferSize;

    // Cable ring (non-owning; owned by FRAME_PIPE)
    PFRAME_PIPE             Pipe;
    ULONG                   RingSizeFrames;         // mirror of pipe WrapBound at RUN

    // Anchor / cursor (frames; QPC in 100ns)
    ULONGLONG               AnchorQpc100ns;
    ULONG                   PublishedFramesSinceAnchor;
    ULONGLONG               DmaCursorFrames;
    ULONGLONG               DmaCursorFramesPrev;

    // Monotonic accounting (frames; mirrors used for PlayPos / WritePos)
    volatile LONGLONG       MonoFramesLow;          // PlayPos source
    volatile LONGLONG       MonoFramesMirror;       // WritePos source
    LONG                    LastAdvanceDelta;       // diagnostic

    // Notification (only when armed; shared-mode never arms)
    ULONG                   NotifyBoundaryBytes;
    UCHAR                   NotifyArmed;
    UCHAR                   NotifyFired;

    // Scratch (per-stream linear staging)
    PVOID                   CableScratchBuffer;
    ULONG                   CableScratchSize;       // = (SampleRate/2) * BlockAlign

    // Click suppression
    LONG                    FadeSampleCounter;      // -96..0; -96 = pre-silence

    // Engine event sizing (for shared timer scheduling)
    ULONG                   FramesPerEvent;
    ULONG                   BytesPerEvent;
    LONGLONG                EventPeriodQpc;
    LONGLONG                NextEventQpc;

    // Statistics
    LONG                    StatOverrunCounter;     // helper-level overrun (skip)

    // Diagnostic shadow counters (Stage 3 shadow mode)
    volatile LONG           DbgShadowAdvanceHits;
    volatile LONG           DbgShadowQueryHits;
    volatile LONG           DbgShadowTimerHits;
} AO_STREAM_RT, *PAO_STREAM_RT;
```

### 3.2 Allocation lifecycle

```c
// First RUN entry
if (m_pTransportRt == NULL) {
    m_pTransportRt = AoTransportAllocStreamRt(this);   // RefCount = 1
}

snapshot.Rt           = m_pTransportRt;
snapshot.IsCapture    = m_bCapture;
snapshot.IsCable      = (m_pMiniport->m_DeviceType is one of eCable*);
snapshot.IsSpeakerSide = !m_bCapture;
snapshot.SampleRate   = m_pWfExt->Format.nSamplesPerSec;
snapshot.Channels     = m_pWfExt->Format.nChannels;
snapshot.BlockAlign   = m_pWfExt->Format.nBlockAlign;
snapshot.Pipe         = pick_pipe_for(m_pMiniport->m_DeviceType);
snapshot.DmaBuffer    = m_pDmaBuffer;
snapshot.DmaBufferSize = m_ulDmaBufferSize;

AoTransportOnRunEx(&snapshot);   // engine RefCount++ on this rt; add to active

// PAUSE  (per ADR-009: drain DPCs before mutating runtime state)
KeFlushQueuedDpcs();
AoTransportOnPauseEx(rt);        // mark inactive; keep allocation
                                 //   (RUN→PAUSE→RUN reuses ring; do NOT reset state)

// STOP   (per ADR-009: drain DPCs before mutating runtime state)
KeFlushQueuedDpcs();
AoTransportOnStopEx(rt);         // mark inactive; AoCableResetRuntimeFields(rt)
                                 //   (RUN→STOP→RUN starts fresh)

// Destructor — for the FreeAudioBuffer path that runs alongside, see § 5.4
//   Step A (query exclusion) → Step B (DPC exclusion) → Step C (DMA teardown)
KeFlushQueuedDpcs();
AoTransportOnStopEx(rt);          // idempotent
AoTransportFreeStreamRt(rt);      // RefCount-- ; frees rt if last (Step D)
```

`AoCableResetRuntimeFields` zeros: `MonoFramesLow`, `MonoFramesMirror`,
`PublishedFramesSinceAnchor`, `AnchorQpc100ns`, `DmaCursorFrames`,
`DmaCursorFramesPrev`, `FadeSampleCounter`, `NotifyArmed`, `NotifyFired`.
It does **not** zero `OverflowCounter`/`UnderrunCounter` (those persist for
diagnostics across pause cycles).

---

## 4. Engine (`AO_TRANSPORT_ENGINE`) detail

Defined in `Source/Utilities/transport_engine.h`. Single global instance
in `transport_engine.cpp`.

### 4.1 Struct

```c
typedef struct _AO_TRANSPORT_ENGINE {
    PEX_TIMER     Timer;
    KSPIN_LOCK    EngineLock;
    BOOLEAN       Initialized;
    BOOLEAN       Running;

    LONGLONG      PeriodQpc;          // 1 ms in QPC ticks per ADR-013
    LONGLONG      NextTickQpc;
    LONGLONG      LastTickQpc;        // for drift correction

    ULONG         ActiveStreamCount;
    LIST_ENTRY    ActiveStreams;

    // Drift-correction state (63/64 phase)
    ULONG         TickCounter;        // resets every 100
    LONGLONG      BaselineQpc;
} AO_TRANSPORT_ENGINE, *PAO_TRANSPORT_ENGINE;
```

### 4.2 Public API

```c
NTSTATUS AoTransportEngineInit(VOID);
VOID     AoTransportEngineCleanup(VOID);

PAO_STREAM_RT AoTransportAllocStreamRt(PCMiniportWaveRTStream stream);

/* Drops the caller's ref on `rt` (`InterlockedDecrement(&rt->RefCount)`).
 * Frees `rt` only if that drop brings RefCount to 0. Called from:
 *   - destructor (drops the owner ref; if engine ref already gone via
 *     AoTransportUnregister, this is the final release and frees);
 *   - timer DPC after the helper returns (drops the transient DPC ref;
 *     normally the engine + owner refs are still held, so does not free —
 *     but covers the destructor-races-DPC corner case).
 * There is **only one** deallocator: do not introduce
 * `AoTransportFreeStreamRtFinal` or any "Final" variant. */
VOID          AoTransportFreeStreamRt(PAO_STREAM_RT rt);

VOID AoTransportOnRunEx (const AO_STREAM_SNAPSHOT* snapshot);
VOID AoTransportOnPauseEx(PAO_STREAM_RT rt);
VOID AoTransportOnStopEx (PAO_STREAM_RT rt);

VOID AoCableAdvanceByQpc(PAO_STREAM_RT rt,
                         ULONGLONG     nowQpcRaw,
                         AO_ADVANCE_REASON reason,
                         ULONG         flags);

VOID AoApplyFadeEnvelope(LONG* samples, ULONG sampleCount, LONG* perStreamCounter);
VOID AoResetFadeCounter (PAO_STREAM_RT rt);

VOID AoCableResetRuntimeFields(PAO_STREAM_RT rt);
```

### 4.3 Timer DPC body (target shape)

```c
EVT_EX_TIMER AoTransportTimerCallback;

VOID AoTransportTimerCallback(PEX_TIMER timer)
{
    LARGE_INTEGER nowQpcRaw = KeQueryPerformanceCounter(NULL);

    // 63/64 phase correction (ADR-007 Decision 2). Engine-owned;
    // updates BaselineQpc / TickCounter / LastTickQpc. At
    // TickCounter == 100 it re-baselines (BaselineQpc = nowQpcRaw,
    // TickCounter = 0). The query path never enters this function
    // and never advances the phase counter.
    apply_drift_correction(&g_engine, nowQpcRaw.QuadPart);
    // Phase 3 Step 5 applies the corrected phase to timer
    // scheduling. The engine-global vs per-stream
    // (rt->NextEventQpc) deadline-model reconciliation is resolved
    // there; do not interpret g_engine.NextTickQpc as the active
    // deadline source until then.

    // Snapshot active list under engine lock; take ref on each
    KIRQL oldIrql;
    KeAcquireSpinLock(&g_engine.EngineLock, &oldIrql);
    snapshot_active_with_refs(&g_engine, snap, &count);
    KeReleaseSpinLock(&g_engine.EngineLock, oldIrql);

    // Iterate; render entries first, then capture (per VB pattern)
    for (i = 0; i < count; ++i) {
        rt = snap[i];
        reason = rt->IsCapture ? AO_ADVANCE_TIMER_CAPTURE
                               : AO_ADVANCE_TIMER_RENDER;
        AoCableAdvanceByQpc(rt, nowQpcRaw.QuadPart, reason, 0);
        AoTransportFreeStreamRt(rt);   // drops the transient DPC ref;
                                       //   frees if this was the last ref
                                       //   (destructor-races-DPC corner case)
    }
}
```

---

## 5. Stream-side WaveRT bridging

`Source/Main/minwavertstream.cpp` is the call-site that bridges WaveRT
contracts into the cable transport runtime.

### 5.1 `GetPosition` / `GetPositions` cable branch

```c
NTSTATUS GetPositions(...)
{
    KIRQL oldIrql;
    KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);

    if (is_cable_stream(this) && m_pTransportRt) {
        LARGE_INTEGER now = KeQueryPerformanceCounter(NULL);
        AoCableAdvanceByQpc(m_pTransportRt, now.QuadPart,
                            AO_ADVANCE_QUERY, 0);

        if (pllPlayPosition)
            *pllPlayPosition = m_pTransportRt->MonoFramesLow * m_pTransportRt->BlockAlign;
        if (pllWritePosition)
            *pllWritePosition = m_pTransportRt->MonoFramesMirror * m_pTransportRt->BlockAlign;
        if (plQpcTime)
            plQpcTime->QuadPart = now.QuadPart;
    } else {
        // non-cable: existing legacy path (unchanged)
    }

    KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);
    return STATUS_SUCCESS;
}
```

### 5.2 `SetState` cable branch

```c
NTSTATUS SetState(KSSTATE NewState)
{
    KSSTATE oldState = m_KsState;
    m_KsState = NewState;

    if (is_cable_stream(this)) {
        switch (NewState) {
        case KSSTATE_RUN:
            if (m_pTransportRt == NULL)
                m_pTransportRt = AoTransportAllocStreamRt(this);
            populate_snapshot(this, &snap);
            AoTransportOnRunEx(&snap);
            break;
        case KSSTATE_PAUSE:
            if (m_pTransportRt) {
                /* ADR-009: drain any DPC that has already snapshotted
                 * this rt before zeroing per-stream cursor state.
                 * Without this, the in-flight helper would write to a
                 * struct that OnPauseEx is concurrently zeroing. */
                KeFlushQueuedDpcs();
                AoTransportOnPauseEx(m_pTransportRt);
            }
            break;
        case KSSTATE_STOP:
            if (m_pTransportRt) {
                /* Same DPC-drain requirement as PAUSE; OnStopEx also
                 * resets SRC phase / residual on the FRAME_PIPE per
                 * Phase 1 Step 0. */
                KeFlushQueuedDpcs();
                AoTransportOnStopEx(m_pTransportRt);
            }
            break;
        case KSSTATE_ACQUIRE:
            // no-op for cable
            break;
        }
    } else {
        // non-cable: existing legacy path (unchanged)
    }

    return STATUS_SUCCESS;
}
```

### 5.3 Destructor

The destructor is one of the unwind paths that must execute the full
**Step A → Step D** contract from § 5.4. Do **not** simplify this
example: order matters for closing the query-path race, the
DPC-path race, and the DMA-buffer race in turn.

```c
~CMiniportWaveRTStream()
{
    if (m_pTransportRt == NULL) {
        // Stream never reached RUN, or already torn down. Nothing to do
        // for the cable transport state; fall through to remaining cleanup.
    } else {
        // --- Step A: exclude the query path ---
        // Acquire m_PositionSpinLock so any in-progress GetPosition /
        // GetPositions call (which holds this lock end-to-end across the
        // helper) finishes before we proceed. Atomically null out
        // m_pTransportRt so future query path calls see NULL and no-op.
        KIRQL psIrql;
        KeAcquireSpinLock(&m_PositionSpinLock, &psIrql);
        PAO_STREAM_RT rtSnapshot = m_pTransportRt;
        m_pTransportRt           = NULL;
        KeReleaseSpinLock(&m_PositionSpinLock, psIrql);

        // --- Step B: exclude the DPC path ---
        AoTransportUnregister(rtSnapshot);   // drops engine ref (2 → 1)
        KeFlushQueuedDpcs();                 // drain any DPC mid-helper-call
        ASSERT(rtSnapshot->RefCount == 1);   // owner-only state

        // Idempotent state reset before tearing down the DMA buffer.
        // Safe at RefCount == 1 because both paths are excluded.
        AoTransportOnStopEx(rtSnapshot);

        // --- Step C: tear down the DMA-buffer-shaped state ---
        rtSnapshot->DmaBuffer     = NULL;    // publish before unmap
        rtSnapshot->DmaBufferSize = 0;
        FreeAudioBuffer();                   // PortCls unmaps

        // --- Step D: drop the owner ref ---
        AoTransportFreeStreamRt(rtSnapshot); // RefCount: 1 → 0; frees rt
    }

    // remaining cleanup (port stream release, etc.)
}
```

The non-obvious ordering rule: `AoTransportFreeStreamRt` is the
**last** step on this path (Step D), not an early "while we're
cleaning up the rt" call. Calling Free before Step C would let
PortCls unmap the DMA buffer while a stale pointer still lived
inside `rtSnapshot`; calling it before Step B would let an in-flight
DPC dereference freed memory. (Round 7 finding #1 caught a previous
draft that did Free before DMA cleanup.)

### 5.4 `FreeAudioBuffer` ordering (DMA buffer lifetime)

`AO_STREAM_RT::DmaBuffer` is a non-owning pointer into the WaveRT-mapped
DMA buffer; PortCls owns the allocation via
`IMiniportWaveRTStream::AllocateAudioBuffer` /
`IMiniportWaveRTStream::FreeAudioBuffer`. If `FreeAudioBuffer` runs
while a transport DPC or canonical helper is mid-call holding a transient
ref to the same `AO_STREAM_RT`, the DPC will dereference an unmapped
buffer (use-after-free, BSOD).

### Helper invocation paths and how each is excluded from teardown

Two paths reach `AoCableAdvanceByQpc(rt, ...)`:

1. **Engine timer DPC.** Snapshots the active list under `EngineLock`,
   `RefCount++` per snapshotted rt, releases `EngineLock`, calls the
   helper, then `RefCount--`. The transient ref is taken under engine
   lock, so it is observable via `RefCount`.
2. **Query path.** `CMiniportWaveRTStream::GetPosition` /
   `GetPositions` (and the no-op `UpdatePosition` cable shim from
   Phase 4 Step 1) hold the stream's `m_PositionSpinLock` end-to-end
   across the helper call. They read `m_pTransportRt` *under that
   lock* and pass it to the helper. The query path **does not**
   touch `RefCount` and **does not** acquire `EngineLock`.

These two paths require **different teardown mechanisms**:

- DPC path is excluded by `AoTransportUnregister` (drops engine ref,
  removes rt from active list) followed by `KeFlushQueuedDpcs`
  (drains any DPC mid-helper-call).
- Query path is excluded by atomically nullifying `m_pTransportRt`
  under `m_PositionSpinLock`. After the publish, any new query reads
  `NULL` and short-circuits; any in-progress query already finishes
  the helper because the unwind acquires `m_PositionSpinLock` to
  perform the publish, which blocks until the in-progress query
  releases.

### Ref-count model recap

`AO_STREAM_RT::RefCount` partitions into **two** components — note
that the query path is intentionally **not** represented here:

- **Owner ref** (`+1` at `AoTransportAllocStreamRt`, `-1` at
  `AoTransportFreeStreamRt` in the destructor) — exactly one for the
  life of the struct.
- **Engine ref** (`+1` at `AoTransportOnRunEx` registration, `-1` at
  `AoTransportUnregister`) — present only while the rt is on the
  engine's active list.
- **Transient DPC ref** (`+1` when the engine timer DPC snapshots
  the rt under `EngineLock`, `-1` after the helper returns) —
  short-lived, only during a DPC-driven helper invocation. The
  query path is excluded from this row by design (it uses
  `m_pTransportRt` publish ordering instead).

Steady states:

| State | Engine ref | DPC ref | Owner ref | Total |
|---|---|---|---|---|
| Idle on active list, no DPC running | 1 | 0 | 1 | **2** |
| DPC executing | 1 | 1 | 1 | 3 (transient) |
| After Unregister + KeFlushQueuedDpcs | 0 | 0 | 1 | **1** (owner-only) |
| After destructor's final release | 0 | 0 | 0 | **0** (free) |

A query path call in flight does NOT change these totals.

### Unwind contract

The unwind contract on every path that reaches `FreeAudioBuffer`
(destructor, `KSSTATE_STOP` from a session that allocated, error rollback
from `AllocateAudioBuffer` failure):

```c
// Required ordering — no exceptions:

// --- Step A: exclude the query path ---
acquire(m_PositionSpinLock);
PAO_STREAM_RT rtSnapshot = m_pTransportRt;
m_pTransportRt           = NULL;            // publish: future query path
                                            //   reads NULL and no-ops.
release(m_PositionSpinLock);                // any in-progress query that was
                                            //   holding this lock has finished
                                            //   the helper and released by now.

// --- Step B: exclude the DPC path ---
1. AoTransportUnregister(rtSnapshot);       // remove from engine active list
                                            //   (drops engine ref: 2 → 1).
                                            // Engine timer DPC stops snapshotting
                                            //   this rt on its next tick.
2. KeFlushQueuedDpcs();                     // drain any DPC that already snapshotted
                                            //   before step 1 took effect.

// At this point RefCount must equal 1 (owner-only). Both paths excluded.
ASSERT(rtSnapshot->RefCount == 1);

// --- Step C: tear down the DMA-buffer-shaped state ---
3. rtSnapshot->DmaBuffer     = NULL;        // defense in depth — even with
   rtSnapshot->DmaBufferSize = 0;           //   step A done, an unforeseen
                                            //   helper caller would see NULL.
4. FreeAudioBuffer();                       // PortCls unmaps; the buffer is gone.

// --- Step D: only the destructor frees rt ---
5. (destructor only) AoTransportFreeStreamRt(rtSnapshot);
                                            //   drops the owner ref: 1 → 0, frees rt.
```

The transport helper must additionally guard reads of `rt->DmaBuffer`
and treat `NULL` as "this stream is unmapping; no-op the render/capture
copy this tick." This guard is cheap (a single compare under
`PositionLock`) and closes the late-DPC race. (Review #11 of 8afa59a;
ref-count + query-path partition tightened by re-review #3 of a038ad6
and #1 of 0eb5920.)

---

## 6. Format negotiation detail

### 6.1 KSDATARANGE intersection handler

In `Source/Filters/cablewavtable.h` (or per-cable filter table):

- declare data ranges that cover the rates / bits / channels listed in
  `docs/AO_CABLE_V1_ARCHITECTURE.md` § 10.3 (KSDATAFORMAT_SUBTYPE_PCM
  only — IEEE_FLOAT not advertised).
- intersection handler returns one of three statuses:
  - `STATUS_SUCCESS` when the requested format matches an advertised
    range (rate ∈ list, bits ∈ {8,16,24,32}, subtype = PCM,
    channels ∈ {1,2}) AND the rate GCD-divides cleanly via 300/100/75
    against `pipe->InternalRate`.
  - `STATUS_NO_MATCH` when the request is **outside** the advertised
    range — wrong subtype (IEEE_FLOAT), unadvertised rate, unsupported
    bit-depth, unsupported channel count, etc.
  - `STATUS_NOT_SUPPORTED` when the request is in-range PCM but
    `PickGCDDivisor(requestedRate, pipe->InternalRate, ...)` returns
    `STATUS_NOT_SUPPORTED` (no first-match in `[300, 100, 75]` —
    e.g. `22050 ↔ {8000, 16000, 32000}`).

The two-tier contract is the same across ARCHITECTURE § 10.3 and
ADR-008. Implementations must not collapse both to one status — see
ARCHITECTURE for rationale.

### 6.2 OEM Default Format

In `Source/Main/aocablea.inx` / `aocableb.inx`:

```text
HKR,"EP\\0", %PKEY_AudioEngine_OEMFormat%, %REG_BINARY%,
   <WAVEFORMATEXTENSIBLE for 48000 / 24-bit / Stereo / KSDATAFORMAT_SUBTYPE_PCM>
```

This is the format Windows Audio Engine adopts as the shared-mode mix
format by default.

---

## 7. Diagnostics IOCTL detail

### 7.1 Layout

```c
// V1 portion (AO_STREAM_STATUS) — always present
typedef struct _AO_ENDPOINT_STATUS {
    BOOLEAN Active;
    ULONG   SampleRate;
    ULONG   BitsPerSample;
    ULONG   Channels;
} AO_ENDPOINT_STATUS;

typedef struct _AO_STREAM_STATUS {
    AO_ENDPOINT_STATUS CableA_Speaker;
    AO_ENDPOINT_STATUS CableA_Mic;
    AO_ENDPOINT_STATUS CableB_Speaker;
    AO_ENDPOINT_STATUS CableB_Mic;
} AO_STREAM_STATUS;

// V2 portion (AO_V2_DIAG) — present iff caller buffer is large enough
typedef struct _AO_V2_DIAG {
    ULONG StructSize;

    // Per Cable * Direction: 7 ULONGs each
    ULONG  A_R_GatedSkipCount;
    ULONG  A_R_OverJumpCount;
    ULONG  A_R_FramesProcessedLow;
    ULONG  A_R_FramesProcessedHigh;
    ULONG  A_R_PumpInvocationCount;
    ULONG  A_R_PumpShadowDivergenceCount;
    ULONG  A_R_PumpFeatureFlags;
    // ...same for A_C, B_R, B_C

    // Stage 4+ render-side drive counters
    ULONG  A_R_PumpDriveCount;
    ULONG  A_R_LegacyDriveCount;
    ULONG  B_R_PumpDriveCount;
    ULONG  B_R_LegacyDriveCount;
} AO_V2_DIAG;
```

`StructSize` lets a V2 client detect the exact layout it's talking to.

### 7.2 Handler sketch

```c
NTSTATUS HandleGetStreamStatus(PIRP irp, ULONG outBufLen)
{
    BYTE* outBuf = irp->AssociatedIrp.SystemBuffer;
    ULONG written = 0;

    if (outBufLen >= sizeof(AO_STREAM_STATUS)) {
        fill_v1_status((AO_STREAM_STATUS*)outBuf);
        written = sizeof(AO_STREAM_STATUS);
    }

    if (outBufLen >= sizeof(AO_STREAM_STATUS) + sizeof(AO_V2_DIAG)) {
        AO_V2_DIAG* diag = (AO_V2_DIAG*)(outBuf + sizeof(AO_STREAM_STATUS));
        diag->StructSize = sizeof(AO_V2_DIAG);
        fill_v2_diag(diag);
        written += sizeof(AO_V2_DIAG);
    }

    irp->IoStatus.Information = written;
    return STATUS_SUCCESS;
}
```

### 7.3 User-mode consumer

`test_stream_monitor.py` uses `ctypes` + `DeviceIoControl` to call
`IOCTL_AO_GET_STREAM_STATUS` at ~10 Hz, prints diff per tick. Must be
updated together with `Source/Main/ioctl.h` + `Source/Main/adapter.cpp` per
REVIEW_POLICY § 7.

---

## 8. Build & install detail

### 8.1 Build entry point

`build-verify.ps1 -Config Release` invokes MSBuild on `VirtualAudioDriver.sln`
with `-p:SpectreMitigation=false` (Spectre libraries not installed in the
local WDK). Builds both `CableA.vcxproj` and `CableB.vcxproj`.

Outputs:

```text
Source/Main/x64/Release/CableA/aocablea.sys
Source/Main/x64/Release/CableA/aocablea.cat
Source/Main/x64/Release/CableA/aocablea.inf
Source/Main/x64/Release/CableB/aocableb.sys
Source/Main/x64/Release/CableB/aocableb.cat
Source/Main/x64/Release/CableB/aocableb.inf
```

`build-verify.ps1` produces `build-manifest.json` with hashes.

### 8.2 Install entry point

`install.ps1 -Action upgrade` (no `-AutoReboot`):

1. detects already-installed AO Cable services
2. issues `IOCTL_AO_PREPARE_UNLOAD` (no-reboot quiesce path)
3. waits for handle drain
4. `pnputil` install of new `.inf` files
5. `devcon update` to reattach driver
6. validates via `verify-install.ps1`

Reboot fallback: if quiesce times out, install registers a `RunOnce` task
to resume install on next boot. Documented in
`docs/M6A_NO_REBOOT_UPGRADE.md`.

### 8.3 Signing

Test signing during development; production code-signing pipeline in
`docs/M6C_SIGNING_PLAN.md`. Both `.sys` files signed individually.

---

## 9. Phase implementation map

The implementation work is organized in `phases/<N>-name/` directories.
Each phase has one or more `step<N>.md` files plus `exit.md`.

| Phase | Directory | Scope summary |
|---|---|---|
| 0 | `phases/0-baseline/` | RE evidence collection, branch consolidation, architecture frozen — **completed** |
| 1 | `phases/1-int32-ring/` | Replace packed-24 ring with INT32 frame-indexed ring; hard-reject overflow + counters |
| 2 | `phases/2-single-pass-src/` | Single-pass linear-interp SRC (write + read); GCD divisor; bit-depth dispatch |
| 3 | `phases/3-canonical-helper-shadow/` | `AoCableAdvanceByQpc` body + drift correction + 8-frame gate + overrun guard, in shadow mode |
| 4 | `phases/4-render-coupling/` | Flip cable render audible path to canonical helper; legacy `UpdatePosition` cable branch becomes shim |
| 5 | `phases/5-capture-coupling/` | Flip cable capture audible path to canonical helper |
| 6 | `phases/6-cleanup/` | Remove retired Phase 5/Step 3-4 scaffolding; delete pump flags / sinc table / packed-24 paths |
| 7 | `phases/7-quality-polish/` | Multi-channel, telephony category metadata, broader format acceptance, polish |

Each phase's `step0.md` defines its scope, `exit.md` defines its exit
gate. Status is tracked in `phases/<N>-name/index.json`.

---

## 10. Pointers

- `docs/AO_CABLE_V1_ARCHITECTURE.md` — system overview, decisions, contracts.
- `docs/ADR.md` — every decision plus rationale.
- `docs/PRD.md` — product identity, scope, success criteria.
- `docs/REVIEW_POLICY.md` — review rules.
- `docs/GIT_POLICY.md` — branch / commit / merge rules.
- `phases/<N>-name/step<N>.md` — implementation guidance per step.
- `results/vbcable_pipeline_analysis.md` — VB pipeline reference.
- `results/vbcable_disasm_analysis.md` — VB SRC + ring layout reference.
- `results/vbcable_capture_contract_answers.md` — VB capture-side Q&A.
- `results/phase6_vb_verification.md` — WinDbg dynamic verification.
- `results/ghidra_decompile/vbcable_all_functions.c` — full VB decompile (12 096 lines).
