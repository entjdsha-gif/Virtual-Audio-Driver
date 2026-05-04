# Phase 0 Step 0: VB-Cable RE evidence

Status: completed (2026-04-12)

## Read First

- `docs/PRD.md`
- `docs/ADR.md` (especially ADR-002)
- `docs/AO_CABLE_V1_ARCHITECTURE.md` § 2 "Why we are starting over"

## Goal

Collect verified static and dynamic evidence about VB-Cable's cable transport
core, sufficient to design a VB-equivalent rewrite for AO Cable V1.

## Acceptance Criteria

All of the following exist in the repo and have been reviewed by the user:

- [x] `results/ghidra_decompile/vbcable_all_functions.c` — full Ghidra
      decompile of VB-Cable A (297 functions, 12 096 lines).
- [x] `results/ghidra_decompile/vbcable_function_index.txt` — function
      address/size index.
- [x] `results/vbcable_pipeline_analysis.md` — pipeline trace covering DPC
      timer, render path (DMA → ring), capture path (ring → DMA), position
      reporting, format handling.
- [x] `results/vbcable_disasm_analysis.md` — `FUN_1400026a0` deep dive
      (write SRC + ring struct layout).
- [x] `results/vbcable_func26a0.asm` — raw assembly of the largest function
      (4 808 bytes).
- [x] `results/vbcable_capture_contract_answers.md` — Q&A on capture
      contract (10 verified answers, including ring +0x00 = TargetLatencyFrames
      correction, `MonoFramesLow/Mirror` semantics, push-driven paced model).
- [x] `results/phase6_vb_verification.md` — WinDbg dynamic verification under
      live TTS payload.
- [x] `results/vb_session.log` — full WinDbg/KDNET session log (~6 336 lines).
- [x] `results/ghidra_logs/vb_re_headless*.txt` — headless Ghidra session
      logs (`FindVbLatency`, registry keys, init function).

Live-call comparison confirmed:

- VB-Cable on the live Phone Link + OpenAI Realtime path: clean.
- AO Cable (pre-rewrite) on the same path: garbled.

This established that the regression is in the AO cable transport core,
not in the call harness or routing.

## Outcome

The 10 design principles in `docs/ADR.md` (ADR-003..ADR-012) all trace
back to evidence collected in this step. No design choice in V1 is invented;
each is either a direct VB observation or an explicit deliberate divergence
documented in an ADR.

## Completion

```powershell
python scripts/execute.py mark 0-baseline 0 completed --message "VB-Cable RE evidence verified; principles traced into ADR-003..012."
```

(Already marked completed.)
