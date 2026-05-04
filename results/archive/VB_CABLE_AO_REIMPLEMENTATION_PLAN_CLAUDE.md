# VB-Cable → AO Reimplementation Plan (Claude)

> **File owner**: Claude
> **Date**: 2026-04-13 (revision 2.5, post sixth cross-review — Codex v4 absorption: two new rules + Phase 5/6 one-owner exit + polling-regime guidance)
> **Status**: Planning baseline complete, ready to execute in phases
> **Counterpart**: `docs/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CODEX.md` (Codex-owned)
> **Filename contract**: Defined in Codex's plan section "Document ownership and naming contract" — do **NOT** merge; keep two independent designs for later comparison.
>
> **Revision 2 cross-review changes** (after reading Codex's plan in full):
> - **Phase ordering reworked**: pump helper skeleton now lands in shadow/observer mode BEFORE state machine fixes, before transport is redirected. This matches Codex's safer incremental order.
> - **`UpdatePosition()` is no longer being rewritten**. Instead a new helper `PumpToCurrentPositionFromQuery()` is added and called from `GetPosition()` / `GetPositions()` alongside the existing `UpdatePosition()`. This preserves AO's WaveRT contract surfaces (`m_ullLinearPosition`, `m_ullPresentationPosition`, `m_ullDmaTimeStamp`, packet counter carry-forward).
> - New Section 1.4 "AO wins to preserve" and new Section 4.0 "Non-negotiable rules (Codex + Claude merged)" make the "do not break" list explicit before any code changes.
> - Language around "position-query driven" softened to "**hybrid** model — main observed paired path is lazily advanced by position polling, while the shared timer subsystem still services auxiliary/shared activity" (Codex Claim 1 wording).
> - Phases 3~9 (was 3~7) renumbered; §5 Gap list, §7 Verification matrix, §8 Failure modes updated to match.
>
> **Revision 2.1 internal consistency pass** (after second cross-review):
> - All stale Phase number references fixed (§4.5, §5 Gap list, §5 closing, §8 numbering, §9 Risk register, §10.5 playbook, §11 Open Questions).
> - §8 Failure mode numbering deduplicated (8.4 duplicate → 8.5/8.6/8.7).
> - §9 R2 rewritten from "Phase 4 UpdatePosition rewrite" (stale) to "Phase 5/6 pump parallel execution with UpdatePosition" (current).
> - **New field `m_ulPumpLastBufferOffset`** added to Phase 1 struct additions. Pump now tracks its own buffer offset independent of `m_ullLinearPosition`. Decouples pump from WaveRT presentation math; prevents long-run drift (new Risk **R9**, new Open Question **OQ7**).
> - **§11 OQ6 resolved** — `UpdatePosition`'s carry fields (`m_ulBlockAlignCarryForward`, `m_hnsElapsedTimeCarryForward`, `m_byteDisplacementCarryForward`) are now explicitly preserved (they belong to `UpdatePosition`'s WaveRT contract role).
> - **§3.1 Codex Bottom line quote added** for alignment.
> - **§4.0 Rule 3** annotated with "Satisfied at Phase 5/6, not Phase 3" to avoid misreading Phase 3 observer mode as "already behaviorally meaningful".
>
> **Revision 2.5 changes** (sixth cross-review — Codex v4 absorption):
> - **Rule 15 added**: "Shadow comparison must be windowed, not naively per-call." Formalizes the rev-2.3 windowed-compare implementation as a rule so future phases cannot silently regress to per-call comparison. (From Codex v4 Rule 14.)
> - **Rule 16 added**: "Ownership-moving phases must prove runtime rollback in a live session." Phase 5/6 cannot close on "the flag exists" — they close on "we toggled the flag during a live call and audio stayed stable". Rollback must revert within 1~2 position queries. (From Codex v4 Rule 15.)
> - **Phase 5 exit criteria** gained explicit **WinDbg one-owner confirmation** bullet (Rule 13) and **1~2 position query rollback latency bound** (Rule 16). Previous rev had runtime smoke test but no latency threshold.
> - **Phase 6 exit criteria** symmetric with Phase 5: WinDbg one-owner BP on `FramePipeReadToDma`, plus bidirectional flag toggle requirement.
> - **§7/§9.1 polling-regime matrix enhanced** with Codex v4 diagnostic guidance: aggressive-only divergence → suspect conversion/handoff bugs first; sparse → OverJumpCount threshold sanity; near-no → verify timer auxiliary duties; mixed → Rule 13 per-client accounting check.
> - **§9.2 drift measurement** gained drift-direction interpretation hint (ahead vs behind tells you whether gate-skip accounting over- or under-counts).
> - **§4.0 enforcement** updated from "these 14 rules" to "these 16 rules", plus Rule 16 specific enforcement clause.
> - **Phase 3 exit criteria** now includes: (a) runtime feature-flag rollback proof as Phase-3 instance of Rule 16 (Phase 3 is the FIRST flag-gated phase, so it owns proving the rollback mechanism itself before Phase 5/6 depends on it); (b) explicit divergence counter meaning-window contract — counter is a parity signal only while legacy owns the path, becomes frozen historical evidence once `DISABLE_LEGACY_*` is set. Prevents Phase 5/6 verification from misreading frozen values as "live zero".
> - **§9.2 drift diagnosis order** clarified: exhaust conversion/accounting diagnosis BEFORE adding drift nudge logic. Unidirectional drift is almost always a fixable source bug; the nudge is a last-resort fallback that masks rather than fixes.
>
> **Revision 2.4 changes** (fifth cross-review — Codex v3 absorption + Claude self-audit):
> - **New §6.5 "Transport ownership matrix"** — table showing, for each phase, who owns each cable direction's transport. Makes Rule 13 ("one owner per direction per phase") mechanically auditable. Audit procedure: count transport call sites per direction; must equal 1; if 0 → regression; if >1 → double-advance bug. (From Codex v3 "Transport ownership matrix" section.)
> - **Rule 13 added**: "Each cable direction must have exactly one transport owner per phase." (From Codex v3 Rule 11.)
> - **Rule 14 added**: "Pump transport must not depend forever on WaveRT presentation offset." Formalizes the `m_ulPumpLastBufferOffset` design choice as a rule. (From Codex v3 Rule 13.)
> - **Phase 3 pipe counter race bug fix**: Previous rev 2.3 had `pFP->XxxCount = m_ulLocalCount` (copy assignment) which raced between Speaker and Mic streams sharing the same pipe. Fixed by splitting `FRAME_PIPE` counters into per-direction pairs (`RenderXxxCount` / `CaptureXxxCount`) and dispatching based on `m_bCapture`. `ioctl.h`, `adapter.cpp`, and Phase 3 code updated consistently.
> - **Phase 0 elevated WinDbg `GetPosition` pre-flight check**: Was OQ3. Now a blocking Phase 0 task — without confirming the entry point actually fires, the pump wiring in Phase 3 would be dead code. Phase 0 duration increased from 0.5 day to 1 day to accommodate this. (From Codex v3 Phase 0 and Codex v3 implementation order step 2.)
> - **Phase 4 elevated `SpeakerActive`/`MicActive` clearing analysis**: Was OQ4. Now a blocking Phase 4 task — must trace current clearing path before moving `FramePipeUnregisterFormat` out of STOP, otherwise stale `Active` flags break the Phase 4 `if (!otherSideActive)` conditional reset. Idempotency requirement added.
> - **Fade-in extract moved from Phase 6 to Phase 4**: The `ApplyFadeInRamp` helper extraction is now a pure refactor in Phase 4 (as a "Phase 4a" sub-task), before Phase 6 uses it. Previous rev had Phase 6 doing both the extract and the new call site in one phase — mixing refactor with behavior change. (From Codex v3 implementation order step 8.)
> - **Phase 1 renamed**: "Struct / diagnostic field additions" → "Diagnostics and rollout scaffolding". Codex v3 naming emphasizes the rollback-safety purpose, not just fields. Same content, clearer intent.
> - **OQ8 added**: "Can `m_ulPumpLastBufferOffset` reuse an existing AO field?" — asked by Codex v3, resolved immediately in rev 2.4 as "stay dedicated per Rule 14".
>
> **Revision 2.3 self-audit pass** (fourth review, Claude caught inconsistencies introduced during rev 2.2):
> - **Phase 1 struct/file updates were incomplete in rev 2.2**. The new `InvocationCount` / `ShadowDivergenceCount` fields were referenced in Phase 3 code but never declared in Phase 1's `FRAME_PIPE` / `ioctl.h` / `adapter.cpp` / `FramePipeInit` / changelog. All five files now list the new fields consistently. Without this fix, the Phase 3 code example would not compile.
> - **Phase 6 had two contradictory versions of `WriteBytes()`** — one hard-coded `if (isCable) return;` (rev 2.1 leftover), one flag-gated `if (pumpOwnsCapture) return;` (rev 2.2 target). Consolidated to the flag-gated version. Same fix for `UpdatePosition`'s capture-branch guard. Dual-gate comment added to explain why both the caller and the function body check the flag (future-proofing against new call sites).
> - **Phase 3 shadow-compare was per-call**, which false-positives on rounding carry in `UpdatePosition`'s byte-per-ms math. Replaced with **windowed comparison**: pump accumulates `newFrames` and `legacyBytes` into rolling totals over `AO_PUMP_SHADOW_WINDOW_CALLS` (default 128) calls, then compares totals with a `max(16 frames, 2% of larger total)` tolerance. This averages out carry noise while still catching systematic drift. New fields: `m_ullPumpShadowWindowPumpFrames`, `m_ullPumpShadowWindowLegacyBytes`, `m_ulPumpShadowWindowCallCount`.
> - **Phase 3 exit criterion tightened**: was `< 1%` divergence ratio per call — now `== 0` on the windowed counter (any tick means systematic drift, not rounding noise). Threshold tuning note updated accordingly. Cross-scenario check added: the exit criterion must hold for at least three different polling patterns.
> - **Phase 3 changelog entry** was still labeled "observer mode" with "call pump helper before UpdatePosition" — both stale from rev 2.1. Rewrote to reflect shadow-and-compare mode and the AFTER-UpdatePosition call order.
> - **Phase 4 Pre-conditions / Expected-live-behavior / Exit criteria** had Phase 3 vs Phase 4 references inverted from rev 2.1 carryover — Phase 3 in rev 2.2 is pump observer (no quality change expected), Phase 4 is state machine fix (where the pipe-collapse dropouts stop). Fixed all three sub-sections.
> - **Phase 5/Phase 6 verification** gained two new steps: "Phase 3/4 pump counter regression check" and "runtime rollback smoke test via IOCTL flag toggle".
> - **New §7.5 "Rollback checkpoints"** — consolidates Codex's Checkpoints A-E pattern into a single lookup table covering Phases 0~9 (Claude letters A-J). Each row lists expected state, first-to-suspect-if-broken, and rollback mechanism. Complements §8's per-scenario narratives.
>
> **Revision 2.2 cross-review changes** (after Codex released their v2 plan with major additions):
> - **Phase 3 upgraded from "observer mode" to "shadow-and-compare mode"**. The pump helper now runs alongside `UpdatePosition` and records divergence when its frame-delta math disagrees materially with `UpdatePosition`'s byte-per-ms math. Codex's Phase 1 "shadow-mode comparison path" pattern. Gives Phase 5/6 a quantitative parity signal before transport is moved.
> - **Feature flag system added** — `m_ulPumpFeatureFlags` bit field with `AO_PUMP_FLAG_ENABLE` / `AO_PUMP_FLAG_SHADOW_ONLY` / `AO_PUMP_FLAG_DISABLE_LEGACY_TRANSPORT`. Allows runtime toggle via IOCTL without rebuild. Critical for safe bring-up and fast rollback.
> - **Three new struct fields** in Phase 1: `m_ulPumpInvocationCount` (total calls), `m_ulPumpShadowDivergenceCount` (shadow-mode mismatches), `m_ulPumpFeatureFlags` (runtime bit flags).
> - **Two new §4.0 rules** — Rule 11 "Rollout must preserve a rollback path until parity is proven" (from Codex Rule 7), Rule 12 "Behavior beats offsets when the two disagree" (from Codex Rule 8).
> - **Phase 9 polling-regime validation added** — 5 scenarios (aggressive / normal / sparse / near-no / mixed) test pump robustness across client cadences, not just Phone Link.
> - **Phase 9 position drift measurement protocol**: quantitative criterion for `|m_ulPumpLastBufferOffset − (m_ullLinearPosition % DmaBufferSize)| ≤ 48 frames` over 30-minute run.
> - **§3.11 evidence normalization tags** — stream offsets now marked as `verified direct` / `verified after normalization` / `provisional` per Codex's caution about interior-pointer (`stream-8`) adjustments in `+0x6320`.

---

## 0. How this document is different from Codex's plan

Codex's plan is written as **architectural strategy** — high-level claims about VB's behavior model, rules of thumb, and principles to preserve or abandon. It is deliberately abstract so that the same design survives multiple refactor rounds.

This Claude plan is written as **tactical execution** — exact file paths, exact line ranges, exact current code, exact replacement code, exact WinDbg verification commands for each step. It is deliberately concrete so that a human or agent can open the file and know precisely which three lines to change.

The two plans are **complementary by design**:
- Use Codex's plan to decide *what principle to uphold*
- Use this Claude plan to decide *where to put the cursor and what to type*

If the two ever disagree in spirit, Codex's plan wins on architecture questions and this plan wins on code-location questions. If they disagree materially on facts, the confidence ranking (see §2) determines which one is correct.

---

## Table of Contents

0. How this document is different from Codex's plan
1. Why this plan exists
   - 1.1 Situation
   - 1.2 Goal of this plan
   - 1.3 Non-goals
   - **1.4 AO wins to preserve** (new in revision 2)
2. Source corpus + confidence ranking
3. Ground truth: what VB-Cable actually does (hybrid model)
4. Current AO state assessment (file-by-file walkthrough)
   - **4.0 Non-negotiable rules** (Codex 6 + Claude 4, new in revision 2)
   - 4.1 ~ 4.6 file-by-file
5. Concrete gap list: AO vs VB (17 gaps, G1~G17)
6. Phase-by-phase implementation plan
   - **6.0 Design principle: helper separation** (new in revision 2)
   - Phase 0: Pre-flight
   - Phase 1: Struct/diagnostic field additions
   - Phase 2: Format normalization corrections
   - **Phase 3: Pump helper in shadow-and-compare mode** (observer mode in rev 2, upgraded to shadow-and-compare in rev 2.2)
   - Phase 4: State machine alignment (was Phase 3)
   - **Phase 5: Pump drives render transport** (new in revision 2)
   - **Phase 6: Pump drives capture transport** (new in revision 2)
   - Phase 7: SRC quality improvements (was Phase 5)
   - Phase 8: Shared timer + notification refinement (was Phase 6)
   - Phase 9: Validation hardening (was Phase 7)
7. Verification matrix per phase
8. Failure modes and rollback policy
9. Risk register
10. Tools, commands, and live-debug playbook
11. Open questions (with resolution path)
12. Success definition (1차 목표, 2차 목표)
13. Appendix A: VB struct offset map (cross-referenced)
14. Appendix B: IAT and kernel APIs used by VB
15. Appendix C: Glossary

---

## 1. Why this plan exists

### 1.1 Situation

- AO already has a working "fixed pipe" (FRAME_PIPE) transport model in `Source/Utilities/loopback.cpp` (lines 1396~1960).
- AO already calls `UpdatePosition()` on-query from `GetPosition()` and `GetPositions()` — see `Source/Main/minwavertstream.cpp` lines 800~816 and 1075~1089.
- AO already calls `FramePipeRegisterFormat` on RUN and `FramePipeUnregisterFormat` on STOP.
- AO already calls `KeFlushQueuedDpcs()` on Pause before subsequent state work.
- AO's live call quality is still **visibly worse** than VB on Phone Link (CLAUDE.md: "VB-Cable: 깨끗함 / AO Cable: 왜곡 심함").

This gap is **no longer** a research problem. We have the ground truth. The remaining problem is the translation layer between that ground truth and AO's existing source code, plus the runtime discipline to land those changes without regressing the many existing AO wins.

### 1.2 Goal of this plan

- Close every behaviorally-meaningful gap between AO and VB for the live-call steady-state path.
- Do so **without rewriting FRAME_PIPE** (it is already close).
- Do so **without regressing** the existing 16ch / multi-format / multi-client / Control Panel wins.
- Do so **incrementally**, so that each phase can be built, installed, tested, and either accepted or reverted.

### 1.3 Non-goals

- **Not** "build a byte-for-byte clone of VB-Cable". The licensing risk alone makes that the wrong goal. The goal is **behavioral equivalence** on the call-quality-critical path.
- **Not** "produce a new single-file rewrite". The merge/diff story must remain reviewable.
- **Not** "delete AO's timer model on day one". See §6 Phase 8.
- **Not** "rewrite SRC as sinc with prettier coefficients". VB uses weighted linear + GCD and it sounds clean; follow suit.

### 1.4 AO wins to preserve (MUST NOT break during this rewrite)

This is the merged list from Codex's plan §"What AO already has that we should keep" plus Claude's own survey of the current source. Any phase that would regress one of these items must stop and re-plan.

**Transport model (already VB-parity or better):**
- `FRAME_PIPE` (`loopback.h` line 214~272, `loopback.cpp` line 1396~1960) is already INT32 + frame-indexed + hard-reject overflow + per-direction scratch buffers. **Do not rewrite this structure.**
- `FramePipeWriteFrames` at `loopback.cpp:1403-1461` already implements hard-reject overflow (`if (frameCount > freeFrames) { DropCount += frameCount; return 0; }`). This matches CLAUDE.md rule 3.
- `FramePipeReadFrames` at `loopback.cpp:1471-1552` already implements startup-phase silence guard, empty-ring silence fill, partial-read tail zero-fill with underrun counter. Keep the startup phase logic — VB does not have it but it's an AO-side quality win that prevents first-call garbage.
- AO's `FillFrames` explicit counter (vs VB deriving fill from `WritePos - ReadPos`) is **safer** than VB because it resolves the full/empty ambiguity without a reserved slot. Keep it.
- Per-direction scratch buffers `ScratchSpk` / `ScratchMic` (vs VB's single shared scratch) are **architecturally better** because Speaker DPC and Mic DPC can run on separate cores without contention. Keep them.

**Position query pattern (already the correct shape):**
- `GetPosition` at `minwavertstream.cpp:790-817` already calls `UpdatePosition(ilQPC)` when `m_KsState == KSSTATE_RUN`. The function shape matches VB's `+0x5420 → +0x6320` pattern. **Do not change the shape.** The change is in what gets computed inside, not where it's called from.
- `GetPositions` at `minwavertstream.cpp:1056-1098` has the same pattern and must be treated identically.
- The `m_PositionSpinLock` acquisition pattern (line 798, 862, 947, 1075) is correct lock-ordering discipline. **Do not move work outside this lock.**

**State machine partial correctness:**
- `SetState` PAUSE branch at `minwavertstream.cpp:1267-1304` already has the comment "do NOT unregister on Pause" and already calls `KeFlushQueuedDpcs()` at line 1284. The direction is correct, only the missing conditional ring reset needs adding.
- ACQUIRE handler (line 1260-1265) is already a correct no-op.

**AO-specific improvements over VB:**
- Fade-in ramp for capture streams (`minwavertstream.cpp:1570-1656`, supports 16-bit / 24-bit / 32-bit float). VB does not have this — it's a pop/click prevention win AO should keep.
- Block-align carry-forward in `UpdatePosition` prevents sample-boundary drift. VB does not have this exact mechanism — keep for non-cable paths.
- `C_ASSERT` struct layout guards at compile time. Critical for catching `loopback.h` ↔ `adapter.cpp` drift. Keep.
- Multi-client render count tracking in `FRAME_PIPE.ActiveRenderCount`. Supports AO's multi-client capture goal. Keep.

**Build / test / install / diagnostic infrastructure:**
- `build-verify.ps1`, `install.ps1`, `verify-install.ps1` — all M1 hardened. Do not change build pipeline as part of this rewrite.
- `test_stream_monitor.py` diagnostic binding to `ioctl.h` struct (CLAUDE.md "Diagnostics Rule" three-file sync). Keep discipline; extend only.
- `tests/live_call/` harness (M3 complete). Critical validation tool. Do not depend on it breaking during rewrite.
- Existing IOCTL SET/GET_CONFIG surface. Control Panel depends on this. Keep compatibility.

**What this list means for this plan:** every Edit in §6 phases must preserve the above. If any phase's code change conflicts with this list, **stop and re-plan that phase**.

---

## 2. Source corpus + confidence ranking

### 2.1 Evidence classes
Every factual claim in this document is labeled with one of the following tags:

- **[DYN]** — Live WinDbg breakpoint / register / callstack evidence captured 2026-04-13.
- **[STAT]** — Direct static disassembly from `results/ghidra_decompile/vbcable_all_functions.c` or `results/vbcable_func26a0.asm`.
- **[DOCO]** — VB-Cable official reference manual (`docs/VBCABLE_ReferenceManual.pdf`).
- **[CLDE]** — Prior Claude analysis (`results/vbcable_runtime_claude.md`, `results/vbcable_pipeline_analysis.md`, `results/vbcable_disasm_analysis.md`).
- **[CDXS]** — Prior Codex analysis (`docs/VB_CABLE_DYNAMIC_ANALYSIS.md`, `docs/VB_CABLE_AO_COMPARISON*.md`).
- **[INF]** — Interpretation / inference not directly observed.

### 2.2 Confidence ranking when sources conflict

1. **[DYN]** beats everything (register values and live stacks don't lie).
2. **[STAT]** beats **[CLDE]**/**[CDXS]** summaries (source code beats summaries).
3. **[CLDE]** and **[CDXS]** are approximately peer-rank — cross-check both.
4. **[DOCO]** is ground truth for *intent* but not for *implementation*.
5. **[INF]** is lowest — must be explicitly labeled as interpretation.

### 2.3 Source files read for this plan

**Static RE corpus:**
- `results/ghidra_decompile/vbcable_all_functions.c` (12,096 lines, 297 functions)
- `results/ghidra_decompile/vbcable_function_index.txt` (function map with sizes)
- `results/vbcable_disasm_analysis.md` (FUN_26a0 deep dive)
- `results/vbcable_pipeline_analysis.md` (full pipeline analysis — this is the origin of CLAUDE.md's 10 principles)
- `results/vbcable_func26a0.asm` (raw disassembly, 49KB)

**Dynamic debug corpus:**
- `results/vb_session.log` (oversized live WinDbg session, 398KB)
- `results/vbcable_runtime_claude.md` (21 sections, runtime discoveries)
- `docs/VB_CABLE_DYNAMIC_ANALYSIS.md` (Codex dynamic session, 28 sections)
- `docs/VB_CABLE_AO_COMPARISON_CODEX_NOTES.md` (Codex cross-validation)

**AO source corpus:**
- `Source/Utilities/loopback.h` (347 lines, FRAME_PIPE declarations)
- `Source/Utilities/loopback.cpp` (relevant ranges 1396~1960 for FRAME_PIPE)
- `Source/Main/minwavertstream.cpp` (relevant ranges 164~1716 for state machine + position + bytes path)
- `Source/Main/minwavertstream.h` (full, 200 lines)
- `Source/Main/adapter.cpp` (IOCTL + init, referenced but not re-read in full — known structure)
- `Source/Main/ioctl.h` (IOCTL struct definitions)
- `CLAUDE.md` (project root, 10 principles and operational rules)

**Planning counterparts:**
- `docs/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CODEX.md` (Codex plan, read in full for filename contract and non-overlap)
- `docs/VBCABLE_SURPASS_PLAN.md` (historical milestones, M1~M6 status)
- `docs/VB_CABLE_PATH_ANALYSIS.md` (telephony passthrough rationale)
- `docs/VB_CABLE_AO_COMPARISON.md` (concrete side-by-side diff table)

### 2.4 Notation convention

When I cite a VB function I use the absolute offset form `+0x5634` without the `FUN_140005634` prefix.
When I cite AO code I use `file.cpp:LINE` or `file.cpp:LINE-LINE` form.
When I cite a VB struct field I use `stream+0x168` or `ring+0x18` form.

---

## 3. Ground truth: what VB-Cable actually does

This section is the **fact base**. Phase plans in §6 refer back to specific numbered items here. Each item is tagged with evidence class.

### 3.1 Top-level transport architecture [DYN][STAT] — HYBRID MODEL

**[IMPORTANT — revision 2 nuance update]** Codex's plan Claim 1 rightfully resists the over-simplification "VB is purely position-query driven, the timer is a lie." The stronger combined reading of the evidence is:

> *VB's main observed paired path appears to be **lazily advanced by position-query polling**, while the shared timer subsystem **still exists and services auxiliary/shared activity**.*

This Claude plan adopts that hybrid wording. The §6 phases still focus on the position-query path because that is where AO's current call-quality gap lives, but **the timer subsystem is not "wrong", only "secondary"**. Phase 8's job is to re-role AO's timer as aux support, not to delete it.

With that understood, here are the two observed call paths to the main write primitive `+0x22b0`:

**Path A — Position query driven (main observed paired path during live calls):**
```
User-mode polls KSPROPERTY_AUDIO_POSITION (hundreds to thousands per second)
  → ks!KspPropertyHandler
  → portcls!PinPropertyPositionEx
  → portcls!CPortPinWaveRT::GetKsAudioPosition
  → vbaudio+0x5420  (position query internal helper — acquires per-stream lock at stream+0x160)
  → vbaudio+0x6320  (elapsed-frame data processor)
    ├─ if (frames < 8) return                           ← 8-FRAME GATE
    ├─ if (frames > wrap_size/2) over-jump error        ← DMA overrun guard
    ├─ render branch → vbaudio+0x6adc  (mic-side-like service; not fully disassembled)
    └─ capture branch → vbaudio+0x5634 (main speaker-side DMA→scratch→ring wrapper)
       └─ vbaudio+0x22b0 (core ring write primitive)
```

**Path B — Shared timer driven (AUXILIARY):**
```
ExTimer (1ms HIGH_RESOLUTION) fires
  → vbaudio+0x5cc0 (shared timer callback)
    ├─ iterates global stream array at vbaudio+0x12f90
    ├─ for each active stream: calls auxiliary write paths
    │   (+0x6293 site, +0x62e1 site — both call +0x22b0 directly with different parameters)
    ├─ runs envelope/VU meter helper (+0x51a8)
    └─ runs peak smoothing (+0x4f2c)
```

Live hot-path BP trace during Phone Link call with speech:
- `+0x5634` fires ~5 Hz (every ~200ms), `edx` carries frame count in 2600~3100 range
- Each `+0x5634` is immediately followed by ~3 `+0x22b0` calls (one inside `+0x5634` at 0x5735, two from the shared timer callback at 0x6293/0x62e1)
- `+0x68ac` does NOT fire during the observed window — it is a higher-level dispatcher that is conditional, not hot-path
- `+0x10ec` does NOT fire during the observed window — it is a ReadPos trim helper used only under specific conditions

**Key conclusion [INF] — hybrid model wording**: During the observed live call window, the main paired path appeared to be **lazily advanced by position-query polling**. The timer subsystem was still live and was writing to auxiliary rings on the same intervals. **Neither can be dismissed.** The implication for AO is: the position-query entry point must become behaviorally meaningful (not just return fresh timestamps), but the timer must **also** be retained for auxiliary duties. The §6 phase plan reflects this: pump helper lands first (Phases 3/5/6), state semantics align (Phase 4), timer re-roles afterward (Phase 8, optional).

> **Codex plan §Bottom line (quoted for alignment)**:
> *"VB has a timer, but the main observed paired transport path appears to be lazily advanced by position-query polling, and VB protects that model with an explicit 8-frame gate. Therefore the AO rewrite should be built around: query-driven pump/update for the main paired path, frame-pipe transport primitives as the actual data movers, timer as auxiliary/shared support, VB-like STOP/PAUSE semantics."*

This Claude plan operationalizes those four bullets into Phase 3 (pump helper in shadow-and-compare mode), Phases 5/6 (pump drives transport via the frame-pipe primitives), Phase 8 (timer re-roled as auxiliary), and Phase 4 (STOP/PAUSE alignment) respectively.

### 3.2 Stream state machine (vbaudio+0x5910) [STAT]

Switch statement after the preamble / parent-interface notification call:

```
state = 0 (STOP)    → jump to +0x5aca   STOP handler
state = 1 (ACQUIRE) → fall through, no-op (only state is written at epilogue)
state = 2 (PAUSE)   → jump to +0x5a39   PAUSE handler
state = 3 (RUN)     → fall through from ACQUIRE comparison, RUN handler
```

**RUN handler (~line 0x5973~0x5a38):**
```
lea rbx, [rdi + 0xF8]               ; stream+0xF8 lock
call [IAT+0x8000]                    ; KeQueryPerformanceCounter
r10 = [rbx]                          ; QPC frequency
; QPC raw → 100ns conversion via magic-number div
[stream+0x190] = 0
[stream+0x198] = 0
[stream+0x108] = elapsed_100ns       ; start timestamp A
[stream+0x1B0] = 0
[stream+0x1A8] = 0
[stream+0x178] = elapsed_100ns       ; start timestamp B
[stream+0x1A0] = [stream+0x70]       ; frames per tick
[stream+0x180] = 0
[stream+0x188] = 0
[stream+0x1C8] = 0
[stream+0x110] = 0
call FUN_140004cf4                   ; check other-side stream status
if (eax != 0) → skip timer register  ; (other stream branch already handled)
lea rcx, [rdi - 8]
call vbaudio+0x65b8                  ; register into shared timer
[stream+0x58] = rax                  ; timer handle saved
```

**PAUSE handler (+0x5a39~0x5acb):**
```
cmp [rdi+0xB4], 2
jle exit                             ; only if previous state > PAUSE (i.e. was RUN)
cmp [rdi+0x58], 0
je skip_timer                        ; if no timer handle, skip
lea rcx, [rdi - 8]
call vbaudio+0x669c                  ; unregister from shared timer
[rdi+0x58] = 0
call [IAT+0x8100]                    ; KeFlushQueuedDpcs   ★ CLAUDE.md rule 10 ★
mov rcx, [rdi+0x168]                 ; ring pointer
test rcx, rcx
je skip_ring_reset
call vbaudio+0x39ac                  ; ring reset            ★ CLAUDE.md rule 10 ★

skip_ring_reset:
; clear per-stream stats fields
[rdi+0xB0] = 0
[rdi+0xF0] = 0                       ; plus more fields
```

**Critical observation [STAT][CDXS§2]**: The PAUSE path's ring reset is **conditional** on:
- previous state > PAUSE (i.e. was RUN)
- timer handle was non-null
- ring pointer was non-null

If any condition fails, the ring is NOT reset on PAUSE. This is a specific and important nuance that AO's current code does not honor. AO's comment at `minwavertstream.cpp:1275-1278` explicitly says "do NOT unregister on Pause" which is partially correct, but AO also does not conditionally reset the ring on PAUSE either way.

**STOP handler (+0x5aca~0x5b26):**
```
lea rbx, [rdi+0x160]                 ; stream-level spinlock
mov rcx, rbx
call [IAT+0x80e0]                    ; KeAcquireSpinLockRaiseToDpc
; Clear state fields:
[rdi+0xF0] = 0
[rdi+0xE8] = -1                      ; position sentinel
[rdi+0xEC] = -1
[rdi+0xC8] = 0
[rdi+0xD0] = 0
[rdi+0xD8] = 0
[rdi+0xE0] = 0
[rdi+0x1B0] = 0
[rdi+0x74] = 0
[rdi+0x15C] = 0
call [IAT+0x80e8]                    ; KeReleaseSpinLock
```

**Critical observation [STAT][CDXS§2]**: **STOP does NOT call `+0x39ac`.** The ring is left as-is. AO currently calls `FramePipeUnregisterFormat` on STOP, which internally resets the pipe. This is a **divergence** that must be corrected.

### 3.3 Position query helper (vbaudio+0x5420) [STAT]

From the live callstack `portcls!GetKsAudioPosition+0x5d → vbaudio+0x54bb`, and static review:

```
+0x5420 prologue: save callee-saved regs, sub rsp, 0x20
+0x5439: lea rbp, [rcx + 0x160]       ; stream+0x160 spinlock
+0x5440: mov rdi, rcx
+0x5443: mov rcx, rbp
+0x5446: mov rsi, rdx                  ; rsi = out-param pointer
+0x5449: call [IAT+0x80e0]             ; KeAcquireSpinLockRaiseToDpc
+0x544f: mov r14b, al                  ; save old IRQL

+0x5452: mov rax, [rdi+0x98]           ; stream parent / port driver
+0x5459: mov ecx, [rax+0x120]          ; ctx id
+0x545f: call vbaudio+0x4080           ; get context by id → rbx
+0x5464: mov rbx, rax
+0x5467: test rax, rax
+0x546a: je +0x5483                    ; no ctx → skip counters

+0x546c: cmp [rdi+0x9C], 0             ; render (1) or capture (0)
+0x5473: je +0x547d
+0x5475: inc [rax+0x198]               ; render-side counter
+0x547b: jmp +0x5483
+0x547d: inc [rax+0x170]               ; capture-side counter

+0x5483: cmp [rdi+0xB4], 3             ; state == RUN?
+0x548a: jne +0x54d7                   ; not RUN → skip pump
+0x548c: cmp [rdi+0xB0], 0              ; some gate flag
+0x5493: jne +0x54d7                   ; gate set → skip pump

+0x5495: mov rax, [rdi+0x180]          ; cached QPC
+0x549c: test rax, rax
+0x549f: jne +0x54a9                   ; use cached
+0x54a1: xor ecx, ecx
+0x54a3: call [IAT+0x8000]             ; KeQueryPerformanceCounter (no freq)
+0x54a9: lea rcx, [rdi - 8]
+0x54ad: mov r8d, 1                     ; direction flag = 1
+0x54b3: mov rdx, rax                   ; QPC value
+0x54b6: call vbaudio+0x6320           ; PUMP: elapsed→frames→write
+0x54bb:  (callstack evidence return site)

+0x54bb: test rbx, rbx
+0x54be: je +0x54d7
+0x54c0: cmp [rdi+0x9C], 0
+0x54c7: je +0x54d1
+0x54c9: inc [rbx+0x18C]                ; render processed counter
+0x54cf: jmp +0x54d7
+0x54d1: inc [rbx+0x164]                ; capture processed counter

+0x54d7: mov rax, [rdi+0xC8]           ; sample position out
+0x54de: mov dl, r14b                   ; restore IRQL
+0x54e1: mov [rsi], rax                  ; output->PlayOffset (first dword)
+0x54e4: mov rcx, rbp
+0x54e7: mov rax, [rdi+0xD0]           ; buffer position out
+0x54ee: mov [rsi+8], rax                ; output->WriteOffset (second dword)
+0x54f2: call [IAT+0x80e8]               ; KeReleaseSpinLock
; return 0
```

This decoding confirms [CDXS§26] and [CLDE§15] simultaneously.

### 3.4 Data processor (vbaudio+0x6320) [STAT][DYN]

Input: `rcx = stream - 8`, `rdx = currentQPC`, `r8d = 1`.
Note the `-8` offset — interior pointer adjustment. Most field references use `rdi` where `rdi = rcx + 8 = stream`.

**Elapsed-to-frames calculation** (lines near 0x6380~0x63be):
```
r9 = [stream+0x100]                    ; stored QPC frequency
rdx:rax = currentQPC × 10,000,000       ; convert to 100ns units via 128-bit mul
rax ÷ r9 → currentQPC_100ns
elapsed_100ns = currentQPC_100ns - [stream+0x180]   ; baseline
r8d = [stream+0x8C]                    ; sample rate
rcx = elapsed_100ns × r8d               ; elapsed × SR
; magic multiplier 0xD6BF94D5E57A42BD, sar 23 → divide by 10,000,000
rdx = rcx × magic >> 23 + sign_correct  ; total frames since baseline
ebx = edx                              ; total frames (32-bit)
ebx -= [stream+0x198]                  ; minus already-processed frames
; ebx = new frames this call
```

**★ 8-FRAME GATE ★** (lines 0x63c4~0x63c7):
```
cmp ebx, 8
jl  +0x65a0                            ; skip entire processing, go to epilogue
```

**Sample-count baseline rebase** (prevents integer overflow for long runs):
```
mov eax, edx
mov [stream+0x198], rax                ; save processed count
r8d = [stream+0x8C] << 7                ; sampleRate * 128
cmp edx, r8d
jb  no_rebase                          ; hasn't reached wrap
[stream+0x198] = 0                     ; reset processed count
[stream+0x180] = r9                    ; new baseline QPC
no_rebase:
```

**Over-jump guard** (DMA overrun protection):
```
r9d = [stream+0xA8]                    ; DMA wrap size
cl  = [stream+0xA4]                    ; render/capture byte
eax = r9d >> 1                         ; half wrap
cmp ebx, eax
jbe normal_path
; over-jump error:
test cl, cl
je  capture_over_jump
; render over-jump:
inc [ctx+0x180]                        ; render over-jump counter
cmp ebx, [stream+0xA8]
jbe +0x65a0                            ; over half but under full → skip only
inc [ctx+0x184]                        ; render massive over-jump counter
jmp +0x65a0
capture_over_jump:
inc [ctx+0x158]                        ; capture over-jump counter
...
jmp +0x65a0                            ; skip
```

**Render vs capture dispatch** (lines 0x644d~0x6495):
```
normal_path:
test cl, cl
je  capture_dispatch
; render dispatch:
r8 = [stream+0xD0]                      ; current DMA pos
edx = ebx                              ; frame count
rcx = rdi
call vbaudio+0x6adc                    ; render helper (mic-like pull?)
; advance position with wrap
[stream+0xD8] = [stream+0xD0]
rdx = [stream+0xD0] + rcx               ; (wrong regs — conceptual)
if rdx >= wrap: rdx -= wrap
[stream+0xD0] = rdx
jmp +0x658a

capture_dispatch:
; frame-count clamping to avoid overshoot
cmp [stream+0x164], 0
je  direct_call
; clamp logic using [stream+0x7C] and [stream+0xA8]
; ...
direct_call:
r8 = [stream+0xD0]
edx = ebx
rcx = rdi
call vbaudio+0x5634                    ; capture service wrapper (main write)
; advance position (similar wrap logic as render)
```

**Accounting update** (lines 0x658a~0x65a0):
```
eax = ebx
[stream+0xE8] += rax                    ; total sample counter #1
[stream+0xE0] += rax                    ; total sample counter #2
[stream+0x1B8] += ebx                   ; latest frame count

+0x65a0:
[stream+0x1D0] = esi (dir flag)
epilogue
```

### 3.5 Ring write primitive (vbaudio+0x22b0) [STAT][CDXS§16]

**Parameter validation prolog:**
```
if ([ring+0x24] == 0)       return -1   ; ring not valid
if (frames == 0)            return -2
if (r9d channels < 1)       return -3
if (samplerate < 8000)      return -4
if (samplerate > 200000)    return -5
```

Note: VB checks `[ring+0x24]` (valid flag), not `[ring+8]` (data offset). The earlier summary in `results/vbcable_pipeline_analysis.md` conflated these; the decompile shows a flag-like check. [INF] I believe this is a separate field.

**Rate mismatch fallback:**
```
if ([ring+0x20] != 0 && [ring+0x20] != samplerate):
    call vbaudio+0x26a0                  ; heavy SRC path
    return
```

**Same-rate write — 32-bit (0x78C) branch:**
```
; count = frames / (channels*4) … but this is confusing and not fully decoded here
; for the direct copy case:
if (count >= 4 AND no-overlap):
    call vbaudio+0x7680 (RtlCopyMemory equivalent)
else:
    per-sample copy loop
```

**Same-rate write — 16-bit (0x10) branch:**
```
eax = (INT16)src[idx]
shl eax, 3                             ; ★ 16-bit → 19-bit normalized
ring[dst_idx] = eax
```

**Same-rate write — 24-bit (0x18) branch:**
```
eax = byte[0] | (byte[1] << 8) | (byte[2] << 16)
shl eax, 8
sar eax, 13                            ; ★ 24-bit → ~19-bit normalized
ring[dst_idx] = eax
```

**Same-rate write — 8-bit (0x08) branch:**
```
eax = (u8)src[idx] - 0x80              ; unsigned → signed
shl eax, 0xB                           ; ★ 8-bit → 19-bit normalized
ring[dst_idx] = eax
```

**Overflow reject:**
```
available = [ring+0x18] - [ring+0x1C] + [ring+0x14] - 2
; [ring+0x18] = WritePos, [ring+0x1C] = ReadPos, [ring+0x14] = WrapBound
; -2 for 2-frame headroom
if (count >= available):
    inc [ring+0x180]                   ; overflow counter
    return -9                          ; hard reject
```

**Silence pad (conditional on [rsp+0xB0] flag):**
```
if ([rsp+0xB0] flag set && position < threshold):
    bytes_to_pad = (threshold - position) & ~3
    call vbaudio+0x7940 (memset 0)
```

### 3.6 Ring read primitive (vbaudio+0x11d4) [STAT][CDXS§15]

Mirror structure of `+0x22b0` but:
- Takes frames out of the ring
- Denormalizes INT32 → target format using the reverse shifts
- On underrun (fill < requested): silence-fills with 0 (for 16/24/32 bit) or 0x80 (for 8 bit)
- On rate mismatch: falls back to `+0x17ac` (read-side heavy SRC)

**Read denormalization, by target format:**
- 8-bit: `(INT32 >> 0xB) + 0x80`
- 16-bit: `(INT32 >> 3)`
- 24-bit: `(INT32 << 5) >> 8` written as three bytes
- 32-bit (0x78C): direct copy

**Read SRC (same-rate loop pattern):**
```
eax = current_read_idx
imul eax, stride_ratio                 ; rate-ratio index scale
rcx = rax + base_offset
eax = ring[rcx*4]                       ; pick sample (nearest-neighbor)
output[dst_idx] = eax
```

[STAT] The read-path same-rate loop does **not** show the weighted accumulator pattern that `+0x26a0` uses. This is the asymmetry that CLAUDE.md principle 6 flags: write SRC is high-quality polyphase linear; read SRC is nearest-neighbor. This is AO's clearest quality-improvement opportunity.

### 3.7 Heavy SRC write path (vbaudio+0x26a0) [STAT]

**Entered from `+0x22b0` only when `ring.internal_rate != source_rate`.**

Structure (simplified from 4,808-byte function):
1. **Format dispatch** (8/16/24/0x78C) — convert input to ring INT32 domain
2. **GCD ratio detection** — try 300, 100, 75 in order; fall back to generic; if no match, return `-486`
3. **Over-budget check** — compute required frame count and compare to ring headroom; if overflow, increment `[ring+0x180]` and return `-3`
4. **Weighted accumulate resampling** — for each input sample, split into weighted contributions across multiple output samples using per-channel stack accumulators (`[rbp-0x70]`) and residual carry (`[rbp-0x30]`)
5. **Persistent carry state** written back to `[rdx+0x34]` and `[rdx+0x38+ch*4]` (SRC state stored on the ring structure itself across calls)

**Key math pattern** (24-bit branch, roughly):
```
for each input_sample:
    raw = decode_to_int19()                        ; 19-bit normalized
    contribution = (raw * weight) / total          ; weight = r9d, total = r13d
    output_accumulator[ch] += contribution
    remainder = raw - contribution                  ; energy conservation
    carry[ch] = remainder                            ; stored for next iteration
```

This is a form of polyphase linear resampling with integer accumulation — more sophisticated than 2-tap linear but cheaper than sinc. It's the "right level of quality" for a general-purpose audio cable.

### 3.8 Shared timer infrastructure (vbaudio+0x65b8 / +0x669c / +0x5cc0) [STAT][DYN]

**+0x65b8 — stream register / timer start:**
```
if (global refcount == 0):
    zero stream array (vbaudio+0x12f90, 9 slots of 8 bytes each)
    zero timer handle (vbaudio+0x12fd8)
    ExAllocateTimer(callback=vbaudio+0x5cc0, context=0, attributes=4)   ; 4 = EX_TIMER_HIGH_RESOLUTION
    [vbaudio+0x12fd8] = new_handle
    ExSetTimer(handle, due_time=-10000 /* 1ms */, period=10000 /* 1ms */, params=0)

; Insert stream into first empty slot:
for i in 0..9:
    if stream_array[i] == 0:
        stream_array[i] = current_stream_ptr
        break
[vbaudio+0x12f84]++  (global refcount)
```

**+0x669c — stream unregister / timer stop:**
```
; Walk stream array, clear matching slot
for i in 0..9:
    if stream_array[i] == current_stream_ptr:
        stream_array[i] = 0
        break
[vbaudio+0x12f84]--  (global refcount)
recompute high_water_slot_count at [vbaudio+0x12f88]

if refcount == 0:
    ExDeleteTimer(handle, 1, 0, 0)
    [vbaudio+0x12fd8] = 0
    [vbaudio+0x298] = 0   (related global timing baseline)
    [vbaudio+0x2A0] = 0
    [vbaudio+0x2A8] = 0
```

**+0x5cc0 — shared timer callback:**
Iterates the stream array and services each active stream with auxiliary work — does **not** carry the main data path during the observed hot path (`+0x68ac`, `+0x6adc`, `+0x10ec` all remained silent during live call trace). Does call `+0x22b0` at sites `+0x6293` and `+0x62e1` for auxiliary rings.

**[INF]** The exact distinction between "main paired ring" (observed at `ffff968f'b0320000` during trace) and "auxiliary rings" (`ffff968f'b2aa0000`, `ffff968f'b27e0000`) is not fully clear. My best interpretation is that the main ring is the currently-paired Speaker/Mic stream and the auxiliaries are additional consumer rings that the shared timer fans data out to — supporting VB's multi-client capture fan-out. This is plausible but not definitively proven. The plan treats auxiliary rings as out-of-scope for Phase 1~5 and revisits in Phase 7.

### 3.9 Position reporting second interface (vbaudio+0x4598 / +0x4664) [STAT][CLDE§5.2]

This is a **separate** position query path — not the one that the live trace showed going through `+0x5420`. `results/vbcable_pipeline_analysis.md` §5.2 documents `+0x4598` as a different KS property handler that:
- Locks `+0x168` spinlock (note: different lock from `+0x5420`'s `+0x160`)
- Reads cached QPC from `+0x188` (or fresh if zero)
- If state == RUN and not paused: calls `+0x6320` to refresh position
- Returns `[stream+0xE0]` (PlayPosition) and `[stream+0xE8]` (WritePosition)

And `+0x4664` converts total-bytes to DMA-buffer-relative position using a scale factor.

**[INF]** It looks like VB has multiple position-query entry points with slightly different conventions. They all end up calling `+0x6320` to do the actual frame-delta work. For AO's purposes, the important property is that **both** paths use position query as an active pump trigger, not a passive counter read.

### 3.10 Ring buffer struct layout [STAT][CLDE§11]

Consolidated from `vbcable_pipeline_analysis.md` §11, `vbcable_disasm_analysis.md` §2, and Codex `VB_CABLE_DYNAMIC_ANALYSIS.md` §"Ring Structure":

| Offset | Size | Field | Notes |
|---|---|---|---|
| +0x00 | 4 | SampleRate (actual) | matches InternalRate at init |
| +0x04 | 4 | SampleRate (requested) | from constructor param |
| +0x08 | 4 | RingDataOffset (signed) | relative to struct base, = 400 |
| +0x0C | 4 | FrameCapacity | `frames` from init |
| +0x10 | 4 | TotalSlots = Channels × Frames | `total_samples` |
| +0x14 | 4 | WrapBound | = FrameCapacity, used for modulo |
| +0x18 | 4 | WritePos (frame index) | wraps at WrapBound |
| +0x1C | 4 | ReadPos (frame index) | wraps at WrapBound |
| +0x20 | 4 | InternalRate | for rate-ratio comparison |
| +0x24 | 4 | Valid flag / allocated size | checked at write-path entry |
| +0x28 | 4 | FrameStride = Channels × 4 | byte stride per frame |
| +0x2C | 4 | DoubleStride | Channels × 8 |
| +0x30 | 4 | Allocated flag | 1 = driver owns memory |
| +0x34 | 4 | SRC fractional accumulator | polyphase carry |
| +0x38 | 64 | SRC history per channel | 16 × INT32 |
| +0xB8 | 4 | SRC prev-sample accumulator | |
| +0xBC | 64 | SRC prev samples per channel | 16 × INT32 |
| +0x13C | 64 | SRC prev-sample state (down path) | |
| +0x174 | 4 | Feature flags | bit 1 = per-channel volume |
| +0x17C | 4 | (counter / flag, reset on +0x39ac) | |
| +0x180 | 4 | **Overflow counter** | write rejected → increment |
| +0x184 | 4 | Underrun counter (SRC path) | |
| +0x188 | 4 | Underrun flag (same-rate read) | |
| +0x190 | 400 | Ring data area begins | INT32 samples |

Struct is allocated as one contiguous block: header (400 bytes) + ring data (`total_samples × 4` bytes). This is VB's choice; AO uses separate allocation which is fine.

### 3.11 Stream object layout [STAT][CDXS][CLDE]

This is the most frequently-cited structure in the plan. Offsets cross-validated from three sources.

**Evidence normalization tags (new in revision 2.2)**: Codex's plan Claim 2 warns about interior-pointer offsets in `+0x6320` (called with `rcx = stream - 8`). Within `+0x6320` most field references use `rdi = rcx + 8 = true stream base`, so the offsets shown in §3.4 are already normalized to the true stream base. Nonetheless, each field in the table below is tagged for the reader:

- **`[direct]`** — Offset observed from a function that received the stream object via `rcx` directly, with no arithmetic adjustment. High confidence.
- **`[norm]`** — Offset observed from `+0x6320` or similar helper that used `rcx = stream - 8`, already normalized in this plan by referring to `rdi = rcx + 8`. High confidence after normalization verification.
- **`[prov]`** — Offset cited from a summary document (CLDE / CDXS) without a direct re-check in the decompile. Lower confidence. **Do not use for AO field layout decisions without promotion to `[direct]` or `[norm]` first.**

Only `[direct]` and `[norm]` offsets should inform AO field design. `[prov]` entries are informational.

Per §4.0 Rule 12, if a `[prov]` offset conflicts with observed whole-function behavior during implementation, trust the behavior and re-verify the offset.

| Offset | Size | Field | Source |
|---|---|---|---|
| +0x50 | list head | **notification list** | [STAT §+0x5cc0 loop] |
| +0x58 | qword | timer handle | [STAT +0x5910 RUN path] |
| +0x60 | qword | auxiliary timer/flag | [STAT §+0x5cc0] |
| +0x70 | dword | frames per tick | [STAT +0x5910 RUN] |
| +0x74 | dword | previous-state arg | [STAT +0x5910] |
| +0x78 | dword | notification param | [STAT +0x5cc0] |
| +0x7C | dword | notification threshold | [STAT +0x6320] |
| +0x86 | word | bits per sample | [STAT +0x5634] |
| +0x88 | word | channels | [STAT +0x5634] |
| +0x8C | dword | **sample rate** | [STAT +0x6320] |
| +0x90 | qword | (parent pointer, old analysis) | [CLDE] |
| +0x98 | qword | **port driver ptr** | [STAT +0x5420] |
| +0x9C | byte | **render(1) / capture(0) flag** | [STAT +0x5420] |
| +0xA4 | byte | **isMic / direction flag** | [STAT +0x6320] |
| +0xA8 | dword | **DMA buffer wrap size** | [STAT +0x5634, +0x6320] |
| +0xB0 | qword | **DMA region base VA** | [STAT +0x5634] |
| +0xB4 | dword | **previous KSSTATE** (used at entry) | [STAT +0x5910] |
| +0xB8 | dword | "active / running" flag | [STAT +0x5cc0] |
| +0xBC | dword | current KSSTATE | [STAT +0x5cc0] |
| +0xC8 | qword | **sample position out** (PlayOffset) | [STAT +0x5420] |
| +0xD0 | qword | **buffer position out** (WriteOffset) | [STAT +0x5420, +0x6320] |
| +0xD8 | qword | previous buffer position | [STAT +0x6320] |
| +0xE0 | qword | accumulated sample counter | [STAT +0x6320] |
| +0xE8 | qword | accumulated sample counter | [STAT +0x6320, +0x4598] |
| +0xF0 | qword | state field cleared on STOP | [STAT +0x5910 STOP] |
| +0xF8 | lock/ptr | QPC freq storage on RUN init | [STAT +0x5910] |
| +0x100 | qword | **stored QPC frequency** (cached) | [STAT +0x6320] |
| +0x108 | qword | **start timestamp A** | [STAT +0x5910 RUN] |
| +0x158 | dword | DMA over-jump counter (capture side) | [STAT +0x6320] |
| +0x160 | spinlock | **position query lock** | [STAT +0x5420] |
| +0x164 | byte | notification-pending flag | [STAT +0x6320] |
| +0x165 | byte | notification-triggered flag | [STAT +0x6320] |
| +0x168 | qword | **ring pointer** | [STAT +0x5910 PAUSE] |
| +0x170 | qword | ring/context pointer for +0x6778 | [STAT +0x6778] |
| +0x178 | qword | **scratch buffer pointer** | [STAT +0x5634] |
| +0x180 | qword | **baseline QPC** (for elapsed calc) | [STAT +0x6320] |
| +0x188 | qword | cached QPC (set by +0x5cc0) | [STAT +0x4598] |
| +0x190 | qword | state-bit cleared on RUN | [STAT +0x5910] |
| +0x198 | qword | **already-processed frame count** (for 8-frame gate) | [STAT +0x6320] |
| +0x1A0 | qword | frames per tick copy | [STAT +0x5910 RUN] |
| +0x1A8 | qword | cleared on RUN | [STAT +0x5910] |
| +0x1B0 | qword | next scheduled event QPC | [STAT +0x5cc0 branch] |
| +0x1B8 | dword | frame counter | [STAT +0x6320] |
| +0x1C8 | qword | previous event QPC | [STAT +0x5cc0] |
| +0x1D0 | dword | last dir flag arg | [STAT +0x6320 epilogue] |

**Critical fields for AO port** (most important for Phase 4):
- `+0x100` — stored QPC frequency at init
- `+0x180` — baseline QPC (resets at start and periodically)
- `+0x198` — already-processed frame count (8-frame gate comparator)
- `+0x1B8` — frame counter (last delta)
- `+0xC8`, `+0xD0` — the two position values returned to caller
- `+0xA8` — DMA wrap size (used for over-jump guard: `frames > wrap/2` rejects)

### 3.12 Global BSS and shared state [STAT]

| Symbol | Purpose |
|---|---|
| vbaudio+0x12A60 | Lookup table (dword entries), level/envelope curve |
| vbaudio+0x12BE0 | /GS stack cookie master |
| vbaudio+0x12F80 | Mode flag (gates certain paths) |
| vbaudio+0x12F84 | Global stream refcount |
| vbaudio+0x12F88 | Stream array high-water slot |
| vbaudio+0x12F90..+0x12FD0 | Stream pointer array (9 slots) |
| vbaudio+0x12FD8 | ExTimer handle (global, single instance) |
| vbaudio+0x298 | Global baseline QPC (timer path) |
| vbaudio+0x2A0 | Global tick counter (timer path) |
| vbaudio+0x2A8 | Global next-event QPC (timer path) |

### 3.13 IAT (kernel API callouts) [DYN][STAT]

From `dps` dump during live session, confirmed:

| IAT offset | Symbol |
|---|---|
| +0x8000 | `nt!KeQueryPerformanceCounter` |
| +0x8040 | `nt!RtlCopyUnicodeString` |
| +0x8048 | `nt!ExFreePool` |
| +0x8060 | `nt!ExAllocatePoolWithTag` |
| +0x8068 | `nt!ExFreePoolWithTag` |
| +0x8070 | `nt!IofCompleteRequest` |
| +0x80E0 | `nt!KeAcquireSpinLockRaiseToDpc` |
| +0x80E8 | `nt!KeReleaseSpinLock` |
| +0x80F0 | `nt!DbgPrintEx` |
| +0x80F8 | `nt!ExInitializePushLock` |
| +0x8100 | **`nt!KeFlushQueuedDpcs`** ★ |
| +0x8108 | `nt!KeSetEvent` |
| +0x8110 | **`nt!ExAllocateTimer`** ★ |
| +0x8118 | **`nt!ExSetTimer`** ★ |
| +0x8120 | **`nt!ExDeleteTimer`** ★ |
| +0x8180 | `nt!guard_check_icall` |
| +0x8188 | `nt!KscpCfgDispatchUserCallTargetEsSmep` (CFG dispatcher — indirect-call validator, not a callback) |
| +0x8190, +0x8198, +0x81a0 | vbaudio internal thunks |

**VB uses Ex*Timer API**, not KeSetTimerEx. This matters because Ex*Timer supports `EX_TIMER_HIGH_RESOLUTION` (attribute = 4 at allocate time) which gives sub-millisecond precision. AO's current timer (see `Source/Main/minwavertstream.cpp:1366-1373`) uses `ExSetTimer` which is correct direction, but AO's timer is still per-stream, not shared.

---

## 4. Current AO state assessment

### 4.0 Non-negotiable rules (Codex + Claude merged)

Before walking through the current state of each AO file, lock in the rules that the §6 phases must not cross. The first six are quoted directly from Codex's plan "Non-negotiable implementation rules" section; the additional four are Claude's additions from the cross-review pass.

**Codex's six rules (verbatim intent):**

1. **No overwrite-oldest write policy.** If AO needs to mimic VB, overflow must reject or skip, not silently advance the read pointer to make room. *(Already satisfied — `FramePipeWriteFrames:1422-1427` hard-rejects.)*
2. **No MicSink-style direct transport bypass.** All real transport must flow through the frame-pipe model. The `LOOPBACK_MIC_SINK` path is being phased out and must not be resurrected. *(Already satisfied — old path comment-disabled in `SetState`.)*
3. **Position polling must become behaviorally meaningful.** It is not enough to return fresher timestamps. Query-time progression must actually matter for cable endpoints. *(Satisfied at **Phase 5** (render) and **Phase 6** (capture). **Not** satisfied at Phase 3 — Phase 3 is shadow-and-compare mode (`AO_PUMP_FLAG_SHADOW_ONLY` set, `DISABLE_LEGACY_*` clear) and does not yet drive transport. This is intentional: Phase 3 verifies the timing math in isolation via windowed divergence comparison before Phase 5/6 adds the behavioral weight.)*
4. **8-frame gate must exist.** This is now directly supported by evidence (§3.4) and must be treated as part of the architecture, not an optional micro-optimization. *(Satisfied at Phase 3 shadow-and-compare mode — gate logic runs even without transport ownership; divergence counter proves the gate threshold is being hit consistently with `UpdatePosition`'s expected amount of work.)*
5. **Timer remains subordinate to the main paired path.** Keep it as auxiliary/shared support, not the primary observed engine. *(Phase 8 re-roles the existing per-stream WaveRT notification timer into aux duties; optional — the main quality win comes from Phases 3~6.)*
6. **State semantics must move toward VB, not away from it.** Especially STOP vs PAUSE (§3.2). *(Satisfied at Phase 4.)*

**Claude's four additional rules from cross-review:**

7. **Do not rewrite `UpdatePosition()`.** It has WaveRT contract duties (`m_ullLinearPosition`, `m_ullPresentationPosition`, `m_ullDmaTimeStamp`, packet counter carry-forward) that `GetReadPacket`/`SetWritePacket` depend on. Add a sibling helper instead — see §6.0.
8. **Do not acquire `m_PositionSpinLock` from inside `FramePipeWriteFromDma` / `FramePipeReadToDma`.** The pipe has its own `PipeLock`, and nesting locks in the wrong order risks deadlock. The pump helper owns the stream-level lock; the pipe primitive owns the pipe-level lock; these must stay strictly ordered.
9. **Do not change `ioctl.h` diagnostic struct without updating `adapter.cpp` IOCTL handler AND `test_stream_monitor.py` in the same commit.** CLAUDE.md "Diagnostics Rule" is load-bearing for debuggability during rollout.
10. **Do not delete `m_ulBlockAlignCarryForward`, `m_hnsElapsedTimeCarryForward`, `m_byteDisplacementCarryForward` in Phase 5/6.** These belong to `UpdatePosition()`'s WaveRT contract role. Even though the pump no longer needs them for cable transport, they still serve non-cable streams and WaveRT packet timing. Let them stay.

**Claude additions from the third cross-review pass (revision 2.2):**

11. **Rollout must preserve a rollback path until parity is proven.** Every phase that adds behavior must be gated by a feature flag (`m_ulPumpFeatureFlags`) that can be cleared at runtime via IOCTL. No phase's new behavior path may be "on" without a corresponding "off" option reachable without rebuild/reinstall/reboot. Rationale: Phase 5/6 move transport ownership, which is the highest-risk change in this plan. If the pump has a subtle math bug, we need to fall back to the legacy path **in seconds**, not after a rebuild cycle. (From Codex Rule 7.)

12. **Behavior beats offsets when the two disagree.** If a raw offset interpretation from a tricky VB helper (e.g. `+0x6320`'s interior-pointer `stream-8`) conflicts with the whole-function behavior model, trust the behavior model until the offset is revalidated. Do not promote provisional offsets into AO struct design. Specifically: §3.11 Stream object layout entries must be marked as `verified direct` / `verified after normalization` / `provisional`, and only `verified` entries may influence AO field naming or placement decisions. (From Codex Rule 8; §3.11 is updated in revision 2.2 to add these markings.)

**Claude additions from the fifth cross-review pass (revision 2.4), absorbing Codex v3:**

13. **Each cable direction must have exactly one transport owner per phase.** At no point should the same cable direction be advanced by both the new query-driven pump AND the legacy cable transport path. If ownership is ambiguous, the phase is not ready. The §6.5 "Transport ownership matrix" (new in rev 2.4) makes this rule mechanically auditable: for each phase, each cable direction has exactly one owner listed. Phase review must reject any state that violates the matrix. (From Codex v3 Rule 11.)

14. **Pump transport must not depend forever on WaveRT presentation offset.** During Phase 3 shadow mode, borrowing `m_ullLinearPosition % DmaBufferSize` for the pump's buffer offset is acceptable as a lazy-init sync point. After a cable direction is pump-owned (Phase 5/6), the pump must maintain its own `m_ulPumpLastBufferOffset` independently. If WaveRT presentation math drifts over long runs, the pump must not silently drift with it into stale data. (From Codex v3 Rule 13.)

**Claude additions from the sixth cross-review pass (revision 2.5), absorbing Codex v4:**

15. **Shadow comparison must be windowed, not naively per-call.** `UpdatePosition()` uses carry-forward math in both time (`m_hnsElapsedTimeCarryForward`) and bytes (`m_byteDisplacementCarryForward`), so single-query pump-vs-legacy comparison false-positives on legitimate rounding. Phase 3 shadow mode **must** accumulate both counts into a rolling window (128 calls) and compare only at window boundary, with tolerance `max(16 frames, 2% of larger total)`. A per-call comparison is forbidden as a bring-up truth source because it cannot distinguish systematic drift from expected carry noise. (From Codex v4 Rule 14; was already implemented in rev 2.3 Phase 3 code but is now formally a rule so future phases cannot silently regress to per-call.)

16. **Ownership-moving phases must prove runtime rollback in a live session.** For Phase 5 and Phase 6, shipping a feature flag is not sufficient on paper. The plan is only valid if Verification actually demonstrates that clearing the ownership flag via IOCTL returns the affected direction to the legacy transport path **within one or two position queries**, during a live call, without reinstall/reboot, and without audible pop or underrun spike. If the rollback smoke test cannot be performed (e.g. no live test harness available at that moment), Phase 5/6 is not considered complete and must not be merged — the guarantee exists to make a real-world emergency rollback credible, and an unproven flag is not a rollback. (From Codex v4 Rule 15.)

**Enforcement**: At the end of each phase's Verification step, re-read these 16 rules and confirm compliance. If in doubt, roll back that phase and re-plan. Rule 13 in particular: if you cannot answer "exactly one owner" for all four cable directions (A-render, A-capture, B-render, B-capture), the phase is incomplete. Rule 16 in particular: Phase 5 and Phase 6 cannot close on "the flag exists in the code" — they close on "we toggled the flag during a live Phone Link call and audio stayed stable through the transition in both directions".

### 4.1 `Source/Utilities/loopback.h` (347 lines)

**Structure inventory:**

- `LB_FORMAT` — stream format descriptor. Used by the legacy `LOOPBACK_BUFFER` path.
- `LB_SRC_STATE` — sinc SRC state (8 taps × 16 channels × INT32 history). Used by legacy path.
- `LOOPBACK_MIC_SINK` — MicSink direct-push descriptor. **CLAUDE.md rule 7 says this path must not exist. It still compiles but is being phased out.**
- `LOOPBACK_BUFFER` — legacy ring buffer with format conversion. Still present, coexisting with FRAME_PIPE.
- `FRAME_PIPE` — VB-style fixed-frame pipe. This is the target structure.

**FRAME_PIPE key fields (lines 214~272):**
- `INT32* RingBuffer` — INT32 samples, `CapacityFrames * PipeChannels` total
- `ULONG PipeChannels` — channels per frame
- `volatile ULONG WriteFrame/ReadFrame/FillFrames` — frame-indexed state
- `KSPIN_LOCK PipeLock` — per-pipe spinlock
- `ULONG PipeSampleRate / PipeBitsPerSample / PipeBlockAlign` — pipe format
- `ULONG Speaker/Mic SampleRate/BitsPerSample/Channels/BlockAlign + IsFloat + Active` — registered stream formats
- `BOOLEAN SpeakerSameRate / MicSameRate` — rate-match flag
- `ULONG TargetFillFrames / CapacityFrames / StartThresholdFrames / StartPhaseComplete` — startup policy
- `volatile ULONG DropCount / UnderrunCount / ActiveRenderCount` — diagnostics
- `BYTE* ScratchDma; INT32* ScratchSpk; INT32* ScratchMic; ULONG ScratchSizeBytes` — per-direction scratch

**Comparison to VB ring struct** (§3.10):
- AO has no explicit "WrapBound" — uses `CapacityFrames`. Functionally equivalent.
- AO has `FillFrames` explicitly; VB derives it from `WritePos - ReadPos`. AO's is actually safer (no full/empty ambiguity).
- AO lacks VB's SRC per-ring carry state (`+0x34`, `+0x38`, `+0xBC`, `+0x13C`). **This is acceptable** because AO does SRC at a different layer (inside write/read call paths), not inside the ring struct.
- AO has per-direction scratch buffers (`ScratchSpk`, `ScratchMic`) which VB lacks. **AO is architecturally better here** because Speaker DPC and Mic DPC can run on different cores without contending.

### 4.2 `Source/Utilities/loopback.cpp` (FRAME_PIPE section, lines 1396~1960)

**`FramePipeWriteFrames`** (1403~1461):
- ✓ Acquires `PipeLock` at function entry
- ✓ Hard-reject overflow: `if (frameCount > freeFrames) { DropCount += frameCount; return 0; }` — matches CLAUDE.md rule 3
- ✓ Wrap-aware `RtlCopyMemory` in two segments when needed
- ✓ Updates `WriteFrame`, `FillFrames` before releasing lock
- ✗ Does NOT enforce 8-frame minimum gate (comment: "that's DPC policy" — needs to be enforced upstream)

**`FramePipeReadFrames`** (1471~1552):
- ✓ Startup phase guard: returns silence until `FillFrames >= StartThresholdFrames`
- ✓ Empty-ring silence fill with `UnderrunCount += frameCount`
- ✓ Wrap-aware `RtlCopyMemory`
- ✓ Partial-read tail zero-fill with underrun increment
- ✓ Updates `ReadFrame`, `FillFrames`

**`FramePipeReset`** (1558~1584):
- ✓ `PAGED_CODE()` — must only be called at PASSIVE_LEVEL, safe after `KeFlushQueuedDpcs`
- ✓ Clears `WriteFrame/ReadFrame/FillFrames/StartPhaseComplete/DropCount/UnderrunCount`
- ✓ Zeros ring data

**`FramePipeRegisterFormat`** (1666~1723): (not re-read in full, but exists)
- Registers stream format, sets `SameRate` flag

**`FramePipeUnregisterFormat`** (1728~1768): (not re-read in full, but exists)
- Marks stream inactive, resets pipe when both sides are inactive via `FramePipeReset(pPipe)` at line 1764
- **★ THIS IS THE DIVERGENCE POINT with VB ★** — VB does NOT reset ring on stream unregister; it only resets on PAUSE with specific conditions

**`FramePipeWriteFromDma`** (1775~~): (partially read)
- Speaker DPC entry: accepts bytes in Speaker format
- Normalizes to INT32, channel-maps, chunks through scratch buffer
- Calls `FramePipeWriteFrames` for each chunk
- Returns total frames written, 0 on overflow

**`FramePipeReadToDma`** (1900~~): (partially read)
- Mic DPC entry: accepts bytes to fill in Mic format
- Reads from pipe, denormalizes, channel-maps, chunks through scratch buffer
- Calls `FramePipeReadFrames` for each chunk
- Always fills `byteCount` bytes (silence on underrun)

### 4.3 `Source/Main/minwavertstream.h`

No FRAME_PIPE-specific fields. All pipe access goes through the global `g_CableAPipe` / `g_CableBPipe`. This is clean.

Key existing fields:
- `m_KsState` — current KSSTATE
- `m_PositionSpinLock` — per-stream lock (used around position updates)
- `m_ullDmaTimeStamp`, `m_ullLastDPCTimeStamp`, `m_ullPerformanceCounterFrequency` — timing
- `m_hnsElapsedTimeCarryForward`, `m_hnsDPCTimeCarryForward`, `m_byteDisplacementCarryForward`, `m_ulBlockAlignCarryForward` — precision carries
- `m_ulDmaMovementRate`, `m_ulDmaBufferSize`, `m_pDmaBuffer` — DMA
- `m_ullPlayPosition`, `m_ullWritePosition`, `m_ullLinearPosition`, `m_ullPresentationPosition` — position outputs

### 4.4 `Source/Main/minwavertstream.cpp` — key functions

**`CMiniportWaveRTStream::Init`** (164+~): (not re-read in detail) — standard initialization

**`GetPosition` (790~817)** — called by portcls for KSPROPERTY_AUDIO_POSITION:
```c++
KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);
if (m_KsState == KSSTATE_RUN) {
    LARGE_INTEGER ilQPC = KeQueryPerformanceCounter(NULL);
    UpdatePosition(ilQPC);
}
Position_->PlayOffset = m_ullPlayPosition;
Position_->WriteOffset = m_ullWritePosition;
KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);
```

**[CLAIM ALIGNED WITH VB MODEL]**: AO already calls `UpdatePosition(QPC)` inside the position query path. That means AO already has the right **public trigger shape** for a VB-like query-pump design. What AO's current `UpdatePosition` does is still **time-elapsed-bytes-based**, not **frame-delta with 8-frame gate**. Phase 3 introduces the shadow-validated frame-delta helper, and Phases 5/6 later move real cable transport ownership to that helper.

**`GetPositions` (1056~1098)** — called for linear + presentation positions:
- Same pattern as `GetPosition`: acquires `m_PositionSpinLock`, calls `KeQueryPerformanceCounter`, calls `UpdatePosition(ilQPC)` if `KSSTATE_RUN`, then returns cached positions.

**`SetState` (1181~1382)** — the big state machine:

- STOP case (1201~1258):
  - `FramePipeUnregisterFormat` on line ~1231
  - Resets per-stream position counters under `m_PositionSpinLock`
  - **★ GAP ★**: VB STOP does NOT call anything equivalent to `FramePipeUnregisterFormat`. VB STOP only clears per-stream state and leaves the ring pipe alone.

- ACQUIRE case (1260~1265): no-op shell. Matches VB.

- PAUSE case (1267~1304):
  - Guarded by `if (m_KsState > KSSTATE_PAUSE)` — only acts when coming from RUN
  - Comment at line 1275: "FRAME_PIPE: do NOT unregister on Pause" — correct
  - `ExCancelTimer(m_pNotificationTimer, NULL)` — stops the WaveRT notification timer
  - `KeFlushQueuedDpcs()` on line 1284 — correct direction, matches VB rule 10
  - Updates DPC time carry-forward
  - Calls `GetPositions(NULL, NULL, NULL)` to update linear/presentation positions
  - **★ GAP ★**: VB also does a **conditional** `FramePipeReset`-equivalent here (ring reset). AO does not.

- RUN case (1306~1377):
  - Calls `FramePipeRegisterFormat` on line 1330
  - Zero-fills `m_pDmaBuffer` on line 1343 (good hygiene)
  - Sets `m_ulFadeInRemaining` for capture fade-in on line 1349 (AO-only fade-in ramp)
  - Resets `m_ulBlockAlignCarryForward = 0` on line 1353 (clean DMA alignment start)
  - Reads `KeQueryPerformanceCounter(&m_ullPerformanceCounterFrequency)` and stores baseline in `m_ullDmaTimeStamp` and `m_ullLastDPCTimeStamp`
  - Calls `ExSetTimer(m_pNotificationTimer, -HNSTIME_PER_MILLISECOND, HNSTIME_PER_MILLISECOND, NULL)` — starts per-stream notification timer
  - **★ MINOR GAP ★**: Per-stream timer, not shared. Lower priority (see §6 Phase 6).

**`UpdatePosition` (1407~1511)** — the critical function:

This is where AO currently computes how many bytes to process per position query. The calculation:
```c++
ULONG TimeElapsedInMS = (ULONG)(hnsCurrentTime - m_ullDmaTimeStamp + m_hnsElapsedTimeCarryForward)/10000;
m_hnsElapsedTimeCarryForward = ... % 10000;
ULONG ByteDisplacement = ((m_ulDmaMovementRate * TimeElapsedInMS) + m_byteDisplacementCarryForward) / 1000;
m_byteDisplacementCarryForward = ... % 1000;
// Block-align ByteDisplacement
ByteDisplacement = (totalBytes / nBlockAlign) * nBlockAlign;
```

Then for capture: `WriteBytes(ByteDisplacement)` → internally calls `FramePipeReadToDma` per chunk.
For render: `ReadBytes(ByteDisplacement)` → internally calls `FramePipeWriteFromDma` per chunk.

**★ MULTIPLE GAPS ★** vs VB's `+0x6320`:
1. **AO uses milliseconds and bytes** as the unit; VB uses frames. AO's block-align carry compensates, but the unit of "work" is bytes-per-ms, which is coarser than VB's "frames per QPC tick".
2. **AO has no 8-frame gate**. Every call to `UpdatePosition` does some work if `TimeElapsedInMS ≥ 1`, even if it's just 1ms × 48kHz × 4bytes = 192 bytes (~48 frames at 48kHz stereo). Under aggressive polling (hundreds of position queries per second), this results in many small calls.
3. **AO has no DMA over-jump guard**. If the OS stalls and then `UpdatePosition` is called with a large `hnsCurrentTime` jump, AO processes the entire big chunk — which may overshoot `m_ulDmaBufferSize / 2`. VB rejects such big jumps.
4. **AO's carry state** lives in `m_hnsElapsedTimeCarryForward`, `m_byteDisplacementCarryForward`, `m_ulBlockAlignCarryForward` — three separate carries that together serve the same purpose as VB's `stream+0x180` (baseline) + `stream+0x198` (processed). AO's model is more complex because it's bytes-based; switching to frames-based would simplify it.

**`WriteBytes` (1515~1658)** (capture path, bytes → Mic DMA):
- Determines `pPipe` (CableA / CableB)
- Loops: while `ByteDisplacement > 0`: `FramePipeReadToDma(pPipe, m_pDmaBuffer + bufferOffset, runWrite)`; advance position
- After the loop: applies fade-in ramp (16/24/32-bit float) if `m_ulFadeInRemaining > 0`

**`ReadBytes` (1662~1716)** (render path, Speaker DMA → ring):
- Determines `pPipe`
- Loops: while `ByteDisplacement > 0`: `FramePipeWriteFromDma(pPipe, m_pDmaBuffer + bufferOffset, runWrite)`; advance position

### 4.5 `Source/Main/adapter.cpp` / `ioctl.h`

(Not re-read in detail — known structure: IOCTL dispatch with config get/set, diagnostics read. Will need small updates in Phase 1 for the new diagnostic counters — GatedSkipCount, OverJumpCount, FramesProcessedTotal, PumpLastBufferOffset.)

### 4.6 Summary: AO is ~70% of the way there

- **Frame pipe transport**: already INT32, frame-indexed, hard-reject overflow, partial-read silence, scratch buffers. **Close enough to VB to ship as-is.**
- **State machine**: correctly handles ACQUIRE no-op, mostly-correct PAUSE (missing ring reset), slightly wrong STOP (unregisters too eagerly), correct RUN.
- **Position query path**: already calls `UpdatePosition` on-query. **The shape is right**; the content is byte-per-ms-based instead of frame-delta-based.
- **Fade-in ramp**: AO-only improvement, keep.
- **Diagnostics**: `DropCount`, `UnderrunCount`, `ActiveRenderCount` already exist; need to add `gated_skips`, `over_jumps`, and `frames_processed_total`.

The "30% that isn't there" is the **behaviorally-meaningful bits** — not the framework itself.

---

## 5. Concrete gap list: AO vs VB

Numbered so phase plans can reference them.

| # | Category | AO current behavior | VB behavior | Severity | Phase (rev 2) |
|---|---|---|---|---|---|
| G1 | STOP ring reset | `FramePipeUnregisterFormat` called on STOP → pipe resets | STOP does NOT reset ring | **CRITICAL** | **4** |
| G2 | PAUSE ring reset | AO comment says "do NOT unregister on Pause" and does nothing to pipe | PAUSE conditionally resets ring (prev > PAUSE, timer handle non-null, ring ptr non-null) | **CRITICAL** | **4** |
| G3 | 8-frame minimum gate | Not enforced in `UpdatePosition` or `FramePipeWriteFrames` | Hard-enforced in `+0x6320` | **HIGH** | **3** (observer) |
| G4 | Position-query pump is bytes-based, not frames-based | AO computes `ByteDisplacement` from `TimeElapsedInMS × DmaMovementRate` | VB computes frame delta directly from `(currentQPC - baseline) × sampleRate / freq` | **HIGH** | **3** (observer) + **5** (render) + **6** (capture) |
| G5 | DMA over-jump guard | Absent | `if (frames > wrap/2) skip & over_jump_counter++` | **HIGH** | **3** (observer) |
| G6 | Timer is per-stream (WaveRT notification timer) | `ExSetTimer` per-stream, 1ms | Shared ExTimer HIGH_RESOLUTION, refcounted, 1ms | **LOW** | **8** |
| G7 | Read-path SRC is nearest-neighbor equivalent | AO uses sinc for read SRC (currently, via `LB_SRC_STATE`) — actually AO's old path used sinc, FRAME_PIPE mostly does same-rate | VB uses linear weighted (for write via `+0x26a0`), nearest-neighbor (for read via `+0x11d4`) | **MEDIUM** | **7** |
| G8 | Unified SRC function | AO has split write/read SRC paths | VB has unified `+0x26a0` with direction flag for write; nearest for read via `+0x11d4` | **MEDIUM** | **7** |
| G9 | Format normalization for 32-bit PCM | AO: `>> 13` | VB: direct copy (native INT32) | **MEDIUM** | 2 |
| G10 | 32-bit float normalization | AO: float → INT24 → `>> 5` | VB: direct copy (bits) | **MEDIUM** | 2 |
| G11 | 8-bit underrun value | AO: `0x00` | VB: `0x80` (center for unsigned 8-bit) | **LOW** | 2 |
| G12 | DMA scratch pre-copy | AO: processes directly from `m_pDmaBuffer + offset`, relying on chunk loop to avoid wrap | VB: always `memcpy` DMA region to linear scratch first, then process from scratch | **LOW** | **5** (render) + **6** (capture) |
| G13 | Notification fire condition | AO: `m_IsCurrentWritePositionUpdated` flag (heuristic) | VB: explicit threshold-reached edge-trigger via `[stream+0x165]` flag + parent vtable slot 22 | **LOW** | **8** |
| G14 | Multi-client capture fan-out | AO: single ring | VB: per-stream independent rings observed in trace | **OUT OF SCOPE** | deferred |
| G15 | Per-channel level meter / VU | AO: no | VB: `+0x4f2c` with 127/128 decay | **LOW** | **8** (optional) |
| G16 | Stream registration with global timer | AO: no such concept | VB: `+0x65b8` / `+0x669c` | **LOW** | **8** |
| G17 | QPC frequency caching on stream | AO: caches in `m_ullPerformanceCounterFrequency` | VB: caches at `stream+0x100` | **EQUIVALENT** | none |

**Severity legend:**
- **CRITICAL**: Directly causes audible quality defects (dropouts, stale data, phase inversion).
- **HIGH**: Causes under-load misbehavior (stalls, over-processing, drift).
- **MEDIUM**: Causes measurable quality differences or maintenance burden.
- **LOW**: Edge cases, code clarity, future-proofing.

**Required for 1차 목표 (VB-equivalent call quality)**: Phases **3, 4, 5, 6** — observer + state + render pump + capture pump. All CRITICAL and HIGH gaps close here.
**Quality / architecture improvements (2차 목표)**: Phases **7** (SRC) and **8** (shared timer, notification refinement).
**Final validation**: Phase **9**.

---

## 6. Phase-by-phase implementation plan

Each phase has:
- **Goal** — what this phase accomplishes
- **Scope** — what is in and out
- **Pre-conditions** — what must be true before starting
- **Exact code changes** — file, line, current code, new code
- **Verification** — build, install, live test, WinDbg checks
- **Rollback** — how to undo if it breaks something
- **Exit criteria** — what must pass before moving on

### 6.0 Design principle: helper separation (do not rewrite `UpdatePosition`)

This is the single most important architectural decision of the revision 2 plan. It is called out here as a preamble so that every subsequent phase is understood in this light.

**The wrong approach (revision 1, superseded)**: rewrite `CMiniportWaveRTStream::UpdatePosition()` body to be frame-delta + 8-frame gate + over-jump guard + call `WriteBytes`/`ReadBytes`.

**Why it was wrong**: `UpdatePosition()` has **two different responsibilities** bundled together in the current code:

- **Responsibility A — WaveRT presentation contract**: compute `m_ullLinearPosition`, `m_ullPresentationPosition`, `m_ullDmaTimeStamp`, packet counter carry-forward. These fields are read by `GetReadPacket()` / `SetWritePacket()` (around line 864~870 and 946~950) to implement the packet-mode WaveRT interface. The timing model for these fields was tuned to match portcls expectations.

- **Responsibility B — data transport**: compute how many bytes have elapsed and push them through `WriteBytes()` (capture) or `ReadBytes()` (render), which call `FramePipeReadToDma()` / `FramePipeWriteFromDma()`. This is the "work" part of a position query.

Rewriting the function to do frame-delta math changes Responsibility A's numeric output, which can subtly break `SetWritePacket`'s "current packet expected" check (line 955-970) and `GetReadPacket`'s timestamp extrapolation (line 897-905). This is a large regression surface for a structural change.

**The right approach (revision 2)**: **add** a new helper `PumpToCurrentPositionFromQuery()` alongside the existing `UpdatePosition()`. The helper owns Responsibility B for cable endpoints only. `UpdatePosition()` continues to own Responsibility A unchanged, but its transport side-effects (`WriteBytes()` / `ReadBytes()` calls) get guarded by `!isCable` so the pump doesn't double-process.

Conceptual call structure after all phases complete:

```cpp
// File: Source/Main/minwavertstream.cpp
// Called by portcls for KSPROPERTY_AUDIO_POSITION
NTSTATUS CMiniportWaveRTStream::GetPosition(KSAUDIO_POSITION* Position_)
{
    KIRQL oldIrql;
    KeAcquireSpinLock(&m_PositionSpinLock, &oldIrql);
    if (m_KsState == KSSTATE_RUN)
    {
        LARGE_INTEGER ilQPC = KeQueryPerformanceCounter(NULL);

        // NEW (Phase 3): pump helper — does nothing but compute+log initially.
        // Phase 5: starts driving render transport for cable endpoints.
        // Phase 6: starts driving capture transport for cable endpoints.
        PumpToCurrentPositionFromQuery(ilQPC);

        // UNCHANGED: WaveRT contract math stays in UpdatePosition.
        // Phase 5/6 guard its WriteBytes/ReadBytes calls with !isCable
        // so the pump doesn't double-process cable streams.
        UpdatePosition(ilQPC);
    }
    Position_->PlayOffset  = m_ullPlayPosition;
    Position_->WriteOffset = m_ullWritePosition;
    KeReleaseSpinLock(&m_PositionSpinLock, oldIrql);
    return STATUS_SUCCESS;
}
```

This structure:
- Preserves the WaveRT contract surface (rule 7 of §4.0)
- Makes it trivial to roll back any one phase independently (the pump helper can be no-op'd by a single flag)
- Lets each phase's verification use the pre-existing WaveRT test infrastructure unchanged
- Maps cleanly to Codex plan §Layer 2 "Query-driven pump/update" concept

With this principle established, the following phases are reorderable into a safer sequence:

| Rev 1 phase | Rev 2 phase | What changed |
|---|---|---|
| Phase 3 (State machine) | Phase 4 | Moved later — state changes sit on top of a verified pump skeleton |
| Phase 4 (Frame-delta pump, rewrite UpdatePosition) | Phase 3 + 5 + 6 | Split into observer / render transport / capture transport, via new helper |
| Phase 5 (SRC) | Phase 7 | Renumbered |
| Phase 6 (Timer) | Phase 8 | Renumbered |
| Phase 7 (Validation) | Phase 9 | Renumbered |

The rest of Section 6 uses the **revision 2 numbering**.

### 6.5 Transport ownership matrix (revision 2.4)

This is the single most important audit table for every phase review. At any point during implementation, **each cable direction must have exactly one transport owner**. If two owners exist, the phase is not safe to land.

Rule 13 in §4.0 formalizes this. This table makes it mechanical to check.

| Phase | Cable render transport owner | Cable capture transport owner | `UpdatePosition()` role | Pump helper role | Timer role |
|---|---|---|---|---|---|
| **0** | legacy `ReadBytes` path | legacy `WriteBytes` path | full WaveRT contract + cable transport | none (not yet implemented) | current per-stream WaveRT notification timer |
| **1** | legacy `ReadBytes` path | legacy `WriteBytes` path | full WaveRT contract + cable transport | none (fields declared, not wired) | current role |
| **2** | legacy `ReadBytes` path | legacy `WriteBytes` path | full WaveRT contract + cable transport | none | current role |
| **3** | legacy `ReadBytes` path | legacy `WriteBytes` path | full WaveRT contract + cable transport | **observer / shadow-compare only** (flag: `ENABLE \| SHADOW_ONLY`) | current role |
| **4** | legacy `ReadBytes` path | legacy `WriteBytes` path | full WaveRT contract + cable transport | observer / shadow-compare only | current role, but STOP/PAUSE state semantics now VB-aligned |
| **5** | **query pump** (flag: `+ DISABLE_LEGACY_RENDER`) | legacy `WriteBytes` path | non-cable timing + WaveRT contract maintenance (render side disowned for cables) | owns cable render transport + still shadows capture | current role |
| **6** | query pump | **query pump** (flag: `+ DISABLE_LEGACY_CAPTURE`) | non-cable timing + WaveRT presentation bookkeeping only | owns both cable directions | current role |
| **7** | query pump | query pump | unchanged from Phase 6 | unchanged | aux role (SRC helper refactor but timer still per-stream) |
| **8** | query pump | query pump | unchanged from Phase 6 | unchanged | **aux / shared duties only** (optional shared-timer migration) |
| **9** | query pump | query pump | unchanged | unchanged | aux / shared |

**Audit procedure** (per phase checkpoint):
1. Pick a cable direction (A-render, A-capture, B-render, B-capture).
2. List every call site that advances transport for that direction.
3. Count. Must be exactly 1.
4. Verify the count is consistent with the matrix row for the current phase.
5. If count > 1 → **STOP the phase and investigate double-advance**.
6. If count = 0 → the direction is silent (regression); investigate.

**Double-transport symptoms** (if the audit catches a violation):
- Audio "plays too fast" (both pump AND legacy ReadBytes pushing frames).
- `DropCount` grows rapidly (pipe overflow from double-write).
- `FramesProcessedTotal` grows at 2× expected rate.
- Live audio is choppy / "stuttery".

**Rule 13 enforcement**: Phase 5's verification step **must** include a WinDbg breakpoint on `FramePipeWriteFromDma` during a live call that logs the call stack. The caller must be `PumpToCurrentPositionFromQuery` *exclusively* for cable render streams; if `ReadBytes → FramePipeWriteFromDma` ever shows up for a cable render stream after Phase 5 lands, the gate check is broken and Phase 5 must roll back.

### Phase 0 — Pre-flight (1 day, up from 0.5 due to critical WinDbg check)

**Goal**: Prove the environment is ready AND prove the AO entry point that the pump will hook into actually fires during the target scenario. Do not change any code.

**Scope**:
- Confirm the current `feature/ao-fixed-pipe-rewrite` branch builds cleanly.
- Confirm the WinDbg target machine is reachable.
- Snapshot the current `docs/PIPELINE_V2_CHANGELOG.md` head-of-log so we know when this plan's entries start.
- Read this plan and Codex's plan in the same session to catch disagreements early.
- Load `memory/project_vbcable_runtime.md` into context.
- **NEW (rev 2.4)**: WinDbg sanity check that `CMiniportWaveRTStream::GetPosition` and/or `GetPositions` actually fire during a Phone Link live call. This resolves OQ3 (elevated from open question to blocking Phase 0 task per Codex v3).

**Exact steps**:

1. Build sanity check:
   ```powershell
   cd d:\mywork\Virtual-Audio-Driver
   .\build-verify.ps1 -Config Release
   ```
   Expected: PASS. If FAIL, stop here and diagnose — do not start any phase on a broken baseline.

2. Install current baseline to target:
   ```powershell
   .\install.ps1 -Action upgrade
   ```
   NO `-AutoReboot`. Expected: installation completes in-session.

3. Smoke test:
   ```powershell
   python test_ioctl_diag.py
   ```
   Expected: GET_CONFIG returns valid, SET/GET roundtrip passes.

4. **★ Blocking check: AO position-query entry point verification**
   Attach WinDbg to the target, then:
   ```
   bu ao_driver!CMiniportWaveRTStream::GetPosition ".printf \"GetPosition fired\\n\"; gc"
   bu ao_driver!CMiniportWaveRTStream::GetPositions ".printf \"GetPositions fired\\n\"; gc"
   g
   ```
   Start a live Phone Link call with `AUDIO_CABLE_PROFILE=ao`. Speak or play TTS for 5 seconds. Observe the output window.

   **Pass criterion**: At least ONE of the two functions fires repeatedly (tens of times per second) during the call. The total hit count over 5 seconds should be at least 100 across the two functions combined.

   **If NEITHER function fires**: portcls is calling a different entry point for position queries on this AO build. **STOP** — Phase 3 will be a no-op because its new pump helper hooks into functions that never run. Investigate which entry point portcls is actually using (candidates: `GetPositionRegister`, `GetPresentationPosition`, a different vtable slot). Update the plan to hook the pump into the correct entry point before landing Phase 3.

   **If only one function fires**: note which one in a Phase 0 deliverable file (`results/phase0_findings.md` or similar). The Phase 3 wiring should match — if only `GetPositions` fires, don't bother wiring the pump into `GetPosition`.

5. Live call sanity baseline (separate from step 4):
   ```powershell
   cd tests/live_call
   python run_test_call.py
   ```
   Expected: Phone Link call starts on PC side, user reports current baseline quality (we already know: distorted on AO, clean on VB). This is the "before" reference for all subsequent phases.

6. Checkpoint commit (optional, on a non-main branch):
   ```
   git commit --allow-empty -m "Phase 0 baseline: pre-rewrite snapshot"
   ```

**Verification**: Build passes, install passes, IOCTL diag passes, **GetPosition/GetPositions fires during live call**.

**Rollback**: N/A — no code changed.

**Exit criteria**:
- All six steps complete without red flags.
- Step 4 confirms at least one position query entry point is hot-path. This is the **most important** Phase 0 gate — without it, subsequent phases are built on false assumptions.
- Phase 0 findings file captures which entry point is active (for Phase 3 wiring reference).

---

### Phase 1 — Diagnostics and rollout scaffolding (1 day)

**Goal**: Add the new stream-level state, diagnostic counters, AND feature-flag infrastructure that later phases need for runtime rollback. This phase is explicitly about setting up the "safety harness" for the subsequent transport-ownership changes — per-direction counters, IOCTL visibility, feature flags — before any behavior change. (Renamed from "Struct / diagnostic field additions" in rev 2.4 per Codex v3 framing: the real purpose is rollout scaffolding, not just fields.)

**Scope**:
- Add new members to `CMiniportWaveRTStream` for frame-delta bookkeeping.
- Add new members to `FRAME_PIPE` for gated-skip and over-jump counters.
- Update `ioctl.h` diagnostic struct to expose the new counters.
- Update `adapter.cpp` IOCTL handler to populate them.
- Update `test_stream_monitor.py` to display them.
- **No behavior change.** Counters initialize to 0 and stay 0.

**Pre-conditions**: Phase 0 passed.

**Rationale**: This phase sets up the visibility needed to debug Phase 3 and Phase 4. If we land Phase 3/4 without these counters in place, we will have no way to tell what's happening under `KeFlushQueuedDpcs` or why frames are getting gated.

**File 1: `Source/Main/minwavertstream.h`**

Add (around line 108, in the protected: block):
```cpp
    // Frame-delta position pump state (G3, G4, G5, OQ7, R9)
    ULONGLONG                   m_ullPumpBaselineHns;        // elapsed-100ns baseline
    ULONG                       m_ulPumpProcessedFrames;      // frames already accounted since baseline
    BOOLEAN                     m_bPumpInitialized;           // set on first RUN call, cleared on STOP/PAUSE
    ULONG                       m_ulPumpLastBufferOffset;     // pump's own view of buffer offset (bytes),
                                                               // decoupled from m_ullLinearPosition (OQ7, R9)
    // Diagnostic counters (Phase 1 populates zero, Phase 3 starts incrementing)
    ULONG                       m_ulPumpInvocationCount;      // total calls to pump helper (any outcome)
    ULONG                       m_ulPumpGatedSkipCount;       // # times 8-frame gate fired
    ULONG                       m_ulPumpOverJumpCount;        // # times over-jump guard fired
    ULONG                       m_ulPumpShadowDivergenceCount;// # times the windowed shadow compare exceeded tolerance
    ULONGLONG                   m_ullPumpFramesProcessed;     // total frames pumped through
    // Runtime feature flags (bit field) — see §6.0 rollback safety
    ULONG                       m_ulPumpFeatureFlags;         // AO_PUMP_FLAG_* bits
    // Phase 3 shadow-mode rolling-window accumulators (avoid per-call carry noise)
    ULONGLONG                   m_ullPumpShadowWindowPumpFrames;  // pump frame-delta total over current window
    ULONGLONG                   m_ullPumpShadowWindowLegacyBytes; // UpdatePosition ByteDisplacement total over current window
    ULONG                       m_ulPumpShadowWindowCallCount;    // calls seen in current window (reset at AO_PUMP_SHADOW_WINDOW_CALLS)
    // Phase 3 shadow-mode intra-call handoff (written by UpdatePosition, read by pump)
    ULONG                       m_ulLastUpdatePositionByteDisplacement; // session-local, only valid within one GetPosition call
```

In `minwavertstream.h` near the top, add the bit flag constants:
```cpp
// Phase 1: runtime feature flags for the query-driven pump helper.
// Default state at Init: SHADOW_ONLY is set, ENABLE is clear, DISABLE_LEGACY is clear.
// Phase 3 exit: ENABLE is set, SHADOW_ONLY remains set (pump computes + compares, no transport).
// Phase 5 exit (render): DISABLE_LEGACY_RENDER set, pump drives render transport for cable speakers.
// Phase 6 exit (capture): DISABLE_LEGACY_CAPTURE set, pump drives capture transport for cable mics.
// IOCTL rollback: clear DISABLE_LEGACY_* bits to fall back to UpdatePosition's byte-per-ms transport.
#define AO_PUMP_FLAG_ENABLE                  0x00000001  // Master enable — pump runs at all
#define AO_PUMP_FLAG_SHADOW_ONLY             0x00000002  // Compute + log + compare; do NOT drive transport
#define AO_PUMP_FLAG_DISABLE_LEGACY_RENDER   0x00000004  // Cable render transport moves to pump exclusively
#define AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE  0x00000008  // Cable capture transport moves to pump exclusively
```

**Naming note**: Codex plan suggests `m_QueryPumpXxx` prefix for clarity (explicitly marks these as belonging to the query-driven pump, distinct from WaveRT's `m_ullXxxPosition` fields). This plan uses `m_xxPumpXxx` to stay close to existing AO field naming conventions. If during implementation the naming conflict causes confusion, rename to the Codex convention in Phase 1.

**Shadow-mode semantics note**: The `m_ulPumpShadowDivergenceCount` field is only meaningful during the **shadow window** — when `AO_PUMP_FLAG_SHADOW_ONLY` is set and transport is still owned by `UpdatePosition`. Once `DISABLE_LEGACY_*` flags are set, the pump owns transport and there's no comparable legacy path to diverge against. After Phase 6, this counter freezes (interpretation changes to "historical parity evidence").

**Shadow-mode windowed comparison**: Phase 3 does NOT do per-call compare — that would produce false positives from legitimate rounding carry in `UpdatePosition`'s byte-per-ms math. Instead, the pump accumulates both its own `newFrames` and `UpdatePosition`'s `ByteDisplacement` into rolling-window totals (`m_ullPumpShadowWindowPumpFrames` and `m_ullPumpShadowWindowLegacyBytes`). Every `AO_PUMP_SHADOW_WINDOW_CALLS` (default 128) invocations, the two totals are compared. If the absolute difference exceeds `max(16 frames, 2% of larger total)`, the divergence counter ticks. Window accumulators reset each cycle. This averages out per-call noise while still catching systematic drift.

**File 2: `Source/Utilities/loopback.h`**

Add to `FRAME_PIPE` struct (after `ActiveRenderCount`, around line 258):

**IMPORTANT (rev 2.4)**: Pipe-level counters MUST be split per-direction because Speaker and Mic streams share the same pipe. If we use a single counter and the stream-level code does `pFP->Count = m_ulLocalCount`, then Speaker's write and Mic's write race/overwrite each other. Using `+=` on cumulative counters fixes the race but causes double-counting (Speaker write + Mic read = 2× effective frames). The clean solution is **separate Render and Capture counter pairs**:

```cpp
    // Render-side pump counters (populated by Speaker stream's pump)
    volatile ULONG      RenderGatedSkipCount;           // frames skipped by 8-frame gate (Speaker pump)
    volatile ULONG      RenderOverJumpCount;             // frames skipped by over-jump guard
    volatile ULONGLONG  RenderFramesProcessedTotal;      // monotonic frames written via pump
    volatile ULONG      RenderPumpInvocationCount;       // pump invocations
    volatile ULONG      RenderPumpShadowDivergenceCount; // shadow-mode math disagreements

    // Capture-side pump counters (populated by Mic stream's pump)
    volatile ULONG      CaptureGatedSkipCount;
    volatile ULONG      CaptureOverJumpCount;
    volatile ULONGLONG  CaptureFramesProcessedTotal;
    volatile ULONG      CapturePumpInvocationCount;
    volatile ULONG      CapturePumpShadowDivergenceCount;
```

**Pump-side assignment**: the pump helper does a direct copy (`=`) from the stream's own counters to the appropriate per-direction pipe field based on `m_bCapture`. No race because only one stream ever writes to a given direction's slot.

**File 3: `Source/Main/ioctl.h`**

(Find the existing diagnostic struct, probably `AO_PIPE_STATUS` or similar.) Add at the end of its data fields (BEFORE any reserved/padding — if no padding, add at end). Per-direction counter split (rev 2.4):

```cpp
    // Render-side (Speaker pump) counters
    ULONG     RenderGatedSkipCount;
    ULONG     RenderOverJumpCount;
    ULONG     RenderFramesProcessedLow;
    ULONG     RenderFramesProcessedHigh;
    ULONG     RenderPumpInvocationCount;
    ULONG     RenderPumpShadowDivergenceCount;
    ULONG     RenderPumpFeatureFlags;   // speaker stream flags snapshot

    // Capture-side (Mic pump) counters
    ULONG     CaptureGatedSkipCount;
    ULONG     CaptureOverJumpCount;
    ULONG     CaptureFramesProcessedLow;
    ULONG     CaptureFramesProcessedHigh;
    ULONG     CapturePumpInvocationCount;
    ULONG     CapturePumpShadowDivergenceCount;
    ULONG     CapturePumpFeatureFlags;  // mic stream flags snapshot
```

(The struct has `C_ASSERT` guards for size — bump the version number and update the guards. CLAUDE.md's "Diagnostics Rule" requires ioctl.h / adapter.cpp / test_stream_monitor.py to stay in sync in the same commit.)

**File 4: `Source/Main/adapter.cpp`**

Find the IOCTL handler that returns the diagnostic struct. Add population lines using the per-direction counter split:
```cpp
    // Render-side snapshot
    pStatus->RenderGatedSkipCount            = pPipe->RenderGatedSkipCount;
    pStatus->RenderOverJumpCount             = pPipe->RenderOverJumpCount;
    pStatus->RenderFramesProcessedLow        = (ULONG)(pPipe->RenderFramesProcessedTotal & 0xFFFFFFFF);
    pStatus->RenderFramesProcessedHigh       = (ULONG)(pPipe->RenderFramesProcessedTotal >> 32);
    pStatus->RenderPumpInvocationCount       = pPipe->RenderPumpInvocationCount;
    pStatus->RenderPumpShadowDivergenceCount = pPipe->RenderPumpShadowDivergenceCount;
    pStatus->RenderPumpFeatureFlags          = 0; // TODO: wire from Speaker stream instance

    // Capture-side snapshot
    pStatus->CaptureGatedSkipCount            = pPipe->CaptureGatedSkipCount;
    pStatus->CaptureOverJumpCount             = pPipe->CaptureOverJumpCount;
    pStatus->CaptureFramesProcessedLow        = (ULONG)(pPipe->CaptureFramesProcessedTotal & 0xFFFFFFFF);
    pStatus->CaptureFramesProcessedHigh       = (ULONG)(pPipe->CaptureFramesProcessedTotal >> 32);
    pStatus->CapturePumpInvocationCount       = pPipe->CapturePumpInvocationCount;
    pStatus->CapturePumpShadowDivergenceCount = pPipe->CapturePumpShadowDivergenceCount;
    pStatus->CapturePumpFeatureFlags          = 0; // TODO: wire from Mic stream instance
```

The `FeatureFlags` TODO exists because the IOCTL handler does not currently have direct access to the stream instance — only the pipe. Wiring this will require a small refactor (e.g. the pipe holds a weak pointer to each direction's current stream, or the stream publishes its flags into the pipe on flag change). Acceptable to leave zeroed until Phase 3 lands; pre-Phase-5 the flags are not yet load-bearing for user-facing status.

**File 5: `test_stream_monitor.py`**

Add parsing and display of the new fields. Look for the existing struct unpack and add **7** more `I` fields (GatedSkipCount, OverJumpCount, PumpFramesTotalLow, PumpFramesTotalHigh, PumpInvocationCount, PumpShadowDivergenceCount, PumpFeatureFlags). Print them in the status line. Also compute and display the derived ratio `ShadowDivergenceCount / max(1, InvocationCount) * 100.0` as a percentage — this is the Phase 3 exit-criterion metric.

**File 6: `Source/Main/minwavertstream.cpp`**

In `CMiniportWaveRTStream::Init` (around line 164), add after existing field initializations:
```cpp
    m_ullPumpBaselineHns                    = 0;
    m_ulPumpProcessedFrames                 = 0;
    m_bPumpInitialized                      = FALSE;
    m_ulPumpLastBufferOffset                = 0;
    m_ulPumpInvocationCount                 = 0;
    m_ulPumpGatedSkipCount                  = 0;
    m_ulPumpOverJumpCount                   = 0;
    m_ulPumpShadowDivergenceCount           = 0;
    m_ullPumpFramesProcessed                = 0;
    // Feature flags default: pump is DISABLED at Init. Phase 3 commit
    // flips ENABLE|SHADOW_ONLY on in SetState RUN for cable endpoints.
    m_ulPumpFeatureFlags                    = 0;
    // Phase 3 shadow-mode windowed comparison accumulators.
    m_ullPumpShadowWindowPumpFrames         = 0;
    m_ullPumpShadowWindowLegacyBytes        = 0;
    m_ulPumpShadowWindowCallCount           = 0;
    m_ulLastUpdatePositionByteDisplacement  = 0;
```

**File 7: `Source/Utilities/loopback.cpp`**

In `FramePipeInit` (around the existing initialization), add (per-direction split, rev 2.4):
```cpp
    pPipe->RenderGatedSkipCount            = 0;
    pPipe->RenderOverJumpCount             = 0;
    pPipe->RenderFramesProcessedTotal      = 0;
    pPipe->RenderPumpInvocationCount       = 0;
    pPipe->RenderPumpShadowDivergenceCount = 0;

    pPipe->CaptureGatedSkipCount            = 0;
    pPipe->CaptureOverJumpCount             = 0;
    pPipe->CaptureFramesProcessedTotal      = 0;
    pPipe->CapturePumpInvocationCount       = 0;
    pPipe->CapturePumpShadowDivergenceCount = 0;
```

In `FramePipeReset`, ADD the per-session counters to the reset list but leave monotonic run-totals alone:
```cpp
    // Per-session counters — reset on ring reset (matches VB FUN_1400039ac)
    pPipe->RenderGatedSkipCount  = 0;
    pPipe->RenderOverJumpCount   = 0;
    pPipe->CaptureGatedSkipCount = 0;
    pPipe->CaptureOverJumpCount  = 0;

    // Monotonic run-totals — preserve across resets so Phase 3 divergence
    // ratio stays measurable through RUN→PAUSE→RUN cycles.
    // Do NOT reset: RenderFramesProcessedTotal, CaptureFramesProcessedTotal,
    //               RenderPumpInvocationCount, CapturePumpInvocationCount,
    //               RenderPumpShadowDivergenceCount, CapturePumpShadowDivergenceCount
```

The distinction: **per-session** counters (`GatedSkipCount`, `OverJumpCount`) reflect current-run steady-state and are useful to reset on ring reset. **Monotonic** counters (`FramesProcessedTotal`, `InvocationCount`, `ShadowDivergenceCount`) are historical-run-totals; resetting them would destroy the Phase 3 exit-criterion measurement mid-call.

**File 8: `docs/PIPELINE_V2_CHANGELOG.md`**

Add an entry:
```markdown
## 2026-04-13 Phase 1 — Diagnostic field additions (Claude plan)

Added new stream-level and pipe-level diagnostic counters + runtime feature flags without any behavior change:

Stream state (minwavertstream.h):
- m_ullPumpBaselineHns                 — elapsed-100ns baseline for the pump
- m_ulPumpProcessedFrames              — already-accounted frame count
- m_bPumpInitialized                   — first-call-after-RUN latch
- m_ulPumpLastBufferOffset             — pump's own buffer-offset tracking (decoupled from m_ullLinearPosition)
- m_ulPumpInvocationCount              — total pump calls (any outcome)
- m_ulPumpGatedSkipCount                — 8-frame gate fires
- m_ulPumpOverJumpCount                 — DMA over-jump guard fires
- m_ulPumpShadowDivergenceCount         — shadow-mode math mismatches vs UpdatePosition
- m_ullPumpFramesProcessed              — total frames pumped (monotonic)
- m_ulPumpFeatureFlags                  — runtime bit flags (AO_PUMP_FLAG_*)
- m_ulLastUpdatePositionByteDisplacement — Phase 3 shadow-mode stash field (UpdatePosition → pump handoff within one call)

Flag constants (minwavertstream.h near top):
- AO_PUMP_FLAG_ENABLE
- AO_PUMP_FLAG_SHADOW_ONLY
- AO_PUMP_FLAG_DISABLE_LEGACY_RENDER
- AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE

Pipe state (loopback.h FRAME_PIPE):
- GatedSkipCount                       — mirrors stream field
- OverJumpCount                        — mirrors stream field
- FramesProcessedTotal                 — monotonic across streams on this pipe
- InvocationCount                      — mirrors stream field
- ShadowDivergenceCount                — mirrors stream field

Diagnostic IOCTL struct (ioctl.h):
- GatedSkipCount, OverJumpCount, PumpFramesTotalLow, PumpFramesTotalHigh
- PumpInvocationCount, PumpShadowDivergenceCount, PumpFeatureFlags

All three files (ioctl.h + adapter.cpp + test_stream_monitor.py) updated together per CLAUDE.md Diagnostics Rule.

Why: Prerequisite visibility for Phase 3 (shadow-mode compare and gate/guard counters), Phase 4 (state machine — needs pump regression visibility), Phases 5/6 (pump drives transport — flags control when). All counters initialize to 0 and stay 0 in Phase 1 — they start populating in Phase 3. Feature flags are 0 at Init, set to `ENABLE|SHADOW_ONLY` in Phase 3 `SetState RUN` for cable endpoints, extended in Phase 5/6.
```

**Verification**:
1. `.\build-verify.ps1 -Config Release` passes.
2. `.\install.ps1 -Action upgrade` passes (no reboot).
3. `python test_ioctl_diag.py` still passes and shows the new counters at 0.
4. Run an existing WAV through Cable A → Cable A capture; counters should stay at 0 (we haven't wired them).

**Rollback**: `git restore` on all 8 files. Build clean.

**Exit criteria**: All four verification steps pass.

---

### Phase 2 — Normalization/denormalization corrections (G9, G10, G11) (1 day)

**Goal**: Make AO's sample format conversion bitwise-matching with VB's observed behavior for 32-bit PCM and 32-bit float (both should be direct copy, not shifted) and correct the 8-bit underrun fill value.

**Scope**:
- Correct 32-bit PCM normalization: AO's `FpNorm32i: s >> 13` → VB's direct copy.
- Correct 32-bit float normalization: AO's `FpNormFloat: FloatBitsToInt24() >> 5` → VB's direct copy of 32 bits.
- Correct 8-bit underrun fill value: AO's `0x00` → VB's `0x80` (center for unsigned 8-bit).
- Correct the corresponding denormalizations symmetrically.

**Pre-conditions**: Phase 1 passed.

**Rationale**: These are the small-but-measurable differences identified in `docs/VB_CABLE_AO_COMPARISON.md` §1.3 and §3.2. They are independent of the state machine and position pump work, so they can land in their own phase. If they introduce regression, rollback is trivial.

**Files**: `Source/Utilities/loopback.cpp`

**Exact changes**:

Find `FpNorm32i` and `FpNorm32f` (or whatever the current helper function names are — AO uses helper function naming). Based on the comparison doc:

Current:
```cpp
static inline INT32 FpNorm32i(INT32 s) { return s >> 13; }
static inline INT32 FpNormFloat(float f) { return FloatBitsToInt24Bits(f) >> 5; }
```

New (matching VB):
```cpp
static inline INT32 FpNorm32i(INT32 s) { return s; }       // G9: VB uses direct copy for 32-bit PCM
static inline INT32 FpNormFloat(UINT32 bits) { return (INT32)bits; }  // G10: VB preserves raw float bits
```

Then find the matching `FpDenorm32i` and `FpDenormFloat`:
```cpp
static inline INT32 FpDenorm32i(INT32 v) { return v; }      // G9 mirror
static inline UINT32 FpDenormFloat(INT32 v) { return (UINT32)v; }  // G10 mirror
```

**★ IMPORTANT ★**: Because the 32-bit ring samples are now "whatever the app gave us" instead of "normalized to 19-bit", the mixing headroom assumption changes. If AO ever implements mixing (Phase 6+), the 32-bit path needs special handling. For now, cable-only paths are single-writer single-reader so no mixing, so the direct-copy is safe.

Find the underrun fill logic in `FramePipeReadFrames` (line 1492 and 1503). This currently uses `RtlZeroMemory`. Change it conditionally based on `pPipe->MicBitsPerSample`:

Actually — **wait**. `FramePipeReadFrames` operates on INT32 samples, not on the final denormalized Mic format. The 8-bit fill value discrepancy applies at the output stage (`FramePipeReadToDma`), not here. Revise:

Find the denormalization loop in `FramePipeReadToDma` (after 1900). When the target format is 8-bit, and we're filling silence because of underrun, the silence value should be `0x80`, not `0x00`. Check whether the current code writes zeros into the 8-bit output or writes the denormalized `FpDenorm8(0)` which already equals `0x80`.

**[DECISION POINT]** Read the actual code at `FramePipeReadToDma` 8-bit branch before editing. If `FpDenorm8(0)` already returns `0x80`, then zero-fill happens in the INT32 scratch domain and the subsequent denormalization produces `0x80` automatically — no change needed. If the code short-circuits to `RtlZeroMemory` when filling silence, change it to denormalize-zero instead.

Suspend the 8-bit fix if inspection shows it's already correct. Otherwise apply:

```cpp
if (pPipe->MicBitsPerSample == 8 && underrun_bytes > 0) {
    RtlFillMemory(dstPos, underrun_bytes, 0x80);  // G11: VB center value for 8-bit unsigned
}
```

**Changelog entry**:
```markdown
## 2026-04-13 Phase 2 — Format conversion corrections (Claude plan)

- loopback.cpp: FpNorm32i changed from `>> 13` to direct copy (G9)
- loopback.cpp: FpNormFloat changed from `FloatBitsToInt24 >> 5` to direct bit copy (G10)
- loopback.cpp: symmetric FpDenorm32i / FpDenormFloat corrections
- loopback.cpp: 8-bit underrun fill value changed from 0x00 to 0x80 (if not already correct) (G11)

Why: Match VB-Cable's observed per-format conversion behavior as documented in VB_CABLE_AO_COMPARISON.md §1.3 / §3.2. Bit-depth normalization was asymmetric for 32-bit paths, causing ~13 bits of dynamic range loss on INT32 PCM and potentially silent data on float passthrough.
```

**Verification**:
1. Build pass.
2. Install pass.
3. Existing 8ch/16ch isolation tests still pass.
4. Existing `test_bit_exact.py` (Q02 silence test, see M4b) still passes.
5. Play a 32-bit PCM test file through Cable A, capture from Cable A capture. Expect bit-exact round-trip.
6. Play a 32-bit IEEE float test file the same way. Expect bit-exact round-trip (new behavior — prior AO lost precision).

**Rollback**: `git restore Source/Utilities/loopback.cpp`. Rebuild.

**Exit criteria**: All verification steps pass, bit-exact test passes for 32-bit PCM and float.

---

### Phase 3 — Pump helper in shadow-and-compare mode (1.5 days)

**Goal**: Introduce `CMiniportWaveRTStream::PumpToCurrentPositionFromQuery(LARGE_INTEGER ilQPC)` as a new helper. In this phase, the helper **does not drive any transport** but DOES **compare** its frame-delta math against `UpdatePosition`'s byte-per-ms math and records divergence. This gives Phase 5/6 a quantitative parity signal before transport is actually moved.

**Scope**:
- Add the helper declaration to `Source/Main/minwavertstream.h` private section.
- Add the helper implementation to `Source/Main/minwavertstream.cpp`.
- Wire the helper into `GetPosition()` and `GetPositions()` so that both call it **after** `UpdatePosition()` returns, when `m_KsState == KSSTATE_RUN`. Calling order matters: `UpdatePosition()` computes `m_ullLinearPosition`-delta from its own math, then the pump reads the delta and compares.
- **Feature flags**: SetState RUN sets `AO_PUMP_FLAG_ENABLE | AO_PUMP_FLAG_SHADOW_ONLY`. The `DISABLE_LEGACY_*` bits remain clear, so transport still flows through `UpdatePosition → WriteBytes/ReadBytes`.
- The helper's body:
  1. Computes frame delta since baseline (same math as all subsequent phases).
  2. Runs 8-frame gate and over-jump guard.
  3. Increments `m_ulPumpInvocationCount`, `m_ulPumpGatedSkipCount`, `m_ulPumpOverJumpCount`, `m_ullPumpFramesProcessed`.
  4. **Shadow comparison**: reads the `ByteDisplacement` that `UpdatePosition()` produced on the same call (via a new shared temporary field), divides by `nBlockAlign` to get "legacy_frames", and compares against the pump's `newFrames`. If `|newFrames − legacy_frames|` exceeds a threshold (recommended: max(2, `newFrames / 16`)), increments `m_ulPumpShadowDivergenceCount`.
  5. Mirrors counters into `FRAME_PIPE` for IOCTL visibility.
- **No transport side effects.** The helper does not call `FramePipeWriteFromDma`, `FramePipeReadToDma`, `WriteBytes`, or `ReadBytes` in this phase. Phase 5 will add render transport, Phase 6 will add capture transport.

**Pre-conditions**: Phase 1 (diagnostic fields including shadow divergence counter and feature flags), Phase 2 (format corrections) passed.

**Rationale**: Codex's Phase 1 "skeleton + shadow-mode comparison path" pattern. Observer-only is insufficient — it can't detect math bugs (e.g. wrong QPC-to-frame conversion constant) until Phase 5/6 actually moves transport, at which point the math bug becomes audible. Shadow mode catches the bug in Phase 3 validation. The threshold-based divergence check tolerates small rounding differences between frame-delta and byte-per-ms math but flags systematic errors.

**Cross-contract between `UpdatePosition` and pump (new in Phase 1/3)**:

Because the pump needs to read what `UpdatePosition` just computed, add a small shared temporary field so the two helpers can communicate within a single `GetPosition()` / `GetPositions()` call. **This field is session-local and does not participate in WaveRT contract.**

Add to `minwavertstream.h` (in the same private: block as the other pump fields):
```cpp
    // Phase 3 shadow-mode handoff: UpdatePosition writes here, pump reads here.
    // Only meaningful within a single GetPosition / GetPositions call under
    // m_PositionSpinLock. Reset by the pump after comparison.
    ULONG                       m_ulLastUpdatePositionByteDisplacement;
```

Modify `UpdatePosition()` (minimal change — no behavioral change):

```cpp
// At the end of UpdatePosition(), after ByteDisplacement is finalized
// and after it's consumed by WriteBytes/ReadBytes, before the function
// returns, stash the value for the pump's shadow comparison:
m_ulLastUpdatePositionByteDisplacement = ByteDisplacement;
```

This is a one-line addition. `UpdatePosition`'s semantics are unchanged.

**File: `Source/Main/minwavertstream.h`**

In the `private:` section (after `UpdatePosition` declaration, around line 180):

```cpp
    VOID PumpToCurrentPositionFromQuery
    (
        _In_ LARGE_INTEGER ilQPC
    );
```

**File: `Source/Main/minwavertstream.cpp`**

Add the implementation (recommended location: immediately before the existing `UpdatePosition()` function around line 1405, so the two sit side-by-side for reviewers):

```cpp
//=============================================================================
// PumpToCurrentPositionFromQuery
//
// Phase 3: Shadow-and-compare mode. Computes frame delta from baseline QPC,
// applies 8-frame minimum gate and DMA over-jump guard, increments diagnostic
// counters, and compares its computed frame count against UpdatePosition's
// byte-per-ms ByteDisplacement to detect math divergence BEFORE Phase 5/6
// moves transport ownership.
//
// Does NOT drive any transport (that comes in Phase 5 and Phase 6).
//
// Called from GetPosition() and GetPositions() AFTER UpdatePosition() under
// m_PositionSpinLock when the stream is in KSSTATE_RUN.
//
// Design rationale: see §6.0 "Design principle: helper separation" — this
// helper owns the transport-advancement Responsibility B for cable endpoints.
// In Phase 3, it owns it *in shadow mode only* (feature flag
// AO_PUMP_FLAG_SHADOW_ONLY set, DISABLE_LEGACY_* clear). Phase 5 and Phase 6
// then flip the DISABLE_LEGACY_* bits one direction at a time.
//=============================================================================
#pragma code_seg()
VOID CMiniportWaveRTStream::PumpToCurrentPositionFromQuery
(
    _In_ LARGE_INTEGER ilQPC
)
{
    // Feature flag master gate — Phase 3 SetState RUN sets ENABLE|SHADOW_ONLY.
    // Non-cable streams never set ENABLE, so they return immediately.
    if (!(m_ulPumpFeatureFlags & AO_PUMP_FLAG_ENABLE)) return;

    // Only cable endpoints use the pump. Redundant with the flag check, but
    // defensive — non-cable miniports should never have the flag set.
    CMiniportWaveRT* pMp = m_pMiniport;
    if (!pMp) return;
    BOOL isCable = (pMp->m_DeviceType == eCableASpeaker ||
                    pMp->m_DeviceType == eCableBSpeaker ||
                    pMp->m_DeviceType == eCableAMic     ||
                    pMp->m_DeviceType == eCableBMic);
    if (!isCable) return;

    // Format sanity.
    if (!m_pWfExt) return;
    ULONG sampleRate  = m_pWfExt->Format.nSamplesPerSec;
    ULONG nBlockAlign = m_pWfExt->Format.nBlockAlign;
    if (sampleRate == 0 || nBlockAlign == 0) return;

    m_ulPumpInvocationCount++;

    // Convert QPC to 100ns units (same conversion as UpdatePosition).
    LONGLONG hnsCurrentTime = KSCONVERT_PERFORMANCE_TIME(
        m_ullPerformanceCounterFrequency.QuadPart, ilQPC);

    // Lazy init: on first call after RUN, set the baseline.
    if (!m_bPumpInitialized)
    {
        m_ullPumpBaselineHns     = (ULONGLONG)hnsCurrentTime;
        m_ulPumpProcessedFrames  = 0;
        m_bPumpInitialized       = TRUE;
        return;  // no work on the first call — nothing to compare against
    }

    // Elapsed 100ns since baseline.
    LONGLONG elapsed_100ns = hnsCurrentTime - (LONGLONG)m_ullPumpBaselineHns;
    if (elapsed_100ns < 0) elapsed_100ns = 0;

    // Total frames since baseline (64-bit, no overflow for reasonable run times).
    ULONGLONG totalFrames =
        ((ULONGLONG)elapsed_100ns * (ULONGLONG)sampleRate) / 10000000ULL;

    ULONG newFrames = 0;
    if (totalFrames > m_ulPumpProcessedFrames)
    {
        newFrames = (ULONG)(totalFrames - m_ulPumpProcessedFrames);
    }

    // G3: 8-frame minimum gate. VB: cmp ebx, 8; jl skip.
    if (newFrames < FP_MIN_GATE_FRAMES)
    {
        m_ulPumpGatedSkipCount++;
        return;
    }

    // Baseline rebase at 128*SR to prevent integer overflow on long runs.
    if (totalFrames >= ((ULONGLONG)sampleRate << 7))
    {
        m_ullPumpBaselineHns    = (ULONGLONG)hnsCurrentTime;
        m_ulPumpProcessedFrames = 0;
    }
    else
    {
        m_ulPumpProcessedFrames = (ULONG)totalFrames;
    }

    // G5: DMA over-jump guard.
    ULONG framesPerDmaBuffer = 0;
    if (m_ulDmaBufferSize > 0 && nBlockAlign > 0)
    {
        framesPerDmaBuffer = m_ulDmaBufferSize / nBlockAlign;
    }
    if (framesPerDmaBuffer > 0 && newFrames > (framesPerDmaBuffer / 2))
    {
        m_ulPumpOverJumpCount++;
        // Re-baseline so we don't keep over-jumping on successive calls.
        m_ullPumpBaselineHns    = (ULONGLONG)hnsCurrentTime;
        m_ulPumpProcessedFrames = 0;
        return;
    }

    // ─────────────────────────────────────────────────────────────────
    // Phase 3 SHADOW COMPARE: check whether the pump's frame-delta math
    // agrees with UpdatePosition's byte-per-ms math OVER A ROLLING WINDOW.
    //
    // Per-call comparison is too noisy: UpdatePosition's
    // m_hnsElapsedTimeCarryForward / m_byteDisplacementCarryForward legitimately
    // cause ByteDisplacement to be 0 on some calls while the pump sees >=8
    // frames, and vice versa. The two models converge on WALL-CLOCK time but
    // diverge on INDIVIDUAL calls by up to a millisecond worth of work.
    //
    // Solution: accumulate both counts into windowed totals and compare the
    // totals every N calls (default N=128). This averages out carry noise
    // while still catching systematic drift (wrong QPC conversion, wrong SR
    // constant, wrong frame-to-byte ratio).
    // ─────────────────────────────────────────────────────────────────
    if (m_ulPumpFeatureFlags & AO_PUMP_FLAG_SHADOW_ONLY)
    {
        // UpdatePosition has already set m_ulLastUpdatePositionByteDisplacement
        // earlier in this same GetPosition() call under the same spinlock.
        ULONG legacyBytes = m_ulLastUpdatePositionByteDisplacement;

        // Accumulate windowed totals (Phase 1 fields, monotonic within window).
        m_ullPumpShadowWindowPumpFrames   += newFrames;
        m_ullPumpShadowWindowLegacyBytes  += legacyBytes;
        m_ulPumpShadowWindowCallCount++;

        // Reset the handoff field so a missed UpdatePosition (e.g. during
        // gate-skip on the next call) cannot poison the accumulator.
        m_ulLastUpdatePositionByteDisplacement = 0;

        // Compare totals every SHADOW_WINDOW calls.
        #define AO_PUMP_SHADOW_WINDOW_CALLS 128
        if (m_ulPumpShadowWindowCallCount >= AO_PUMP_SHADOW_WINDOW_CALLS)
        {
            ULONGLONG legacyFrames =
                (nBlockAlign > 0) ? (m_ullPumpShadowWindowLegacyBytes / nBlockAlign) : 0;
            ULONGLONG pumpFrames = m_ullPumpShadowWindowPumpFrames;

            // Compute absolute difference and tolerance.
            // Tolerance: 2% of the larger total, with a floor of 16 frames.
            // Over 128 calls at Phone Link ~100 Hz → window ~1.28 sec → ~61k
            // frames at 48kHz → 2% is ~1228 frames → much larger than per-call
            // carry noise (< 48 frames/call) → catches systematic error only.
            ULONGLONG larger = (pumpFrames > legacyFrames) ? pumpFrames : legacyFrames;
            ULONGLONG tolerance = larger / 50;  // 2%
            if (tolerance < 16) tolerance = 16;

            ULONGLONG diff = (pumpFrames > legacyFrames)
                ? (pumpFrames - legacyFrames)
                : (legacyFrames - pumpFrames);

            if (diff > tolerance)
            {
                m_ulPumpShadowDivergenceCount++;
                // On divergence, log the window totals to ETW/DbgPrint so we
                // can see WHICH way they're drifting (pump > legacy means the
                // pump overcounts; pump < legacy means undercounts).
                DbgPrint("AO_SHADOW_DIVERGE pump=%llu legacy=%llu diff=%llu tol=%llu\n",
                         pumpFrames, legacyFrames, diff, tolerance);
            }

            // Reset window for next round.
            m_ullPumpShadowWindowPumpFrames  = 0;
            m_ullPumpShadowWindowLegacyBytes = 0;
            m_ulPumpShadowWindowCallCount    = 0;
        }
    }

    // Phase 3 observer/shadow mode: everything beyond this point is counters only.
    // Phase 5 will add: if (!m_bCapture && (flags & DISABLE_LEGACY_RENDER))  { render transport }
    // Phase 6 will add: if (m_bCapture  && (flags & DISABLE_LEGACY_CAPTURE)) { capture transport }

    m_ullPumpFramesProcessed += newFrames;

    // Mirror to pipe-level diagnostics for IOCTL visibility.
    PFRAME_PIPE pFP = NULL;
    if (m_bCapture)
    {
        if (pMp->m_DeviceType == eCableAMic) pFP = &g_CableAPipe;
        else if (pMp->m_DeviceType == eCableBMic) pFP = &g_CableBPipe;
    }
    else
    {
        if (pMp->m_DeviceType == eCableASpeaker) pFP = &g_CableAPipe;
        else if (pMp->m_DeviceType == eCableBSpeaker) pFP = &g_CableBPipe;
    }
    if (pFP && pFP->Initialized)
    {
        // Per-direction counter assignment (rev 2.4 race fix).
        // Speaker and Mic streams share the same pipe, so they must write to
        // DIFFERENT counter slots. Otherwise the two streams would race and
        // each overwrite the other. Split by m_bCapture.
        if (m_bCapture)
        {
            pFP->CaptureFramesProcessedTotal      += newFrames;
            pFP->CaptureGatedSkipCount             = m_ulPumpGatedSkipCount;
            pFP->CaptureOverJumpCount              = m_ulPumpOverJumpCount;
            pFP->CapturePumpShadowDivergenceCount  = m_ulPumpShadowDivergenceCount;
            pFP->CapturePumpInvocationCount        = m_ulPumpInvocationCount;
        }
        else
        {
            pFP->RenderFramesProcessedTotal       += newFrames;
            pFP->RenderGatedSkipCount              = m_ulPumpGatedSkipCount;
            pFP->RenderOverJumpCount               = m_ulPumpOverJumpCount;
            pFP->RenderPumpShadowDivergenceCount   = m_ulPumpShadowDivergenceCount;
            pFP->RenderPumpInvocationCount         = m_ulPumpInvocationCount;
        }
    }
}
#pragma code_seg()
```

**New FRAME_PIPE diagnostic fields** (Phase 1 addition to loopback.h):
```cpp
volatile ULONG      InvocationCount;          // pump invocations (from stream)
volatile ULONG      ShadowDivergenceCount;    // shadow-mode math disagreements
```

Wire into `GetPosition()` (line 790~817) — add the pump call AFTER `UpdatePosition()` so the shadow comparison can read `m_ulLastUpdatePositionByteDisplacement`:

```cpp
    if (m_KsState == KSSTATE_RUN)
    {
        LARGE_INTEGER ilQPC = KeQueryPerformanceCounter(NULL);
        UpdatePosition(ilQPC);                   // UNCHANGED (except one-line stash)
        PumpToCurrentPositionFromQuery(ilQPC);   // NEW Phase 3 — reads stash, compares
    }
```

Same for `GetPositions()` (line 1056~1098). **Order matters** — reversing the two calls would cause the pump to read stale `m_ulLastUpdatePositionByteDisplacement` from the *previous* query. Under `m_PositionSpinLock` there are no concurrency concerns within a single query.

**SetState RUN feature flag set** (Phase 4 will reconfirm this):

Inside `SetState` KSSTATE_RUN branch, after the existing `FramePipeRegisterFormat` call and after initializing the pump fields added in Phase 1, set the pump feature flags for cable streams:

```cpp
    // Phase 3: enable pump in shadow-and-compare mode for cable endpoints.
    if (m_pMiniport)
    {
        BOOL isCable = (m_pMiniport->m_DeviceType == eCableASpeaker ||
                        m_pMiniport->m_DeviceType == eCableBSpeaker ||
                        m_pMiniport->m_DeviceType == eCableAMic     ||
                        m_pMiniport->m_DeviceType == eCableBMic);
        if (isCable)
        {
            m_ulPumpFeatureFlags = AO_PUMP_FLAG_ENABLE | AO_PUMP_FLAG_SHADOW_ONLY;
        }
    }
```

**SetState STOP / PAUSE feature flag clear** (Phase 4 expands this — here we only set to zero):

```cpp
    // Pump is disabled outside RUN. Phase 4 will additionally clear
    // m_ullPumpBaselineHns, m_ulPumpProcessedFrames, m_bPumpInitialized, m_ulPumpLastBufferOffset.
    m_ulPumpFeatureFlags = 0;
```

**Changelog entry**:
```markdown
## 2026-04-13 Phase 3 — Pump helper in shadow-and-compare mode (Claude plan)

- minwavertstream.h: added private method PumpToCurrentPositionFromQuery declaration and m_ulLastUpdatePositionByteDisplacement stash field
- minwavertstream.cpp: added PumpToCurrentPositionFromQuery implementation with shadow-mode comparison against UpdatePosition's ByteDisplacement (no transport side effects when AO_PUMP_FLAG_SHADOW_ONLY is set alone)
- minwavertstream.cpp UpdatePosition: one-line addition to stash final ByteDisplacement into m_ulLastUpdatePositionByteDisplacement so the pump can read it
- minwavertstream.cpp GetPosition / GetPositions: call pump helper AFTER UpdatePosition when KSSTATE_RUN so shadow comparison can read the stash
- minwavertstream.cpp SetState RUN (cable endpoints): set m_ulPumpFeatureFlags = AO_PUMP_FLAG_ENABLE | AO_PUMP_FLAG_SHADOW_ONLY
- minwavertstream.cpp SetState STOP/PAUSE: clear m_ulPumpFeatureFlags to 0

Why: Cross-review with Codex plan flagged that rewriting UpdatePosition() risks breaking WaveRT packet interface (GetReadPacket/SetWritePacket). Added a sibling helper instead to isolate the new frame-delta math from the existing WaveRT presentation math. Phase 3 runs in SHADOW_ONLY mode — the pump computes frame delta + gate + over-jump + compares against UpdatePosition's byte-per-ms math, and records divergence, but does NOT drive transport. This gives Phase 5/6 a quantitative parity signal, and the Phase 3 acceptance bar is now the stricter **windowed `ShadowDivergenceCount == 0`** criterion before transport ownership moves. Reference: results/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CLAUDE.md §6.0.
```

**Verification**:
1. `.\build-verify.ps1 -Config Release` passes.
2. `.\install.ps1 -Action upgrade` passes without reboot.
3. Launch any audio app that polls position (Windows Sound Mixer shows live levels for Cable A speaker output; Phone Link call).
4. `python test_stream_monitor.py` shows:
   - `InvocationCount` growing with wall-clock time (cable streams in RUN are actively polled)
   - `GatedSkipCount` growing (polling-rate dependent; under aggressive polling it dominates `InvocationCount`)
   - `OverJumpCount` at zero under normal operation
   - `FramesProcessedTotal` growing at approximately `sampleRate` per second per active cable stream
   - **`ShadowDivergenceCount == 0` over the validation window** — this is the key Phase 3 signal
5. Existing audio tests still pass (this phase adds no transport change for cable streams — shadow flag is set, legacy path still runs).
6. Live call quality unchanged vs Phase 2 baseline (AO Cable still distorted, same level as before Phase 3 — this is expected because Phase 3 does not fix quality yet).

**Rollback options**:
- **Immediate (no reinstall)**: IOCTL clears `m_ulPumpFeatureFlags` → pump becomes a no-op. Runtime rollback, no rebuild needed. Audio path falls back to Phase 2 baseline.
- **Full**: `git restore Source/Main/minwavertstream.h Source/Main/minwavertstream.cpp`. Phase 1/2 state preserved.

**Exit criteria**:
- Build + install pass.
- `InvocationCount > 0` during a 1-minute RUN stream.
- `FramesProcessedTotal` is growing linearly with wall-clock time during a RUN stream.
- `OverJumpCount == 0` under normal operation.
- **`ShadowDivergenceCount == 0` over a 5-minute RUN interval under normal Phone Link polling**. The windowed compare (128-call window, 2% tolerance) should **never** tick in a correct implementation. Any nonzero value means systematic drift between the pump's frame-delta math and `UpdatePosition`'s byte-per-ms math — STOP and debug in Phase 3 before proceeding. Do NOT land transport ownership on broken math.
- All existing tests green.
- If `GatedSkipCount` is suspiciously zero (gate never firing), investigate polling rate — some paths may poll too slowly to cross 8 frames in a single call. This is OK as long as `FramesProcessedTotal` still grows.
- Cross-scenario check: the exit criterion must hold for **at least three** different client polling patterns (e.g. Phone Link live call + Windows Sound Mixer open + test harness `poll_stress.py` at 200 Hz). If divergence appears at one polling rate but not another, the pump's math has a rate-dependent bug and must be fixed.
- **Runtime feature-flag rollback proof (Phase 3 instance of Rule 16)**: clear `AO_PUMP_FLAG_ENABLE` via IOCTL during a live RUN stream → `InvocationCount` must stop ticking within one `GetPosition()` call, the helper must become a no-op, and `FramesProcessedTotal` must freeze. Re-set the flag → counters resume. Phase 3 is the first flag-gated phase in the plan, so this proof establishes the rollback mechanism itself works before Phase 5/6 relies on it for transport ownership. A failure here means Phase 5/6 cannot land because their rollback story is broken at the foundation.
- **Divergence counter meaning contract (Rule 15 corollary)**: `ShadowDivergenceCount` is a parity signal *while legacy cable transport still owns the path*. Once Phase 5 or Phase 6 sets `DISABLE_LEGACY_RENDER` / `DISABLE_LEGACY_CAPTURE`, the counter's interpretation changes: there is no longer a legacy math stream running alongside to compare against, so the counter is **frozen** at its final Phase 3 value and must be read as historical evidence, not live parity. Phase 5/6 verification steps that reference `ShadowDivergenceCount` must be explicit about this — "should not have grown since Phase 4 final value" is the only correct phrasing, not "should be zero live". (From Codex v4 Phase 3 "define the meaning window of divergence counters up front".)

**Divergence threshold tuning**: The windowed comparison uses `max(16 frames, 2% of larger total)` as tolerance over 128 calls. If 2% turns out to be too tight after real measurement, relax to 5% but never past 10% — beyond that we can't tell systematic error from carry noise. If 2% is comfortably achievable across all client types, consider tightening to 1% for Phase 3 re-validation.

---

### Phase 4 — State machine alignment (G1, G2) (2 days)

**Goal**: Make STOP stop collapsing the pipe, and make PAUSE conditionally reset the pipe the way VB does.

**Scope**:
- Remove the `FramePipeUnregisterFormat` call from the STOP branch.
- Add `FramePipeReset` call to the PAUSE branch, guarded by the same three conditions VB uses.
- Move format unregistration to a "stream teardown" point that's actually equivalent to VB's scope (probably the destructor, or `PnpStop`, or never — TBD after inspection).
- **Blocking task (rev 2.4, elevated from OQ4 per Codex v3)**: Before moving `FramePipeUnregisterFormat` out of STOP, **trace exactly how `SpeakerActive` / `MicActive` are cleared today** and confirm the new teardown path does not leave stale active-state in the pipe. If current code sets `SpeakerActive = FALSE` inside `FramePipeUnregisterFormat`, then moving the unregister out of STOP means a STOP'd-but-not-destroyed stream still has `Active = TRUE` in the pipe. This affects the Phase 4 PAUSE conditional reset check (`if (!otherSideActive) FramePipeReset`) — a stale `otherSideActive == TRUE` would prevent legitimate resets.
- **If needed, make `FramePipeUnregisterFormat` / `FramePipeReset` operations idempotent** before relocating them. "Idempotent" means calling them twice is the same as calling them once — required for moving them to stream destructor where the lifecycle is less predictable.
- **Phase 4a: Extract `ApplyFadeInRamp` helper (moved from Phase 6, rev 2.4)**. The fade-in-ramp code in `WriteBytes()` lines 1570-1656 must be extracted into a standalone `CMiniportWaveRTStream::ApplyFadeInRamp(fadeStartOffset, totalBytes)` method **before** Phase 6 wants to use it. Doing the extract here in Phase 4 (while still in the "legacy transport owns everything" state) makes it a pure refactor: `WriteBytes` now calls `ApplyFadeInRamp` at its tail, behavior identical. Then Phase 6 can simply call `ApplyFadeInRamp` from its new pump capture branch without also doing the extract work in the same phase. **Codex v3 implementation order step 8** recommends this ordering; adopted here.

**Pre-conditions**: Phase 1, Phase 2, Phase 3 passed. Phase 3's zero-divergence windowed acceptance criterion MUST be satisfied before landing Phase 4 — the state-machine changes here should not mask a pump math bug.

**Rationale**: This is the biggest STRUCTURAL gap. VB's steady-state call flow involves ACQUIRE → PAUSE → RUN → PAUSE → RUN → … cycles during the call, with `STOP` only happening at teardown. AO's current behavior is to tear down the pipe every time a client goes to STOP, which is **the wrong place** to reset state. (Phase 5/6 is the biggest TRANSPORT gap — those two fix the cable-specific distortion. Phase 4 fixes the pipe-collapse-on-STOP dropouts.)

**File 1: `Source/Main/minwavertstream.cpp`**

**Change A — STOP branch (lines 1211~1232)**:

Current:
```cpp
// FRAME_PIPE: unregister format on stop
if (m_pMiniport)
{
    PFRAME_PIPE pFP = NULL;
    BOOLEAN fpIsSpeaker = !m_bCapture;
    if (m_bCapture)
    {
        if (m_pMiniport->m_DeviceType == eCableAMic)
            pFP = &g_CableAPipe;
        else if (m_pMiniport->m_DeviceType == eCableBMic)
            pFP = &g_CableBPipe;
    }
    else
    {
        if (m_pMiniport->m_DeviceType == eCableASpeaker)
            pFP = &g_CableAPipe;
        else if (m_pMiniport->m_DeviceType == eCableBSpeaker)
            pFP = &g_CableBPipe;
    }
    if (pFP)
        FramePipeUnregisterFormat(pFP, fpIsSpeaker);
}
```

New:
```cpp
// G1 (VB parity): STOP does NOT unregister format or reset the pipe.
// VB's FUN_140005910 STOP path only clears per-stream timing state.
// Format unregistration happens at stream teardown (destructor / PnP stop).
// This prevents PipeReset on every STOP->RUN cycle during normal call flow.
```

Also clear the pump state added in Phase 1:
```cpp
// Clear pump baseline (Phase 1 fields) — stream is no longer live
m_ullPumpBaselineHns      = 0;
m_ulPumpProcessedFrames   = 0;
m_bPumpInitialized        = FALSE;
m_ulPumpLastBufferOffset  = 0;
```

**Change B — PAUSE branch (lines 1267~1304)**:

Currently the PAUSE branch does timer cancel + `KeFlushQueuedDpcs`, then calls `GetPositions(NULL, NULL, NULL)` to refresh positions. Before the GetPositions call, add the VB-style conditional pipe reset:

```cpp
case KSSTATE_PAUSE:

    if (m_KsState > KSSTATE_PAUSE)
    {
        //
        // Run -> Pause
        //

        // FRAME_PIPE: do NOT unregister on Pause.
        // Windows audio engine pauses/resumes streams frequently between audio chunks.
        // Unregistering on Pause causes pipe drain → massive underrun.
        // Only teardown should unregister.

        // Pause DMA
        if (m_ulNotificationIntervalMs > 0)
        {
            ExCancelTimer(m_pNotificationTimer, NULL);
            KeFlushQueuedDpcs();

            // [existing DPC time carry-forward logic unchanged]
            // ...
        }

        // G2 (VB parity): conditionally reset pipe on PAUSE.
        // VB FUN_140005910 PAUSE path:
        //   if (previous state > 2)            // was RUN
        //     if (stream+0x58 != 0)            // timer handle was registered
        //       if (stream+0x168 != 0)         // ring pointer is valid
        //         call FUN_1400039ac          // ring reset
        //
        // AO equivalent: all three conditions check out if we reached this
        // point (previous state was RUN because m_KsState > KSSTATE_PAUSE,
        // timer was active because m_ulNotificationIntervalMs > 0,
        // pipe is valid if pFP is non-null).
        if (m_pMiniport)
        {
            PFRAME_PIPE pFP = NULL;
            if (m_bCapture)
            {
                if (m_pMiniport->m_DeviceType == eCableAMic)
                    pFP = &g_CableAPipe;
                else if (m_pMiniport->m_DeviceType == eCableBMic)
                    pFP = &g_CableBPipe;
            }
            else
            {
                if (m_pMiniport->m_DeviceType == eCableASpeaker)
                    pFP = &g_CableAPipe;
                else if (m_pMiniport->m_DeviceType == eCableBSpeaker)
                    pFP = &g_CableBPipe;
            }

            // Only reset the pipe if THIS stream is the last active side.
            // If the other direction is still RUN, leave the ring alone so it
            // keeps flowing. Match VB's conditional semantics.
            if (pFP && pFP->Initialized)
            {
                BOOLEAN otherSideActive = m_bCapture ? pFP->SpeakerActive : pFP->MicActive;
                if (!otherSideActive)
                {
                    // Safe: we're inside the SetState path, PASSIVE_LEVEL, and
                    // we already called KeFlushQueuedDpcs above.
                    FramePipeReset(pFP);
                }
            }
        }
    }

    // Also clear pump baseline — we're no longer advancing the stream.
    m_ullPumpBaselineHns     = 0;
    m_ulPumpProcessedFrames  = 0;
    m_bPumpInitialized       = FALSE;

    // This call updates the linear buffer and presentation positions.
    GetPositions(NULL, NULL, NULL);
    break;
```

**[IMPORTANT NUANCE]** VB's condition check is "previous state was above PAUSE" which means previous state was RUN. The AO check `m_KsState > KSSTATE_PAUSE` accomplishes the same thing *at entry*. We also need to honor "only reset if the other side isn't already running" — otherwise pausing the Speaker would clear the Mic's in-flight data, which would be the wrong behavior for a virtual cable where both sides can transition independently.

**Change C — find stream teardown point and move format unregistration there**

Look for `CMiniportWaveRTStream::~CMiniportWaveRTStream()` (the destructor) or a `Close()` / `PnpStop()` path. Add the `FramePipeUnregisterFormat` call there instead. Be careful:

- The destructor is called at PASSIVE_LEVEL
- The destructor runs when portcls drops the last reference
- If we pause and never destroy (long-lived stream across many call attempts), we never unregister → pipe keeps its format registration, `SameRate` stays set, `SpeakerActive/MicActive` stays set → this is correct behavior matching VB

**Changelog entry**:
```markdown
## 2026-04-13 Phase 4 — State machine alignment with VB (Claude plan)

- minwavertstream.cpp SetState STOP: removed FramePipeUnregisterFormat call (G1, VB parity)
- minwavertstream.cpp SetState PAUSE: added conditional FramePipeReset when transitioning from RUN (G2, VB parity)
- minwavertstream.cpp stream destructor/teardown: moved FramePipeUnregisterFormat here (unchanged net effect at real teardown, but no longer fires on STOP-during-call)
- SetState STOP / PAUSE: clear m_ullPumpBaselineHns / m_ulPumpProcessedFrames / m_bPumpInitialized for next RUN

Why: VB's FUN_140005910 has different STOP/PAUSE semantics than AO assumed. Verified by direct decompile inspection + Codex cross-check. STOP collapse was causing AO to lose state on every stream state transition during a normal call, triggering apparent "dropouts" that were actually ring resets.
```

**Verification**:
1. Build + install.
2. Run `tests/live_call/run_test_call.py` with `AUDIO_CABLE_PROFILE=ao`.
3. During the call, have the user report whether the audio is clean or distorted.
4. Monitor `test_stream_monitor.py` for `DropCount` / `UnderrunCount` deltas.
5. **Pump counter regression check**: `ShadowDivergenceCount` must remain at the accepted Phase 3 value (normally `0`) after Phase 4 changes. If Phase 4's state-machine reordering (STOP no longer resets, PAUSE conditional reset) causes pump baseline to desync, the divergence counter will tick — investigate before continuing.
6. WinDbg: break at `CMiniportWaveRTStream::SetState` and observe the STOP path — confirm it no longer calls `FramePipeUnregisterFormat`.

**Expected live behavior**: After Phase 4, call-quality dropouts caused by STOP-era pipe collapse should stop. PAUSE → RUN churn during a normal call should no longer drain the pipe. The remaining distortion (if any) is the transport-side-bytes issue that Phase 5 (render) and Phase 6 (capture) address. Phase 3's pump is still in SHADOW_ONLY mode at this point — it runs and logs but does not drive transport, so quality improvement here comes purely from the state-machine fix.

**Rollback**: `git restore Source/Main/minwavertstream.cpp` (runtime rollback via `m_ulPumpFeatureFlags` only affects the pump, not the state-machine fix in this phase).

**Exit criteria**:
- Live call does not crash.
- Audio is at least no worse than before Phase 4 (ideally noticeably better due to pipe no longer collapsing on STOP cycles).
- `FramePipeUnregisterFormat` is not being called on STOP (verified via log or WinDbg).
- `FramePipeReset` is being called when a stream transitions from RUN to PAUSE if the other side isn't also RUN (verified via log).
- Phase 3 `ShadowDivergenceCount` is still at the accepted Phase 3 value (normally `0`).

---

### Phase 5 — Pump drives render transport for cable speakers (G3, G4, G5, G12 for render side) (1.5 days)

**Goal**: The Phase 3 shadow-and-compare pump now starts driving render-side transport for cable speaker endpoints. Specifically: when a cable speaker is in RUN, `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` is set, and the pump computes `newFrames >= 8`, the pump calls `FramePipeWriteFromDma` with the frame-derived byte count. `UpdatePosition()`'s render branch has its `ReadBytes()` call guarded by the flag (`!pumpOwnsRender`) so the transport is not double-processed. The flag provides runtime rollback — IOCTL clears it → legacy `ReadBytes` transport resumes within one `GetPosition` call.

**Scope**:
- Modify `PumpToCurrentPositionFromQuery()` (from Phase 3) to call `FramePipeWriteFromDma` for cable **render** streams that pass the gate and the over-jump guard.
- Modify `UpdatePosition()`'s existing render branch to skip `ReadBytes()` when the stream is a cable render endpoint (the pump handles it now).
- **Capture path is not touched in this phase** — that's Phase 6. AO cable capture continues to run through `UpdatePosition() → WriteBytes() → FramePipeReadToDma()` unchanged during Phase 5.

**Pre-conditions**: Phase 3 (pump skeleton) and Phase 4 (state machine alignment) both passed. Phase 4 must land before Phase 5 because the state machine change is what makes PAUSE stop collapsing the pipe during call flow — if we move render transport to the pump while state machine is still collapsing on every STOP, we'd see drop-out symptoms that mix state-change and transport-change causes.

**Rationale**: Codex's Phase 2 "move render-side progression behind the query-driven helper" — one side at a time. Render first because the user-observed distortion during the original test runs was described as render-side (Speaker → Phone Link) being choppy.

**File: `Source/Main/minwavertstream.cpp` — modify `PumpToCurrentPositionFromQuery` (extend the Phase 3 pump body)**:

Original Phase 3 has a placeholder comment in the pump body. Replace it with an actual transport call:

Find this block from Phase 3:
```cpp
    // Phase 3 shadow-and-compare mode: everything beyond this point is counters only.
    // Phase 5 will add: if (!m_bCapture && (flags & DISABLE_LEGACY_RENDER))  { render transport }
    // Phase 6 will add: if (m_bCapture  && (flags & DISABLE_LEGACY_CAPTURE)) { capture transport }

    m_ullPumpFramesProcessed += newFrames;
```

Replace with:
```cpp
    // Phase 5: render transport driven from pump for cable speakers.
    // Gated by AO_PUMP_FLAG_DISABLE_LEGACY_RENDER. Phase 4 SetState RUN
    // does NOT set this flag. Phase 5 SetState RUN sets it. IOCTL can
    // clear it at runtime for emergency fallback to legacy transport.
    // (Phase 6 will add the symmetric capture branch.)
    if (!m_bCapture &&
        (m_ulPumpFeatureFlags & AO_PUMP_FLAG_DISABLE_LEGACY_RENDER) &&
        pFP && pFP->Initialized && m_pDmaBuffer && m_ulDmaBufferSize > 0)
    {
        ULONG bytesToMove = newFrames * nBlockAlign;

        // R9 / OQ7 mitigation: use the pump's own buffer offset rather than
        // m_ullLinearPosition. On first call after RUN, sync to the current
        // WaveRT view so we pick up where UpdatePosition left off, then advance
        // independently to prevent long-run drift.
        if (m_ulPumpLastBufferOffset == 0 && m_ullLinearPosition > 0)
        {
            m_ulPumpLastBufferOffset = (ULONG)(m_ullLinearPosition % m_ulDmaBufferSize);
        }
        ULONG bufferOffset = m_ulPumpLastBufferOffset;

        // Drive transport in chunks to handle DMA buffer wrap.
        ULONG remaining = bytesToMove;
        while (remaining > 0)
        {
            ULONG chunk = min(remaining, m_ulDmaBufferSize - bufferOffset);
            FramePipeWriteFromDma(pFP, m_pDmaBuffer + bufferOffset, chunk);
            bufferOffset = (bufferOffset + chunk) % m_ulDmaBufferSize;
            remaining -= chunk;
        }
        m_ulPumpLastBufferOffset = bufferOffset;

        // Note: we do NOT advance m_ullLinearPosition / m_ullWritePosition here.
        // UpdatePosition() still owns those fields (WaveRT contract).
    }

    m_ullPumpFramesProcessed += newFrames;
```

Phase 5 also promotes the flag in SetState RUN for cable speakers:
```cpp
    if (isCable && !m_bCapture)
    {
        // Phase 5: render transport now owned by pump.
        m_ulPumpFeatureFlags |= AO_PUMP_FLAG_DISABLE_LEGACY_RENDER;
        // SHADOW_ONLY bit remains set — pump still runs shadow compare for
        // counters visibility, even though divergence is no longer meaningful
        // on a disowned legacy path. Consider clearing SHADOW_ONLY here if
        // divergence counter is noisy.
    }
```

And in `UpdatePosition()`'s render branch, the guard changes from "skip for cable" to "skip for cable WHEN pump owns it":
```cpp
    CMiniportWaveRT* pMp = m_pMiniport;
    BOOL isCable = (pMp && (pMp->m_DeviceType == eCableASpeaker ||
                             pMp->m_DeviceType == eCableBSpeaker));
    BOOL pumpOwnsRender = isCable &&
                          (m_ulPumpFeatureFlags & AO_PUMP_FLAG_DISABLE_LEGACY_RENDER);
    if (!pumpOwnsRender && !g_DoNotCreateDataFiles)
    {
        ReadBytes(ByteDisplacement);
    }
```

**This dual-gate pattern is the rollback safety net**: if the pump fails for any reason and IOCTL clears `DISABLE_LEGACY_RENDER`, the very next `UpdatePosition()` call will immediately resume driving transport through the legacy `ReadBytes()` path. No rebuild, no reinstall, no reboot — just one IOCTL write.

Then in `UpdatePosition()` (around line 1484~1494), find the existing render branch:

```cpp
    {
        // Cable A/B always calls ReadBytes for loopback.
        // Non-cable devices only call ReadBytes when saving to file.
        CMiniportWaveRT* pMp = m_pMiniport;
        BOOL isCable = (pMp && (pMp->m_DeviceType == eCableASpeaker ||
                                 pMp->m_DeviceType == eCableBSpeaker));
        if (isCable || !g_DoNotCreateDataFiles)
        {
            ReadBytes(ByteDisplacement);
        }
    }
```

Change the `if` so that cable render streams skip `ReadBytes()` (the pump handles them now):

```cpp
    {
        // Cable render: transport is driven by PumpToCurrentPositionFromQuery.
        // Non-cable render: continue to use ReadBytes (savedata fallback).
        CMiniportWaveRT* pMp = m_pMiniport;
        BOOL isCable = (pMp && (pMp->m_DeviceType == eCableASpeaker ||
                                 pMp->m_DeviceType == eCableBSpeaker));
        if (!isCable && !g_DoNotCreateDataFiles)
        {
            ReadBytes(ByteDisplacement);
        }
    }
```

**Important subtlety**: `UpdatePosition()` still computes `ByteDisplacement` from bytes-per-ms math and still advances `m_ullLinearPosition`, `m_ullWritePosition`, `m_ullPresentationPosition` accordingly. **This is intentional**. WaveRT packet-mode clients expect those fields to progress at the bytes-per-ms rate. The pump advances the pipe transport at a (slightly different, frame-delta-derived) rate. The two rates agree in the long run because they both track wall-clock time; short-term differences average out over the position query interval.

**Changelog entry**:
```markdown
## 2026-04-13 Phase 5 — Pump drives render transport (Claude plan)

- minwavertstream.cpp PumpToCurrentPositionFromQuery: added FramePipeWriteFromDma call for cable speaker endpoints (render branch)
- minwavertstream.cpp UpdatePosition: render branch now skips ReadBytes for cable streams (handled by pump); non-cable render continues to use ReadBytes as before

Why: Phase 3 shadow-and-compare mode confirmed the frame-delta math without any windowed divergence events under the acceptance run. Now the pump takes over render transport for cables, gated by AO_PUMP_FLAG_DISABLE_LEGACY_RENDER. Runtime rollback: IOCTL clears the flag and UpdatePosition's ReadBytes call resumes immediately (no rebuild, no reinstall). UpdatePosition's WaveRT presentation contract fields (m_ullLinearPosition, m_ullPresentationPosition, m_ullDmaTimeStamp) are unchanged. Rule 7 (helper separation) + Rule 11 (rollback path) compliant.
```

**Verification**:
1. Build + install.
2. Live call test — AO cable render path. User reports quality.
3. `test_stream_monitor.py`: `FramesProcessedTotal` grows linearly, `GatedSkipCount` grows, `OverJumpCount` stays zero, `DropCount` stays zero (pipe has plenty of space).
4. Non-cable render path (savedata file capture in non-CABLE_A/B builds) still works.
5. WinDbg spot check: BP on `FramePipeWriteFromDma` during a live call, confirm it fires from `PumpToCurrentPositionFromQuery` and NOT from `ReadBytes` for cable streams.
6. **Runtime rollback smoke test**: via IOCTL, clear `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` during a live call → audio should seamlessly transition back to legacy `ReadBytes` transport with no audible interruption. Re-set the flag → transport transitions back to pump. Both transitions clean.
7. **Phase 3 shadow counter** semantics note: once `DISABLE_LEGACY_RENDER` is set for a cable render stream, the pump owns transport, so `ShadowDivergenceCount` is no longer meaningful for that stream (there's no legacy math running in parallel to compare against). The counter freezes at its Phase 3 final value; interpret as historical parity evidence.

**Rollback options**:
- **Immediate (no reinstall)**: IOCTL clears `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER`. Transport falls back to legacy `ReadBytes` within one `GetPosition()` call.
- **Full**: `git restore Source/Main/minwavertstream.cpp`. Phase 3 and Phase 4 changes preserved.

**Exit criteria**:
- Live render test: user reports improvement vs pre-Phase-5 baseline (this is the first phase where user-reported quality should demonstrably change for the better on the Speaker → Phone Link path).
- Diagnostic counters confirm pump is the render transport driver (BP confirms call origin).
- `DropCount == 0` under normal load; aggressive polling may cause legitimate drops — document if so.
- `OverJumpCount == 0` under normal operation.
- Non-cable paths untouched (no regression in savedata or non-cable capture paths).
- **WinDbg one-owner confirmation (Rule 13)**: BP on `FramePipeWriteFromDma` during a live call must show call stack originating exclusively from `PumpToCurrentPositionFromQuery` for cable render streams. If `ReadBytes → FramePipeWriteFromDma` appears even once for a cable render stream, Phase 5 is incomplete — roll back.
- **Runtime rollback smoke test (Rule 16)**: during a live Phone Link call, IOCTL toggling `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` must revert render transport to legacy `ReadBytes` **within one or two position queries** (observable via `FramesProcessedTotal` growth moving from pump counters back to legacy path), with no audible pop, underrun spike, or `DropCount` burst on either transition direction. Unverified = Phase 5 not merged.
- No crashes, no BSOD.

---

### Phase 6 — Pump drives capture transport for cable mics (G3, G4, G5 for capture side) (1.5 days)

**Goal**: Symmetric to Phase 5 but for capture. Pump drives `FramePipeReadToDma` for cable mic endpoints; `UpdatePosition`'s capture branch skips `WriteBytes()` for cable streams.

**Scope**:
- Add the capture branch to `PumpToCurrentPositionFromQuery`.
- Guard `UpdatePosition`'s `WriteBytes()` call with `!isCable` for cable mic streams.
- Preserve the existing fade-in ramp logic (which lives inside `WriteBytes`) — **this is critical**. See "Important subtlety #2" below.

**Pre-conditions**: Phase 5 passed. User-reported render-side quality is good.

**Rationale**: Codex's Phase 3 "move capture-side progression behind the query-driven helper" — same pattern as Phase 5 but for capture. Splitting render and capture into two phases lets us verify each direction independently; if one breaks we know exactly which one.

**Important subtlety #1 — fade-in ramp already extracted in Phase 4 (rev 2.4)**: The fade-in ramp code was moved to a standalone helper `CMiniportWaveRTStream::ApplyFadeInRamp()` during Phase 4 per Codex v3's recommended implementation order. Phase 6 can now simply call this existing helper from the new pump capture branch — no extraction work here. The `WriteBytes()` function still calls `ApplyFadeInRamp()` at its tail (unchanged behavior for the legacy path). The pump's new capture branch calls it after `FramePipeReadToDma` returns.

**Rationale for the Phase 4 extraction vs. Phase 6 extraction**: Performing the extract in the same phase that uses it (Phase 6) would mix two independent changes (refactor + behavior change) in one phase. Codex v3 correctly recommends extracting in Phase 4 (where it's a pure refactor), verifying Phase 4's audio is unchanged, then Phase 6 adds the new call site without also moving code.

**Important subtlety #2 — mic capture needs scratch buffer wrap handling**: `WriteBytes()` loops over `ByteDisplacement` in chunks up to `m_ulDmaBufferSize - bufferOffset` to handle DMA buffer wrap. The pump must do the same:

```cpp
    // Phase 6: capture transport driven from pump for cable mics.
    // Gated by AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE (same pattern as Phase 5 render gate).
    if (m_bCapture &&
        (m_ulPumpFeatureFlags & AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE) &&
        pFP && pFP->Initialized && m_pDmaBuffer && m_ulDmaBufferSize > 0)
    {
        ULONG bytesToMove = newFrames * nBlockAlign;

        // R9 / OQ7 mitigation: use pump's own buffer offset (same pattern as Phase 5).
        if (m_ulPumpLastBufferOffset == 0 && m_ullLinearPosition > 0)
        {
            m_ulPumpLastBufferOffset = (ULONG)(m_ullLinearPosition % m_ulDmaBufferSize);
        }
        ULONG bufferOffset = m_ulPumpLastBufferOffset;
        ULONG fadeStartOffset = bufferOffset;

        ULONG remaining = bytesToMove;
        while (remaining > 0)
        {
            ULONG chunk = min(remaining, m_ulDmaBufferSize - bufferOffset);
            FramePipeReadToDma(pFP, m_pDmaBuffer + bufferOffset, chunk);
            bufferOffset = (bufferOffset + chunk) % m_ulDmaBufferSize;
            remaining -= chunk;
        }
        m_ulPumpLastBufferOffset = bufferOffset;

        // Apply fade-in ramp if this is the start of the capture stream.
        // (Helper extracted from the original WriteBytes fade-in block.)
        if (m_ulFadeInRemaining > 0 && bytesToMove > 0)
        {
            ApplyFadeInRamp(fadeStartOffset, bytesToMove);
        }
    }
```

Phase 6 SetState RUN sets the capture flag for cable mics:
```cpp
    if (isCable && m_bCapture)
    {
        m_ulPumpFeatureFlags |= AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE;
    }
```

And `WriteBytes()`'s cable short-circuit similarly gates on the flag:
```cpp
    BOOL isCable = (devType == eCableAMic || devType == eCableBMic);
    BOOL pumpOwnsCapture = isCable &&
                           (m_ulPumpFeatureFlags & AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE);
    if (pumpOwnsCapture) return;  // pump handles it
    // ... non-cable fallback ...
```

`ApplyFadeInRamp()` was already defined in Phase 4 as a pure refactor (see Phase 4 scope). Phase 6 just uses it from the new pump capture branch. The definition (copied here for reference, but the actual commit happened in Phase 4) looks like this:

```cpp
// Defined in Phase 4, NOT added here. Phase 6 just calls it.
VOID CMiniportWaveRTStream::ApplyFadeInRamp(ULONG fadeStartOffset, ULONG totalBytes)
{
    if (m_ulFadeInRemaining == 0 || totalBytes == 0 || !m_pWfExt) return;
    // ... 16-bit / 24-bit / 32-bit float branches extracted from old WriteBytes ...
    m_ulFadeInRemaining -= min(sampleCount, m_ulFadeInRemaining);
}
```

Phase 6 simplifies `WriteBytes()` to short-circuit when the pump owns capture transport (flag-gated), then falls through to legacy transport otherwise:

```cpp
VOID CMiniportWaveRTStream::WriteBytes(_In_ ULONG ByteDisplacement)
{
    ULONG bufferOffset = m_ullLinearPosition % m_ulDmaBufferSize;
    ULONG fadeStartOffset = bufferOffset;
    ULONG totalBytes = ByteDisplacement;

    // Phase 6: cable capture is handled by PumpToCurrentPositionFromQuery
    // ONLY when AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE is set. If the flag is
    // cleared (Phase 3/4 state, or runtime IOCTL rollback), WriteBytes runs
    // as before (legacy capture fallback).
    PFRAME_PIPE pPipe = NULL;
    CMiniportWaveRT* pMiniport = m_pMiniport;
    eDeviceType devType = eMaxDeviceType;
    if (pMiniport)
    {
        devType = pMiniport->m_DeviceType;
        if (devType == eCableAMic) pPipe = &g_CableAPipe;
        else if (devType == eCableBMic) pPipe = &g_CableBPipe;
    }

    BOOL isCable = (devType == eCableAMic || devType == eCableBMic);
    BOOL pumpOwnsCapture = isCable &&
                           (m_ulPumpFeatureFlags & AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE);
    if (pumpOwnsCapture) return;  // pump handles it — see Phase 6 pump branch

    // Cable or non-cable fallback: run the legacy transport.
    while (ByteDisplacement > 0)
    {
        ULONG runWrite = min(ByteDisplacement, m_ulDmaBufferSize - bufferOffset);
        if (pPipe)
        {
            // Cable fallback: legacy pipe read → DMA
            FramePipeReadToDma(pPipe, m_pDmaBuffer + bufferOffset, runWrite);
        }
        else
        {
            // Non-cable fallback: silence fill
            RtlZeroMemory(m_pDmaBuffer + bufferOffset, runWrite);
        }
        bufferOffset = (bufferOffset + runWrite) % m_ulDmaBufferSize;
        ByteDisplacement -= runWrite;
    }

    ApplyFadeInRamp(fadeStartOffset, totalBytes);
}
```

Also modify `UpdatePosition()`'s capture branch (around line 1451~1454). Since `WriteBytes` now short-circuits internally on the flag, the caller could stay unchanged — but for explicit symmetry with Phase 5's render guard, mirror the flag-gated pattern at the call site too:

```cpp
    if (m_bCapture)
    {
        CMiniportWaveRT* pMp = m_pMiniport;
        BOOL isCable = (pMp && (pMp->m_DeviceType == eCableAMic ||
                                 pMp->m_DeviceType == eCableBMic));
        BOOL pumpOwnsCapture = isCable &&
                               (m_ulPumpFeatureFlags & AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE);
        if (!pumpOwnsCapture)
        {
            WriteBytes(ByteDisplacement);  // legacy fallback (cable or non-cable)
        }
        // pumpOwnsCapture: handled by PumpToCurrentPositionFromQuery earlier in this call
    }
```

**Dual-gate symmetry**: The guard is duplicated at the call site AND inside `WriteBytes`. This is intentional — if someone calls `WriteBytes` from another place (future code), the inner check is a safety net. Both must be flag-gated so runtime IOCTL rollback works no matter which path triggers `WriteBytes`.

**Changelog entry**:
```markdown
## 2026-04-13 Phase 6 — Pump drives capture transport (Claude plan)

- minwavertstream.cpp PumpToCurrentPositionFromQuery: added FramePipeReadToDma call for cable mic endpoints (capture branch) with DMA wrap handling
- minwavertstream.cpp: added ApplyFadeInRamp helper method extracting fade-in logic from WriteBytes
- minwavertstream.cpp WriteBytes: simplified for non-cable streams only; cable mics now short-circuit and rely on pump
- minwavertstream.cpp UpdatePosition: capture branch skips WriteBytes call for cable streams

Why: Symmetric to Phase 5. Capture side of cable transport now driven by position polling via the pump. Fade-in ramp preserved via helper extraction (rule: do not regress AO wins). Reference: §6.0 helper separation principle.
```

**Verification**:
1. Build + install.
2. Live call test with actual microphone speech from remote side. User reports quality.
3. **Fade-in regression check**: stop and restart capture stream (e.g., toggle `tests/live_call/run_test_call.py` twice). Verify no pop/click on second start.
4. Bit-exact regression (Q02 silence) still passes.
5. 16-channel isolation test still passes.
6. `test_stream_monitor.py`: both sides now show `FramesProcessedTotal` growth.
7. **Phase 3/4/5 regression check**: `ShadowDivergenceCount` should remain at its Phase 3 final value (frozen — divergence is no longer measured on DISOWNED legacy paths, but the counter should not tick). `OverJumpCount` should still be zero. `DropCount` should stay low (any sustained growth means pipe overflow — investigate Capacity vs poll rate).
8. **Runtime rollback smoke test (both directions)**: IOCTL clears `AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE` → capture transport reverts to legacy `WriteBytes` within one `GetPosition()` call. Re-set → back to pump. Repeat with `DISABLE_LEGACY_RENDER` (Phase 5's flag). No audible glitch on transitions.

**Rollback options**:
- **Immediate (no reinstall)**: IOCTL clears `AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE` (and `DISABLE_LEGACY_RENDER` from Phase 5 if needed). Transport falls back to legacy within one call.
- **Full**: `git restore Source/Main/minwavertstream.cpp`. Phase 5 changes preserved.

**Exit criteria**:
- Live call is clean bidirectionally (user-reported).
- Fade-in ramp still audibly smooths capture start (no pop/click).
- No regression on bit-exact or isolation tests.
- `ShadowDivergenceCount` has NOT grown since Phase 4 — Phase 6's transport change is flag-gated and should not affect the counter (which was already frozen once `DISABLE_LEGACY_*` was set).
- **WinDbg one-owner confirmation (Rule 13)**: BP on `FramePipeReadToDma` during a live call must show call stack originating exclusively from `PumpToCurrentPositionFromQuery` for cable capture streams. If `WriteBytes → FramePipeReadToDma` appears even once for a cable capture stream, Phase 6 is incomplete — roll back.
- **Runtime rollback smoke test both directions (Rule 16)**: IOCTL toggling `AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE` must revert capture transport to legacy `WriteBytes` **within one or two position queries** during a live Phone Link call; and the same flag must be togglable together with `DISABLE_LEGACY_RENDER` (Phase 5's flag) without audible glitch in either direction. Unverified = Phase 6 not merged.
- **1차 목표 (VB-equivalent call quality) achieved.** Phase 6 is the terminal phase for the primary goal; Phases 7 and 8 are optional quality/architecture improvements.

---

### Phase 7 — SRC quality improvements (G7, G8) (2 days, OPTIONAL for 1차 목표)

**Goal**: Replace AO's sinc-based SRC with the VB-proven weighted-accumulate linear polyphase approach. Unify write-path and read-path SRC into a single function with a direction flag.

**Scope**:
- This phase is **optional** if Phases 3~6 already produce VB-equivalent call quality at matched rates (48kHz end-to-end). It becomes **required** only when we need mismatched-rate support to match VB quality.
- Add a unified resampler function in `loopback.cpp`.
- Route both `FramePipeWriteFromDma` and `FramePipeReadToDma` through it when rates differ.
- Remove the `LB_SRC_STATE` sinc sinc table usage for the FRAME_PIPE path.

**Pre-conditions**: Phase 6 passed (both render and capture pump paths stable).

**Rationale**: VB's write-path SRC (`+0x26a0`) is linear-weighted polyphase with GCD ratio detection. AO's current sinc implementation is mathematically higher quality in theory but has been implicated in several quality bugs over the project's history. Switching to linear matches VB and eliminates a class of bugs.

**Sketch**:

```cpp
enum SrcDirection { SRC_WRITE_TO_PIPE, SRC_READ_FROM_PIPE };

static ULONG ResampleLinearPolyphase(
    const INT32* src, ULONG srcFrames, ULONG srcRate,
    INT32* dst, ULONG dstFrames, ULONG dstRate,
    ULONG channels,
    INT32* carryAccum,     // [channels] state kept across calls
    INT32* carryResid,     // [channels]
    SrcDirection dir)
{
    // 1. Try GCD candidates 300/100/75 (matches VB +0x26a0)
    ULONG gcd = 0;
    const ULONG candidates[] = { 300, 100, 75 };
    for (ULONG c : candidates) {
        if (srcRate % c == 0 && dstRate % c == 0) { gcd = c; break; }
    }
    if (gcd == 0) gcd = SimpleGcd(srcRate, dstRate);
    ULONG srcPeriod = srcRate / gcd;
    ULONG dstPeriod = dstRate / gcd;
    // 2. Weighted accumulate loop (matches VB pattern)
    for (ULONG i = 0; i < srcFrames; i++) {
        for (ULONG ch = 0; ch < channels; ch++) {
            INT32 sample = src[i * channels + ch];
            INT64 contribution = ((INT64)sample * dstPeriod) / srcPeriod;
            carryAccum[ch] += (INT32)contribution;
            carryResid[ch] = sample - (INT32)contribution;
            if (/* output tick */) {
                dst[...] = carryAccum[ch];
                carryAccum[ch] = carryResid[ch];
            }
        }
    }
    // 3. Return frames written to dst
}
```

The exact loop structure needs careful attention to maintain per-channel phase. [INF] This is the hardest function to get right; allow for multiple revisions.

**Carry state location**: Add `INT32 SrcAccumulator[FP_MAX_CHANNELS]; INT32 SrcResidual[FP_MAX_CHANNELS];` to `FRAME_PIPE` struct (per-pipe, reset in `FramePipeReset`).

**Changelog entry** (when complete):
```markdown
## 2026-04-13 Phase 7 — Unified SRC (Claude plan)
- loopback.cpp: added ResampleLinearPolyphase unified function
- FramePipeWriteFromDma / FramePipeReadToDma: route through unified SRC on rate mismatch
- FRAME_PIPE: added SrcAccumulator / SrcResidual carry state
- Retired sinc-based LB_SRC_STATE use from FRAME_PIPE path (legacy LOOPBACK_BUFFER path still uses it until phased out)

Why: Match VB +0x26a0 resampler pattern. Linear polyphase with GCD ratio detection matches VB exactly and eliminates AO's sinc-related quality bugs. Unified write/read function guarantees symmetric behavior (CLAUDE.md rule 5).
```

**Verification**:
1. Build + install.
2. Play 44.1kHz WAV through Cable A (speaker side) with Cable A pipe at 48kHz internal.
3. Capture from Cable A (mic side).
4. Analyze captured WAV: SNR, THD+N, spectrogram vs reference.
5. Compare against VB baseline (same test on VB Cable).
6. Expected: AO within 3dB of VB on SNR, no visible aliasing in spectrogram.

**Rollback**: `git restore Source/Utilities/loopback.cpp`.

**Exit criteria**: Mixed-rate test passes at VB-equivalent quality.

---

### Phase 8 — Shared timer + auxiliary work + notification refinement (was Phase 6) (G6, G13, G15, G16) (2 days, OPTIONAL)

**Goal**: Replace AO's per-stream WaveRT notification timer with a shared `ExAllocateTimer(HIGH_RESOLUTION)` + refcount model, matching VB's `+0x65b8` / `+0x669c`. Optionally add peak meter helper. Tune notification firing.

**Scope**:
- This phase is **optional** for 1차 목표 (functional equivalence). It is required for 2차 목표 (architecture-level VB parity).
- Most of the user-visible quality wins come from Phases 3~6.

**Pre-conditions**: Phase 7 passed (or skipped).

**Rationale**: See Codex plan §Target AO architecture Layer 5. The shared-timer model is architecturally nicer but doesn't change the critical-path behavior.

**Sketch**:

1. Add `GlobalTimerState` structure (one per driver or one per cable — decide based on whether A/B should share or not):
   ```cpp
   struct GlobalTimerState {
       PEX_TIMER TimerHandle;
       LONG      ActiveStreamCount;
       CMiniportWaveRTStream* StreamArray[MAX_STREAMS];
       KSPIN_LOCK Lock;
   };
   static GlobalTimerState g_CableATimerState;
   static GlobalTimerState g_CableBTimerState;
   ```

2. `RegisterStream(stream)`: acquire lock, add to array, if first call `ExAllocateTimer(EX_TIMER_HIGH_RESOLUTION=4)` and `ExSetTimer(-10000, 10000, NULL)`.

3. `UnregisterStream(stream)`: remove from array, decrement count, if last call `ExDeleteTimer(handle, TRUE, FALSE, NULL)`.

4. Timer callback: iterate array and do per-stream auxiliary work. **Do NOT duplicate the position-driven pump here** — that's the job of Phases 3, 5, and 6. Only do things like watchdog checks, meter updates, or notification scheduling.

5. In `SetState` RUN: replace `ExSetTimer(m_pNotificationTimer, ...)` with `RegisterStream(this)`.
6. In `SetState` PAUSE (and stream teardown): replace `ExCancelTimer(m_pNotificationTimer, NULL)` with `UnregisterStream(this)`.

**Exit criteria**: Build passes, tests pass, no per-stream timer allocations remain.

---

### Phase 9 — Validation hardening and benchmarks (was Phase 7) (2.5 days)

**Goal**: Run the full benchmark suite, compare against VB baseline, prove the pump is robust across polling regimes, measure long-run position drift, document results.

**Scope**:
- `tests/live_call/run_test_call.py` A/B against VB
- `test_compare_vb.py` (from M4e) automated benchmark
- Long-run stability (30min, 60min) with drift measurement
- Application compatibility matrix (Teams, OBS, Discord, Phone Link)
- Q02 silence bit-exact regression check
- Q01 (if the harness is fixed) bit-exact round-trip
- **Polling-regime stress matrix (new in revision 2.2)** — 5 client cadence scenarios
- **Position drift measurement protocol (new in revision 2.2)** — quantitative thresholds

#### 9.1 Polling-regime stress matrix

The 8-frame gate behavior depends on how often `GetPosition()` / `GetPositions()` fire. Real-world clients poll at very different cadences. This matrix ensures the pump is robust across the distribution.

| Scenario | Poll rate | Expected pump behavior | Pass criterion |
|---|---|---|---|
| **Aggressive** | ≥ 500 Hz (stress harness hammering `GetPosition`) | `GatedSkipCount` dominates `InvocationCount` (ratio > 0.5); audio remains clean | No drops, no distortion, **`ShadowDivergenceCount == 0`** on the windowed counter. If divergence ticks ONLY under aggressive polling but stays clean under normal cadence, **suspect conversion/handoff bugs first** (format converter re-entry, DMA offset recomputation under rapid calls, pipe lock contention) — not the pump math itself. (Guidance from Codex v4 polling-regime validation.) |
| **Normal** | ~100 Hz (Phone Link live call) | Mix of gated and processing; `FramesProcessedTotal` grows at `sampleRate`/sec | Live call reported as "clean"; `ShadowDivergenceCount == 0` over 5-min window |
| **Sparse** | ~10 Hz (Discord/OBS typical capture polling) | Most calls pass the gate (large `newFrames` each); `GatedSkipCount` is minority | No drops; each pump call moves ~`sampleRate/10` frames. Sparse cadence must not create unexplained `OverJumpCount` ticks in a healthy build — if it does, the over-jump guard threshold is too tight for realistic client cadences. |
| **Near-no** | < 1 Hz (near-quiescent client) | Pump only activates when a query arrives; auxiliary timer (Phase 8) keeps continuity if any | Audio quality on return from quiescent state is clean (no stale data, no underrun after sudden query burst). **Explicit check**: auxiliary timer/watchdog duties must not accidentally disappear when the public client goes quiet — confirm notifications and metering still fire from the timer path after Phase 8. |
| **Mixed** | Multiple clients at different cadences simultaneously | No double-advance of transport; pipe counters match expected per-stream throughput | No `DropCount` growth, no duplicate frames visible in captured audio. Mixed polling from multiple clients querying the same stream at different cadences must not create double-advance bugs — if `FramesProcessedTotal` exceeds `sampleRate × elapsed_sec`, Rule 13 (one owner per direction) is being violated by a per-client accounting bug even though the call-site owner is singular. |

**Test harness suggestion**: write a small user-mode tool (`tests/live_call/poll_stress.py`) that opens a cable capture handle, spins a thread that calls `IAudioCaptureClient::GetNextPacketSize` (which triggers `GetPosition` internally) at a controlled rate. Vary the rate and observe `test_stream_monitor.py` counters + audio output.

#### 9.2 Position drift measurement protocol

**Background**: The pump tracks `m_ulPumpLastBufferOffset` independently from `m_ullLinearPosition`. Both represent "where are we in the DMA buffer?", computed from different math models. They should stay close, but will not be bit-exact.

**Metric**: `drift_bytes = |m_ulPumpLastBufferOffset − (m_ullLinearPosition % m_ulDmaBufferSize)|`

**Threshold**: `drift_bytes ≤ 1ms × sampleRate × blockAlign`
- At 48000 Hz stereo 16-bit: `≤ 192 bytes` (48 frames × 4 bytes/frame)
- At 48000 Hz 7.1 24-bit: `≤ 1152 bytes` (48 frames × 24 bytes/frame)
- Rule: "no more than 1ms of audio data disagreement between the two position models"

**Measurement cadence**: Sample via IOCTL every 60 seconds for 30 minutes → 30 samples.

**Pass criterion**: `max(drift_bytes) ≤ threshold` AND `drift_bytes` does not monotonically grow (i.e. the two models are co-drifting around wall clock, not accumulating error).

**If drift grows monotonically**:
- **Direction hint**: if `m_ulPumpLastBufferOffset` runs *ahead* of `m_ullLinearPosition % m_ulDmaBufferSize`, the pump is over-counting frames per query (elapsed-to-frames rounds up too often, or the gate skip accounting double-commits). If it runs *behind*, the pump is under-counting (gate threshold loses remainder frames between calls, or the 8-frame skip forgets the skipped elapsed time instead of carrying it forward into the next call).
- Suspect QPC conversion rounding error in the pump's elapsed-to-frames math
- Suspect `UpdatePosition`'s `m_hnsElapsedTimeCarryForward` leaking remainder differently than the pump's `m_ulPumpProcessedFrames`
- **Diagnosis order (Codex v4)**: if drift grows in one direction only, **exhaust conversion/accounting diagnosis before adding drift nudge logic**. Unidirectional drift almost always means a specific bug (rounding bias, remainder-leak) that can be fixed at the source — adding a nudge masks the bug and makes future debugging harder. Only introduce the nudge if diagnosis shows the two models genuinely cannot track each other over long runs for reasons unrelated to pump math (e.g. wall-clock drift between QPC and the DMA hardware timestamp).
- Action (fallback only): add a periodic "drift nudge" — if `drift_bytes` exceeds threshold, re-sync `m_ulPumpLastBufferOffset = m_ullLinearPosition % m_ulDmaBufferSize` and increment a `DriftResyncCount` counter
- This is a fail-safe fallback that sacrifices the pump/UpdatePosition independence in order to keep audio clean; use only if Phase 9 validation shows the two models genuinely can't track each other over long runs.

**Expected outcome in a clean build**: `drift_bytes` oscillates in the ±32 byte range, never growing, averaging near zero.

#### 9.3 Standard benchmark suite

(Unchanged from revision 2: bit-exact Q02, latency, dropout, 16-channel isolation, multi-client, device switching.)

**Deliverables**:
- Updated `docs/BENCHMARK_SUMMARY.md`
- Test log files in `results/benchmark_YYYYMMDD_HHMMSS/`
- Polling-regime matrix results table
- Position drift measurement chart (30-sample time series over 30 minutes)
- Sign-off in `docs/VBCABLE_SURPASS_PLAN.md` if M5 criteria now hold

---

## 7. Verification matrix per phase (revision 2 numbering)

| Phase | Build | Install | Unit tests | IOCTL diag | Live call | Bit-exact | SNR vs VB |
|---|---|---|---|---|---|---|---|
| 0 | ✓ | ✓ | ✓ | ✓ | baseline | ✓ | baseline |
| 1 | ✓ | ✓ | ✓ | ✓ (+new fields, all zero) | baseline | ✓ | baseline |
| 2 | ✓ | ✓ | ✓ | ✓ | no regression | ✓ (32bit improved) | no change |
| 3 (pump observer) | ✓ | ✓ | ✓ | ✓ (FramesProcessedTotal growing, OverJump==0) | no change | ✓ | no change |
| 4 (state machine) | ✓ | ✓ | ✓ | ✓ (STOP no longer resets) | **improved** (PAUSE/STOP churn no longer collapses pipe) | ✓ | moderate improvement |
| 5 (pump→render) | ✓ | ✓ | ✓ | ✓ (GatedSkip > 0 on render path) | **render side clean** | ✓ | near-VB on render |
| 6 (pump→capture) | ✓ | ✓ | ✓ | ✓ (both sides counters active) | **target: VB-equivalent bidirectional** | ✓ | near-VB bidirectional |
| 7 (SRC) | ✓ | ✓ | ✓ | ✓ | equivalent | ✓ | VB-equivalent or better at mismatched rates |
| 8 (shared timer) | ✓ | ✓ | ✓ | ✓ (shared timer visible) | equivalent | ✓ | equivalent |
| 9 (validation) | ✓ | ✓ | full matrix | full matrix | full matrix | full matrix | **measured and documented** |

**1차 목표 완료 시점**: Phase 6 통과. 이 시점에 call quality가 VB-equivalent가 되고, Phase 7/8은 선택, Phase 9는 최종 문서화.

---

## 7.5 Rollback checkpoints (consolidated)

This is a single-table summary of what the driver state should look like after each phase, what to suspect first if the phase broke something, and which rollback mechanism to use. It complements §8 (which has per-scenario failure narratives) with a quick-reference lookup.

| Checkpoint | After Phase | Expected driver state | First things to suspect if broken | Rollback mechanism |
|---|---|---|---|---|
| **A** | 0 (Pre-flight) | Build passes, current baseline installed, IOCTL diag passes. No code change. | Build pipeline, WDK version, signing, target machine reachability. | N/A (no change yet). |
| **B** | 1 (Struct + diagnostic fields) | Compiles, installs, all new counters visible in `test_stream_monitor.py` at zero. Existing tests green. | `C_ASSERT` struct size mismatch in `ioctl.h`; missing init in `Init()`; three-file diagnostic sync drift. | `git restore` the 8 touched files. |
| **C** | 2 (Format normalization) | 32-bit PCM bit-exact round-trip works. 32-bit float bit-exact round-trip works. 8-bit silence is `0x80` (if applicable). Q02 still passes. | Symbol lookup (`FpNorm*` / `FpDenorm*` names may differ from plan). Scratch buffer still uses normalized samples internally — check post-Phase-2 mixing assumptions. | `git restore Source/Utilities/loopback.cpp`. |
| **D** | 3 (Pump observer + shadow compare) | `InvocationCount` growing linearly. `FramesProcessedTotal` growing linearly. `OverJumpCount == 0`. **`ShadowDivergenceCount == 0`** over 5-min windowed comparison. Live audio quality unchanged from pre-Phase-3 baseline (pump is SHADOW_ONLY, transport still legacy). | QPC-to-100ns conversion wrong (check `KSCONVERT_PERFORMANCE_TIME`). Sample-rate constant wrong. `m_ullPumpBaselineHns` not initialized on first call. Pump called under wrong lock. Handoff field (`m_ulLastUpdatePositionByteDisplacement`) not set by `UpdatePosition`. | **Immediate**: IOCTL clears `m_ulPumpFeatureFlags` → pump becomes no-op. **Full**: `git restore minwavertstream.h minwavertstream.cpp`. |
| **E** | 4 (State machine alignment) | STOP no longer calls `FramePipeUnregisterFormat`. PAUSE from RUN calls `FramePipeReset` conditionally. Live call has fewer dropouts than pre-Phase-4. **`ShadowDivergenceCount` still 0.** | Format unregistration not moved to destructor → stream leaks. PAUSE reset condition wrong (check `otherSideActive`). Pump baseline not cleared correctly on state transitions → divergence ticks. | `git restore` the `SetState` section. Runtime rollback not applicable (state change isn't flag-gated). |
| **F** | 5 (Pump → render transport) | `FramePipeWriteFromDma` BP confirms caller = `PumpToCurrentPositionFromQuery`, not `ReadBytes`. Render-side live call quality NOTICEABLY improved. Non-cable render unchanged. `DropCount == 0`. | Double transport (both pump AND legacy running) — verify `!pumpOwnsRender` guard in `UpdatePosition`. `m_ulPumpLastBufferOffset` not lazy-initialized → pump writes at wrong offset. Flag not actually set in `SetState` RUN. | **Immediate**: IOCTL clears `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` → `UpdatePosition`'s `ReadBytes` resumes within one call. **Full**: `git restore`. |
| **G** | 6 (Pump → capture transport) | Both directions clean. Fade-in ramp still smooths capture start. Bit-exact regression still passes. 16-ch isolation still passes. **1차 목표 achieved.** | Fade-in ramp bypassed (check `ApplyFadeInRamp` extraction and call site). Capture wrap math wrong. Double transport on capture side. Flag not set. | **Immediate**: IOCTL clears `AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE`. **Full**: `git restore`. |
| **H** | 7 (SRC unification, optional) | 44.1 ↔ 48 mixed-rate test passes VB-equivalent quality. Existing same-rate path unchanged. | GCD ratio wrong. Per-channel carry state reset prematurely. Polyphase accumulator overflow on high-channel-count streams. | `git restore loopback.cpp`. |
| **I** | 8 (Shared timer, optional) | Shared timer visible. Per-stream timer retired. No shared-timer deadlock at teardown. | `ExAllocateTimer` / `ExDeleteTimer` race under heavy pause/resume churn. Refcount off-by-one. | `git restore` the timer ownership section. |
| **J** | 9 (Validation) | Benchmark suite green. Polling-regime matrix all scenarios pass. Position drift measurement within 1ms threshold over 30 min. | Drift growing monotonically (QPC rounding). Polling-regime failures on sparse/mixed scenarios (pump logic assumes steady cadence). | No code rollback — fix the failing scenario and re-run. If fundamental, drop back to Phase 6 and redesign. |

**Rule of thumb**: If a checkpoint fails, rollback to the **previous** checkpoint and re-plan. Do NOT stack speculation on top of a broken state. If two consecutive checkpoints fail, the plan itself may have a flaw — review the design, not just the code.

Runtime feature-flag rollback (Phases 3 / 5 / 6) is cheap: it takes a single IOCTL write and applies within one `GetPosition()` call. This is why the plan insists on flag-gating every transport-ownership transition (Rule 11). If a phase's failure mode is subtle or intermittent, leaving the flag-clear rollback in place while investigating in parallel is SAFER than `git restore` → rebuild → reinstall → retest, because the latter loses runtime state and may mask transient failures.

---

## 8. Failure modes and rollback policy

### 8.1 "Phase X broke something that Phase X-1 had working"

**Policy**: Immediate `git restore` on the files that Phase X touched. Do not try to debug on top of a broken phase. Re-test Phase X-1 to confirm it's back to green, then re-plan Phase X.

### 8.2 "Phase 3 pump helper crashes the driver"

**Likely cause**: `PumpToCurrentPositionFromQuery` called before `m_bPumpInitialized` baseline is set, or called outside `m_PositionSpinLock`, or shadow-mode stash field (`m_ulLastUpdatePositionByteDisplacement`) accessed without the lock held.

**Remediation**: Verify the helper is only called from `GetPosition()` / `GetPositions()` under `m_PositionSpinLock` and AFTER `UpdatePosition()` (so the stash field is populated). The lazy-init branch is idempotent and should be safe.

### 8.3 "Phase 4 state alignment: STOP leaks pipe state"

**Symptom**: Phase 4 removes `FramePipeUnregisterFormat` from STOP but doesn't move it anywhere. If the stream destructor (Phase 4 Change C) isn't wired correctly, streams that never actually destruct (long-lived WaveRT streams across many call attempts) will leak their format registration in the pipe.

**Remediation**: Verify that `FramePipeUnregisterFormat` runs from `~CMiniportWaveRTStream()`. Put a BP on it during a teardown. If the destructor is never called, the leak is real — need to find another teardown point (e.g. `PnpStop` or `Close`).

### 8.4 "Phase 5/6 pump drives transport but audio is still distorted"

**Diagnostic path**:
1. Check `GatedSkipCount` growth rate. If it's zero on the cable path, the gate isn't firing — check that `PumpToCurrentPositionFromQuery` is actually reaching the gate comparison (single-step in WinDbg).
2. Check `OverJumpCount` growth rate. If nonzero, the QPC → frame conversion has a wrong constant or unit, OR the system is legitimately stalling.
3. Check `FramesProcessedTotal` growth rate. Should be ~`sampleRate` frames/sec.
4. WinDbg: BP on `FramePipeWriteFromDma` during a live call. Confirm the caller is `PumpToCurrentPositionFromQuery`, not `ReadBytes`.
5. Verify that `UpdatePosition`'s `ReadBytes` / `WriteBytes` calls for cable streams are actually guarded by `!isCable` (the guard edits from Phase 5 and Phase 6). If they aren't, transport is running **twice** — once from the pump, once from `UpdatePosition` — which would push twice as much data as expected.

### 8.5 "KeFlushQueuedDpcs hangs (Phase 4 PAUSE path)"

**Unlikely but possible**: `FramePipeReset` is being called at the wrong IRQL, or a nested lock is being held.

**Remediation**: Ensure `FramePipeReset` has `PAGED_CODE()` assertion. Ensure the call path from `SetState PAUSE` is at PASSIVE_LEVEL (it should be — SetState is called from portcls at PASSIVE). Also: do NOT hold `m_PositionSpinLock` while calling `KeFlushQueuedDpcs` (§4.0 Rule 8, lock nesting prohibition).

### 8.6 "The 8-frame gate makes streams start slowly (Phase 3/5/6)"

**Symptom**: First audio is delayed by up to ~1ms after RUN.

**Tradeoff**: This is inherent to the gate design. VB does the same thing. If this is a problem for a specific workload, the gate threshold can be lowered to 4 or even 1 (defeating the purpose). Don't lower it without data showing it matters. Note that the gate only affects the **first** call after each polling interval that crosses the threshold — steady-state polling cadence is unaffected.

### 8.7 Pre-commit hook failure

**Policy**: Per CLAUDE.md, if a pre-commit hook fails, **do not** `--amend`. Fix the issue and create a new commit. Also update `docs/PIPELINE_V2_CHANGELOG.md` to reflect the final state.

---

## 9. Risk register

| # | Risk | Probability | Impact | Mitigation |
|---|---|---|---|---|
| R1 | Position query lock-ordering regression (pump + UpdatePosition both inside `m_PositionSpinLock`) | LOW | HIGH | Keep all state mutations inside `m_PositionSpinLock`; do NOT nest `PipeLock` inside `m_PositionSpinLock` (§4.0 Rule 8) |
| R2 | **Phase 5/6 pump runs transport in parallel with UpdatePosition's bytes-per-ms math, causing double-processing or math divergence** | MEDIUM | HIGH | Phase 5/6 explicitly guards `UpdatePosition`'s `ReadBytes`/`WriteBytes` with `!isCable`; verification step 5 in Phase 5/6 requires WinDbg confirmation that transport fires exactly once per position query |
| R3 | Phase 4 PAUSE conditional reset collapses the other direction's in-flight data | MEDIUM | HIGH | Guard reset on `!otherSideActive`; regression test transitioning only one side while the other is RUN |
| R4 | Phase 7 linear polyphase worse than current sinc on some content | LOW | MEDIUM | Quality benchmark (SNR, spectrogram) before committing; keep sinc path alive behind compile flag as fallback |
| R5 | Phase 8 shared timer deadlock at teardown | MEDIUM | CRITICAL | Don't hold the global lock across `ExDeleteTimer` — signal-then-wait pattern; test pause/resume churn heavily |
| R6 | VB decompile has a detail wrong that we didn't catch | MEDIUM | MEDIUM | Cross-check with Codex plan and dynamic trace; if in doubt, preserve AO's existing behavior for that detail and file as Open Question |
| R7 | Installation or device-state churn reveals a latent pipe-lifecycle bug | MEDIUM | HIGH | Run the M4e automated comparison after every phase, not just Phase 9 |
| R8 | Phase 1 struct field additions break C_ASSERT size guards in ioctl.h | LOW | MEDIUM | Update C_ASSERT with new expected sizes in the same commit as the struct change (Diagnostics Rule three-file sync) |
| R9 | **Pump and UpdatePosition position drift over long runs** — pump uses frame-delta from baseline QPC, UpdatePosition uses bytes-per-ms-carry. Over 30+ minutes, the two views of "current position" may diverge enough that the pump's buffer offset lookup (`m_ullLinearPosition % m_ulDmaBufferSize`) lands on stale data. | MEDIUM | MEDIUM | Add dedicated `m_ulPumpLastBufferOffset` field (§Phase 1) and advance it inside the pump independently of `m_ullLinearPosition`. This decouples the pump's buffer-offset tracking from WaveRT's presentation position entirely. Long-run validation in Phase 9 measures drift. |

---

## 10. Tools, commands, and live-debug playbook

### 10.1 Build + install cycle
```powershell
.\build-verify.ps1 -Config Release
.\install.ps1 -Action upgrade
```

### 10.2 Live call test
```powershell
cd tests\live_call
# Set AUDIO_CABLE_PROFILE=ao in .env
python run_test_call.py
```

### 10.3 Stream monitor (parallel window)
```powershell
python test_stream_monitor.py
# watch DropCount, UnderrunCount, GatedSkipCount, OverJumpCount, FramesProcessedTotal
```

### 10.4 WinDbg kernel session
Target-side connection info already recorded in `results/vb_session.log` format. To attach:
```
# On host:
windbg -k net:port=50000,key=<key>,target=192.168.0.4
```

### 10.5 Useful WinDbg commands for AO debugging

Find AO driver base (post-reboot, since kernel ASLR changes it):
```
lm m ao*
```

Break on state transitions:
```
bu ao_driver!CMiniportWaveRTStream::SetState "dd @rdx L1; gc"
```

Break on position pump entry (after Phase 3):
```
bu ao_driver!CMiniportWaveRTStream::PumpToCurrentPositionFromQuery ".printf \"Pump call\\n\"; gc"
```

Monitor gate hits (after Phase 3 shadow-and-compare helper is in place):
```
# Phase 3 adds the helper + counters. This BP fires only when the gate triggers.
bu ao_driver!CMiniportWaveRTStream::PumpToCurrentPositionFromQuery+XX ".printf \"Gated skip\\n\"; gc"
# where +XX is the address of the `if (newFrames < FP_MIN_GATE_FRAMES)` branch
```

Monitor shadow divergence (fires only when windowed compare exceeds tolerance):
```
bu ao_driver!CMiniportWaveRTStream::PumpToCurrentPositionFromQuery+YY ".printf \"Shadow divergence\\n\"; gc"
# where +YY is the address of the m_ulPumpShadowDivergenceCount++ line
```

Verify pump drives transport exactly once after Phase 5/6:
```
bu ao_driver!FramePipeWriteFromDma "k 3; gc"
# Expected caller: PumpToCurrentPositionFromQuery (not ReadBytes, not UpdatePosition)
bu ao_driver!FramePipeReadToDma "k 3; gc"
# Expected caller: PumpToCurrentPositionFromQuery (not WriteBytes, not UpdatePosition)
```

Dump pipe state:
```
dt ao_driver!_FRAME_PIPE g_CableAPipe
```

### 10.6 Changelog discipline
Per CLAUDE.md:
- Every code change logged in `docs/PIPELINE_V2_CHANGELOG.md`
- Write entry BEFORE or IMMEDIATELY AFTER the edit
- Date, files, what, why
- No exceptions

### 10.7 Diagnostics rule
Per CLAUDE.md:
- If you change stream-status diagnostics, update all three in one commit:
  - `Source/Main/ioctl.h`
  - `Source/Main/adapter.cpp`
  - `test_stream_monitor.py`
- Do not trust hardcoded struct offsets without re-verifying layout

---

## 11. Open questions (with resolution path)

### OQ1. Does VB's position-query path use `stream+0x5420`, `stream+0x4598`, or both?

**Observed**: Live stack showed `GetKsAudioPosition → vbaudio+0x54bb` (inside `+0x5420`).
**Static**: `results/vbcable_pipeline_analysis.md` §5.2 documents `+0x4598` as a separate handler that also calls `+0x6320`.
**Resolution**: Read decompile of both functions side-by-side. Decide which path Windows portcls actually invokes for `KSPROPERTY_AUDIO_POSITION`. If both exist, they may be for different KS properties (AudioPosition vs Position vs PositionEx).
**Blocking**: No — AO's `GetPosition` / `GetPositions` path is already the correct shape; we just need to match content.

### OQ2. VB's `+0x6adc` is render helper or capture helper?

**Observed**: Live trace never saw it fire. Static disassembly shows it called from `+0x68ac` render branch AND from `+0x6320` render branch. But [CDXS§4] notes "Mic capture path" based on prior static analysis.
**Resolution**: Read full decompile of `+0x6adc` carefully before Phase 5 render-side code.
**Blocking**: Not for Phase 5 (we use AO's existing `FramePipeWriteFromDma`), but good to know.

### OQ3. Does AO have a currently-working `IMiniportWaveRTStream::GetPosition` equivalent that portcls actually calls?

**Current assumption**: Yes, `CMiniportWaveRTStream::GetPosition` at line 790. Called via WaveRT interface.
**Verification**: Put a BP on `GetPosition` during a live AO call and confirm it fires. If it doesn't, portcls is using a different entry point, and the Phase 3 shadow-and-compare pump will never run.
**Blocking**: **Critical for Phase 3.** This is the entry point that `PumpToCurrentPositionFromQuery` hooks into. Verify BEFORE coding Phase 3 — spend 15 minutes in WinDbg on a current-state driver to confirm `GetPosition` fires during a Phone Link call.

### OQ4. How is the pipe's `Speaker/Mic Active` flag updated when `FramePipeUnregisterFormat` moves to the destructor?

**Current**: `FramePipeUnregisterFormat` is called on STOP from `SetState`. When we move it to the destructor (Phase 4), the `Active` flag stays set between STOP and destruct.
**Implication**: The `FramePipeReset` condition check `if (!otherSideActive)` in Phase 4 PAUSE branch may need to be re-examined.
**Resolution**: Trace through: what if Speaker goes to STOP (we don't unregister), Mic is still RUN, Speaker stream object is then destroyed? Does `g_CableAPipe.SpeakerActive` go to FALSE on destroy? If yes, good. If no, we have stale state.
**Mitigation**: Make `FramePipeUnregisterFormat` safe to call multiple times (idempotent) and call it both on stream destroy and (for safety) in PAUSE if the stream is really going away.

### OQ5. VB's auxiliary write-only rings (`b2aa0000`, `b27e0000`) — what are they?

**Observation**: During live call, trace showed writes-only to two additional A-side rings beyond the main `b0320000`.
**Current interpretation**: [INF] Multi-client capture fan-out — each capture stream gets its own ring, shared timer writes to all.
**Resolution**: Inspect `+0x5cc0` callback logic more carefully to understand how it picks rings. This is **out of scope for Phases 1~8**; note for Phase 9 or later.

### OQ6. (Resolved in revision 2) Do we need to retire `m_ulBlockAlignCarryForward` / `m_hnsElapsedTimeCarryForward` / `m_byteDisplacementCarryForward`?

**Status**: **RESOLVED — DO NOT TOUCH THESE FIELDS.**

**Rationale**: Revision 2 adopts the helper-separation principle (§6.0). `UpdatePosition()` is **not** being rewritten. The carry fields belong to `UpdatePosition`'s WaveRT contract role and continue to serve:
- Non-cable streams (savedata / file capture paths)
- The WaveRT packet timing surface (`GetReadPacket` / `SetWritePacket`) which reads `m_ullLinearPosition` / `m_ullPresentationPosition` / `m_ullDmaTimeStamp`
- Block-alignment drift prevention on every path

The carry fields are explicitly listed in §4.0 Rule 10 as **preserved**. The old concern "grep the source before deleting" is moot — we're not deleting anything.

### OQ7. Does `m_ullLinearPosition` lag or lead the pump's own buffer-offset view, and by how much?

**Context**: Phase 5/6 pump computes `bufferOffset = m_ullLinearPosition % m_ulDmaBufferSize` to determine where in the DMA buffer to start reading/writing. `m_ullLinearPosition` is advanced by `UpdatePosition()` on the same call, using bytes-per-ms math. The pump uses frame-delta math. These two answer "where are we now?" using slightly different models.
**Risk**: Over long runs the two views may drift. At the extreme, the pump's buffer offset could land on stale data that the app has already overwritten, or on a region the app hasn't filled yet.
**Resolution**: Introduce `m_ulPumpLastBufferOffset` (see Phase 1 struct additions) and advance it independently inside the pump. This makes the pump fully self-contained for buffer-offset tracking and removes the hidden dependency on `m_ullLinearPosition`. Long-run drift is then measured in Phase 9 validation — if the two offsets diverge by more than N frames in 30 minutes, we have a problem to investigate.
**Blocking**: Not immediately, but the struct field must land in Phase 1 so that Phases 5/6 can use it.

### OQ8. Can `m_ulPumpLastBufferOffset` reuse an existing AO field, or must it stay a dedicated member?

**Context**: (New in rev 2.4, from Codex v3 open question list.) AO already has several position-related fields: `m_ullLinearPosition` (WaveRT contract), `m_ullWritePosition` (WaveRT read pointer), `m_ullPlayPosition`, `m_byteDisplacementCarryForward`. The pump currently introduces a new `m_ulPumpLastBufferOffset` as a dedicated member because none of the existing fields exactly capture "the pump's own view of the DMA buffer offset at end of last call".

**Options**:
- **A (current plan)**: Keep `m_ulPumpLastBufferOffset` as a dedicated member. Pro: clear ownership, no aliasing with WaveRT contract fields, easy to audit. Con: one more field.
- **B**: Reuse `m_ullWritePosition` on the capture side and `m_ullPlayPosition` on the render side. Pro: no new field. Con: these fields are part of the WaveRT contract surface; the pump writing to them might conflict with `UpdatePosition`'s own updates to the same fields.
- **C**: Reuse a single existing field by repurposing. Pro: minimal. Con: semantic confusion for future readers, violates Rule 14 (pump must maintain its own offset truth).

**Resolution**: Stay with Option A (dedicated member). Rule 14 explicitly says "the pump should maintain its own transport offset truth for that direction" — reusing WaveRT contract fields would violate this rule because those fields are owned by `UpdatePosition` as part of its WaveRT contract.

**Blocking**: No. Decided in rev 2.4: stick with dedicated `m_ulPumpLastBufferOffset`. Open question closed as "resolved by rule".

---

## 12. Success definition

### 12.1 1차 목표 (Primary, required)

- **User reports Phone Link call quality as "clean"** when running through AO Cable A, equivalent to VB Cable A baseline.
- **Existing automated tests pass**: Q02 silence, 16ch isolation, multi-client, device switching.
- **Build / install pipeline stable**: `build-verify.ps1` + `install.ps1 -Action upgrade` passes without reboot.
- **Diagnostics visible**: `test_stream_monitor.py` shows `GatedSkipCount` > 0 (gate working), `OverJumpCount` ≈ 0 (no glitches), `FramesProcessedTotal` linear with wall-clock.
- **No crashes** during 30 minute continuous call.

### 12.2 2차 목표 (Secondary, nice-to-have)

- **Measurable SRC quality improvement** over VB for 44.1↔48 and 48↔96 cases (AO Phase 5).
- **Shared timer model** matching VB architecture (AO Phase 6).
- **Updated `docs/BENCHMARK_SUMMARY.md`** with new AO-vs-VB numbers.
- **Position-drift measurement** under 1ms over 30 minute run (matches VB).
- **VBCABLE_SURPASS_PLAN.md M5** criteria met with evidence.

### 12.3 What this plan explicitly does NOT commit to

- Byte-for-byte clone of VB.
- Multi-client capture fan-out (OQ5).
- Per-channel volume (`+0x51a8`).
- Peak meter (`+0x4f2c`).
- EX_TIMER coalescing tolerance tuning.
- 192kHz / 8ch / 32bit float triple-stress scenarios beyond what the existing test suite covers.

---

## 13. Appendix A: VB struct offset map (cross-referenced)

See §3.10 and §3.11. Consolidated here for quick reference during coding.

**Ring struct** (base → `+0x190 + N*INT32` data area):
```
+0x00 SR(actual)   +0x14 WrapBound    +0x28 FrameStride
+0x04 SR(req)      +0x18 WritePos     +0x2C DoubleStride
+0x08 DataOffset   +0x1C ReadPos      +0x30 Allocated
+0x0C FrameCap     +0x20 InternalRate +0x34 SrcAccumFrac
+0x10 TotalSlots   +0x24 ValidFlag    +0x38 SrcHistory[16]
                                      +0xB8 SrcPrevAccum
                                      +0xBC SrcPrevSamp[16]
+0x174 FeatureFlags
+0x180 OverflowCount
+0x184 UnderrunCount(SRC)
+0x188 UnderrunFlag(same-rate)
+0x190 DataArea[frames*channels*4]
```

**Stream struct** (most important fields for the port):
```
+0x58 TimerHandle        +0xD0 BufferPosOut
+0x70 FramesPerTick      +0xE0 SampleAccum1
+0x8C SampleRate         +0xE8 SampleAccum2
+0x9C Render/Capture     +0xF0 ClearedOnStop
+0xA4 IsMic              +0x100 QpcFrequency
+0xA8 DmaWrapSize        +0x108 StartHns
+0xB0 DmaRegionBase      +0x160 PositionLock
+0xB4 PrevState          +0x168 RingPtr
+0xB8 ActiveFlag         +0x170 AuxRingPtr
+0xBC CurKsState         +0x178 ScratchPtr
+0xC8 SamplePosOut       +0x180 BaselineQpc
                         +0x198 ProcessedFrames
                         +0x1B0 NextEventQpc
                         +0x1B8 LastFrameCount
                         +0x1D0 LastDirFlag
```

---

## 14. Appendix B: IAT and kernel APIs used by VB

See §3.13 for the full table. Key APIs AO also needs to use correctly:

- `KeQueryPerformanceCounter` — AO already uses
- `KeAcquireSpinLockRaiseToDpc` / `KeReleaseSpinLock` — AO already uses
- **`KeFlushQueuedDpcs`** — AO already uses (line 1284 of `minwavertstream.cpp`)
- **`ExAllocateTimer` with `EX_TIMER_HIGH_RESOLUTION`** — Phase 6 (AO currently uses `ExSetTimer` on an already-allocated `PEX_TIMER` but per-stream)
- **`ExSetTimer` / `ExDeleteTimer`** — AO uses `ExSetTimer`; `ExDeleteTimer` usage in Phase 6
- `KeSetEvent` — AO uses for WaveRT notification; may refine in Phase 6

---

## 15. Appendix C: Glossary

- **DPC** — Deferred Procedure Call. Kernel-level callback that runs at DISPATCH_LEVEL on the CPU that scheduled it.
- **FRAME_PIPE** — AO's fixed-frame pipe structure, defined in `loopback.h`. The target transport model.
- **GCD** — Greatest Common Divisor. Used to find integer-ratio resampling factors.
- **ISR** — Interrupt Service Routine. Runs at DIRQL (hardware interrupt level).
- **KDNET** — Kernel Debugger over network. WinDbg's network-mode kernel debug protocol.
- **KSSTATE** — KS Pin state enum. STOP=0, ACQUIRE=1, PAUSE=2, RUN=3.
- **KSPROPERTY_AUDIO_POSITION** — The property Windows uses to query a stream's current position.
- **MDL** — Memory Descriptor List. Kernel structure for pinned/locked pages.
- **portcls** — Microsoft's PORTable audio Class driver. Provides most of the WaveRT framework AO uses.
- **QPC** — QueryPerformanceCounter. High-resolution time source.
- **WaveRT** — Windows Wave Real-Time. The direct-buffer-access audio driver model AO and VB both use.
- **1차 / 2차 목표** — Primary / secondary goal in Korean, used in several places to distinguish required vs nice-to-have.

---

## Changelog for this document

- 2026-04-13 — Initial creation (Claude). Based on:
  - Full read of `docs/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CODEX.md` (Codex plan)
  - Full read of `docs/VB_CABLE_DYNAMIC_ANALYSIS.md` (Codex dynamic notes with ~28 follow-ups)
  - Full read of `docs/VB_CABLE_AO_COMPARISON_CODEX_NOTES.md`
  - Full read of `docs/VB_CABLE_AO_COMPARISON.md` (side-by-side diff)
  - Full read of `docs/VB_CABLE_PATH_ANALYSIS.md`
  - Full read of `docs/VBCABLE_SURPASS_PLAN.md`
  - Full read of `results/vbcable_pipeline_analysis.md` (12-section deep dive)
  - Full read of `results/vbcable_disasm_analysis.md` (FUN_26a0 detail)
  - Full read of `results/vbcable_runtime_claude.md` (21 sections, live dynamic session)
  - Full read of `results/ghidra_decompile/vbcable_function_index.txt`
  - Targeted read of `results/ghidra_decompile/vbcable_all_functions.c` via index
  - Full read of `Source/Utilities/loopback.h`
  - Relevant range read of `Source/Utilities/loopback.cpp` (FRAME_PIPE section 1396~1960)
  - Full read of `Source/Main/minwavertstream.h`
  - Relevant range read of `Source/Main/minwavertstream.cpp` (164, 790-817, 1050-1720)
  - Full read of `CLAUDE.md`
  - Live-trace derived context from 2026-04-13 WinDbg session (stored in `results/vb_session.log`)

- 2026-04-13 (revision 2) — Cross-review pass after full read of Codex's plan. Structural changes:
  - **§0 revision note added** summarizing the 5 key changes
  - **§1.4 "AO wins to preserve"** new section listing 20+ items that MUST NOT break (transport model, position query shape, state machine partial correctness, AO-specific improvements like fade-in ramp, build/test/install infrastructure)
  - **§3.1 "Hybrid model" wording adopted** — softened claims that VB is "position-query driven" to "main observed paired path is lazily advanced by position polling, while shared timer subsystem services auxiliary activity"
  - **§4.0 "Non-negotiable rules" new section** — Codex's 6 rules (no overwrite-oldest, no MicSink bypass, position polling must be meaningful, 8-frame gate required, timer subordinate, state semantics toward VB) PLUS Claude's 4 additions (no UpdatePosition rewrite, no lock nesting, diagnostic struct sync, preserve WaveRT carry fields)
  - **§6.0 "Helper separation principle" new section** — explains why `PumpToCurrentPositionFromQuery()` is added as a sibling to `UpdatePosition()` instead of rewriting it, with a conceptual call structure showing both functions living side-by-side
  - **Phase restructure**: old Phase 3 → new Phase 4, old Phase 4 split into new Phase 3 (observer) + Phase 5 (pump→render) + Phase 6 (pump→capture), old Phase 5 → Phase 7, old Phase 6 → Phase 8, old Phase 7 → Phase 9
  - **Phase 3 "Pump helper in observer mode"** new content — introduces the helper in observer-only mode (computes frame delta, runs gate, runs guard, increments counters, NO transport side effects). Full C++ implementation sketch included.
  - **Phase 4 "State machine alignment"** — content preserved from old Phase 3
  - **Phase 5 "Pump drives render transport"** new content — adds the transport call to the Phase 3 observer helper for cable speaker endpoints, guards `UpdatePosition`'s `ReadBytes` with `!isCable`
  - **Phase 6 "Pump drives capture transport"** new content — symmetric for cable mic endpoints, includes `ApplyFadeInRamp` helper extraction to preserve AO's fade-in win
  - §5 Gap list phase column updated for new numbering
  - §7 Verification matrix rewritten for 10-phase (0~9) sequence with 1차 목표 milestone marked at Phase 6
  - §8 Failure modes updated: new 8.2 (Phase 3 observer crashes), 8.3 (Phase 4 STOP leak), 8.4 (Phase 5/6 double-transport)
  - Table of Contents updated to list all new subsections

  Motivation for revision 2: Cross-review against Codex plan revealed three critical concerns in revision 1:
  1. Rewriting `UpdatePosition()` was risking WaveRT packet interface regression (GetReadPacket/SetWritePacket depend on its output fields)
  2. State machine changes and pump changes bundled into one phase made rollback analysis harder
  3. The "position-query driven" claim was overstated — the timer subsystem is still real, just secondary

  Revision 2 addresses all three: helper separation (concern 1), phase split render+capture into separate phases (concern 2), hybrid model wording (concern 3). The revision does NOT change the §3 ground truth (that's still the evidence) — only the §6 phase plan and the preamble/rules sections.

- 2026-04-13 (revision 2.1) — Internal consistency pass after the second cross-review (user-directed: "check your own document and update"). Fixes:
  - §4.5 adapter.cpp note: Phase 6 → Phase 1 (diagnostic counters land in Phase 1, not Phase 6)
  - §5 Gap list Phase column: 9 entries updated to revision 2 numbering (G1/G2 → Phase 4; G3/G4/G5 → Phases 3+5+6; G6/G13/G15/G16 → Phase 8; G7/G8 → Phase 7; G12 → Phase 5+6)
  - §5 closing sentence rewritten to reflect "required = Phases 3~6; optional quality/arch = Phases 7~8; validation = Phase 9"
  - §8 Failure modes: 8.4 duplicate resolved → 8.4 (pump transport distorted), 8.5 (KeFlushQueuedDpcs hang), 8.6 (8-frame gate startup delay), 8.7 (pre-commit hook)
  - §9 Risk register:
    - R2 rewritten from "Phase 4 breaks WaveRT packet timing" (stale) to "Phase 5/6 pump parallel execution with UpdatePosition double-processes transport"
    - R3/R4/R5/R7 phase numbers corrected
    - **R8 replaced** with "Phase 1 struct field additions break C_ASSERT size guards" (the old R8 about `m_ulDmaMovementRate` is moot now that revision 2 preserves all carry fields)
    - **R9 added** — "Pump and UpdatePosition position drift over long runs", mitigated by new `m_ulPumpLastBufferOffset` field
  - §10.5 WinDbg playbook: BP addresses updated from `UpdatePosition+XX` to `PumpToCurrentPositionFromQuery+XX`; added verification BPs for Phase 5/6 transport
  - §11 Open Questions:
    - OQ2 phase ref: Phase 4 → Phase 5
    - OQ3 phase ref: Phase 4 → Phase 3 (this is the entry point that Phase 3 observer hooks into; verification critical for Phase 3)
    - OQ4 phase ref: Phase 3 → Phase 4
    - **OQ6 marked resolved** — revision 2 preserves carry fields per §4.0 Rule 10; the question no longer applies
    - **OQ7 added** — pump vs WaveRT position drift; resolved via new `m_ulPumpLastBufferOffset` field
  - **New Phase 1 struct field: `m_ulPumpLastBufferOffset`** — tracks the pump's own DMA buffer offset independent of `m_ullLinearPosition`. Phase 5 render-transport code and Phase 6 capture-transport code both use this field. On first call after RUN, the field is lazy-initialized from `m_ullLinearPosition` to pick up where UpdatePosition left off, then advances independently. This is the Codex plan's "m_QueryPumpLastBufferPosition" suggestion adapted to AO naming conventions.
  - §4.0 Rule 3 annotated: "Satisfied at Phase 5/6, not Phase 3" (Phase 3 observer mode intentionally does not drive transport; Rule 3 becomes behaviorally true only when transport moves to the pump)
  - §3.1 Key conclusion: Codex §Bottom line quoted directly for alignment; the four Codex bullets mapped to specific Claude plan phases

  Revision 2.1 is purely internal consistency. No new design decisions, no new gaps, no new rules — only making sure every cross-reference in the document points to the revision-2 phase numbers and that the new design decisions from revision 2 are consistently reflected throughout.

- 2026-04-13 (revision 2.2) — Third cross-review after Codex released their v2 plan (24 KB → 32 KB). New content absorbed from Codex v2:

  **Phase 3 upgraded to shadow-and-compare mode**:
  - Previously: "observer mode" — pump computes frame delta, increments counters, does nothing transport-side.
  - Now: additionally reads `m_ulLastUpdatePositionByteDisplacement` (new handoff field set by `UpdatePosition`) and compares pump's `newFrames` against `legacy_bytes / blockAlign`. Increments `m_ulPumpShadowDivergenceCount` if the absolute difference exceeds `max(2, newFrames/16)`.
  - Phase 3 exit criterion was first tightened from "counters are non-zero" to a quantified divergence threshold in revision 2.2; later revision 2.3 tightened it again to the current **windowed `ShadowDivergenceCount == 0`** rule before Phase 5/6 ownership transfer.
  - Call order in `GetPosition`/`GetPositions` changed: pump is now called AFTER `UpdatePosition` so the stash field is populated in time.

  **Feature flag system added**:
  - New fields `m_ulPumpFeatureFlags`, `m_ulPumpInvocationCount`, `m_ulPumpShadowDivergenceCount` in Phase 1 struct additions.
  - Flag constants: `AO_PUMP_FLAG_ENABLE` (master), `AO_PUMP_FLAG_SHADOW_ONLY` (compare-only), `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER`, `AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE`.
  - Phase 3 SetState RUN sets `ENABLE|SHADOW_ONLY`.
  - Phase 5 SetState RUN adds `DISABLE_LEGACY_RENDER`.
  - Phase 6 SetState RUN adds `DISABLE_LEGACY_CAPTURE`.
  - `UpdatePosition`'s cable-skip guards are now flag-gated, not hard-coded: `if (!pumpOwnsRender) ReadBytes(...)`.
  - **Rollback is now runtime**: IOCTL clears the flags → next `UpdatePosition()` resumes legacy transport within one call. No rebuild/reinstall/reboot.

  **Two new §4.0 rules (Rules 11, 12)**:
  - Rule 11: "Rollout must preserve a rollback path until parity is proven" (mirrors Codex Rule 7).
  - Rule 12: "Behavior beats offsets when the two disagree" (mirrors Codex Rule 8) — §3.11 stream layout now tagged with `[direct]` / `[norm]` / `[prov]` confidence classes.

  **Phase 9 validation expanded**:
  - Phase 9 duration: 1.5 days → 2.5 days.
  - New §9.1 Polling-regime stress matrix — 5 scenarios (aggressive/normal/sparse/near-no/mixed polling) with explicit pass criteria. Tests pump robustness across client cadences, not just Phone Link.
  - New §9.2 Position drift measurement protocol — quantitative criterion: `|m_ulPumpLastBufferOffset − (m_ullLinearPosition % m_ulDmaBufferSize)| ≤ 1ms` of data at the stream's sample rate and block alignment, measured every 60s for 30min. Specifies diagnosis path if drift grows monotonically (suspect QPC conversion, carry leakage, etc.) and a "drift nudge" fallback if models genuinely can't co-track.

  **§3.11 Stream object layout evidence tagging**:
  - Added three tag classes: `[direct]` (observed from function receiving stream via `rcx` directly), `[norm]` (observed from `+0x6320` with `rcx = stream - 8`, normalized via `rdi = rcx + 8`), `[prov]` (cited from summary, not directly re-verified in decompile).
  - Only `[direct]` and `[norm]` offsets may inform AO field design decisions (per Rule 12).

  Motivation for revision 2.2: Codex's v2 plan added shadow mode, feature flags, polling-regime validation, and two new rules that directly address risks Claude's revision 2.1 didn't cover. Specifically:
  1. Revision 2.1 had no way to detect frame-math bugs before Phase 5/6 ownership transfer. Phase 3 was observation-only, which could pass even with a broken QPC conversion.
  2. Revision 2.1's rollback was `git restore` — requires rebuild + reinstall. Codex's feature flag system gives us IOCTL-based runtime rollback.
  3. Revision 2.1 didn't consider non-Phone-Link client cadences. The 8-frame gate and the pump's behavior are polling-rate dependent, and a plan that only validates against one client is insufficient.
  4. Revision 2.1 had no quantitative threshold for position drift. "Measure drift in Phase 9" was hand-waving.

  Revision 2.2 addresses all four concerns. Every change is additive: no revision 2.1 content is removed, only extended or gated behind flags.

- 2026-04-13 (revision 2.3) — Fourth review pass, self-audit triggered by user direction "check your own document and prepare to update". Found 16 internal inconsistencies introduced during rev 2.2:

  **Stale references (8 items)** — Phase 3 changelog still said "observer mode" after upgrade to "shadow-and-compare mode"; Phase 4 "Expected live behavior" / "Exit criteria" referenced Phase 3 quality improvement (rev-1 meaning) when rev-2.2 Phase 3 is observer-only; Phase 5 changelog "Why" cited "observer-mode pump" instead of shadow-and-compare; Phase 4 Pre-conditions missing Phase 3.

  **Code inconsistency (2 items)** — Phase 6 `WriteBytes()` body had TWO versions shown: a hard-coded `if (isCable) return;` (rev-2.1 leftover) AND the flag-gated `if (pumpOwnsCapture) return;` (rev-2.2 target). Same problem in Phase 6 `UpdatePosition` capture guard. Consolidated to flag-gated version with "dual-gate" comment explaining why both caller and callee check.

  **Phase 1 incompleteness (5 items)** — Rev 2.2 Phase 3 code referenced `pFP->ShadowDivergenceCount` and `pFP->InvocationCount` (pipe-level mirrors of stream-level counters), but Phase 1 never declared these fields in `FRAME_PIPE`, never added them to `ioctl.h`, never populated them in `adapter.cpp`, never initialized them in `FramePipeInit`, and Phase 1 changelog entry omitted them. Without these fixes, Phase 3 code would not compile. Also extended changelog entry to list all stream-level pump fields and flag constants (were truncated in rev 2.1/2.2).

  **Design flaw (1 item)** — Phase 3 shadow-compare was per-call, which false-positives on legitimate rounding carry in `UpdatePosition`'s byte-per-ms math (carry absorbs sub-millisecond remainder, so some calls produce `ByteDisplacement = 0` while pump sees 10+ frames). Replaced with **windowed cumulative compare** (`AO_PUMP_SHADOW_WINDOW_CALLS` default 128, `max(16, 2% of larger)` tolerance). New fields: `m_ullPumpShadowWindowPumpFrames`, `m_ullPumpShadowWindowLegacyBytes`, `m_ulPumpShadowWindowCallCount`. Phase 3 exit criterion tightened to `ShadowDivergenceCount == 0` (any windowed tick means systematic drift, not rounding noise).

  **Verification gaps (2 items)** — Phase 4/5/6 verification added "Phase 3 pump counter regression check" (divergence ratio must not regress as state-machine / transport-ownership changes land) and "runtime rollback smoke test" (IOCTL flag toggle should be seamless during live call).

  **New structure (1 item)** — Added §7.5 "Rollback checkpoints" table (A-J mapping Phases 0-9 to expected state, first-to-suspect, and rollback mechanism). Consolidates Codex's Checkpoints A-E pattern into a single lookup table that complements §8's per-scenario failure narratives.

  Revision 2.3 is a consistency audit triggered by the realization that rev 2.2's cross-review updates left several stale rev-2.1 references behind. No new design decisions beyond the windowed-compare redesign (which was required because the per-call approach was unimplementable). No new rules, no new phases — the plan structure is stable at 10 phases (0-9) with flag-gated transport ownership.

- 2026-04-13 (revision 2.4) — Fifth review pass: Codex v3 was released with major additions (Transport Ownership Matrix, Rules 11/13, Phase 0 pre-flight elevation, Phase 4 tasks). Claude absorbed the valuable new content and did an additional self-audit that caught a serious race bug in rev 2.3.

  **Codex v3 absorption**:
  - **New §6.5 "Transport ownership matrix"** with full per-phase table showing cable render / cable capture ownership through all 10 phases. Includes audit procedure and double-transport symptoms. This is a significant new safety mechanism: Rule 13 can now be verified in minutes by scanning the table and counting call sites.
  - **Rule 13 (new)**: "Each cable direction must have exactly one transport owner per phase." Maps to Codex v3 Rule 11 and the Transport Ownership Matrix.
  - **Rule 14 (new)**: "Pump transport must not depend forever on WaveRT presentation offset." Formalizes the `m_ulPumpLastBufferOffset` design as a rule. Maps to Codex v3 Rule 13.
  - **Phase 0 step 4 (new)**: Blocking WinDbg check that `CMiniportWaveRTStream::GetPosition` / `GetPositions` actually fires during live Phone Link call. Elevated from OQ3. Codex v3 made this a Phase 0 deliverable; Claude adopted with full WinDbg command sequence and pass/fail criteria. Phase 0 duration bumped from 0.5d to 1d.
  - **Phase 4 blocking task (new)**: Trace `SpeakerActive` / `MicActive` clearing path before moving `FramePipeUnregisterFormat` out of STOP. Elevated from OQ4. Idempotency requirement added.
  - **Phase 4a sub-task (new)**: Extract `ApplyFadeInRamp` helper from `WriteBytes` as a pure refactor, BEFORE Phase 6 uses it. Codex v3 implementation order step 8 recommends this separation. Phase 6 is now a pure "add new call site" change, not "extract + add call site".
  - **Phase 1 renamed**: "Struct / diagnostic field additions" → "Diagnostics and rollout scaffolding". Codex v3 framing emphasizes that Phase 1's real purpose is to set up the rollback safety harness (feature flags, divergence counters, per-direction diagnostics) before any transport-behavior change.
  - **OQ8 added and resolved**: "Can `m_ulPumpLastBufferOffset` reuse an existing AO field?" — Codex v3 raised; Claude resolved immediately as "no, Rule 14 requires dedicated member".

  **Claude self-audit (critical race bug)**:
  - **Phase 3 pipe counter race**: rev 2.3's code did `pFP->GatedSkipCount = m_ulPumpGatedSkipCount` (copy assignment) on a pipe shared by Speaker and Mic streams. Both streams' pump helpers wrote to the same slot, racing and overwriting each other. The `ShadowDivergenceCount` / `InvocationCount` / `GatedSkipCount` / `OverJumpCount` would show oscillating values in IOCTL output, making the Phase 3 exit criterion (`ShadowDivergenceCount == 0`) unmeasurable. **Fix**: split `FRAME_PIPE` counters into per-direction pairs: `RenderXxxCount` + `CaptureXxxCount`. The pump now writes to one side only based on `m_bCapture`. No race. Consistent updates across `loopback.h` (FRAME_PIPE), `ioctl.h` (struct), `adapter.cpp` (population), `FramePipeInit` / `FramePipeReset`, and Phase 3 pump code block.
  - **Phase 1 file-by-file review**: Confirmed all five touched files (`minwavertstream.h`, `loopback.h`, `ioctl.h`, `adapter.cpp`, `loopback.cpp`) + changelog entry consistently reflect the per-direction split.

  Motivation for revision 2.4: Codex v3 added strategic-level structure (the Transport Ownership Matrix is the single best reviewer tool in the whole plan — it makes violations of Rule 13 trivially auditable) and operational-level rigor (Phase 0 WinDbg check, Phase 4 SpeakerActive analysis). Absorbing these made Claude's plan noticeably safer. The self-audit caught a bug that would have manifested as "IOCTL diagnostic counters look weird" in Phase 1 testing — easy to mis-diagnose as "test_stream_monitor.py is broken" rather than "counters race". Fixing it now, in the plan, prevents a real bring-up session from hitting it.

  Revision 2.4 is the most substantial rev since 2.2. Expected to be the last major revision before actual Phase 0 execution begins.
