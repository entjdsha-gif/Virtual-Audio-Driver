"""
AO Virtual Cable - 48 Format Exclusive Mode Test
Tests all supported format combinations on Cable A Speaker/Mic.

Requirements:
    pip install pyaudio numpy

Usage:
    python test_formats.py                  # Test all 48 formats
    python test_formats.py --quick          # Test 6 key formats only
    python test_formats.py --cable B        # Test Cable B instead of A
    python test_formats.py --render-only    # Only test render (speaker)
    python test_formats.py --capture-only   # Only test capture (mic)
"""

import pyaudio
import numpy as np
import sys
import time
import argparse
import struct

# All supported formats: 12 rates x 2 depths x 2 channels = 48
RATES = [8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000, 88200, 96000, 176400, 192000]
DEPTHS = [16, 24]
CHANNELS = [1, 2]

# Quick test: key formats only
QUICK_FORMATS = [
    (44100, 16, 2),
    (48000, 16, 2),
    (48000, 24, 2),
    (96000, 24, 2),
    (44100, 16, 1),
    (192000, 24, 2),
]

PA_FORMAT_MAP = {
    16: pyaudio.paInt16,
    24: pyaudio.paInt24,
}


def find_device(pa, name_fragment, is_input):
    """Find audio device index by name fragment."""
    for i in range(pa.get_device_count()):
        info = pa.get_device_info_by_index(i)
        if name_fragment.lower() in info['name'].lower():
            if is_input and info['maxInputChannels'] > 0:
                return i, info['name']
            if not is_input and info['maxOutputChannels'] > 0:
                return i, info['name']
    return None, None


def generate_sine(rate, bits, channels, freq=1000, duration_ms=100):
    """Generate a sine wave test signal."""
    n_samples = int(rate * duration_ms / 1000)
    t = np.arange(n_samples) / rate
    sine = np.sin(2 * np.pi * freq * t)

    if bits == 16:
        amplitude = 16000  # ~50% of INT16 max
        samples = (sine * amplitude).astype(np.int16)
        if channels == 2:
            stereo = np.column_stack([samples, samples])
            return stereo.tobytes()
        return samples.tobytes()
    else:  # 24-bit
        amplitude = 4000000  # ~50% of 24-bit max
        samples_32 = (sine * amplitude).astype(np.int32)
        buf = bytearray()
        for s in samples_32:
            for _ in range(channels):
                # Pack as 3 bytes little-endian
                val = int(s) & 0xFFFFFF
                buf.append(val & 0xFF)
                buf.append((val >> 8) & 0xFF)
                buf.append((val >> 16) & 0xFF)
        return bytes(buf)


def test_render(pa, device_idx, rate, bits, channels, duration_ms=200):
    """Test exclusive mode render (speaker output)."""
    try:
        fmt = PA_FORMAT_MAP[bits]
        stream = pa.open(
            format=fmt,
            channels=channels,
            rate=rate,
            output=True,
            output_device_index=device_idx,
            frames_per_buffer=int(rate * 0.01),  # 10ms buffer
        )

        data = generate_sine(rate, bits, channels, duration_ms=duration_ms)
        stream.write(data)
        time.sleep(0.05)

        stream.stop_stream()
        stream.close()
        return True, None
    except Exception as e:
        return False, str(e)


def test_capture(pa, device_idx, rate, bits, channels, duration_ms=200):
    """Test exclusive mode capture (mic input)."""
    try:
        fmt = PA_FORMAT_MAP[bits]
        frames = int(rate * duration_ms / 1000)
        stream = pa.open(
            format=fmt,
            channels=channels,
            rate=rate,
            input=True,
            input_device_index=device_idx,
            frames_per_buffer=int(rate * 0.01),  # 10ms buffer
        )

        data = stream.read(frames, exception_on_overflow=False)
        stream.stop_stream()
        stream.close()

        # Verify we got data of expected size
        bytes_per_sample = bits // 8
        expected = frames * channels * bytes_per_sample
        if len(data) < expected:
            return False, f"Short read: got {len(data)} bytes, expected {expected}"

        return True, None
    except Exception as e:
        return False, str(e)


def test_loopback(pa, render_idx, capture_idx, rate, bits, channels, duration_ms=300):
    """Test render->capture loopback: play sine on speaker, record on mic, verify non-silence."""
    try:
        fmt = PA_FORMAT_MAP[bits]
        frames_per_buf = max(int(rate * 0.01), 256)

        # Open capture first
        cap_stream = pa.open(
            format=fmt,
            channels=channels,
            rate=rate,
            input=True,
            input_device_index=capture_idx,
            frames_per_buffer=frames_per_buf,
        )

        # Open render
        rnd_stream = pa.open(
            format=fmt,
            channels=channels,
            rate=rate,
            output=True,
            output_device_index=render_idx,
            frames_per_buffer=frames_per_buf,
        )

        # Play sine
        data = generate_sine(rate, bits, channels, freq=1000, duration_ms=duration_ms)
        rnd_stream.write(data)

        # Small delay for loopback propagation
        time.sleep(0.05)

        # Capture
        cap_frames = int(rate * duration_ms / 1000)
        captured = cap_stream.read(cap_frames, exception_on_overflow=False)

        rnd_stream.stop_stream()
        rnd_stream.close()
        cap_stream.stop_stream()
        cap_stream.close()

        # Check captured data is not all zeros (silence)
        if bits == 16:
            samples = np.frombuffer(captured, dtype=np.int16)
        else:
            # Decode 24-bit
            raw = np.frombuffer(captured, dtype=np.uint8)
            n_samples_total = len(raw) // 3
            samples = np.zeros(n_samples_total, dtype=np.int32)
            for i in range(n_samples_total):
                b0 = int(raw[i * 3])
                b1 = int(raw[i * 3 + 1])
                b2 = int(raw[i * 3 + 2])
                val = b0 | (b1 << 8) | (b2 << 16)
                if val & 0x800000:
                    val -= 0x1000000
                samples[i] = val

        peak = np.max(np.abs(samples))
        if peak < 10:
            return False, f"Silence detected (peak={peak})"

        return True, f"peak={peak}"
    except Exception as e:
        return False, str(e)


def main():
    parser = argparse.ArgumentParser(description='AO Virtual Cable Format Test')
    parser.add_argument('--quick', action='store_true', help='Test 6 key formats only')
    parser.add_argument('--cable', default='A', choices=['A', 'B'], help='Cable to test (default: A)')
    parser.add_argument('--render-only', action='store_true', help='Only test render')
    parser.add_argument('--capture-only', action='store_true', help='Only test capture')
    parser.add_argument('--loopback', action='store_true', help='Also test render->capture loopback')
    args = parser.parse_args()

    pa = pyaudio.PyAudio()

    # Find Cable devices
    cable_name = f"Cable {args.cable}"
    render_idx, render_name = find_device(pa, cable_name, is_input=False)
    capture_idx, capture_name = find_device(pa, cable_name, is_input=True)

    if render_idx is None and not args.capture_only:
        # Try alternative names
        render_idx, render_name = find_device(pa, f"AO Cable {args.cable} Output", is_input=False)
    if capture_idx is None and not args.render_only:
        capture_idx, capture_name = find_device(pa, f"AO Cable {args.cable} Input", is_input=True)

    print(f"=== AO Virtual Cable Format Test (Cable {args.cable}) ===")
    print()

    if render_idx is not None:
        print(f"  Render device [{render_idx}]: {render_name}")
    else:
        print("  Render device: NOT FOUND")
        if not args.capture_only:
            print("  Install the driver first or check device name.")

    if capture_idx is not None:
        print(f"  Capture device [{capture_idx}]: {capture_name}")
    else:
        print("  Capture device: NOT FOUND")
        if not args.render_only:
            print("  Install the driver first or check device name.")

    print()

    # Build format list
    if args.quick:
        formats = QUICK_FORMATS
    else:
        formats = [(r, b, c) for r in RATES for b in DEPTHS for c in CHANNELS]

    total = len(formats)
    passed = 0
    failed = 0
    skipped = 0
    failures = []

    print(f"Testing {total} format(s)...")
    print("-" * 80)
    print(f"{'#':>3}  {'Rate':>6}  {'Bits':>4}  {'Ch':>2}  {'Render':>10}  {'Capture':>10}  {'Loopback':>10}")
    print("-" * 80)

    for i, (rate, bits, ch) in enumerate(formats):
        render_result = "-"
        capture_result = "-"
        loopback_result = "-"
        fmt_pass = True

        # Test render
        if render_idx is not None and not args.capture_only:
            ok, err = test_render(pa, render_idx, rate, bits, ch)
            if ok:
                render_result = "PASS"
            else:
                render_result = "FAIL"
                fmt_pass = False
                failures.append(f"Render {rate}/{bits}/{ch}ch: {err}")

        # Test capture
        if capture_idx is not None and not args.render_only:
            ok, err = test_capture(pa, capture_idx, rate, bits, ch)
            if ok:
                capture_result = "PASS"
            else:
                capture_result = "FAIL"
                fmt_pass = False
                failures.append(f"Capture {rate}/{bits}/{ch}ch: {err}")

        # Test loopback (only if both available and --loopback flag)
        if args.loopback and render_idx is not None and capture_idx is not None:
            ok, info = test_loopback(pa, render_idx, capture_idx, rate, bits, ch)
            if ok:
                loopback_result = "PASS"
            else:
                loopback_result = "FAIL"
                fmt_pass = False
                failures.append(f"Loopback {rate}/{bits}/{ch}ch: {info}")

        if fmt_pass:
            passed += 1
        else:
            failed += 1

        ch_str = "mono" if ch == 1 else "stereo"
        print(f"{i+1:>3}  {rate:>6}  {bits:>4}  {ch_str:>6}  {render_result:>10}  {capture_result:>10}  {loopback_result:>10}")

        # Small delay between tests to let driver settle
        time.sleep(0.05)

    print("-" * 80)
    print()
    print(f"Results: {passed} passed, {failed} failed, {skipped} skipped / {total} total")

    if failures:
        print()
        print("Failures:")
        for f in failures:
            print(f"  - {f}")

    pa.terminate()

    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
