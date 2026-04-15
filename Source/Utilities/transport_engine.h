//=============================================================================
// transport_engine.h
//
// Phase 6 (VB-equivalent core replacement) — Step 1 skeleton.
//
// Owns the global shared transport timer and the active-stream scheduler
// metadata. Does NOT own the ring (that stays in loopback.cpp / FRAME_PIPE)
// and does NOT own stream lifetime (that stays in CMiniportWaveRTStream).
//
// Step 1 goals (see docs/PHASE6_PLAN.md §8):
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
#include "loopback.h"   // FRAME_PIPE full definition — engine event runners need it

// Forward declaration — the miniport header pulls in the whole WaveRT/PortCls
// world, so we keep the stream type opaque at the engine API boundary.
class CMiniportWaveRTStream;

//=============================================================================
// AO_STREAM_RT — per-stream transport runtime state
//
// Owned by CMiniportWaveRTStream via m_pTransportRt. The engine active list
// holds non-owning references only. All latency values are stored in frames
// (samples) — see docs/PHASE6_PLAN.md § "Latency Unit Policy".
//=============================================================================
typedef struct _AO_STREAM_RT {
    // Active-list linkage (engine-owned field, not the stream).
    LIST_ENTRY              Link;

    // Non-owning backpointer to the stream that owns this struct.
    CMiniportWaveRTStream*  Stream;

    // Reference count. +1 from the stream owner at alloc time. The timer
    // callback takes additional refs under EngineLock while building its
    // due-stream snapshot and releases them after each event runs. The
    // allocation is freed when RefCount reaches zero — either by the
    // destructor's FreeStreamRt call or by the last in-flight event's
    // release, whichever comes later. This is how we close the UAF race
    // between the timer callback's drop-lock-then-run pattern and stream
    // destruction.
    volatile LONG           RefCount;

    // Classification — filled on first registration, stable thereafter.
    BOOLEAN                 IsCapture;
    BOOLEAN                 IsCable;
    BOOLEAN                 IsSpeakerSide;

    // Scheduling state — flipped by OnRun / OnPause / OnStop.
    BOOLEAN                 Active;

    // Format snapshot — populated at RUN, used by event runners.
    ULONG                   SampleRate;
    ULONG                   Channels;
    ULONG                   BlockAlign;

    // Target FRAME_PIPE and DMA buffer info — populated at RUN so the
    // engine event runners can move data without looking up globals or
    // touching the stream object.
    PFRAME_PIPE             Pipe;
    BYTE*                   DmaBuffer;
    ULONG                   DmaBufferSize;

    // Render-side DMA cursor anchoring. The render event runner is NOT
    // allowed to free-run a DmaOffset that drifts away from what WaveRT
    // has actually produced into the DMA — that makes the engine re-read
    // stale or future bytes on every cadence skew, which is what the
    // Step 3-alone live-call showed as screechy audio. Instead, the
    // stream side publishes the authoritative produced byte count
    // (monotonic LinearPosition for cable render streams) into
    // DmaProducedMono from CMiniportWaveRTStream::UpdatePosition. The
    // render runner moves only the produced->consumed delta.
    volatile LONGLONG       DmaProducedMono;
    ULONGLONG               DmaConsumedMono;

    // Event sizing — frame-count authoritative.
    ULONG                   FramesPerEvent;
    ULONG                   BytesPerEvent;
    LONGLONG                EventPeriodQpc;
    LONGLONG                NextEventQpc;

    // Startup state machine — used by Step 4 capture path.
    BOOLEAN                 StartupArmed;
    ULONG                   StartupTargetFrames;
    ULONG                   StartupThresholdFrames;
    ULONG                   MinHeadroomFrames;

    // Diagnostic counters — populated by event runners in Step 3/4.
    ULONG                   LateEventCount;
    ULONG                   UnderrunEvents;
    ULONG                   DropEvents;

    // Sub-sample carry for non-integer event sizes (44.1k family etc.).
    // Still zero until the GCD SRC step lands; kept here so the later
    // work doesn't have to widen the runtime struct.
    ULONG                   CarryFrames;
} AO_STREAM_RT, *PAO_STREAM_RT;

//=============================================================================
// AO_TRANSPORT_ENGINE — global singleton. Declared here so the public API
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
// Public API — invoked by adapter.cpp (engine bring-up / teardown) and by
// CMiniportWaveRTStream::SetState (per-stream lifecycle events).
//=============================================================================

// Global engine bring-up. Called from adapter.cpp after the FRAME_PIPE globals
// have been initialized. Idempotent — calling twice returns STATUS_SUCCESS.
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

#endif // AO_TRANSPORT_ENGINE_H
