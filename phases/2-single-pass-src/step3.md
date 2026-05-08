# Phase 2 Step 3: Round-trip rate tests

## Read First

- Phase 2 Step 1 (`AoRingWriteFromScratch` write SRC, commit
  `c728a58`'s parent chain `fb021e4`).
- Phase 2 Step 2 (`AoRingReadToScratch` read SRC, commit `c728a58`).
- `docs/ADR.md` ADR-004 (single-pass linear-interp SRC, GCD divisor
  first-match policy), ADR-005 (hard-reject overflow + hysteretic
  underrun + monotone counters), ADR-008 (KSDATARANGE / PCM-only).
- `docs/AO_CABLE_V1_DESIGN.md` section 2.3 (write SRC), section 2.4
  (read SRC), section 2.5 (bit-depth dispatch / 19-bit headroom).
- `docs/REVIEW_POLICY.md` section 5 (runtime observable proof).
- Step 0/1/2 Python equivalents (untracked):
  `tests/phase2-runtime/gcd_helper_equiv.py`,
  `tests/phase2-runtime/write_src_equiv.py`,
  `tests/phase2-runtime/read_src_equiv.py`.

## Goal

Run a chunk-driven end-to-end round-trip rate matrix to validate the
**SRC framing / contract** of Steps 1 and 2. The validation target is
the algorithmic contract -- status codes, state invariants, counter
behavior, and phase-aware count ratios -- **not** any fidelity quality
metric. SRC linear interpolation is lossy by definition; treating
this step as "is the resampler good?" would conflate algorithmic
correctness with quality tuning that belongs to Phase 7. Quality
diagnostics (SNR, max-abs-error) MAY be recorded as non-gating
artifacts, but step PASS/FAIL hinges only on the contract gates
listed in section "Contract gates" below.

## Planned Files

| Path | Tracked? | Purpose |
|---|---|---|
| `tests/phase2-runtime/round_trip_rate_test.py` | untracked | Test driver. Iterates the supported / unsupported / same-rate matrices and asserts contract gates. Imports the Step 0/1/2 Python equivalents for cross-check. |
| `tests/phase2-runtime/round_trip_results.json` | untracked | Per-cell results record (status, advances, counters, optional diagnostics). One pass run per phase commit if useful. |
| `tests/phase2-runtime/round_trip_run_<phase>_<shorthash>.txt` | untracked | Free-form log per run, per the project's experiment commit rule. |
| `phases/2-single-pass-src/step3-evidence.md` | **tracked** | Step 3 evidence summary -- "what was verified and what is NOT claimed". Phase 1 Step 4/5/6 evidence pattern. |

No code changes. `loopback.cpp` / `loopback.h` are not edited.
`AO_STREAM_RT` is not edited. No caller wiring change.

## Driver pattern (chunk-driven interleaved round-trip)

The naive "write all input, then read all output" fails for any pair
that produces output longer than `WrapBound` (e.g. 5 s of 192 kHz =
960 k frames, far above the default `WrapBound = 96000`). The driver
must therefore interleave write and read at chunk granularity so the
ring fill stays bounded:

```text
For each (internal_rate, client_rate, dst_bits) cell:
  pipe = FRAME_PIPE init at internal_rate
  Generate ref_input at client_rate (1 kHz sine, ~1 s of frames)
  Generate ref_output expectation at client_rate
    (compute via Step 0/1/2 simulator chain so the test cross-checks
     itself against the kernel's algorithmic intent)

  total_input_frames = len(ref_input)
  chunk_size = ~10 ms at client_rate (e.g. 480 frames at 48 k)
  output = []

  while input not exhausted:
    AoRingWriteFromScratch(pipe, input_chunk, frames=chunk_size,
                           srcRate=client_rate, ...)
    -> assert STATUS_SUCCESS
    AoRingReadToScratch(pipe, output_chunk, frames=chunk_size,
                        dstRate=client_rate, ...)
    -> assert STATUS_SUCCESS
    output.append(output_chunk)

  Run contract gates on (ref_input, output, pipe state).
```

Why interleaved: write-then-read in a single batch can underrun
(read called before write produces enough ring frames) or overflow
(write produces more than ring can hold) on extreme rate ratios.
Interleaving at ~10 ms keeps ring fill steady around the
configured target latency, which is what the production driver
will see at runtime.

Why ~1 s total: enough for the SRC loop to leave the startup
transient and reach steady-state count ratios. Diagnostic windows
(if recorded) skip the leading transient (see "Startup transient
exclusion" below).

## Test Matrix

The matrix covers every rate listed in `docs/ADR.md` ADR-008 (10
rates):
8000, 16000, 22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000.

### Group classification (per GCD divisor coverage)

For each rate, the table below records which priority-list candidate
divisors leave a zero remainder:

| Rate   | mod 300 == 0 | mod 100 == 0 | mod 75 == 0 |
|-------:|:---:|:---:|:---:|
|   8000 |     |  Y  |     |
|  16000 |     |  Y  |     |
|  22050 |     |     |  Y  |
|  32000 |     |  Y  |     |
|  44100 |  Y  |  Y  |  Y  |
|  48000 |  Y  |  Y  |  Y  |
|  88200 |  Y  |  Y  |  Y  |
|  96000 |  Y  |  Y  |  Y  |
| 176400 |  Y  |  Y  |  Y  |
| 192000 |  Y  |  Y  |  Y  |

A pair `(internal_rate, client_rate)` is **supported** when both
rates share at least one divisor in `[300, 100, 75]`. The runtime
picker `PickGCDDivisor` in `Source/Utilities/loopback.cpp` tries 300,
then 100, then 75 under ADR-004 Decision step 1 (first-match) and
returns on the first candidate that divides both rates evenly.

### Resulting support matrix

`Y` = supported (at least one common divisor); `N` = not supported.

| internal \ client | 8k | 16k | 22.05k | 32k | 44.1k | 48k | 88.2k | 96k | 176.4k | 192k |
|-------------------|:--:|:---:|:------:|:---:|:-----:|:---:|:-----:|:---:|:------:|:----:|
| 8k                |  Y |  Y  |   N    |  Y  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |
| 16k               |  Y |  Y  |   N    |  Y  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |
| 22.05k            |  N |  N  |   Y    |  N  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |
| 32k               |  Y |  Y  |   N    |  Y  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |
| 44.1k             |  Y |  Y  |   Y    |  Y  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |
| 48k               |  Y |  Y  |   Y    |  Y  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |
| 88.2k             |  Y |  Y  |   Y    |  Y  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |
| 96k               |  Y |  Y  |   Y    |  Y  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |
| 176.4k            |  Y |  Y  |   Y    |  Y  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |
| 192k              |  Y |  Y  |   Y    |  Y  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |

The only `N` cells are the three unordered pairs
`22.05k <-> {8k, 16k, 32k}` (six directional cells once mirrors are
counted).

### Supported round-trip examples (helper-cross-checked)

For each supported pair `(internal_rate, client_rate)`, the test
makes two helper calls per round trip:

- write side: `PickGCDDivisor(srcRate=client_rate, dstRate=internal_rate)`
- read side:  `PickGCDDivisor(srcRate=internal_rate, dstRate=client_rate)`

Both must return `STATUS_SUCCESS` with the same divisor (the
priority list is symmetric in its two arguments). Concrete values:

| Round-trip | internal | client | divisor | write SrcRatio:DstRatio | read SrcRatio:DstRatio |
|---|--:|--:|---|---|---|
| `48k -> 44.1k -> 48k`     | 44100 | 48000 | 300 | 160:147 | 147:160 |
| `44.1k -> 48k -> 44.1k`   | 48000 | 44100 | 300 | 147:160 | 160:147 |
| `48k -> 96k -> 48k`       | 96000 | 48000 | 300 | 160:320 | 320:160 |
| `96k -> 48k -> 96k`       | 48000 | 96000 | 300 | 320:160 | 160:320 |
| `48k -> 8k -> 48k`        |  8000 | 48000 | 100 | 480:80  | 80:480  |
| `8k -> 48k -> 8k`         | 48000 |  8000 | 100 | 80:480  | 480:80  |
| `48k -> 22.05k -> 48k`    | 22050 | 48000 |  75 | 640:294 | 294:640 |
| `48k -> 48k -> 48k`       | 48000 | 48000 | n/a | (same-rate fast path; no SRC) | (same-rate fast path; no SRC) |

### Unsupported examples (helper-cross-checked)

For each unsupported pair, the test verifies BOTH directions
independently:

| pair | direction A | direction B |
|---|---|---|
| `8k <-> 22.05k`  | internal=22050, client write at 8000 | internal=8000, client read at 22050 |
| `16k <-> 22.05k` | internal=22050, client write at 16000 | internal=16000, client read at 22050 |
| `32k <-> 22.05k` | internal=22050, client write at 32000 | internal=32000, client read at 22050 |
| `12345` (helper-level) | internal=48000, client write at 12345 | internal=48000, client read at 12345 |

`12345` is non-advertised; it exercises the helper directly without
relying on the KSDATARANGE intersection. The first SRC call (write
or read at the unsupported rate) returns `STATUS_NOT_SUPPORTED`
cleanly with no state mutation.

### Helper-equivalent cross-check (mandatory)

For every cell in the matrix, the test computes its own
`expected_supported` flag by running the Python `pick_gcd_divisor`
mirror (from `gcd_helper_equiv.py`) against `(internal_rate,
client_rate)` and comparing against the doc matrix above. If the
two disagree, the test fails the matrix cell with a clear
"matrix-vs-helper drift" diagnostic. This catches stale matrix
hardcodes the next time someone changes the helper without updating
this doc.

## Contract gates

Step 3 PASS / FAIL is judged ONLY against the gates below. Quality
metrics (SNR, max-abs-error) are recorded but do NOT affect the
verdict.

### Per supported pair

For each supported `(internal_rate, client_rate, dst_bits)` cell:

1. **Write status**: every chunked `AoRingWriteFromScratch` call
   returns `STATUS_SUCCESS`.
2. **Read status**: every chunked `AoRingReadToScratch` call returns
   `STATUS_SUCCESS`.
3. **Output non-silent**: at least one output sample has nonzero
   absolute value across the run (excluding the startup transient
   window). Catches "function returned SUCCESS but produced silence".
4. **Output finite and bounded**: every output sample is within the
   PCM range for `dst_bits` (no `INT_MIN` overflow, no NaN -- which
   is impossible in this code path but recorded for completeness).
5. **OverflowCounter == 0** at end of run.
6. **UnderrunCounter == 0** at end of run.
7. **UnderrunFlag == 0** at end of run.
8. **Phase-aware count ratio match**: cumulative
   `WritePos`-advance and cumulative `ReadPos`-advance match the
   helper-equivalent expected counts (`gcd_helper_equiv.py` driver
   computes the expected ring-frame counts from total client frames
   and the divisor pair).
9. **Chunked vs single-call simulator parity** (where applicable):
   the Python simulator chain
   (`write_src_equiv.write_src_branch` and
   `read_src_equiv.read_src_branch`) must produce byte-identical
   final state when fed the same total input split differently into
   chunks. This re-runs the Step 1 / 2 phase carry parity check
   end-to-end.

### Per unsupported pair

For each unsupported `(internal_rate, client_rate)` cell, BOTH
directions:

1. **Write side**: with `pipe.InternalRate = internal_rate`, calling
   `AoRingWriteFromScratch(pipe, ..., srcRate = client_rate, ...)`
   returns `STATUS_NOT_SUPPORTED` on the first call.
2. **Read side**: with `pipe.InternalRate = internal_rate`, calling
   `AoRingReadToScratch(pipe, ..., dstRate = client_rate, ...)`
   returns `STATUS_NOT_SUPPORTED` on the first call.
3. **State invariance** on both directions: `WritePos`, `ReadPos`,
   `WriteSrcPhase`, `ReadSrcPhase`, every channel of
   `WriteSrcResidual[]` and `ReadSrcResidual[]`, `OverflowCounter`,
   `UnderrunCounter`, `UnderrunFlag`, and the ring `Data[]` are all
   unchanged across the call. Pre-call snapshot vs post-call
   snapshot must match byte-for-byte.

### Per same-rate identity (Phase 1 Step 2 / 3 regression)

For each ADR-008 rate `r` and each `dst_bits in {8, 16, 24, 32}`:

1. Same-rate path: `pipe.InternalRate = r`, client write/read at
   `r`. SRC branch is NOT entered (control flow falls through to
   the Phase 1 fast path).
2. **8/16/24-bit**: round-trip identity is bit-exact (Phase 1
   Step 3 `DenormalizeFromInt19` clamp + pack returns the original
   PCM bytes when fed the same value).
3. **32-bit**: top 19 bits preserved, lower 13 bits zeroed (per
   `DESIGN section 2.5` 19-bit headroom contract -- write applies
   `>> 13`, read applies `<< 13`, so the lower 13 bits are
   intentionally lost). This is NOT a Step 3 defect -- it is the
   AO Cable V1 internal representation.
4. `OverflowCounter == 0`, `UnderrunCounter == 0`, `UnderrunFlag
   == 0` at end of run.

## Optional diagnostic (NON-GATING)

The test MAY also record the following per cell into
`round_trip_results.json` for inspection:

- **SNR** (dB) of the 1 kHz sine over the diagnostic window.
- **Max abs error** in the 19-bit normalized scale, in the
  diagnostic window.
- **Emit count vs expected** (already part of the contract gates,
  but the absolute deltas are useful in diagnostic logs).
- **Skip window size** (frames excluded at the start due to
  startup transient and chunk boundary residual transient).

These are diagnostics. Step 3 PASS does NOT depend on any SNR or
error threshold. Threshold-based fidelity gating is explicitly out
of scope for this step. Tuning the resampler quality belongs to
Phase 7 (`docs/ADR.md` Phase 7 quality polish).

If a diagnostic looks bad but every contract gate passes, file an
observation in `step3-evidence.md` ("recorded but not gating") and
move on. Algorithm bug suspicion is reserved for situations where
contract gates also fail (e.g. count-ratio mismatch or counter
non-zero at end of run).

## Startup transient exclusion (diagnostic window only)

The diagnostic comparison window starts after the algorithm leaves
its leading residual=0 transient on both sides:

- **Write side residual=0 transient**: the first ring frames the
  write SRC produces blend `WriteSrcResidual = 0` against real
  input. Length: roughly
  `ceil(SrcRatio_write / DstRatio_write)` ring frames (the
  number of input samples the SRC has to consume before the
  residual is no longer zero).
- **Read side residual=0 transient**: same on the read direction
  with the rate ratio inverted.
- **Chunk boundary residual carry transient**: each chunk boundary
  itself is mathematically continuous because phase + residual
  carry, but the diagnostic FFT may show a small artifact if the
  chunk size is not aligned. This is a measurement artifact, not
  an algorithm defect.

Skip window = write_skip + read_skip + safety margin (e.g. one
extra chunk on each side). Diagnostic window = output frames
`[skip_start, len(output) - end_skip]`. The contract gates above
do NOT skip any frames; they apply to the whole run.

## Acceptance Criteria

- [ ] All supported pairs in the matrix pass every contract gate
      (write/read STATUS_SUCCESS, non-silent, finite/bounded,
      counters 0, count-ratio match, chunked vs single-call
      simulator parity).
- [ ] All unsupported pairs return `STATUS_NOT_SUPPORTED` on the
      first SRC call in BOTH directions, with byte-identical state
      snapshot before vs after the call.
- [ ] Same-rate identity contract holds for all 10 ADR-008 rates x
      4 bit depths (8/16/24/32-bit), per the 19-bit headroom rules.
- [ ] Helper-equivalent cross-check: every matrix cell's
      supported / unsupported flag matches `pick_gcd_divisor` output
      for both `(internal, client)` orderings.
- [ ] No `STATUS_INSUFFICIENT_RESOURCES` (write) and no silent-fill
      `STATUS_SUCCESS` (read underrun) leakage in any normal
      supported-pair run.
- [ ] `tests/phase2-runtime/round_trip_results.json` produced
      (untracked).
- [ ] `phases/2-single-pass-src/step3-evidence.md` written and
      committed (tracked) summarizing what was verified, what is
      not claimed, and where the diagnostic numbers live.

## State invariants (cross-reference)

Step 3 does not introduce new state contracts; it re-verifies the
contracts established by Steps 1 and 2 in an end-to-end setting:

- ADR-005 hard-reject (write overflow) -- Step 1 regression. Step 3
  supported-pair runs MUST NOT trip this in normal traffic.
- ADR-005 hysteretic underrun (read) -- Step 2 regression. Step 3
  supported-pair runs MUST NOT trip this in normal traffic.
- Hostile srcChannels reject -- Step 1 Python regression
  (`write_src_equiv.test_channel_reject` /
  `test_hostile_inputs`). Re-running Step 1 / 2 Python tests in
  the Step 3 driver is OPTIONAL but recommended as a single
  combined run.
- Hostile dstChannels reject -- Step 2 Python regression. Same
  optional re-run.
- Phase-aware capacity / lookahead -- Step 1 / 2 Python
  regressions cover the boundary cases (`test_emit_count_invariant`,
  `test_no_post_final_consume_read`,
  `test_trailing_output_lookahead`). Step 3 contract gates verify
  the same invariants end-to-end on all matrix cells.

## What This Step Does NOT Do

- No code changes in `loopback.cpp` or `loopback.h`.
- Does NOT claim any SNR or fidelity quality threshold. Step 3
  PASS / FAIL judges contract correctness only.
- Does NOT claim bit-exact identity for SRC pairs. SRC linear
  interpolation is lossy; the diagnostic window may show several
  LSB of error and that is expected.
- Does NOT change the SRC algorithm (capacity check formulas,
  reload pattern, hysteresis) under any pressure to "make the
  test pass". If a contract gate fails, fix the algorithm or fix
  the test expectation -- not the test result.
- Does NOT change the unsupported-pair status code from
  `STATUS_NOT_SUPPORTED` to `STATUS_INVALID_PARAMETER` or any
  other variant. The two are distinct per ADR-008 / Step 0
  contract.
- Does NOT introduce a sinc fallback, a 4-stage pipeline, or any
  second SRC path (CLAUDE.md Forbidden Compromises).

## STOP conditions

If any of the following arise during Step 3 work, stop and report:

1. `loopback.cpp` or `loopback.h` requires modification to make
   a test pass. Step 3 is validation only; algorithmic changes
   belong to Step 1 / 2 or to a follow-up phase.
2. `PickGCDDivisor`, `AoRingWriteFromScratch`, or
   `AoRingReadToScratch` signatures need to change.
3. The test or its driver pattern requires SRC algorithm changes
   (different capacity formula, different reload policy,
   different hysteresis threshold).
4. A "bit-exact" or "SNR >= X dB" claim is being added to the
   contract gate to mask a measurement artifact -- this would
   conflate Step 3 (contract verification) with Phase 7 (quality
   tuning).
5. `STATUS_NOT_SUPPORTED` for an unsupported pair is being changed
   to `STATUS_INVALID_PARAMETER` or any other code.
6. The doc matrix and `pick_gcd_divisor` helper output disagree.
   The helper is authoritative (per ADR-004); fix the matrix.
   But if the helper itself disagrees with ADR-004 first-match
   policy, that is a Step 0 regression -- stop and investigate.
7. A SNR / max-abs-error diagnostic threshold is being elevated to
   a gating contract.

## Completion

```powershell
python scripts/execute.py mark 2-single-pass-src 3 completed --message "Round-trip rate matrix PASS (contract gates; diagnostics non-gating)."
```
