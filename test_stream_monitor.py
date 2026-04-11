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

# AO_STREAM_STATUS = 64 bytes (4 endpoints x 16 bytes)
# AO_V2_DIAG starts at offset 64 when driver supports it
V1_STATUS_SIZE = 64


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
    buf_size = 512  # generous for V2 extension
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

    # Parse V2 extension: AO_V2_DIAG at offset 64
    # Layout: StructSize(4) + CableA(BOOL+pad=4, 6xULONG=24) + CableB(BOOL+pad=4, 6xULONG=24)
    # Total AO_V2_DIAG = 4 + 28 + 28 = 60 bytes
    # Check if V2 data is present by verifying StructSize
    if ret.value > V1_STATUS_SIZE + 4:
        v2_offset = V1_STATUS_SIZE
        struct_size = struct.unpack_from('<I', buf.raw, v2_offset)[0]

        if struct_size > 0 and struct_size <= 256:  # sanity check
            # CableA: BOOLEAN(pad4) + 7xULONG + pad4 + 2xULONGLONG + 2xULONG
            off = v2_offset + 4
            a_pt = bool(struct.unpack_from('<I', buf.raw, off)[0]); off += 4
            a_rc, a_push, a_pull, a_cof, a_csf, a_pipe, a_fill = struct.unpack_from('<IIIIIII', buf.raw, off); off += 28
            off += 4  # padding for ULONGLONG alignment
            a_spk_w, a_mic_r = struct.unpack_from('<QQ', buf.raw, off); off += 16
            a_maxdpc, a_pjump = struct.unpack_from('<II', buf.raw, off); off += 8
            # CableB: same layout
            b_pt = bool(struct.unpack_from('<I', buf.raw, off)[0]); off += 4
            b_rc, b_push, b_pull, b_cof, b_csf, b_pipe, b_fill = struct.unpack_from('<IIIIIII', buf.raw, off); off += 28
            # CableB offset 96 is already 8-byte aligned, no padding needed
            b_spk_w, b_mic_r = struct.unpack_from('<QQ', buf.raw, off); off += 16
            b_maxdpc, b_pjump = struct.unpack_from('<II', buf.raw, off)

            result['v2'] = True
            result['CableA_Passthrough'] = a_pt
            result['CableA_RenderCount'] = a_rc
            result['CableA_PushLoss'] = a_push
            result['CableA_PullLoss'] = a_pull
            result['CableA_ConvOF'] = a_cof
            result['CableA_ConvSF'] = a_csf
            result['CableA_PipeFrames'] = a_pipe
            result['CableA_PipeFill'] = a_fill
            result['CableA_SpkWrite'] = a_spk_w
            result['CableA_MicRead'] = a_mic_r
            result['CableA_MaxDpcUs'] = a_maxdpc
            result['CableA_PosJump'] = a_pjump
            result['CableB_Passthrough'] = b_pt
            result['CableB_RenderCount'] = b_rc
            result['CableB_PushLoss'] = b_push
            result['CableB_PullLoss'] = b_pull
            result['CableB_ConvOF'] = b_cof
            result['CableB_ConvSF'] = b_csf
            result['CableB_PipeFrames'] = b_pipe
            result['CableB_PipeFill'] = b_fill
            result['CableB_SpkWrite'] = b_spk_w
            result['CableB_MicRead'] = b_mic_r
            result['CableB_MaxDpcUs'] = b_maxdpc
            result['CableB_PosJump'] = b_pjump
        else:
            result['v2'] = False
    else:
        result['v2'] = False

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
        if cable_label == "CableA" and not name.startswith("CableA"):
            continue
        if cable_label == "CableB" and not name.startswith("CableB"):
            continue
        tag = "SPK" if "Speaker" in name else "MIC"
        print(f"    {tag}: {format_endpoint(ep)}")

    if status.get('v2'):
        pt = status.get(f"{cable_label}_Passthrough", False)
        rc = status.get(f"{cable_label}_RenderCount", 0)
        push = status.get(f"{cable_label}_PushLoss", 0)
        pull = status.get(f"{cable_label}_PullLoss", 0)
        cof = status.get(f"{cable_label}_ConvOF", 0)
        csf = status.get(f"{cable_label}_ConvSF", 0)
        pipe = status.get(f"{cable_label}_PipeFrames", 0)
        fill = status.get(f"{cable_label}_PipeFill", 0)
        pt_str = "YES" if pt else "NO"

        # Delta calculation
        prev_pull = _prev.get(f"{cable_label}_pull", 0)
        d_pull = pull - prev_pull
        _prev[f"{cable_label}_pull"] = pull

        fill_pct = f"{fill*100//pipe}%" if pipe > 0 else "?"
        spk_state = "WRITE" if fill > 0 or d_pull == 0 else "IDLE"

        spk_w = status.get(f"{cable_label}_SpkWrite", 0)
        mic_r = status.get(f"{cable_label}_MicRead", 0)
        maxdpc = status.get(f"{cable_label}_MaxDpcUs", 0)
        pjump = status.get(f"{cable_label}_PosJump", 0)

        # Write/Read deltas
        prev_w = _prev.get(f"{cable_label}_w", 0)
        prev_r = _prev.get(f"{cable_label}_r", 0)
        d_w = spk_w - prev_w
        d_r = mic_r - prev_r
        _prev[f"{cable_label}_w"] = spk_w
        _prev[f"{cable_label}_r"] = mic_r

        print(f"    PT={pt_str} RC={rc} Fill={fill}/{pipe}({fill_pct}) Pull={pull}(+{d_pull}) {spk_state}")
        print(f"    W={spk_w}(+{d_w}) R={mic_r}(+{d_r}) MaxDPC={maxdpc}us Jump={pjump}")
    else:
        print(f"    (V2 diag not available)")


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
