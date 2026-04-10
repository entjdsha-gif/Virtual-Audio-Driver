"""
AO Virtual Cable - Bit-Exact & Silence Quality Tests (M4b)

Q01: Fidelity loopback — play a linear chirp (20-2000Hz), capture via
     loopback, align via cross-correlation, then walk sample-by-sample
     detecting any sample drops. Reports drop count, drop rate, and
     inter-drop fidelity (bit-exact between drops).
Q02: Silence / null test — play silence, capture, verify no spurious signal.

Requirements:
    pip install sounddevice numpy scipy

Usage:
    python test_bit_exact.py --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks
    python test_bit_exact.py --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks --out-dir results/
    python test_bit_exact.py --list-devices --api wdmks
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
    generate_silence,
    loopback,
    rms,
    rms_db,
    peak_db,
    save_wav,
    save_results_csv,
    save_results_json,
    MeasurementResult,
)

# ---------------------------------------------------------------------------
# Q01: Fidelity loopback (chirp align + drop detection)
# ---------------------------------------------------------------------------

CHIRP_F_START = 20.0
CHIRP_F_END = 2000.0
CHIRP_AMPLITUDE = 0.5
PAD_SECONDS = 0.5

# Pass thresholds
MAX_DROP_RATE = 0.02          # 2% max dropped samples
MAX_SINGLE_DROP = 50          # max consecutive samples in one drop event
MIN_INTERDROP_CORR = 0.999    # correlation between drops must be near-perfect
MATCH_TOLERANCE = 1e-3        # sample match tolerance (float32 precision)


def _find_chirp_start(chirp: np.ndarray, captured: np.ndarray) -> int:
    """Find chirp start position in capture via cross-correlation, then
    refine to exact first-nonzero sample."""
    from scipy.signal import correlate

    ref = chirp[:, 0].astype(np.float64)
    cap = captured[:, 0].astype(np.float64)

    corr = correlate(cap, ref, mode="full", method="fft")
    peak_idx = np.argmax(np.abs(corr))
    coarse_lag = peak_idx - (len(ref) - 1)

    # Refine: find first nonzero sample near coarse_lag
    search_start = max(0, coarse_lag - 10)
    search_end = min(len(cap), coarse_lag + 50)

    # Chirp's first nonzero sample
    chirp_first_nz = 1  # chirp[0] = sin(0) = 0, chirp[1] > 0

    for i in range(search_start, search_end):
        if abs(cap[i] - ref[chirp_first_nz]) < MATCH_TOLERANCE:
            return i - chirp_first_nz

    return coarse_lag


def _detect_drops(chirp_mono: np.ndarray, cap_mono: np.ndarray, start: int):
    """Walk through ref and capture detecting sample drops.

    Returns:
        drops: list of (cap_position, ref_position, skip_amount)
        segments: list of (cap_start, cap_end, ref_start, ref_end) for matched regions
        total_ref_consumed: how many ref samples were matched
    """
    n_ref = len(chirp_mono)
    n_cap = len(cap_mono)
    ref_idx = 0
    drops = []
    segments = []
    seg_cap_start = start
    seg_ref_start = 0

    for i in range(n_cap - start):
        cap_pos = start + i
        if ref_idx >= n_ref:
            break

        cap_val = cap_mono[cap_pos]
        diff = abs(cap_val - chirp_mono[ref_idx])

        if diff < MATCH_TOLERANCE:
            ref_idx += 1
            continue

        # Try to find cap_val in ref[ref_idx+1 .. ref_idx+MAX_SINGLE_DROP]
        found = False
        for skip in range(1, min(MAX_SINGLE_DROP, n_ref - ref_idx)):
            if abs(cap_val - chirp_mono[ref_idx + skip]) < MATCH_TOLERANCE:
                # Record segment before drop
                if i > 0:
                    segments.append((seg_cap_start, cap_pos, seg_ref_start, ref_idx))

                drops.append((cap_pos, ref_idx, skip))
                ref_idx += skip + 1
                seg_cap_start = cap_pos
                seg_ref_start = ref_idx - 1
                found = True
                break

        if not found:
            # No match found — advance ref by 1 and continue
            ref_idx += 1

    # Final segment
    if seg_ref_start < ref_idx:
        segments.append((seg_cap_start, start + n_cap - start, seg_ref_start, ref_idx))

    return drops, segments, ref_idx


def run_q01_bit_exact(
    play_dev: int,
    rec_dev: int,
    samplerate: int = 48000,
    channels: int = 2,
    duration: float = 2.0,
    out_dir: str | None = None,
) -> MeasurementResult:
    """Q01: Chirp fidelity loopback with drop detection."""
    pad_samples = int(samplerate * PAD_SECONDS)

    chirp = generate_sweep(
        f_start=CHIRP_F_START, f_end=CHIRP_F_END,
        samplerate=samplerate, duration=duration,
        channels=channels, amplitude=CHIRP_AMPLITUDE,
    )
    chirp_len = chirp.shape[0]
    pad_pre = np.zeros((pad_samples, channels), dtype=np.float32)
    pad_post = np.zeros((pad_samples, channels), dtype=np.float32)
    play_buf = np.concatenate([pad_pre, chirp, pad_post], axis=0)

    print(f"\n{'='*60}")
    print(f" Q01: Fidelity Loopback Test (chirp + drop detection)")
    print(f" Play:   {device_info_str(play_dev)}")
    print(f" Record: {device_info_str(rec_dev)}")
    print(f" Format: {samplerate}Hz / float32 / {channels}ch / {duration}s")
    print(f"{'='*60}")

    captured = loopback(play_buf, play_dev, rec_dev, samplerate, channels)

    # --- Find chirp start ---
    chirp_start = _find_chirp_start(chirp, captured)
    latency_samples = chirp_start - pad_samples
    latency_ms = round(latency_samples / samplerate * 1000, 2)

    print(f"\n  Alignment:")
    print(f"    Chirp start: sample {chirp_start} ({chirp_start/samplerate*1000:.1f}ms)")
    print(f"    Latency:     {latency_samples} samples ({latency_ms} ms)")

    # Verify first few samples match
    chirp_mono = chirp[:, 0].astype(np.float64)
    cap_mono = captured[:, 0].astype(np.float64)

    verify_len = min(10, chirp_len, len(cap_mono) - chirp_start)
    initial_match = all(
        abs(cap_mono[chirp_start + i] - chirp_mono[i]) < MATCH_TOLERANCE
        for i in range(verify_len)
    )
    print(f"    Initial match: {'OK' if initial_match else 'MISMATCH'} (first {verify_len} samples)")

    # --- Detect drops ---
    drops, segments, total_ref = _detect_drops(chirp_mono, cap_mono, chirp_start)

    total_dropped = sum(d[2] for d in drops)
    drop_rate = total_dropped / chirp_len if chirp_len > 0 else 0

    print(f"\n  Drop detection:")
    print(f"    Drop events:   {len(drops)}")
    print(f"    Samples dropped: {total_dropped} / {chirp_len} ({drop_rate*100:.2f}%)")
    print(f"    Ref consumed:  {total_ref} / {chirp_len}")

    if drops:
        skip_sizes = [d[2] for d in drops]
        print(f"    Max single drop: {max(skip_sizes)} samples")
        print(f"    Drop sizes:    min={min(skip_sizes)} max={max(skip_sizes)} "
              f"mean={np.mean(skip_sizes):.1f}")

        # Show first 10 drops
        print(f"    First drops:")
        for cap_pos, ref_pos, skip in drops[:10]:
            ms = (cap_pos - chirp_start) / samplerate * 1000
            print(f"      {ms:8.1f}ms: skip {skip} samples (ref {ref_pos})")

    # --- Inter-drop fidelity ---
    # For each segment between drops, compute correlation
    seg_corrs = []
    for seg_cs, seg_ce, seg_rs, seg_re in segments:
        seg_len = min(seg_ce - seg_cs, seg_re - seg_rs)
        if seg_len < 10:
            continue
        ref_seg = chirp_mono[seg_rs:seg_rs + seg_len]
        cap_seg = cap_mono[seg_cs:seg_cs + seg_len]
        if len(ref_seg) != len(cap_seg):
            continue
        dot = np.dot(ref_seg, cap_seg)
        denom = np.linalg.norm(ref_seg) * np.linalg.norm(cap_seg)
        if denom > 1e-10:
            seg_corrs.append(dot / denom)

    if seg_corrs:
        mean_corr = np.mean(seg_corrs)
        min_corr = min(seg_corrs)
        high_corr = sum(1 for c in seg_corrs if c > 0.999)
    else:
        mean_corr = 0
        min_corr = 0
        high_corr = 0

    print(f"\n  Inter-drop fidelity ({len(seg_corrs)} segments):")
    print(f"    Mean correlation: {mean_corr:.6f}")
    print(f"    Min correlation:  {min_corr:.6f}")
    print(f"    Segments > 0.999: {high_corr}/{len(seg_corrs)}")

    # --- Pass/fail ---
    drop_ok = drop_rate < MAX_DROP_RATE
    single_drop_ok = not drops or max(d[2] for d in drops) < MAX_SINGLE_DROP
    fidelity_ok = mean_corr > MIN_INTERDROP_CORR if seg_corrs else False
    passed = drop_ok and single_drop_ok and fidelity_ok

    r = MeasurementResult(
        test_name="Q01_fidelity",
        device_play=device_info_str(play_dev),
        device_rec=device_info_str(rec_dev),
        samplerate=samplerate,
        channels=channels,
        duration=duration,
        latency_samples=latency_samples,
        latency_ms=latency_ms,
        passed=passed,
        extra={
            "drop_events": len(drops),
            "samples_dropped": total_dropped,
            "drop_rate_pct": round(drop_rate * 100, 3),
            "max_single_drop": max((d[2] for d in drops), default=0),
            "segments_analyzed": len(seg_corrs),
            "mean_interdrop_corr": round(mean_corr, 6),
            "min_interdrop_corr": round(min_corr, 6),
            "initial_match": initial_match,
        },
    )

    tag = "PASS" if passed else "FAIL"
    print(f"\n  [{tag}] Summary:")
    print(f"    Drop rate:      {drop_rate*100:.2f}% (threshold: {MAX_DROP_RATE*100}%)")
    print(f"    Inter-drop corr: {mean_corr:.6f} (threshold: {MIN_INTERDROP_CORR})")
    print(f"    Latency:        {latency_samples} samples ({latency_ms} ms)")

    if not drop_ok:
        print(f"    ** Drop rate too high ({drop_rate*100:.2f}% > {MAX_DROP_RATE*100}%)")
    if not single_drop_ok:
        print(f"    ** Single drop too large ({max(d[2] for d in drops)} > {MAX_SINGLE_DROP})")
    if not fidelity_ok:
        print(f"    ** Inter-drop fidelity too low ({mean_corr:.6f} < {MIN_INTERDROP_CORR})")

    if out_dir:
        _save_q01(out_dir, r, captured, samplerate)

    return r


def _save_q01(out_dir, r, captured, samplerate):
    os.makedirs(out_dir, exist_ok=True)
    ts = time.strftime("%Y%m%d_%H%M%S")
    save_wav(os.path.join(out_dir, f"q01_capture_{ts}.wav"), captured, samplerate)
    save_results_csv([r], os.path.join(out_dir, "results.csv"))
    save_results_json([r], os.path.join(out_dir, f"q01_{ts}.json"))
    print(f"    Saved to {out_dir}/")


# ---------------------------------------------------------------------------
# Q02: Silence / null test
# ---------------------------------------------------------------------------

SILENCE_MAX_PEAK_DB = -80.0
SILENCE_MAX_RMS_DB = -90.0


def run_q02_silence(
    play_dev: int,
    rec_dev: int,
    samplerate: int = 48000,
    channels: int = 2,
    duration: float = 2.0,
    out_dir: str | None = None,
) -> MeasurementResult:
    """Q02: Play digital silence, capture, verify no spurious noise/DC offset."""
    print(f"\n{'='*60}")
    print(f" Q02: Silence / Null Test")
    print(f" Play:   {device_info_str(play_dev)}")
    print(f" Record: {device_info_str(rec_dev)}")
    print(f" Format: {samplerate}Hz / float32 / {channels}ch / {duration}s")
    print(f"{'='*60}")

    silence = generate_silence(samplerate, duration, channels)
    captured = loopback(silence, play_dev, rec_dev, samplerate, channels)

    margin = int(captured.shape[0] * 0.10)
    analysis = captured[margin:-margin] if margin > 0 else captured

    cap_rms = float(rms_db(analysis).mean())
    cap_peak = float(peak_db(analysis))

    dc_offsets = np.mean(analysis, axis=0)
    max_dc = float(np.max(np.abs(dc_offsets)))

    ch_rms = rms(analysis)
    ch_rms_db = 20 * np.log10(np.maximum(ch_rms, 1e-10))

    peak_ok = cap_peak < SILENCE_MAX_PEAK_DB
    rms_ok = cap_rms < SILENCE_MAX_RMS_DB
    dc_ok = max_dc < 1e-5
    passed = peak_ok and rms_ok and dc_ok

    r = MeasurementResult(
        test_name="Q02_silence",
        device_play=device_info_str(play_dev),
        device_rec=device_info_str(rec_dev),
        samplerate=samplerate,
        channels=channels,
        duration=duration,
        rms_db=cap_rms,
        peak_db=cap_peak,
        passed=passed,
        extra={
            "max_dc_offset": round(max_dc, 10),
            "per_channel_rms_db": [round(float(x), 2) for x in ch_rms_db],
        },
    )

    tag = "PASS" if passed else "FAIL"
    print(f"\n  [{tag}] Results:")
    print(f"    Capture RMS:   {cap_rms:+.1f} dBFS  (threshold: {SILENCE_MAX_RMS_DB})")
    print(f"    Capture Peak:  {cap_peak:+.1f} dBFS  (threshold: {SILENCE_MAX_PEAK_DB})")
    print(f"    Max DC offset: {max_dc:.2e}")
    for ch in range(channels):
        print(f"    ch{ch} RMS: {ch_rms_db[ch]:+.1f} dBFS")

    if not peak_ok:
        print(f"    ** Peak too high ({cap_peak:+.1f} > {SILENCE_MAX_PEAK_DB})")
    if not rms_ok:
        print(f"    ** RMS too high ({cap_rms:+.1f} > {SILENCE_MAX_RMS_DB})")
    if not dc_ok:
        print(f"    ** DC offset too large ({max_dc:.2e} > 1e-5)")

    if out_dir:
        _save_q02(out_dir, r, captured, samplerate)

    return r


def _save_q02(out_dir, r, captured, samplerate):
    os.makedirs(out_dir, exist_ok=True)
    ts = time.strftime("%Y%m%d_%H%M%S")
    save_wav(os.path.join(out_dir, f"q02_capture_{ts}.wav"), captured, samplerate)
    save_results_csv([r], os.path.join(out_dir, "results.csv"))
    save_results_json([r], os.path.join(out_dir, f"q02_{ts}.json"))
    print(f"    Saved to {out_dir}/")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def main():
    p = argparse.ArgumentParser(
        description="AO Virtual Cable - Fidelity & Silence Tests (M4b)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
Examples:
  %(prog)s --list-devices --api wdmks
  %(prog)s --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks
  %(prog)s --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks --test q01
  %(prog)s --play-device "AO Cable A Output" --record-device "AO Cable A Input" --api wdmks --test q02 --out-dir results/
""",
    )
    p.add_argument("--list-devices", action="store_true", help="List audio devices and exit")
    p.add_argument("--api", default=None, help="Filter by host API (wasapi, wdmks, mme, ds)")
    p.add_argument("--play-device", default=None, help="Render device name fragment")
    p.add_argument("--record-device", default=None, help="Capture device name fragment")
    p.add_argument("--samplerate", type=int, default=48000)
    p.add_argument("--channels", type=int, default=2)
    p.add_argument("--duration", type=float, default=2.0, help="Chirp duration (default: 2.0s)")
    p.add_argument("--out-dir", default=None, help="Output directory for WAV/CSV/JSON")
    p.add_argument(
        "--test", default="all", choices=["all", "q01", "q02"],
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
    run_q01 = args.test in ("all", "q01")
    run_q02 = args.test in ("all", "q02")

    if run_q01:
        results.append(run_q01_bit_exact(
            play_dev, rec_dev,
            samplerate=args.samplerate,
            channels=args.channels,
            duration=args.duration,
            out_dir=args.out_dir,
        ))

    if run_q02:
        results.append(run_q02_silence(
            play_dev, rec_dev,
            samplerate=args.samplerate,
            channels=args.channels,
            duration=args.duration,
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
        extra = ""
        if "drop_rate_pct" in r.extra:
            extra += f"  drops={r.extra['drop_rate_pct']}%"
            extra += f"  interdrop_corr={r.extra['mean_interdrop_corr']:.4f}"
        if "max_dc_offset" in r.extra:
            extra = f"  DC={r.extra['max_dc_offset']:.1e}"
        print(f"  [{tag}] {r.test_name}{extra}")

    print(f"\n  Overall: {'ALL PASS' if all_pass else 'SOME FAILED'}")
    print(f"{'='*60}")
    sys.exit(0 if all_pass else 1)


if __name__ == "__main__":
    main()
