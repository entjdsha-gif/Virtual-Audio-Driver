#include "definitions.h"
#include <limits.h>
#include <ks.h>
#include "endpoints.h"
#include "minwavert.h"
#include "minwavertstream.h"
#include "loopback.h"
#include "transport_engine.h"
#define MINWAVERTSTREAM_POOLTAG 'SRWM'
// ~4ms fade-in to prevent pop/click, scaled to sample rate
#define FADE_MS 4
#define FADE_SAMPLES_FOR_RATE(rate) ((ULONG)((ULONGLONG)(rate) * FADE_MS / 1000))

#pragma warning (disable : 4127)

//=============================================================================
// CMiniportWaveRTStream
//=============================================================================

//=============================================================================
#pragma code_seg("PAGE")
CMiniportWaveRTStream::~CMiniportWaveRTStream
( 
    void 
)
/*++

Routine Description:

  Destructor for wavertstream 

Arguments:

Return Value:

  NT status code.

--*/
{
    PAGED_CODE();

    // Phase 4 (2026-04-13): stream-lifetime teardown order.
    //
    // Correct order per Codex Phase 4 proposal:
    //   1. stream-close notify (does not use cable pipe state)
    //   2. cancel / delete notification timer
    //   3. KeFlushQueuedDpcs so no in-flight DPC can touch the pipe
    //   4. resolve cable pipe via still-alive m_pMiniport->m_DeviceType
    //      and call FramePipeUnregisterFormat() exactly here
    //   5. release m_pMiniport (now that the unregister is done)
    //   6. free remaining per-stream pool allocations
    //
    // STOP no longer owns cable pipe unregister; that moved here.
    // Non-cable streams skip the unregister step but still follow the
    // same teardown order, so cable and non-cable paths stay aligned.

    if (NULL != m_pMiniport && m_bUnregisterStream)
    {
        m_pMiniport->StreamClosed(m_ulPin, this);
        m_bUnregisterStream = FALSE;
    }

    if (m_pNotificationTimer)
    {
        ExDeleteTimer
        (
            m_pNotificationTimer,
            TRUE, // Cancel the timer if it is currently set.
            TRUE, // Wait for the timer to finish expiring and for any callback to a ExTimerCallback routine to finish.
            NULL
        );
    }

    // Since we just cancelled the notification timer, wait for all queued
    // DPCs to complete before we touch any pipe state or free DPC-owned
    // resources. After this point, no timer/DPC callback can still be
    // advancing the cable pipe via UpdatePosition() + the Phase 3 pump.
    KeFlushQueuedDpcs();

    //
    // Phase 6 Step 1: unlink this stream from the shared transport engine
    // and free its runtime object. AoTransportOnStopEx is idempotent (safe
    // even if we never called OnRunEx) and AoTransportFreeStreamRt detaches
    // defensively before freeing. KeFlushQueuedDpcs above covers the stream
    // timer path; the engine timer is drained separately in
    // AoTransportEngineCleanup during driver unload, which runs after all
    // stream destructors. Step 3/4 will tighten ordering once the callback
    // actually touches pipe state.
    //
    if (m_pTransportRt)
    {
        AoTransportOnStopEx(m_pTransportRt);
        AoTransportFreeStreamRt(m_pTransportRt);
        m_pTransportRt = NULL;
    }

    // Cable pipe unregister at the true stream-lifetime edge.
    // Moved out of SetState(KSSTATE_STOP) in Phase 4 so state churn no
    // longer collapses the cable data path.
    if (NULL != m_pMiniport)
    {
        PFRAME_PIPE pFP = NULL;
        BOOLEAN isSpeaker = !m_bCapture;
        eDeviceType dt = m_pMiniport->m_DeviceType;
        if (dt == eCableASpeaker || dt == eCableAMic)
        {
            pFP = &g_CableAPipe;
        }
        else if (dt == eCableBSpeaker || dt == eCableBMic)
        {
            pFP = &g_CableBPipe;
        }
        if (pFP)
        {
            FramePipeUnregisterFormat(pFP, isSpeaker);
        }
    }

    if (NULL != m_pMiniport)
    {
        m_pMiniport->Release();
        m_pMiniport = NULL;
    }

    if (m_pDpc)
    {
        ExFreePoolWithTag( m_pDpc, MINWAVERTSTREAM_POOLTAG );
        m_pDpc = NULL;
    }

    if (m_pTimer)
    {
        ExFreePoolWithTag( m_pTimer, MINWAVERTSTREAM_POOLTAG );
        m_pTimer = NULL;
    }

    if (m_pbMuted)
    {
        ExFreePoolWithTag( m_pbMuted, MINWAVERTSTREAM_POOLTAG );
        m_pbMuted = NULL;
    }

    if (m_plVolumeLevel)
    {
        ExFreePoolWithTag( m_plVolumeLevel, MINWAVERTSTREAM_POOLTAG );
        m_plVolumeLevel = NULL;
    }

    if (m_plPeakMeter)
    {
        ExFreePoolWithTag( m_plPeakMeter, MINWAVERTSTREAM_POOLTAG );
        m_plPeakMeter = NULL;
    }

    if (m_pWfExt)
    {
        ExFreePoolWithTag( m_pWfExt, MINWAVERTSTREAM_POOLTAG );
        m_pWfExt = NULL;
    }

    DPF_ENTER(("[CMiniportWaveRTStream::~CMiniportWaveRTStream]"));
} // ~CMiniportWaveRTStream

//=============================================================================
#pragma code_seg("PAGE")

#if !defined(CABLE_A) && !defined(CABLE_B)
NTSTATUS CMiniportWaveRTStream::ReadRegistrySettings()
{
    PAGED_CODE();

    NTSTATUS                    ntStatus;
    PDRIVER_OBJECT              DriverObject;
    HANDLE                      DriverKey;
    RTL_QUERY_REGISTRY_TABLE    paramTable[] = {
        // QueryRoutine     Flags                                               Name                            EntryContext                            DefaultType                                                     DefaultData                                 DefaultLength
        { NULL,   RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_TYPECHECK, L"HostCaptureToneFrequency",        &m_ulHostCaptureToneFrequency,          (REG_DWORD << RTL_QUERY_REGISTRY_TYPECHECK_SHIFT) | REG_DWORD,  &m_ulHostCaptureToneFrequency,              sizeof(DWORD) },
        { NULL,   RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_TYPECHECK, L"HostCaptureToneAmplitude",        &m_dwHostCaptureToneAmplitude,          (REG_DWORD << RTL_QUERY_REGISTRY_TYPECHECK_SHIFT) | REG_DWORD,  &m_dwHostCaptureToneAmplitude,              sizeof(DWORD) },
        { NULL,   RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_TYPECHECK, L"HostCaptureToneDCOffset",         &m_dwHostCaptureToneDCOffset,           (REG_DWORD << RTL_QUERY_REGISTRY_TYPECHECK_SHIFT) | REG_DWORD,  &m_dwHostCaptureToneDCOffset,               sizeof(DWORD) },
        { NULL,   RTL_QUERY_REGISTRY_DIRECT | RTL_QUERY_REGISTRY_TYPECHECK, L"HostCaptureToneInitialPhase",     &m_dwHostCaptureToneInitialPhase,       (REG_DWORD << RTL_QUERY_REGISTRY_TYPECHECK_SHIFT) | REG_DWORD,  &m_dwHostCaptureToneInitialPhase,           sizeof(DWORD) },
        { NULL,   0,                                                        NULL,                               NULL,                                   0,                                                              NULL,                                       0 }
    };

    DriverObject = WdfDriverWdmGetDriverObject(WdfGetDriver());
    DriverKey = NULL;
    ntStatus = IoOpenDriverRegistryKey(DriverObject,
                                 DriverRegKeyParameters,
                                 KEY_READ,
                                 0,
                                 &DriverKey);

    if (!NT_SUCCESS(ntStatus))
    {
        return ntStatus;
    }

    ntStatus = RtlQueryRegistryValues(RTL_REGISTRY_HANDLE,
                                  (PCWSTR) DriverKey,
                                  &paramTable[0],
                                  NULL,
                                  NULL);

    if (!NT_SUCCESS(ntStatus))
    {
        DPF(D_VERBOSE, ("RtlQueryRegistryValues failed, using default values, 0x%x", ntStatus));
        //
        // Don't return error because we will operate with default values.
        //
    }

    if (DriverKey)
    {
        ZwClose(DriverKey);
    }

    return ntStatus;
}
#endif // !CABLE_A && !CABLE_B

NTSTATUS
CMiniportWaveRTStream::Init
( 
    _In_ PCMiniportWaveRT           Miniport_,
    _In_ PPORTWAVERTSTREAM          PortStream_,
    _In_ ULONG                      Pin_,
    _In_ BOOLEAN                    Capture_,
    _In_ PKSDATAFORMAT              DataFormat_,
    _In_ GUID                       SignalProcessingMode
)
/*++

Routine Description:

  Initializes the stream object.

Arguments:

  Miniport_ -

  Pin_ -

  DataFormat -

  SignalProcessingMode - The driver uses the signalProcessingMode to configure
    driver and/or hardware specific signal processing to be applied to this new
    stream.

Return Value:

  NT status code.

--*/
{
    PAGED_CODE();

    PWAVEFORMATEX pWfEx = NULL;
    NTSTATUS ntStatus = STATUS_SUCCESS;

    m_pMiniport = NULL;
    m_ulPin = 0;
    m_bUnregisterStream = FALSE;
    m_bCapture = FALSE;
    m_ulDmaBufferSize = 0;
    m_pDmaBuffer = NULL;
    m_pTransportRt = NULL;
    m_ulNotificationsPerBuffer = 0;
    m_KsState = KSSTATE_STOP;
    m_pTimer = NULL;
    m_pDpc = NULL;
    m_llPacketCounter = 0;
    m_ullPlayPosition = 0;
    m_ullWritePosition = 0;
    m_ullDmaTimeStamp = 0;
    m_hnsElapsedTimeCarryForward = 0;
    m_ullLastDPCTimeStamp = 0;
    m_hnsDPCTimeCarryForward = 0;
    m_ulDmaMovementRate = 0;
    m_byteDisplacementCarryForward = 0;
    m_ulBlockAlignCarryForward = 0;
    m_bLfxEnabled = FALSE;
    m_pbMuted = NULL;
    m_plVolumeLevel = NULL;
    m_plPeakMeter = NULL;
    m_pWfExt = NULL;
    m_ullLinearPosition = 0;
    m_ullPresentationPosition = 0;
    m_ulContentId = 0;
    m_ulCurrentWritePosition = 0;
    m_ulLastOsReadPacket = ULONG_MAX;
    m_ulLastOsWritePacket = ULONG_MAX;
    m_IsCurrentWritePositionUpdated = 0;
    m_SignalProcessingMode = SignalProcessingMode;
    m_bEoSReceived = FALSE;
    m_bLastBufferRendered = FALSE;
    m_ulFadeInRemaining = 0;

    // Phase 1: pump state zeros (matches new fields in minwavertstream.h).
    // No Phase 1 code path writes any non-zero into these; Phase 3 is the
    // first writer. Guarantees the "idle = 0" Exit Criterion.
    m_ullPumpBaselineHns                    = 0;
    m_ulPumpProcessedFrames                 = 0;
    m_bPumpInitialized                      = FALSE;
    m_ulPumpLastBufferOffset                = 0;
    m_ulPumpInvocationCount                 = 0;
    m_ulPumpGatedSkipCount                  = 0;
    m_ulPumpOverJumpCount                   = 0;
    m_ulPumpShadowDivergenceCount           = 0;
    m_ullPumpFramesProcessed                = 0;
    m_ulPumpFeatureFlags                    = 0;
    m_ullPumpShadowWindowPumpFrames         = 0;
    m_ullPumpShadowWindowLegacyBytes        = 0;
    m_ulPumpShadowWindowCallCount           = 0;
    m_ulLastUpdatePositionByteDisplacement  = 0;

#if !defined(CABLE_A) && !defined(CABLE_B)
    m_ulHostCaptureToneFrequency = IsEqualGUID(SignalProcessingMode, AUDIO_SIGNALPROCESSINGMODE_RAW) ? 1000 : 2000;
    m_dwHostCaptureToneAmplitude = 50;
    m_dwHostCaptureToneDCOffset = 0;
    m_dwHostCaptureToneInitialPhase = 0;
#endif

    m_pPortStream = PortStream_;
    InitializeListHead(&m_NotificationList);
    m_ulNotificationIntervalMs = 0;

    // Initialize the spinlock to synchronize position updates
    KeInitializeSpinLock(&m_PositionSpinLock);

    m_pNotificationTimer = ExAllocateTimer(
         TimerNotifyRT,
         this,
         EX_TIMER_HIGH_RESOLUTION
    );
    if (!m_pNotificationTimer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    pWfEx = GetWaveFormatEx(DataFormat_);
    if (NULL == pWfEx) 
    { 
        return STATUS_UNSUCCESSFUL; 
    }

    m_pMiniport = reinterpret_cast<CMiniportWaveRT*>(Miniport_);
    if (m_pMiniport == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }
    m_pMiniport->AddRef();
    if (!NT_SUCCESS(ntStatus))
    {
        return ntStatus;
    }
    m_ulPin = Pin_;
    m_bCapture = Capture_;
    m_ulDmaMovementRate = pWfEx->nAvgBytesPerSec;

    m_pDpc = (PRKDPC)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(KDPC), MINWAVERTSTREAM_POOLTAG);
    if (!m_pDpc)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    m_pWfExt = (PWAVEFORMATEXTENSIBLE)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(WAVEFORMATEX) + pWfEx->cbSize, MINWAVERTSTREAM_POOLTAG);
    if (m_pWfExt == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlCopyMemory(m_pWfExt, pWfEx, sizeof(WAVEFORMATEX) + pWfEx->cbSize);

    m_pbMuted = (PBOOL)ExAllocatePool2(POOL_FLAG_NON_PAGED, m_pWfExt->Format.nChannels * sizeof(BOOL), MINWAVERTSTREAM_POOLTAG);
    if (m_pbMuted == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    m_plVolumeLevel = (PLONG)ExAllocatePool2(POOL_FLAG_NON_PAGED, m_pWfExt->Format.nChannels * sizeof(LONG), MINWAVERTSTREAM_POOLTAG);
    if (m_plVolumeLevel == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    m_plPeakMeter = (PLONG)ExAllocatePool2(POOL_FLAG_NON_PAGED, m_pWfExt->Format.nChannels * sizeof(LONG), MINWAVERTSTREAM_POOLTAG);
    if (m_plPeakMeter == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

#if !defined(CABLE_A) && !defined(CABLE_B)
    if (m_bCapture)
    {
        ReadRegistrySettings();
        DWORD toneFrequency = 0;
        DWORD toneAmplitude = 0;
        DWORD toneDCOffset = 0;
        DWORD toneInitialPhase = 0;

        double toneAmplitudeDouble = 0;
        double toneDCOffsetDouble = 0;
        double toneInitialPhaseDouble = 0;

        toneFrequency = m_ulHostCaptureToneFrequency;
        toneAmplitude = m_dwHostCaptureToneAmplitude;
        toneDCOffset = m_dwHostCaptureToneDCOffset;
        toneInitialPhase = m_dwHostCaptureToneInitialPhase;

        if (labs(toneAmplitude) > 100)
        {
            toneAmplitude = toneAmplitude > 0 ? 100 : -100;
        }

        if (labs(toneDCOffset) > 100)
        {
            toneDCOffset = toneDCOffset > 0 ? 100 : -100;
        }

        DWORD abssum = labs(toneAmplitude) + labs(toneDCOffset);

        if (abssum > 100)
        {
            toneAmplitudeDouble = ((double)toneAmplitude) / abssum;
            toneDCOffsetDouble = ((double)toneDCOffset) / abssum;
        }
        else
        {
            toneAmplitudeDouble = ((double)toneAmplitude) / 100.0;
            toneDCOffsetDouble = ((double)toneDCOffset) / 100.0;
        }

        if (labs(toneInitialPhase) > 31416)
        {
            toneInitialPhase = toneInitialPhase > 0 ? 31416 : -31416;
        }

        toneInitialPhaseDouble = (double)toneInitialPhase / 10000;

        ntStatus = m_ToneGenerator.Init(toneFrequency, toneAmplitudeDouble, toneDCOffsetDouble, toneInitialPhaseDouble, m_pWfExt);

        if (!NT_SUCCESS(ntStatus))
        {
            return ntStatus;
        }
    }
    else if (!g_DoNotCreateDataFiles)
    {
        //
        // Create an output file for the render data.
        //
        DPF(D_TERSE, ("SaveData %p", &m_SaveData));
        ntStatus = m_SaveData.SetDataFormat(DataFormat_);
        if (NT_SUCCESS(ntStatus))
        {
            ntStatus = m_SaveData.Initialize();
        }

        if (!NT_SUCCESS(ntStatus))
        {
            return ntStatus;
        }
    }
#endif // !CABLE_A && !CABLE_B

    //
    // Register this stream.
    //
    ntStatus = m_pMiniport->StreamCreated(m_ulPin, this);
    if (NT_SUCCESS(ntStatus))
    {
        m_bUnregisterStream = TRUE;
    }

    return ntStatus;
} // Init

//=============================================================================
#pragma code_seg("PAGE")
STDMETHODIMP_(NTSTATUS)
CMiniportWaveRTStream::NonDelegatingQueryInterface
( 
    _In_ REFIID  Interface,
    _COM_Outptr_ PVOID * Object 
)
/*++

Routine Description:

  QueryInterface

Arguments:

  Interface - GUID

  Object - interface pointer to be returned

Return Value:

  NT status code.

--*/
{
    PAGED_CODE();

    ASSERT(Object);

    if (IsEqualGUIDAligned(Interface, IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(PMINIPORTWAVERTSTREAM(this)));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveRTStream))
    {
        *Object = PVOID(PMINIPORTWAVERTSTREAM(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveRTStreamNotification))
    {
        *Object = PVOID(PMINIPORTWAVERTSTREAMNOTIFICATION(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveRTInputStream) && (this->m_bCapture))
    {
        // This interface is supported only on capture streams
        *Object = PVOID(PMINIPORTWAVERTINPUTSTREAM(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveRTOutputStream))
    {
        // This interface is supported only on host render streams
        *Object = PVOID(PMINIPORTWAVERTOUTPUTSTREAM(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IDrmAudioStream))
    {
        *Object = (PVOID)(IDrmAudioStream*)this;
    }
    else
    {
        *Object = NULL;
    }

    if (*Object)
    {
        PUNKNOWN(*Object)->AddRef();
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
} // NonDelegatingQueryInterface

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS CMiniportWaveRTStream::AllocateBufferWithNotification
(
    _In_    ULONG               NotificationCount_,
    _In_    ULONG               RequestedSize_,
    _Out_   PMDL                *AudioBufferMdl_,
    _Out_   ULONG               *ActualSize_,
    _Out_   ULONG               *OffsetFromFirstPage_,
    _Out_   MEMORY_CACHING_TYPE *CacheType_
)
{
    PAGED_CODE();

    ULONG ulBufferDurationMs = 0;

    if ( (0 == RequestedSize_) || (RequestedSize_ < m_pWfExt->Format.nBlockAlign) )
    { 
        return STATUS_UNSUCCESSFUL; 
    }
    
    if ((NotificationCount_ == 0) || (RequestedSize_ % NotificationCount_ != 0))
    {
        return STATUS_INVALID_PARAMETER;
    }

    RequestedSize_ -= RequestedSize_ % (m_pWfExt->Format.nBlockAlign);
    
#if !defined(CABLE_A) && !defined(CABLE_B)
    if (!m_bCapture && (!g_DoNotCreateDataFiles))
    {
        NTSTATUS ntStatus;

        // Simple Audio Sample uses following buffer to hold data before writing to a file.
        // Allocating larger buffer will reduce File I/O operations.
        ntStatus = m_SaveData.SetMaxWriteSize(RequestedSize_ * 4);
        if (!NT_SUCCESS(ntStatus))
        {
            return ntStatus;
        }
    }
#endif

    PHYSICAL_ADDRESS highAddress;
    highAddress.HighPart = 0;
    highAddress.LowPart = MAXULONG;

    PMDL pBufferMdl = m_pPortStream->AllocatePagesForMdl (highAddress, RequestedSize_);

    if (NULL == pBufferMdl)
    {
        return STATUS_UNSUCCESSFUL;
    }

    // From MSDN: 
    // "Since the Windows audio stack does not support a mechanism to express memory access 
    //  alignment requirements for buffers, audio drivers must select a caching type for mapped
    //  memory buffers that does not impose platform-specific alignment requirements. In other 
    //  words, the caching type used by the audio driver for mapped memory buffers, must not make 
    //  assumptions about the memory alignment requirements for any specific platform.
    //
    //  This method maps the physical memory pages in the MDL into kernel-mode virtual memory. 
    //  Typically, the miniport driver calls this method if it requires software access to the 
    //  scatter-gather list for an audio buffer. In this case, the storage for the scatter-gather 
    //  list must have been allocated by the IPortWaveRTStream::AllocatePagesForMdl or 
    //  IPortWaveRTStream::AllocateContiguousPagesForMdl method. 
    //
    //  A WaveRT miniport driver should not require software access to the audio buffer itself."
    //   
    m_pDmaBuffer = (BYTE*)m_pPortStream->MapAllocatedPages(pBufferMdl, MmCached);
    m_ulNotificationsPerBuffer = NotificationCount_;
    m_ulDmaBufferSize = RequestedSize_;
    ulBufferDurationMs = (RequestedSize_ * 1000) / m_ulDmaMovementRate;
    m_ulNotificationIntervalMs = ulBufferDurationMs / NotificationCount_;

    *AudioBufferMdl_ = pBufferMdl;
    *ActualSize_ = RequestedSize_;
    *OffsetFromFirstPage_ = 0;
    *CacheType_ = MmCached;

    return STATUS_SUCCESS;
}

//=============================================================================
#pragma code_seg("PAGE")
VOID CMiniportWaveRTStream::FreeBufferWithNotification
(
    _In_        PMDL    Mdl_,
    _In_        ULONG   Size_
)
{
    UNREFERENCED_PARAMETER(Size_);

    PAGED_CODE();

    if (Mdl_ != NULL)
    {
        if (m_pDmaBuffer != NULL)
        {
            m_pPortStream->UnmapAllocatedPages(m_pDmaBuffer, Mdl_);
            m_pDmaBuffer = NULL;
        }
        
        m_pPortStream->FreePagesFromMdl(Mdl_);
    }
    
    m_ulDmaBufferSize = 0;
    m_ulNotificationsPerBuffer = 0;

    return;
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS CMiniportWaveRTStream::RegisterNotificationEvent
(
    _In_ PKEVENT NotificationEvent_
)
{
    UNREFERENCED_PARAMETER(NotificationEvent_);

    PAGED_CODE();

    NotificationListEntry *nleNew = (NotificationListEntry*)ExAllocatePool2( 
        POOL_FLAG_NON_PAGED,
        sizeof(NotificationListEntry),
        MINWAVERTSTREAM_POOLTAG);
    if (NULL == nleNew)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    nleNew->NotificationEvent = NotificationEvent_;

    // Fail if the notification event already exists in our list.
    if (!IsListEmpty(&m_NotificationList))
    {
        PLIST_ENTRY leCurrent = m_NotificationList.Flink;
        while (leCurrent != &m_NotificationList)
        {
            NotificationListEntry* nleCurrent = CONTAINING_RECORD( leCurrent, NotificationListEntry, ListEntry);
            if (nleCurrent->NotificationEvent == NotificationEvent_)
            {
                ExFreePoolWithTag( nleNew, MINWAVERTSTREAM_POOLTAG );
                return STATUS_UNSUCCESSFUL;
            }

            leCurrent = leCurrent->Flink;
        }
    }

    InsertTailList(&m_NotificationList, &(nleNew->ListEntry));
    
    return STATUS_SUCCESS;
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS CMiniportWaveRTStream::UnregisterNotificationEvent
(
    _In_ PKEVENT NotificationEvent_
)
{
    UNREFERENCED_PARAMETER(NotificationEvent_);

    PAGED_CODE();

    if (!IsListEmpty(&m_NotificationList))
    {
        PLIST_ENTRY leCurrent = m_NotificationList.Flink;
        while (leCurrent != &m_NotificationList)
        {
            NotificationListEntry* nleCurrent = CONTAINING_RECORD( leCurrent, NotificationListEntry, ListEntry);
            if (nleCurrent->NotificationEvent == NotificationEvent_)
            {
                RemoveEntryList( leCurrent );
                ExFreePoolWithTag( nleCurrent, MINWAVERTSTREAM_POOLTAG );
                return STATUS_SUCCESS;
            }

            leCurrent = leCurrent->Flink;
        }
    }

    return STATUS_NOT_FOUND;
}


//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS CMiniportWaveRTStream::GetClockRegister
(
    _Out_ PKSRTAUDIO_HWREGISTER Register_
)
{
    UNREFERENCED_PARAMETER(Register_);

    PAGED_CODE();

    return STATUS_NOT_IMPLEMENTED;
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS CMiniportWaveRTStream::GetPositionRegister
(
    _Out_ PKSRTAUDIO_HWREGISTER Register_
)
{
    UNREFERENCED_PARAMETER(Register_);

    PAGED_CODE();

    return STATUS_NOT_IMPLEMENTED;
}

//=============================================================================
#pragma code_seg("PAGE")
VOID CMiniportWaveRTStream::GetHWLatency
(
    _Out_ PKSRTAUDIO_HWLATENCY  Latency_
)
{
    PAGED_CODE();

    ASSERT(Latency_);

    Latency_->ChipsetDelay = 0;
    Latency_->CodecDelay = 0;
    Latency_->FifoSize = 0;
}

//=============================================================================
#pragma code_seg("PAGE")
VOID CMiniportWaveRTStream::FreeAudioBuffer
(
_In_opt_    PMDL        Mdl_,
_In_        ULONG       Size_
)
{
    UNREFERENCED_PARAMETER(Size_);

    PAGED_CODE();

    if (Mdl_ != NULL)
    {
        if (m_pDmaBuffer != NULL)
        {
            m_pPortStream->UnmapAllocatedPages(m_pDmaBuffer, Mdl_);
            m_pDmaBuffer = NULL;
        }

        m_pPortStream->FreePagesFromMdl(Mdl_);
    }

    m_ulDmaBufferSize = 0;
    m_ulNotificationsPerBuffer = 0;
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS CMiniportWaveRTStream::AllocateAudioBuffer
(
_In_    ULONG                   RequestedSize_,
_Out_   PMDL                   *AudioBufferMdl_,
_Out_   ULONG                  *ActualSize_,
_Out_   ULONG                  *OffsetFromFirstPage_,
_Out_   MEMORY_CACHING_TYPE    *CacheType_
)
{
    PAGED_CODE();

    if ((0 == RequestedSize_) || (RequestedSize_ < m_pWfExt->Format.nBlockAlign))
    {
        return STATUS_UNSUCCESSFUL;
    }

    RequestedSize_ -= RequestedSize_ % (m_pWfExt->Format.nBlockAlign);

    PHYSICAL_ADDRESS highAddress;
    highAddress.HighPart = 0;
    highAddress.LowPart = MAXULONG;

    PMDL pBufferMdl = m_pPortStream->AllocatePagesForMdl(highAddress, RequestedSize_);

    if (NULL == pBufferMdl)
    {
        return STATUS_UNSUCCESSFUL;
    }

    // From MSDN: 
    // "Since the Windows audio stack does not support a mechanism to express memory access 
    //  alignment requirements for buffers, audio drivers must select a caching type for mapped
    //  memory buffers that does not impose platform-specific alignment requirements. In other 
    //  words, the caching type used by the audio driver for mapped memory buffers, must not make 
    //  assumptions about the memory alignment requirements for any specific platform.
    //
    //  This method maps the physical memory pages in the MDL into kernel-mode virtual memory. 
    //  Typically, the miniport driver calls this method if it requires software access to the 
    //  scatter-gather list for an audio buffer. In this case, the storage for the scatter-gather 
    //  list must have been allocated by the IPortWaveRTStream::AllocatePagesForMdl or 
    //  IPortWaveRTStream::AllocateContiguousPagesForMdl method. 
    //
    //  A WaveRT miniport driver should not require software access to the audio buffer itself."
    //   
    m_pDmaBuffer = (BYTE*)m_pPortStream->MapAllocatedPages(pBufferMdl, MmCached);

    m_ulDmaBufferSize = RequestedSize_;
    m_ulNotificationsPerBuffer = 0;

    *AudioBufferMdl_ = pBufferMdl;
    *ActualSize_ = RequestedSize_;
    *OffsetFromFirstPage_ = 0;
    *CacheType_ = MmCached;

    return STATUS_SUCCESS;
}

//=============================================================================
#pragma code_seg()
NTSTATUS CMiniportWaveRTStream::GetPosition
(
    _Out_   KSAUDIO_POSITION    *Position_
)
{
    NTSTATUS ntStatus;

    KIRQL oldIrql;
    KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);

    if (m_KsState == KSSTATE_RUN)
    {
        //
        // Get the current time and update position.
        //
        LARGE_INTEGER ilQPC = KeQueryPerformanceCounter(NULL);
        UpdatePosition(ilQPC);

        // Phase 6 Y1C shadow hook-up: invoke the canonical cable advance
        // helper in shadow mode so GetPosition becomes a visible Y1C
        // call source. Cable streams only; non-cable MSVAD streams have
        // no Y runtime fields populated. The helper's return values are
        // intentionally NOT consumed — Position_->PlayOffset / WriteOffset
        // below still reflect the legacy UpdatePosition output (legacy
        // authoritative until Y2/Y3 retire the legacy path).
        if (m_pTransportRt && m_pMiniport &&
            (m_pMiniport->m_DeviceType == eCableASpeaker ||
             m_pMiniport->m_DeviceType == eCableBSpeaker ||
             m_pMiniport->m_DeviceType == eCableAMic      ||
             m_pMiniport->m_DeviceType == eCableBMic))
        {
            AoCableAdvanceByQpc(m_pTransportRt,
                                (ULONGLONG)ilQPC.QuadPart,
                                AO_ADVANCE_QUERY,
                                0);
        }
    }

    Position_->PlayOffset = m_ullPlayPosition;
    Position_->WriteOffset = m_ullWritePosition;

    KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);

    ntStatus = STATUS_SUCCESS;

    return ntStatus;
}

//=============================================================================
// CMiniportWaveRTStream::GetReadPacket
//
//  Returns information about the next packet for the OS to read.
//
// Return value
//
//  Returns STATUS_DEVICE_NOT_READY if no new packets are available.
//
// IRQL - PASSIVE_LEVEL
//
// Remarks
//  Although called at passive level, this routine is non-paged code because
//  it is called in the streaming path where page faults should be avoided.
//
// ISSUE-2014/10/4 Will this work correctly across pause/play?
#pragma code_seg()
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS CMiniportWaveRTStream::GetReadPacket
(
    _Out_ ULONG* PacketNumber,
    _Out_ DWORD* Flags,
    _Out_ ULONG64* PerformanceCounterValue,
    _Out_ BOOL* MoreData
)
{
    ULONG availablePacketNumber;
    ULONG droppedPackets;

    // The call must be from event driven mode
    if (m_ulNotificationsPerBuffer == 0)
    {
        return STATUS_NOT_SUPPORTED;
    }

    *Flags = 0;

    if (m_KsState < KSSTATE_PAUSE)
    {
        return STATUS_INVALID_DEVICE_STATE;
    }

    KIRQL oldIrql;
    KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);

    LONGLONG packetCounter = m_llPacketCounter;
    ULONGLONG ullLinearPosition = m_ullLinearPosition;
    ULONGLONG hnsElapsedTimeCarryForward = m_hnsElapsedTimeCarryForward;
    ULONGLONG ullDmaTimeStamp = m_ullDmaTimeStamp;

    KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);

    // The 0-based number of the last completed packet
    // FUTURE-2014/10/27 Update to allow different numbers of packets per WaveRT buffer
    availablePacketNumber = LODWORD(packetCounter - 1);  // Note this might be ULONG_MAX if called during the first packet

    // If no new packets are available...
    if (availablePacketNumber == m_ulLastOsReadPacket)
    {
        return STATUS_DEVICE_NOT_READY;
    }

    // If more than one packet has transferred since the last packet read by
    // the OS, then those were dropped. That is, a glitch occurred.
    droppedPackets = availablePacketNumber - m_ulLastOsReadPacket - 1;
    if (droppedPackets > 0)
    {
        // Trace a glitch
    }

    // Return next packet number to be read
    *PacketNumber = availablePacketNumber;

    // Compute and return timestamp corresponding to the end of the available packet. In a real hardware
    // driver, the timestamp would be computed in a driver and hardware specific manner. In this sample
    // driver, it is extrapolated from the sample driver's internal simulated position correlation
    // [m_ullLinearPosition @ m_ullDmaTimeStamp] and the sample's internal 64-bit packet counter, subtracting
    // 1 from the packet counter to compute the time at the start of that last completed packet.
    ULONGLONG linearPositionOfAvailablePacket = packetCounter * (m_ulDmaBufferSize / m_ulNotificationsPerBuffer);
    // Need to divide by (1000 * 10000 because m_ulDmaMovementRate is average bytes per sec
    ULONGLONG carryForwardBytes = (hnsElapsedTimeCarryForward * m_ulDmaMovementRate) / 10000000;
    ULONGLONG deltaLinearPosition = ullLinearPosition + carryForwardBytes - linearPositionOfAvailablePacket;
    ULONGLONG deltaTimeInHns = deltaLinearPosition * 10000000 / m_ulDmaMovementRate;
    ULONGLONG timeOfAvailablePacketInHns = ullDmaTimeStamp - deltaTimeInHns;
    ULONGLONG timeOfAvailablePacketInQpc = timeOfAvailablePacketInHns * m_ullPerformanceCounterFrequency.QuadPart / 10000000;

    *PerformanceCounterValue = timeOfAvailablePacketInQpc;

    // No flags are defined yet
    *Flags = 0;

    // This sample does not internally buffer data so there is never more data
    // than revealed by the results from this routine.
    *MoreData = FALSE;

    // Update the last packet read by the OS
    m_ulLastOsReadPacket = availablePacketNumber;

    return STATUS_SUCCESS;
}

#pragma code_seg()
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS CMiniportWaveRTStream::SetWritePacket
(
    _In_ ULONG      PacketNumber,
    _In_ DWORD      Flags,
    _In_ ULONG      EosPacketLength
)
{
    UNREFERENCED_PARAMETER(EosPacketLength);
    NTSTATUS ntStatus;

    // The call must be from event driven mode
    if (m_ulNotificationsPerBuffer == 0)
    {
        return STATUS_NOT_SUPPORTED;
    }

    ULONG oldLastOsWritePacket = m_ulLastOsWritePacket;

    // This function should not be called once EoS has been set.
    if (m_bEoSReceived)
    {
        return STATUS_INVALID_DEVICE_STATE;
    }

    KIRQL oldIrql;
    KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);
    // 1-based count of completed packets, 0-based packet number of current packet
    LONGLONG currentPacket = m_llPacketCounter;
    KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);

    // If not running, the current packet hasn't actually started transfering so OS should be writing
    // to the current packet. If running, then the current packing is already transfering to hardware
    // so the OS should write the packet after the current packet.
    ULONG expectedPacket = LODWORD(currentPacket);
    if (m_KsState == KSSTATE_RUN)
    {
        expectedPacket++;
    }

    // Check if OS PacketNumber is behind or too far ahead of current packet
    LONG deltaFromExpectedPacket = PacketNumber - expectedPacket;   // Modulo arithemetic
    if (deltaFromExpectedPacket < 0)
    {
        return STATUS_DATA_LATE_ERROR;
    }
    else if (deltaFromExpectedPacket > 0)
    {
        return STATUS_DATA_OVERRUN;
    }

    ULONG packetSize = (m_ulDmaBufferSize / m_ulNotificationsPerBuffer);
    ULONG packetIndex = PacketNumber % m_ulNotificationsPerBuffer;
    ULONG ulCurrentWritePosition = packetIndex * packetSize;

    // Check if EOS flag was passed
    if (Flags & KSSTREAM_HEADER_OPTIONSF_ENDOFSTREAM)
    {
        return STATUS_INVALID_PARAMETER;
    }
    else
    {
        m_ulLastOsWritePacket = PacketNumber;

        // This function sets the current write position to the specified byte in the DMA buffer.
        // Will check if the write position is smaller than the DMA buffer size.
        // Will not return an error when the passed in parameter is 0.
        // Will also check if this function was called with the same write position(in event mode only)
        // Underruning will also be checked via timer mechanism
        KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);
        ntStatus = SetCurrentWritePositionInternal(ulCurrentWritePosition);
        KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);
    }

    if (!NT_SUCCESS(ntStatus))
    {
        m_ulLastOsWritePacket = oldLastOsWritePacket;
    }

    return ntStatus;
}

//=============================================================================
#pragma code_seg()
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS CMiniportWaveRTStream::GetOutputStreamPresentationPosition
(
    _Out_ KSAUDIO_PRESENTATION_POSITION *pPresentationPosition
)
{
    ASSERT (pPresentationPosition);
    
    // The call must be from event driven mode
    if(m_ulNotificationsPerBuffer == 0)
    {
        return STATUS_NOT_SUPPORTED;
    }
    
    return GetPresentationPosition(pPresentationPosition);
}

//=============================================================================
#pragma code_seg()
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS CMiniportWaveRTStream::GetPacketCount
(
    _Out_ ULONG *pPacketCount
)
{
    ASSERT(pPacketCount);

    // The call must be from event driven mode
    if(m_ulNotificationsPerBuffer == 0)
    {
        return STATUS_NOT_SUPPORTED;
    }
    
    KIRQL oldIrql;
    KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);

    if (m_KsState == KSSTATE_RUN)
    {
        // Get the current time and update simulated position.
        LARGE_INTEGER ilQPC = KeQueryPerformanceCounter(NULL);
        UpdatePosition(ilQPC);
    }

    *pPacketCount = LODWORD(m_llPacketCounter);
    KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);

    return STATUS_SUCCESS;
}

//linear and presentation positions
#pragma code_seg()
NTSTATUS CMiniportWaveRTStream::GetPositions(
    _Out_opt_  ULONGLONG* _pullLinearBufferPosition,
    _Out_opt_  ULONGLONG* _pullPresentationPosition,
    _Out_opt_  LARGE_INTEGER* _pliQPCTime
)
{
    DPF_ENTER(("[CMiniportWaveRTStream::GetPositions]"));

    NTSTATUS        ntStatus;
    LARGE_INTEGER   ilQPC;
    KIRQL           oldIrql;

    // Update *_pullLinearBufferPosition with the the number of bytes fetched from waveRT ever since a stream got set into RUN
    // state.
    // Once the stream is set to STOP state, any further read on this call would return zero.

    //
    // Get the current time and update position.
    //
    KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);
    ilQPC = KeQueryPerformanceCounter(NULL);
    if (m_KsState == KSSTATE_RUN)
    {
        UpdatePosition(ilQPC);
        // Phase 3: shadow-only pump on the confirmed hot path (GetPositions).
        // Same-call placement keeps m_ulLastUpdatePositionByteDisplacement
        // fresh and stays under m_PositionSpinLock. No transport mutation.
        PumpToCurrentPositionFromQuery(ilQPC);

        // Phase 6 Y1C shadow hook-up: GetPositions is the confirmed hot
        // query path. Invoke the canonical helper in shadow mode so Y1C
        // diagnostic counters register this call source. Return values
        // (*_pullLinearBufferPosition etc.) below still come from the
        // legacy m_ullLinearPosition that UpdatePosition just updated —
        // legacy authoritative until Y2/Y3 retire it.
        if (m_pTransportRt && m_pMiniport &&
            (m_pMiniport->m_DeviceType == eCableASpeaker ||
             m_pMiniport->m_DeviceType == eCableBSpeaker ||
             m_pMiniport->m_DeviceType == eCableAMic      ||
             m_pMiniport->m_DeviceType == eCableBMic))
        {
            AoCableAdvanceByQpc(m_pTransportRt,
                                (ULONGLONG)ilQPC.QuadPart,
                                AO_ADVANCE_QUERY,
                                0);
        }
    }
    if (_pullLinearBufferPosition)
    {
        *_pullLinearBufferPosition = m_ullLinearPosition;
    }
    if (_pullPresentationPosition)
    {
        *_pullPresentationPosition = m_ullPresentationPosition;
    }
    KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);
    if (_pliQPCTime)
    {
        *_pliQPCTime = ilQPC;
    }

    ntStatus = STATUS_SUCCESS;

    return ntStatus;
}

NTSTATUS CMiniportWaveRTStream::GetPresentationPosition(_Out_  KSAUDIO_PRESENTATION_POSITION* _pPresentationPosition)
{
    ASSERT(_pPresentationPosition);
    LARGE_INTEGER timeStamp;

    DPF_ENTER(("[CMiniportWaveRTStream::GetPresentationPosition]"));

    ULONGLONG ullLinearPosition = { 0 };
    ULONGLONG ullPresentationPosition = { 0 };
    NTSTATUS status = STATUS_SUCCESS;

    status = GetPositions(&ullLinearPosition, &ullPresentationPosition, &timeStamp);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    _pPresentationPosition->u64PositionInBlocks = ullPresentationPosition * m_pWfExt->Format.nSamplesPerSec / m_pWfExt->Format.nAvgBytesPerSec;
    _pPresentationPosition->u64QPCPosition = (UINT64)timeStamp.QuadPart;

    return STATUS_SUCCESS;
}

#pragma code_seg()
NTSTATUS CMiniportWaveRTStream::SetCurrentWritePositionInternal(_In_  ULONG _ulCurrentWritePosition)
{
    DPF_ENTER(("[CMiniportWaveRTStream::SetCurrentWritePositionInternal]"));

    ASSERT(m_bEoSReceived == FALSE);

    if (m_bEoSReceived)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    if (_ulCurrentWritePosition > m_ulDmaBufferSize)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    PADAPTERCOMMON pAdapterComm = m_pMiniport->GetAdapterCommObj();

    //Event type: eMINIPORT_SET_WAVERT_BUFFER_WRITE_POSITION
    //Parameter 1: Current linear buffer position    
    //Parameter 2: Previous WaveRtBufferWritePosition that the driver received    
    //Parameter 3: Target WaveRtBufferWritePosition received from portcls
    //Parameter 4: 0
    pAdapterComm->WriteEtwEvent(eMINIPORT_SET_WAVERT_BUFFER_WRITE_POSITION,
        m_ullLinearPosition, // replace with the correct "Current linear buffer position"    
        m_ulCurrentWritePosition,
        _ulCurrentWritePosition, // this is new write position
        0); // always zero

//
// Check for eMINIPORT_GLITCH_REPORT - Same WaveRT buffer write during event driven mode.
//
    if (m_ulNotificationIntervalMs > 0)
    {
        if (m_ulCurrentWritePosition == _ulCurrentWritePosition)
        {
            //Event type: eMINIPORT_GLITCH_REPORT
            //Parameter 1: Current linear buffer position 
            //Parameter 2: Previous WaveRtBufferWritePosition that the driver received 
            //Parameter 3: Major glitch code: 3: Received same WaveRT buffer twice in a row during event driven mode
            //Parameter 4: Minor code for the glitch cause
            pAdapterComm->WriteEtwEvent(eMINIPORT_GLITCH_REPORT,
                m_ullLinearPosition, // replace with the correct "Current linear buffer position"
                m_ulCurrentWritePosition,
                3, // received same WaveRT buffer twice in a row during event driven mode
                _ulCurrentWritePosition);
        }
    }

    m_ulCurrentWritePosition = _ulCurrentWritePosition;
    InterlockedExchange(&m_IsCurrentWritePositionUpdated, 1);

    return STATUS_SUCCESS;
}

//=============================================================================
#pragma code_seg()
NTSTATUS CMiniportWaveRTStream::SetState
(
    _In_    KSSTATE State_
)
{
    NTSTATUS        ntStatus        = STATUS_SUCCESS;
    KIRQL oldIrql;

    // Spew an event for a pin state change request from portcls
    //Event type: eMINIPORT_PIN_STATE
    // Log all state transitions for FRAME_PIPE diagnostics
    if (m_pMiniport)
    {
        DbgPrint("AO_STATE: DevType=%d capture=%d %d->%d\n",
            (int)m_pMiniport->m_DeviceType, (int)m_bCapture,
            (int)m_KsState, (int)State_);
    }

    switch (State_)
    {
        case KSSTATE_STOP:
            if (m_KsState == KSSTATE_ACQUIRE)
            {
                // Acquire stream resources
            }

            // Phase 4 (2026-04-13): STOP no longer owns pipe unregister.
            // FramePipeUnregisterFormat() has moved to destructor teardown.
            // Rationale: STOP was the accidental hard-reset trigger for the
            // cable data path. Unregister+reset belong to true stream
            // lifetime end, not to a state-machine transition.

            KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);
            // Reset DMA
            m_llPacketCounter = 0;
            m_ullPlayPosition = 0;
            m_ullWritePosition = 0;
            m_ullLinearPosition = 0;
            m_ullPresentationPosition = 0;
            
            // Reset OS read/write positions
            m_ulLastOsReadPacket = ULONG_MAX;
            m_ulCurrentWritePosition = 0;
            m_ulLastOsWritePacket = ULONG_MAX;
            m_bEoSReceived = FALSE;
            m_bLastBufferRendered = FALSE;

            // Phase 3: clear shadow pump state so a new STOP->RUN session
            // starts clean. Cable-only; non-cable streams never touched these.
            if (m_pMiniport &&
                (m_pMiniport->m_DeviceType == eCableASpeaker ||
                 m_pMiniport->m_DeviceType == eCableBSpeaker ||
                 m_pMiniport->m_DeviceType == eCableAMic      ||
                 m_pMiniport->m_DeviceType == eCableBMic))
            {
                m_ulPumpFeatureFlags               = 0;
                m_bPumpInitialized                 = FALSE;
                m_ullPumpBaselineHns               = 0;
                m_ulPumpProcessedFrames            = 0;
                m_ulPumpLastBufferOffset           = 0;
                m_ullPumpShadowWindowPumpFrames    = 0;
                m_ullPumpShadowWindowLegacyBytes   = 0;
                m_ulPumpShadowWindowCallCount      = 0;
                m_ulLastUpdatePositionByteDisplacement = 0;
                m_ulPumpGatedSkipCount             = 0;
                m_ulPumpOverJumpCount              = 0;
                m_ulPumpInvocationCount            = 0;
                m_ulPumpShadowDivergenceCount      = 0;
                m_ullPumpFramesProcessed           = 0;

                PFRAME_PIPE pFP = NULL;
                BOOLEAN isSpeaker = !m_bCapture;
                if (m_pMiniport->m_DeviceType == eCableASpeaker ||
                    m_pMiniport->m_DeviceType == eCableAMic)
                {
                    pFP = &g_CableAPipe;
                }
                else
                {
                    pFP = &g_CableBPipe;
                }
                FramePipeResetPumpFeatureFlags(pFP, /* isRenderSide */ isSpeaker);
            }

            KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);

            // Phase 6 Step 1: tell the shared transport engine that this
            // stream is no longer scheduling-active. Does not free the
            // runtime allocation — that remains attached to the stream
            // until the destructor runs. Safe if never registered.
            if (m_pTransportRt)
            {
                AoTransportOnStopEx(m_pTransportRt);
            }

            // Wait until all work items are completed.
#if !defined(CABLE_A) && !defined(CABLE_B)
            if (!m_bCapture && !g_DoNotCreateDataFiles)
            {
                m_SaveData.WaitAllWorkItems();
            }
#endif
            break;

        case KSSTATE_ACQUIRE:
            if (m_KsState == KSSTATE_STOP)
            {
                // Acquire stream resources
            }
            break;
            
        case KSSTATE_PAUSE:

            if (m_KsState > KSSTATE_PAUSE)
            {
                //
                // Run -> Pause
                //

                // Phase 6 Step 1: tell the shared transport engine to stop
                // scheduling events for this stream. Runtime allocation is
                // preserved so a PAUSE -> RUN cycle can reuse it. No data
                // movement is wired onto the engine yet so this is a no-op
                // effect-wise, but keeping the call here means Step 3/4 will
                // not need to revisit the SetState path.
                if (m_pTransportRt)
                {
                    AoTransportOnPauseEx(m_pTransportRt);
                }

                // FRAME_PIPE: do NOT unregister on Pause.
                // Windows audio engine pauses/resumes streams frequently between audio chunks.
                // Unregistering on Pause causes pipe drain -> massive underrun.
                // Unregister now lives in destructor teardown (Phase 4, 2026-04-13).

                // Pause DMA
                if (m_ulNotificationIntervalMs > 0)
                {
                    ExCancelTimer(m_pNotificationTimer, NULL);
                    KeFlushQueuedDpcs();

                    // If pin is transitioning from RUN, save the time since last buffer completion event was sent
                    // so if the pin goes to RUN state again we can send the buffer completion event at correct time.
                    if (m_ullLastDPCTimeStamp > 0)
                    {
                        LARGE_INTEGER qpc;
                        LARGE_INTEGER qpcFrequency;
                        LONGLONG  hnsCurrentTime;

                        qpc = KeQueryPerformanceCounter(&qpcFrequency);

                        // Convert ticks to 100ns units.
                        hnsCurrentTime = KSCONVERT_PERFORMANCE_TIME(m_ullPerformanceCounterFrequency.QuadPart, qpc);
                        m_hnsDPCTimeCarryForward = hnsCurrentTime - m_ullLastDPCTimeStamp + m_hnsDPCTimeCarryForward;
                    }

                    // Phase 4 (2026-04-13): conditional VB-style pipe reset.
                    // After timer cancel + KeFlushQueuedDpcs, no DPC/timer
                    // callback can still be advancing this pipe. If the
                    // opposite direction is not currently active on the
                    // same pipe, it is safe to clear ring/fade state so the
                    // next RUN starts from a clean baseline. If the other
                    // side is still active we keep the conservative rule:
                    // do not touch the pipe (under-reset is safer than
                    // wiping a still-running peer).
                    if (m_pMiniport &&
                        (m_pMiniport->m_DeviceType == eCableASpeaker ||
                         m_pMiniport->m_DeviceType == eCableBSpeaker ||
                         m_pMiniport->m_DeviceType == eCableAMic      ||
                         m_pMiniport->m_DeviceType == eCableBMic))
                    {
                        PFRAME_PIPE pFP = NULL;
                        BOOLEAN isSpeaker = !m_bCapture;
                        if (m_pMiniport->m_DeviceType == eCableASpeaker ||
                            m_pMiniport->m_DeviceType == eCableAMic)
                        {
                            pFP = &g_CableAPipe;
                        }
                        else
                        {
                            pFP = &g_CableBPipe;
                        }
                        if (pFP)
                        {
                            // FramePipeIsDirectionActive folds the
                            // pipe NULL + Initialized + per-direction
                            // Active checks behind the loopback.cpp
                            // ownership boundary (Phase 1 Step 0).
                            BOOLEAN otherSideActive =
                                FramePipeIsDirectionActive(
                                    pFP, /* isSpeaker */ !isSpeaker);
                            if (!otherSideActive)
                            {
                                FramePipeReset(pFP);
                            }
                        }

                        // Phase 4: clear per-run pump working state on PAUSE
                        // so the next RUN starts with a fresh timing baseline.
                        // Monotonic Phase 3 evidence counters are preserved.
                        KIRQL pauseIrql;
                        KeAcquireSpinLock(&m_PositionSpinLock, &pauseIrql);
                        m_ulPumpFeatureFlags               = 0;
                        m_bPumpInitialized                 = FALSE;
                        m_ullPumpBaselineHns               = 0;
                        m_ulPumpProcessedFrames            = 0;
                        m_ulPumpLastBufferOffset           = 0;
                        m_ullPumpShadowWindowPumpFrames    = 0;
                        m_ullPumpShadowWindowLegacyBytes   = 0;
                        m_ulPumpShadowWindowCallCount      = 0;
                        m_ulLastUpdatePositionByteDisplacement = 0;
                        // Preserve: m_ulPumpInvocationCount,
                        //           m_ulPumpShadowDivergenceCount,
                        //           m_ullPumpFramesProcessed.
                        // Preserve per-session GatedSkipCount/OverJumpCount
                        // so churn-era diagnostics survive into RUN.
                        KeReleaseSpinLock(&m_PositionSpinLock, pauseIrql);
                    }
                }
            }
            // This call updates the linear buffer and presentation positions.
            GetPositions(NULL, NULL, NULL);
            break;

        case KSSTATE_RUN:
            // FRAME_PIPE: register stream format on RUN
            if (m_pMiniport && m_pWfExt)
            {
                PFRAME_PIPE pFP = NULL;
                BOOLEAN fpIsSpeaker = !m_bCapture;
                if (m_bCapture)
                {
                    if (m_pMiniport->m_DeviceType == eCableAMic)
                        pFP = &g_CableAPipe;
                    else if (m_pMiniport->m_DeviceType == eCableBMic)
                        pFP = &g_CableBPipe;
                }
                else
                {
                    if (m_pMiniport->m_DeviceType == eCableASpeaker)
                        pFP = &g_CableAPipe;
                    else if (m_pMiniport->m_DeviceType == eCableBSpeaker)
                        pFP = &g_CableBPipe;
                }
                if (pFP)
                {
                    BOOLEAN isFloat = (BOOLEAN)IsEqualGUIDAligned(
                        m_pWfExt->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
                    FramePipeRegisterFormat(pFP, fpIsSpeaker,
                        m_pWfExt->Format.nSamplesPerSec,
                        m_pWfExt->Format.wBitsPerSample,
                        m_pWfExt->Format.nChannels,
                        m_pWfExt->Format.nBlockAlign,
                        isFloat);

                    //
                    // Phase 6 Step 1: register this stream with the shared
                    // transport engine. Runtime allocation is lazy — we do
                    // it here on first RUN after construction. Subsequent
                    // PAUSE -> RUN cycles reuse m_pTransportRt.
                    //
                    // The engine callback is currently a no-op, so this
                    // registration has no observable effect on transport.
                    // It exists to validate the skeleton — if something
                    // goes wrong (BSOD, lock order violation, leak) we
                    // want it to surface here before Step 3/4 wires data
                    // movement onto the engine.
                    //
                    if (m_pTransportRt == NULL)
                    {
                        m_pTransportRt = AoTransportAllocStreamRt(this);
                    }
                    if (m_pTransportRt)
                    {
                        AO_STREAM_SNAPSHOT snap;
                        RtlZeroMemory(&snap, sizeof(snap));
                        snap.Rt            = m_pTransportRt;
                        snap.IsCapture     = m_bCapture;
                        snap.IsCable       = TRUE;
                        snap.IsSpeakerSide = fpIsSpeaker;
                        snap.SampleRate    = m_pWfExt->Format.nSamplesPerSec;
                        snap.Channels      = m_pWfExt->Format.nChannels;
                        snap.BlockAlign    = m_pWfExt->Format.nBlockAlign;
                        // Phase 6 Step 3: engine needs pipe + DMA buffer info
                        // so AoRunRenderEvent can push DMA content into the
                        // ring without looking up globals or calling back
                        // into the stream object. DmaBuffer comes from the
                        // portcls-allocated WaveRT buffer already owned by
                        // this stream.
                        snap.Pipe          = pFP;
                        snap.DmaBuffer     = m_pDmaBuffer;
                        snap.DmaBufferSize = m_ulDmaBufferSize;
                        AoTransportOnRunEx(&snap);
                    }
                }
            }
            // [OLD PATH DISABLED — LoopbackRegisterFormat, MicSink stash/register removed]

            // Zero-fill DMA buffer to prevent garbage data at stream start
            if (m_pDmaBuffer && m_ulDmaBufferSize > 0)
            {
                RtlZeroMemory(m_pDmaBuffer, m_ulDmaBufferSize);
            }

            // Phase 6: Initialize fade-in for capture to prevent pop/click
            if (m_bCapture && m_pWfExt)
            {
                m_ulFadeInRemaining = FADE_SAMPLES_FOR_RATE(m_pWfExt->Format.nSamplesPerSec);
            }

            // Reset block alignment carry for clean start
            m_ulBlockAlignCarryForward = 0;

            // Start DMA
            LARGE_INTEGER ullPerfCounterTemp;
            ullPerfCounterTemp = KeQueryPerformanceCounter(&m_ullPerformanceCounterFrequency);
            m_ullLastDPCTimeStamp = m_ullDmaTimeStamp = KSCONVERT_PERFORMANCE_TIME(m_ullPerformanceCounterFrequency.QuadPart, ullPerfCounterTemp);

            if (m_ulNotificationIntervalMs > 0)
            {
                // Set timer for 1 ms. This will cause DPC to run every 1 ms but driver will send out 
                // notification events only after notification interval. This timer is used by Simple Audio Sample to 
                // emulate hardware and send out notification event. Real hardware should not use this
                // timer to fire notification event as it will drain power if the timer is running at 1 msec.
                ExSetTimer
                (
                    m_pNotificationTimer,
                    (-1) * HNSTIME_PER_MILLISECOND,
                    HNSTIME_PER_MILLISECOND, // 1 ms
                    NULL
                 );

            }

            // Phase 3: arm shadow-only pump for cable endpoints.
            // Re-arm per-run state; preserve monotonic stream counters across
            // PAUSE->RUN so the caller can observe total activity.
            if (m_pMiniport &&
                (m_pMiniport->m_DeviceType == eCableASpeaker ||
                 m_pMiniport->m_DeviceType == eCableBSpeaker ||
                 m_pMiniport->m_DeviceType == eCableAMic      ||
                 m_pMiniport->m_DeviceType == eCableBMic))
            {
                m_ulPumpFeatureFlags = AO_PUMP_FLAG_ENABLE | AO_PUMP_FLAG_SHADOW_ONLY;

                m_bPumpInitialized                 = FALSE;
                m_ullPumpBaselineHns               = 0;
                m_ulPumpProcessedFrames            = 0;
                m_ulPumpLastBufferOffset           = 0;
                m_ullPumpShadowWindowPumpFrames    = 0;
                m_ullPumpShadowWindowLegacyBytes   = 0;
                m_ulPumpShadowWindowCallCount      = 0;
                m_ulLastUpdatePositionByteDisplacement = 0;

                // Per-session counters: clear on fresh RUN arm.
                m_ulPumpGatedSkipCount = 0;
                m_ulPumpOverJumpCount  = 0;

                // Monotonic counters (InvocationCount, ShadowDivergenceCount,
                // FramesProcessed) are intentionally preserved.

                // Snapshot feature flags into the FRAME_PIPE direction slot.
                PFRAME_PIPE pFP = NULL;
                BOOLEAN isSpeaker = !m_bCapture;
                if (m_pMiniport->m_DeviceType == eCableASpeaker ||
                    m_pMiniport->m_DeviceType == eCableAMic)
                {
                    pFP = &g_CableAPipe;
                }
                else
                {
                    pFP = &g_CableBPipe;
                }
                FramePipeSetPumpFeatureFlags(pFP, /* isRenderSide */ isSpeaker, m_ulPumpFeatureFlags);
            }

            break;
    }

    m_KsState = State_;

    return ntStatus;
}

//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS CMiniportWaveRTStream::SetFormat
(
    _In_    KSDATAFORMAT    *DataFormat_
)
{
    UNREFERENCED_PARAMETER(DataFormat_);

    PAGED_CODE();

    //if (!m_fCapture && !g_DoNotCreateDataFiles)
    //{
    //    ntStatus = m_SaveData.SetDataFormat(Format);
    //}

    return STATUS_NOT_SUPPORTED;
}

#pragma code_seg()

//=============================================================================
#pragma code_seg()
//=============================================================================
// Phase 3 shadow-only pump. Invoked from GetPositions() in the same call as
// UpdatePosition(ilQPC) and still under m_PositionSpinLock. Performs compute,
// compare, and counter publication only. No transport mutation, no ownership
// move. See results/phase3_edit_proposal.md.

// Rolling compare window length (calls). Locked by approval record.
#define AO_PUMP_SHADOW_WINDOW_CALLS 128u

VOID CMiniportWaveRTStream::PumpToCurrentPositionFromQuery
(
    _In_ LARGE_INTEGER ilQPC
)
{
    // 1. Master gate.
    if ((m_ulPumpFeatureFlags & AO_PUMP_FLAG_ENABLE) == 0)
    {
        return;
    }

    // 2. Cable-only gate (single source of truth; call sites stay simple).
    if (m_pMiniport == NULL)
    {
        return;
    }
    BOOLEAN isCable =
        (m_pMiniport->m_DeviceType == eCableASpeaker ||
         m_pMiniport->m_DeviceType == eCableBSpeaker ||
         m_pMiniport->m_DeviceType == eCableAMic      ||
         m_pMiniport->m_DeviceType == eCableBMic);
    if (!isCable)
    {
        return;
    }

    // 3. Format sanity.
    if (m_pWfExt == NULL)
    {
        return;
    }
    ULONG nSamplesPerSec = m_pWfExt->Format.nSamplesPerSec;
    ULONG nBlockAlign    = m_pWfExt->Format.nBlockAlign;
    if (nSamplesPerSec == 0 || nBlockAlign == 0)
    {
        return;
    }

    // 4. Invocation count (monotonic).
    m_ulPumpInvocationCount++;

    // Convert current QPC to 100ns units, matching UpdatePosition() math.
    LONGLONG hnsNow = KSCONVERT_PERFORMANCE_TIME(
        m_ullPerformanceCounterFrequency.QuadPart, ilQPC);

    // 5. First-call latch: capture baseline, zero working state, mirror, bail.
    if (!m_bPumpInitialized)
    {
        m_ullPumpBaselineHns             = (ULONGLONG)hnsNow;
        m_ulPumpProcessedFrames          = 0;
        m_ullPumpShadowWindowPumpFrames  = 0;
        m_ullPumpShadowWindowLegacyBytes = 0;
        m_ulPumpShadowWindowCallCount    = 0;
        m_bPumpInitialized               = TRUE;

        // Mirror and return. m_ulLastUpdatePositionByteDisplacement is cleared
        // below as part of normal cleanup so the next call starts fresh.
        m_ulLastUpdatePositionByteDisplacement = 0;
        goto Mirror;
    }

    // 6. Elapsed frame math from baseline.
    //    totalFrames = elapsed_100ns * rate / 10_000_000
    {
        ULONGLONG elapsedHns =
            (ULONGLONG)hnsNow - m_ullPumpBaselineHns;
        // Guard against non-monotonic QPC conversions (should not happen).
        if ((LONGLONG)elapsedHns < 0)
        {
            m_ulLastUpdatePositionByteDisplacement = 0;
            goto Mirror;
        }

        ULONGLONG totalFrames =
            (elapsedHns * (ULONGLONG)nSamplesPerSec) / 10000000ull;

        if (totalFrames < (ULONGLONG)m_ulPumpProcessedFrames)
        {
            // Defensive: clock went backward vs. our run-local counter.
            m_ulLastUpdatePositionByteDisplacement = 0;
            goto Mirror;
        }

        ULONGLONG newFrames64 =
            totalFrames - (ULONGLONG)m_ulPumpProcessedFrames;

        // 7. 8-frame gate.
        if (newFrames64 < (ULONGLONG)FP_MIN_GATE_FRAMES)
        {
            m_ulPumpGatedSkipCount++;
            m_ulLastUpdatePositionByteDisplacement = 0;
            goto Mirror;
        }

        // 8. Over-jump diagnostic (Phase 3 revision, 2026-04-13).
        //    Phase 3 is SHADOW_ONLY. Over-jump here is count-only: it flags
        //    unusually large query-to-query frame deltas but must NOT
        //    suppress accounting or shadow compare, otherwise the rolling
        //    window never flushes and divergence stays a false zero.
        //    Threshold: max(framesPerDmaBuffer * 2, sampleRate / 4) so
        //    normal WASAPI / Phone Link polling intervals pass cleanly and
        //    only true outliers (multi-buffer stall, >=250ms gap) trip it.
        //    Real skip/clamp semantics return in Phase 5/6 where the pump
        //    owns transport.
        {
            ULONG framesPerDmaBuffer = 0;
            if (m_ulDmaBufferSize >= nBlockAlign)
            {
                framesPerDmaBuffer = m_ulDmaBufferSize / nBlockAlign;
            }
            ULONG twoBuffers      = framesPerDmaBuffer * 2u;
            ULONG quarterSecond   = nSamplesPerSec / 4u;
            ULONG overJumpThreshold =
                (twoBuffers > quarterSecond) ? twoBuffers : quarterSecond;

            if (newFrames64 > (ULONGLONG)overJumpThreshold)
            {
                m_ulPumpOverJumpCount++;
                // No return. No rebase. No window reset.
                // Fall through to accounting + compare.
            }
        }

        // 9. Accepted-frame accounting.
        ULONG newFrames = (ULONG)newFrames64;
        m_ulPumpProcessedFrames += newFrames;
        m_ullPumpFramesProcessed += newFrames;

        // 10. Rolling-window shadow compare vs. same-call legacy bytes.
        m_ullPumpShadowWindowPumpFrames  += newFrames;
        m_ullPumpShadowWindowLegacyBytes += m_ulLastUpdatePositionByteDisplacement;
        m_ulPumpShadowWindowCallCount++;

        if (m_ulPumpShadowWindowCallCount >= AO_PUMP_SHADOW_WINDOW_CALLS)
        {
            ULONGLONG legacyFrames =
                m_ullPumpShadowWindowLegacyBytes / (ULONGLONG)nBlockAlign;
            ULONGLONG pumpFrames = m_ullPumpShadowWindowPumpFrames;

            ULONGLONG diff = (pumpFrames > legacyFrames)
                ? (pumpFrames - legacyFrames)
                : (legacyFrames - pumpFrames);

            ULONGLONG larger = (pumpFrames > legacyFrames)
                ? pumpFrames
                : legacyFrames;

            // Tolerance: max(16 frames, 2% of larger total).
            ULONGLONG twoPercent = larger / 50ull;
            ULONGLONG tolerance  = (twoPercent > 16ull) ? twoPercent : 16ull;

            if (diff > tolerance)
            {
                m_ulPumpShadowDivergenceCount++;
            }

            m_ullPumpShadowWindowPumpFrames  = 0;
            m_ullPumpShadowWindowLegacyBytes = 0;
            m_ulPumpShadowWindowCallCount    = 0;
        }
    }

    // 11. Cleanup: consume the stash so the next call sees a fresh value.
    m_ulLastUpdatePositionByteDisplacement = 0;

Mirror:
    // Mirror stream counters into the correct FRAME_PIPE direction slot.
    // Phase 1 split render/capture slots; no cross-direction race.
    {
        PFRAME_PIPE pFP = NULL;
        BOOLEAN isSpeaker = !m_bCapture;
        if (m_pMiniport->m_DeviceType == eCableASpeaker ||
            m_pMiniport->m_DeviceType == eCableAMic)
        {
            pFP = &g_CableAPipe;
        }
        else if (m_pMiniport->m_DeviceType == eCableBSpeaker ||
                 m_pMiniport->m_DeviceType == eCableBMic)
        {
            pFP = &g_CableBPipe;
        }
        FramePipePublishPumpCounters(
            pFP,
            /* isRenderSide */            isSpeaker,
            /* gatedSkipCount */          m_ulPumpGatedSkipCount,
            /* overJumpCount */           m_ulPumpOverJumpCount,
            /* framesProcessedTotal */    m_ullPumpFramesProcessed,
            /* invocationCount */         m_ulPumpInvocationCount,
            /* shadowDivergenceCount */   m_ulPumpShadowDivergenceCount,
            /* featureFlags */            m_ulPumpFeatureFlags);
    }
}

//=============================================================================
VOID CMiniportWaveRTStream::UpdatePosition
(
    _In_ LARGE_INTEGER ilQPC
)
{
    // Phase 6 Y1C shadow shim — cable streams only.
    //
    // UpdatePosition is the common funnel for every legacy query path
    // (GetPosition, GetPositions, GetPacketCount, and the per-stream
    // MSVAD notification timer TimerNotifyRT). Hook it here so any
    // path that bypasses the explicit GetPosition/GetPositions hooks
    // still routes through the canonical helper in shadow mode. In
    // particular this catches the TimerNotifyRT code path which is
    // retired in Y4 but still active in Y1C.
    //
    // Intentional double-count with GetPosition/GetPositions hooks:
    // when called from those entry points the helper will be invoked
    // twice per call (once from this shim, once from the caller-site
    // hook). This is acceptable for shadow diagnostics — the counters
    // are observability-only, not authoritative. Y4 retires the
    // caller-site hooks along with the legacy body, leaving only this
    // shim as the single funnel.
    //
    // The legacy body below still runs unchanged. No externally
    // visible truth moves into the helper yet — that is Y2 (render)
    // and Y3 (capture).
    if (m_pTransportRt && m_pMiniport &&
        (m_pMiniport->m_DeviceType == eCableASpeaker ||
         m_pMiniport->m_DeviceType == eCableBSpeaker ||
         m_pMiniport->m_DeviceType == eCableAMic      ||
         m_pMiniport->m_DeviceType == eCableBMic))
    {
        AoCableAdvanceByQpc(m_pTransportRt,
                            (ULONGLONG)ilQPC.QuadPart,
                            AO_ADVANCE_QUERY,
                            0);
    }

    // Convert ticks to 100ns units.
    LONGLONG  hnsCurrentTime = KSCONVERT_PERFORMANCE_TIME(m_ullPerformanceCounterFrequency.QuadPart, ilQPC);
    
    // Calculate the time elapsed since the last call to GetPosition() or since the
    // DMA engine started.  Note that the division by 10000 to convert to milliseconds
    // may cause us to lose some of the time, so we will carry the remainder forward 
    // to the next GetPosition() call.
    //
    ULONG TimeElapsedInMS = (ULONG)(hnsCurrentTime - m_ullDmaTimeStamp + m_hnsElapsedTimeCarryForward)/10000;
    
    // Carry forward the remainder of this division so we don't fall behind with our position too much.
    //
    m_hnsElapsedTimeCarryForward = (hnsCurrentTime - m_ullDmaTimeStamp + m_hnsElapsedTimeCarryForward) % 10000;
    
    // Calculate how many bytes in the DMA buffer would have been processed in the elapsed
    // time.  Note that the division by 1000 to convert to milliseconds may cause us to 
    // lose some bytes, so we will carry the remainder forward to the next GetPosition() call.
    //
    // need to divide by 1000 because m_ulDmaMovementRate is average bytes per sec.

    ULONG ByteDisplacement = ((m_ulDmaMovementRate * TimeElapsedInMS) + m_byteDisplacementCarryForward) / 1000 ;
    m_byteDisplacementCarryForward = ((m_ulDmaMovementRate * TimeElapsedInMS) + m_byteDisplacementCarryForward) % 1000;

    // Phase 1: Block-align ByteDisplacement to prevent sample boundary misalignment.
    // Carry forward sub-block remainder to avoid long-term drift.
    if (m_pWfExt)
    {
        ULONG nBlockAlign = m_pWfExt->Format.nBlockAlign;
        ULONG totalBytes = ByteDisplacement + m_ulBlockAlignCarryForward;
        ByteDisplacement = (totalBytes / nBlockAlign) * nBlockAlign;
        m_ulBlockAlignCarryForward = totalBytes - ByteDisplacement;
    }

    // Phase 6 Y2-1.5 diagnostic: accumulate post-block-align legacy
    // ByteDisplacement into the transport runtime so the Y2 helper can
    // compare its own cumulative advance-bytes total against the
    // authoritative legacy total. Cable RENDER streams only — cable
    // mic has no Y2 render path and non-cable streams have no
    // transport rt attached.
    if (m_pTransportRt && m_pMiniport && !m_bCapture &&
        (m_pMiniport->m_DeviceType == eCableASpeaker ||
         m_pMiniport->m_DeviceType == eCableBSpeaker))
    {
        InterlockedAdd64(&m_pTransportRt->DbgY2LegacyRenderBytes,
                         (LONGLONG)ByteDisplacement);
    }

    // [OLD PATH DISABLED — MicSink gap zero-fill removed.
    //  FRAME_PIPE handles underrun silence via FramePipeReadFrames zero-fill.]

    // Increment presentation position even after last buffer is rendered.
    m_ullPresentationPosition += ByteDisplacement;

    if (m_bCapture)
    {
        // Phase 6 "Option Z" revert: restore Phase 4 legacy behavior.
        // Cable mic capture transport runs synchronously from
        // UpdatePosition via WriteBytes — same reason as the cable
        // speaker ReadBytes path above. Engine-owned transport is
        // deferred to the Option Y rework.
        WriteBytes(ByteDisplacement);
    }
    else
    {

        if (m_bEoSReceived)
        {
            // since EoS flag is set, we'll need to make sure not to read data beyond EOS position.
            // If driver's current position is less than EoS position, then make sure not to read data beyond EoS.
            if (m_ullWritePosition <= m_ulCurrentWritePosition)
            {
                ByteDisplacement = min(ByteDisplacement, m_ulCurrentWritePosition - (ULONG)m_ullWritePosition);
            }
            // If our current position is ahead of EoS position and we'll wrap around after new position then adjust
            // new position if it crosses EoS.
            else if ((m_ullWritePosition + ByteDisplacement) % m_ulDmaBufferSize < m_ullWritePosition)
            {
                if ((m_ullWritePosition + ByteDisplacement) % m_ulDmaBufferSize > m_ulCurrentWritePosition)
                {
                    ByteDisplacement = ByteDisplacement - (((ULONG)m_ullWritePosition + ByteDisplacement) % m_ulDmaBufferSize - m_ulCurrentWritePosition);
                }
            }
        }

        // If the last packet was rendered(read in the sample driver's case), send out an etw event.
        if (m_bEoSReceived && !m_bLastBufferRendered
            && (m_ullWritePosition + ByteDisplacement) % m_ulDmaBufferSize == m_ulCurrentWritePosition)
        {
            m_bLastBufferRendered = TRUE;
        }

        {
            // Phase 3 shadow-only baseline: legacy ReadBytes is the
            // sole audible owner for cable render. AoCableAdvanceByQpc
            // is invoked from the query and timer call sources (Phase
            // 3 step 2 / step 3 wiring) but operates in shadow mode
            // only -- it never touches the audible cable render data
            // flow during Phase 3. The Phase 6 Y2-2 audible-flip
            // (helper-owned ReadBytes replacement) is rolled back for
            // Phase 3 baseline; it lands in Phase 4 in a single
            // ownership-flip commit per ADR-006.
            //
            // file-save diagnostic gate g_DoNotCreateDataFiles is
            // independent of the cable audible owner and is preserved
            // unchanged.
            if (!g_DoNotCreateDataFiles)
            {
                ReadBytes(ByteDisplacement);
            }
        }
    }
    
    // Increment the DMA position by the number of bytes displaced since the last
    // call to UpdatePosition() and ensure we properly wrap at buffer length.
    //
    m_ullPlayPosition = m_ullWritePosition =
        (m_ullWritePosition + ByteDisplacement) % m_ulDmaBufferSize;
    
    // m_ullDmaTimeStamp is updated in both GetPostion and GetLinearPosition calls
    // so m_ullLinearPosition needs to be updated accordingly here
    //
    m_ullLinearPosition += ByteDisplacement;

    // Phase 3 shadow-only baseline: publish the monotonic position
    // for every cable stream (render AND capture). Legacy
    // UpdatePosition is the sole publisher of DmaProducedMono in
    // Phase 3; AoCableAdvanceByQpc is a shadow reader only. The
    // helper's render branch reads DmaProducedMono for diagnostic
    // advance computation but never writes it during Phase 3 (the
    // Y2-2 helper-owned writer was rolled back to restore the
    // single-publisher invariant).
    //
    // Cable capture retains its legacy owner unchanged through
    // Phase 5; cable render returns to legacy owner here for
    // Phase 3 and through Phase 4 entry. Phase 4 audible flip will
    // retire this publish for cable render in the same commit that
    // promotes the helper to producer ownership (ADR-006: no
    // dual-publisher window).
    //
    // Non-cable streams skip this entirely -- they still run on the
    // legacy ReadBytes/WriteBytes diagnostic path and have no engine
    // runtime attached.
    if (m_pTransportRt && m_pMiniport &&
        (m_pMiniport->m_DeviceType == eCableASpeaker ||
         m_pMiniport->m_DeviceType == eCableBSpeaker ||
         m_pMiniport->m_DeviceType == eCableAMic ||
         m_pMiniport->m_DeviceType == eCableBMic))
    {
        AoTransportPublishProducedBytes(m_pTransportRt, m_ullLinearPosition);
    }
    
    // Update the DMA time stamp for the next call to GetPosition()
    //
    m_ullDmaTimeStamp = hnsCurrentTime;

    // Phase 3 (Revision 2, 2026-04-13): accumulate finalized (post-EOS-clamp,
    // post-block-align) ByteDisplacement into the shadow pump stash.
    // Must be `+=`, not `=`: TimerNotifyRT also calls UpdatePosition every
    // 1 ms for cable endpoints, so per-call overwrite would drop the
    // timer-path contribution and the pump's rolling window would only see
    // a tiny GetPositions-only residual. With `+=`, the stash holds the
    // true total bytes legacy transport processed between pump consumes.
    // The pump helper reads and zeros this field under m_PositionSpinLock
    // in PumpToCurrentPositionFromQuery(), so reader/writer are serialized.
    m_ulLastUpdatePositionByteDisplacement += ByteDisplacement;
}

//=============================================================================
#pragma code_seg()
VOID CMiniportWaveRTStream::WriteBytes
(
    _In_ ULONG ByteDisplacement
)
/*++

Routine Description:

WriteBytes is called by CAPTURE devices (writes data INTO DMA for app to read).
For Cable A/B mic: pull from loopback ring buffer into DMA buffer.
For other devices: output silence (original behavior).

--*/
{
    ULONG bufferOffset = m_ullLinearPosition % m_ulDmaBufferSize;
    ULONG fadeStartOffset = bufferOffset;
    ULONG totalBytes = ByteDisplacement;

    // WriteBytes = capture device. Cable MIC reads from pipe.
    PFRAME_PIPE pPipe = NULL;
    CMiniportWaveRT* pMiniport = m_pMiniport;
    eDeviceType devType = eMaxDeviceType;
    if (pMiniport)
    {
        devType = pMiniport->m_DeviceType;
        if (devType == eCableAMic)
            pPipe = &g_CableAPipe;
        else if (devType == eCableBMic)
            pPipe = &g_CableBPipe;
    }

    // FRAME_PIPE: always pull from pipe (no MicSink direct-push path)
    {
        while (ByteDisplacement > 0)
        {
            ULONG runWrite = min(ByteDisplacement, m_ulDmaBufferSize - bufferOffset);

            if (pPipe)
            {
                // Pipe read → channel map → denormalize → DMA
                FramePipeReadToDma(pPipe, m_pDmaBuffer + bufferOffset, runWrite);
            }
            else
            {
                RtlZeroMemory(m_pDmaBuffer + bufferOffset, runWrite);
            }

            bufferOffset = (bufferOffset + runWrite) % m_ulDmaBufferSize;
            ByteDisplacement -= runWrite;
        }
    }

    // Phase 6: Apply fade-in ramp to prevent pop/click at capture stream start.
    // Applies to data from both paths (MicSink direct-push or LoopbackRead).
    // Supports 16-bit, 24-bit, and 32-bit float formats.
    if (m_ulFadeInRemaining > 0 && totalBytes > 0 && m_pWfExt)
    {
        ULONG nBlockAlign = m_pWfExt->Format.nBlockAlign;
        ULONG nChannels = m_pWfExt->Format.nChannels;
        ULONG bitsPerSample = m_pWfExt->Format.wBitsPerSample;
        ULONG sampleCount = totalBytes / nBlockAlign;
        ULONG fadePos = fadeStartOffset;
        ULONG fadeTotalSamples = FADE_SAMPLES_FOR_RATE(m_pWfExt->Format.nSamplesPerSec);
        if (fadeTotalSamples == 0) fadeTotalSamples = 1;

        for (ULONG i = 0; i < sampleCount && m_ulFadeInRemaining > 0; i++)
        {
            ULONG rampPos = fadeTotalSamples - m_ulFadeInRemaining;

            if (bitsPerSample == 16)
            {
                INT16* pSample = (INT16*)(m_pDmaBuffer + fadePos);
                for (ULONG ch = 0; ch < nChannels; ch++)
                {
                    pSample[ch] = (INT16)(((INT32)pSample[ch] * rampPos) / fadeTotalSamples);
                }
            }
            else if (bitsPerSample == 24)
            {
                BYTE* pFrame = m_pDmaBuffer + fadePos;
                for (ULONG ch = 0; ch < nChannels; ch++)
                {
                    BYTE* p = pFrame + ch * 3;
                    INT32 val = (INT32)(p[0] | (p[1] << 8) | ((INT8)p[2] << 16));
                    val = (INT32)(((INT64)val * rampPos) / fadeTotalSamples);
                    p[0] = (BYTE)(val & 0xFF);
                    p[1] = (BYTE)((val >> 8) & 0xFF);
                    p[2] = (BYTE)((val >> 16) & 0xFF);
                }
            }
            else if (bitsPerSample == 32)
            {
                // 32-bit IEEE float: integer-only ramp using sign+magnitude scaling.
                // Extract sign and exponent, scale mantissa, reconstruct.
                UINT32* pSample = (UINT32*)(m_pDmaBuffer + fadePos);
                for (ULONG ch = 0; ch < nChannels; ch++)
                {
                    UINT32 bits = pSample[ch];
                    UINT32 sign = bits & 0x80000000;
                    INT32 exp = (INT32)((bits >> 23) & 0xFF) - 127;
                    UINT32 mant = (bits & 0x7FFFFF) | 0x800000;

                    // Float to INT24: value = mant * 2^exp (scaled to [-0x800000, +0x7FFFFF])
                    INT32 val;
                    if (exp < -23)      val = 0;
                    else if (exp >= 0)  val = (exp > 0) ? 0x7FFFFF : (INT32)mant;
                    else                val = (INT32)(mant >> (-exp));

                    // Apply ramp in INT24 domain
                    val = (INT32)(((INT64)val * rampPos) / fadeTotalSamples);

                    // INT24 back to float bits (same algorithm as loopback.cpp Int24ToFloatBits)
                    if (val == 0)
                    {
                        pSample[ch] = sign;
                    }
                    else
                    {
                        UINT32 abs_val = (UINT32)val;
                        // Binary search for highest set bit position
                        ULONG bitpos = 0;
                        UINT32 tmp = abs_val;
                        if (tmp & 0xFF0000) { bitpos += 16; tmp >>= 16; }
                        if (tmp & 0xFF00)   { bitpos += 8;  tmp >>= 8;  }
                        if (tmp & 0xF0)     { bitpos += 4;  tmp >>= 4;  }
                        if (tmp & 0xC)      { bitpos += 2;  tmp >>= 2;  }
                        if (tmp & 0x2)      { bitpos += 1; }
                        // IEEE exponent: biased = 127 + bitpos - 23
                        INT32 bexp = 127 + (INT32)bitpos - 23;
                        if (bexp <= 0) { pSample[ch] = sign; }
                        else {
                            UINT32 m = (bitpos >= 23) ?
                                (abs_val >> (bitpos - 23)) : (abs_val << (23 - bitpos));
                            pSample[ch] = sign | ((UINT32)bexp << 23) | (m & 0x7FFFFF);
                        }
                    }
                }
            }

            fadePos = (fadePos + nBlockAlign) % m_ulDmaBufferSize;
            m_ulFadeInRemaining--;
        }
    }
}

//=============================================================================
#pragma code_seg()
VOID CMiniportWaveRTStream::ReadBytes
(
    _In_ ULONG ByteDisplacement
)
/*++

Routine Description:

ReadBytes is called by RENDER devices (reads app data FROM DMA buffer).
For Cable A/B speaker: push app audio into loopback ring buffer.
For other devices: save data to file (original behavior).

--*/
{
    ULONG bufferOffset = m_ullLinearPosition % m_ulDmaBufferSize;

    // ReadBytes = render device. Cable SPEAKER writes to loopback.
    PFRAME_PIPE pPipe = NULL;
    CMiniportWaveRT* pMiniport = m_pMiniport;
    eDeviceType devType = eMaxDeviceType;
    if (pMiniport)
    {
        devType = pMiniport->m_DeviceType;
        if (devType == eCableASpeaker)
            pPipe = &g_CableAPipe;
        else if (devType == eCableBSpeaker)
            pPipe = &g_CableBPipe;
    }

#if DBG
    DbgPrint("AO_DBG ReadBytes: DeviceType=%d, ByteDisp=%lu, pPipe=%p\n",
        (int)devType, ByteDisplacement, pPipe);
#endif

    while (ByteDisplacement > 0)
    {
        ULONG runWrite = min(ByteDisplacement, m_ulDmaBufferSize - bufferOffset);

        if (pPipe)
        {
            // FRAME_PIPE: normalize + channel map + pipe write
            FramePipeWriteFromDma(pPipe, m_pDmaBuffer + bufferOffset, runWrite);
        }
#if !defined(CABLE_A) && !defined(CABLE_B)
        else
        {
            // Default: save to file
            m_SaveData.WriteData(m_pDmaBuffer + bufferOffset, runWrite);
        }
#endif

        bufferOffset = (bufferOffset + runWrite) % m_ulDmaBufferSize;
        ByteDisplacement -= runWrite;
    }
}

//=============================================================================
#pragma code_seg("PAGE")
STDMETHODIMP_(NTSTATUS) 
CMiniportWaveRTStream::SetContentId
(
    _In_  ULONG                   contentId,
    _In_  PCDRMRIGHTS             drmRights
)
/*++

Routine Description:

  Sets DRM content Id for this stream. Also updates the Mixed content Id.

Arguments:

  contentId - new content id

  drmRights - rights for this stream.

Return Value:

  NT status code.

--*/
{
    PAGED_CODE();

    DPF_ENTER(("[CMiniportWaveRT::SetContentId]"));

    NTSTATUS    ntStatus;
    ULONG       ulOldContentId = contentId;

    m_ulContentId = contentId;

    //
    // Miniport should create a mixed DrmRights.
    //
    ntStatus = m_pMiniport->UpdateDrmRights();

    //
    // Restore the passed-in content Id.
    //
    if (!NT_SUCCESS(ntStatus))
    {
        m_ulContentId = ulOldContentId;
    }

    //
    // Simple Audio Sample writes each stream seperately to disk. If the rights for this
    // stream indicates that the stream is CopyProtected, stop writing to disk.
    //
#if !defined(CABLE_A) && !defined(CABLE_B)
    m_SaveData.Disable(drmRights->CopyProtect);
#else
    UNREFERENCED_PARAMETER(drmRights);
#endif

    //
    // From MSDN:
    //
    // This sample doesn't forward protected content, but if your driver uses 
    // lower layer drivers or a different stack to properly work, please see the 
    // following info from MSDN:
    //
    // "Before allowing protected content to flow through a data path, the system
    // verifies that the data path is secure. To do so, the system authenticates
    // each module in the data path beginning at the upstream end of the data path
    // and moving downstream. As each module is authenticated, that module gives
    // the system information about the next module in the data path so that it
    // can also be authenticated. To be successfully authenticated, a module's 
    // binary file must be signed as DRM-compliant.
    //
    // Two adjacent modules in the data path can communicate with each other in 
    // one of several ways. If the upstream module calls the downstream module 
    // through IoCallDriver, the downstream module is part of a WDM driver. In 
    // this case, the upstream module calls the DrmForwardContentToDeviceObject
    // function to provide the system with the device object representing the 
    // downstream module. (If the two modules communicate through the downstream
    // module's COM interface or content handlers, the upstream module calls 
    // DrmForwardContentToInterface or DrmAddContentHandlers instead.)
    //
    // DrmForwardContentToDeviceObject performs the same function as 
    // PcForwardContentToDeviceObject and IDrmPort2::ForwardContentToDeviceObject." 
    //
    // Other supported DRM DDIs for down-level module validation are: 
    // DrmForwardContentToInterfaces and DrmAddContentHandlers.
    //
    // For more information, see MSDN's DRM Functions and Interfaces.
    //

    return ntStatus;
} // SetContentId

//=============================================================================
#pragma code_seg()
void
TimerNotifyRT
(
    _In_      PEX_TIMER    Timer,
    _In_opt_  PVOID        DeferredContext
)
{
    LARGE_INTEGER qpc;
    LARGE_INTEGER qpcFrequency;
    BOOL bufferCompleted = FALSE;

    UNREFERENCED_PARAMETER(Timer);

    _IRQL_limited_to_(DISPATCH_LEVEL);

    CMiniportWaveRTStream* _this = (CMiniportWaveRTStream*)DeferredContext;
    
    if (NULL == _this)
    {
        return;
    }

    KIRQL oldIrql;
    KeAcquireSpinLock(&_this->m_PositionSpinLock, &oldIrql);

    qpc = KeQueryPerformanceCounter(&qpcFrequency);

    // Convert ticks to 100ns units.
    LONGLONG  hnsCurrentTime = KSCONVERT_PERFORMANCE_TIME(_this->m_ullPerformanceCounterFrequency.QuadPart, qpc);

    // Calculate the time elapsed since the last we ran DPC that matched Notification interval. Note that the division by 10000 
    // to convert to milliseconds may cause us to lose some of the time, so we will carry the remainder forward.

    ULONG TimeElapsedInMS = (ULONG)(hnsCurrentTime - _this->m_ullLastDPCTimeStamp + _this->m_hnsDPCTimeCarryForward)/10000;

    if (TimeElapsedInMS >= _this->m_ulNotificationIntervalMs)
    {
        // Carry forward the time greater than notification interval to adjust time to signal next buffer completion event accordingly.
        _this->m_hnsDPCTimeCarryForward = hnsCurrentTime - _this->m_ullLastDPCTimeStamp + _this->m_hnsDPCTimeCarryForward - (_this->m_ulNotificationIntervalMs * 10000);
        // Save the last time DPC ran at notification interval
        _this->m_ullLastDPCTimeStamp = hnsCurrentTime;
        bufferCompleted = TRUE;
    }

    // Cable endpoints: update position every tick for smooth data flow.
    // Non-cable endpoints: only update at notification interval (original behavior).
    BOOL isCableEndpoint = FALSE;
    if (_this->m_pMiniport)
    {
        eDeviceType dt = _this->m_pMiniport->GetDeviceType();
        isCableEndpoint = (dt == eCableASpeaker || dt == eCableAMic ||
                           dt == eCableBSpeaker || dt == eCableBMic);
    }

    if (isCableEndpoint)
    {
        // Always update position for cable endpoints (every 1ms tick).
        _this->UpdatePosition(qpc);

        if (bufferCompleted && !_this->m_bEoSReceived)
        {
            _this->m_llPacketCounter++;
        }
    }
    else
    {
        if (!bufferCompleted && !_this->m_bEoSReceived)
        {
            goto End;
        }

        _this->UpdatePosition(qpc);

        if (!_this->m_bEoSReceived)
        {
            _this->m_llPacketCounter++;
        }
    }

    if (_this->m_KsState != KSSTATE_RUN)
    {
        goto End;
    }

    // Cable endpoints call UpdatePosition every tick but notifications/underrun
    // checks only at buffer completion boundaries.
    if (!bufferCompleted && isCableEndpoint && !_this->m_bEoSReceived)
    {
        goto End;
    }

    {
        PADAPTERCOMMON  pAdapterComm = _this->m_pMiniport->GetAdapterCommObj();

        // Simple buffer underrun detection.
        if (!_this->IsCurrentWaveRTWritePositionUpdated() && !_this->m_bEoSReceived)
        {
            //Event type: eMINIPORT_GLITCH_REPORT
            //Parameter 1: Current linear buffer position
            //Parameter 2: Previous WaveRtBufferWritePosition that the driver received
            //Parameter 3: Major glitch code: 1:WaveRT buffer is underrun
            //Parameter 4: Minor code for the glitch cause
            pAdapterComm->WriteEtwEvent(eMINIPORT_GLITCH_REPORT,
                                        _this->m_ullLinearPosition,
                                        _this->GetCurrentWaveRTWritePosition(),
                                        1,      // WaveRT buffer is underrun
                                        0);
        }

        // Send buffer completion event if either of the following is true
        // 1. Driver consumed a complete buffer for this stream
        // 2. Driver consumed a partial buffer containing EoS for this stream

        if (!IsListEmpty(&_this->m_NotificationList) &&
            (bufferCompleted || _this->m_bLastBufferRendered))
        {
            PLIST_ENTRY leCurrent = _this->m_NotificationList.Flink;
            while (leCurrent != &_this->m_NotificationList)
            {
                NotificationListEntry* nleCurrent = CONTAINING_RECORD( leCurrent, NotificationListEntry, ListEntry);
                KeSetEvent(nleCurrent->NotificationEvent, 0, 0);

                leCurrent = leCurrent->Flink;
            }
        }

        if (_this->m_bLastBufferRendered)
        {
            ExCancelTimer(_this->m_pNotificationTimer, NULL);
        }
    }

End:
    KeReleaseSpinLock(&_this->m_PositionSpinLock, oldIrql);
    return;
}
//=============================================================================

