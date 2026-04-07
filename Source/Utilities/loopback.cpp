/*++
Module Name:
    loopback.cpp
Abstract:
    Loopback ring buffer implementation for virtual audio cable.
    Uses DataCount for full/empty disambiguation.
--*/

#include "loopback.h"

// Global instances
LOOPBACK_BUFFER g_CableALoopback = { 0 };
LOOPBACK_BUFFER g_CableBLoopback = { 0 };

#pragma code_seg("PAGE")
NTSTATUS LoopbackInit(PLOOPBACK_BUFFER pLoopback)
{
    PAGED_CODE();

    pLoopback->BufferSize = LOOPBACK_BUFFER_SIZE;
    pLoopback->Buffer = (BYTE*)ExAllocatePool2(
        POOL_FLAG_NON_PAGED,
        pLoopback->BufferSize,
        'pooL'
    );

    if (!pLoopback->Buffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(pLoopback->Buffer, pLoopback->BufferSize);
    pLoopback->WritePos = 0;
    pLoopback->ReadPos = 0;
    pLoopback->DataCount = 0;
    KeInitializeSpinLock(&pLoopback->SpinLock);

    // MicSink starts inactive - registered when Mic stream opens.
    pLoopback->MicSink.DmaBuffer = NULL;
    pLoopback->MicSink.DmaBufferSize = 0;
    pLoopback->MicSink.WritePos = 0;
    pLoopback->MicSink.Active = FALSE;
    pLoopback->MicSink.TotalBytesWritten = 0;

    pLoopback->Initialized = TRUE;

    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
VOID LoopbackCleanup(PLOOPBACK_BUFFER pLoopback)
{
    PAGED_CODE();

    if (pLoopback->Buffer)
    {
        ExFreePoolWithTag(pLoopback->Buffer, 'pooL');
        pLoopback->Buffer = NULL;
    }
    pLoopback->Initialized = FALSE;
}

#pragma code_seg()
VOID LoopbackWrite(
    PLOOPBACK_BUFFER pLoopback,
    const BYTE* Data,
    ULONG Count
)
{
    if (Count == 0 || !pLoopback || !pLoopback->Initialized || !pLoopback->Buffer)
    {
        return;
    }

    KIRQL oldIrql;
    KeAcquireSpinLock(&pLoopback->SpinLock, &oldIrql);

    ULONG bufSize = pLoopback->BufferSize;
    ULONG writePos = pLoopback->WritePos;
    ULONG remaining = Count;
    ULONG srcOffset = 0;

    while (remaining > 0)
    {
        ULONG chunk = min(remaining, bufSize - writePos);
        RtlCopyMemory(pLoopback->Buffer + writePos, Data + srcOffset, chunk);
        writePos = (writePos + chunk) % bufSize;
        srcOffset += chunk;
        remaining -= chunk;
    }

    pLoopback->WritePos = writePos;

    // Update data count, cap at buffer size (overwrite oldest on overflow)
    pLoopback->DataCount += Count;
    if (pLoopback->DataCount > bufSize)
    {
        // Overflow: advance read position to match
        pLoopback->DataCount = bufSize;
        pLoopback->ReadPos = writePos;
    }

    // Direct push to Mic DMA - eliminates async timing gap.
    if (pLoopback->MicSink.Active && pLoopback->MicSink.DmaBuffer)
    {
        BYTE*  micBuf   = pLoopback->MicSink.DmaBuffer;
        ULONG  micSize  = pLoopback->MicSink.DmaBufferSize;
        ULONG  micPos   = pLoopback->MicSink.WritePos;
        ULONG  rem      = Count;
        ULONG  src      = 0;

        while (rem > 0)
        {
            ULONG chunk = min(rem, micSize - micPos);
            RtlCopyMemory(micBuf + micPos, Data + src, chunk);
            micPos = (micPos + chunk) % micSize;
            src   += chunk;
            rem   -= chunk;
        }

        pLoopback->MicSink.WritePos = micPos;
        pLoopback->MicSink.TotalBytesWritten += Count;
    }

    KeReleaseSpinLock(&pLoopback->SpinLock, oldIrql);
}

#pragma code_seg()
VOID LoopbackRead(
    PLOOPBACK_BUFFER pLoopback,
    BYTE* Data,
    ULONG Count
)
{
    if (Count == 0)
    {
        return;
    }

    if (!pLoopback || !pLoopback->Initialized || !pLoopback->Buffer)
    {
        RtlZeroMemory(Data, Count);
        return;
    }

    KIRQL oldIrql;
    KeAcquireSpinLock(&pLoopback->SpinLock, &oldIrql);

    ULONG bufSize = pLoopback->BufferSize;
    ULONG readPos = pLoopback->ReadPos;
    ULONG available = pLoopback->DataCount;

    ULONG toRead = min(Count, available);
    ULONG dstOffset = 0;
    ULONG remaining = toRead;

    while (remaining > 0)
    {
        ULONG chunk = min(remaining, bufSize - readPos);
        RtlCopyMemory(Data + dstOffset, pLoopback->Buffer + readPos, chunk);
        readPos = (readPos + chunk) % bufSize;
        dstOffset += chunk;
        remaining -= chunk;
    }

    pLoopback->ReadPos = readPos;
    pLoopback->DataCount -= toRead;

    if (toRead < Count)
    {
        RtlZeroMemory(Data + toRead, Count - toRead);
    }

    KeReleaseSpinLock(&pLoopback->SpinLock, oldIrql);
}

//=============================================================================
// Mic DMA sink registration
//=============================================================================
#pragma code_seg()
VOID LoopbackRegisterMicSink(
    PLOOPBACK_BUFFER pLoopback,
    BYTE* DmaBuffer,
    ULONG DmaBufferSize
)
{
    if (!pLoopback || !pLoopback->Initialized)
        return;

    KIRQL oldIrql;
    KeAcquireSpinLock(&pLoopback->SpinLock, &oldIrql);

    pLoopback->MicSink.DmaBuffer     = DmaBuffer;
    pLoopback->MicSink.DmaBufferSize = DmaBufferSize;
    pLoopback->MicSink.WritePos      = 0;
    pLoopback->MicSink.TotalBytesWritten = 0;
    pLoopback->MicSink.Active        = TRUE;

    KeReleaseSpinLock(&pLoopback->SpinLock, oldIrql);
}

#pragma code_seg()
VOID LoopbackUnregisterMicSink(
    PLOOPBACK_BUFFER pLoopback
)
{
    if (!pLoopback || !pLoopback->Initialized)
        return;

    KIRQL oldIrql;
    KeAcquireSpinLock(&pLoopback->SpinLock, &oldIrql);

    pLoopback->MicSink.Active        = FALSE;
    pLoopback->MicSink.DmaBuffer     = NULL;
    pLoopback->MicSink.DmaBufferSize = 0;
    pLoopback->MicSink.WritePos      = 0;
    pLoopback->MicSink.TotalBytesWritten = 0;

    KeReleaseSpinLock(&pLoopback->SpinLock, oldIrql);
}

#pragma code_seg()
VOID LoopbackReset(PLOOPBACK_BUFFER pLoopback)
{
    if (!pLoopback || !pLoopback->Initialized || !pLoopback->Buffer)
        return;

    KIRQL oldIrql;
    KeAcquireSpinLock(&pLoopback->SpinLock, &oldIrql);

    pLoopback->WritePos = 0;
    pLoopback->ReadPos = 0;
    pLoopback->DataCount = 0;
    RtlZeroMemory(pLoopback->Buffer, pLoopback->BufferSize);

    KeReleaseSpinLock(&pLoopback->SpinLock, oldIrql);
}
