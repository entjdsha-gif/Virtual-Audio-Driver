# docs/archive — superseded planning documents

These files are kept for historical reference only. They are **not** the current source of truth.

## Current source of truth

- **Architecture (single source):** `docs/AO_FIXED_PIPE_ARCHITECTURE.md`
- **Roadmap / current state:** `docs/CURRENT_STATE.md`
- **Evidence (Ghidra/WinDbg):** `results/vbcable_*.md`, `results/ghidra_decompile/`, `results/ghidra_logs/`, `results/phase6_vb_verification.md`, `results/vb_session.log`

## What is archived here and why

### Phase 6 plans (Codex authoritative drafts, now consolidated)

- `PHASE6_PLAN.md` — original Phase 6 design rules. Content folded into `AO_FIXED_PIPE_ARCHITECTURE.md` § 4 (design principles) and § 17 (failure modes).
- `PHASE6_OPTION_Y_CABLE_REWRITE.md` — detailed Option Y rewrite spec. Content folded into § 6 (data structures), § 7 (pipeline), § 13 (code organization), § 14 (migration), § 15 (stages).
- `PHASE6_Y_IMPLEMENTATION_WORK_ORDER.md` — Y1A/B/C/Y2/Y3/Y4 step-by-step order. Replaced by Stage 1–6 in `AO_FIXED_PIPE_ARCHITECTURE.md` § 15 (renamed to remove confusing Y-letter nomenclature).

### VB parity verification drafts

- `VB_PARITY_DEBUG_RESULTS.md` — WinDbg findings under TTS payload. Evidence reference now lives at `results/phase6_vb_verification.md`; conclusions folded into § 3 (evidence) and § 17 (failure modes).
- `VB_PARITY_NEXT_DEBUG_CHECKLIST.md` — next WinDbg session plan. Open items moved to § 11.2 (notification, provisional) and § 12 (diagnostics).
- `VB_PARITY_CLOSURE_PLAN.md` — closure outline. Folded into § 16 (validation).

### Older comparison / reimplementation drafts

- `VB_CABLE_AO_COMPARISON.md`, `VB_CABLE_AO_COMPARISON_CODEX_NOTES.md` — early AO vs VB diffs. Folded into § 2 (why we are starting over) and Appendix A (offset map).
- `VB_CABLE_AO_REIMPLEMENTATION_PLAN_CODEX.md` — Codex's 1392-line reimplementation plan. Content distilled into § 4 (principles) and § 15 (stages).
- `VB_CABLE_DUAL_PLAN_OPERATING_RULES.md` — older Codex/Claude split rule. Operational rule still applies (memory `feedback_codex_claude_plan_split.md`); the doc itself is no longer needed.

### Older AO design

- `AO_V2_ARCHITECTURE_PLAN.md` — V2 SessionPassthrough plan. Superseded; the V2 audio core was abandoned (`feature/ao-pipeline-v2` is frozen reference).
- `MILESTONES_M1-M6_ACHIEVED.md` — completed milestones M1/M2 (build/install + 16-channel selectable). Recorded for product history.

## Re-reading rule

If something in `AO_FIXED_PIPE_ARCHITECTURE.md` looks unclear and you want to dig into the original analysis, the archived files often have richer context. **But never edit them as if they were live.** If a fact in an archived doc is still relevant, lift it into the current architecture doc instead.
