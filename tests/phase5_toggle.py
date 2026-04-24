"""
Phase 5 live A/B toggle. Press ENTER to flip
AO_PUMP_FLAG_DISABLE_LEGACY_RENDER on the active cable render stream.
Use during a real Phone Link call to compare pump vs legacy quality by
ear. 'q' + ENTER to quit.
"""

import ctypes
import struct
import sys

kernel32 = ctypes.windll.kernel32


def CTL_CODE(func, access):
    return (0x22 << 16) | (access << 14) | (func << 2) | 0


IOCTL_SET = CTL_CODE(0x806, 2)
GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3
FLAG_RENDER = 0x00000004


def main():
    h = kernel32.CreateFileW(
        r"\\.\AOCableA", GENERIC_READ | GENERIC_WRITE, 0, None, OPEN_EXISTING, 0, None
    )
    if h == -1 or h == 0xFFFFFFFF:
        print("ERROR: cannot open \\.\\AOCableA")
        return 1

    pump_owned = True
    print("Start: PUMP-OWNED (flag=0x07)")
    print("ENTER to toggle, 'q'+ENTER to quit")

    try:
        while True:
            cmd = input("> ").strip().lower()
            if cmd == "q":
                break
            if pump_owned:
                buf = struct.pack("<II", 0, FLAG_RENDER)  # clear render
                label = "LEGACY-OWNED (flag=0x03, ReadBytes path)"
            else:
                buf = struct.pack("<II", FLAG_RENDER, 0)  # set render
                label = "PUMP-OWNED   (flag=0x07, pump transport)"
            buf_c = ctypes.create_string_buffer(buf, len(buf))
            ret = ctypes.c_ulong(0)
            ok = kernel32.DeviceIoControl(
                h, IOCTL_SET, buf_c, len(buf), None, 0, ctypes.byref(ret), None
            )
            if ok:
                pump_owned = not pump_owned
                print(f"  -> {label}")
            else:
                print(f"  IOCTL failed err={ctypes.get_last_error()}")
    finally:
        kernel32.CloseHandle(h)
    return 0


if __name__ == "__main__":
    sys.exit(main())
