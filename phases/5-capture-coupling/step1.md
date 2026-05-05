# Phase 5 Step 1: Live-call end-to-end validation

## Read First

- Phase 4 Step 2 (render-only validation pattern).
- Phase 5 Step 0 (capture audible flip atomic).
- `tests/live_call/run_test_call.py`.
- `docs/PRD.md` § 8 success criteria (especially #6, #7 — live-call
  parity with VB and STT accuracy).

## Goal

Run the full live-call path (render + capture both on helper) against
AO Cable A, confirm parity with VB-Cable on the same harness. This is
the **defining V1 success gate**.

## Planned Files

No source edits. Run-only step. Captured artifacts to
`tests/phase5-runtime/` (untracked).

## Procedure

1. Pre-experiment Cable / Phone Link checks (CLAUDE.md 엄수).
2. Run AO live call:

```powershell
$env:AUDIO_CABLE_PROFILE = "ao"
cd tests\live_call
python run_test_call.py
```

3. User judges call quality, STT transcript, and any audible artifact.
4. Capture `test_stream_monitor.py` log during the call to
   `tests/phase5-runtime/ao_phase5_stream_monitor.log`.
5. Run VB live call same harness:

```powershell
$env:AUDIO_CABLE_PROFILE = "vb"
python run_test_call.py
```

6. User judges VB call quality.
7. Compare AO vs VB on three axes:
   - Subjective audio quality (clean / garbled / silent).
   - STT transcript accuracy (count of misheard words).
   - Driver counters (overflow / underrun / drop / underrun flag).

## Acceptance Criteria

- [ ] AO subjective quality: **clean**, comparable to VB.
- [ ] AO STT accuracy: same level as VB (delta within natural call
      variance).
- [ ] AO `OverflowCounter` and `UnderrunCounter` both 0 in steady-
      state speech for at least one full call;
      `UnderrunFlag = 0` in steady state.
- [ ] No mid-call chopping reported in user judgment.
- [ ] Cable B parallel sanity (open Cable B simultaneously, no
      Cable A degradation).

## Failure Path

If AO quality is significantly worse than VB:

- This is a **BLOCKER** for Phase 5 exit.
- Capture `test_stream_monitor.log` + dbgview log + the user
  judgment, save to `tests/phase5-runtime/ao_phase5_FAIL_run<N>/`.
- Do **not** flip step status to completed.
- Open a fix-up step (`Phase 5 Step 1-fix-1`) describing the failure
  signature, the hypothesis for the cause, and the proposed fix.
  Repeat until parity is achieved.

## Completion

```powershell
python scripts/execute.py mark 5-capture-coupling 1 completed --message "Live call AO parity with VB."
```
