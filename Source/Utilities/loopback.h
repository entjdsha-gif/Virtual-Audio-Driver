/*++
Module Name:
    loopback.h
Abstract:
    Loopback ring buffer with format conversion for virtual audio cable.
    Internal ring buffer format: 48kHz/24bit/stereo (configurable rate via IOCTL).
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
#define LB_INTERNAL_CHANNELS           2
#define LB_INTERNAL_BLOCKALIGN         6   // 24bit * 2ch = 6 bytes/frame
#define LB_DEFAULT_LATENCY_MS          20
#define LB_MIN_LATENCY_MS             5
#define LB_MAX_LATENCY_MS             100

// Convert buffer size: enough for 192kHz/24bit/stereo 2ms = 2304 bytes
#define LB_CONVERT_BUF_SIZE           4096

// Legacy: fixed buffer size for backward compat calculation
#define LOOPBACK_BUFFER_SIZE  (48000 * 6 * 4)  // 4 seconds @ internal format

//=============================================================================
// Stream format descriptor
//=============================================================================
typedef struct _LB_FORMAT {
    ULONG  SampleRate;
    ULONG  BitsPerSample;
    ULONG  nChannels;
    ULONG  nBlockAlign;
} LB_FORMAT;

//=============================================================================
// SRC (sample rate converter) state - persists across DPC ticks
//=============================================================================
typedef struct _LB_SRC_STATE {
    ULONGLONG  Accumulator;     // 32.32 fixed-point position
    INT32      PrevSamples[2];  // previous samples (L, R) for interpolation
    BOOLEAN    Valid;            // PrevSamples initialized?
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

    // Scratch buffers for format conversion (NonPaged, pre-allocated)
    BYTE*          ConvertBufA;        // Speaker side conversion scratch
    BYTE*          ConvertBufB;        // Mic side conversion scratch
    ULONG          ConvertBufSize;

    // Dynamic configuration (IOCTL-settable)
    ULONG          InternalRate;       // Default 48000
    ULONG          MaxLatencyMs;       // Default 20
} LOOPBACK_BUFFER, *PLOOPBACK_BUFFER;

//=============================================================================
// Core ring buffer functions
//=============================================================================
NTSTATUS LoopbackInit(PLOOPBACK_BUFFER pLoopback);
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
    ULONG            nBlockAlign
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

#endif
