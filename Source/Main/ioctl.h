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

// Registry value names for persistent settings (stored under service Parameters key)
// e.g. HKLM\SYSTEM\CurrentControlSet\Services\AOCableA\Parameters
#define AO_REG_INTERNAL_RATE L"InternalRate"
#define AO_REG_MAX_LATENCY   L"MaxLatencyMs"
#define AO_REG_MAX_CHANNELS  L"MaxChannelCount"

#endif // _AO_VIRTUAL_CABLE_IOCTL_H_
