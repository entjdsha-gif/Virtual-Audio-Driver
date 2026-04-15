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

Phase 6 replaces Phase 5's failed GetPositions-driven approach with a **dedicated 1 ms high-resolution timer** as the cadence owner, while keeping the rest of the AO scaffold (install, service, PnP, ControlPanel, IOCTL, test harness) intact.

### P6 steps

**P6-0 — Frames-based unit normalization**
- `minwavertstream.cpp` position math: byte displacement + hns → frame displacement
- Commit Bug A Option 2 (`hns-precision` position math) which is currently uncommitted
- Prerequisite for clean P6-1

**P6-1 — Cadence timer** *(the big step)*
- `FramePipeCadenceTimer` — `ExAllocateTimer(EX_TIMER_HIGH_RESOLUTION)` per pipe (Cable A, Cable B independent)
- Period 1 ms, due time 1 ms (matches VB)
- Armed on first `FramePipeRegisterFormat` (either direction)
- DPC callback:
  - Elapsed frames since last fire via QPC diff
  - 8-frame minimum gate (skip sub-sample noise)
  - 63/64 drift correction, 100-tick rebase
  - Speaker active → pull from upstream DMA into ring
  - Mic active → push ring content to downstream DMA
- Disarmed on last `UnregisterFormat` via `KeFlushQueuedDpcs` + reset

**P6-2 — Decouple UpdatePosition from transport**
- `UpdatePosition` becomes position-counter-only (WaveRT contract still satisfied)
- All data movement via cadence timer
- Remove `ReadBytes` / `WriteBytes` coupling to FRAME_PIPE

**P6-3 — Remove Phase 5 scaffold**
- `PumpToCurrentPositionFromQuery`, `pumpOwnsRender` dual-gate, Phase 5 IOCTL, feature flags, Phase 5 counters — delete
- Phase 5c comments cleaned up

**P6-4 — Cable A + B live-call validation**
- **Pre-experiment cable check mandatory** (default playback = AO Cable A, default recording = AO Cable B; see CLAUDE.md § Pre-Experiment Cable Check)
- 4-point soundcard capture + user perceptual judgment vs VB
- Iterate until both cables match VB quality

**P6-5 — Diagnosis cleanup**
- `FP_READ` / `FP_WRITE` / `FP_PREFILL` DbgPrint → rate-limited or #ifdef
- `FP_STARTUP_HEADROOM_MS`, `FramePipePrefillSilence` — remove (timer owns cadence, prefill unnecessary)

**P6-6 — GCD linear-interp SRC** *(Principle 6)*
- Replace current rate-mismatch drop with resampling
- Enables arbitrary sample-rate input

**P6-7 — Phase 6 commit + phase-number consolidation**
- Changelog entry
- Update `CURRENT_STATE.md` (this file)
- Update memory `project_remaining_tasks.md`

### Phase 6 keystone

**P6-1 (cadence timer)** is the keystone. Every other step is straightforward once it works. If the cadence timer goes in cleanly and live-call chopping disappears, the rest of Phase 6 is mechanical cleanup + SRC addition.

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
