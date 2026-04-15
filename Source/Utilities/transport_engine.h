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

// Forward declaration — avoids pulling the full miniport header into callers
// that only need the engine surface.
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

    // Classification — filled on first registration, stable thereafter.
    BOOLEAN                 IsCapture;
    BOOLEAN                 IsCable;
    BOOLEAN                 IsSpeakerSide;

    // Scheduling state — flipped by OnRun / OnPause / OnStop.
    BOOLEAN                 Active;

    // Format snapshot — populated at RUN, used by event runners in Step 3/4.
    ULONG                   SampleRate;
    ULONG                   Channels;
    ULONG                   BlockAlign;

    // Event sizing — frame-count authoritative. Derived fields are optional
    // caches filled in Step 2+.
    ULONG                   FramesPerEvent;
    ULONG                   BytesPerEvent;
    LONGLONG                EventPeriodQpc;
    LONGLONG                NextEventQpc;

    // Startup state machine — used by Step 4 capture path.
    BOOLEAN                 StartupArmed;
    ULONG                   StartupTargetFrames;
    ULONG                   StartupThresholdFrames;
    ULONG                   MinHeadroomFrames;

    // Diagnostic counters — filled in Step 3/4 when events actually run.
    ULONG                   LateEventCount;
    ULONG                   UnderrunEvents;
    ULONG                   DropEvents;

    // DMA-side cursor and sub-sample carry (non-integer event-size rates).
    ULONG                   DmaOffset;
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

// Allocate (if needed) and register a stream's runtime state with the engine.
// The stream takes ownership of the returned AO_STREAM_RT via its own
// m_pTransportRt member — the engine only holds a non-owning link pointer.
NTSTATUS AoTransportRegisterStream(CMiniportWaveRTStream* stream);

// Detach the stream from the engine and free its runtime state. Safe to call
// even if the stream was never registered.
VOID     AoTransportUnregisterStream(CMiniportWaveRTStream* stream);

// Lifecycle hooks called from CMiniportWaveRTStream::SetState transitions.
// Step 1: these set the Active flag and update the active list; no data
// movement, no timer tick scheduling side effects beyond engine registration.
VOID     AoTransportOnRun(CMiniportWaveRTStream* stream);
VOID     AoTransportOnPause(CMiniportWaveRTStream* stream);
VOID     AoTransportOnStop(CMiniportWaveRTStream* stream);

//=============================================================================
// Stream accessor shims
//
// The public API above takes CMiniportWaveRTStream* so adapter-side / test
// code can use it without knowing the runtime struct layout. The stream
// class itself owns AO_STREAM_RT and needs direct access to it during RUN /
// PAUSE / STOP and in the destructor. The "Ex" variants below take the
// runtime pointer directly and skip the stream lookup.
//
// AO_STREAM_SNAPSHOT is the minimum set of stream facts the engine needs to
// populate a new AO_STREAM_RT on first RUN, kept separate so the stream
// class does not have to expose its internals through a header pulled into
// the engine TU.
//=============================================================================
typedef struct _AO_STREAM_SNAPSHOT {
    AO_STREAM_RT*  Rt;
    BOOLEAN        IsCapture;
    BOOLEAN        IsCable;
    BOOLEAN        IsSpeakerSide;
    ULONG          SampleRate;
    ULONG          Channels;
    ULONG          BlockAlign;
} AO_STREAM_SNAPSHOT;

extern "C" AO_STREAM_RT* AoTransportAllocStreamRt(CMiniportWaveRTStream* stream);
extern "C" VOID          AoTransportFreeStreamRt(AO_STREAM_RT* rt);
extern "C" VOID          AoTransportOnRunEx(const AO_STREAM_SNAPSHOT* snapshot);
extern "C" VOID          AoTransportOnPauseEx(AO_STREAM_RT* rt);
extern "C" VOID          AoTransportOnStopEx(AO_STREAM_RT* rt);

#endif // AO_TRANSPORT_ENGINE_H
