/*++
Module Name:
    loopback.h
Abstract:
    Loopback ring buffer for virtual audio cable.
--*/

#ifndef _VIRTUAL_AUDIO_LOOPBACK_H_
#define _VIRTUAL_AUDIO_LOOPBACK_H_

// portcls.h includes ntddk.h and all kernel headers
#include <portcls.h>

#define LOOPBACK_BUFFER_SIZE  (48000 * 4 * 4)  // 4 seconds @ 48kHz 16-bit stereo

// Mic DMA sink - registered by capture stream so Speaker can push directly.
typedef struct _LOOPBACK_MIC_SINK {
    BYTE*       DmaBuffer;          // Mic's DMA buffer pointer
    ULONG       DmaBufferSize;      // Mic's DMA buffer size in bytes
    volatile ULONG  WritePos;       // Next write position in Mic DMA (updated by Speaker DPC)
    volatile BOOLEAN  Active;       // TRUE while Mic stream is open
    volatile ULONGLONG TotalBytesWritten; // monotonic counter of bytes Speaker pushed to Mic DMA
} LOOPBACK_MIC_SINK, *PLOOPBACK_MIC_SINK;

typedef struct _LOOPBACK_BUFFER {
    BYTE*       Buffer;
    ULONG       BufferSize;
    ULONG       WritePos;
    ULONG       ReadPos;
    ULONG       DataCount;      // bytes currently in buffer (full/empty disambiguation)
    KSPIN_LOCK  SpinLock;
    BOOLEAN     Initialized;

    // Direct-push sink: Speaker DPC writes here AND to Mic DMA simultaneously.
    LOOPBACK_MIC_SINK  MicSink;
} LOOPBACK_BUFFER, *PLOOPBACK_BUFFER;

NTSTATUS LoopbackInit(PLOOPBACK_BUFFER pLoopback);
VOID LoopbackCleanup(PLOOPBACK_BUFFER pLoopback);
VOID LoopbackWrite(PLOOPBACK_BUFFER pLoopback, const BYTE* Data, ULONG Count);
VOID LoopbackRead(PLOOPBACK_BUFFER pLoopback, BYTE* Data, ULONG Count);
VOID LoopbackReset(PLOOPBACK_BUFFER pLoopback);

// Mic stream registration: enables direct push from Speaker DPC.
VOID LoopbackRegisterMicSink(PLOOPBACK_BUFFER pLoopback, BYTE* DmaBuffer, ULONG DmaBufferSize);
VOID LoopbackUnregisterMicSink(PLOOPBACK_BUFFER pLoopback);

extern LOOPBACK_BUFFER g_CableALoopback;
extern LOOPBACK_BUFFER g_CableBLoopback;

#endif
