"""
AO Virtual Cable - Dropout Detection Test (M4d)

Plays a continuous 1kHz sine via loopback, then scans the capture for
dropout events (windows where RMS drops below threshold).

Requirements:
    pip install sounddevice numpy scipy

Usage:
    python test_dropout.py --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks
    python test_dropout.py --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks --duration 600
    python test_dropout.py --list-devices --api wdmks
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
    generate_sine,
    loopback,
    cross_correlate,
    rms_db,
    peak_db,
    count_dropouts,
    save_wav,
    save_results_csv,
    save_results_json,
    MeasurementResult,
)

# Thresholds
MAX_DROPOUT_COUNT = 0       # zero dropouts for PASS
MAX_DROPOUT_TOTAL_MS = 0.0
DROPOUT_WINDOW_MS = 5.0     # analysis window size
DROPOUT_THRESHOLD_DB = -40.0  # signal expected ~-6dBFS, dropout if below this
SIGNAL_MIN_DB = -20.0       # minimum expected signal level

# Settle: skip first/last N seconds from analysis
SETTLE_SECONDS = 0.5


def run_dropout_test(
    play_dev: int,
    rec_dev: int,
    samplerate: int = 48000,
    channels: int = 2,
    duration: float = 60.0,
    out_dir: str | None = None,
) -> MeasurementResult:
    """Play continuous 1kHz sine, capture, scan for dropouts."""
    print(f"\n{'='*60}")
    print(f" Dropout Detection Test")
    print(f" Play:     {device_info_str(play_dev)}")
    print(f" Record:   {device_info_str(rec_dev)}")
    print(f" Format:   {samplerate}Hz / float32 / {channels}ch")
    print(f" Duration: {duration}s")
    print(f"{'='*60}")

    tone = generate_sine(1000.0, samplerate, duration, channels, amplitude=0.5)

    print(f"  Playing + recording {duration}s ...")
    t0 = time.time()
    captured = loopback(tone, play_dev, rec_dev, samplerate, channels)
    elapsed = time.time() - t0
    print(f"  Done in {elapsed:.1f}s")

    # Trim settle period
    settle = int(samplerate * SETTLE_SECONDS)
    analysis = captured[settle : captured.shape[0] - settle]

    # Overall signal check
    sig_rms = float(rms_db(analysis).mean())
    sig_peak = float(peak_db(analysis))
    print(f"\n  Signal: RMS={sig_rms:+.1f}dBFS  Peak={sig_peak:+.1f}dBFS")

    if sig_rms < SIGNAL_MIN_DB:
        print(f"  WARNING: signal too quiet ({sig_rms:.1f} < {SIGNAL_MIN_DB}), results unreliable")

    # Dropout detection
    dropouts = count_dropouts(
        analysis,
        samplerate=samplerate,
        window_ms=DROPOUT_WINDOW_MS,
        threshold_db=DROPOUT_THRESHOLD_DB,
        min_expected_db=SIGNAL_MIN_DB,
        margin_windows=3,
    )

    total_dropout_samples = sum(d[1] for d in dropouts)
    total_dropout_ms = total_dropout_samples / samplerate * 1000
    longest_samples = max((d[1] for d in dropouts), default=0)
    longest_ms = longest_samples / samplerate * 1000

    print(f"\n  Dropout analysis (window={DROPOUT_WINDOW_MS}ms, threshold={DROPOUT_THRESHOLD_DB}dB):")
    print(f"    Count:    {len(dropouts)}")
    print(f"    Total:    {total_dropout_ms:.1f} ms")
    print(f"    Longest:  {longest_ms:.1f} ms")

    if dropouts:
        print(f"    Events:")
        for i, (start, dur_samp) in enumerate(dropouts[:20]):
            t_ms = (start + settle) / samplerate * 1000
            d_ms = dur_samp / samplerate * 1000
            print(f"      #{i+1}: {t_ms:.1f}ms  duration={d_ms:.1f}ms")
        if len(dropouts) > 20:
            print(f"      ... and {len(dropouts) - 20} more")

    passed = len(dropouts) <= MAX_DROPOUT_COUNT

    r = MeasurementResult(
        test_name="dropout_test",
        device_play=device_info_str(play_dev),
        device_rec=device_info_str(rec_dev),
        samplerate=samplerate,
        channels=channels,
        duration=duration,
        rms_db=sig_rms,
        peak_db=sig_peak,
        dropout_count=len(dropouts),
        passed=passed,
        extra={
            "total_dropout_ms": round(total_dropout_ms, 2),
            "longest_dropout_ms": round(longest_ms, 2),
            "window_ms": DROPOUT_WINDOW_MS,
            "threshold_db": DROPOUT_THRESHOLD_DB,
            "settle_seconds": SETTLE_SECONDS,
        },
    )

    tag = "PASS" if passed else "FAIL"
    print(f"\n  [{tag}] {len(dropouts)} dropout(s) in {duration}s")

    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
        ts = time.strftime("%Y%m%d_%H%M%S")
        save_results_csv([r], os.path.join(out_dir, "results.csv"))
        save_results_json([r], os.path.join(out_dir, f"dropout_{ts}.json"))
        if duration <= 120:
            save_wav(os.path.join(out_dir, f"dropout_capture_{ts}.wav"), captured, samplerate)
        print(f"    Saved to {out_dir}/")

    return r


def main():
    p = argparse.ArgumentParser(
        description="AO Virtual Cable - Dropout Detection Test (M4d)",
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

    result = run_dropout_test(
        play_dev, rec_dev,
        samplerate=args.samplerate,
        channels=args.channels,
        duration=args.duration,
        out_dir=args.out_dir,
    )
    sys.exit(0 if result.passed else 1)


if __name__ == "__main__":
    main()
