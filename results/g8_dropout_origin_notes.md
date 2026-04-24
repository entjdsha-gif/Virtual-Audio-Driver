# G8 — AO Baseline Loopback Dropout Origin (Read-Only Notes)

**Branch:** `feature/ao-fixed-pipe-rewrite` @ 2c733f1 (working tree dirty with B1/G2/G6
instrumentation untouched; analysis is read-only regardless of tree state)
**Date:** 2026-04-14
**Scope:** Read-only comparison of VB-Cable, AO baseline (b856d94), and AO V2
(2c733f1) read-side behavior, to answer **why the AO loopback dropout pattern
(~14% silence / ~1-2ms gaps) appears in both baseline and V2 while VB-Cable stays
effectively clean (~0.27% silence).** No code changes, no commits, no proposals.

**Fixed question woven through every section:**
> *When the mic-side 1ms DPC tick arrives and the ring/buffer does not hold at least
> one tick worth of frames (< 48 frames at 48 kHz), what does each driver do?*

**Scope cap**: find 1-2 concrete divergences between VB and AO; do not map the
entire read path exhaustively. Stop at hypothesis ranking once divergences are
named.

**Phase 6 status**: BLOCKED. No driver code edits result from G8.

---

## § 1. Observation recap

From the `loopback_rec.py` / `g6_runtime/*.wav` measurement set (2026-04-14 session,
all with the same test harness: Chrome/YouTube → Windows default playback → cable
speaker → cable mic capture → sd.rec wav):

| Configuration | Leading silence | Silence runs > 1ms | Total silence |
|---|---|---|---|
| **VB-Cable A** (reference) | **30 ms** | **9** | **0.27%** |
| AO Cable A pump + B1 + G2 + G6 | 2057 ms | 59 | 14.50% |
| AO Cable A legacy rollback (Phase 5 off) | 1011 ms | 323 | 10.22% |
| AO Cable A Phase 5 CLOSED clean (no instr) | 1429 ms | 238 | 14.31% |
| **AO Cable A baseline b856d94 clean** | **1981 ms** | **116** | **14.34%** |

Body-of-stream (excluding the leading gap) dropout characteristics across all three
AO configurations show **~100-240 short gaps of 1.0-5.0 ms each per 15 s window**,
structured as integer multiples of the ~1 ms DPC tick. This is not random noise; it
is DPC-synchronous silence-fill events.

**"예전엔 좋았다" user memory**: user reports that an earlier iteration of this
exact driver had a clean loopback. This has not been reconciled with the b856d94
baseline data. **Open question carried forward.**

---

## § 2. AO V2 read-side path (2c733f1)

Entry: `FramePipeReadToDma(pPipe, dmaData, byteCount)` at
[loopback.cpp:2082+](../Source/Utilities/loopback.cpp#L2082), called from the
mic-side DPC.

Chunk loop at [L2127-2183](../Source/Utilities/loopback.cpp#L2127-L2183) repeatedly
invokes `FramePipeReadFrames(pPipe, scratch, chunk)` at [L1496+](../Source/Utilities/loopback.cpp#L1496).

### 2.1 `FramePipeReadFrames` underrun policy — [L1506-1572](../Source/Utilities/loopback.cpp#L1506-L1572)

Three cases under `PipeLock`:

```
Case 1  fill < StartThresholdFrames && !StartPhaseComplete
    → RtlZeroMemory(dst, frameCount)
    → UnderrunCount not incremented (startup silence)
    → return 0

Case 2  fill == 0 (post-startup)
    → RtlZeroMemory(dst, frameCount)
    → UnderrunCount += frameCount
    → return 0

Case 3/4  0 < fill
    → framesToRead = min(fill, frameCount)
    → RtlCopyMemory available frames from ring with wrap handling
    → If framesToRead < frameCount: RtlZeroMemory the tail, UnderrunCount += remainder
    → return framesToRead
```

**Answer to the fixed question for V2**: if fill < 1 tick (48 frames), the reader
produces a partial copy plus a zero-filled tail. If fill == 0, the reader produces
a fully zero-filled tick. Either way, **any data missing at the moment of the tick
becomes audible silence on the mic-side output. No waiting, no retry, no carry.**

### 2.2 V2 writer cadence (current Phase 5 CLOSED config)

Speaker-side writer is the Phase 5 pump `PumpToCurrentPositionFromQuery` at
[minwavertstream.cpp:1884+](../Source/Main/minwavertstream.cpp#L1884), called from
`GetPositions()` which is driven by the audio client's position query cadence. For
WASAPI shared (Chrome/YouTube), that is typically 10 ms or longer per query. The
pump then batch-writes `newFrames` = elapsed-since-last-fire into the pipe.

On the reader side, the mic-stream DPC timer is the 1 ms `TimerNotifyRT` at
[minwavertstream.cpp:1567+](../Source/Main/minwavertstream.cpp#L1567). The mic DPC
thus fires **~10x more often** than pump writer fires — there is a structural
cadence mismatch (writer period ~10 ms, reader period 1 ms).

Between two pump fires, the reader drains approximately `pumpInterval × 48` =
480 frames. Any moment the ring cushion drops below 48 frames, the next mic tick
triggers zero-fill = dropout.

Phase 5 CLOSED default sets the pump flag, making pump the sole writer. Legacy
rollback (toggling `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` off) puts the legacy
`ReadBytes` on the 1 ms timer instead — writer and reader now both on the same 1 ms
cadence, but via the QPC-based carry-forward byte count formula (see § 3.2 for the
baseline equivalent), which has its own jitter.

---

## § 3. AO baseline read-side path (b856d94)

Entry: `LoopbackRead(pLoopback, Data, Count)` (read via
`git show b856d94:Source/Utilities/loopback.cpp` lines 646-712), called from the
mic-side DPC's `WriteBytes` equivalent in baseline `minwavertstream.cpp`.

### 3.1 `LoopbackRead` underrun policy (b856d94 lines 646-712)

```
toRead = min(Count, pLoopback->DataCount)
memcpy toRead bytes from ring with wrap handling
if toRead < Count:
    RtlZeroMemory(Data + toRead, Count - toRead)
```

**Exact same partial-read + zero-fill tail policy as V2 `FramePipeReadFrames`.**
No difference in the underrun handling itself. Answer to the fixed question:
baseline produces silence in the hole, same as V2. **Read-side policy is NOT
where the divergence lives.**

### 3.2 Baseline writer cadence — the MicSink direct-push bypass

Baseline has a `LoopbackWrite` fast path that V2 removed. From `git show b856d94`:

```c
VOID LoopbackWrite(pLoopback, Data, Count)
{
    // ... standard ring write ...

    // Direct push to Mic DMA - eliminates async timing gap.
    if (pLoopback->MicSink.Active && pLoopback->MicSink.DmaBuffer)
    {
        // copy Data directly into MicSink.DmaBuffer at MicSink.WritePos
        // advancing WritePos and TotalBytesWritten
    }
    ...
}
```

This path is **gated on `pLoopback->MicSink.Active`**. The activation is in
baseline `minwavertstream.cpp` around lines 1372-1396:

> `LoopbackRegisterFormat auto-activates MicSink if` *(format match condition)*

and

> `Always stash Mic DMA info for deferred MicSink activation. ...
>  LoopbackRegisterMicSink(pLB, m_pDmaBuffer, m_ulDmaBufferSize);`

The activation is conditional on `FormatMatch` — defined as both speaker and mic
client formats matching the **internal loopback format (48 kHz / 24-bit / 8-ch)**.

**Crucial implication**: when the mic reader path executes at
[minwavertstream.cpp:1654 in baseline](git show b856d94:Source/Main/minwavertstream.cpp),
the check is `pLoopback && pLoopback->MicSink.Active && pLoopback->FormatMatch`.
WASAPI shared clients (Chrome, YouTube) typically negotiate `48 kHz / 16-bit / 2-ch`
or `48 kHz / float / 2-ch`. These do **not** match internal `48k/24/8ch`.
Therefore:
- `FormatMatch = FALSE` for typical client formats
- `MicSink.Active` may be TRUE but the gate at line 1654 excludes the fast path
- Baseline falls back to the **slow read path** (loopback ring → LoopbackReadConverted
  → format conversion into mic DMA)

In other words, **the direct-push bypass that the comment claims "eliminates async
timing gap" is almost never active in practice** for standard consumer audio apps.
When the G8 measurement ran, baseline was exercising the same slow-path structure
as V2, with a different underlying ring type but an identical underrun policy and
similar writer/reader timing characteristics. That matches the observed ~14.34%
silence being within 0.3 percentage points of V2's ~14.31%.

### 3.3 Baseline writer: 1 ms timer + QPC-based byte displacement

Baseline writer runs on the same 1 ms `TimerNotifyRT` and uses the carry-forward
byte-displacement formula seen in `minwavertstream.cpp` (pre-V2):

```
ByteDisplacement = ((m_ulDmaMovementRate × TimeElapsedInMS) + carry) / 1000
```

where `TimeElapsedInMS` is derived from `KeQueryPerformanceCounter()` delta since
the last DPC. The quotient is exact on uniform 1 ms ticks, but QPC deltas are
**not strictly uniform** — Windows high-resolution timer coalescing, OS scheduling,
virtualization / power management all add sub-millisecond jitter. Over many ticks
the carry keeps the average byte count exact, but the **instantaneous per-tick
byte count varies**, and when the speaker DPC happens to emit 47 frames while the
mic DPC independently consumes 49 frames in the same millisecond, the ring
cushion shrinks by 2 frames. Over a sequence of unfortunate jitter events, the
cushion can dip to 0 transiently → next mic tick → zero-fill → visible dropout.

**Answer to the fixed question for baseline**: when the mic tick arrives and the
ring has less than 48 frames, baseline produces the same zero-filled tail as V2.
The interesting part is **how often the ring has less than 48 frames**, not what
happens when it does.

---

## § 4. VB-Cable read-side and cadence behavior

Source: [results/vbcable_pipeline_analysis.md](vbcable_pipeline_analysis.md)
decompile. (Read-only reference; original VB source unavailable.)

### 4.1 Timer drift correction — § 2.2 of the decompile

> The magic constant `-0x5c28f5c28f5c28f5` in the 128-bit multiply ... yields
> `value * 63/64`. This implements **phase correction**: the next tick deadline is
> `base + (count * interval) × 63/64`, which subtly catches up if ticks are late,
> preventing cumulative drift.
>
> Every 100 ticks, the base timestamp resets.

VB uses `ExAllocateTimer` with a 1 ms period and then actively phase-corrects the
next-tick deadline based on how many ticks have elapsed since the base timestamp.
If a tick is late, the next tick comes slightly sooner to compensate. Over 100-tick
cycles, the base timestamp resets to the QPC wall clock.

AO (both baseline and V2) uses `ExSetTimer(-HNSTIME_PER_MILLISECOND,
HNSTIME_PER_MILLISECOND, NULL)` at
[minwavertstream.cpp:1567-1573](../Source/Main/minwavertstream.cpp#L1567-L1573).
No phase correction; the next tick is simply relative to the previous one.

**Divergence #1 (strongly supported, structural)**: VB actively compensates for
tick skew every tick; AO does not. Over a 15 s window with an ExSetTimer base
period of 1 ms, measurable drift between speaker DPC scheduling and mic DPC
scheduling can push fill to zero for brief moments, producing the observed
~1-2 ms silence gaps at ~8-20 events/sec.

### 4.2 Pre-calculated per-tick frame count

From § 2.3 of the decompile:

```c
// param_1+0x1bc = bytes per tick (pre-calculated)
// param_1+0x1c0 = frames per tick (pre-calculated)
uVar1 = *(uint *)(param_1 + 0x1bc);  // bytes this tick
lVar8 = *(int *)(param_1 + 0x1c0);   // frames this tick
```

VB pre-computes "bytes per tick" and "frames per tick" at stream initialization and
reads those constants per tick. Combined with the drift-corrected timer, the
per-tick write count is **uniform by construction**: no QPC delta math, no carry,
no jitter.

AO re-computes per-tick byte count on every DPC invocation via
`(DmaMovementRate × TimeElapsedInMS + carry) / 1000`, which produces jittery counts
whenever the QPC delta is not an exact multiple of 1 ms. See § 3.3 for the
mechanism.

**Divergence #2 (strongly supported, structural)**: AO recalculates per-tick frame
count from QPC delta on every tick; VB uses a pre-calculated constant. AO's speaker
and mic DPCs therefore emit slightly different sample counts per tick even under
nominally identical 1 ms timing, creating instantaneous producer/consumer imbalance.
VB does not have this class of imbalance because its per-tick counts are constants.

### 4.3 VB read-side underrun policy — § 4.1 and § 4.2 of the decompile

Path A (decompile § 4.1):

```c
if (...available < param_2) {
    iVar5 = 100;
    FUN_140007940(*(param_1+0x178), 0, param_2);  // fill scratch with silence
}
```

Fills the scratch buffer with zeros if available < requested. Same as AO's
"fill==0 → entire zero-fill". **Read-side underrun fill policy is equivalent
between VB and AO — not the divergence.**

§ 4.2 of the decompile describes a "recovery mode" keyed on `fill < half capacity`
that returns error code `-12` to the caller. **This is a flag for upstream logic,
not a different output behavior at the instant of the empty read**. We did not
trace the caller of `-12` in the decompile. Flagged as open for a deeper dive if
needed; not in the G8 scope cap.

### 4.4 Answer to the fixed question for VB

When the mic tick arrives and the ring has less than 1 tick worth of frames, VB
also produces zero-filled output. **Its advantage is not in what it does at the
moment of the empty tick; its advantage is in how rarely it encounters an empty
tick**, driven by (a) drift-corrected timer reducing long-term skew and (b)
pre-calculated per-tick frame count eliminating instantaneous jitter.

---

## § 5. Common weakness vs divergent behavior

### Common across VB / AO baseline / AO V2

1. **Underrun fill policy is equivalent** — all three zero-fill missing frames
   at the moment of the tick.
2. **1 ms tick period** — all three use the high-resolution kernel timer at 1 ms.
3. **Separate speaker DPC and mic DPC** — no shared loop; producer and consumer
   are independent schedulable units.
4. **Single-ring architecture** — one ring per cable pair shared between speaker
   DPC writes and mic DPC reads.
5. **8-frame minimum gate** — VB's decompile and AO V2's `FP_MIN_GATE_FRAMES` both
   use 8 frames as a floor to skip sub-sample noise ticks.

### Divergent

1. **Timer scheduling**: VB uses `ExAllocateTimer` with drift correction and
   phase recovery on tick skew; AO uses `ExSetTimer` with uniform period and no
   phase correction.
2. **Per-tick frame count**: VB uses pre-calculated constants stored at stream
   init (`param_1+0x1bc`, `param_1+0x1c0`); AO computes from QPC delta every tick
   with carry-forward.
3. **Baseline bypass path existence**: baseline has a `MicSink` direct-push that
   bypasses ring read on speaker write, but gated by `FormatMatch` which requires
   both sides to equal internal `48k/24/8ch`. Typical WASAPI shared clients do
   not negotiate that format, so the bypass is inactive in practice.
4. **V2 removed even the bypass path option** — removing dead code that was rarely
   active for real clients, but also removing the architectural possibility of
   reactivating it later. Minor and not the root cause.

---

## § 6. Hypothesis ranking

Graded against the fixed question and the measurement data.

### Rank 1 — strongly supported, structural

**AO's QPC-based per-tick frame count produces instantaneous jitter between
speaker DPC and mic DPC, causing the ring cushion to transiently hit 0 at a rate
matching the observed ~8-20 dropouts per second. VB avoids this by pre-computing
per-tick counts at init and using drift-corrected timer scheduling.**

Evidence:
- Observed gap pattern (1.0 / 1.1 / 1.2 / 1.5 / 1.8 / 2.1 / 5 ms, all 1-2 tick
  multiples) is consistent with "tick N had too little data, tick N+1 had extra"
  alternating jitter.
- Baseline and V2 both show this pattern to within 0.3 percentage points (14.34%
  vs 14.31%) despite having completely different ring implementations — the
  constant factor is the per-tick-from-QPC math and the uncorrected timer, both
  of which are identical between baseline and V2.
- VB's timer and frame-count discipline differ in exactly these two aspects.

### Rank 2 — supported, secondary

**Phase 5 pump-owned render transport exacerbates the issue by widening the
writer inter-fire interval from 1 ms to ~10-20 ms (WASAPI shared cadence), making
the ring cushion budget much tighter between writer fires. B1 prefill masks this
in short test windows but cannot fix the structural tightness.**

Evidence:
- G3 §12 pump-owned analysis found ~7.6 Hz pump fires during a sine test.
- Phase 5 CLOSED clean measurement shows slightly higher "silence runs" count
  (238) than legacy rollback (323 runs but shorter total silence) — Phase 5 has
  more burst-y writer cadence so each dropout covers more frames per event.
- B1 prefill produces ~1 s leading silence regardless of transport, plus a
  one-shot cushion that does not regenerate.

Note: this is a SECOND issue on top of Rank 1, not a replacement. Rank 1 affects
baseline which has no Phase 5; Rank 2 only affects V2 pump mode.

### Rank 3 — possible, not evidenced

**VB's `-12` error code recovery mode at `fill < half capacity` may drive some
upstream compensation (writer acceleration, tick deferral) that AO does not
replicate.**

Evidence: decompile § 4.2 mentions the error flag but not its consumers. Not
traced further.

### Rejected in G8

- **Read-side underrun policy difference** — three drivers use the same policy.
- **B1 / G2 / G6 / INF changes** — session-5 measurements already eliminated
  these.
- **Format-pair mismatch** — G5 + G6 eliminated this.
- **Phase 5 pump ownership as root cause** — baseline without Phase 5 shows the
  same class of dropouts; Phase 5 is an exacerbator, not the cause.

---

## § 7. Next experiments (read-only first, then minimal code)

### 7.1 Read-only verifications to strengthen Rank 1 before any code change

1. **Kernel instrumentation one-liner (read-only in intent)**: add a 1-s-window
   DbgPrint of per-tick byte displacement distribution in `ReadBytes`. Histogram
   of actual per-tick frame counts across 15 s. Expect to see a distribution
   centered at 48 with visible ±1 or ±2 frame spread at the speaker DPC. Same on
   mic DPC side. If the histogram is narrow and centered, Rank 1 is weakened.
   If spread exists and speaker vs mic show uncorrelated drift, Rank 1 is
   strengthened. *(Requires loopback.cpp edit, but no structural change — this
   counts as "instrumentation, uncommitted" per the same rules as G2/G6.)*

2. **VB decompile deeper dive**: find the producer of `param_1+0x1bc` /
   `param_1+0x1c0` at VB stream init. Verify that these really are constants and
   not updated per tick. Confirm the 63/64 phase correction math. 100% read-only.

3. **AO baseline minwavertstream read**: `git show b856d94:Source/Main/minwavertstream.cpp`
   — locate the baseline `ReadBytes` and confirm it uses the same QPC-delta
   byte-displacement formula. If it does, Rank 1 hypothesis applies to both
   baseline and V2. 100% read-only.

### 7.2 Minimal-surface experiments (only after 7.1)

These would graduate into a proposal, not implemented in G8:

- **Experiment A — pre-calculate per-tick frame count at stream init**. Replace
  the QPC-delta byte-displacement in `ReadBytes` with a stored constant computed
  from `m_ulDmaMovementRate`. Align speaker DPC and mic DPC to emit the same
  fixed 48 frames per tick regardless of QPC jitter. Expected: silence gaps drop
  toward the VB rate (~0-2 events per 15 s).
- **Experiment B — widen ring cushion**. Raise `TargetFillFrames` or the legacy
  equivalent so transient jitter has headroom to absorb without hitting 0. B1
  prefill partially validated this but only in startup; Experiment B would
  structurally raise the steady-state cushion. Probably complementary to A
  rather than standalone fix.
- **Experiment C — timer drift correction**. Switch from `ExSetTimer` to
  `ExAllocateTimer` + periodic phase adjustment mimicking VB's 63/64. Structural
  change to timer model.
- **Experiment D — unified DPC**. Collapse speaker DPC and mic DPC into a single
  combined tick handler that does both write and read under one lock per tick.
  Most invasive; structural architecture change.

**Priority**: A first, B second as safety net, C/D only if A+B insufficient.

### 7.3 Open question held forward

**"예전엔 좋았다" user memory** — not resolved by G8. Possibilities still in play:

- User's earlier test exercised a code path where `FormatMatch` was true
  (e.g., a test tone player that used 48k/24/8ch output), activating the MicSink
  direct-push bypass and producing a clean result. If so, the clean memory was
  real for that specific path, and our current test using WASAPI shared
  (Chrome/YouTube) has never exercised the bypass on any of the git commits we
  tested.
- User may have heard a cleaner result at a commit we have not yet tested
  (between earlier main commits and b856d94). Bisect between an older known
  commit and b856d94 is the only way to narrow this.
- User's clean memory may be of a different device entirely (VB-Cable in a prior
  session, physical device) rather than AO. Human memory of audio routing over
  days is fallible — we observed exactly this routing-memory confusion earlier
  in the session when Chrome per-app routing was stuck on Realtek.

G8 cannot resolve this. Carry to the next gate.

---

## § 8. Wording discipline

- No "confirmed" on any mechanism; "strongly supported, structural" is the
  ceiling. Applied to Rank 1 and the timer/per-tick-count divergences.
- The 1-2 ms gap pattern interpretation as "DPC-tick-quantized silence fill" is
  treated as strongly supported, structural per the user rule that repeated
  integer-tick-multiple gaps qualify.
- The measurement numbers themselves are treated as confirmed data points
  within the limits of the specific test harness used.
- Rank 2 (Phase 5 as exacerbator) is "supported, secondary" — weaker than
  Rank 1 because baseline shows the same silence percent without Phase 5.
- Rank 3 (`-12` recovery) is "possible" — we have no decompiled caller chain.

---

## § 9. Operating state after G8

- No source code changes, no INF changes, no commits.
- Working tree still carries uncommitted B1 + G2 + G6 instrumentation + INF
  plus session untracked files — no change from session start.
- Installed driver: Phase 5 CLOSED clean (2c733f1 reinstall completed pre-G8).
- Phase 6: BLOCKED.
- Next session path: § 7.1 read-only verifications, then § 7.2 Experiment A as
  the first fix-track proposal. Not started in G8.

---

## § 10. POST-G8 CORRECTION (2026-04-14 session close)

G8 § 6 ranked Option 2 (hns-precision byte displacement, a.k.a. Experiment A in
§ 7.2) as the most promising mechanism for the observed dropout pattern. After
G8 was written, Experiment A was implemented and measured. Results require two
corrections to the § 6 hypothesis framing.

### 10.1 Experiment A result — Bug A confirmed fixed

With Option 2 applied to `minwavertstream.cpp UpdatePosition` (uncommitted),
Phase 5 rolled back to legacy via IOCTL, Chrome/YouTube loopback via
`loopback_rec.py`:

| Metric | AO baseline b856d94 | AO Phase 5 CLOSED clean | **AO Option 2 (legacy)** | VB reference |
|---|---|---|---|---|
| Leading silence | 1981 ms | 1429 ms | 650 ms | 30 ms |
| Silence runs > 1 ms | 116 | 238 | **17** | 9 |
| Total silence | 14.34% | 14.31% | **4.52%** | 0.27% |
| **Body silence** | ~13% | ~13% | **0.20%** | ~0.27% |

Body silence rate dropped from ~14% to 0.20% — at or below VB-reference level.
Option 2 resolved the class of silence drops that G8 § 6 Rank 1 described. The
"ms-quantization bimodal per-call rounding" hypothesis is **strongly supported,
structural** and the fix is now demonstrated end-to-end (not committed).

Call this **Bug A** — resolved by Option 2.

### 10.2 Experiment A exposed a second, distinct symptom — Bug B

With Bug A's silence drops removed, a previously masked symptom became
perceptually dominant: for the first ~4 seconds after audio starts, the AO mic
capture plays a **20-ms audio chunk repeated over and over**, then at ~4.0 s
transitions abruptly to clean audio. User describes this as "트랜스포머 소리" —
a robotic chopped voice.

Signature (full details: [results/g9_bug_b_signature.md](g9_bug_b_signature.md)):

- Autocorrelation `AC[20 ms] ≈ 0.75–0.81` in every AO test (including baseline)
- Direct sample equality check: 20-ms chunks at 1.00 s / 1.02 s / 1.04 s in
  Option 2 capture differ by ≤ 2 int16 units (noise-floor identical)
- VB reference shows `AC[20 ms] = −0.18` → no replay
- Abrupt transition to clean audio at ~3997 ms (coinciding with a 1.85 ms
  silence event)

Call this **Bug B** — not addressed by Option 2, signature captured only.

### 10.3 Bug A vs Bug B separation

| | Bug A | Bug B |
|---|---|---|
| Symptom | Periodic 1–2 ms silence drops across whole stream | 20 ms audio chunk repeated for ~4 s after start |
| Silence detection | Catches it easily | **Misses it** — samples are non-zero |
| Autocorrelation signature | None specific | `AC[20 ms] ≈ 0.8` |
| Masked by | N/A | Bug A silence drops (while Bug A present) |
| Masked other bug | N/A (dominant) | N/A |
| Fixed by Option 2 | **Yes** (confirmed in measurement) | **No** |
| Present in VB reference | No | No |
| Present in AO baseline b856d94 | Yes (116 silence runs) | Yes (`AC[20 ms] = 0.73`) |

G8 § 6 Rank 1 wording only applies to Bug A. **Bug B was not in G8's model at
all** and needs its own investigation gate (G9+).

### 10.4 G8 § 6 Rank 1 wording update

Original: "AO's QPC-based per-tick frame count produces instantaneous jitter
between speaker DPC and mic DPC... VB avoids this by pre-computing per-tick
counts at init and using drift-corrected timer scheduling."

This remains **strongly supported, structural** as the explanation for Bug A.
Option 2 (hns-precision replacement of the ms-quantized math, not full VB-style
pre-computed constants) confirms the mechanism: removing the ms-level integer
rounding alone drops body silence from ~14% to 0.20%. Drift-corrected timer and
pre-computed per-tick constants may further tighten the remaining ~0.2 % but
are not required to close the bulk of Bug A.

### 10.5 User evidence requiring re-investigation

During session close, user stated:

> 원래 이번에 구조변경하기전에는 이런게 없었어. 통화시에는 안좋았고 유투브
> 녹음할때는 처음부터 좋았었어. 그런데 vb처럼 바꾸려고 하면서부터 이렇게된거야.

This contradicts today's baseline b856d94 measurement (which shows Bug B
present). Either b856d94 is already inside the restructuring range, or the
pre-V2 test path used a different mode (exclusive vs shared, different player,
different default-device configuration). **Introduction point of Bug B is
pending bisect.** See [g9_bug_b_signature.md § 4.1](g9_bug_b_signature.md) and
§ 6 for next-session plan.

### 10.6 G8 § 7.2 Experiment priorities re-ranked

- **Experiment A (hns-precision byte displacement)** — ✅ implemented,
  measured, works on Bug A. Uncommitted. Commit decision deferred until Bug B
  is understood.
- **Experiment B (widen ring cushion)** — deferred; Bug A being fixed removes
  the main motivation, and Bug B is not a cushion problem.
- **Experiment C (timer drift correction)** — deferred; may close the residual
  ~0.2 % body silence in Option 2 but low-priority vs Bug B.
- **Experiment D (unified DPC)** — deferred; most invasive, not justified by
  current evidence alone.

### 10.7 Next-session entry point

G8 continues into **G9** (new gate: Bug B stale-replay origin). First action is
**not** another driver edit. It is to fix a deterministic repro path and then
bisect older commits for the earliest appearance of the 20 ms autocorrelation
signature. Once the introduction commit is found, read that diff and propose a
read-only analysis (G9) before any fix.

Option 2 edit stays in the working tree uncommitted. Phase 5 IOCTL state stays
as "legacy rollback" (resets automatically on next driver reload).
