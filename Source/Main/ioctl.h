/*++
Module Name:
    ioctl.h
Abstract:
    IOCTL codes and shared structures for AO Virtual Cable driver <-> control panel.
    This header is shared between kernel driver and user-mode control panel app.
--*/

#ifndef _AO_VIRTUAL_CABLE_IOCTL_H_
#define _AO_VIRTUAL_CABLE_IOCTL_H_

#ifndef _KERNEL_MODE
#include <windows.h>
#include <winioctl.h>
#endif

// Device interface GUID for control panel to open handle
// {7B3E4A10-1B2C-4D5E-9F8A-0B1C2D3E4F5A}
DEFINE_GUID(GUID_DEVINTERFACE_AO_VIRTUAL_CABLE,
    0x7B3E4A10, 0x1B2C, 0x4D5E, 0x9F, 0x8A, 0x0B, 0x1C, 0x2D, 0x3E, 0x4F, 0x5A);

// IOCTL codes
#define IOCTL_AO_SET_INTERNAL_RATE  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_AO_SET_MAX_LATENCY    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_AO_GET_STREAM_STATUS  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_AO_GET_CONFIG         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_READ_ACCESS)
// SET_MAX_CHANNELS: writes to registry only. Takes effect after device restart.
#define IOCTL_AO_SET_MAX_CHANNELS   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_WRITE_ACCESS)

// PREPARE_UNLOAD: signals driver to reject new opens, delete symlink,
// and delete control device once all existing handles are closed.
// Used by install.ps1 to enable in-session upgrade without reboot.
#define IOCTL_AO_PREPARE_UNLOAD     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_WRITE_ACCESS)

// Phase 5 (2026-04-14): SET_PUMP_FEATURE_FLAGS - runtime rollback knob for
// pump transport ownership. Write-only. Input buffer is AO_PUMP_FLAGS_REQ.
// In Phase 5, accepted mask bits are restricted to
// AO_PUMP_FLAG_DISABLE_LEGACY_RENDER; all other bits in SetMask/ClearMask
// are silently dropped. Handler finds the active cable render stream on
// the target miniport and atomically applies
//   flags |= (SetMask & AcceptedMask); flags &= ~(ClearMask & AcceptedMask)
// under the stream's m_PositionSpinLock. Effect is visible at the next
// PumpToCurrentPositionFromQuery() invocation (1-2 position queries).
#define IOCTL_AO_SET_PUMP_FEATURE_FLAGS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_WRITE_ACCESS)

// Input struct for IOCTL_AO_SET_PUMP_FEATURE_FLAGS.
// Both masks are applied in order: SetMask bits turn on, then ClearMask
// bits turn off. Caller may therefore pass the same bit in both fields
// to force a clear regardless of current state.
typedef struct _AO_PUMP_FLAGS_REQ {
    ULONG   SetMask;
    ULONG   ClearMask;
} AO_PUMP_FLAGS_REQ;

// Stream status for a single cable endpoint
typedef struct _AO_ENDPOINT_STATUS {
    BOOLEAN Active;
    ULONG   SampleRate;
    ULONG   BitsPerSample;
    ULONG   Channels;
} AO_ENDPOINT_STATUS;

// Stream status for both cables
typedef struct _AO_STREAM_STATUS {
    AO_ENDPOINT_STATUS CableA_Speaker;
    AO_ENDPOINT_STATUS CableA_Mic;
    AO_ENDPOINT_STATUS CableB_Speaker;
    AO_ENDPOINT_STATUS CableB_Mic;
} AO_STREAM_STATUS;

// Current driver configuration
typedef struct _AO_CONFIG {
    ULONG InternalRate;         // Internal ring buffer sample rate (default 48000)
    ULONG MaxLatencyMs;         // Max latency in ms (default 20)
    ULONG InternalBits;         // Internal bit depth (always 24)
    ULONG InternalChannels;     // Internal channel count
} AO_CONFIG;

//
// Phase 1: AO_V2_DIAG -- extended diagnostic payload returned by
// IOCTL_AO_GET_STREAM_STATUS after the AO_STREAM_STATUS block when the
// caller's output buffer is large enough.
//
// Layout contract:
//   IOCTL_AO_GET_STREAM_STATUS output =
//     [ AO_STREAM_STATUS ]   // V1, always present
//     [ AO_V2_DIAG        ]   // V2, present iff OutBufLen >= V1 + V2
//
// V1 clients (sizeof(AO_STREAM_STATUS) buffer) never see AO_V2_DIAG.
// V2 clients (sizeof(AO_STREAM_STATUS) + sizeof(AO_V2_DIAG) buffer)
// unpack both. First field is StructSize so a V2 client can detect the
// exact layout it is talking to even if AO_V2_DIAG grows in later phases.
//
// Phase 1 contract: every counter is zero-initialized at FramePipeInit
// and never written by any execution path. Phase 3 is the first phase
// that increments these. Feature flag slots are declared now, stored as
// zero, and not read by any Phase 1 code path.
//
// Field naming: <Cable>_<Direction>_<Field>
//   A = Cable A, B = Cable B
//   R = Render (Speaker stream), C = Capture (Mic stream)
//
typedef struct _AO_V2_DIAG {
    ULONG   StructSize;                     // sizeof(AO_V2_DIAG) as built

    // ----- Cable A Render -----
    ULONG   A_R_GatedSkipCount;
    ULONG   A_R_OverJumpCount;
    ULONG   A_R_FramesProcessedLow;
    ULONG   A_R_FramesProcessedHigh;
    ULONG   A_R_PumpInvocationCount;
    ULONG   A_R_PumpShadowDivergenceCount;
    ULONG   A_R_PumpFeatureFlags;

    // ----- Cable A Capture -----
    ULONG   A_C_GatedSkipCount;
    ULONG   A_C_OverJumpCount;
    ULONG   A_C_FramesProcessedLow;
    ULONG   A_C_FramesProcessedHigh;
    ULONG   A_C_PumpInvocationCount;
    ULONG   A_C_PumpShadowDivergenceCount;
    ULONG   A_C_PumpFeatureFlags;

    // ----- Cable B Render -----
    ULONG   B_R_GatedSkipCount;
    ULONG   B_R_OverJumpCount;
    ULONG   B_R_FramesProcessedLow;
    ULONG   B_R_FramesProcessedHigh;
    ULONG   B_R_PumpInvocationCount;
    ULONG   B_R_PumpShadowDivergenceCount;
    ULONG   B_R_PumpFeatureFlags;

    // ----- Cable B Capture -----
    ULONG   B_C_GatedSkipCount;
    ULONG   B_C_OverJumpCount;
    ULONG   B_C_FramesProcessedLow;
    ULONG   B_C_FramesProcessedHigh;
    ULONG   B_C_PumpInvocationCount;
    ULONG   B_C_PumpShadowDivergenceCount;
    ULONG   B_C_PumpFeatureFlags;

    // Phase 5 (2026-04-14): per-side drive counters for one-owner proof.
    // Only render-side counters are wired in Phase 5; capture-side will
    // come in Phase 6. These live at the V2 tail so Phase 1/3/4 consumers
    // that read only the first 116 bytes still work (the driver writes
    // the old layout too when the buffer is smaller).
    ULONG   A_R_PumpDriveCount;
    ULONG   A_R_LegacyDriveCount;
    ULONG   B_R_PumpDriveCount;
    ULONG   B_R_LegacyDriveCount;

    // Phase 1 Step 6 (ADR-014 phase/1-int32-ring): per-cable canonical
    // FRAME_PIPE diagnostics. Sourced under pipe->Lock from a single
    // snapshot helper so OverflowCount / UnderrunCount / UnderrunFlag /
    // RingFillFrames / WrapBoundFrames are mutually consistent.
    //
    //   OverflowCount    -- hard-reject hits per ADR-005.
    //   UnderrunCount    -- read-insufficient hits per ADR-005.
    //   RingFillFrames   -- current fill (WritePos - ReadPos, wrap-corrected).
    //                      Healthy band: a small live-latency value, well
    //                      below WrapBoundFrames. Drift toward capacity
    //                      over time is a leak.
    //   WrapBoundFrames  -- ring capacity (== TargetLatencyFrames after
    //                      reconcile_wrapbound_to_target settles).
    //   UnderrunFlag     -- UCHAR; 0 = normal, 1 = drained-recovery.
    //                      Must clear back to 0 within one fill cycle
    //                      (<= WrapBound/2 frames of writer activity).
    //
    // UnderrunFlag is UCHAR per step6.md and step5 evidence rationale
    // (counters alone cannot prove the 50%-WrapBound recovery path is
    // operating). The 3-byte Reserved pad keeps the next ULONG aligned.
    ULONG   A_OverflowCount;
    ULONG   A_UnderrunCount;
    ULONG   A_RingFillFrames;
    ULONG   A_WrapBoundFrames;
    UCHAR   A_UnderrunFlag;
    UCHAR   A_Reserved0[3];

    ULONG   B_OverflowCount;
    ULONG   B_UnderrunCount;
    ULONG   B_RingFillFrames;
    ULONG   B_WrapBoundFrames;
    UCHAR   B_UnderrunFlag;
    UCHAR   B_Reserved1[3];

    // Phase 3 Step 2 prep: per-stream shadow helper counters.
    // Sourced from AO_STREAM_RT::DbgShadow{Advance,Query,Timer}Hits via
    // AoTransportSnapshotShadowCounters() in transport_engine.cpp. The
    // "Dbg" prefix is dropped on this ABI surface; runtime fields keep
    // the prefix as a debug-build trace marker.
    //
    // Block order matches the Phase 1 V2 diag block order: A_Render,
    // A_Capture, B_Render, B_Capture. 4 streams * 3 ULONGs = 48 bytes
    // appended at the V2 tail.
    //
    // Backwards compatibility: callers that still pass a Phase 1 / Phase 5
    // / Phase 6 sized output buffer (116 / 132 / 172 bytes after V1) keep
    // working because the IOCTL_AO_GET_STREAM_STATUS handler in
    // adapter.cpp does a partial-write keyed off OutputBufferLength.
    // StructSize reports the actual bytes written so a consumer can tell
    // which tier it received.
    ULONG   A_R_ShadowAdvanceHits;
    ULONG   A_R_ShadowQueryHits;
    ULONG   A_R_ShadowTimerHits;

    ULONG   A_C_ShadowAdvanceHits;
    ULONG   A_C_ShadowQueryHits;
    ULONG   A_C_ShadowTimerHits;

    ULONG   B_R_ShadowAdvanceHits;
    ULONG   B_R_ShadowQueryHits;
    ULONG   B_R_ShadowTimerHits;

    ULONG   B_C_ShadowAdvanceHits;
    ULONG   B_C_ShadowQueryHits;
    ULONG   B_C_ShadowTimerHits;

    // Phase 3 Step 4 tail: per-stream shadow divergence counter.
    // Sourced from AO_STREAM_RT::DbgShadowDivergenceHits via
    // AoTransportSnapshotShadowCounters. Bumped only on AO_ADVANCE_QUERY
    // helper invocations (and only while the stream is Active = RUN)
    // where the helper-vs-legacy frame-anchor cumulative differs by
    // more than the rate-aware tolerance
    //   ((SampleRate + 999) / 1000) + AO_CABLE_MIN_FRAMES_GATE
    // = legacy 1 ms quantization envelope + helper 8-frame gate.
    // 48 kHz -> 56, 44.1 kHz -> 53, 96 kHz -> 104.
    // Distinct from the Phase 1 PumpShadowDivergenceCount above
    // (window-sliding pump-vs-legacy comparison, force-zeroed by
    // adapter.cpp since the source FRAME_PIPE counter was retired in
    // Phase 1).
    //
    // Block order matches the V2 diag block convention: A_Render,
    // A_Capture, B_Render, B_Capture. 4 streams * 1 ULONG = 16 bytes
    // appended at the V2 tail (P8 layout).
    //
    // Backwards compatibility: callers that pass a Phase 1 / 5 / 6 / 7
    // sized output buffer (116 / 132 / 172 / 220 bytes after V1) keep
    // working because IOCTL_AO_GET_STREAM_STATUS does partial-write keyed
    // off OutputBufferLength.
    ULONG   A_R_ShadowDivergenceCount;
    ULONG   A_C_ShadowDivergenceCount;
    ULONG   B_R_ShadowDivergenceCount;
    ULONG   B_C_ShadowDivergenceCount;
} AO_V2_DIAG;

// Compile-time shape guard. Bump this C_ASSERT whenever AO_V2_DIAG grows.
// Phase 6 layout: StructSize (1) + 4 blocks * 7 ULONGs (28) +
// 4 render-side drive counters (4) + 2 cables * 5 ring-diag ULONG-equivs (10)
// = 43 ULONGs = 172 bytes. Each cable's UCHAR UnderrunFlag + 3-byte
// Reserved pad together occupy one ULONG slot.
// Phase 3 Step 2 prep tail: 4 streams * 3 shadow-helper-counter ULONGs
// (12) = 55 ULONGs = 220 bytes (P7 layout).
// Phase 3 Step 4 tail: 4 streams * 1 ShadowDivergenceCount ULONG (4)
// = 59 ULONGs = 236 bytes (P8 layout).
C_ASSERT(sizeof(AO_V2_DIAG) ==
         (1 + 4 * 7 + 4 + 2 * 5 + 4 * 3 + 4) * sizeof(ULONG));

// Registry value names for persistent settings (stored under service Parameters key)
// e.g. HKLM\SYSTEM\CurrentControlSet\Services\AOCableA\Parameters
#define AO_REG_INTERNAL_RATE L"InternalRate"
#define AO_REG_MAX_LATENCY   L"MaxLatencyMs"
#define AO_REG_MAX_CHANNELS  L"MaxChannelCount"

#endif // _AO_VIRTUAL_CABLE_IOCTL_H_
