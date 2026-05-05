# AO Cable V1 Architecture Decision Records

Status: active
Date: 2026-04-25

This file is the single source of truth for AO Cable V1 architectural
decisions. New decisions append a new ADR-NNN section. Decisions are not
edited after acceptance; if a decision is reversed, a new ADR supersedes it
explicitly.

---

## ADR-001: Stay on PortCls / WaveRT for V1

Status: accepted

### Context

The previous AO Cable codebase (telephony-V1, then pipeline-V2) is built on
PortCls + WaveRT (MSVAD-derived). It already has working install / signing /
service / WaveRT pin / topology code. A separate experimental track
(`ao-cable-v2-step2b-merge`, in a different repo) is rebuilding the same
product on ACX/KMDF from scratch.

V1's goal is not "modernize the framework" — it is "fix the cable transport
core so live-call quality matches VB-Cable." Switching framework on top of
that doubles risk for the same goal.

### Decision

V1 stays on **PortCls + WaveRT + KMDF (legacy KS)**. The WaveRT shell, INF
identity, install/signing pipeline, and non-cable code paths are preserved.

ACX/KMDF migration is a separate future product track and is out of scope
for V1.

### Rationale

- minimal blast radius — only the cable transport core is rewritten,
  everything else is verified-working code
- reuses existing install/signing/build/no-reboot-upgrade tooling
- aligns with VB-Cable's actual design (VB-Cable is also PortCls — the
  framework is not the problem)
- keeps the V2 ACX track free to be a clean greenfield experiment without
  shipping pressure

### Consequences

- the rewrite must operate within PortCls/WaveRT object lifecycle
- ACX-specific patterns (`AcxRtStreamCreate`, `AcxJackCreate`, etc.) do not
  apply to V1 reviews; PortCls/KMDF/KS API sequence rules apply instead
- V1 cannot benefit from ACX's shared-stream packet model — the cable
  pair core implements its own equivalent

### Forbidden as a result

- silently swapping V1 to ACX to make a build/runtime problem disappear
- copying ACX-specific structure from `ao-cable-v2-step2b-merge` into V1

---

## ADR-002: VB-Cable Pattern As Cable Transport Reference

Status: accepted

### Context

VB-Cable, on the same Phone Link + OpenAI Realtime live-call path, produces
clean audio. AO Cable V1 (pre-rewrite) produces garbled audio. Both run on
PortCls/WaveRT, on the same machine, in the same OS configuration.

The difference is the **cable transport core**. VB-Cable's core has been
verified through:

- full Ghidra decompile (`results/ghidra_decompile/vbcable_all_functions.c`,
  297 functions, 12096 lines)
- WinDbg dynamic analysis (`results/phase6_vb_verification.md`,
  `results/vb_session.log`)
- targeted analyses (`results/vbcable_pipeline_analysis.md`,
  `results/vbcable_disasm_analysis.md`,
  `results/vbcable_capture_contract_answers.md`)

### Decision

V1 adopts VB-Cable's verified cable transport pattern. Specifically, the
following VB-observed properties are accepted as design targets for V1:

1. INT32 frame-indexed ring (not packed 24-bit, not byte-indexed) —
   ADR-003.
2. Single-pass linear-interpolation SRC per direction (not 4-stage
   pipeline, not sinc) — ADR-004.
3. Hard-reject on ring overflow with counter (not silent overwrite) —
   ADR-005.
4. One canonical cable advance helper — ADR-006.
5. Position recalculation on every query — ADR-007.
6. 63/64 phase-corrected timer + 8-frame gate + DMA overrun guard —
   ADR-007.
7. No `MicSink` dual-write — ADR-008.
8. KSDATARANGE accepts client formats; internal SRC handles mismatch —
   ADR-008.
9. `KeFlushQueuedDpcs` before resetting per-stream runtime fields —
   ADR-009.

### Rationale

VB-Cable is a production-grade reference for the same product category.
"Match VB" is a falsifiable design target with a working reference, instead
of "design something better than VB" which has no falsifiable target.

### Consequences

- Every V1 design choice in cable transport must justify itself either as
  matching VB or as an explicit deliberate divergence.
- "Different from VB" without justification is a review finding.
- VB matches what the Ghidra static analysis shows; if dynamic behavior
  contradicts static, dynamic wins (note in the relevant ADR).

---

## ADR-003: INT32 Frame-Indexed Cable Ring

Status: accepted

### Context

Pre-rewrite AO Cable stored cable ring samples as packed 24-bit (3 bytes
per sample) with byte-indexed read/write positions. The 3-byte stride
caused alignment quirks and made wrap-around math fragile. Format
conversion happened in 4 separate stages.

VB-Cable stores ring samples as INT32 (4 bytes per sample) with
frame-indexed positions. All formats (8/16/24/32-bit) are normalized to
~19-bit signed range during ring write. SRC operates on INT32 directly.

### Decision

V1's cable ring (`FRAME_PIPE`) stores samples as **INT32** (4 bytes per
sample), indexed by **frame** (not byte). The 19-bit normalization shape
is:

| Input format          | Conversion to ring INT32                        |
|-----------------------|-------------------------------------------------|
| 8-bit unsigned PCM    | `(byte - 0x80) << 11`                           |
| 16-bit signed PCM     | `(int)int16 << 3`                               |
| 24-bit packed signed  | `(3-byte assembled << 8) >> 13`                 |
| 32-bit signed PCM     | `(int)int32 >> 13`                              |

Read path inverts (`>> 11`, `>> 3`, `<< 5` then 3-byte pack, `<< 13`).
V1 supports `KSDATAFORMAT_SUBTYPE_PCM` only (see ADR-008);
`KSDATAFORMAT_SUBTYPE_IEEE_FLOAT` is rejected at intersection. 32-bit
PCM is **not** a direct copy — the full INT32 range must be normalized
to 19-bit before entering the ring so the SRC accumulator's 13-bit
headroom invariant holds.

Ring layout fields: `TargetLatencyFrames`, `WrapBound` (current depth),
`FrameCapacityMax`, `Channels`, `WritePos` (frames), `ReadPos` (frames),
`OverflowCounter`, `UnderrunCounter`, `UnderrunFlag`, `Data` (INT32 array).

### Rationale

- 4-byte alignment eliminates a class of unaligned-access bugs.
- 19-bit normalized range gives the SRC accumulator headroom against
  INT32 overflow.
- Frame-indexed positions make wrap math trivial (`pos = (pos + 1) %
  WrapBound`).

### Consequences

- existing packed-24 ring code is replaced (not patched).
- format conversion code lives at the ring write/read boundary, not in a
  separate pre-stage.
- exclusive-mode clients that expect direct 24-bit access get the
  ring's 19-bit normalized range only after read-side denormalization
  back to 24-bit; lossy in the lowest 5 bits, intentional.

---

## ADR-004: Single-Pass Linear-Interpolation SRC Per Direction

Status: accepted

### Context

Pre-rewrite AO Cable used 8-tap windowed sinc SRC with a 2048-coefficient
table inside a 4-stage pipeline:

```text
Client format  ->  ConvertToInternal  ->  SrcConvert (sinc)  ->  ConvertFromInternal  ->  LoopbackWrite
```

Each stage held a separate spinlock; format could change between stages,
and overflow risks the sinc accumulator at high amplitudes.

VB-Cable performs format conversion + SRC + ring write atomically per
direction in two functions: a write SRC (client format -> ring INT32) and
a read SRC (ring INT32 -> client format). Algorithm: GCD-based
integer-ratio linear interpolation.

### Decision

V1 implements two SRC functions:

```text
AoRingWriteFromScratch(pipe, scratch, frames, srcRate, srcChannels, srcBits)
AoRingReadToScratch (pipe, scratch, frames, dstRate, dstChannels, dstBits)
```

Algorithm:

1. Try GCD divisor 300, then 100, then 75. If none divides both rates
   evenly, return error.
2. `srcRatio = srcRate / divisor`, `dstRatio = dstRate / divisor`.
3. Linear interpolation with weighted accumulator (per-channel).
4. Same-rate path is direct copy (still doing per-sample format
   normalization).

Bit-depth dispatch (8/16/24/32) happens once, inside the SRC function.

### Rationale

- one atomic operation per direction → no inter-stage race window.
- linear interpolation is robust against amplitude spikes.
- GCD divisor handles the rate pairs that matter for live calls
  (48000/44100 via 300, 48000/96000 via 100, etc.).
- VB has been running this for years on millions of installs.

### Consequences

- the existing 8-tap sinc table and 4-stage pipeline are deleted.
- client requesting an exotic rate that doesn't GCD-match (e.g. 3kHz)
  gets `STATUS_NOT_SUPPORTED` instead of falling back to a sinc path.
- HD-audio quality at extreme upsampling ratios is bounded by
  linear-interp; acceptable for V1 because Phone Link / VoIP target
  rates all GCD-match cleanly.

---

## ADR-005: Hard-Reject Ring Overflow + Hysteretic Underrun Recovery

Status: accepted

### Context

Pre-rewrite AO Cable silently overwrote the cable ring on overflow,
advancing `ReadPos` and discarding old data. This is what produces
mid-stream discontinuities on heavy load. There is no counter for
when this happens.

VB-Cable refuses the write entirely on overflow and increments a counter.
The ring is never corrupted. On underrun, VB enters a hysteretic recovery
state: silence is returned until ring fill recovers to ≥ `WrapBound / 2`.

### Decision

V1 ring write semantics:

```text
if (frames_to_write > available_space - 2):
    pipe.OverflowCounter += 1
    return STATUS_INSUFFICIENT_RESOURCES  // do NOT advance WritePos
else:
    write frames; advance WritePos
```

V1 ring read semantics:

```text
if (UnderrunFlag == 1):
    if (available_frames < WrapBound / 2):
        zero-fill output; return  // stay in recovery
    else:
        UnderrunFlag = 0  // exit recovery, fall through

if (frames_to_read > available_frames):
    pipe.UnderrunCounter += 1
    UnderrunFlag = 1
    zero-fill output; return
else:
    read frames; advance ReadPos
```

### Rationale

- producer-side hard-reject prevents in-ring data corruption.
- consumer-side hysteresis prevents boundary chatter when fill oscillates
  around empty (would otherwise alternate between "data" and "silence" on
  every read).
- the `WrapBound / 2` threshold gives ≈74.7 ms recovery window at the
  default 7168-frame WrapBound — long enough to refill, short enough to
  not be perceptually catastrophic.

### Consequences

- caller of ring write must handle `STATUS_INSUFFICIENT_RESOURCES` (the
  canonical helper does).
- monitoring `OverflowCounter` and `UnderrunCounter` is the primary
  health signal for the cable transport.
- in steady-state speech, both counters should stay at 0; non-zero is a
  diagnostic flag, not a "normal" condition.

---

## ADR-006: One Canonical Cable Advance Path (`AoCableAdvanceByQpc`)

Status: accepted

### Context

Phase 5 and Phase 6 attempts split cable transport ownership between query
path (`UpdatePosition` from `GetPosition`/`GetPositions`) and a separate
timer-driven engine. The result was stale-cursor reads, race-like
artifacts, and audible chopping. Multiple iterations of "publish a better
cursor and let the timer move data" failed for the same root reason: two
owners producing inconsistent truth.

VB-Cable has multiple call sources (query, timer, packet) but **one
canonical advance function**. Every source funnels into it.

### Decision

V1 has a single canonical cable advance function:

```c
VOID AoCableAdvanceByQpc(AO_STREAM_RT* rt,
                         ULONGLONG nowQpcRaw,
                         AO_ADVANCE_REASON reason,
                         ULONG flags);

typedef enum _AO_ADVANCE_REASON {
    AO_ADVANCE_QUERY         = 0,  // GetPosition / GetPositions
    AO_ADVANCE_TIMER_RENDER  = 1,  // shared timer DPC, render stream
    AO_ADVANCE_TIMER_CAPTURE = 2,  // shared timer DPC, capture stream
    AO_ADVANCE_PACKET        = 3,  // event-driven WaveRT packet surface
} AO_ADVANCE_REASON;
```

This function owns, for cable streams, **all of**:

1. QPC delta → elapsed frames
2. minimum-frame gate (8 frames)
3. monotonic cursor / accounting updates
4. DMA wrap / scratch linearization
5. ring read/write (calls `AoRingWriteFromScratch` /
   `AoRingReadToScratch`)
6. position freshness for WaveRT clients
7. packet-ready accounting (when arming exists)

Plus, mandatory invariants inside the helper:

8. 63/64 phase-corrected scheduling
9. DMA overrun guard (skip if `advance > sampleRate / 2`)

**No second path is allowed to "publish state for another cadence to move
later."** In V1 the shared timer DPC and query path are the wired call
sources; both funnel into this function. The `AO_ADVANCE_PACKET` enum
value is reserved for a future event-driven WaveRT packet caller and has
no wiring in V1 phases 0-6 (deferred to Phase 7). Whatever caller is added
later must funnel into the same function — no new transport owner.

### Rationale

- one writer per cable transport state → no race window.
- multiple call sources are fine as long as they funnel into one owner.
- VB has both `+0x6320` (query path) and `+0x68ac`/`+0x5cc0` (timer path)
  feeding the same advance logic; this is verified in
  `results/vbcable_capture_contract_answers.md`.

### Consequences

- legacy `CMiniportWaveRTStream::UpdatePosition` cable branch becomes a
  thin shim that calls `AoCableAdvanceByQpc`.
- the shared transport engine timer is one of multiple call sources, not
  the owner.
- `m_pNotificationTimer` and `m_pDpc` per-stream cable transport
  ownership is retired during phase 4-6.

---

## ADR-007: Position Recalculation On Query + Drift-Corrected Timer + 8-Frame Gate

Status: accepted

### Context

Pre-rewrite AO Cable returns the most recent DPC's position value when
WASAPI calls `GetPosition` / `GetPositions`. This can be up to 1 ms stale.
Long calls also drift because the timer is not phase-corrected against
QPC.

VB-Cable, on `GetPosition`, recalculates position to the **current QPC**
inside the position spinlock before returning. VB also rebases the
notional sample clock periodically against fresh QPC (the long-window
rebase, ~128 seconds at 48 kHz, see Decision 2 below) and applies a
63/64 phase correction to the per-tick deadline. And VB applies an
8-frame minimum gate to prevent sub-sample-jitter from causing
irregular tiny transfers.

### Decision

V1 implements all three:

1. **Position on query** — `GetPosition` and `GetPositions` for cable
   streams call `AoCableAdvanceByQpc(rt, KeQueryPerformanceCounter(),
   AO_ADVANCE_QUERY, 0)` inside the position spinlock before reading
   `MonoFramesLow` / `MonoFramesMirror`. Returned values reflect "now".

2. **63/64 phase correction + long-window QPC rebase** — inside
   `AoCableAdvanceByQpc`, two distinct mechanisms work together:
   - Per-tick phase correction: the timer cadence is phase-corrected
     against QPC drift (`base + (count * interval) * 63/64`), so
     small per-tick scheduling jitter does not accumulate. This is
     applied every helper invocation.
   - Long-window QPC rebase: when the elapsed frames exceed
     `sampleRate << 7` (i.e. 128 seconds of stream time at any rate),
     the helper resets `AnchorQpc100ns` to the current QPC and zeros
     `PublishedFramesSinceAnchor`. This bounds the integer growth of
     the elapsed-frames computation and re-syncs the notional clock
     against wall time. (At 48 kHz this fires once every ~128 s; at
     192 kHz also every ~128 s of stream time.)

   The two mechanisms are independent: the 63/64 correction is per-
   tick fine adjustment; the long-window rebase is a coarse periodic
   re-sync. Earlier drafts mistakenly described the rebase as "every
   100 ticks" — that came from a 1-ms-tick × 100 = 100 ms reading
   that did not match the `<< 7` constant. The actual trigger is
   `elapsedFrames >= sampleRate << 7`.

3. **8-frame minimum gate** — `if (advance < 8): return` skips
   sub-sample-precision noise. At 48 kHz, 8 frames = 167 µs.

### Rationale

- query freshness keeps WASAPI / Phone Link's position polling accurate
  to within sub-DPC precision instead of 1 ms.
- drift correction prevents long-call cumulative timing slip.
- the gate prevents irregular small transfers when the OS happens to
  fire the timer late by < 8 frames.

### Consequences

- `GetPosition` is no longer a side-effect-free read; it acquires the
  position spinlock and may run the helper.
- the helper must be tight enough that holding the spinlock during a
  query does not introduce additional latency that defeats the
  freshness gain.
- the gate floor is rate-aware in spirit: at extreme low rates (8 kHz
  mono), 8 frames = 1 ms = exactly one tick; for V1, rates that low are
  marginal but not blocked. (Stage 7 may revisit a rate-aware gate.)

---

## ADR-008: KSDATARANGE Accepts Client Formats; Internal SRC Handles Mismatch

Status: accepted

### Context

Pre-rewrite AO Cable's `FormatMatch` policy required Speaker == Mic ==
Internal at 48k/24/8ch. Any client requesting a different format triggered
Windows Audio Engine to SRC user-mode first, then the driver SRC'd again
internally. Double conversion. Quality lost.

VB-Cable's KSDATARANGE accepts client-native formats. Driver internal SRC
handles the difference once.

### Decision

V1 KSDATARANGE accepts:

- **Sample rates**: 8000, 16000, 22050, 32000, 44100, 48000, 88200, 96000,
  176400, 192000 Hz (the rates that GCD-divide cleanly by 300, 100, or 75
  against the registry-driven internal rate).
- **Bit depths**: 8 (PCM uint), 16 (PCM int), 24 (packed PCM int), 32
  (PCM int).
- **Subtype**: `KSDATAFORMAT_SUBTYPE_PCM` only.
  `KSDATAFORMAT_SUBTYPE_IEEE_FLOAT` is **not** advertised; the
  intersection handler must reject IEEE_FLOAT requests with
  `STATUS_NO_MATCH`. Rationale: distinguishing 32-bit PCM from 32-bit
  float at the cable transport API requires plumbing `SubFormat` /
  `ValidBitsPerSample` through every SRC entry point (`AoRingWrite/Read
  FromScratch`); for V1 ship parity with VB-Cable we constrain to PCM
  and defer float to a post-V1 ADR.
- **Channels**: 1 (mono), 2 (stereo).

OEM Default Format (advertised in INF for shared-mode default): **48 kHz
/ 24-bit / Stereo** — matches VB-Cable.

Internal ring runs at the registry-configured rate (default 48 kHz),
16-channel slot capacity. Driver-internal SRC handles client→ring (write
direction) and ring→client (read direction) using ADR-004's algorithm.

### Rationale

- shared-mode WASAPI clients negotiate against OEM Default Format and
  rarely request anything else; conversion cost is moot in that path.
- exclusive-mode clients can pick formats from KSDATARANGE; the driver's
  single-pass SRC is cheap and correct for those.
- "Driver does its own conversion once" is consistently better than
  "Windows converts then driver converts again."

### Consequences

- `FormatMatch` enforcement code is removed from the cable path.
- KSDATARANGE intersection handler uses a **two-tier failure status**
  (see ARCHITECTURE § 10.3 and DESIGN § 6.1):
  - `STATUS_NO_MATCH` for outside-range requests (IEEE_FLOAT,
    unadvertised rate / bit / channel).
  - `STATUS_NOT_SUPPORTED` for in-range PCM requests whose rate fails
    the 300/100/75 GCD divisor check against the pipe's internal rate.
- multi-channel (>2) is out of scope for V1; revisit in Stage 7.

---

## ADR-009: `KeFlushQueuedDpcs` On Pause / Stop / Destructor

Status: accepted

### Context

The shared transport engine timer DPC may be running with a transient
reference to an `AO_STREAM_RT` while the stream's destructor wants to free
it. Without DPC drain, the timer DPC could touch already-freed memory.

VB-Cable's destructor path calls `KeFlushQueuedDpcs` before clearing
per-stream cursor state.

### Decision

V1 lifecycle ordering. **PAUSE, STOP, and destructor each have a
distinct contract**; they are not interchangeable:

```text
KSSTATE_PAUSE (RUN → PAUSE):
    1. KeFlushQueuedDpcs()           // drain any in-flight DPC
    2. AoTransportOnPauseEx(rt)      // mark inactive; KEEP allocation;
                                     //   do NOT reset monotonic/cursor state
                                     //   (RUN → PAUSE → RUN keeps the ring
                                     //    usable, per REVIEW_POLICY § 5)

KSSTATE_STOP (RUN → STOP):
    1. KeFlushQueuedDpcs()           // drain any in-flight DPC
    2. AoTransportOnStopEx(rt)       // mark inactive; reset monotonic
                                     //   counters / cursor / fade / SRC
                                     //   state on the FRAME_PIPE
                                     //   (RUN → STOP → RUN starts fresh)

Stream destructor:
    1. KeFlushQueuedDpcs()           // drain any in-flight DPC
    2. AoTransportOnStopEx(rt)       // idempotent state reset
    3. AoTransportFreeStreamRt(rt)   // owner ref drops (1 → 0), free
```

`AoTransportOnStopEx` is idempotent and may be called from STOP and
the destructor. `AoTransportOnPauseEx` is **only** for PAUSE — never
called from STOP or the destructor (it would skip the state reset
that those paths require).

`AoTransportFreeStreamRt` is ref-count-aware: it drops the caller's
ref and actually frees only when `RefCount` hits 0. The two helper
invocation paths use **different** teardown mechanisms (per DESIGN
§ 5.4 "Helper invocation paths"):

- **Engine timer DPC** is excluded by `AoTransportUnregister`
  (drops engine ref) followed by `KeFlushQueuedDpcs` (drains any
  in-flight DPC). The DPC takes its transient ref under
  `EngineLock`, so `RefCount` reflects DPC activity.
- **Query path** (`GetPosition` / `GetPositions`) is excluded by
  publishing `m_pTransportRt = NULL` under `m_PositionSpinLock`.
  Query path **does not** take a `RefCount` — it is serialized
  against unwind through `m_PositionSpinLock` instead. Do not add a
  query-path `RefCount++` "for safety": it would force every
  `GetPosition` to do an interlocked op without changing the
  observable race window.

The full unwind ordering (Step A query exclusion → Step B DPC
exclusion → Step C DMA teardown → Step D destructor's owner-ref
release) is in DESIGN § 5.4 "Unwind contract".

### Rationale

- `KeFlushQueuedDpcs` is the documented mechanism to ensure all DPCs
  queued before the call have completed.
- ref-count discipline closes the "DPC drops the engine lock then runs
  per-stream work while destructor races" race.

### Consequences

- destructor must not bypass the flush "for performance"; the cost is
  one drain per stream destruction.
- the engine must `RefCount++` on each stream when snapshotting under
  engine lock, and `RefCount--` after the per-stream helper returns.

---

## ADR-010: Two Independent Cable Pairs (A and B)

Status: accepted

### Context

VB-Cable in its free single-cable form has one cable pair. The
donation-tier "A+B" provides two pairs.

AO Cable existing builds expose two pairs (A and B). Maintaining this
gives users split-routing scenarios: one cable for AI voice, another for
music, etc.

### Decision

V1 ships **two cable pairs**: A and B. Each is independent — separate
ring, separate runtime state, separate engine registration. Heavy traffic
on one cable does not affect the other.

Build artifacts: `aocablea.sys` and `aocableb.sys` (separate `.sys` files
via the `CABLE_A` / `CABLE_B` build macros).

Service identifiers: `AOCableA`, `AOCableB`.

Hardware IDs: `ROOT\AOCableA`, `ROOT\AOCableB`.

### Rationale

- existing AO build/install pipeline already supports two cables.
- two-cable scenarios (split routing) are the most-requested user
  scenario beyond simple loopback.
- code path is identical between A and B; only identity differs.

### Consequences

- INF and INX files for A and B remain separate (they already are).
- diagnostics IOCTL exposes A and B separately.
- live-call test harness `AUDIO_CABLE_PROFILE=ao` defaults to A; users
  pick A or B.

---

## ADR-011: Frame Counts Are The Authoritative Unit

Status: accepted

### Context

Pre-rewrite code mixes `ms`, bytes, and frames in transport math. `ms`
math hides rate-dependence and breaks at extreme rates.

VB-Cable's transport math is entirely in frames; bytes are derived
(`frames * BlockAlign`), QPC time is converted on demand.

### Decision

V1 transport math uses **frames** as the authoritative unit. Bytes and
QPC are derived. **`ms` may not be stored as runtime state** in the cable
transport path. `ms` is allowed only in:

- comments
- external UI / Control Panel
- log strings for human readability

### Rationale

- frames are rate-independent; ms is rate-dependent.
- runtime math in frames matches VB exactly.
- prevents accidental rate-coupling bugs (e.g., "20 ms of buffer" being
  half the frames at 24 kHz vs 48 kHz).

### Consequences

- existing code that stores `ms` in runtime state is updated to frames
  during the rewrite.
- IOCTL `AO_CONFIG.MaxLatencyMs` field stays as a UI-facing value, but
  the driver converts to frames immediately on receipt.

---

## ADR-012: Single Branch + Commit Prefix For Phase Isolation

Status: accepted

### Context

V2 (`ao-cable-v2-step2b-merge`) uses `phase/<N>-name` git branches with
`--no-ff` merges to master. The model fits a multi-developer or
high-coordination project where reviewing across branches matters.

V1 just consolidated multiple worktrees and parallel branches back into
one (`feature/ao-fixed-pipe-rewrite`) to remove coordination overhead.
Reintroducing per-phase branches re-creates the problem we just solved.

### Decision

V1 uses a **single active development branch + commit-prefix** model:

- Single branch: `feature/ao-fixed-pipe-rewrite`.
- Phase identity comes from `phases/<N>-name/` directory and commit
  prefixes (`phase1/step0`, `phase1/exit`, etc.).
- Merge to `main` is a deliberate shipping operation, not a per-phase
  exit operation.
- Frozen reference branches (`feature/ao-pipeline-v2`,
  `feature/ao-telephony-passthrough-v1`) stay frozen.

Full rules in `docs/GIT_POLICY.md`.

### Rationale

- single branch removes worktree management overhead.
- phase isolation is achieved by directory + commit-prefix structure,
  which is enough for a single-developer / single-AI-pair workflow.
- merging to `main` is reserved for shipping milestones (M1/M2/M6, etc.),
  which is a small number of events per year.

### Consequences

- this is a deliberate divergence from V2 git policy. Reviews comparing
  V1 to V2 must not flag the absence of phase branches as a violation.
- if V1 ever scales to multi-developer, this ADR will be reconsidered;
  for now, single-branch is correct.

---

## ADR-013: Shared Transport Engine Timer Period — 1 ms

Status: accepted

### Context

VB-Cable's transport timer fires every 1 ms (`ExSetTimer` with period
`10000` × 100 ns = 1 ms — verified in
`results/vbcable_pipeline_analysis.md` § 2.1 and the Ghidra decompile
`FUN_1400065b8`).

The current AO Cable V1 source (inherited from Phase 6 Y2-2 work) uses
a 20 ms tick (`AO_TE_REFERENCE_RATE = 48000` /
`AO_TE_EVENT_FRAMES_AT_REF = 960` in
`Source/Utilities/transport_engine.cpp`). 20 ms was chosen during the
failed Step 3/4 timer-owned transport experiment as a "safer" cadence;
it does not match VB and does not match V1's design intent (see
ADR-002 — VB is the cable transport reference).

### Decision

The shared transport engine timer period is **1 ms** (10 000 × 100 ns).

This applies to:

- `AO_TRANSPORT_ENGINE::PeriodQpc` — initialized to 1 ms in QPC ticks
  at engine init.
- `ExSetTimer(...)` invocation in `AoTransportEngineInit` — period
  argument is `10000` (100 ns units).
- The timer DPC body — must complete each tick well within 1 ms to
  avoid back-to-back queueing.

### Rationale

- VB parity (ADR-002): VB's 1 ms cadence is verified and works on
  millions of installs.
- 1 ms granularity supports the 8-frame minimum gate (ADR-007) at
  every supported sample rate.
- Position-on-query (ADR-007) reduces the tick-rate dependency for
  perceived latency, so the dominant constraint becomes "tick fast
  enough to keep ring fill steady" — 1 ms easily satisfies this.

### Consequences

- Phase 3 Step 3 (`phases/3-canonical-helper-shadow/step3.md`)
  acceptance criterion ("`DbgShadowTimerHits` increases at 1 ms
  cadence") is now consistent with the engine period.
- The current 20 ms code is changed to 1 ms during Phase 3
  Step 3 (or earlier as part of engine timer setup).
- DPC execution under 1 ms cadence requires the helper body to be
  tight — this is already a constraint per ADR-006 / ARCHITECTURE
  § 8 (no allocations, no waits at DISPATCH_LEVEL).

### Forbidden as a result

- Reverting to 20 ms timer to "smooth jitter" without an ADR
  superseding this one.
- A second timer at a different period.
