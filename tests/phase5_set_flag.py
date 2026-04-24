"""One-shot render ownership flag setter. Usage: phase5_set_flag.py pump|legacy"""
import ctypes, struct, sys
k = ctypes.windll.kernel32
CTL = lambda f, a: (0x22 << 16) | (a << 14) | (f << 2) | 0
IOCTL = CTL(0x806, 2)
FLAG = 0x00000004
mode = sys.argv[1] if len(sys.argv) > 1 else "pump"
h = k.CreateFileW(r"\\.\AOCableA", 0xC0000000, 0, None, 3, 0, None)
if h in (-1, 0xFFFFFFFF):
    print("ERROR: open failed"); sys.exit(1)
if mode == "legacy":
    buf = struct.pack("<II", 0, FLAG)  # clear
elif mode == "pump":
    buf = struct.pack("<II", FLAG, 0)  # set
else:
    print("usage: pump|legacy"); sys.exit(2)
b = ctypes.create_string_buffer(buf, len(buf))
ret = ctypes.c_ulong(0)
ok = k.DeviceIoControl(h, IOCTL, b, len(b), None, 0, ctypes.byref(ret), None)
k.CloseHandle(h)
print(f"{'OK' if ok else 'FAIL'} -> {mode}")
sys.exit(0 if ok else 3)
