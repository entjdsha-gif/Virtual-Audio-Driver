# AO Virtual Cable — Fixed Frame Pipe Rewrite Plan

> **⚠️ HISTORICAL — See `docs/CURRENT_STATE.md` for the current roadmap.**
> This document captures the M1–M6 productization milestones on the byte-ring baseline.
> The audio pipeline has since transitioned to the INT32 frame pipe (Phase 1–5c) and
> is now moving toward a dedicated cadence timer (Phase 6). Process/install/control
> panel content below is still accurate; the architecture comparison sections are
> superseded by the Phase 5/5c work recorded in `PIPELINE_V2_CHANGELOG.md`.

> Date: 2026-04-12
> Active Branch: `feature/ao-fixed-pipe-rewrite`
> Rewrite Base: `main` @ `b856d94`
> Research Baseline: `feature/ao-pipeline-v2` @ `416ad22` (frozen, do not modify)
> Status: **Architecture confirmed — ready for implementation**

---

## Direction Summary

AO Virtual Cable의 통화 품질 문제 근본 원인: 현재 byte-ring 기반 transport는 오디오를
"바이트 스트림"으로 취급하며, overflow 시 oldest를 덮어쓰고, 내부 포맷 변환이 항상
개입하며, ring fill level이 latency를 결정하는 구조.

VB-Cable reverse engineering 결과, VB는 완전히 다른 철학을 사용:
**fixed latency frame pipe + passthrough-first**.

이 플랜은 AO의 내부 transport를 VB-style fixed frame pipe로 재설계한다.
V2 연구는 이 결론에 도달하기 위한 evidence trail이며, V2 코어를 계속 확장하지 않는다.

핵심 원칙:
1. **Frame pipe가 유일한 transport** — Mic DMA direct-push 없음
2. **No-SRC fast path** — rate 일치 시 SRC 생략. INT32 normalization과 channel mapping은 항상 수행 (VB-Cable 동일)
3. **Hard reject overflow** — 절대 oldest를 덮어쓰지 않음
4. **Fixed latency frames** — fill level이 아닌 고정 frame count가 latency의 source of truth
5. **Pipe format state machine** — format 결정/해제 시점이 명확한 규칙 기반
6. **Underrun = silence fill** — Mic DPC가 pipe에서 데이터 못 읽으면 DMA에 silence(0) 기록. 절대 skip 안 함
7. **Diagnostics는 별도 phase가 아닌 각 phase에 통합** — 디버깅 불가능한 코어는 만들지 않음

---

## Core Architecture Comparison

| 항목 | 현재 AO (byte-ring) | 목표 (fixed frame pipe) | VB-Cable (reference) |
|------|---------------------|------------------------|---------------------|
| 단위 | 바이트 | 프레임(sample) | 프레임(sample) |
| ring 크기 | 4초 고정 ring | LatencyFrames 기반 고정 | 7168 frames default |
| 내부 포맷 | packed 24-bit (3 bytes/sample) | INT32 (4 bytes/sample, ~19-bit) | INT32 (~19-bit normalized) |
| overflow | overwrite-oldest (ReadPos 점프) | hard reject + DropCount++ | hard reject + counter |
| underrun | 감지만, stale data 방치 | silence fill + UnderrunCount++ | silence/center fill |
| passthrough | FormatMatch flag → MicSink dual-write | no-SRC fast path (rate 일치 시 SRC 생략, INT32+ChMap은 항상) | same-rate → no SRC, INT32 항상 |
| latency | ring fill level 의존 | 고정 frame count 보장 | 고정 sample count |
| SRC | 항상 pipeline 내 (8-tap sinc) | mismatch일 때만 별도 path | GCD ratio 선형보간 |
| DMA 처리 | DMA에서 직접 처리 | DMA → scratch 선복사 후 처리 | DMA → scratch 선복사 |
| position | on-query QPC 재계산 (이미 구현) | 유지 | on-query QPC 재계산 |
| timer drift | 보정 없음 | 63/64 phase 보정 (**신규**) | 63/64 phase 보정 |
| min gate | 없음 | 8 frames 최소 (**신규**) | 8 frames 최소 |
| KeFlushQueuedDpcs | Pause 시 호출 (이미 구현) | 유지 | Pause 시 호출 |
| fill 제어 | 없음 (thin-fill) | StartThreshold + TargetFill (**신규**) | 8-frame gate + large capacity |
| Mic 전달 | MicSink direct DMA push | pipe read only | ring read only |

---

## Data Flow

### 현재 AO (문제)

```
Speaker DPC → [byte convert] → [SRC] → [4s byte ring] → [SRC] → [byte convert] → Mic DPC
                    항상 개입            overwrite-oldest           항상 개입
                                    + MicSink DMA direct-push (race condition)
```

### 목표 아키텍처

```
Speaker DPC → [DMA scratch] → [INT32 norm] → [ChMap to Nch] → [same-rate?]
                                                                    │
                                              YES (no SRC) ─────────┤────── NO (SRC needed)
                                                                    │            │
                                                                    ▼            ▼
                                                            [frame pipe]    [SRC] → [frame pipe]
                                                                    │            │
                                              ┌─────────────────────┘            │
                                              ▼                                  ▼
Mic DPC ← [denorm] ← [ChMap to Mch] ← [frame pipe read]   ← [SRC] ← [frame pipe read]
    │
    ▼
[Mic DMA copy]
```

**핵심:**
- **INT32 normalization + channel mapping은 항상 수행** (VB-Cable과 동일)
- "Fast path" = no SRC (rate 일치). "Fallback" = SRC 추가 (rate 불일치)
- Speaker DPC는 pipe에 write만 한다
- Mic DPC는 pipe에서 read한 후 자신의 DMA에 copy한다
- Mic DMA에 직접 push하는 경로는 없다

---

## Keep / Drop / Rewrite / Validate

### Keep (그대로 유지)

| 항목 | 파일 | 이유 |
|------|------|------|
| install.ps1, no-reboot upgrade | `install.ps1` | 전체 install flow 유지 |
| IOCTL_AO_PREPARE_UNLOAD | `adapter.cpp` | quiesce path 유지 |
| test_stream_monitor.py | `test_stream_monitor.py` | 진단 도구 (출력 포맷만 업데이트) |
| tests/live_call/* | `tests/live_call/` | 품질 하네스 전체 |
| IOCTL 인터페이스 | `ioctl.h` | 확장만, 기존 호환 유지 |
| Control panel app | `Source/ControlPanel/*` | 확장만 |
| RE 자료 | `results/`, `docs/VB_CABLE_PATH_ANALYSIS.md` | 참조용 |
| C_ASSERT layout guard 패턴 | 전체 | FRAME_PIPE용으로 갱신 |
| sinc_table.h | `Source/Utilities/sinc_table.h` | fallback SRC 경로에서 재사용 가능 |
| Spinlock 순서 규칙 | 전체 | PositionSpinLock → PipeLock 순서 유지 |
| **On-query position 재계산** | `minwavertstream.cpp:790,1079` | **이미 구현됨** — GetPosition/GetPositions에서 QPC→UpdatePosition 호출 |
| **KeFlushQueuedDpcs on Pause** | `minwavertstream.cpp:1318` | **이미 구현됨** — Pause 시 ExCancelTimer 후 KeFlushQueuedDpcs 호출 |
| **ActiveRenderCount** | `loopback.h:126` | multi-render 진단, FRAME_PIPE에 보존 |

### Drop (폐기)

| 항목 | 파일 | 이유 |
|------|------|------|
| LOOPBACK_BUFFER 4초 byte ring | `loopback.h` | frame pipe로 교체 |
| DataCount / WritePos / ReadPos (byte) | `loopback.h` | frame index로 교체 |
| LoopbackWrite() / LoopbackRead() | `loopback.cpp` | frame 단위 함수로 교체 |
| Overflow overwrite-oldest 정책 | `loopback.cpp` | hard reject로 교체 |
| **LOOPBACK_MIC_SINK 구조 전체** | `loopback.h/cpp` | **pipe가 유일한 transport** |
| MicDmaStash / deferred activation | `loopback.cpp` | MicSink와 함께 폐기 |
| CalcBufferSize() 4초 safety margin | `loopback.cpp` | LatencyFrames 기반으로 교체 |
| ConvertToInternal/ConvertFromInternal packed-24 path | `loopback.cpp` | INT32 path로 교체 |
| Speaker DPC → Mic DMA direct-push | `minwavertstream.cpp` | pipe read가 유일한 Mic 경로 |
| g_CableALoopback / g_CableBLoopback | `loopback.h` | FRAME_PIPE로 교체 |

### Rewrite (새로 작성)

| 항목 | 파일 | 내용 |
|------|------|------|
| FRAME_PIPE 구조체 | `loopback.h` | frame 단위 pipe, INT32 ring |
| FramePipeInit/Cleanup | `loopback.cpp` | pipe 할당/해제 |
| FramePipeWriteFrames | `loopback.cpp` | Speaker: scratch → pipe write |
| FramePipeReadFrames | `loopback.cpp` | Mic: pipe read → scratch |
| FramePipeWriteConverted | `loopback.cpp` | SRC fallback write path |
| FramePipeReadConverted | `loopback.cpp` | SRC fallback read path |
| FramePipeRegisterFormat | `loopback.cpp` | format state machine |
| FramePipeReset | `loopback.cpp` | stream restart |
| ReadBytes/WriteBytes | `minwavertstream.cpp` | pipe API 호출로 전환 |
| UpdatePosition | `minwavertstream.cpp` | frame 단위 position 추적 |
| Timer drift correction | `minwavertstream.cpp` | 63/64 phase 보정 (**신규**) |
| Min gate (8 frames) | `minwavertstream.cpp` | 작은 tick skip |
| DMA scratch copy | `minwavertstream.cpp` | DMA → linear scratch |
| INT32 normalization | `loopback.cpp` | ~19-bit range conversion |
| AO_STREAM_STATUS 확장 | `ioctl.h` | pipe 상태 필드 |
| Monitor 출력 포맷 | `test_stream_monitor.py` | pipe 진단 표시 |

### Validate (검증에 활용)

| 검증 항목 | 도구/방법 | 통과 기준 |
|-----------|-----------|-----------|
| Pipe fill level | `test_stream_monitor.py` | 안정적 fill, 0 drop |
| AO vs VB 비교 | live call A/B | AO clean (VB와 동등) |
| INT32 round-trip | 테스트 신호 왕복 | 16-bit: input == denorm(norm(input)), 정보 손실 없음 |
| Overflow rejection | 의도적 과부하 | DropCount 증가, garble 없음 |
| Underrun silence | Speaker 없이 Mic | 깔끔한 무음 |
| SRC mismatch | 44.1k↔48k 통화 | clean 판정 |
| Phone Link 실통화 | `run_test_call.py` | 사용자 clean 판정 |

---

## FRAME_PIPE 구조체 설계

```c
// ============================================================
// Fixed Frame Pipe — VB-style transport
// Source of truth: pipe 자체. Mic DMA direct-push 없음.
// ============================================================

#define FP_DEFAULT_TARGET_FILL      3584    // ~74ms @ 48kHz (실제 latency)
#define FP_MIN_TARGET_FILL          512     // ~10ms @ 48kHz
#define FP_MAX_TARGET_FILL          16384   // ~341ms @ 48kHz
#define FP_CAPACITY_MULTIPLIER      2       // CapacityFrames = TargetFill × 2
#define FP_MIN_GATE_FRAMES          8       // VB 기준: 8-frame minimum

typedef struct _FRAME_PIPE {
    // ─── INT32 Frame Ring (transport core) ───
    INT32*      RingBuffer;         // CapacityFrames * PipeChannels INT32 samples
    ULONG       PipeChannels;       // channels per frame
    volatile ULONG  WriteFrame;     // producer frame index [0, CapacityFrames)
    volatile ULONG  ReadFrame;      // consumer frame index [0, CapacityFrames)
    KSPIN_LOCK  PipeLock;

    // ─── Pipe Format (configured, not dynamic) ───
    ULONG       PipeSampleRate;     // Hz = configured InternalRate
    ULONG       PipeBitsPerSample;  // INT32 고정 (32)
    ULONG       PipeBlockAlign;     // PipeChannels * sizeof(INT32)

    // ─── Stream Format Registration ───
    ULONG       SpeakerSampleRate;
    ULONG       SpeakerBitsPerSample;
    ULONG       SpeakerChannels;
    ULONG       SpeakerBlockAlign;
    BOOLEAN     SpeakerActive;

    ULONG       MicSampleRate;
    ULONG       MicBitsPerSample;
    ULONG       MicChannels;
    ULONG       MicBlockAlign;
    BOOLEAN     MicActive;

    // ─── Path Selection ───
    BOOLEAN     SpeakerSameRate;    // Speaker rate == PipeSampleRate → no SRC
    BOOLEAN     MicSameRate;        // Mic rate == PipeSampleRate → no SRC
    // Note: INT32 normalization + channel mapping은 항상 수행. SameRate는 SRC 생략만 의미

    // ─── Fixed Latency (3개 분리) ───
    ULONG       TargetFillFrames;   // steady-state fill level = 실제 latency
    ULONG       CapacityFrames;     // ring 크기 = TargetFillFrames × 2
    ULONG       StartThresholdFrames; // Mic read 시작 최소 fill (= TargetFillFrames)
    BOOLEAN     StartPhaseComplete; // TRUE after fill >= StartThreshold reached

    // ─── Overflow / Underrun / Diagnostics ───
    volatile ULONG  DropCount;      // frames rejected on write (pipe full)
    volatile ULONG  UnderrunCount;  // frames silence-filled on read (pipe empty, post-startup)
    volatile ULONG  ActiveRenderCount;  // active render streams (multi-client 진단)

    // ─── SRC State (fallback path only) ───
    INT32       SpeakerSrcAccum[16];
    INT32       SpeakerSrcPrev[16];
    ULONG       SpeakerSrcFraction;
    INT32       MicSrcAccum[16];
    INT32       MicSrcPrev[16];
    ULONG       MicSrcFraction;

    // ─── Scratch Buffers (PASSIVE_LEVEL에서 할당, DPC에서 사용) ───
    BYTE*       ScratchDma;         // DMA linearization buffer
    INT32*      ScratchConvert;     // format conversion intermediate
    ULONG       ScratchSizeBytes;   // max: LatencyFrames * PipeChannels * sizeof(INT32)

    // ─── Drift Correction State ───
    LONGLONG    DriftBaseQpc;       // base timestamp for drift calc
    ULONG       DriftTickCount;     // ticks since base (wraps at 100)
    LONGLONG    DriftNextDeadline;  // next expected tick QPC

    // ─── Configuration ───
    BOOLEAN     Initialized;
} FRAME_PIPE, *PFRAME_PIPE;
```

### 구조체 설계 근거

1. **MicDmaBuffer 필드 없음** — pipe가 유일한 transport. Mic DPC는 pipe에서 read한 후
   자신의 DMA buffer에 쓴다. Speaker DPC가 Mic DMA에 push하지 않음.

2. **INT32 RingBuffer** — VB-Cable과 동일. packed 24-bit (3 bytes) 대신 4 bytes/sample.
   ~19-bit normalized range로 SRC 곱셈 시 INT32 overflow 방지.

3. **SpeakerSameRate / MicSameRate 분리** — 각 방향이 독립적으로 rate 일치 여부 판단.
   Rate 일치 → SRC 생략. INT32 normalization + channel mapping은 항상 수행.

4. **SRC state per direction** — 방향별 accumulator 분리. State는 persistent across ticks.

5. **CapacityFrames = TargetFillFrames × 2** — ring 크기는 target fill의 2배. Headroom 확보.

6. **StartThresholdFrames = TargetFillFrames** — fill level이 이 값에 도달할 때까지
   Mic DPC는 silence 반환. 실제 fixed latency를 보장하는 메커니즘. thin-fill 방지.

7. **ActiveRenderCount** — 기존 multi-render 진단 유지. Transport 변경과 무관.

### IRQL 계약

| 함수 | IRQL | 비고 |
|------|------|------|
| FramePipeInit / Cleanup | PASSIVE_LEVEL | pool 할당/해제 |
| FramePipeRegisterFormat / Unregister | PASSIVE_LEVEL | format 변경 |
| FramePipeWriteFrames / ReadFrames | DISPATCH_LEVEL | DPC에서 호출, pool 할당 금지 |
| FramePipeReset | PASSIVE_LEVEL | KeFlushQueuedDpcs 후 호출 |
| FramePipeGetFillFrames | any | 읽기 전용 |

### Scratch buffer 할당 규칙

- **할당 시점**: FramePipeInit() (PASSIVE_LEVEL, DriverEntry 또는 stream open)
- **해제 시점**: FramePipeCleanup() (PASSIVE_LEVEL)
- **DPC(DISPATCH_LEVEL)에서 pool 할당 절대 금지**
- 크기: `max(DMA buffer size, LatencyFrames * PipeChannels * sizeof(INT32))`

### 메모리 상한

- 최대 ring: 32768 frames × 16ch × 4 bytes = **2MB NonPagedPoolNx per pipe**
- Cable A + B = 최대 4MB
- 기본값 (7168 × 2ch × 4) = **56KB per pipe** — 일반 사용 시 무시 가능

---

## Fixed Latency 유지 메커니즘

### 문제

CapacityFrames(ring 크기)만으로는 fixed latency가 보장되지 않는다.
현재 AO도 4초 ring이 있지만, Speaker DPC가 쓰면 Mic DPC가 바로 소비하여
실제 fill level은 수십 frames에 불과 (thin-fill). 단위를 frames로 바꿔도 같은 문제.

### 해결: 3개 값 분리

| 이름 | 의미 | 기본값 (48kHz) | 사용자 설정 |
|------|------|---------------|------------|
| **TargetFillFrames** | steady-state fill = **실제 latency** | 3584 frames (~74ms) | ms UI로 설정 |
| **CapacityFrames** | ring 크기 = TargetFill × 2 (headroom) | 7168 frames (~149ms) | 자동 계산 |
| **StartThresholdFrames** | Mic read 시작 최소 fill | = TargetFillFrames | 자동 (= TargetFill) |

**관계**: `CapacityFrames = TargetFillFrames × 2` (자동). 사용자는 TargetFillFrames만 제어.

### VB-Cable의 7168과 AO의 해석

> **이것은 VB confirmed behavior가 아닌 AO design choice입니다.**
>
> VB-Cable의 7168 frames가 capacity인지, target fill인지, 또는 다른 의미인지는
> reverse engineering으로 확정되지 않았습니다. VB의 ring 크기가 7168이고,
> startup threshold 메커니즘이 있는지도 불확실합니다.
>
> **AO의 결정**: 7168을 **CapacityFrames** (ring 크기)로 사용하고,
> **TargetFillFrames = 3584** (~74ms @ 48kHz)를 실제 latency로 설정합니다.
>
> 이유:
> - VB와 동일한 메모리 사용량 (7168 × channels × 4 bytes)
> - 명시적 StartThreshold로 thin-fill 방지 (VB에는 없을 수 있는 AO 추가 안전장치)
> - 실제 latency 74ms는 Phone Link 통화에 충분히 낮음
> - CapacityFrames=14336 (TargetFill=7168로 할 경우)은 불필요하게 큰 메모리 사용

### 왜 CapacityFrames > TargetFillFrames 인가

```
CapacityFrames = 7168
                 ┌────────────────────────────────────────────┐
                 │            ring buffer                     │
                 │  [headroom]  [TargetFill=3584]  [headroom] │
                 │   jitter     ← steady-state →    jitter    │
                 │   absorb       fill level        absorb    │
                 └────────────────────────────────────────────┘
                 ^                                            ^
              reject                                       reject
            (DropCount++)                              (overflow)
```

- TargetFill을 기준으로 양쪽에 headroom이 있어 DPC scheduling jitter를 흡수
- Fill이 0에 도달 → underrun (silence fill)
- Fill이 CapacityFrames에 도달 → overflow (reject)
- 정상 상태에서 fill은 TargetFill 근처에서 진동

### Startup Flow

```
Speaker DPC writes → fill 증가
                                    ← Mic DPC는 fill >= StartThreshold 될 때까지 silence 반환
                                    ← 이후 정상 read 시작

시간 →  [silence phase]  [data phase ─────────────────────]
fill →  0 ... 증가 ... StartThreshold=TargetFill 도달 → steady-state
```

### FramePipeReadFrames 동작

```
requestedFrames = N
currentFill = (WriteFrame - ReadFrame) or wrapped equivalent

Case 1: Startup (not yet filled)
  if (!StartPhaseComplete && currentFill < StartThresholdFrames):
    → output 전체에 silence(0) 기록
    → return 0 (startup phase, underrun 아님)

Case 2: Empty (post-startup)
  if (StartPhaseComplete && currentFill == 0):
    → output 전체에 silence(0) 기록
    → UnderrunCount += requestedFrames
    → return 0

Case 3: Partial (0 < currentFill < requestedFrames)
  if (StartPhaseComplete && currentFill < requestedFrames):
    → available = currentFill frames를 output에 copy
    → remainder (requestedFrames - available) frames를 silence(0)로 fill
    → UnderrunCount += (requestedFrames - available)
    → return available

Case 4: Normal (currentFill >= requestedFrames)
  → StartPhaseComplete = TRUE
  → requestedFrames만큼 output에 copy
  → return requestedFrames
```

**UnderrunCount는 frames 단위** (이벤트 횟수가 아님). DropCount도 frames 단위.

### IOCTL에서의 의미

| IOCTL | 제어하는 값 | 의미 |
|-------|-----------|------|
| `SET_MAX_LATENCY` (ms) | TargetFillFrames | **실제 latency** |
| `GET_CONFIG` | TargetFillMs + CapacityFrames + TargetFillFrames | 모두 반환 |

UI에서 사용자가 "75ms"로 설정 → TargetFillFrames=3584, CapacityFrames=7168 (자동)

---

## Pipe Rate 정책

### 결정: Pipe rate = configured InternalRate (first-opener wins 아님)

현재 AO는 `InternalRate`를 registry/IOCTL/control panel로 관리하고 있다 (`adapter.cpp:1541`).
VB-Cable도 configured internal rate를 사용한다.

**"First-opener wins" 정책을 폐기하고, configured rate를 사용한다.**

| 항목 | 정책 |
|------|------|
| Pipe sample rate | `InternalRate` (registry, default 48000) |
| Pipe channels | `InternalChannels` (registry, 8 or 16) |
| Pipe bit depth | INT32 고정 (~19-bit normalized) |
| SameRate 판정 | stream rate == InternalRate → SRC 생략 |

### 이유

1. IOCTL `SET_INTERNAL_RATE` / `GET_CONFIG`의 의미가 명확: pipe rate = configured rate
2. Control panel 슬라이더가 실제 pipe rate를 제어
3. SRC 규칙이 단순: stream rate != InternalRate → SRC, otherwise → no SRC
4. VB-Cable과 동일한 접근

### No-SRC (Fast Path) 조건

```
SpeakerSameRate = (SpeakerSampleRate == PipeSampleRate)
MicSameRate     = (MicSampleRate == PipeSampleRate)
```

- Rate 일치 → SRC 생략 (fast path)
- Rate 불일치 → SRC 수행 (fallback)
- **INT32 normalization은 항상 수행** (16-bit → shift left 3, 24-bit → shift right 5, etc.)
- **Channel mapping은 항상 수행** (stream channels → pipe channels → stream channels)

### Channel Mapping 규칙

| Stream | Pipe | Write 시 | Read 시 |
|--------|------|---------|---------|
| 2ch | 8ch | ch0,1에 write, ch2-7은 0 | ch0,1만 read, ch2-7 무시 |
| 6ch (5.1) | 8ch | ch0-5에 write, ch6-7은 0 | ch0-5 read, ch6-7 무시 |
| 8ch | 8ch | 직접 대응 | 직접 대응 |
| 2ch | 16ch | ch0,1에 write, ch2-15는 0 | ch0,1만 read |

Channel mapping은 SameRate 여부와 무관하게 항상 수행.

### Rate 변경 시

`IOCTL_AO_SET_INTERNAL_RATE` 호출 → pipe rate 변경 → TargetFillFrames 재계산 →
CapacityFrames 재계산 → 모든 active stream의 SameRate 재평가.
Active stream 있으면 reject (기존 동작 유지).

---

## Pipe Format State Machine

```
                    ┌─────────────────────┐
                    │  IDLE               │  Ring allocated, pipe format = configured InternalRate
                    │  (no active stream) │  StartPhaseComplete = FALSE
                    └──────────┬──────────┘
                               │ Speaker or Mic enters RUN
                               ▼
                    ┌─────────────────────┐
                    │  ONE_ACTIVE         │  SameRate 판정 (stream rate vs configured rate)
                    │                     │  Speaker: write to pipe 시작
                    │                     │  Mic: fill < StartThreshold → silence 반환
                    └──────────┬──────────┘
                               │ Second stream enters RUN
                               ▼
                    ┌─────────────────────┐
                    │  BOTH_ACTIVE        │  양방향 동작
                    │                     │  fill >= StartThreshold → Mic read 시작
                    │                     │  steady-state 진입
                    └──────────┬──────────┘
                               │ One stream stops
                               ▼
                    ┌─────────────────────┐
                    │  ONE_ACTIVE         │  남은 stream 계속 동작
                    │                     │  Ring NOT reset (잔여 데이터 유지)
                    └──────────┬──────────┘
                               │ Last stream stops
                               ▼
                    ┌─────────────────────┐
                    │  IDLE               │  WriteFrame = ReadFrame = 0
                    │                     │  StartPhaseComplete = FALSE
                    │                     │  SRC state reset
                    └─────────────────────┘
```

### 규칙

1. **Pipe format = configured InternalRate + InternalChannels** (항상). Stream format이 아닌 설정값이 기준.
2. **SameRate 판정**: stream rate == PipeSampleRate → SRC 생략. Channel/bit depth는 항상 변환.
3. **Pause 순서** (race condition 방지, 기존 동작 유지):
   1. Stream state → PAUSE
   2. ExCancelTimer (timer 중지)
   3. KeFlushQueuedDpcs (in-flight DPC 완료 대기)
   4. 이후에만 ring position 변경 가능
4. **Stop clears stream, not pipe**: 해당 방향 Active=FALSE, SRC state reset. 양쪽 다 Stop → ring reset + StartPhaseComplete=FALSE
5. **Speaker Stop + Mic Active**: Speaker Active=FALSE. Ring 유지 (reset 안 함). Mic는 남은 데이터를 계속 read. Ring 비워지면 silence fill + UnderrunCount++. StartPhaseComplete는 TRUE 유지 (양쪽 다 Stop할 때만 FALSE로).
6. **Mic Stop + Speaker Active**: Mic Active=FALSE. Speaker는 계속 pipe에 write. Fill이 CapacityFrames에 도달하면 reject (DropCount++). StartPhaseComplete는 TRUE 유지.
7. **Underrun 시 Mic DMA zero-fill 필수**: pipe에서 데이터 부족 → silence(0)을 Mic DMA에 기록. skip하면 OS가 stale data를 읽어 noise 발생.
8. **Rate 변경 거부**: active stream이 있으면 IOCTL_AO_SET_INTERNAL_RATE reject (기존 동작 유지)

---

## Latency / Config Migration

### 원칙: 내부는 frames, UI는 ms

- **Source of truth**: `FRAME_PIPE.TargetFillFrames` (frames) = 실제 latency
- **파생**: `CapacityFrames = TargetFillFrames × 2` (자동)
- **UI/IOCTL/Registry**: milliseconds (사용자 호환)
- **변환**: `TargetFillFrames = (ms * PipeSampleRate) / 1000`
- **역변환**: `ms = (TargetFillFrames * 1000) / PipeSampleRate`

### IOCTL Migration

| 기존 IOCTL | 변경 사항 |
|------------|----------|
| `IOCTL_AO_SET_MAX_LATENCY` | 입력: ms (기존 호환). 내부: TargetFillFrames로 변환, CapacityFrames 자동 계산 |
| `IOCTL_AO_GET_CONFIG` | 출력에 `LatencyMs` (기존) + `TargetFillFrames` + `CapacityFrames` 모두 포함 |
| `IOCTL_AO_SET_INTERNAL_RATE` | rate 변경 시 `TargetFillFrames = (savedMs * newRate) / 1000` 재계산 |

### Registry

- `MaxLatencyMs` (REG_DWORD, ms) 유지 — rate-independent, TargetFill을 의미
- DriverEntry에서 ms 읽어서 TargetFillFrames 계산, CapacityFrames = × 2
- Registry에 frames를 저장하지 않음

### 기본값 변경

| 항목 | 기존 | 신규 | 의미 |
|------|------|------|------|
| Default | 20ms | 74ms (TargetFill=3584, Capacity=7168 @ 48kHz) | VB-Cable capacity의 절반 = 실제 latency |
| Min | 5ms | 10ms (TargetFill=512) | 최소 실용 latency |
| Max | 100ms | 341ms (TargetFill=16384, Capacity=32768) | VB-Cable maximum capacity의 절반 |

---

## INT32 Normalization

~19-bit normalized range (VB-Cable 동일):

| 입력 포맷 | 변환 | Range |
|-----------|------|-------|
| 8-bit unsigned | `(byte - 0x80) << 11` | ~19-bit |
| 16-bit signed | `short << 3` | ~19-bit |
| 24-bit signed | `(int24 << 8) >> 13` | ~19-bit |
| 32-bit int | `int32 >> 13` | ~19-bit |
| 32-bit float | `(int)(float * 262143.0f)` | ~19-bit |

**결정: INT32 normalization은 모든 경로에서 항상 수행 (VB-Cable 동일).**
- "No-SRC fast path"도 INT32 norm + channel map을 거침. SRC만 생략.
- 16-bit → INT32 → 16-bit round-trip은 lossless (`short << 3 >> 3 == short`)
- 24-bit → INT32 → 24-bit은 하위 5-bit 손실 (VB-Cable과 동일 trade-off)
- Ring 포맷이 단일(INT32)이므로 코드 경로가 하나, 버그 surface가 작음

---

## SRC Algorithm: Linear Interpolation (VB-Cable 방식)

```
1. GCD 계산: try divisors 300 → 100 → 75
   예: 44100/48000 → GCD=300 → ratio 147:160
2. 선형보간: output = (accum * dst_ratio + prev * src_ratio) / total_ratio
3. Per-channel state: accumulator[16], previous[16], fraction
```

sinc SRC (sinc_table.h)는 보존, 향후 옵션 전환 가능.

---

## Implementation Phases

### Phase 0: Branch + Scaffolding ✅ (완료)

브랜치 분리, RE 자산 이동, CLAUDE.md 설정 완료.

---

### Phase 1: Frame Pipe Core (Ring + API + 기본 진단)

**목표**: FRAME_PIPE 구조체 정의, core write/read, format state machine, 기본 IOCTL 진단

**작업**:
1. `loopback.h`에 FRAME_PIPE 구조체 선언 (기존 LOOPBACK_BUFFER와 공존, 아직 교체 안 함)
2. `loopback.cpp`에 core 함수 구현:
   - `FramePipeInit` — ring + scratch 할당 (PASSIVE_LEVEL only)
   - `FramePipeCleanup` — 해제
   - `FramePipeReset` — KeFlushQueuedDpcs 후에만 호출
   - `FramePipeRegisterFormat / UnregisterFormat` — state machine
   - `FramePipeWriteFrames` — pipe full → reject entire write, DropCount++
   - `FramePipeReadFrames`:
     - `!StartPhaseComplete && fill < StartThreshold` → silence 반환 (startup, underrun 아님)
     - `StartPhaseComplete && fill == 0` → silence 반환 + UnderrunCount++ (실제 underrun)
     - `fill >= StartThreshold` → StartPhaseComplete=TRUE, 정상 read
   - `FramePipeGetFillFrames`
3. Ring wrap: compare-and-reset (no modulo)
4. MinGateFrames (8 frames) 미달 시 read 거부
5. StartThresholdFrames = TargetFillFrames, CapacityFrames = TargetFillFrames × 2
6. C_ASSERT layout guard
6. **진단 (Phase 1부터)**: `ioctl.h`에 PipeFillFrames/DropCount/UnderrunCount 필드 추가,
   `adapter.cpp` handler에서 FRAME_PIPE에서 읽어 반환,
   `test_stream_monitor.py`에 기본 pipe 상태 출력 추가

**파일**: `loopback.h`, `loopback.cpp`, `ioctl.h`, `adapter.cpp`, `test_stream_monitor.py`
**검증**: `build-verify.ps1` 컴파일 + monitor에서 pipe 상태 조회

---

### Phase 2: Stream Integration (DPC Path Rewrite)

**목표**: ReadBytes/WriteBytes가 FRAME_PIPE를 사용하도록 전환

**작업**:
1. `g_CableALoopback` → `g_CableAPipe` (FRAME_PIPE)
2. ReadBytes() (Speaker DPC):
   - DMA circular → ScratchDma (linearize, wrap-around 해소)
   - INT32 normalization (항상) + channel mapping (항상)
   - SpeakerSameRate → FramePipeWriteFrames(normalized, nFrames) (SRC 없음)
   - !SameRate → SRC → FramePipeWriteFrames (Phase 3에서 SRC 구현, 여기서는 stub)
3. WriteBytes() (Mic DPC):
   - **항상 pipe에서 read** (MicDma direct-push 없음)
   - FramePipeReadFrames → channel mapping (항상) + INT32 denormalization (항상)
   - MicSameRate → denormalized 결과를 Mic DMA에 copy (SRC 없음)
   - !SameRate → SRC 후 denormalize → Mic DMA copy (Phase 3에서 SRC 구현)
   - **underrun 시: silence(0)을 Mic DMA에 기록. skip 절대 안 함**
4. UpdatePosition: frame 단위 position 추적, on-query QPC 재계산 (GetPosition)
5. Timer drift correction (63/64 phase 보정, 100-tick rebase)
6. DMA scratch copy, 8-frame minimum gate (frames 기준, bytes 아님)
7. Stream state hooks:
   - `Run` → FramePipeRegisterFormat (PASSIVE_LEVEL)
   - `Pause` → state→PAUSE → KeCancelTimer → KeFlushQueuedDpcs → (이후에만 ring 조작 가능)
   - `Stop` → FramePipeUnregisterFormat
8. Old LOOPBACK_BUFFER 호출 제거 또는 #ifdef
9. **진단 업데이트**: monitor에 fill level + drop/underrun delta 실시간 표시

**파일**: `minwavertstream.cpp`, `minwavertstream.h`, `loopback.h`, `loopback.cpp`, `test_stream_monitor.py`
**검증**: `build-verify.ps1` + same-format 재생 + `test_stream_monitor.py` fill/drop/underrun 확인

---

### Phase 3: Fallback Conversion Path

**목표**: format mismatch (rate/depth/channel 다를 때) 동작

**작업**:
1. INT32 normalization 함수 (NormalizeToInt32 / DenormalizeFromInt32)
2. Channel mapping (기존 loopback.cpp 로직 재사용, INT32 기준으로 수정)
3. Linear interpolation SRC (GCD ratio 300/100/75, per-channel accumulator)
4. FramePipeWriteConverted: Normalize → SRC (rate mismatch시) → ChannelMap (ch mismatch시) → WriteFrames
5. FramePipeReadConverted: ReadFrames → ChannelMap → SRC → Denormalize
6. Channel asymmetry 처리: pipe=8ch, mic=2ch → channels 0/1 passthrough (mix-down 아님)
7. **진단 업데이트**: monitor에 SameRate/SRC 경로 표시, SRC ratio 표시

**파일**: `loopback.cpp`, `loopback.h`, `test_stream_monitor.py`
**검증**: 44.1k↔48k mismatch 재생 + monitor에서 fallback 경로 확인

---

### Phase 4: Cleanup + IOCTL 완성 + Final Validation

**목표**: old code 제거, IOCTL 마무리, VB-Cable 비교

**작업**:
1. Old code 제거:
   - LOOPBACK_BUFFER, LoopbackWrite/Read, LOOPBACK_MIC_SINK, MicDmaStash 전부 삭제
   - g_CableALoopback/g_CableBLoopback 삭제
   - #ifdef guard 정리
2. C_ASSERT를 FRAME_PIPE용으로 갱신
3. Latency config migration 완성:
   - IOCTL_AO_SET_MAX_LATENCY: ms 입력 → TargetFillFrames 변환, CapacityFrames = ×2
   - IOCTL_AO_GET_CONFIG: LatencyMs + TargetFillFrames + CapacityFrames 반환
   - IOCTL_AO_SET_INTERNAL_RATE: rate 변경 시 TargetFillFrames 재계산
   - Control panel 슬라이더 range: 10~341ms (TargetFill 기준)
4. `PIPELINE_V2_CHANGELOG.md` 마무리

**Final Validation Matrix:**

| 항목 | 방법 | 기준 |
|------|------|------|
| Same-format 통화 | Phone Link 실통화 | **clean** (VB 동등) |
| Mismatch 통화 | 44.1k↔48k | clean |
| Overflow | 의도적 과부하 | DropCount↑, no garble |
| Underrun | Speaker 없이 Mic | **깔끔한 무음** (DMA zero-fill 확인) |
| Pause/Resume | 반복 | no crash, no stale data |
| Long-running | 10분+ | drift < 1 frame |
| 16ch | 16채널 모드 | 정상 |
| No-reboot upgrade | `install.ps1 -Action upgrade` | 정상 |
| Multi-client | 2개 앱 동시 재생 | WaveRT audio engine mixer 후 pin 1개 확인 |

**파일**: `loopback.h/cpp`, `minwavertstream.cpp`, `adapter.cpp`, `ioctl.h`, `ControlPanel/main.cpp`

---

## Success Criteria

1. Same-format 통화 품질: Phone Link 실통화 **clean** (VB 동등 이상)
2. Frame pipe 유일 transport: Mic DMA direct-push 경로 없음
3. Overflow: reject + counter, garble 없음
4. Underrun: **Mic DMA에 silence(0) 기록**, stale data 없음
5. Diagnostics: fill/drop/underrun **각 phase부터** 실시간 확인 가능
6. No-reboot upgrade: 정상 동작
7. SRC mismatch: clean 통화
8. Long-running: drift < 1 frame, dropout 0
