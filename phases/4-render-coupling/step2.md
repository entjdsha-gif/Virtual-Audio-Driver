# Phase 4 Step 2: Live-call render validation

## Read First

- Phase 4 Steps 0-1.
- `tests/live_call/run_test_call.py` (existing live-call harness).
- CLAUDE.md "Pre-Experiment Phone Link Connection Check" and
  "Pre-Experiment Cable Check" sections — both are 엄수.

## Goal

Run the live-call test harness against AO Cable A render with the
helper as audible owner. Confirm the render path (PC audio → call
uplink) is at least as clean as VB-Cable.

## Planned Files

No source edits. Run-only step. Captured artifacts go under
`tests/phase4-runtime/` (untracked).

## Procedure

1. Pre-experiment Cable Check: confirm system default playback is
   "AO Cable A", default recording is "AO Cable B" (or whichever
   pairing the test harness expects).
2. Pre-experiment Phone Link Check: confirm phone-side toggle ON,
   Phone Link app on dialer state.
3. Run AO baseline:

```powershell
$env:AUDIO_CABLE_PROFILE = "ao"
cd tests\live_call
python run_test_call.py
```

4. User judges call quality (clean / garbled / silent), saves
   judgment to `tests/phase4-runtime/ao_phase4_run1_judgment.txt`.
5. Run VB comparison:

```powershell
$env:AUDIO_CABLE_PROFILE = "vb"
python run_test_call.py
```

6. User judges VB call quality.
7. Compare AO vs VB. If AO < VB significantly, treat as
   `BLOCKER` and investigate before marking step completed.

## Acceptance Criteria

- [ ] AO live call: user judgment **clean**, comparable to VB on the
      same path.
- [ ] STT transcript accuracy on AO: not garbled or hallucinating.
- [ ] `test_stream_monitor.py` during AO call shows
      `OverflowCounter` and `UnderrunCounter` both at 0 in steady
      state, and `UnderrunFlag = 0`.
- [ ] Capture path quality not worse than pre-Phase-4 baseline (capture
      is still on legacy until Phase 5; this gate is "no render-side
      regression caused capture-side quality drop").

## What This Step Does NOT Do

- Does not flip capture audible.
- Does not test multi-format / multi-channel (Phase 7).

## Completion

```powershell
python scripts/execute.py mark 4-render-coupling 2 completed --message "Render live-call clean, parity with VB on render side."
```
