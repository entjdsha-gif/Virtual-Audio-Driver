# AO Virtual Cable V2 — Corrected Architecture Plan

> Date: 2026-04-11
> Branch: `feature/ao-pipeline-v2`
> Baseline: `main` @ b856d94
> Status: APPROVED — Phase 1 ready to implement

## Context

AO Virtual Cable의 실시간 통화 품질이 VB-Cable보다 떨어지는 근본 원인: Phone Link이 48k/16/2ch PCM으로 양쪽을 열지만, AO의 `FormatMatch`는 `Speaker==Mic==Internal(48k/24/8ch)`을 요구해서 항상 변환 경로를 탄다. VB-Cable은 같은 상황에서 무변환 통과.

### V2 Philosophy: Passthrough-First

1. **Phase 1 target**: 48k/16/2 PCM comms-first (Phone Link, Discord 등 실시간 통화 경로)
2. **V2 final target**: 모든 지원 포맷에 대해 same-format session이면 raw passthrough (PCM, float, mono~multichannel 전부)
3. **Conversion path = fallback only**: format mismatch일 때만 conversion 경로 사용

### Critical Design Decision: Ring-Read Mode

**Phase 1은 ring-read 모드를 선택한다** (MicSink direct push를 사용하지 않음).

근거:
- **Observability**: ring을 경유해야 PipeFillFrames, PushLoss, PullLoss 카운터가 의미를 가짐
- **Safety**: MicSink race condition을 Phase 1에서 걱정할 필요 없음
- **Simplicity**: ring write + ring read는 이미 가장 많이 테스트된 코드 경로
- **Latency**: ~1ms 추가 (1 DPC tick)는 comms 앱에서 무시 가능

Ring-read 강제 포인트 3개:
1. `LoopbackWrite()`: `!SessionPassthrough` MicSink push 가드
2. `WriteBytes()`: `!SessionPassthrough` MicSink no-op 가드
3. `UpdatePosition()`: `!SessionPassthrough` MicSink zero-fill 가드

---

## Phase 1: SessionPassthrough + Loss Counters + Frame Fix

4 files, ~130 lines. 상세 변경 내용은 plan file 참조:
`C:\Users\jongw\.claude\plans\serene-launching-lynx.md`

| File | Changes |
|------|---------|
| `Source/Utilities/loopback.h` | +SessionPassthrough, PipeFormat, PipeBlockAlign, PipeSizeFrames, loss counters (4종). C_ASSERT |
| `Source/Utilities/loopback.cpp` | +UpdateSessionPassthrough(). RegisterFormat/UnregisterFormat 호출. Write/Read loss 카운터. MicSink 가드. SessionPassthrough 분기. +1 frame fix. Conv counters. Init |
| `Source/Main/minwavertstream.cpp` | WriteBytes MicSink no-op 가드 + ring read. UpdatePosition zero-fill 가드. ReadBytes ring write |
| `Source/Main/adapter.cpp` | LOOPBACK_BUFFER C_ASSERT sizeof/offset 업데이트 |

## Phase 2: Diagnostics IOCTL + Monitor

ioctl.h (AO_PIPE_STATUS, AO_STREAM_STATUS_V2 with StructSize), adapter.cpp, test_stream_monitor.py

## Phase 3: Control Panel + SessionPassthrough 범위 확장

ControlPanel/main.cpp, loopback.cpp (PCM multichannel -> float32)

## Phase 4: Direct-Push 최적화 + Stress

MicSink race hardening, optional direct-push, 1hr stress test
