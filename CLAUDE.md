# AO Virtual Cable - Claude Instructions (Fixed Pipe Rewrite)

> **Current state and roadmap: `docs/CURRENT_STATE.md`** — single source of truth for where we are (Phase 5c, about to start Phase 6) and what's next. Read this first if resuming cold.
>
> **Phase 6 authoritative plan: `docs/PHASE6_PLAN.md`** (Codex-owned). Do not act on Phase 6 design hints from elsewhere — this file is the source of truth for the VB-equivalent shared-timer transport core.

## Opinion Rule (엄수)

- 사용자가 실행을 지시해도, Claude에게 **더 나은 의견 또는 개선안이 있다면 반드시 실행 전에 먼저 말할 것**.
- 침묵하고 시키는 대로만 실행하지 말 것. 개선 여지가 보이면 짧게라도 대안을 제시한 뒤, 사용자의 판단을 받고 진행.
- 개선안이 없다면 바로 진행해도 됨. 즉 이 규칙은 "의견이 있을 때 감추지 말라"는 규칙이지, 매번 의견을 억지로 만들라는 규칙이 아님.
- 적용 범위: 설계, 계측, 수정, 빌드, 설치, 테스트, commit, 어떤 단계든.

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

## Pre-Experiment Cable Check (엄수)

라이브콜 실험 전 **매번** 시스템 기본장치가 AO Cable A/B인지 확인할 것. install.ps1의 restore 경로가 이전 세션의 default device(보통 VB-Audio)를 복원해버려 사용자도 모르게 VB 경로로 실험이 돌아가는 사고가 실제로 발생했음.

확인해야 할 것:
- **기본 재생장치** = "스피커 (AO Cable A)" — VB Cable A 아님
- **기본 녹음장치** = "마이크 배열 (AO Cable B)" — VB Cable B 아님
- 둘 다 AO가 아니면 실험 결과는 AO 드라이버가 아닌 VB baseline을 측정한 것이므로 **무효**

확인 도구:
```powershell
# 빠른 체크
Get-CimInstance -ClassName Win32_PnPEntity | Where-Object { $_.Name -like "*Cable*" -and $_.Status -eq "OK" }
# 또는 설정 실행
powershell -File C:\Users\jongw\AppData\Local\Temp\set_ao_by_name.ps1
```

install 직후에는 반드시 `set_ao_by_name.ps1` 실행. run 시작 전 로그에 AO Cable A/B default 전환이 기록되는지 확인.

## Experiment Commit Rule (엄수)

모든 실험은 진행하면서 commit 한다. 나중에 어느 phase/commit에서 어떤 결과가 나왔는지 찾아야 하므로:

- **Source/test/config 변경은 실험 단위로 commit** — batch하지 말고 한 실험 끝나면 바로 commit
- **실험 결과 파일명에 commit 식별자 붙이기**: `<phase>_<shorthash>_<test>_<point>.<ext>`
  - 예: `phase5c_wip_run1_A_spk.wav` (commit이 없는 WIP은 `wip`, 또는 `<phase>_<shorthash>_...`)
- **결과 파일도 commit** — wav/log/md 전부 repo에 포함 (대용량 우려 시 사용자에게 먼저 확인)
- **memory에도 요약**: `project_remaining_tasks.md`에 phase별 결과 섹션 추가 (어느 commit에서 어떤 판정을 받았는지)
- **git merge policy 병행**: 브랜치 생성/merge/rebase/push는 여전히 사전 보고 필요. 단순 commit은 이 규칙 아래서 proactive로 진행 가능.

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
