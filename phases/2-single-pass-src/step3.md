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

```text
      Output rate
      8000  16000 22050 44100 48000 96000 192000
8000   X     X     -     X     X     X     X
16000  X     X     -     X     X     X     X
22050  -     -     X     X     -     X     -
44100  X     X     X     X     X     X     X
48000  X     X     -     X     X     X     X
96000  X     X     -     X     X     X     X
192000 X     X     -     X     -     X     X
```

`X` = supported via GCD divisor 300, 100, or 75. `-` = unsupported pair
(should return `STATUS_NOT_SUPPORTED` cleanly without garbled output).

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
