"""
Phase 5 rollback smoke test — one-owner direct counter evidence.

Drives a continuous WASAPI exclusive sine into AO Cable A speaker while
toggling IOCTL_AO_SET_PUMP_FEATURE_FLAGS to flip
AO_PUMP_FLAG_DISABLE_LEGACY_RENDER off and back on. Asserts the
four-quadrant counter signature from the Phase 5 proposal §4.5:

  State                               | RenderPumpDriveCount | RenderLegacyDriveCount
  ----------------------------------- | -------------------- | ----------------------
  Pump-owned (flag set, default)      | grows                | frozen
  Legacy-owned (flag cleared)         | frozen               | grows

Both transitions must keep audio flowing (no stall, no DropCount spike).

This is the machine-checkable alternative to a WinDbg one-owner
breakpoint: both call sites (the pump helper's transport block and
ReadBytes's cable branch) bump disjoint counters, so a single sample
window cleanly proves who owns render transport right now.
"""

import ctypes
import struct
import sys
import time

sys.path.insert(0, "tests")

from phase3_shadow_active import (
    ContinuousSine,
    find_cable_a_render_device,
    kernel32,
    open_device,
)


def CTL_CODE(func, access):
    return (0x22 << 16) | (access << 14) | (func << 2) | 0


IOCTL_AO_GET_STREAM_STATUS     = CTL_CODE(0x802, 1)
IOCTL_AO_SET_PUMP_FEATURE_FLAGS = CTL_CODE(0x806, 2)  # FILE_WRITE_ACCESS

V1_STATUS_SIZE   = 64
V2_DIAG_SIZE_P5  = 4 + 4 * 7 * 4 + 16        # Phase 5 = 132
# Phase 1 Step 6 (2026-05-08): driver may now return 172-byte tail; accept
# it too. A_Render block + drive-counter tail offsets are unchanged.
V2_DIAG_SIZE_P6  = V2_DIAG_SIZE_P5 + 2 * 5 * 4   # 172
V2_BUF_SIZE      = V1_STATUS_SIZE + V2_DIAG_SIZE_P6

AO_PUMP_FLAG_ENABLE                  = 0x00000001
AO_PUMP_FLAG_SHADOW_ONLY             = 0x00000002
AO_PUMP_FLAG_DISABLE_LEGACY_RENDER   = 0x00000004

SHADOW_WINDOW = 128


def read_a_render_full(h):
    """Return dict with A_Render block fields + Phase 5 tail drive counters."""
    buf = ctypes.create_string_buffer(V2_BUF_SIZE)
    ret = ctypes.c_ulong(0)
    ok = kernel32.DeviceIoControl(
        h, IOCTL_AO_GET_STREAM_STATUS, None, 0, buf, V2_BUF_SIZE, ctypes.byref(ret), None
    )
    if not ok or ret.value < V1_STATUS_SIZE + V2_DIAG_SIZE_P5:
        return None
    struct_size = struct.unpack_from("<I", buf.raw, V1_STATUS_SIZE)[0]
    if struct_size not in (V2_DIAG_SIZE_P5, V2_DIAG_SIZE_P6):
        return None
    # A_Render block starts right after StructSize.
    off = V1_STATUS_SIZE + 4
    fields = struct.unpack_from("<IIIIIII", buf.raw, off)
    # Tail fields: A_R_PumpDriveCount, A_R_LegacyDriveCount,
    # B_R_PumpDriveCount, B_R_LegacyDriveCount at V1 + 4 + 4*7*4 = V1 + 116.
    tail_off = V1_STATUS_SIZE + 4 + 4 * 7 * 4
    a_r_pump_drv, a_r_legacy_drv, _b_r_pump, _b_r_legacy = struct.unpack_from(
        "<IIII", buf.raw, tail_off
    )
    return {
        "GatedSkipCount":            fields[0],
        "OverJumpCount":             fields[1],
        "FramesProcessedTotal":      fields[2] | (fields[3] << 32),
        "PumpInvocationCount":       fields[4],
        "PumpShadowDivergenceCount": fields[5],
        "PumpFeatureFlags":          fields[6],
        "RenderPumpDriveCount":      a_r_pump_drv,
        "RenderLegacyDriveCount":    a_r_legacy_drv,
    }


def send_flag_ioctl(h, set_mask, clear_mask):
    """Issue IOCTL_AO_SET_PUMP_FEATURE_FLAGS with AO_PUMP_FLAGS_REQ payload."""
    buf = struct.pack("<II", set_mask, clear_mask)
    buf_c = ctypes.create_string_buffer(buf, len(buf))
    ret = ctypes.c_ulong(0)
    ok = kernel32.DeviceIoControl(
        h,
        IOCTL_AO_SET_PUMP_FEATURE_FLAGS,
        buf_c,
        len(buf),
        None,
        0,
        ctypes.byref(ret),
        None,
    )
    return bool(ok)


def fmt(s):
    if s is None:
        return "(read failed)"
    return (
        f"Inv={s['PumpInvocationCount']:>4} "
        f"Div={s['PumpShadowDivergenceCount']} "
        f"Flags=0x{s['PumpFeatureFlags']:08X} "
        f"PumpDrv={s['RenderPumpDriveCount']:>4} "
        f"LegacyDrv={s['RenderLegacyDriveCount']:>4}"
    )


def delta(a, b, key):
    return b[key] - a[key]


def assert_grow(name, a, b, key):
    d = delta(a, b, key)
    if d > 0:
        print(f"  [PASS] {name}: {key} grew by {d}")
        return True
    print(f"  [FAIL] {name}: {key} did not grow (delta={d})")
    return False


def assert_frozen(name, a, b, key):
    d = delta(a, b, key)
    if d == 0:
        print(f"  [PASS] {name}: {key} frozen")
        return True
    print(f"  [FAIL] {name}: {key} changed by {d} (expected 0)")
    return False


def main():
    dev = find_cable_a_render_device()
    if dev is None:
        print("ERROR: no AO Cable A render device")
        return 2

    h = open_device(r"\\.\AOCableA")
    if h is None:
        print("ERROR: could not open \\.\\AOCableA (is driver installed?)")
        return 3

    ok_all = True
    try:
        print("Phase 5 rollback smoke test")
        print("=" * 60)

        with ContinuousSine(dev):
            # 1. Let the stream reach RUN and the pump helper run a few times.
            time.sleep(1.0)

            # 2. Initial snapshot (pump owned — flag set by SetState RUN arm).
            s0 = read_a_render_full(h)
            print(f"\n[t=run+1.0s]   {fmt(s0)}")
            if s0 is None:
                return 3

            # 3. Observe for 3 s in pump-owned state.
            time.sleep(3.0)
            s1 = read_a_render_full(h)
            print(f"[t=run+4.0s]   {fmt(s1)}")
            if s1["PumpFeatureFlags"] != (
                AO_PUMP_FLAG_ENABLE
                | AO_PUMP_FLAG_SHADOW_ONLY
                | AO_PUMP_FLAG_DISABLE_LEGACY_RENDER
            ):
                print(
                    f"  [FAIL] flags before IOCTL = 0x{s1['PumpFeatureFlags']:08X}, "
                    f"expected 0x00000007"
                )
                ok_all = False
            ok_all &= assert_grow("pump-owned", s0, s1, "RenderPumpDriveCount")
            ok_all &= assert_frozen("pump-owned", s0, s1, "RenderLegacyDriveCount")

            # 4. Clear the render ownership bit — rollback to legacy path.
            print("\n[IOCTL] clear DISABLE_LEGACY_RENDER")
            if not send_flag_ioctl(
                h, set_mask=0, clear_mask=AO_PUMP_FLAG_DISABLE_LEGACY_RENDER
            ):
                print("  [FAIL] IOCTL failed")
                return 4

            # 5. Wait briefly for the transition to take effect.
            time.sleep(0.25)
            s2 = read_a_render_full(h)
            print(f"[t=run+~4.3s]  {fmt(s2)}")

            # 6. Observe for 3 s in legacy-owned state.
            time.sleep(3.0)
            s3 = read_a_render_full(h)
            print(f"[t=run+~7.3s]  {fmt(s3)}")
            if s3["PumpFeatureFlags"] != (
                AO_PUMP_FLAG_ENABLE | AO_PUMP_FLAG_SHADOW_ONLY
            ):
                print(
                    f"  [FAIL] flags after clear = 0x{s3['PumpFeatureFlags']:08X}, "
                    f"expected 0x00000003"
                )
                ok_all = False
            ok_all &= assert_frozen("legacy-owned", s2, s3, "RenderPumpDriveCount")
            ok_all &= assert_grow("legacy-owned", s2, s3, "RenderLegacyDriveCount")

            # 7. Restore the render ownership bit.
            print("\n[IOCTL] set DISABLE_LEGACY_RENDER")
            if not send_flag_ioctl(
                h, set_mask=AO_PUMP_FLAG_DISABLE_LEGACY_RENDER, clear_mask=0
            ):
                print("  [FAIL] IOCTL failed")
                return 4

            time.sleep(0.25)
            s4 = read_a_render_full(h)
            print(f"[t=run+~7.6s]  {fmt(s4)}")

            # 8. Observe for 3 s back in pump-owned state.
            time.sleep(3.0)
            s5 = read_a_render_full(h)
            print(f"[t=run+~10.6s] {fmt(s5)}")
            if s5["PumpFeatureFlags"] != (
                AO_PUMP_FLAG_ENABLE
                | AO_PUMP_FLAG_SHADOW_ONLY
                | AO_PUMP_FLAG_DISABLE_LEGACY_RENDER
            ):
                print(
                    f"  [FAIL] flags after re-set = 0x{s5['PumpFeatureFlags']:08X}, "
                    f"expected 0x00000007"
                )
                ok_all = False
            ok_all &= assert_grow("pump re-owned", s4, s5, "RenderPumpDriveCount")
            ok_all &= assert_frozen("pump re-owned", s4, s5, "RenderLegacyDriveCount")

            # 9. Divergence must remain 0 across the whole run.
            div_total = s5["PumpShadowDivergenceCount"] - s0["PumpShadowDivergenceCount"]
            if div_total != 0:
                print(
                    f"\n  [FAIL] PumpShadowDivergenceCount rose by {div_total} during the test"
                )
                ok_all = False
            else:
                print("\n  [PASS] PumpShadowDivergenceCount stayed 0 across rollback")

    finally:
        kernel32.CloseHandle(h)

    print("\n" + ("ALL PASS" if ok_all else "FAIL"))
    return 0 if ok_all else 1


if __name__ == "__main__":
    sys.exit(main())
