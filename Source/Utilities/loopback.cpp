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
C_ASSERT(sizeof(LB_SRC_STATE) == 528);
C_ASSERT(sizeof(LOOPBACK_BUFFER) == 1248);
C_ASSERT(FIELD_OFFSET(LOOPBACK_BUFFER, InternalRate) == 0x4CC);
C_ASSERT(FIELD_OFFSET(LOOPBACK_BUFFER, MaxLatencyMs) == 0x4D0);
C_ASSERT(FIELD_OFFSET(LOOPBACK_BUFFER, InternalChannels) == 0x4D4);
C_ASSERT(FIELD_OFFSET(LOOPBACK_BUFFER, InternalBlockAlign) == 0x4D8);

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
    INT32*          outSamples,
    ULONG           nInternalCh
)
{
    const ULONG nCh = nInternalCh;
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
    BYTE*           outData,
    ULONG           nInternalCh
)
{
    const ULONG nCh = nInternalCh;
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
            // Zero remaining output channels beyond internal count
            for (ULONG ch = chToCopy; ch < outFmt->nChannels; ch++)
            {
                WriteSample(p + ch * bps, 0, outFmt);
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
    LB_SRC_STATE*   state,
    ULONG           nInternalCh
)
{
    const ULONG nCh = nInternalCh;
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
static BOOLEAN FormatMatchesInternal(const LB_FORMAT* fmt, ULONG internalRate, ULONG internalChannels)
{
    return (fmt->SampleRate == internalRate &&
            fmt->BitsPerSample == LB_INTERNAL_BITS &&
            fmt->nChannels == internalChannels &&
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
            FormatMatchesInternal(&pLB->SpeakerFormat, pLB->InternalRate, pLB->InternalChannels);
    }
    else if (pLB->SpeakerActive)
    {
        pLB->FormatMatch = FormatMatchesInternal(&pLB->SpeakerFormat, pLB->InternalRate, pLB->InternalChannels);
    }
    else if (pLB->MicActive)
    {
        pLB->FormatMatch = FormatMatchesInternal(&pLB->MicFormat, pLB->InternalRate, pLB->InternalChannels);
    }
    else
    {
        pLB->FormatMatch = TRUE; // no streams, doesn't matter
    }
}

//=============================================================================
// Calculate buffer size from rate and latency
//=============================================================================
static ULONG CalcBufferSize(ULONG internalRate, ULONG latencyMs, ULONG blockAlign)
{
    // 4 seconds of internal format buffer for ring stability
    // (latencyMs controls the conceptual latency, but ring is larger for safety)
    ULONG minSize = internalRate * blockAlign * latencyMs / 1000;
    // Round up to block alignment
    minSize = ((minSize + blockAlign - 1) / blockAlign) * blockAlign;
    // At least 4 seconds for ring stability
    ULONG safeSize = internalRate * blockAlign * 4;
    return max(minSize, safeSize);
}

//=============================================================================
// LoopbackInit
//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS LoopbackInit(PLOOPBACK_BUFFER pLoopback, ULONG internalChannels)
{
    PAGED_CODE();

    // Validate channel count (runtime, 8 or 16)
    if (internalChannels != 8 && internalChannels != 16)
        internalChannels = LB_INTERNAL_CHANNELS;

    // Set defaults
    pLoopback->InternalRate = LB_DEFAULT_INTERNAL_RATE;
    pLoopback->MaxLatencyMs = LB_DEFAULT_LATENCY_MS;
    pLoopback->InternalChannels = internalChannels;
    pLoopback->InternalBlockAlign = (LB_INTERNAL_BITS / 8) * internalChannels;

    pLoopback->BufferSize = CalcBufferSize(pLoopback->InternalRate, pLoopback->MaxLatencyMs, pLoopback->InternalBlockAlign);
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
    ULONG intCh;
    ULONG intBA;

    // Snapshot format + channel config under spinlock
    KIRQL oldIrql;
    KeAcquireSpinLock(&pLoopback->SpinLock, &oldIrql);
    spkFmt = pLoopback->SpeakerFormat;
    internalRate = pLoopback->InternalRate;
    intCh = pLoopback->InternalChannels;
    intBA = pLoopback->InternalBlockAlign;
    KeReleaseSpinLock(&pLoopback->SpinLock, oldIrql);

    if (spkFmt.nBlockAlign == 0 || spkFmt.SampleRate == 0)
        return;
    if (intCh == 0 || intBA == 0)
        return;

    ULONG inFrames = ByteCount / spkFmt.nBlockAlign;
    if (inFrames == 0)
        return;

    // Step 1: Convert input to internal N-channel INT32 samples (24-bit range)
    const ULONG nCh = intCh;
    ULONG maxIntFrames = pLoopback->ConvertBufSize / (nCh * sizeof(INT32));
    if (inFrames > maxIntFrames)
        inFrames = maxIntFrames;

    INT32* intSamples = (INT32*)pLoopback->SpkConvertBufA;
    ULONG intFrames = ConvertToInternal(Data, inFrames, &spkFmt, intSamples, nCh);

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
        &pLoopback->SpeakerSrcState,
        nCh
    );

    if (srcFrames == 0)
        return;

    // Step 3: Convert N-channel INT32 -> internal packed format
    LB_FORMAT internalFmt = { internalRate, LB_INTERNAL_BITS, nCh, intBA, FALSE };
    ULONG packedBytes = ConvertFromInternal(srcOutput, srcFrames, &internalFmt, pLoopback->SpkConvertBufA, nCh);

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
        if (FormatMatchesInternal(&pLoopback->MicFormat, internalRate, pLoopback->InternalChannels))
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
    ULONG intCh;
    ULONG intBA;

    KeAcquireSpinLock(&pLoopback->SpinLock, &oldIrql);
    micFmt = pLoopback->MicFormat;
    internalRate = pLoopback->InternalRate;
    intCh = pLoopback->InternalChannels;
    intBA = pLoopback->InternalBlockAlign;
    KeReleaseSpinLock(&pLoopback->SpinLock, oldIrql);

    if (micFmt.nBlockAlign == 0 || micFmt.SampleRate == 0)
    {
        RtlZeroMemory(Data, ByteCount);
        return;
    }
    if (intCh == 0 || intBA == 0)
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
    ULONG internalFrames = (ULONG)(((ULONGLONG)outFrames * internalRate + micFmt.SampleRate - 1) / micFmt.SampleRate);
    internalFrames += 1;

    // Limit to scratch buffer capacity
    const ULONG nCh = intCh;
    ULONG maxFrames = pLoopback->ConvertBufSize / (nCh * sizeof(INT32));
    if (internalFrames > maxFrames)
        internalFrames = maxFrames;

    // Step 1: Read internal format bytes from ring buffer
    ULONG internalBytes = internalFrames * intBA;
    if (internalBytes > pLoopback->ConvertBufSize)
        internalBytes = pLoopback->ConvertBufSize;

    KeAcquireSpinLock(&pLoopback->SpinLock, &oldIrql);

    ULONG bufSize = pLoopback->BufferSize;
    ULONG readPos = pLoopback->ReadPos;
    ULONG available = pLoopback->DataCount;
    ULONG toRead = min(internalBytes, available);
    // Align to internal block
    toRead = (toRead / intBA) * intBA;

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
    LB_FORMAT internalFmt = { internalRate, LB_INTERNAL_BITS, nCh, intBA, FALSE };
    ULONG readFrames = toRead / intBA;
    // Clamp to scratch buffer capacity
    if (readFrames > maxFrames)
        readFrames = maxFrames;

    INT32* intSamples = (INT32*)pLoopback->MicConvertBufB;
    ULONG intFrames = ConvertToInternal(pLoopback->MicConvertBufA, readFrames, &internalFmt, intSamples, nCh);

    // Step 3: SRC from internal rate to mic rate
    INT32* srcOutput = (INT32*)pLoopback->MicConvertBufA;

    // Consume reset flag
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
        &pLoopback->MicSrcState,
        nCh
    );

    // Step 4: Convert N-channel INT32 -> Mic output format
    if (srcFrames > 0)
    {
        ULONG written = ConvertFromInternal(srcOutput, srcFrames, &micFmt, Data, nCh);
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

    ULONG newSize = CalcBufferSize(pLoopback->InternalRate, newLatencyMs, pLoopback->InternalBlockAlign);

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
    ULONG newSize = CalcBufferSize(newRate, pLoopback->MaxLatencyMs, pLoopback->InternalBlockAlign);

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


//=============================================================================
//=============================================================================
//
//  FRAME_PIPE Implementation — Fixed Frame Pipe (VB-style transport)
//  Phase 1: Core ring + write/read + diagnostics
//
//=============================================================================
//=============================================================================

// Pool tag for Frame Pipe allocations
#define FP_POOL_TAG  'ePpF'     // "FpPe"

// Global instances
FRAME_PIPE g_CableAPipe = { 0 };
FRAME_PIPE g_CableBPipe = { 0 };

//=============================================================================
// FramePipeInit — Allocate ring and scratch buffers (PASSIVE_LEVEL)
//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS FramePipeInit(
    PFRAME_PIPE     pPipe,
    ULONG           pipeSampleRate,
    ULONG           pipeChannels,
    ULONG           targetFillFrames)
{
    PAGED_CODE();

    if (!pPipe || pipeChannels == 0 || pipeChannels > FP_MAX_CHANNELS ||
        pipeSampleRate == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Guard against re-init: clean up existing allocations first
    if (pPipe->Initialized)
        FramePipeCleanup(pPipe);

    // Clamp target fill
    if (targetFillFrames < FP_MIN_TARGET_FILL)
        targetFillFrames = FP_MIN_TARGET_FILL;
    if (targetFillFrames > FP_MAX_TARGET_FILL)
        targetFillFrames = FP_MAX_TARGET_FILL;

    ULONG capacityFrames = targetFillFrames * FP_CAPACITY_MULTIPLIER;
    ULONG ringBytes = capacityFrames * pipeChannels * sizeof(INT32);
    ULONG scratchBytes = targetFillFrames * pipeChannels * sizeof(INT32);

    // Allocate ring buffer (NonPagedPoolNx — used at DISPATCH_LEVEL)
    INT32* ringBuf = (INT32*)ExAllocatePool2(POOL_FLAG_NON_PAGED, ringBytes, FP_POOL_TAG);
    if (!ringBuf)
        return STATUS_INSUFFICIENT_RESOURCES;

    // Allocate scratch DMA buffer
    BYTE* scratchDma = (BYTE*)ExAllocatePool2(POOL_FLAG_NON_PAGED, scratchBytes, FP_POOL_TAG);
    if (!scratchDma)
    {
        ExFreePoolWithTag(ringBuf, FP_POOL_TAG);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Allocate speaker scratch buffer
    INT32* scratchSpk = (INT32*)ExAllocatePool2(POOL_FLAG_NON_PAGED, scratchBytes, FP_POOL_TAG);
    if (!scratchSpk)
    {
        ExFreePoolWithTag(scratchDma, FP_POOL_TAG);
        ExFreePoolWithTag(ringBuf, FP_POOL_TAG);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Allocate mic scratch buffer
    INT32* scratchMic = (INT32*)ExAllocatePool2(POOL_FLAG_NON_PAGED, scratchBytes, FP_POOL_TAG);
    if (!scratchMic)
    {
        ExFreePoolWithTag(scratchSpk, FP_POOL_TAG);
        ExFreePoolWithTag(scratchDma, FP_POOL_TAG);
        ExFreePoolWithTag(ringBuf, FP_POOL_TAG);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Zero all buffers
    RtlZeroMemory(ringBuf, ringBytes);
    RtlZeroMemory(scratchDma, scratchBytes);
    RtlZeroMemory(scratchSpk, scratchBytes);
    RtlZeroMemory(scratchMic, scratchBytes);

    // Zero the pipe struct, then fill fields
    RtlZeroMemory(pPipe, sizeof(FRAME_PIPE));

    pPipe->RingBuffer           = ringBuf;
    pPipe->PipeChannels         = pipeChannels;
    pPipe->WriteFrame           = 0;
    pPipe->ReadFrame            = 0;
    pPipe->FillFrames           = 0;
    KeInitializeSpinLock(&pPipe->PipeLock);

    pPipe->PipeSampleRate       = pipeSampleRate;
    pPipe->PipeBitsPerSample    = 32;
    pPipe->PipeBlockAlign       = pipeChannels * sizeof(INT32);

    pPipe->TargetFillFrames     = targetFillFrames;
    pPipe->CapacityFrames       = capacityFrames;
    // No startup threshold — Mic reads immediately.
    // This maximizes buffer available during Speaker STOP gaps.
    // Steady-state fill will be near capacity (VB-Cable behavior).
    pPipe->StartThresholdFrames = 0;
    pPipe->StartPhaseComplete   = TRUE;

    pPipe->DropCount            = 0;
    pPipe->UnderrunCount        = 0;
    pPipe->ActiveRenderCount    = 0;

    // Phase 1: per-direction pump counters — all zero at init.
    // Phase 1 has no writer for any of these fields; Phase 3 is the first.
    pPipe->RenderGatedSkipCount             = 0;
    pPipe->RenderOverJumpCount              = 0;
    pPipe->RenderFramesProcessedTotal       = 0;
    pPipe->RenderPumpInvocationCount        = 0;
    pPipe->RenderPumpShadowDivergenceCount  = 0;
    pPipe->RenderPumpFeatureFlags           = 0;

    pPipe->CaptureGatedSkipCount            = 0;
    pPipe->CaptureOverJumpCount             = 0;
    pPipe->CaptureFramesProcessedTotal      = 0;
    pPipe->CapturePumpInvocationCount       = 0;
    pPipe->CapturePumpShadowDivergenceCount = 0;
    pPipe->CapturePumpFeatureFlags          = 0;

    // Phase 5: per-side drive counters. Monotonic; not touched by
    // FramePipeReset(). See loopback.h for the one-owner contract.
    pPipe->RenderPumpDriveCount             = 0;
    pPipe->RenderLegacyDriveCount           = 0;

    pPipe->ScratchDma           = scratchDma;
    pPipe->ScratchSpk           = scratchSpk;
    pPipe->ScratchMic           = scratchMic;
    pPipe->ScratchSizeBytes     = scratchBytes;

    pPipe->Initialized          = TRUE;

    return STATUS_SUCCESS;
}
#pragma code_seg()

//=============================================================================
// FramePipeCleanup — Free all buffers (PASSIVE_LEVEL)
//=============================================================================
#pragma code_seg("PAGE")
VOID FramePipeCleanup(PFRAME_PIPE pPipe)
{
    PAGED_CODE();

    if (!pPipe)
        return;

    if (pPipe->ScratchSpk)
    {
        ExFreePoolWithTag(pPipe->ScratchSpk, FP_POOL_TAG);
        pPipe->ScratchSpk = NULL;
    }
    if (pPipe->ScratchMic)
    {
        ExFreePoolWithTag(pPipe->ScratchMic, FP_POOL_TAG);
        pPipe->ScratchMic = NULL;
    }
    if (pPipe->ScratchDma)
    {
        ExFreePoolWithTag(pPipe->ScratchDma, FP_POOL_TAG);
        pPipe->ScratchDma = NULL;
    }
    if (pPipe->RingBuffer)
    {
        ExFreePoolWithTag(pPipe->RingBuffer, FP_POOL_TAG);
        pPipe->RingBuffer = NULL;
    }

    pPipe->Initialized = FALSE;
}
#pragma code_seg()

//=============================================================================
// FramePipeWriteFrames — Speaker DPC writes INT32 frames into pipe
//
// Returns: frameCount on success, 0 on reject.
// Overflow: entire write is rejected (all-or-nothing), DropCount incremented.
// Min gate (FP_MIN_GATE_FRAMES) is NOT enforced here — that's DPC policy.
// Called at DISPATCH_LEVEL.
//=============================================================================
ULONG FramePipeWriteFrames(
    PFRAME_PIPE     pPipe,
    const INT32*    srcFrames,
    ULONG           frameCount)
{
    if (!pPipe || !pPipe->Initialized || !srcFrames || frameCount == 0)
        return 0;

    KIRQL oldIrql;
    KeAcquireSpinLock(&pPipe->PipeLock, &oldIrql);

    ULONG capacity = pPipe->CapacityFrames;
    ULONG channels = pPipe->PipeChannels;
    ULONG writeIdx = pPipe->WriteFrame;
    ULONG fill     = pPipe->FillFrames;

    ULONG freeFrames = capacity - fill;

    // Hard reject: entire write rejected if not enough space.
    if (frameCount > freeFrames)
    {
        pPipe->DropCount += frameCount;
        KeReleaseSpinLock(&pPipe->PipeLock, oldIrql);
        return 0;
    }

    ULONG samplesPerFrame = channels;
    ULONG framesBeforeWrap = capacity - writeIdx;

    if (frameCount <= framesBeforeWrap)
    {
        RtlCopyMemory(
            &pPipe->RingBuffer[writeIdx * samplesPerFrame],
            srcFrames,
            frameCount * samplesPerFrame * sizeof(INT32));
    }
    else
    {
        ULONG firstFrames = framesBeforeWrap;
        ULONG secondFrames = frameCount - firstFrames;

        RtlCopyMemory(
            &pPipe->RingBuffer[writeIdx * samplesPerFrame],
            srcFrames,
            firstFrames * samplesPerFrame * sizeof(INT32));

        RtlCopyMemory(
            &pPipe->RingBuffer[0],
            &srcFrames[firstFrames * samplesPerFrame],
            secondFrames * samplesPerFrame * sizeof(INT32));
    }

    pPipe->WriteFrame = (writeIdx + frameCount) % capacity;
    pPipe->FillFrames = fill + frameCount;

    KeReleaseSpinLock(&pPipe->PipeLock, oldIrql);

    return frameCount;
}

//=============================================================================
// FramePipePrefillSilence — Inject silence into ring up to TargetFillFrames
//
// Purpose: give the reader immediate headroom on speaker RUN transition so
// Phone Link's ~1ms WASAPI pulls don't starve while the writer ramps.
// Called from SetState(KSSTATE_RUN) speaker branch after RegisterFormat.
// No-op if the ring is not empty — we respect persisted data across
// STOP/RUN gaps and only cushion fresh openings.
//=============================================================================
VOID FramePipePrefillSilence(
    PFRAME_PIPE     pPipe)
{
    if (!pPipe || !pPipe->Initialized || !pPipe->RingBuffer)
        return;

    KIRQL oldIrql;
    KeAcquireSpinLock(&pPipe->PipeLock, &oldIrql);

    if (pPipe->FillFrames != 0)
    {
        KeReleaseSpinLock(&pPipe->PipeLock, oldIrql);
        return;
    }

    ULONG capacity = pPipe->CapacityFrames;
    ULONG channels = pPipe->PipeChannels;
    // Fixed 40 ms silence cushion regardless of TargetFillFrames.
    // Using TargetFillFrames would inject multi-second silence when
    // the pipe is configured with large headroom, which is audible as
    // a delay at the start of each speaker run.
    ULONG toFill = (pPipe->PipeSampleRate * FP_STARTUP_HEADROOM_MS) / 1000;
    if (toFill == 0)
        toFill = FP_MIN_TARGET_FILL;
    if (toFill > capacity - 1)
        toFill = capacity - 1;

    // Zero the prefill region — ring may contain stale samples from a
    // previous session even though FillFrames == 0 marks it empty.
    RtlZeroMemory(
        pPipe->RingBuffer,
        toFill * channels * sizeof(INT32));

    pPipe->ReadFrame  = 0;
    pPipe->WriteFrame = toFill % capacity;
    pPipe->FillFrames = toFill;

    ULONG prefillFrames = toFill;
    KeReleaseSpinLock(&pPipe->PipeLock, oldIrql);

    DbgPrint("[FP_PREFILL] pipe=%p frames=%u\n", pPipe, prefillFrames);
}

//=============================================================================
// FramePipeReadFrames — Mic DPC reads INT32 frames from pipe
//
// Returns: number of frames read from ring (0 if silence-filled).
// Underrun: output is zero-filled, UnderrunCount incremented (frames).
// Startup: silence until FillFrames reaches StartThresholdFrames.
// Called at DISPATCH_LEVEL.
//=============================================================================
ULONG FramePipeReadFrames(
    PFRAME_PIPE     pPipe,
    INT32*          dstFrames,
    ULONG           frameCount)
{
    if (!pPipe || !pPipe->Initialized || !dstFrames || frameCount == 0)
        return 0;

    LARGE_INTEGER qpc = KeQueryPerformanceCounter(NULL);

    KIRQL oldIrql;
    KeAcquireSpinLock(&pPipe->PipeLock, &oldIrql);

    ULONG capacity = pPipe->CapacityFrames;
    ULONG channels = pPipe->PipeChannels;
    ULONG readIdx  = pPipe->ReadFrame;
    ULONG fill     = pPipe->FillFrames;
    ULONG samplesPerFrame = channels;
    ULONG outputSamples = frameCount * samplesPerFrame;

    // Case 1: Startup phase — not yet filled to threshold
    if (!pPipe->StartPhaseComplete && fill < pPipe->StartThresholdFrames)
    {
        RtlZeroMemory(dstFrames, outputSamples * sizeof(INT32));
        KeReleaseSpinLock(&pPipe->PipeLock, oldIrql);
        DbgPrint("[FP_READ] pipe=%p qpc=%lld req=%u fill=%u STARTUP\n",
                 pPipe, qpc.QuadPart, frameCount, fill);
        return 0;  // startup silence, not counted as underrun
    }

    // We've reached threshold at least once
    pPipe->StartPhaseComplete = TRUE;

    // Case 2: Empty (post-startup)
    if (fill == 0)
    {
        RtlZeroMemory(dstFrames, outputSamples * sizeof(INT32));
        pPipe->UnderrunCount += frameCount;
        KeReleaseSpinLock(&pPipe->PipeLock, oldIrql);
        DbgPrint("[FP_READ] pipe=%p qpc=%lld req=%u fill=0 UNDERRUN\n",
                 pPipe, qpc.QuadPart, frameCount);
        return 0;
    }

    // Case 3 & 4: Partial or full read
    ULONG framesToRead = (fill < frameCount) ? fill : frameCount;
    ULONG framesBeforeWrap = capacity - readIdx;

    if (framesToRead <= framesBeforeWrap)
    {
        RtlCopyMemory(
            dstFrames,
            &pPipe->RingBuffer[readIdx * samplesPerFrame],
            framesToRead * samplesPerFrame * sizeof(INT32));
    }
    else
    {
        ULONG firstFrames = framesBeforeWrap;
        ULONG secondFrames = framesToRead - firstFrames;

        RtlCopyMemory(
            dstFrames,
            &pPipe->RingBuffer[readIdx * samplesPerFrame],
            firstFrames * samplesPerFrame * sizeof(INT32));

        RtlCopyMemory(
            &dstFrames[firstFrames * samplesPerFrame],
            &pPipe->RingBuffer[0],
            secondFrames * samplesPerFrame * sizeof(INT32));
    }

    pPipe->ReadFrame = (readIdx + framesToRead) % capacity;
    pPipe->FillFrames = fill - framesToRead;

    // Partial read: zero-fill remainder
    if (framesToRead < frameCount)
    {
        ULONG remainFrames = frameCount - framesToRead;
        RtlZeroMemory(
            &dstFrames[framesToRead * samplesPerFrame],
            remainFrames * samplesPerFrame * sizeof(INT32));
        pPipe->UnderrunCount += remainFrames;
    }

    ULONG readOut = framesToRead;
    ULONG fillAfter = pPipe->FillFrames;
    BOOLEAN partial = (framesToRead < frameCount);
    KeReleaseSpinLock(&pPipe->PipeLock, oldIrql);

    DbgPrint("[FP_READ] pipe=%p qpc=%lld req=%u fill=%u out=%u fillAfter=%u %s\n",
             pPipe, qpc.QuadPart, frameCount, fill, readOut, fillAfter,
             partial ? "PARTIAL" : "FULL");

    return framesToRead;
}

//=============================================================================
// FramePipeReset — Clear ring state (PASSIVE_LEVEL, after KeFlushQueuedDpcs)
//=============================================================================
#pragma code_seg("PAGE")
VOID FramePipeReset(PFRAME_PIPE pPipe)
{
    PAGED_CODE();

    if (!pPipe || !pPipe->Initialized)
        return;

    KIRQL oldIrql;
    KeAcquireSpinLock(&pPipe->PipeLock, &oldIrql);

    pPipe->WriteFrame         = 0;
    pPipe->ReadFrame          = 0;
    pPipe->FillFrames         = 0;
    pPipe->StartPhaseComplete = FALSE;
    pPipe->DropCount          = 0;
    pPipe->UnderrunCount      = 0;

    // Phase 1: per-session pump counters reset on ring reset.
    // Mirrors VB FUN_1400039ac — per-session fields cleared so the user
    // sees a clean slate on each RUN, while monotonic run-totals survive
    // so Phase 3's shadow-window divergence ratio stays measurable across
    // RUN -> PAUSE -> RUN cycles.
    pPipe->RenderGatedSkipCount  = 0;
    pPipe->RenderOverJumpCount   = 0;
    pPipe->CaptureGatedSkipCount = 0;
    pPipe->CaptureOverJumpCount  = 0;

    // Do NOT reset on session boundary (monotonic):
    //   Render/CaptureFramesProcessedTotal
    //   Render/CapturePumpInvocationCount
    //   Render/CapturePumpShadowDivergenceCount
    //   Render/CapturePumpFeatureFlags

    // Zero the ring
    if (pPipe->RingBuffer)
    {
        RtlZeroMemory(
            pPipe->RingBuffer,
            pPipe->CapacityFrames * pPipe->PipeChannels * sizeof(INT32));
    }

    KeReleaseSpinLock(&pPipe->PipeLock, oldIrql);
}
#pragma code_seg()

//=============================================================================
// FramePipeGetFillFrames — Query current fill level (any IRQL)
//=============================================================================
ULONG FramePipeGetFillFrames(PFRAME_PIPE pPipe)
{
    if (!pPipe || !pPipe->Initialized)
        return 0;

    return pPipe->FillFrames;
}


//=============================================================================
//=============================================================================
//
//  FRAME_PIPE Phase 2: Format Registration + INT32 Normalization +
//                      Channel Mapping + DMA Batch Conversion
//
//=============================================================================
//=============================================================================

//=============================================================================
// INT32 ~19-bit normalization helpers (all integer-only, DISPATCH safe)
//
// Range: [-262144, +262143] (~18-bit + sign)
// 16-bit round-trip is lossless: (s16 << 3) >> 3 == s16
// 24-bit loses lower 5 bits (same trade-off as VB-Cable)
//=============================================================================

static __forceinline INT32 FpNorm16(INT16 s)
{
    return (INT32)s << 3;
}

static __forceinline INT16 FpDenorm16(INT32 v)
{
    return (INT16)(v >> 3);
}

static __forceinline INT32 FpNorm24(const BYTE* p)
{
    // Read 24-bit packed little-endian, sign-extend, shift right 5
    INT32 s24 = (INT32)(p[0] | (p[1] << 8) | ((INT8)p[2] << 16));
    return s24 >> 5;
}

static __forceinline VOID FpDenorm24(INT32 v, BYTE* p)
{
    INT32 s24 = v << 5;
    p[0] = (BYTE)(s24 & 0xFF);
    p[1] = (BYTE)((s24 >> 8) & 0xFF);
    p[2] = (BYTE)((s24 >> 16) & 0xFF);
}

// Phase 2 (G9): direct copy matches VB-Cable's observed 32-bit PCM
// behavior. The pipe carries the application's raw 32-bit samples.
// Safe on cable-only single-writer / single-reader path; see the
// Phase 2 proposal (results/phase2_edit_proposal.md §2.3) for the
// headroom caveat if mixing is ever added in a later phase.
static __forceinline INT32 FpNorm32i(INT32 s)
{
    return s;
}

// Phase 2 (G9 mirror): direct copy, symmetric with FpNorm32i.
static __forceinline INT32 FpDenorm32i(INT32 v)
{
    return v;
}

// Phase 2 (G10): direct bit cast matches VB-Cable's observed 32-bit
// float behavior. The pipe carries the raw IEEE-754 bit pattern
// reinterpreted as INT32. Consumer (FpDenormFloat) casts it back.
// Safe on single-writer / single-reader paired cable.
static __forceinline INT32 FpNormFloat(UINT32 bits)
{
    return (INT32)bits;
}

// Phase 2 (G10 mirror): direct bit cast, symmetric with FpNormFloat.
static __forceinline UINT32 FpDenormFloat(INT32 v)
{
    return (UINT32)v;
}

//=============================================================================
// FramePipeRegisterFormat — Stream announces its format (PASSIVE_LEVEL)
//=============================================================================
#pragma code_seg("PAGE")
VOID FramePipeRegisterFormat(
    PFRAME_PIPE     pPipe,
    BOOLEAN         isSpeaker,
    ULONG           sampleRate,
    ULONG           bitsPerSample,
    ULONG           nChannels,
    ULONG           nBlockAlign,
    BOOLEAN         isFloat)
{
    PAGED_CODE();

    if (!pPipe || !pPipe->Initialized)
        return;

    KIRQL oldIrql;
    KeAcquireSpinLock(&pPipe->PipeLock, &oldIrql);

    BOOLEAN wasActive = isSpeaker ? pPipe->SpeakerActive : pPipe->MicActive;

    if (isSpeaker)
    {
        pPipe->SpeakerSampleRate    = sampleRate;
        pPipe->SpeakerBitsPerSample = bitsPerSample;
        pPipe->SpeakerChannels      = nChannels;
        pPipe->SpeakerBlockAlign    = nBlockAlign;
        pPipe->SpeakerIsFloat       = isFloat;
        pPipe->SpeakerActive        = TRUE;
        pPipe->SpeakerSameRate      = (sampleRate == pPipe->PipeSampleRate);
        // Do NOT reset pipe or StartPhaseComplete on Speaker re-register.
        // Ring data and state persist across STOP/RUN gaps (VB-Cable behavior).
    }
    else
    {
        pPipe->MicSampleRate        = sampleRate;
        pPipe->MicBitsPerSample     = bitsPerSample;
        pPipe->MicChannels          = nChannels;
        pPipe->MicBlockAlign        = nBlockAlign;
        pPipe->MicIsFloat           = isFloat;
        pPipe->MicActive            = TRUE;
        pPipe->MicSameRate          = (sampleRate == pPipe->PipeSampleRate);

        // Re-arm startup gate on every fresh mic open (!wasActive).
        // FramePipeInit sets StartPhaseComplete=TRUE so the first call
        // after a reset skips the gate entirely — that is precisely the
        // bug observed in phase5c_instr_ed23271 where Cable B mic's first
        // ~7300 reads all returned UNDERRUN zero-fill. Rearming here with
        // threshold = FP_STARTUP_HEADROOM_MS ensures each new call waits
        // for the speaker-side prefill cushion to materialize before
        // delivering data to the reader.
        if (!wasActive)
        {
            ULONG threshold = (pPipe->PipeSampleRate * FP_STARTUP_HEADROOM_MS) / 1000;
            if (threshold == 0)
                threshold = FP_MIN_TARGET_FILL;
            pPipe->StartThresholdFrames = threshold;
            pPipe->StartPhaseComplete   = FALSE;
        }
    }

    // Only increment if transitioning from inactive to active
    if (isSpeaker && !wasActive)
        pPipe->ActiveRenderCount++;

    KeReleaseSpinLock(&pPipe->PipeLock, oldIrql);

    {
        const char* id = (pPipe == &g_CableAPipe) ? "A" : (pPipe == &g_CableBPipe) ? "B" : "?";
        DbgPrint("AO_PIPE[%s]: RegisterFormat %s rate=%u bps=%u ch=%u align=%u float=%d SameRate=%d PipeRate=%u PipeCh=%u\n",
            id, isSpeaker ? "SPK" : "MIC", sampleRate, bitsPerSample, nChannels, nBlockAlign,
            (int)isFloat, isSpeaker ? (int)pPipe->SpeakerSameRate : (int)pPipe->MicSameRate,
            pPipe->PipeSampleRate, pPipe->PipeChannels);
    }
}
#pragma code_seg()

//=============================================================================
// FramePipeUnregisterFormat — Stream leaving (PASSIVE_LEVEL)
//=============================================================================
#pragma code_seg("PAGE")
VOID FramePipeUnregisterFormat(
    PFRAME_PIPE     pPipe,
    BOOLEAN         isSpeaker)
{
    PAGED_CODE();

    if (!pPipe || !pPipe->Initialized)
        return;

    KIRQL oldIrql;
    KeAcquireSpinLock(&pPipe->PipeLock, &oldIrql);

    if (isSpeaker)
    {
        pPipe->SpeakerActive = FALSE;
        if (pPipe->ActiveRenderCount > 0)
            pPipe->ActiveRenderCount--;
    }
    else
    {
        pPipe->MicActive = FALSE;
    }

    // Both stopped → reset pipe
    BOOLEAN bothStopped = !pPipe->SpeakerActive && !pPipe->MicActive;

    KeReleaseSpinLock(&pPipe->PipeLock, oldIrql);

    {
        const char* id = (pPipe == &g_CableAPipe) ? "A" : (pPipe == &g_CableBPipe) ? "B" : "?";
        DbgPrint("AO_PIPE[%s]: UnregisterFormat %s bothStopped=%d Fill=%u Drop=%u Underrun=%u\n",
            id, isSpeaker ? "SPK" : "MIC", (int)bothStopped,
            pPipe->FillFrames, pPipe->DropCount, pPipe->UnderrunCount);
    }

    if (bothStopped)
        FramePipeReset(pPipe);
}
#pragma code_seg()

//=============================================================================
// FramePipeWriteFromDma — Speaker DPC batch: DMA bytes → normalize →
//                         channel map → pipe write (DISPATCH_LEVEL)
//
// Returns frames written (0 = rejected or error).
// Caller loops over DMA wrap boundary; each call gets contiguous bytes.
//=============================================================================
ULONG FramePipeWriteFromDma(
    PFRAME_PIPE     pPipe,
    const BYTE*     dmaData,
    ULONG           byteCount)
{
    if (!pPipe || !pPipe->Initialized || !pPipe->SpeakerActive ||
        !dmaData || byteCount == 0)
    {
        return 0;
    }

    // Fail-closed: rate mismatch requires SRC (Phase 3). Drop until then.
    if (!pPipe->SpeakerSameRate)
    {
        ULONG frameCount = byteCount / pPipe->SpeakerBlockAlign;
        if (frameCount > 0)
            pPipe->DropCount += frameCount;
        {
            const char* id = (pPipe == &g_CableAPipe) ? "A" : (pPipe == &g_CableBPipe) ? "B" : "?";
            DbgPrint("AO_PIPE[%s] WR: DROP rate mismatch SpkRate=%u PipeRate=%u frames=%u\n",
                id, pPipe->SpeakerSampleRate, pPipe->PipeSampleRate, frameCount);
        }
        return 0;
    }

    ULONG spkBlockAlign = pPipe->SpeakerBlockAlign;
    ULONG totalFrames = byteCount / spkBlockAlign;
    if (totalFrames == 0)
        return 0;

    ULONG pipeChannels = pPipe->PipeChannels;
    ULONG spkChannels  = pPipe->SpeakerChannels;
    ULONG bps          = pPipe->SpeakerBitsPerSample;
    BOOLEAN isFloat    = pPipe->SpeakerIsFloat;
    INT32* scratch     = pPipe->ScratchSpk;
    ULONG maxFrames    = pPipe->ScratchSizeBytes / (pipeChannels * sizeof(INT32));
    ULONG copyChannels = (spkChannels < pipeChannels) ? spkChannels : pipeChannels;

    ULONG totalWritten = 0;
    ULONG offset = 0;

    while (offset < totalFrames)
    {
        ULONG chunk = totalFrames - offset;
        if (chunk > maxFrames)
            chunk = maxFrames;

        // Normalize + channel map into scratch
        for (ULONG f = 0; f < chunk; f++)
        {
            const BYTE* src = dmaData + (offset + f) * spkBlockAlign;
            INT32* dst = scratch + f * pipeChannels;

            for (ULONG ch = 0; ch < pipeChannels; ch++)
                dst[ch] = 0;

            if (bps == 16 && !isFloat)
            {
                const INT16* s16 = (const INT16*)src;
                for (ULONG ch = 0; ch < copyChannels; ch++)
                    dst[ch] = FpNorm16(s16[ch]);
            }
            else if (bps == 24 && !isFloat)
            {
                for (ULONG ch = 0; ch < copyChannels; ch++)
                    dst[ch] = FpNorm24(src + ch * 3);
            }
            else if (bps == 32 && isFloat)
            {
                const UINT32* bits = (const UINT32*)src;
                for (ULONG ch = 0; ch < copyChannels; ch++)
                    dst[ch] = FpNormFloat(bits[ch]);
            }
            else if (bps == 32 && !isFloat)
            {
                const INT32* s32 = (const INT32*)src;
                for (ULONG ch = 0; ch < copyChannels; ch++)
                    dst[ch] = FpNorm32i(s32[ch]);
            }
        }

        // Write chunk to pipe (all-or-nothing per chunk)
        ULONG written = FramePipeWriteFrames(pPipe, scratch, chunk);
        totalWritten += written;

        // If pipe rejected this chunk, stop (pipe full)
        if (written == 0)
            break;

        offset += chunk;
    }

    // Rate-limited diagnostics (~1s interval)
    pPipe->DbgWriteFrames += totalWritten;
    {
        LARGE_INTEGER now;
        now = KeQueryPerformanceCounter(NULL);
        LONGLONG elapsed = now.QuadPart - pPipe->DbgLastPrintQpc;
        if (elapsed > 10000000 || pPipe->DbgLastPrintQpc == 0)
        {
            {
                const char* id = (pPipe == &g_CableAPipe) ? "A" : (pPipe == &g_CableBPipe) ? "B" : "?";
                DbgPrint("AO_PIPE[%s] WR: Fill=%u/%u Drop=%u Underrun=%u Start=%d "
                    "ReqF=%u DoneF=%u RdF=%u SpkAct=%d MicAct=%d\n",
                    id, pPipe->FillFrames, pPipe->CapacityFrames,
                    pPipe->DropCount, pPipe->UnderrunCount,
                    (int)pPipe->StartPhaseComplete,
                    totalFrames, totalWritten, pPipe->DbgReadFrames,
                    (int)pPipe->SpeakerActive, (int)pPipe->MicActive);
            }
            pPipe->DbgWriteFrames = 0;
            pPipe->DbgReadFrames = 0;
            pPipe->DbgLastPrintQpc = now.QuadPart;
        }
    }

    return totalWritten;
}

//=============================================================================
// FramePipeReadToDma — Mic DPC batch: pipe read → channel map →
//                      denormalize → DMA bytes (DISPATCH_LEVEL)
//
// Always fills byteCount bytes. Underrun → silence (handled by ReadFrames).
//=============================================================================
VOID FramePipeReadToDma(
    PFRAME_PIPE     pPipe,
    BYTE*           dmaData,
    ULONG           byteCount)
{
    if (!pPipe || !pPipe->Initialized || !dmaData || byteCount == 0)
    {
        if (dmaData && byteCount > 0)
            RtlZeroMemory(dmaData, byteCount);
        return;
    }

    // Fail-closed: rate mismatch requires SRC (Phase 3). Output silence.
    if (!pPipe->MicSameRate || !pPipe->MicActive)
    {
        // Log once per second max
        {
            LARGE_INTEGER now = KeQueryPerformanceCounter(NULL);
            if (now.QuadPart - pPipe->DbgLastPrintQpc > 10000000 || pPipe->DbgLastPrintQpc == 0)
            {
                {
                    const char* id = (pPipe == &g_CableAPipe) ? "A" : (pPipe == &g_CableBPipe) ? "B" : "?";
                    DbgPrint("AO_PIPE[%s] RD: SILENCE (MicSameRate=%d MicActive=%d MicRate=%u PipeRate=%u)\n",
                        id, (int)pPipe->MicSameRate, (int)pPipe->MicActive,
                        pPipe->MicSampleRate, pPipe->PipeSampleRate);
                }
                pPipe->DbgLastPrintQpc = now.QuadPart;
            }
        }
        RtlZeroMemory(dmaData, byteCount);
        return;
    }

    ULONG micBlockAlign = pPipe->MicBlockAlign;
    ULONG totalFrames = byteCount / micBlockAlign;
    if (totalFrames == 0)
        return;

    ULONG pipeChannels = pPipe->PipeChannels;
    ULONG micChannels  = pPipe->MicChannels;
    ULONG bps          = pPipe->MicBitsPerSample;
    BOOLEAN isFloat    = pPipe->MicIsFloat;
    INT32* scratch     = pPipe->ScratchMic;
    ULONG maxFrames    = pPipe->ScratchSizeBytes / (pipeChannels * sizeof(INT32));
    ULONG copyChannels = (pipeChannels < micChannels) ? pipeChannels : micChannels;

    ULONG offset = 0;

    while (offset < totalFrames)
    {
        ULONG chunk = totalFrames - offset;
        if (chunk > maxFrames)
            chunk = maxFrames;

        // Read chunk from pipe (zero-fills on underrun/startup)
        FramePipeReadFrames(pPipe, scratch, chunk);

        // Channel map + denormalize into DMA
        for (ULONG f = 0; f < chunk; f++)
        {
            INT32* src = scratch + f * pipeChannels;
            BYTE* dst = dmaData + (offset + f) * micBlockAlign;

            if (bps == 16 && !isFloat)
            {
                INT16* d16 = (INT16*)dst;
                ULONG ch;
                for (ch = 0; ch < copyChannels; ch++)
                    d16[ch] = FpDenorm16(src[ch]);
                for (; ch < micChannels; ch++)
                    d16[ch] = 0;
            }
            else if (bps == 24 && !isFloat)
            {
                ULONG ch;
                for (ch = 0; ch < copyChannels; ch++)
                    FpDenorm24(src[ch], dst + ch * 3);
                for (; ch < micChannels; ch++)
                {
                    BYTE* p = dst + ch * 3;
                    p[0] = p[1] = p[2] = 0;
                }
            }
            else if (bps == 32 && isFloat)
            {
                UINT32* d32 = (UINT32*)dst;
                ULONG ch;
                for (ch = 0; ch < copyChannels; ch++)
                    d32[ch] = FpDenormFloat(src[ch]);
                for (; ch < micChannels; ch++)
                    d32[ch] = 0;
            }
            else if (bps == 32 && !isFloat)
            {
                INT32* d32 = (INT32*)dst;
                ULONG ch;
                for (ch = 0; ch < copyChannels; ch++)
                    d32[ch] = FpDenorm32i(src[ch]);
                for (; ch < micChannels; ch++)
                    d32[ch] = 0;
            }
        }

        offset += chunk;
    }

    pPipe->DbgReadFrames += totalFrames;

    // Handle trailing bytes that don't form a complete frame
    ULONG usedBytes = totalFrames * micBlockAlign;
    if (usedBytes < byteCount)
        RtlZeroMemory(dmaData + usedBytes, byteCount - usedBytes);
}
