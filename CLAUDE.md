# AO Virtual Cable - Claude Instructions (Fixed Pipe Rewrite)

## Current Branching Rule

- Current implementation branch: `feature/ao-fixed-pipe-rewrite`
- Stable baseline: `main` @ b856d94
- Research reference: `feature/ao-pipeline-v2` @ 416ad22 (frozen, do not modify)
- `feature/ao-telephony-passthrough-v1` is history only

## Build

```powershell
.\build-verify.ps1 -Config Release
```

## Driver Upgrade

- Use `.\install.ps1 -Action upgrade` (NO `-AutoReboot` flag)
- The no-reboot quiesce path (PREPARE_UNLOAD) is already implemented
- Never use `-AutoReboot` unless explicitly asked

## Real-Time Call Quality Test

Live call test harness: `tests/live_call/`

### Setup (once)
```powershell
cd tests/live_call
cp .env.example .env   # set OPENAI_API_KEY, PHONE_LINK_DEVICE_ID
pip install -r requirements.txt
```

`.env` 필수 설정:
- `OPENAI_API_KEY` — OpenAI API key
- `PHONE_LINK_DEVICE_ID` — Phone Link 연결 디바이스 ID (DeviceMetadataStorage.json에서 확인)
- `PHONE_LINK_DIAL_MODE=hidden_uri_only` — hidden URI 다이얼 방식
- `AMD_ENABLED=false` — 테스트 시 응답기 감지 비활성화

### Cable 전환
`.env`에서 `AUDIO_CABLE_PROFILE=ao` 또는 `AUDIO_CABLE_PROFILE=vb`

### Claude가 테스트 실행하는 방법
```powershell
cd tests/live_call && python run_test_call.py
```

자동 흐름:
1. 기본장치를 Cable A/B로 전환 (+ 다른 앱은 원래 장치로 역라우팅)
2. Phone Link hidden URI로 테스트번호(01058289554) 발신
3. 8초 대기 (상대방 수신 대기)
4. TTS 직접 재생 테스트 (24k→48k 업샘플, Cable B에 직접 write)
5. OpenAI Realtime API로 AI 대화 시작
6. silence timeout 또는 max turns로 자동 종료
7. 기본장치 복원

### 유저 역할
전화 받고 통화 품질 보고: **clean / garbled / silent**

### Claude 모니터링
- `tests/live_call/runtime_logs/test_call.log` — 전체 로그
- conversation log 확인 (USER 전사 정확도, AI 응답)
- `test_stream_monitor.py` — 드라이버 레벨 진단 (병행 실행)

### 현재 확인된 상태 (2026-04-12)
- VB-Cable: 깨끗함 (TTS/AI 모두 정상, 전사 정확)
- AO Cable: 왜곡 심함 (동일 경로, 드라이버 품질 문제 확정)

## Key Files

- `Source/Utilities/loopback.h` / `loopback.cpp` — ring buffer, format conversion, passthrough logic
- `Source/Main/minwavertstream.cpp` — WaveRT stream state, DPC, timer, position tracking
- `Source/Main/adapter.cpp` — IOCTL handlers, driver init
- `Source/Main/ioctl.h` — shared status/config structures
- `Source/ControlPanel/main.cpp` — control panel behavior and exposed driver settings
- `test_stream_monitor.py` — live stream diagnostics
- `tests/live_call/run_test_call.py` — one-command live call quality test
- `tests/live_call/audio_router.py` — system default device switching for call routing

## VB-Cable Reference (from reverse engineering)

- `results/vbcable_pipeline_analysis.md` — full pipeline: DPC, SRC, ring, position
- `results/vbcable_disasm_analysis.md` — SRC algorithm, ring struct layout
- `results/ghidra_decompile/vbcable_all_functions.c` — complete decompile (12096 lines)
- `docs/V2_RESEARCH_INDEX.md` — all research assets index

## Diagnostics Rule

When changing stream-status diagnostics, update these together:

- `Source/Main/ioctl.h`
- `Source/Main/adapter.cpp`
- `test_stream_monitor.py`

Do not trust hardcoded struct offsets without re-verifying layout.

## Changelog Rule

All code changes must be logged in:
- `docs/PIPELINE_V2_CHANGELOG.md`

Rules:
- Write the entry BEFORE or IMMEDIATELY AFTER the code edit
- Include: date, changed file(s), what changed, why
- No exceptions — even small one-line fixes get logged

## Fixed Pipe Rewrite Design Principles

Based on VB-Cable reverse engineering findings:

1. **INT32 ring buffer** — 4 bytes/sample, ~19-bit normalized (not packed 24-bit)
2. **Frame-indexed** — ring positions in frames, not bytes
3. **Hard reject on overflow** — increment counter and return error, never silent overwrite
4. **DMA → scratch → ring** — linearize DMA circular region before processing
5. **Single SRC function** — direction flag for write/read, symmetric behavior
6. **Linear interpolation SRC** — GCD ratio, stable first; sinc optimization later
7. **No MicSink dual-write** — ring is the sole data path
8. **Position on-query** — recalculate to current QPC in position handler
9. **8-frame minimum gate** — skip sub-sample noise
10. **KeFlushQueuedDpcs on Pause** — guarantee no DPC in-flight before ring reset
