import base64
import logging
import os
import queue
import threading
import time
from dataclasses import dataclass, field
from enum import Enum
from typing import Optional

import numpy as np
from openai import OpenAI

from audio_bridge import AudioBridge, CAPTURE_RATE

log = logging.getLogger(__name__)

REALTIME_PCM_RATE = 24000
PLAYBACK_BATCH_TARGET_MS = 120
PLAYBACK_BATCH_TARGET_BYTES = int(REALTIME_PCM_RATE * 2 * (PLAYBACK_BATCH_TARGET_MS / 1000.0))
PLAYBACK_BATCH_MAX_WAIT_SEC = 0.06
PLAYBACK_DRAIN_TIMEOUT_SEC = 2.0
PLAYBACK_DRAIN_TAIL_SEC = 0.25
OPENING_CAPTURE_FLUSH_SEC = float(os.getenv("OPENING_CAPTURE_FLUSH_SECONDS", "0.10"))
AMD_LISTEN_SEC = float(os.getenv("AMD_LISTEN_SECONDS", "1.5"))
AMD_EARLY_SPEECH_THRESHOLD_SEC = float(os.getenv("AMD_EARLY_SPEECH_THRESHOLD_SECONDS", "0.3"))
VOICEMAIL_KEYWORDS = [
    "메시지를 남겨", "삐 소리", "연결이 되지 않", "자리를 비웠",
    "사서함", "받을 수 없", "전원이 꺼져", "통화 중이", "음성사서함",
    "leave a message", "not available", "voicemail",
]


class EndReason(str, Enum):
    NONE = "none"
    NORMAL = "normal_end"
    VOICEMAIL = "voicemail"
    USER_BUSY = "user_busy"
    MAX_TURNS = "max_turns"
    HARD_CAP = "hard_cap"
    SILENCE = "silence_timeout"
    STT_FAIL = "stt_fail"
    API_ERROR = "api_error"


@dataclass
class RealtimeCallSession:
    phone_id: int
    script_name: str
    opening_message: str
    instructions: str
    max_turns: int
    log_lines: list[str] = field(default_factory=list)
    user_turns: int = 0
    assistant_turns: int = 0
    last_activity_at: float = field(default_factory=time.time)
    ignore_next_assistant_transcript: bool = True
    opening_pcm: Optional[bytes] = None
    opening_pcm_ready: threading.Event = field(default_factory=threading.Event)
    amd_phase: bool = False
    amd_result: Optional[str] = None
    amd_event: threading.Event = field(default_factory=threading.Event)


_OPENING_PCM_CACHE: dict[tuple[str, str, str], bytes] = {}


class RealtimeEngine:
    def __init__(self):
        self.api_key = os.getenv("OPENAI_API_KEY")
        self.model = os.getenv("REALTIME_MODEL", "gpt-realtime-mini")
        self.voice = os.getenv("REALTIME_VOICE", "marin")
        self.opening_tts_model = os.getenv("OPENING_TTS_MODEL", "tts-1")
        self.opening_tts_voice = os.getenv("OPENING_TTS_VOICE", os.getenv("TTS_VOICE", "nova"))
        self.transcription_model = os.getenv(
            "REALTIME_TRANSCRIPTION_MODEL",
            "gpt-4o-mini-transcribe",
        )
        self.max_output_tokens = int(os.getenv("REALTIME_MAX_OUTPUT_TOKENS", 512))
        self.max_call_duration = int(os.getenv("MAX_CALL_DURATION_SECONDS", 180))
        self.silence_timeout = int(os.getenv("SILENCE_TIMEOUT_SECONDS", 10))

        self._client = OpenAI(api_key=self.api_key) if self.api_key else None
        self._connection = None
        self._connection_cm = None
        self._send_lock = threading.Lock()
        self._internal_stop = threading.Event()
        self._response_in_flight = threading.Event()
        self._playback_queue: "queue.Queue[bytes]" = queue.Queue()
        self._fatal_error: Optional[Exception] = None
        self._ready_event = threading.Event()
        self._preconnected_phone_id: Optional[int] = None
        self._receiver_thread: Optional[threading.Thread] = None
        self._current_bridge: Optional[AudioBridge] = None
        self._capture_bytes_total = 0

    def new_session(self, phone_id: int, script: dict) -> RealtimeCallSession:
        system_prompt = script.get("system_prompt", "").strip()
        style = (
            "You are responding on a live phone call. "
            "Reply immediately in concise spoken Korean unless the caller changes language. "
            "Use one or two short sentences. "
            "If you are unsure, say you are unsure instead of guessing. "
            "Do not narrate actions or mention being an AI unless the script requires it."
        )
        instructions = "\n\n".join(part for part in [system_prompt, style] if part)

        session = RealtimeCallSession(
            phone_id=phone_id,
            script_name=script.get("name", "realtime"),
            opening_message=script.get("opening_message", "").strip(),
            instructions=instructions,
            max_turns=int(script.get("max_turns", 5)),
        )
        if session.opening_message:
            threading.Thread(
                target=self._prefetch_opening_pcm,
                args=(session,),
                daemon=True,
                name=f"rt-opening-{phone_id}",
            ).start()
        else:
            session.opening_pcm_ready.set()
        return session

    def request_stop(self) -> None:
        self._internal_stop.set()
        connection = self._connection
        if connection is not None:
            try:
                connection.close()
            except Exception:
                pass
        self._close_connection()

    def preconnect(self, session: RealtimeCallSession) -> None:
        if not self._client:
            raise RuntimeError("OPENAI_API_KEY is not set")

        if (
            self._connection is not None
            and self._preconnected_phone_id == session.phone_id
            and self._ready_event.is_set()
        ):
            return

        self._internal_stop.clear()
        self._response_in_flight.clear()
        self._fatal_error = None
        self._playback_queue = queue.Queue()
        self._ready_event.clear()
        self._preconnected_phone_id = session.phone_id

        try:
            log.info("[%s] Connecting to Realtime API model=%s", session.phone_id, self.model)
            self._connection_cm = self._client.realtime.connect(model=self.model)
            self._connection = self._connection_cm.__enter__()
            log.info("[%s] Realtime API connected, configuring session", session.phone_id)
            self._configure_session(self._connection, session)
            self._receiver_thread = threading.Thread(
                target=self._receive_loop,
                args=(self._connection, session),
                daemon=True,
                name=f"rt-recv-{session.phone_id}",
            )
            self._receiver_thread.start()
            self._ready_event.set()
        except Exception:
            self._close_connection()
            raise

    def wait_until_ready(self, timeout: float) -> bool:
        return self._ready_event.wait(timeout)

    def get_log(self, session: RealtimeCallSession) -> str:
        return "\n".join(session.log_lines)

    def run(self, session: RealtimeCallSession, bridge: AudioBridge, stop_event: threading.Event) -> EndReason:
        if not self._client:
            raise RuntimeError("OPENAI_API_KEY is not set")

        self._internal_stop.clear()
        self._response_in_flight.clear()
        self._fatal_error = None
        self._playback_queue = queue.Queue()
        self._current_bridge = bridge
        self._capture_bytes_total = 0

        start_time = time.time()
        session.last_activity_at = start_time
        end_reason = EndReason.NORMAL

        try:
            opening_pcm = self._wait_for_opening_pcm(session, timeout=0.8)
            if self._connection is None or self._preconnected_phone_id != session.phone_id:
                self.preconnect(session)
            elif not self.wait_until_ready(timeout=2.5):
                raise RuntimeError("Realtime preconnect did not become ready in time")

            connection = self._connection
            if connection is None:
                raise RuntimeError("Realtime connection is not available")

            playback = threading.Thread(
                target=self._playback_loop,
                args=(bridge,),
                daemon=True,
                name=f"rt-play-{session.phone_id}",
            )
            playback.start()

            self._flush_capture(bridge, duration_sec=OPENING_CAPTURE_FLUSH_SEC)

            # Capture must start BEFORE AMD so Realtime API can hear incoming audio
            capture = threading.Thread(
                target=self._capture_loop,
                args=(connection, bridge, session, stop_event),
                daemon=True,
                name=f"rt-cap-{session.phone_id}",
            )
            capture.start()

            # AMD: classify human vs voicemail before playing opening
            amd_result = self._run_amd(connection, session)
            if stop_event.is_set() or self._internal_stop.is_set() or amd_result == "stopped":
                log.info("[%s] stop detected before opening playback; skipping opening", session.phone_id)
                end_reason = EndReason.SILENCE
            elif amd_result == "voicemail":
                log.info("[%s] AMD: voicemail/machine detected — ending call", session.phone_id)
                end_reason = EndReason.VOICEMAIL
            else:
                if opening_pcm:
                    session.ignore_next_assistant_transcript = False
                    log.info("[%s] AI(opening/start): %s", session.phone_id, session.opening_message)
                    bridge.play_tts(opening_pcm)
                    session.log_lines.append(f"[AI] {session.opening_message}")
                    session.assistant_turns += 1
                    session.last_activity_at = time.time()
                    log.info("[%s] AI(opening/local): %s", session.phone_id, session.opening_message)
                else:
                    log.info("[%s] AI(opening/realtime-request): %s", session.phone_id, session.opening_message)
                    self._send_initial_greeting(connection, session)

            if end_reason != EndReason.VOICEMAIL:
                while True:
                    if self._fatal_error is not None:
                        log.error("[%s] Fatal error detected: %s", session.phone_id, self._fatal_error)
                        raise self._fatal_error
                    if stop_event.is_set() or self._internal_stop.is_set():
                        log.info("[%s] Stop detected: stop_event=%s internal=%s", session.phone_id, stop_event.is_set(), self._internal_stop.is_set())
                        end_reason = EndReason.SILENCE
                        break

                    now = time.time()
                    if now - start_time >= self.max_call_duration:
                        end_reason = EndReason.HARD_CAP
                        break

                    if (
                        session.assistant_turns >= session.max_turns
                        and not self._response_in_flight.is_set()
                    ):
                        end_reason = EndReason.MAX_TURNS
                        break

                    if (
                        session.assistant_turns > 0
                        and not self._response_in_flight.is_set()
                        and now - session.last_activity_at >= self.silence_timeout
                    ):
                        end_reason = EndReason.SILENCE
                        break

                    time.sleep(0.1)

        except Exception as exc:
            log.exception("[%s] realtime failure: %s", session.phone_id, exc)
            end_reason = EndReason.API_ERROR
        finally:
            if end_reason != EndReason.API_ERROR:
                self._wait_for_playback_drain()
            self.request_stop()
            self._connection = None
            self._current_bridge = None
            self._ready_event.clear()
            self._preconnected_phone_id = None
            self._drain_playback_queue()
            bridge.set_playing(False)

        return end_reason

    def _run_amd(self, connection, session: RealtimeCallSession) -> str:
        """Listen for AMD_LISTEN_SEC, ask Realtime API to classify HUMAN vs VOICEMAIL."""
        if os.getenv("AMD_ENABLED", "true").lower() not in ("1", "true", "yes"):
            log.info("[%s] AMD disabled", session.phone_id)
            return "human"

        session.amd_phase = True
        session.amd_result = None
        session.amd_event.clear()
        t0 = time.time()
        log.info("[%s] AMD: listening %.1fs for classification", session.phone_id, AMD_LISTEN_SEC)
        deadline = time.monotonic() + AMD_LISTEN_SEC
        while time.monotonic() < deadline:
            if self._internal_stop.is_set():
                session.amd_phase = False
                return "stopped"
            time.sleep(0.05)

        # Explicitly commit audio buffer in case VAD hasn't auto-committed yet
        min_commit_bytes = int(REALTIME_PCM_RATE * 2 * 0.10)
        if self._internal_stop.is_set():
            session.amd_phase = False
            return "stopped"
        if self._capture_bytes_total >= min_commit_bytes:
            try:
                with self._send_lock:
                    connection.input_audio_buffer.commit()
                log.info("[%s] AMD: buffer committed (%.2fs elapsed)", session.phone_id, time.time() - t0)
            except Exception as exc:
                log.warning("[%s] AMD: buffer commit failed: %s", session.phone_id, exc)
        else:
            log.info(
                "[%s] AMD: skipping commit, buffered audio too small (%s bytes)",
                session.phone_id,
                self._capture_bytes_total,
            )
            session.amd_phase = False
            return "human"

        # Ask Realtime API to classify — request text-only to avoid audio playback
        amd_instructions = (
            "Listen carefully to the audio you just received. "
            "Is this a live human voice directly answering the phone, "
            "or is it a voicemail system, answering machine, or automated announcement? "
            "Reply with exactly one word: HUMAN or VOICEMAIL."
        )
        payload = {
            "type": "response.create",
            "response": {
                "instructions": amd_instructions,
                "output_modalities": ["text"],
                "max_output_tokens": 10,
            },
        }
        try:
            with self._send_lock:
                connection.send(payload)
            self._response_in_flight.set()
            log.info("[%s] AMD: classification request sent", session.phone_id)
        except Exception as exc:
            log.warning("[%s] AMD: failed to send classification request: %s", session.phone_id, exc)
            session.amd_phase = False
            return "human"

        # Wait up to 4s for _receive_loop to set the result
        wait_deadline = time.monotonic() + 4.0
        while time.monotonic() < wait_deadline:
            if session.amd_event.wait(timeout=0.1):
                break
            if self._internal_stop.is_set():
                session.amd_phase = False
                return "stopped"

        if session.amd_event.is_set():
            result = (session.amd_result or "human").lower()
        else:
            log.warning("[%s] AMD: classification timed out — defaulting to human", session.phone_id)
            result = "human"

        session.amd_phase = False
        log.info("[%s] AMD final result: %s (%.2fs total)", session.phone_id, result, time.time() - t0)
        return result

    def _configure_session(self, connection, session: RealtimeCallSession) -> None:
        payload = {
            "type": "session.update",
            "session": {
                "type": "realtime",
                "instructions": session.instructions,
                "max_output_tokens": self.max_output_tokens,
                "output_modalities": ["audio"],
                "audio": {
                    "input": {
                        "format": {"type": "audio/pcm", "rate": REALTIME_PCM_RATE},
                        "noise_reduction": {"type": "near_field"},
                        "transcription": {
                            "model": self.transcription_model,
                            "language": "ko",
                        },
                        "turn_detection": {
                            "type": "server_vad",
                            "create_response": True,
                            "interrupt_response": True,
                            "prefix_padding_ms": 300,
                            "silence_duration_ms": 500,
                            "idle_timeout_ms": self.silence_timeout * 1000,
                        },
                    },
                    "output": {
                        "format": {"type": "audio/pcm", "rate": 24000},
                        "voice": self.voice,
                    },
                },
            },
        }
        with self._send_lock:
            connection.send(payload)

    def _send_initial_greeting(self, connection, session: RealtimeCallSession) -> None:
        if not session.opening_message:
            session.ignore_next_assistant_transcript = False
            return

        payload = {
            "type": "response.create",
            "response": {
                "instructions": f"Start the call now. Say exactly: {session.opening_message}",
                "output_modalities": ["audio"],
                "max_output_tokens": 80,
            },
        }
        with self._send_lock:
            connection.send(payload)
        self._response_in_flight.set()

    def _capture_loop(self, connection, bridge: AudioBridge, session: RealtimeCallSession, stop_event: threading.Event) -> None:
        while not stop_event.is_set() and not self._internal_stop.is_set():
            chunk = bridge.read_capture_chunk()
            if not chunk:
                time.sleep(0.01)
                continue

            pcm24 = self._upsample_to_24k(chunk)
            self._capture_bytes_total += len(pcm24)
            b64 = base64.b64encode(pcm24).decode("ascii")
            try:
                with self._send_lock:
                    connection.input_audio_buffer.append(audio=b64)
            except Exception as exc:
                if not self._internal_stop.is_set():
                    self._fatal_error = exc
                    self._internal_stop.set()
                return

    def _receive_loop(self, connection, session: RealtimeCallSession) -> None:
        try:
            for event in connection:
                if self._internal_stop.is_set():
                    return

                event_type = getattr(event, "type", "")
                if event_type not in ("response.output_audio.delta", "input_audio_buffer.speech_started", "input_audio_buffer.speech_stopped", "input_audio_buffer.committed"):
                    log.info("[%s] RT event: %s", session.phone_id, event_type)

                if event_type == "response.created":
                    self._response_in_flight.set()
                    continue

                if event_type == "response.done":
                    self._response_in_flight.clear()
                    session.last_activity_at = time.time()
                    continue

                if event_type == "response.output_audio.delta":
                    delta = getattr(event, "delta", None)
                    if delta and not session.amd_phase:  # suppress audio during AMD
                        pcm = base64.b64decode(delta)
                        self._playback_queue.put(pcm)
                        session.last_activity_at = time.time()
                        if not hasattr(self, '_delta_count'):
                            self._delta_count = 0
                        self._delta_count += 1
                        if self._delta_count <= 3 or self._delta_count % 50 == 0:
                            log.info("[%s] audio delta #%d: %d bytes, queue=%d",
                                     session.phone_id, self._delta_count, len(pcm),
                                     self._playback_queue.qsize())
                    continue

                # text-only modality response (AMD classification)
                if event_type == "response.text.done":
                    text = (getattr(event, "text", "") or "").strip()
                    if session.amd_phase and text:
                        upper = text.upper()
                        session.amd_result = "voicemail" if "VOICEMAIL" in upper else "human"
                        session.amd_event.set()
                        log.info("[%s] AMD text.done: %r → %s", session.phone_id, text, session.amd_result)
                    continue

                if event_type == "response.output_audio_transcript.done":
                    transcript = (getattr(event, "transcript", "") or "").strip()
                    if session.amd_phase and transcript:
                        # Fallback: audio transcript used if text-only modality not supported
                        upper = transcript.upper()
                        session.amd_result = "voicemail" if "VOICEMAIL" in upper else "human"
                        session.amd_event.set()
                        log.info("[%s] AMD audio-transcript fallback: %r → %s", session.phone_id, transcript, session.amd_result)
                    elif transcript:
                        session.log_lines.append(f"[AI] {transcript}")
                        if session.ignore_next_assistant_transcript:
                            session.ignore_next_assistant_transcript = False
                        else:
                            session.assistant_turns += 1
                        session.last_activity_at = time.time()
                        log.info("[%s] AI: %s", session.phone_id, transcript)
                    continue

                if event_type == "conversation.item.input_audio_transcription.completed":
                    transcript = (getattr(event, "transcript", "") or "").strip()
                    if transcript:
                        if session.amd_phase:
                            # Keyword backup: if voicemail keywords appear in incoming audio
                            lower = transcript.lower()
                            if any(kw in lower for kw in VOICEMAIL_KEYWORDS):
                                log.info("[%s] AMD keyword hit: %r", session.phone_id, transcript)
                                if not session.amd_event.is_set():
                                    session.amd_result = "voicemail"
                                    session.amd_event.set()
                        else:
                            session.user_turns += 1
                            session.log_lines.append(f"[USER] {transcript}")
                            session.last_activity_at = time.time()
                            log.info("[%s] USER: %s", session.phone_id, transcript)
                    continue

                if event_type == "input_audio_buffer.speech_started":
                    session.last_activity_at = time.time()
                    self._drain_playback_queue()
                    if self._current_bridge is not None:
                        self._current_bridge.set_playing(False)
                    if self._response_in_flight.is_set():
                        self._response_in_flight.clear()
                        try:
                            with self._send_lock:
                                connection.response.cancel()
                        except Exception:
                            pass
                    continue

                if event_type == "error":
                    error = getattr(event, "error", event)
                    error_code = getattr(error, "code", None)
                    error_message = str(error)
                    if session.amd_phase and error_code == "input_audio_buffer_commit_empty":
                        log.info(
                            "[%s] AMD benign empty-commit race; defaulting AMD to human",
                            session.phone_id,
                        )
                        session.amd_result = "human"
                        session.amd_event.set()
                        self._response_in_flight.clear()
                        continue
                    if error_code == "response_cancel_not_active" or "Cancellation failed: no active response found" in error_message:
                        log.info("[%s] Ignoring benign realtime cancel race: %s", session.phone_id, error_message)
                        self._response_in_flight.clear()
                        continue
                    self._fatal_error = RuntimeError(error_message)
                    self._internal_stop.set()
                    return

        except Exception as exc:
            if not self._internal_stop.is_set():
                self._fatal_error = exc
                self._internal_stop.set()

    def _playback_loop(self, bridge: AudioBridge) -> None:
        while not self._internal_stop.is_set() or not self._playback_queue.empty():
            try:
                pcm_bytes = self._playback_queue.get(timeout=0.1)
            except queue.Empty:
                bridge.set_playing(False)
                continue

            batch = bytearray(pcm_bytes)
            deadline = time.monotonic() + PLAYBACK_BATCH_MAX_WAIT_SEC
            while len(batch) < PLAYBACK_BATCH_TARGET_BYTES:
                timeout = deadline - time.monotonic()
                if timeout <= 0:
                    break
                try:
                    batch.extend(self._playback_queue.get(timeout=timeout))
                except queue.Empty:
                    break

            bridge.set_playing(True)
            if not hasattr(self, '_play_count'):
                self._play_count = 0
            self._play_count += 1
            if self._play_count <= 3 or self._play_count % 20 == 0:
                log.info("[%s] playback #%d: %d bytes", getattr(self, '_preconnected_phone_id', '?'), self._play_count, len(batch))
            bridge.play_audio_chunk(bytes(batch))

        bridge.set_playing(False)

    def _drain_playback_queue(self) -> None:
        while not self._playback_queue.empty():
            try:
                self._playback_queue.get_nowait()
            except queue.Empty:
                return

    def _wait_for_playback_drain(self) -> None:
        deadline = time.monotonic() + PLAYBACK_DRAIN_TIMEOUT_SEC
        while time.monotonic() < deadline:
            if self._playback_queue.empty() and not self._response_in_flight.is_set():
                time.sleep(PLAYBACK_DRAIN_TAIL_SEC)
                return
            time.sleep(0.02)

    @staticmethod
    def _flush_capture(bridge: AudioBridge, duration_sec: float) -> None:
        deadline = time.monotonic() + duration_sec
        while time.monotonic() < deadline:
            bridge.read_capture_chunk()

    def _close_connection(self) -> None:
        cm = self._connection_cm
        self._connection_cm = None
        if cm is None:
            return
        try:
            cm.__exit__(None, None, None)
        except Exception:
            pass

    def _prefetch_opening_pcm(self, session: RealtimeCallSession) -> None:
        try:
            key = (self.opening_tts_model, self.opening_tts_voice, session.opening_message)
            cached = _OPENING_PCM_CACHE.get(key)
            if cached is None:
                client = OpenAI(api_key=self.api_key)
                response = client.audio.speech.create(
                    model=self.opening_tts_model,
                    voice=self.opening_tts_voice,
                    input=session.opening_message,
                    response_format="pcm",
                )
                cached = response.content
                _OPENING_PCM_CACHE[key] = cached
            session.opening_pcm = cached
        except Exception as exc:
            log.warning("[%s] opening TTS prefetch failed: %s", session.phone_id, exc)
        finally:
            session.opening_pcm_ready.set()

    @staticmethod
    def _wait_for_opening_pcm(session: RealtimeCallSession, timeout: float) -> Optional[bytes]:
        if session.opening_pcm_ready.wait(timeout):
            return session.opening_pcm
        return None

    @staticmethod
    def _upsample_to_24k(pcm_bytes: bytes) -> bytes:
        samples = np.frombuffer(pcm_bytes, dtype=np.int16)
        if len(samples) == 0 or CAPTURE_RATE == REALTIME_PCM_RATE:
            return pcm_bytes

        duration = len(samples) / CAPTURE_RATE
        target_len = max(1, int(duration * REALTIME_PCM_RATE))
        source_x = np.linspace(0.0, 1.0, num=len(samples), endpoint=False)
        target_x = np.linspace(0.0, 1.0, num=target_len, endpoint=False)
        interpolated = np.interp(target_x, source_x, samples.astype(np.float32))
        return interpolated.astype(np.int16).tobytes()
