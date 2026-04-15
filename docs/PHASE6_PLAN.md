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

    ULONG          FramesPerEvent;     // primary transport unit
    ULONG          BytesPerEvent;      // derived: FramesPerEvent * BlockAlign
    LONGLONG       EventPeriodQpc;     // derived from FramesPerEvent and SampleRate
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

## Latency Unit Policy — **samples only, never ms**

> 규범: 드라이버/ControlPanel/Registry/IOCTL/test harness 어디에서도 latency 값으로 ms를 쓰지 않는다. 언제 어디서나 **samples (frames)** 만 쓴다.

### 엄격 규칙

1. **Primary storage unit = samples (frames)**. 모든 latency/cushion/period 필드는 frame count로 저장된다. `FramesPerEvent`, `StartupTargetFrames`, `StartupThresholdFrames`, `MinHeadroomFrames` 등.
2. **QPC/bytes는 파생값**. `EventPeriodQpc = FramesPerEvent × qpc_freq / SampleRate`, `BytesPerEvent = FramesPerEvent × BlockAlign`. 저장할지 캐싱할지는 구현 자유.
3. **ms는 어디에도 저장되지 않는다**. 구조체 필드 금지. 상수 이름에 ms 금지. 주석에 "20 ms @ 48k" 같은 설명이 들어가는 건 OK지만 **값 자체가 ms로 저장되는 필드는 존재 금지**.
4. **Registry / ControlPanel / IOCTL 구성값도 samples**. 사용자 입력도 samples로 직접 받는다. 이전 ms 기반 설정이 있다면 마이그레이션 단계에서 1회 frames로 변환 후 그 값으로만 저장.
5. **non-48k 정밀도 보존**. 44.1k / 96k / 192k 등에서 ms로 표현하면 정수로 안 떨어지는 값도 samples로 저장하면 그대로 보존. 예: 20 ms @ 44.1k = 882 samples (정수). samples를 primary로 두면 round-trip 손실 zero.
6. **Bug A 재발 방지**. Bug A는 `bytes = ms_rounded × sample_rate × blockAlign / 1000` 식의 ms quantization이 원인이었다. Phase 6 core에는 `elapsed_qpc → frames` 직통 경로만 존재하고 ms quantization step 자체가 없으므로 구조적으로 같은 종류의 오차가 재발할 수 없다.
7. **로그/표시도 frames 우선**. 사람이 읽는 로그도 가능한 한 samples 단위. 필요하면 표시 시점에 한 번 `frames × 1000 / SampleRate` 를 계산해서 `(≈X ms)` 보조 표기 추가 허용 — 단 저장값은 여전히 frames.

### 금지 리스트 (Phase 6 진입 시 제거/리네임 대상)

- `FP_STARTUP_HEADROOM_MS` 상수 → `FP_STARTUP_HEADROOM_FRAMES` 또는 제거
- `EventPeriodMs` 필드 (초안에 있었음, 이미 제거됨)
- `savedLatency` (ms 단위로 추정되는 이름) → `savedLatencyFrames`
- `latencyMs`, `targetFillMs` 같은 이름 가진 기존 변수/함수
- Registry key 중 ms 값이 있다면 frames 기반 key로 교체 (legacy key 유지 불가)

### 영향 범위 (Phase 6 작업 동안 같이 정리)

- `Source/Utilities/loopback.cpp` / `loopback.h` — 모든 latency 상수/필드 frames 기반으로
- `Source/Main/adapter.cpp` — registry load/store 경로 frames로 교체
- `Source/Main/ioctl.h` — 공유 구조체 latency 필드 frames로
- `Source/ControlPanel/main.cpp` — 사용자 UI 단위 samples로 교체
- `tests/` — 테스트 구성값도 frames 기반

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

- deterministic local-in-Chrome repro shows no first-4-second 20ms stale replay signature
  - practical gate: `AC[20ms] @ start+2s <= 0.20`

- local loopback에서 VB 수준 clean
- Phone Link live call에서 phone chopping 없음
- Cable B mic read cadence가 1ms여도 steady-state underrun 없음
- startup 첫 구간이 잘리지 않음
- `GetPositions` 호출 빈도 변화가 transport 품질에 영향 없음

## One-Line Definition

**Phase 6은 AO 위 patch가 아니라, VB-equivalent shared-timer transport core replacement이다.**

---

## Next follow-up (pending)

## Addendum — Working Baseline

- `439bbcd` is the effective last-known-good baseline for the phone path.
- `2c733f1` remains in history as the archived failed Phase 5 attempt.
- Phase 6 starts from the Phase 4 baseline semantics, not from the Phase 5 transport hook.

## Addendum — File-Level Breakdown (authoritative)

This addendum pins the Phase 6 skeleton to the current AO file layout so Step 1
implementation can begin without re-deciding ownership.

### 1. New files

Create a dedicated transport engine pair:

- `Source/Utilities/transport_engine.h`
- `Source/Utilities/transport_engine.cpp`

Reason:

- `loopback.cpp` remains the home of `FRAME_PIPE` ring semantics
- `minwavertstream.cpp` remains stream-facing WaveRT glue
- the shared timer engine is a third concern and should not be hidden inside
  either ring code or stream code

### 2. What stays in `loopback.cpp/.h`

Keep `FRAME_PIPE` here, including:

- ring storage and indices
- normalize / denormalize
- channel mapping
- `FramePipeWriteFromDma`
- `FramePipeReadToDma` and partial-read helpers
- fill / drop / underrun accounting
- format registration

Do not place the shared transport scheduler or timer callback here.

### 3. What goes in `transport_engine.cpp/.h`

This file pair owns:

- `AO_TRANSPORT_ENGINE`
- `AO_STREAM_RT`
- global shared timer creation / destruction
- active stream list management
- timer callback
- due-stream scheduling
- render event runner
- capture event runner
- startup arm / re-arm policy

Suggested public surface:

```c
NTSTATUS AoTransportEngineInit();
VOID     AoTransportEngineCleanup();

NTSTATUS AoTransportRegisterStream(CMiniportWaveRTStream* stream);
VOID     AoTransportUnregisterStream(CMiniportWaveRTStream* stream);
VOID     AoTransportOnRun(CMiniportWaveRTStream* stream);
VOID     AoTransportOnPause(CMiniportWaveRTStream* stream);
VOID     AoTransportOnStop(CMiniportWaveRTStream* stream);
```

### 4. What changes in `adapter.cpp`

`adapter.cpp` is the correct place to create and destroy the engine because it
already owns global driver bring-up / teardown.

Add:

- engine init after global pipe init succeeds
- engine cleanup during adapter teardown / uninstall path

Do not hide engine creation inside first-stream-open lazy init unless later
stability work proves that necessary.

### 5. What changes in `minwavertstream.h`

Add a per-stream transport pointer to `CMiniportWaveRTStream`:

```c
AO_STREAM_RT* m_pTransportRt;
```

Ownership model:

- `CMiniportWaveRTStream` owns the pointer for its lifetime
- the engine active list holds non-owning references
- allocate once on first registration (or stream construction)
- free on stream destruction

Do not use a global map keyed by stream pointer unless later debugging proves
it is necessary. A direct stream-owned pointer is simpler and fits the current
architecture better.

### 6. What changes in `minwavertstream.cpp`

Use `SetState(RUN/PAUSE/STOP)` as the authoritative engine registration /
activation bridge.

Expected wiring:

- `RUN`
  - ensure `m_pTransportRt` exists
  - register / activate stream in engine
  - arm startup state for capture side
- `PAUSE`
  - mark stream inactive for scheduling
  - keep runtime object attached to stream
- `STOP`
  - unregister from active scheduling
  - reset transport-facing per-session state

Important:

- `GetPositions` and `UpdatePosition` must not move audio data
- they remain reporting / bookkeeping only

### 7. Locking rules

Do **not** allow ad hoc nested locking. The Phase 6 engine should use this
contract:

- `m_PositionSpinLock`: stream-local position bookkeeping only
- `EngineLock`: active stream list + scheduler metadata only
- `PipeLock`: ring read/write only

Preferred rule:

- never hold `EngineLock` while taking `PipeLock`
- never hold `m_PositionSpinLock` while taking `EngineLock`
- never hold `m_PositionSpinLock` while taking `PipeLock`

Implementation pattern:

1. timer callback takes `EngineLock`
2. snapshots due streams into a small local array
3. releases `EngineLock`
4. runs render/capture events per stream
5. each event may briefly take that stream's `PipeLock`

This snapshot-and-drop pattern is the default. If any future code needs a
different order, it must document the exception explicitly.

### 7.5. Unit discipline

Phase 6 stores transport and latency state in **frames**, not milliseconds.

Rules:

- `FramesPerEvent` is the authoritative event size
- `BytesPerEvent` is derived from `FramesPerEvent * BlockAlign`
- `EventPeriodQpc` is derived from `FramesPerEvent` and stream sample rate
- startup thresholds and headroom values are stored as frame counts
- milliseconds are allowed only at UI / registry / display boundaries and must
  be converted to frames immediately

Practical implication:

- remove `EventPeriodMs` from runtime state
- avoid any `elapsed_ms -> bytes` transport math in the engine
- keep scheduling and recovery math in frame/sample space end-to-end

### 8. Explicit non-goals for Step 1

Step 1 should **not** attempt to:

- port every existing diagnostic counter
- preserve Phase 5 rollback flags
- add Phone Link specific special-casing
- tune the final event size perfectly

Step 1 only needs:

- shared timer exists
- streams can register/unregister
- timer callback can schedule no-op or stub events safely

That is enough to validate the skeleton before data movement is switched over.

Codex는 이 skeleton을 현재 파일 구조(`loopback.cpp` / `minwavertstream.cpp` / `adapter.cpp`)에 어떻게 배치할지 파일 단위로 풀어줄 수 있다. Step 1 진입 전에 이 file-level 브레이크다운을 받아두는 것을 권장한다.
