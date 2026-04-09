"""
AO Virtual Cable - 108 Format Exclusive Mode Test
Tests all supported format combinations on Cable A Speaker/Mic.
Includes PCM 16/24-bit, IEEE Float 32-bit, and multichannel 5.1/7.1.

Requirements:
    pip install pyaudio numpy

Usage:
    python test_formats.py                  # Test all 108 formats
    python test_formats.py --quick          # Test key formats only
    python test_formats.py --pcm-only       # Test 48 PCM formats only
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

# PCM formats: 12 rates x 2 depths x 2 channels = 48
RATES = [8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000, 88200, 96000, 176400, 192000]
DEPTHS = [16, 24]
CHANNELS = [1, 2]

# Float formats: 12 rates x 2 channels = 24
FLOAT_CHANNELS = [1, 2]

# Multichannel formats: 6 rates x 2 depths x 2 layouts (5.1 + 7.1) = 24 PCM
# + 6 rates x 2 layouts = 12 Float
MC_RATES = [44100, 48000, 88200, 96000, 176400, 192000]
MC_CHANNELS = [6, 8]  # 5.1 and 7.1

# Quick test: key formats across all types
QUICK_FORMATS = [
    (44100, 16, 2, 'pcm'),
    (48000, 16, 2, 'pcm'),
    (48000, 24, 2, 'pcm'),
    (96000, 24, 2, 'pcm'),
    (44100, 16, 1, 'pcm'),
    (192000, 24, 2, 'pcm'),
    (48000, 32, 2, 'float'),
    (44100, 32, 1, 'float'),
    (48000, 16, 6, 'pcm'),
    (48000, 16, 8, 'pcm'),
    (48000, 32, 6, 'float'),
    (48000, 32, 8, 'float'),
]

PA_FORMAT_MAP = {
    16: pyaudio.paInt16,
    24: pyaudio.paInt24,
    32: pyaudio.paFloat32,
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


def generate_sine(rate, bits, channels, freq=1000, duration_ms=100, fmt_type='pcm'):
    """Generate a sine wave test signal."""
    n_samples = int(rate * duration_ms / 1000)
    t = np.arange(n_samples) / rate
    sine = np.sin(2 * np.pi * freq * t)

    if fmt_type == 'float' or bits == 32:
        # 32-bit IEEE float
        samples = (sine * 0.5).astype(np.float32)  # 50% amplitude
        if channels > 1:
            multi = np.column_stack([samples] + [np.zeros_like(samples)] * (channels - 1))
            return multi.tobytes()
        return samples.tobytes()
    elif bits == 16:
        amplitude = 16000
        samples = (sine * amplitude).astype(np.int16)
        if channels > 1:
            multi = np.column_stack([samples] + [np.zeros_like(samples)] * (channels - 1))
            return multi.tobytes()
        return samples.tobytes()
    else:  # 24-bit
        amplitude = 4000000
        samples_32 = (sine * amplitude).astype(np.int32)
        buf = bytearray()
        for s in samples_32:
            for ch in range(channels):
                val = int(s) & 0xFFFFFF if ch == 0 else 0
                buf.append(val & 0xFF)
                buf.append((val >> 8) & 0xFF)
                buf.append((val >> 16) & 0xFF)
        return bytes(buf)


def test_render(pa, device_idx, rate, bits, channels, duration_ms=200, fmt_type='pcm'):
    """Test exclusive mode render (speaker output)."""
    try:
        fmt = PA_FORMAT_MAP[bits]
        stream = pa.open(
            format=fmt,
            channels=channels,
            rate=rate,
            output=True,
            output_device_index=device_idx,
            frames_per_buffer=max(int(rate * 0.01), 256),
        )

        data = generate_sine(rate, bits, channels, duration_ms=duration_ms, fmt_type=fmt_type)
        stream.write(data)
        time.sleep(0.05)

        stream.stop_stream()
        stream.close()
        return True, None
    except Exception as e:
        return False, str(e)


def test_capture(pa, device_idx, rate, bits, channels, duration_ms=200, fmt_type='pcm'):
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
            frames_per_buffer=max(int(rate * 0.01), 256),
        )

        data = stream.read(frames, exception_on_overflow=False)
        stream.stop_stream()
        stream.close()

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
    parser.add_argument('--quick', action='store_true', help='Test key formats only')
    parser.add_argument('--pcm-only', action='store_true', help='Test 48 PCM formats only')
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

    # Build format list: (rate, bits, channels, fmt_type)
    if args.quick:
        formats = QUICK_FORMATS
    elif args.pcm_only:
        formats = [(r, b, c, 'pcm') for r in RATES for b in DEPTHS for c in CHANNELS]
    else:
        # All 108 formats
        formats = []
        # PCM stereo/mono: 12 rates x 2 depths x 2 ch = 48
        for r in RATES:
            for b in DEPTHS:
                for c in CHANNELS:
                    formats.append((r, b, c, 'pcm'))
        # Float stereo/mono: 12 rates x 2 ch = 24
        for r in RATES:
            for c in FLOAT_CHANNELS:
                formats.append((r, 32, c, 'float'))
        # 5.1/7.1 PCM: 6 rates x 2 depths x 2 layouts = 24
        for r in MC_RATES:
            for b in DEPTHS:
                for c in MC_CHANNELS:
                    formats.append((r, b, c, 'pcm'))
        # 5.1/7.1 Float: 6 rates x 2 layouts = 12
        for r in MC_RATES:
            for c in MC_CHANNELS:
                formats.append((r, 32, c, 'float'))

    total = len(formats)
    passed = 0
    failed = 0
    skipped = 0
    failures = []

    print(f"Testing {total} format(s)...")
    print("-" * 90)
    print(f"{'#':>3}  {'Rate':>6}  {'Bits':>4}  {'Ch':>6}  {'Type':>5}  {'Render':>10}  {'Capture':>10}  {'Loopback':>10}")
    print("-" * 90)

    for i, (rate, bits, ch, fmt_type) in enumerate(formats):
        render_result = "-"
        capture_result = "-"
        loopback_result = "-"
        fmt_pass = True

        # Test render
        if render_idx is not None and not args.capture_only:
            ok, err = test_render(pa, render_idx, rate, bits, ch, fmt_type=fmt_type)
            if ok:
                render_result = "PASS"
            else:
                render_result = "FAIL"
                fmt_pass = False
                failures.append(f"Render {rate}/{bits}/{ch}ch/{fmt_type}: {err}")

        # Test capture
        if capture_idx is not None and not args.render_only:
            ok, err = test_capture(pa, capture_idx, rate, bits, ch, fmt_type=fmt_type)
            if ok:
                capture_result = "PASS"
            else:
                capture_result = "FAIL"
                fmt_pass = False
                failures.append(f"Capture {rate}/{bits}/{ch}ch/{fmt_type}: {err}")

        # Test loopback (only if both available and --loopback flag)
        if args.loopback and render_idx is not None and capture_idx is not None:
            ok, info = test_loopback(pa, render_idx, capture_idx, rate, bits, ch)
            if ok:
                loopback_result = "PASS"
            else:
                loopback_result = "FAIL"
                fmt_pass = False
                failures.append(f"Loopback {rate}/{bits}/{ch}ch/{fmt_type}: {info}")

        if fmt_pass:
            passed += 1
        else:
            failed += 1

        ch_names = {1: 'mono', 2: 'stereo', 6: '5.1', 8: '7.1'}
        ch_str = ch_names.get(ch, f'{ch}ch')
        print(f"{i+1:>3}  {rate:>6}  {bits:>4}  {ch_str:>6}  {fmt_type:>5}  {render_result:>10}  {capture_result:>10}  {loopback_result:>10}")

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
