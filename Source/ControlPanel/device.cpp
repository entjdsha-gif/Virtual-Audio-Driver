/*++
Module Name:
    device.cpp
Abstract:
    Device communication implementation for AO Virtual Cable Control Panel.
    Opens the driver's control device by symbolic link name and uses
    DeviceIoControl for IOCTL communication with the kernel driver.
--*/

#include <windows.h>
#include "device.h"

// Control device symbolic link names (must match driver's adapter.cpp)
static const WCHAR* g_DeviceNames[] = {
    L"\\\\.\\AOCableA",
    L"\\\\.\\AOCableB",
};

HANDLE
AoOpenDevice(int index)
{
    if (index < 0 || index >= _countof(g_DeviceNames))
        return INVALID_HANDLE_VALUE;

    return CreateFileW(
        g_DeviceNames[index],
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);
}

BOOL
AoGetConfig(HANDLE hDevice, AO_CONFIG* pConfig)
{
    if (hDevice == INVALID_HANDLE_VALUE || !pConfig) {
        return FALSE;
    }

    DWORD bytesReturned = 0;
    return DeviceIoControl(
        hDevice,
        IOCTL_AO_GET_CONFIG,
        NULL,
        0,
        pConfig,
        sizeof(AO_CONFIG),
        &bytesReturned,
        NULL);
}

BOOL
AoSetInternalRate(HANDLE hDevice, ULONG rate)
{
    if (hDevice == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    DWORD bytesReturned = 0;
    return DeviceIoControl(
        hDevice,
        IOCTL_AO_SET_INTERNAL_RATE,
        &rate,
        sizeof(rate),
        NULL,
        0,
        &bytesReturned,
        NULL);
}

BOOL
AoSetMaxLatency(HANDLE hDevice, ULONG ms)
{
    if (hDevice == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    DWORD bytesReturned = 0;
    return DeviceIoControl(
        hDevice,
        IOCTL_AO_SET_MAX_LATENCY,
        &ms,
        sizeof(ms),
        NULL,
        0,
        &bytesReturned,
        NULL);
}

BOOL
AoGetStreamStatus(HANDLE hDevice, AO_STREAM_STATUS* pStatus)
{
    if (hDevice == INVALID_HANDLE_VALUE || !pStatus) {
        return FALSE;
    }

    DWORD bytesReturned = 0;
    return DeviceIoControl(
        hDevice,
        IOCTL_AO_GET_STREAM_STATUS,
        NULL,
        0,
        pStatus,
        sizeof(AO_STREAM_STATUS),
        &bytesReturned,
        NULL);
}

BOOL
AoSetMaxChannels(HANDLE hDevice, ULONG channels)
{
    if (hDevice == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    DWORD bytesReturned = 0;
    return DeviceIoControl(
        hDevice,
        IOCTL_AO_SET_MAX_CHANNELS,
        &channels,
        sizeof(channels),
        NULL,
        0,
        &bytesReturned,
        NULL);
}
