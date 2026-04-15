# Phase 6 — VB-equivalent Core Replacement

> **Authoritative Phase 6 plan. Source: Codex (2026-04-15).**
> Per CLAUDE.md ordering, this plan takes precedence over Claude drafts if they conflict.
> Live status and step progress live in `docs/CURRENT_STATE.md`.

## 목표

기존 AO transport를 계속 보수하지 않는다.
`439bbcd`를 작업 베이스로 두고, 그 위에 **VB-equivalent transport core**를 새로 올린다.

이 Phase의 목표는 다음과 같다.

1. 드라이버가 transport cadence를 완전히 소유한다
2. render/capture transport를 query-driven이 아니라 **event-driven**으로 바꾼다
3. startup / underrun / recovery를 명시적인 상태 기계로 관리한다
4. position reporting과 transport를 분리한다
5. 기존 AO 스타일 기능은 코어가 안정화된 뒤 위에 다시 입힌다

## 폐기 대상

Phase 6에서는 아래 접근을 코어에서 제거한다.

- `GetPositions` 기반 transport
- `UpdatePosition` 기반 transport
- per-call time math로 write/read 양을 결정하는 방식
- Phase 5 render ownership hook
- Phase 5 rollback/scaffold를 코어 설계의 전제로 두는 방식

즉, **position query가 transport를 drive하는 구조를 완전히 버린다**.

---

## Core Model

### 1. Global Engine

드라이버 전역에 high-resolution shared timer 1개를 둔다.

```c
typedef struct _AO_TRANSPORT_ENGINE {
    EX_TIMER*      Timer;
    KSPIN_LOCK     Lock;
    BOOLEAN        Running;
    LONGLONG       PeriodQpc;          // default 20ms
    LONGLONG       NextTickQpc;
    ULONG          ActiveStreamCount;
    LIST_ENTRY     ActiveStreams;      // all active render/capture streams
} AO_TRANSPORT_ENGINE;
```

**핵심 규칙:**

- timer는 전역 1개
- 모든 active stream은 같은 tick 안에서 처리
- stream별 독립 timer 금지
- render/capture가 같은 tick 경계 안에서 순차 실행

### 2. Per-Stream Runtime

각 stream은 transport용 runtime state를 가진다.

```c
typedef struct _AO_STREAM_RT {
    LIST_ENTRY     Link;

    CMiniportWaveRTStream* Stream;
    BOOLEAN        IsCapture;
    BOOLEAN        IsCable;
    BOOLEAN        IsSpeakerSide;
    BOOLEAN        Active;

    ULONG          SampleRate;
    ULONG          Channels;
    ULONG          BlockAlign;

    ULONG          FramesPerEvent;
    ULONG          BytesPerEvent;
    ULONG          EventPeriodMs;      // default 20
    LONGLONG       EventPeriodQpc;
    LONGLONG       NextEventQpc;

    BOOLEAN        StartupArmed;
    ULONG          StartupTargetFrames;
    ULONG          StartupThresholdFrames;
    ULONG          MinHeadroomFrames;

    ULONG          LateEventCount;
    ULONG          UnderrunEvents;
    ULONG          DropEvents;

    ULONG          DmaOffset;          // current DMA-side transport offset
    ULONG          CarryFrames;        // non-integer rate/period carry
} AO_STREAM_RT;
```

**핵심 규칙:**

- stream마다 `next_event_qpc` 보유
- event마다 처리할 frame 수는 고정
- 44.1k 등 비정수는 `CarryFrames`로 보정
- render/capture 모두 동일한 event contract 사용

## Scheduling Model

### 3. Timer Callback

전역 timer callback은 stream 배열/리스트를 순회한다.

```c
VOID AoTransportTimerCallback(...)
{
    qpc = KeQueryPerformanceCounter(NULL);

    Acquire(engine->Lock);

    for each stream in engine->ActiveStreams:
        if (!stream->Active)
            continue;

        if (qpc < stream->NextEventQpc)
            continue;

        if (stream->IsCapture)
            AoRunCaptureEvent(stream, qpc);
        else
            AoRunRenderEvent(stream, qpc);

        stream->NextEventQpc += stream->EventPeriodQpc;

    Release(engine->Lock);
}
```

**핵심 규칙:**

- timer tick은 단지 "검사 펄스"
- 실제 transport는 `now >= next_event_qpc`인 stream만 수행
- 같은 callback에서 render/capture 모두 처리
- 같은 cable pair에서는 render를 먼저, capture를 나중에 처리해 위상차를 최소화

## Transport Paths

### 4. Render Event

render event는 DMA에서 pipe로 고정 chunk를 밀어넣는다.

```c
VOID AoRunRenderEvent(AO_STREAM_RT* rt, LONGLONG qpc)
{
    frames = AoComputeFramesForEvent(rt);   // fixed + carry
    bytes  = frames * rt->BlockAlign;

    AoLinearizeDmaIfWrapped(rt->Stream, rt->DmaOffset, bytes, scratch);
    FramePipeWriteFromDma(rt->Pipe, scratch, bytes);

    rt->DmaOffset = (rt->DmaOffset + bytes) % rt->Stream->m_ulDmaBufferSize;
}
```

**규칙:**

- event마다 고정 frame 수 처리
- query rate와 무관
- DMA wrap은 scratch로 선형화
- overflow는 reject + counter

### 5. Capture Event

capture event는 pipe에서 읽어 mic DMA에 고정 chunk를 채운다.

```c
VOID AoRunCaptureEvent(AO_STREAM_RT* rt, LONGLONG qpc)
{
    frames = AoComputeFramesForEvent(rt);
    bytes  = frames * rt->BlockAlign;

    if (rt->StartupArmed)
    {
        if (FramePipeGetFillFrames(rt->Pipe) < rt->StartupThresholdFrames)
        {
            AoWriteSilenceToMicDma(rt, bytes);
            return;
        }
        rt->StartupArmed = FALSE;
    }

    readFrames = FramePipeReadToDmaPartial(rt->Pipe,
                    rt->DmaBuffer + rt->DmaOffset, bytes);

    if (readFrames < frames)
    {
        AoZeroTail(rt, frames - readFrames);
        rt->UnderrunEvents++;
    }

    rt->DmaOffset = (rt->DmaOffset + bytes) % rt->Stream->m_ulDmaBufferSize;
}
```

**규칙:**

- startup arm/threshold는 capture side의 일급 개념
- underrun 시 전체 zero-fill이 아니라 partial-read + tail zero-fill
- Phone Link의 1ms read는 이미 준비된 DMA chunk를 읽게 됨

## Startup / Recovery

### 6. Startup State Machine

새 call / 새 mic session / speaker reopen 시 명시적으로 startup을 재무장한다.

**상태:**

- `StartupArmed = TRUE`
- `StartupThresholdFrames = 20~40ms 수준`
- threshold 도달 전까지 capture는 silence
- threshold 도달 후 steady-state 진입

**규칙:**

- `StartPhaseComplete` 같은 sticky one-shot 플래그에 의존하지 않는다
- session boundary마다 반드시 재무장
- startup cushion과 steady-state target fill은 분리한다

## Position Decoupling

### 7. Position Reporting

`GetPositions`와 `UpdatePosition`은 transport를 drive하지 않는다.

**역할:**

- linear/presentation position bookkeeping
- WaveRT packet accounting
- QPC timestamp reporting

**금지:**

- pipe write/read 호출
- transport chunk move
- query cadence에 transport를 묶는 모든 동작

즉:

- **position path = reporting only**
- **timer engine = transport only**

## Pipe Contract

### 8. FRAME_PIPE Role

FRAME_PIPE는 그대로 **"유일한 transport ring"** 으로 유지한다.

**유지할 것:**

- INT32 ring
- normalize/denormalize
- channel mapping
- hard reject overflow
- underrun counter
- format registration

**바꿀 것:**

- write/read를 호출 시점마다 즉흥적으로 하지 않음
- engine event가 pipe를 drive

## Implementation Order

### Step 1
- 전역 `AO_TRANSPORT_ENGINE` + `AO_STREAM_RT` 구조체 추가
- 동작 변화 없음

### Step 2
- stream RUN/PAUSE/STOP에서 engine register/unregister 추가
- 아직 legacy transport 유지

### Step 3
- render side를 engine event transport로 전환
- `GetPositions`/`UpdatePosition` transport 제거

### Step 4
- capture side를 engine event transport로 전환
- startup/recovery 적용

### Step 5
- position path decouple 완료
- legacy transport 완전 제거

### Step 6
- Phase 5 scaffold 정리
  - `IOCTL_AO_SET_PUMP_FEATURE_FLAGS` 제거
  - `RenderPumpDriveCount`/`LegacyDriveCount` 제거
  - query-driven pump helper 제거

## Success Criteria

- local loopback에서 VB 수준 clean
- Phone Link live call에서 phone chopping 없음
- Cable B mic read cadence가 1ms여도 steady-state underrun 없음
- startup 첫 구간이 잘리지 않음
- `GetPositions` 호출 빈도 변화가 transport 품질에 영향 없음

## One-Line Definition

**Phase 6은 AO 위 patch가 아니라, VB-equivalent shared-timer transport core replacement이다.**

---

## Next follow-up (pending)

Codex는 이 skeleton을 현재 파일 구조(`loopback.cpp` / `minwavertstream.cpp` / `adapter.cpp`)에 어떻게 배치할지 파일 단위로 풀어줄 수 있다. Step 1 진입 전에 이 file-level 브레이크다운을 받아두는 것을 권장한다.
