"""
G2 measurement driver — 48 kHz / 16-bit / 2-ch PCM WASAPI exclusive.

Drives both Cable A and Cable B render (speaker) with a 440 Hz sine and,
in parallel, opens the matching capture (mic) stream so the reader-side
DPC fires. The purpose is to populate the loopback.cpp G2 1s-window
DbgPrint lines (AO_PIPE[?] 1S WR / AO_PIPE[?] 1S RD) so we can see
WrF/s, RdF/s, Fill/dFill, dDrop, dUnderrun under exact Phone Link format.

This script does not read any driver counters. All signal comes from
DebugView capturing DbgPrint. Run DebugView first, File->Log To File,
then run this script.

Usage:
    python tests/g2_sine_48_16_2.py               # both cables, 20s each
    python tests/g2_sine_48_16_2.py --cable a     # cable A only
    python tests/g2_sine_48_16_2.py --cable b     # cable B only
    python tests/g2_sine_48_16_2.py --duration 30

No IOCTL, no driver state changes, no file output. Pure sine + capture.
"""

import argparse
import sys
import threading
import time

import numpy as np
import sounddevice as sd


SAMPLE_RATE = 48000
CHANNELS = 2
DTYPE = "int16"
FREQ_HZ = 440.0
AMPLITUDE = 0.2  # -14 dBFS, safe for 16-bit


def find_device(name_contains, want_output):
    apis = sd.query_hostapis()
    wasapi_idx = None
    for i, a in enumerate(apis):
        if a.get("name") == "Windows WASAPI":
            wasapi_idx = i
            break
    best = None
    for idx, d in enumerate(sd.query_devices()):
        name = d.get("name", "")
        if name_contains not in name:
            continue
        if want_output and d.get("max_output_channels", 0) <= 0:
            continue
        if (not want_output) and d.get("max_input_channels", 0) <= 0:
            continue
        if wasapi_idx is not None and d.get("hostapi") == wasapi_idx:
            return idx
        if best is None:
            best = idx
    return best


class SineFeeder:
    def __init__(self, device_idx):
        self.device_idx = device_idx
        self.phase = 0.0
        self.stream = None

    def _cb(self, outdata, frames, time_info, status):
        two_pi_f_over_sr = 2.0 * np.pi * FREQ_HZ / SAMPLE_RATE
        idx = np.arange(frames, dtype=np.float64)
        s = AMPLITUDE * np.sin(self.phase + two_pi_f_over_sr * idx)
        self.phase = (self.phase + two_pi_f_over_sr * frames) % (2.0 * np.pi)
        s16 = (s * 32767.0).astype(np.int16)
        outdata[:] = np.tile(s16[:, None], (1, CHANNELS))

    def __enter__(self):
        extra = None
        try:
            extra = sd.WasapiSettings(exclusive=True)
        except Exception:
            extra = None
        self.stream = sd.OutputStream(
            device=self.device_idx,
            samplerate=SAMPLE_RATE,
            channels=CHANNELS,
            dtype=DTYPE,
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


class MicDrain:
    def __init__(self, device_idx):
        self.device_idx = device_idx
        self.stream = None
        self.frames_read = 0

    def _cb(self, indata, frames, time_info, status):
        self.frames_read += frames

    def __enter__(self):
        extra = None
        try:
            extra = sd.WasapiSettings(exclusive=True)
        except Exception:
            extra = None
        self.stream = sd.InputStream(
            device=self.device_idx,
            samplerate=SAMPLE_RATE,
            channels=CHANNELS,
            dtype=DTYPE,
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


def run_cable(cable_letter, duration_s):
    spk_name = f"AO Cable {cable_letter}"
    spk_idx = find_device(spk_name, want_output=True)
    mic_idx = find_device(spk_name, want_output=False)
    if spk_idx is None:
        print(f"ERROR: could not find {spk_name} render device")
        return False
    if mic_idx is None:
        print(f"WARN: could not find {spk_name} capture device; 1S RD will stay idle")

    print(f"\n=== Cable {cable_letter}  48k/16/2 PCM  {duration_s}s ===")
    print(f"  render idx={spk_idx}  capture idx={mic_idx}")

    try:
        if mic_idx is not None:
            with SineFeeder(spk_idx), MicDrain(mic_idx) as drain:
                time.sleep(duration_s)
                print(f"  capture frames drained: {drain.frames_read}")
        else:
            with SineFeeder(spk_idx):
                time.sleep(duration_s)
    except Exception as e:
        print(f"  ERROR during run: {e}")
        return False
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cable", choices=["a", "b", "both"], default="a")
    ap.add_argument("--duration", type=float, default=20.0)
    args = ap.parse_args()

    ok = True
    if args.cable in ("a", "both"):
        ok = run_cable("A", args.duration) and ok
        time.sleep(1.0)
    if args.cable in ("b", "both"):
        ok = run_cable("B", args.duration) and ok

    print("\nG2 run complete. Check DebugView log for AO_PIPE[?] 1S WR / 1S RD lines.")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
