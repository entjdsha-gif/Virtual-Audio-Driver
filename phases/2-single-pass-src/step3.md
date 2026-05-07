# Phase 2 Step 3: Round-trip rate tests

## Read First

- Phase 2 Steps 1, 2.
- `docs/ADR.md` ADR-004 (single-pass linear-interp SRC, GCD divisor
  first-match policy).
- `docs/REVIEW_POLICY.md` § 5 (runtime observable proof).

## Goal

Run a round-trip rate matrix to validate Phase 2 SRC correctness end-to-
end (write SRC -> ring at internal rate -> read SRC -> output). Compare
against expected output for each pair.

## Planned Files

Edit only:

- `tests/phase2-runtime/round_trip_rate_matrix.py` (untracked).

## Test Matrix

The matrix covers every rate listed in `docs/ADR.md` ADR-008 (10 rates):
8000, 16000, 22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000.

### Group classification (per GCD divisor coverage)

For each rate, the table below records which priority-list candidate
divisors leave a zero remainder (i.e. the rate is divisible by that
divisor):

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

A pair `(src, dst)` is **supported** when both rates share **at least
one** divisor in `[300, 100, 75]`. The runtime picker
(`PickGCDDivisor` in `loopback.cpp`) tries 300, then 100, then 75
under ADR-004 Decision step 1 (first-match) and returns on the first
candidate that divides both rates evenly.

### Resulting support matrix

`Y` = supported (at least one common divisor); `N` = not supported
(no common divisor in `[300, 100, 75]`).

| src \ dst | 8k | 16k | 22.05k | 32k | 44.1k | 48k | 88.2k | 96k | 176.4k | 192k |
|-----------|:--:|:---:|:------:|:---:|:-----:|:---:|:-----:|:---:|:------:|:----:|
| 8k        |  Y |  Y  |   N    |  Y  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |
| 16k       |  Y |  Y  |   N    |  Y  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |
| 22.05k    |  N |  N  |   Y    |  N  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |
| 32k       |  Y |  Y  |   N    |  Y  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |
| 44.1k     |  Y |  Y  |   Y    |  Y  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |
| 48k       |  Y |  Y  |   Y    |  Y  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |
| 88.2k     |  Y |  Y  |   Y    |  Y  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |
| 96k       |  Y |  Y  |   Y    |  Y  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |
| 176.4k    |  Y |  Y  |   Y    |  Y  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |
| 192k      |  Y |  Y  |   Y    |  Y  |   Y   |  Y  |   Y   |  Y  |   Y    |   Y  |

The only `N` cells are the three unordered pairs
`22.05k <-> {8k, 16k, 32k}` (six directional cells once mirrors are
counted), because 8k, 16k, and 32k each have only `mod 100 == 0` while
22.05k has only `mod 75 == 0` -- no shared candidate in
`[300, 100, 75]`. Every other ADR-008 rate has at least one divisor in
common with 22.05k (via `mod 75`), including 192k
(`192000 / 75 = 2560`, `22050 / 75 = 294`).

### Unsupported pairs (definitive list)

- `22.05k <-> 8k`
- `22.05k <-> 16k`
- `22.05k <-> 32k`

(plus their mirrored direction -- six directional cells in total).
These return `STATUS_NOT_SUPPORTED` from `PickGCDDivisor` cleanly,
with no garbled output and no ring state mutation.

### Implementer's verification rule

For each pair the test should:
- supported -> assert `STATUS_SUCCESS` + bit-stable round trip on a sine.
- unsupported -> assert `STATUS_NOT_SUPPORTED` returned at the first
  call (write or read), output buffer untouched.

The authoritative source for divisor selection is **ADR-004 Decision
step 1 (first-match across `[300, 100, 75]`)** as implemented by
`PickGCDDivisor` in `Source/Utilities/loopback.cpp`. If this matrix
disagrees with helper output for any cell, **the helper is
authoritative -- fix this matrix**, not the helper.

For each `Y` cell, the test:

1. Generates a known input signal at the input rate (1 kHz sine, 5
   seconds of frames).
2. Calls `AoRingWriteFromScratch(pipe, input, frames, inputRate, ...)`.
3. Calls `AoRingReadToScratch(pipe, output, expectedOutputFrames,
   outputRate, ...)`.
4. Verifies that the output is a 1 kHz sine at the output rate, modulo
   linear-interp fidelity.
5. Confirms `OverflowCounter == 0` and `UnderrunCounter == 0`.

For each `N` cell, the test confirms `STATUS_NOT_SUPPORTED` is returned
on the first SRC call.

## Acceptance Criteria

- [ ] All `Y` cells pass.
- [ ] All `N` cells return `STATUS_NOT_SUPPORTED` cleanly.
- [ ] No `STATUS_INSUFFICIENT_RESOURCES` leakage from non-overflow
      scenarios.
- [ ] Test trace saved to `tests/phase2-runtime/round_trip_results.json`
      (untracked).

## What This Step Does NOT Do

- No code changes (this is validation of Steps 1 / 2).
- No caller wiring.

## Completion

```powershell
python scripts/execute.py mark 2-single-pass-src 3 completed --message "Round-trip rate matrix PASS."
```
