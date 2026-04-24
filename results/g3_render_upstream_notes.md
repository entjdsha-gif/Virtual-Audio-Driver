# G3 — Render Write Upstream (Read-Only Analysis)

**Branch:** `feature/ao-fixed-pipe-rewrite` @ `2c733f1` (Phase 5 CLOSED)
**Date:** 2026-04-14
**Scope:** Read-only trace of the code path that drives `FramePipeWriteFromDma` for cable render (speaker) streams. No code changes, no proposals. Evidence-gathering only.

**Pattern under investigation:** `hypothesis: writer cadence quantization leaves near-zero headroom (strongly supported, structural)`. Observed in G2 on **both** Cable A and Cable B with 48k/16/2 PCM + WASAPI exclusive sine + active mic drain: each 1-second window shows WrF/s cycling through ~46625 × 3 + ~51815 × 1; Fill drops to 17 frames (Cable B) / 221 frames (Cable A); long-run average ≈ 47934 fps vs reader 48000 fps.

---

## § 1. Timer / DPC cadence

`ExSetTimer` at [minwavertstream.cpp:1567-1573](../Source/Main/minwavertstream.cpp#L1567-L1573) uses `HNSTIME_PER_MILLISECOND` for both the initial offset and the period: **1 ms timer, fires via `TimerNotifyRT`**.

`TimerNotifyRT` at [minwavertstream.cpp:2544-2624](../Source/Main/minwavertstream.cpp#L2544-L2624):
- Computes `TimeElapsedInMS` from `hnsCurrentTime - m_ullLastDPCTimeStamp + carry` ([L2576](../Source/Main/minwavertstream.cpp#L2576))
- Fires `bufferCompleted = TRUE` only when `TimeElapsedInMS >= m_ulNotificationIntervalMs` ([L2578-2584](../Source/Main/minwavertstream.cpp#L2578-L2584))
- **For cable endpoints**, `UpdatePosition(qpc)` is called **unconditionally every 1 ms tick** ([L2597-2606](../Source/Main/minwavertstream.cpp#L2597-L2606)), regardless of notification interval

Two stream-state fields driving the math:
- `m_ulDmaMovementRate` — set once at stream init to `pWfEx->nAvgBytesPerSec` ([L366](../Source/Main/minwavertstream.cpp#L366))
- `m_ulNotificationIntervalMs` — computed at buffer allocation, gates event emission but **not** position updates for cable ([L628](../Source/Main/minwavertstream.cpp#L628))

**At 48k/16/2, `m_ulDmaMovementRate = 192000 bytes/sec`.**

---

## § 2. `FramePipeWriteFromDma` call sites

| # | Function | Path | Driven by | File:Line |
|---|---|---|---|---|
| 1 | `PumpToCurrentPositionFromQuery` | Phase 5 pump render transport | `GetPositions()` queries from audio client | [minwavertstream.cpp:1884](../Source/Main/minwavertstream.cpp#L1884) |
| 2 | `ReadBytes` | Legacy render path | 1-ms timer `UpdatePosition → ReadBytes` | [minwavertstream.cpp:2296](../Source/Main/minwavertstream.cpp#L2296) |

There are **only these two call sites**. Capture side uses `FramePipeReadToDma` on the mic stream's own timer DPC path.

---

## § 3. `byteCount` derivation per call site

| Call site | Formula | Sources | File:Line |
|---|---|---|---|
| Pump | `bytesToMove = newFrames × nBlockAlign`; chunked by DMA wrap | `newFrames = totalElapsedFrames - m_ulPumpProcessedFrames` (query-driven time delta math) | [L1862](../Source/Main/minwavertstream.cpp#L1862), [L1876-1890](../Source/Main/minwavertstream.cpp#L1876-L1890) |
| Legacy | `ByteDisplacement = ((m_ulDmaMovementRate × TimeElapsedInMS) + carry) / 1000`, then block-aligned | `TimeElapsedInMS` from QPC delta since last `UpdatePosition` call; two carry fields: `m_hnsElapsedTimeCarryForward`, `m_byteDisplacementCarryForward`, plus `m_ulBlockAlignCarryForward` | [L1981-2004](../Source/Main/minwavertstream.cpp#L1981-L2004), [L2289-2307](../Source/Main/minwavertstream.cpp#L2289-L2307) |

### Legacy math verification at 192000 bytes/s with 1 ms timer

For exact 1 ms elapsed: `TimeElapsedInMS = 1`, carry unchanged.
`ByteDisplacement = (192000 × 1 + carry) / 1000 = 192 bytes` (no remainder; carry stays 0).
Block-align at 4 bytes: 192 / 4 × 4 = 192 (no sub-block carry).
→ **Per-tick output is exactly 48 frames under uniform 1 ms timing.** Over 1 s with 1000 uniform ticks: 48000 frames. Perfect.

Key observation: the carry-forward math is mathematically exact at 192000 B/s with 1 ms ticks. **If ticks are uniform, there is no slippage.** So the 46625/51815 pattern cannot come from arithmetic rounding in isolation — it requires *non-uniform tick spacing*.

---

## § 4. DMA wrap handling

Both call sites use the same "chunk-loop" pattern:
- Pump: `while (remaining > 0)` with `chunk = min(remaining, m_ulDmaBufferSize - bufferOffset)` ([L1876-1890](../Source/Main/minwavertstream.cpp#L1876-L1890))
- Legacy: `while (ByteDisplacement > 0)` with `runWrite = min(ByteDisplacement, m_ulDmaBufferSize - bufferOffset)` ([L2289-2307](../Source/Main/minwavertstream.cpp#L2289-L2307))

**No linearization scratch buffer is used upstream**. Wrap is handled by calling `FramePipeWriteFromDma` **1 or 2 times** per invocation (1 normally, 2 when the window straddles buffer boundary). The pipe-side diagnostic counter `DbgWriteFrames` aggregates both chunks since it increments per call — wrap does not cause lost frames.

---

## § 5. Calls per tick

- **Legacy timer path**: per 1 ms `TimerNotifyRT` → `UpdatePosition` → `ReadBytes` → `FramePipeWriteFromDma` **1 or 2 times** (wrap handling). At 48k/16/2, per-tick byte count is 192 bytes = 48 frames, so wrap happens every 20 ms (when `bufferOffset` crosses the 960-frame DMA buffer boundary).
- **Pump path**: `GetPositions` query from audio client → `UpdatePosition(qpc)` → `PumpToCurrentPositionFromQuery` → `FramePipeWriteFromDma` **1 or 2 times** (wrap). The pump is NOT driven by the 1 ms timer; it only fires when the client queries positions. Cadence is client-driven (typically 10-20 ms for WASAPI exclusive).

**This is a critical asymmetry.** Legacy is uniform 1 ms timer. Pump is client-polling-rate.

---

## § 6. Legacy vs pump split (Phase 5 dual-gate)

| Path | Entry condition | Frame count | Counter field | File:Line |
|---|---|---|---|---|
| Legacy | `isCable && !pumpOwnsRender` (line 2062) | `ByteDisplacement / nBlockAlign` via carry-forward math | `RenderLegacyDriveCount++` per invocation | [L2058-2064](../Source/Main/minwavertstream.cpp#L2058-L2064), [L2286](../Source/Main/minwavertstream.cpp#L2286) |
| Pump | `isCable && (m_ulPumpFeatureFlags & AO_PUMP_FLAG_DISABLE_LEGACY_RENDER)` | `newFrames = currentElapsedFrames - m_ulPumpProcessedFrames` | `RenderPumpDriveCount++` per invocation | [L1854-1897](../Source/Main/minwavertstream.cpp#L1854-L1897), [L1896](../Source/Main/minwavertstream.cpp#L1896) |

**Phase 5 RUN default** (per Phase 5 proposal): on `SetState(RUN)` for cable speaker, the flag is set, so `pumpOwnsRender == TRUE`. Legacy `ReadBytes()` is gated out at [L2062-2065](../Source/Main/minwavertstream.cpp#L2062-L2065). **Under Phase 5 default, only the pump path writes to the FRAME_PIPE.**

---

## § 7. Cadence-shaping candidates

Listed bluntly with plausibility for producing the observed 3 slow + 1 fast:

| # | Candidate | File:Line | Plausibility for 3+1 pattern |
|---|---|---|---|
| a | `m_byteDisplacementCarryForward` remainder of `(DmaMovementRate × ms) / 1000` | [L1993-1994](../Source/Main/minwavertstream.cpp#L1993-L1994) | **Low** — at 192000 B/s × 1 ms, remainder = 0. No drift on uniform ticks. |
| b | `m_ulBlockAlignCarryForward` sub-block remainder | [L2001-2003](../Source/Main/minwavertstream.cpp#L2001-L2003) | **Low** — 192 % 4 = 0 at every tick. |
| c | `m_hnsElapsedTimeCarryForward` sub-ms QPC residue | [L1981-1985](../Source/Main/minwavertstream.cpp#L1981-L1985) | **Medium** — if `UpdatePosition` is called from BOTH the 1 ms timer AND mid-tick `GetPositions`, the time stamp advances out-of-phase. Each mid-tick query "steals" elapsed time from the next timer tick. Can produce uneven `TimeElapsedInMS` values (0, 1, 1, 2, 0, 1, 1, 2, …). Over a 1-sec window the total bytes is preserved by carry, but the *instantaneous* per-tick count varies. |
| d | `FP_MIN_GATE_FRAMES = 8` pump min-gate | [loopback.h:208](../Source/Utilities/loopback.h#L208), pump helper | **Low-medium** — only fires on pump path with < 8-frame deltas. In pump mode with ~10-20 ms query cadence, newFrames ≈ 480-960, gate never fires. |
| e | Over-jump threshold `max(framesPerDmaBuffer*2, sampleRate/4) = 12000` | pump helper | **Very low** — only fires on >250 ms gap, never in normal polling. |
| f | **`UpdatePosition` called from both timer (1 ms) AND `GetPositions` (client rate)** | [L891](../Source/Main/minwavertstream.cpp#L891), [L1130](../Source/Main/minwavertstream.cpp#L1130), [L1164](../Source/Main/minwavertstream.cpp#L1164), [L2600](../Source/Main/minwavertstream.cpp#L2600) | **HIGH** — this is the likely origin of quantization. Every `GetPositions` call advances `m_ullDmaTimeStamp` via `UpdatePosition`, so the next timer tick sees sub-1-ms elapsed and emits 0 bytes (carry preserved). Subsequent timer tick emits 2 ms worth as catch-up. The 3+1 pattern would emerge if the client's `GetPositions` cadence lands at a specific beat frequency against the 1 ms timer. |
| g | Pump `newFrames` computation in `PumpToCurrentPositionFromQuery` | [L1854-1860](../Source/Main/minwavertstream.cpp#L1854-L1860) | **Unknown** — if Phase 5 pump owns, writes happen on client query cadence. Need to read that block. |

**Candidate (f) is the current leading explanation.** Multiple `UpdatePosition` call sites advance the same time stamp, and any mid-tick call fragments the 1 ms timer bucket.

---

## § 8. DMA buffer size

`m_ulDmaBufferSize` is set at buffer allocation from the OS-provided `RequestedSize_` ([L626 area](../Source/Main/minwavertstream.cpp#L626)). **The driver does not hardcode buffer size** — it accepts whatever portcls hands it via `AllocateBufferWithNotification`. For WASAPI exclusive at 48k/16/2 with low-latency, typical buffer is 960 frames × 4 bytes = 3840 bytes (20 ms). Exact value for the G2 run was not captured.

---

## § 9. Supported / against / ambiguous

### Supported (by code + G2 numbers)

1. **1 ms timer + legacy path produces exact 48 frames/tick under uniform timing**, so arithmetic alone cannot explain the 3+1 pattern.
2. **`UpdatePosition` is called from at least 4 sites** ([L891](../Source/Main/minwavertstream.cpp#L891), [L1130](../Source/Main/minwavertstream.cpp#L1130), [L1164](../Source/Main/minwavertstream.cpp#L1164), [L2600](../Source/Main/minwavertstream.cpp#L2600)), each advancing `m_ullDmaTimeStamp`. Mid-tick calls will fragment the 1-ms bucket into uneven sub-units.
3. **Both cables share one minwavertstream.cpp codepath** with per-instance state. The identical pattern on A and B rules out cable-local bugs and points upstream.
4. **Reader side is a pure 1 ms timer path** (no `GetPositions` interference from capture client), and indeed RdF/s sits exactly at ~48000 in G2. This asymmetry between writer (wobbly) and reader (stable) is **consistent** with the theory that client-driven `GetPositions` calls on the speaker side cause the jitter.
5. **No accumulator/gate/overjump in the pump path matches the 3+1 beat numerically** — ruling out candidates (d) and (e).

### Against (could complicate or invalidate the theory)

1. **Phase 5 RUN default is pump-owned** — meaning legacy `ReadBytes` is gated out per [L2062-2065](../Source/Main/minwavertstream.cpp#L2062-L2065). If that's active during G2, the 1 ms timer path is NOT writing to the pipe at all. In that case the pump (query-driven) is the sole writer, and the 1 ms timer + `GetPositions` interaction cannot directly shape pipe writes. The observed pattern would then need a different origin inside `PumpToCurrentPositionFromQuery`.
2. **We did not confirm which transport was active during G2**. No `RenderPumpDriveCount` vs `RenderLegacyDriveCount` snapshot was captured around the G2 sine run. This is the single biggest unknown blocking G4.
3. **If pump path owns**, then `newFrames` comes from a QPC-vs-`m_ulPumpProcessedFrames` delta, not from `m_byteDisplacementCarryForward`. The legacy carry math is irrelevant. The cadence would instead reflect client polling granularity.

### Ambiguous / not yet checked

1. **Was `AO_PUMP_FLAG_DISABLE_LEGACY_RENDER` set during G2 measurement?** If yes → pump owns, legacy dead. If no → legacy owns, pump shadow-only. We need to query `m_ulPumpFeatureFlags` via the existing `IOCTL_AO_GET_STREAM_STATUS` on a fresh sine run to answer this.
2. **What is the actual `m_ulDmaBufferSize` for Cable A / B under G2 conditions?** Unknown. Matters for wrap-boundary analysis.
3. **What is the `GetPositions` call rate from WASAPI exclusive capture/render on this workload?** Unknown. Matters for the candidate-(f) theory.
4. **Does `PumpToCurrentPositionFromQuery` have any short-interval conditional skip** (other than the 8-frame gate and over-jump gate) that could produce 3+1 beats? Not yet read in detail; the agent noted the transport block at [L1854-1897](../Source/Main/minwavertstream.cpp#L1854-L1897) but did not analyze the `newFrames` computation above line 1860.
5. **The 4-tick cycle is at 1-second granularity in our log, not 1-ms granularity.** This is important. The 1S WR/RD print fires once per 1-second window. So the "3 slow + 1 fast" is a cycle at the **1-second observation rate**, meaning there is a **~4-second super-cycle** in writer throughput. Over 4 seconds, the writer emits ~191688 frames (vs expected 192000 = 312 short). The 4-second period is suspicious and does not match any obvious 1 ms / 20 ms rhythm. **Origin of the 4-second period is the biggest unexplained observation.**

---

## § 10. Recommended next steps before G4 decision

Read-only continuation within the G3 envelope:

1. **Check Phase 5 ownership state during a G2 repeat**. Run `test_stream_monitor.py --once` (or similar) immediately after a `g2_sine_48_16_2.py` run and inspect `PumpFeatureFlags` (0x03 = shadow-only legacy; 0x07 = legacy gated, pump owns). This alone decides whether the legacy theory or the pump theory is relevant.
2. **Read `PumpToCurrentPositionFromQuery` in full**, especially the `newFrames` computation at [L1854-1890](../Source/Main/minwavertstream.cpp#L1854-L1890) and any entry-side filtering above it.
3. **Find the 4-second super-cycle origin**. Possibilities worth investigating:
   - A Phase 3 shadow-window flush at `AO_PUMP_SHADOW_WINDOW_CALLS = 128` calls ([L1904](../Source/Main/minwavertstream.cpp#L1904)) — 128 × 20 ms pump invocations ≈ 2.56 s (not quite 4, but adjacent)
   - A WASAPI client internal polling beat
   - Clock drift between QPC and the audio device clock (but reader is clean against QPC, so unlikely)
4. **Capture `m_ulDmaBufferSize`** by exposing it via the existing IOCTL (already has a struct, would be a free add) — deferred if not needed for G4.

These are all read-only + existing IOCTL queries, no code changes required.

---

## § 12. Post-snapshot resolution (2026-04-14, same session)

After §11 was written, a `test_stream_monitor.py --once` snapshot was taken while a fresh 15 s Cable A sine run was in flight. This resolves the single biggest ambiguity that §9 flagged (pump vs legacy ownership) and reframes the leading hypothesis.

### Snapshot raw values

**Snapshot 1 — 17:19:22 (mid-sine run):**
```
CableA Render : Gated=1 OverJump=0 Frames=130208 Inv=28 Div=0 Flags=0x00000007
                | PumpDrv=26 LegacyDrv=0
CableA Capture: Gated=0 OverJump=0 Frames=0     Inv=0  Div=0 Flags=0x00000003
```

**Snapshot 2 — 17:19:50 (~10 s after sine ended):**
```
CableA Render : Gated=0 OverJump=0 Frames=721159 Inv=142 Div=0 Flags=0x00000000
                | PumpDrv=140 LegacyDrv=0
```

**Delta between snapshots (28 s wall-clock, ~15 s of actual sine playback inside that window):**
- `PumpDrv += 114`
- `LegacyDrv += 0`
- `Inv += 114`
- `Frames += 590951`

### What this tells us

1. **Phase 5 render ownership held pump-side throughout the G2 run.** `Flags=0x00000007` in snapshot 1 = `ENABLE | SHADOW_ONLY | DISABLE_LEGACY_RENDER`. The Phase 5 dual-gate was armed.
2. **`LegacyDrv = 0` across the full 28 s window → legacy `ReadBytes` fired zero times** for Cable A's speaker stream. The legacy carry-forward math and candidate (f) from §7 (multi-site `UpdatePosition` fragmenting 1 ms timer buckets) are **not relevant** to the observed cadence.
3. **Pump was the sole writer.** All frames that reached `FramePipeWriteFromDma` came through `PumpToCurrentPositionFromQuery`, driven by the audio client's `GetPositions` queries.

### Rough pump fire rate (bounded by active-window assumption)

With 114 pump fires observed across the 28 s window, expressing the value as "fires per second of active sine" depends on how much of the 28 s window the sine stream was actually in RUN state. **Under the assumption that the sine held RUN for its full nominal 15 s inside this window**, the effective rate is approximately **~7.6 pump fires/s (≈ 130 ms between fires)**. Treating this as *order-of-magnitude* is safe; treating it as a precise figure is not, because:
- The bounding-window assumption is approximate (sine start / stop instants not captured against the monitor's timestamps)
- `Inv` includes both transport-firing and Gated-skip counts (`Gated` went from 1 to 0 between snapshots)
- `Frames` total includes per-call accumulated deltas, not post-facto written bytes

The takeaway is not the exact Hz but the **order of magnitude**: pump is firing at **single-digit to low-tens of Hz**, *not* the ~50 Hz rate implied by a 20 ms WASAPI buffer period.

### How this explains the G2 observation

The 1 ms reader (mic DPC `FramePipeReadToDma` on the capture-side timer) drains the pipe at ~48 frames per ms. Between two pump fires spaced ≈ 130 ms apart, the reader drains ≈ 6240 frames. Each pump fire then delivers roughly that many frames back in one burst.

**Mean pipe fill is therefore pinned at the level reached just after each pump fire**, and between fires it marches downward toward near zero. Observed Fill minima at the 1 s DbgPrint sample moments:
- Cable A: **221 frames** (~4.6 ms headroom)
- Cable B: **17 frames** (~0.35 ms headroom)

These are the instantaneous fill values the 1 s DbgPrint happened to capture — by selection bias near (but not at) the cycle trough. Real cycle troughs are very likely **at or below zero during transient DPC delays**, which is consistent with the small `dUnderrun` values (2, 9) observed on Cable B under otherwise lab-clean conditions.

### Why "3 slow + 1 fast" at the 1-second window — beat artifact

If pump fires at ≈ 7.6 Hz, then `7.6 × 4 s = 30.4` fires per 4-second window. A 1-second observation window can contain **either 7 or 8 pump fires** depending on phase. With ≈ 6125 frames / fire average:
- 7-fire window: `7 × 6125 ≈ 42875` frames — observed slow tick ≈ 46625 (same order)
- 8-fire window: `8 × 6125 ≈ 49000` frames — observed fast tick ≈ 51815 (same order)

Exact per-fire frame count varies because each fire drains whatever elapsed since the previous one. The "3 slow + 1 fast" visual rhythm is consistent with a **beat artifact between an irrational-ratio fire rate and the 1-second observation window**, not with any 4-second rhythm inside the driver. There is no ~4 Hz accumulator, gate, or window flush in the upstream code — the 4-second visual rhythm is a measurement side-effect of the observation cadence, not a driver-side period.

### Updated leading hypothesis

Previous wording: `writer cadence quantization leaves near-zero headroom (strongly supported, structural)`

Refined to:
> **`pump-owned render path fires at client query cadence (~7-8 Hz under this workload), leaving near-zero fill headroom against a 1 ms reader drain; strongly supported, structural`**

Phrasing notes:
- The earlier "cadence quantization" framing is **demoted** — it was not arithmetic quantization inside the driver, it was beat-frequency aliasing at the observation window.
- The primary mechanism is a **writer/reader period mismatch / sparse query-driven writer cadence**: writer period ≈ 130 ms, reader period 1 ms, order-of-magnitude gap.
- "Structural" still applies because it is rooted in Phase 5's design choice to drive cable render transport exclusively from `GetPositions` queries. Any workload with sparse position queries will exhibit the same pipe-drain-to-near-zero cycle.
- "Strongly supported" not "confirmed": we observed ownership and delta counts, and the cycle math lines up; we did not instrument per-fire frame deltas directly, and we did not measure the fire rate with its own counter window.

### Post-§12 posture toward G4

- `render_fix` is now the leading branch by a wide margin (no sub-branches decided yet)
- `widen_scope` is unlikely given the clear upstream origin
- `reinstrument` is useful only for per-fire granularity if a proposed fix needs to be measured
- `phase6_ready` is explicitly off the table until render-fix branch resolves

## § 11. G4 branch posture (NOT a decision)

Based on this note alone, G4 branch selection is **still ambiguous**:
- If step (1) above shows pump owns and the 4-second super-cycle has a pump-path origin → likely `render_fix` after locating it
- If pump owns but the 4-second cycle has no code-level origin → `widen_scope` (look at WASAPI / audio engine polling, clock drift, etc.)
- If legacy owns somehow (Phase 5 dual-gate broken) → that itself is a regression; `render_fix` + closer look at gate logic
- If nothing matches and we need finer timing resolution → `reinstrument` (per-100ms or per-tick DbgPrint)
- **`phase6_ready` is not a current option** — writer anomaly is real and reproducible, and we haven't isolated it to a benign cause.

No decision made. This note closes G3's read-only phase with open questions listed, not answered.
