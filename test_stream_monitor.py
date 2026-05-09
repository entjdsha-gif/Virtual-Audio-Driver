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
# Phase 6 tail extension: +2 cables * 5 ULONG-equivalents (Overflow,
# Underrun, RingFill, WrapBound, UnderrunFlag UCHAR + 3-byte pad) = 172.
# Phase 3 Step 2 prep tail: +4 streams * 3 ULONGs (per-stream
# Shadow{Advance,Query,Timer}Hits from AO_STREAM_RT) = 220.
# Phase 3 Step 4 tail: +4 streams * 1 ULONG (per-stream
# ShadowDivergenceCount from AO_STREAM_RT::DbgShadowDivergenceHits) = 236.
V2_DIAG_SIZE_P1  = 4 + 4 * 7 * 4                 # 116
V2_DIAG_SIZE_P5  = V2_DIAG_SIZE_P1 + 4 * 4       # 132
V2_DIAG_SIZE_P6  = V2_DIAG_SIZE_P5 + 2 * 5 * 4   # 172
V2_DIAG_SIZE_P7  = V2_DIAG_SIZE_P6 + 4 * 3 * 4   # 220
V2_DIAG_SIZE_P8  = V2_DIAG_SIZE_P7 + 4 * 1 * 4   # 236
V2_DIAG_SIZE     = V2_DIAG_SIZE_P8
V2_BUF_SIZE      = V1_STATUS_SIZE + V2_DIAG_SIZE
V2_FIELDS_PER_BLOCK = 7
RING_DIAG_BLOCK_BYTES   = 5 * 4                  # per-cable Phase 6 ring diag
SHADOW_BLOCK_BYTES      = 3 * 4                  # per-stream Phase 3 prep shadow counters
DIVERGENCE_BLOCK_BYTES  = 1 * 4                  # per-stream Phase 3 Step 4 ShadowDivergenceCount


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
    # Accept Phase 1 (116), Phase 5 (132), or Phase 6 (172) tail, whichever
    # the driver reports. Phase 6 clients see the full tail and 10 extra
    # ULONG-equivalents (per-cable ring diag); Phase 5 stops at the drive
    # counters; Phase 1 stops at the per-direction pump counters.
    if ret.value >= V1_STATUS_SIZE + V2_DIAG_SIZE_P1:
        v2_offset = V1_STATUS_SIZE
        struct_size = struct.unpack_from('<I', buf.raw, v2_offset)[0]

        if struct_size in (V2_DIAG_SIZE_P1, V2_DIAG_SIZE_P5,
                           V2_DIAG_SIZE_P6, V2_DIAG_SIZE_P7,
                           V2_DIAG_SIZE_P8):
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
            # Present iff the driver returned the 132-byte shape or larger.
            if struct_size in (V2_DIAG_SIZE_P5, V2_DIAG_SIZE_P6,
                               V2_DIAG_SIZE_P7, V2_DIAG_SIZE_P8):
                a_r_pump, a_r_legacy, b_r_pump, b_r_legacy = struct.unpack_from(
                    '<IIII', buf.raw, cursor)
                result['CableA_Render']['RenderPumpDriveCount']   = a_r_pump
                result['CableA_Render']['RenderLegacyDriveCount'] = a_r_legacy
                result['CableB_Render']['RenderPumpDriveCount']   = b_r_pump
                result['CableB_Render']['RenderLegacyDriveCount'] = b_r_legacy
                cursor += 4 * 4
            else:
                for key in ('CableA_Render', 'CableB_Render'):
                    result[key]['RenderPumpDriveCount']   = None
                    result[key]['RenderLegacyDriveCount'] = None

            # Phase 6 tail: per-cable canonical FRAME_PIPE ring diag.
            # 5 ULONG-equivalents per cable: OverflowCount, UnderrunCount,
            # RingFillFrames, WrapBoundFrames, UnderrunFlag (UCHAR) + 3-byte
            # Reserved pad. Layout = '<IIIIB3x' per cable. Present iff the
            # driver returned the 172-byte shape or larger.
            if struct_size in (V2_DIAG_SIZE_P6, V2_DIAG_SIZE_P7,
                               V2_DIAG_SIZE_P8):
                ovA, urA, fillA, wrapA, ufA = struct.unpack_from(
                    '<IIIIB3x', buf.raw, cursor)
                cursor += RING_DIAG_BLOCK_BYTES
                ovB, urB, fillB, wrapB, ufB = struct.unpack_from(
                    '<IIIIB3x', buf.raw, cursor)
                cursor += RING_DIAG_BLOCK_BYTES

                result['CableA_Ring'] = {
                    'OverflowCount':   ovA,
                    'UnderrunCount':   urA,
                    'RingFillFrames':  fillA,
                    'WrapBoundFrames': wrapA,
                    'UnderrunFlag':    ufA,
                }
                result['CableB_Ring'] = {
                    'OverflowCount':   ovB,
                    'UnderrunCount':   urB,
                    'RingFillFrames':  fillB,
                    'WrapBoundFrames': wrapB,
                    'UnderrunFlag':    ufB,
                }

            # Phase 3 Step 2 prep tail: per-stream shadow helper counters.
            # 4 streams * 3 ULONGs each = 48 bytes. Present iff the driver
            # returned the 220-byte shape. Order: A_Render, A_Capture,
            # B_Render, B_Capture. Fields per stream: ShadowAdvanceHits,
            # ShadowQueryHits, ShadowTimerHits (sourced from AO_STREAM_RT
            # DbgShadow*Hits via AoTransportSnapshotShadowCounters).
            if struct_size in (V2_DIAG_SIZE_P7, V2_DIAG_SIZE_P8):
                for key in ('CableA_Render', 'CableA_Capture',
                            'CableB_Render', 'CableB_Capture'):
                    sha, shq, sht = struct.unpack_from('<III', buf.raw, cursor)
                    result[key]['ShadowAdvanceHits'] = sha
                    result[key]['ShadowQueryHits']   = shq
                    result[key]['ShadowTimerHits']   = sht
                    cursor += SHADOW_BLOCK_BYTES
            else:
                for key in ('CableA_Render', 'CableA_Capture',
                            'CableB_Render', 'CableB_Capture'):
                    result[key]['ShadowAdvanceHits'] = None
                    result[key]['ShadowQueryHits']   = None
                    result[key]['ShadowTimerHits']   = None

            # Phase 3 Step 4 tail: per-stream ShadowDivergenceCount.
            # 4 streams * 1 ULONG = 16 bytes. Present iff driver returned
            # the 236-byte (P8) shape. Bumped by helper only on
            # AO_ADVANCE_QUERY (and only while the stream is Active=RUN)
            # when |helper - legacy| frame-anchor cumulative exceeds
            # ((SampleRate + 999) / 1000) + AO_CABLE_MIN_FRAMES_GATE
            # (legacy 1 ms quantization envelope + helper 8-frame gate;
            # 56 frames at 48 kHz). Distinct from PumpShadowDivergenceCount
            # in the per-direction P1 block above (force-zeroed; source
            # retired).
            if struct_size == V2_DIAG_SIZE_P8:
                for key in ('CableA_Render', 'CableA_Capture',
                            'CableB_Render', 'CableB_Capture'):
                    shdiv, = struct.unpack_from('<I', buf.raw, cursor)
                    result[key]['ShadowDivergenceCount'] = shdiv
                    cursor += DIVERGENCE_BLOCK_BYTES
            else:
                for key in ('CableA_Render', 'CableA_Capture',
                            'CableB_Render', 'CableB_Capture'):
                    result[key]['ShadowDivergenceCount'] = None

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
            # Phase 3 Step 2 prep: per-stream canonical-helper shadow
            # counters. None if the driver is on a pre-P7 layout.
            sha = block.get('ShadowAdvanceHits')
            shq = block.get('ShadowQueryHits')
            sht = block.get('ShadowTimerHits')
            if sha is not None and shq is not None and sht is not None:
                line += f" | Shadow=Adv{sha}/Q{shq}/T{sht}"
            # Phase 3 Step 4 helper-vs-legacy divergence counter.
            # ShDiv=N means the helper's frame-anchor cumulative differed
            # from the legacy DmaProducedMono / BlockAlign cumulative by
            # more than the rate-aware tolerance
            #   ((SampleRate + 999) / 1000) + AO_CABLE_MIN_FRAMES_GATE
            # (legacy 1 ms quantization envelope + helper 8-frame gate;
            # 56 frames at 48 kHz) on N AO_ADVANCE_QUERY invocations
            # since the last anchor seed / rebase / backwards-baseline
            # re-seed. None means the driver is on a pre-P8 layout and
            # this metric is not exposed yet.
            shdiv = block.get('ShadowDivergenceCount')
            if shdiv is not None:
                line += f" ShDiv={shdiv}"
            return line

        print(fmt_block("Render ", render, is_render=True))
        print(fmt_block("Capture", capture))

        # Phase 6 per-cable ring diag (Phase 1 Step 6). Present iff the
        # driver returned the 172-byte V2 shape.
        ring = status.get(f"{cable_label}_Ring")
        if ring is not None:
            ov   = ring['OverflowCount']
            ur   = ring['UnderrunCount']
            fill = ring['RingFillFrames']
            wrap = ring['WrapBoundFrames']
            flag = ring['UnderrunFlag']
            flag_tag = "RECOVER" if flag else "ok"
            print(f"    Ring   : Overflow={ov} Underrun={ur} "
                  f"Flag={flag}({flag_tag}) Fill={fill} WrapBound={wrap}")
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
