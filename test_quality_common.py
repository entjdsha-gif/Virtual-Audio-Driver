"""
AO Virtual Cable - Quality Measurement Harness (M4a)
Reusable utilities for bit-exact, latency, dropout, and comparison tests.

Requirements:
    pip install sounddevice numpy scipy

Usage:
    python test_quality_common.py --list-devices
    python test_quality_common.py --list-devices --api wasapi
    python test_quality_common.py --play-device "AO Cable A" --record-device "AO Cable A" --duration 2
    python test_quality_common.py --play-device "AO Cable A" --record-device "AO Cable A" --selftest
"""

import argparse
import csv
import json
import os
import sys
import time
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Optional

import numpy as np
import sounddevice as sd

# ---------------------------------------------------------------------------
# Device lookup
# ---------------------------------------------------------------------------

HOST_API_ALIASES = {
    "wasapi": "Windows WASAPI",
    "wdmks": "Windows WDM-KS",
    "mme": "MME",
    "ds": "Windows DirectSound",
}


def list_devices(api_filter: Optional[str] = None):
    """Print all audio devices, optionally filtered by host API."""
    devs = sd.query_devices()
    apis = {i: sd.query_hostapis(i)["name"] for i in range(len(sd.query_hostapis()))}
    api_match = HOST_API_ALIASES.get(api_filter, api_filter) if api_filter else None

    print(f"{'Idx':>4}  {'In':>3}  {'Out':>3}  {'API':<22}  Name")
    print("-" * 80)
    for i, d in enumerate(devs):
        api_name = apis[d["hostapi"]]
        if api_match and api_match.lower() not in api_name.lower():
            continue
        print(
            f"{i:>4}  {d['max_input_channels']:>3}  "
            f"{d['max_output_channels']:>3}  {api_name:<22}  {d['name']}"
        )


def find_device(
    name_fragment: str,
    is_input: bool,
    api_filter: Optional[str] = None,
    min_channels: int = 1,
) -> Optional[int]:
    """Find device index by name substring and optional host API filter.

    Returns the device index or None.
    """
    devs = sd.query_devices()
    api_match = HOST_API_ALIASES.get(api_filter, api_filter) if api_filter else None

    for i, d in enumerate(devs):
        if name_fragment.lower() not in d["name"].lower():
            continue
        api_name = sd.query_hostapis(d["hostapi"])["name"]
        if api_match and api_match.lower() not in api_name.lower():
            continue
        ch_key = "max_input_channels" if is_input else "max_output_channels"
        if d[ch_key] >= min_channels:
            return i
    return None


def device_info_str(idx: int) -> str:
    """Return a human-readable one-liner for a device index."""
    d = sd.query_devices(idx)
    api = sd.query_hostapis(d["hostapi"])["name"]
    return f"[{idx}] {d['name']}  (in={d['max_input_channels']} out={d['max_output_channels']} api={api})"


# ---------------------------------------------------------------------------
# Signal generation
# ---------------------------------------------------------------------------


def generate_sine(
    freq: float,
    samplerate: int = 48000,
    duration: float = 1.0,
    channels: int = 2,
    amplitude: float = 0.8,
    dtype: str = "float32",
) -> np.ndarray:
    """Generate a multi-channel sine tone. Signal on all channels."""
    n = int(samplerate * duration)
    t = np.arange(n, dtype=np.float32) / samplerate
    mono = (amplitude * np.sin(2 * np.pi * freq * t)).astype(dtype)
    return np.column_stack([mono] * channels)


def generate_silence(
    samplerate: int = 48000,
    duration: float = 1.0,
    channels: int = 2,
    dtype: str = "float32",
) -> np.ndarray:
    n = int(samplerate * duration)
    return np.zeros((n, channels), dtype=dtype)


def generate_impulse(
    samplerate: int = 48000,
    channels: int = 2,
    amplitude: float = 0.9,
    dtype: str = "float32",
) -> np.ndarray:
    """Single-sample impulse for latency measurement (preceded/followed by silence)."""
    pre = int(samplerate * 0.1)
    post = int(samplerate * 0.5)
    n = pre + 1 + post
    buf = np.zeros((n, channels), dtype=dtype)
    buf[pre, :] = amplitude
    return buf


def generate_sweep(
    f_start: float = 20.0,
    f_end: float = 20000.0,
    samplerate: int = 48000,
    duration: float = 1.0,
    channels: int = 2,
    amplitude: float = 0.8,
    dtype: str = "float32",
) -> np.ndarray:
    """Linear frequency sweep (chirp)."""
    n = int(samplerate * duration)
    t = np.arange(n, dtype=np.float64) / samplerate
    phase = 2 * np.pi * (f_start * t + (f_end - f_start) / (2 * duration) * t ** 2)
    mono = (amplitude * np.sin(phase)).astype(dtype)
    return np.column_stack([mono] * channels)


def generate_mls(
    nbits: int = 16,
    channels: int = 2,
    amplitude: float = 0.5,
    reps: int = 1,
    dtype: str = "float32",
) -> np.ndarray:
    """Generate a Maximum Length Sequence (MLS) test signal.

    MLS has ideal autocorrelation (sharp delta peak) — perfect for
    unambiguous sample-accurate alignment via cross-correlation.

    Args:
        nbits: MLS order. Length = 2**nbits - 1 samples.
                16 → 65535 samples (~1.36s @48kHz)
                17 → 131071 samples (~2.73s @48kHz)
        channels: number of output channels (same signal on all).
        amplitude: peak amplitude (MLS is bipolar +-amplitude).
        reps: number of times to repeat the sequence.

    Returns:
        float32 ndarray of shape (length * reps, channels).
    """
    from scipy.signal import max_len_seq

    seq, _ = max_len_seq(nbits)
    # max_len_seq returns {0,1} → convert to bipolar {-1, +1}
    mono = (2.0 * seq - 1.0).astype(np.float64) * amplitude
    mono = mono.astype(dtype)
    if reps > 1:
        mono = np.tile(mono, reps)
    return np.column_stack([mono] * channels)


# ---------------------------------------------------------------------------
# Render / Capture session helpers
# ---------------------------------------------------------------------------


def play(
    data: np.ndarray,
    device: int,
    samplerate: int = 48000,
    blocking: bool = True,
):
    """Play audio buffer on a device."""
    sd.play(data, samplerate=samplerate, device=device, blocking=blocking)


def record(
    duration: float,
    device: int,
    samplerate: int = 48000,
    channels: int = 2,
    dtype: str = "float32",
) -> np.ndarray:
    """Record audio from a device."""
    frames = int(samplerate * duration)
    return sd.rec(
        frames,
        samplerate=samplerate,
        channels=channels,
        device=device,
        dtype=dtype,
        blocking=True,
    )


def loopback(
    data: np.ndarray,
    play_device: int,
    rec_device: int,
    samplerate: int = 48000,
    rec_channels: int = 2,
    dtype: str = "float32",
) -> np.ndarray:
    """Play on one device and simultaneously record on another. Returns captured audio."""
    return sd.playrec(
        data,
        samplerate=samplerate,
        device=(rec_device, play_device),
        channels=rec_channels,
        dtype=dtype,
        blocking=True,
    )


# ---------------------------------------------------------------------------
# WAV save / load helpers
# ---------------------------------------------------------------------------


def save_wav(path: str, data: np.ndarray, samplerate: int = 48000):
    """Save numpy array as WAV file (uses scipy)."""
    from scipy.io import wavfile

    # scipy expects (samples, channels) for multi-channel
    if data.dtype == np.float32 or data.dtype == np.float64:
        wavfile.write(path, samplerate, data)
    else:
        wavfile.write(path, samplerate, data)


def load_wav(path: str):
    """Load WAV file, return (samplerate, data)."""
    from scipy.io import wavfile

    sr, data = wavfile.read(path)
    return sr, data.astype(np.float32) if data.dtype != np.float32 else data


# ---------------------------------------------------------------------------
# Analysis helpers
# ---------------------------------------------------------------------------


def rms(data: np.ndarray, axis: int = 0) -> np.ndarray:
    """RMS per channel (axis=0) or overall."""
    return np.sqrt(np.mean(data ** 2, axis=axis))


def rms_db(data: np.ndarray, ref: float = 1.0, axis: int = 0) -> np.ndarray:
    """RMS in dBFS."""
    r = rms(data, axis=axis)
    return 20 * np.log10(np.maximum(r, 1e-10) / ref)


def peak_db(data: np.ndarray, ref: float = 1.0) -> float:
    """Peak amplitude in dBFS."""
    p = np.max(np.abs(data))
    return 20 * np.log10(max(p, 1e-10) / ref)


def fft_magnitude(data: np.ndarray, samplerate: int = 48000):
    """Return (freq_axis, magnitude) for single-channel data."""
    if data.ndim > 1:
        data = data[:, 0]
    n = len(data)
    spectrum = np.abs(np.fft.rfft(data)) / n
    freqs = np.fft.rfftfreq(n, 1.0 / samplerate)
    return freqs, spectrum


def dominant_frequency(data: np.ndarray, samplerate: int = 48000) -> float:
    """Find the dominant frequency in a signal."""
    freqs, mag = fft_magnitude(data, samplerate)
    # Skip DC bin
    idx = np.argmax(mag[1:]) + 1
    return freqs[idx]


def thd(data: np.ndarray, fundamental_freq: float, samplerate: int = 48000) -> float:
    """Total Harmonic Distortion (%) up to 5th harmonic."""
    freqs, mag = fft_magnitude(data, samplerate)
    bin_width = freqs[1] - freqs[0]

    def energy_at(f):
        idx = int(round(f / bin_width))
        lo = max(0, idx - 2)
        hi = min(len(mag), idx + 3)
        return np.sqrt(np.sum(mag[lo:hi] ** 2))

    h1 = energy_at(fundamental_freq)
    if h1 < 1e-10:
        return 0.0
    harmonics_sq = sum(
        energy_at(fundamental_freq * k) ** 2 for k in range(2, 6)
    )
    return 100.0 * np.sqrt(harmonics_sq) / h1


def cross_correlate(ref: np.ndarray, captured: np.ndarray):
    """Cross-correlation between two signals. Returns (lag_samples, correlation_peak).

    Useful for latency measurement: lag_samples / samplerate = latency_seconds.
    Uses scipy FFT-based correlation for O(n log n) performance on long signals.
    """
    from scipy.signal import correlate

    if ref.ndim > 1:
        ref = ref[:, 0]
    if captured.ndim > 1:
        captured = captured[:, 0]
    corr = correlate(captured, ref, mode="full", method="fft")
    peak_idx = np.argmax(np.abs(corr))
    lag = peak_idx - (len(ref) - 1)
    return lag, corr[peak_idx]


def count_dropouts(
    data: np.ndarray,
    samplerate: int = 48000,
    window_ms: float = 5.0,
    threshold_db: float = -60.0,
    min_expected_db: float = -40.0,
    margin_windows: int = 3,
) -> list:
    """Count dropout events: windows where RMS drops below threshold while
    surrounding windows have signal above min_expected_db.

    Args:
        margin_windows: number of windows to exclude from both edges to avoid
            false positives from settle/tail transients (default: 3).

    Returns list of (start_sample, duration_samples) for each dropout.
    """
    if data.ndim > 1:
        data = data[:, 0]
    win = int(samplerate * window_ms / 1000)
    n_windows = len(data) // win
    if n_windows == 0:
        return []

    rms_values = np.array([
        np.sqrt(np.mean(data[i * win : (i + 1) * win] ** 2))
        for i in range(n_windows)
    ])
    rms_db_values = 20 * np.log10(np.maximum(rms_values, 1e-10))

    # Only flag as dropout if there's actual signal context
    median_rms = np.median(rms_db_values)
    if median_rms < min_expected_db:
        return []  # no meaningful signal to have dropouts in

    # Skip edge windows to avoid settle/tail false positives
    start_win = min(margin_windows, n_windows // 4)
    end_win = max(start_win, n_windows - margin_windows)
    if start_win >= end_win:
        return []

    dropouts = []
    in_dropout = False
    start = 0
    for i in range(start_win, end_win):
        db = rms_db_values[i]
        if db < threshold_db and not in_dropout:
            in_dropout = True
            start = i * win
        elif db >= threshold_db and in_dropout:
            in_dropout = False
            dropouts.append((start, i * win - start))
    if in_dropout:
        dropouts.append((start, end_win * win - start))
    return dropouts


# ---------------------------------------------------------------------------
# Result storage
# ---------------------------------------------------------------------------


@dataclass
class MeasurementResult:
    test_name: str
    timestamp: str = ""
    device_play: str = ""
    device_rec: str = ""
    samplerate: int = 48000
    channels: int = 2
    duration: float = 0.0
    rms_db: float = 0.0
    peak_db: float = 0.0
    dominant_freq: float = 0.0
    thd_pct: float = 0.0
    latency_samples: int = 0
    latency_ms: float = 0.0
    dropout_count: int = 0
    passed: bool = False
    notes: str = ""
    extra: dict = field(default_factory=dict)

    def __post_init__(self):
        if not self.timestamp:
            self.timestamp = time.strftime("%Y-%m-%dT%H:%M:%S")


def save_results_csv(results: list, path: str):
    """Append measurement results to a CSV file."""
    file_exists = os.path.exists(path) and os.path.getsize(path) > 0
    fieldnames = list(MeasurementResult.__dataclass_fields__.keys())
    with open(path, "a", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        if not file_exists:
            writer.writeheader()
        for r in results:
            d = asdict(r)
            d["extra"] = json.dumps(_to_jsonable(d["extra"])) if d["extra"] else ""
            writer.writerow(d)


def _to_jsonable(value):
    """Recursively convert numpy scalars/arrays into plain Python types."""
    if isinstance(value, dict):
        return {str(k): _to_jsonable(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [_to_jsonable(v) for v in value]
    if isinstance(value, np.ndarray):
        return value.tolist()
    if isinstance(value, np.generic):
        return value.item()
    return value


def save_results_json(results: list, path: str):
    """Write measurement results to a JSON file."""
    data = [_to_jsonable(asdict(r)) for r in results]
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)


# ---------------------------------------------------------------------------
# Self-test: quick loopback quality check
# ---------------------------------------------------------------------------


def run_selftest(
    play_dev: int,
    rec_dev: int,
    samplerate: int = 48000,
    channels: int = 2,
    duration: float = 2.0,
    out_dir: Optional[str] = None,
) -> MeasurementResult:
    """Run a basic loopback self-test: play 1kHz sine, capture, and analyze."""
    print(f"\n--- Self-test: 1kHz sine loopback ---")
    print(f"  Play:   {device_info_str(play_dev)}")
    print(f"  Record: {device_info_str(rec_dev)}")
    print(f"  Format: {samplerate}Hz / float32 / {channels}ch / {duration}s")

    tone = generate_sine(1000.0, samplerate, duration, channels)
    captured = loopback(tone, play_dev, rec_dev, samplerate, channels)

    # Use cross-correlation to find where the tone actually starts in capture
    lag, _ = cross_correlate(tone, captured)
    tone_len = tone.shape[0]

    # Determine active region: from lag offset, length = tone duration,
    # with 10% margin trimmed from both ends to avoid transients
    active_start = max(0, lag)
    active_end = min(captured.shape[0], active_start + tone_len)
    margin = int((active_end - active_start) * 0.10)
    analysis_start = active_start + margin
    analysis_end = active_end - margin
    if analysis_end <= analysis_start:
        # Fallback: skip 15% from start
        analysis_start = int(captured.shape[0] * 0.15)
        analysis_end = captured.shape[0]
    analysis = captured[analysis_start:analysis_end]

    r = MeasurementResult(
        test_name="selftest_1kHz",
        device_play=device_info_str(play_dev),
        device_rec=device_info_str(rec_dev),
        samplerate=samplerate,
        channels=channels,
        duration=duration,
    )
    r.rms_db = float(rms_db(analysis).mean())
    r.peak_db = float(peak_db(analysis))
    r.dominant_freq = float(dominant_frequency(analysis, samplerate))
    r.thd_pct = float(thd(analysis, 1000.0, samplerate))

    r.latency_samples = int(lag)
    r.latency_ms = round(lag / samplerate * 1000, 2)

    drops = count_dropouts(analysis, samplerate)
    r.dropout_count = len(drops)

    freq_ok = 950 < r.dominant_freq < 1050
    signal_ok = r.rms_db > -20
    r.passed = freq_ok and signal_ok and r.dropout_count == 0

    tag = "PASS" if r.passed else "FAIL"
    print(f"\n  [{tag}] Results:")
    print(f"    RMS:       {r.rms_db:+.1f} dBFS")
    print(f"    Peak:      {r.peak_db:+.1f} dBFS")
    print(f"    Freq:      {r.dominant_freq:.1f} Hz (expect ~1000)")
    print(f"    THD:       {r.thd_pct:.3f} %")
    print(f"    Latency:   {r.latency_samples} samples ({r.latency_ms} ms)")
    print(f"    Dropouts:  {r.dropout_count}")

    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
        ts = time.strftime("%Y%m%d_%H%M%S")
        wav_path = os.path.join(out_dir, f"selftest_{ts}.wav")
        save_wav(wav_path, captured, samplerate)
        print(f"    WAV saved: {wav_path}")

        csv_path = os.path.join(out_dir, "results.csv")
        save_results_csv([r], csv_path)
        json_path = os.path.join(out_dir, f"selftest_{ts}.json")
        save_results_json([r], json_path)
        print(f"    CSV:       {csv_path}")
        print(f"    JSON:      {json_path}")

    return r


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="AO Virtual Cable - Quality Measurement Harness",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
Examples:
  %(prog)s --list-devices
  %(prog)s --list-devices --api wasapi
  %(prog)s --play-device "AO Cable A" --record-device "AO Cable A" --selftest
  %(prog)s --play-device "AO Cable A" --record-device "AO Cable A" --duration 3 --out-dir results/
""",
    )
    p.add_argument("--list-devices", action="store_true", help="List audio devices and exit")
    p.add_argument("--api", default=None, help="Filter by host API (wasapi, wdmks, mme, ds)")
    p.add_argument("--play-device", default=None, help="Render device name fragment")
    p.add_argument("--record-device", default=None, help="Capture device name fragment")
    p.add_argument("--samplerate", type=int, default=48000, help="Sample rate (default: 48000)")
    p.add_argument("--channels", type=int, default=2, help="Channel count (default: 2)")
    p.add_argument("--duration", type=float, default=2.0, help="Duration in seconds (default: 2.0)")
    p.add_argument("--out-dir", default=None, help="Output directory for WAV/CSV/JSON")
    p.add_argument("--selftest", action="store_true", help="Run 1kHz loopback self-test")
    return p


def main():
    parser = build_parser()
    args = parser.parse_args()

    if args.list_devices:
        list_devices(args.api)
        return

    if not args.play_device and not args.record_device:
        parser.print_help()
        return

    play_dev = None
    rec_dev = None

    if args.play_device:
        play_dev = find_device(
            args.play_device, is_input=False,
            api_filter=args.api, min_channels=args.channels,
        )
        if play_dev is None:
            print(f"ERROR: Play device not found: '{args.play_device}' (need >={args.channels}ch output)")
            list_devices(args.api)
            sys.exit(1)
        print(f"Play device:   {device_info_str(play_dev)}")

    if args.record_device:
        rec_dev = find_device(
            args.record_device, is_input=True,
            api_filter=args.api, min_channels=args.channels,
        )
        if rec_dev is None:
            print(f"ERROR: Record device not found: '{args.record_device}' (need >={args.channels}ch input)")
            list_devices(args.api)
            sys.exit(1)
        print(f"Record device: {device_info_str(rec_dev)}")

    if args.selftest:
        if play_dev is None or rec_dev is None:
            print("ERROR: --selftest requires both --play-device and --record-device")
            sys.exit(1)
        result = run_selftest(
            play_dev, rec_dev,
            samplerate=args.samplerate,
            channels=args.channels,
            duration=args.duration,
            out_dir=args.out_dir,
        )
        sys.exit(0 if result.passed else 1)

    # Default: record only
    if rec_dev is not None and play_dev is None:
        print(f"\nRecording {args.duration}s ...")
        data = record(args.duration, rec_dev, args.samplerate, args.channels)
        r = rms_db(data)
        p = peak_db(data)
        print(f"  RMS:  {r.mean():+.1f} dBFS")
        print(f"  Peak: {p:+.1f} dBFS")
        if args.out_dir:
            os.makedirs(args.out_dir, exist_ok=True)
            ts = time.strftime("%Y%m%d_%H%M%S")
            wav_path = os.path.join(args.out_dir, f"capture_{ts}.wav")
            save_wav(wav_path, data, args.samplerate)
            print(f"  WAV:  {wav_path}")

    # Play only
    elif play_dev is not None and rec_dev is None:
        print(f"\nPlaying 1kHz sine for {args.duration}s ...")
        tone = generate_sine(1000.0, args.samplerate, args.duration, args.channels)
        play(tone, play_dev, args.samplerate)
        print("  Done.")

    # Both: default loopback
    else:
        print(f"\nLoopback: play 1kHz sine → capture for {args.duration}s ...")
        tone = generate_sine(1000.0, args.samplerate, args.duration, args.channels)
        captured = loopback(tone, play_dev, rec_dev, args.samplerate, args.channels)
        skip = int(captured.shape[0] * 0.1)
        analysis = captured[skip:]
        print(f"  RMS:      {rms_db(analysis).mean():+.1f} dBFS")
        print(f"  Peak:     {peak_db(analysis):+.1f} dBFS")
        print(f"  Freq:     {dominant_frequency(analysis, args.samplerate):.1f} Hz")
        drops = count_dropouts(analysis, args.samplerate)
        print(f"  Dropouts: {len(drops)}")
        if args.out_dir:
            os.makedirs(args.out_dir, exist_ok=True)
            ts = time.strftime("%Y%m%d_%H%M%S")
            save_wav(os.path.join(args.out_dir, f"loopback_{ts}.wav"), captured, args.samplerate)
            print(f"  WAV saved to {args.out_dir}/")


if __name__ == "__main__":
    main()
