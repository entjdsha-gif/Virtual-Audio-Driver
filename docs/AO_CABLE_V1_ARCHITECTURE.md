# AO Cable V1 Architecture

Status: active
Date: 2026-04-25

This file is the architecture overview — the **what** and **why**. The
companion `docs/AO_CABLE_V1_DESIGN.md` covers the **how** (file-level layout,
field-level offsets, function signatures, data flows). Architecture decisions
are tracked in `docs/ADR.md`.

If anything here disagrees with `docs/ADR.md`, the ADR wins. If anything here
disagrees with the installed WDK headers / Microsoft samples, the official
references win — file an ADR amendment.

---

## 1. System diagram

```text
                          ┌──────────────────────────┐
WASAPI / DirectSound ───▶ │ AO Cable A render pin    │
shared-mode app           │ (KSCATEGORY_AUDIO/RENDER │
                          │  + REALTIME)             │
                          └────────────┬─────────────┘
                                       │ WaveRT mapped DMA buffer
                                       ▼
                          ┌──────────────────────────┐
                          │ Cable A cable transport  │ ◀──────┐
                          │ (canonical helper)       │        │
                          │  ┌──────────────────┐    │        │
                          │  │ FRAME_PIPE A     │    │        │
                          │  │  INT32 ring      │    │        │
                          │  │  WrapBound frames │    │        │
                          │  └──────────────────┘    │        │
                          └────────────┬─────────────┘        │
                                       │                      │ shared
                                       ▼                      │ timer
                          ┌──────────────────────────┐        │  +
                          │ AO Cable A capture pin   │        │ query
                          │ (KSCATEGORY_AUDIO/CAPTURE│        │ entries
                          │  + REALTIME)             │        │
                          └────────────┬─────────────┘        │
                                       │ WaveRT mapped DMA buffer
                                       ▼                      │
                          WASAPI / DirectSound consumer       │
                                                              │
                          (Cable B is a parallel pair) ───────┘
```

Two cable pairs (A, B) with identical structure and isolated state.

PortCls + WaveRT shell handles pin / topology / filter / KS interface
registration. The cable transport core sits between the render DMA and the
capture DMA per cable.

## 2. Layered structure

| Layer | Owns | Where |
|---|---|---|
| Adapter / device | PortCls init, KS interfaces, IOCTL dispatch, engine bring-up | `Source/Main/adapter.cpp` |
| Miniport / pin / topology | WaveRT pin contract, format negotiation, KS state | `Source/Main/minwavert.*`, `Source/Filters/*` |
| Stream | WaveRT-facing entrypoints (`Init`, `SetState`, `GetPosition`, `GetReadPacket`, `SetWritePacket`), per-stream lock, lifecycle | `Source/Main/minwavertstream.*` |
| Cable transport runtime | `AO_STREAM_RT`, canonical advance helper, fade envelope, drift correction, engine timer DPC | `Source/Utilities/transport_engine.*` |
| Cable ring | `FRAME_PIPE` (INT32 ring, write SRC, read SRC) | `Source/Utilities/loopback.*` |

The cable transport core lives in `Source/Utilities/transport_engine.*` plus
`Source/Utilities/loopback.*`. The miniport / stream layer is a thin call-site
that bridges WaveRT into the cable core.

## 3. Data flow per cable

### 3.1 Render path (speaker → ring)

1. WASAPI writes into the WaveRT-mapped DMA buffer (driver does not see
   per-write events; the buffer is shared memory).
2. The canonical helper (`AoCableAdvanceByQpc`) is invoked from one of:
   shared timer DPC, query path (`GetPosition`/`GetPositions`), or packet
   surface.
3. Helper computes `advance = elapsedFrames - publishedFrames` and gates on
   the 8-frame minimum.
4. Helper copies `advance * BlockAlign` bytes from `DmaBuffer + DmaCursor`
   into the per-stream linear scratch buffer (handles wrap).
5. Helper calls `AoRingWriteFromScratch` with scratch + format → ring write
   SRC produces INT32 normalized samples in the ring at `WritePos`.
6. Helper applies the fade-in envelope on the freshly written samples
   (click suppression at packet boundaries).
7. Helper advances DMA cursor and monotonic counters.

The ring write may return `STATUS_INSUFFICIENT_RESOURCES` if the ring is
full; the helper increments `OverflowCounter` and does not advance.

### 3.2 Capture path (ring → mic)

1. The canonical helper is invoked (same call sources as render).
2. Helper computes `advance` and gates on the 8-frame minimum.
3. Helper calls `AoRingReadToScratch` for `advance` frames at the client's
   format → ring read SRC produces denormalized samples in scratch.
4. If ring fill is insufficient, the read function silence-fills scratch
   and increments `UnderrunCounter`. If `UnderrunFlag` is set, recovery
   continues until ring fill ≥ `WrapBound / 2`.
5. Helper copies scratch into the WaveRT-mapped capture DMA at
   `DmaCursor` (handles wrap).
6. Helper advances DMA cursor and monotonic counters.

WASAPI reads the capture DMA directly via WaveRT mapping; the driver is not
notified per-read.

### 3.3 Position freshness

When WASAPI calls `GetPosition` or `GetPositions`, the cable stream
implementation:

1. Acquires the position spinlock.
2. Calls `AoCableAdvanceByQpc(rt, KeQueryPerformanceCounter(),
   AO_ADVANCE_QUERY, 0)`.
3. Reads `MonoFramesLow * BlockAlign` (PlayPosition) and
   `MonoFramesMirror * BlockAlign` (WritePosition).
4. Releases the lock.

This makes the returned position reflect "now" within sub-DPC precision,
not the last DPC tick.

## 4. Canonical advance helper

`AoCableAdvanceByQpc` is the single owner of cable transport / accounting /
position freshness. Every active call source funnels into it. Pseudocode
(per ADR-006, ADR-007):

```c
AoCableAdvanceByQpc(rt, nowQpcRaw, reason, flags) {
    // KeAcquireSpinLockRaiseToDpc returns the old IRQL — capture it.
    oldIrql = KeAcquireSpinLockRaiseToDpc(&rt->PositionLock);

    apply_drift_correction(rt, nowQpcRaw);                    // 63/64 phase
    nowQpc100ns  = AoQpcTo100ns(nowQpcRaw);

    // Long-window rebase BEFORE elapsed/advance calc — the rebase tick
    // consumes itself; next tick measures fresh elapsed against new anchor.
    elapsedProbe = ((nowQpc100ns - rt->AnchorQpc100ns)
                     * rt->SampleRate) / 10000000ULL;
    if (elapsedProbe >= ((uint64_t)rt->SampleRate << 7)) {    // ~128 s
        rt->AnchorQpc100ns             = nowQpc100ns;
        rt->PublishedFramesSinceAnchor = 0;
        rt->LastAdvanceDelta           = 0;
        KeReleaseSpinLock(&rt->PositionLock, oldIrql);
        return;
    }

    elapsed = elapsedProbe;
    advance = (LONG)(elapsed - rt->PublishedFramesSinceAnchor);

    if (advance < 8) {                                         // 8-frame gate
        KeReleaseSpinLock(&rt->PositionLock, oldIrql);
        return;
    }

    if ((uint32_t)advance > (rt->SampleRate / 2)) {           // overrun guard
        AoHandleAdvanceOverrun(rt, reason, advance);
        KeReleaseSpinLock(&rt->PositionLock, oldIrql);
        return;
    }

    if (!rt->IsCapture) {                                      // RENDER
        bytes = advance * rt->BlockAlign;
        linearize_dma_window_to_scratch(rt, bytes);
        // Fade envelope applied to scratch BEFORE ring write — otherwise
        // the un-faded samples are already in the ring and the fade
        // affects nothing audible (#15 from review of 8afa59a).
        AoApplyFadeEnvelope(rt->ScratchBuffer, advance, &rt->FadeSampleCounter);
        AoRingWriteFromScratch(rt->Pipe, rt->ScratchBuffer,
                               advance, rt->SampleRate,
                               rt->Channels, rt->BitsPerSample);
        advance_render_cursors(rt, advance, bytes);
    } else {                                                   // CAPTURE
        bytes = advance * rt->BlockAlign;
        AoRingReadToScratch(rt->Pipe, rt->ScratchBuffer,
                             advance, rt->SampleRate,
                             rt->Channels, rt->BitsPerSample);
        copy_scratch_to_dma_with_wrap(rt, bytes);
        advance_capture_cursors(rt, advance, bytes);

        // V1 scope: shared-mode capture clients (Phone Link path) never
        // arm packet notification, so NotifyArmed stays 0 and the
        // dispatch below is unreachable. Event-driven packet
        // notification dispatch (the bridge that calls back into PortCls
        // when the cursor crosses NotifyBoundaryBytes) is deferred to
        // Phase 7. Until then NotifyArmed/NotifyFired/NotifyBoundaryBytes
        // are reserved fields populated by future code.
    }

    rt->MonoFramesLow              += advance;
    rt->MonoFramesMirror           += advance;
    rt->LastAdvanceDelta            = advance;
    rt->PublishedFramesSinceAnchor  = (uint32_t)elapsed;

    KeReleaseSpinLock(&rt->PositionLock, oldIrql);
}
```

### Reason values

| Reason | Caller |
|---|---|
| `AO_ADVANCE_QUERY` | `GetPosition` / `GetPositions` (cable streams only) |
| `AO_ADVANCE_TIMER_RENDER` | shared timer DPC, render-side cable stream entry |
| `AO_ADVANCE_TIMER_CAPTURE` | shared timer DPC, capture-side cable stream entry |
| `AO_ADVANCE_PACKET` | event-driven WaveRT packet surface (deferred to Phase 7; reserved enum value, no caller wired in V1 phases 0-6) |

The internal logic is reason-agnostic for transport; reason exists for
diagnostics counters and ordering rules.

## 5. State per stream (`AO_STREAM_RT`)

Allocated on first RUN, freed in destructor. Key fields (see
`docs/AO_CABLE_V1_DESIGN.md` for the full list):

- `IsCapture`, `IsCable`, `IsSpeakerSide`, `Active`
- `SampleRate`, `Channels`, `BlockAlign`, `BitsPerSample`
- `Pipe` (non-owning pointer to cable's `FRAME_PIPE`)
- `DmaBuffer`, `DmaBufferSize`
- `RingSizeFrames`
- Anchor / cursor state: `AnchorQpc100ns`, `PublishedFramesSinceAnchor`,
  `DmaCursorFrames`, `DmaCursorFramesPrev`
- Monotonic: `MonoFramesLow`, `MonoFramesMirror`, `LastAdvanceDelta`
- Notification: `NotifyBoundaryBytes`, `NotifyArmed`, `NotifyFired`
- Scratch: `CableScratchBuffer`, `CableScratchSize`
- Fade: `FadeSampleCounter`
- Diagnostics: `StatOverrunCounter`, hits / divergence counters

## 6. State per cable ring (`FRAME_PIPE`)

One per cable (A, B). Storage and indexing per ADR-003:

- `TargetLatencyFrames` — registry-driven target depth (frames)
- `WrapBound` — current ring depth (frames), reconciled toward
  `TargetLatencyFrames` at every op
- `FrameCapacityMax` — hard upper bound (frames)
- `Channels` — internal channel slot count (typically 16, fixed at init)
- `WritePos` (frames), `ReadPos` (frames) — wrap at `WrapBound`
- `OverflowCounter`, `UnderrunCounter`, `UnderrunFlag` — diagnostics state
- `Data` — INT32 array of `WrapBound * Channels` samples

## 7. State per engine (`AO_TRANSPORT_ENGINE`)

Single global instance. Owns:

- shared timer (`ExAllocateTimer` + `ExSetTimer`, period **1 ms** per
  ADR-013)
- active-stream registry (linked list under `EngineLock`)
- timer DPC dispatcher that snapshots active list, takes per-stream refs,
  drops engine lock, calls `AoCableAdvanceByQpc` per stream under per-stream
  lock, releases refs

The engine is **not** a transport owner — it's a wake source that funnels
into the canonical helper.

## 8. Threading & locking

| Lock | Protects | Acquired by |
|---|---|---|
| `EngineLock` | active-stream list, ref-count discipline, shared timer state | engine init / shutdown / DPC snapshot |
| Stream `m_PositionSpinLock` | per-stream WaveRT-facing fields (`m_KsState`, position publication, etc.) | `CMiniportWaveRTStream` member functions (SetState, GetPosition, GetPositions, ...) |
| Runtime `AO_STREAM_RT::PositionLock` | per-stream cable-runtime cursor / counter / fade fields | canonical helper `AoCableAdvanceByQpc` |
| `FRAME_PIPE::Lock` | ring read / write positions, ring data, counters | ring write/read SRC functions |

Acquisition order (top → bottom; never invert):

```text
EngineLock
  →  m_PositionSpinLock  (stream, WaveRT-facing)
       →  AO_STREAM_RT::PositionLock  (cable runtime)
            →  FRAME_PIPE::Lock  (ring)
```

In practice:

- The DPC dispatcher acquires `EngineLock` only to snapshot the active
  list + bump ref-counts, then **releases it** before calling the helper.
  The helper runs without `EngineLock`.
- The query path (`GetPositions` cable branch) acquires
  `m_PositionSpinLock` first, then calls the helper which acquires
  `AO_STREAM_RT::PositionLock` nested. Both stream and runtime locks
  are held briefly across the helper body.
- Inside the helper, the ring SRC functions briefly take
  `FRAME_PIPE::Lock` to advance `WritePos`/`ReadPos` and write/read
  ring data. They release before returning to the helper.

The two "Position" locks (stream-side `m_PositionSpinLock` vs runtime-
side `AO_STREAM_RT::PositionLock`) are intentionally separate: the
stream lock guards WaveRT-facing miniport state that PortCls may touch
on its own threads; the runtime lock guards cable-transport-only state
that only the canonical helper touches. Keeping them distinct lets the
helper run from the timer DPC without contending with PortCls's own
calls into `m_PositionSpinLock`.

DPC vs destructor race is closed by ref-counting on `AO_STREAM_RT`:
engine `RefCount++` on snapshot, `RefCount--` after helper returns;
destructor calls `KeFlushQueuedDpcs` first (per ADR-009).

## 9. Lifecycle

```text
RUN entry:
  if (m_pTransportRt == NULL)
      m_pTransportRt = AoTransportAllocStreamRt(this);
  populate snapshot from format / DMA / pipe
  AoTransportOnRunEx(&snapshot);                  // engine ref += 1, add to active

PAUSE:
  AoTransportOnPauseEx(rt);                       // mark inactive, keep allocation

STOP:
  AoTransportOnStopEx(rt);                        // mark inactive, zero cursors

Destructor:
  KeFlushQueuedDpcs();                             // drain in-flight DPCs
  AoTransportOnStopEx(rt);                         // idempotent
  AoTransportFreeStreamRt(rt);                    // ref-count-aware free
```

## 10. External contract (advertised to Windows)

### INF declarations (per cable, A and B)

```text
KSCATEGORY_AUDIO       (both render and capture)
KSCATEGORY_REALTIME    (both)
KSCATEGORY_RENDER      (render side)
KSCATEGORY_CAPTURE     (capture side)
KSCATEGORY_TOPOLOGY    (topology filter)

PKEY_AudioEndpoint_Association  =  KSNODETYPE_ANY
```

### OEM Default Format (shared-mode default)

`WAVEFORMATEXTENSIBLE`:

- format tag: `WAVE_FORMAT_EXTENSIBLE`
- subformat: `KSDATAFORMAT_SUBTYPE_PCM`
- 48 000 Hz, 24 bit, 2 channel (Stereo)

### KSDATARANGE intersection

Accepts any rate / bit-depth / channel combination that the SRC functions
can convert (per ADR-008):

- rates: 8000, 16000, 22050, 32000, 44100, 48000, 88200, 96000, 176400,
  192000 Hz
- bits: 8, 16, 24 (PCM int) and 32 (PCM int / IEEE float)
- channels: 1, 2

Unsupported requests get `STATUS_NOT_SUPPORTED` cleanly (no fall-back
silent re-conversion).

### Position contract

`GetPosition` and `GetPositions` recalculate to current QPC inside the
position spinlock before returning. Returned values are monotonic byte
counters.

### Packet contract

`GetReadPacket`, `SetWritePacket`, `GetPacketCount` work only when
`m_ulNotificationsPerBuffer != 0` (event-driven mode). For shared-mode
clients (the Phone Link path), these return `STATUS_NOT_SUPPORTED` and
position polling is the supported flow.

## 11. Diagnostics

The IOCTL `IOCTL_AO_GET_STREAM_STATUS` returns:

- `AO_STREAM_STATUS` (V1, always present): per-endpoint Active /
  SampleRate / BitsPerSample / Channels.
- `AO_V2_DIAG` (V2 extension, when caller buffer is large enough): per-
  endpoint counters: GatedSkip, OverJump, FramesProcessed, PumpInvocation,
  ShadowDivergence, FeatureFlags, plus per-render-side Pump/Legacy drive
  counts.

Schema lives in `Source/Main/ioctl.h`. Diagnostics rule (REVIEW_POLICY § 7):
`ioctl.h`, `adapter.cpp`, and `test_stream_monitor.py` must be updated
together.

User-mode consumer: `test_stream_monitor.py`.

## 12. Code organization

| File | Owns | Does NOT own |
|---|---|---|
| `Source/Main/adapter.cpp` | engine init/teardown, IOCTL dispatch | transport policy |
| `Source/Main/minwavert.*` | miniport / topology / filter / pin contract | per-stream transport |
| `Source/Main/minwavertstream.*` | WaveRT-facing entrypoints, call-site bridging | transport math, ring access |
| `Source/Main/ioctl.h` | IOCTL codes, `AO_V2_DIAG` schema | implementation |
| `Source/Filters/*` | filter / pin / topology static data | dynamic state |
| `Source/Utilities/transport_engine.*` | `AO_STREAM_RT`, helper, fade, engine timer DPC | ring internals |
| `Source/Utilities/loopback.*` | `FRAME_PIPE`, ring SRC, format dispatch | transport ownership |

## 13. What V1 explicitly does not do

(Per `docs/PRD.md` § 7 and `docs/ADR.md`. Listed here so reviews can flag
violations without cross-referencing.)

- ACX/KMDF rewrite (separate product track, frozen reference branch).
- Mixing, volume, mute, APO, DSP, AGC, EQ, limiter, noise suppression.
- Built-in ASIO driver.
- Built-in HFP forwarding.
- More than two cable pairs.
- Float-PCM-specific processing inside the driver beyond format
  acceptance.
- ms units in runtime transport math.
- Silent ring overflow (must hard-reject + counter).
- Stale capture data after STOP/RUN cycle.

## 14. Pointers to detail

- `docs/AO_CABLE_V1_DESIGN.md` — file-level layout, struct field offsets,
  function signatures.
- `docs/ADR.md` — every decision plus rationale and consequences.
- `docs/PRD.md` — product identity, scope, success criteria.
- `docs/REVIEW_POLICY.md` — review rules and forbidden drift.
- `docs/GIT_POLICY.md` — branch / commit / merge rules.
- `phases/<N>-name/step<N>.md` — per-step implementation guidance.
- `results/vbcable_pipeline_analysis.md` — VB pipeline reference.
- `results/vbcable_disasm_analysis.md` — VB SRC + ring layout reference.
- `results/vbcable_capture_contract_answers.md` — VB capture-side
  Q&A, verified.
- `results/phase6_vb_verification.md` — WinDbg dynamic verification.
