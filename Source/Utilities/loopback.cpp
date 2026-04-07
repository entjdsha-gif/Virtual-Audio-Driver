/*++
Module Name:
    loopback.cpp
Abstract:
    Loopback ring buffer with format conversion engine.
    Internal format: 48kHz/24bit/stereo (configurable rate via IOCTL).
    Handles bit depth (16 to 24), channel (mono to stereo), and
    sample rate conversion using fixed-point linear interpolation.
    All arithmetic is integer-only, safe at DISPATCH_LEVEL.
--*/

#include "loopback.h"

// Global instances
LOOPBACK_BUFFER g_CableALoopback = { 0 };
LOOPBACK_BUFFER g_CableBLoopback = { 0 };

//=============================================================================
// Internal helpers: sample conversion (all integer, no float)
//=============================================================================

// Read a 24-bit packed sample from 3 bytes (little-endian, sign-extended)
static __forceinline INT32 Read24(const BYTE* p)
{
    return (INT32)(p[0] | (p[1] << 8) | ((INT8)p[2] << 16));
}

// Write a 24-bit packed sample to 3 bytes (little-endian)
static __forceinline VOID Write24(BYTE* p, INT32 val)
{
    p[0] = (BYTE)(val & 0xFF);
    p[1] = (BYTE)((val >> 8) & 0xFF);
    p[2] = (BYTE)((val >> 16) & 0xFF);
}

// 16-bit -> 24-bit (lossless)
static __forceinline INT32 Convert16to24(INT16 s16)
{
    return (INT32)s16 << 8;
}

// 24-bit -> 16-bit (rounded)
static __forceinline INT16 Convert24to16(INT32 s24)
{
    return (INT16)((s24 + 128) >> 8);
}

// Clamp INT32 to 24-bit range
static __forceinline INT32 Clamp24(INT32 val)
{
    if (val > 0x7FFFFF) return 0x7FFFFF;
    if (val < -0x800000) return -0x800000;
    return val;
}

//=============================================================================
// Convert input data to internal format (24-bit stereo samples in INT32 pairs)
// Returns number of stereo frames produced.
// outBuf must hold at least (inputFrames) stereo frame pairs.
//=============================================================================
static ULONG ConvertToInternal(
    const BYTE*     inData,
    ULONG           inFrames,
    const LB_FORMAT* inFmt,
    INT32*          outSamples     // output: L,R,L,R... as INT32 (24-bit range)
)
{
    ULONG produced = 0;
    const BYTE* p = inData;

    for (ULONG i = 0; i < inFrames; i++)
    {
        INT32 left = 0, right = 0;

        if (inFmt->BitsPerSample == 16)
        {
            left = Convert16to24(*(INT16*)p);
            if (inFmt->nChannels >= 2)
            {
                right = Convert16to24(*(INT16*)(p + 2));
            }
            else
            {
                right = left; // mono -> stereo duplication
            }
        }
        else // 24-bit
        {
            left = Read24(p);
            if (inFmt->nChannels >= 2)
            {
                right = Read24(p + 3);
            }
            else
            {
                right = left; // mono -> stereo duplication
            }
        }

        outSamples[produced * 2]     = left;
        outSamples[produced * 2 + 1] = right;
        produced++;
        p += inFmt->nBlockAlign;
    }

    return produced;
}

//=============================================================================
// Convert from internal stereo INT32 samples to output format bytes
// Returns bytes written.
//=============================================================================
static ULONG ConvertFromInternal(
    const INT32*    inSamples,     // L,R,L,R... in 24-bit range
    ULONG           inFrames,
    const LB_FORMAT* outFmt,
    BYTE*           outData
)
{
    BYTE* p = outData;

    for (ULONG i = 0; i < inFrames; i++)
    {
        INT32 left  = inSamples[i * 2];
        INT32 right = inSamples[i * 2 + 1];

        if (outFmt->nChannels == 1)
        {
            // stereo -> mono: average
            INT32 mono = (left + right) / 2;
            if (outFmt->BitsPerSample == 16)
            {
                *(INT16*)p = Convert24to16(mono);
            }
            else
            {
                Write24(p, Clamp24(mono));
            }
        }
        else
        {
            // stereo output
            if (outFmt->BitsPerSample == 16)
            {
                *(INT16*)p       = Convert24to16(left);
                *(INT16*)(p + 2) = Convert24to16(right);
            }
            else
            {
                Write24(p, Clamp24(left));
                Write24(p + 3, Clamp24(right));
            }
        }

        p += outFmt->nBlockAlign;
    }

    return (ULONG)(p - outData);
}

//=============================================================================
// Sample Rate Conversion (SRC) using fixed-point linear interpolation
// Converts stereo INT32 sample pairs from srcRate to dstRate.
// Returns number of output stereo frames.
//=============================================================================
static ULONG SrcConvert(
    const INT32*    inSamples,     // L,R pairs, inFrames count
    ULONG           inFrames,
    ULONG           srcRate,
    ULONG           dstRate,
    INT32*          outSamples,    // output L,R pairs
    ULONG           maxOutFrames,
    LB_SRC_STATE*   state          // persistent state across DPC ticks
)
{
    if (srcRate == dstRate)
    {
        // Passthrough - just copy
        ULONG frames = min(inFrames, maxOutFrames);
        RtlCopyMemory(outSamples, inSamples, frames * 2 * sizeof(INT32));
        // Update state for continuity
        if (inFrames > 0)
        {
            state->PrevSamples[0] = inSamples[(inFrames - 1) * 2];
            state->PrevSamples[1] = inSamples[(inFrames - 1) * 2 + 1];
            state->Valid = TRUE;
        }
        return frames;
    }

    // phase = (srcRate << 32) / dstRate -- 32.32 fixed-point step
    ULONGLONG phase = ((ULONGLONG)srcRate << 32) / dstRate;
    ULONGLONG acc = state->Accumulator;
    ULONG outCount = 0;

    // Previous samples for interpolation across DPC boundaries
    INT32 prevL = state->Valid ? state->PrevSamples[0] : (inFrames > 0 ? inSamples[0] : 0);
    INT32 prevR = state->Valid ? state->PrevSamples[1] : (inFrames > 0 ? inSamples[1] : 0);

    while (outCount < maxOutFrames)
    {
        ULONG idx = (ULONG)(acc >> 32);
        ULONG frac = (ULONG)(acc & 0xFFFFFFFF);

        if (idx >= inFrames)
            break;

        // Get current and next samples
        INT32 curL, curR, nextL, nextR;

        if (idx == 0 && state->Valid)
        {
            // First sample can interpolate with previous DPC's last sample
            curL = prevL;
            curR = prevR;
            nextL = inSamples[0];
            nextR = inSamples[1];
        }
        else if (idx > 0)
        {
            curL = inSamples[(idx - 1) * 2];
            curR = inSamples[(idx - 1) * 2 + 1];
            nextL = inSamples[idx * 2];
            nextR = inSamples[idx * 2 + 1];
        }
        else
        {
            // No previous data, use current sample (no interpolation for first)
            curL = inSamples[idx * 2];
            curR = inSamples[idx * 2 + 1];
            nextL = (idx + 1 < inFrames) ? inSamples[(idx + 1) * 2] : curL;
            nextR = (idx + 1 < inFrames) ? inSamples[(idx + 1) * 2 + 1] : curR;
        }

        // Linear interpolation: out = cur + (next - cur) * frac / 2^32
        INT64 diffL = (INT64)nextL - (INT64)curL;
        INT64 diffR = (INT64)nextR - (INT64)curR;
        outSamples[outCount * 2]     = (INT32)(curL + (INT32)((diffL * frac) >> 32));
        outSamples[outCount * 2 + 1] = (INT32)(curR + (INT32)((diffR * frac) >> 32));
        outCount++;

        acc += phase;
    }

    // Save state for next DPC tick
    // Subtract consumed input frames from accumulator
    ULONGLONG consumed = (ULONGLONG)inFrames << 32;
    if (acc >= consumed)
        state->Accumulator = acc - consumed;
    else
        state->Accumulator = 0;

    if (inFrames > 0)
    {
        state->PrevSamples[0] = inSamples[(inFrames - 1) * 2];
        state->PrevSamples[1] = inSamples[(inFrames - 1) * 2 + 1];
        state->Valid = TRUE;
    }

    return outCount;
}

//=============================================================================
// Helper: check if format matches internal format
//=============================================================================
static BOOLEAN FormatMatchesInternal(const LB_FORMAT* fmt, ULONG internalRate)
{
    return (fmt->SampleRate == internalRate &&
            fmt->BitsPerSample == LB_INTERNAL_BITS &&
            fmt->nChannels == LB_INTERNAL_CHANNELS);
}

//=============================================================================
// Helper: update FormatMatch flag
//=============================================================================
static VOID UpdateFormatMatch(PLOOPBACK_BUFFER pLB)
{
    if (pLB->SpeakerActive && pLB->MicActive)
    {
        // Both active: check if Speaker == Mic == Internal
        pLB->FormatMatch =
            (pLB->SpeakerFormat.SampleRate == pLB->MicFormat.SampleRate) &&
            (pLB->SpeakerFormat.BitsPerSample == pLB->MicFormat.BitsPerSample) &&
            (pLB->SpeakerFormat.nChannels == pLB->MicFormat.nChannels) &&
            FormatMatchesInternal(&pLB->SpeakerFormat, pLB->InternalRate);
    }
    else if (pLB->SpeakerActive)
    {
        pLB->FormatMatch = FormatMatchesInternal(&pLB->SpeakerFormat, pLB->InternalRate);
    }
    else if (pLB->MicActive)
    {
        pLB->FormatMatch = FormatMatchesInternal(&pLB->MicFormat, pLB->InternalRate);
    }
    else
    {
        pLB->FormatMatch = TRUE; // no streams, doesn't matter
    }
}

//=============================================================================
// Calculate buffer size from rate and latency
//=============================================================================
static ULONG CalcBufferSize(ULONG internalRate, ULONG latencyMs)
{
    // 4 seconds of internal format buffer for ring stability
    // (latencyMs controls the conceptual latency, but ring is larger for safety)
    ULONG minSize = internalRate * LB_INTERNAL_BLOCKALIGN * latencyMs / 1000;
    // Round up to LB_INTERNAL_BLOCKALIGN alignment
    minSize = ((minSize + LB_INTERNAL_BLOCKALIGN - 1) / LB_INTERNAL_BLOCKALIGN) * LB_INTERNAL_BLOCKALIGN;
    // At least 4 seconds for ring stability
    ULONG safeSize = internalRate * LB_INTERNAL_BLOCKALIGN * 4;
    return max(minSize, safeSize);
}

//=============================================================================
// LoopbackInit
//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS LoopbackInit(PLOOPBACK_BUFFER pLoopback)
{
    PAGED_CODE();

    // Set defaults
    pLoopback->InternalRate = LB_DEFAULT_INTERNAL_RATE;
    pLoopback->MaxLatencyMs = LB_DEFAULT_LATENCY_MS;

    pLoopback->BufferSize = CalcBufferSize(pLoopback->InternalRate, pLoopback->MaxLatencyMs);
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

    // MicSink starts inactive
    pLoopback->MicSink.DmaBuffer = NULL;
    pLoopback->MicSink.DmaBufferSize = 0;
    pLoopback->MicSink.WritePos = 0;
    pLoopback->MicSink.Active = FALSE;
    pLoopback->MicSink.TotalBytesWritten = 0;

    // Stashed Mic DMA info for deferred activation
    pLoopback->MicDmaStash = NULL;
    pLoopback->MicDmaStashSize = 0;

    // Format conversion: nothing registered initially
    RtlZeroMemory(&pLoopback->SpeakerFormat, sizeof(LB_FORMAT));
    RtlZeroMemory(&pLoopback->MicFormat, sizeof(LB_FORMAT));
    pLoopback->SpeakerActive = FALSE;
    pLoopback->MicActive = FALSE;
    pLoopback->FormatMatch = TRUE;

    RtlZeroMemory(&pLoopback->SpeakerSrcState, sizeof(LB_SRC_STATE));
    RtlZeroMemory(&pLoopback->MicSrcState, sizeof(LB_SRC_STATE));

    // Allocate conversion scratch buffers
    pLoopback->ConvertBufSize = LB_CONVERT_BUF_SIZE;
    pLoopback->ConvertBufA = (BYTE*)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, LB_CONVERT_BUF_SIZE, 'cvtA');
    pLoopback->ConvertBufB = (BYTE*)ExAllocatePool2(
        POOL_FLAG_NON_PAGED, LB_CONVERT_BUF_SIZE, 'cvtB');

    if (!pLoopback->ConvertBufA || !pLoopback->ConvertBufB)
    {
        if (pLoopback->ConvertBufA) ExFreePoolWithTag(pLoopback->ConvertBufA, 'cvtA');
        if (pLoopback->ConvertBufB) ExFreePoolWithTag(pLoopback->ConvertBufB, 'cvtB');
        ExFreePoolWithTag(pLoopback->Buffer, 'pooL');
        pLoopback->Buffer = NULL;
        pLoopback->ConvertBufA = NULL;
        pLoopback->ConvertBufB = NULL;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    pLoopback->Initialized = TRUE;

    return STATUS_SUCCESS;
}

//=============================================================================
// LoopbackCleanup
//=============================================================================
#pragma code_seg("PAGE")
VOID LoopbackCleanup(PLOOPBACK_BUFFER pLoopback)
{
    PAGED_CODE();

    if (pLoopback->Buffer)
    {
        ExFreePoolWithTag(pLoopback->Buffer, 'pooL');
        pLoopback->Buffer = NULL;
    }
    if (pLoopback->ConvertBufA)
    {
        ExFreePoolWithTag(pLoopback->ConvertBufA, 'cvtA');
        pLoopback->ConvertBufA = NULL;
    }
    if (pLoopback->ConvertBufB)
    {
        ExFreePoolWithTag(pLoopback->ConvertBufB, 'cvtB');
        pLoopback->ConvertBufB = NULL;
    }
    pLoopback->Initialized = FALSE;
}

//=============================================================================
// LoopbackWrite - raw bytes to ring buffer (used for passthrough path)
//=============================================================================
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

//=============================================================================
// LoopbackRead - raw bytes from ring buffer (used for passthrough/fallback)
//=============================================================================
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
// LoopbackReset
//=============================================================================
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

    // Reset SRC states for clean start
    RtlZeroMemory(&pLoopback->SpeakerSrcState, sizeof(LB_SRC_STATE));
    RtlZeroMemory(&pLoopback->MicSrcState, sizeof(LB_SRC_STATE));

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

//=============================================================================
// Format registration
//=============================================================================
#pragma code_seg()
VOID LoopbackRegisterFormat(
    PLOOPBACK_BUFFER pLoopback,
    BOOLEAN          isSpeaker,
    ULONG            sampleRate,
    ULONG            bitsPerSample,
    ULONG            nChannels,
    ULONG            nBlockAlign
)
{
    if (!pLoopback || !pLoopback->Initialized)
        return;

    KIRQL oldIrql;
    KeAcquireSpinLock(&pLoopback->SpinLock, &oldIrql);

    LB_FORMAT* fmt = isSpeaker ? &pLoopback->SpeakerFormat : &pLoopback->MicFormat;
    fmt->SampleRate = sampleRate;
    fmt->BitsPerSample = bitsPerSample;
    fmt->nChannels = nChannels;
    fmt->nBlockAlign = nBlockAlign;

    if (isSpeaker)
    {
        pLoopback->SpeakerActive = TRUE;
        RtlZeroMemory(&pLoopback->SpeakerSrcState, sizeof(LB_SRC_STATE));
    }
    else
    {
        pLoopback->MicActive = TRUE;
        RtlZeroMemory(&pLoopback->MicSrcState, sizeof(LB_SRC_STATE));
    }

    BOOLEAN oldMatch = pLoopback->FormatMatch;
    UpdateFormatMatch(pLoopback);

    // Auto-activate MicSink when FormatMatch transitions FALSE->TRUE
    // and Mic DMA info is stashed but MicSink is not yet active.
    if (pLoopback->FormatMatch && !oldMatch &&
        !pLoopback->MicSink.Active &&
        pLoopback->MicDmaStash != NULL)
    {
        pLoopback->MicSink.DmaBuffer     = pLoopback->MicDmaStash;
        pLoopback->MicSink.DmaBufferSize = pLoopback->MicDmaStashSize;
        pLoopback->MicSink.WritePos      = 0;
        pLoopback->MicSink.TotalBytesWritten = 0;
        pLoopback->MicSink.Active        = TRUE;
    }

    KeReleaseSpinLock(&pLoopback->SpinLock, oldIrql);
}

#pragma code_seg()
VOID LoopbackUnregisterFormat(
    PLOOPBACK_BUFFER pLoopback,
    BOOLEAN          isSpeaker
)
{
    if (!pLoopback || !pLoopback->Initialized)
        return;

    KIRQL oldIrql;
    KeAcquireSpinLock(&pLoopback->SpinLock, &oldIrql);

    if (isSpeaker)
    {
        pLoopback->SpeakerActive = FALSE;
        RtlZeroMemory(&pLoopback->SpeakerSrcState, sizeof(LB_SRC_STATE));
    }
    else
    {
        pLoopback->MicActive = FALSE;
        RtlZeroMemory(&pLoopback->MicSrcState, sizeof(LB_SRC_STATE));
    }

    UpdateFormatMatch(pLoopback);

    KeReleaseSpinLock(&pLoopback->SpinLock, oldIrql);
}

//=============================================================================
// LoopbackWriteConverted
// Speaker DPC calls this to write audio data with format conversion.
// Pipeline: Speaker format -> bit/channel -> SRC -> internal format -> ring buffer
//=============================================================================
#pragma code_seg()
VOID LoopbackWriteConverted(
    PLOOPBACK_BUFFER pLoopback,
    const BYTE*      Data,
    ULONG            ByteCount
)
{
    if (ByteCount == 0 || !pLoopback || !pLoopback->Initialized || !pLoopback->Buffer)
        return;

    // If format matches internal, use raw path
    if (pLoopback->FormatMatch)
    {
        LoopbackWrite(pLoopback, Data, ByteCount);
        return;
    }

    LB_FORMAT spkFmt;
    ULONG internalRate;

    // Snapshot format under spinlock
    KIRQL oldIrql;
    KeAcquireSpinLock(&pLoopback->SpinLock, &oldIrql);
    spkFmt = pLoopback->SpeakerFormat;
    internalRate = pLoopback->InternalRate;
    KeReleaseSpinLock(&pLoopback->SpinLock, oldIrql);

    if (spkFmt.nBlockAlign == 0 || spkFmt.SampleRate == 0)
        return;

    ULONG inFrames = ByteCount / spkFmt.nBlockAlign;
    if (inFrames == 0)
        return;

    // Step 1: Convert input to stereo INT32 samples (24-bit range)
    // Max frames we can handle in scratch buffer:
    // ConvertBufA used as INT32 array: bufSize / (2 * sizeof(INT32)) stereo frames
    ULONG maxStereoFrames = pLoopback->ConvertBufSize / (2 * sizeof(INT32));
    if (inFrames > maxStereoFrames)
        inFrames = maxStereoFrames;

    INT32* stereoSamples = (INT32*)pLoopback->ConvertBufA;
    ULONG stereoFrames = ConvertToInternal(Data, inFrames, &spkFmt, stereoSamples);

    // Step 2: SRC from speaker rate to internal rate
    // Use ConvertBufB for SRC output
    ULONG maxOutFrames = pLoopback->ConvertBufSize / (2 * sizeof(INT32));
    INT32* srcOutput = (INT32*)pLoopback->ConvertBufB;

    ULONG srcFrames = SrcConvert(
        stereoSamples, stereoFrames,
        spkFmt.SampleRate, internalRate,
        srcOutput, maxOutFrames,
        &pLoopback->SpeakerSrcState
    );

    if (srcFrames == 0)
        return;

    // Step 3: Convert stereo INT32 -> internal packed format (24-bit stereo)
    // Reuse ConvertBufA for packed output
    LB_FORMAT internalFmt = { internalRate, LB_INTERNAL_BITS, LB_INTERNAL_CHANNELS, LB_INTERNAL_BLOCKALIGN };
    ULONG packedBytes = ConvertFromInternal(srcOutput, srcFrames, &internalFmt, pLoopback->ConvertBufA);

    // Step 4: Write packed data to ring buffer
    // Need to acquire spinlock for ring buffer write AND mic sink push
    KeAcquireSpinLock(&pLoopback->SpinLock, &oldIrql);

    ULONG bufSize = pLoopback->BufferSize;
    ULONG writePos = pLoopback->WritePos;
    ULONG remaining = packedBytes;
    ULONG srcOffset = 0;

    while (remaining > 0)
    {
        ULONG chunk = min(remaining, bufSize - writePos);
        RtlCopyMemory(pLoopback->Buffer + writePos, pLoopback->ConvertBufA + srcOffset, chunk);
        writePos = (writePos + chunk) % bufSize;
        srcOffset += chunk;
        remaining -= chunk;
    }

    pLoopback->WritePos = writePos;
    pLoopback->DataCount += packedBytes;
    if (pLoopback->DataCount > bufSize)
    {
        pLoopback->DataCount = bufSize;
        pLoopback->ReadPos = writePos;
    }

    // If MicSink is active AND formats match (Speaker==Mic==Internal with MicSink),
    // also push converted data to Mic DMA directly.
    // For format mismatch with Mic, the Mic DPC will use LoopbackReadConverted instead.
    if (pLoopback->MicSink.Active && pLoopback->MicSink.DmaBuffer && pLoopback->MicActive)
    {
        // Check if Mic format == internal format for direct push
        if (FormatMatchesInternal(&pLoopback->MicFormat, internalRate))
        {
            BYTE*  micBuf  = pLoopback->MicSink.DmaBuffer;
            ULONG  micSize = pLoopback->MicSink.DmaBufferSize;
            ULONG  micPos  = pLoopback->MicSink.WritePos;
            ULONG  rem     = packedBytes;
            ULONG  src     = 0;

            while (rem > 0)
            {
                ULONG chunk = min(rem, micSize - micPos);
                RtlCopyMemory(micBuf + micPos, pLoopback->ConvertBufA + src, chunk);
                micPos = (micPos + chunk) % micSize;
                src   += chunk;
                rem   -= chunk;
            }

            pLoopback->MicSink.WritePos = micPos;
            pLoopback->MicSink.TotalBytesWritten += packedBytes;
        }
        // else: Mic has different format from internal - Mic DPC handles conversion via ReadConverted
    }

    KeReleaseSpinLock(&pLoopback->SpinLock, oldIrql);
}

//=============================================================================
// LoopbackReadConverted
// Mic DPC calls this to read data from ring buffer with format conversion.
// Pipeline: ring buffer (internal) -> SRC -> channel/bit -> Mic format
//=============================================================================
#pragma code_seg()
VOID LoopbackReadConverted(
    PLOOPBACK_BUFFER pLoopback,
    BYTE*            Data,
    ULONG            ByteCount
)
{
    if (ByteCount == 0)
        return;

    if (!pLoopback || !pLoopback->Initialized || !pLoopback->Buffer)
    {
        RtlZeroMemory(Data, ByteCount);
        return;
    }

    // If format matches internal, use raw path
    if (pLoopback->FormatMatch)
    {
        LoopbackRead(pLoopback, Data, ByteCount);
        return;
    }

    LB_FORMAT micFmt;
    ULONG internalRate;

    // Snapshot format
    KIRQL oldIrql;
    KeAcquireSpinLock(&pLoopback->SpinLock, &oldIrql);
    micFmt = pLoopback->MicFormat;
    internalRate = pLoopback->InternalRate;
    KeReleaseSpinLock(&pLoopback->SpinLock, oldIrql);

    if (micFmt.nBlockAlign == 0 || micFmt.SampleRate == 0)
    {
        RtlZeroMemory(Data, ByteCount);
        return;
    }

    ULONG outFrames = ByteCount / micFmt.nBlockAlign;
    if (outFrames == 0)
    {
        RtlZeroMemory(Data, ByteCount);
        return;
    }

    // Calculate how many internal frames we need to read
    // internalFrames = outFrames * internalRate / micRate (approximately)
    ULONG internalFrames = (ULONG)(((ULONGLONG)outFrames * internalRate + micFmt.SampleRate - 1) / micFmt.SampleRate);
    // Add 1 extra for SRC interpolation
    internalFrames += 1;

    // Limit to scratch buffer capacity
    ULONG maxFrames = pLoopback->ConvertBufSize / (2 * sizeof(INT32));
    if (internalFrames > maxFrames)
        internalFrames = maxFrames;

    // Step 1: Read internal format bytes from ring buffer
    ULONG internalBytes = internalFrames * LB_INTERNAL_BLOCKALIGN;
    // Use ConvertBufA for raw ring data
    if (internalBytes > pLoopback->ConvertBufSize)
        internalBytes = pLoopback->ConvertBufSize;

    KeAcquireSpinLock(&pLoopback->SpinLock, &oldIrql);

    ULONG bufSize = pLoopback->BufferSize;
    ULONG readPos = pLoopback->ReadPos;
    ULONG available = pLoopback->DataCount;
    ULONG toRead = min(internalBytes, available);
    // Align to internal block
    toRead = (toRead / LB_INTERNAL_BLOCKALIGN) * LB_INTERNAL_BLOCKALIGN;

    ULONG dstOff = 0;
    ULONG rem = toRead;
    while (rem > 0)
    {
        ULONG chunk = min(rem, bufSize - readPos);
        RtlCopyMemory(pLoopback->ConvertBufA + dstOff, pLoopback->Buffer + readPos, chunk);
        readPos = (readPos + chunk) % bufSize;
        dstOff += chunk;
        rem -= chunk;
    }
    pLoopback->ReadPos = readPos;
    pLoopback->DataCount -= toRead;

    KeReleaseSpinLock(&pLoopback->SpinLock, oldIrql);

    if (toRead == 0)
    {
        RtlZeroMemory(Data, ByteCount);
        return;
    }

    // Step 2: Convert internal packed bytes -> stereo INT32
    LB_FORMAT internalFmt = { internalRate, LB_INTERNAL_BITS, LB_INTERNAL_CHANNELS, LB_INTERNAL_BLOCKALIGN };
    ULONG readFrames = toRead / LB_INTERNAL_BLOCKALIGN;

    // Use ConvertBufB for stereo INT32
    INT32* stereoSamples = (INT32*)pLoopback->ConvertBufB;
    ULONG stereoFrames = ConvertToInternal(pLoopback->ConvertBufA, readFrames, &internalFmt, stereoSamples);

    // Step 3: SRC from internal rate to mic rate
    // Reuse ConvertBufA for SRC output
    INT32* srcOutput = (INT32*)pLoopback->ConvertBufA;
    ULONG srcFrames = SrcConvert(
        stereoSamples, stereoFrames,
        internalRate, micFmt.SampleRate,
        srcOutput, outFrames,   // limit to what mic needs
        &pLoopback->MicSrcState
    );

    // Step 4: Convert stereo INT32 -> Mic output format
    if (srcFrames > 0)
    {
        ULONG written = ConvertFromInternal(srcOutput, srcFrames, &micFmt, Data);
        // Zero-fill remainder
        if (written < ByteCount)
        {
            RtlZeroMemory(Data + written, ByteCount - written);
        }
    }
    else
    {
        RtlZeroMemory(Data, ByteCount);
    }
}

//=============================================================================
// Dynamic buffer resize (called from IOCTL at PASSIVE_LEVEL)
//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS LoopbackResizeBuffer(PLOOPBACK_BUFFER pLoopback, ULONG newLatencyMs)
{
    PAGED_CODE();

    if (!pLoopback || !pLoopback->Initialized)
        return STATUS_INVALID_PARAMETER;

    if (newLatencyMs < LB_MIN_LATENCY_MS)
        newLatencyMs = LB_MIN_LATENCY_MS;
    if (newLatencyMs > LB_MAX_LATENCY_MS)
        newLatencyMs = LB_MAX_LATENCY_MS;

    ULONG newSize = CalcBufferSize(pLoopback->InternalRate, newLatencyMs);

    BYTE* newBuf = (BYTE*)ExAllocatePool2(POOL_FLAG_NON_PAGED, newSize, 'pooL');
    if (!newBuf)
        return STATUS_INSUFFICIENT_RESOURCES;

    RtlZeroMemory(newBuf, newSize);

    KIRQL oldIrql;
    KeAcquireSpinLock(&pLoopback->SpinLock, &oldIrql);

    BYTE* oldBuf = pLoopback->Buffer;
    pLoopback->Buffer = newBuf;
    pLoopback->BufferSize = newSize;
    pLoopback->WritePos = 0;
    pLoopback->ReadPos = 0;
    pLoopback->DataCount = 0;
    pLoopback->MaxLatencyMs = newLatencyMs;

    // Reset SRC states
    RtlZeroMemory(&pLoopback->SpeakerSrcState, sizeof(LB_SRC_STATE));
    RtlZeroMemory(&pLoopback->MicSrcState, sizeof(LB_SRC_STATE));

    KeReleaseSpinLock(&pLoopback->SpinLock, oldIrql);

    if (oldBuf)
        ExFreePoolWithTag(oldBuf, 'pooL');

    return STATUS_SUCCESS;
}

//=============================================================================
// Set internal sample rate (called from IOCTL at PASSIVE_LEVEL)
//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS LoopbackSetInternalRate(PLOOPBACK_BUFFER pLoopback, ULONG newRate)
{
    PAGED_CODE();

    if (!pLoopback || !pLoopback->Initialized)
        return STATUS_INVALID_PARAMETER;

    // Validate rate
    if (newRate != 44100 && newRate != 48000 && newRate != 96000 && newRate != 192000)
        return STATUS_INVALID_PARAMETER;

    // Resize buffer for new rate
    ULONG newSize = CalcBufferSize(newRate, pLoopback->MaxLatencyMs);

    BYTE* newBuf = (BYTE*)ExAllocatePool2(POOL_FLAG_NON_PAGED, newSize, 'pooL');
    if (!newBuf)
        return STATUS_INSUFFICIENT_RESOURCES;

    RtlZeroMemory(newBuf, newSize);

    KIRQL oldIrql;
    KeAcquireSpinLock(&pLoopback->SpinLock, &oldIrql);

    BYTE* oldBuf = pLoopback->Buffer;
    pLoopback->Buffer = newBuf;
    pLoopback->BufferSize = newSize;
    pLoopback->WritePos = 0;
    pLoopback->ReadPos = 0;
    pLoopback->DataCount = 0;
    pLoopback->InternalRate = newRate;

    // Reset SRC states
    RtlZeroMemory(&pLoopback->SpeakerSrcState, sizeof(LB_SRC_STATE));
    RtlZeroMemory(&pLoopback->MicSrcState, sizeof(LB_SRC_STATE));

    // Update format match
    UpdateFormatMatch(pLoopback);

    KeReleaseSpinLock(&pLoopback->SpinLock, oldIrql);

    if (oldBuf)
        ExFreePoolWithTag(oldBuf, 'pooL');

    return STATUS_SUCCESS;
}
