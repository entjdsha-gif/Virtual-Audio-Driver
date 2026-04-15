"""Unified soundcard capture: speaker loopback OR mic read, WASAPI shared.

Usage:
    python tests/cap_sc.py --mode speaker --name "AO Cable A" --duration 90 --out A_spk.wav
    python tests/cap_sc.py --mode mic     --name "AO Cable A" --duration 90 --out A_mic.wav

In mic mode, WASAPI shared is used so multiple readers can coexist with
another process reading the same mic (if that other process is also shared).
"""
import argparse
import sys
import numpy as np
import soundcard as sc
from scipy.io import wavfile

RATE = 48000
CHANNELS = 2


def find_speaker_loopback(name):
    mics = sc.all_microphones(include_loopback=True)
    for m in mics:
        try:
            if name.lower() in m.name.lower() and getattr(m, "isloopback", False):
                return m
        except Exception:
            pass
    print("Available loopback mics:")
    for m in mics:
        if getattr(m, "isloopback", False):
            print(f"  {m.name}")
    sys.exit(f"no loopback for '{name}'")


def find_regular_mic(name):
    mics = sc.all_microphones(include_loopback=False)
    for m in mics:
        try:
            if name.lower() in m.name.lower():
                return m
        except Exception:
            pass
    print("Available mics:")
    for m in mics:
        print(f"  {m.name}")
    sys.exit(f"no mic for '{name}'")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--mode", choices=["speaker", "mic"], required=True)
    p.add_argument("--name", required=True)
    p.add_argument("--duration", type=float, default=90.0)
    p.add_argument("--out", required=True)
    args = p.parse_args()

    dev = find_speaker_loopback(args.name) if args.mode == "speaker" else find_regular_mic(args.name)
    print(f"[{args.mode}] {dev.name}  {args.duration}s  {RATE}/int16/{CHANNELS}ch -> {args.out}")

    total = int(args.duration * RATE)
    try:
        with dev.recorder(samplerate=RATE, channels=CHANNELS, blocksize=1024) as rec:
            data = rec.record(numframes=total)
    except Exception as e:
        sys.exit(f"capture failed: {e}")

    if data.dtype != np.int16:
        data = np.clip(data * 32767.0, -32768, 32767).astype(np.int16)
    wavfile.write(args.out, RATE, data)
    peak = int(np.max(np.abs(data)))
    rms = float(np.sqrt(np.mean(data.astype(np.float64) ** 2)))
    print(f"saved {args.out}  peak={peak}  rms={rms:.1f}")


if __name__ == "__main__":
    main()
