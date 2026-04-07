/*++
Module Name:
    device.cpp
Abstract:
    Device communication implementation for AO Virtual Cable Control Panel.
    Uses SetupDi API to enumerate device interfaces and DeviceIoControl
    for IOCTL communication with the kernel driver.
--*/

// initguid.h MUST come before any header that uses DEFINE_GUID
#include <windows.h>
#include <initguid.h>

// Force GUID storage emission: undefine guard, re-include ioctl.h
#undef _AO_VIRTUAL_CABLE_IOCTL_H_
#include "ioctl.h"

#include "device.h"
#include <setupapi.h>

#pragma comment(lib, "setupapi.lib")

HANDLE
AoOpenDevice(int index)
{
    HDEVINFO devInfo = SetupDiGetClassDevsW(
        &GUID_DEVINTERFACE_AO_VIRTUAL_CABLE,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (devInfo == INVALID_HANDLE_VALUE) {
        return INVALID_HANDLE_VALUE;
    }

    SP_DEVICE_INTERFACE_DATA ifaceData = {};
    ifaceData.cbSize = sizeof(ifaceData);

    if (!SetupDiEnumDeviceInterfaces(
            devInfo,
            NULL,
            &GUID_DEVINTERFACE_AO_VIRTUAL_CABLE,
            (DWORD)index,
            &ifaceData))
    {
        SetupDiDestroyDeviceInfoList(devInfo);
        return INVALID_HANDLE_VALUE;
    }

    // First call to get required size.
    DWORD requiredSize = 0;
    SetupDiGetDeviceInterfaceDetailW(
        devInfo, &ifaceData, NULL, 0, &requiredSize, NULL);

    if (requiredSize == 0) {
        SetupDiDestroyDeviceInfoList(devInfo);
        return INVALID_HANDLE_VALUE;
    }

    // Allocate and fill detail data.
    PSP_DEVICE_INTERFACE_DETAIL_DATA_W detailData =
        (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)HeapAlloc(
            GetProcessHeap(), HEAP_ZERO_MEMORY, requiredSize);

    if (!detailData) {
        SetupDiDestroyDeviceInfoList(devInfo);
        return INVALID_HANDLE_VALUE;
    }

    detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

    BOOL ok = SetupDiGetDeviceInterfaceDetailW(
        devInfo, &ifaceData, detailData, requiredSize, NULL, NULL);

    HANDLE hDevice = INVALID_HANDLE_VALUE;

    if (ok) {
        hDevice = CreateFileW(
            detailData->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);
    }

    HeapFree(GetProcessHeap(), 0, detailData);
    SetupDiDestroyDeviceInfoList(devInfo);

    return hDevice;
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
