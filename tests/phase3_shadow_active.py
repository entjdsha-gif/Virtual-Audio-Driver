"""
Phase 3 shadow-mode active-stream validation (A-step).

Plays a 440 Hz sine tone into AO Cable A (render) for a short duration
under three polling regimes (normal / aggressive / sparse), reading the
V2 diag counters before and after each regime via GET_STREAM_STATUS.

Pass criteria per regime:
  - PumpInvocationCount increases  (helper actually fires)
  - PumpShadowDivergenceCount stays at the pre-run value (== 0)
  - No Gated/OverJump storm (non-zero is OK; we just record)

This script does not change driver state, does not touch transport, and
does not persist any file beyond this source. One-shot validation tool
for Phase 3 closure. See results/phase3_edit_proposal.md.
"""

import ctypes
import struct
import sys
import threading
import time

import numpy as np
import sounddevice as sd

# --- IOCTL plumbing (copied from test_stream_monitor.py) ---

kernel32 = ctypes.windll.kernel32


def CTL_CODE(func, access):
    return (0x22 << 16) | (access << 14) | (func << 2) | 0


IOCTL_AO_GET_STREAM_STATUS = CTL_CODE(0x802, 1)
GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3

V1_STATUS_SIZE = 64
# Phase 5 (2026-04-14): driver now returns 132-byte V2 tail (was 116 in
# Phase 1/3/4). Allocate the larger buffer so the driver writes the full
# new shape including the render-side drive counters at the tail. The
# Phase 1 blocks stay at the same offsets, so this script's slicing of
# the A_Render block is unchanged.
V2_DIAG_SIZE_P1 = 4 + 4 * 7 * 4          # 116
V2_DIAG_SIZE    = V2_DIAG_SIZE_P1 + 16   # 132
V2_BUF_SIZE     = V1_STATUS_SIZE + V2_DIAG_SIZE


def open_device(name):
    h = kernel32.CreateFileW(
        name, GENERIC_READ | GENERIC_WRITE, 0, None, OPEN_EXISTING, 0, None
    )
    if h == -1 or h == 0xFFFFFFFF or h == ctypes.c_void_p(-1).value:
        return None
    return h


def read_render_counters(h):
    """Read CableA_Render block from V2 diag."""
    buf = ctypes.create_string_buffer(V2_BUF_SIZE)
    ret = ctypes.c_ulong(0)
    ok = kernel32.DeviceIoControl(
        h, IOCTL_AO_GET_STREAM_STATUS, None, 0, buf, V2_BUF_SIZE, ctypes.byref(ret), None
    )
    if not ok or ret.value < V1_STATUS_SIZE + V2_DIAG_SIZE_P1:
        return None
    struct_size = struct.unpack_from("<I", buf.raw, V1_STATUS_SIZE)[0]
    # Accept either the Phase 1 (116) or the Phase 5 (132) shape.
    if struct_size != V2_DIAG_SIZE_P1 and struct_size != V2_DIAG_SIZE:
        return None
    # CableA_Render is the first block, right after StructSize.
    offset = V1_STATUS_SIZE + 4
    fields = struct.unpack_from("<IIIIIII", buf.raw, offset)
    return {
        "GatedSkipCount": fields[0],
        "OverJumpCount": fields[1],
        "FramesProcessedTotal": fields[2] | (fields[3] << 32),
        "PumpInvocationCount": fields[4],
        "PumpShadowDivergenceCount": fields[5],
        "PumpFeatureFlags": fields[6],
    }


# --- sine tone generator into Cable A ---


def find_cable_a_render_device():
    # Prefer the WASAPI host API (hostapi index matches 'Windows WASAPI')
    # so WasapiSettings(exclusive=True) is accepted by PortAudio.
    apis = sd.query_hostapis()
    wasapi_idx = None
    for i, a in enumerate(apis):
        if a.get("name") == "Windows WASAPI":
            wasapi_idx = i
            break
    best = None
    for idx, d in enumerate(sd.query_devices()):
        if d.get("max_output_channels", 0) <= 0:
            continue
        name = d.get("name", "")
        if "AO Cable A" not in name:
            continue
        if wasapi_idx is not None and d.get("hostapi") == wasapi_idx:
            return idx
        if best is None:
            best = idx
    return best


class ContinuousSine:
    """Context manager that keeps a single OutputStream open and feeds a
    continuous sine tone via a callback, so the audio engine's polling
    pattern on GetPositions() is representative of a real long-lived
    render stream rather than a short sd.play() burst."""

    def __init__(self, device_idx, samplerate=48000, freq=440.0, channels=None):
        self.device_idx = device_idx
        self.samplerate = samplerate
        self.freq = freq
        # Default channel count from the device info (WASAPI exclusive
        # requires the exact channel count the endpoint exposes).
        if channels is None:
            info = sd.query_devices(device_idx)
            channels = int(info.get("max_output_channels", 2))
        self.channels = channels
        self.phase = 0.0
        self.stream = None

    def _cb(self, outdata, frames, time_info, status):
        two_pi_f_over_sr = 2.0 * np.pi * self.freq / self.samplerate
        idx = np.arange(frames, dtype=np.float32)
        samples = 0.1 * np.sin(self.phase + two_pi_f_over_sr * idx).astype(np.float32)
        self.phase = (self.phase + two_pi_f_over_sr * frames) % (2.0 * np.pi)
        outdata[:] = np.tile(samples[:, None], (1, self.channels))

    def __enter__(self):
        # WASAPI exclusive mode with a small buffer forces the engine to
        # poll the driver at buffer boundaries, which is the polling
        # pattern Phase 3 is built for. Shared-mode playback only issues
        # a single GetPositions() at stream open in our observations.
        extra = None
        try:
            extra = sd.WasapiSettings(exclusive=True)
        except Exception:
            extra = None
        self.stream = sd.OutputStream(
            device=self.device_idx,
            samplerate=self.samplerate,
            channels=self.channels,
            dtype="float32",
            latency="low",
            extra_settings=extra,
            callback=self._cb,
        )
        self.stream.start()
        return self

    def __exit__(self, *a):
        if self.stream is not None:
            self.stream.stop()
            self.stream.close()
            self.stream = None


# --- regime runner ---


def fmt_block(tag, b):
    if b is None:
        return f"{tag}: (read failed)"
    return (
        f"{tag}: Inv={b['PumpInvocationCount']} "
        f"Div={b['PumpShadowDivergenceCount']} "
        f"Gated={b['GatedSkipCount']} "
        f"OverJump={b['OverJumpCount']} "
        f"Frames={b['FramesProcessedTotal']} "
        f"Flags=0x{b['PumpFeatureFlags']:08X}"
    )


def run_regime(name, duration_s, poll_interval_s, device_idx, h):
    print(f"\n=== {name}  (duration={duration_s}s, poll_interval={poll_interval_s}s) ===")

    # We must sample counters WHILE the stream is still in KSSTATE_RUN.
    # Once stream.close() runs, the driver's Phase 3 STOP clear zeros all
    # m_ul* pump state; the pipe mirror then only reflects the stale last
    # helper-written value, so a post-close read cannot prove anything.
    stop_flag = threading.Event()

    def poller():
        while not stop_flag.is_set():
            _ = read_render_counters(h)
            time.sleep(poll_interval_s)

    t = threading.Thread(target=poller, daemon=True)
    t.start()

    t_in_run = None
    t_mid = None
    t_near_end = None
    try:
        with ContinuousSine(device_idx):
            # Give the engine a moment to enter RUN and start polling.
            time.sleep(0.3)
            t_in_run = read_render_counters(h)
            # Mid-duration read: helper should be actively running.
            time.sleep(duration_s * 0.5)
            t_mid = read_render_counters(h)
            # Near-end read: just before we tear the stream down.
            time.sleep(max(duration_s * 0.5 - 0.3, 0.0))
            t_near_end = read_render_counters(h)
    finally:
        stop_flag.set()
        t.join(timeout=1.0)

    print("  " + fmt_block("t=run+0.3s ", t_in_run))
    print("  " + fmt_block("t=mid      ", t_mid))
    print("  " + fmt_block("t=near-end ", t_near_end))
    if t_in_run is None or t_mid is None or t_near_end is None:
        return False

    inv_delta = t_near_end["PumpInvocationCount"] - t_in_run["PumpInvocationCount"]
    div_delta = t_near_end["PumpShadowDivergenceCount"] - t_in_run["PumpShadowDivergenceCount"]
    frames_delta = t_near_end["FramesProcessedTotal"] - t_in_run["FramesProcessedTotal"]
    print(f"  delta (t_in_run -> t_near_end): Inv+={inv_delta} Div+={div_delta} Frames+={frames_delta}")

    ok = True
    # Phase 3 expected 0x00000003 (ENABLE|SHADOW_ONLY).
    # Phase 5 adds DISABLE_LEGACY_RENDER for cable speaker RUN, so the
    # accepted set widens to {0x03, 0x07} on the render side.
    if t_near_end["PumpFeatureFlags"] not in (0x00000003, 0x00000007):
        print(
            f"  [FAIL] Flags=0x{t_near_end['PumpFeatureFlags']:08X} while stream should be in RUN with ENABLE|SHADOW_ONLY or Phase 5 render-armed"
        )
        ok = False
    if inv_delta <= 0:
        print("  [FAIL] PumpInvocationCount did not increase - helper never fired")
        ok = False
    if div_delta != 0:
        print(f"  [FAIL] PumpShadowDivergenceCount rose by {div_delta}")
        ok = False
    # Phase 3 Revision 1 exit criterion #7: at least one real rolling-window
    # flush must execute during validation. The window length is 128 calls
    # (AO_PUMP_SHADOW_WINDOW_CALLS), so we require > 128 accepted invocations
    # across this regime, and we also ensure over-jump no longer dominates.
    SHADOW_WINDOW = 128
    if inv_delta < SHADOW_WINDOW:
        print(
            f"  [FAIL] inv_delta={inv_delta} < {SHADOW_WINDOW}; rolling shadow window never flushed"
        )
        ok = False
    overjump_delta = t_near_end["OverJumpCount"] - t_in_run["OverJumpCount"]
    if inv_delta > 0 and overjump_delta * 2 > inv_delta:
        print(
            f"  [WARN] over-jump rate {overjump_delta}/{inv_delta} > 50% (threshold too tight?)"
        )
    if ok:
        print("  [PASS]")
    return ok


def main():
    dev_idx = find_cable_a_render_device()
    if dev_idx is None:
        print("ERROR: could not find AO Cable A render device")
        return 2
    dev_info = sd.query_devices(dev_idx)
    print(f"Cable A render device: [{dev_idx}] {dev_info['name']}")
    print(f"  default samplerate: {dev_info['default_samplerate']}")

    h = open_device(r"\\.\AOCableA")
    if h is None:
        print("ERROR: could not open \\.\\AOCableA")
        return 3

    try:
        regimes = [
            # 20s per regime so the invocation count crosses
            # AO_PUMP_SHADOW_WINDOW_CALLS = 128 and at least one rolling
            # window flush actually executes (Phase 3 Revision 1 exit
            # criterion #7).
            ("normal polling  ", 20.0, 0.050),   # 20 Hz observer
            ("aggressive poll ", 20.0, 0.002),   # 500 Hz observer
            ("sparse polling  ", 20.0, 0.250),   # 4 Hz observer
        ]
        results = []
        for name, dur, poll in regimes:
            results.append(run_regime(name, dur, poll, dev_idx, h))
            # Small gap between regimes to let the engine settle.
            time.sleep(0.5)

        print("\n=== SUMMARY ===")
        all_ok = all(results)
        for (name, *_), ok in zip(regimes, results):
            print(f"  {name}: {'PASS' if ok else 'FAIL'}")
        print(f"\n{'ALL PASS' if all_ok else 'FAIL'}")
        return 0 if all_ok else 1
    finally:
        kernel32.CloseHandle(h)


if __name__ == "__main__":
    sys.exit(main())
