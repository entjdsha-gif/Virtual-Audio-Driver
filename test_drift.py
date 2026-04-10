"""
AO Virtual Cable - Clock Drift Measurement Test (M4d)

Plays a chirp-marked signal via loopback, then measures sample offset
at start/mid/end to estimate clock drift between play and capture streams.

Design:
  Play buffer: [warmup] [chirp₁][tone][chirp₂][tone][chirp₃] [tail]
  - 3 chirp markers placed at start, middle, end of the signal
  - Cross-correlate each chirp independently to get local lag
  - Drift = difference in lag between markers

Requirements:
    pip install sounddevice numpy scipy

Usage:
    python test_drift.py --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks
    python test_drift.py --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks --duration 600
    python test_drift.py --list-devices --api wdmks
"""

import argparse
import os
import sys
import time

import numpy as np

from test_quality_common import (
    find_device,
    device_info_str,
    list_devices,
    generate_sweep,
    generate_sine,
    loopback,
    save_results_csv,
    save_results_json,
    MeasurementResult,
)

# Chirp marker config
CHIRP_DURATION = 0.05       # 50ms
CHIRP_F_START = 200.0
CHIRP_F_END = 8000.0
CHIRP_AMPLITUDE = 0.7

# Tone fill between markers
TONE_FREQ = 1000.0
TONE_AMPLITUDE = 0.5

# Settle padding
WARMUP_SECONDS = 0.5
TAIL_SECONDS = 0.5

# Drift thresholds
MAX_DRIFT_MS_PER_HOUR = 500.0  # generous for initial smoke test

# Number of chirp markers (placed evenly)
N_MARKERS = 5


def _build_drift_buffer(
    samplerate: int,
    channels: int,
    duration: float,
    n_markers: int,
) -> tuple:
    """Build play buffer with N chirp markers evenly spaced, filled with sine.

    Returns (play_buf, chirp_ref, marker_positions).
    """
    chirp = generate_sweep(
        f_start=CHIRP_F_START, f_end=CHIRP_F_END,
        samplerate=samplerate, duration=CHIRP_DURATION,
        channels=channels, amplitude=CHIRP_AMPLITUDE,
    )
    chirp_len = chirp.shape[0]

    warmup = int(samplerate * WARMUP_SECONDS)
    tail = int(samplerate * TAIL_SECONDS)
    signal_len = int(samplerate * duration)
    total = warmup + signal_len + tail

    # Fill with sine
    t = np.arange(signal_len, dtype=np.float32) / samplerate
    sine_mono = (TONE_AMPLITUDE * np.sin(2 * np.pi * TONE_FREQ * t)).astype(np.float32)
    sine = np.column_stack([sine_mono] * channels)

    play_buf = np.zeros((total, channels), dtype=np.float32)
    play_buf[warmup : warmup + signal_len] = sine

    # Place chirp markers evenly within the signal region
    marker_positions = []
    for i in range(n_markers):
        pos = warmup + int(i * (signal_len - chirp_len) / max(n_markers - 1, 1))
        play_buf[pos : pos + chirp_len] = chirp
        marker_positions.append(pos)

    return play_buf, chirp, marker_positions


def _measure_marker_lags(
    cap_mono: np.ndarray,
    chirp_mono: np.ndarray,
    marker_positions: list,
    samplerate: int,
) -> list:
    """For each marker, find lag via local cross-correlation.

    Uses chirp 1 for global lag estimate, then narrow search for others.
    """
    from scipy.signal import correlate

    chirp_len = len(chirp_mono)
    wide_margin = int(samplerate * 0.25)
    narrow_margin = int(samplerate * 0.015)

    results = []

    # Global lag from first marker
    first_pos = marker_positions[0]
    win_s = max(0, first_pos)
    win_e = min(len(cap_mono), first_pos + chirp_len + wide_margin)
    corr = correlate(cap_mono[win_s:win_e], chirp_mono, mode="full", method="fft")
    peak = int(np.argmax(np.abs(corr)))
    global_lag = win_s + peak - (chirp_len - 1) - first_pos

    for pos in marker_positions:
        expected = pos + global_lag
        ws = max(0, expected - narrow_margin)
        we = min(len(cap_mono), expected + chirp_len + narrow_margin)
        cap_win = cap_mono[ws:we]

        corr = correlate(cap_win, chirp_mono, mode="full", method="fft")
        peak_idx = int(np.argmax(np.abs(corr)))
        lag = ws + peak_idx - (chirp_len - 1) - pos

        results.append({
            "position": pos,
            "position_sec": pos / samplerate,
            "lag_samples": lag,
            "lag_ms": lag / samplerate * 1000,
        })

    return results


def run_drift_test(
    play_dev: int,
    rec_dev: int,
    samplerate: int = 48000,
    channels: int = 2,
    duration: float = 60.0,
    out_dir: str | None = None,
) -> MeasurementResult:
    """Measure clock drift between play and capture streams."""
    n_markers = max(3, min(N_MARKERS, int(duration / 10) + 2))

    print(f"\n{'='*60}")
    print(f" Clock Drift Measurement")
    print(f" Play:     {device_info_str(play_dev)}")
    print(f" Record:   {device_info_str(rec_dev)}")
    print(f" Format:   {samplerate}Hz / float32 / {channels}ch")
    print(f" Duration: {duration}s, {n_markers} markers")
    print(f"{'='*60}")

    play_buf, chirp_ref, marker_positions = _build_drift_buffer(
        samplerate, channels, duration, n_markers,
    )
    chirp_mono = chirp_ref[:, 0].astype(np.float64)

    print(f"  Buffer: {play_buf.shape[0]} samples ({play_buf.shape[0]/samplerate:.1f}s)")
    print(f"  Playing + recording ...")
    t0 = time.time()
    captured = loopback(play_buf, play_dev, rec_dev, samplerate, channels)
    elapsed = time.time() - t0
    print(f"  Done in {elapsed:.1f}s")

    cap_mono = captured[:, 0].astype(np.float64)

    # Measure lags at each marker
    marker_results = _measure_marker_lags(
        cap_mono, chirp_mono, marker_positions, samplerate,
    )

    print(f"\n  Marker lags:")
    for i, mr in enumerate(marker_results):
        print(f"    Marker {i+1}: pos={mr['position_sec']:.1f}s  "
              f"lag={mr['lag_samples']:+d} ({mr['lag_ms']:+.2f}ms)")

    # Compute drift: linear regression on markers 2..N (skip startup-contaminated first)
    steady_results = marker_results[1:]  # discard marker 1
    positions = np.array([mr["position"] for mr in steady_results], dtype=np.float64)
    lags = np.array([mr["lag_samples"] for mr in steady_results], dtype=np.float64)

    insufficient = len(positions) < 2
    is_smoke = duration < 30

    if not insufficient:
        coeffs = np.polyfit(positions, lags, 1)
        drift_rate = coeffs[0]  # samples of drift per sample of signal
        drift_offset = coeffs[1]

        # Convert to meaningful units
        drift_samples_per_hour = drift_rate * samplerate * 3600
        drift_ms_per_hour = drift_samples_per_hour / samplerate * 1000

        # Total drift over steady-state span
        total_drift_samples = lags[-1] - lags[0]
        total_drift_ms = total_drift_samples / samplerate * 1000
        span_seconds = (positions[-1] - positions[0]) / samplerate

        # Residual from linear fit (jitter)
        fitted = np.polyval(coeffs, positions)
        residuals = lags - fitted
        jitter_samples = float(np.std(residuals))
        jitter_ms = jitter_samples / samplerate * 1000
    else:
        drift_rate = 0
        drift_samples_per_hour = 0
        drift_ms_per_hour = 0
        total_drift_samples = 0
        total_drift_ms = 0
        span_seconds = 0
        jitter_samples = 0
        jitter_ms = 0

    print(f"\n  Drift analysis (markers 2..{n_markers}, excluding startup):")
    if insufficient:
        print(f"    Insufficient steady-state markers ({len(positions)}<2) for regression")
    else:
        print(f"    Total drift:     {total_drift_samples:+.0f} samples "
              f"({total_drift_ms:+.2f} ms) over {span_seconds:.1f}s")
        print(f"    Drift rate:      {drift_rate*1e6:+.2f} ppm")
        print(f"    Drift/hour:      {drift_samples_per_hour:+.0f} samples "
              f"({drift_ms_per_hour:+.1f} ms)")
        print(f"    Jitter (stddev): {jitter_samples:.1f} samples ({jitter_ms:.2f} ms)")
    if is_smoke:
        print(f"    Note: duration {duration}s < 30s - smoke-only, relaxed criteria")

    if insufficient:
        passed = True  # can't judge, don't fail
        notes = "insufficient markers for steady-state drift"
    elif is_smoke:
        passed = True  # smoke run, don't hard-fail on drift
        notes = f"smoke ({duration}s)"
    else:
        passed = abs(drift_ms_per_hour) < MAX_DRIFT_MS_PER_HOUR
        notes = ""

    r = MeasurementResult(
        test_name="drift_test",
        device_play=device_info_str(play_dev),
        device_rec=device_info_str(rec_dev),
        samplerate=samplerate,
        channels=channels,
        duration=duration,
        latency_samples=int(marker_results[0]["lag_samples"]),
        latency_ms=round(marker_results[0]["lag_ms"], 2),
        passed=passed,
        notes=notes,
        extra={
            "n_markers": n_markers,
            "steady_markers_used": len(positions),
            "drift_rate_ppm": round(drift_rate * 1e6, 2),
            "drift_samples_per_hour": round(drift_samples_per_hour, 1),
            "drift_ms_per_hour": round(drift_ms_per_hour, 2),
            "total_drift_samples": int(total_drift_samples),
            "total_drift_ms": round(total_drift_ms, 2),
            "jitter_samples": round(jitter_samples, 2),
            "jitter_ms": round(jitter_ms, 3),
            "marker_lags_ms": [round(mr["lag_ms"], 2) for mr in marker_results],
        },
    )

    if insufficient:
        print(f"\n  [SKIP] Insufficient markers for drift judgment")
    elif is_smoke:
        tag = "PASS" if abs(drift_ms_per_hour) < MAX_DRIFT_MS_PER_HOUR else "WARN"
        print(f"\n  [{tag}] Smoke: drift {drift_ms_per_hour:+.1f} ms/hour "
              f"(formal test requires >=30s)")
    else:
        tag = "PASS" if passed else "FAIL"
        print(f"\n  [{tag}] Drift: {drift_ms_per_hour:+.1f} ms/hour "
              f"(threshold: +/-{MAX_DRIFT_MS_PER_HOUR}ms/hour)")

    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
        ts = time.strftime("%Y%m%d_%H%M%S")
        save_results_csv([r], os.path.join(out_dir, "results.csv"))
        save_results_json([r], os.path.join(out_dir, f"drift_{ts}.json"))
        print(f"    Saved to {out_dir}/")

    return r


def main():
    p = argparse.ArgumentParser(
        description="AO Virtual Cable - Clock Drift Measurement (M4d)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
Examples:
  %(prog)s --list-devices --api wdmks
  %(prog)s --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks
  %(prog)s --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks --duration 600
""",
    )
    p.add_argument("--list-devices", action="store_true")
    p.add_argument("--api", default=None)
    p.add_argument("--play-device", default=None)
    p.add_argument("--record-device", default=None)
    p.add_argument("--samplerate", type=int, default=48000)
    p.add_argument("--channels", type=int, default=2)
    p.add_argument("--duration", type=float, default=60.0,
                   help="Test duration in seconds (default: 60)")
    p.add_argument("--out-dir", default=None)
    args = p.parse_args()

    if args.list_devices:
        list_devices(args.api)
        return

    if not args.play_device or not args.record_device:
        p.print_help()
        return

    play_dev = find_device(args.play_device, is_input=False,
                           api_filter=args.api, min_channels=args.channels)
    rec_dev = find_device(args.record_device, is_input=True,
                          api_filter=args.api, min_channels=args.channels)
    if play_dev is None or rec_dev is None:
        print("ERROR: device not found")
        list_devices(args.api)
        sys.exit(1)

    result = run_drift_test(
        play_dev, rec_dev,
        samplerate=args.samplerate,
        channels=args.channels,
        duration=args.duration,
        out_dir=args.out_dir,
    )
    sys.exit(0 if result.passed else 1)


if __name__ == "__main__":
    main()
