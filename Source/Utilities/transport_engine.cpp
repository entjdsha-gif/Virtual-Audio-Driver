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

// --- Phase 6 Option Y cable advance constants ---
// Defined up here (not next to the Y1B helper body below) because
// AoTransportAllocStreamRt seeds rt->FadeSampleCounter with
// AO_CABLE_FADE_PREFIX_LEN in Y1C and needs the symbol visible.
#define AO_CABLE_MIN_FRAMES_GATE      8U     // VB: cmp ebx,8; jl end
#define AO_CABLE_REBASE_SHIFT         7U     // VB: shl r8d,7 (127-frame rebase)
#define AO_CABLE_OVERRUN_DIVISOR      2U     // VB: sampleRate/2 (0.5s stall reject)
#define AO_CABLE_FADE_PREFIX_LEN      96     // FadeSampleCounter starts at -96
#define AO_CABLE_FADE_TABLE_SIZE      96     // clamp index 0..95 (VB cmovg r11d=0x5F)

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

    // Phase 6 Y1C: explicit fade counter init. RtlZeroMemory already
    // leaves this at 0, but the helper interprets 0 as "saturated unity"
    // (idx = 0 + 96 clamped to 95 = full gain). Seeding to -AO_CABLE_FADE_
    // PREFIX_LEN so the first audible packet boundary (Y2) starts at the
    // silence prefix and ramps up cleanly. Harmless while the helper is
    // shadow-only, makes the intent explicit.
    rt->FadeSampleCounter = -(LONG)AO_CABLE_FADE_PREFIX_LEN;

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

    // Phase 6 Y1C: reset Y cable runtime fields on STOP. Matches the VB
    // 669c caller-side pattern — clears DMA cursor, monotonic counters,
    // anchor QPC, published frames, and fade state, while preserving
    // notify fields (NotifyBoundaryBytes/NotifyArmed/NotifyFired) across
    // STOP/resume cycles per VB parity (verified in §9.6).
    //
    // Only cable streams have Y runtime fields populated — non-cable
    // Step 1 streams use a disjoint field set, so the reset is harmless
    // for them too, but gated on IsCable to keep intent clear.
    if (rt->IsCable)
    {
        AoCableResetRuntimeFields(rt);
    }

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

    // Phase 6 Y1C shadow hook-up: invoke the canonical helper in
    // shadow mode so the shared timer path is a visible call source
    // for the Y1C gate. The helper updates only shadow state; legacy
    // render transport (ReadBytes via UpdatePosition) remains the
    // audible owner until Y2 retires it.
    if (rt != NULL && rt->IsCable)
    {
        AoCableAdvanceByQpc(rt, (ULONGLONG)qpc, AO_ADVANCE_TIMER_RENDER, 0);
    }
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
    // Phase 6 "Option Z" revert: capture transport runs from
    // CMiniportWaveRTStream::UpdatePosition's legacy WriteBytes path.
    // See AoRunRenderEvent above for rationale — Option Y will replace
    // both runners with a VB-equivalent implementation that couples
    // update chain and transport in the engine itself.
    //
    // Prior body (DmaProducedMono-anchored capture runner with startup
    // state machine and underrun counter) preserved in git history at
    // commit a8ac0fb.

    // Phase 6 Y1C shadow hook-up: invoke the canonical helper in
    // shadow mode so the shared timer capture path is a visible call
    // source for the Y1C gate. Shadow bookkeeping only; legacy capture
    // transport (WriteBytes via UpdatePosition) remains the audible
    // owner until Y3 retires it.
    if (rt != NULL && rt->IsCable)
    {
        AoCableAdvanceByQpc(rt, (ULONGLONG)qpc, AO_ADVANCE_TIMER_CAPTURE, 0);
    }
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

//=============================================================================
// Phase 6 Option Y — Y1B canonical cable advance helper (shadow mode)
//
// These functions implement the VB-equivalent cable transport body. Y1B
// runs the helper in shadow mode: advance math, gate/rebase/overrun
// reject, mirrored monotonic increment, shadow cursor update, and
// diagnostic counter bumps are all performed. But:
//
//   - the helper MUST NOT write FRAME_PIPE (no FramePipeWriteFromDma /
//     FramePipeReadToDma calls in Y1B)
//   - the helper MUST NOT update rt->DmaProducedMono (that is the legacy
//     authoritative produced-byte count published by the stream side in
//     CMiniportWaveRTStream::UpdatePosition, and is still the audible
//     owner in Y1)
//   - the helper MUST NOT replace GetPosition return values or
//     ReadBytes/WriteBytes inputs (Y1C wires call sites to invoke the
//     helper in shadow, not to consume its results)
//
// All shadow state lives in the new Y1A-added fields: AnchorQpc100ns,
// PublishedFramesSinceAnchor, DmaCursorFrames{,Prev}, MonoFramesLow/
// Mirror, LastAdvanceDelta, StatOverrunCounter, FadeSampleCounter.
// Those fields start zero (ExAllocatePool2 + RtlZeroMemory) and the
// helper is the sole writer.
//
// Y2 (render) and Y3 (capture) retire legacy audible ownership and
// promote the helper's state to authoritative.
//
// Source of truth: results/phase6_vb_verification.md §2, §7, §9.5
// Work order:     docs/PHASE6_Y_IMPLEMENTATION_WORK_ORDER.md §Y1B
//=============================================================================

// --- Fade-in envelope (VB +0x12a60 direct copy, 96 entries × int32) ---
// Advance constants (AO_CABLE_MIN_FRAMES_GATE etc.) are defined near the
// top of this file next to the Step 1 AO_TE_* constants, so Y1C's
// AoTransportAllocStreamRt can reference AO_CABLE_FADE_PREFIX_LEN.
//
// Indexed by (FadeSampleCounter + 96) clamped to [0, 95]. Values are
// 7-bit fixed-point gains: `sample = (sample * table[idx]) >> 7`.
// Index 0..51 is the silence prefix (52 zeros); index 52..95 is the
// ramp from 0 to 128 (unity gain in 7-bit fixed point). Applied per
// sample at packet boundaries to suppress discontinuity clicks.
//
// Verified byte-for-byte against WinDbg `dps vbaudio_cableb64_win10+
// 0x12a60 L80` captured 2026-04-16. Do not alter values without
// re-verification — VB's audibly-clean output depends on them.
static const LONG g_aoFadeEnvelope[AO_CABLE_FADE_TABLE_SIZE] = {
    // 0..15   silence prefix
    0, 0, 0, 0,  0, 0, 0, 0,
    0, 0, 0, 0,  0, 0, 0, 0,
    // 16..31  silence prefix
    0, 0, 0, 0,  0, 0, 0, 0,
    0, 0, 0, 0,  0, 0, 0, 0,
    // 32..47  silence prefix
    0, 0, 0, 0,  0, 0, 0, 0,
    0, 0, 0, 0,  0, 0, 0, 0,
    // 48..51  silence prefix tail
    0, 0, 0, 0,
    // 52..55
    0, 1, 1, 1,
    // 56..59
    1, 1, 1, 2,
    // 60..63
    2, 2, 2, 3,
    // 64..67
    3, 4, 4, 5,
    // 68..71
    5, 6, 7, 8,
    // 72..75
    9, 10, 11, 12,
    // 76..79
    14, 16, 18, 20,
    // 80..83
    22, 25, 28, 32,
    // 84..87
    36, 40, 45, 50,
    // 88..91
    57, 64, 71, 80,
    // 92..95
    90, 101, 114, 128,
};

//-----------------------------------------------------------------------------
// AoApplyFadeEnvelope — apply the VB 51a8-equivalent per-sample gain.
//
// samples       : in-place int32_t buffer (channel-interleaved or planar —
//                 the envelope is symmetric w.r.t. channel order since
//                 every sample gets the same indexed coefficient)
// sampleCount   : total int32 samples in the buffer
// perStreamCounter : pointer to AO_STREAM_RT::FadeSampleCounter (-96 at
//                    packet boundary, advancing toward 0 at which point
//                    the gain saturates at 128 = unity)
//
// Not called from anywhere in Y1B; declared here so Y2's render write
// path can pull it in unchanged. Safe at any IRQL (no memory access
// beyond the passed buffer + counter).
//-----------------------------------------------------------------------------
extern "C" VOID
AoApplyFadeEnvelope(LONG* samples, ULONG sampleCount, LONG* perStreamCounter)
{
    if (samples == NULL || perStreamCounter == NULL || sampleCount == 0)
    {
        return;
    }

    LONG counter = *perStreamCounter;

    for (ULONG i = 0; i < sampleCount; i++)
    {
        LONG idx = counter + AO_CABLE_FADE_PREFIX_LEN;
        if (idx < 0)
        {
            idx = 0;
        }
        else if (idx >= AO_CABLE_FADE_TABLE_SIZE)
        {
            idx = AO_CABLE_FADE_TABLE_SIZE - 1;   // saturate at unity
        }

        LONG coef = g_aoFadeEnvelope[idx];
        samples[i] = (LONG)(((LONGLONG)samples[i] * coef) >> 7);

        // Advance counter toward 0 (unity). Once saturated, stay there.
        if (counter < 0)
        {
            counter++;
        }
    }

    *perStreamCounter = counter;
}

//-----------------------------------------------------------------------------
// AoResetFadeCounter — set the fade counter to -96 so the next sample is
// the start of the silence prefix. Called at packet boundary transitions
// by the render path.
//-----------------------------------------------------------------------------
extern "C" VOID
AoResetFadeCounter(AO_STREAM_RT* rt)
{
    if (rt == NULL)
    {
        return;
    }
    rt->FadeSampleCounter = -(LONG)AO_CABLE_FADE_PREFIX_LEN;
}

//-----------------------------------------------------------------------------
// AoCableResetRuntimeFields — STOP/destructor reset of the Y cable
// runtime state. Matches the VB 669c caller-side pattern: clears the
// DMA cursor, monotonic counters, anchor QPC, published frames, and
// fade state. Does NOT touch legacy Step 1 fields (DmaProducedMono,
// DmaConsumedMono, NextEventQpc, LateEventCount, etc.) — those stay
// owned by the Step 1 scaffold until Y2/Y3 retire it.
//
// Notification fields (NotifyBoundaryBytes, NotifyArmed, NotifyFired)
// are ALL preserved across STOP. Verified against VB Ghidra decompile
// on 2026-04-16: none of the three FUN_14000669c caller sites (lines
// 4544, 4798, 11464 in vbcable_all_functions.c) touch +0x7C / +0x164 /
// +0x165. VB's sole initialization of these fields is at fresh stream
// register time (lines 11738-11739: `+0x7c = 0` and a 2-byte
// `+0x164 = 0` clearing both NotifyArmed and NotifyFired).
//
// AO's equivalent "fresh stream register time" is the ExAllocatePool2 +
// RtlZeroMemory inside AoTransportAllocStreamRt — that zeroes all
// notify fields once per stream object lifetime, which is where they
// should stay zero until the client arms them. STOP/resume cycles must
// NOT reset them or an event-driven client would silently lose its
// arm state.
//
// Source: results/phase6_vb_verification.md §9.6 (NotifyArmed closure)
//-----------------------------------------------------------------------------
extern "C" VOID
AoCableResetRuntimeFields(AO_STREAM_RT* rt)
{
    if (rt == NULL)
    {
        return;
    }

    rt->AnchorQpc100ns              = 0;
    rt->PublishedFramesSinceAnchor  = 0;
    rt->DmaCursorFrames             = 0;
    rt->DmaCursorFramesPrev         = 0;

    // Volatile mirror pair — use InterlockedExchange64 so a concurrent
    // shadow reader in the advance helper (Y1C will wire one via the
    // timer DPC) cannot observe a torn 64-bit value.
    InterlockedExchange64(&rt->MonoFramesLow,    0);
    InterlockedExchange64(&rt->MonoFramesMirror, 0);

    rt->LastAdvanceDelta   = 0;
    rt->StatOverrunCounter = 0;

    // Notification fields (NotifyBoundaryBytes, NotifyArmed, NotifyFired)
    // are intentionally NOT reset here — see function header comment for
    // the VB parity verification that led to this rule.

    // Fade state — reset so the next audible run starts with a fresh
    // silence prefix. AoResetFadeCounter is intentionally inlined here
    // instead of called so this function is self-contained and safe at
    // any IRQL regardless of the caller's context.
    rt->FadeSampleCounter  = -(LONG)AO_CABLE_FADE_PREFIX_LEN;

    // Debug shadow counters — kept across STOP intentionally so the
    // Y1C gate diagnostic can accumulate across multiple run/stop
    // cycles. They get removed in Y4 anyway.
}

//-----------------------------------------------------------------------------
// Phase 6 Y2-1 — render audible path scaffolding
//
// Both functions below are defined but only invoked when
// rt->RenderAudibleActive is TRUE, which Y2-1 never sets. Y2-2 flips
// the switch and retires legacy ReadBytes ownership for cable render
// streams.
//-----------------------------------------------------------------------------

// AoCableApplyRenderFadeInScratch — opaque-pointer adapter called
// from loopback.cpp's FramePipeWriteFromDmaEx. Wraps AoApplyFadeEnvelope
// with the stream's FadeSampleCounter so loopback.cpp does not need to
// know AO_STREAM_RT layout. Safe with NULL rtOpaque.
extern "C" VOID
AoCableApplyRenderFadeInScratch(
    PVOID   rtOpaque,
    LONG*   scratch,
    ULONG   sampleCount)
{
    if (rtOpaque == NULL)
    {
        return;
    }
    AO_STREAM_RT* rt = (AO_STREAM_RT*)rtOpaque;
    AoApplyFadeEnvelope(scratch, sampleCount, &rt->FadeSampleCounter);
}

// AoCableWriteRenderFromDma — DMA -> scratch -> envelope -> FRAME_PIPE
// transfer for cable render streams. Called from the render branch of
// AoCableAdvanceByQpc when rt->RenderAudibleActive is TRUE.
//
// advanceFrames is the helper-authoritative per-tick frame count
// (already subject to the 8-frame gate and 127-rebase + 0.5s overrun
// reject in the caller). This function:
//
//   1. Converts advanceFrames to bytes via rt->BlockAlign
//   2. Computes the DMA start offset from rt->DmaProducedMono
//      (published by legacy UpdatePosition tail — this is the current
//      monotonic "bytes ever written by the WaveRT client")
//   3. Loops over the DMA circular buffer wrap boundary, calling
//      FramePipeWriteFromDmaEx for each contiguous run with rtOpaque
//      set so the fade envelope is applied on scratch during the
//      normalize-to-pipe step
//
// Cursor semantics for Y2-1 (switch off): DmaProducedMono still comes
// from legacy UpdatePosition, so advanceBytes computed from
// advanceFrames × BlockAlign must equal the legacy ByteDisplacement for
// this call to be cursor-consistent. Y2-2 validates this invariant at
// the switchover and, if necessary, re-homes DmaProducedMono publish
// into the helper.
//
// DMA start offset math: the legacy path computes
// `bufferOffset = LinearPosition_old % DmaBufferSize` where
// LinearPosition_old is the value BEFORE the legacy UpdatePosition
// advance. The legacy publish puts LinearPosition_new into
// DmaProducedMono after the advance, so we recover the old offset via
// `(DmaProducedMono - advanceBytes) % DmaBufferSize`. This is correct
// as long as advanceBytes matches the legacy displacement.
extern "C" VOID
AoCableWriteRenderFromDma(AO_STREAM_RT* rt, ULONG advanceFrames)
{
    if (rt == NULL || advanceFrames == 0 || rt->BlockAlign == 0 ||
        rt->DmaBuffer == NULL || rt->DmaBufferSize == 0 ||
        rt->Pipe == NULL)
    {
        return;
    }

    ULONGLONG advanceBytes = (ULONGLONG)advanceFrames * rt->BlockAlign;

    // Defensive: never try to read more than one DMA buffer worth of
    // bytes in a single call. If advanceBytes exceeds the buffer size
    // the canonical helper's overrun reject should have caught it, but
    // we still guard here so a runaway advance cannot wrap past itself.
    if (advanceBytes > rt->DmaBufferSize)
    {
        advanceBytes = rt->DmaBufferSize;
    }

    // Start offset from the legacy-published produced byte count.
    // DmaProducedMono is the NEW (post-advance) LinearPosition value,
    // so we back off by advanceBytes to find where this tick's data
    // begins in the circular DMA buffer.
    ULONGLONG producedNew = (ULONGLONG)rt->DmaProducedMono;
    ULONGLONG producedOld = producedNew - advanceBytes;
    ULONG     startOffset = (ULONG)(producedOld % (ULONGLONG)rt->DmaBufferSize);

    ULONG bytesRemaining = (ULONG)advanceBytes;
    ULONG bufferOffset   = startOffset;

    while (bytesRemaining > 0)
    {
        ULONG runWrite = bytesRemaining;
        ULONG spaceToEnd = rt->DmaBufferSize - bufferOffset;
        if (runWrite > spaceToEnd)
        {
            runWrite = spaceToEnd;
        }

        (VOID)FramePipeWriteFromDmaEx(
            rt->Pipe,
            rt->DmaBuffer + bufferOffset,
            runWrite,
            (PVOID)rt);

        bufferOffset  = (bufferOffset + runWrite) % rt->DmaBufferSize;
        bytesRemaining -= runWrite;
    }
}

//-----------------------------------------------------------------------------
// AoCableAdvanceByQpc — Y1B shadow body.
//
// Direct translation of VB FUN_140006320 per results/phase6_vb_verification.md
// §2. Runs at any IRQL ≤ DISPATCH_LEVEL. Not called from anywhere in
// Y1B — Y1C wires GetPosition / GetPositions / shared timer DPC.
//
// SHADOW RULE: this function writes ONLY the Y-specific shadow fields
// listed in the file header comment above. It never writes
// DmaProducedMono, DmaConsumedMono, NextEventQpc, or any legacy
// authoritative state. It never calls into FRAME_PIPE.
//
// Race discipline in Y1B: single-threaded shadow updates are good
// enough (double counting would only affect DbgShadow* diagnostics).
// Y2 will wrap real cursor advancement in a per-stream spinlock when
// audible ownership arrives. For now we rely on Interlocked* on the
// volatile counters and tolerate minor races on ULONGLONG cursors.
//-----------------------------------------------------------------------------
extern "C" VOID
AoCableAdvanceByQpc(
    AO_STREAM_RT*     rt,
    ULONGLONG         nowQpcRaw,
    AO_ADVANCE_REASON reason,
    ULONG             flags)
{
    UNREFERENCED_PARAMETER(flags);

    if (rt == NULL || rt->SampleRate == 0)
    {
        return;
    }

    // Unconditional "helper was reached" counter — Y1C gate uses the
    // per-reason counters below for a finer breakdown, this one is the
    // aggregate.
    InterlockedIncrement(&rt->DbgShadowAdvanceHits);

    switch (reason)
    {
    case AO_ADVANCE_QUERY:
        InterlockedIncrement(&rt->DbgShadowQueryHits);
        break;
    case AO_ADVANCE_TIMER_RENDER:
    case AO_ADVANCE_TIMER_CAPTURE:
        InterlockedIncrement(&rt->DbgShadowTimerHits);
        break;
    case AO_ADVANCE_PACKET:
    default:
        break;
    }

    // --- QPC raw -> 100ns conversion ---
    // VB 6320 does ((arg_r8 * 10M) + (arg_rdx * 10M)) / freq. The
    // caller passes a single monotonic QPC counter so we collapse to
    // one 10M-scale conversion. Use the cached QPC frequency captured
    // at engine init.
    if (g_AoTeQpcFrequency <= 0)
    {
        return;
    }

    ULONGLONG nowQpc100ns =
        (nowQpcRaw * 10000000ULL) / (ULONGLONG)g_AoTeQpcFrequency;

    // --- Elapsed frames since anchor ---
    // First invocation (anchor == 0) seeds the anchor and bails out
    // without recording a frame delta — the next call will establish
    // the real baseline. This matches VB's startup gate behavior in
    // FUN_1400068ac (+0x190 == 0 branch).
    if (rt->AnchorQpc100ns == 0)
    {
        rt->AnchorQpc100ns             = nowQpc100ns;
        rt->PublishedFramesSinceAnchor = 0;
        return;
    }

    LONGLONG  qpc100nsDelta    = (LONGLONG)(nowQpc100ns - rt->AnchorQpc100ns);
    if (qpc100nsDelta <= 0)
    {
        // Clock went backwards or stayed put — nothing to advance.
        return;
    }

    LONGLONG  elapsedFrames64  =
        (qpc100nsDelta * (LONGLONG)rt->SampleRate) / 10000000LL;

    if (elapsedFrames64 < 0)
    {
        return;
    }

    LONG advance = (LONG)(elapsedFrames64 - (LONGLONG)rt->PublishedFramesSinceAnchor);

    // --- 8-frame minimum gate (VB: cmp ebx,8; jl end) ---
    if (advance < (LONG)AO_CABLE_MIN_FRAMES_GATE)
    {
        return;
    }

    // --- 127-frame rebase (VB: elapsed >= sampleRate << 7) ---
    // Reset the anchor once the accumulated frame count exceeds
    // ~2.7 seconds (at 48 kHz) to prevent the 100ns arithmetic from
    // losing precision over long-running streams.
    if (elapsedFrames64 >= ((LONGLONG)rt->SampleRate << AO_CABLE_REBASE_SHIFT))
    {
        rt->PublishedFramesSinceAnchor = 0;
        rt->AnchorQpc100ns             = nowQpc100ns;
    }

    // --- 0.5s overrun reject (VB: advance > sampleRate/2) ---
    // If this single advance would move more than half a second of
    // frames, treat it as a stall recovery and reject the move. Bump
    // the overrun stat so Y1C shadow diagnostics show how often the
    // path would have bailed if audible ownership were active.
    if ((ULONG)advance > (rt->SampleRate / AO_CABLE_OVERRUN_DIVISOR))
    {
        rt->StatOverrunCounter++;
        return;
    }

    // --- Shadow cursor update ---
    // VB 6320 advances +0xD0 by `advance` frames modulo buffer size.
    // In Y1B the wrap uses RingSizeFrames which is still zero (Y1A
    // didn't populate it — Y1C seeds it from the stream format). Fall
    // back to unwrapped advancement when ring size is unknown so the
    // shadow cursor still grows monotonically for observation.
    rt->DmaCursorFramesPrev = rt->DmaCursorFrames;
    if (rt->RingSizeFrames > 0)
    {
        rt->DmaCursorFrames =
            (rt->DmaCursorFrames + (ULONGLONG)advance) % rt->RingSizeFrames;
    }
    else
    {
        rt->DmaCursorFrames += (ULONGLONG)advance;
    }

    // --- Y2-1.5 render byte diff diagnostic (cable render only) ---
    // Bump helper's cumulative render-byte total and update the max
    // |helper - legacy| diff observed so far. Legacy side is bumped by
    // CMiniportWaveRTStream::UpdatePosition after its block-align step.
    // See AO_STREAM_RT comments for interpretation rules. The helper
    // runs for both query and timer reasons, so cumulative helper
    // counter grows faster than legacy cumulative whenever a timer
    // tick fires without a matching UpdatePosition — that is normal
    // and reflected in the diff reading. The meaningful signal is
    // whether the two cumulatives converge over multi-second windows,
    // not whether they match on any single advance call.
    if (rt->IsCable && !rt->IsCapture && rt->BlockAlign > 0)
    {
        LONGLONG advanceBytes = (LONGLONG)advance * (LONGLONG)rt->BlockAlign;
        LONGLONG newHelper    =
            InterlockedAdd64(&rt->DbgY2HelperRenderBytes, advanceBytes);

        // Volatile read of the legacy counter — single 64-bit load on
        // x64 is atomic enough for a diagnostic snapshot. We do not
        // need a strict happens-before edge here; worst case the diff
        // reads a slightly stale legacy total and the next helper
        // entry records a more up-to-date one.
        LONGLONG legacy  = rt->DbgY2LegacyRenderBytes;
        LONGLONG diff    = newHelper - legacy;
        LONGLONG absDiff = (diff < 0) ? -diff : diff;

        // Max update is best-effort — a racing writer could overwrite
        // with a smaller value, but over time the true max will land.
        if (absDiff > rt->DbgY2RenderByteDiffMax)
        {
            rt->DbgY2RenderByteDiffMax = absDiff;
        }
        if (diff != 0)
        {
            InterlockedIncrement(&rt->DbgY2RenderMismatchHits);
        }
    }

    // --- Y2-1 render audible path (switch off by default) ---
    // When RenderAudibleActive is TRUE, the helper owns the DMA ->
    // FRAME_PIPE transfer for cable render streams. In Y2-1 this flag
    // is never set, so the call is dead code for diagnostic build
    // purposes — compiles, links, passes kernel verifier, but never
    // executes. Y2-2 is the commit that flips the switch and retires
    // legacy ReadBytes ownership. Gated on IsCable && !IsCapture so
    // capture streams (which enter the same helper) skip it.
    if (rt->RenderAudibleActive && rt->IsCable && !rt->IsCapture)
    {
        AoCableWriteRenderFromDma(rt, (ULONG)advance);
    }

    // --- Packet notification check (shadow only) ---
    // VB fires [+0x8188] when the cursor crosses +0x7C. In Y1B we do
    // NOT dispatch the callback — shared-mode clients never arm this
    // path (NotifyArmed stays 0), and event-driven clients are still
    // served by the legacy PortCls contract. We keep the predicate
    // here so Y1C can observe boundary crossings in diagnostic logs
    // without calling into portcls.
    if (rt->NotifyArmed && !rt->NotifyFired && rt->RingSizeFrames > 0)
    {
        if ((rt->DmaCursorFrames % rt->RingSizeFrames) == rt->NotifyBoundaryBytes)
        {
            rt->NotifyFired = 1;
            // Intentionally no call-through in Y1B. Y3 will decide
            // whether event-driven clients need direct dispatch here
            // or whether the portcls contract handles it upstream.
        }
    }

    // --- Monotonic counter mirror (VB +0xE0/+0xE8) ---
    // Both counters receive the same delta. Volatile + interlocked
    // add so a concurrent shadow reader cannot observe a torn 64-bit
    // mid-write value — matches VB's single-writer-under-lock pattern
    // without needing the per-stream spinlock Y2 will introduce.
    InterlockedAdd64(&rt->MonoFramesLow,    (LONGLONG)advance);
    InterlockedAdd64(&rt->MonoFramesMirror, (LONGLONG)advance);

    rt->LastAdvanceDelta           = advance;
    rt->PublishedFramesSinceAnchor = (ULONG)elapsedFrames64;
}
