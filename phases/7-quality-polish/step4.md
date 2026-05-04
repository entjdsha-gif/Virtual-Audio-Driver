# Phase 7 Step 4: Benchmark suite extension

## Goal

`run_benchmark_suite.ps1` and the `test_*.py` scripts at repo root
already exist. This step extends them to:

1. Cover the full V1 KSDATARANGE rate matrix (Phase 2 Step 3 is per-
   pair; this is end-to-end via WASAPI).
2. Compare AO vs VB on bit-exactness, latency, dropout, drift in a
   single run.
3. Output a JSON + Markdown summary suitable for `docs/`.

## Planned Files

- Edit `run_benchmark_suite.ps1` to add new test invocations.
- Edit/add `test_compare_vb.py`, `test_bit_exact.py`,
  `test_drift.py`, `test_dropout.py`, `test_latency.py` as needed.
- Promote a representative summary to
  `docs/BENCHMARK_SUMMARY.md` (single canonical comparison file).

## Acceptance Criteria

- [ ] One-shot benchmark run produces:
      - per-rate AO results (bit-exact / latency / dropout / drift),
      - same for VB,
      - delta summary,
      - PASS/FAIL roll-up against `docs/PRD.md` § 8 success criteria.
- [ ] Reproducible on a clean machine.

## Completion

```powershell
python scripts/execute.py mark 7-quality-polish 4 completed --message "Benchmark suite covers V1 KSDATARANGE; AO vs VB comparison automated."
```
