"""
Manual-dial variant of the live-call quality test.

Usage:
    py -3 run_test_call_manual.py
    py -3 run_test_call_manual.py --wait 15
    py -3 run_test_call_manual.py --prompt

Flow:
    1. Switch system default audio to AO/VB cable profile
    2. User manually places or answers the Phone Link call
    3. Script waits a short fixed window for call setup
    4. Script plays direct TTS to the uplink device
    5. Script starts the AI conversation bridge
    6. Restore original default devices
"""

from __future__ import annotations

import argparse
import logging
import os
import threading
import time

from dotenv import load_dotenv

load_dotenv(os.path.join(os.path.dirname(__file__), ".env"))

from audio_bridge import AudioBridge
from audio_router import restore_routing, set_call_routing
from realtime_engine import EndReason, RealtimeEngine


TEST_SCRIPT = {
    "name": "driver-test-manual",
    "opening_message": "안녕하세요. AO 오디오브리지 테스트 통화입니다. 잘 들리시나요?",
    "system_prompt": (
        "You are on a test phone call to verify audio driver quality. "
        "Speak in Korean. Ask the caller if they can hear you clearly, "
        "then have a brief natural conversation (2-3 exchanges). "
        "If they report audio issues, ask them to describe what they hear."
    ),
    "closing_message": "",
    "max_turns": 5,
}

log = logging.getLogger(__name__)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--wait",
        type=float,
        default=8.0,
        help="Seconds to wait before starting TTS/AI. Default: 8s.",
    )
    parser.add_argument(
        "--prompt",
        action="store_true",
        help="Wait for Enter before starting the --wait countdown.",
    )
    return parser.parse_args()


def _play_direct_tts(playback_dev: str) -> None:
    import pyaudio
    import numpy as np
    from openai import OpenAI

    log.info("[3b] Generating TTS and playing directly to %s...", playback_dev)

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
            input="안녕하세요. 테스트입니다. 제 목소리가 잘 들리시나요?",
            response_format="pcm",
        )
        tts_pcm = resp.content
        log.info("[3b] TTS PCM: %d bytes (24kHz)", len(tts_pcm))

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
            stream.write(upsampled.tobytes())
        finally:
            stream.stop_stream()
            stream.close()

        log.info("[3b] Direct TTS playback done")
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
                os.path.join(log_dir, "test_call_manual.log"), encoding="utf-8"
            ),
        ],
        force=True,
    )

    cable_profiles = {
        "ao": {
            "capture_device": "AO Cable A",
            "playback_device": "AO Cable B",
            "routing_output": "AO Cable A",
            "routing_input": "AO Cable B",
        },
        "vb": {
            "capture_device": "CABLE-A Output",
            "playback_device": "CABLE-B Input",
            "routing_output": "CABLE-A Input",
            "routing_input": "CABLE-B Output",
        },
    }
    profile_name = os.getenv("AUDIO_CABLE_PROFILE", "ao").strip().lower()
    profile = cable_profiles.get(profile_name, cable_profiles["ao"])

    capture_dev = os.getenv("CAPTURE_DEVICE_NAME", profile["capture_device"])
    playback_dev = os.getenv("PLAYBACK_DEVICE_NAME", profile["playback_device"])
    routing_out = os.getenv("ROUTING_OUTPUT_DEVICE", profile["routing_output"])
    routing_in = os.getenv("ROUTING_INPUT_DEVICE", profile["routing_input"])

    log.info("=== %s Cable Manual Test Call ===", profile_name.upper())
    log.info("Capture: %s  Playback: %s", capture_dev, playback_dev)

    log.info("[1/4] Setting audio routing...")
    set_call_routing(routing_out, routing_in)

    try:
        log.info("[2/4] Manual call stage")
        log.info("Place or answer the Phone Link call manually now.")
        if args.prompt:
            input("When the call is connected, press Enter to continue...")
        if args.wait > 0:
            log.info("Waiting %.1fs before TTS/AI start...", args.wait)
            time.sleep(max(0.0, args.wait))

        _play_direct_tts(playback_dev)
        time.sleep(1.0)

        log.info("[3/4] Starting AI conversation...")
        engine = RealtimeEngine()
        session = engine.new_session(phone_id=0, script=TEST_SCRIPT)
        bridge = AudioBridge(capture_dev, playback_dev)
        stop = threading.Event()

        bridge.open()
        try:
            engine.preconnect(session)
            if not engine.wait_until_ready(5.0):
                log.error("Realtime API connection timeout")
                return

            reason = engine.run(session, bridge, stop)
        finally:
            stop.set()
            bridge.close()

        log.info("[4/4] Call ended: %s", reason.value)
        log.info("--- Conversation ---")
        for line in session.log_lines:
            log.info("  %s", line)
        log.info("Turns: user=%d  ai=%d", session.user_turns, session.assistant_turns)
    finally:
        log.info("Restoring audio routing...")
        restore_routing()
        log.info("=== Manual Test Complete ===")


if __name__ == "__main__":
    main()
