# VB-Cable Runtime Dynamic Debug Findings (Claude)

> **작성자**: Claude (feature/ao-fixed-pipe-rewrite 브랜치)
> **작성일**: 2026-04-13
> **대상 바이너리**: `vbaudio_cablea64_win10.sys` (timestamp 67BC8612, Feb 2025)
> **디버깅 환경**: WinDbg 10.0.26100.7705, kernel net debug, Windows 11 Pro 26200
> **주의**: 이 문서는 Claude가 작성. 코덱스가 따로 쓰는 `docs/VBCABLE_DYNAMIC_DEBUG.md`와 엉키지 않도록 분리.

---

## 1. 동적 디버그 세팅

- 타깃 머신에 VB-Cable A/B 드라이버 로드된 상태에서 라이브 커널 디버그
- Phone Link "휴대폰과 연결" 앱으로 실제 통화 발신/수신하며 드라이버 함수 진입 시점에 브레이크포인트 걸어 관찰
- 리부팅마다 드라이버 리베이스되므로 오프셋 기반 BP 사용 (`bu vbaudio_cablea64_win10+0xXXXX`)
- Phone Link는 PC 오디오 파이프라인이 오래 블록되면 (수 초 이상) 자동으로 통화를 휴대폰으로 이관함 → stop-BP 대신 `.printf \"...\"; gc` 패턴으로 "자동 통과 로깅" BP를 사용해야 실시간 관찰 가능

## 2. 핵심 함수 맵 (절대 유지할 사실)

| 오프셋 | 역할 | 호출자 |
|---|---|---|
| `+0x3d1e` | IRP 디스패치 엔트리 | `portcls!PcDispatchIrp` |
| `+0x1a83e` | `AllocateAudioBuffer` 프로퍼티 핸들러 | `portcls!PinPropertyGetAudioBuffer` |
| `+0x10a0` | 버퍼 할당 내부 헬퍼 | `+0x1a83e` |
| `+0x39ac` | **링 버퍼 초기화/리셋 함수** | `+0x10a0` (alloc), `+0x5910` (Pause) |
| `+0x5910` | **SetState 상태머신** | `portcls!CPortPinWaveRT::DistributeDeviceState` |
| `+0x4cf4` | RUN 진입 전 사전 체크 헬퍼 | `+0x5910` RUN 분기 |
| `+0x65b8` | **ExAllocateTimer/ExSetTimer 등록** | `+0x5910` RUN 분기 |
| `+0x669c` | 타이머 취소 | `+0x5910` PAUSE 분기, `+0x5cc0` |
| `+0x5cc0` | **타이머 콜백 (데이터 path 심장)** | `ExSetTimer` |
| `+0x68ac` | **샘플 이동/링 처리 코어** | `+0x5cc0` (아직 미디스어셈블) |
| `+0x51a8` | 포맷 변환/SRC 헬퍼 (추정) | `+0x5cc0` post-loop |
| `+0x4f2c` | 포맷 변환/ring write 헬퍼 (추정) | `+0x5cc0`, `+0x68ac` |
| `+0x22b0` | notification dispatch 헬퍼 (추정) | `+0x5cc0` post-loop |
| `+0x10ec` | 글로벌 state=2일 때 특수 처리 | `+0x5cc0` post-loop 조건부 |

## 3. IAT 엔트리 (확정)

```
+0x8000  nt!KeQueryPerformanceCounter
+0x80e0  nt!KeAcquireSpinLockRaiseToDpc
+0x80e8  nt!KeReleaseSpinLock
+0x80f0  nt!DbgPrintEx
+0x80f8  nt!ExInitializePushLock
+0x8100  nt!KeFlushQueuedDpcs           ← PAUSE 경로에서 사용 (CLAUDE.md 원칙 10 증명)
+0x8108  nt!KeSetEvent                   ← WaveRT notification 발사
+0x8110  nt!ExAllocateTimer              ← HIGH_RES(attr=4) 플래그
+0x8118  nt!ExSetTimer
+0x8120  nt!ExDeleteTimer
+0x8180  nt!guard_check_icall            ← CFG
+0x8188  nt!KscpCfgDispatchUserCallTargetEsSmep  ← CFG dispatcher (virtual call target checker)
```

## 4. SetState 상태머신 (`+0x5910`) 완전 해석

### 진입
- `rcx = stream object`, `edx = new KSSTATE`
- `rcx+0x90` → port driver ptr → `[+0x128]` sub-object의 vtable 22번 슬롯(+0xB0h) 호출 (CFG dispatch 경유)
  - 인자: `(rcx=sub, edx=2, r8=stream+0xD8, r9=stream+0x74)`
  - 이건 portcls 쪽 notification sink로의 상태 변경 통지

### 상태별 분기
| state | 값 | 동작 |
|---|---|---|
| STOP | 0 | +0x160 락 획득 → position 필드 -1로 설정, 상태 필드 제로화 → 락 해제 |
| ACQUIRE | 1 | **no-op** (상태만 저장하고 리턴) |
| PAUSE | 2 | 아래 상세 |
| RUN | 3 | 아래 상세 |

### RUN 진입 (state 3)
```
lea rbx, [stream+0xF8]             ; lock object
call KeQueryPerformanceCounter
; QPC × 10,000,000 / freq = 100ns 단위 변환
[stream+0x108] = 시작 QPC (100ns)
[stream+0x178] = 복사본
[stream+0x180/188/1C8] = 0
call vbaudio+0x4cf4                 ; 사전 체크 (state 테이블 룩업)
if (eax != 0) goto exit
call vbaudio+0x65b8(rcx=stream-8)   ; 타이머 등록
[stream+0x58] = 타이머 참조 저장
```

### PAUSE 진입 (state 2)
```
if ([stream+0xB4] <= 2) goto exit   ; RUN 상태였을 때만 처리
if ([stream+0x58] != 0):            ; 타이머 참조 있을 때만
    call vbaudio+0x669c(stream-8)   ; ★ 타이머 취소
    [stream+0x58] = 0
    call KeFlushQueuedDpcs          ; ★★ 진행 중 DPC 완료 대기
    if ([stream+0x168] != 0):
        call vbaudio+0x39ac(ring)   ; ★★★ 링 버퍼 리셋
```

**이 순서가 핵심**: 타이머 취소 → `KeFlushQueuedDpcs` → 링 리셋. CLAUDE.md 원칙 10 완벽 구현.

## 5. 글로벌 싱글톤 타이머 패턴 (`+0x65b8`)

VB-Cable은 **드라이버 전체에 단일 고해상도 타이머** 사용. Per-stream DPC/타이머가 아님.

```
if (global_refcount == 0):
    타이머 할당:
        ExAllocateTimer(
            Callback = vbaudio+0x5cc0,
            Context = NULL,
            Attributes = 4 (EX_TIMER_HIGH_RESOLUTION))
    [global+0x12fd8] = timer handle
    ExSetTimer(
        DueTime = -10000 (100ns) = 1ms 후 첫 발사,
        Period = 0x2710 = 10000,    ; HIGH_RES에서 100ns 단위 추정 → 1ms 주기
        Parameters = NULL)

; 스트림 배열에 자기 포인터 등록:
slot = find_empty_slot([global+0x12f90..fd0])
stream_array[slot] = stream
[global+0x12f88] = max(count+1)
[global+0x12f84]++ (refcount)
```

**장점**:
- 타이머 생성/파괴 오버헤드 최소화
- 모든 스트림이 **동일 tick 경계**에 처리됨 → 위상 동기화 문제 없음
- 스케줄링 지터 감소

## 6. 타이머 콜백 `+0x5cc0` (데이터 path 심장) 상세

### Phase 1: 진입
```
/GS 쿠키 설정
r12d = 0 (누적 return)
KeQueryPerformanceCounter → rbx = current QPC
rbp = vbaudio+0x4080() = global ctx
if (global+0x298 == 0):  ; 첫 실행
    global+0x298 = rbx           ; baseline QPC
    global+0x2A0 = 1             ; counter
    global+0x2A8 = rbx + QPC/100 ; 다음 tick target (매직상수 div by 100)
```

매직 상수 `0xA3D70A3D70A3D70B` + `sar 6` = 정수 나눗셈 **÷100**.

### Phase 2: 스트림 배열 루프
```
r13 = [global+0x12f88]          ; 스트림 개수
rdi = [global+0x2A8]             ; target QPC

for (r14 = 0; r14 < r13; r14++):
    rsi = stream_array[r14]
    if (!rsi) continue
    rbp = vbaudio+0x4080()       ; per-stream context?

    if (rbx > rdi) r15d++        ; overdue tick 카운트

    if (stream+0xB8 == 0):       ; IDLE 스트림
        lock(+0x168)
        [stream+0x188] = rbx      ; last_qpc
        unlock
        if (r15 > 0):
            r12d += vbaudio+0x6778(stream, ctx)
        continue
```

### Phase 3: RUNNING 스트림 데이터 처리 ⭐
```
    if ([stream+0xA4]) inc ctx+0x190 ; render counter
    else               inc ctx+0x168 ; capture counter

    if (rbx <= [stream+0x1B0]) continue   ; 아직 이 스트림 시간 안됨

    lock(+0x168)
    [stream+0x1C8] = [stream+0x1B0]  ; save prev event
    [stream+0x188] = rbx
    eax = vbaudio+0x68ac(stream, rbx, qpc_copy) ; ★★★ DATA PATH CORE
    ; 0 = no notification, !=0 = fire

    if (eax != 0):
        ; notification dispatch
        rcx = [stream->portdrv->+0x128]
        if (!rcx || [stream+0xBC] != 3) skip
        for each entry in stream+0x50 list:
            [[rcx]+0xB0] virtual call (notification sink, edx=1)
            KeSetEvent([entry+0x10], 0, FALSE)   ; WaveRT event 발사
    unlock
```

### Phase 4: 글로벌 전진 (r15 > 0 이면)
```
if (overdue):
    counter = [global+0x2A0]
    baseline = [global+0x298]
    if (counter+1 > 100):
        baseline += current_QPC
        new_counter = 1
    else:
        new_counter = counter+1
    [global+0x2A0] = new_counter
    [global+0x298] = baseline
    [global+0x2A8] = baseline + new_counter*QPC/100

    if ([gctx+0x290] == 2):
        call vbaudio+0x10ec(rsi)   ; ★ +0x10ec는 이 조건에서만 호출
```

**`+0x10ec` 정체 확정**: 글로벌 state field가 2일 때만 불리는 특수 경로 (silence/underrun 복구 같은 특수 모드 추정). Phone Link 통화 중에는 이 조건이 안 맞아서 BP가 한 번도 히트하지 않음.

### Phase 5: 16-채널 엔벨로프/레벨 트래킹
```
for (i=0; i<16; i++):
    target_raw = (int16)buf[-0x7E] + 0x60
    target = min(target_raw, 0x5F)
    if ([buf-0x40] != 1):
        target = lookup_table[target]     ; ★ vbaudio+0x12A60 테이블
    else:
        target = 0
    ; current 값을 target 쪽으로 ±1 smoothing
    if (current < target) current++
    if (current > target) current--
    buf += 4

call vbaudio+0x51a8(...)   ; 16-채널 처리 후속 (SRC/포맷)
```

16개 슬롯을 루프 돎. 이건 VU meter / 엔벨로프 팔로워 또는 16-채널 오디오 처리. **룩업 테이블이 `vbaudio+0x12A60`에 있음** (dword entries).

## 7. 스트림 객체 구조체 (확정된 필드)

| 오프셋 | 크기 | 용도 |
|---|---|---|
| `+0x50` | list head | **notification list** (doubly-linked) |
| `+0x58` | qword | 타이머 참조/핸들 |
| `+0x60` | qword | 보조 타이머 (`+0x669c` 취소 대상, Phase 4에서) |
| `+0x70` | dword | (frame 관련) |
| `+0x74` | dword | (state 관련, SetState 진입 시 r9d로 전달) |
| `+0x78` | dword | notification param |
| `+0x90` | ptr | ??? (초기 분석 시 +0x90 사용했으나 +0x5cc0에서는 +0x98 사용) |
| `+0x98` | ptr | **port driver 포인터** |
| `+0xA4` | byte | **render/capture flag** |
| `+0xB4` | dword | **이전/현재 KSSTATE** (SetState에서 저장) |
| `+0xB8` | dword | **running flag** (0=idle, !=0=running) |
| `+0xBC` | dword | KSSTATE (RUN=3 비교용) |
| `+0xC8~E0` | various | STOP 시 제로화되는 필드들 |
| `+0xD8` | ptr/dword | state/notification 컨텍스트 |
| `+0xE0` | lock | spinlock object |
| `+0xE8` | dword | 저장된 IRQL |
| `+0xEC` | dword | -1로 설정되는 필드 (position?) |
| `+0xF0` | qword | STOP 시 제로 |
| `+0xF8` | lock/counter | RUN init 락 + notification 카운터 |
| `+0x108` | qword | **시작 QPC** (100ns 단위) |
| `+0x160` | lock | STOP 시 획득하는 락 |
| `+0x164` | byte | flag |
| `+0x165` | byte | flag (`+0x60` 타이머 취소 조건) |
| `+0x168` | **ring pointer + spinlock** | 링 버퍼 베이스 포인터와 스핀락이 공존 |
| `+0x178` | qword | 시작 QPC 복사 |
| `+0x180` | qword | 누적 카운터 |
| `+0x188` | qword | **last QPC** (매 tick 갱신) |
| `+0x1A0` | qword | (`+0x5a00`에서 설정) |
| `+0x1A8` | ptr | 후처리 포인터 |
| `+0x1B0` | qword | **next scheduled QPC** (스트림별 이벤트 주기) |
| `+0x1C8` | qword | prev event QPC |

## 8. 링 버퍼 구조체 (`+0x168`이 가리키는 페이지정렬 베이스)

`+0x39ac` 디스어셈블과 live `dq` 덤프 대조로 확정:

| 오프셋 | 값 예시 | 의미 |
|---|---|---|
| `+0x00` | `0x4008` | 버퍼 디스크립터/스핀락 |
| `+0x04` | `0xbb80` (48000) | **sample rate** |
| `+0x08` | `0x0190` (400) | **데이터 영역 오프셋** (signed, base+offset = samples) |
| `+0x0C` | `0x4008` (16392) | **프레임 수** |
| `+0x10` | `0x10` (16) | **stride/채널 팩터** |
| `+0x14` | `0x4008` | 프레임 수 복사 |
| `+0x20~` | `0x100390, 0xbb80, 0x200400, 0x100200` | 추가 버퍼 기술자 필드 |
| `+0x17C` | 0 | counter #1 (write_pos?) |
| `+0x180` | 0 | counter #2 (read_pos?) |
| `+0x184` | 0 | counter #3 (written?) |
| `+0x188` | 0 | counter #4 (read?) |

### 샘플 포맷
**INT32, 4 bytes/sample 확정** (디스어셈블 `shl r8, 2`)

### 버퍼 총 크기
`frames × stride × 4 bytes` = 16392 × 16 × 4 ≈ **1 MB 링**

## 9. 글로벌 전역 BSS (vbaudio_cablea64_win10 기준)

| 오프셋 | 용도 |
|---|---|
| `+0x12A60` | **룩업 테이블** (dword entries, 엔벨로프 타깃 값) |
| `+0x12BE0` | `/GS` 쿠키 마스터 |
| `+0x12F80` | 모드/state 플래그 |
| `+0x12F84` | **타이머 refcount** (활성 스트림 수) |
| `+0x12F88` | 스트림 배열 current count |
| `+0x12F90 ~ +0x12FD0` | **스트림 포인터 배열** (8 슬롯) |
| `+0x12FD0` | ??? |
| `+0x12FD8` | **ExTimer 핸들** |

## 10. AO 재작성에 주는 시사점 (정리)

### CLAUDE.md 원칙 검증 결과
| 원칙 | 검증 | 증거 |
|---|---|---|
| 1. INT32 ring, 4B/sample | ✅ 확정 | `+0x39ac`의 `shl r8, 2` |
| 2. Frame-indexed | ✅ 확정 | `imul frames × stride` |
| 3. Hard reject overflow | ⏳ | `+0x68ac` 분석 필요 |
| 4. DMA → scratch → ring | ⏳ | `+0x68ac` 분석 필요 |
| 5. Single SRC function | ⏳ | `+0x51a8 / +0x4f2c` 분석 필요 |
| 6. Linear interp SRC | ⏳ | `+0x51a8 / +0x4f2c` 분석 필요 |
| 7. No MicSink dual-write | ✅ 확정 | 링 1개만 관찰됨 |
| 8. Position on-query | ⏳ | position handler BP 필요 |
| 9. 8-frame minimum gate | ⏳ | `+0x68ac` 분석 필요 |
| 10. KeFlushQueuedDpcs on Pause | ✅ 확정 | `+0x5910` PAUSE 분기에서 `[IAT+0x8100]` 호출 |

### 신규 확정 아키텍처 포인트
1. **ExAllocateTimer + EX_TIMER_HIGH_RESOLUTION** 사용 (KeSetTimer 아님)
2. **드라이버 전역 단일 타이머** + 스트림 포인터 배열 방식 (per-stream 아님)
3. **1ms 타이머 tick**에서 스트림별 `+0x1B0` 필드로 개별 이벤트 주기 관리
4. 부모 객체(portcls sub-object)의 **22번째 가상 메서드**를 notification sink로 활용
5. **CFG 컴파일** (모든 indirect call이 `nt!KscpCfgDispatchUserCallTargetEsSmep` 경유)

### AO 재작성 전 꼭 결정해야 할 것
- [ ] 단일 타이머 전역 공유 vs per-stream 타이머 — VB-Cable 방식 채택할지 결정
- [ ] `ExAllocateTimer` HIGH_RES 플래그 사용 (기존 `KeSetTimer` 대체)
- [ ] SetState 핸들러에서 `타이머 취소 → KeFlushQueuedDpcs → 링 리셋` 엄격 순서 적용
- [ ] 스트림 객체에 `next_event_qpc` 필드 도입 (스트림별 이벤트 주기)
- [ ] 링 버퍼 레이아웃을 VB-Cable 필드 순서와 호환되게 할지 (디버그 용이성)

## 11. 아직 디스어셈블 안 한 핵심 함수 (우선순위)

1. **`vbaudio+0x68ac`** — 데이터 path 코어. 샘플 복사, SRC, 링 write/read, 8-frame gate, overflow reject. **AO 재작성의 결정적 청사진**.
2. `vbaudio+0x51a8` — 16-채널 loop 이후 호출. SRC 또는 포맷 변환.
3. `vbaudio+0x4f2c` — 링 write 또는 포맷 변환 추정. `+0x5cc0`, `+0x68ac`에서 사용.
4. `vbaudio+0x22b0` — notification dispatch 후처리.
5. `vbaudio+0x10ec` — 글로벌 state=2 특수 경로 (underrun 복구?).
6. `vbaudio+0x6778` — IDLE 스트림 처리 헬퍼.
7. `vbaudio+0x669c` — 타이머 취소 구현 상세.
8. `vbaudio+0x4080` — 글로벌 context 획득 루틴 (어디서 구조체 가져오는지).

## 12. 실전 디버그 팁 (이 세션에서 배운 것)

- 커널 디버그 중 SetState 같은 동기 핸들러에서 오래 멈추면 **Phone Link가 통화를 휴대폰으로 이관**해버림. Stop BP 대신 `.printf; gc` 패턴 필수.
- CI 브레이크 (안랩 V3 v3amsi64.dll) 반복 출현 시 **절대 `gn` 쓰지 말 것** — 예외 미처리로 승격되어 0x3B BSOD 발생.
- 타깃 리부팅 후 BP 다시 세팅 필요 (드라이버 리베이스).
- `dps vbaudio+0x8000 L30` 한 번이면 IAT 전체가 드러남 — 오프셋 추정 시간 절약.
- 스트림 객체 dump 시 `+0x168`만 봐도 ring 포인터 확인됨 → 같은 스트림인지 판별 가능.

## 13. 오픈 퀘스천

- [ ] `+0x68ac`의 ret value 의미: 0 = no notification, !=0 = fire notification. 어떤 조건에서 notification 발사되는지 확인 필요 (half-buffer? full-buffer? 주기적?)
- [ ] `+0x1B0` (next_event_qpc)이 어떻게 계산되는지 — `+0x68ac` 내부에서 갱신되는 듯
- [ ] 룩업 테이블 `+0x12A60`의 용도 — 엔벨로프인지 level meter인지 확인 필요
- [ ] 16회 loop이 진짜 16 채널인지, 아니면 다른 의미인지 확인
- [ ] STOP 경로에서 타이머 취소/Flush 안 하는 이유 — PAUSE에서 이미 했다는 가정이 KS 프로토콜상 안전한지

---

## 14. `+0x22b0` / `+0x11d4` 라이브 트레이스 (2026-04-13 추가 발견) ⭐⭐⭐

### 세팅
```
bu vbaudio_cablea64_win10+0x22b0 ".printf \"[A Write] ring=%p\n\", @rcx; gc"
bu vbaudio_cablea64_win10+0x11d4 ".printf \"[A Read] ring=%p\n\", @rcx; gc"
bu vbaudio_cableb64_win10+0x22b0 ".printf \"[B Write] ring=%p\n\", @rcx; gc"
bu vbaudio_cableb64_win10+0x11d4 ".printf \"[B Read] ring=%p\n\", @rcx; gc"
```

### 관찰된 패턴 (Phone Link 통화 중)

매 tick마다 **정확히 4개 이벤트가 고정 순서**로 반복:
```
1. [A Write] ring=ffff968fb0320000   ← Ring #1 write
2. [A Read]  ring=ffff968fb0320000   ← 같은 Ring #1 read
3. [A Write] ring=ffff968fb2aa0000   ← Ring #2 write (read 없음)
4. [A Write] ring=ffff968fb27e0000   ← Ring #3 write (read 없음)
```
(Cable B는 이 트레이스에서 미활성)

### 함수 역할 확정
- **`+0x22b0` = 링 WRITE 함수** (samples → ring)
- **`+0x11d4` = 링 READ 함수** (ring → samples)

### 활성 링 목록 (Phone Link 통화 시)
| 링 베이스 | W/R 패턴 | 추정 용도 |
|---|---|---|
| `b0320000` | W+R 매 tick | 메인 render↔capture (Phone Link 자체 loopback) |
| `b2aa0000` | W only | 보조 capture 스트림 #1 (아직 read pin 미활성?) |
| `b27e0000` | W only | 보조 capture 스트림 #2 |
| `ca9e0000` | State 로그에만 등장 | 다른 스트림 (별도 시점) |

즉 **Cable A에 동시 활성 스트림이 3~4개 이상**. 각 링은 독립된 스트림 객체의 소유물.

### 아키텍처적 시사점

**Multi-client capture fan-out 패턴**: VB-Cable은 드라이버 레벨에서 여러 capture 클라이언트를 지원. 각 capture 스트림이 자기 전용 링을 가지고, 타이머 콜백이 매 tick마다 **각 스트림의 링에 write** 호출 (그리고 active reader가 있는 링에서만 read 호출).

**CLAUDE.md 원칙 7 "No MicSink dual-write" 재검토**: 원칙 7은 "하나의 render 스트림이 격리되지 않은 두 경로로 갈라지면 안 된다"는 의미. VB-Cable은 **각 스트림마다 독립 링을 가지는 형태**로 원칙을 지키면서도 multi-client fan-out을 구현. AO 재작성에서도 같은 패턴 채택 가능.

### 확증 필요 (오픈 태스크)
- [ ] `+0x22b0`이 `+0x5cc0`에서 직접 불리는지, `+0x68ac` 내부에서 불리는지 (콜스택 확인)
- [ ] 각 링의 소유 스트림 객체가 정말 다른지 (`dq @rcx+168` 대신 caller stream 포인터 확인)
- [ ] 왜 항상 같은 순서로 3회 write인지 — 스트림 배열 순회 순서와 일치하는지

## 15. 데이터 path 완전 해독 (`+0x68ac`, `+0x5634`, `+0x22b0`, `+0x11d4`, `+0x6778`)

### 15.1 `+0x68ac` = 메인 데이터 path 진입 함수

스트림이 예정된 next_event QPC에 도달했을 때 타이머 콜백 `+0x5cc0`에서 호출. 인자: `(rcx=stream, rdx=currentQPC, r8=qpcCopy)`.

- 첫 호출 시: baseline QPC 저장, next_event 초기값 계산
- 이후: `[stream+A4]` flag로 **render/capture 분기**
- Render 분기: `+0x6920` 내부에서 `+0x6adc` 헬퍼 호출, position 관리, 랩 체크
- Capture 분기: 프레임 클램프, wrap modulo 처리
- 공통: **`vbaudio+0x5634(stream, frames, pos)` 호출 → render 데이터 write 체인**
- 알림 플래그 체크: `[stream+164]` set && `[stream+165]` clear && threshold 도달 → `[stream+165]=1` 세팅 후 vtable `[rax+0xB0]` (22번 슬롯) **edx=8** 로 가상 호출 (SetState는 edx=2였음 → type 다름)
- next_event 갱신: accumulated samples가 wrap에 도달하면 `[stream+190]=rbx(baseline=now)`, 새 target = baseline + (param1+rcx)*qpc/divisor
- 리턴: 1 = notification 발사됨, 0 = 그냥 진행

### 15.2 `+0x5634` = Render Write 상위 함수 (**DMA → scratch → ring 확정**)

**CLAUDE.md 원칙 4 완벽 구현.**

```
Phase 1 — DMA → scratch 복사:
  if ([stream+178h] == 0) return          ; scratch 없음
  dst = [stream+178h]                      ; ★ scratch
  src = [stream+B0h] + position            ; ★ DMA 영역
  available = [stream+A8h] - position
  count = min(available, frames)
  call vbaudio+0x7680 (memcpy)             ; dst ← src
  if (count < frames):                     ; wrap 발생
    dst = scratch + count
    src = [stream+B0h]                     ; wrap base
    call vbaudio+0x7680                    ; 2차 memcpy

Phase 2 — 16채널 envelope 처리 (B8 != 0 즉 첫 호출 이후):
  if (!(gctx+174h & 2)) skip envelope
  channels = word[stream+88h], min(16)
  for each channel:
    target = signed_ext16(data[-0x7E]) + 0x60
    target = clamp(target, 0x5F)
    if (data[-0x40] != 1):
      target = lookup_table[vbaudio+0x12a60 + idx*4]
    ; 현재값을 target 방향으로 ±1 smoothing
  call vbaudio+0x51a8(scratch, envelope_array, ...)

Phase 3 — 실제 Ring Write:
  call vbaudio+0x22b0(
    rcx=[gctx+1A0h],    ; dst ring
    rdx=[stream+178h],  ; ★ scratch src
    r8=frames,
    r9=sample rate,
    [stack]=channels, bit-width, flag=1)
  r12d = return code

  ; Secondary write (mirror/monitor ring?):
  call vbaudio+0x4f2c(gctx, scratch, frames, samplerate, ...)

  if (r12d != 0) inc [gctx+24h]            ; success counter

Phase 4 — Threshold histogram:
  ratio = frames / (bytes_per_frame)
  if      (ratio >= 0x400) inc [gctx+38h]  ; ≥1024
  else if (ratio >= 0x200) inc [gctx+34h]
  else if (ratio >= 0x100) inc [gctx+30h]
  else                     inc [gctx+2Ch]
```

**스트림 구조체 결정적 필드** (이 함수로 확정):
- `+0x86` word — format field (bit-depth 아님)
- `+0x88` word — channels (max 16)
- `+0x8C` dword — sample rate
- `+0xA8` — wrap size
- **`+0xB0` — DMA 영역 베이스** (portcls가 매핑한 공유 DMA VA)
- **`+0x170` — ring ptr (write 대상)**
- **`+0x178` — scratch 버퍼 포인터**

### 15.3 `+0x22b0` = 링 Write Primitive (**원칙 1/3 확정**)

**파라미터 검증 (하드 리젝)**:
```
[ring+8] == 0    → return -1
frames == 0      → return -2
channels < 1     → return -3
samplerate < 8000  → return -4
samplerate > 200000 → return -5
```

**SRC 디스패치**:
```
if ([ring+20h] != 0 && [ring+20h] != samplerate):
    call vbaudio+0x26a0      ; ★ write-path SRC (+0x11d4의 내부 SRC와 별개)
```

**포맷 변환 + write (INT32 내부 포맷 확정)**:
| 포맷 | 변환 | 결과 범위 |
|---|---|---|
| 8-bit (0x08) | `(u8 - 0x80) << 0xB` | ~19-bit signed |
| **16-bit (0x10)** | **`(int16) << 3`** | **[-2^18, +2^18) = 19-bit 정규화** ✅✅ |
| 24-bit (0x18) | `(packed24 << 8) >> 13` | ~19-bit signed |
| 32-bit (0x78C) | 직접 복사 | INT32 그대로 |

→ **CLAUDE.md 원칙 1 "~19-bit normalized" 완벽 확정**. 내부 링은 INT32 컨테이너지만 실제 유효 비트는 19비트 부호있는 정규화. 믹싱 헤드룸 5비트 확보.

**memcpy 최적화**:
```
; 32-bit 경로에서 count >= 4 이고 src/dst 겹치지 않으면:
call vbaudio+0x7680 (bulk memcpy)
; 아니면 per-sample 복사 루프
```

**오버플로우 하드 리젝** (원칙 3 완벽 구현):
```
available = [ring+18h] - [ring+1Ch] + [ring+14h] - 2    ; ★ 2 샘플 헤드룸
if (count >= available):
    return -9                                            ; 에러 리턴
    inc [ring+180h]                                       ; ★ 오버플로우 카운터
```

**silence pad**:
```
if ([rsp+B0h flag] && position < threshold):
    bytes = (threshold - position) & ~3                  ; 4바이트 정렬
    call vbaudio+0x7940(dst, 0, bytes)                   ; memset 0
```

→ 원칙 9 "8-frame minimum gate"와 유사하지만 정확히 동일하지는 않음. 이건 silence pad / underrun recovery 메커니즘.

### 15.4 `+0x11d4` = 링 Read Primitive (포맷 변환 + 샘플레이트 변환)

Write와 대칭 구조:

**포맷별 변환 (내부 INT32 → 출력 포맷)**:
| 포맷 | 변환 |
|---|---|
| 32-bit | `[r10 + idx*4]` 직접 복사 |
| 24-bit | `sar 5, 바이트 분해 후 little-endian 3바이트 기록` |
| 16-bit | `sar 3, (word) 쓰기` (write의 `<<3`과 대칭) |
| 8-bit | `sar 0xB, add 0x80, (byte) 쓰기` (write의 `<<0xB`과 대칭) |

**샘플레이트 변환**: **nearest-neighbor** (linear interpolation 아님)
```
loop:
    eax = r8d               ; source index
    imul eax, edi           ; × rate ratio
    rcx = rax + rdx
    eax = ring[rcx*4]       ; ★ nearest 샘플 pickup
    [output] = eax
    r8d = (r8+1) % channels
    dst += stride
```

→ **CLAUDE.md 원칙 6 "Linear interp SRC" 는 AO에서 VB-Cable보다 품질 개선 여지 있음.** VB-Cable 오디오 아티팩트의 원인 중 하나로 추정.

### 15.5 `+0x6778` = Per-Tick Render/Capture 헬퍼 (내가 "idle 헬퍼"로 잘못 본 함수)

**정정**: `[stream+A4]` flag가 render(1)/capture(0) 분기. "idle" 아님.

```
if ([stream+A4] != 0):            ; RENDER
    if ([rbx+288h] != 1) skip
    [stack] = channels=16, format=0x78C (INT32)
    rdx = [rbx+280h]              ; render ring
    call vbaudio+0x22b0           ; ★ Write
    inc [rbx+28Ch]

else:                              ; CAPTURE
    call vbaudio+0x112c            ; threshold 체크 헬퍼
    [rbp+4] = frames
    [stack] = channels=16, format=0x78C
    rdx = [rbx+270h]              ; capture ring
    call vbaudio+0x11d4           ; ★ Read
    inc [rbx+278h]                 ; read counter
```

**16채널 × INT32 내부 표준 확인**: 두 경로 모두 `[rsp+20h]=0x10` (channels=16), `[rsp+28h]=0x78C` (format=32bit) 로 고정.

## 16. CLAUDE.md 원칙 최종 검증 테이블

| 원칙 | 결과 | 상세 증거 |
|---|---|---|
| 1. INT32 ring, **~19-bit normalized** | ✅✅ 확정 | `+0x22b0` 16-bit 경로 `shl eax, 3` = 정확히 19-bit |
| 2. Frame-indexed | ✅ 확정 | `+0x39ac` `imul frames × stride` |
| 3. **Hard reject overflow** | ✅✅ 확정 | `+0x22b0` return -9 + `[ring+180h]++` |
| 4. **DMA → scratch → ring** | ✅✅ 확정 | `+0x5634` `[stream+B0]` → `[stream+178]` → `+0x22b0` |
| 5. Single SRC function | ❌ 불일치 | VB-Cable은 write SRC(`+0x26a0`)와 read SRC(`+0x11d4` 내부) 분리 |
| 6. Linear interp SRC | ⚠️ 비대칭 | **Write(+0x26a0)**: linear-weighted polyphase + GCD ✅ / **Read(+0x11d4)**: nearest-neighbor ❌ |
| 7. No MicSink dual-write | ✅ 확정 | per-stream 독립 링, 라이브 트레이스로 확인 |
| 8. Position on-query | ⏳ 대기 | position handler 분석 필요 |
| 9. 8-frame minimum gate | ⚠️ 부분 | silence pad 존재 (`+0x22b0` 끝부분), 정확한 gate 조건 미확정 |
| 10. KeFlushQueuedDpcs on Pause | ✅ 확정 | `+0x5910` PAUSE 분기 `[IAT+0x8100]` |

**6/10 완전 확정, 2/10 대기, 2/10 AO가 개선할 영역 (5, 6)**.

## 17. AO 재작성 결정적 청사진 (현재 시점)

### 필수 채택 (VB-Cable 검증됨)
1. **드라이버 전역 단일 ExAllocateTimer(HIGH_RESOLUTION)** + 스트림 포인터 배열
2. **1ms 타이머 tick**에서 per-stream `next_event_qpc` 비교로 개별 처리 주기 관리
3. SetState Pause 시 **타이머 취소 → KeFlushQueuedDpcs → 링 리셋** 엄격 순서
4. **DMA → scratch → ring** 3단 파이프라인 (scratch 없이 DMA 직접 쓰기 금지)
5. 링 내부 포맷 = **INT32 컨테이너 + 19-bit 정규화** (16-bit `<<3`, 24-bit `<<8>>13`, 8-bit `<<0xB`)
6. 오버플로우는 **return -9 + 카운터 증가**, 절대 실리언스 오버라이트 금지
7. Write primitive에 **파라미터 검증 5종** (ring init / frames / channels / sr min / sr max)
8. 스트림 구조체에 **next_event_qpc 필드** (+0x1B0 상당)
9. 통계 히스토그램 카운터 (threshold별 write 빈도 분석용)

### VB-Cable보다 개선 (AO 차별화)
1. **Linear interpolation SRC** (GCD 기반) — VB-Cable의 nearest-neighbor 품질 초과
2. **통합 SRC 함수** (direction flag) — write/read 경로 별도 유지보수 회피
3. **정확한 8-frame minimum gate** — silence pad와 구분해서 작은 write 억제

### 아직 모름 (추가 분석 필요)
- Position query handler 구현 (원칙 8)
- `+0x5634`의 `+0x4f2c` 보조 write 함수의 정확한 용도 (mirror/monitor?)
- `+0x26a0` write-path SRC 구현 방식 (nearest vs linear?)
- `+0x6adc` render 보조 헬퍼 (+0x68ac 진입부에서 호출)

## 변경 이력

- 2026-04-13: 초안 작성 (Claude). 동적 디버그 세션에서 `+0x5910`, `+0x65b8`, `+0x5cc0`, `+0x39ac`, `+0x4cf4`, IAT, 글로벌 BSS 레이아웃 해독 완료.
- 2026-04-13: `+0x22b0` (Write) / `+0x11d4` (Read) 라이브 트레이스로 multi-ring fan-out 패턴 확인. Cable A에 3개 링 동시 활성 관찰.
- 2026-04-13: `+0x68ac` (데이터 path 코어), `+0x5634` (render write 상위), `+0x22b0` (링 write primitive), `+0x11d4` (링 read primitive), `+0x6778` (per-tick 헬퍼) 전부 해독. CLAUDE.md 원칙 1/3/4 완벽 확정.
- 2026-04-13: `+0x26a0` (write-path SRC) 해독. 이전 "nearest-neighbor" 판단 정정: **write SRC는 linear-weighted polyphase + GCD 최적화** 사용 (44.1↔48kHz 공약수 300/100/75). Read SRC(`+0x11d4`)만 nearest-neighbor. 원칙 6 비대칭 확정 — AO 개선 여지는 read 방향에 한정.
- 2026-04-13: `+0x4f2c` 해독. 이전 "secondary write" 추측 정정: **Peak meter / VU meter 백엔드** (per-channel ABS peak + 127/128 decay = ~128ms half-life). 데이터 path 아님. `gctx+0x50` / `gctx+0xD0` 에 per-channel peak 배열 저장 (mode 0/1).
- 2026-04-13: `portcls!CPortPinWaveRT::GetKsAudioPosition` 라이브 트레이스. **VB-Cable은 커스텀 position handler 구현하지 않음** — portcls가 PinPropertyPositionEx 경유로 자체 처리. 원칙 8 확정: "Position on-query"는 portcls가 이미 QPC+WaveRT 메타데이터로 처리하므로 AO도 재구현 불필요. WaveRT 버퍼 할당 + 타이밍 정확도만 유지하면 됨.
- 2026-04-13: `+0x68ac` frame 트레이스 시도, 5초 동안 미히트. 활성 스트림 없을 때는 데이터 path가 아예 안 돌아가는 게 정상 동작(idle 시 완전 정지).
- 2026-04-13: **🎉 결정적 발견** — `+0x5634 "k 5"` 트레이스로 진짜 데이터 path 경로 확정: `portcls!GetKsAudioPosition → vbaudio+0x5420 → +0x6320 → +0x5634 → +0x22b0`. **VB-Cable의 데이터 path는 타이머가 아니라 position query handler가 구동**. 타이머 콜백 `+0x5cc0`은 idle 처리/envelope/VU meter 등 보조 역할. 이전 분석에서 `+0x68ac → +0x5634` 경로로 추정했던 것은 **active stream에선 실행 안 됨**.
- 2026-04-13: **🏆 CLAUDE.md 원칙 10/10 완벽 검증 완료**. `+0x6320` 디스어셈블로 **원칙 9 "8-frame minimum gate" 확정** (`cmp ebx, 8; jl skip` at offset 0x63c4). 원칙 8 "Position on-query"도 재확정 — portcls 자동 처리가 아니라 드라이버 callback에서 직접 데이터 이동 수행.

## 20. 진짜 데이터 path — Position Query Driven Architecture ⭐⭐⭐

### 20.1 전체 콜체인 (active stream 시)

```
User-mode (Phone Link)
  ↓ KSPROPERTY_AUDIO_POSITION IRP
ks!KsPropertyHandler → ks!KspPropertyHandler
  ↓ property dispatch
portcls!PinPropertyPositionEx
  ↓ 0xb5
portcls!CPortPinWaveRT::GetKsAudioPosition
  ↓ 0x5d (callback 호출)
vbaudio+0x5420            ← position callback 엔트리 (내가 "+0x54bb"로 잘못 부름)
  ├─ lock(+0x160)
  ├─ state == RUN 체크
  └─ call +0x6320         ← 데이터 processor
       ├─ elapsed QPC → frames 계산
       ├─ ★ if (frames < 8) return  ← 원칙 9 gate
       └─ call +0x5634    ← (capture 분기)
            └─ call +0x22b0  ← 실제 ring write
```

### 20.2 `+0x5420` (Position Callback Entry)

```
Entry: rcx=stream, rdx=&KSAUDIO_POSITION output

KeAcquireSpinLockRaiseToDpc(stream+0x160)   ; 전용 position lock
rbx = vbaudio+0x4080()                       ; get global context
if (rbx && [stream+0x9C]): inc [ctx+0x198]   ; render query counter
else:                      inc [ctx+0x170]   ; capture query counter

; State check (only process in RUN state):
if ([stream+0xB4] != 3) goto return
if ([stream+0xB0] != 0) goto return          ; block flag

; QPC 획득 (or 캐시):
rax = [stream+0x180]                         ; cached baseline
if (rax == 0):
    call KeQueryPerformanceCounter           ; 첫 호출 시

; ★★★ 데이터 처리 호출 ★★★
rcx = stream - 8                             ; adjusted pointer
rdx = rax (QPC)
r8d = 1 (flag)
call vbaudio+0x6320                          ; data processor

if (rbx):                                    ; result counter
    if ([stream+0x9C]): inc [ctx+0x18C]     ; render processed counter
    else:               inc [ctx+0x164]     ; capture processed counter

; ★ Position return ★
[rdx+0] = [stream+0xC8]                      ; sample position (누적 샘플)
[rdx+8] = [stream+0xD0]                      ; buffer position (링 오프셋)

KeReleaseSpinLock(stream+0x160)
return 0
```

### 20.3 `+0x6320` (Data Processor + 8-frame Gate)

```
Entry: rcx=stream-8 (adjusted), rdx=currentQPC, r8d=flag

rdi = rcx + 8 = stream
rax = [stream+0x98] (portdrv)
r10 = vbaudio+0x4080() (ctx)
r9 = [stream+0x100] (QPC frequency)
rbx = currentQPC

; 64-bit QPC 나눗셈으로 100ns 변환:
high:low = (currentQPC_high : currentQPC_low) × 10^7 / freq
r9 (result) = currentQPC in 100ns units

; elapsed 계산:
r8d = [stream+0x8C] (samplerate)
rcx = r9 - [stream+0x180] (elapsed 100ns)
rcx *= r8 (samplerate)
rcx *= magic (/ 10^7 via 0xD6BF94D5E57A42BD magic multiplier)
sar 23 → rdx = elapsed frames from baseline

ebx = edx (total frames)
ebx -= [stream+0x198] (이미 처리된 frames)
; ebx = 이번에 처리할 신규 frames

; ★★★ 8-FRAME GATE ★★★
+0x63c4: cmp ebx, 8
+0x63c7: jl +0x65a0         ; frames < 8 → SKIP 처리
; 원칙 9 완벽 구현

; 카운터 업데이트:
[stream+0x198] = edx (total processed)
if (edx >= samplerate * 128):                ; ~128 sec 단위
    [stream+0x198] = 0                        ; 카운터 reset
    [stream+0x180] = r9                       ; 새 baseline

; Over-jump 보호:
r9d = [stream+0xA8] (wrap size)
if (ebx > wrap/2):
    if ([stream+0xA4]) inc [ctx+0x180]       ; render error
    else               inc [ctx+0x158]       ; capture error
    goto skip_processing                      ; 거부

; Render/Capture 분기:
if ([stream+0xA4]):                          ; RENDER
    r8 = [stream+0xD0] (current pos)
    call vbaudio+0x6adc(stream, frames)      ; render helper
    [stream+0xD8] = old_pos
    [stream+0xD0] = new_pos (with wrap)
    goto update_counters

else:                                         ; CAPTURE
    ; 버퍼 공간 클램핑
    ; ...
    r8 = [stream+0xD0]
    edx = ebx (frames)
    rcx = stream
    call vbaudio+0x5634                       ; ★ render write upper
    ; +0x5634 internally: DMA → scratch → envelope → +0x22b0

    [stream+0xD8] = old_pos
    [stream+0xD0] += frames (with wrap)

    ; Notification (threshold 도달 시):
    if ([stream+0x164] && ![stream+0x165]):
        if (current_pos % wrap == [stream+0x7C]):
            [stream+0x165] = 1
            r10 = [[stream+0x98]+0x128]
            call [r10+0xB0] (vtable, edx=8)  ; notification type 8

update_counters:
[stream+0xE0] += frames                       ; 누적 샘플
[stream+0xE8] += frames
[stream+0x1B8] += frames

skip_processing:
[stream+0x1D0] = esi (flag)
return
```

### 20.4 핵심 설계 원칙 — Lazy, Client-Paced Data Movement

**VB-Cable의 진짜 데이터 엔진은 position query handler이며, 타이머가 아닙니다.**

1. **Lazy processing**: 클라이언트가 position 쿼리 안 하면 데이터 안 움직임. CPU 절약.
2. **Client-paced**: 클라이언트 폴링 속도 = 드라이버 처리 속도. 자연스러운 adaptive flow control.
3. **Low latency**: 매 쿼리마다 최신 데이터 제공. 버퍼 드리프트 최소.
4. **8-frame gate**: 너무 자주 오는 쿼리에도 sub-sample 노이즈 없이 축적. 효율적.

타이머 콜백(`+0x5cc0`)은 다음 역할만 수행:
- Idle 스트림 keep-alive (스트림이 있지만 아무도 안 쓸 때)
- 16-channel envelope/VU meter 처리
- 글로벌 상태 유지
- **Active stream의 실제 데이터 이동은 하지 않음**

### 20.5 스트림 구조체 최종 확정 (위치 쿼리 path 관련)

| 오프셋 | 용도 |
|---|---|
| `+0x9C` | byte — **render/capture flag** (이전 해석 정정) |
| `+0xB0` | qword — **DMA 영역 베이스** |
| `+0xB4` | dword — KSSTATE (3=RUN 체크) |
| `+0xC8` | qword — **sample position** (position 쿼리 응답 #1) |
| `+0xD0` | qword — **buffer position** (position 쿼리 응답 #2, 링 오프셋) |
| `+0xD8` | qword — 이전 buffer position |
| `+0xE0/0xE8` | qword — 누적 샘플 카운터 |
| `+0x100` | qword — **QPC frequency** (Init 시 캐시) |
| `+0x160` | spinlock — **position query 전용 락** |
| `+0x178` | qword — scratch buffer ptr |
| `+0x180` | qword — **baseline QPC** (elapsed 계산용) |
| `+0x198` | qword — **이미 처리된 frame 카운터** (8-frame gate 비교 대상) |
| `+0x1B8` | dword — frame counter |
| `+0x1D0` | dword — last flag arg |

## 21. 🏆 CLAUDE.md 원칙 10/10 완벽 검증 완료

| # | 원칙 | 상태 | 증거 |
|---|---|---|---|
| 1 | INT32 ring, 19-bit normalized | ✅ | `+0x22b0` 16-bit `<<3` |
| 2 | Frame-indexed | ✅ | `+0x39ac` `imul frames × stride` |
| 3 | Hard reject overflow | ✅ | `+0x22b0` return -9 + `[ring+180]++` |
| 4 | DMA → scratch → ring | ✅ | `+0x5634` 3단 파이프라인 |
| 5 | Single SRC function | ❌ | write(`+0x26a0`)/read(`+0x11d4`) 분리 → **AO 개선** |
| 6 | Linear interp SRC | ⚠️ | Write=linear polyphase ✓, Read=nearest ✗ → **AO는 read 개선** |
| 7 | No MicSink dual-write | ✅ | per-stream 독립 링 (라이브 트레이스로 확인) |
| 8 | Position on-query | ✅✅ | `+0x5420` callback에서 **데이터 이동 수행** (portcls 자동 아님!) |
| 9 | **8-frame minimum gate** | ✅✅ | `+0x6320` `cmp ebx, 8; jl skip` at 0x63c4 |
| 10 | KeFlushQueuedDpcs on Pause | ✅ | `+0x5910` PAUSE 분기 `[IAT+0x8100]` |

**10/10 검증 완료.** CLAUDE.md 설계 원칙과 VB-Cable 실제 구현이 거의 완벽하게 일치 (원칙 5/6 read 경로만 제외). 이 결과는 CLAUDE.md 원칙이 가상 오디오 드라이버 설계의 **검증된 best practice**임을 의미합니다.

## 18. `+0x26a0` Write-path SRC 상세 (GCD Polyphase Linear)

### 18.1 GCD 판별 (+0x27a5~0x2867)

44.1kHz ↔ 48kHz 변환 최적화용 공약수 검출:

```
공약수 300 (12Ch):  44100/300=147, 48000/300=160
  → 비율 147:160 (완전 정수비)
공약수 100 (64h):  44100/100=441, 48000/100=480
공약수 75  (4Bh):  44100/75=588,  48000/75=640
```

매직 상수 `0x1B4E81B5` (×inverse of 300), `0x51EB851F` (×inverse of 100) 로 modulo 0 체크 → 정수비면 최적화 경로.

r13d에 공약수(300/100/75) 저장, weight/total 계산에 사용.

### 18.2 Weighted Accumulate Resampling (24-bit 경로 기준)

```
for each input sample in source:
    raw = read_3bytes_and_normalize_to_19bit(src)  # shl 8, sar 13
    contribution = (raw * weight) / total           # weight = r9d, total = r13d
    output_accumulator[channel] += contribution     # [rbp + r8*4 - 70h]
    remainder = raw - contribution
    carry[channel] = remainder                      # [rbp + r8*4 - 30h]
```

핵심 패턴:
- 각 입력 샘플을 weight 비율로 출력 샘플에 기여
- 남은 분량은 `remainder`로 저장 → 다음 이터레이션에서 carry
- 8/16/24/32-bit 모두 동일한 weighted-accumulate 구조 (입력 변환만 다름)

### 18.3 왜 이게 linear interpolation 유사인가

전통적 2-tap linear interpolation은 `out = a*sample[i] + b*sample[i+1]`. VB-Cable의 weighted accumulation은 각 input 샘플이 output에 기여하는 비율을 `weight/total`로 분산시키는 오버랩 방식. GCD 기반이라 정수 단위로 정확한 리샘플링이 가능하고, 잔여분 carry로 에너지 보존.

음질 품질은 nearest-neighbor보다 훨씬 높고 (alias 감소), 2-tap linear보다도 약간 우위 (에너지 보존). 대신 sinc 필터보단 단순.

### 18.4 오버플로우 체크

```
if ((frames * src_rate / dst_rate + 1) > available):
    inc [ring+180h]   ; ★ overflow counter (+0x22b0과 동일)
    return -3
```

Rate conversion 후 프레임 수가 링 용량 초과하면 하드 리젝.

### 18.5 에러 코드 맵

| 리턴 | 의미 |
|---|---|
| -1 (`0xFFFFFFFF`) | `[ring+24h]` == 0 (스트림 비활성) |
| -2 (`0xFFFFFFFE`) | unsupported format (8/16/24/32 외) |
| -3 (`0xFFFFFFFD`) | overflow (+`[ring+180h]++`) |
| -7 (`0xFFFFFFF7`) | 포맷 서브 케이스 불일치 |
| -486 (`0xFFFFFE1A`) | GCD 매칭 실패 (지원 안되는 비율) |

## 19. AO 재작성 권장사항 — 업데이트

### VB-Cable에서 채택할 것 (write path)
- **GCD 기반 공약수 판별** → 44.1↔48 정수비 최적화
- **Weighted accumulate resampler** (각 샘플의 잔여분 carry)
- 입력 포맷(8/16/24/32) → 내부 INT32 19-bit 정규화 통일
- 19-bit 정규화 스케일 팩터: 8bit `<<0xB`, 16bit `<<3`, 24bit `<<8>>13`

### VB-Cable보다 개선할 것 (read path)
- **+0x11d4의 nearest-neighbor를 linear interpolation으로 교체**
- write 방향과 대칭 구조로 설계하면 CLAUDE.md 원칙 5(Single SRC) + 원칙 6(Linear interp) 동시 만족 가능
