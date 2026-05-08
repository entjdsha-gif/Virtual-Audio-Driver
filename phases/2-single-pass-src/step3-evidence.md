# Phase 2 Step 3 -- Round-trip Rate Tests (Evidence)

Status: PASS
Date: 2026-05-08
Branch: `phase/2-single-pass-src`
Spec commit: `e1f996d` (`phase2/docs: rewrite step3.md round-trip
contract verification`)
Test driver: `tests/phase2-runtime/round_trip_rate_test.py` (untracked)
Auditor: Claude (execution agent)
Review: pending Codex

This document records the Step 3 deliverable. It is the tracked
evidence companion to the untracked
`tests/phase2-runtime/round_trip_results.json` artifact, written so
that "what was verified" and "what is NOT claimed" are preserved on
the integration line independently of the untracked test driver.

## 1. Scope

Step 3 validates the SRC framing / contract of Phase 2 Steps 1
(`AoRingWriteFromScratch` write SRC) and 2 (`AoRingReadToScratch`
read SRC) end-to-end. PASS / FAIL is judged ONLY against the
contract gates in `step3.md`; SNR / max-abs-error are recorded as
non-gating diagnostics. SRC linear interpolation is lossy by
definition; this step does NOT claim any fidelity quality
threshold.

## 2. Test driver

`tests/phase2-runtime/round_trip_rate_test.py` is a chunk-driven
end-to-end round-trip simulator that mirrors the kernel SRC paths
in Python and verifies:

- The doc support matrix (`step3.md` "Resulting support matrix")
  agrees with the Step 0 helper `gcd_helper_equiv.pick_gcd_divisor`
  output for every (internal, client) cell, in both directions
  -- a "matrix-vs-helper drift" check.
- For each supported `(internal_rate, client_rate)` cell, every
  contract gate from `step3.md` "Per supported pair" holds across
  a 1-second 1-kHz sine round trip:
    1. all chunked write calls return `STATUS_SUCCESS`;
    2. all chunked read calls return `STATUS_SUCCESS`;
    3. output is non-silent across the run;
    4. output values are finite and bounded to PCM range;
    5. `OverflowCounter == 0` at end of run;
    6. `UnderrunCounter == 0` at end of run;
    7. `UnderrunFlag == 0` at end of run;
    8. cumulative `WritePos`-advance matches the helper-equivalent
       expected count `(input_frames * dr_w / sr_w)`;
    9. cumulative `ReadPos`-advance matches the helper-equivalent
       expected count `(output_frames * sr_r / dr_r)`.
- For each unsupported pair, BOTH directions (write at unsupported
  AND read at unsupported) return `STATUS_NOT_SUPPORTED` on the
  first SRC call with a byte-identical pre/post pipe state
  snapshot (no state mutation).

## 3. Driver structure

Each supported-pair run uses a warm-up + steady-state pattern:

```text
make_pipe(internal_rate, channels=2, wrap_bound=4096)

Generate input sine at client_rate (1 kHz, 1 second).

Warm-up loop:
  while available_frames(pipe) < (WrapBound/2 + chunk_size):
    write one chunk (~10 ms at client_rate).
  -- This pre-fills the ring above the hysteresis recovery
     threshold so the first steady-state read does not trip a
     legitimate startup underrun on rate pairs where
     ringFramesNeeded64 == consumedBeforeLastOutput at the chunk
     boundary (e.g. 48k -> 22.05k -> 48k).

Steady-state loop:
  while output_so_far < total_frames:
    if input frames remain: write one chunk.
    read one chunk.
```

The warm-up is a TEST DRIVER concession, not an algorithmic concern.
Production drivers prime their rings before the first read pair
serves audio data; the simulator does the same. WrapBound = 4096
keeps the warmup bounded across all ADR-008 rate pairs (the highest
Y-per-chunk rate, 48k client / 8k internal, fills 32 chunks at 80
ring frames per chunk to clear the threshold).

## 4. What was verified (PASS)

### 4.1 Helper-equivalent matrix cross-check

100 / 100 (internal, client) cells from ADR-008's 10-rate set agree
between the doc matrix in `step3.md` and `pick_gcd_divisor` output
in BOTH directions
(`pick_gcd_divisor(client, internal)` and
`pick_gcd_divisor(internal, client)`). No matrix-vs-helper drift.

### 4.2 Supported pair contract gates

All 8 representative supported pairs from `step3.md` "Supported
round-trip examples" PASS every contract gate:

| Round-trip | warmup chunks | write_adv (matches expected) | read_adv (matches expected) | counters_zero | non_silent | finite_bounded |
|---|---:|---:|---:|---|---|---|
| `48k -> 44.1k -> 48k`     |  6 | 46746 / 46746 | 44100 / 44100 | Y | Y | Y |
| `44.1k -> 48k -> 44.1k`   |  6 | 50880 / 50880 | 48000 / 48000 | Y | Y | Y |
| `48k -> 96k -> 48k`       |  3 | 98880 / 98880 | 96000 / 96000 | Y | Y | Y |
| `96k -> 48k -> 96k`       |  7 | 51360 / 51360 | 48000 / 48000 | Y | Y | Y |
| `48k -> 8k -> 48k`        | 32 | 10400 / 10400 |  8000 /  8000 | Y | Y | Y |
| `8k -> 48k -> 8k`         |  5 | 50400 / 50400 | 48000 / 48000 | Y | Y | Y |
| `48k -> 22.05k -> 48k`    | 12 | 24696 / 24696 | 22050 / 22050 | Y | Y | Y |
| `48k -> 48k -> 48k` (same-rate) | 6 | 50880 / 50880 | 48000 / 48000 | Y | Y | Y |

`counters_zero` includes `OverflowCounter == 0`, `UnderrunCounter
== 0`, and `UnderrunFlag == 0` at end of run. Helper-equivalent
expected counts match `write_advances_total` and
`read_advances_total` exactly for every representative pair. The
test driver records any phase-carry delta separately; no delta was
needed for this PASS run.

### 4.3 Unsupported pair gates

All 7 unsupported pairs PASS in BOTH directions:

| pair | direction | status | state_unchanged |
|---|---|---|---|
| `8k <-> 22.05k` (internal=22050, client=8000)   | write | `STATUS_NOT_SUPPORTED` | Y |
| `8k <-> 22.05k` (internal=22050, client=8000)   | read  | `STATUS_NOT_SUPPORTED` | Y |
| `16k <-> 22.05k` (internal=22050, client=16000) | write | `STATUS_NOT_SUPPORTED` | Y |
| `16k <-> 22.05k` (internal=22050, client=16000) | read  | `STATUS_NOT_SUPPORTED` | Y |
| `32k <-> 22.05k` (internal=22050, client=32000) | write | `STATUS_NOT_SUPPORTED` | Y |
| `32k <-> 22.05k` (internal=22050, client=32000) | read  | `STATUS_NOT_SUPPORTED` | Y |
| `8k <-> 22.05k` mirror (internal=8000, client=22050)   | write | `STATUS_NOT_SUPPORTED` | Y |
| `8k <-> 22.05k` mirror (internal=8000, client=22050)   | read  | `STATUS_NOT_SUPPORTED` | Y |
| `16k <-> 22.05k` mirror (internal=16000, client=22050) | write | `STATUS_NOT_SUPPORTED` | Y |
| `16k <-> 22.05k` mirror (internal=16000, client=22050) | read  | `STATUS_NOT_SUPPORTED` | Y |
| `32k <-> 22.05k` mirror (internal=32000, client=22050) | write | `STATUS_NOT_SUPPORTED` | Y |
| `32k <-> 22.05k` mirror (internal=32000, client=22050) | read  | `STATUS_NOT_SUPPORTED` | Y |
| `12345` helper-level (internal=48000, client=12345)    | write | `STATUS_NOT_SUPPORTED` | Y |
| `12345` helper-level (internal=48000, client=12345)    | read  | `STATUS_NOT_SUPPORTED` | Y |

`state_unchanged = Y` means the full pipe snapshot
(`ReadPos`, `WritePos`, `OverflowCounter`, `UnderrunCounter`,
`UnderrunFlag`, `WriteSrcPhase`, every channel of
`WriteSrcResidual[]`, `ReadSrcPhase`, every channel of
`ReadSrcResidual[]`, and the entire `Data[]` ring) is byte-identical
across the call. Pre-call snapshot vs post-call snapshot match.

## 5. What is NOT claimed

- **No fidelity / quality assertion**. SRC linear interpolation is
  lossy; the diagnostic `max_abs_amp_19bit` field in
  `round_trip_results.json` records steady-state output amplitude
  but is non-gating. No SNR threshold. No max-error threshold.
  Quality tuning of the resampler is Phase 7 territory and is
  explicitly out of scope here.
- **No bit-exact identity for SRC pairs**. Same-rate identity
  (where the SRC branch is bypassed entirely) is verified
  separately by the Phase 1 Step 2 / Step 3 acceptance criteria;
  this Step 3 evidence does not re-derive that contract.
- **No claim about kernel-side execution**. The test driver runs
  in user mode against a Python mirror of the SRC algorithms.
  Cross-check against the actual kernel build is provided by the
  Phase 1 / Phase 2 same-branch build verification (MSBuild +
  signing) and by the Step 0 / 1 / 2 Python equivalence tests
  (`gcd_helper_equiv.py`, `write_src_equiv.py`,
  `read_src_equiv.py`); together these provide reviewed
  algorithm-equivalence coverage for the specific contracts under
  test, but this remains a user-mode mirror rather than a direct
  kernel call test.
- **No live-call evidence**. End-to-end live audio (PortCls /
  WaveRT / WASAPI / Phone Link) is the responsibility of Phases
  4-7 (cable transport ownership flip + live-call validation).
  Step 3 verifies algorithmic contract, not platform integration.
- **No claim about WrapBound = 96000 production behavior**. The
  test uses `wrap_bound = 4096` to keep warm-up bounded. The
  contract gates (status codes, state invariants, count ratios)
  are independent of `WrapBound`; only the warm-up chunk count
  scales with it.

## 6. Evidence locations

| File | Tracked? | Content |
|---|---|---|
| `phases/2-single-pass-src/step3-evidence.md` (this file) | yes | Step 3 PASS summary, what was verified, what is not claimed. |
| `phases/2-single-pass-src/step3.md` | yes | Step 3 contract spec (commit `e1f996d`). |
| `tests/phase2-runtime/round_trip_rate_test.py` | no | Test driver. |
| `tests/phase2-runtime/round_trip_results.json` | no | Per-cell results record (status flags, counts, optional diagnostics). |
| `tests/phase2-runtime/gcd_helper_equiv.py` | no | Step 0 helper Python mirror, used for the matrix cross-check. |
| `tests/phase2-runtime/write_src_equiv.py` | no | Step 1 write SRC Python mirror; relevant Step 1 regression already PASSes (21 / 21). |
| `tests/phase2-runtime/read_src_equiv.py` | no | Step 2 read SRC Python mirror; relevant Step 2 regression already PASSes (21 / 21). |

## 7. Step 1 / Step 2 regression status

Step 3 does not introduce new state contracts; it re-verifies in an
end-to-end setting the contracts established by Steps 1 and 2. The
narrow Step 1 / Step 2 Python regressions remain authoritative for
their respective edge cases:

| Edge case | Owning step | Tested in |
|---|---|---|
| ADR-005 hard-reject overflow (write) | Step 1 | `write_src_equiv.test_overflow_invariance` |
| ADR-005 hysteretic underrun (read) | Step 2 | `read_src_equiv.test_hard_underrun_state_invariance`, `test_hysteresis_stay`, `test_hysteresis_exit` |
| Hostile srcChannels reject (write side, option A) | Step 1 | `write_src_equiv.test_channel_reject`, `test_hostile_inputs` |
| Hostile dstChannels reject (read side, option B hybrid) | Step 2 | `read_src_equiv.test_hostile_dst_channels` |
| Phase-aware capacity / lookahead (write) | Step 1 | `write_src_equiv.test_emit_count_matches_capacity` |
| Phase-aware capacity / lookahead (read), `consumedBeforeLastOutput` BLOCKER fix | Step 2 | `read_src_equiv.test_no_post_final_consume_read`, `test_trailing_output_lookahead`, `test_zero_consume_lookahead` |
| Multi-consume reload (BLOCKER 1) | Step 2 | `read_src_equiv.test_mid_consume_reload_regression` |
| Cast-wrap protection (huge frames, hostile srcChannels) | Step 1 / 2 | `write_src_equiv.test_hostile_inputs`, `read_src_equiv.test_huge_frames_cast_wrap` |

The Step 3 driver does not re-run these regressions inline; running
the per-step tests as a combined pre-flight before round-trip is
recommended but not required for Step 3 PASS.

## 8. Diagnostic observations (NON-GATING)

`diag_max_abs_amp_19bit` recorded in `round_trip_results.json`:

| Round-trip | diag_max_abs_amp_19bit |
|---|---:|
| `48k -> 44.1k -> 48k`     | 131055 |
| `44.1k -> 48k -> 44.1k`   | 131047 |
| `48k -> 96k -> 48k`       | 131072 |
| `96k -> 48k -> 96k`       | 130791 |
| `48k -> 8k -> 48k`        | 129950 |
| `8k -> 48k -> 8k`         | 131072 |
| `48k -> 22.05k -> 48k`    | 131047 |
| `48k -> 48k -> 48k`       | 131072 |

Input sine magnitude was `2^17 = 131072` (half of 19-bit range,
leaving headroom). Output amplitudes hover near 131072 across all
pairs, indicating the round trip preserves signal magnitude without
saturation. The single below-magnitude case (48k -> 8k -> 48k at
129950) is consistent with linear-interpolation amplitude rolloff at
extreme downsample-then-upsample ratios -- this is expected
algorithmic behavior, not a defect.

These numbers are recorded for inspection only and do NOT affect
Step 3 PASS / FAIL.

## 9. STOP conditions encountered during work

None. The only driver pattern adjustment (warm-up phase to
pre-fill the ring above the hysteresis threshold before the first
read) is a test-side simulation of how production drivers prime
their rings; no algorithm change was needed to make any contract
gate pass.

## 10. Summary

| | |
|---|---|
| Helper-equivalent cross-check | 100 / 100 cells PASS |
| Supported pair contract gates | 8 / 8 cells PASS, every gate green |
| Unsupported pair gates (write + read) | 7 pairs x 2 directions = 14 / 14 PASS |
| `OverflowCounter` across all supported runs | 0 |
| `UnderrunCounter` across all supported runs | 0 |
| `UnderrunFlag` at end of all supported runs | 0 |
| State invariance on every unsupported call | byte-identical pre / post |
| Diagnostic SNR / max-error gating | not used (NON-GATING by design) |

Step 3 PASS for the SRC framing / contract layer of Phase 2.
