"""WASAPI loopback capture from a render endpoint.

Unlike loopback_rec.py (which reads the Output mic side), this opens the
Input speaker side in WASAPI loopback mode. This does NOT conflict with
another process reading the corresponding mic — useful for recording
during live_call test that itself reads the mic side.

Usage:
    python tests/loopback_wasapi.py --render "AO Cable A" --duration 90 \
        --out results/bugb_runtime/livecall/phase1_cableA.wav
"""
import argparse
import sys
import numpy as np
import sounddevice as sd
from scipy.io import wavfile

RATE = 48000
CHANNELS = 2
DTYPE = "int16"


def find_wasapi_render(name_contains: str) -> int:
    hostapis = sd.query_hostapis()
    wasapi_idx = None
    for i, h in enumerate(hostapis):
        if "WASAPI" in h["name"]:
            wasapi_idx = i
            break
    if wasapi_idx is None:
        sys.exit("WASAPI host API not available")

    for i, d in enumerate(sd.query_devices()):
        if (d["hostapi"] == wasapi_idx
                and d["max_output_channels"] > 0
                and name_contains.lower() in d["name"].lower()):
            return i
    sys.exit(f"WASAPI render device matching '{name_contains}' not found")


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--render", required=True, help="render device name substring")
    p.add_argument("--duration", type=float, default=90.0)
    p.add_argument("--out", required=True)
    args = p.parse_args()

    idx = find_wasapi_render(args.render)
    info = sd.query_devices(idx)
    print(f"wasapi loopback [{idx}] {info['name']}  {args.duration}s  {RATE}/{DTYPE}/{CHANNELS}ch")

    extras = sd.WasapiSettings(loopback=True)
    frames = sd.rec(
        int(args.duration * RATE),
        samplerate=RATE,
        channels=CHANNELS,
        dtype=DTYPE,
        device=idx,
        extra_settings=extras,
    )
    sd.wait()

    wavfile.write(args.out, RATE, frames)
    peak = int(np.max(np.abs(frames)))
    rms = float(np.sqrt(np.mean(frames.astype(np.float64) ** 2)))
    print(f"saved {args.out}  peak={peak}  rms={rms:.1f}")


if __name__ == "__main__":
    main()
