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
//  FRAME_PIPE Implementation — Canonical V1 Cable Ring (DESIGN § 2.1 / § 2.2)
//
//  Phase 1 Step 1 (ADR-014 phase/1-int32-ring):
//   - struct shape replaced; 6 canonical APIs introduced (4 real + 2 stubs).
//   - 12 legacy FramePipe* functions kept as a compile-preserving shim
//     so untouched translation units (minwavertstream.cpp,
//     transport_engine.cpp) continue to link.
//   - 4 Phase 1 Step 0 cross-TU helpers kept as behavior-absent stubs
//     against the new shape (their legacy fields are gone).
//   - LOOPBACK_BUFFER above is a separate legacy ring; not touched here.
//
//  This commit does NOT claim runtime cable-transport correctness. Step 2
//  (write same-rate) and Step 3 (read same-rate) restore behavior on the
//  canonical API; Step 4-6 migrate callers off the shim; Phase 6 deletes
//  the shim layer.
//
//=============================================================================
//=============================================================================

// Pool tag for Frame Pipe allocations ("FpPe" reversed for little-endian)
#define FP_POOL_TAG  'ePpF'

// Default cap on FrameCapacityMax when registry value is unavailable.
// 192000 frames = ~4 s @ 48 kHz; same upper bound the legacy path used.
#define FP_DEFAULT_CAPACITY_MAX_FRAMES   192000

// Global instances (declared extern in loopback.h)
FRAME_PIPE g_CableAPipe = { 0 };
FRAME_PIPE g_CableBPipe = { 0 };

//=============================================================================
// AoRingAvailableFrames — wrap-corrected (WritePos - ReadPos)
//
// Defined here ahead of the lifetime helpers so the wrappers below can
// forward to it without a separate forward-declare. Any IRQL.
//=============================================================================
ULONG AoRingAvailableFrames(PFRAME_PIPE pipe)
{
    if (!pipe || !pipe->Data || pipe->WrapBound <= 0)
        return 0;

    KIRQL oldIrql;
    KeAcquireSpinLock(&pipe->Lock, &oldIrql);

    LONG wrap = pipe->WrapBound;
    LONG diff = pipe->WritePos - pipe->ReadPos;
    if (diff < 0)
        diff += wrap;

    KeReleaseSpinLock(&pipe->Lock, oldIrql);
    return (ULONG)diff;
}

//=============================================================================
// FramePipeInitCable — allocate ring, set fields, init lock (PASSIVE_LEVEL)
//
// DESIGN § 2.1: Data allocation = initialFrames * channels * sizeof(LONG)
// from non-paged pool. TargetLatencyFrames = WrapBound = initialFrames.
// FrameCapacityMax = max(initialFrames, registry-driven max).
//=============================================================================
#pragma code_seg("PAGE")
NTSTATUS FramePipeInitCable(
    PFRAME_PIPE  pipe,
    ULONG        internalRate,
    LONG         channels,
    LONG         initialFrames)
{
    PAGED_CODE();

    if (!pipe || internalRate == 0 ||
        channels <= 0 || channels > 16 ||
        initialFrames <= 0)
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Re-init guard: free any prior allocation first
    if (pipe->Data)
        FramePipeFree(pipe);

    SIZE_T allocBytes = (SIZE_T)initialFrames * (SIZE_T)channels * sizeof(LONG);
    if (allocBytes == 0)
        return STATUS_INVALID_PARAMETER;

    LONG* data = (LONG*)ExAllocatePool2(POOL_FLAG_NON_PAGED, allocBytes, FP_POOL_TAG);
    if (!data)
        return STATUS_INSUFFICIENT_RESOURCES;

    // ExAllocatePool2 already zeroes; explicit Zero is defense in depth.
    RtlZeroMemory(pipe, sizeof(FRAME_PIPE));

    KeInitializeSpinLock(&pipe->Lock);

    pipe->InternalRate          = internalRate;
    pipe->InternalBitsPerSample = 32;
    pipe->InternalBlockAlign    = (LONG)(channels * sizeof(LONG));

    pipe->TargetLatencyFrames   = initialFrames;
    pipe->WrapBound             = initialFrames;
    pipe->FrameCapacityMax      =
        (initialFrames > FP_DEFAULT_CAPACITY_MAX_FRAMES)
            ? initialFrames
            : FP_DEFAULT_CAPACITY_MAX_FRAMES;
    pipe->Channels              = channels;

    // WritePos / ReadPos / counters / UnderrunFlag / SRC state already zero
    // from RtlZeroMemory above.

    pipe->Data                  = data;
    pipe->DataAllocBytes        = allocBytes;

    return STATUS_SUCCESS;
}
#pragma code_seg()

//=============================================================================
// FramePipeFree — release Data, zero struct (PASSIVE_LEVEL)
//=============================================================================
#pragma code_seg("PAGE")
VOID FramePipeFree(PFRAME_PIPE pipe)
{
    PAGED_CODE();

    if (!pipe)
        return;

    if (pipe->Data)
    {
        ExFreePoolWithTag(pipe->Data, FP_POOL_TAG);
        pipe->Data = NULL;
    }

    RtlZeroMemory(pipe, sizeof(FRAME_PIPE));
}
#pragma code_seg()

//=============================================================================
// FramePipeResetCable — STOP path: zero positions + UnderrunFlag + SRC state
//
// Counters (OverflowCounter / UnderrunCounter) are preserved across reset
// for diagnostics (Phase 1 Step 6 acceptance).
//=============================================================================
#pragma code_seg("PAGE")
VOID FramePipeResetCable(PFRAME_PIPE pipe)
{
    PAGED_CODE();

    if (!pipe || !pipe->Data)
        return;

    KIRQL oldIrql;
    KeAcquireSpinLock(&pipe->Lock, &oldIrql);

    pipe->WritePos      = 0;
    pipe->ReadPos       = 0;
    pipe->UnderrunFlag  = 0;

    pipe->WriteSrcPhase = 0;
    pipe->ReadSrcPhase  = 0;
    RtlZeroMemory(pipe->WriteSrcResidual, sizeof(pipe->WriteSrcResidual));
    RtlZeroMemory(pipe->ReadSrcResidual,  sizeof(pipe->ReadSrcResidual));

    KeReleaseSpinLock(&pipe->Lock, oldIrql);
}
#pragma code_seg()

//=============================================================================
// AoRingAvailableSpaceFrames_Locked — writable frames behind the 2-frame
// guard band. Caller must already hold pipe->Lock.
//
// Returns `WrapBound - currentFill - 2` (clamped to >= 0). The -2 guard
// band prevents writer/reader cursor collision (single source of guard
// per step1.md / step2.md — callers must NOT subtract another 2).
//=============================================================================
static __forceinline LONG
AoRingAvailableSpaceFrames_Locked(PFRAME_PIPE pipe)
{
    LONG wrap = pipe->WrapBound;
    LONG fill = pipe->WritePos - pipe->ReadPos;
    if (fill < 0) fill += wrap;          // wrap-corrected current fill
    LONG free = wrap - fill - 2;         // 2-frame guard band
    return free < 0 ? 0 : free;
}

//=============================================================================
// AoRingAvailableFrames_Locked — current fill (readable frames). Caller
// must already hold pipe->Lock; no guard band is applied (distinct from
// AoRingAvailableSpaceFrames_Locked). Distinct from public
// AoRingAvailableFrames, which acquires the lock internally.
//=============================================================================
static __forceinline LONG
AoRingAvailableFrames_Locked(PFRAME_PIPE pipe)
{
    LONG fill = pipe->WritePos - pipe->ReadPos;
    if (fill < 0) fill += pipe->WrapBound;
    return fill;
}

//=============================================================================
// NormalizeToInt19 — bit-depth dispatch per DESIGN § 2.5.
//
// Reads one channel of one frame from `scratch` and returns the value
// normalized to ~19-bit signed-magnitude (range ≈ [-2^18, +2^18]) so the
// SRC accumulator's 13-bit headroom invariant holds across width changes.
//
// 8 PCM (unsigned, byte-centered on 0x80): (byte - 0x80) << 11
// 16 PCM (signed LE):                      ((INT16)int16) << 3
// 24 PCM (signed packed LE, 3 bytes):      sign-extend 24->32 then >> 5
//                                          (equivalent to DESIGN § 2.5
//                                          (raw24 << 8) >> 13 after the
//                                          implicit sign extension that
//                                          the <<8 lift causes)
// 32 PCM (signed LE):                      int32 >> 13
//
// 24-bit sign extension: explicit branch instead of `(<< 8) >> 13` to
// avoid C signed-shift UB. The ring stores INT32 values directly.
// Caller must filter `srcBits` to {8, 16, 24, 32} before reaching here;
// the default branch returns 0 (silence) defensively.
//=============================================================================
static __forceinline LONG
NormalizeToInt19(
    const BYTE* scratch,
    ULONG       srcBits,
    ULONG       frame,
    ULONG       channel,
    ULONG       srcChannels)
{
    SIZE_T bytesPerSample = (SIZE_T)(srcBits / 8);
    SIZE_T offset = ((SIZE_T)frame * (SIZE_T)srcChannels + (SIZE_T)channel)
                  * bytesPerSample;
    const BYTE* p = scratch + offset;

    switch (srcBits)
    {
    case 8:
    {
        // 8-bit PCM is unsigned, centered on 0x80. Range -128..127.
        // <<11 lifts into ~19-bit signed-magnitude.
        return ((LONG)(*p) - 0x80) << 11;
    }

    case 16:
    {
        // Little-endian INT16. <<3 lifts 16-bit signed into ~19-bit.
        LONG v = (LONG)(SHORT)((USHORT)p[0] | ((USHORT)p[1] << 8));
        return v << 3;
    }

    case 24:
    {
        // 24-bit packed signed LE. Explicit sign-extension to avoid the
        // signed-shift UB risk of the (<<8) >> 13 trick from DESIGN § 2.5.
        // Net effect of (raw24 << 8) >> 13 (signed INT32 ops with sign
        // extension via the <<8 lift) is equivalent to a 5-bit
        // arithmetic right shift on the sign-extended 24-bit value.
        LONG raw24 = (LONG)p[0] | ((LONG)p[1] << 8) | ((LONG)p[2] << 16);
        if (raw24 & 0x00800000)            // bit 23 set → negative
            raw24 |= (LONG)0xFF000000;     // sign-extend into bits 24..31
        return raw24 >> 5;
    }

    case 32:
    {
        // INT32 LE. Same value range as machine-native, so simple OR-build
        // and arithmetic right shift suffices. Per DESIGN § 2.5, 32-bit
        // PCM is NOT direct copy: full INT32 range >> 13 to match the
        // 19-bit invariant.
        LONG v = (LONG)((ULONG)p[0]
                      | ((ULONG)p[1] << 8)
                      | ((ULONG)p[2] << 16)
                      | ((ULONG)p[3] << 24));
        return v >> 13;
    }

    default:
        return 0;
    }
}

//=============================================================================
// PickGCDDivisor — first-match GCD divisor for Phase 2 SRC rate ratios.
//
// Phase 2 Step 0. ADR-004 Decision step 1: tries the priority list
// [300, 100, 75] in fixed order and returns on the first candidate that
// divides BOTH rates evenly. NOT "smallest divisor" — for example
// 48000/96000 matches at 300 (ratio 160:320), not at 100 (480:960).
//
// Status contract (per phases/2-single-pass-src/step0.md, commit 04609ae):
//   STATUS_INVALID_PARAMETER — caller bug: out == NULL, srcRate == 0,
//                              or dstRate == 0
//   STATUS_NOT_SUPPORTED     — rate pair with no matching divisor in
//                              [300, 100, 75] (ADR-008 § Consequences)
//   STATUS_SUCCESS           — fills out->{Divisor, SrcRatio, DstRatio}
//
// On every non-SUCCESS path the out fields are zeroed (when out != NULL)
// so callers do not read stale state. No allocations, no locks, no side
// effects. Any IRQL.
//=============================================================================
typedef struct _AO_GCD_RATIO {
    ULONG Divisor;     // 300, 100, 75 on success; 0 on failure
    ULONG SrcRatio;    // srcRate / Divisor on success; 0 on failure
    ULONG DstRatio;    // dstRate / Divisor on success; 0 on failure
} AO_GCD_RATIO;

static
NTSTATUS
PickGCDDivisor(
    _In_  ULONG          srcRate,
    _In_  ULONG          dstRate,
    _Out_ AO_GCD_RATIO*  out)
{
    // Caller-bug guard: NULL out cannot be touched.
    if (out == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    // Zero output up front so every failure path leaves it clean.
    out->Divisor  = 0;
    out->SrcRatio = 0;
    out->DstRatio = 0;
    if (srcRate == 0 || dstRate == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    // First-match across the priority list (ADR-004 Decision step 1).
    // The first candidate that evenly divides BOTH rates wins. This is
    // NOT "smallest divisor": e.g. 48000/96000 matches at 300 (ratio
    // 160:320), not at 100 (480:960).
    static const ULONG divisors[] = { 300, 100, 75 };
    for (ULONG i = 0; i < ARRAYSIZE(divisors); ++i) {
        ULONG d = divisors[i];
        if ((srcRate % d) == 0 && (dstRate % d) == 0) {
            out->Divisor  = d;
            out->SrcRatio = srcRate / d;
            out->DstRatio = dstRate / d;
            return STATUS_SUCCESS;
        }
    }
    // Rate pair that fails the priority probe.
    // ADR-008 § Consequences requires STATUS_NOT_SUPPORTED here.
    return STATUS_NOT_SUPPORTED;
}

//=============================================================================
// AoRingWriteFromScratch — ring write with optional SRC (Phase 1 Step 2 +
// Phase 2 Step 1).
//
// Path:
//   1. Validate parameters (defensive guards).
//   2. ADR-008: srcBits must be PCM 8/16/24/32; otherwise STATUS_NOT_SUPPORTED.
//   3. Acquire pipe->Lock at DISPATCH_LEVEL.
//   4. Branch on srcRate vs pipe->InternalRate:
//        - SRC branch (rate mismatch): PickGCDDivisor (ADR-004 first-
//          match across [300, 100, 75]); reject hostile srcChannels
//          (srcChannels > pipe->Channels or
//           srcChannels > ARRAYSIZE(WriteSrcResidual)) with
//          STATUS_INVALID_PARAMETER before any SRC state mutation;
//          phase-aware EXACT capacity
//            outputFrames = floor((WriteSrcPhase + frames*DstRatio) / SrcRatio)
//          (deliberate divergence from VB's floor+1 safety per
//          vbcable_pipeline_analysis.md § 3.4 — keeps OverflowCounter
//          free of false positives); ADR-005 hard-reject on
//          outputFrames > writable; input-driven linear-interp loop
//          per DESIGN § 2.3 with WriteSrcPhase + WriteSrcResidual[]
//          carried across calls.
//        - Same-rate fast path (rate match): ADR-005 hard-reject if
//          frames > available; per-frame normalize-and-write with
//          NormalizeToInt19; channel mapping copies
//          min(srcChannels, pipe->Channels), excess ring slots retain
//          prior data; WritePos advances with WrapBound modulus.
//   5. Both paths: on hard-reject, OverflowCounter++ and WritePos
//      (and SRC state, when applicable) are all unchanged.
//
// NOT covered here:
//   - Read SRC (Phase 2 Step 2 — AoRingReadToScratch).
//   - Caller migration — external callers still use legacy shims
//     until Phases 4-6.
//=============================================================================
NTSTATUS AoRingWriteFromScratch(
    PFRAME_PIPE  pipe,
    const BYTE*  scratch,
    ULONG        frames,
    ULONG        srcRate,
    ULONG        srcChannels,
    ULONG        srcBits)
{
    // --- Defensive guards (no state change on failure) ---
    if (!pipe || !scratch)
        return STATUS_INVALID_PARAMETER;
    if (frames == 0 || srcChannels == 0)
        return STATUS_INVALID_PARAMETER;
    if (!pipe->Data || pipe->Channels <= 0 || pipe->WrapBound <= 2)
        return STATUS_INVALID_PARAMETER;

    // ADR-008: V1 supports KSDATAFORMAT_SUBTYPE_PCM only.
    // Bit widths {8, 16, 24, 32} match DESIGN § 2.5 dispatch table.
    if (srcBits != 8 && srcBits != 16 && srcBits != 24 && srcBits != 32)
        return STATUS_NOT_SUPPORTED;

    KIRQL oldIrql;
    KeAcquireSpinLock(&pipe->Lock, &oldIrql);

    // SRC branch — Phase 2 Step 1. Same-rate fast path follows below
    // (byte-identical to Phase 1 Step 2 — control flow falls through
    // to it when srcRate == pipe->InternalRate).
    if (srcRate != pipe->InternalRate)
    {
        AO_GCD_RATIO ratio;
        NTSTATUS gcdSt = PickGCDDivisor(srcRate, pipe->InternalRate, &ratio);
        if (!NT_SUCCESS(gcdSt))
        {
            // Helper status propagated as-is. No state mutation.
            KeReleaseSpinLock(&pipe->Lock, oldIrql);
            return gcdSt;
        }

        // Reject hostile srcChannels before any state-mutating step.
        // The ring's per-frame slot count is pipe->Channels and the SRC
        // per-channel residual is sized by ARRAYSIZE(WriteSrcResidual);
        // a client requesting more channels than either bound cannot be
        // served, so reject cleanly here without touching OverflowCounter
        // or any SRC state. Placed AFTER PickGCDDivisor (so genuinely
        // unsupported rate pairs surface STATUS_NOT_SUPPORTED first) and
        // BEFORE the capacity check (so a hostile input never increments
        // OverflowCounter). The unsigned comparisons guard against
        // signed-cast wrap on srcChannels values up to ULONG_MAX.
        if (srcChannels > (ULONG)pipe->Channels ||
            srcChannels > (ULONG)ARRAYSIZE(pipe->WriteSrcResidual))
        {
            KeReleaseSpinLock(&pipe->Lock, oldIrql);
            return STATUS_INVALID_PARAMETER;
        }

        // Phase-aware EXACT output frame count for this call.
        //
        // VB-Cable (results/vbcable_pipeline_analysis.md § 3.4 line 193)
        // uses a `floor(frames*DstRatio/SrcRatio) + 1` safety idiom.
        // AO Cable V1 deliberately diverges to a phase-aware EXACT form:
        // WriteSrcPhase carries the precise common-tick offset since the
        // last emit, so the actual emit count for this call is
        //   outputFrames = floor((WriteSrcPhase + frames*DstRatio) / SrcRatio)
        // Both forms satisfy ADR-005 hard-reject; the exact form keeps
        // OverflowCounter free of false positives in cases where the
        // emit count would have fit. WriteSrcPhase is a single LONG
        // already in the cache line we touched on Lock acquire, so the
        // accuracy gain has zero observable cost.
        //
        // INVARIANT (1:1 emit count <-> capacity check):
        // The SRC loop below performs `accum += DstRatio` once per input
        // frame and `accum -= SrcRatio` once per emit (inside the inner
        // while). Total accumulator delta over the call is therefore
        // `frames*DstRatio - emits*SrcRatio`, while the loop terminates
        // with accum in [0, SrcRatio). Solving:
        //   emits = floor((WriteSrcPhase + frames*DstRatio) / SrcRatio)
        //         = outputFrames64 (computed below).
        // The capacity check therefore matches the actual emit count
        // bit-for-bit; no over- or under-counting is possible.
        ULONGLONG totalDst = (ULONGLONG)(ULONG)pipe->WriteSrcPhase
                           + (ULONGLONG)frames * (ULONGLONG)ratio.DstRatio;
        ULONGLONG outputFrames64 = totalDst / (ULONGLONG)ratio.SrcRatio;

        LONG writable = AoRingAvailableSpaceFrames_Locked(pipe);
        if (outputFrames64 > (ULONGLONG)writable)
        {
            // ADR-005 hard-reject. Mutate ONLY OverflowCounter; the SRC
            // loop below has not been entered, so WritePos /
            // WriteSrcPhase / WriteSrcResidual[] are guaranteed
            // unchanged. The caller may retry once the consumer drains
            // the ring.
            //
            // The comparison stays in ULONGLONG. Truncating outputFrames64
            // to ULONG (or LONG) before the compare would let
            // pathological frames * DstRatio combinations wrap to a
            // small positive (or negative) value that defeats this
            // check, breaking the hard-reject invariant. writable is
            // LONG-bounded and non-negative
            // (AoRingAvailableSpaceFrames_Locked clamps to >= 0), so
            // the (ULONGLONG)cast on writable is safe.
            pipe->OverflowCounter++;
            KeReleaseSpinLock(&pipe->Lock, oldIrql);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        // Channel mapping. srcChannels was bounded by the hostile-input
        // reject guard above (srcChannels <= pipe->Channels &&
        // srcChannels <= ARRAYSIZE(pipe->WriteSrcResidual)), so the cast
        // to LONG is safe and the value already fits the residual array.
        LONG ringChannels = pipe->Channels;
        LONG copyChannels = (LONG)srcChannels;

        // Input-driven linear-interpolation loop (DESIGN § 2.3 / ADR-004).
        //
        // Time model: per common-tick GCD basis, one input frame spans
        // DstRatio common ticks and one output (ring) frame spans SrcRatio
        // common ticks. accum is the common-tick distance since the last
        // emit, in [0, SrcRatio); it carries across calls via WriteSrcPhase.
        //
        // Per emit (the inner while body), the fractional position within
        // the [prev, curr] segment is (DstRatio - accum) / DstRatio after
        // the SrcRatio subtract:
        //   sample = (prev*accum + curr*(DstRatio - accum)) / DstRatio
        // Range invariant (DESIGN § 2.5 19-bit headroom):
        //   prev, curr in [-2^18, 2^18]; accum, DstRatio-accum in
        //   [0, DstRatio-1] with DstRatio <= 2560. LONGLONG intermediate
        //   matches DenormalizeFromInt19's defensive style and removes
        //   any future doubt about overflow at the two-product sum.
        LONG accum = pipe->WriteSrcPhase;

        for (ULONG f = 0; f < frames; ++f)
        {
            LONG curr[FP_MAX_CHANNELS];
            for (LONG ch = 0; ch < copyChannels; ++ch)
            {
                curr[ch] = NormalizeToInt19(scratch, srcBits, f,
                                            (ULONG)ch, srcChannels);
            }

            accum += (LONG)ratio.DstRatio;

            while (accum >= (LONG)ratio.SrcRatio)
            {
                accum -= (LONG)ratio.SrcRatio;

                LONG slotBase = pipe->WritePos * ringChannels;
                for (LONG ch = 0; ch < copyChannels; ++ch)
                {
                    LONGLONG mixed =
                        ((LONGLONG)pipe->WriteSrcResidual[ch] * (LONGLONG)accum)
                      + ((LONGLONG)curr[ch] *
                         (LONGLONG)((LONG)ratio.DstRatio - accum));
                    pipe->Data[slotBase + ch] =
                        (LONG)(mixed / (LONGLONG)ratio.DstRatio);
                }
                // Ring slots beyond copyChannels keep prior contents
                // (Phase 1 Step 2 channel policy preserved).

                pipe->WritePos++;
                if (pipe->WritePos >= pipe->WrapBound)
                    pipe->WritePos = 0;
            }

            for (LONG ch = 0; ch < copyChannels; ++ch)
            {
                pipe->WriteSrcResidual[ch] = curr[ch];
            }
        }

        // Persist phase across the call boundary. New phase satisfies
        // 0 <= accum < SrcRatio (post-subtract invariant of the inner
        // while loop), matching the contract WriteSrcPhase in [0, SrcRatio).
        pipe->WriteSrcPhase = accum;

        KeReleaseSpinLock(&pipe->Lock, oldIrql);
        return STATUS_SUCCESS;
    }

    // ADR-005: hard-reject overflow. WritePos NOT advanced on full ring.
    LONG writable = AoRingAvailableSpaceFrames_Locked(pipe);
    if ((LONG)frames > writable)
    {
        pipe->OverflowCounter++;
        KeReleaseSpinLock(&pipe->Lock, oldIrql);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Channel mapping: client channels [0 .. min(srcChannels, ringCh)).
    // Ring slots beyond the client channel count keep their prior contents
    // (no zero-fill — DESIGN § 2.3 contract: "moja란 채널은 0-fill 안 함").
    LONG ringChannels = pipe->Channels;
    LONG copyChannels = (LONG)srcChannels < ringChannels
                      ? (LONG)srcChannels
                      : ringChannels;

    for (ULONG f = 0; f < frames; ++f)
    {
        LONG slotBase = pipe->WritePos * ringChannels;
        for (LONG ch = 0; ch < copyChannels; ++ch)
        {
            LONG sample = NormalizeToInt19(scratch, srcBits, f,
                                           (ULONG)ch, srcChannels);
            pipe->Data[slotBase + ch] = sample;
        }

        pipe->WritePos++;
        if (pipe->WritePos >= pipe->WrapBound)
            pipe->WritePos = 0;
    }

    KeReleaseSpinLock(&pipe->Lock, oldIrql);
    return STATUS_SUCCESS;
}

//=============================================================================
// DenormalizeFromInt19 — bit-depth dispatch per DESIGN § 2.5 (read direction).
//
// Writes one channel of one frame to `scratch`, denormalizing the 19-bit
// signed-magnitude `sample` value into the destination format.
//
//  8 PCM (unsigned, 0x80-centered): (int >> 11) + 0x80, clamp 0..255
// 16 PCM (signed LE):              int >> 3, clamp INT16
// 24 PCM (signed packed LE):       (LONGLONG)int << 5, clamp INT24, 3-byte pack
// 32 PCM (signed LE):              (LONGLONG)int << 13, clamp INT32, 4-byte pack
//
// Why LONGLONG for 24/32-bit shifts:
//   The ring stores 19-bit signed-magnitude (±2^18), so `sample << 13`
//   on the worst-case +2^18 input would equal +2^31 — outside the
//   representable INT32 range. Computing in INT32 directly is undefined
//   behavior under the C standard for signed left shift overflow. The
//   LONGLONG intermediate sidesteps the UB; the explicit clamp then
//   bounds the value back into the destination range before pack.
//
// Why clamp for 8/16-bit too:
//   Same headroom argument: +2^18 >> 3 = +2^15 = 32768, one above
//   INT16_MAX. (>> on signed types is implementation-defined arithmetic
//   shift in MSVC, so the right-shift itself is safe — but the post-shift
//   value can still exceed the dst format's range and must be clamped.)
//
// Caller must filter `dstBits` to {8, 16, 24, 32} before reaching here;
// the default branch is a no-op (defensive).
//=============================================================================
static __forceinline VOID
DenormalizeFromInt19(
    BYTE*       scratch,
    ULONG       dstBits,
    ULONG       frame,
    ULONG       channel,
    ULONG       dstChannels,
    LONG        sample)
{
    SIZE_T bytesPerSample = (SIZE_T)(dstBits / 8);
    SIZE_T offset = ((SIZE_T)frame * (SIZE_T)dstChannels + (SIZE_T)channel)
                  * bytesPerSample;
    BYTE* p = scratch + offset;

    switch (dstBits)
    {
    case 8:
    {
        // (sample >> 11) + 0x80, clamp 0..255 (unsigned 8-bit PCM range).
        // MSVC arithmetic right shift on signed LONG is well-defined.
        LONG v = (sample >> 11) + 0x80;
        if (v > 255) v = 255;
        if (v < 0)   v = 0;
        *p = (BYTE)v;
        break;
    }

    case 16:
    {
        // sample >> 3, clamp INT16. Signed-LE 16-bit pack.
        LONG v = sample >> 3;
        if (v >  32767) v =  32767;
        if (v < -32768) v = -32768;
        p[0] = (BYTE)( v        & 0xFF);
        p[1] = (BYTE)((v >>  8) & 0xFF);
        break;
    }

    case 24:
    {
        // ((LONGLONG)sample) << 5, clamp INT24, 3-byte signed-LE pack.
        // LONGLONG intermediate avoids signed-shift UB at the +2^18 boundary.
        LONGLONG v64 = ((LONGLONG)sample) << 5;
        if (v64 >  8388607LL)  v64 =  8388607LL;   // (1 << 23) - 1
        if (v64 < -8388608LL)  v64 = -8388608LL;   // -(1 << 23)
        LONG v = (LONG)v64;
        p[0] = (BYTE)( v        & 0xFF);
        p[1] = (BYTE)((v >>  8) & 0xFF);
        p[2] = (BYTE)((v >> 16) & 0xFF);
        break;
    }

    case 32:
    {
        // ((LONGLONG)sample) << 13, clamp INT32, 4-byte signed-LE pack.
        // Per DESIGN § 2.5: 32-bit PCM is NOT a direct copy — << 13
        // restores the 19-bit normalization shift applied on write.
        LONGLONG v64 = ((LONGLONG)sample) << 13;
        if (v64 >  2147483647LL)   v64 =  2147483647LL;   // INT32_MAX
        if (v64 < -2147483647LL-1) v64 = -2147483647LL-1; // INT32_MIN
        LONG v = (LONG)v64;
        p[0] = (BYTE)( v        & 0xFF);
        p[1] = (BYTE)((v >>  8) & 0xFF);
        p[2] = (BYTE)((v >> 16) & 0xFF);
        p[3] = (BYTE)((v >> 24) & 0xFF);
        break;
    }

    default:
        // Defensive: caller filters dstBits to {8,16,24,32} upstream.
        break;
    }
}

//=============================================================================
// ZeroFillScratch — write `frames * dstChannels * (dstBits/8)` silence
// bytes to `scratch`. 8-bit PCM silence is 0x80 (mid-rail unsigned);
// 16/24/32-bit PCM silence is 0x00. Caller guarantees dstBits ∈ {8,16,24,32}.
//=============================================================================
static __forceinline VOID
ZeroFillScratch(
    BYTE*       scratch,
    ULONG       frames,
    ULONG       dstBits,
    ULONG       dstChannels)
{
    SIZE_T totalBytes = (SIZE_T)frames
                      * (SIZE_T)dstChannels
                      * (SIZE_T)(dstBits / 8);
    if (dstBits == 8)
        RtlFillMemory(scratch, totalBytes, 0x80);
    else
        RtlZeroMemory(scratch, totalBytes);
}

//=============================================================================
// AoRingReadToScratch — ring read with optional SRC (Phase 1 Step 3 +
// Phase 2 Step 2).
//
// Path:
//   1. Validate parameters (defensive guards).
//   2. ADR-008: dstBits must be PCM 8/16/24/32; otherwise STATUS_NOT_SUPPORTED.
//   3. Acquire pipe->Lock at DISPATCH_LEVEL.
//   4. Branch on dstRate vs pipe->InternalRate:
//        - SRC branch (rate mismatch): PickGCDDivisor (ADR-004 first-
//          match across [300, 100, 75]); reject hostile dstChannels
//          (dstChannels > ARRAYSIZE(ReadSrcResidual)) with
//          STATUS_INVALID_PARAMETER before any state mutation;
//          phase-aware EXACT
//            ringFramesNeeded64 =
//              floor((ReadSrcPhase + frames*SrcRatio) / DstRatio)
//          drives ReadPos advance count, and lookahead-aware
//            consumedBeforeLastOutput =
//              floor((ReadSrcPhase + (frames-1)*SrcRatio) / DstRatio)
//            requiredReadableFrames64 =
//              (consumedBeforeLastOutput == ringFramesNeeded64)
//                ? ringFramesNeeded64 + 1
//                : ringFramesNeeded64
//          drives the capacity check (the +1 covers the trailing
//          lookahead when the final consume precedes the last
//          output frame -- e.g. 48k->96k frames=3 phase=0 where
//          K=1 but f=2 still needs ring[R+1]). ADR-005 hysteresis
//          stay/exit identical to same-rate; hard underrun on
//          (requiredReadableFrames64 > available) sets UnderrunFlag,
//          increments UnderrunCounter, delivers silence via
//          ZeroFillScratch with all other state unchanged. Output-
//          driven linear-interp loop with CONDITIONAL multi-consume
//          reload (mid-consume reload only; final-consume position
//          left unread to keep memory reads tied exactly to
//          requiredReadableFrames64). dstChannels > pipe->Channels
//          is allowed; surplus client channels receive inline silence
//          per Phase 1 Step 3 policy / DESIGN § 2.3 "no hidden upmix".
//        - Same-rate fast path (rate match): Phase 1 Step 3 logic,
//          BYTE-IDENTICAL — ADR-005 hysteresis stay/exit, hard
//          underrun on (LONG)frames > available, per-frame
//          DenormalizeFromInt19 with surplus-channel silence,
//          ReadPos advance with WrapBound modulus.
//   5. Both paths: underrun / recovery paths deliver silence (via
//      ZeroFillScratch); parameter / status failures (defensive
//      guards, ADR-008 dstBits gate, PickGCDDivisor failure, hostile
//      dstChannels reject) return without touching scratch. ReadPos
//      / ReadSrcPhase / ReadSrcResidual[] mutated only on SRC
//      success.
//
// NOT covered here:
//   - Caller migration — external callers still use legacy shims
//     until Phases 4-6.
//=============================================================================
NTSTATUS AoRingReadToScratch(
    PFRAME_PIPE  pipe,
    BYTE*        scratch,
    ULONG        frames,
    ULONG        dstRate,
    ULONG        dstChannels,
    ULONG        dstBits)
{
    // --- Defensive guards (no state change on failure) ---
    if (!pipe || !scratch)
        return STATUS_INVALID_PARAMETER;
    if (frames == 0 || dstChannels == 0)
        return STATUS_INVALID_PARAMETER;
    if (!pipe->Data || pipe->Channels <= 0 || pipe->WrapBound <= 2)
        return STATUS_INVALID_PARAMETER;

    // ADR-008: V1 supports KSDATAFORMAT_SUBTYPE_PCM only.
    // Bit widths {8, 16, 24, 32} match DESIGN § 2.5 dispatch table.
    if (dstBits != 8 && dstBits != 16 && dstBits != 24 && dstBits != 32)
        return STATUS_NOT_SUPPORTED;

    KIRQL oldIrql;
    KeAcquireSpinLock(&pipe->Lock, &oldIrql);

    // SRC branch — Phase 2 Step 2. Same-rate fast path follows below
    // (byte-identical to Phase 1 Step 3 — control flow falls through
    // to it when dstRate == pipe->InternalRate).
    if (dstRate != pipe->InternalRate)
    {
        AO_GCD_RATIO ratio;
        NTSTATUS gcdSt = PickGCDDivisor(pipe->InternalRate, dstRate, &ratio);
        if (!NT_SUCCESS(gcdSt))
        {
            // Helper status propagated as-is. No state mutation.
            KeReleaseSpinLock(&pipe->Lock, oldIrql);
            return gcdSt;
        }

        // Reject hostile dstChannels before any state-mutating step.
        // Option B (hybrid) — reject only on the residual-array bound.
        // dstChannels > pipe->Channels is a legitimate Phase 1 Step 3
        // case (mono ring -> stereo client, etc.) handled by inline
        // surplus-channel silence below; the SRC branch preserves that
        // policy. The unsigned comparison guards against signed-cast
        // wrap on dstChannels values up to ULONG_MAX. Placed AFTER
        // PickGCDDivisor (so unsupported rate pairs surface
        // STATUS_NOT_SUPPORTED first) and BEFORE hysteresis / hard
        // underrun (so hostile input never touches UnderrunFlag /
        // UnderrunCounter or any SRC state).
        if (dstChannels > (ULONG)ARRAYSIZE(pipe->ReadSrcResidual))
        {
            KeReleaseSpinLock(&pipe->Lock, oldIrql);
            return STATUS_INVALID_PARAMETER;
        }

        // Phase-aware EXACT ReadPos advance count for this call.
        //
        // VB-Cable (results/vbcable_pipeline_analysis.md § 3.4) uses a
        // `floor + 1` safety idiom on the write direction. AO Cable V1
        // chooses the symmetric phase-aware EXACT form on read:
        // ReadSrcPhase carries the precise common-tick offset since
        // the most recently consumed ring frame, so the actual ReadPos
        // advance count for this call is
        //   ringFramesNeeded64 =
        //     floor((ReadSrcPhase + frames*SrcRatio) / DstRatio)
        //
        // INVARIANT (1:1 ReadPos advance <-> ringFramesNeeded64):
        // The SRC loop below `accum += SrcRatio` once per output frame
        // and `accum -= DstRatio` once per ReadPos++; total advances
        // bit-for-bit match this formula (proof: accum delta =
        // frames*SrcRatio - emits*DstRatio, terminator accum in
        // [0, DstRatio), solve).
        //
        // requiredReadableFrames64 is the lookahead-aware capacity
        // bound. The naive `max(K, 1)` form is INSUFFICIENT for
        // multi-output-frame upsample calls where the final consume
        // happens BEFORE the last output frame: the trailing outer
        // iteration would still need to load curr from a position
        // ONE PAST ringFramesNeeded64, which is outside the readable
        // window when `available == ringFramesNeeded64` exactly.
        //
        // Detect that case by comparing the cumulative consume count
        // up to (but not including) the final output frame against
        // ringFramesNeeded64:
        //   consumedBeforeLastOutput =
        //     floor((ReadSrcPhase + (frames-1)*SrcRatio) / DstRatio)
        // When consumedBeforeLastOutput == ringFramesNeeded64, the
        // final consume has already happened before f = frames - 1
        // begins, and that last iteration must load curr from
        // ring[ReadPos + ringFramesNeeded64] -> require K + 1.
        // Otherwise the final consume is inside the last output
        // frame's while loop, and no extra lookahead is needed beyond
        // ringFramesNeeded64 -> require K.
        //
        // Examples:
        //   K=0 (small upsample chunk): consumedBeforeLastOutput == 0
        //     == K -> require K+1 = 1 (initial lookahead of ring[R]).
        //   48k->96k frames=2: K=1, consumedBeforeLastOutput=0 != K
        //     -> require K=1 (final consume IS in f=1's while loop).
        //   48k->96k frames=3: K=1, consumedBeforeLastOutput=1 == K
        //     -> require K+1=2 (final consume in f=1, but f=2 still
        //     needs lookahead from ring[R+1]).
        //   96k->48k frames=2: K=4, consumedBeforeLastOutput=2 != K
        //     -> require K=4 (final consume IS in f=1's while loop).
        //
        // ReadPos advance count is ALWAYS == ringFramesNeeded64 (NOT
        // requiredReadableFrames64) -- the +1 only covers the trailing
        // lookahead, never actual consumption. frames > 0 is
        // guaranteed by the entry guard, so (frames - 1) does not
        // underflow.
        ULONGLONG totalSrc = (ULONGLONG)(ULONG)pipe->ReadSrcPhase
                           + (ULONGLONG)frames * (ULONGLONG)ratio.SrcRatio;
        ULONGLONG ringFramesNeeded64 = totalSrc / (ULONGLONG)ratio.DstRatio;
        ULONGLONG consumedBeforeLastOutput =
            ((ULONGLONG)(ULONG)pipe->ReadSrcPhase
              + (ULONGLONG)(frames - 1) * (ULONGLONG)ratio.SrcRatio)
            / (ULONGLONG)ratio.DstRatio;
        ULONGLONG requiredReadableFrames64 =
            (consumedBeforeLastOutput == ringFramesNeeded64)
                ? (ringFramesNeeded64 + 1ULL)
                : ringFramesNeeded64;

        LONG available = AoRingAvailableFrames_Locked(pipe);

        // ADR-005 hysteretic underrun -- mirror Phase 1 Step 3 on the
        // SRC branch. The same WrapBound/2 threshold separates "stay"
        // (deliver silence, no state change) from "exit" (clear flag,
        // fall through to capacity check).
        if (pipe->UnderrunFlag)
        {
            if (available < pipe->WrapBound / 2)
            {
                // Stay in recovery — silence delivered, no SRC state
                // mutation, UnderrunCounter unchanged.
                ZeroFillScratch(scratch, frames, dstBits, dstChannels);
                KeReleaseSpinLock(&pipe->Lock, oldIrql);
                return STATUS_SUCCESS;
            }
            // Refill crossed the 50% threshold -- exit recovery, fall
            // through to the capacity check.
            pipe->UnderrunFlag = 0;
        }

        if (requiredReadableFrames64 > (ULONGLONG)available)
        {
            // Hard underrun — enter recovery, deliver silence. ReadPos
            // / ReadSrcPhase / ReadSrcResidual[] all unchanged so the
            // caller may retry on the next tick once more frames
            // arrive. The (ULONGLONG) comparison keeps the
            // hard-underrun invariant intact under hostile inputs --
            // truncating ringFramesNeeded64 (or
            // requiredReadableFrames64) to ULONG before the compare
            // would let pathological frames * SrcRatio combos wrap to
            // a small value that defeats this check.
            pipe->UnderrunCounter++;
            pipe->UnderrunFlag = 1;
            ZeroFillScratch(scratch, frames, dstBits, dstChannels);
            KeReleaseSpinLock(&pipe->Lock, oldIrql);
            return STATUS_SUCCESS;
        }

        // SRC channel mapping. dstChannels was bounded by the
        // residual-array reject above (dstChannels <=
        // ARRAYSIZE(ReadSrcResidual)). copyChannels is the count of
        // channels the SRC actually computes for; surplus client
        // channels (copyChannels..dstChannels-1) get inline silence
        // below per Phase 1 Step 3 policy.
        LONG  ringChannels   = pipe->Channels;
        ULONG copyChannelsU  = dstChannels;
        if (copyChannelsU > (ULONG)ringChannels)
            copyChannelsU = (ULONG)ringChannels;
        LONG   copyChannels   = (LONG)copyChannelsU;
        SIZE_T bytesPerSample = (SIZE_T)(dstBits / 8);

        // Output-driven linear-interpolation loop (DESIGN § 2.4 / ADR-004).
        //
        // Time model: per common-tick GCD basis, one ring frame spans
        // DstRatio common ticks and one client (output) frame spans
        // SrcRatio common ticks. accum is the common-tick distance
        // since the most recently consumed ring frame, in
        // [0, DstRatio); it carries across calls via ReadSrcPhase.
        //
        // Per emit, alpha = accum / DstRatio gives the fractional
        // position within the [prev, curr] ring interval:
        //   sample = (prev*(DstRatio-accum) + curr*accum) / DstRatio
        // Range invariant (DESIGN § 2.5 19-bit headroom):
        //   prev, curr in [-2^18, 2^18]; accum, (DstRatio-accum) in
        //   [0, DstRatio-1] with DstRatio <= 2560. LONGLONG
        //   intermediate matches DenormalizeFromInt19's defensive
        //   style.
        LONG accum = pipe->ReadSrcPhase;
        LONG prev[FP_MAX_CHANNELS];
        LONG curr[FP_MAX_CHANNELS];

        // prev[] carries from the prior call. curr[] is loaded fresh
        // at every outer iteration's start and conditionally inside
        // the inner while (mid-consume only).
        for (LONG ch = 0; ch < copyChannels; ++ch)
        {
            prev[ch] = pipe->ReadSrcResidual[ch];
        }

        for (ULONG f = 0; f < frames; ++f)
        {
            // Per-iteration curr reload from current ReadPos. The
            // capacity check guarantees the position read here is
            // inside the readable window:
            //   f == 0 is covered either as the first consumed
            //          position or as the trailing/lookahead slot
            //          required by the consumedBeforeLastOutput rule.
            //   f >= 1: ReadPos is one position past the previous
            //          iteration's last consume (no reload happened
            //          after that consume — this iteration's start
            //          reads the now-current outer position). The
            //          position is covered by requiredReadableFrames64
            //          either as a consumed position or as the
            //          trailing lookahead slot.
            {
                LONG slotBase = pipe->ReadPos * ringChannels;
                for (LONG ch = 0; ch < copyChannels; ++ch)
                {
                    curr[ch] = pipe->Data[slotBase + ch];
                }
            }

            // Emit at fractional position accum/DstRatio between prev
            // and curr (alpha = accum / DstRatio).
            for (LONG ch = 0; ch < copyChannels; ++ch)
            {
                LONGLONG mixed =
                    ((LONGLONG)prev[ch] *
                     (LONGLONG)((LONG)ratio.DstRatio - accum))
                  + ((LONGLONG)curr[ch] * (LONGLONG)accum);
                LONG sample = (LONG)(mixed / (LONGLONG)ratio.DstRatio);
                DenormalizeFromInt19(scratch, dstBits, f, (ULONG)ch,
                                     dstChannels, sample);
            }
            // Surplus client channels (ch >= ringChannels): inline
            // silence per Phase 1 Step 3 policy / DESIGN § 2.3 "no
            // hidden upmix". Same idiom as the same-rate body
            // (8-bit 0x80, else RtlZeroMemory).
            for (ULONG ch = (ULONG)ringChannels; ch < dstChannels; ++ch)
            {
                SIZE_T off = ((SIZE_T)f * (SIZE_T)dstChannels + (SIZE_T)ch)
                           * bytesPerSample;
                BYTE* p = scratch + off;
                if (dstBits == 8)
                    *p = 0x80;
                else
                    RtlZeroMemory(p, bytesPerSample);
            }

            accum += (LONG)ratio.SrcRatio;

            // Multi-consume reload (BLOCKER fix), CONDITIONAL on more
            // consumes ahead in this iteration.
            //
            // Two cases per inner advance:
            // (a) MID-consume: another consume is still pending in this
            //     iteration (`accum >= DstRatio` after the subtract).
            //     curr MUST be reloaded so the next prev = curr
            //     promotion captures the actual intermediate ring
            //     sample. Without this reload, prev = curr would store
            //     the SAME value as the previous promotion and
            //     ring[R+1..R+K-1] would never enter the prev sequence.
            //     96k -> 48k (SrcRatio=320, DstRatio=160) emits once,
            //     accum becomes 320, the while body runs twice; the
            //     FIRST consume MUST reload curr = ring[R+1].
            // (b) FINAL consume: `accum < DstRatio` after the subtract.
            //     curr MUST NOT be reloaded — the new ReadPos can sit
            //     outside the readable window when `available ==
            //     ringFramesNeeded64` exactly. Next outer iteration's
            //     start (when present) handles the reload; otherwise
            //     the call ends with the post-final-consume position
            //     untouched until the next call's capacity re-eval.
            //
            // The conditional reload below ties memory reads to
            // requiredReadableFrames64: all consumed positions, plus
            // exactly one trailing lookahead when
            // consumedBeforeLastOutput == ringFramesNeeded64. It must
            // not add an unconditional K + 1 read.
            while (accum >= (LONG)ratio.DstRatio)
            {
                accum -= (LONG)ratio.DstRatio;
                for (LONG ch = 0; ch < copyChannels; ++ch)
                {
                    prev[ch] = curr[ch];
                }
                pipe->ReadPos++;
                if (pipe->ReadPos >= pipe->WrapBound) pipe->ReadPos = 0;
                if (accum >= (LONG)ratio.DstRatio)
                {
                    // Mid-consume: more consumes ahead, reload curr.
                    LONG slotBase = pipe->ReadPos * ringChannels;
                    for (LONG ch = 0; ch < copyChannels; ++ch)
                    {
                        curr[ch] = pipe->Data[slotBase + ch];
                    }
                }
                // else: final consume; skip reload.
            }
        }

        // Persist phase + residuals across the call boundary. New
        // accum satisfies 0 <= accum < DstRatio (post-subtract
        // invariant of the inner while), matching the contract
        // ReadSrcPhase in [0, DstRatio).
        for (LONG ch = 0; ch < copyChannels; ++ch)
        {
            pipe->ReadSrcResidual[ch] = prev[ch];
        }
        pipe->ReadSrcPhase = accum;

        KeReleaseSpinLock(&pipe->Lock, oldIrql);
        return STATUS_SUCCESS;
    }

    LONG available = AoRingAvailableFrames_Locked(pipe);

    // ADR-005 hysteretic underrun recovery.
    if (pipe->UnderrunFlag)
    {
        if (available < pipe->WrapBound / 2)
        {
            // Stay in recovery — deliver silence, do NOT advance ReadPos.
            ZeroFillScratch(scratch, frames, dstBits, dstChannels);
            KeReleaseSpinLock(&pipe->Lock, oldIrql);
            return STATUS_SUCCESS;
        }
        // Refill crossed the 50% threshold — exit recovery.
        pipe->UnderrunFlag = 0;
    }

    // Hard underrun → enter recovery, deliver silence.
    if ((LONG)frames > available)
    {
        pipe->UnderrunCounter++;
        pipe->UnderrunFlag = 1;
        ZeroFillScratch(scratch, frames, dstBits, dstChannels);
        KeReleaseSpinLock(&pipe->Lock, oldIrql);
        return STATUS_SUCCESS;
    }

    // Per-frame, per-channel denormalize-and-write.
    LONG ringChannels = pipe->Channels;
    SIZE_T bytesPerSample = (SIZE_T)(dstBits / 8);

    for (ULONG f = 0; f < frames; ++f)
    {
        LONG slotBase = pipe->ReadPos * ringChannels;

        for (ULONG ch = 0; ch < dstChannels; ++ch)
        {
            if ((LONG)ch < ringChannels)
            {
                LONG sample = pipe->Data[slotBase + (LONG)ch];
                DenormalizeFromInt19(scratch, dstBits, f, ch,
                                     dstChannels, sample);
            }
            else
            {
                // dstChannels > pipe->Channels: surplus client channels
                // receive silence (8-bit 0x80 / else 0x00). DESIGN § 2.3
                // forbids hidden upmix; this is silence, not synthesis.
                SIZE_T off = ((SIZE_T)f * (SIZE_T)dstChannels + (SIZE_T)ch)
                           * bytesPerSample;
                BYTE* p = scratch + off;
                if (dstBits == 8)
                    *p = 0x80;
                else
                    RtlZeroMemory(p, bytesPerSample);
            }
        }

        pipe->ReadPos++;
        if (pipe->ReadPos >= pipe->WrapBound)
            pipe->ReadPos = 0;
    }

    KeReleaseSpinLock(&pipe->Lock, oldIrql);
    return STATUS_SUCCESS;
}

//=============================================================================
//=============================================================================
//
//  Legacy compile-preserving shim layer — Phase 1 Step 1.
//
//  Each entry point below was a real legacy FramePipe* function whose body
//  touched fields that no longer exist on the canonical FRAME_PIPE shape.
//  We keep the *symbols* so untouched translation units link without
//  modification, and route the bodies to either the new canonical API
//  (forward wrappers, where semantics carry over) or to a no-op /
//  zero-return stub (where they do not). Step 2+ migrates each external
//  caller off these shims; Phase 6 cleanup deletes this entire section.
//
//=============================================================================
//=============================================================================

//---- Forward wrappers --------------------------------------------------------

#pragma code_seg("PAGE")
NTSTATUS FramePipeInit(
    PFRAME_PIPE  pPipe,
    ULONG        pipeSampleRate,
    ULONG        pipeChannels,
    ULONG        targetFillFrames)
{
    PAGED_CODE();

    return FramePipeInitCable(pPipe,
                              pipeSampleRate,
                              (LONG)pipeChannels,
                              (LONG)targetFillFrames);
}
#pragma code_seg()

#pragma code_seg("PAGE")
VOID FramePipeCleanup(PFRAME_PIPE pPipe)
{
    PAGED_CODE();
    FramePipeFree(pPipe);
}
#pragma code_seg()

#pragma code_seg("PAGE")
VOID FramePipeReset(PFRAME_PIPE pPipe)
{
    PAGED_CODE();
    FramePipeResetCable(pPipe);
}
#pragma code_seg()

ULONG FramePipeGetFillFrames(PFRAME_PIPE pPipe)
{
    return AoRingAvailableFrames(pPipe);
}

//---- Behavior-absent stubs ---------------------------------------------------
//
// These functions used to drive the legacy transport. After the struct
// rewrite their body fields are gone; rather than re-implement against the
// canonical shape (which is Step 2+ work), we return a safe no-op so that
// external callers compile and link. Audio will not move through these
// paths until Step 2 / Step 3 land and external callers migrate to the
// new canonical API.
//
// Step 1 acceptance explicitly notes: "Existing transport callers ...
// compile, even if behavior incorrect."
//------------------------------------------------------------------------------

ULONG FramePipeWriteFrames(
    PFRAME_PIPE   pPipe,
    const INT32*  srcFrames,
    ULONG         frameCount)
{
    UNREFERENCED_PARAMETER(pPipe);
    UNREFERENCED_PARAMETER(srcFrames);
    UNREFERENCED_PARAMETER(frameCount);
    return 0;
}

ULONG FramePipeReadFrames(
    PFRAME_PIPE  pPipe,
    INT32*       dstFrames,
    ULONG        frameCount)
{
    UNREFERENCED_PARAMETER(pPipe);
    UNREFERENCED_PARAMETER(dstFrames);
    UNREFERENCED_PARAMETER(frameCount);
    return 0;
}

VOID FramePipePrefillSilence(PFRAME_PIPE pPipe)
{
    UNREFERENCED_PARAMETER(pPipe);
}

#pragma code_seg("PAGE")
VOID FramePipeRegisterFormat(
    PFRAME_PIPE  pPipe,
    BOOLEAN      isSpeaker,
    ULONG        sampleRate,
    ULONG        bitsPerSample,
    ULONG        nChannels,
    ULONG        nBlockAlign,
    BOOLEAN      isFloat)
{
    PAGED_CODE();
    UNREFERENCED_PARAMETER(pPipe);
    UNREFERENCED_PARAMETER(isSpeaker);
    UNREFERENCED_PARAMETER(sampleRate);
    UNREFERENCED_PARAMETER(bitsPerSample);
    UNREFERENCED_PARAMETER(nChannels);
    UNREFERENCED_PARAMETER(nBlockAlign);
    UNREFERENCED_PARAMETER(isFloat);
}
#pragma code_seg()

#pragma code_seg("PAGE")
VOID FramePipeUnregisterFormat(
    PFRAME_PIPE  pPipe,
    BOOLEAN      isSpeaker)
{
    PAGED_CODE();
    UNREFERENCED_PARAMETER(pPipe);
    UNREFERENCED_PARAMETER(isSpeaker);
}
#pragma code_seg()

ULONG FramePipeWriteFromDma(
    PFRAME_PIPE  pPipe,
    const BYTE*  dmaData,
    ULONG        byteCount)
{
    UNREFERENCED_PARAMETER(pPipe);
    UNREFERENCED_PARAMETER(dmaData);
    UNREFERENCED_PARAMETER(byteCount);
    return 0;
}

ULONG FramePipeWriteFromDmaEx(
    PFRAME_PIPE  pPipe,
    const BYTE*  dmaData,
    ULONG        byteCount,
    PVOID        rtOpaque)
{
    UNREFERENCED_PARAMETER(pPipe);
    UNREFERENCED_PARAMETER(dmaData);
    UNREFERENCED_PARAMETER(byteCount);
    UNREFERENCED_PARAMETER(rtOpaque);
    return 0;
}

VOID FramePipeReadToDma(
    PFRAME_PIPE  pPipe,
    BYTE*        dmaData,
    ULONG        byteCount)
{
    UNREFERENCED_PARAMETER(pPipe);
    UNREFERENCED_PARAMETER(dmaData);
    UNREFERENCED_PARAMETER(byteCount);
}

//=============================================================================
//=============================================================================
//
//  Phase 1 Step 0 cross-TU helpers — converted to behavior-absent stubs
//  against the new FRAME_PIPE shape (Phase 1 Step 1).
//
//  These four helpers were introduced in commit ddbb977 (Phase 1 Step 0)
//  to remove direct FRAME_PIPE field access from minwavertstream.cpp.
//  Their bodies in Step 0 read/wrote legacy fields (Initialized,
//  Speaker/MicActive, Render/CapturePumpFeatureFlags, all six
//  Render/Capture*Count fields). Those fields are gone in the canonical
//  shape, so the bodies are now stubs:
//    - FramePipeIsDirectionActive returns FALSE unconditionally
//    - FramePipeSetPumpFeatureFlags is a no-op
//    - FramePipeResetPumpFeatureFlags is a no-op
//    - FramePipePublishPumpCounters is a no-op
//
//  minwavertstream.cpp is untouched; it continues to call these helpers
//  in the legacy pump / pause-reset / counter-publish paths. The calls
//  link, but produce no observable cable-transport behavior. Phase 5
//  ownership flip preceded this step, so the legacy pump path is no
//  longer driving audio. Phase 6 cleanup deletes both these helpers
//  and the minwavertstream.cpp call sites together with the AO_V2_DIAG
//  legacy fields.
//
//=============================================================================
//=============================================================================

BOOLEAN FramePipeIsDirectionActive(PFRAME_PIPE pipe, BOOLEAN isSpeaker)
{
    UNREFERENCED_PARAMETER(isSpeaker);
    UNREFERENCED_PARAMETER(pipe);
    return FALSE;
}

VOID FramePipeSetPumpFeatureFlags(
    PFRAME_PIPE  pipe,
    BOOLEAN      isRenderSide,
    ULONG        flags)
{
    UNREFERENCED_PARAMETER(pipe);
    UNREFERENCED_PARAMETER(isRenderSide);
    UNREFERENCED_PARAMETER(flags);
}

VOID FramePipeResetPumpFeatureFlags(PFRAME_PIPE pipe, BOOLEAN isRenderSide)
{
    UNREFERENCED_PARAMETER(pipe);
    UNREFERENCED_PARAMETER(isRenderSide);
}

VOID FramePipePublishPumpCounters(
    PFRAME_PIPE  pipe,
    BOOLEAN      isRenderSide,
    ULONG        gatedSkipCount,
    ULONG        overJumpCount,
    ULONGLONG    framesProcessedTotal,
    ULONG        invocationCount,
    ULONG        shadowDivergenceCount,
    ULONG        featureFlags)
{
    UNREFERENCED_PARAMETER(pipe);
    UNREFERENCED_PARAMETER(isRenderSide);
    UNREFERENCED_PARAMETER(gatedSkipCount);
    UNREFERENCED_PARAMETER(overJumpCount);
    UNREFERENCED_PARAMETER(framesProcessedTotal);
    UNREFERENCED_PARAMETER(invocationCount);
    UNREFERENCED_PARAMETER(shadowDivergenceCount);
    UNREFERENCED_PARAMETER(featureFlags);
}

//=============================================================================
//
//  AoPumpApplyRenderFlagMask — fail-closed link target stub
//
//  History:
//    2c733f1 (2026-04-14, Phase 5 CLOSED) — original definition added
//      to minwavertstream.cpp, with per-cable active-stream globals
//      (g_CableA/BActiveRenderStream, g_CableA/BActiveRenderLock),
//      static helpers AoPumpRegister/UnregisterActiveRenderStream, and
//      the member body CMiniportWaveRTStream::ApplyPumpFlagMaskUnderLock.
//    5a013b1 (2026-04-15, Phase 6 Step 1 skeleton) — wholesale removed
//      the definition AND its dependent globals/helpers/member body
//      while migrating cable transport ownership to the shared transport
//      engine. The declaration in loopback.h and the four call sites in
//      adapter.cpp's IOCTL_AO_SET_PUMP_FEATURE_FLAGS handler stayed,
//      leaving the link unresolved.
//    f7801bd (Phase 1 build fix) — fail-closed stub added here so the
//      link target exists. Phase 6 cleanup is expected to retire both
//      the IOCTL handler and this stub together.
//
//=============================================================================
extern "C"
NTSTATUS
AoPumpApplyRenderFlagMask(
    _In_ ULONG cableIndex,
    _In_ ULONG setMask,
    _In_ ULONG clearMask)
{
    UNREFERENCED_PARAMETER(cableIndex);
    UNREFERENCED_PARAMETER(setMask);
    UNREFERENCED_PARAMETER(clearMask);

    return STATUS_NOT_SUPPORTED;
}
