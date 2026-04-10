"""
AO Virtual Cable vs VB-Cable - Baseline Comparison (M4e)

Runs the same test suite on both AO and VB-Cable, then prints a
side-by-side comparison table.

Tests included:
  - Q02 Silence/null
  - L02 Chirp latency (multi-chirp single session)
  - Dropout detection (configurable duration)
  - Clock drift measurement (configurable duration)

Requirements:
    pip install sounddevice numpy scipy

Usage:
    python test_compare_vb.py --api wdmks
    python test_compare_vb.py --api wdmks --duration 60 --out-dir results/
    python test_compare_vb.py --api wdmks --ao-play "AO Cable A Output" --ao-rec "AO Cable A Input" --vb-play "VB-Audio Point A" --vb-rec "CABLE-A Output"
    python test_compare_vb.py --list-devices --api wdmks
"""

import argparse
import os
import sys
import time

from test_quality_common import (
    find_device,
    device_info_str,
    list_devices,
    save_results_json,
)

from test_bit_exact import run_q02_silence
from test_latency import run_l02_chirp
from test_dropout import run_dropout_test
from test_drift import run_drift_test

def _sanitize(obj):
    """Convert numpy scalars/bools to Python natives for JSON serialization."""
    import numpy as np
    if isinstance(obj, dict):
        return {k: _sanitize(v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple)):
        return [_sanitize(v) for v in obj]
    if isinstance(obj, (np.bool_, bool)):
        return bool(obj)
    if isinstance(obj, (np.integer,)):
        return int(obj)
    if isinstance(obj, (np.floating,)):
        return float(obj)
    return obj


# Default device name fragments
DEFAULT_AO_PLAY = "AO Cable A Output"
DEFAULT_AO_REC = "AO Cable A Input"
DEFAULT_VB_PLAY = "VB-Audio Point A"
DEFAULT_VB_REC = "CABLE-A Output"


def _find_or_fail(name, is_input, api, channels, label):
    idx = find_device(name, is_input=is_input, api_filter=api, min_channels=channels)
    if idx is None:
        print(f"ERROR: {label} device not found: '{name}' (need >={channels}ch)")
        return None
    return idx


def _run_suite(label, play_dev, rec_dev, samplerate, channels, latency_trials,
               test_duration, out_dir):
    """Run all tests for one cable. Returns dict of results."""
    sub_dir = os.path.join(out_dir, label) if out_dir else None

    print(f"\n{'#'*60}")
    print(f" {label}")
    print(f" Play:   {device_info_str(play_dev)}")
    print(f" Record: {device_info_str(rec_dev)}")
    print(f"{'#'*60}")

    results = {}

    # Q02 Silence
    results["silence"] = run_q02_silence(
        play_dev, rec_dev, samplerate, channels, duration=2.0, out_dir=sub_dir,
    )

    # L02 Latency
    results["latency"] = run_l02_chirp(
        play_dev, rec_dev, samplerate, channels,
        trials=latency_trials, out_dir=sub_dir,
    )

    # Dropout
    results["dropout"] = run_dropout_test(
        play_dev, rec_dev, samplerate, channels,
        duration=test_duration, out_dir=sub_dir,
    )

    # Drift
    results["drift"] = run_drift_test(
        play_dev, rec_dev, samplerate, channels,
        duration=test_duration, out_dir=sub_dir,
    )

    return results


def _print_comparison(ao_results, vb_results):
    """Print side-by-side comparison table."""
    print(f"\n{'='*72}")
    print(f" AO Virtual Cable vs VB-Cable - Comparison")
    print(f"{'='*72}")

    rows = []

    # Silence
    ao_s = ao_results["silence"]
    vb_s = vb_results["silence"]
    rows.append(("Silence RMS (dBFS)",
                 f"{ao_s.rms_db:+.1f}", f"{vb_s.rms_db:+.1f}",
                 "lower=better"))
    rows.append(("Silence Peak (dBFS)",
                 f"{ao_s.peak_db:+.1f}", f"{vb_s.peak_db:+.1f}",
                 "lower=better"))
    rows.append(("Silence PASS",
                 "PASS" if ao_s.passed else "FAIL",
                 "PASS" if vb_s.passed else "FAIL", ""))

    # Latency
    ao_l = ao_results["latency"]
    vb_l = vb_results["latency"]
    rows.append(("Latency mean (ms)",
                 f"{ao_l.extra['mean_ms']:.2f}",
                 f"{vb_l.extra['mean_ms']:.2f}",
                 "lower=better"))
    rows.append(("Latency std (ms)",
                 f"{ao_l.extra['std_ms']:.3f}",
                 f"{vb_l.extra['std_ms']:.3f}",
                 "lower=better"))
    rows.append(("Latency range (ms)",
                 f"{ao_l.extra['min_ms']:.1f}..{ao_l.extra['max_ms']:.1f}",
                 f"{vb_l.extra['min_ms']:.1f}..{vb_l.extra['max_ms']:.1f}",
                 ""))
    rows.append(("Latency PASS",
                 "PASS" if ao_l.passed else "FAIL",
                 "PASS" if vb_l.passed else "FAIL", ""))

    # Dropout
    ao_d = ao_results["dropout"]
    vb_d = vb_results["dropout"]
    rows.append(("Dropouts",
                 str(ao_d.dropout_count), str(vb_d.dropout_count),
                 "0=best"))
    rows.append(("Dropout total (ms)",
                 f"{ao_d.extra['total_dropout_ms']:.1f}",
                 f"{vb_d.extra['total_dropout_ms']:.1f}",
                 "lower=better"))
    rows.append(("Dropout longest (ms)",
                 f"{ao_d.extra['longest_dropout_ms']:.1f}",
                 f"{vb_d.extra['longest_dropout_ms']:.1f}",
                 "lower=better"))
    rows.append(("Dropout PASS",
                 "PASS" if ao_d.passed else "FAIL",
                 "PASS" if vb_d.passed else "FAIL", ""))

    # Drift
    ao_dr = ao_results["drift"]
    vb_dr = vb_results["drift"]
    rows.append(("Drift (ms/hour)",
                 f"{ao_dr.extra['drift_ms_per_hour']:+.1f}",
                 f"{vb_dr.extra['drift_ms_per_hour']:+.1f}",
                 "closer to 0=better"))
    rows.append(("Drift (ppm)",
                 f"{ao_dr.extra['drift_rate_ppm']:+.1f}",
                 f"{vb_dr.extra['drift_rate_ppm']:+.1f}",
                 "closer to 0=better"))
    rows.append(("Drift jitter (ms)",
                 f"{ao_dr.extra['jitter_ms']:.3f}",
                 f"{vb_dr.extra['jitter_ms']:.3f}",
                 "lower=better"))
    rows.append(("Drift PASS",
                 "PASS" if ao_dr.passed else "FAIL",
                 "PASS" if vb_dr.passed else "FAIL", ""))

    # Print table
    w_metric = max(len(r[0]) for r in rows)
    w_val = 16
    header = f"  {'Metric':<{w_metric}}  {'AO Cable':>{w_val}}  {'VB-Cable':>{w_val}}  Note"
    print(header)
    print(f"  {'-'*(w_metric + 2*w_val + 10)}")

    for metric, ao_val, vb_val, note in rows:
        # Highlight winner
        marker = ""
        if note and "better" in note:
            try:
                a = float(ao_val.replace("+", ""))
                v = float(vb_val.replace("+", ""))
                if "lower" in note:
                    marker = " <<" if a < v else (" >>" if v < a else "")
                elif "closer to 0" in note:
                    marker = " <<" if abs(a) < abs(v) else (" >>" if abs(v) < abs(a) else "")
            except ValueError:
                pass
        print(f"  {metric:<{w_metric}}  {ao_val:>{w_val}}  {vb_val:>{w_val}}  {note}{marker}")

    # Overall score
    ao_pass = sum(1 for k in ["silence", "latency", "dropout", "drift"]
                  if ao_results[k].passed)
    vb_pass = sum(1 for k in ["silence", "latency", "dropout", "drift"]
                  if vb_results[k].passed)
    print(f"\n  Overall: AO {ao_pass}/4 PASS  |  VB {vb_pass}/4 PASS")
    print(f"{'='*72}")


def main():
    p = argparse.ArgumentParser(
        description="AO vs VB-Cable Baseline Comparison (M4e)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
Examples:
  %(prog)s --api wdmks
  %(prog)s --api wdmks --duration 60 --out-dir results/
  %(prog)s --list-devices --api wdmks
""",
    )
    p.add_argument("--list-devices", action="store_true")
    p.add_argument("--api", default="wdmks", help="Host API filter (default: wdmks)")
    p.add_argument("--ao-play", default=DEFAULT_AO_PLAY)
    p.add_argument("--ao-rec", default=DEFAULT_AO_REC)
    p.add_argument("--vb-play", default=DEFAULT_VB_PLAY)
    p.add_argument("--vb-rec", default=DEFAULT_VB_REC)
    p.add_argument("--samplerate", type=int, default=48000)
    p.add_argument("--channels", type=int, default=2)
    p.add_argument("--latency-trials", type=int, default=10)
    p.add_argument("--duration", type=float, default=30.0,
                   help="Duration for dropout/drift tests in seconds (default: 30)")
    p.add_argument("--out-dir", default=None)
    args = p.parse_args()

    if args.list_devices:
        list_devices(args.api)
        return

    # Resolve devices
    ao_play = _find_or_fail(args.ao_play, False, args.api, args.channels, "AO play")
    ao_rec = _find_or_fail(args.ao_rec, True, args.api, args.channels, "AO record")
    vb_play = _find_or_fail(args.vb_play, False, args.api, args.channels, "VB play")
    vb_rec = _find_or_fail(args.vb_rec, True, args.api, args.channels, "VB record")

    if any(d is None for d in [ao_play, ao_rec, vb_play, vb_rec]):
        print("\nAvailable devices:")
        list_devices(args.api)
        sys.exit(1)

    print(f"\n{'*'*72}")
    print(f" AO Virtual Cable vs VB-Cable - Baseline Comparison")
    print(f" Format:   {args.samplerate}Hz / float32 / {args.channels}ch")
    print(f" Duration: {args.duration}s (dropout/drift)")
    print(f" Latency:  {args.latency_trials} chirps")
    print(f"{'*'*72}")

    # Run AO suite
    ao_results = _run_suite(
        "AO Cable A", ao_play, ao_rec,
        args.samplerate, args.channels, args.latency_trials,
        args.duration, args.out_dir,
    )

    # Run VB suite
    vb_results = _run_suite(
        "VB-Cable A", vb_play, vb_rec,
        args.samplerate, args.channels, args.latency_trials,
        args.duration, args.out_dir,
    )

    # Comparison
    _print_comparison(ao_results, vb_results)

    # Save combined results
    if args.out_dir:
        os.makedirs(args.out_dir, exist_ok=True)
        ts = time.strftime("%Y%m%d_%H%M%S")
        combined = {
            "timestamp": ts,
            "format": f"{args.samplerate}Hz/float32/{args.channels}ch",
            "ao": {k: {"passed": bool(v.passed), "extra": _sanitize(v.extra)}
                   for k, v in ao_results.items()},
            "vb": {k: {"passed": bool(v.passed), "extra": _sanitize(v.extra)}
                   for k, v in vb_results.items()},
        }
        import json
        path = os.path.join(args.out_dir, f"compare_{ts}.json")
        with open(path, "w", encoding="utf-8") as f:
            json.dump(combined, f, indent=2, ensure_ascii=False)
        print(f"\n  Combined results: {path}")

    # Exit code: 0 = comparison completed successfully (regardless of pass/fail)
    sys.exit(0)


if __name__ == "__main__":
    main()
