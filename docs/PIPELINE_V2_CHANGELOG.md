# Pipeline V2 Changelog

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
