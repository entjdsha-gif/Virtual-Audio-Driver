//=============================================================================
// transport_engine.cpp
//
// Phase 6 Step 1 skeleton — shared transport engine.
// See transport_engine.h for the public contract and docs/PHASE6_PLAN.md for
// the full design.
//
// Step 1 behavior:
//   - engine singleton with a high-resolution periodic timer (20 ms default)
//   - AoTransportRegisterStream allocates and links per-stream runtime
//   - AoTransportOnRun/OnPause/OnStop flip the Active flag and arm startup
//   - the timer callback iterates the active list and does NOTHING — it is a
//     safety / liveness probe only, not a transport driver
//
// Step 3 adds the render event runner here, Step 4 adds the capture runner.
//=============================================================================

#include "transport_engine.h"
#include "loopback.h"   // FRAME_PIPE + FramePipeWriteFromDma — only engine TU needs this

#define AO_TE_POOL_TAG              'eTOA'      // 'AOTe' little-endian

//
// Unit discipline: internal constants are expressed as frame counts at a
// reference sample rate. Per-stream values are scaled from the reference
// using the stream's actual SampleRate — never via a millisecond step.
//
// The Windows kernel timer API (ExSetTimer) requires 100ns units on its
// boundary, so the reference frame count is converted to 100ns exactly once
// during Init. That is the only place a 100ns-valued literal appears in the
// engine, and it is derived from the frame constant, not the other way
// around.
//
// AO_TE_REFERENCE_RATE is the reference rate used to express the other
// constants below. 48000 Hz is chosen because the vast majority of AO cable
// clients negotiate 48 kHz; the engine scales to non-48k streams at RUN time.
//
// AO_TE_EVENT_FRAMES_AT_REF is the shared-timer event quantum at the
// reference rate. 960 frames @ 48000 Hz is the VB-Cable-equivalent cadence
// step; expressing it as frames keeps the constant invariant under rate
// changes.
//
// AO_TE_STARTUP_THRESHOLD_FRAMES_AT_REF / AO_TE_STARTUP_TARGET_FRAMES_AT_REF
// are the capture startup cushion values from PHASE6_PLAN.md §6, also in
// frames. Step 4 will tune these; Step 1 seeds them so the runtime has
// sensible defaults for the no-op callback path.
//
#define AO_TE_REFERENCE_RATE                 48000U
#define AO_TE_EVENT_FRAMES_AT_REF              960U   // 20 ms-equivalent @ 48k, stored as frames
#define AO_TE_STARTUP_THRESHOLD_FRAMES_AT_REF  960U
#define AO_TE_STARTUP_TARGET_FRAMES_AT_REF    1440U

// Bounded catch-up: if the shared timer DPC is delivered late (system
// load, hibernation resume, etc.) the callback will run multiple virtual
// ticks per real tick to drain the backlog. This constant caps the per-
// callback drain so one real tick can never spend an unbounded amount of
// time at DISPATCH_LEVEL. 8 × 20 ms = 160 ms max catch-up per callback.
//
// IMPORTANT: the cap does NOT drop the excess backlog. NextEventQpc only
// advances by the capped count, so the remaining (overdueTicks - cap)
// ticks reappear as still-overdue on the next real callback and get
// processed there (possibly subject to another cap). The backlog
// converges over multiple callbacks without losing any audio frames.
// LateEventCount increments by the *deferred* tick count as a drift /
// cap-hit diagnostic; it is not a "dropped frames" counter.
#define AO_TE_MAX_OVERDUE_TICKS                  8U

//=============================================================================
// Global singleton. Not exported — all access goes through the public API in
// transport_engine.h so call sites never take a pointer to this directly.
//=============================================================================
static AO_TRANSPORT_ENGINE g_AoTransportEngine;

//=============================================================================
// Forward declarations
//=============================================================================
static EXT_CALLBACK AoTransportTimerCallback;

//=============================================================================
// Helper: QPC frequency is constant for the life of the system — we query it
// once on init and reuse it for all period conversions.
//=============================================================================
static LONGLONG g_AoTeQpcFrequency = 0;
static LONGLONG g_AoTeEventPeriod100ns = 0;    // derived from frames at boundary

// frames_at_rate -> QPC ticks (samples-only, no ms step)
static LONGLONG
AoTeFramesToQpc(ULONG frames, ULONG sampleRate)
{
    if (g_AoTeQpcFrequency <= 0 || sampleRate == 0)
    {
        return 0;
    }
    return ((LONGLONG)frames * g_AoTeQpcFrequency) / (LONGLONG)sampleRate;
}

// Scale a frame count from the engine's reference rate to an arbitrary
// stream rate. Used to translate AO_TE_*_FRAMES_AT_REF constants into the
// per-stream FramesPerEvent / StartupThresholdFrames values. No ms step.
static ULONG
AoTeScaleFramesToStreamRate(ULONG refFrames, ULONG streamSampleRate)
{
    if (streamSampleRate == 0 || AO_TE_REFERENCE_RATE == 0)
    {
        return refFrames;
    }
    return (ULONG)(((ULONGLONG)refFrames * streamSampleRate) / AO_TE_REFERENCE_RATE);
}

//=============================================================================
// AoTransportEngineInit — global bring-up. Called from adapter.cpp after
// FRAME_PIPE globals are ready. Idempotent.
//=============================================================================
NTSTATUS
AoTransportEngineInit(VOID)
{
    if (g_AoTransportEngine.Initialized)
    {
        return STATUS_SUCCESS;
    }

    RtlZeroMemory(&g_AoTransportEngine, sizeof(g_AoTransportEngine));
    KeInitializeSpinLock(&g_AoTransportEngine.Lock);
    InitializeListHead(&g_AoTransportEngine.ActiveStreams);

    LARGE_INTEGER qpcFreq;
    KeQueryPerformanceCounter(&qpcFreq);
    g_AoTeQpcFrequency = qpcFreq.QuadPart;

    // Derive the engine period in QPC ticks from the reference frame count.
    // This is the authoritative period in the engine's scheduling domain.
    g_AoTransportEngine.PeriodQpc =
        AoTeFramesToQpc(AO_TE_EVENT_FRAMES_AT_REF, AO_TE_REFERENCE_RATE);

    // Also derive the 100ns equivalent of that period for ExSetTimer, which
    // only accepts 100ns units. This is the *only* place in the engine that
    // a 100ns literal appears, and it is computed from the frame constant —
    // never hard-coded. Rounded up to the nearest 100ns to avoid under-period
    // ticks that would accumulate drift.
    g_AoTeEventPeriod100ns =
        ((LONGLONG)AO_TE_EVENT_FRAMES_AT_REF * 10000000LL) / AO_TE_REFERENCE_RATE;

    // EX_TIMER_HIGH_RESOLUTION is Win8.1+. Our minimum target is already
    // Win10 so this is safe unconditionally.
    g_AoTransportEngine.Timer = ExAllocateTimer(
        AoTransportTimerCallback,
        NULL,
        EX_TIMER_HIGH_RESOLUTION);

    if (g_AoTransportEngine.Timer == NULL)
    {
        DbgPrint("[AoTransport] ExAllocateTimer failed\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    g_AoTransportEngine.Initialized = TRUE;
    g_AoTransportEngine.Running = FALSE;

    DbgPrint("[AoTransport] engine init ok (periodQpc=%lld qpcFreq=%lld)\n",
        g_AoTransportEngine.PeriodQpc, g_AoTeQpcFrequency);

    return STATUS_SUCCESS;
}

//=============================================================================
// AoTransportEngineCleanup — global teardown. Stops the timer, waits for any
// in-flight DPC via ExDeleteTimer(Wait=TRUE), and zeroes the engine.
//=============================================================================
VOID
AoTransportEngineCleanup(VOID)
{
    if (!g_AoTransportEngine.Initialized)
    {
        return;
    }

    if (g_AoTransportEngine.Timer)
    {
        // Cancel + delete with wait. Blocks until any in-flight callback has
        // returned. Safe at PASSIVE_LEVEL.
        (VOID)ExCancelTimer(g_AoTransportEngine.Timer, NULL);
        (VOID)ExDeleteTimer(
            g_AoTransportEngine.Timer,
            TRUE,   // Cancel
            TRUE,   // Wait
            NULL);
        g_AoTransportEngine.Timer = NULL;
    }

    // Active stream list should be empty by now — every stream's destructor
    // must have called AoTransportUnregisterStream. If anything is still
    // linked, we cannot free their AO_STREAM_RT here because those buffers
    // are owned by the stream objects; just clear the engine's view.
    InitializeListHead(&g_AoTransportEngine.ActiveStreams);
    g_AoTransportEngine.ActiveStreamCount = 0;
    g_AoTransportEngine.Running = FALSE;
    g_AoTransportEngine.Initialized = FALSE;

    DbgPrint("[AoTransport] engine cleanup ok\n");
}

//=============================================================================
// AoTransportAllocStreamRt / AoTransportFreeStreamRt — low-level helpers the
// stream class uses to allocate and free its own AO_STREAM_RT without the
// engine needing to touch private stream members.
//
// Rationale: keeps include graph clean. transport_engine.cpp only knows
// about AO_STREAM_RT and a forward-declared CMiniportWaveRTStream pointer.
// The non-"Ex" stream-pointer façade from the Step 1 draft was removed —
// the Ex surface is now the only public lifecycle API.
//=============================================================================
extern "C" AO_STREAM_RT*
AoTransportAllocStreamRt(CMiniportWaveRTStream* stream)
{
    if (!g_AoTransportEngine.Initialized)
    {
        return NULL;
    }

    AO_STREAM_RT* rt = (AO_STREAM_RT*)ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        sizeof(AO_STREAM_RT),
        AO_TE_POOL_TAG);

    if (rt == NULL)
    {
        return NULL;
    }

    RtlZeroMemory(rt, sizeof(*rt));
    InitializeListHead(&rt->Link);
    rt->Stream = stream;
    rt->Active = FALSE;
    rt->RefCount = 1;   // stream owner holds the initial ref
    return rt;
}

// Publish the stream's authoritative produced byte count into the runtime
// struct so the render event runner can anchor its cursor against it.
// Called from CMiniportWaveRTStream::UpdatePosition after LinearPosition
// advances. Monotonic — callers must never pass a value smaller than the
// previous publish. We store via InterlockedExchange64 so DISPATCH-level
// readers see a consistent 64-bit value even on unusual alignments.
extern "C" VOID
AoTransportPublishProducedBytes(AO_STREAM_RT* rt, ULONGLONG producedBytesMono)
{
    if (rt == NULL)
    {
        return;
    }
    InterlockedExchange64(&rt->DmaProducedMono, (LONGLONG)producedBytesMono);
}

// Internal: drop one ref, free if zero. Caller must not touch rt after.
// Safe to call at DISPATCH_LEVEL (no lock acquired, no paged memory access
// beyond the pool free which is non-paged).
static VOID
AoTeReleaseRt(AO_STREAM_RT* rt)
{
    if (rt == NULL)
    {
        return;
    }
    if (InterlockedDecrement(&rt->RefCount) == 0)
    {
        ExFreePoolWithTag(rt, AO_TE_POOL_TAG);
    }
}

extern "C" VOID
AoTransportFreeStreamRt(AO_STREAM_RT* rt)
{
    if (rt == NULL)
    {
        return;
    }

    // Defensive: make sure the rt is no longer linked into the active list.
    // A well-behaved caller should have invoked OnStopEx first, but this
    // path must be idempotent in case the destructor order drifts.
    KIRQL old;
    KeAcquireSpinLock(&g_AoTransportEngine.Lock, &old);
    if (rt->Link.Flink != NULL && !IsListEmpty(&rt->Link))
    {
        RemoveEntryList(&rt->Link);
        if (g_AoTransportEngine.ActiveStreamCount > 0)
        {
            g_AoTransportEngine.ActiveStreamCount--;
        }
        InitializeListHead(&rt->Link);
    }
    KeReleaseSpinLock(&g_AoTransportEngine.Lock, old);

    // Drop the owner ref. If the timer callback still holds a ref from a
    // previous due-list snapshot, free is deferred until that ref drains.
    AoTeReleaseRt(rt);
}

//=============================================================================
// AoTransportOnRunEx — stream transitioned to KSSTATE_RUN.
//
// Actions:
//   - snapshot format fields into the runtime struct
//   - compute FramesPerEvent at the stream's own sample rate from the
//     reference frame-per-event constant (samples-only, no ms step)
//   - link into ActiveStreams
//   - arm StartupArmed for capture direction
//   - first active stream arms the shared timer
//
// AO_STREAM_SNAPSHOT is declared in transport_engine.h so stream-side
// callers and the engine share the definition.
//=============================================================================
extern "C" VOID
AoTransportOnRunEx(const AO_STREAM_SNAPSHOT* snapshot)
{
    if (snapshot == NULL || snapshot->Rt == NULL)
    {
        return;
    }

    AO_STREAM_RT* rt = snapshot->Rt;

    KIRQL old;
    KeAcquireSpinLock(&g_AoTransportEngine.Lock, &old);

    rt->IsCapture     = snapshot->IsCapture;
    rt->IsCable       = snapshot->IsCable;
    rt->IsSpeakerSide = snapshot->IsSpeakerSide;
    rt->SampleRate    = snapshot->SampleRate;
    rt->Channels      = snapshot->Channels;
    rt->BlockAlign    = snapshot->BlockAlign;
    rt->Pipe          = snapshot->Pipe;
    rt->DmaBuffer     = snapshot->DmaBuffer;
    rt->DmaBufferSize = snapshot->DmaBufferSize;

    // Fresh session → reset monotonic cursors. Both render and capture
    // anchor their event runners against rt->DmaProducedMono, which is
    // published by the stream side via AoTransportPublishProducedBytes
    // from within CMiniportWaveRTStream::UpdatePosition whenever the
    // stream's LinearPosition advances.
    rt->DmaProducedMono  = 0;
    rt->DmaConsumedMono  = 0;

    // Samples-only seeding: scale the reference frame constants to this
    // stream's actual rate. The engine period in QPC is the same for every
    // stream (shared timer), but each stream may have a different
    // FramesPerEvent count because the stream rate can differ from the
    // reference rate (e.g. 44.1k cable consumer reading a 48k pipe).
    if (rt->SampleRate > 0)
    {
        rt->FramesPerEvent =
            AoTeScaleFramesToStreamRate(AO_TE_EVENT_FRAMES_AT_REF, rt->SampleRate);
        rt->BytesPerEvent  = rt->FramesPerEvent * rt->BlockAlign;
        rt->EventPeriodQpc = g_AoTransportEngine.PeriodQpc;

        LARGE_INTEGER now = KeQueryPerformanceCounter(NULL);
        rt->NextEventQpc = now.QuadPart + rt->EventPeriodQpc;
    }

    // Arm startup for capture side. Render side does not use the startup
    // gate. Thresholds come from the reference frame constants, scaled to
    // the stream's sample rate. Step 4 tunes the actual numbers; Step 1
    // just seeds them so the runtime struct has sensible defaults.
    if (rt->IsCapture)
    {
        rt->StartupArmed = TRUE;
        rt->StartupThresholdFrames =
            AoTeScaleFramesToStreamRate(AO_TE_STARTUP_THRESHOLD_FRAMES_AT_REF, rt->SampleRate);
        rt->StartupTargetFrames    =
            AoTeScaleFramesToStreamRate(AO_TE_STARTUP_TARGET_FRAMES_AT_REF, rt->SampleRate);
    }
    else
    {
        rt->StartupArmed = FALSE;
    }

    // Link into active list if not already linked. Active flag gates event
    // processing but link presence gates visibility to the callback.
    if (rt->Link.Flink == NULL || IsListEmpty(&rt->Link))
    {
        InsertTailList(&g_AoTransportEngine.ActiveStreams, &rt->Link);
        g_AoTransportEngine.ActiveStreamCount++;
    }

    rt->Active = TRUE;

    // First active stream arms the timer. Subsequent streams just link in.
    // g_AoTeEventPeriod100ns was computed from the reference frame constant
    // during Init — it is the single boundary where the engine interacts
    // with the Windows 100ns unit system. Everything upstream/downstream of
    // this line is in frames or QPC ticks.
    if (!g_AoTransportEngine.Running && g_AoTransportEngine.Timer != NULL)
    {
        LARGE_INTEGER dueTime;
        dueTime.QuadPart = -g_AoTeEventPeriod100ns;
        (VOID)ExSetTimer(
            g_AoTransportEngine.Timer,
            dueTime.QuadPart,
            g_AoTeEventPeriod100ns,
            NULL);
        g_AoTransportEngine.Running = TRUE;
        DbgPrint("[AoTransport] timer armed\n");
    }

    KeReleaseSpinLock(&g_AoTransportEngine.Lock, old);

    DbgPrint("[AoTransport] OnRun rt=%p cap=%d spk=%d cable=%d rate=%u fpe=%u\n",
        rt, rt->IsCapture, rt->IsSpeakerSide, rt->IsCable,
        rt->SampleRate, rt->FramesPerEvent);
}

//=============================================================================
// AoTransportOnPauseEx — stream paused. Flip Active flag off, keep link.
//=============================================================================
extern "C" VOID
AoTransportOnPauseEx(AO_STREAM_RT* rt)
{
    if (rt == NULL)
    {
        return;
    }

    KIRQL old;
    KeAcquireSpinLock(&g_AoTransportEngine.Lock, &old);
    rt->Active = FALSE;
    KeReleaseSpinLock(&g_AoTransportEngine.Lock, old);

    DbgPrint("[AoTransport] OnPause rt=%p\n", rt);
}

//=============================================================================
// AoTransportOnStopEx — stream fully stopped. Detach from active list, clear
// Active flag. Does NOT free the AO_STREAM_RT — that is the stream
// destructor's responsibility via AoTransportFreeStreamRt.
//
// If this was the last active stream, cancel the shared timer so we don't
// churn DPCs indefinitely after all streams are gone. The timer is re-armed
// on the next OnRunEx via the same Running flag check.
//=============================================================================
extern "C" VOID
AoTransportOnStopEx(AO_STREAM_RT* rt)
{
    if (rt == NULL)
    {
        return;
    }

    BOOLEAN shouldCancelTimer = FALSE;

    KIRQL old;
    KeAcquireSpinLock(&g_AoTransportEngine.Lock, &old);

    rt->Active = FALSE;

    if (rt->Link.Flink != NULL && !IsListEmpty(&rt->Link))
    {
        RemoveEntryList(&rt->Link);
        if (g_AoTransportEngine.ActiveStreamCount > 0)
        {
            g_AoTransportEngine.ActiveStreamCount--;
        }
        InitializeListHead(&rt->Link);
    }

    if (g_AoTransportEngine.ActiveStreamCount == 0 && g_AoTransportEngine.Running)
    {
        shouldCancelTimer = TRUE;
        g_AoTransportEngine.Running = FALSE;
    }

    KeReleaseSpinLock(&g_AoTransportEngine.Lock, old);

    // Cancel the timer outside the spinlock. ExCancelTimer is safe at
    // PASSIVE_LEVEL and must not hold another spinlock.
    if (shouldCancelTimer && g_AoTransportEngine.Timer != NULL)
    {
        (VOID)ExCancelTimer(g_AoTransportEngine.Timer, NULL);
        DbgPrint("[AoTransport] timer cancelled (last stream stopped)\n");
    }

    DbgPrint("[AoTransport] OnStop rt=%p\n", rt);
}

//=============================================================================
// Event runners — Step 3 wires the render runner. Step 4 wires capture.
//=============================================================================

// AoComputeFramesForEvent — return the number of frames this tick should
// move for this stream. Step 3 scope handles only rates that divide the
// reference-rate frame constant evenly (48 kHz, 96 kHz, 192 kHz). Non-
// integer cases (44.1 kHz family) will use rt->CarryFrames in a later step;
// for now they still land on the integer floor and the carry is updated
// so the error averages to zero over time.
//
// Unused during "Option Z" revert (both event runners are no-ops), kept
// here so the Option Y re-implementation can call it unchanged.
// NOLINTNEXTLINE — intentionally unreferenced during revert.
#pragma warning(push)
#pragma warning(disable: 4505)   // unreferenced local function has been removed
static ULONG
AoComputeFramesForEvent(AO_STREAM_RT* rt)
{
    return rt->FramesPerEvent;
}
#pragma warning(pop)

// AoRunRenderEvent — move produced-but-not-yet-consumed bytes from the
// stream's DMA buffer into its FRAME_PIPE ring.
//
// Anchor: rt->DmaProducedMono is the monotonic byte count the stream side
// has certified as actually produced by the WaveRT client. The engine
// owns rt->DmaConsumedMono and advances it only by the delta it actually
// moved this tick. The engine never re-reads DMA bytes that have already
// been consumed, and never reads past what WaveRT has published — so
// stale/future reads are impossible regardless of how far the engine
// timer drifts from the WaveRT clock.
//
// Per-tick budget: AoComputeFramesForEvent(rt) frames. If the WaveRT
// client has produced less than that since the previous tick (stream
// briefly idle, or engine ran early), this tick moves whatever is
// available and catches up next tick. If the client has produced more
// (we ran late), this tick moves the budgeted chunk and the next tick
// catches up. Either way no data is lost and no stale data is re-read,
// because DmaConsumedMono only ever advances by "bytes actually moved".
//
// Runs at DISPATCH_LEVEL from the timer callback. Acquires PipeLock via
// FramePipeWriteFromDma — never holds EngineLock while calling in.
static VOID
AoRunRenderEvent(AO_STREAM_RT* rt, LONGLONG qpc)
{
    UNREFERENCED_PARAMETER(qpc);
    UNREFERENCED_PARAMETER(rt);

    // Phase 6 "Option Z" revert: render transport runs from
    // CMiniportWaveRTStream::UpdatePosition's legacy ReadBytes path,
    // to restore the coupled update-chain-and-transport invariant that
    // VB-Cable relies on. The engine scaffold stays wired (timer, due
    // list, snapshot, refcount) so Option Y (full VB-equivalent
    // structure — UpdatePosition no-op for cable streams, engine 1 ms
    // timer owning position + transport + cursor together) can be
    // dropped in without touching the rest of the lifecycle plumbing.
    //
    // Prior body of this function (DmaProducedMono anchored render
    // transport) preserved in git history at commit a8ac0fb.
}

// AoRunCaptureEvent — fill the capture DMA with pipe content up to the
// WaveRT-anchored target byte count. Same cursor contract as render: the
// engine never free-runs a DmaOffset, never stomps bytes the client has
// not yet read, and never leaves bytes the client expects unfilled.
//
// Anchor: rt->DmaProducedMono for a capture stream carries the stream
// side's "clock-target bytes" value — that is, LinearPosition, which
// UpdatePosition advances based on elapsed QPC × sample rate. Engine
// fills DMA bytes from its own DmaConsumedMono cursor up to whichever
// of (anchor, cursor + per-tick budget) is smaller.
//
// Startup gate: if the ring fill hasn't reached StartupThresholdFrames
// yet, the entire target region is zero-filled to DMA and the pipe read
// cursor is NOT advanced. Once the cushion is ready, StartupArmed is
// dropped.
//
// Partial read / tail zero fill: ring underruns are detected before the
// read and counted in rt->UnderrunEvents. FramePipeReadToDma already
// silence-fills any shortfall internally so the DMA tail is guaranteed
// clean regardless.
//
// Runs at DISPATCH_LEVEL. Acquires PipeLock via FramePipeReadToDma.
static VOID
AoRunCaptureEvent(AO_STREAM_RT* rt, LONGLONG qpc)
{
    UNREFERENCED_PARAMETER(qpc);
    UNREFERENCED_PARAMETER(rt);

    // Phase 6 "Option Z" revert: capture transport runs from
    // CMiniportWaveRTStream::UpdatePosition's legacy WriteBytes path.
    // See AoRunRenderEvent above for rationale — Option Y will replace
    // both runners with a VB-equivalent implementation that couples
    // update chain and transport in the engine itself.
    //
    // Prior body (DmaProducedMono-anchored capture runner with startup
    // state machine and underrun counter) preserved in git history at
    // commit a8ac0fb.
}

//=============================================================================
// AoTransportTimerCallback — shared timer fires here at the engine period.
//
// Pattern: snapshot due streams under EngineLock (taking a ref on each so
// the destructor cannot free the rt while we still hold a pointer), drop
// EngineLock, run events, release refs (freeing any rt whose final ref we
// are holding).
//
// Step 3 dispatches render events for speaker streams. Capture streams are
// collected into the snapshot but their events are still no-ops — Step 4
// fills in the capture runner.
//=============================================================================
static VOID
AoTransportTimerCallback(
    _In_ PEX_TIMER Timer,
    _In_opt_ PVOID Context)
{
    UNREFERENCED_PARAMETER(Timer);
    UNREFERENCED_PARAMETER(Context);

    // Per-stream catch-up entry. The snapshot under engine lock records
    // how many virtual ticks each stream is overdue and the QPC time that
    // corresponds to virtual tick #0 for that stream. The execution phase
    // replays virtual ticks in per-tick render->capture order and passes
    // each runner the QPC time that logically belongs to that virtual
    // tick (not the real "now"). That keeps any future drift-correction
    // or per-tick diagnostics meaningful — five replayed ticks should
    // look like five distinct logical points on the timeline, not five
    // events stamped with the same wall-clock time.
    typedef struct _AO_TE_DUE_ENTRY {
        AO_STREAM_RT* Rt;
        ULONG         OverdueCount;   // >= 1 for every entry in due[]
        LONGLONG      BaseTickQpc;    // QPC time of this stream's virtual tick #0
        LONGLONG      PeriodQpc;      // per-stream tick period, copied so the
                                      // execution loop does not chase rt->
    } AO_TE_DUE_ENTRY;

    // ActiveStreams should be tiny (typically ≤ 4: Cable A/B render +
    // capture for one client each). A fixed local array is safe.
    AO_TE_DUE_ENTRY due[16];
    ULONG           dueCount   = 0;
    ULONG           maxOverdue = 0;

    KIRQL old;
    KeAcquireSpinLock(&g_AoTransportEngine.Lock, &old);

    LARGE_INTEGER now = KeQueryPerformanceCounter(NULL);

    PLIST_ENTRY entry = g_AoTransportEngine.ActiveStreams.Flink;
    while (entry != &g_AoTransportEngine.ActiveStreams && dueCount < 16)
    {
        AO_STREAM_RT* rt = CONTAINING_RECORD(entry, AO_STREAM_RT, Link);
        entry = entry->Flink;

        if (!rt->Active)
        {
            continue;
        }

        // Defense: EventPeriodQpc must be a positive tick count for the
        // divide-and-advance math below to be meaningful. A zero or
        // negative period would be a construction-time bug in OnRunEx
        // (SampleRate == 0, QPC frequency not yet known, etc.). Skip the
        // stream entirely and leave NextEventQpc untouched — OnRunEx has
        // another chance to populate the period on the next state change.
        if (rt->EventPeriodQpc <= 0)
        {
            continue;
        }

        if (now.QuadPart < rt->NextEventQpc)
        {
            continue;
        }

        // Virtual tick #0 for this stream is the tick it was supposed to
        // fire on — the current NextEventQpc value BEFORE we advance it.
        // Save it now so the execution phase can compute each virtual
        // tick's own QPC time as BaseTickQpc + tickIdx * PeriodQpc.
        LONGLONG baseTickQpc = rt->NextEventQpc;

        // Compute overdue ticks including the one we are about to run.
        // `1 +` because at `now == NextEventQpc` exactly we still owe the
        // stream one tick; anything past that is bonus catch-up.
        LONGLONG behindQpc = now.QuadPart - rt->NextEventQpc;
        ULONG    overdueTicks =
            1U + (ULONG)(behindQpc / rt->EventPeriodQpc);

        // Bounded catch-up — see AO_TE_MAX_OVERDUE_TICKS header comment.
        // Anything past the cap is deferred: we advance NextEventQpc
        // only by the capped count, so the remaining backlog ticks
        // stay "overdue" and get picked up on the next real callback.
        // LateEventCount is bumped by the deferred count as a drift /
        // cap-hit diagnostic — NOT a dropped-frames counter. Audio
        // itself is not lost here; it is just processed one (or more)
        // callbacks later.
        if (overdueTicks > AO_TE_MAX_OVERDUE_TICKS)
        {
            rt->LateEventCount += (overdueTicks - AO_TE_MAX_OVERDUE_TICKS);
            overdueTicks = AO_TE_MAX_OVERDUE_TICKS;
        }

        // Advance NextEventQpc by exactly the number of ticks we are
        // taking responsibility for in this callback. Any backlog past
        // the cap remains as still-overdue time and will be handled on
        // the next real callback (possibly subject to another cap if
        // the drift keeps growing).
        rt->NextEventQpc += (LONGLONG)overdueTicks * rt->EventPeriodQpc;

        // Take a ref under the engine lock so the destructor cannot free
        // this rt while we still hold a pointer in due[].
        InterlockedIncrement(&rt->RefCount);

        due[dueCount].Rt           = rt;
        due[dueCount].OverdueCount = overdueTicks;
        due[dueCount].BaseTickQpc  = baseTickQpc;
        due[dueCount].PeriodQpc    = rt->EventPeriodQpc;
        dueCount++;

        if (overdueTicks > maxOverdue)
        {
            maxOverdue = overdueTicks;
        }
    }

    KeReleaseSpinLock(&g_AoTransportEngine.Lock, old);

    // Execution phase — always outside the engine lock. Iterate one
    // virtual tick at a time and do the full render-then-capture pass
    // for that virtual tick before moving on, so that within each
    // virtual tick Cable B mic reads the same tick's render content.
    // A stream with fewer overdue ticks than the maximum only
    // participates until it runs out.
    //
    // The QPC time passed to each runner is the *virtual* tick's own
    // time (BaseTickQpc + tickIdx * PeriodQpc), not the real "now"
    // wall-clock. This is per-stream because EventPeriodQpc is per-
    // stream even when the shared timer period is uniform today; it
    // keeps diagnostics, drift correction, and future startup-window
    // logging meaningful.
    for (ULONG tickIdx = 0; tickIdx < maxOverdue; tickIdx++)
    {
        // Render pass for this virtual tick
        for (ULONG i = 0; i < dueCount; i++)
        {
            if (due[i].OverdueCount > tickIdx && !due[i].Rt->IsCapture)
            {
                LONGLONG tickQpc =
                    due[i].BaseTickQpc + (LONGLONG)tickIdx * due[i].PeriodQpc;
                AoRunRenderEvent(due[i].Rt, tickQpc);
            }
        }
        // Capture pass for this virtual tick (same tick — cable pair
        // causal order: write to pipe first, then read from pipe).
        for (ULONG i = 0; i < dueCount; i++)
        {
            if (due[i].OverdueCount > tickIdx && due[i].Rt->IsCapture)
            {
                LONGLONG tickQpc =
                    due[i].BaseTickQpc + (LONGLONG)tickIdx * due[i].PeriodQpc;
                AoRunCaptureEvent(due[i].Rt, tickQpc);
            }
        }
    }

    // Release refs. Any rt whose final owner released after we snapshotted
    // it will be freed here.
    for (ULONG i = 0; i < dueCount; i++)
    {
        AoTeReleaseRt(due[i].Rt);
    }
}
