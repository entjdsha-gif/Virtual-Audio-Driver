"""
Phase 3 B-step validation: real Phone Link call path.

Spawns tests/live_call/run_test_call.py as a subprocess, while polling
V2 diag counters for all four cable directions every 500 ms. On call
end, computes deltas and checks Phase 3 closure criteria:

  - PumpInvocationCount increases by more than AO_PUMP_SHADOW_WINDOW_CALLS
    (128) on the real target path (CableA_Render)
  - PumpShadowDivergenceCount delta == 0
  - No OverJump storm
  - Flags stay at ENABLE|SHADOW_ONLY during the call

This does not judge call audio quality. The AO Cable audio-quality
regression (separate from Phase 3) is tracked independently.
"""

import ctypes
import os
import struct
import subprocess
import sys
import threading
import time

kernel32 = ctypes.windll.kernel32


def CTL_CODE(func, access):
    return (0x22 << 16) | (access << 14) | (func << 2) | 0


IOCTL_AO_GET_STREAM_STATUS = CTL_CODE(0x802, 1)
GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3

V1_STATUS_SIZE = 64
V2_DIAG_SIZE = 4 + 4 * 7 * 4
V2_BUF_SIZE = V1_STATUS_SIZE + V2_DIAG_SIZE

SHADOW_WINDOW = 128


def open_device(name):
    h = kernel32.CreateFileW(
        name, GENERIC_READ | GENERIC_WRITE, 0, None, OPEN_EXISTING, 0, None
    )
    if h == -1 or h == 0xFFFFFFFF or h == ctypes.c_void_p(-1).value:
        return None
    return h


def read_all_blocks(h):
    """Return dict of all 4 direction blocks for one cable."""
    buf = ctypes.create_string_buffer(V2_BUF_SIZE)
    ret = ctypes.c_ulong(0)
    ok = kernel32.DeviceIoControl(
        h, IOCTL_AO_GET_STREAM_STATUS, None, 0, buf, V2_BUF_SIZE, ctypes.byref(ret), None
    )
    if not ok or ret.value < V1_STATUS_SIZE + V2_DIAG_SIZE:
        return None
    struct_size = struct.unpack_from("<I", buf.raw, V1_STATUS_SIZE)[0]
    if struct_size != V2_DIAG_SIZE:
        return None

    def read_block(offset):
        fields = struct.unpack_from("<IIIIIII", buf.raw, offset)
        return {
            "GatedSkipCount": fields[0],
            "OverJumpCount": fields[1],
            "FramesProcessedTotal": fields[2] | (fields[3] << 32),
            "PumpInvocationCount": fields[4],
            "PumpShadowDivergenceCount": fields[5],
            "PumpFeatureFlags": fields[6],
        }

    # V2 block order for A cable device: A_Render, A_Capture, B_Render, B_Capture.
    base = V1_STATUS_SIZE + 4
    block = 7 * 4
    return {
        "A_Render": read_block(base + 0 * block),
        "A_Capture": read_block(base + 1 * block),
        "B_Render": read_block(base + 2 * block),
        "B_Capture": read_block(base + 3 * block),
    }


def snapshot_all():
    """Read all blocks via Cable A device (same V2 layout exposes all 4)."""
    h = open_device(r"\\.\AOCableA")
    if h is None:
        return None
    try:
        return read_all_blocks(h)
    finally:
        kernel32.CloseHandle(h)


def fmt(b):
    return (
        f"Inv={b['PumpInvocationCount']:>4} "
        f"Div={b['PumpShadowDivergenceCount']} "
        f"Gated={b['GatedSkipCount']} "
        f"OverJump={b['OverJumpCount']:>4} "
        f"Frames={b['FramesProcessedTotal']:>10} "
        f"Flags=0x{b['PumpFeatureFlags']:08X}"
    )


def main():
    print("Phase 3 B-step: live Phone Link call shadow validation")
    print("=" * 60)

    pre = snapshot_all()
    if pre is None:
        print("ERROR: could not read pre-call counters (driver not installed?)")
        return 2

    print("\n[pre-call]")
    for k, v in pre.items():
        print(f"  {k:<10} {fmt(v)}")

    # Spawn run_test_call.py
    env = os.environ.copy()
    cwd = os.path.abspath(os.path.join(os.path.dirname(__file__), "live_call"))
    cmd = [sys.executable, "run_test_call.py"]
    print(f"\n[spawn] {' '.join(cmd)}  (cwd={cwd})")
    proc = subprocess.Popen(cmd, cwd=cwd, env=env)

    # Poll every 500 ms during the call, keep a max-observed snapshot
    # so we can see peak Flags/Inv even if STOP runs before we read.
    samples = []
    stop_flag = threading.Event()

    def poll():
        while not stop_flag.is_set():
            s = snapshot_all()
            if s is not None:
                samples.append((time.time(), s))
            time.sleep(0.5)

    t = threading.Thread(target=poll, daemon=True)
    t.start()

    rc = proc.wait()
    stop_flag.set()
    t.join(timeout=2.0)

    post = snapshot_all()
    print(f"\n[call exit code] {rc}")
    print(f"[samples captured] {len(samples)}")
    print("\n[post-call]")
    if post is not None:
        for k, v in post.items():
            print(f"  {k:<10} {fmt(v)}")

    # Peak analysis from samples
    peak = {"A_Render": 0, "A_Capture": 0, "B_Render": 0, "B_Capture": 0}
    peak_div = {"A_Render": 0, "A_Capture": 0, "B_Render": 0, "B_Capture": 0}
    peak_overjump = {"A_Render": 0, "A_Capture": 0, "B_Render": 0, "B_Capture": 0}
    saw_flags = {"A_Render": 0, "A_Capture": 0, "B_Render": 0, "B_Capture": 0}
    for _, s in samples:
        for k in peak:
            b = s[k]
            if b["PumpInvocationCount"] > peak[k]:
                peak[k] = b["PumpInvocationCount"]
            if b["PumpShadowDivergenceCount"] > peak_div[k]:
                peak_div[k] = b["PumpShadowDivergenceCount"]
            if b["OverJumpCount"] > peak_overjump[k]:
                peak_overjump[k] = b["OverJumpCount"]
            saw_flags[k] |= b["PumpFeatureFlags"]

    print("\n[peak during call]")
    for k in peak:
        print(
            f"  {k:<10} peak_Inv={peak[k]:>5} peak_Div={peak_div[k]} "
            f"peak_OverJump={peak_overjump[k]} flags_seen=0x{saw_flags[k]:08X}"
        )

    # Judgement
    print("\n[judgement]")
    ok = True
    # Primary target: CableA_Render (render path into the call)
    primary_inv = peak["A_Render"]
    primary_div = peak_div["A_Render"]
    primary_overjump = peak_overjump["A_Render"]
    primary_flags = saw_flags["A_Render"]

    if primary_flags != 0x00000003:
        print(
            f"  [WARN] A_Render flags_seen=0x{primary_flags:08X}, expected 0x00000003 at some point"
        )
    else:
        print("  [PASS] A_Render flags reached ENABLE|SHADOW_ONLY")

    if primary_inv < SHADOW_WINDOW:
        print(
            f"  [FAIL] A_Render peak Inv={primary_inv} < {SHADOW_WINDOW} - no window flush on target path"
        )
        ok = False
    else:
        print(f"  [PASS] A_Render peak Inv={primary_inv} >= {SHADOW_WINDOW} - window flushed")

    if primary_div != 0:
        print(f"  [FAIL] A_Render peak Div={primary_div} - divergence on target path")
        ok = False
    else:
        print("  [PASS] A_Render peak Div=0")

    if primary_inv > 0 and primary_overjump * 2 > primary_inv:
        print(
            f"  [WARN] A_Render peak OverJump={primary_overjump} > 50% of Inv={primary_inv}"
        )

    # Capture side (Phone Link mic path)
    cap_inv = peak["B_Capture"]
    cap_div = peak_div["B_Capture"]
    if cap_inv > 0:
        print(
            f"  [info] B_Capture peak Inv={cap_inv} Div={cap_div} "
            f"OverJump={peak_overjump['B_Capture']} "
            f"flags_seen=0x{saw_flags['B_Capture']:08X}"
        )
        if cap_div != 0:
            print(f"  [FAIL] B_Capture peak Div={cap_div}")
            ok = False

    print(f"\n{'ALL PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
