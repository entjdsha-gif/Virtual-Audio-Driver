/*++
Module Name:
    loopback.h
Abstract:
    Loopback ring buffer with format conversion for virtual audio cable.
    Internal ring buffer format: 48kHz/24bit/8ch (configurable rate via IOCTL).
    Speaker and Mic can use any supported format; conversion happens automatically.
--*/

#ifndef _VIRTUAL_AUDIO_LOOPBACK_H_
#define _VIRTUAL_AUDIO_LOOPBACK_H_

// portcls.h includes ntddk.h and all kernel headers
#include <portcls.h>

//=============================================================================
// Internal ring buffer format constants
//=============================================================================
#define LB_DEFAULT_INTERNAL_RATE       48000
#define LB_INTERNAL_BITS               24
#define LB_INTERNAL_CHANNELS           8
#define LB_INTERNAL_BLOCKALIGN         24  // 24bit(3bytes) * 8ch = 24 bytes/frame
#define LB_MAX_INTERNAL_CHANNELS       16  // for static array sizing only (PrevSamples)
#define LB_DEFAULT_LATENCY_MS          20
#define LB_MIN_LATENCY_MS             5
#define LB_MAX_LATENCY_MS             100

// Convert buffer: sized for up to 16ch at 192kHz/2ms
#define LB_CONVERT_BUF_SIZE           32768

// Default buffer size: 4 seconds @ internal format
#define LOOPBACK_BUFFER_SIZE  (48000 * LB_INTERNAL_BLOCKALIGN * 4)

// Max render streams for multi-client support
#define LB_MAX_RENDER_STREAMS         4

//=============================================================================
// Stream format descriptor
//=============================================================================
typedef struct _LB_FORMAT {
    ULONG   SampleRate;
    ULONG   BitsPerSample;
    ULONG   nChannels;
    ULONG   nBlockAlign;
    BOOLEAN IsFloat;        // TRUE for IEEE_FLOAT, FALSE for PCM
} LB_FORMAT;

//=============================================================================
// SRC (sample rate converter) state - persists across DPC ticks
//=============================================================================
// Sinc interpolation constants
#define LB_SINC_TAPS       8
#define LB_SINC_PHASES     256
#define LB_SINC_TABLE_SIZE (LB_SINC_TAPS * LB_SINC_PHASES)

typedef struct _LB_SRC_STATE {
    ULONGLONG  Accumulator;                                     // 32.32 fixed-point position
    INT32      PrevSamples[LB_SINC_TAPS * LB_MAX_INTERNAL_CHANNELS]; // history sized for max channels
    ULONG      HistoryCount;                                    // valid history frames
    BOOLEAN    Valid;
} LB_SRC_STATE;

//=============================================================================
// Mic DMA sink - registered by capture stream so Speaker can push directly.
//=============================================================================
typedef struct _LOOPBACK_MIC_SINK {
    BYTE*       DmaBuffer;          // Mic's DMA buffer pointer
    ULONG       DmaBufferSize;      // Mic's DMA buffer size in bytes
    volatile ULONG  WritePos;       // Next write position in Mic DMA (updated by Speaker DPC)
    volatile BOOLEAN  Active;       // TRUE while Mic stream is open
    volatile ULONGLONG TotalBytesWritten; // monotonic counter of bytes Speaker pushed to Mic DMA
} LOOPBACK_MIC_SINK, *PLOOPBACK_MIC_SINK;

//=============================================================================
// Main loopback buffer with format conversion support
// NOTE: explicit packing to guarantee identical layout across translation units
// (adapter.cpp in Main and loopback.cpp in Utilities must agree on field offsets)
//=============================================================================
typedef struct _LOOPBACK_BUFFER {
    BYTE*       Buffer;
    ULONG       BufferSize;         // Dynamic: based on InternalRate * latency
    ULONG       WritePos;
    ULONG       ReadPos;
    ULONG       DataCount;          // bytes currently in buffer
    KSPIN_LOCK  SpinLock;
    BOOLEAN     Initialized;

    // Direct-push sink: Speaker DPC writes here AND to Mic DMA simultaneously.
    LOOPBACK_MIC_SINK  MicSink;

    // Stashed Mic DMA info for deferred MicSink activation.
    // Set when Mic enters RUN but FormatMatch is FALSE.
    // Used by LoopbackRegisterFormat to auto-activate MicSink
    // when FormatMatch later transitions to TRUE.
    BYTE*       MicDmaStash;
    ULONG       MicDmaStashSize;

    // Format conversion support
    LB_FORMAT      SpeakerFormat;      // Current Speaker stream format
    LB_FORMAT      MicFormat;          // Current Mic stream format
    BOOLEAN        SpeakerActive;      // Speaker stream registered
    BOOLEAN        MicActive;          // Mic stream registered (format-level)
    BOOLEAN        FormatMatch;        // All 3 formats identical -> raw passthrough

    // SRC states (persist across DPC ticks for continuity)
    LB_SRC_STATE   SpeakerSrcState;    // Speaker->Internal SRC
    LB_SRC_STATE   MicSrcState;        // Internal->Mic SRC
    volatile BOOLEAN SpeakerSrcResetPending; // Set by IOCTL, consumed by Speaker DPC
    volatile BOOLEAN MicSrcResetPending;     // Set by IOCTL, consumed by Mic DPC

    // Scratch buffers for format conversion (NonPaged, pre-allocated)
    // Speaker and Mic DPCs run on separate cores, so each needs its own pair.
    BYTE*          SpkConvertBufA;     // Speaker DPC scratch 1
    BYTE*          SpkConvertBufB;     // Speaker DPC scratch 2
    BYTE*          MicConvertBufA;     // Mic DPC scratch 1
    BYTE*          MicConvertBufB;     // Mic DPC scratch 2
    ULONG          ConvertBufSize;

    // Dynamic configuration (IOCTL-settable)
    ULONG          InternalRate;       // Default 48000
    ULONG          MaxLatencyMs;       // Default 20
    ULONG          InternalChannels;   // 8 or 16 (set at init, read-only after)
    ULONG          InternalBlockAlign; // (LB_INTERNAL_BITS/8) * InternalChannels

    // Multi-client render stream tracking
    ULONG          ActiveRenderCount;  // Number of active render streams
} LOOPBACK_BUFFER, *PLOOPBACK_BUFFER;


//=============================================================================
// Core ring buffer functions
//=============================================================================
NTSTATUS LoopbackInit(PLOOPBACK_BUFFER pLoopback, ULONG internalChannels = LB_INTERNAL_CHANNELS);
VOID LoopbackCleanup(PLOOPBACK_BUFFER pLoopback);
VOID LoopbackWrite(PLOOPBACK_BUFFER pLoopback, const BYTE* Data, ULONG Count);
VOID LoopbackRead(PLOOPBACK_BUFFER pLoopback, BYTE* Data, ULONG Count);
VOID LoopbackReset(PLOOPBACK_BUFFER pLoopback);

//=============================================================================
// Mic sink registration
//=============================================================================
VOID LoopbackRegisterMicSink(PLOOPBACK_BUFFER pLoopback, BYTE* DmaBuffer, ULONG DmaBufferSize);
VOID LoopbackUnregisterMicSink(PLOOPBACK_BUFFER pLoopback);

//=============================================================================
// Format registration: called by streams on Init/Stop
//=============================================================================
VOID LoopbackRegisterFormat(
    PLOOPBACK_BUFFER pLoopback,
    BOOLEAN          isSpeaker,      // TRUE=Speaker, FALSE=Mic
    ULONG            sampleRate,
    ULONG            bitsPerSample,
    ULONG            nChannels,
    ULONG            nBlockAlign,
    BOOLEAN          isFloat = FALSE  // TRUE for IEEE_FLOAT
);
VOID LoopbackUnregisterFormat(
    PLOOPBACK_BUFFER pLoopback,
    BOOLEAN          isSpeaker
);

//=============================================================================
// Format-converting write/read (Speaker DPC / Mic DPC)
// These handle bit depth, channel, and sample rate conversion.
//=============================================================================
VOID LoopbackWriteConverted(
    PLOOPBACK_BUFFER pLoopback,
    const BYTE*      Data,
    ULONG            ByteCount       // bytes in Speaker format
);

VOID LoopbackReadConverted(
    PLOOPBACK_BUFFER pLoopback,
    BYTE*            Data,
    ULONG            ByteCount       // bytes in Mic format to fill
);

//=============================================================================
// Dynamic buffer resize (IOCTL)
//=============================================================================
NTSTATUS LoopbackResizeBuffer(PLOOPBACK_BUFFER pLoopback, ULONG newLatencyMs);
NTSTATUS LoopbackSetInternalRate(PLOOPBACK_BUFFER pLoopback, ULONG newRate);

//=============================================================================
// Global instances
//=============================================================================
extern LOOPBACK_BUFFER g_CableALoopback;
extern LOOPBACK_BUFFER g_CableBLoopback;


//=============================================================================
//=============================================================================
//
//  FRAME_PIPE — Fixed Frame Pipe (VB-style transport)
//  Phase 1: Core struct + write/read/diagnostics
//  Coexists with LOOPBACK_BUFFER until Phase 2 integration.
//
//=============================================================================
//=============================================================================

//=============================================================================
// Frame Pipe constants
//=============================================================================
#define FP_DEFAULT_TARGET_FILL      3584    // ~74ms @ 48kHz (actual latency)
#define FP_MIN_TARGET_FILL          512     // ~10ms @ 48kHz
#define FP_MAX_TARGET_FILL          192000  // ~4s @ 48kHz (survive Speaker STOP gaps)
#define FP_CAPACITY_MULTIPLIER      2       // CapacityFrames = TargetFill * 2
#define FP_MIN_GATE_FRAMES          8       // skip sub-sample noise (VB reference)
#define FP_MAX_CHANNELS             16      // static array sizing for SRC state
// Startup headroom: fixed silence cushion injected on speaker
// RegisterFormat / KSSTATE_RUN. Size in milliseconds; actual frame count
// is derived from PipeSampleRate at fire time so it scales across
// 44.1/48/96/192k.
//
// Sizing is driven by measured upstream burst cadence. WaveRT delivers
// cable speaker data in ~200-250 ms bursts that match the OpenAI
// Realtime API's 250 ms write chunks. Between bursts, FramePipeWriteFromDma
// is idle for ~200 ms while Phone Link reads the ring at 48 kHz. A
// cushion smaller than the inter-burst gap drains to zero between
// bursts and returns zero-fill silence to the reader — not perceived
// as clean drop-outs but as degraded quality because the silence
// fragments interleave with real speech samples.
//
// 300 ms covers the observed 200-250 ms gap with ~50 ms margin. Startup
// latency of 300 ms is noticeable but acceptable for a freshly-answered
// call (user is still raising the phone to their ear).
#define FP_STARTUP_HEADROOM_MS      300

//=============================================================================
// FRAME_PIPE structure (canonical V1 shape — DESIGN § 2.1)
//
// Phase 1 Step 1 (ADR-014 phase/1-int32-ring): replaced the prior ad-hoc
// FRAME_PIPE shape with the design-locked INT32 frame-indexed shape.
//
// Compile-preserving shim layer:
//   - 12 legacy FramePipe* functions kept in loopback.h/.cpp so untouched
//     translation units (minwavertstream.cpp, transport_engine.cpp) link
//     without modification (forward wrappers + behavior-absent stubs).
//   - 4 Phase 1 Step 0 cross-TU helpers (FramePipeIsDirectionActive,
//     FramePipeSetPumpFeatureFlags, FramePipeResetPumpFeatureFlags,
//     FramePipePublishPumpCounters) kept as behavior-absent stubs against
//     the new shape — minwavertstream.cpp continues to call them but the
//     bodies do not consult the legacy fields that no longer exist.
//
// LOOPBACK_BUFFER above is a separate legacy ring; not touched here.
//
// All fields are protected by Lock when touched at DISPATCH_LEVEL.
// Counters use plain LONG — increments happen under Lock.
//=============================================================================
typedef struct _FRAME_PIPE {
    // Lifetime
    KSPIN_LOCK  Lock;                  // protects all fields below

    // Internal pipe format — fixed at FramePipeInitCable time, never
    // changes for the life of the pipe.
    ULONG       InternalRate;          // ring sample rate, Hz
    USHORT      InternalBitsPerSample; // ring sample width — always 32 (INT32)
    LONG        InternalBlockAlign;    // = (InternalBitsPerSample/8) * Channels

    // Latency / capacity (frames)
    LONG        TargetLatencyFrames;   // registry-driven target depth
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
    // the same kind of state per ring (results/vbcable_capture_contract_answers.md
    // § 0, +0xB8 phase + +0xBC..+0xFB per-channel residual). 16 channel
    // slots mirror Channels.
    LONG        WriteSrcPhase;         // accumulator phase (write direction)
    LONG        WriteSrcResidual[16];  // per-channel residual carry
    LONG        ReadSrcPhase;          // accumulator phase (read direction)
    LONG        ReadSrcResidual[16];   // per-channel residual carry

    // Backing storage — INT32 array of size WrapBound * Channels
    LONG*       Data;
    SIZE_T      DataAllocBytes;
} FRAME_PIPE, *PFRAME_PIPE;

//=============================================================================
// Frame Pipe — Canonical V1 API (DESIGN § 2.2)
//
// Phase 1 Step 1: declarations introduced + lifetime helpers implemented;
// AoRingWriteFromScratch / AoRingReadToScratch are STATUS_NOT_IMPLEMENTED
// stubs until Step 2 / Step 3.
//=============================================================================

NTSTATUS FramePipeInitCable(
    PFRAME_PIPE  pipe,
    ULONG        internalRate,
    LONG         channels,
    LONG         initialFrames
);

VOID     FramePipeFree(PFRAME_PIPE pipe);

// STOP path: zero positions, underrun flag, and SRC phase/residual state.
VOID     FramePipeResetCable(PFRAME_PIPE pipe);

// (WritePos - ReadPos) wrap-corrected; any IRQL.
ULONG    AoRingAvailableFrames(PFRAME_PIPE pipe);

// Write/read with format conversion + persistent SRC state.
// Step 1: stubs returning STATUS_NOT_IMPLEMENTED.
NTSTATUS AoRingWriteFromScratch(
    PFRAME_PIPE  pipe,
    const BYTE*  scratch,
    ULONG        frames,
    ULONG        srcRate,
    ULONG        srcChannels,
    ULONG        srcBits
);

NTSTATUS AoRingReadToScratch(
    PFRAME_PIPE  pipe,
    BYTE*        scratch,
    ULONG        frames,
    ULONG        dstRate,
    ULONG        dstChannels,
    ULONG        dstBits
);

//=============================================================================
// Frame Pipe — Core API (legacy compile-preserving shim — Phase 1 Step 1)
//
// The 12 legacy FramePipe* functions stay so that minwavertstream.cpp /
// transport_engine.cpp link without modification. Their bodies are
// forward wrappers (where new canonical API has matching semantics) or
// behavior-absent stubs. Phase 6 cleanup deletes the shim.
//=============================================================================

// Init/Cleanup (PASSIVE_LEVEL)
NTSTATUS FramePipeInit(
    PFRAME_PIPE     pPipe,
    ULONG           pipeSampleRate,     // Hz (e.g. 48000)
    ULONG           pipeChannels,       // channels per frame (e.g. 2)
    ULONG           targetFillFrames    // actual latency in frames
);
VOID FramePipeCleanup(PFRAME_PIPE pPipe);

// Write/Read (DISPATCH_LEVEL, under PipeLock)
ULONG FramePipeWriteFrames(
    PFRAME_PIPE     pPipe,
    const INT32*    srcFrames,          // PipeChannels INT32s per frame
    ULONG           frameCount
);
ULONG FramePipeReadFrames(
    PFRAME_PIPE     pPipe,
    INT32*          dstFrames,          // PipeChannels INT32s per frame
    ULONG           frameCount
);

// Reset (PASSIVE_LEVEL, after KeFlushQueuedDpcs)
VOID FramePipeReset(PFRAME_PIPE pPipe);

// Prefill ring with silence up to TargetFillFrames so reader sees headroom
// immediately on speaker RUN transition. No-op if ring is not empty.
// Called from SetState(KSSTATE_RUN) speaker branch. Any IRQL OK, but
// designed for PASSIVE_LEVEL usage.
VOID FramePipePrefillSilence(PFRAME_PIPE pPipe);

// Query (any IRQL)
ULONG FramePipeGetFillFrames(PFRAME_PIPE pPipe);

//=============================================================================
// Frame Pipe — Cross-TU helpers (Phase 1 Step 0 boundary, Step 1 stubs)
//
// These helpers were introduced in Phase 1 Step 0 (commit ddbb977) so that
// minwavertstream.cpp does not depend on the legacy FRAME_PIPE field
// layout. The Step 0 bodies read/wrote legacy fields (Initialized,
// Speaker/MicActive, Render/CapturePumpFeatureFlags, all six
// Render/Capture*Count fields).
//
// Phase 1 Step 1 (canonical FRAME_PIPE shape) removed those legacy fields.
// As of Step 1 these helpers are **kept as behavior-absent stubs**:
//   - FramePipeIsDirectionActive returns FALSE unconditionally.
//   - FramePipeSetPumpFeatureFlags / FramePipeResetPumpFeatureFlags /
//     FramePipePublishPumpCounters are no-ops.
//
// minwavertstream.cpp continues to call these helpers in the legacy
// pump / pause-reset / counter-publish paths, but the calls produce no
// observable cable-transport behavior. Phase 5 ownership flip preceded
// this step, so the legacy pump path is no longer driving audio. Phase 6
// cleanup deletes both these helpers and the minwavertstream.cpp call
// sites together with the AO_V2_DIAG legacy fields.
//=============================================================================

// Behavior-absent stub (Phase 1 Step 1): always returns FALSE.
// Direction selector: TRUE = speaker side, FALSE = mic side.
// Any IRQL.
BOOLEAN FramePipeIsDirectionActive(
    PFRAME_PIPE  pipe,
    BOOLEAN      isSpeaker
);

// Behavior-absent stub (Phase 1 Step 1): no-op.
// Direction selector: TRUE = render side, FALSE = capture side.
// Any IRQL.
VOID FramePipeSetPumpFeatureFlags(
    PFRAME_PIPE  pipe,
    BOOLEAN      isRenderSide,
    ULONG        flags
);

// Behavior-absent stub (Phase 1 Step 1): no-op.
// Direction selector: TRUE = render side, FALSE = capture side.
// Any IRQL.
VOID FramePipeResetPumpFeatureFlags(
    PFRAME_PIPE  pipe,
    BOOLEAN      isRenderSide
);

// Behavior-absent stub (Phase 1 Step 1): no-op.
// Direction selector: TRUE = render side, FALSE = capture side.
// Any IRQL.
VOID FramePipePublishPumpCounters(
    PFRAME_PIPE  pipe,
    BOOLEAN      isRenderSide,
    ULONG        gatedSkipCount,
    ULONG        overJumpCount,
    ULONGLONG    framesProcessedTotal,
    ULONG        invocationCount,
    ULONG        shadowDivergenceCount,
    ULONG        featureFlags
);

//=============================================================================
// Frame Pipe — Phase 2: Format Registration + DMA Batch API
//=============================================================================

// Format registration (PASSIVE_LEVEL)
VOID FramePipeRegisterFormat(
    PFRAME_PIPE     pPipe,
    BOOLEAN         isSpeaker,
    ULONG           sampleRate,
    ULONG           bitsPerSample,
    ULONG           nChannels,
    ULONG           nBlockAlign,
    BOOLEAN         isFloat
);
VOID FramePipeUnregisterFormat(
    PFRAME_PIPE     pPipe,
    BOOLEAN         isSpeaker
);

// Batch DMA conversion (DISPATCH_LEVEL)
// WriteFromDma: Speaker DPC — normalize + channel map + pipe write
// Returns frames written (0 = overflow, entire batch rejected)
ULONG FramePipeWriteFromDma(
    PFRAME_PIPE     pPipe,
    const BYTE*     dmaData,        // contiguous DMA bytes in Speaker format
    ULONG           byteCount
);

// Phase 6 Y2-1: Ex variant with optional per-stream runtime hook for
// fade envelope application. rtOpaque is an AO_STREAM_RT* passed
// through as a void* to avoid cyclic includes between loopback.h and
// transport_engine.h. When non-NULL, the callee invokes
// AoCableApplyRenderFadeInScratch(rtOpaque, scratch, ...) between the
// normalize-to-scratch step and the pipe-write step. When NULL, the
// behavior is byte-identical to the legacy FramePipeWriteFromDma.
//
// The legacy FramePipeWriteFromDma is kept as a thin forward to this
// function with rtOpaque == NULL so the existing ReadBytes caller is
// unchanged. Y2-2 switches the authoritative cable render write over
// to the Ex path via AoCableWriteRenderFromDma.
ULONG FramePipeWriteFromDmaEx(
    PFRAME_PIPE     pPipe,
    const BYTE*     dmaData,
    ULONG           byteCount,
    PVOID           rtOpaque        // AO_STREAM_RT* or NULL
);
// ReadToDma: Mic DPC — pipe read + channel map + denormalize
// Always fills byteCount bytes in DMA (silence on underrun)
VOID FramePipeReadToDma(
    PFRAME_PIPE     pPipe,
    BYTE*           dmaData,        // DMA buffer to fill in Mic format
    ULONG           byteCount
);

//=============================================================================
// Frame Pipe — Global instances
//=============================================================================
extern FRAME_PIPE g_CableAPipe;
extern FRAME_PIPE g_CableBPipe;

//=============================================================================
// IOCTL_AO_SET_PUMP_FEATURE_FLAGS C-style entry point — fail-closed stub.
//
// History:
//   2c733f1 (Phase 5 CLOSED, 2026-04-14) — original definition lived in
//     minwavertstream.cpp and applied a mask-constrained render-ownership
//     flag update to the active cable speaker stream. Returned
//     STATUS_SUCCESS (treated no-active-stream as a no-op).
//   5a013b1 (Phase 6 Step 1 skeleton, 2026-04-15) — wholesale removed
//     the definition and its dependent globals/static helpers/member body
//     while migrating cable transport ownership to the shared transport
//     engine. The declaration here and the four call sites in
//     adapter.cpp's IOCTL_AO_SET_PUMP_FEATURE_FLAGS handler stayed,
//     leaving the link unresolved.
//   f7801bd (Phase 1 build fix) — fail-closed stub added in
//     Source/Utilities/loopback.cpp so the link target exists.
//
// Current contract (Phase 1):
//   - Implementation lives in Source/Utilities/loopback.cpp, NOT
//     minwavertstream.cpp.
//   - Returns STATUS_NOT_SUPPORTED for any (cableIndex, setMask,
//     clearMask) input. No driver state is mutated.
//   - The IOCTL caller sees a clear "not supported in this build"
//     result. Phase 6 cleanup is expected to retire both the IOCTL
//     handler in adapter.cpp and this stub together.
//=============================================================================
#ifdef __cplusplus
extern "C" {
#endif

NTSTATUS
AoPumpApplyRenderFlagMask(
    _In_ ULONG cableIndex,
    _In_ ULONG setMask,
    _In_ ULONG clearMask
);

#ifdef __cplusplus
}
#endif

#endif
