# Phase 7 Step 3: Control Panel diagnostics polish

## Goal

The user-mode Control Panel (`Source/ControlPanel/`) currently shows
basic status. This step extends it to surface the V1 `AO_V2_DIAG`
counters in a readable form: per-cable, per-direction, with
trend lines (overflow / underrun / fill) and alerts when steady-state
counters increment.

## Planned Files

- `Source/ControlPanel/main.cpp` — diagnostics view.
- `test_stream_monitor.py` — already exposes the same data; the
  Control Panel becomes the user-facing equivalent.

## Acceptance Criteria

- [ ] Builds.
- [ ] Live counter display matches `test_stream_monitor.py` output.
- [ ] Alert badge appears if `OverflowCount` or `UnderrunCount`
      increments during a 60-second sample window.

## Completion

```powershell
python scripts/execute.py mark 7-quality-polish 3 completed --message "Control Panel diagnostics view ships."
```
