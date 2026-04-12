# Pipeline V2 Changelog

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
