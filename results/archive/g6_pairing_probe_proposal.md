# G6 — Pairing Probe Proposal (Instrumentation-Only)

**Branch:** `feature/ao-fixed-pipe-rewrite` @ 2c733f1 + uncommitted B1 + G2 + B1 prefill
**Date:** 2026-04-14
**Status:** PROPOSAL — not implemented, not committed
**Phase 6:** still BLOCKED

---

## § 1. Goal

During a real Phone Link call, capture the exact **(bps, isFloat, channels, blockAlign,
SameRate, copyChannels, PipeChannels)** tuple that each of the two hot paths
(`FramePipeWriteFromDma`, `FramePipeReadToDma`) is seeing at the moment it chooses its
conversion branch. Emit **one DbgPrint line per direction on first observation, and
additional lines only when any field in the tuple changes mid-call.**

This is not throughput measurement and not format discovery at the WDM level.
It only confirms which write/read **branch pair** is actually selected at runtime, so
that the § 5 decision table in the G5 notes ([results/g5_format_path_notes.md](g5_format_path_notes.md))
can be resolved:

- If WR and RD both stay at `48k/16/2/SameRate=1/copyChannels=2` for the whole call →
  the format-pair mismatch hypothesis weakens and the next investigation moves to G7
  (KS filter range) or deeper value-path tracing.
- If any direction diverges, OR if any field changes mid-call → that log line is the
  direct evidence for the next fix proposal (SRC-on-mismatch vs format-pair-reject
  vs mid-call renegotiation handling).

---

## § 2. Why on-change-only, not rate-limited periodic

Rate-limited periodic was rejected up front:

1. **Signal-to-noise.** A 30-second call at 1-second periodic print per direction yields
   ~60 log lines where the answer is identical on all of them. On-change yields 2-4
   lines total for a clean steady-state run and one extra line for every real transition.
2. **Mid-call transitions are visible immediately.** Periodic prints hide a transition
   until the next tick. On-change prints the transition frame, not the frame after.
3. **No clock-sharing conflict with G2.** G2's `DbgLastPrintQpc` is under active
   `InterlockedCompareExchange64` pressure on both writer and reader. Adding G6 time
   bookkeeping to the same field would either corrupt G2's window or need a second
   timestamp field. On-change removes the time dimension entirely.
4. **Smaller, deterministic implementation.** No timer math, no window ownership
   protocol. Compare a packed snapshot ULONG, print on mismatch, atomically update.
   That is all.

---

## § 3. Touch surface

**File:** `Source/Utilities/loopback.cpp` — and only this file.

**Not touched:**
- `Source/Utilities/loopback.h` — no struct field added, no API change
- `FRAME_PIPE` struct — no new members (G6 state lives in file-scope static storage)
- `Source/Main/*` — no caller changes
- G2 1s-window instrumentation — untouched
- B1 prefill helper — untouched

**Physical separation:** G6 code goes in a new dedicated comment-marked block at the
top of the "Frame Pipe — Phase 2: batch DMA conversion" section of `loopback.cpp`,
**before** `FramePipeWriteFromDma`. The block declares the file-scope snapshots and a
single tiny helper. The two hot paths each get **one call** to that helper, with no
other code movement. A future `git restore Source/Utilities/loopback.cpp` reverts
G2 + B1 + B1 prefill + G6 together — the single-file rule preserves independent restore
from other files, and the within-file section markers preserve human review.

---

## § 4. File-scope state (no header leak, no FRAME_PIPE field)

```c
//============================================================================
// G6 (2026-04-14) — Write/Read format-pair probe (instrumentation only)
// Prints one line per direction when the observed (bps, isFloat, channels,
// blockAlign, SameRate, copyChannels, PipeChannels) tuple differs from
// the last print. No rate-limiting, no clock dependency. Uncommitted.
//============================================================================

// Packed signature bits (low → high):
//   [ 0.. 7] bps (0, 8, 16, 24, 32)
//   [ 8.. 8] isFloat
//   [ 9..13] channels            (1..31)
//   [14..21] blockAlign          (1..255)
//   [22..22] SameRate
//   [23..27] copyChannels        (0..31)
//   [28..31] PipeChannels        (1..15)   // pipe uses ≤ FP_MAX_CHANNELS = 16
//
// Cable 0 = A, 1 = B. Direction 0 = WR, 1 = RD. 4 snapshot slots total.
static volatile LONG g_G6PairingSnapshot[2][2] = { { 0, 0 }, { 0, 0 } };

static __forceinline ULONG G6PackSignature(
    ULONG bps, BOOLEAN isFloat, ULONG ch, ULONG blockAlign,
    BOOLEAN sameRate, ULONG copyCh, ULONG pipeCh)
{
    return  ((bps        & 0xFF)  <<  0)
          | (((isFloat  ? 1U : 0U) & 0x1)  <<  8)
          | ((ch         & 0x1F) <<  9)
          | ((blockAlign & 0xFF) << 14)
          | (((sameRate  ? 1U : 0U) & 0x1)  << 22)
          | ((copyCh     & 0x1F) << 23)
          | ((pipeCh     & 0x0F) << 28);
}

// Called at the very start of each direction's batch helper, after the
// null/initialized guard but BEFORE the SameRate fast-fail branch, so
// rate-mismatch conditions also get printed.
static VOID G6LogPairingIfChanged(
    PFRAME_PIPE pPipe, BOOLEAN isWriter,
    ULONG bps, BOOLEAN isFloat, ULONG ch, ULONG blockAlign,
    BOOLEAN sameRate, ULONG copyCh)
{
    ULONG cableIdx = (pPipe == &g_CableAPipe) ? 0
                   : (pPipe == &g_CableBPipe) ? 1
                   : 0xFFFFFFFF;
    if (cableIdx > 1)
        return;

    ULONG dirIdx = isWriter ? 0 : 1;
    LONG  sig    = (LONG)G6PackSignature(
                       bps, isFloat, ch, blockAlign,
                       sameRate, copyCh, pPipe->PipeChannels);

    LONG prev = InterlockedCompareExchange(
                    &g_G6PairingSnapshot[cableIdx][dirIdx], sig, 0);
    BOOLEAN changed;
    if (prev == 0)
    {
        // First observation for this slot — CAS above stored sig already.
        changed = TRUE;
    }
    else if (prev != sig)
    {
        // Subsequent mismatch — publish the new sig. Lossy on simultaneous
        // cross-DPC races, but two writers of the same new value converge.
        InterlockedExchange(&g_G6PairingSnapshot[cableIdx][dirIdx], sig);
        changed = TRUE;
    }
    else
    {
        changed = FALSE;
    }

    if (changed)
    {
        const char* id   = (cableIdx == 0) ? "A" : "B";
        const char* side = isWriter ? "WR" : "RD";
        DbgPrint("AO_PIPE[%s] G6 %s: bps=%u float=%d ch=%u align=%u "
                 "SameRate=%d copyCh=%u PipeCh=%u\n",
            id, side,
            bps, (int)isFloat, ch, blockAlign,
            (int)sameRate, copyCh, pPipe->PipeChannels);
    }
}
```

Race discipline:
- Snapshot is a single `LONG` per slot. Torn reads/writes are impossible on x64 for
  naturally aligned 32-bit values.
- Worst-case race: two DPCs on different cores observe the same change simultaneously
  and both print. Acceptable — the cost is one duplicate line in a log that is
  otherwise near-silent. The steady state after both return is correct.
- `InterlockedCompareExchange` on the first observation guarantees exactly-once
  initialization from 0 even under contention.
- **Does NOT acquire `PipeLock`.** The tuple fields (`SpeakerBitsPerSample`, etc.)
  are read by the existing hot path without the lock already, and G6 only reads the
  same values at the same moment. No new lock requirement introduced.

---

## § 5. Insertion points (exact)

### 5.1 `FramePipeWriteFromDma` — [loopback.cpp:1935+](../Source/Utilities/loopback.cpp#L1935)

Insert **immediately after** the null/initialized/speakerActive/empty-input guard at
[L1940-1945](../Source/Utilities/loopback.cpp#L1940-L1945), and **before** the
`!SpeakerSameRate` drop branch at [L1947](../Source/Utilities/loopback.cpp#L1947). This
placement guarantees:
- Rate-mismatch drops are visible (G6 logs them, then the existing drop code runs).
- The normal path is logged exactly once per observed tuple change.

Pseudo-diff:

```c
if (!pPipe || !pPipe->Initialized || !pPipe->SpeakerActive ||
    !dmaData || byteCount == 0)
{
    return 0;
}

+ // G6 pairing probe — on-change only, no rate limit
+ {
+     ULONG spkCh    = pPipe->SpeakerChannels;
+     ULONG pipeCh6  = pPipe->PipeChannels;
+     ULONG copyCh6  = (spkCh < pipeCh6) ? spkCh : pipeCh6;
+     G6LogPairingIfChanged(
+         pPipe, TRUE,
+         pPipe->SpeakerBitsPerSample,
+         pPipe->SpeakerIsFloat,
+         spkCh,
+         pPipe->SpeakerBlockAlign,
+         pPipe->SpeakerSameRate,
+         copyCh6);
+ }
+
// Fail-closed: rate mismatch requires SRC (Phase 3). Drop until then.
if (!pPipe->SpeakerSameRate)
{
    ...
```

`copyCh6` is recomputed here instead of reused from the existing `copyChannels` local
because that local is computed further down at [L1971](../Source/Utilities/loopback.cpp#L1971),
AFTER the rate-mismatch early return. To log on rate-mismatch drops we need the value
before that return, which costs three extra lines of local computation. Accepted.

### 5.2 `FramePipeReadToDma` — [loopback.cpp:2082+](../Source/Utilities/loopback.cpp#L2082)

Insert **immediately after** the null/initialized/empty guard, and **before** the
`!MicSameRate || !MicActive` silence branch at [L2088-2110](../Source/Utilities/loopback.cpp#L2088-L2110).

Pseudo-diff:

```c
if (!pPipe || !pPipe->Initialized ||
    !dmaData || byteCount == 0)
{
    return;
}

+ // G6 pairing probe — on-change only, no rate limit
+ {
+     ULONG micCh6   = pPipe->MicChannels;
+     ULONG pipeCh6  = pPipe->PipeChannels;
+     ULONG copyCh6  = (pipeCh6 < micCh6) ? pipeCh6 : micCh6;
+     G6LogPairingIfChanged(
+         pPipe, FALSE,
+         pPipe->MicBitsPerSample,
+         pPipe->MicIsFloat,
+         micCh6,
+         pPipe->MicBlockAlign,
+         pPipe->MicSameRate,
+         copyCh6);
+ }
+
// Fail-closed: rate mismatch requires SRC (Phase 3). Output silence.
if (!pPipe->MicSameRate || !pPipe->MicActive)
{
    ...
```

Note: the existing early-silence branch fires on either `!MicSameRate` or `!MicActive`.
G6 logs both conditions (SameRate is the probe's main output; MicActive=0 would make
the probe print `ch=0/align=0` values since there is no active mic format — but in
practice the probe is called *after* the stream is active, so this is a theoretical
edge case).

---

## § 6. Example log output

**Clean steady state (hypothesis: format-pair mismatch is NOT the cause):**
```
AO_PIPE[A] G6 WR: bps=16 float=0 ch=2 align=4 SameRate=1 copyCh=2 PipeCh=8
AO_PIPE[A] G6 RD: bps=16 float=0 ch=2 align=4 SameRate=1 copyCh=2 PipeCh=8
AO_PIPE[B] G6 WR: bps=16 float=0 ch=2 align=4 SameRate=1 copyCh=2 PipeCh=8
AO_PIPE[B] G6 RD: bps=16 float=0 ch=2 align=4 SameRate=1 copyCh=2 PipeCh=8
```
Four lines. No more.

**Rate mismatch (hypothesis: Phone Link opens at 16 kHz narrowband):**
```
AO_PIPE[A] G6 WR: bps=16 float=0 ch=2 align=4 SameRate=1 copyCh=2 PipeCh=8
AO_PIPE[A] G6 RD: bps=16 float=0 ch=1 align=2 SameRate=0 copyCh=1 PipeCh=8
```
The RD line shows `SameRate=0`, which the existing code translates into full silence
output → explains the call-goes-silent symptom directly.

**Float/int cross-pair (hypothesis: bit-depth mismatch corruption):**
```
AO_PIPE[A] G6 WR: bps=32 float=1 ch=2 align=8 SameRate=1 copyCh=2 PipeCh=8
AO_PIPE[A] G6 RD: bps=16 float=0 ch=2 align=4 SameRate=1 copyCh=2 PipeCh=8
```
Writer stores IEEE-754 bits via `FpNormFloat`, reader applies `FpDenorm16((INT16)(v>>3))`
to a float bit pattern → pure noise → explains the audible garble directly.

**Mid-call transition:**
```
AO_PIPE[A] G6 WR: bps=16 float=0 ch=2 align=4 SameRate=1 copyCh=2 PipeCh=8
(... call runs for 10s, no more lines ...)
AO_PIPE[A] G6 WR: bps=16 float=0 ch=1 align=2 SameRate=0 copyCh=1 PipeCh=8
```
The second line is the renegotiation moment. Timestamp (via DebugView wall clock)
identifies WHEN. This alone would be the fix proposal's direct evidence.

---

## § 7. What G6 does NOT do

- Does not log every batch invocation — on-change only.
- Does not acquire `PipeLock`.
- Does not touch `FRAME_PIPE` struct layout.
- Does not add new fields to the G2 Dbg* group.
- Does not modify any existing DbgPrint line (drop/silence prints stay as-is).
- Does not change IRQL requirements — both call sites run at DISPATCH like before.
- Does not interact with B1 prefill or Phase 5 pump transport.
- Does not survive across driver reloads (file-scope statics reset to zero, which is
  by design — first observation after reload always prints).
- Does not enable Phase 6.

---

## § 8. Build & install

```powershell
# Build
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" `
    VirtualAudioDriver.sln /p:Configuration=Release /p:Platform=x64

# Verify
.\build-verify.ps1 -Config Release

# Install (no reboot)
.\install.ps1 -Action upgrade
```

No reboot, no `-AutoReboot`, same path as G2/B1.

---

## § 9. Measurement protocol

1. Start DebugView with kernel capture to a new log:
   `results/g6_runtime/g6_pairing.log` (directory created as needed).
2. Run one real Phone Link call via
   [tests/live_call/run_test_call.py](../tests/live_call/run_test_call.py). User picks
   up the phone, listens, and subjectively labels audio as `clean / garbled / silent`.
3. Do NOT run a lab sine beforehand — lab sine would populate the snapshots at
   48/16/2 and hide any transition during the first moments of the real call.
   (Snapshot resets on driver reload, so reinstall is sufficient. Alternatively,
   load driver fresh and go straight to live_call.)
4. After the call, grep the log for `G6 WR` / `G6 RD` lines.

Parsing recipe:
- Count lines per (cable, direction): expected total is `≥ 1` per slot that was used.
- Collect unique `(bps, float, ch, align, SameRate, copyCh)` tuples per slot.
- Cross-check writer slot and reader slot on the same cable: if tuples differ in any
  field beyond channel counts, that is a pairing mismatch.
- Check for more than one unique tuple per slot: that is a mid-call transition.

---

## § 10. Decision table

| Signal | Interpretation | Next step |
|---|---|---|
| 4 lines total, all `bps=16 float=0 SameRate=1 copyCh=2 PipeCh=8`, both cables both directions | Format pair is not the mechanism; throughput was already clean after B1; garble origin is elsewhere | **G7** — read KS filter range (`minwavert.cpp`) to check whether the engine pre-upmixes client 2ch to 8ch before DMA, AND/OR deeper value-path tracing |
| Any line shows `SameRate=0` | Rate mismatch on that direction; existing code path drops (WR) or silences (RD) for that direction for the whole call | Fix proposal scope: **SRC-on-mismatch** (VB parity) — target only the affected direction first, reader side vs writer side |
| WR and RD on same cable show different `bps` or different `float` | Bit-depth / float cross-pair → raw-bit reinterpret on read → audible garble | Fix proposal scope: **format-pair reject with explicit log**, OR unified pipe format tag so `FpDenorm*` can detect mismatch. Choose after seeing which cross-pair is real |
| Same slot prints more than one unique tuple within one call | Mid-call renegotiation; current code does not re-align on renegotiation unless `SetState(RUN)` is re-entered | Fix proposal scope: hook format renegotiation path (portcls KS event), re-call `FramePipeRegisterFormat` on every format change, not only on RUN |
| `copyCh` ≠ `ch` on either side | Channel upmix/downmix is happening; may or may not be audible issue depending on the actual source channel semantics | Note for later; not by itself a fix trigger unless combined with one of the rows above |
| No G6 lines at all for a direction | That direction's hot path was never entered during the call — means Phone Link chose a different device, or the cable is not actually wired into the call path | Check `AUDIO_CABLE_PROFILE` / Communication Device settings before touching code; re-run |

---

## § 11. Scope guardrails

**In scope for G6:**
- One new file-scope comment-marked block in `Source/Utilities/loopback.cpp`:
  `g_G6PairingSnapshot`, `G6PackSignature`, `G6LogPairingIfChanged`.
- Exactly one 9-line insertion in `FramePipeWriteFromDma`.
- Exactly one 9-line insertion in `FramePipeReadToDma`.
- Changelog entry in `docs/PIPELINE_V2_CHANGELOG.md` marked `UNCOMMITTED`.
- Measurement run + results appended to this file as § 12.

**Out of scope for G6:**
- Any change to `loopback.h`.
- Any new field in `FRAME_PIPE`.
- Any change to `minwavert.cpp` / `minwavertstream.cpp`.
- Any change to `adapter.cpp`.
- Any IOCTL or control panel change.
- Any existing DbgPrint line modification (drop/silence logs stay as-is).
- Any restore of G2 / B1 / B1 prefill (those stay as-is in the working tree).

**Non-negotiables:**
- Commit prohibited until § 12 is filled and the decision table row picks the next gate.
- Phase 6 stays BLOCKED regardless of G6 outcome.
- Wording of observations stays at "strongly supported, structural" — no "confirmed".

---

## § 12. Measurement results

*To be filled after the measurement run.*

### § 12.1 Log excerpt (G6 lines only)

*(pending)*

### § 12.2 Unique tuples per slot

| Slot | Count | Tuples |
|---|---|---|
| Cable A WR | *(pending)* | *(pending)* |
| Cable A RD | *(pending)* | *(pending)* |
| Cable B WR | *(pending)* | *(pending)* |
| Cable B RD | *(pending)* | *(pending)* |

### § 12.3 Subjective call label

*(pending — clean / garbled / silent)*

### § 12.4 Decision

*(pending — apply § 10 table)*
