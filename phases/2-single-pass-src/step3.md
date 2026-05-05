# Phase 2 Step 3: Round-trip rate tests

## Read First

- Phase 2 Steps 1, 2.
- `docs/REVIEW_POLICY.md` § 5 (runtime observable proof).

## Goal

Run a round-trip rate matrix to validate Phase 2 SRC correctness end-to-
end (write SRC → ring at internal rate → read SRC → output). Compare
against expected output for each pair.

## Planned Files

Edit only:

- `tests/phase2-runtime/round_trip_rate_matrix.py` (untracked).

## Test Matrix

The matrix covers every rate listed in `docs/ADR.md` ADR-008 (10 rates):
8000, 16000, 22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000.

### Group classification (per GCD divisor coverage)

Each rate belongs to one or more divisor groups (300, 100, 75):

| Rate | ÷ 300 | ÷ 100 | ÷ 75 |
|---|---|---|---|
| 8000  |   |  ✓ |   |
| 16000 |   |  ✓ |   |
| 22050 |   |    | ✓ |
| 32000 |   |  ✓ |   |
| 44100 | ✓ |  ✓ | ✓ |
| 48000 | ✓ |  ✓ | ✓ |
| 88200 | ✓ |  ✓ | ✓ |
| 96000 | ✓ |  ✓ | ✓ |
| 176400| ✓ |  ✓ | ✓ |
| 192000| ✓ |  ✓ | ✓ |

A pair `(src, dst)` is **supported** when both rates share **at least
one divisor**. The picker tries 300, then 100, then 75 (per `pickGCD`
in `loopback.cpp`).

### Resulting support matrix

| src \ dst | 8k | 16k | 22.05k | 32k | 44.1k | 48k | 88.2k | 96k | 176.4k | 192k |
|---|---|---|---|---|---|---|---|---|---|---|
| 8k     | ✓ | ✓ | ✗ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| 16k    | ✓ | ✓ | ✗ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| 22.05k | ✗ | ✗ | ✓ | ✗ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| 32k    | ✓ | ✓ | ✗ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| 44.1k  | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| 48k    | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| 88.2k  | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| 96k    | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| 176.4k | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| 192k   | ✓ | ✓ | ✗ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |

Wait — `192k ↔ 22.05k`: 192000 ÷ 75 = 2560 ✓ ; 22050 ÷ 75 = 294 ✓.
That pair **is** supported. The `192k ↔ 22.05k` cell should read ✓.
The same logic applies to 8k/16k/32k (Group {÷100 only}) ↔ 22.05k
(Group {÷75 only}) — those four pairs are the **only** `✗` cells in
the matrix (and their mirrors).

### Unsupported pairs (definitive list)

- `22.05k ↔ 8k`
- `22.05k ↔ 16k`
- `22.05k ↔ 32k`

(plus their mirrored direction). These return `STATUS_NOT_SUPPORTED`
cleanly without garbled output.

### Implementer's verification rule

For each pair the test should:
- supported → assert `STATUS_SUCCESS` + bit-stable round trip on a sine.
- unsupported → assert `STATUS_NOT_SUPPORTED` returned at first call,
  output buffer untouched.

If the matrix above disagrees with `pickGCD` output, the matrix is
authoritative — fix `pickGCD`.

For each `X` cell, the test:

1. Generates a known input signal at the input rate (1 kHz sine, 5
   seconds of frames).
2. Calls `AoRingWriteFromScratch(pipe, input, frames, inputRate, ...)`.
3. Calls `AoRingReadToScratch(pipe, output, expectedOutputFrames,
   outputRate, ...)`.
4. Verifies that the output is a 1 kHz sine at the output rate, modulo
   linear-interp fidelity.
5. Confirms `OverflowCounter == 0` and `UnderrunCounter == 0`.

For each `-` cell, the test confirms `STATUS_NOT_SUPPORTED` is returned
on the first SRC call.

## Acceptance Criteria

- [ ] All `X` cells pass.
- [ ] All `-` cells return `STATUS_NOT_SUPPORTED` cleanly.
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
