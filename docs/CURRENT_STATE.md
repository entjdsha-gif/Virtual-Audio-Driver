# AO Virtual Cable — Current State & Roadmap

**Last updated:** 2026-04-15
**Branch:** `feature/ao-fixed-pipe-rewrite`
**Effective baseline commit:** `ed23271` (Phase 5c)
**Canonical doc:** this file. Older plan docs kept as historical reference (see bottom).

---

## Where we are

### Completed phases (VB-equivalent principles applied)

| Phase | Scope | Commit | Status |
|---|---|---|---|
| 1 | Diagnostic counters + feature flag scaffolding | — | ✅ |
| 2 | Format parity (32-bit PCM, 32-bit float) | — | ✅ |
| 3 | Shadow-mode query pump on GetPositions | — | ✅ |
| 4 | STOP/PAUSE semantic alignment | `439bbcd` | ✅ |
| 5 | Render transport ownership → GetPositions pump | `2c733f1` | ❌ **reverted** (regression) |
| 5c | Phase 5 transport revert + scaffold kept | `ed23271` | ⚠️ current tip |

### What Phase 5c did and did not solve

Phase 5 attempted to move the cable-render transport call site from the legacy `UpdatePosition` packet cadence into a query-driven helper called from `GetPositions`. Measurement proved the design wrong: `GetPositions` fires at ~130 ms in practice while Phone Link reads Cable B mic at ~1 ms, producing a burst+gap pattern at the pipe output and phone-side chopping. Phase 5c reverted the transport call site back to `UpdatePosition` packet cadence but kept the Phase 5 scaffold (IOCTL, counters, ownership flags) for later reuse.

Live-call measurement confirmed:
- **Front-chopping** (first AI utterance lost): resolved by a 40 ms startup headroom prefill added during Phase 5c iterations.
- **Mid-call chopping** (degraded voice quality on phone): **still present**. Cable B mic runs at ~5 ms ring depth in steady state because writer (UpdatePosition packet cadence) and reader (Phone Link 1 ms pulls) are rate-matched but have zero headroom to absorb WaveRT packet jitter and AI's 200–250 ms burst cadence. Both 40 ms and 300 ms fixed-size prefill experiments proved prefill size is not the right axis — a cushion that depletes between AI bursts cannot fix a cadence mismatch.

### Why VB is not chopping and we are

Reverse engineering (see `results/vbcable_pipeline_analysis.md`, `VB_CABLE_AO_COMPARISON_CODEX_NOTES.md`) shows VB-Cable does **not** rely on WaveRT packet cadence for ring fills. Ring fill is driven by the position-query callback (`KSPROPERTY_AUDIO_POSITION`) recalculating elapsed frames from QPC on every query, with a 1 ms high-resolution `ExAllocateTimer` running alongside for metering and drift correction (63/64 phase correction, rebase every 100 ticks). The cadence owner is the driver itself, not the upstream audio source. AO still ties writer cadence to upstream packet delivery, which is why the AI's 250 ms burst pattern surfaces as phone chopping.

---

## Where we are going — Phase 6

**Authoritative plan: `docs/PHASE6_PLAN.md` (Codex, 2026-04-15).** Per CLAUDE.md "플랜은 Codex, 실행은 Claude" rule, that file is the source of truth for Phase 6 design. This section is a summary only.

### One-line definition

Phase 6은 AO 위 patch가 아니라, **VB-equivalent shared-timer transport core replacement**이다.

### What changes vs the current codebase

- A global `AO_TRANSPORT_ENGINE` with a single high-resolution timer owns transport cadence for *all* active streams (render + capture, Cable A + B). No per-pipe timers.
- Each stream gets an `AO_STREAM_RT` runtime struct tracking `NextEventQpc`, `FramesPerEvent`, `StartupArmed`, `CarryFrames` etc.
- Transport is **event-driven** (`now >= NextEventQpc`), not query-driven.
- `GetPositions` / `UpdatePosition` become reporting-only. They never touch FRAME_PIPE.
- `StartPhaseComplete` sticky flag is removed; startup is re-armed per session via an explicit state machine.
- Phase 5 scaffold (`IOCTL_AO_SET_PUMP_FEATURE_FLAGS`, `RenderPumpDriveCount`, query-driven pump helpers) is deleted in the final step.

### Six implementation steps

1. Add `AO_TRANSPORT_ENGINE` + `AO_STREAM_RT` structures (no behavior change)
2. Register/unregister streams in engine at RUN/PAUSE/STOP (legacy transport still active)
3. Move **render** side to engine event transport; drop legacy render path
4. Move **capture** side to engine event transport; apply startup/recovery state machine
5. Finish position-path decouple; remove all legacy transport coupling
6. Delete Phase 5 scaffold (IOCTLs, feature flags, pump helpers, rollback infra)

See `docs/PHASE6_PLAN.md` for full pseudo-code of `AoTransportTimerCallback`, `AoRunRenderEvent`, `AoRunCaptureEvent`, and the success criteria.

### Phase 6 keystone

Steps 1–2 are mechanical. Step 3 (render move) is the first behavior change. **Step 4 (capture move + startup state machine) is the actual chopping fix** — this is where the mid-call chopping must disappear. Steps 5–6 are cleanup.

### Pending before Step 1 kickoff

- Request file-level breakdown from Codex: where `AO_TRANSPORT_ENGINE` lives, how the engine links into `adapter.cpp` init and `minwavertstream.cpp` RUN/STOP, whether Step 1 is a new source file or an addition to `loopback.*`. Recorded as a followup in `PHASE6_PLAN.md` § "Next follow-up".
- Commit or revert the uncommitted Bug A Option 2 hns-precision math so Phase 6 starts from a clean working tree.

---

## After Phase 6 — product extension (Stage B)

User requirement beyond VB-equivalent: **accept more channel counts** and **accept all input formats** so the cable is a universal audio sink. Stage B work is scoped after Phase 6 ships:

1. Channel extension — verify current `FP_MAX_CHANNELS = 16` path, expose 7.1/Atmos configurations through ControlPanel
2. Broader format wildcard — 44.1 / 48 / 96 / 192 kHz, int16 / int24 / int32 / float32, any pipe sample rate via SRC
3. Answering-machine detection (AMD) — telephony feature
4. ControlPanel runtime config — latency / channels / format live switch

Stage B is NOT part of Phase 6. Phase 6 must land cleanly first.

---

## Stage C — production polish (after Stage B or parallel)

1. `install.ps1` quiesce stability — reduce reboot-fallback frequency
2. `install.ps1` default-device restore bug — avoid silently reverting from AO to VB after install cycles (observed this session)
3. Test harness auto Cable A/B verification — enforce pre-experiment check
4. Changelog / memory consolidation pass
5. Phase number cleanup

---

## Authoritative doc hierarchy

When two docs disagree about current state, resolve in this order:

1. **Runtime evidence** (captures, DbgView logs, user perceptual reports)
2. **This file (`docs/CURRENT_STATE.md`)** — single source of truth for current phase, roadmap, and scope
3. **`docs/PIPELINE_V2_CHANGELOG.md`** — per-commit historical record
4. **`docs/VB_CABLE_AO_COMPARISON_CODEX_NOTES.md`** — VB reverse-engineering factual reference
5. **`docs/VB_CABLE_DUAL_PLAN_OPERATING_RULES.md`** — Codex/Claude workflow discipline
6. **`docs/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CODEX.md`** — long-form architecture blueprint (historical, Phase 5c addendum pending)
7. **`docs/AO_V2_ARCHITECTURE_PLAN.md`** — M1-M6 productization history (byte-ring era, architecturally superseded)
8. **`docs/archive/`** — historical plan docs kept for context only

CLAUDE.md points to this file as the roadmap. Memory `project_remaining_tasks.md` mirrors the current-phase summary.

---

## Archived

- `docs/archive/MILESTONES_M1-M6_ACHIEVED.md` — former `VBCABLE_SURPASS_PLAN.md`. M1-M6 milestone completion claim, valid when written but the "Surpass VB" outcome is now contingent on Phase 6.
