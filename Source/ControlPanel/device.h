/*++
Module Name:
    device.h
Abstract:
    Device communication helpers for AO Virtual Cable Control Panel.
    Opens driver control device by name and uses DeviceIoControl
    to send/receive IOCTL commands.
--*/

#ifndef _AO_CONTROLPANEL_DEVICE_H_
#define _AO_CONTROLPANEL_DEVICE_H_

#include <windows.h>
#include "ioctl.h"

// Open the Nth device instance (index = 0 or 1 for Cable A / Cable B).
// Returns INVALID_HANDLE_VALUE on failure.
HANDLE AoOpenDevice(int index);

// Retrieve current driver configuration.
BOOL AoGetConfig(HANDLE hDevice, AO_CONFIG* pConfig);

// Set the internal ring buffer sample rate (44100, 48000, 96000, 192000).
BOOL AoSetInternalRate(HANDLE hDevice, ULONG rate);

// Set max latency in milliseconds (5 .. 100).
BOOL AoSetMaxLatency(HANDLE hDevice, ULONG ms);

// Get per-endpoint stream status for all 4 endpoints.
BOOL AoGetStreamStatus(HANDLE hDevice, AO_STREAM_STATUS* pStatus);

#endif // _AO_CONTROLPANEL_DEVICE_H_
