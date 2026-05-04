# Evidence Inventory

Last checked: 2026-04-24

This file records the raw evidence that must be kept for VB-Cable parity work.
Generated audio captures (`*.wav`) and NumPy arrays (`*.npy`) are not part of
this required source-evidence set.

## Required Ghidra Evidence

| Purpose | Path | Current state |
|---|---|---|
| Full VB-Cable decompile dump | `results/ghidra_decompile/vbcable_all_functions.c` | Present, 10,565 lines, 337.5 KB |
| Function index | `results/ghidra_decompile/vbcable_function_index.txt` | Present, 297 lines |
| Headless Ghidra run logs | `results/ghidra_logs/vb_re_headless*.txt` | Present |

Note: the full decompile dump is sometimes referred to as the "12,096-line"
dump in discussion, but the current checked file counts as 10,565 lines in
PowerShell. After the 2026-04-25 branch consolidation, the only worktree is
`D:/mywork/Virtual-Audio-Driver` on `feature/ao-fixed-pipe-rewrite`.

## Required Dynamic Debug Evidence

| Purpose | Path | Current state |
|---|---|---|
| Raw live WinDbg/KDNET session | `results/vb_session.log` | Present, 6,336 lines, 388.5 KB |
| VB runtime analysis notes | `results/vbcable_runtime_claude.md` | Present |
| VB pipeline analysis | `results/vbcable_pipeline_analysis.md` | Present |
| VB capture contract answers | `results/vbcable_capture_contract_answers.md` | Present |
| Phase 6 VB verification | `results/phase6_vb_verification.md` | Present |
| VB dynamic analysis summary | `docs/VB_CABLE_DYNAMIC_ANALYSIS.md` | Present |
| VB parity debug results | `docs/VB_PARITY_DEBUG_RESULTS.md` | Present |
| Phase 6/Y implementation plan | `docs/PHASE6_Y_IMPLEMENTATION_WORK_ORDER.md` | Present |
| Pipeline experiment changelog | `docs/PIPELINE_V2_CHANGELOG.md` | Present |

## AO Runtime Logs Still Present

The main Phase 6 runtime logs are preserved in
`D:/mywork/Virtual-Audio-Driver/results/`:

- `results/phase6_step1_run1.dbgview.log`
- `results/phase6_optionZ_run1.dbgview.log`
- `tests/live_call/runtime_logs/test_call.log`
- `tests/live_call/runtime_logs/phone_link_worker_debug.log`

Additional older AO debug logs remain in this repository root:

- `g2_dbgview.log`
- `g2_dbgview_b.log`
- `dbgview_capture.log`

## Deleted Runtime Artifacts

Large generated files were removed during cleanup, then restored from Windows
File Recovery on 2026-04-24:

- ETW/WPR traces restored to repository root:
  - `ao_events.xml` (1,961,486,547 bytes)
  - `ao_trace.etl` (1,028,653,056 bytes)
  - `vb_trace.etl` (1,552,941,056 bytes)
- Backup copies remain under
  `C:/Recovery_20260424_182627/mywork/Virtual-Audio-Driver/`
- Trace symbol cache folders removed and not restored:
  - `ao_trace.etl.NGENPDB`
  - `vb_trace.etl.NGENPDB`
- Visual Studio cache: `.vs`
- Audio/NumPy runtime captures: `*.wav`, `*.npy`

The ETW/WPR traces are raw evidence and should be preserved outside git even if
they are not part of the canonical markdown/Ghidra/WinDbg evidence path.
