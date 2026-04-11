# AO Telephony Raw Passthrough V1 - Change Log

Branch: `feature/ao-telephony-passthrough-v1`

## Test Procedure

실시간 스트림 통화 품질 테스트 시:
- 클로드가 아웃바운드 서버(server/main.py), 에이전트(agent/main.py) 기동
- 캠페인 시작 및 테스트번호 01058289554 상태 확인
- 다른 상태면 초기화 후 세션 진행
- 통화 중 stream monitor, 서버 로그, dump 파일 전부 클로드가 확인
- 사용자는 상대방 전화기로 전화만 받아서 통화 품질만 판정 (clean / garbled / silent)

---

## Commits (newest first)

### 2ebaebb+ - Re-enable MicSink direct push + position catch-up for both sinks
**File:** `Source/Main/minwavertstream.cpp`
**Changes:**
- WriteBytes capture: MicSink direct push 다시 활성화 (FormatMatch path)
- UpdatePosition: MicSink에도 one-time position catch-up 추가 (TelMicSink과 동일 로직)
  - `spkWritten==0 && m_ullLinearPosition>0`일 때 TotalBytesWritten과 WritePos를 mic 위치로 동기화
**Reason:** ring buffer만으로는 무음 (LoopbackRead가 데이터를 못 읽음). MicSink direct push를 다시 켜되 position 동기화 문제를 catch-up으로 해결.
**Status:** FormatMatch=speaker==mic 완화 + MicSink direct push + position catch-up

### bcd5a04+ - EXPERIMENT: Disable MicSink direct push, force ring buffer path (REVERTED)
**Files:** `Source/Main/minwavertstream.cpp` (WriteBytes capture, ~line 1764), `test_stream_monitor.py`
**Change:** TelMicSink direct push와 MicSink direct push 둘 다 `if (FALSE)`로 비활성화. Mic WriteBytes는 항상 ring buffer에서 LoopbackRead/LoopbackReadConverted로 읽음.
**Purpose:** MicSink position 동기화 문제를 우회. Ring buffer 경유 시 왜곡이 사라지는지 확인.
**판정:** 깨끗 → MicSink direct push의 position accounting이 범인. 나쁨 → ring buffer read 타이밍 문제.
**Monitor:** MS(MicSink.Active) 필드 다시 추가

### bcd5a04 - EXPERIMENT: FormatMatch speaker==mic only (bypass internal check)
**File:** `Source/Utilities/loopback.cpp` (UpdateFormatMatch, line ~498)
**Change:** speaker+mic 양쪽 active일 때 FormatMatch 조건에서 `FormatMatchesInternal()` 제거. speaker==mic same format이면 FormatMatch=TRUE → LoopbackWrite raw copy path.
**Purpose:** converted path(2ch→8ch→2ch)를 완전 건너뛰고 raw copy로 가면 왜곡이 사라지는지 확인.
**판정 기준:**
- 깨끗해짐 → converted/internal ring 경로가 범인
- 안 좋음 유지 → WaveRT timer/DPC/position이 근본 원인
**Note:** telephony raw는 여전히 disabled (IsTelephonyFormat returns FALSE)

### (pending) - Generic path fix + narrow raw experiment
**Files:** `loopback.cpp`, `loopback.h`, `ioctl.h`, `adapter.cpp`, `test_stream_monitor.py`
**Changes:**
1. **Same-rate +1 frame fix** (loopback.cpp LoopbackReadConverted ~line 1276):
   - `internalFrames += 1` now only applies when `internalRate != micFmt.SampleRate`
   - Same-rate (48k->48k) no longer over-consumes ring by 1 frame per DPC tick
2. **Converted path counters** (loopback.h, loopback.cpp, ioctl.h, adapter.cpp):
   - `ConvWriteOverflowCount`: ring overflow (oldest data overwritten)
   - `ConvReadShortfallCount`: ring didn't have enough data for requested read
   - Reset in LoopbackReset, exposed via GET_STREAM_STATUS IOCTL
3. **Narrow raw-path experiment** (loopback.cpp UpdateFormatMatch):
   - 48k/16/2 PCM same-format: FormatMatch=TRUE (raw LoopbackWrite/Read)
   - All other formats: standard FormatMatchesInternal check preserved
   - IsTelephonyFormat() restored to actual check (was returning FALSE)
   - TelephonyRaw path still disabled (FormatMatch takes priority over TelephonyRaw)
4. **C_ASSERT** sizeof(LOOPBACK_BUFFER) = 1352

### (pending) - Disable telephony raw, focus on generic path baseline
**Files:**
- `Source/Utilities/loopback.cpp` — IsTelephonyFormat() returns FALSE unconditionally
- `test_stream_monitor.py` — removed broken SpkW/MicEnd parsing, simplified display
**Reason:** telephony raw ON/OFF 둘 다 통화 품질 나쁨 → 공통 원인은 generic WaveRT path. raw 디버깅보다 baseline 복구가 우선.
**Status:** telephony raw는 버리는 게 아니라, generic path 수정 후 다시 켜는 것.

### d7e5270+ - Fix stream monitor IOCTL buffer too small
**File:** `test_stream_monitor.py` (line 60)
**Problem:** buf_size=128 but AO_STREAM_STATUS grew with new diagnostic fields → IOCTL returns STATUS_BUFFER_TOO_SMALL → "IOCTL failed"
**Fix:** buf_size 128 → 256

### 40f934a - Add SpkWritten/MicEnd/WritePos diagnostics
**Files:** `Source/Utilities/loopback.h`, `Source/Main/minwavertstream.cpp`, `Source/Main/ioctl.h`, `Source/Main/adapter.cpp`, `test_stream_monitor.py`
**Purpose:** ZeroFill 폭증 원인 분석을 위해 비교식 양쪽 값을 IOCTL에 노출
**추가 필드 (loopback.h):**
- `TelMicTotalWritten` (ULONGLONG) — TelMicSink.TotalBytesWritten snapshot
- `LastMicEndBytes` (ULONGLONG) — micEnd = m_ullLinearPosition + ByteDisplacement
- `TelMicWritePosDiag` (ULONG) — TelMicSink.WritePos snapshot
**수정 위치:** UpdatePosition()에서 zero-fill 비교 직전에 snapshot 저장
**IOCTL:** AO_STREAM_STATUS에 CableA/B 각각 3필드 추가
**Monitor 출력:** `SpkW=xxx MicEnd=xxx gap=xxx WPos=xxx` 형태로 표시
**C_ASSERT:** sizeof(LOOPBACK_BUFFER) = 1344

### 4208866 - Fix CableA stash ordering + CableB position catch-up
**File:** `Source/Main/minwavertstream.cpp`

**CableA 버그 (stash 순서):**
- **문제:** Mic RUN 시 LoopbackRegisterFormat()이 MicDmaStash 저장보다 먼저 호출됨. RegisterFormat 안의 UpdateTelephonyRawState()가 stash를 확인할 때 아직 NULL → TelMicSink 자동 등록 실패 → CableA TelRaw=YES인데 MicSink=0
- **수정 위치:** KSSTATE_RUN 블록, RegisterFormat 호출 전
- **수정 내용:** `if (!isSpeaker && m_pDmaBuffer)` 블록을 RegisterFormat 호출 전으로 이동. stash를 먼저 저장한 뒤 RegisterFormat 호출
- **제거:** 아래쪽 Mic RUN 블록의 중복 stash 저장 코드 제거. FormatMatch MicSink 등록만 남김

**CableB 버그 (position catch-up):**
- **문제:** 이전 catch-up 코드가 `spkWritten < m_ullLinearPosition` 조건으로 매 DPC tick마다 실행됨. Speaker가 push해서 TotalBytesWritten을 올려도 다음 tick에서 다시 m_ullLinearPosition으로 리셋 → push된 데이터 무시 → ZeroFill 계속 폭증
- **수정 위치:** UpdatePosition, TelMicSink.Active 분기 (~line 1609)
- **수정 내용:** catch-up 조건을 `spkWritten == 0 && m_ullLinearPosition > 0`으로 변경. 한 번만 실행되고, 이후 Speaker push가 정상 카운팅됨

### 6b0640b - Fix TelMicSink position catch-up (superseded by 4208866)
**File:** `Source/Main/minwavertstream.cpp` (UpdatePosition, ~line 1609)
**Problem:** TelMicSink 등록 시 TotalBytesWritten=0으로 시작하지만, Mic의 m_ullLinearPosition은 이미 진행되어 있음. 결과: `micEnd > spkWritten`이 항상 true → ZeroFill 폭증 → 상대방에게 AI 목소리 안 들림.
**Fix:** UpdatePosition에서 `spkWritten < m_ullLinearPosition`이면 TotalBytesWritten과 WritePos를 Mic 현재 위치로 catch up.
**Note:** 이 수정은 매 tick 반복 실행 문제가 있어 4208866에서 one-time 조건으로 개선됨.

### 585f0ce - Fix ActiveRenderCount leak
**Files:** `Source/Main/minwavertstream.cpp`, `Source/Utilities/loopback.cpp`
**Problem:** Speaker PAUSE 시 ActiveRenderCount--가 없어서 RUN→PAUSE→RUN 반복 시 count 누수. RC>1이면 telephony raw 조건 깨짐.
**Fix:** Speaker PAUSE에 count-- 추가. telephony 조건을 `==1` → `>=1`로 완화.

### 214f2a0 - Fix downlink mute regression (speaker PAUSE teardown)
**File:** `Source/Main/minwavertstream.cpp` (KSSTATE_PAUSE)
**Problem:** Codex가 Speaker PAUSE에서 TelephonyRawActive=FALSE + TelMicSink.Active=FALSE를 직접 설정. Phone Link이 PAUSE→RUN 전환 시 raw path 영구 비활성.
**Fix:** Speaker PAUSE에서 telephony state를 건드리지 않음. TelephonyRawActive는 UpdateTelephonyRawState()에서만 관리.

### 5295f4e - Fix TelMicSink registration race
**File:** `Source/Main/minwavertstream.cpp` (Capture KSSTATE_RUN)
**Problem:** Mic RUN 시점에 TelephonyRawActive가 아직 FALSE → TelMicSink 등록 안 됨. UpdateTelephonyRawState의 stash 자동 등록에 의존해야 하는데, minwavertstream에서 직접 등록 시도가 race 유발.
**Fix:** Mic RUN에서 TelMicSink 직접 등록 제거. FormatMatch 기존 MicSink만 조건부 등록. 자동 등록은 UpdateTelephonyRawState에 맡김.

### 0d67a4f - Add diagnostic counters to stream status IOCTL
**Files:** `Source/Main/ioctl.h`, `Source/Main/adapter.cpp`, `Source/Utilities/loopback.h`, `Source/Utilities/loopback.cpp`
**Added:** ActiveRenderCount, TelMicSink.Active, TelRawDataCount를 GET_STREAM_STATUS IOCTL에 추가.

### 52c5a45 - Add TelRawActive + ZeroFill + Underrun counters
**Files:** `Source/Main/ioctl.h`, `Source/Main/adapter.cpp`, `Source/Utilities/loopback.h`, `Source/Utilities/loopback.cpp`, `test_stream_monitor.py`
**Added:** TelephonyRawActive, ZeroFillCount, UnderrunCount를 IOCTL과 stream monitor에 노출.

### 0a8985d - Initial telephony raw passthrough implementation
**Files:** `Source/Utilities/loopback.h`, `Source/Utilities/loopback.cpp`, `Source/Main/adapter.cpp`, `Source/Main/minwavertstream.cpp`
**Added:** 
- TelRawBuffer (768KB 전용 ring buffer)
- TelMicSink (direct DMA push)
- IsTelephonyFormat (48k/16/2/PCM 조건)
- UpdateTelephonyRawState (5조건 활성화)
- TelephonyRawWrite/Read (raw ring + DMA push)
- LoopbackWriteConverted/ReadConverted telephony 분기
- ActiveRenderCount 관리

---

## Key Architecture

```
Telephony raw path (48k/16bit/2ch same-format):

  Speaker DPC WriteBytes (render)
    → LoopbackWriteConverted
      → TelephonyRawActive? YES
        → TelephonyRawWrite
          → TelRawBuffer (ring write)
          → TelMicSink DMA (direct push)

  Mic DPC UpdatePosition (capture)
    → TelMicSink.Active? YES
      → compare TelMicSink.TotalBytesWritten vs m_ullLinearPosition
      → zero-fill gap if mic ahead of speaker

  Mic DPC WriteBytes (capture)
    → TelMicSink.Active && TelephonyRawActive? YES
      → no copy (data already in DMA from Speaker push)

Activation conditions (all must be true):
  1. SpeakerActive
  2. MicActive
  3. IsTelephonyFormat(SpeakerFormat) = 48k/16/2/PCM
  4. IsTelephonyFormat(MicFormat) = 48k/16/2/PCM
  5. ActiveRenderCount >= 1
  6. TelRawBuffer != NULL
```

---

## Known Issues (as of latest commit)

1. **ZeroFill 폭증** - position catch-up 수정으로 완화 기대, 실통화 테스트 필요
2. **CableA MicSink=0** - CableA에서 TelMicSink 자동 등록이 안 되는 경우 있음
3. **Converted path 지지직** - telephony raw 미적용 시 기존 8ch/24bit 변환 품질 문제 잔존
