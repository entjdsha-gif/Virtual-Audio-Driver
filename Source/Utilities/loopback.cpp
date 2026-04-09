/*++
Module Name:
    loopback.cpp
Abstract:
    Loopback ring buffer with format conversion engine.
    Internal format: 48kHz/24bit/stereo (configurable rate via IOCTL).
    Handles bit depth (16/24/32float), channel (mono/stereo), and
    sample rate conversion using 8-tap windowed sinc interpolation.
    All arithmetic is integer-only, safe at DISPATCH_LEVEL.
--*/

#include "loopback.h"

// Layout verification (must match adapter.cpp)
C_ASSERT(sizeof(LOOPBACK_BUFFER) == 728);
C_ASSERT(FIELD_OFFSET(LOOPBACK_BUFFER, InternalRate) == 0x2CC);
C_ASSERT(FIELD_OFFSET(LOOPBACK_BUFFER, MaxLatencyMs) == 0x2D0);

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

// 24-bit -> 16-bit (rounded, clamped)
static __forceinline INT16 Convert24to16(INT32 s24)
{
    INT32 tmp = (s24 + 128) >> 8;
    if (tmp > 32767) tmp = 32767;
    if (tmp < -32768) tmp = -32768;
    return (INT16)tmp;
}

// Clamp INT32 to 24-bit range
static __forceinline INT32 Clamp24(INT32 val)
{
    if (val > 0x7FFFFF) return 0x7FFFFF;
    if (val < -0x800000) return -0x800000;
    return val;
}

//=============================================================================
// IEEE 754 float <-> INT24 conversion (integer-only, safe at DISPATCH_LEVEL)
// Float range [-1.0, +1.0] maps to INT24 range [-0x800000, +0x7FFFFF]
//=============================================================================

// Convert IEEE 754 float bits (reinterpreted as UINT32) to INT24
static __forceinline INT32 FloatBitsToInt24(UINT32 bits)
{
    // Extract IEEE 754 fields
    UINT32 sign = bits >> 31;
    INT32  exp  = (INT32)((bits >> 23) & 0xFF) - 127;  // unbiased exponent
    UINT32 mant = (bits & 0x7FFFFF) | 0x800000;        // implicit leading 1

    // Denormalized or zero
    if (exp < -23)
        return 0;

    // Float 1.0 has exp=0, mant=0x800000.  We want 1.0 -> 0x7FFFFF (23-bit max).
    // So we shift mantissa by (23 - exp) to scale: result = mant >> (23 - exp)
    // At exp=0: result = 0x800000 >> 23 ... no, let's think differently.
    //
    // The float value is: (-1)^sign * mant/2^23 * 2^exp = (-1)^sign * mant * 2^(exp-23)
    // We want to multiply by 2^23 (to scale [-1,+1] to [-0x800000, +0x7FFFFF]):
    //   result = mant * 2^(exp-23) * 2^23 = mant * 2^exp
    // So shift = exp.  If exp >= 0, left-shift; if exp < 0, right-shift.

    INT32 result;
    if (exp >= 0)
    {
        // Clamp to prevent overflow (exp > 0 means |value| > 1.0)
        if (exp > 0)
            result = 0x7FFFFF;  // saturate
        else
            result = (INT32)(mant);  // exp==0: mant is exactly 0x800000 for 1.0
    }
    else
    {
        // exp < 0: right-shift
        if (-exp >= 24)
            return 0;
        result = (INT32)(mant >> (-exp));
    }

    // Clamp: positive to +0x7FFFFF, negative can reach -0x800000
    if (sign)
    {
        if (result > 0x800000) result = 0x800000;
        return -(INT32)result;
    }
    else
    {
        if (result > 0x7FFFFF) result = 0x7FFFFF;
        return result;
    }
}

// Convert INT24 to IEEE 754 float bits (returned as UINT32)
static __forceinline UINT32 Int24ToFloatBits(INT32 s24)
{
    if (s24 == 0)
        return 0x00000000;  // +0.0f

    UINT32 sign = 0;
    UINT32 abs_val;
    if (s24 < 0)
    {
        sign = 1;
        abs_val = (UINT32)(-s24);
    }
    else
    {
        abs_val = (UINT32)s24;
    }

    // We want to produce float = s24 / 2^23, i.e., value = abs_val * 2^(-23)
    // Find highest set bit position (0-based from LSB)
    ULONG bitpos = 0;
    UINT32 tmp = abs_val;
    // Manual bit scan (safe at DISPATCH_LEVEL, no intrinsic dependency)
    if (tmp & 0xFF0000) { bitpos += 16; tmp >>= 16; }
    if (tmp & 0xFF00)   { bitpos += 8;  tmp >>= 8;  }
    if (tmp & 0xF0)     { bitpos += 4;  tmp >>= 4;  }
    if (tmp & 0xC)      { bitpos += 2;  tmp >>= 2;  }
    if (tmp & 0x2)      { bitpos += 1; }

    // IEEE exponent: biased = 127 + bitpos - 23 (because value = abs_val * 2^(-23))
    INT32 biased_exp = 127 + (INT32)bitpos - 23;
    if (biased_exp <= 0)
        return 0;  // too small, flush to zero
    if (biased_exp >= 255)
        biased_exp = 254;  // clamp (shouldn't happen for 24-bit input)

    // Mantissa: shift to align leading bit at position 23, then mask out implicit 1
    UINT32 mantissa;
    if (bitpos >= 23)
        mantissa = (abs_val >> (bitpos - 23)) & 0x7FFFFF;
    else
        mantissa = (abs_val << (23 - bitpos)) & 0x7FFFFF;

    return (sign << 31) | ((UINT32)biased_exp << 23) | mantissa;
}

//=============================================================================
// Read one sample from input buffer based on format (16/24/32float)
//=============================================================================
static __forceinline INT32 ReadSample(const BYTE* p, const LB_FORMAT* fmt)
{
    if (fmt->BitsPerSample == 32 && fmt->IsFloat)
        return FloatBitsToInt24(*(UINT32*)p);
    else if (fmt->BitsPerSample == 16)
        return Convert16to24(*(INT16*)p);
    else
        return Read24(p);
}

// Bytes per sample for a given format
static __forceinline ULONG BytesPerSample(const LB_FORMAT* fmt)
{
    if (fmt->BitsPerSample == 32) return 4;
    if (fmt->BitsPerSample == 24) return 3;
    return 2; // 16-bit
}

//=============================================================================
// Write one sample to output buffer based on format
//=============================================================================
static __forceinline void WriteSample(BYTE* p, INT32 val, const LB_FORMAT* fmt)
{
    if (fmt->BitsPerSample == 32 && fmt->IsFloat)
        *(UINT32*)p = Int24ToFloatBits(val);
    else if (fmt->BitsPerSample == 16)
        *(INT16*)p = Convert24to16(val);
    else
        Write24(p, Clamp24(val));
}

//=============================================================================
// Convert input data to internal N-channel format (INT32 per channel)
// Output: ch0,ch1,...ch(N-1) per frame, N = LB_INTERNAL_CHANNELS (8)
// Channel mapping:
//   1ch -> FL=FR=input, rest silence
//   2ch -> FL,FR from input, rest silence
//   6ch (5.1) -> FL,FR,FC,LFE,BL,BR from input, SL,SR silence
//   8ch (7.1) -> all channels from input
// Returns number of internal frames produced.
//=============================================================================
static ULONG ConvertToInternal(
    const BYTE*     inData,
    ULONG           inFrames,
    const LB_FORMAT* inFmt,
    INT32*          outSamples
)
{
    const ULONG nCh = LB_INTERNAL_CHANNELS;
    const ULONG bps = BytesPerSample(inFmt);
    const BYTE* p = inData;

    for (ULONG i = 0; i < inFrames; i++)
    {
        INT32* out = outSamples + i * nCh;
        // Zero all channels first
        RtlZeroMemory(out, nCh * sizeof(INT32));

        if (inFmt->nChannels == 1)
        {
            INT32 val = ReadSample(p, inFmt);
            out[0] = val;  // FL
            out[1] = val;  // FR (duplicate mono)
        }
        else
        {
            // Read min(input channels, internal channels)
            ULONG chToCopy = min(inFmt->nChannels, nCh);
            for (ULONG ch = 0; ch < chToCopy; ch++)
            {
                out[ch] = ReadSample(p + ch * bps, inFmt);
            }
        }

        p += inFmt->nBlockAlign;
    }

    return inFrames;
}

//=============================================================================
// Convert from internal N-channel INT32 samples to output format bytes
// Channel downmix/copy from 8ch internal:
//   8ch -> 1ch: average FL + FR
//   8ch -> 2ch: copy FL, FR
//   8ch -> 6ch (5.1): copy FL,FR,FC,LFE,BL,BR
//   8ch -> 8ch (7.1): copy all channels
// Returns bytes written.
//=============================================================================
static ULONG ConvertFromInternal(
    const INT32*    inSamples,
    ULONG           inFrames,
    const LB_FORMAT* outFmt,
    BYTE*           outData
)
{
    const ULONG nCh = LB_INTERNAL_CHANNELS;
    const ULONG bps = BytesPerSample(outFmt);
    BYTE* p = outData;

    for (ULONG i = 0; i < inFrames; i++)
    {
        const INT32* in = inSamples + i * nCh;

        if (outFmt->nChannels == 1)
        {
            INT32 mono = (in[0] + in[1]) / 2;  // FL + FR average
            WriteSample(p, mono, outFmt);
        }
        else
        {
            ULONG chToCopy = min(outFmt->nChannels, nCh);
            for (ULONG ch = 0; ch < chToCopy; ch++)
            {
                WriteSample(p + ch * bps, in[ch], outFmt);
            }
        }

        p += outFmt->nBlockAlign;
    }

    return (ULONG)(p - outData);
}

//=============================================================================
// 8-tap Kaiser-windowed sinc coefficient table (Q23 fixed-point)
// 256 sub-phases x 8 taps = 2048 coefficients
// Generated: Kaiser beta=6.0, normalized sinc
//=============================================================================
#include "sinc_table.h"

//=============================================================================
// Sample Rate Conversion using 8-tap windowed sinc interpolation
// Processes LB_INTERNAL_CHANNELS per frame.
// Returns number of output frames.
//=============================================================================
static ULONG SrcConvert(
    const INT32*    inSamples,
    ULONG           inFrames,
    ULONG           srcRate,
    ULONG           dstRate,
    INT32*          outSamples,
    ULONG           maxOutFrames,
    LB_SRC_STATE*   state
)
{
    const ULONG nCh = LB_INTERNAL_CHANNELS;
    const ULONG halfTaps = LB_SINC_TAPS / 2;

    if (srcRate == dstRate)
    {
        ULONG frames = min(inFrames, maxOutFrames);
        RtlCopyMemory(outSamples, inSamples, frames * nCh * sizeof(INT32));
        // Save last frames as history
        if (inFrames > 0)
        {
            ULONG histStart = (inFrames > LB_SINC_TAPS) ? inFrames - LB_SINC_TAPS : 0;
            ULONG histCount = inFrames - histStart;
            RtlCopyMemory(state->PrevSamples,
                          inSamples + histStart * nCh,
                          histCount * nCh * sizeof(INT32));
            state->HistoryCount = histCount;
            state->Valid = TRUE;
        }
        return frames;
    }

    ULONGLONG phase = ((ULONGLONG)srcRate << 32) / dstRate;
    ULONGLONG acc = state->Accumulator;
    ULONG outCount = 0;

    while (outCount < maxOutFrames)
    {
        ULONG idx = (ULONG)(acc >> 32);
        if (idx >= inFrames)
            break;

        // Sub-phase: top 8 bits of fractional part -> 0..255
        ULONG fracPhase = (ULONG)((acc >> 24) & 0xFF);
        const INT32* coeff = &g_SincTable[fracPhase * LB_SINC_TAPS];

        for (ULONG ch = 0; ch < nCh; ch++)
        {
            INT64 sum = 0;
            for (ULONG tap = 0; tap < LB_SINC_TAPS; tap++)
            {
                // tapIdx centers the filter: idx - halfTaps + 1 + tap
                INT32 tapIdx = (INT32)idx - (INT32)halfTaps + 1 + (INT32)tap;
                INT32 sample;

                if (tapIdx < 0)
                {
                    // Fetch from history buffer
                    INT32 histIdx = (INT32)state->HistoryCount + tapIdx;
                    if (histIdx >= 0 && state->Valid)
                        sample = state->PrevSamples[histIdx * nCh + ch];
                    else
                        sample = 0;
                }
                else if ((ULONG)tapIdx < inFrames)
                {
                    sample = inSamples[tapIdx * nCh + ch];
                }
                else
                {
                    sample = 0;  // beyond input, zero-pad
                }

                sum += (INT64)sample * coeff[tap];
            }
            outSamples[outCount * nCh + ch] = Clamp24((INT32)(sum >> 23));
        }

        outCount++;
        acc += phase;
    }

    // Save state
    ULONGLONG consumed = (ULONGLONG)inFrames << 32;
    state->Accumulator = (acc >= consumed) ? (acc - consumed) : 0;

    // Store last LB_SINC_TAPS frames as history
    if (inFrames > 0)
    {
        ULONG histStart = (inFrames > LB_SINC_TAPS) ? inFrames - LB_SINC_TAPS : 0;
        ULONG histCount = inFrames - histStart;
        RtlCopyMemory(state->PrevSamples,
                      inSamples + histStart * nCh,
                      histCount * nCh * sizeof(INT32));
        state->HistoryCount = histCount;
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
            fmt->nChannels == LB_INTERNAL_CHANNELS &&
            !fmt->IsFloat);  // internal format is always PCM
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
    pLoopback->SpeakerSrcResetPending = FALSE;
    pLoopback->MicSrcResetPending = FALSE;

    // Allocate conversion scratch buffers (separate for Speaker/Mic DPCs to avoid race)
    pLoopback->ConvertBufSize = LB_CONVERT_BUF_SIZE;
    pLoopback->SpkConvertBufA = (BYTE*)ExAllocatePool2(POOL_FLAG_NON_PAGED, LB_CONVERT_BUF_SIZE, 'scvA');
    pLoopback->SpkConvertBufB = (BYTE*)ExAllocatePool2(POOL_FLAG_NON_PAGED, LB_CONVERT_BUF_SIZE, 'scvB');
    pLoopback->MicConvertBufA = (BYTE*)ExAllocatePool2(POOL_FLAG_NON_PAGED, LB_CONVERT_BUF_SIZE, 'mcvA');
    pLoopback->MicConvertBufB = (BYTE*)ExAllocatePool2(POOL_FLAG_NON_PAGED, LB_CONVERT_BUF_SIZE, 'mcvB');

    if (!pLoopback->SpkConvertBufA || !pLoopback->SpkConvertBufB ||
        !pLoopback->MicConvertBufA || !pLoopback->MicConvertBufB)
    {
        if (pLoopback->SpkConvertBufA) ExFreePoolWithTag(pLoopback->SpkConvertBufA, 'scvA');
        if (pLoopback->SpkConvertBufB) ExFreePoolWithTag(pLoopback->SpkConvertBufB, 'scvB');
        if (pLoopback->MicConvertBufA) ExFreePoolWithTag(pLoopback->MicConvertBufA, 'mcvA');
        if (pLoopback->MicConvertBufB) ExFreePoolWithTag(pLoopback->MicConvertBufB, 'mcvB');
        ExFreePoolWithTag(pLoopback->Buffer, 'pooL');
        pLoopback->Buffer = NULL;
        pLoopback->SpkConvertBufA = pLoopback->SpkConvertBufB = NULL;
        pLoopback->MicConvertBufA = pLoopback->MicConvertBufB = NULL;
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
    if (pLoopback->SpkConvertBufA) { ExFreePoolWithTag(pLoopback->SpkConvertBufA, 'scvA'); pLoopback->SpkConvertBufA = NULL; }
    if (pLoopback->SpkConvertBufB) { ExFreePoolWithTag(pLoopback->SpkConvertBufB, 'scvB'); pLoopback->SpkConvertBufB = NULL; }
    if (pLoopback->MicConvertBufA) { ExFreePoolWithTag(pLoopback->MicConvertBufA, 'mcvA'); pLoopback->MicConvertBufA = NULL; }
    if (pLoopback->MicConvertBufB) { ExFreePoolWithTag(pLoopback->MicConvertBufB, 'mcvB'); pLoopback->MicConvertBufB = NULL; }
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
    ULONG            nBlockAlign,
    BOOLEAN          isFloat
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
    fmt->IsFloat = isFloat;

    if (isSpeaker)
    {
        pLoopback->SpeakerActive = TRUE;
        pLoopback->SpeakerSrcResetPending = TRUE;
    }
    else
    {
        pLoopback->MicActive = TRUE;
        pLoopback->MicSrcResetPending = TRUE;
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

    // Step 1: Convert input to internal N-channel INT32 samples (24-bit range)
    const ULONG nCh = LB_INTERNAL_CHANNELS;
    ULONG maxIntFrames = pLoopback->ConvertBufSize / (nCh * sizeof(INT32));
    if (inFrames > maxIntFrames)
        inFrames = maxIntFrames;

    INT32* intSamples = (INT32*)pLoopback->SpkConvertBufA;
    ULONG intFrames = ConvertToInternal(Data, inFrames, &spkFmt, intSamples);

    // Step 2: SRC from speaker rate to internal rate
    ULONG maxOutFrames = pLoopback->ConvertBufSize / (nCh * sizeof(INT32));
    INT32* srcOutput = (INT32*)pLoopback->SpkConvertBufB;

    // Consume reset flag (set by IOCTL/RegisterFormat, safe to reset here in DPC context)
    if (pLoopback->SpeakerSrcResetPending)
    {
        RtlZeroMemory(&pLoopback->SpeakerSrcState, sizeof(LB_SRC_STATE));
        pLoopback->SpeakerSrcResetPending = FALSE;
    }

    ULONG srcFrames = SrcConvert(
        intSamples, intFrames,
        spkFmt.SampleRate, internalRate,
        srcOutput, maxOutFrames,
        &pLoopback->SpeakerSrcState
    );

    if (srcFrames == 0)
        return;

    // Step 3: Convert N-channel INT32 -> internal packed format
    LB_FORMAT internalFmt = { internalRate, LB_INTERNAL_BITS, LB_INTERNAL_CHANNELS, LB_INTERNAL_BLOCKALIGN, FALSE };
    ULONG packedBytes = ConvertFromInternal(srcOutput, srcFrames, &internalFmt, pLoopback->SpkConvertBufA);

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
        RtlCopyMemory(pLoopback->Buffer + writePos, pLoopback->SpkConvertBufA + srcOffset, chunk);
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
                RtlCopyMemory(micBuf + micPos, pLoopback->SpkConvertBufA + src, chunk);
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
    const ULONG nCh = LB_INTERNAL_CHANNELS;
    ULONG maxFrames = pLoopback->ConvertBufSize / (nCh * sizeof(INT32));
    if (internalFrames > maxFrames)
        internalFrames = maxFrames;

    // Step 1: Read internal format bytes from ring buffer
    ULONG internalBytes = internalFrames * LB_INTERNAL_BLOCKALIGN;
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
        RtlCopyMemory(pLoopback->MicConvertBufA + dstOff, pLoopback->Buffer + readPos, chunk);
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

    // Step 2: Convert internal packed bytes -> N-channel INT32
    LB_FORMAT internalFmt = { internalRate, LB_INTERNAL_BITS, LB_INTERNAL_CHANNELS, LB_INTERNAL_BLOCKALIGN, FALSE };
    ULONG readFrames = toRead / LB_INTERNAL_BLOCKALIGN;
    // Clamp to scratch buffer capacity: ConvertToInternal outputs readFrames * nCh * sizeof(INT32)
    if (readFrames > maxFrames)
        readFrames = maxFrames;

    INT32* intSamples = (INT32*)pLoopback->MicConvertBufB;
    ULONG intFrames = ConvertToInternal(pLoopback->MicConvertBufA, readFrames, &internalFmt, intSamples);

    // Step 3: SRC from internal rate to mic rate
    INT32* srcOutput = (INT32*)pLoopback->MicConvertBufA;

    // Consume reset flag (set by IOCTL/RegisterFormat, safe to reset here in DPC context)
    if (pLoopback->MicSrcResetPending)
    {
        RtlZeroMemory(&pLoopback->MicSrcState, sizeof(LB_SRC_STATE));
        pLoopback->MicSrcResetPending = FALSE;
    }

    // Clamp output frames to scratch buffer capacity
    ULONG maxSrcOut = min(outFrames, maxFrames);
    ULONG srcFrames = SrcConvert(
        intSamples, intFrames,
        internalRate, micFmt.SampleRate,
        srcOutput, maxSrcOut,
        &pLoopback->MicSrcState
    );

    // Step 4: Convert N-channel INT32 -> Mic output format
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

    // Signal DPCs to reset their own SRC states (avoids race with DPC using SrcState)
    pLoopback->SpeakerSrcResetPending = TRUE;
    pLoopback->MicSrcResetPending = TRUE;

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

    // Signal DPCs to reset their own SRC states (avoids race with DPC using SrcState)
    pLoopback->SpeakerSrcResetPending = TRUE;
    pLoopback->MicSrcResetPending = TRUE;

    // Update format match
    UpdateFormatMatch(pLoopback);

    KeReleaseSpinLock(&pLoopback->SpinLock, oldIrql);

    if (oldBuf)
        ExFreePoolWithTag(oldBuf, 'pooL');

    return STATUS_SUCCESS;
}
