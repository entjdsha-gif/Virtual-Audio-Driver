"""
AO Virtual Cable - Live Stream Monitor (V2)

Polls GET_STREAM_STATUS and GET_CONFIG via IOCTL to show
negotiated format, SessionPassthrough state, and loss counters.

Run as Administrator.

Usage:
    python test_stream_monitor.py              # poll every 1s until Ctrl+C
    python test_stream_monitor.py --once       # single snapshot
    python test_stream_monitor.py --interval 2 # poll every 2s
"""

import ctypes
import ctypes.wintypes as wt
import struct
import sys
import time
import argparse

kernel32 = ctypes.windll.kernel32

def CTL_CODE(func, access):
    return (0x22 << 16) | (access << 14) | (func << 2) | 0

IOCTL_AO_GET_CONFIG        = CTL_CODE(0x803, 1)
IOCTL_AO_GET_STREAM_STATUS = CTL_CODE(0x802, 1)

GENERIC_READ  = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3

# AO_STREAM_STATUS = 64 bytes (4 endpoints x 16 bytes).
# Phase 1 AO_V2_DIAG = 116 bytes, appended directly after AO_STREAM_STATUS
# when the caller requests a buffer large enough. Layout (from ioctl.h):
#   ULONG StructSize                                  (4 bytes)
#   4 blocks * 7 ULONGs each = 28 ULONGs             (112 bytes)
#     block order: A_Render, A_Capture, B_Render, B_Capture
#     field order: GatedSkipCount, OverJumpCount,
#                  FramesProcessedLow, FramesProcessedHigh,
#                  PumpInvocationCount, PumpShadowDivergenceCount,
#                  PumpFeatureFlags
V1_STATUS_SIZE = 64
# Phase 1 V2 diag: StructSize + 4 * 7 ULONGs = 116 bytes.
# Phase 5 tail extension: +4 ULONGs (A_R/B_R pump/legacy drive counts) = 132.
V2_DIAG_SIZE_P1  = 4 + 4 * 7 * 4                 # 116
V2_DIAG_SIZE     = V2_DIAG_SIZE_P1 + 4 * 4       # 132
V2_BUF_SIZE      = V1_STATUS_SIZE + V2_DIAG_SIZE
V2_FIELDS_PER_BLOCK = 7


def open_device(name):
    h = kernel32.CreateFileW(
        name, GENERIC_READ | GENERIC_WRITE, 0, None, OPEN_EXISTING, 0, None)
    if h == -1 or h == 0xFFFFFFFF or h == ctypes.c_void_p(-1).value:
        return None
    return h


def get_config(h):
    buf = ctypes.create_string_buffer(16)
    ret = ctypes.c_ulong(0)
    ok = kernel32.DeviceIoControl(
        h, IOCTL_AO_GET_CONFIG, None, 0, buf, 16, ctypes.byref(ret), None)
    if not ok:
        return None
    rate, latency, bits, channels = struct.unpack('<IIII', buf.raw)
    return {'rate': rate, 'latency_ms': latency, 'bits': bits, 'channels': channels}


def get_stream_status(h):
    # Phase 1: request V1 + V2 in one call. Driver writes V1 if the buffer
    # only holds V1, and V1 + V2 if the buffer is large enough.
    buf_size = V2_BUF_SIZE
    buf = ctypes.create_string_buffer(buf_size)
    ret = ctypes.c_ulong(0)
    ok = kernel32.DeviceIoControl(
        h, IOCTL_AO_GET_STREAM_STATUS, None, 0, buf, buf_size, ctypes.byref(ret), None)
    if not ok:
        return None

    # Parse V1: 4 endpoints, each = (BOOLEAN padded to ULONG, ULONG, ULONG, ULONG) = 16 bytes
    endpoints = []
    for i in range(4):
        offset = i * 16
        active, rate, bits, ch = struct.unpack_from('<IIII', buf.raw, offset)
        endpoints.append({
            'active': bool(active),
            'rate': rate,
            'bits': bits,
            'channels': ch,
        })

    result = {
        'CableA_Speaker': endpoints[0],
        'CableA_Mic':     endpoints[1],
        'CableB_Speaker': endpoints[2],
        'CableB_Mic':     endpoints[3],
    }

    # Phase 1 AO_V2_DIAG — 4 blocks of 7 ULONGs each, in order:
    #   A_Render, A_Capture, B_Render, B_Capture.
    # Each block unpacks into:
    #   GatedSkipCount, OverJumpCount,
    #   FramesProcessedLow, FramesProcessedHigh,
    #   PumpInvocationCount, PumpShadowDivergenceCount,
    #   PumpFeatureFlags
    result['v2'] = False
    # Accept either Phase 1 (116) or Phase 5 (132) tail, whichever the
    # driver reports. Phase 5 clients see the full tail and 4 extra
    # render-side drive counters; older drivers return the Phase 1 shape.
    if ret.value >= V1_STATUS_SIZE + V2_DIAG_SIZE_P1:
        v2_offset = V1_STATUS_SIZE
        struct_size = struct.unpack_from('<I', buf.raw, v2_offset)[0]

        if struct_size == V2_DIAG_SIZE_P1 or struct_size == V2_DIAG_SIZE:
            def read_block(offset):
                fields = struct.unpack_from('<IIIIIII', buf.raw, offset)
                return {
                    'GatedSkipCount':            fields[0],
                    'OverJumpCount':             fields[1],
                    'FramesProcessedLow':        fields[2],
                    'FramesProcessedHigh':       fields[3],
                    'PumpInvocationCount':       fields[4],
                    'PumpShadowDivergenceCount': fields[5],
                    'PumpFeatureFlags':          fields[6],
                    'FramesProcessedTotal':      fields[2] | (fields[3] << 32),
                }

            cursor = v2_offset + 4  # skip StructSize
            block_bytes = V2_FIELDS_PER_BLOCK * 4
            result['v2'] = True
            result['v2_struct_size'] = struct_size
            result['CableA_Render']  = read_block(cursor); cursor += block_bytes
            result['CableA_Capture'] = read_block(cursor); cursor += block_bytes
            result['CableB_Render']  = read_block(cursor); cursor += block_bytes
            result['CableB_Capture'] = read_block(cursor); cursor += block_bytes

            # Phase 5 tail: 4 ULONGs of render-side drive counters.
            # Present iff the driver returned the 132-byte shape.
            if struct_size == V2_DIAG_SIZE:
                a_r_pump, a_r_legacy, b_r_pump, b_r_legacy = struct.unpack_from(
                    '<IIII', buf.raw, cursor)
                result['CableA_Render']['RenderPumpDriveCount']   = a_r_pump
                result['CableA_Render']['RenderLegacyDriveCount'] = a_r_legacy
                result['CableB_Render']['RenderPumpDriveCount']   = b_r_pump
                result['CableB_Render']['RenderLegacyDriveCount'] = b_r_legacy
            else:
                for key in ('CableA_Render', 'CableB_Render'):
                    result[key]['RenderPumpDriveCount']   = None
                    result[key]['RenderLegacyDriveCount'] = None

    return result


def format_endpoint(ep):
    if not ep['active']:
        return "inactive"
    return f"{ep['rate']}Hz/{ep['bits']}bit/{ep['channels']}ch"


# Track previous values for delta calculation
_prev = {}

def print_status(cable_label, handle):
    cfg = get_config(handle)
    status = get_stream_status(handle)

    if not cfg or not status:
        print(f"  {cable_label}: IOCTL failed")
        return

    print(f"  {cable_label} config: internal={cfg['rate']}Hz/{cfg['bits']}bit/{cfg['channels']}ch latency={cfg['latency_ms']}ms")

    for name, ep in status.items():
        if not isinstance(ep, dict):
            continue
        # Only V1 endpoint rows have an 'active' key. Skip V2 diag blocks
        # (CableA_Render, CableA_Capture, ...) which carry pump counters.
        if 'active' not in ep:
            continue
        if cable_label == "CableA" and not name.startswith("CableA"):
            continue
        if cable_label == "CableB" and not name.startswith("CableB"):
            continue
        tag = "SPK" if "Speaker" in name else "MIC"
        print(f"    {tag}: {format_endpoint(ep)}")

    if status.get('v2'):
        # Phase 1 counter display: per-direction block for this cable.
        # All counters stay at 0 in Phase 1; Phase 3 starts populating.
        render = status.get(f"{cable_label}_Render", {})
        capture = status.get(f"{cable_label}_Capture", {})

        def fmt_block(tag, block, is_render=False):
            gs = block.get('GatedSkipCount', 0)
            oj = block.get('OverJumpCount', 0)
            fp = block.get('FramesProcessedTotal', 0)
            inv = block.get('PumpInvocationCount', 0)
            div = block.get('PumpShadowDivergenceCount', 0)
            flags = block.get('PumpFeatureFlags', 0)
            # Shadow divergence ratio is the Phase 3 exit-criterion metric.
            ratio = (div / inv * 100.0) if inv > 0 else 0.0
            line = (f"    {tag}: Gated={gs} OverJump={oj} Frames={fp} "
                    f"Inv={inv} Div={div} ({ratio:.2f}%) Flags=0x{flags:08x}")
            if is_render:
                # Phase 5 render-side drive counters (may be None if the
                # driver is still on Phase 1/3/4 layout).
                pump_drv = block.get('RenderPumpDriveCount')
                leg_drv  = block.get('RenderLegacyDriveCount')
                if pump_drv is not None and leg_drv is not None:
                    line += f" | PumpDrv={pump_drv} LegacyDrv={leg_drv}"
            return line

        print(fmt_block("Render ", render, is_render=True))
        print(fmt_block("Capture", capture))
    else:
        print(f"    (V2 diag not available — expected {V2_DIAG_SIZE} bytes at offset {V1_STATUS_SIZE})")


def main():
    parser = argparse.ArgumentParser(description="AO Cable Live Stream Monitor (V2)")
    parser.add_argument("--once", action="store_true", help="Single snapshot")
    parser.add_argument("--interval", type=float, default=1.0, help="Poll interval in seconds")
    args = parser.parse_args()

    hA = open_device(r"\\.\AOCableA")
    hB = open_device(r"\\.\AOCableB")

    if not hA and not hB:
        print("ERROR: Cannot open any AO control device. Run as Administrator.")
        sys.exit(1)

    print("AO Virtual Cable - Live Stream Monitor (V2)")
    print("Press Ctrl+C to stop\n")

    try:
        while True:
            ts = time.strftime("%H:%M:%S")
            print(f"[{ts}]")
            if hA:
                print_status("CableA", hA)
            if hB:
                print_status("CableB", hB)
            print()

            if args.once:
                break

            time.sleep(args.interval)

    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        if hA:
            kernel32.CloseHandle(hA)
        if hB:
            kernel32.CloseHandle(hB)


if __name__ == "__main__":
    main()
