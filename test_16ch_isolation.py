"""
AO Virtual Cable - 16ch Channel Isolation Test
Stage 1: Sequential single-channel RMS isolation
Stage 2: (optional) Simultaneous 16-tone FFT crosstalk matrix

Requires: sounddevice (pip install sounddevice), numpy
Run as Administrator (for WDM-KS device access).
"""
import numpy as np
import sounddevice as sd
import sys

# ── Config ──────────────────────────────────────────────
SAMPLERATE = 48000
CHANNELS = 16
DURATION = 0.5          # seconds per channel probe
FREQ = 1000.0           # Hz sine for isolation test
AMPLITUDE = 0.8
SETTLE_FRAC = 0.15      # skip first 15% of capture (loopback latency)

# Thresholds
SIGNAL_RMS_MIN = 0.01   # target channel must exceed this
SILENCE_RMS_MAX = 0.005 # other channels must stay below this


def find_wdmks_devices(cable="AO Cable A"):
    """Find WDM-KS output (render) and input (capture) device indices by name."""
    devs = sd.query_devices()
    out_idx = None
    in_idx = None
    for i, d in enumerate(devs):
        api = sd.query_hostapis(d['hostapi'])['name']
        if cable not in d['name'] or api != 'Windows WDM-KS':
            continue
        if d['max_output_channels'] >= CHANNELS:
            out_idx = i
        if d['max_input_channels'] >= CHANNELS:
            in_idx = i
    return in_idx, out_idx


def generate_single_channel_tone(target_ch):
    """Generate float32 buffer: sine on target_ch, silence elsewhere."""
    num_frames = int(SAMPLERATE * DURATION)
    t = np.arange(num_frames, dtype=np.float32) / SAMPLERATE
    buf = np.zeros((num_frames, CHANNELS), dtype=np.float32)
    buf[:, target_ch] = AMPLITUDE * np.sin(2 * np.pi * FREQ * t)
    return buf


def measure_rms(recording):
    """Return per-channel RMS, skipping initial settle period."""
    skip = int(recording.shape[0] * SETTLE_FRAC)
    data = recording[skip:]
    return np.sqrt(np.mean(data ** 2, axis=0))


# ── Stage 1: Sequential isolation ──────────────────────
def run_stage1(in_dev, out_dev):
    print(f"\n{'='*60}")
    print(f" Stage 1: Sequential 1-channel isolation (16 probes)")
    print(f" Output [{out_dev}]  Input [{in_dev}]")
    print(f" {SAMPLERATE}Hz float32, {DURATION}s/probe, {FREQ}Hz tone")
    print(f"{'='*60}\n")

    mapping = list(range(1, CHANNELS + 1))
    passed = 0

    for ch in range(CHANNELS):
        tone = generate_single_channel_tone(ch)
        try:
            rec = sd.playrec(
                tone,
                samplerate=SAMPLERATE,
                input_mapping=mapping,
                output_mapping=mapping,
                device=(in_dev, out_dev),
                dtype='float32',
                blocking=True,
            )
        except Exception as e:
            print(f"  ch{ch:2d}: [FAIL] playrec error: {e}")
            continue

        rms = measure_rms(rec)
        sig = rms[ch]
        has_signal = sig >= SIGNAL_RMS_MIN

        # Find crosstalk channels
        crosstalk = []
        for c in range(CHANNELS):
            if c != ch and rms[c] > SILENCE_RMS_MAX:
                crosstalk.append((c, rms[c]))

        ok = has_signal and len(crosstalk) == 0
        tag = "PASS" if ok else "FAIL"
        if ok:
            passed += 1

        detail = f"signal={sig:.4f}"
        if not has_signal:
            detail += " (no signal!)"
        if crosstalk:
            xt = ", ".join(f"ch{c}={r:.4f}" for c, r in crosstalk[:3])
            detail += f"  crosstalk=[{xt}]"

        print(f"  ch{ch:2d}: [{tag}] {detail}")

    print(f"\n  Stage 1 result: {passed}/{CHANNELS} channels isolated")
    return passed


# ── Stage 2: Simultaneous 16-tone FFT matrix ───────────
def run_stage2(in_dev, out_dev):
    """All 16 channels play unique frequencies simultaneously.
    Capture then FFT each channel to verify frequency ownership."""
    base_freq = 200
    step = 100
    freqs = [base_freq + i * step for i in range(CHANNELS)]  # 200..1700 Hz

    print(f"\n{'='*60}")
    print(f" Stage 2: Simultaneous 16-tone FFT crosstalk matrix")
    print(f" Frequencies: {freqs[0]}~{freqs[-1]} Hz (step {step})")
    print(f"{'='*60}\n")

    num_frames = int(SAMPLERATE * DURATION * 2)  # longer for FFT resolution
    t = np.arange(num_frames, dtype=np.float32) / SAMPLERATE
    buf = np.zeros((num_frames, CHANNELS), dtype=np.float32)
    for ch in range(CHANNELS):
        buf[:, ch] = AMPLITUDE * np.sin(2 * np.pi * freqs[ch] * t)

    mapping = list(range(1, CHANNELS + 1))
    try:
        rec = sd.playrec(
            buf,
            samplerate=SAMPLERATE,
            input_mapping=mapping,
            output_mapping=mapping,
            device=(in_dev, out_dev),
            dtype='float32',
            blocking=True,
        )
    except Exception as e:
        print(f"  [FAIL] playrec error: {e}")
        return 0

    skip = int(rec.shape[0] * SETTLE_FRAC)
    data = rec[skip:]

    # FFT analysis per channel
    n = data.shape[0]
    freq_axis = np.fft.rfftfreq(n, 1.0 / SAMPLERATE)

    # Build energy matrix: energy[capture_ch][tone_idx]
    energy = np.zeros((CHANNELS, CHANNELS))
    bin_width = SAMPLERATE / n
    for cap_ch in range(CHANNELS):
        spectrum = np.abs(np.fft.rfft(data[:, cap_ch]))
        for tone_idx in range(CHANNELS):
            # Find bin closest to this tone's frequency
            target_bin = int(round(freqs[tone_idx] / bin_width))
            # Sum energy in a small window around the bin
            lo = max(0, target_bin - 2)
            hi = min(len(spectrum), target_bin + 3)
            energy[cap_ch, tone_idx] = np.sum(spectrum[lo:hi])

    # Normalize: convert to dB relative to expected diagonal
    diag = np.array([energy[i, i] for i in range(CHANNELS)])
    diag_max = np.max(diag) if np.max(diag) > 0 else 1.0

    passed = 0
    for ch in range(CHANNELS):
        own = energy[ch, ch]
        if own < 1e-6:
            print(f"  ch{ch:2d}: [FAIL] no energy at {freqs[ch]}Hz")
            continue
        # Find peak frequency in this capture channel
        spectrum = np.abs(np.fft.rfft(data[:, ch]))
        peak_bin = np.argmax(spectrum[1:]) + 1
        peak_freq = peak_bin * bin_width
        # Worst crosstalk
        others = [energy[ch, t] for t in range(CHANNELS) if t != ch]
        worst_xt = max(others) if others else 0
        snr_db = 20 * np.log10(own / worst_xt) if worst_xt > 0 else 99.0

        ok = abs(peak_freq - freqs[ch]) < bin_width * 3 and snr_db > 20
        tag = "PASS" if ok else "FAIL"
        if ok:
            passed += 1
        print(f"  ch{ch:2d}: [{tag}] expect={freqs[ch]}Hz peak={peak_freq:.0f}Hz SNR={snr_db:.1f}dB")

    # Print crosstalk matrix (compact)
    print(f"\n  Crosstalk matrix (dB, diagonal = 0):")
    header = "       " + "".join(f"ch{i:<4d}" for i in range(CHANNELS))
    print(header)
    for cap_ch in range(CHANNELS):
        row = f"  ch{cap_ch:2d}: "
        ref = energy[cap_ch, cap_ch] if energy[cap_ch, cap_ch] > 0 else 1e-10
        for tone_idx in range(CHANNELS):
            val = energy[cap_ch, tone_idx]
            if tone_idx == cap_ch:
                row += "  0  "
            elif val < 1e-10:
                row += " -inf"
            else:
                db = 20 * np.log10(val / ref)
                row += f"{db:5.0f}"
        print(row)

    print(f"\n  Stage 2 result: {passed}/{CHANNELS} channels frequency-matched")
    return passed


# ── Main ───────────────────────────────────────────────
if __name__ == '__main__':
    in_dev, out_dev = find_wdmks_devices("AO Cable A")
    if in_dev is None or out_dev is None:
        print("[FAIL] Cannot find AO Cable A WDM-KS 16ch devices")
        devs = sd.query_devices()
        for i, d in enumerate(devs):
            if 'AO Cable' in d['name']:
                api = sd.query_hostapis(d['hostapi'])['name']
                print(f"  [{i}] {d['name']} in={d['max_input_channels']} out={d['max_output_channels']} api={api}")
        sys.exit(1)

    print("AO Virtual Cable - 16ch Channel Isolation Test")
    print(f"Input:  [{in_dev}] {sd.query_devices(in_dev)['name']}")
    print(f"Output: [{out_dev}] {sd.query_devices(out_dev)['name']}")

    s1 = run_stage1(in_dev, out_dev)

    if s1 == CHANNELS:
        s2 = run_stage2(in_dev, out_dev)
    else:
        print("\n  Skipping Stage 2 (Stage 1 not fully passed)")
        s2 = 0

    print(f"\n{'='*60}")
    if s1 == CHANNELS and s2 == CHANNELS:
        print(f" FINAL: ALL PASS - 16ch isolation + frequency ownership verified")
    elif s1 == CHANNELS:
        print(f" FINAL: Stage 1 PASS ({s1}/{CHANNELS}), Stage 2 {s2}/{CHANNELS}")
    else:
        print(f" FINAL: Stage 1 {s1}/{CHANNELS} - channel isolation incomplete")
    print(f"{'='*60}")
