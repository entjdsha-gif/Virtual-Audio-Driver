"""
A/B comparison test: play identical PCM through AO Cable and VB-Cable sequentially.
Isolates driver quality difference from app write pattern differences.

Usage:
    python test_ab_compare.py              # generates 3s test tone
    python test_ab_compare.py --wav file.wav  # use existing wav file
"""

import argparse
import logging
import time
import numpy as np
import pyaudio

log = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")

RATE = 48000
CHANNELS = 2
FORMAT = pyaudio.paInt16
CHUNK = 960  # 20ms

PROFILES = {
    "ao": "AO Cable B",
    "vb": "CABLE-B Input",
}


def generate_test_tone(duration_sec=3.0, freq=440):
    """Generate a clean sine tone at 48kHz/16bit/stereo."""
    t = np.linspace(0, duration_sec, int(RATE * duration_sec), endpoint=False)
    tone = (np.sin(2 * np.pi * freq * t) * 16000).astype(np.int16)
    # Stereo: duplicate to both channels
    stereo = np.column_stack([tone, tone]).flatten()
    return stereo.tobytes()


def find_device(pa, name, output=True):
    for i in range(pa.get_device_count()):
        info = pa.get_device_info_by_index(i)
        if name.lower() in info["name"].lower():
            if output and info["maxOutputChannels"] > 0:
                return i
            if not output and info["maxInputChannels"] > 0:
                return i
    return None


def play_pcm(pa, device_name, pcm_bytes):
    """Play PCM data through a specific device, logging write cadence."""
    idx = find_device(pa, device_name, output=True)
    if idx is None:
        log.error("Device not found: %s", device_name)
        return

    stream = pa.open(
        format=FORMAT,
        channels=CHANNELS,
        rate=RATE,
        output=True,
        output_device_index=idx,
        frames_per_buffer=CHUNK,
    )

    log.info("Playing %d bytes through %s (device %d)", len(pcm_bytes), device_name, idx)

    offset = 0
    frame_size = CHANNELS * 2  # 16-bit stereo = 4 bytes/frame
    chunk_bytes = CHUNK * frame_size
    write_count = 0
    start = time.monotonic()

    while offset < len(pcm_bytes):
        end = min(offset + chunk_bytes, len(pcm_bytes))
        chunk = pcm_bytes[offset:end]
        # Pad last chunk if needed
        if len(chunk) < chunk_bytes:
            chunk += b'\x00' * (chunk_bytes - len(chunk))
        stream.write(chunk)
        write_count += 1
        offset = end

    elapsed = time.monotonic() - start
    log.info("Done: %d writes in %.2fs (avg %.1fms/write)", write_count, elapsed, elapsed * 1000 / max(write_count, 1))

    stream.stop_stream()
    stream.close()


def main():
    parser = argparse.ArgumentParser(description="A/B cable comparison")
    parser.add_argument("--wav", help="WAV file to play (default: generate tone)")
    parser.add_argument("--freq", type=int, default=440, help="Tone frequency Hz")
    parser.add_argument("--duration", type=float, default=3.0, help="Tone duration sec")
    args = parser.parse_args()

    if args.wav:
        import wave
        with wave.open(args.wav, 'rb') as wf:
            pcm_bytes = wf.readframes(wf.getnframes())
            log.info("Loaded %s: %d frames", args.wav, wf.getnframes())
    else:
        pcm_bytes = generate_test_tone(args.duration, args.freq)
        log.info("Generated %dHz tone: %.1fs, %d bytes", args.freq, args.duration, len(pcm_bytes))

    pa = pyaudio.PyAudio()

    for profile_name, device_name in PROFILES.items():
        log.info("=== %s: %s ===", profile_name.upper(), device_name)
        play_pcm(pa, device_name, pcm_bytes)
        log.info("Listen and rate quality. Pausing 2s before next...\n")
        time.sleep(2.0)

    pa.terminate()
    log.info("A/B test complete. Compare AO vs VB quality.")


if __name__ == "__main__":
    main()
