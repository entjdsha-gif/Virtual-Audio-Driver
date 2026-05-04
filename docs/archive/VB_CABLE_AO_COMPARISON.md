# VB-Cable vs AO Cable — 전체 코드 비교 문서

## 1. 입력 경로 (Speaker DPC → Ring Write) 비교

### 1-1. Ring Write 함수

| VB-Cable (FUN_1400022b0) | AO (FramePipeWriteFrames) | Match | 차이점 |
|---|---|---|---|
| `if (param_1[2]==0) return -1` — ring 유효성 검사 | `if (!pPipe \|\| !pPipe->Initialized)` | ✅ | 동일 목적 |
| 8/16/24/32-bit 분기 후 **normalization 포함** | normalization 없음 (별도 함수에서 처리) | ❌ | VB: write 안에서 변환. AO: WriteFromDma에서 변환 후 WriteFrames는 INT32만 |
| Overflow: `if (space-2 < needed) → return -11, counter++` | Overflow: `if (frameCount > freeFrames) → return 0, DropCount++` | ✅ | 둘 다 hard reject. VB는 2프레임 여유 |
| Per-sample loop: `ring[WritePos*ch+c] = converted` | Bulk copy: `RtlCopyMemory(ring+writeIdx*ch, src, bytes)` | ❌ | VB: 샘플 단위. AO: 프레임 블록 단위 |
| WritePos++ with `if (pos >= capacity) pos = 0` | `WriteFrame = (writeIdx + frameCount) % capacity` | ✅ | 같은 wrap 로직, 다른 표현 |
| **FillFrames 필드 없음** — WritePos-ReadPos로 계산 | `FillFrames = fill + frameCount` (명시적 카운터) | ❌ | AO가 더 안전 (full/empty 구분) |
| **Spinlock: 함수 내부에 없음** — 호출자(DPC)가 stream-level lock 보유 | `KeAcquireSpinLock(&PipeLock)` 함수 진입 시 | ❌ | VB: DPC에서 1회 lock. AO: WriteFrames 내부에서 lock |

### 1-2. DMA → Scratch → Ring 변환

| VB-Cable (FUN_140005634) | AO (FramePipeWriteFromDma) | Match | 차이점 |
|---|---|---|---|
| DMA→scratch raw memcpy (FUN_140007680) | DMA 직접 읽기 (chunk loop, DMA wrap은 ReadBytes가 처리) | ❌ | VB: 항상 scratch에 선복사. AO: DMA에서 직접 변환 |
| Scratch→ring: `FUN_1400022b0(ring, scratch, bytes, rate, ch, bps, dir=1)` | Scratch→ring: normalize loop → `FramePipeWriteFrames(pipe, scratch, chunk)` | ⚠️ | VB: 한 함수에서 변환+write. AO: 2단계 분리 |
| Per-channel volume envelope (lines 4428-4467) | 없음 | ❌ | VB: 채널별 볼륨. AO: 볼륨 없음 |

### 1-3. INT32 Normalization

| Format | VB-Cable | AO | Match |
|--------|----------|-----|-------|
| 8-bit | `(byte - 0x80) * 0x800` (<<11) | 미구현 | ❌ deferred — parity-first, no new functionality before parity closure |
| 16-bit | `short << 3` | `FpNorm16: (INT32)s << 3` | ✅ 동일 |
| 24-bit | `(3bytes << 8) >> 13` (net >>5) | `FpNorm24: Read24() >> 5` | ✅ 동일 결과 |
| 32-bit int | memcpy (원본 유지) | `FpNorm32i: direct copy` (Phase 2 / G9) | ✅ 동일 |
| 32-bit float | memcpy (원본 유지) | `FpNormFloat: (INT32)bits` 직접 캐스트 (Phase 2 / G10) | ✅ 동일 |

---

## 2. 파이프라인 (Ring 구조 + State Management) 비교

### 2-1. Ring 구조체

| VB-Cable 오프셋 | 역할 | AO FRAME_PIPE 필드 | Match |
|---|---|---|---|
| +0x00 | buffer 포인터 (상대) | RingBuffer (절대 포인터) | ⚠️ 표현만 다름 |
| +0x08 | buffer 크기/오프셋 | (없음, RingBuffer에 포함) | ❌ |
| +0x0C | channels | PipeChannels | ✅ |
| +0x10 | frame capacity | (없음, CapacityFrames 사용) | ⚠️ |
| +0x14 | WrapBound (wrap 기준) | CapacityFrames | ✅ 동일 역할 |
| +0x18 | WritePos | WriteFrame | ✅ |
| +0x1C | ReadPos | ReadFrame | ✅ |
| (없음) | — | **FillFrames (명시적)** | ❌ AO만 있음 |
| +0x20 | stride/rate 관련 | PipeSampleRate | ⚠️ |
| +0x17C | counter1 | DropCount | ✅ |
| +0x180 | overflow counter | (DropCount에 통합) | ⚠️ |
| +0x184-188 | counters/flags | UnderrunCount, ActiveRenderCount | ⚠️ |
| (없음) | — | **SpeakerActive, MicActive, SameRate 등** | ❌ AO만 있음 |
| (없음) | — | **StartPhaseComplete, StartThresholdFrames** | ❌ AO만 있음 |
| (없음) | — | **ScratchSpk, ScratchMic** | ❌ AO만 있음 |

### 2-2. State Management (SetState)

| 상태 전환 | VB-Cable (FUN_140005910) | AO (minwavertstream SetState) | Match | 핵심 차이 |
|---|---|---|---|---|
| **STOP (0)** | stream 변수만 클리어 (+0xF0,E8,EC,D0,D8,E0,1B0,74,15C). **Ring 안 건드림** | FramePipeUnregisterFormat → bothStopped → FramePipeReset | ❌ | **VB: STOP에서 ring reset 안 함. AO: STOP에서 unregister + reset** |
| **PAUSE (2, from RUN)** | cancel timer → **KeFlushQueuedDpcs** → **FUN_1400039ac (ring reset)** → stats 클리어 | 아무것도 안 함 (disabled) | ❌ | **VB: PAUSE에서 ring reset. AO: PAUSE에서 아무것도 안 함** |
| **RUN (3)** | QPC 초기화, 타이머 period 계산, 공유 타이머 등록, state=3 | FramePipeRegisterFormat (format+SameRate 설정) | ⚠️ | VB: 타이밍 초기화 중심. AO: format 등록 중심 |

### 2-3. Ring Reset

| VB-Cable (FUN_1400039ac) | AO (FramePipeReset) | Match |
|---|---|---|
| ring buffer 내용 zero-fill (FUN_140007940) | ring buffer zero-fill (RtlZeroMemory) | ✅ |
| WritePos/ReadPos → FUN_140003968으로 리셋 | WriteFrame=0, ReadFrame=0, FillFrames=0 | ✅ |
| +0x17C,180,184,188 counters → 0 | DropCount=0, UnderrunCount=0 | ✅ |
| (없음) | StartPhaseComplete = FALSE | ❌ AO만 |

### 2-4. Available Frames 계산

| VB-Cable (FUN_140001144) | AO (FramePipeGetFillFrames) | Match |
|---|---|---|
| `(WritePos - ReadPos)`, 음수면 `+WrapBound` | `return pPipe->FillFrames` (명시적 카운터) | ❌ |
| rate ratio 스케일링 옵션 | 없음 | ❌ |

---

## 3. 출력 경로 (Ring Read → Mic DPC → DMA) 비교

### 3-1. Ring Read 함수

| VB-Cable (FUN_1400011d4) | AO (FramePipeReadFrames) | Match | 차이점 |
|---|---|---|---|
| available = WritePos - ReadPos (wrap 처리) | fill = pPipe->FillFrames | ❌ | VB: 계산. AO: 명시적 |
| `if (available < requested)` → silence fill + return error | `if (fill == 0)` → zero-fill + UnderrunCount++ | ⚠️ | VB: available<requested 비교. AO: fill==0만 체크 후 partial read |
| **8-bit underrun**: fill 0x80 (center value) | fill 0x00 (zero) | ❌ | 8-bit 차이 |
| **16/24/32-bit underrun**: fill 0x00 | fill 0x00 | ✅ | 동일 |
| Per-sample denormalization IN read function | INT32 bulk copy, denorm은 별도 (ReadToDma) | ❌ | VB: read에서 변환. AO: 2단계 분리 |
| ReadPos 1프레임씩 advance, wrap check | `ReadFrame = (readIdx + framesToRead) % capacity` | ✅ | 같은 결과, 다른 구현 |
| Partial read: available만큼만 read, 나머지 skip | Partial read: available 읽고 **remainder zero-fill** | ❌ | **AO가 더 안전** (항상 요청 바이트 채움) |

### 3-2. Denormalization

| Format | VB-Cable | AO | Match |
|--------|----------|-----|-------|
| 8-bit | `(INT32 >> 11) + 0x80` | 미구현 | ❌ deferred — parity-first, no new functionality before parity closure |
| 16-bit | `INT32 >> 3` | `FpDenorm16: (INT16)(v >> 3)` | ✅ 동일 |
| 24-bit | `INT32 << 5` → 3 bytes | `FpDenorm24: (v << 5)` → 3 bytes | ✅ 동일 |
| 32-bit int | memcpy | `FpDenorm32i: direct copy` (Phase 2 / G9 mirror) | ✅ 동일 |
| 32-bit float | memcpy | `FpDenormFloat: (UINT32)v` 직접 캐스트 (Phase 2 / G10 mirror) | ✅ 동일 |

### 3-3. Mic DPC가 Speaker 상태를 체크하는가?

| VB-Cable | AO | Match |
|---|---|---|
| FUN_140004cf4로 other stream RUN 체크. **하지만 결과에 관계없이 ring read 진행** | `if (!MicActive)` → silence. 그 외에는 항상 read | ⚠️ | 둘 다 독립적으로 read. AO는 MicActive 가드만 |

---

## 4. DPC 타이머 비교

| 항목 | VB-Cable | AO | Match |
|---|---|---|---|
| 타이머 종류 | ExAllocateTimer (공유, 전체 stream 순회) | WaveRT per-stream notification timer | ❌ 변경 불가 |
| 주기 | 1ms (10000 × 100ns) | OS 설정 (보통 10ms) | ❌ |
| Drift correction | 63/64 phase, magic constant, 100-tick rebase | 없음 | ❌ |
| Minimum gate | 8 frames 미만 skip | 없음 (상수만 정의) | ❌ |
| DMA overrun guard | bytes > half DMA → skip | 없음 | ❌ |

---

## 5. Spinlock / 동기화 비교

| 항목 | VB-Cable | AO | Match |
|---|---|---|---|
| Lock 종류 | stream-level KSPIN_LOCK at +0x168 | FRAME_PIPE.PipeLock | ✅ 유사 |
| Lock 범위 | DPC 전체 (1회 acquire, 모든 처리, 1회 release) | WriteFrames/ReadFrames 각각 acquire/release | ❌ VB: 넓은 범위. AO: 좁은 범위 |
| Lock contention | Speaker/Mic DPC가 같은 stream lock 공유 | PipeLock을 Speaker/Mic DPC가 공유 | ✅ |

---

## 6. 핵심 차이 정리 (우선순위)

### 🔴 Critical (통화 품질에 직접 영향)

| # | 차이점 | VB-Cable | AO | 영향 |
|---|---|---|---|---|
| C1 | **PAUSE에서 ring reset** | ✅ 함 | ❌ 안 함 | Speaker PAUSE→STOP→RUN gap에서 stale data drain → underrun |
| C2 | **STOP에서 ring 안 건드림** | ✅ stream만 정리 | ❌ UnregisterFormat + bothStopped reset | STOP 시 pipe 상태 소실 |
| C3 | **Unregister 시점** | stream 소멸 시 | STOP 시 | STOP/RUN 사이클마다 등록/해제 반복 |

### 🟡 Medium (정확성/안정성)

| # | 차이점 | VB-Cable | AO |
|---|---|---|---|
| M1 | 32-bit int normalization | memcpy (원본) | >>13 (정규화) |
| M2 | 32-bit float normalization | memcpy (원본) | float→INT24→>>5 |
| M3 | 8-bit underrun value | 0x80 (center) | 0x00 (zero) |
| M4 | DMA scratch 선복사 | 항상 | 안 함 |
| M5 | Per-channel volume | 있음 | 없음 |

### 🟢 Low (후순위 최적화)

| # | 차이점 | VB-Cable | AO |
|---|---|---|---|
| L1 | 공유 타이머 (1ms) | 있음 | WaveRT timer |
| L2 | Drift correction | 63/64 phase | 없음 |
| L3 | 8-frame minimum gate | 있음 | 상수만 |
| L4 | DMA overrun guard | 있음 | 없음 |
| L5 | Lock 범위 | DPC 전체 | 함수별 |
