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

#define AO_TE_POOL_TAG              'eTOA'      // 'AOTe' little-endian
#define AO_TE_DEFAULT_EVENT_PERIOD_100NS  (200000LL)   // 20 ms in 100ns units

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

static LONGLONG
AoTePeriod100nsToQpc(LONGLONG period100ns)
{
    // period_qpc = period_100ns * qpc_freq / 10_000_000
    // Guard: if qpc_freq isn't known yet, return the period as-is so caller
    // can still do a sane comparison without dividing by zero.
    if (g_AoTeQpcFrequency <= 0)
    {
        return period100ns;
    }
    return (period100ns * g_AoTeQpcFrequency) / 10000000LL;
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

    g_AoTransportEngine.PeriodQpc =
        AoTePeriod100nsToQpc(AO_TE_DEFAULT_EVENT_PERIOD_100NS);

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
// AoTransportRegisterStream — allocate and attach a per-stream runtime. The
// caller (CMiniportWaveRTStream) stores the returned pointer in its own
// m_pTransportRt member and owns its lifetime.
//
// Step 1 does NOT start the timer from here — see AoTransportOnRun.
//=============================================================================
NTSTATUS
AoTransportRegisterStream(CMiniportWaveRTStream* stream)
{
    if (stream == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }
    if (!g_AoTransportEngine.Initialized)
    {
        return STATUS_DEVICE_NOT_READY;
    }

    AO_STREAM_RT* rt = (AO_STREAM_RT*)ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        sizeof(AO_STREAM_RT),
        AO_TE_POOL_TAG);

    if (rt == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(rt, sizeof(*rt));
    InitializeListHead(&rt->Link);
    rt->Stream = stream;
    rt->Active = FALSE;

    // We do NOT link into ActiveStreams yet — OnRun is the entry point for
    // scheduling visibility. Register just allocates and attaches ownership
    // to the stream.

    // Step 2 will extend this to reach into the stream object (via a public
    // accessor) to set the pointer. For Step 1 the caller is expected to
    // receive the pointer via its own m_pTransportRt assignment path.

    DbgPrint("[AoTransport] register stream=%p rt=%p\n", stream, rt);

    // Hand the allocation back to the caller via the stream's own field —
    // see minwavertstream.cpp wiring. We return STATUS_SUCCESS here and
    // trust the caller to install the pointer. If the caller doesn't, the
    // allocation leaks until driver teardown; Step 2 tightens this contract.
    //
    // NOTE: a true public setter would be cleaner. Step 1 keeps the wiring
    // minimal so the skeleton is easy to bisect if something goes wrong.

    // We can't access the stream object's private members from here without
    // pulling in the full miniport header, and that header pulls half the
    // WDM world in. For Step 1 we use an out-parameter shim instead — see
    // the companion AoTransportAllocStreamRt helper below.
    ExFreePoolWithTag(rt, AO_TE_POOL_TAG);
    return STATUS_NOT_IMPLEMENTED;
}

//=============================================================================
// AoTransportAllocStreamRt / AoTransportFreeStreamRt — low-level helpers the
// stream class uses to allocate and free its own AO_STREAM_RT without the
// engine needing to touch private stream members.
//
// Rationale: keeps include graph clean. transport_engine.cpp only knows
// about AO_STREAM_RT and a forward-declared CMiniportWaveRTStream pointer.
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
    return rt;
}

extern "C" VOID
AoTransportFreeStreamRt(AO_STREAM_RT* rt)
{
    if (rt == NULL)
    {
        return;
    }

    KIRQL old;
    KeAcquireSpinLock(&g_AoTransportEngine.Lock, &old);

    // Detach from active list if still linked. A well-behaved caller should
    // have called OnStop first, but we defensively detach here.
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

    ExFreePoolWithTag(rt, AO_TE_POOL_TAG);
}

//=============================================================================
// AoTransportUnregisterStream — remove the stream from engine view. The
// stream's AO_STREAM_RT buffer is freed here.
//=============================================================================
VOID
AoTransportUnregisterStream(CMiniportWaveRTStream* stream)
{
    // The engine only holds a non-owning link via the AO_STREAM_RT. Callers
    // wanting to actually free the allocation must call AoTransportFreeStreamRt
    // on their own m_pTransportRt. This function exists as a symmetry to
    // Register and is a placeholder for Step 2+ bookkeeping.
    UNREFERENCED_PARAMETER(stream);
}

//=============================================================================
// AoTransportOnRun — stream transitioned to KSSTATE_RUN. Step 1 action:
//   - snapshot format fields from the stream (via accessors, not direct)
//   - link into ActiveStreams
//   - arm StartupArmed = TRUE (for capture)
//   - first active stream starts the timer
//=============================================================================

// AO_STREAM_SNAPSHOT is declared in transport_engine.h — stream-side callers
// and the engine share the same definition via that header.

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

    // Step 1: event-size tuning is a non-goal. We seed FramesPerEvent at
    // (SampleRate * 20 / 1000) so the field has a reasonable default for
    // Step 3/4 to start from, but it is NOT used for anything yet because
    // the callback is a no-op.
    if (rt->SampleRate > 0)
    {
        rt->FramesPerEvent = (rt->SampleRate * 20) / 1000;
        rt->BytesPerEvent  = rt->FramesPerEvent * rt->BlockAlign;
        rt->EventPeriodQpc = g_AoTransportEngine.PeriodQpc;

        LARGE_INTEGER now = KeQueryPerformanceCounter(NULL);
        rt->NextEventQpc = now.QuadPart + rt->EventPeriodQpc;
    }

    // Arm startup for capture side. Render side does not use the startup
    // gate. Threshold is a placeholder that Step 4 will tune.
    if (rt->IsCapture)
    {
        rt->StartupArmed = TRUE;
        rt->StartupThresholdFrames = (rt->SampleRate * 20) / 1000;
        rt->StartupTargetFrames    = (rt->SampleRate * 30) / 1000;
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
    if (!g_AoTransportEngine.Running && g_AoTransportEngine.Timer != NULL)
    {
        // ExSetTimer takes the due time in negative 100ns units (relative).
        // Period is positive 100ns units.
        LARGE_INTEGER dueTime;
        dueTime.QuadPart = -AO_TE_DEFAULT_EVENT_PERIOD_100NS;
        (VOID)ExSetTimer(
            g_AoTransportEngine.Timer,
            dueTime.QuadPart,
            AO_TE_DEFAULT_EVENT_PERIOD_100NS,
            NULL);
        g_AoTransportEngine.Running = TRUE;
        DbgPrint("[AoTransport] timer armed\n");
    }

    KeReleaseSpinLock(&g_AoTransportEngine.Lock, old);

    DbgPrint("[AoTransport] OnRun rt=%p cap=%d spk=%d cable=%d rate=%u fpe=%u\n",
        rt, rt->IsCapture, rt->IsSpeakerSide, rt->IsCable,
        rt->SampleRate, rt->FramesPerEvent);
}

// Thin wrapper kept for public API symmetry — real work is in OnRunEx which
// takes the accessor snapshot.
VOID
AoTransportOnRun(CMiniportWaveRTStream* stream)
{
    // Step 1: OnRun is called from minwavertstream.cpp via OnRunEx directly
    // with the snapshot already filled. The stream-pointer overload exists
    // only because the public header declares it; it is a no-op here so the
    // caller contract stays header-only.
    UNREFERENCED_PARAMETER(stream);
}

//=============================================================================
// AoTransportOnPause — stream paused. Flip Active flag off, keep link.
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

VOID
AoTransportOnPause(CMiniportWaveRTStream* stream)
{
    UNREFERENCED_PARAMETER(stream);
}

//=============================================================================
// AoTransportOnStop — stream fully stopped. Detach from active list, clear
// Active flag. Does NOT free the AO_STREAM_RT — that is the stream
// destructor's responsibility via AoTransportFreeStreamRt.
//=============================================================================
extern "C" VOID
AoTransportOnStopEx(AO_STREAM_RT* rt)
{
    if (rt == NULL)
    {
        return;
    }

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

    // If this was the last active stream, we could cancel the timer here to
    // save power. Step 1 keeps the timer running once armed; Step 2 or later
    // can add graceful cancel.

    KeReleaseSpinLock(&g_AoTransportEngine.Lock, old);

    DbgPrint("[AoTransport] OnStop rt=%p\n", rt);
}

VOID
AoTransportOnStop(CMiniportWaveRTStream* stream)
{
    UNREFERENCED_PARAMETER(stream);
}

//=============================================================================
// AoTransportTimerCallback — shared timer fires here at ~20 ms intervals.
//
// Step 1 policy: iterate the active list and log the fact that the callback
// ran. Absolutely no data movement. This validates that:
//   - ExAllocateTimer/ExSetTimer succeeded
//   - the DPC path is reachable
//   - spinlock discipline around the list is correct
//
// Step 3 replaces the body of the loop with render event dispatch, Step 4
// adds capture dispatch.
//=============================================================================
static VOID
AoTransportTimerCallback(
    _In_ PEX_TIMER Timer,
    _In_opt_ PVOID Context)
{
    UNREFERENCED_PARAMETER(Timer);
    UNREFERENCED_PARAMETER(Context);

    // Snapshot-and-drop pattern: take the lock, copy due-stream pointers to
    // a small stack array, release the lock, then run events. Step 1 has no
    // events to run, so we just count.
    //
    // ActiveStreams should be tiny (typically ≤ 4: Cable A/B render + capture
    // for one client each). A fixed local array is safe.
    AO_STREAM_RT* due[16];
    ULONG         dueCount = 0;

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

        if (now.QuadPart < rt->NextEventQpc)
        {
            continue;
        }

        due[dueCount++] = rt;
        rt->NextEventQpc += rt->EventPeriodQpc;
    }

    KeReleaseSpinLock(&g_AoTransportEngine.Lock, old);

    // Step 1: no event dispatch. Step 3/4 replaces this loop body.
    //
    // We intentionally suppress per-tick DbgPrint here so the callback is
    // cheap enough to leave armed during full live-call tests. Uncomment
    // during skeleton validation only.
    //
    // for (ULONG i = 0; i < dueCount; i++)
    // {
    //     DbgPrint("[AoTransport] tick due rt=%p\n", due[i]);
    // }
    UNREFERENCED_PARAMETER(due);
    UNREFERENCED_PARAMETER(dueCount);
}
