//=============================================================================
// transport_engine.h
//
// Phase 6 (VB-equivalent core replacement) -- Step 1 skeleton.
//
// Owns the global shared transport timer and the active-stream scheduler
// metadata. Does NOT own the ring (that stays in loopback.cpp / FRAME_PIPE)
// and does NOT own stream lifetime (that stays in CMiniportWaveRTStream).
//
// Step 1 goals (see docs/PHASE6_PLAN.md section8):
//   - shared timer exists
//   - streams can register/unregister
//   - timer callback can schedule no-op events safely
//
// Non-goals for Step 1:
//   - moving any audio data
//   - porting diagnostic counters
//   - preserving Phase 5 rollback flags
//   - tuning the final event size
//
// Data flow will be switched onto this engine in Step 3 (render) and Step 4
// (capture). Position reporting decoupling is Step 5.
//=============================================================================

#ifndef AO_TRANSPORT_ENGINE_H
#define AO_TRANSPORT_ENGINE_H

#include <ntddk.h>
#include <wdm.h>
#include "loopback.h"   // FRAME_PIPE full definition -- engine event runners need it

// Forward declaration -- the miniport header pulls in the whole WaveRT/PortCls
// world, so we keep the stream type opaque at the engine API boundary.
class CMiniportWaveRTStream;

//=============================================================================
// AO_STREAM_RT -- per-stream transport runtime state
//
// Owned by CMiniportWaveRTStream via m_pTransportRt. The engine active list
// holds non-owning references only. All latency values are stored in frames
// (samples) -- see docs/PHASE6_PLAN.md section "Latency Unit Policy".
//=============================================================================
typedef struct _AO_STREAM_RT {
    // Active-list linkage (engine-owned field, not the stream).
    LIST_ENTRY              Link;

    // Non-owning backpointer to the stream that owns this struct.
    CMiniportWaveRTStream*  Stream;

    // Reference count. +1 from the stream owner at alloc time. The timer
    // callback takes additional refs under EngineLock while building its
    // due-stream snapshot and releases them after each event runs. The
    // allocation is freed when RefCount reaches zero -- either by the
    // destructor's FreeStreamRt call or by the last in-flight event's
    // release, whichever comes later. This is how we close the UAF race
    // between the timer callback's drop-lock-then-run pattern and stream
    // destruction.
    volatile LONG           RefCount;

    // Per-stream advance state lock. Introduced in Phase 3 Step 0 and
    // initialized in AoTransportAllocStreamRt. Phase 3 Step 1 makes
    // AoCableAdvanceByQpc acquire it before reading/writing cursor,
    // phase, and residual state; later call-source wiring funnels
    // through the helper under that discipline.
    KSPIN_LOCK              PositionLock;

    // Classification -- filled on first registration, stable thereafter.
    BOOLEAN                 IsCapture;
    BOOLEAN                 IsCable;
    BOOLEAN                 IsSpeakerSide;

    // Scheduling state -- flipped by OnRun / OnPause / OnStop.
    BOOLEAN                 Active;

    // Format snapshot -- populated at RUN, used by event runners.
    ULONG                   SampleRate;
    ULONG                   Channels;
    ULONG                   BlockAlign;

    // Target FRAME_PIPE and DMA buffer info -- populated at RUN so the
    // engine event runners can move data without looking up globals or
    // touching the stream object.
    PFRAME_PIPE             Pipe;
    BYTE*                   DmaBuffer;
    ULONG                   DmaBufferSize;

    // Render-side DMA cursor anchoring. The render event runner is NOT
    // allowed to free-run a DmaOffset that drifts away from what WaveRT
    // has actually produced into the DMA -- that makes the engine
    // re-read stale or future bytes on every cadence skew, which is
    // what the Step 3-alone live-call showed as screechy audio.
    // Instead, the stream side publishes the authoritative produced
    // byte count (monotonic LinearPosition) into DmaProducedMono from
    // CMiniportWaveRTStream::UpdatePosition. The render runner moves
    // only the produced->consumed delta.
    //
    // TODO(Phase 4 / Phase 5): Phase 3 baseline (commit 422a598)
    // restored UpdatePosition (minwavertstream.cpp:~2089) as the sole
    // publisher of DmaProducedMono for both render and capture; the
    // helper-side write inside AoCableWriteRenderFromDma (cpp:~1210)
    // is dead in shadow because RenderAudibleActive is forced FALSE.
    // Owner: event runners (cpp:~564 render, ~644 capture) + helper
    // shadow read. Retire: Phase 4 (render) + Phase 5 (capture)
    // audible flip commits transfer producer ownership to the helper.
    volatile LONGLONG       DmaProducedMono;
    ULONGLONG               DmaConsumedMono;

    // Event sizing -- frame-count authoritative.
    ULONG                   FramesPerEvent;
    ULONG                   BytesPerEvent;
    LONGLONG                EventPeriodQpc;
    LONGLONG                NextEventQpc;

    // Startup state machine -- used by Step 4 capture path.
    //
    // TODO(Phase 5): capture startup gate. StartupArmed is set TRUE on
    // capture RUN (cpp:~406), FALSE on render RUN (cpp:~414); paired
    // StartupTargetFrames (cpp:~409 init) and StartupThresholdFrames
    // (cpp:~407 init, cpp:~650 read in AoRunCaptureEvent gate).
    // Owner: capture event runner. Retire: Phase 5 capture coupling.
    BOOLEAN                 StartupArmed;
    ULONG                   StartupTargetFrames;
    ULONG                   StartupThresholdFrames;

    // Diagnostic counter -- populated by the timer DPC overdue path.
    //
    // TODO(Phase 4 / Phase 5): timer DPC overdue/drift counter.
    // Incremented by AoTransportTimerCallback overdue handler
    // (cpp:~783) when the scheduler defers more than the per-tick
    // budget. Owner: timer DPC overdue path. Retire: Phase 4 (render)
    // / Phase 5 (capture) when audible coupling lands and timer DPC
    // bookkeeping is reorganized.
    ULONG                   LateEventCount;

    // Sub-sample carry for non-integer event sizes (44.1k family etc.).
    // Still zero until the GCD SRC step lands; kept here so the later
    // work doesn't have to widen the runtime struct.
    //
    // TODO(TBD): preserved for future SRC integration. Currently
    // unused (no init/read/write outside this declaration). Owner:
    // future cable SRC integration step (no current caller). Retire:
    // TBD when 44.1 kHz cable consumer SRC integration lands.
    ULONG                   CarryFrames;

    //=========================================================================
    // Phase 6 Option Y -- Y1A runtime structure freeze
    //
    // Fields below are the VB-equivalent cable runtime state. Added in Y1A
    // (header freeze); populated + read by AoCableAdvanceByQpc starting in
    // Y1B (shadow mode -- helper computes everything, externally visible
    // truth stays on legacy MSVAD path). Retired in Y2 (render) and Y3
    // (capture) when audible ownership moves into the canonical helper.
    //
    // Source spec: results/phase6_vb_verification.md section2, section5, section7, section9.5
    // Layout mapped 1:1 to VB stream offsets where practical.
    //=========================================================================

    // Audio format (extends the Step 1 SampleRate/Channels/BlockAlign set).
    // VB +0x86: container bits; AO Y1A stores explicitly for 4-way bpp
    // dispatch in AoRingWriteFromClient (22b0 equivalent).
    USHORT                  BitsPerSample;

    // Cable ring buffer size in frames. Distinct from DmaBufferSize (bytes
    // of raw DMA container) -- this is the frame-denominated cable ring the
    // canonical helper wraps cursor arithmetic against. VB +0xA8.
    ULONG                   RingSizeFrames;

    // Canonical advance helper state -- VB 6320 direct translation.
    ULONGLONG               AnchorQpc100ns;              // VB +0x180
    ULONG                   PublishedFramesSinceAnchor;  // VB +0x198
    ULONGLONG               DmaCursorFrames;             // VB +0x0D0 (frame-denominated)
    ULONGLONG               DmaCursorFramesPrev;         // VB +0x0D8
    volatile LONGLONG       MonoFramesLow;               // VB +0x0E0
    volatile LONGLONG       MonoFramesMirror;            // VB +0x0E8 (mirror of Low)
    LONG                    LastAdvanceDelta;            // VB +0x1B8
    ULONG                   StatOverrunCounter;          // canonical helper reject count

    // Packet notification state. VB shared-mode clients never arm these
    // (NotifyArmed stays 0) so the [+0x8188] dispatch is bypassed; AO's
    // existing STATUS_NOT_SUPPORTED path for m_ulNotificationsPerBuffer==0
    // is already semantically equivalent. Preserved for event-driven
    // parity only. VB +0x7C / +0x164 / +0x165.
    ULONG                   NotifyBoundaryBytes;
    UCHAR                   NotifyArmed;
    UCHAR                   NotifyFired;

    // Per-stream scratch buffer for DMA-circular -> linear staging before
    // FRAME_PIPE conversion/write. VB stores at stream +0x178 and uses a
    // plain memcpy (FUN_140007680) into it. Allocated at RUN, freed at
    // STOP / free. Size is bounded by the worst-case single advance
    // (overrun guard = SampleRate/2 frames x BlockAlign bytes).
    PVOID                   CableScratchBuffer;
    ULONG                   CableScratchSize;

    // Fade-in envelope counter. Reset to -96 at packet boundary; advanced
    // per sample by AoApplyFadeEnvelope until it reaches 0 (saturated).
    // Drives the 95-entry +0x12a60 fade table in 51a8 equivalent.
    // This is the VB click-suppression mechanism.
    LONG                    FadeSampleCounter;

    // Y1 debug hit counters. Shadow-mode observability only -- lets the
    // Y1C gate confirm the helper is reached from query + timer paths
    // without log flooding. Remove in Y4. Guarded by AO_PHASE6_DEBUG in
    // the cpp so release builds can drop them cheaply.
    volatile LONG           DbgShadowAdvanceHits;
    volatile LONG           DbgShadowQueryHits;
    volatile LONG           DbgShadowTimerHits;

    //=========================================================================
    // Render audible-flip scaffold (Phase 6 Y2-1 / Y2-2 origin)
    //
    // When TRUE, AoCableAdvanceByQpc's render branch does the actual
    // DMA -> scratch -> FRAME_PIPE write via AoCableWriteRenderFromDma
    // and applies the fade envelope. When FALSE the render branch
    // stays in shadow mode and the legacy ReadBytes path (called
    // from CMiniportWaveRTStream::UpdatePosition) remains the audible
    // owner.
    //=========================================================================
    //
    // TODO(Phase 4): Phase 6 Y2-2 audible-flip scaffold. Phase 3
    // baseline (commit 422a598) forces this to FALSE in
    // AoTransportOnRunEx (cpp:~382) and alloc init (cpp:~1060) so the
    // AoCableAdvanceByQpc render gate (cpp:~1399) is unreachable.
    // Field, gate, and AoCableWriteRenderFromDma function body are
    // retained as scaffold for the Phase 4 audible-flip commit
    // (single-step ownership transfer per ADR-006).
    BOOLEAN                 RenderAudibleActive;

    //=========================================================================
    // Phase 6 Y2-1.5 render byte diff diagnostic
    //
    // Tracks cumulative helper-computed vs legacy-computed render byte
    // totals so Y2-2 switchover can validate that the two cursor sources
    // agree before the audible path is handed over.
    //
    // Writer discipline:
    //   - DbgY2HelperRenderBytes : bumped by AoCableAdvanceByQpc's render
    //     branch. advanceBytes = advance * BlockAlign. InterlockedAdd64.
    //   - DbgY2LegacyRenderBytes : bumped by CMiniportWaveRTStream::
    //     UpdatePosition's cable-render branch AFTER block-align.
    //     InterlockedAdd64.
    //   - DbgY2RenderByteDiffMax : running max(|helper - legacy|),
    //     updated best-effort by the helper after each advance bump.
    //   - DbgY2RenderMismatchHits : count of helper entries where the
    //     running diff was non-zero.
    //
    // Interpretation of readings:
    //   - Bounded |diff| near `8 * BlockAlign` = 8-frame gate asymmetry,
    //     expected and harmless.
    //   - Unbounded growing diff = cursor calculation divergence between
    //     QPC-delta math (helper) and ms-delta math (legacy). Y2-2 must
    //     NOT flip the audible switch while this condition exists.
    //   - Both totals moving in lockstep (diff stays near zero) = the
    //     two sources agree and Y2-2 can safely switch DmaProducedMono
    //     ownership from legacy to helper.
    //
    // Removed in Y4 with the rest of the DbgShadow* counters.
    //=========================================================================
    //
    // TODO(Phase 3 step4 / Phase 4): helper-vs-legacy render byte diff
    // diagnostic. Helper-side bumps DbgY2HelperRenderBytes /
    // DbgY2RenderByteDiffMax / DbgY2RenderMismatchHits inside
    // AoCableAdvanceByQpc shadow body (cpp:~1366-1385); 1 Hz delta
    // print uses DbgY2LastPrintQpc / DbgY2HelperPrevSnapshot /
    // DbgY2LegacyPrevSnapshot (cpp:~615-633). Legacy side bumps
    // DbgY2LegacyRenderBytes from CMiniportWaveRTStream::
    // UpdatePosition (minwavertstream.cpp:~1989). Owner: helper shadow
    // body + legacy UpdatePosition. Phase 3 step4 plans to rename
    // DbgY2RenderMismatchHits to DbgShadowDivergenceHits per
    // step4.md:23. Retire: Phase 4 with the audible-flip commit.
    volatile LONGLONG       DbgY2HelperRenderBytes;
    volatile LONGLONG       DbgY2LegacyRenderBytes;
    volatile LONGLONG       DbgY2RenderByteDiffMax;
    volatile LONG           DbgY2RenderMismatchHits;

    // Snapshots used by the 1 Hz DbgPrint in AoRunRenderEvent so the
    // log can show per-second helper/legacy byte deltas in addition to
    // cumulative totals. Written only by the timer DPC while holding
    // its per-stream reference, so no atomics required.
    //
    // TODO(Phase 4): per-second helper/legacy snapshots paired with
    // the diagnostic above. Read+written by AoCableAdvanceByQpc shadow
    // body (cpp:~615-633). Owner: 1 Hz delta print pacer in helper.
    // Retire: Phase 4 with the diagnostic block above.
    LONGLONG                DbgY2LastPrintQpc;
    LONGLONG                DbgY2HelperPrevSnapshot;
    LONGLONG                DbgY2LegacyPrevSnapshot;
} AO_STREAM_RT, *PAO_STREAM_RT;

//=============================================================================
// AO_TRANSPORT_ENGINE -- global singleton. Declared here so the public API
// documentation has something concrete to point at; the single instance lives
// in transport_engine.cpp and is not exposed directly.
//=============================================================================
typedef struct _AO_TRANSPORT_ENGINE {
    PEX_TIMER               Timer;
    KSPIN_LOCK              Lock;
    BOOLEAN                 Initialized;
    BOOLEAN                 Running;
    LONGLONG                PeriodQpc;          // default 20 ms in QPC ticks
    LONGLONG                NextTickQpc;
    ULONG                   ActiveStreamCount;
    LIST_ENTRY              ActiveStreams;
} AO_TRANSPORT_ENGINE, *PAO_TRANSPORT_ENGINE;

//=============================================================================
// Public API -- invoked by adapter.cpp (engine bring-up / teardown) and by
// CMiniportWaveRTStream::SetState (per-stream lifecycle events).
//=============================================================================

// Global engine bring-up. Called from adapter.cpp after the FRAME_PIPE globals
// have been initialized. Idempotent -- calling twice returns STATUS_SUCCESS.
NTSTATUS AoTransportEngineInit(VOID);

// Global engine teardown. Stops the timer, waits for any in-flight DPC via
// ExDeleteTimer's Wait parameter, and zeros the engine. Idempotent.
VOID     AoTransportEngineCleanup(VOID);

//=============================================================================
// Per-stream lifecycle API
//
// AO_STREAM_SNAPSHOT is the minimum set of stream facts the engine needs to
// populate a new AO_STREAM_RT on first RUN. The stream owns its AO_STREAM_RT
// allocation via its own m_pTransportRt member; the engine only holds a
// non-owning Link pointer.
//
// Usage from CMiniportWaveRTStream::SetState:
//
//   // on RUN:
//   if (m_pTransportRt == NULL)
//       m_pTransportRt = AoTransportAllocStreamRt(this);
//   AoTransportOnRunEx(&snapshotWithRt);
//
//   // on PAUSE:
//   AoTransportOnPauseEx(m_pTransportRt);
//
//   // on STOP:
//   AoTransportOnStopEx(m_pTransportRt);
//
//   // on destructor (after KeFlushQueuedDpcs):
//   AoTransportOnStopEx(m_pTransportRt);
//   AoTransportFreeStreamRt(m_pTransportRt);
//
// This "Ex" surface is the only lifecycle API Phase 6 exposes. An earlier
// Step 1 draft had a stream-pointer variant that looked up the runtime via
// the stream, but that caused the engine TU to need private miniport
// internals; the snapshot-and-rt-pointer pattern avoids that entirely.
//=============================================================================
typedef struct _AO_STREAM_SNAPSHOT {
    AO_STREAM_RT*  Rt;
    BOOLEAN        IsCapture;
    BOOLEAN        IsCable;
    BOOLEAN        IsSpeakerSide;
    ULONG          SampleRate;
    ULONG          Channels;
    ULONG          BlockAlign;
    PFRAME_PIPE    Pipe;
    BYTE*          DmaBuffer;
    ULONG          DmaBufferSize;
} AO_STREAM_SNAPSHOT;

extern "C" AO_STREAM_RT* AoTransportAllocStreamRt(CMiniportWaveRTStream* stream);
extern "C" VOID          AoTransportFreeStreamRt(AO_STREAM_RT* rt);
extern "C" VOID          AoTransportOnRunEx(const AO_STREAM_SNAPSHOT* snapshot);
extern "C" VOID          AoTransportOnPauseEx(AO_STREAM_RT* rt);
extern "C" VOID          AoTransportOnStopEx(AO_STREAM_RT* rt);

// Publish the stream side's authoritative "bytes produced so far" count
// (monotonic LinearPosition for a cable render stream) into the engine's
// runtime struct. Called from UpdatePosition whenever the render stream
// advances its byte displacement. The render event runner reads this
// value under a plain volatile load and moves only the delta between it
// and its own DmaConsumedMono cursor. Safe at DISPATCH_LEVEL; does not
// take any engine locks.
extern "C" VOID          AoTransportPublishProducedBytes(AO_STREAM_RT* rt,
                                                          ULONGLONG producedBytesMono);

//=============================================================================
// Phase 6 Option Y -- canonical cable advance API (Y1A prototypes)
//
// These functions implement the VB-equivalent cable transport model. In Y1
// the helper runs in shadow mode: it computes advance, updates shadow
// bookkeeping, and bumps diagnostic counters, but the externally visible
// truth (GetPosition return values, FRAME_PIPE contents, DMA cursors
// consumed by legacy ReadBytes/WriteBytes) stays on the legacy MSVAD path.
//
// Audible ownership migrates to this helper in Y2 (render) and Y3 (capture).
//=============================================================================

// Reason-tagged entry into AoCableAdvanceByQpc. The helper dispatches
// render vs capture paths internally based on AO_STREAM_RT::IsCapture,
// but the reason lets the helper distinguish query-driven advance (from
// GetPosition / GetPositions) from timer-driven advance (from the shared
// transport timer DPC), and lets the Y1C shadow gate count both separately.
typedef enum _AO_ADVANCE_REASON {
    AO_ADVANCE_QUERY = 0,           // from GetPosition / GetPositions
    AO_ADVANCE_TIMER_RENDER = 1,    // from shared timer DPC, render stream
    AO_ADVANCE_TIMER_CAPTURE = 2,   // from shared timer DPC, capture stream
    AO_ADVANCE_PACKET = 3,          // event-driven WaveRT packet surface
} AO_ADVANCE_REASON;

// Canonical advance helper. Single owner of cable transport/accounting
// truth once Y2/Y3 complete. nowQpcRaw is a raw KeQueryPerformanceCounter
// result (the helper converts to 100ns internally and consults the QPC
// frequency itself). Flags reserved for future use; pass 0 in Y1/Y2.
//
// Y1B rule: this function may compute and update shadow state, but must
// not write FRAME_PIPE and must not replace the legacy audible truth.
extern "C" VOID AoCableAdvanceByQpc(AO_STREAM_RT* rt,
                                     ULONGLONG nowQpcRaw,
                                     AO_ADVANCE_REASON reason,
                                     ULONG flags);

// Fade envelope application. Used by the render path inside the canonical
// helper starting in Y2 to suppress packet-boundary clicks. samples is an
// in-place int32_t array of frameCount frames x channel samples (channel-
// planar or interleaved -- envelope is applied per sample regardless).
// perStreamCounter is the stream's FadeSampleCounter, advanced by this
// function. Values match VB's 95-entry +0x12a60 table exactly.
extern "C" VOID AoApplyFadeEnvelope(LONG* samples,
                                     ULONG sampleCount,
                                     LONG* perStreamCounter);

// Reset the fade counter to -96 (pre-silence prefix). Called at packet
// boundary transitions in the render path to start a fresh fade-in.
extern "C" VOID AoResetFadeCounter(AO_STREAM_RT* rt);

// Cable-specific runtime field reset. Called from AoTransportOnStopEx and
// from the destructor path after KeFlushQueuedDpcs. Matches the VB 669c
// caller-side pattern: clears DMA cursor, monotonic counters, anchor QPC,
// published frames, and fade state. Shared timer teardown is engine-
// global and handled separately by AoTransportEngineCleanup.
extern "C" VOID AoCableResetRuntimeFields(AO_STREAM_RT* rt);

//=============================================================================
// Phase 6 Y2-1 -- render audible path API
//
// AoCableWriteRenderFromDma performs DMA-bytes -> scratch linearize ->
// normalize -> fade envelope -> FRAME_PIPE write for cable render streams.
// Called from AoCableAdvanceByQpc's render branch when RenderAudibleActive
// is set (Y2-2 flips the switch). Matches VB 22b0 + 51a8 combined path.
//
// advanceFrames is the helper-authoritative frame count for this tick.
// The function converts to bytes via rt->BlockAlign, computes the DMA
// start offset from rt->DmaProducedMono, handles the circular wrap
// across rt->DmaBufferSize boundaries, and calls FramePipeWriteFromDmaEx
// for each contiguous run with rt passed through so the envelope can
// be applied on scratch before pipe-write.
extern "C" VOID AoCableWriteRenderFromDma(AO_STREAM_RT* rt, ULONG advanceFrames);

// Opaque-pointer adapter called from loopback.cpp's FramePipeWriteFromDmaEx.
// Wraps AoApplyFadeEnvelope with the stream's FadeSampleCounter so the
// loopback TU does not need to know AO_STREAM_RT layout. Safe to call
// with rtOpaque == NULL (no-op).
extern "C" VOID AoCableApplyRenderFadeInScratch(PVOID rtOpaque,
                                                 LONG* scratch,
                                                 ULONG sampleCount);

//=============================================================================
// Phase 3 Step 2 prep -- shadow helper counter snapshot for AO_V2_DIAG
// telemetry surface.
//
// Per-stream slot fields are unprefixed (Shadow*Hits) on the ABI surface;
// they map 1:1 to the runtime AO_STREAM_RT::DbgShadow*Hits counters that
// AoCableAdvanceByQpc bumps. The "Dbg" prefix is dropped on the public
// surface because consumer-facing schema (ioctl.h, test_stream_monitor.py)
// has no use for the kernel-side debug-build trace marker.
//=============================================================================
typedef struct _AO_SHADOW_COUNTERS_PER_STREAM {
    ULONG ShadowAdvanceHits;   // <- AO_STREAM_RT::DbgShadowAdvanceHits
    ULONG ShadowQueryHits;     // <- AO_STREAM_RT::DbgShadowQueryHits
    ULONG ShadowTimerHits;     // <- AO_STREAM_RT::DbgShadowTimerHits
} AO_SHADOW_COUNTERS_PER_STREAM;

typedef struct _AO_SHADOW_COUNTERS_SNAPSHOT {
    AO_SHADOW_COUNTERS_PER_STREAM A_Render;
    AO_SHADOW_COUNTERS_PER_STREAM A_Capture;
    AO_SHADOW_COUNTERS_PER_STREAM B_Render;
    AO_SHADOW_COUNTERS_PER_STREAM B_Capture;
} AO_SHADOW_COUNTERS_SNAPSHOT;

// Snapshot the shadow helper counters for every currently-registered cable
// stream. Called from adapter.cpp's IOCTL_AO_GET_STREAM_STATUS handler.
//
// Stream identity mapping is by FRAME_PIPE pointer (g_CableAPipe vs
// g_CableBPipe) and by AO_STREAM_RT::IsCapture. Streams whose RT is not on
// the engine's ActiveStreams list at snapshot time contribute zero --
// `out` is zero-initialized on entry. Out is null-checked; passing NULL
// is a no-op.
//
// Multi-stream-per-endpoint: counters AGGREGATE per endpoint slot.
// When more than one active AO_STREAM_RT maps to the same (cable,
// direction) -- e.g. concurrent client opens, or a paused stream
// that is still on ActiveStreams alongside a new live stream -- the
// per-stream Dbg*Hits values sum into the slot. The ABI surface
// (A_R_ShadowQueryHits etc.) is named like an endpoint total, and
// aggregation keeps that reading honest. 32-bit wrap matches the
// underlying DbgShadow*Hits wrap semantics.
//
// Lock discipline: takes g_AoTransportEngine.Lock for ActiveStreams list
// stability only. The counter values themselves are bumped via
// InterlockedIncrement on a different code path; this snapshot uses
// InterlockedCompareExchange(..., 0, 0) as the no-op atomic read so the
// snapshot is not racing against the writer. Safe at DISPATCH_LEVEL.
extern "C" VOID
AoTransportSnapshotShadowCounters(AO_SHADOW_COUNTERS_SNAPSHOT* out);

#endif // AO_TRANSPORT_ENGINE_H
