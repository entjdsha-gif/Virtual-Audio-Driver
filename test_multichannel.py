"""Test multichannel (5.1/7.1) support on AO Cable A."""
import pyaudio
import struct
import math

pa = pyaudio.PyAudio()

# Find AO Cable A
render_idx = None
capture_idx = None
for i in range(pa.get_device_count()):
    info = pa.get_device_info_by_index(i)
    if 'ao cable a' in info['name'].lower():
        if info['maxOutputChannels'] > 0 and render_idx is None:
            render_idx = i
            print(f"Render: [{i}] {info['name']} (maxOut={info['maxOutputChannels']})")
        if info['maxInputChannels'] > 0 and capture_idx is None:
            capture_idx = i
            print(f"Capture: [{i}] {info['name']} (maxIn={info['maxInputChannels']})")

if render_idx is None:
    print("AO Cable A not found")
    exit(1)

# Test channel counts
for channels in [6, 8]:
    label = "5.1" if channels == 6 else "7.1"

    # Render test
    try:
        stream = pa.open(format=pyaudio.paInt16, channels=channels, rate=48000,
                         output=True, output_device_index=render_idx,
                         frames_per_buffer=1024)
        # Generate 100ms of audio (440Hz on channel 0, silence on rest)
        samples = b''
        for i in range(4800):
            val = int(8000 * math.sin(2 * math.pi * 440 * i / 48000))
            frame = [val] + [0] * (channels - 1)
            samples += struct.pack(f'<{channels}h', *frame)
        stream.write(samples)
        stream.close()
        print(f"[PASS] {label} ({channels}ch) render")
    except Exception as e:
        print(f"[FAIL] {label} ({channels}ch) render: {e}")

    # Capture test
    if capture_idx is not None:
        try:
            stream = pa.open(format=pyaudio.paInt16, channels=channels, rate=48000,
                             input=True, input_device_index=capture_idx,
                             frames_per_buffer=1024)
            data = stream.read(1024, exception_on_overflow=False)
            stream.close()
            print(f"[PASS] {label} ({channels}ch) capture ({len(data)} bytes)")
        except Exception as e:
            print(f"[FAIL] {label} ({channels}ch) capture: {e}")

pa.terminate()
