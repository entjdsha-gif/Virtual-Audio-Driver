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

    // Backing storage — INT32 array of size WrapBound * Channels
    LONG*       Data;
    SIZE_T      DataAllocBytes;
} FRAME_PIPE, *PFRAME_PIPE;
```

### 2.2 Public API (target)

```c
NTSTATUS FramePipeInitCable(PFRAME_PIPE pipe,
                            LONG initialFrames,
                            LONG channels);

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

available = AoRingAvailableSpaceFrames(pipe);  // WrapBound - currentFill - 2

if (frames > available) {
    pipe->OverflowCounter += 1;
    release(pipe->Lock);
    return STATUS_INSUFFICIENT_RESOURCES;
}

// GCD divisor selection
divisor = pickGCD(srcRate, pipe->InternalRate, [300, 100, 75]);
if (divisor < 0) {
    release(pipe->Lock);
    return STATUS_INVALID_PARAMETER;
}
srcRatio = srcRate / divisor;
dstRatio = pipe->InternalRate / divisor;

// Per-channel linear-interp accumulator
for (i = 0; i < frames; ++i) {
    for (ch = 0; ch < min(srcChannels, pipe->Channels); ++ch) {
        sample = read_and_normalize_to_int19(scratch, srcBits, ch);  // 8/16/24/32 dispatch
        // weighted accumulator linear interp...
        write_int32_to_ring(pipe, slot(ch, frame), interpolated);
    }
    pipe->WritePos = (pipe->WritePos + 1) % pipe->WrapBound;
}

release(pipe->Lock);
return STATUS_SUCCESS;
```

### 2.4 Read SRC algorithm (`AoRingReadToScratch`)

Same structure, inverted direction. Underrun handling:

```c
acquire(pipe->Lock);
reconcile_wrapbound_to_target(pipe);

if (pipe->UnderrunFlag) {
    if (AoRingAvailableFrames(pipe) < pipe->WrapBound / 2) {
        zero_fill(scratch, frames * dstBlockAlign);
        release(pipe->Lock);
        return STATUS_SUCCESS;  // silence delivered
    }
    pipe->UnderrunFlag = 0;     // exit recovery
}

if (frames > AoRingAvailableFrames(pipe)) {
    pipe->UnderrunCounter += 1;
    pipe->UnderrunFlag = 1;
    zero_fill(scratch, frames * dstBlockAlign);
    release(pipe->Lock);
    return STATUS_SUCCESS;
}

// SRC + format-specific denormalization (>>11/+0x80, >>3, <<5+pack3, copy)
read_and_advance_readpos(pipe, scratch, frames, dstRate, dstChannels, dstBits);

release(pipe->Lock);
return STATUS_SUCCESS;
```

### 2.5 Bit-depth dispatch

| Bits | Read (scratch → INT32 ring) | Write (INT32 ring → scratch) |
|---|---|---|
| 8 | `(byte - 0x80) << 11` | `(int >> 11) + 0x80` |
| 16 | `(int)short << 3` | `int >> 3` |
| 24 | `((b0 \| (b1<<8) \| (b2<<16)) << 8) >> 13` | `int << 5` then write 3 bytes |
| 32 (PCM) | direct copy | direct copy |
| 32 (FP) | `(int)(float * (1<<19))` clamped | `(int)((float)int / (1<<19))` |

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

// PAUSE
AoTransportOnPauseEx(rt);        // mark inactive; keep allocation

// STOP
AoTransportOnStopEx(rt);         // mark inactive; AoCableResetRuntimeFields(rt)

// Destructor
KeFlushQueuedDpcs();
AoTransportOnStopEx(rt);          // idempotent
AoTransportFreeStreamRt(rt);      // RefCount-- ; free if last
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

    LONGLONG      PeriodQpc;          // 1 ms in QPC ticks (computed at init)
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
        InterlockedDecrement(&rt->RefCount);
        if (rt->RefCount == 0) {
            AoTransportFreeStreamRtFinal(rt);
        }
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
            if (m_pTransportRt) AoTransportOnPauseEx(m_pTransportRt);
            break;
        case KSSTATE_STOP:
            if (m_pTransportRt) AoTransportOnStopEx(m_pTransportRt);
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

```c
~CMiniportWaveRTStream()
{
    KeFlushQueuedDpcs();   // critical: drain in-flight engine DPCs

    if (m_pTransportRt) {
        AoTransportOnStopEx(m_pTransportRt);    // idempotent
        AoTransportFreeStreamRt(m_pTransportRt);
        m_pTransportRt = NULL;
    }

    // remaining cleanup (DMA buffer, port stream release, etc.)
}
```

---

## 6. Format negotiation detail

### 6.1 KSDATARANGE intersection handler

In `Source/Filters/cablewavtable.h` (or per-cable filter table):

- declare data ranges that cover the rates / bits / channels listed in
  `docs/AO_CABLE_V1_ARCHITECTURE.md` § 10.3.
- intersection handler picks the requested format if it matches a range
  AND `pickGCD(requestedRate, pipe.InternalRate)` succeeds; otherwise
  return `STATUS_NO_MATCH`.

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
