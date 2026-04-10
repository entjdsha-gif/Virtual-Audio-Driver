"""
AO Virtual Cable - Round-Trip Latency Test (M4c)

Measures loopback latency using a single long full-duplex session with
multiple chirp probes embedded. This eliminates stream startup jitter
that corrupts per-trial measurements.

  L01: Impulse — single-sample spike, detect peak (auxiliary, single trial)
  L02: Chirp   — N chirps in one session, cross-correlate each independently

Requirements:
    pip install sounddevice numpy scipy

Usage:
    python test_latency.py --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks
    python test_latency.py --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks --trials 10
    python test_latency.py --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks --test l02 --out-dir results/
    python test_latency.py --list-devices --api wdmks
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
    loopback,
    save_results_csv,
    save_results_json,
    MeasurementResult,
)

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

CHIRP_DURATION = 0.05      # 50ms chirp
CHIRP_F_START = 200.0
CHIRP_F_END = 8000.0
CHIRP_AMPLITUDE = 0.7
CHIRP_SPACING = 0.3        # 300ms silence between chirps
WARMUP_SECONDS = 0.5       # silence at start (stream settles)
TAIL_SECONDS = 0.5         # silence at end

# Pass thresholds
MAX_LATENCY_MS = 200.0
MAX_JITTER_MS = 5.0        # tighter now that startup jitter is removed


# ---------------------------------------------------------------------------
# L01: Impulse latency (single trial, auxiliary)
# ---------------------------------------------------------------------------

def run_l01_impulse(
    play_dev: int,
    rec_dev: int,
    samplerate: int = 48000,
    channels: int = 2,
    out_dir: str | None = None,
) -> MeasurementResult:
    """L01: Single impulse loopback (auxiliary quick check)."""
    print(f"\n{'='*60}")
    print(f" L01: Impulse Latency (single shot)")
    print(f" Play:   {device_info_str(play_dev)}")
    print(f" Record: {device_info_str(rec_dev)}")
    print(f"{'='*60}")

    # Build: [warmup 0.5s] + [impulse] + [tail 0.5s]
    warmup = int(samplerate * WARMUP_SECONDS)
    tail = int(samplerate * TAIL_SECONDS)
    total = warmup + 1 + tail
    play_buf = np.zeros((total, channels), dtype=np.float32)
    play_buf[warmup, :] = 0.9
    impulse_pos = warmup

    captured = loopback(play_buf, play_dev, rec_dev, samplerate, channels)

    cap_ch0 = np.abs(captured[:, 0])
    peak_idx = int(np.argmax(cap_ch0))
    peak_val = float(cap_ch0[peak_idx])
    latency_samples = peak_idx - impulse_pos
    latency_ms = round(latency_samples / samplerate * 1000, 2)

    passed = 0 < latency_ms < MAX_LATENCY_MS

    r = MeasurementResult(
        test_name="L01_impulse",
        device_play=device_info_str(play_dev),
        device_rec=device_info_str(rec_dev),
        samplerate=samplerate,
        channels=channels,
        latency_samples=latency_samples,
        latency_ms=latency_ms,
        passed=passed,
        extra={
            "peak_val": round(peak_val, 6),
            "note": "single-shot, includes stream startup; use L02 for steady-state",
        },
    )

    tag = "PASS" if passed else "FAIL"
    print(f"\n  [{tag}] Latency: {latency_ms} ms ({latency_samples} samples)")
    print(f"    Peak: {peak_val:.4f}")
    print(f"    Note: includes stream startup overhead")

    if out_dir:
        _save(out_dir, "l01", r)

    return r


# ---------------------------------------------------------------------------
# L02: Multi-chirp single-session latency (primary)
# ---------------------------------------------------------------------------

def _build_multi_chirp_buffer(
    samplerate: int,
    channels: int,
    trials: int,
) -> tuple:
    """Build a single play buffer with N chirps spaced by silence.

    Returns (play_buf, chirp_ref, chirp_positions) where chirp_positions
    is a list of sample indices where each chirp starts in play_buf.
    """
    chirp = generate_sweep(
        f_start=CHIRP_F_START, f_end=CHIRP_F_END,
        samplerate=samplerate, duration=CHIRP_DURATION,
        channels=channels, amplitude=CHIRP_AMPLITUDE,
    )
    chirp_len = chirp.shape[0]
    spacing = int(samplerate * CHIRP_SPACING)
    warmup = int(samplerate * WARMUP_SECONDS)
    tail = int(samplerate * TAIL_SECONDS)

    # Total length
    total = warmup + trials * chirp_len + (trials - 1) * spacing + tail
    play_buf = np.zeros((total, channels), dtype=np.float32)

    positions = []
    pos = warmup
    for i in range(trials):
        play_buf[pos : pos + chirp_len] = chirp
        positions.append(pos)
        pos += chirp_len + spacing

    return play_buf, chirp, positions


def run_l02_chirp(
    play_dev: int,
    rec_dev: int,
    samplerate: int = 48000,
    channels: int = 2,
    trials: int = 10,
    out_dir: str | None = None,
) -> MeasurementResult:
    """L02: Multi-chirp latency in a single full-duplex session.

    Embeds N chirps in one play buffer, runs a single playrec call,
    then cross-correlates each chirp region independently to measure
    steady-state latency without stream startup jitter.
    """
    from scipy.signal import correlate

    play_buf, chirp_ref, chirp_positions = _build_multi_chirp_buffer(
        samplerate, channels, trials,
    )
    chirp_len = chirp_ref.shape[0]
    chirp_mono = chirp_ref[:, 0].astype(np.float64)

    print(f"\n{'='*60}")
    print(f" L02: Chirp Latency ({trials} chirps, single session)")
    print(f" Play:   {device_info_str(play_dev)}")
    print(f" Record: {device_info_str(rec_dev)}")
    print(f" Format: {samplerate}Hz / float32 / {channels}ch")
    print(f" Chirp:  {CHIRP_F_START}-{CHIRP_F_END}Hz, {CHIRP_DURATION*1000:.0f}ms")
    print(f" Buffer: {play_buf.shape[0]} samples ({play_buf.shape[0]/samplerate:.2f}s)")
    print(f"{'='*60}")

    captured = loopback(play_buf, play_dev, rec_dev, samplerate, channels)
    cap_mono = captured[:, 0].astype(np.float64)

    # --- Phase 1: find global latency from chirp 1 (wide search) ---
    wide_margin = int(samplerate * 0.25)  # ±250ms for first chirp only
    first_pos = chirp_positions[0]
    win_start = max(0, first_pos)
    win_end = min(len(cap_mono), first_pos + chirp_len + wide_margin)
    cap_window = cap_mono[win_start:win_end]

    corr = correlate(cap_window, chirp_mono, mode="full", method="fft")
    abs_corr = np.abs(corr)
    peak_idx = int(np.argmax(abs_corr))
    lag_in_window = peak_idx - (chirp_len - 1)
    global_latency = win_start + lag_in_window - first_pos

    print(f"\n  Global latency (chirp 1): {global_latency} samples "
          f"({global_latency/samplerate*1000:.2f} ms)")

    # --- Phase 2: narrow search around expected position for each chirp ---
    narrow_margin = int(samplerate * 0.015)  # ±15ms
    trial_results = []

    for i, chirp_pos in enumerate(chirp_positions):
        expected_cap_pos = chirp_pos + global_latency
        win_start = max(0, expected_cap_pos - narrow_margin)
        win_end = min(len(cap_mono), expected_cap_pos + chirp_len + narrow_margin)
        cap_window = cap_mono[win_start:win_end]

        corr = correlate(cap_window, chirp_mono, mode="full", method="fft")
        abs_corr = np.abs(corr)
        peak_idx = int(np.argmax(abs_corr))

        lag_in_window = peak_idx - (chirp_len - 1)
        lag_in_capture = win_start + lag_in_window

        latency_samples = lag_in_capture - chirp_pos
        latency_ms = latency_samples / samplerate * 1000

        # Sidelobe quality
        masked = abs_corr.copy()
        lo = max(0, peak_idx - 50)
        hi = min(len(masked), peak_idx + 51)
        masked[lo:hi] = 0
        sidelobe = float(np.max(masked)) if len(masked) > 0 else 0
        sidelobe_db = 20 * np.log10(abs_corr[peak_idx] / max(sidelobe, 1e-10))

        trial_results.append({
            "latency_samples": latency_samples,
            "latency_ms": latency_ms,
            "sidelobe_db": sidelobe_db,
        })

        print(f"  Chirp {i+1:2d}: {latency_ms:+7.2f} ms "
              f"({latency_samples:5d} samples, sidelobe={sidelobe_db:.1f}dB)")

    latencies_ms = [r["latency_ms"] for r in trial_results]

    # Discard first trial (may still have startup effect)
    steady = latencies_ms[1:] if len(latencies_ms) > 2 else latencies_ms
    lat_mean = float(np.mean(steady))
    lat_std = float(np.std(steady))
    lat_min = float(min(steady))
    lat_max = float(max(steady))
    mean_sidelobe = float(np.mean([r["sidelobe_db"] for r in trial_results[1:]]))

    passed = 0 < lat_mean < MAX_LATENCY_MS and lat_std < MAX_JITTER_MS

    r = MeasurementResult(
        test_name="L02_chirp_latency",
        device_play=device_info_str(play_dev),
        device_rec=device_info_str(rec_dev),
        samplerate=samplerate,
        channels=channels,
        latency_samples=int(round(np.mean([r["latency_samples"] for r in trial_results[1:]]))),
        latency_ms=round(lat_mean, 2),
        passed=passed,
        extra={
            "trials": trials,
            "steady_trials": len(steady),
            "mean_ms": round(lat_mean, 2),
            "std_ms": round(lat_std, 3),
            "min_ms": round(lat_min, 2),
            "max_ms": round(lat_max, 2),
            "first_trial_ms": round(latencies_ms[0], 2),
            "mean_sidelobe_db": round(mean_sidelobe, 1),
            "per_trial_ms": [round(x, 2) for x in latencies_ms],
        },
    )

    tag = "PASS" if passed else "FAIL"
    print(f"\n  [{tag}] Steady-state results (trials 2-{trials}):")
    print(f"    Mean:     {lat_mean:.2f} ms  (threshold: {MAX_LATENCY_MS}ms)")
    print(f"    Std:      {lat_std:.3f} ms  (threshold: {MAX_JITTER_MS}ms)")
    print(f"    Range:    {lat_min:.2f} .. {lat_max:.2f} ms")
    print(f"    Sidelobe: {mean_sidelobe:.1f} dB (mean)")
    if len(latencies_ms) > 1:
        print(f"    Trial 1:  {latencies_ms[0]:.2f} ms (discarded, includes startup)")

    if out_dir:
        _save(out_dir, "l02", r)

    return r


def _save(out_dir, prefix, r):
    os.makedirs(out_dir, exist_ok=True)
    ts = time.strftime("%Y%m%d_%H%M%S")
    save_results_csv([r], os.path.join(out_dir, "results.csv"))
    save_results_json([r], os.path.join(out_dir, f"{prefix}_{ts}.json"))
    print(f"    Saved to {out_dir}/")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    p = argparse.ArgumentParser(
        description="AO Virtual Cable - Round-Trip Latency Test (M4c)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
Examples:
  %(prog)s --list-devices --api wdmks
  %(prog)s --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks
  %(prog)s --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks --trials 10
  %(prog)s --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks --test l02 --out-dir results/
""",
    )
    p.add_argument("--list-devices", action="store_true", help="List audio devices and exit")
    p.add_argument("--api", default=None, help="Filter by host API (wasapi, wdmks, mme, ds)")
    p.add_argument("--play-device", default=None, help="Render device name fragment")
    p.add_argument("--record-device", default=None, help="Capture device name fragment")
    p.add_argument("--samplerate", type=int, default=48000)
    p.add_argument("--channels", type=int, default=2)
    p.add_argument("--trials", type=int, default=10, help="Number of chirp probes (default: 10)")
    p.add_argument("--out-dir", default=None, help="Output directory for CSV/JSON")
    p.add_argument(
        "--test", default="all", choices=["all", "l01", "l02"],
        help="Which test to run (default: all)",
    )
    args = p.parse_args()

    if args.list_devices:
        list_devices(args.api)
        return

    if not args.play_device or not args.record_device:
        p.print_help()
        return

    play_dev = find_device(
        args.play_device, is_input=False,
        api_filter=args.api, min_channels=args.channels,
    )
    if play_dev is None:
        print(f"ERROR: Play device not found: '{args.play_device}' (need >={args.channels}ch output)")
        list_devices(args.api)
        sys.exit(1)

    rec_dev = find_device(
        args.record_device, is_input=True,
        api_filter=args.api, min_channels=args.channels,
    )
    if rec_dev is None:
        print(f"ERROR: Record device not found: '{args.record_device}' (need >={args.channels}ch input)")
        list_devices(args.api)
        sys.exit(1)

    results = []
    run_l01 = args.test in ("all", "l01")
    run_l02 = args.test in ("all", "l02")

    if run_l01:
        results.append(run_l01_impulse(
            play_dev, rec_dev,
            samplerate=args.samplerate,
            channels=args.channels,
            out_dir=args.out_dir,
        ))

    if run_l02:
        results.append(run_l02_chirp(
            play_dev, rec_dev,
            samplerate=args.samplerate,
            channels=args.channels,
            trials=args.trials,
            out_dir=args.out_dir,
        ))

    # Summary
    print(f"\n{'='*60}")
    print(f" Summary")
    print(f"{'='*60}")
    all_pass = True
    for r in results:
        tag = "PASS" if r.passed else "FAIL"
        all_pass = all_pass and r.passed
        ext = r.extra
        if "mean_ms" in ext:
            detail = (f"{ext['mean_ms']:.2f}ms "
                      f"(std={ext['std_ms']:.3f}ms, "
                      f"range={ext['min_ms']:.2f}..{ext['max_ms']:.2f}ms)")
        else:
            detail = f"{r.latency_ms}ms"
        print(f"  [{tag}] {r.test_name}: {detail}")

    print(f"\n  Overall: {'ALL PASS' if all_pass else 'SOME FAILED'}")
    print(f"{'='*60}")
    sys.exit(0 if all_pass else 1)


if __name__ == "__main__":
    main()
