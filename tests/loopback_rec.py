"""
Loopback quality check — capture from a cable's Output (mic side) to wav.

Usage:
    # List devices
    python tests/loopback_rec.py --list

    # Record 15s from AO Cable A Output
    python tests/loopback_rec.py --device "AO Cable A" --duration 15 \
        --out results/g6_runtime/ao_loopback_a.wav

    # Record 15s from VB CABLE-A Output
    python tests/loopback_rec.py --device "CABLE-A Output" --duration 15 \
        --out results/g6_runtime/vb_loopback_a.wav

Procedure:
    1. In Windows Sound panel, set the matching Input side as default playback
       (e.g. "AO Cable A Input" for AO, "CABLE-A Input" for VB).
    2. Start this script.
    3. While it records, play any audio (YouTube, WMP, TTS, music) — it will
       go to the default playback = Cable Input side.
    4. Script captures from Cable Output side = reader of the same pipe.
    5. Open the resulting wav and listen. Clean = driver pipe fine.
       Garbled = pipe DSP issue.
"""
from __future__ import annotations

import argparse
import sys
import numpy as np
import sounddevice as sd
from scipy.io import wavfile

RATE = 48000
CHANNELS = 2
DTYPE = "int16"


def list_devices() -> None:
    for i, d in enumerate(sd.query_devices()):
        if d["max_input_channels"] > 0:
            print(f"  [{i:3d}] in  ch={d['max_input_channels']}  {d['name']}")
    for i, d in enumerate(sd.query_devices()):
        if d["max_output_channels"] > 0:
            print(f"  [{i:3d}] out ch={d['max_output_channels']}  {d['name']}")


def find_input(name_contains: str) -> int:
    hits = []
    for i, d in enumerate(sd.query_devices()):
        if d["max_input_channels"] > 0 and name_contains.lower() in d["name"].lower():
            hits.append((i, d["name"]))
    if not hits:
        sys.exit(f"no input device matching '{name_contains}'")
    if len(hits) > 1:
        print(f"multiple matches, picking first: {hits[0][1]}")
    return hits[0][0]


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--list", action="store_true")
    p.add_argument("--device", type=str)
    p.add_argument("--duration", type=float, default=15.0)
    p.add_argument("--out", type=str)
    args = p.parse_args()

    if args.list:
        list_devices()
        return

    if not args.device or not args.out:
        sys.exit("--device and --out required (or --list)")

    idx = find_input(args.device)
    info = sd.query_devices(idx)
    print(f"capture [{idx}] {info['name']}  {args.duration}s  {RATE}/{DTYPE}/{CHANNELS}ch")
    print("START PLAYING AUDIO NOW (YouTube / WMP / anything on default playback)")

    frames = sd.rec(
        int(args.duration * RATE),
        samplerate=RATE,
        channels=CHANNELS,
        dtype=DTYPE,
        device=idx,
    )
    sd.wait()

    wavfile.write(args.out, RATE, frames)
    peak = int(np.max(np.abs(frames)))
    rms = float(np.sqrt(np.mean(frames.astype(np.float64) ** 2)))
    print(f"saved {args.out}  peak={peak}  rms={rms:.1f}  (silence = both ~0)")


if __name__ == "__main__":
    main()
