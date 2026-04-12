"""
Direct loopback test: play tone to AO Cable B Speaker, record from AO Cable B Mic.
No Phone Link, no Realtime API. Pure pipe test.
Compare with VB-Cable A for reference.
"""
import sounddevice as sd
import numpy as np
import time
import sys

SAMPLE_RATE = 48000
DURATION = 3.0  # seconds
FREQ = 440  # Hz

def find_device(name_part, is_input):
    devices = sd.query_devices()
    for i, d in enumerate(devices):
        if name_part.lower() in d['name'].lower():
            if is_input and d['max_input_channels'] > 0:
                return i, d['name']
            if not is_input and d['max_output_channels'] > 0:
                return i, d['name']
    return None, None

def generate_tone(freq, duration, sr):
    t = np.arange(int(sr * duration)) / sr
    return (np.sin(2 * np.pi * freq * t) * 0.5).astype(np.float32)

def test_cable(cable_name):
    spk_id, spk_name = find_device(cable_name, False)
    mic_id, mic_name = find_device(cable_name, True)

    if spk_id is None or mic_id is None:
        print(f"  ERROR: Cannot find {cable_name} devices")
        return None

    print(f"  Speaker: [{spk_id}] {spk_name}")
    print(f"  Mic:     [{mic_id}] {mic_name}")

    tone = generate_tone(FREQ, DURATION, SAMPLE_RATE)
    recorded = np.zeros(int(SAMPLE_RATE * (DURATION + 1)), dtype=np.float32)

    # Start recording first
    rec_frames = []
    def callback(indata, frames, time_info, status):
        rec_frames.append(indata[:, 0].copy())

    with sd.InputStream(device=mic_id, samplerate=SAMPLE_RATE, channels=1,
                        dtype='float32', callback=callback, blocksize=1024):
        time.sleep(0.5)  # let capture stabilize

        # Play tone
        print(f"  Playing {FREQ}Hz tone for {DURATION}s...")
        sd.play(tone, samplerate=SAMPLE_RATE, device=spk_id, blocking=True)
        time.sleep(0.5)  # capture tail

    recorded = np.concatenate(rec_frames)

    # Analyze
    peak = np.max(np.abs(recorded))
    rms = np.sqrt(np.mean(recorded**2))

    # Find the tone region (where signal > 10% of peak)
    threshold = peak * 0.1
    active = np.abs(recorded) > threshold
    if np.any(active):
        first = np.argmax(active)
        last = len(active) - np.argmax(active[::-1]) - 1
        tone_region = recorded[first:last]
        tone_rms = np.sqrt(np.mean(tone_region**2))
        tone_peak = np.max(np.abs(tone_region))

        # Check for dropouts (sudden dips in amplitude)
        chunk_size = 480  # 10ms chunks
        chunks = len(tone_region) // chunk_size
        chunk_rms = np.array([np.sqrt(np.mean(tone_region[i*chunk_size:(i+1)*chunk_size]**2))
                              for i in range(chunks)])

        # Dropout = chunk RMS < 20% of median
        median_rms = np.median(chunk_rms)
        dropouts = np.sum(chunk_rms < median_rms * 0.2)

        print(f"  Peak: {tone_peak:.4f}, RMS: {tone_rms:.4f}")
        print(f"  Tone region: {first}-{last} ({(last-first)/SAMPLE_RATE:.2f}s)")
        print(f"  Chunks: {chunks}, Dropouts: {dropouts} ({dropouts/chunks*100:.1f}%)")

        if dropouts == 0:
            print(f"  *** CLEAN ***")
        else:
            print(f"  *** {dropouts} DROPOUTS ***")

        return {'peak': tone_peak, 'rms': tone_rms, 'dropouts': dropouts, 'chunks': chunks}
    else:
        print(f"  *** SILENT - no signal detected (peak={peak:.6f}) ***")
        return {'peak': peak, 'rms': rms, 'dropouts': -1, 'chunks': 0}

print("=" * 50)
print("Direct Loopback Test")
print("=" * 50)

# Test AO Cable B
print("\n--- AO Cable B ---")
ao_result = test_cable("AO Cable B")

# Test VB-Cable A
print("\n--- VB-Cable A ---")
vb_result = test_cable("VB-Audio Virtual Cable A")

print("\n--- Comparison ---")
if ao_result and vb_result:
    print(f"  AO: peak={ao_result['peak']:.4f} dropouts={ao_result['dropouts']}/{ao_result['chunks']}")
    print(f"  VB: peak={vb_result['peak']:.4f} dropouts={vb_result['dropouts']}/{vb_result['chunks']}")
