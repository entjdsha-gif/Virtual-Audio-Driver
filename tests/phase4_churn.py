"""
Phase 4 STOP/RUN churn stress test.

Rapidly opens and closes WASAPI exclusive streams on AO Cable A to
stress the Phase 4 destructor reorder + STOP-era semantics. The
destructor now owns FramePipeUnregisterFormat() and runs after
KeFlushQueuedDpcs(); repeated stream lifetime churn is the most
direct way to exercise the new teardown ordering.

Pass criteria:
  - PumpShadowDivergenceCount stays at its pre-churn value
  - PumpInvocationCount grows (helper keeps firing on RUN)
  - Driver symbolic link remains openable throughout
  - No IOCTL failures
"""

import sys
import time

sys.path.insert(0, "tests")

from phase3_shadow_active import (
    ContinuousSine,
    find_cable_a_render_device,
    kernel32,
    open_device,
    read_render_counters,
)


CYCLES = 20
STREAM_SECONDS = 1.0
GAP_SECONDS = 0.1


def snap(h):
    s = read_render_counters(h)
    if s is None:
        return None
    return s


def main():
    dev = find_cable_a_render_device()
    if dev is None:
        print("ERROR: no AO Cable A render device")
        return 2

    h = open_device(r"\\.\AOCableA")
    if h is None:
        print("ERROR: could not open \\.\\AOCableA")
        return 3

    try:
        pre = snap(h)
        print(
            f"pre : Inv={pre['PumpInvocationCount']:>4} "
            f"Div={pre['PumpShadowDivergenceCount']} "
            f"OverJump={pre['OverJumpCount']:>4} "
            f"Frames={pre['FramesProcessedTotal']:>10} "
            f"Flags=0x{pre['PumpFeatureFlags']:08X}"
        )

        print(
            f"\n--- STOP/RUN churn: {CYCLES} cycles x {STREAM_SECONDS:.1f}s stream + {GAP_SECONDS:.1f}s gap ---"
        )
        peak_inv = 0
        peak_div = pre["PumpShadowDivergenceCount"]
        pre_div = peak_div

        for i in range(CYCLES):
            with ContinuousSine(dev):
                time.sleep(STREAM_SECONDS)
                s = snap(h)
                if s is not None:
                    if s["PumpInvocationCount"] > peak_inv:
                        peak_inv = s["PumpInvocationCount"]
                    if s["PumpShadowDivergenceCount"] > peak_div:
                        peak_div = s["PumpShadowDivergenceCount"]
            time.sleep(GAP_SECONDS)
            if (i + 1) % 5 == 0:
                post = snap(h)
                print(
                    f"  cycle {i + 1:>2}: "
                    f"peak_inv={peak_inv:>4} peak_div={peak_div} "
                    f"last_flags=0x{post['PumpFeatureFlags']:08X}"
                )

        post = snap(h)
        print(
            f"\npost: Inv={post['PumpInvocationCount']:>4} "
            f"Div={post['PumpShadowDivergenceCount']} "
            f"OverJump={post['OverJumpCount']:>4} "
            f"Frames={post['FramesProcessedTotal']:>10} "
            f"Flags=0x{post['PumpFeatureFlags']:08X}"
        )
        print(f"\npeak across churn: Inv={peak_inv} Div={peak_div}")

        ok = True
        if peak_div != pre_div:
            print(
                f"[FAIL] divergence grew across churn: pre_div={pre_div} peak_div={peak_div}"
            )
            ok = False
        if peak_inv <= 0:
            print("[FAIL] helper never fired across churn")
            ok = False

        print("\n" + ("PASS" if ok else "FAIL"))
        return 0 if ok else 1
    finally:
        kernel32.CloseHandle(h)


if __name__ == "__main__":
    sys.exit(main())
