# Pipeline V2 Changelog

## 2026-04-12

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
