"""
AO Virtual Cable - IOCTL GET_CONFIG Diagnostic
Directly tests IOCTL communication with the driver's control device.
Run as Administrator.
"""
import ctypes
import ctypes.wintypes as wt
import struct
import sys
import winreg

kernel32 = ctypes.windll.kernel32

# IOCTL codes (CTL_CODE macro expansion)
# CTL_CODE(FILE_DEVICE_UNKNOWN=0x22, Function, METHOD_BUFFERED=0, Access)
def CTL_CODE(func, access):
    return (0x22 << 16) | (access << 14) | (func << 2) | 0

IOCTL_AO_SET_INTERNAL_RATE = CTL_CODE(0x800, 2)   # FILE_WRITE_ACCESS
IOCTL_AO_SET_MAX_LATENCY   = CTL_CODE(0x801, 2)
IOCTL_AO_GET_STREAM_STATUS = CTL_CODE(0x802, 1)   # FILE_READ_ACCESS
IOCTL_AO_GET_CONFIG        = CTL_CODE(0x803, 1)

GENERIC_READ  = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3

def open_device(name):
    h = kernel32.CreateFileW(
        name, GENERIC_READ | GENERIC_WRITE, 0, None, OPEN_EXISTING, 0, None)
    if h == -1 or h == 0xFFFFFFFF or h == ctypes.c_void_p(-1).value:
        return None, kernel32.GetLastError()
    return h, 0

def ioctl_get_config(h):
    buf = ctypes.create_string_buffer(16)
    ret = ctypes.c_ulong(0)
    ok = kernel32.DeviceIoControl(
        h, IOCTL_AO_GET_CONFIG, None, 0, buf, 16, ctypes.byref(ret), None)
    if not ok:
        return None, kernel32.GetLastError()
    rate, latency, bits, channels = struct.unpack('<IIII', buf.raw)
    return {'Rate': rate, 'Latency': latency, 'Bits': bits, 'Channels': channels, 'BytesReturned': ret.value}, 0

def ioctl_set_rate(h, rate):
    inp = struct.pack('<I', rate)
    inp_buf = ctypes.create_string_buffer(inp)
    ret = ctypes.c_ulong(0)
    ok = kernel32.DeviceIoControl(
        h, IOCTL_AO_SET_INTERNAL_RATE, inp_buf, 4, None, 0, ctypes.byref(ret), None)
    return ok, kernel32.GetLastError() if not ok else 0

def ioctl_set_latency(h, ms):
    inp = struct.pack('<I', ms)
    inp_buf = ctypes.create_string_buffer(inp)
    ret = ctypes.c_ulong(0)
    ok = kernel32.DeviceIoControl(
        h, IOCTL_AO_SET_MAX_LATENCY, inp_buf, 4, None, 0, ctypes.byref(ret), None)
    return ok, kernel32.GetLastError() if not ok else 0

def read_registry(svc_name):
    """Read InternalRate, MaxLatencyMs, and MaxChannelCount from service Parameters key."""
    try:
        key = winreg.OpenKey(
            winreg.HKEY_LOCAL_MACHINE,
            rf"SYSTEM\CurrentControlSet\Services\{svc_name}\Parameters",
            0, winreg.KEY_READ)
        rate = winreg.QueryValueEx(key, "InternalRate")[0]
        latency = winreg.QueryValueEx(key, "MaxLatencyMs")[0]
        try:
            channels = winreg.QueryValueEx(key, "MaxChannelCount")[0]
        except FileNotFoundError:
            channels = 8  # default when key absent
        winreg.CloseKey(key)
        return {'InternalRate': rate, 'MaxLatencyMs': latency, 'MaxChannelCount': channels}
    except FileNotFoundError:
        return None
    except OSError as e:
        return f"Error: {e}"

def run_diag(device_path, svc_name, label):
    print(f"\n{'='*50}")
    print(f" {label}: {device_path}")
    print(f"{'='*50}")

    # Step 1: Open device
    h, err = open_device(device_path)
    if h is None:
        print(f"  [FAIL] CreateFileW failed (error={err})")
        return False
    print(f"  [OK] Device opened (handle={h})")

    # Step 2: GET_CONFIG (before SET)
    cfg, err = ioctl_get_config(h)
    if cfg is None:
        print(f"  [FAIL] GET_CONFIG failed (error={err})")
        kernel32.CloseHandle(h)
        return False
    print(f"  [GET_CONFIG] Rate={cfg['Rate']}, Latency={cfg['Latency']}, "
          f"Bits={cfg['Bits']}, Channels={cfg['Channels']} "
          f"({cfg['BytesReturned']} bytes returned)")

    bug_detected = (cfg['Rate'] == 0 and cfg['Latency'] == 0)
    if bug_detected:
        print(f"  [!!] Rate=0, Latency=0 -> BUG or old driver version")
    elif cfg['Rate'] > 0 and cfg['Latency'] > 0:
        print(f"  [OK] Values look valid")

    # Step 3: SET rate=96000, latency=10
    print(f"\n  --- SET_INTERNAL_RATE=96000 ---")
    ok, err = ioctl_set_rate(h, 96000)
    print(f"  [{'OK' if ok else 'FAIL'}] SET_INTERNAL_RATE {'succeeded' if ok else f'failed (error={err})'}")

    print(f"  --- SET_MAX_LATENCY=10 ---")
    ok, err = ioctl_set_latency(h, 10)
    print(f"  [{'OK' if ok else 'FAIL'}] SET_MAX_LATENCY {'succeeded' if ok else f'failed (error={err})'}")

    # Step 4: GET_CONFIG (after SET)
    cfg2, err = ioctl_get_config(h)
    if cfg2 is None:
        print(f"  [FAIL] GET_CONFIG after SET failed (error={err})")
    else:
        print(f"  [GET_CONFIG] Rate={cfg2['Rate']}, Latency={cfg2['Latency']}, "
              f"Bits={cfg2['Bits']}, Channels={cfg2['Channels']}")

        if cfg2['Rate'] == 96000 and cfg2['Latency'] == 10:
            print(f"  [OK] SET -> GET roundtrip PASSED")
        elif cfg2['Rate'] == 0 and cfg2['Latency'] == 0:
            print(f"  [FAIL] Still (0,0) after SET -> driver code bug or old binary")
        else:
            print(f"  [WARN] Unexpected values after SET")

    # Step 5: Restore defaults
    ioctl_set_rate(h, 48000)
    ioctl_set_latency(h, 20)

    kernel32.CloseHandle(h)

    # Step 6: Registry check
    print(f"\n  --- Registry: Services\\{svc_name}\\Parameters ---")
    reg = read_registry(svc_name)
    if reg is None:
        print(f"  [INFO] Key not found (driver may not have written yet)")
    elif isinstance(reg, str):
        print(f"  [WARN] {reg}")
    else:
        print(f"  [REG] InternalRate={reg['InternalRate']}, MaxLatencyMs={reg['MaxLatencyMs']}, MaxChannelCount={reg['MaxChannelCount']}")

    return not bug_detected

# ============================================================
# Main
# ============================================================
print("AO Virtual Cable - IOCTL GET_CONFIG Diagnostic")
print(f"IOCTL codes: GET_CONFIG=0x{IOCTL_AO_GET_CONFIG:08X}, "
      f"SET_RATE=0x{IOCTL_AO_SET_INTERNAL_RATE:08X}, "
      f"SET_LATENCY=0x{IOCTL_AO_SET_MAX_LATENCY:08X}")

# Check installed driver file info
import os
for name in ['aocablea.sys', 'aocableb.sys']:
    path = os.path.join(os.environ['SystemRoot'], 'System32', 'drivers', name)
    if os.path.exists(path):
        stat = os.stat(path)
        print(f"  Installed {name}: {stat.st_size} bytes, modified {stat.st_mtime:.0f}")

ok_a = run_diag(r"\\.\AOCableA", "AOCableA", "Cable A")
ok_b = run_diag(r"\\.\AOCableB", "AOCableB", "Cable B")

print(f"\n{'='*50}")
if ok_a and ok_b:
    print(" RESULT: ALL PASSED - GET_CONFIG returns valid values")
else:
    print(" RESULT: ISSUE DETECTED - see details above")
    print(" Next: reinstall driver with latest build and re-run this test")
print(f"{'='*50}")
