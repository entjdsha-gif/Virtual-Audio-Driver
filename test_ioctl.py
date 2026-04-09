"""Test if we can open the AO Virtual Cable device interface."""
import ctypes
from ctypes import wintypes
import struct

setupapi = ctypes.windll.setupapi
kernel32 = ctypes.windll.kernel32

# GUID_DEVINTERFACE_AO_VIRTUAL_CABLE {7B3E4A10-1B2C-4D5E-9F8A-0B1C2D3E4F5A}
class GUID(ctypes.Structure):
    _fields_ = [("Data1", wintypes.DWORD), ("Data2", wintypes.WORD),
                ("Data3", wintypes.WORD), ("Data4", ctypes.c_byte * 8)]

guid = GUID(0x7B3E4A10, 0x1B2C, 0x4D5E,
            (ctypes.c_byte * 8)(0x9F, 0x8A, 0x0B, 0x1C, 0x2D, 0x3E, 0x4F, 0x5A))

DIGCF_PRESENT = 0x02
DIGCF_DEVICEINTERFACE = 0x10
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value

class SP_DEVICE_INTERFACE_DATA(ctypes.Structure):
    _fields_ = [("cbSize", wintypes.DWORD), ("InterfaceClassGuid", GUID),
                ("Flags", wintypes.DWORD), ("Reserved", ctypes.POINTER(ctypes.c_ulong))]

# Get device info set
hDevInfo = setupapi.SetupDiGetClassDevsW(ctypes.byref(guid), None, None,
                                          DIGCF_PRESENT | DIGCF_DEVICEINTERFACE)
print(f"hDevInfo: {hDevInfo}")

if hDevInfo == INVALID_HANDLE_VALUE:
    print("FAIL: SetupDiGetClassDevs returned INVALID_HANDLE_VALUE")
    print(f"  GetLastError: {kernel32.GetLastError()}")

    # Try without DIGCF_PRESENT
    hDevInfo = setupapi.SetupDiGetClassDevsW(ctypes.byref(guid), None, None,
                                              DIGCF_DEVICEINTERFACE)
    print(f"Without DIGCF_PRESENT: {hDevInfo}")
    if hDevInfo == INVALID_HANDLE_VALUE:
        print("Still INVALID_HANDLE_VALUE")
        exit(1)

# Enumerate interfaces
for i in range(10):
    ifData = SP_DEVICE_INTERFACE_DATA()
    ifData.cbSize = ctypes.sizeof(SP_DEVICE_INTERFACE_DATA)

    result = setupapi.SetupDiEnumDeviceInterfaces(hDevInfo, None, ctypes.byref(guid), i, ctypes.byref(ifData))
    if not result:
        print(f"\nTotal interfaces found: {i}")
        break

    print(f"\nInterface {i}: Flags=0x{ifData.Flags:08X}")

    # Get detail
    requiredSize = wintypes.DWORD(0)
    setupapi.SetupDiGetDeviceInterfaceDetailW(hDevInfo, ctypes.byref(ifData), None, 0, ctypes.byref(requiredSize), None)

    if requiredSize.value > 0:
        buf = ctypes.create_string_buffer(requiredSize.value)
        # Set cbSize to size of fixed part (4 + pointer)
        struct.pack_into('I', buf, 0, 8)  # sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) on x64

        result = setupapi.SetupDiGetDeviceInterfaceDetailW(hDevInfo, ctypes.byref(ifData), buf, requiredSize, None, None)
        if result:
            path = buf[8:].decode('utf-16-le').rstrip('\x00')
            print(f"  Path: {path}")

            # Try to open
            GENERIC_READ = 0x80000000
            GENERIC_WRITE = 0x40000000
            OPEN_EXISTING = 3

            hFile = kernel32.CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, None, OPEN_EXISTING, 0, None)
            if hFile != INVALID_HANDLE_VALUE and hFile != 0 and hFile != -1:
                print(f"  CreateFileW: SUCCESS (handle={hFile})")
                kernel32.CloseHandle(hFile)
            else:
                err = kernel32.GetLastError()
                print(f"  CreateFileW: FAILED (error={err})")

                # Try read-only
                hFile = kernel32.CreateFileW(path, GENERIC_READ, 0, None, OPEN_EXISTING, 0, None)
                if hFile != INVALID_HANDLE_VALUE and hFile != 0 and hFile != -1:
                    print(f"  CreateFileW (read-only): SUCCESS")
                    kernel32.CloseHandle(hFile)
                else:
                    print(f"  CreateFileW (read-only): FAILED (error={kernel32.GetLastError()})")
        else:
            print(f"  GetDetail failed: {kernel32.GetLastError()}")

setupapi.SetupDiDestroyDeviceInfoList(hDevInfo)
