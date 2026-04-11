"""
Audio bridge for call capture/playback.

Capture:
- mono 8 kHz from the call downlink device

Playback:
- mono 24 kHz to the call uplink device

The bridge keeps a short playback guard window to avoid obvious self-echo,
but keeps the turn latency low enough for conversational use.
"""

import io
import logging
import threading
import time
import wave
from typing import Optional

import numpy as np
import pyaudio

log = logging.getLogger(__name__)

CAPTURE_RATE = 48000
PLAYBACK_RATE = 48000
CHANNELS = 1
FORMAT = pyaudio.paInt16
CHUNK = 512  # ~64 ms at 8 kHz

VAD_THRESHOLD = 500
SILENCE_CHUNKS = max(3, int(CAPTURE_RATE * 0.35 / CHUNK))
MIN_SPEECH_CHUNKS = max(2, int(CAPTURE_RATE * 0.2 / CHUNK))
SILENCE_TIMEOUT_SEC = 10


def _find_device_index(pa: pyaudio.PyAudio, name: str, input: bool) -> Optional[int]:
    if name.isdigit():
        idx = int(name)
        info = pa.get_device_info_by_index(idx)
        log.info("Using device index directly [%s] %s", idx, info["name"])
        return idx

    count = pa.get_device_count()
    for idx in range(count):
        info = pa.get_device_info_by_index(idx)
        if name.lower() not in info["name"].lower():
            continue
        if input and info["maxInputChannels"] > 0:
            log.info("Matched input device [%s] %s", idx, info["name"])
            return idx
        if not input and info["maxOutputChannels"] > 0:
            log.info("Matched output device [%s] %s", idx, info["name"])
            return idx

    log.warning("Device '%s' not found, falling back to PyAudio default", name)
    return None


def probe_capture_activity(
    capture_device: str,
    threshold: float = VAD_THRESHOLD,
    timeout_sec: float = 2.0,
    min_hits: int = 2,
    frames_per_buffer: int = CHUNK,
) -> bool:
    """
    Return True if the capture device shows real audio activity within timeout.

    This is used as a lightweight guard before promoting a Phone Link-only call
    to answered/in_call. It avoids obvious false positives where UI elements are
    visible but no call audio is actually flowing into the PC.
    """
    pa: Optional[pyaudio.PyAudio] = None
    stream = None
    hits = 0
    deadline = time.monotonic() + timeout_sec
    try:
        pa = pyaudio.PyAudio()
        device_index = _find_device_index(pa, capture_device, input=True)
        stream = pa.open(
            format=FORMAT,
            channels=CHANNELS,
            rate=CAPTURE_RATE,
            input=True,
            input_device_index=device_index,
            frames_per_buffer=frames_per_buffer,
        )
        while time.monotonic() < deadline:
            chunk = stream.read(frames_per_buffer, exception_on_overflow=False)
            rms = AudioBridge._rms(chunk)
            if rms > threshold:
                hits += 1
                if hits >= min_hits:
                    log.info(
                        "Capture activity detected on %s (rms=%.1f hits=%s)",
                        capture_device,
                        rms,
                        hits,
                    )
                    return True
            else:
                hits = 0
        log.info("No capture activity detected on %s within %.1fs", capture_device, timeout_sec)
        return False
    except Exception as exc:
        log.warning("Capture activity probe failed on %s: %s", capture_device, exc)
        return False
    finally:
        if stream is not None:
            try:
                stream.stop_stream()
                stream.close()
            except Exception:
                pass
        if pa is not None:
            try:
                pa.terminate()
            except Exception:
                pass


class AudioBridge:
    def __init__(
        self,
        capture_device: str,
        playback_device: str,
        monitor_device: Optional[str] = None,
    ):
        self.capture_device = capture_device
        self.playback_device = playback_device
        self.monitor_device = monitor_device
        self._pa: Optional[pyaudio.PyAudio] = None
        self._capture_stream = None
        self._playback_stream = None
        self._monitor_stream = None
        self._playing = threading.Event()
        self._playback_lock = threading.Lock()
        self._last_volume_set_at = 0.0
        self._last_volume_level: Optional[float] = None
        self._last_write_time = 0.0
        self._write_count = 0

    def open(self) -> None:
        self._pa = pyaudio.PyAudio()

        cap_idx = self._find_device(self.capture_device, input=True)
        play_idx = self._find_device(self.playback_device, input=False)

        self._capture_stream = self._pa.open(
            format=FORMAT,
            channels=CHANNELS,
            rate=CAPTURE_RATE,
            input=True,
            input_device_index=cap_idx,
            frames_per_buffer=CHUNK,
        )
        self._playback_stream = self._pa.open(
            format=FORMAT,
            channels=CHANNELS,
            rate=PLAYBACK_RATE,
            output=True,
            output_device_index=play_idx,
        )

        if self.monitor_device:
            try:
                mon_idx = self._find_device(self.monitor_device, input=False)
                self._monitor_stream = self._pa.open(
                    format=FORMAT,
                    channels=CHANNELS,
                    rate=PLAYBACK_RATE,
                    output=True,
                    output_device_index=mon_idx,
                )
                log.info(f"Monitor stream opened: {self.monitor_device}")
            except Exception as exc:
                log.warning(f"Monitor device open failed, ignored: {exc}")
                self._monitor_stream = None

        log.info(
            "Audio bridge opened: capture=%s playback=%s",
            self.capture_device,
            self.playback_device,
        )

        self._set_app_volume(1.0, force=True)

    def close(self) -> None:
        if self._capture_stream:
            self._capture_stream.stop_stream()
            self._capture_stream.close()
        if self._playback_stream:
            self._playback_stream.stop_stream()
            self._playback_stream.close()
        if self._monitor_stream:
            self._monitor_stream.stop_stream()
            self._monitor_stream.close()
        if self._pa:
            self._pa.terminate()
        log.info("Audio bridge closed")

    def capture_utterance(
        self,
        stop_event: Optional[threading.Event] = None,
        timeout_sec: int = SILENCE_TIMEOUT_SEC,
    ) -> Optional[bytes]:
        """
        Capture one utterance using a simple VAD.

        Returns raw PCM bytes, or None on silence timeout / stop.
        """
        speech_buf: list[bytes] = []
        silence_count = 0
        in_speech = False
        no_speech_chunks = 0
        timeout_chunks = int(CAPTURE_RATE / CHUNK * timeout_sec)

        while True:
            if stop_event and stop_event.is_set():
                return None

            if self._playing.is_set():
                time.sleep(0.03)
                no_speech_chunks = 0
                continue

            try:
                chunk = self._capture_stream.read(CHUNK, exception_on_overflow=False)
            except Exception as exc:
                log.warning(f"Capture error: {exc}")
                break

            rms = self._rms(chunk)

            if rms > VAD_THRESHOLD:
                in_speech = True
                silence_count = 0
                no_speech_chunks = 0
                speech_buf.append(chunk)
            else:
                if in_speech:
                    silence_count += 1
                    speech_buf.append(chunk)
                    if silence_count >= SILENCE_CHUNKS:
                        if len(speech_buf) >= MIN_SPEECH_CHUNKS:
                            log.debug("Utterance captured: %s chunks", len(speech_buf))
                            return b"".join(speech_buf)

                        speech_buf.clear()
                        in_speech = False
                        silence_count = 0
                else:
                    no_speech_chunks += 1
                    if no_speech_chunks >= timeout_chunks:
                        log.info("Silence timeout after %ss", timeout_sec)
                        return None

        return None

    def play_tts(self, pcm_bytes: bytes) -> None:
        """
        Play TTS PCM to the uplink device and optionally mirror to a monitor.
        """
        self.set_playing(True)
        try:
            self._set_app_volume(1.0, force=True)
            self.play_audio_chunk(pcm_bytes)
            time.sleep(0.15)
        finally:
            self.set_playing(False)

    def set_playing(self, active: bool) -> None:
        if active:
            self._playing.set()
        else:
            self._playing.clear()

    def read_capture_chunk(self, frames_per_buffer: int = CHUNK) -> Optional[bytes]:
        """
        Read one raw PCM chunk from the capture device for streaming paths.
        """
        if self._capture_stream is None:
            return None

        try:
            return self._capture_stream.read(frames_per_buffer, exception_on_overflow=False)
        except Exception as exc:
            log.warning("Streaming capture error: %s", exc)
            return None

    def play_audio_chunk(self, pcm_bytes: bytes, source_rate: int = 24000) -> None:
        """
        Play one PCM chunk, resampling from source_rate to PLAYBACK_RATE if needed.
        """
        if not pcm_bytes or self._playback_stream is None:
            return

        if source_rate != PLAYBACK_RATE:
            samples = np.frombuffer(pcm_bytes, dtype=np.int16)
            if len(samples) > 0:
                duration = len(samples) / source_rate
                target_len = max(1, int(duration * PLAYBACK_RATE))
                src_x = np.linspace(0.0, 1.0, num=len(samples), endpoint=False)
                tgt_x = np.linspace(0.0, 1.0, num=target_len, endpoint=False)
                pcm_bytes = np.interp(tgt_x, src_x, samples.astype(np.float32)).astype(np.int16).tobytes()

        with self._playback_lock:
            try:
                now = time.monotonic()
                gap_ms = (now - self._last_write_time) * 1000 if self._last_write_time > 0 else 0
                self._last_write_time = now
                chunk_frames = len(pcm_bytes) // (2 * CHANNELS)  # 16-bit samples
                self._write_count += 1
                if self._write_count <= 5 or self._write_count % 20 == 0 or gap_ms > 50:
                    log.info("[cadence] write #%d: %d frames (%.1fms data) gap=%.1fms",
                             self._write_count, chunk_frames, chunk_frames * 1000 / PLAYBACK_RATE, gap_ms)
                self._playback_stream.write(pcm_bytes)
                if self._monitor_stream:
                    self._monitor_stream.write(pcm_bytes)
            except Exception as exc:
                log.warning("Streaming playback error: %s", exc)

    @staticmethod
    def pcm_to_wav(pcm_bytes: bytes, sample_rate: int = CAPTURE_RATE) -> bytes:
        buf = io.BytesIO()
        with wave.open(buf, "wb") as wf:
            wf.setnchannels(CHANNELS)
            wf.setsampwidth(2)
            wf.setframerate(sample_rate)
            wf.writeframes(pcm_bytes)
        return buf.getvalue()

    @staticmethod
    def _rms(chunk: bytes) -> float:
        arr = np.frombuffer(chunk, dtype=np.int16).astype(np.float32)
        return float(np.sqrt(np.mean(arr**2))) if len(arr) > 0 else 0.0

    @staticmethod
    def _set_app_volume(self, level: float = 1.0, force: bool = False) -> None:
        """Set the current process volume in the Windows volume mixer."""
        now = time.monotonic()
        if not force:
            if self._last_volume_level == level and now - self._last_volume_set_at < 3.0:
                return

        try:
            from pycaw.pycaw import AudioUtilities, ISimpleAudioVolume
            import os

            pid = os.getpid()
            sessions = AudioUtilities.GetAllSessions()
            for session in sessions:
                if session.Process and session.Process.pid == pid:
                    vol = session._ctl.QueryInterface(ISimpleAudioVolume)
                    vol.SetMasterVolume(level, None)
                    vol.SetMute(0, None)
                    self._last_volume_level = level
                    self._last_volume_set_at = now
                    log.info("Python app volume set: %.2f", level)
                    return
            log.debug("Python audio session not found in mixer")
        except Exception as exc:
            log.debug(f"Python app volume set failed: {exc}")

    def _find_device(self, name: str, input: bool) -> Optional[int]:
        if self._pa is None:
            return None
        return _find_device_index(self._pa, name, input=input)
