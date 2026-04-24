"""
Play a deterministic outbound payload directly into the cable uplink device.

Purpose:
    Use this during an active Phone Link call so WinDbg can observe a real
    outbound audio payload without depending on the full AI conversation path.

Usage:
    py -3 play_cable_payload.py
    py -3 play_cable_payload.py --profile vb
    py -3 play_cable_payload.py --device "CABLE-B Input" --repeat 12
    py -3 play_cable_payload.py --device "AO Cable B" --seconds 8
"""

from __future__ import annotations

import argparse
import array
import logging
import math
import os
import sys
import time
from typing import Optional

import pyaudio

try:
    from dotenv import load_dotenv
except ImportError:
    load_dotenv = None


RATE = 48000
CHANNELS = 1
FORMAT = pyaudio.paInt16
AMPLITUDE = 0.22
DEFAULT_REPEAT = 10


def _find_output_device(pa: pyaudio.PyAudio, name: str) -> Optional[int]:
    if name.isdigit():
        index = int(name)
        info = pa.get_device_info_by_index(index)
        if info["maxOutputChannels"] > 0:
            return index
        raise RuntimeError(f"Device index {index} is not an output device")

    for index in range(pa.get_device_count()):
        info = pa.get_device_info_by_index(index)
        if info["maxOutputChannels"] <= 0:
            continue
        if name.lower() in info["name"].lower():
            return index
    return None


def _default_device_for_profile(profile_name: str) -> str:
    if profile_name == "ao":
        return "AO Cable B"
    return "CABLE-B Input"


def _tone(freq_hz: float, duration_sec: float) -> array.array:
    frame_count = int(RATE * duration_sec)
    samples = array.array("h")
    for i in range(frame_count):
        t = i / RATE
        sample = int(AMPLITUDE * 32767.0 * math.sin(2.0 * math.pi * freq_hz * t))
        if sample > 32767:
            sample = 32767
        elif sample < -32768:
            sample = -32768
        samples.append(sample)
    return samples


def _silence(duration_sec: float) -> array.array:
    return array.array("h", [0] * int(RATE * duration_sec))


def _build_payload(repeat: int) -> bytes:
    # Audible but simple pattern: two tones plus short silence, repeated.
    chunk = array.array("h")
    chunk.extend(_tone(880.0, 0.30))
    chunk.extend(_silence(0.08))
    chunk.extend(_tone(660.0, 0.30))
    chunk.extend(_silence(0.12))
    chunk.extend(_tone(1046.5, 0.22))
    chunk.extend(_silence(0.18))

    payload = array.array("h")
    for _ in range(max(1, repeat)):
        payload.extend(chunk)
    return payload.tobytes()


def _list_output_devices(pa: pyaudio.PyAudio) -> None:
    logging.info("Available output devices:")
    for index in range(pa.get_device_count()):
        info = pa.get_device_info_by_index(index)
        if info["maxOutputChannels"] > 0:
            logging.info("  [%s] %s", index, info["name"])


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", choices=["vb", "ao"], default=None)
    parser.add_argument("--device", default=None)
    parser.add_argument("--repeat", type=int, default=DEFAULT_REPEAT)
    parser.add_argument(
        "--seconds",
        type=float,
        default=None,
        help="Optional target duration. Overrides repeat.",
    )
    parser.add_argument(
        "--list-devices",
        action="store_true",
        help="List output devices and exit.",
    )
    return parser.parse_args()


def main() -> int:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    if load_dotenv is not None:
        load_dotenv(os.path.join(script_dir, ".env"))

    log_dir = os.path.join(script_dir, "runtime_logs")
    os.makedirs(log_dir, exist_ok=True)
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(message)s",
        handlers=[
            logging.StreamHandler(),
            logging.FileHandler(
                os.path.join(log_dir, "payload_probe.log"), encoding="utf-8"
            ),
        ],
        force=True,
    )

    args = parse_args()
    profile_name = args.profile or os.getenv("AUDIO_CABLE_PROFILE", "vb").strip().lower()
    if profile_name not in ("vb", "ao"):
        profile_name = "vb"

    device_name = (
        args.device
        or os.getenv("PLAYBACK_DEVICE_NAME")
        or _default_device_for_profile(profile_name)
    )

    logging.info("=== Cable Payload Probe ===")
    logging.info("Profile: %s", profile_name)
    logging.info("Playback target: %s", device_name)

    pa = pyaudio.PyAudio()
    try:
        if args.list_devices:
            _list_output_devices(pa)
            return 0

        device_index = _find_output_device(pa, device_name)
        if device_index is None:
            _list_output_devices(pa)
            raise RuntimeError(f"Output device not found: {device_name}")

        info = pa.get_device_info_by_index(device_index)
        logging.info("Using output device [%s] %s", device_index, info["name"])

        if args.seconds is not None:
            seconds = max(0.5, float(args.seconds))
            chunk_sec = 0.30 + 0.08 + 0.30 + 0.12 + 0.22 + 0.18
            repeat = max(1, int(math.ceil(seconds / chunk_sec)))
        else:
            repeat = max(1, int(args.repeat))

        payload = _build_payload(repeat)
        sample_count = len(payload) // 2
        duration_sec = sample_count / RATE
        logging.info(
            "Prepared payload: samples=%s duration=%.2fs repeat=%s",
            sample_count,
            duration_sec,
            repeat,
        )

        stream = pa.open(
            format=FORMAT,
            channels=CHANNELS,
            rate=RATE,
            output=True,
            output_device_index=device_index,
        )
        try:
            start = time.monotonic()
            logging.info("Playback start")
            stream.write(payload)
            elapsed = time.monotonic() - start
            logging.info("Playback done in %.2fs", elapsed)
        finally:
            stream.stop_stream()
            stream.close()
    except Exception as exc:
        logging.exception("Payload probe failed: %s", exc)
        return 1
    finally:
        pa.terminate()

    logging.info("=== Payload Probe Complete ===")
    return 0


if __name__ == "__main__":
    sys.exit(main())
