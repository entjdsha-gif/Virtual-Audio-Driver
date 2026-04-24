"""WASAPI loopback capture via `soundcard` package.

Captures what's being played to a render endpoint WITHOUT opening the
capture-side mic. Non-conflicting with other processes reading the mic.

Usage:
    python tests/loopback_sc.py --speaker "AO Cable A" --duration 90 \
        --out results/bugb_runtime/livecall/phase1_cableA.wav
"""
import argparse
import sys
import numpy as np
import soundcard as sc
from scipy.io import wavfile

RATE = 48000
CHANNELS = 2


def find_loopback_mic(name_contains: str):
    mics = sc.all_microphones(include_loopback=True)
    # Prefer loopback entries (isloopback attribute)
    matches = []
    for m in mics:
        try:
            if name_contains.lower() in m.name.lower() and getattr(m, "isloopback", False):
                matches.append(m)
        except Exception:
            continue
    if not matches:
        print("Available loopback mics:")
        for m in mics:
            if getattr(m, "isloopback", False):
                print(f"  {m.name}")
        sys.exit(f"no loopback mic matching '{name_contains}'")
    return matches[0]


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--speaker", required=True, help="render endpoint name substring")
    p.add_argument("--duration", type=float, default=90.0)
    p.add_argument("--out", required=True)
    args = p.parse_args()

    mic = find_loopback_mic(args.speaker)
    print(f"loopback: {mic.name}  {args.duration}s  {RATE}/int16/{CHANNELS}ch")

    total = int(args.duration * RATE)
    with mic.recorder(samplerate=RATE, channels=CHANNELS, blocksize=1024) as rec:
        data = rec.record(numframes=total)

    # soundcard returns float32, convert to int16
    if data.dtype != np.int16:
        data = np.clip(data * 32767.0, -32768, 32767).astype(np.int16)
    wavfile.write(args.out, RATE, data)
    peak = int(np.max(np.abs(data)))
    rms = float(np.sqrt(np.mean(data.astype(np.float64) ** 2)))
    print(f"saved {args.out}  peak={peak}  rms={rms:.1f}")


if __name__ == "__main__":
    main()
