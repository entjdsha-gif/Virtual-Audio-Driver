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

#define LOOPBACK_BUFFER_SIZE  (16000 * 2 * 2)  // 2 seconds @ 16kHz 16-bit mono

typedef struct _LOOPBACK_BUFFER {
    BYTE*       Buffer;
    ULONG       BufferSize;
    ULONG       WritePos;
    ULONG       ReadPos;
    ULONG       DataCount;      // bytes currently in buffer (full/empty disambiguation)
    KSPIN_LOCK  SpinLock;
    BOOLEAN     Initialized;
} LOOPBACK_BUFFER, *PLOOPBACK_BUFFER;

NTSTATUS LoopbackInit(PLOOPBACK_BUFFER pLoopback);
VOID LoopbackCleanup(PLOOPBACK_BUFFER pLoopback);
VOID LoopbackWrite(PLOOPBACK_BUFFER pLoopback, const BYTE* Data, ULONG Count);
VOID LoopbackRead(PLOOPBACK_BUFFER pLoopback, BYTE* Data, ULONG Count);

extern LOOPBACK_BUFFER g_CableALoopback;
extern LOOPBACK_BUFFER g_CableBLoopback;

#endif
