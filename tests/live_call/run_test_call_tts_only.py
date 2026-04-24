"""
Manual-dial + direct TTS only variant.

Purpose:
    Use this when the machine is too slow for the full realtime bridge/AI loop.
    It performs routing, waits a short window for a manual Phone Link call to
    connect, plays one direct TTS utterance into the uplink device, then exits.

Usage:
    py -3 run_test_call_tts_only.py
    py -3 run_test_call_tts_only.py --wait 8
    py -3 run_test_call_tts_only.py --prompt
"""

from __future__ import annotations

import argparse
import logging
import os
import time

from dotenv import load_dotenv

load_dotenv(os.path.join(os.path.dirname(__file__), ".env"))

from audio_router import restore_routing, set_call_routing


log = logging.getLogger(__name__)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--wait",
        type=float,
        default=8.0,
        help="Seconds to wait before TTS playback. Default: 8s.",
    )
    parser.add_argument(
        "--prompt",
        action="store_true",
        help="Wait for Enter before starting the --wait countdown.",
    )
    return parser.parse_args()


def _play_direct_tts(playback_dev: str) -> None:
    import numpy as np
    import pyaudio
    from openai import OpenAI

    log.info("[3/3] Generating TTS and playing directly to %s...", playback_dev)

    pa = pyaudio.PyAudio()
    play_idx = None
    try:
        for i in range(pa.get_device_count()):
            info = pa.get_device_info_by_index(i)
            if (
                playback_dev.lower() in info["name"].lower()
                and info["maxOutputChannels"] > 0
            ):
                play_idx = i
                break

        if play_idx is None:
            raise RuntimeError(f"Could not find playback device: {playback_dev}")

        client = OpenAI()
        resp = client.audio.speech.create(
            model="tts-1",
            voice="nova",
            input="안녕하세요. 테스트 통화입니다. 제 목소리가 잘 들리시나요?",
            response_format="pcm",
        )
        tts_pcm = resp.content
        log.info("[3/3] TTS PCM: %d bytes (24kHz)", len(tts_pcm))

        samples = np.frombuffer(tts_pcm, dtype=np.int16)
        target_len = len(samples) * 2
        src_x = np.linspace(0, 1, len(samples), endpoint=False)
        tgt_x = np.linspace(0, 1, target_len, endpoint=False)
        upsampled = np.interp(tgt_x, src_x, samples.astype(np.float32)).astype(
            np.int16
        )

        stream = pa.open(
            format=pyaudio.paInt16,
            channels=1,
            rate=48000,
            output=True,
            output_device_index=play_idx,
        )
        try:
            log.info("[3/3] Playback start")
            stream.write(upsampled.tobytes())
            log.info("[3/3] Playback done")
        finally:
            stream.stop_stream()
            stream.close()
    finally:
        pa.terminate()


def main() -> None:
    args = parse_args()

    log_dir = os.path.join(os.path.dirname(__file__), "runtime_logs")
    os.makedirs(log_dir, exist_ok=True)
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(message)s",
        handlers=[
            logging.StreamHandler(),
            logging.FileHandler(
                os.path.join(log_dir, "test_call_tts_only.log"), encoding="utf-8"
            ),
        ],
        force=True,
    )

    cable_profiles = {
        "ao": {
            "playback_device": "AO Cable B",
            "routing_output": "AO Cable A",
            "routing_input": "AO Cable B",
        },
        "vb": {
            "playback_device": "CABLE-B Input",
            "routing_output": "CABLE-A Input",
            "routing_input": "CABLE-B Output",
        },
    }
    profile_name = os.getenv("AUDIO_CABLE_PROFILE", "ao").strip().lower()
    profile = cable_profiles.get(profile_name, cable_profiles["ao"])

    playback_dev = os.getenv("PLAYBACK_DEVICE_NAME", profile["playback_device"])
    routing_out = os.getenv("ROUTING_OUTPUT_DEVICE", profile["routing_output"])
    routing_in = os.getenv("ROUTING_INPUT_DEVICE", profile["routing_input"])

    log.info("=== %s Cable Manual TTS-Only Test ===", profile_name.upper())
    log.info("Playback: %s", playback_dev)

    log.info("[1/3] Setting audio routing...")
    set_call_routing(routing_out, routing_in)

    try:
        log.info("[2/3] Manual call stage")
        log.info("Place or answer the Phone Link call manually now.")
        if args.prompt:
            input("When the call is connected, press Enter to continue...")
        if args.wait > 0:
            log.info("Waiting %.1fs before TTS playback...", args.wait)
            time.sleep(max(0.0, args.wait))

        _play_direct_tts(playback_dev)
    finally:
        log.info("Restoring audio routing...")
        restore_routing()
        log.info("=== Manual TTS-Only Test Complete ===")


if __name__ == "__main__":
    main()
