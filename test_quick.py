"""Minimal SET->GET->Registry test without restoring defaults."""
import ctypes, struct, winreg

kernel32 = ctypes.windll.kernel32

def CTL_CODE(func, access):
    return (0x22 << 16) | (access << 14) | (func << 2) | 0

IOCTL_AO_GET_CONFIG = CTL_CODE(0x803, 1)
IOCTL_AO_SET_INTERNAL_RATE = CTL_CODE(0x800, 2)
IOCTL_AO_SET_MAX_LATENCY = CTL_CODE(0x801, 2)

def get_config(h):
    buf = ctypes.create_string_buffer(16)
    ret = ctypes.c_ulong(0)
    ok = kernel32.DeviceIoControl(h, IOCTL_AO_GET_CONFIG, None, 0, buf, 16, ctypes.byref(ret), None)
    if not ok:
        return None
    return struct.unpack('<IIII', buf.raw)

def set_rate(h, rate):
    inp = ctypes.create_string_buffer(struct.pack('<I', rate))
    ret = ctypes.c_ulong(0)
    return kernel32.DeviceIoControl(h, IOCTL_AO_SET_INTERNAL_RATE, inp, 4, None, 0, ctypes.byref(ret), None)

def set_latency(h, ms):
    inp = ctypes.create_string_buffer(struct.pack('<I', ms))
    ret = ctypes.c_ulong(0)
    return kernel32.DeviceIoControl(h, IOCTL_AO_SET_MAX_LATENCY, inp, 4, None, 0, ctypes.byref(ret), None)

def read_reg(svc):
    try:
        key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
            rf"SYSTEM\CurrentControlSet\Services\{svc}\Parameters", 0, winreg.KEY_READ)
        rate = winreg.QueryValueEx(key, "InternalRate")[0]
        lat = winreg.QueryValueEx(key, "MaxLatencyMs")[0]
        winreg.CloseKey(key)
        return rate, lat
    except Exception as e:
        return str(e)

# Open device
h = kernel32.CreateFileW("\\\\.\\AOCableA", 0xC0000000, 0, None, 3, 0, None)
if h == -1 or h == 0xFFFFFFFF:
    print(f"FAIL: CreateFile error={kernel32.GetLastError()}")
    exit(1)
print(f"Device opened: handle={h}")

# Step 1: GET before SET
cfg = get_config(h)
print(f"1. GET before SET: Rate={cfg[0]}, Lat={cfg[1]}, Bits={cfg[2]}, Ch={cfg[3]}")

# Step 2: Registry before SET
reg = read_reg("AOCableA")
print(f"2. Registry before SET: {reg}")

# Step 3: SET rate=96000, latency=5
ok_r = set_rate(h, 96000)
ok_l = set_latency(h, 5)
print(f"3. SET rate=96000: ok={ok_r}, SET latency=5: ok={ok_l}")

# Step 4: GET after SET (NO RESTORE)
cfg2 = get_config(h)
print(f"4. GET after SET: Rate={cfg2[0]}, Lat={cfg2[1]}, Bits={cfg2[2]}, Ch={cfg2[3]}")

# Step 5: Registry after SET (NO RESTORE)
reg2 = read_reg("AOCableA")
print(f"5. Registry after SET: {reg2}")

# Step 6: Diagnosis
if cfg2[0] == 96000 and cfg2[1] == 5:
    print("\n=> PASS: GET_CONFIG reflects SET values")
elif cfg2[0] == 0:
    if reg2 == (96000, 5):
        print("\n=> BUG: SET writes registry OK, but GET_CONFIG reads 0 from struct")
    elif reg2 == (48000, 20):
        print("\n=> BUG: SET doesn't write registry either -> SET handler not reached?")
    else:
        print(f"\n=> BUG: unexpected registry state: {reg2}")

# Restore defaults
set_rate(h, 48000)
set_latency(h, 20)
print("\n(Restored defaults: 48000/20)")

kernel32.CloseHandle(h)
