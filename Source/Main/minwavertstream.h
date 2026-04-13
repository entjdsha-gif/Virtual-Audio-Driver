/*++

Copyright (c) Microsoft Corporation All Rights Reserved

Module Name:

    minwavertstream.h

Abstract:

    Definition of wavert miniport class.
--*/

#ifndef _VIRTUALAUDIODRIVER_MINWAVERTSTREAM_H_
#define _VIRTUALAUDIODRIVER_MINWAVERTSTREAM_H_

#if !defined(CABLE_A) && !defined(CABLE_B)
#include "savedata.h"
#include "ToneGenerator.h"
#endif

//
// Phase 1: runtime feature flags for the query-driven pump helper.
//
// Default at CMiniportWaveRTStream::Init: 0 (all flags clear).
// Phase 3 entry:   ENABLE | SHADOW_ONLY on cable endpoints in SetState RUN.
// Phase 5 entry:   DISABLE_LEGACY_RENDER  set after render-side pump parity.
// Phase 6 entry:   DISABLE_LEGACY_CAPTURE set after capture-side pump parity.
// IOCTL rollback:  clear DISABLE_LEGACY_* to fall back to UpdatePosition
//                  byte-per-ms transport.
//
// Phase 1 contract: these constants are declared here and stored into
// m_ulPumpFeatureFlags as zero. No Phase 1 code reads them or branches on
// their value; Phase 3 is the first reader.
//
#define AO_PUMP_FLAG_ENABLE                  0x00000001u
#define AO_PUMP_FLAG_SHADOW_ONLY             0x00000002u
#define AO_PUMP_FLAG_DISABLE_LEGACY_RENDER   0x00000004u
#define AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE  0x00000008u

//
// Structure to store notifications events in a protected list
//
typedef struct _NotificationListEntry
{
    LIST_ENTRY  ListEntry;
    PKEVENT     NotificationEvent;
} NotificationListEntry;

EXT_CALLBACK   TimerNotifyRT;

//=============================================================================
// Referenced Forward
//=============================================================================
class CMiniportWaveRT;
typedef CMiniportWaveRT *PCMiniportWaveRT;

//=============================================================================
// Classes
//=============================================================================
///////////////////////////////////////////////////////////////////////////////
// CMiniportWaveRTStream 
// 
class CMiniportWaveRTStream : 
    public IDrmAudioStream,
    public IMiniportWaveRTStreamNotification,
    public IMiniportWaveRTInputStream,
    public IMiniportWaveRTOutputStream,
    public CUnknown
{
protected:
    PPORTWAVERTSTREAM           m_pPortStream;
    LIST_ENTRY                  m_NotificationList;
    PEX_TIMER                   m_pNotificationTimer;
    ULONG                       m_ulNotificationIntervalMs;
    ULONG                       m_ulCurrentWritePosition;
    LONG                        m_IsCurrentWritePositionUpdated;
    
public:
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportWaveRTStream);
    ~CMiniportWaveRTStream();

    IMP_IMiniportWaveRTStream;
    IMP_IMiniportWaveRTStreamNotification;
    IMP_IMiniportWaveRTInputStream;
    IMP_IMiniportWaveRTOutputStream;
    IMP_IMiniportWaveRT;
    IMP_IDrmAudioStream;

    NTSTATUS                    Init
    ( 
        _In_  PCMiniportWaveRT    Miniport,
        _In_  PPORTWAVERTSTREAM   Stream,
        _In_  ULONG               Channel,
        _In_  BOOLEAN             Capture,
        _In_  PKSDATAFORMAT       DataFormat,
        _In_  GUID                SignalProcessingMode
    );

    // Friends
    friend class                CMiniportWaveRT;
    friend EXT_CALLBACK         TimerNotifyRT;
protected:
    CMiniportWaveRT*            m_pMiniport;
    ULONG                       m_ulPin;
    BOOLEAN                     m_bCapture;
    BOOLEAN                     m_bUnregisterStream;
    ULONG                       m_ulDmaBufferSize;
    BYTE*                       m_pDmaBuffer;
    ULONG                       m_ulNotificationsPerBuffer;
    KSSTATE                     m_KsState;
    PKTIMER                     m_pTimer;
    PRKDPC                      m_pDpc;
    ULONGLONG                   m_ullPlayPosition;
    ULONGLONG                   m_ullWritePosition;
    ULONGLONG                   m_ullLinearPosition;
    ULONGLONG                   m_ullPresentationPosition;
    ULONG                       m_ulLastOsReadPacket;
    ULONG                       m_ulLastOsWritePacket;
    LONGLONG                    m_llPacketCounter;
    ULONGLONG                   m_ullDmaTimeStamp;
    LARGE_INTEGER               m_ullPerformanceCounterFrequency;
    ULONGLONG                   m_hnsElapsedTimeCarryForward;
    ULONGLONG                   m_ullLastDPCTimeStamp;
    ULONGLONG                   m_hnsDPCTimeCarryForward;
    ULONG                       m_byteDisplacementCarryForward;
    ULONG                       m_ulBlockAlignCarryForward;
    ULONG                       m_ulDmaMovementRate;
    BOOL                        m_bLfxEnabled;
    PBOOL                       m_pbMuted;
    PLONG                       m_plVolumeLevel;
    PLONG                       m_plPeakMeter;
    PWAVEFORMATEXTENSIBLE       m_pWfExt;
    ULONG                       m_ulContentId;
#if !defined(CABLE_A) && !defined(CABLE_B)
    CSaveData                   m_SaveData;
    ToneGenerator               m_ToneGenerator;
    ULONG                       m_ulHostCaptureToneFrequency;
    DWORD                       m_dwHostCaptureToneAmplitude;
    DWORD                       m_dwLoopbackCaptureToneAmplitude;
    DWORD                       m_dwHostCaptureToneDCOffset;
    DWORD                       m_dwLoopbackCaptureToneDCOffset;
    DWORD                       m_dwHostCaptureToneInitialPhase;
    DWORD                       m_dwLoopbackCaptureToneInitialPhase;
#endif
    GUID                        m_SignalProcessingMode;
    BOOLEAN                     m_bEoSReceived;
    BOOLEAN                     m_bLastBufferRendered;
    KSPIN_LOCK                  m_PositionSpinLock;
    ULONG                       m_ulFadeInRemaining;

    // Phase 1: query-driven pump state (no behavior change; Phase 3 starts
    // writing these, Phase 5/6 start reading them). All fields init to zero
    // in Init() and stay zero through Phase 1 and Phase 2.
    ULONGLONG                   m_ullPumpBaselineHns;               // elapsed-100ns baseline for pump math
    ULONG                       m_ulPumpProcessedFrames;            // already-accounted frames in this run
    BOOLEAN                     m_bPumpInitialized;                 // first-call-after-RUN latch
    ULONG                       m_ulPumpLastBufferOffset;           // pump's own buffer-offset view (decoupled from m_ullLinearPosition)

    // Phase 1: stream-level diagnostic counters (copied into FRAME_PIPE
    // render/capture slots when the pump runs; Phase 3 first increment).
    ULONG                       m_ulPumpInvocationCount;            // total pump calls (any outcome)
    ULONG                       m_ulPumpGatedSkipCount;             // 8-frame gate fires
    ULONG                       m_ulPumpOverJumpCount;              // DMA over-jump guard fires
    ULONG                       m_ulPumpShadowDivergenceCount;      // Phase 3 shadow-window divergence hits
    ULONGLONG                   m_ullPumpFramesProcessed;           // monotonic run-total of pump-driven frames

    // Phase 1: runtime feature flags (AO_PUMP_FLAG_* bits). Stored as ULONG,
    // never read during Phase 1. Phase 3 is the first reader.
    ULONG                       m_ulPumpFeatureFlags;

    // Phase 1: Phase 3 shadow-window accumulators (declared now so Phase 3
    // can populate them without touching this header again).
    ULONGLONG                   m_ullPumpShadowWindowPumpFrames;
    ULONGLONG                   m_ullPumpShadowWindowLegacyBytes;
    ULONG                       m_ulPumpShadowWindowCallCount;
    ULONG                       m_ulLastUpdatePositionByteDisplacement;

public:

    //presentation
    NTSTATUS GetPresentationPosition
    (
        _Out_  KSAUDIO_PRESENTATION_POSITION *_pPresentationPosition
    );
        
    ULONG GetCurrentWaveRTWritePosition() 
    {
        return m_ulCurrentWritePosition;
    };

    // To support simple underrun validation.
    BOOL IsCurrentWaveRTWritePositionUpdated() 
    {
        return InterlockedExchange(&m_IsCurrentWritePositionUpdated, 0) ? TRUE : FALSE;
    };

    GUID GetSignalProcessingMode()
    {
        return m_SignalProcessingMode;
    }
    
private:

    //
    // Helper functions.
    //
    
#pragma code_seg()

    VOID WriteBytes
    (
        _In_ ULONG ByteDisplacement
    );
        
    VOID ReadBytes
    (
        _In_ ULONG ByteDisplacement
    );
    
    VOID UpdatePosition
    (
        _In_ LARGE_INTEGER ilQPC
    );

    // Phase 3: shadow-only query-path pump helper.
    // Invoked from GetPositions() in the same call as UpdatePosition(ilQPC),
    // still under m_PositionSpinLock. SHADOW_ONLY - no transport mutation,
    // no ownership move. See results/phase3_edit_proposal.md.
    VOID PumpToCurrentPositionFromQuery
    (
        _In_ LARGE_INTEGER ilQPC
    );

    NTSTATUS SetCurrentWritePositionInternal
    (
        _In_  ULONG ulCurrentWritePosition
    );
    
    NTSTATUS GetPositions
    (
        _Out_opt_  ULONGLONG *      _pullLinearBufferPosition, 
        _Out_opt_  ULONGLONG *      _pullPresentationPosition, 
        _Out_opt_  LARGE_INTEGER *  _pliQPCTime
    );

#if !defined(CABLE_A) && !defined(CABLE_B)
    NTSTATUS ReadRegistrySettings();
#endif
    
};
typedef CMiniportWaveRTStream *PCMiniportWaveRTStream;
#endif // _VIRTUALAUDIODRIVER_MINWAVERTSTREAM_H_

