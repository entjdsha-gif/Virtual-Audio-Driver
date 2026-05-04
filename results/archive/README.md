# results/archive — historical phase proposals & probe notes

These are AO-side implementation proposals and bug-investigation notes from
earlier phases (phase 0 → phase 5, plus the G-series probe rounds and Bug B
reproduction work).

They are kept for traceability — when reviewing why a particular design
decision was made or what was tried earlier, look here.

## Current source of truth

- Architecture: `docs/AO_FIXED_PIPE_ARCHITECTURE.md`
- Roadmap: `docs/CURRENT_STATE.md`
- Live evidence (Ghidra/WinDbg): the rest of `results/` (not this archive)

## What is archived here

### Per-phase edit proposals (AO V1 / V2 / Phase 5 era)

- `phase0_findings.md`
- `phase1_edit_proposal.md`
- `phase2_edit_proposal.md`
- `phase3_edit_proposal.md`
- `phase4_edit_proposal.md`
- `phase5_edit_proposal.md`

These describe the per-phase code changes proposed at the time. The current
architecture doc supersedes them; the actual code state on the current branch
already reflects what landed and what was reverted.

### G-series probe / proposal notes

- `g3_render_upstream_notes.md`
- `g4_b1_prefill_proposal.md`
- `g5_format_path_notes.md`
- `g6_pairing_probe_proposal.md`
- `g7_routing_vs_endpoint_notes.md`
- `g8_dropout_origin_notes.md`
- `g9_bug_b_signature.md`

Round-by-round probe notes from the AO live-call quality investigation. Useful
historical evidence; not authoritative for the rewrite.

### Bug B repro

- `bugb_repro_protocol.md` — the protocol for reproducing Bug B (specific
  AO failure mode pre-rewrite).

### Older Codex-side reimplementation plan

- `VB_CABLE_AO_REIMPLEMENTATION_PLAN_CLAUDE.md` (3,230 lines) — early Claude-
  side reimplementation draft. Distilled into `docs/AO_FIXED_PIPE_ARCHITECTURE.md`.
- `VB_CABLE_DUAL_PLAN_OPERATING_RULES_POINTER.md` — pointer to the
  Codex/Claude operating rules. Rule itself lives in memory.

## Re-reading rule

Same as `docs/archive/README.md`: don't edit these as if they were live.
If a fact still matters, lift it into the current architecture doc.
