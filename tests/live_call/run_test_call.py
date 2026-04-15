"""
AO Virtual Cable — live call quality test.

Usage:
    python run_test_call.py              # dial default test number
    python run_test_call.py 01012345678  # dial specific number

Flow:
    1. Switch system default audio to AO Cable A/B
    2. Dial via Phone Link
    3. Wait for answer (popup detection)
    4. Run AI conversation through the cable
    5. Restore original default devices
"""
import logging
import os
import sys
import threading
import time

from dotenv import load_dotenv

load_dotenv(os.path.join(os.path.dirname(__file__), ".env"))

from audio_bridge import AudioBridge
from audio_router import set_call_routing, restore_routing
from phone_link_dialer import dial
from realtime_engine import RealtimeEngine, EndReason

TEST_NUMBER = "01058289554"
ANSWER_WAIT_SEC = 3  # seconds to wait after dial before starting AI

TEST_SCRIPT = {
    "name": "driver-test",
    "opening_message": "안녕하세요, AO 오디오 드라이버 테스트 통화입니다. 잘 들리시나요?",
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


def main() -> None:
    phone = sys.argv[1] if len(sys.argv) > 1 else TEST_NUMBER

    log_dir = os.path.join(os.path.dirname(__file__), "runtime_logs")
    os.makedirs(log_dir, exist_ok=True)
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(message)s",
        handlers=[
            logging.StreamHandler(),
            logging.FileHandler(
                os.path.join(log_dir, "test_call.log"), encoding="utf-8"
            ),
        ],
        force=True,
    )

    # Cable profile: AUDIO_CABLE_PROFILE=ao (default) or vb
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

    log.info("=== %s Cable Test Call ===", profile_name.upper())
    log.info("Target: %s", phone)
    log.info("Capture: %s  Playback: %s", capture_dev, playback_dev)

    # 1. Route
    log.info("[1/5] Setting audio routing...")
    set_call_routing(routing_out, routing_in)

    try:
        # 2. Dial
        log.info("[2/5] Dialing %s ...", phone)
        accepted = dial(phone)
        if not accepted:
            log.error("Dial failed — Phone Link could not initiate call")
            return

        # 3. Wait for pickup
        log.info("[3/5] Waiting %ds for pickup...", ANSWER_WAIT_SEC)
        time.sleep(ANSWER_WAIT_SEC)

        # 3b. Direct TTS test — bypass bridge, write TTS PCM straight to Cable B
        log.info("[3b] Generating TTS and playing directly to %s...", playback_dev)
        import numpy as np
        import pyaudio
        from openai import OpenAI
        _pa = pyaudio.PyAudio()
        _play_idx = None
        for i in range(_pa.get_device_count()):
            info = _pa.get_device_info_by_index(i)
            if playback_dev.lower() in info['name'].lower() and info['maxOutputChannels'] > 0:
                _play_idx = i
                break
        if _play_idx is not None:
            # Get TTS PCM from OpenAI (24kHz mono int16)
            client = OpenAI()
            resp = client.audio.speech.create(
                model="tts-1", voice="nova", input="안녕하세요, 테스트입니다. 잘 들리시나요?",
                response_format="pcm",
            )
            tts_pcm = resp.content
            log.info("[3b] TTS PCM: %d bytes (24kHz)", len(tts_pcm))

            # Upsample 24k -> 48k
            samples = np.frombuffer(tts_pcm, dtype=np.int16)
            target_len = len(samples) * 2
            src_x = np.linspace(0, 1, len(samples), endpoint=False)
            tgt_x = np.linspace(0, 1, target_len, endpoint=False)
            upsampled = np.interp(tgt_x, src_x, samples.astype(np.float32)).astype(np.int16)
            log.info("[3b] Upsampled to 48kHz: %d samples, peak=%d", len(upsampled), int(np.max(np.abs(upsampled))))

            PLAY_RATE = 48000
            _stream = _pa.open(format=pyaudio.paInt16, channels=1, rate=PLAY_RATE,
                               output=True, output_device_index=_play_idx)
            _stream.write(upsampled.tobytes())
            _stream.stop_stream()
            _stream.close()
            log.info("[3b] Direct TTS playback done")
        else:
            log.error("[3b] Could not find playback device")
        _pa.terminate()

        # Wait for TTS audio to fully drain through Phone Link before AI starts
        time.sleep(1.0)

        # 4. AI conversation
        log.info("[4/5] Starting AI conversation...")
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

        # 5. Results
        log.info("[5/5] Call ended: %s", reason.value)
        log.info("--- Conversation ---")
        for line in session.log_lines:
            log.info("  %s", line)
        log.info("Turns: user=%d  ai=%d", session.user_turns, session.assistant_turns)

    finally:
        log.info("Restoring audio routing...")
        restore_routing()
        log.info("=== Test Complete ===")


if __name__ == "__main__":
    main()
