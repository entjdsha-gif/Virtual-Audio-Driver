# Pipeline V2 Changelog

## 2026-04-15 — Phase 5c: revert pump-driven render transport to UpdatePosition cadence

**Files changed:**
- `Source/Main/minwavertstream.cpp` — removed Phase 5 sub-3 pump transport block (was inside GetPositions-triggered `PumpToCurrentPositionFromQuery` helper). Removed `pumpOwnsRender` dual-gate in `UpdatePosition` and restored unconditional legacy `ReadBytes(ByteDisplacement)` for cable speaker render. Phase 5 scaffolding (flags, counters, IOCTL, register map) kept intact; only the cadence of actual DMA→ring transport reverts to WaveRT packet cadence.
- `Source/Main/aocablea.inx`, `Source/Main/aocableb.inx` — `DriverVer` bumped to `04/15/2026, 5.3.0.1` so pnputil treats build as new package and refreshes DriverStore (otherwise staged signed .sys would be silently replaced by old cached copy on re-install).
- `CLAUDE.md` — new "Experiment Commit Rule" section: commit experiments incrementally, tag result filenames with phase+shorthash, mirror results in memory.

**What:** Phase 5 original (commit `2c733f1`) moved cable render transport (`FramePipeWriteFromDma`) from UpdatePosition packet cadence (~10 ms) into a query-driven helper called from GetPositions. Observed GetPositions fire rate in practice was ~130 ms — 13× slower than WASAPI reader period — producing a burst+gap output pattern at the pipe. Phase 5c keeps all Phase 5 infrastructure (runtime rollback IOCTL, diag counters, ownership bookkeeping) but reverts the actual transport call-site back to UpdatePosition packet cadence.

**Why:** The live-call regression user reported for Phase 5 matched a burst+gap pattern: Phone Link reads Cable B via WASAPI shared at a much shorter period than our transport refresh and sees silence packets during the gap. Reverting the transport site is the smallest-surface fix while retaining the Phase 5 scaffold for future experiments.

**Test result (Phase 5c run1, 2026-04-15 18:42):**
- Driver: signed .sys `81e020f0...` (= signtool output from build `cb9160...`)
- 4-point capture saved as `results/bugb_runtime/livecall/phase5c_wip_run1_{A,B}_{spk,mic}.wav`
- Cable fidelity (recording): clean on all four points
- Phone-side subjective: **"녹음파일은 깨끗한데 통화음질은 깨끗하지 않았음"** — phone chopping still present
- Conclusion: Phase 5c does NOT fix pre-existing phone-side chopping. Same pattern as Phase 1 (06751aa) and Phase 4 (439bbcd). Phase 5 series (render ownership cadence) is not the right surface for this bug. Next probe direction: Cable B mic capture side / Phone Link read cadence vs our write burst pattern mismatch.

---

## 2026-04-14 — G4 B1: Render prefill diagnostic gate (UNCOMMITTED, working tree only)

**Files changed:**
- `Source/Utilities/loopback.h` — new `FramePipePrefillIfEmpty()` declaration in a dedicated "G4 B1" comment block, physically separated from Phase 1/3/5 sections so independent `git restore` works against the G2 instrumentation block.
- `Source/Utilities/loopback.cpp` — new `FramePipePrefillIfEmpty()` helper placed right after `FramePipeUnregisterFormat()`. Guards on `FillFrames == 0` under `PipeLock` (empty-ring only; Speaker STOP/RUN ring persistence is respected — no stacking on existing data). Prefill size = `max(TargetFillFrames/2, PipeSampleRate/8)`, clamped to `CapacityFrames - 1`. Content is zero (silence cushion). Wrap-safe ring write mirrors `FramePipeWriteFrames` layout. One DbgPrint on successful prefill. PASSIVE_LEVEL entry, locks internally.
- `Source/Main/minwavertstream.cpp` — `SetState(KSSTATE_RUN)` cable branch: call `FramePipePrefillIfEmpty(pFP)` immediately after `FramePipeRegisterFormat()` for the speaker direction only (`fpIsSpeaker == TRUE`). Capture direction untouched.
- `results/g4_b1_prefill_proposal.md` — proposal locked before implementation; §§1-10 filled, §11 measurement results pending.

**What:** G4 B1 adds a one-shot zero-fill silence cushion of ~125 ms (at 48 kHz default) on each cable speaker `SetState(KSSTATE_RUN)` transition where the pipe ring is empty. Positioned as a **diagnostic gate**, not a fix: its purpose is to resolve whether the near-zero trough / occasional `dUnderrun` pattern from G2 (Cable B trough 17 frames, dUnderrun 2 and 9) is recoverable with a standing cushion, or whether it requires a structural change to `FP_DEFAULT_TARGET_FILL` (B2).

**Why:** G3 §12 established pump ownership (`Flags=0x00000007`, `LegacyDrv=0`) and order-of-magnitude pump fire rate (~7-8 Hz → ~130 ms inter-fire interval). Against a 1 ms reader drain, that gives ~6240 frames drained per pump cycle, which is larger than the default `TargetFillFrames = 3584`. A `TargetFillFrames/2 = 1792` frame prefill would be too small to cover even one cycle and would produce an ambiguous B1 result. Raising the floor to `SampleRate/8 = 6000` frames (~125 ms) sizes the cushion to approximately one pump inter-fire interval, making B1's result a crisp yes/no on B2 necessity.

**Empty-ring guard rationale:** `FramePipeRegisterFormat` explicitly does not reset the ring on Speaker re-register (persistent ring across STOP/RUN gaps per VB-Cable behavior). Unconditional prefill would stack on top of persisted data. `StartPhaseComplete` is initialized to `TRUE` in `FramePipeInit` and therefore cannot be used as a first-RUN flag. `FillFrames == 0` under `PipeLock` is the correct guard.

**Commit posture:** UNCOMMITTED. B1 stays in the working tree alongside the existing G2 1s-window instrumentation until the B1 measurement run (§ 11 of the proposal) resolves the go/no-go for B2. Phase 6 remains BLOCKED.

**Physical separation from G2 instrumentation:** B1 code lives in its own header block (new comment section after `FramePipeUnregisterFormat`) and its own function block in `loopback.cpp` (new `#pragma code_seg("PAGE")` stanza after `FramePipeUnregisterFormat`). It does not touch `DbgQpcTicksPerSecond`, `DbgFillAtLastPrint`, `DbgDropAtLastPrint`, or `DbgUnderrunAtLastPrint`, so G2 can be `git restore`-d independently.

## 2026-04-14 — Phase 5: Pump owns cable render transport (IOCTL-gated runtime rollback)

**Files changed:**
- `Source/Utilities/loopback.h` — 2 new `FRAME_PIPE` fields (`RenderPumpDriveCount`, `RenderLegacyDriveCount`); new C-style entry `AoPumpApplyRenderFlagMask()` declared here.
- `Source/Utilities/loopback.cpp` — `FramePipeInit()` zeros the new fields. `FramePipeReset()` intentionally does NOT touch them (monotonic evidence counters, like the Phase 1 pump counters).
- `Source/Main/ioctl.h` — new `IOCTL_AO_SET_PUMP_FEATURE_FLAGS` code, new `AO_PUMP_FLAGS_REQ` request struct, `AO_V2_DIAG` extended with 4 tail ULONGs (`A_R_PumpDriveCount`, `A_R_LegacyDriveCount`, `B_R_PumpDriveCount`, `B_R_LegacyDriveCount`), `C_ASSERT` bumped from 116 to 132.
- `Source/Main/adapter.cpp` — mirror `C_ASSERT` bumped to 132; V2 diag fill populates the 4 new tail fields for whichever cable(s) the build targets; new `IOCTL_AO_SET_PUMP_FEATURE_FLAGS` dispatch that unpacks the `AO_PUMP_FLAGS_REQ` and calls `AoPumpApplyRenderFlagMask()`.
- `Source/Main/minwavertstream.h` — new public `ApplyPumpFlagMaskUnderLock()` method.
- `Source/Main/minwavertstream.cpp` —
  - pump helper body `PumpToCurrentPositionFromQuery()`: cable pipe resolution hoisted out of the Mirror block to just after the cable-only gate so both the new transport block and the Mirror block share one `pFP`; new render transport block between step 9 (accepted-frame accounting) and step 10 (rolling-window shadow compare), gated on `!m_bCapture && cable speaker && (flags & AO_PUMP_FLAG_DISABLE_LEGACY_RENDER) && pFP->Initialized && m_pDmaBuffer && m_ulDmaBufferSize > 0 && newFrames > 0`; first-call sync from `m_ullLinearPosition % m_ulDmaBufferSize` so the first pump chunk lands where the legacy path would have written; wrap-safe DMA chunks into `FramePipeWriteFromDma()`; `m_ulPumpLastBufferOffset` advances independently of `m_ullLinearPosition`; `RenderPumpDriveCount` incremented once per helper invocation that executes the block (not per chunk);
  - `UpdatePosition()` render branch: existing cable `ReadBytes(ByteDisplacement)` call is now gated `!pumpOwnsRender` where `pumpOwnsRender = isCable && (m_ulPumpFeatureFlags & AO_PUMP_FLAG_DISABLE_LEGACY_RENDER)`. Non-cable render (savedata fallback) is unchanged;
  - `ReadBytes()` cable branch: `RenderLegacyDriveCount` incremented once per invocation that enters the cable-pipe branch (not per DMA chunk);
  - `SetState(KSSTATE_RUN)` cable speaker arm: new `runFlags` builder adds `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` for cable speaker; after arming, registers this stream via `AoPumpRegisterActiveRenderStream()` so the IOCTL rollback knob can find it;
  - `SetState(KSSTATE_PAUSE)` from RUN (cable speaker): `AoPumpUnregisterActiveRenderStream()` called after the Phase 4 pump working-state clear;
  - `SetState(KSSTATE_STOP)` (cable speaker): `AoPumpUnregisterActiveRenderStream()` called after the Phase 3 STOP clear, outside `m_PositionSpinLock`;
  - destructor: unconditional `AoPumpUnregisterActiveRenderStream()` before the Phase 4 cable unregister, so a torn-down stream that skipped clean SetState transitions still drops its registration;
  - new static file-scope globals `g_CableAActiveRenderStream` / `g_CableBActiveRenderStream` with per-cable `KSPIN_LOCK`s, plus static register/unregister helpers, plus the `extern "C" AoPumpApplyRenderFlagMask()` implementation that resolves the target stream under the per-cable lock and calls `ApplyPumpFlagMaskUnderLock()` on it with mask-constrained bits.
- `test_stream_monitor.py` — parser accepts both Phase 1 (116) and Phase 5 (132) V2 diag shapes; Phase 5 render rows display `PumpDrv` and `LegacyDrv` counters when present.
- `tests/phase3_shadow_active.py`, `tests/phase3_live_call_shadow.py` — buffer size bumped to 132 and `StructSize` check accepts either shape. A_Render block offsets are unchanged so the rest of these scripts work without edits.
- `tests/phase5_rollback.py` (new) — drives the Phase 5 rollback smoke test: starts a continuous WASAPI exclusive sine into AO Cable A speaker, toggles `IOCTL_AO_SET_PUMP_FEATURE_FLAGS` to clear then re-set `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER`, and asserts the four-quadrant counter signature (`RenderPumpDriveCount` grows / `RenderLegacyDriveCount` frozen when pump-owned; opposite when legacy-owned).
- `results/phase5_edit_proposal.md` — locked approval record in checkpoint `a1d8d3a`.

**What:** Cable render transport moves from the legacy `UpdatePosition() -> ReadBytes() -> FramePipeWriteFromDma()` chain to the Phase 3 pump helper. Both call sites now check `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` on a single flag bit, with logically complementary gates — the pump transport block runs iff the bit is set, and `ReadBytes()` runs for cable render iff the bit is clear. The two paths are therefore mutually exclusive by construction at the source level.

Runtime rollback: a new write-only IOCTL (`IOCTL_AO_SET_PUMP_FEATURE_FLAGS`) takes an `AO_PUMP_FLAGS_REQ { SetMask, ClearMask }` struct and atomically updates the active cable render stream's `m_ulPumpFeatureFlags` under its `m_PositionSpinLock`. The IOCTL is mask-constrained to `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` in Phase 5; other bits are silently dropped. Effect is visible at the very next `PumpToCurrentPositionFromQuery()` invocation (1-2 position queries), matching Codex exit criterion #5's literal requirement. The IOCTL is not persistent — each RUN transition re-arms the default (flag set).

One-owner evidence: two new `FRAME_PIPE` fields per cable (`RenderPumpDriveCount`, `RenderLegacyDriveCount`) are incremented at the two respective call sites, each at most once per invocation. The V2 diag struct exposes them at the tail (`V2_DIAG_SIZE` 116 -> 132). The rollback smoke test asserts the four-quadrant signature: pump-owned state grows `RenderPumpDriveCount` and freezes `RenderLegacyDriveCount`; legacy-owned state does the opposite.

**Why:** Codex Phase 5 "move render-side progression behind the query-driven helper". Phase 3 established the shadow compare math, Phase 4 aligned STOP/PAUSE state semantics, and this phase actually moves render transport ownership. Capture side stays on the legacy path until Phase 6. The IOCTL is scope-expanded rollout/rollback infrastructure, not a product feature; without it Codex exit criterion #5 can't be proved.

**Non-negotiable guardrails (all held):**
- Capture ownership is NOT touched. `AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE` stays clear. `UpdatePosition()`'s `WriteBytes()` call site is unchanged. The pump helper gains no capture transport block in this phase.
- `UpdatePosition()` still owns `m_ullLinearPosition`, `m_ullWritePosition`, `m_ullPresentationPosition`, `m_ullDmaTimeStamp`, and the three carry-forward fields. The pump transport block intentionally does NOT touch any of these.
- Pump helper uses its own buffer offset (`m_ulPumpLastBufferOffset`), synced to WaveRT view on first accepted call after RUN, then advances independently.
- Overflow is reject, not overwrite. `FramePipeWriteFromDma()` return value is ignored; pipe's `DropCount` is the drop telemetry.
- Phase 3 SHADOW_ONLY stays set alongside the new `DISABLE_LEGACY_RENDER` bit. The rolling-window shadow compare still runs; semantically it now compares the pump's frame math against `UpdatePosition()`'s byte math (drift detector) rather than against a live legacy transport owner.
- Phase 4 STOP/PAUSE state semantics are untouched.
- Phase 4 destructor teardown order is untouched; the new pump unregister is inserted before the Phase 4 cable unregister.
- No fade-in refactor.

**Phase 3 shadow semantics after Phase 5:** `PumpShadowDivergenceCount` is no longer live parity proof against an active legacy transport owner. It is drift-detector proof that the pump's frame-delta math still agrees with `UpdatePosition()`'s byte-delta math. Any new divergence tick during Phase 5 validation is therefore evidence of a pump/UpdatePosition accounting drift, not a legacy-vs-pump race, and blocks commit regardless.

**Verification:**
- `build-verify.ps1 -Config Release` — green, 17/17 PASS.
- `install.ps1 -Action upgrade` — expected `INSTALL_EXIT=0`, manifest match.
- `test_ioctl_diag.py` — V1 IOCTL path intact, `ALL PASSED`.
- `test_stream_monitor.py --once` — idle counters sane, `StructSize == 132`, drive counters visible.
- `tests/phase3_shadow_active.py` — 3 regimes × 20s, `Inv > 128`, `Div == 0`, `OverJump == 0`, `Flags == 0x00000007` (ENABLE|SHADOW_ONLY|DISABLE_LEGACY_RENDER).
- `tests/phase4_churn.py` — 20 STOP/RUN cycles × 1s, `Div == 0`, symbolic link stable.
- `tests/phase3_live_call_shadow.py` — real Phone Link call, `A_Render peak Inv > 128`, `Div == 0`, flags reach `0x00000007`.
- `tests/phase5_rollback.py` — four-quadrant one-owner assertion via IOCTL rollback.

**Exit criteria (Codex Phase 5):**
1. Render-side data progression works while query path is active.
2. No byte-ring semantics reintroduced.
3. No direct old-path overwrite behavior remains.
4. One-owner confirmation that render transport fires from exactly one owner on the query path (via direct per-side counter evidence in `tests/phase5_rollback.py`).
5. Runtime rollback smoke test passes: clearing the render-ownership flag returns render transport to the legacy path within one or two position queries (via `IOCTL_AO_SET_PUMP_FEATURE_FLAGS`, validated in `tests/phase5_rollback.py`).

**Rollback:**
- **Runtime (preferred, no rebuild):** issue `IOCTL_AO_SET_PUMP_FEATURE_FLAGS` with `ClearMask = AO_PUMP_FLAG_DISABLE_LEGACY_RENDER`. Render transport returns to legacy `ReadBytes` path on the next `GetPositions()` call. No reboot, no reinstall, no stream restart. The same IOCTL issued with `SetMask = AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` re-enables pump ownership equally fast.
- **Full source rollback:** `git revert <phase5-commit>`, rebuild, reinstall. Phase 3 and Phase 4 remain intact because Phase 5 is a strictly additive patch.

---

## 2026-04-13 — Phase 4: Align STOP/PAUSE state semantics with VB before ownership move

**Files changed:**
- `Source/Main/minwavertstream.cpp` — destructor reorder + teardown-time cable unregister; `SetState(KSSTATE_STOP)` no longer calls `FramePipeUnregisterFormat()`; `SetState(KSSTATE_PAUSE)` from RUN now does a conservative VB-style conditional `FramePipeReset()` after `KeFlushQueuedDpcs()` and also clears per-run Phase 3 pump working state while preserving monotonic Phase 3 evidence counters.
- `results/phase4_edit_proposal.md` — approval record locked in checkpoint commit `0f531b9`.

**What:** Phase 4 is a pure state-semantics realignment before any transport ownership move. Three structural changes:

1. **STOP no longer unregisters the cable pipe.** The `FramePipeUnregisterFormat()` block is removed from `SetState(KSSTATE_STOP)`. STOP still owns stream-local DMA/position reset, end-of-stream flags, and the Phase 3 STOP-clear block for stream-local pump state. It no longer touches `SpeakerActive` / `MicActive` / `ActiveRenderCount` or the pipe ring.

2. **PAUSE from RUN now owns conditional pipe reset + pump working-state clear.** Inside the existing `m_KsState > KSSTATE_PAUSE` + `m_ulNotificationIntervalMs > 0` branch, after the existing `ExCancelTimer()` + `KeFlushQueuedDpcs()` + DPC-time carryforward bookkeeping, the PAUSE path now:
   - resolves the cable pipe from `m_pMiniport->m_DeviceType`,
   - reads `pFP->MicActive` (for render streams) or `pFP->SpeakerActive` (for capture streams) as the conservative `!otherSideActive` gate,
   - calls `FramePipeReset(pFP)` only when the opposite direction is not currently active,
   - under `m_PositionSpinLock`, clears per-run pump working state (`m_ulPumpFeatureFlags`, `m_bPumpInitialized`, `m_ullPumpBaselineHns`, `m_ulPumpProcessedFrames`, `m_ulPumpLastBufferOffset`, `m_ullPumpShadowWindowPumpFrames`, `m_ullPumpShadowWindowLegacyBytes`, `m_ulPumpShadowWindowCallCount`, `m_ulLastUpdatePositionByteDisplacement`),
   - preserves monotonic Phase 3 evidence (`m_ulPumpInvocationCount`, `m_ulPumpShadowDivergenceCount`, `m_ullPumpFramesProcessed`) plus per-session `GatedSkipCount` / `OverJumpCount` so churn-era diagnostics survive into the next RUN.

3. **Destructor owns cable unregister at the true stream-lifetime edge, and the lifetime teardown order is rewritten.** Previous order released `m_pMiniport` first, then freed pool resources, then deleted the notification timer, then flushed queued DPCs. New order:
   1. `StreamClosed()` (does not touch cable pipe state),
   2. `ExDeleteTimer()` (waits for any in-flight timer callback),
   3. `KeFlushQueuedDpcs()` (guarantees no DPC/timer path can still be advancing the pipe via `UpdatePosition()` and the Phase 3 pump),
   4. resolve cable pipe via still-alive `m_pMiniport->m_DeviceType` and call `FramePipeUnregisterFormat(pFP, isSpeaker)`,
   5. `m_pMiniport->Release()`,
   6. free per-stream pool allocations (DPC, timer, muted, volume, peak, WfExt).

**Why:** `VB-Cable` keeps its pipe/ring alive across `KSSTATE_STOP` and only collapses it when the stream is truly going away. AO's pre-Phase-4 behavior made STOP the accidental hard-reset trigger: Windows audio engine pauses/resumes streams around normal workflow events (including brief speaker gaps during a call), and STOP-time unregister cleared `SpeakerActive` / `MicActive` / ring state on a path that is not actually the end of the stream. Moving unregister to destructor teardown restores VB-like stream lifetime while PAUSE's conservative `!otherSideActive` reset gives us the one place that VB does collapse ring/fade state after a RUN: after the DPC flush and only when the peer direction is not still running.

**Non-negotiable guardrails (all held):**
- Phase 3 stays SHADOW_ONLY. `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` / `AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE` remain clear.
- `KeFlushQueuedDpcs()` ordering in PAUSE is untouched and is the barrier the new reset block sits behind.
- STOP never calls `FramePipeUnregisterFormat()` after this commit.
- No transport ownership move. `UpdatePosition()` / `WriteBytes()` / `ReadBytes()` / Phase 3 pump helper are unchanged.
- No IOCTL layout change. No `test_stream_monitor.py` schema change. No new `FRAME_PIPE` fields.
- No fade-in refactor in this commit (`Phase 4a` can pick that up later if needed).

**Highest risk: destructor reorder.** The teardown reorder affects both cable and non-cable streams. Non-cable streams skip the unregister step but still go through the new ordering (StreamClosed → timer delete → DPC flush → Release → free pool). Expected Phase 4 validation therefore repeats the full Phase 3 A/B validation, plus STOP → RUN / PAUSE → RUN churn, to catch any lifetime regression introduced by the reorder.

**Verification:**
- `build-verify.ps1 -Config Release` — expected green.
- `install.ps1 -Action upgrade` — expected `INSTALL_EXIT=0`, hash match.
- `test_ioctl_diag.py` — expected `ALL PASSED` (V1 IOCTL path intact).
- `test_stream_monitor.py --once` — idle counters sane, `StructSize == 116`.
- `tests/phase3_shadow_active.py` — A-step regression: `Inv > 128`, `Div == 0`, `OverJump == 0`, flags reach `0x00000003`, frame accounting matches duration × samplerate.
- `tests/phase3_live_call_shadow.py` — B-step regression: real Phone Link call, `A_Render` peak `Inv > 128`, `Div == 0`, no visible STOP-era collapse symptoms.

**Exit criteria (Codex Phase 4):**
1. State churn no longer causes the old AO-style continuity failures.
2. STOP is no longer the accidental hard-reset trigger for cable flow.
3. Phase 3 shadow/divergence counters do not newly regress after STOP/PAUSE changes land.

**Rollback:** `git revert <this commit>`, rebuild, reinstall. Rollback surface is 1 source file + this changelog. Phase 4 is not flag-gated; rollback is source-level.

---

## 2026-04-13 — Phase 3: Shadow-mode query pump on GetPositions (no ownership move)

**Files changed:**
- `Source/Main/minwavertstream.h` — +`PumpToCurrentPositionFromQuery(LARGE_INTEGER ilQPC)` private helper declaration, placed between `UpdatePosition` and `SetCurrentWritePositionInternal`. No new fields — all 14 Phase 1 state fields are reused.
- `Source/Main/minwavertstream.cpp` — +helper implementation (right before `UpdatePosition`). `GetPositions()` now calls `PumpToCurrentPositionFromQuery(ilQPC)` in the same call as `UpdatePosition(ilQPC)`, still under `m_PositionSpinLock`. `UpdatePosition()` stashes the finalized (post-EOS-clamp, post-block-align) `ByteDisplacement` into `m_ulLastUpdatePositionByteDisplacement` at function end. `SetState(KSSTATE_RUN)` arms `AO_PUMP_FLAG_ENABLE | AO_PUMP_FLAG_SHADOW_ONLY` for cable endpoints and re-arms per-run state (monotonic counters preserved). `SetState(KSSTATE_STOP)` clears all pump state and zeros the mirrored pipe feature flags.
- `results/phase3_edit_proposal.md` — approval record already locked in checkpoint commit `f042b79`.

**What:** Phase 3 introduces a SHADOW-ONLY pump helper on the confirmed hot path (`GetPositions()`, proven in Phase 0 runtime evidence at 120 calls / 5 s during a real Phone Link run). The helper computes elapsed frames from a run-local QPC baseline, applies the 8-frame minimum gate (`FP_MIN_GATE_FRAMES`), applies a dynamic over-jump guard at `max(FP_MIN_GATE_FRAMES, framesPerDmaBuffer / 2)`, and runs a rolling-window compare against the same-call legacy `ByteDisplacement` over `AO_PUMP_SHADOW_WINDOW_CALLS = 128` calls with a `max(16 frames, 2% of larger total)` tolerance. Stream counters are mirrored into the Phase 1 per-direction `FRAME_PIPE` slots.

**Why:** Phase 3 is the parity-first gate in front of Phase 5 (render ownership move) and Phase 6 (capture ownership move). Before any transport ownership moves from legacy `UpdatePosition()` → `ReadBytes()/WriteBytes()` into a query-driven pump, the pump's timing/accounting has to prove itself against the legacy math on real runtime under the same spinlock. Shadow-mode isolates timing and accounting risk from transport risk.

**Non-goals:**
- No transport mutation. The helper never calls `FramePipeWriteFromDma`, `FramePipeReadToDma`, `WriteBytes`, or `ReadBytes`.
- `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` / `AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE` stay clear in Phase 3.
- `GetPosition()` (cold path per Phase 0) is untouched.
- `TimerNotifyRT` (legacy timer path) is untouched — Phase 3 is strictly query-path bring-up.
- No IOCTL shape change. No `test_stream_monitor.py` schema change. Phase 1 already landed the diagnostic contract.
- No new `FRAME_PIPE` fields. Phase 1 already split render/capture slots.

**Stash semantic (Revision 2, locked 2026-04-13 after first-flush false divergence):**
`UpdatePosition()` now accumulates the finalized ByteDisplacement into `m_ulLastUpdatePositionByteDisplacement` with `+=`, not `=`. Reason: `TimerNotifyRT` also calls `UpdatePosition()` every 1 ms for cable endpoints, under the same `m_PositionSpinLock`. With per-call overwrite, the pump's rolling-window legacy-byte sum only captured the residual bytes between the most recent timer tick and each `GetPositions()` call (~1 ms worth). The pump's elapsed-time math, in contrast, covered the full baseline-to-now interval (~100 ms per call). The resulting ~99% mismatch guaranteed a false divergence firing on every window flush. With `+=`, the stash holds the true total bytes legacy transport processed between pump consumes, matching the pump's baseline-to-now scope. The pump helper reads and zeros this field under the same spinlock, so reader/writer are fully serialized.

**Locked defaults (from approval record in `results/phase3_edit_proposal.md`, Revision 1 + Revision 2):**
1. `AO_PUMP_SHADOW_WINDOW_CALLS = 128`
2. Divergence tolerance = `max(16 frames, 2% of larger total)`
3. Over-jump threshold = `max(framesPerDmaBuffer * 2, sampleRate / 4)`
4. Cable-endpoint guard lives inside `PumpToCurrentPositionFromQuery()` (single source of truth).

**Over-jump semantic (Revision 1, locked 2026-04-13 after false-green bring-up):**
Phase 3 over-jump is count-only diagnostic. When `newFrames` exceeds the threshold, `m_ulPumpOverJumpCount++` fires but the helper does NOT return, does NOT rebase `m_ulPumpProcessedFrames`, and does NOT reset the rolling-window accumulators. Frame accounting and shadow compare proceed normally. Real skip/clamp semantics return in Phase 5/6 when the pump starts owning transport. This revision was required because the first bring-up run hit the over-jump branch on ~96% of calls, so the rolling shadow window never flushed and `PumpShadowDivergenceCount == 0` was a false green. Phase 3 closure now additionally requires at least one real window flush during active validation.

**State-transition contract:**
- **RUN arm (cable only):** set flags = `ENABLE|SHADOW_ONLY`; zero baseline, processed-frames, last-buffer-offset, window accumulators, stash, per-session `GatedSkipCount`/`OverJumpCount`; preserve monotonic `InvocationCount`/`ShadowDivergenceCount`/`FramesProcessed`; snapshot flags into the correct `FRAME_PIPE` direction slot.
- **STOP clear (cable only):** zero everything including monotonic counters and mirrored pipe feature flags so a new STOP→RUN session starts from zero.
- **PAUSE:** untouched. RUN re-arm rebuilds the timing baseline on resume.
- **Runtime rollback:** clearing `AO_PUMP_FLAG_ENABLE` at runtime immediately turns the helper into a no-op — no reinstall/reboot.

**Exit criteria (Codex Phase 3, Revision 1):**
1. AO builds green.
2. No regression in basic playback/capture open/close.
3. Helper shows sane frame deltas under position polling.
4. Shadow mode proves gate/accounting stability.
5. 5-minute Phone Link-like run: `PumpShadowDivergenceCount` windowed count stays at `0`.
6. Zero-divergence holds under normal, aggressive, and sparse polling.
7. **At least one real rolling-window compare executes during active validation** — `0` divergence with zero window flushes does not pass.
8. Clearing the feature flag at runtime turns the helper into an accounting-only no-op.

**Rollback:** `git revert <this commit>`, rebuild, reinstall. Expected surface is 2 source files + this changelog.

---

## 2026-04-13 — Phase 2: Format parity fixes for 32-bit PCM and 32-bit float (G9, G10)

**Files changed:**
- `Source/Utilities/loopback.cpp` — `FpNorm32i`, `FpDenorm32i`, `FpNormFloat`, `FpDenormFloat` now do direct copy / direct bit cast, matching VB-Cable's observed behavior on 32-bit PCM and 32-bit float paths.
- `docs/VB_CABLE_AO_COMPARISON.md` — §1-3 and §3-2 tables: 32-bit int and 32-bit float rows flip from `❌` to `✅`. 8-bit rows stay `❌ 미구현` and are annotated "deferred — parity-first, no new functionality before parity closure."

**What:** The Phase 1 FRAME_PIPE kept AO's pre-rewrite normalization strategy where 32-bit int samples were right-shifted by 13 on the way in and 32-bit float samples went through `FloatBitsToInt24(bits) >> 5`. Phase 2 replaces both with direct copy so the cable preserves the application's original 32-bit bit pattern bit-for-bit on the cable-only single-writer / single-reader transport.

**Why:** Documented in `docs/VB_CABLE_AO_COMPARISON.md` §1-3 / §3-2. VB-Cable passes 32-bit PCM and 32-bit float through unchanged. AO's pre-Phase-2 behavior truncated 13 bits of INT32 dynamic range and converted float via a 24-bit intermediate, losing ~5 bits of float mantissa precision through the round-trip. The mismatch was identified as G9 (int) and G10 (float) in the Phase 2 target list.

**Parity-first principle:** Phase 2 is governed by the parity-first rule — no new functionality lands until every parity phase is closed. AO must first behave identically to VB-Cable on the paths AO already implements; only after all parity phases are closed may additions / improvements / extensions be considered.

**8-bit (G11) deferred — parity-first:** Inspection during Phase 2 planning found that 8-bit format is not implemented at all in AO — neither `FpNorm8` / `FpDenorm8` nor 8-bit branches exist in `FramePipeWriteFromDma` / `FramePipeReadToDma`. The Claude plan's G11 item was written assuming 8-bit was implemented-but-wrong, which is not the current state. Implementing 8-bit is therefore a new code path, not a parity correction. **Deferred because parity-first, no new functionality before parity closure.** `docs/VB_CABLE_AO_COMPARISON.md` continues to mark the 8-bit rows as `❌ 미구현`.

**32-bit headroom note:** Pipe samples on 32-bit paths now carry the application's raw bit pattern. This is safe on the cable-only single-writer / single-reader transport but eliminates the prior mixing headroom. If a future phase introduces mixing of 32-bit streams on a single pipe, the headroom strategy must be re-evaluated before or alongside that phase. 16-bit and 24-bit paths are unchanged and retain the normalized ~19-bit pipe representation that matches VB-Cable.

**Which helpers stayed:** `FloatBitsToInt24` (lines 73–122) and `Int24ToFloatBits` (lines 124–167) are NOT removed because they are still used by `ReadSample` / `WriteSample` (the legacy `LOOPBACK_BUFFER` path, lines 175 and 196). Only the `FpNormFloat` / `FpDenormFloat` wrappers bypass them after Phase 2. A later phase that removes the LOOPBACK_BUFFER code entirely can re-evaluate those helpers.

**Verification:**
- `build-verify.ps1 -Config Release` — green, 17 PASS / 0 FAIL.
- `install.ps1 -Action upgrade` — `INSTALL_EXIT=0`.
- `test_ioctl_diag.py` — `ALL PASSED` on both cables (V1 IOCTL path intact).
- `test_stream_monitor.py --once` — Phase 1 counters stay at 0, `StructSize == 116`, no new divergence.

**Exit criteria (Codex Phase 2):** AO builds, format-focused tests stay green, no transport ownership change. All met.

**Rollback:** `git revert <this commit>`, rebuild, reinstall.

---

## 2026-04-13 — Phase 1: Diagnostic counters and rollout scaffolding

**Files changed:**
- `Source/Main/ioctl.h` — +`AO_V2_DIAG` struct (`StructSize` + 4 per-cable-per-direction blocks of 7 ULONGs = 116 bytes) returned via existing `IOCTL_AO_GET_STREAM_STATUS` when the caller's output buffer is large enough. V1 callers unchanged. Compact naming (`A_R_GatedSkipCount` style). `C_ASSERT` on struct shape.
- `Source/Utilities/loopback.h` — +12 per-direction fields on `FRAME_PIPE`: `RenderGatedSkipCount`, `RenderOverJumpCount`, `RenderFramesProcessedTotal`, `RenderPumpInvocationCount`, `RenderPumpShadowDivergenceCount`, `RenderPumpFeatureFlags`, and `Capture*` mirrors. Per-direction split avoids the Speaker-vs-Mic race inherent in any single-counter design.
- `Source/Utilities/loopback.cpp` — `FramePipeInit` zeros all new fields. `FramePipeReset` zeros only per-session counters (`RenderGatedSkipCount`, `RenderOverJumpCount`, `CaptureGatedSkipCount`, `CaptureOverJumpCount`) and preserves monotonic run-totals (`*FramesProcessedTotal`, `*PumpInvocationCount`, `*PumpShadowDivergenceCount`, `*PumpFeatureFlags`) so Phase 3's shadow-window divergence ratio stays measurable across RUN→PAUSE→RUN.
- `Source/Main/minwavertstream.h` — +4 `AO_PUMP_FLAG_*` bit constants (`ENABLE`, `SHADOW_ONLY`, `DISABLE_LEGACY_RENDER`, `DISABLE_LEGACY_CAPTURE`). +14 member fields on `CMiniportWaveRTStream` for pump state, counters, feature flags, and Phase 3 shadow-window accumulators.
- `Source/Main/minwavertstream.cpp` — `Init()` zeros every new field.
- `Source/Main/adapter.cpp` — `IOCTL_AO_GET_STREAM_STATUS` handler extended with a length-gated V2 branch that snapshots `g_CableAPipe` / `g_CableBPipe` into `AO_V2_DIAG`. `bytesReturned` now set explicitly in both V1 and V2 paths. +`C_ASSERT(sizeof(AO_V2_DIAG) == 116)` as a belt-and-suspenders shape guard mirroring the one in `ioctl.h`.
- `test_stream_monitor.py` — speculative V2 parser (Passthrough / PushLoss / PullLoss / ConvOF / ConvSF / PipeFrames / PipeFill / SpkWrite / MicRead / MaxDpcUs / PosJump) removed and replaced with a real `AO_V2_DIAG` parser matching the driver-side struct byte-for-byte. Opens the device with `V1_STATUS_SIZE + V2_DIAG_SIZE = 180` bytes. Displays per-direction counter rows with the derived shadow-divergence ratio (`div / inv * 100`) which is the Phase 3 exit-criterion metric.

**What:** Phase 1 lands the diagnostic data contract and feature-flag skeleton that Phase 3 (pump helper), Phase 5 (render transport rebind), and Phase 6 (capture transport rebind) will read and write. No execution path in Phase 1 ever writes a non-zero value into these fields or reads a feature flag for a decision. Phase 1 is pure declaration plus zero initialization plus an IOCTL surface extension.

**Why:** Codex Phase 1 scope — "we should not start moving transport without first making it observable." Runtime visibility must land before the transport-ownership changes in Phase 3/5/6, or debugging those phases becomes blind. Keeping ioctl.h, adapter.cpp, and test_stream_monitor.py in lock-step in a single commit satisfies the Diagnostics Rule from `CLAUDE.md`.

**Exit criteria (Codex):**
- AO builds — `build-verify.ps1 -Config Release` green.
- Diagnostics sane and zero at idle — `python test_stream_monitor.py --once` prints every new counter as `0` after install and remains `0` across a Phone Link AO call (Phase 1 has no writer for any counter).
- No transport behavior change is observable — `test_ioctl_diag.py ALL PASSED` continues; no functional regression.

**Rollback:** `git revert <this commit>`, rebuild, reinstall. No state outside the 8 changed files.

---

## 2026-04-13 — Driver INF target OS widening

**Files changed:**
- `Source/Main/aocablea.inx` — `NT$ARCH$.10.0...22000` → `NT$ARCH$.10.0...14393` in both `[Manufacturer]` and the `[AOCABLEA.NT$ARCH$.14393]` Models section header.
- `Source/Main/aocableb.inx` — same change for `[AOCABLEB...]`.
- `Source/Main/VirtualAudioDriver.inx` — same change for `[VIRTUALAUDIODRIVER...]`.

**What:** Lowered the minimum target OS build number encoded in every driver INF from 22000 (Windows 11 21H2) to 14393 (Windows 10 1607 / LTSC 2016). No C++ or H changes; only the INF sectioning used by PnP manager to decide installability.

**Why:** The prior value restricted installs to Windows 11 only. Target PCs running Windows 10 22H2 (build 19045) were rejected by PnP manager with no error surfaced past `devcon update` — the symptom was `devcon.exe failed` and `Expected 2 active devices, found 0`. 14393 is the lowest value WDK 10.0.26100 `infverif` accepts with an explicit build number (it rejects 10240 and below with `ERROR(1288): must be '10.0.14310' or greater`). 14393 covers Windows 10 1607 LTSC 2016, all later Windows 10 releases (1709–22H2), all Windows 11 releases, and Windows Server 2016/2019/2022/2025.

**Verification:**
- `infverif /v /w` on the built `aocablea.inf` and `aocableb.inf`: passes (previously failed with `ERROR(1288)` when tried at 10240).
- Host (Windows 11 26200) install via `install.ps1 -Action upgrade`: `INSTALL_EXIT=0`, `test_ioctl_diag.py` reports `ALL PASSED` for Cable A and Cable B.
- Target PC (Windows 10 22H2 / build 19045): installer package built from this INF reaches `Installation complete!`; both AO Cable A and AO Cable B appear in sound settings. This is what unblocked the Phase 0 Step 4/5 two-box KDNET target.

**Non-goal:** runtime driver behavior is unchanged. This is an installability-only fix.

---

## 2026-04-12 — Fixed Pipe Rewrite

### Phase 2 fix: Old path disable + rate mismatch fail-closed

**Files changed:**
- `Source/Main/minwavertstream.cpp` — Disabled: LoopbackRegisterFormat/UnregisterFormat/MicSink stash/register in RUN/STOP/PAUSE, MicSink gap zero-fill in UpdatePosition. Added: Speaker PAUSE → FramePipeUnregisterFormat (fixes ActiveRenderCount drift).
- `Source/Utilities/loopback.cpp` — WriteFromDma: !SpeakerSameRate → drop entire batch + DropCount. ReadToDma: !MicSameRate || !MicActive → silence fill.

**What:** Old LOOPBACK_BUFFER control path fully disabled in state transitions — no more LoopbackRegisterFormat, MicSink, MicDmaStash calls. Rate mismatch is fail-closed: data dropped (speaker) or silence (mic) until Phase 3 SRC. Speaker Pause now properly unregisters from FRAME_PIPE.

**Why:** Old and new paths were both active, risking MicSink interference and stale state. Rate guard prevents garbled output from unimplemented SRC path.

**Build:** 17 PASS / 0 FAIL

---

### Phase 2: Stream Integration — DPC Path Rewrite

**Files changed:**
- `Source/Utilities/loopback.h` — +IsFloat fields (SpeakerIsFloat, MicIsFloat), +Phase 2 API declarations (RegisterFormat, UnregisterFormat, WriteFromDma, ReadToDma)
- `Source/Utilities/loopback.cpp` — +INT32 ~19-bit normalization helpers (FpNorm16/24/32i/Float, FpDenorm*), +FramePipeRegisterFormat/UnregisterFormat (format state machine with both-stopped reset), +FramePipeWriteFromDma (batch: DMA→normalize→channel map→pipe write), +FramePipeReadToDma (batch: pipe read→channel map→denormalize→DMA)
- `Source/Main/adapter.cpp` — +FramePipeInit in DriverEntry (ms→targetFillFrames conversion), +FramePipeCleanup in unload and error paths
- `Source/Main/minwavertstream.cpp` — ReadBytes: g_CableALoopback→g_CableAPipe, LoopbackWrite/WriteConverted→FramePipeWriteFromDma. WriteBytes: MicSink direct-push path removed, LoopbackRead/ReadConverted→FramePipeReadToDma. SetState RUN: +FramePipeRegisterFormat. SetState STOP/PAUSE: +FramePipeUnregisterFormat.

**What:** DPC data path now flows through FRAME_PIPE instead of LOOPBACK_BUFFER. Speaker DPC normalizes DMA bytes to INT32 ~19-bit, channel-maps to pipe width, writes via FramePipeWriteFrames (all-or-nothing). Mic DPC reads from pipe, channel-maps, denormalizes to native format, writes to DMA. MicSink direct-push path eliminated — pipe is the sole transport. Old LOOPBACK_BUFFER code still compiled but no longer called from DPC paths.

**Why:** Core transport switch from byte-ring (packed 24-bit, overwrite-oldest) to frame-indexed INT32 pipe (hard reject, explicit fill tracking). This is the functional activation of Phase 1 infrastructure.

**Normalization ranges:**
- 16-bit: `s16 << 3` (lossless round-trip)
- 24-bit: `s24 >> 5` (loses lower 5 bits, same as VB-Cable)
- 32-bit int: `s32 >> 13`
- 32-bit float: `FloatBitsToInt24(bits) >> 5` (integer-only, DISPATCH safe)

**Build:** 17 PASS / 0 FAIL (build-verify.ps1 Release)

---

### Phase 1: FRAME_PIPE Core Struct + API (feature/ao-fixed-pipe-rewrite)

**Files changed:**
- `Source/Utilities/loopback.h` — +FRAME_PIPE struct (with FillFrames explicit counter), +constants, +core API declarations, +g_CableAPipe/g_CableBPipe externs
- `Source/Utilities/loopback.cpp` — +FRAME_PIPE implementation: Init (with re-init guard), Cleanup, WriteFrames (entire-write hard reject), ReadFrames (startup silence + underrun zero-fill), Reset, GetFillFrames, +global instances

**What:** VB-style fixed frame pipe core. INT32 ring buffer indexed by frames with explicit FillFrames counter (no full/empty ambiguity). Hard reject on overflow = entire write rejected (all-or-nothing, DropCount += frameCount). Silence fill on underrun (UnderrunCount++). StartThreshold gate prevents thin-fill at startup. Min gate (FP_MIN_GATE_FRAMES) defined as constant but NOT enforced in core API — DPC layer responsibility. Re-init safe (cleanup-before-init guard). Coexists with existing LOOPBACK_BUFFER — no existing code modified.

**Why:** Replaces byte-ring transport (packed 24-bit, overwrite-oldest) with frame-indexed INT32 pipe per VB-Cable RE findings. This is the foundation for Phase 2 (stream integration) and Phase 3 (format conversion).

**Review fixes (same session):**
1. Full/empty ambiguity → added `FillFrames` explicit counter (was WriteFrame-ReadFrame diff only)
2. Overflow partial write → entire write reject (all-or-nothing per architecture plan)
3. MIN_GATE removed from core WriteFrames → DPC policy layer
4. Re-init memory leak → cleanup-before-init guard in FramePipeInit

**Build:** 17 PASS / 0 FAIL (build-verify.ps1 Release)

---

## 2026-04-12 — V2 Research (feature/ao-pipeline-v2, frozen)

### Phase 2: V2 Diagnostics IOCTL + PipeFillFrames + RESET_COUNTERS

**Files changed:**
- `Source/Main/ioctl.h` — +AO_V2_DIAG struct (SessionPassthrough, ActiveRenderCount, loss counters, PipeSizeFrames, PipeFillFrames per cable), +IOCTL_AO_RESET_COUNTERS
- `Source/Main/adapter.cpp` — GET_STREAM_STATUS V2 extension (appends AO_V2_DIAG when buffer large enough), RESET_COUNTERS handler
- `test_stream_monitor.py` — V2 rewrite: parses AO_V2_DIAG, shows Fill level + PullLoss delta + WRITE/IDLE state

**What:** V2 diagnostics IOCTL extension. GET_STREAM_STATUS returns V1 (64 bytes) for old callers, appends AO_V2_DIAG for V2 callers. RESET_COUNTERS zeros all loss counters. Monitor shows real-time pipe fill level and PullLoss delta per poll to distinguish speech-active underrun from idle-silence underrun.

**Why:** Needed to verify SessionPassthrough activation and diagnose remaining VB-Cable quality gap. PipeFillFrames confirmed ring is healthy during speech (Fill=48-96 frames, PullLoss delta=0). PullLoss only increases during AI silence/idle turns — not a driver quality issue.

**Key finding:** CableB PullLoss is 100% idle-silence zero-fill. Speech playback has zero underrun. Remaining quality gap is timing/position, not buffer/conversion.

### Phase 1: SessionPassthrough + Loss Counters + Same-Rate Fix

**Files changed:**
- `Source/Utilities/loopback.h` — +SessionPassthrough, PipeFormat, PipeBlockAlign, PipeSizeFrames, PushLossFrames, PullLossFrames, ConvWriteOverflowFrames, ConvReadShortfallFrames
- `Source/Utilities/loopback.cpp` — +UpdateSessionPassthrough(), RegisterFormat/UnregisterFormat hook, LoopbackWrite PushLoss+MicSink guard, LoopbackRead PullLoss, WriteConverted/ReadConverted SessionPassthrough branch+ConvCounters, ReadConverted same-rate +1 fix, Init
- `Source/Main/minwavertstream.cpp` — WriteBytes MicSink no-op guard, UpdatePosition zero-fill guard, WriteBytes/ReadBytes SessionPassthrough branch
- `Source/Main/adapter.cpp` — C_ASSERT sizeof/offset update

**What:** V2 Phase 1 — ring-read passthrough for same-format PCM mono/stereo sessions. When Speaker and Mic open with identical format (e.g., 48k/16/2ch from Phone Link), data bypasses all conversion and flows raw through ring buffer. Loss counters track overflow/underrun/conversion shortfall in frames. Fixed same-rate `internalFrames += 1` bug that over-consumed ring by 1 frame per DPC tick.

**Why:** AO's FormatMatch required Speaker==Mic==Internal(48k/24/8ch), forcing conversion even when Phone Link opens both sides at 48k/16/2ch. VB-Cable passes through without conversion in this case. Ring-read mode chosen over MicSink direct-push for observability (PipeFill/PushLoss/PullLoss counters) and safety (no MicSink race).

**Design decisions:**
- Ring-read mode: MicSink disabled during passthrough via 3 `!SessionPassthrough` guards
- PipeSizeFrames = BufferSize / PipeBlockAlign (actual backing capacity)
- Phase 1 scope: PCM mono/stereo only (float/multichannel deferred to Phase 3)
- Loss counters against backing buffer (4s), not logical target latency

## 2026-04-11

### tests/live_call/ — Simplified live call quality test

**Files:**
- `tests/live_call/run_test_call.py` — single-command test entry point
- `tests/live_call/realtime_engine.py` — OpenAI Realtime API, capture/playback threads, AMD
- `tests/live_call/audio_bridge.py` — PyAudio AO Cable A capture (8kHz) / Cable B playback (24kHz)
- `tests/live_call/audio_router.py` — system default device switching (set_call_routing / restore_routing)
- `tests/live_call/phone_link_dialer.py` — Phone Link dial automation (hidden URI + UI)
- `tests/live_call/phone_link_worker.py` — Phone Link UI worker subprocess
- `tests/live_call/tools/svcl/svcl.exe` — SoundVolumeView CLI for per-app routing
- `tests/live_call/.env.example` — configuration template

**What:** Ported and simplified live call test from AIoutboundcall repo
(backup/hidden-uri-realtime-baseline @ 7946d42 + work/ao-cable-driver-20260407 audio_router).
Removed FastAPI server, database, dashboard, WebSocket — replaced with single `run_test_call.py`.

**Why:** V2 pipeline changes need end-to-end validation with real phone calls through AO Cable A/B.
Claude runs the script, user answers phone and reports quality. No server/campaign overhead.

**Changes from original:**
- Removed: main.py, call_orchestrator.py, database.py, templates/
- Added: run_test_call.py (route → dial → wait → AI → restore)
- Hardcoded paths → relative `Path(__file__)` resolution
- `C:\Users\admin\` → `%LOCALAPPDATA%`
- Default device names → `AO Cable A Output` / `AO Cable B Input`

### CLAUDE.md — Updated for V2

**Files changed:** `CLAUDE.md`

**What:** Replaced V1 instructions with V2 branching rules, diagnostics rule,
V2 changelog rule, design bias, and simplified test procedure.

**Why:** V2 branch needs its own instructions; V1 CLAUDE.md was outdated.
