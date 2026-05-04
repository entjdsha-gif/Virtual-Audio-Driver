# AO Virtual Cable - Claude Instructions (Fixed Pipe Rewrite)

> **Architecture (single source of truth): `docs/AO_FIXED_PIPE_ARCHITECTURE.md`** — read this before touching cable transport code. It supersedes all prior Phase 5/6 plans and Option Y/Z drafts.
>
> **Current state / next step: `docs/CURRENT_STATE.md`** — read this if resuming cold to know which stage is active.
>
> **Older plans:** `docs/archive/` and `results/archive/` (reference only, do not edit as live).

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

## Pre-Experiment Phone Link Connection Check (엄수)

라이브콜 실험 전 **매번** Phone Link가 실제로 휴대폰과 연결되어 있는지 확인할 것. Phone Link 앱이 떠있어도 폰쪽 "Windows와 연결" 토글이 꺼져 있으면 dial이 나가지 않고 실험 전체가 무효가 됨. 이번 세션 실제 사고:

- Phone Link 앱 자체는 PhoneExperienceHost PID 존재 + 메인 윈도우 있음
- 그런데 폰쪽 설정이 꺼져 있어 "모바일 장치에서 Windows와 연결이 꺼져 있음" 에러 화면 표시
- 현재 `phone_link_main_dialer_disconnected()` 헬퍼는 이 에러 화면을 miss함 (다른 automation id 사용)
- dialer가 hidden URI를 성공적으로 launch해도 Phone Link가 dial 요청을 무시함

확인 방법 (순서):
1. Phone Link 앱 열어서 "통화" 탭 상태 시각 확인 — 다이얼 패드가 정상인지, "모바일 장치에서 Windows와 연결이 꺼져 있음" 에러 화면인지
2. 에러 화면이면 폰 퀵세팅에서 "Windows와 연결" 토글 ON
3. Phone Link 앱이 dialer 상태로 복구된 것 확인
4. 그 다음에 run_test_call.py 실행

TODO: `phone_link_dialer.py`의 연결 감지 헬퍼를 강화해서 이 에러 화면도 잡도록 하고, `run_test_call.py` 시작 시 hard-gate로 실행. 현재까지는 수동 확인 엄수.

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

Full reasoning: `docs/AO_FIXED_PIPE_ARCHITECTURE.md` § 4. Summary:

1. **INT32 frame-indexed ring** — 4 bytes/sample, ~19-bit normalized (not packed 24-bit)
2. **Hard-reject on overflow** — increment counter, never silent overwrite
3. **Single SRC per direction** — linear interpolation, GCD divisor (300/100/75)
4. **DMA → scratch → ring** — linearize before processing
5. **No MicSink, no dual-write** — ring is sole data path
6. **Position recalculated on query** — every `GetPosition` invokes the canonical helper with current QPC
7. **Canonical cable advance helper** (`AoCableAdvanceByQpc`) — single owner of transport+accounting+freshness; all entry points (query, timer, packet) funnel into it
8. **8-frame minimum gate** — skip sub-sample noise
9. **Frame-only units** — bytes and QPC are derived; `ms` only in comments / UI / logs
10. **KeFlushQueuedDpcs before ring reset** — guarantees no in-flight DPC

Also mandatory inside the canonical helper (non-optional):

- 63/64-style drift correction
- DMA overrun guard (skip if computed advance > sampleRate / 2 frames)
- Scratch linearization step

If any of the 10 principles is violated by a proposed change, surface it in the Opinion Rule conversation before implementing.
