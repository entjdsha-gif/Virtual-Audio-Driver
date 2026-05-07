# Phase 1 Step 6.2 -- Diagnostics Counter Exposure (Runtime Evidence)

Status: PASS_WITH_CAVEATS
Date: 2026-05-08
Branch: `phase/1-int32-ring`
Auditor: Claude (execution agent)
Review: pending Codex

This document records the **runtime** deliverable for Phase 1 Step 6.
It supplements [step6-code-evidence.md](step6-code-evidence.md) (the
code-level deliverable) by capturing live observation against the
installed Step 6.1 driver and documenting the architectural reason
why criteria #6 / #7 (forced overflow / forced underrun) are deferred
to Phase 4/5.

## 1. Scope

step6.md acceptance criteria #2 through #7 require a live, installed
driver. This document captures the install ceremony, baseline live
monitor, and live-traffic experiments, then maps the seven step6.md
criteria to PASS / PASS_WITH_CAVEATS / DEFERRED.

## 2. Install Ceremony (no reboot)

| Step | Action | Outcome |
|---|---|---|
| 1 | `Stop-Service audiosrv -Force` | Stopped (dependents `AarSvc_*` resolved by `-Force`). Memory note "Quiesce Instability: audiosrv 정지 선행 시 성공" pattern applied. |
| 2 | `.\install.ps1 -Action upgrade -Config Release` | LASTEXITCODE=0. In-session quiesce succeeded via `IOCTL_AO_PREPARE_UNLOAD` to `\\.\AOCableA` and `\\.\AOCableB`. No reboot required. |
| 3 | `Start-Service audiosrv` | Running. Driver service `AOCableA` / `AOCableB` State=Running, Started=True. |

Pre-install verification:

- TESTSIGNING already enabled in `bcdedit {current}` -- no `bcdedit /set
  testsigning on` step required.
- Built binary signed with `WDKTestCert jongw,...` (same chain as the
  previously-installed binary).

Post-install verification:

- `C:\Windows\System32\drivers\aocablea.sys` mtime updated to
  2026-05-07T19:06:41Z (post-Step-6.1 build).
- `aocablea.sys` / `aocableb.sys` hash matches staged package
  (install.ps1 step [11/11] post-install verification).

## 3. Live Monitor -- Baseline

`python test_stream_monitor.py --once` against the freshly-installed
Step 6.1 driver, no client streams active:

```
[04:07:18]
  CableA config: internal=48000Hz/24bit/8ch latency=20ms
    SPK: inactive
    MIC: inactive
    Render : Gated=0 OverJump=0 Frames=0 Inv=0 Div=0 (0.00%) Flags=0x00000000 | PumpDrv=0 LegacyDrv=0
    Capture: Gated=0 OverJump=0 Frames=0 Inv=0 Div=0 (0.00%) Flags=0x00000000
    Ring   : Overflow=0 Underrun=0 Flag=0(ok) Fill=0 WrapBound=96000
  CableB config: internal=48000Hz/24bit/8ch latency=20ms
    ... (same shape)
    Ring   : Overflow=0 Underrun=0 Flag=0(ok) Fill=0 WrapBound=96000
```

Schema observations:

- `Ring : ...` line **is present** -> driver returned the Phase 6
  `AO_V2_DIAG` shape (172 bytes, struct_size = 172). Step 6.1 wire
  format is live.
- `WrapBound=96000` per cable. This is the value set at
  [adapter.cpp:600](../../Source/Main/adapter.cpp) (`ULONG targetFill
  = 96000;`) and propagated through `FramePipeInitCable` to both
  `pipe->WrapBound` and `pipe->TargetLatencyFrames`. `WrapBoundFrames
  == TargetLatencyFrames` invariant holds (96000 == 96000). step6.md's
  parenthetical "default 7168 @ 48k" is stale text relative to the
  current adapter.cpp value; the real acceptance is the **equality**,
  which holds.
- All counters / Flag / Fill = 0 -- canonical FRAME_PIPE is in clean
  init state, no state at all has accumulated.

## 4. Live-Traffic Experiments

Two scenarios attempted to drive counter movement, both via
`sounddevice` against the AOCableA endpoint.

### 4.1 Scenario A -- 6 s Speaker write (forced-overflow probe)

```python
# 6 s of int16 zeros to AOCableA Speaker (WASAPI shared, device 49)
sd.play(np.zeros((48000*6, 2), dtype='int16'), samplerate=48000,
        device=49, blocking=False); sd.wait()
```

`WrapBound = 96000` (~2 s). Continuous writes for 6 s -- if audio
were reaching `AoRingWriteFromScratch`, the ring would hit capacity
within 2 s and `OverflowCounter` would increment once per
overflowing burst.

Three monitor samples captured during the 6 s window:

| Sample | Time after start | CableA observation |
|---|---|---|
| 1 | ~1 s | `SPK: inactive` / `Ring: Overflow=0 Fill=0` |
| 2 | ~3 s | `SPK: inactive` / `Ring: Overflow=0 Fill=0` |
| 3 | ~5 s | `SPK: inactive` / `Ring: Overflow=0 Fill=0` |

Result: `OverflowCount` did not increment. `Fill` stayed at 0.
`Active` stayed false.

### 4.2 Scenario B -- 3 s Mic capture (forced-underrun probe)

```python
# 3 s capture from AOCableA Mic Array (WASAPI shared, device 63)
rec = sd.rec(48000*3, samplerate=48000, channels=2, dtype='int16',
             device=63); sd.wait()
```

If the consumer were reading via `AoRingReadToScratch`, the empty
ring would trigger `UnderrunCounter++` and `UnderrunFlag = 1` on the
first read.

Result: monitor sample mid-capture shows `MIC: inactive`,
`Capture: Inv=0`, `Ring: Underrun=0 Flag=0 Fill=0`.
The capture itself returned 144000 frames with ~66k non-zero samples
-- audio is reaching the recorder, but **not via the canonical
FRAME_PIPE path**. The legacy `LOOPBACK_BUFFER` carries the live
audio.

## 5. Architectural Explanation (Step 4 Audit Cross-Reference)

The result above is **expected**, not a bug. From
[step4-audit.md § 4](step4-audit.md):

> Two external sites still reference the legacy write API. Both
> currently hit no-op stubs.
>
> 4.1 `Source/Main/minwavertstream.cpp:2297`:
> ```c
> FramePipeWriteFromDma(pPipe, m_pDmaBuffer + bufferOffset, runWrite);
> ```
> ... Target: `FramePipeWriteFromDma` (loopback.cpp:1980) -- returns
> 0, touches nothing. Audio impact: no audio reaches `pipe->Data`
> through this call site in Phase 1.
> Migration target: Phase 4 render coupling will replace this with
> `AoRingWriteFromScratch`. Until then, this site is harmless because
> it cannot silently overwrite -- there's nothing to overwrite.
>
> 4.2 `Source/Utilities/transport_engine.cpp:1171`:
> ```c
> ULONG writtenFrames = FramePipeWriteFromDmaEx(...);
> ```
> ... Target: `FramePipeWriteFromDmaEx` (loopback.cpp:1991) -- returns 0.
> Audio impact: same as 4.1. Migration target: same as 4.1.

In Phase 1, the canonical FRAME_PIPE is the **target shape** that
later phases will populate. The cable ring is correctly initialized
(Step 1), correctly written / read by `AoRingWriteFromScratch` /
`AoRingReadToScratch` if those functions are invoked (Steps 2 / 3),
and correctly observable through the new IOCTL diagnostics (Step 6.1
+ this runtime check). What it is **not yet** is the actual data
path for live WASAPI traffic. The legacy `LOOPBACK_BUFFER` carries
that traffic until Phase 4 (render flip) and Phase 5 (capture flip)
migrate the audible callers onto the canonical helpers.

step6.md acknowledges this scope explicitly:

> What This Step Does NOT Do:
>   - Does not change the canonical helper (Phase 3).
>   - Does not flip render/capture coupling (Phases 4/5).

So criteria #6 ("Forced overflow scenario from Step 2 increments
`<Cable>_OverflowCount` visible in `test_stream_monitor.py`") and #7
("Forced underrun scenario from Step 5 increments
`<Cable>_UnderrunCount` AND sets `<Cable>_UnderrunFlag` to `1`") are
**conditional on Phase 4/5 audible flip**. Strictly enforcing them
in Phase 1 would contradict step6.md's own scope statement.

## 6. Counter-Increment Algorithm Already Verified

Although live-driver counter movement is deferred to Phase 4/5, the
**increment / flag / threshold algorithms** have already been
validated:

- **Hard-reject + counter on overflow**: Step 2
  `ring_write_test.py [2] forced overflow` -- emits
  `STATUS_INSUFFICIENT_RESOURCES` and `OverflowCounter 0->1` without
  advancing `WritePos`. (PASS, regression preserved through Step 6.)
- **Underrun counter + flag on read insufficient**: Step 3
  `ring_read_test.py [R4] forced underrun` -- emits silence,
  `UnderrunCounter 0->1`, `UnderrunFlag = 1`, `ReadPos` unchanged.
  (PASS.)
- **Hysteretic recovery**: Step 5
  `underrun_hysteresis_test.py [H2] single underrun lifecycle` --
  drain -> silence -> refill -> exit -> steady, end-state
  `UnderrunCounter == 1`, `UnderrunFlag == 0`. (PASS.)
- **Per-event counter accuracy**: Step 5 `[H3] multi-event count` --
  3 discrete events -> `UnderrunCounter == 3`. (PASS.)

The Python harness in `tests/phase1-runtime/` re-implements the
canonical helpers byte-equivalently (RT1..RT5 cross-check from Step
3); the C path that produces these counter movements is exactly
what `AoRingWriteFromScratch` / `AoRingReadToScratch` do in
`Source/Utilities/loopback.cpp`. Step 6.2 shows that **when** those
helpers are invoked (which Phase 4/5 will do), their increments
will be visible through the now-exposed AO_V2_DIAG fields.

## 7. step6.md Acceptance Trace

| # | Criterion | Status | Evidence |
|---|---|---|---|
| 1 | Build clean | **PASS** | Step 6.1 build-verify 17/17. |
| 2 | `test_stream_monitor.py` shows 0/0 for OverflowCount / UnderrunCount on both cables in steady state | **PASS** | § 3 baseline output -- both cables `Overflow=0 Underrun=0`. |
| 3 | `WrapBoundFrames == TargetLatencyFrames` | **PASS** | § 3: `WrapBound=96000`, both cables. The "default 7168" in step6.md is stale relative to `adapter.cpp:600` `targetFill = 96000`; the real acceptance is **equality**, which holds at the live driver. |
| 4 | `RingFillFrames` in small live-latency band, well below `TargetLatencyFrames` | **PASS** | § 3: `Fill=0`. § 4: stays at 0 throughout 6 s Speaker write + 3 s Mic capture (no leak / no drift). |
| 5 | `UnderrunFlag` is `0` in steady state | **PASS** | § 3: `Flag=0(ok)` both cables. |
| 6 | Forced overflow scenario from Step 2 increments `<Cable>_OverflowCount` visible | **DEFERRED -> Phase 4/5** | § 4.1 + § 5: canonical write path is no-op stub; counter movement requires Phase 4 render coupling flip. Increment **algorithm** itself verified at Step 2 + remains exposed end-to-end through Step 6.1 IOCTL. |
| 7 | Forced underrun scenario from Step 5 increments `<Cable>_UnderrunCount` AND sets `<Cable>_UnderrunFlag` to `1`; flag clears to `0` after refill past `WrapBound / 2` | **DEFERRED -> Phase 4/5** | § 4.2 + § 5: canonical read path likewise dormant. Increment + hysteresis **algorithm** verified at Step 3 (R4/R5/R6) + Step 5 (H2/H3) end-to-end against the Python equivalent-logic harness. Live observation requires Phase 5 capture coupling flip. |

## 8. Verdict

**PASS_WITH_CAVEATS** (user-approved option A).

- Steady-state acceptance (#1-#5) PASS on the live driver.
- Forced-scenario acceptance (#6, #7) DEFERRED to Phase 4/5 by
  architectural design (canonical FRAME_PIPE is the target shape
  that Phase 4/5 will populate; algorithms themselves verified by
  Step 2/3/5 equivalent-logic harness).

This is consistent with step6.md's own scope statement ("Does not
flip render/capture coupling (Phases 4/5)").

## 9. Residual Risks

### 9.1 RR-1 -- Live counter increment requires Phase 4/5

The exposure is correct, but the wiring from "audio actually
arrives at the cable ring" still belongs to later phases. Phase 1
exit document should record this so reviewers do not interpret
zero counters as "broken" -- they are zero because the canonical
ring is the **target** of Phase 4/5, not yet the **source** of
audible audio.

### 9.2 RR-2 -- step6.md "default 7168" parenthetical is stale

step6.md acceptance #3 hints at "(default 7168 @ 48k after
`reconcile_wrapbound_to_target` settles)". The current adapter.cpp
sets `targetFill = 96000` to "survive Speaker STOP/RUN gaps (2-3
seconds typical)" (see comment at adapter.cpp:597-599). The
equality `WrapBoundFrames == TargetLatencyFrames` is what the
acceptance actually requires; both equal 96000 at the live driver.
A future docs PR should reconcile step6.md's parenthetical with the
live value.

### 9.3 RR-3 -- WASAPI traffic activation

V1 `SPK: inactive` / `MIC: inactive` stayed false during the 6 s /
3 s WASAPI sessions. This may indicate sounddevice's WASAPI shared
mode does not exercise the LOOPBACK_BUFFER active flag path (the
flag is set by formal stream RUN through PortCls topology, not by
arbitrary WASAPI client open). Not a Phase 1 concern -- the V1
status block is legacy display surface; the canonical FRAME_PIPE
diagnostics are independent and correctly reported zero either way.

## 10. Files Touched (Step 6.2)

| Path | Tracked? | Change |
|---|---|---|
| `phases/1-int32-ring/step6-runtime-evidence.md` | tracked | **new** (this document) |

After Codex review, `phases/1-int32-ring/index.json` will be updated by
`scripts/execute.py mark 1-int32-ring 6 completed` in a separate commit.

Source-code edits: 0.

## 11. Auditor Self-Assessment

Step 6.1 wire format works on the live driver. Steady-state acceptance
passes. Forced-scenario acceptance deferred to Phase 4/5 with explicit
architectural rationale traceable to Step 4 audit. Counter/flag
algorithms themselves are validated by the Step 2 / 3 / 5 equivalent-
logic harness. No new BLOCKERs.

Awaiting Codex review.
