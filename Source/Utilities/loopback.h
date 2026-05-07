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
// FRAME_PIPE structure
//=============================================================================
typedef struct _FRAME_PIPE {
    // ─── INT32 Frame Ring (transport core) ───
    INT32*              RingBuffer;         // CapacityFrames * PipeChannels INT32 samples
    ULONG               PipeChannels;       // channels per frame
    volatile ULONG      WriteFrame;         // producer frame index [0, CapacityFrames)
    volatile ULONG      ReadFrame;          // consumer frame index [0, CapacityFrames)
    volatile ULONG      FillFrames;         // explicit fill count (resolves full/empty ambiguity)
    KSPIN_LOCK          PipeLock;

    // ─── Pipe Format (configured, not dynamic) ───
    ULONG               PipeSampleRate;     // Hz = configured InternalRate
    ULONG               PipeBitsPerSample;  // always 32 (INT32)
    ULONG               PipeBlockAlign;     // PipeChannels * sizeof(INT32)

    // ─── Stream Format Registration ───
    ULONG               SpeakerSampleRate;
    ULONG               SpeakerBitsPerSample;
    ULONG               SpeakerChannels;
    ULONG               SpeakerBlockAlign;
    BOOLEAN             SpeakerIsFloat;     // TRUE for IEEE_FLOAT
    BOOLEAN             SpeakerActive;

    ULONG               MicSampleRate;
    ULONG               MicBitsPerSample;
    ULONG               MicChannels;
    ULONG               MicBlockAlign;
    BOOLEAN             MicIsFloat;         // TRUE for IEEE_FLOAT
    BOOLEAN             MicActive;

    // ─── Path Selection ───
    BOOLEAN             SpeakerSameRate;    // Speaker rate == PipeSampleRate -> no SRC
    BOOLEAN             MicSameRate;        // Mic rate == PipeSampleRate -> no SRC

    // ─── Fixed Latency (3 separate values) ───
    ULONG               TargetFillFrames;   // steady-state fill = actual latency
    ULONG               CapacityFrames;     // ring size = TargetFillFrames * 2
    ULONG               StartThresholdFrames; // Mic read starts when fill >= this
    BOOLEAN             StartPhaseComplete; // TRUE after fill >= StartThreshold reached

    // ─── Overflow / Underrun / Diagnostics ───
    volatile ULONG      DropCount;          // frames rejected on write (pipe full)
    volatile ULONG      UnderrunCount;      // frames silence-filled on read (post-startup)
    volatile ULONG      ActiveRenderCount;  // active render streams

    // ─── Phase 1: per-direction pump counter pairs (rev 2.4 split) ───
    // Speaker and Mic streams share one FRAME_PIPE. A single counter would
    // race between the two DPCs. Per-direction slots remove the race because
    // only one stream direction ever writes a given slot (Speaker's pump
    // writes Render*, Mic's pump writes Capture*).
    //
    // Phase 1 contract: every field below is zero-initialized in
    // FramePipeInit and never written by any execution path. Phase 3 is the
    // first phase that increments these. FramePipeReset is the only place
    // that touches per-session fields (GatedSkip/OverJump); monotonic
    // run-totals (Frames/Invocation/ShadowDivergence) and FeatureFlags
    // survive reset so Phase 3's shadow-window ratio stays measurable
    // across RUN -> PAUSE -> RUN cycles.
    volatile ULONG      RenderGatedSkipCount;            // per-session
    volatile ULONG      RenderOverJumpCount;             // per-session
    volatile ULONGLONG  RenderFramesProcessedTotal;      // monotonic
    volatile ULONG      RenderPumpInvocationCount;       // monotonic
    volatile ULONG      RenderPumpShadowDivergenceCount; // monotonic

    volatile ULONG      CaptureGatedSkipCount;            // per-session
    volatile ULONG      CaptureOverJumpCount;             // per-session
    volatile ULONGLONG  CaptureFramesProcessedTotal;      // monotonic
    volatile ULONG      CapturePumpInvocationCount;       // monotonic
    volatile ULONG      CapturePumpShadowDivergenceCount; // monotonic

    // Per-direction feature-flag snapshots. Stored non-volatile because they
    // are set by a stream on SetState transition (rare) and read by the
    // IOCTL snapshot (also rare). Phase 1 leaves both at zero.
    ULONG               RenderPumpFeatureFlags;
    ULONG               CapturePumpFeatureFlags;

    // Phase 5 (2026-04-14): per-side drive counters for one-owner proof.
    // Each counter is incremented at most once per UpdatePosition/pump
    // helper invocation that actually reaches the FramePipeWriteFromDma
    // call site for this direction. RenderPumpDriveCount is written by
    // PumpToCurrentPositionFromQuery's render transport block.
    // RenderLegacyDriveCount is written by ReadBytes() when it enters the
    // cable-pipe branch. Both are monotonic evidence counters, like the
    // Phase 1 pump counters; FramePipeReset() does not touch them.
    // Exclusivity is the Phase 5 one-owner property: at any moment,
    // exactly one of the two is growing for a given cable render stream.
    volatile ULONG      RenderPumpDriveCount;   // Phase 5 pump transport fires
    volatile ULONG      RenderLegacyDriveCount; // Phase 5 ReadBytes cable render fires

    // ─── Scratch Buffers (allocated at PASSIVE, used at DISPATCH) ───
    // Speaker and Mic DPCs run on separate cores — each needs its own scratch.
    BYTE*               ScratchDma;         // DMA linearization buffer (future use)
    INT32*              ScratchSpk;         // Speaker DPC: normalize → pipe write
    INT32*              ScratchMic;         // Mic DPC: pipe read → denormalize
    ULONG               ScratchSizeBytes;   // size of each scratch buffer

    // ─── Configuration ───
    BOOLEAN             Initialized;

    // ─── Debug (rate-limited DbgPrint) ───
    LONGLONG            DbgLastPrintQpc;    // last print timestamp (QPC ticks)
    ULONG               DbgWriteFrames;     // frames written since last print
    ULONG               DbgReadFrames;      // frames read since last print
} FRAME_PIPE, *PFRAME_PIPE;

//=============================================================================
// Frame Pipe — Core API (Phase 1)
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
// Frame Pipe — Cross-TU helpers (Phase 1 Step 0)
//
// Narrow accessors that own all FRAME_PIPE field reads/writes needed by
// translation units other than loopback.cpp. They exist so that
// minwavertstream.cpp (and any future external caller) does not depend
// on the legacy FRAME_PIPE field layout, which lets Phase 1 Step 1
// replace the struct shape without a cross-TU contradiction.
//
// All helpers are NULL-safe: passing pPipe == NULL is a no-op (or
// returns FALSE for the active-query helper). The `Initialized`
// state is consulted by `FramePipeIsDirectionActive` only — it
// is the helper that gates the pause-time reset on whether the
// pipe has been brought up at all. The write/publish helpers do
// not check `Initialized`, exactly matching the prior inline
// direct-access behavior in minwavertstream.cpp (which also
// wrote unconditionally once a non-NULL pPipe had been resolved
// from the cable globals).
//
// Phase 1 Step 0 contract: these helpers preserve the legacy
// FRAME_PIPE behavior exactly. They do not change semantics; they
// only relocate the field access into loopback.cpp.
//=============================================================================

// Returns TRUE iff `pipe` is initialized AND the named direction is
// currently registered/active. Direction selector: TRUE = speaker side,
// FALSE = mic side. Any IRQL.
BOOLEAN FramePipeIsDirectionActive(
    PFRAME_PIPE  pipe,
    BOOLEAN      isSpeaker
);

// Sets the per-direction pump feature-flags field. Direction selector:
// TRUE = render side, FALSE = capture side. Any IRQL.
VOID FramePipeSetPumpFeatureFlags(
    PFRAME_PIPE  pipe,
    BOOLEAN      isRenderSide,
    ULONG        flags
);

// Resets the per-direction pump feature-flags field to 0. Direction
// selector: TRUE = render side, FALSE = capture side. Any IRQL.
VOID FramePipeResetPumpFeatureFlags(
    PFRAME_PIPE  pipe,
    BOOLEAN      isRenderSide
);

// Publishes the per-direction pump counter snapshot from the caller's
// per-stream state. Direction selector: TRUE = render side,
// FALSE = capture side. Any IRQL.
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
// Phase 5 (2026-04-14): IOCTL_AO_SET_PUMP_FEATURE_FLAGS C-style entry point.
// adapter.cpp (which does not include minwavert.h / minwavertstream.h) calls
// this to apply a mask-constrained render-ownership flag update to the
// currently-active cable speaker stream. cableIndex: 0 = Cable A, 1 = Cable B.
// Masks are silently restricted to AO_PUMP_FLAG_DISABLE_LEGACY_RENDER inside
// the helper implementation in minwavertstream.cpp. Returns STATUS_SUCCESS
// even when there is no active stream (treated as a no-op). Effect is
// visible at the next PumpToCurrentPositionFromQuery() invocation and the
// next UpdatePosition() render branch (1-2 position queries).
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
