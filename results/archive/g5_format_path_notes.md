# G5 — Format / Value Path Read-Only Analysis

**Branch:** `feature/ao-fixed-pipe-rewrite` @ 2c733f1 + uncommitted B1 + G2 instrumentation
**Date:** 2026-04-14
**Scope:** Read-only trace of the format/value-conversion path between client DMA and the
INT32 ring — `FpNorm*/FpDenorm*`, `FramePipeWriteFromDma`, `FramePipeReadToDma`,
channel mapping, `PipeChannels` origin. No code changes, no proposals. Evidence gathering
triggered by the G4 B1 outcome where driver-internal throughput was fixed (dUnderrun = 0,
trough ~44k frames) but real call audio remained subjectively garble/silent.

**G5 must answer:**
1. Is AO's 16-bit path semantically equivalent to VB-Cable's 16-bit path?
2. Are the 2→8 write path and 8→2 read path *"mathematically symmetric"* AND
   *"correct on the actual path"*?
3. Does the fixed 8-channel pipe create meaning mismatches on Phone Link 2ch routes?
4. Are the format registration values honored, or does some branch inject different
   assumptions?

---

## § 1. Confirmed (verified directly against source)

### 1.1 Pipe channels = 8, source-of-truth chain

- [adapter.cpp:530-531](../Source/Main/adapter.cpp#L530-L531):
  `ULONG savedChannels = LB_INTERNAL_CHANNELS; AoReadRegistryConfig(..., &savedChannels);`
- [adapter.cpp:1493](../Source/Main/adapter.cpp#L1493): `AoReadRegistryConfig` sets
  `*pMaxChannelCount = LB_INTERNAL_CHANNELS;` as the default. Registry override is read
  from the same routine but the default (and the current runtime value observed in the
  G2 log `internal=48000Hz/24bit/8ch`) is **8**.
- [adapter.cpp:591/597/603/609](../Source/Main/adapter.cpp#L591):
  `FramePipeInit(&g_Cable?Pipe, savedRate, savedChannels, targetFill)` — `savedChannels`
  goes straight into `pipeChannels`.
- [loopback.cpp:1326](../Source/Utilities/loopback.cpp#L1326): `pPipe->PipeChannels = pipeChannels;`
  pinned at init and never touched afterward.
- **FP_MAX_CHANNELS = 16** ([loopback.h:209](../Source/Utilities/loopback.h#L209)) — upper
  limit for the struct's scratch arrays.

Result: PipeChannels is a **global build/registry constant** (8 by default), not
negotiated from any individual stream's WAVEFORMATEX.

### 1.2 Normalization semantics

All helpers in [loopback.cpp:1660-1720](../Source/Utilities/loopback.cpp#L1660-L1720):

| Format | Normalize (client → pipe INT32) | Denormalize (pipe INT32 → client) |
|---|---|---|
| 16-bit PCM | `(INT32)s << 3` | `(INT16)(v >> 3)` |
| 24-bit PCM | `s24 >> 5` (where `s24` is sign-extended from 3 LE bytes) | `v << 5`, write 3 LE bytes |
| 32-bit PCM | identity (`return s`) | identity |
| 32-bit float | **raw bit reinterpret** `(INT32)bits` | **raw bit reinterpret** `(UINT32)v` |

Round-trip sanity:
- 16/16 round trip: `((s << 3) >> 3) == s` — **lossless** within INT16 range.
- 24/24 round trip: `((s24 >> 5) << 5) == (s24 & ~0x1F)` — **loses the 5 LSBs**, as
  documented inline: "24-bit loses lower 5 bits (same trade-off as VB-Cable)."
- 32-bit PCM round trip: identity — lossless, but any true 32-bit-range value is not
  rescaled into the ~19-bit normalized range that 16-bit values live in.
- Float round trip: identity via bit reinterpret — lossless only if **both sides use
  the float path**. No actual float → int normalization.

### 1.3 Write path (`FramePipeWriteFromDma`) — [loopback.cpp:1935-2025](../Source/Utilities/loopback.cpp#L1935-L2025)

Core branch structure:
1. **Rate mismatch = hard drop.** `if (!pPipe->SpeakerSameRate) { DropCount += frames;
   return 0; }` ([L1947-1958](../Source/Utilities/loopback.cpp#L1947-L1958)).
   The comment says "Fail-closed: rate mismatch requires SRC (Phase 3). Drop until then."
2. **Per-chunk scratch build:**
   - `copyChannels = min(spkChannels, pipeChannels)` ([L1971](../Source/Utilities/loopback.cpp#L1971)).
   - Zero all `pipeChannels` slots in the frame scratch ([L1988-1989](../Source/Utilities/loopback.cpp#L1988-L1989)).
   - Copy `copyChannels` samples from the client DMA into `dst[0..copyChannels-1]`,
     applying the format's `FpNorm*`.
   - Remaining `dst[copyChannels..pipeChannels-1]` stay zero.
3. **Bit-depth branch selected by `pPipe->SpeakerBitsPerSample` + `SpeakerIsFloat`.**
   Supports 16-bit PCM, 24-bit PCM, 32-bit float, 32-bit PCM. **No 8-bit branch.**
4. `FramePipeWriteFrames(pPipe, scratch, chunk)` writes the assembled INT32 frames into
   the ring. Whole-chunk or zero.

### 1.4 Read path (`FramePipeReadToDma`) — [loopback.cpp:2082-2184](../Source/Utilities/loopback.cpp#L2082-L2184)

Core branch structure:
1. **Rate mismatch OR mic inactive → full silence output, early return.**
   [L2088-2110](../Source/Utilities/loopback.cpp#L2088-L2110): `if (!pPipe->MicSameRate || !pPipe->MicActive) { RtlZeroMemory(dmaData, byteCount); return; }`
2. `copyChannels = min(pipeChannels, micChannels)` ([L2123](../Source/Utilities/loopback.cpp#L2123)).
3. `FramePipeReadFrames(pPipe, scratch, chunk)` fills `scratch[0..chunk*pipeChannels]`
   (or zero-fills on underrun).
4. Per-frame: copy `copyChannels` samples `scratch[ch]` → mic DMA via `FpDenorm*`,
   then zero-fill `micChannels - copyChannels` tail channels.
5. Bit-depth branch mirrors the write path: 16, 24, 32-float, 32-int. **No 8-bit branch.**

### 1.5 Round-trip sanity for the common 2ch/16-bit path

Lab condition (`g4_b1_sine.log` and stream monitor):
- Speaker register: `rate=48000 bps=16 ch=2 align=4 float=0 SameRate=1 PipeRate=48000 PipeCh=8`
- Mic register: same.

Trace a single frame through the pipe:
1. Writer receives `[L, R]` as two INT16 samples.
2. `copyChannels = min(2, 8) = 2`.
3. Scratch frame initialized to `[0, 0, 0, 0, 0, 0, 0, 0]`.
4. `dst[0] = FpNorm16(L) = (INT32)L << 3`, `dst[1] = (INT32)R << 3`.
5. Frame written to ring as `[L<<3, R<<3, 0, 0, 0, 0, 0, 0]`.
6. Reader pulls the same frame.
7. `copyChannels = min(8, 2) = 2`.
8. `d16[0] = FpDenorm16(L<<3) = (INT16)((L<<3) >> 3) = L`, `d16[1] = R`.
9. No tail channels to zero (copyChannels == micChannels).

**16-bit 2ch→8ch→2ch round trip is exact.** No loss, no mapping drift, no sign/shift bug.

### 1.6 No 8-bit PCM support

Neither write nor read branches handle `bps == 8`. If a client ever opens at 8-bit PCM,
it falls through all the `if/else if` branches and the scratch frame stays at zero
(writer) or the DMA bytes stay undisturbed from the zero-fill (reader). This is a known
gap but not the current garble case — lab log shows 16-bit only.

### 1.7 G4 B1 prefill interaction

B1 writes **zero-filled INT32 frames** ([loopback.cpp:1898-1912](../Source/Utilities/loopback.cpp#L1898-L1912))
directly into the ring before any real writer has run. A reader then pulls those zero
frames through `FpDenorm16(0) = 0` → output silence bytes. This is safe for any format
on the reader side (zero is silence in every format we support).

**But:** B1 zeroes `prefill` frames of `pipeChannels` INT32 slots each. If PipeChannels
changes between init and run, the prefill size is wrong. PipeChannels is static after
`FramePipeInit`, so this cannot happen in practice. Confirmed safe.

---

## § 2. Strongly supported (clear logical inference, not measured)

### 2.1 VB-Cable has SRC on both directions; AO does not

From [results/vbcable_pipeline_analysis.md](vbcable_pipeline_analysis.md):

> When rates differ, FUN_1400026a0 handles SRC + ring write atomically ...
> Uses linear interpolation using `local_c8[]` / `local_88[]` accumulators (16-channel max)

VB's write path and read path BOTH have in-line SRC branches that fire when
client rate ≠ ring rate. AO's equivalent branches are:
- Writer: `if (!SpeakerSameRate) DropCount += frames; return 0;`
- Reader: `if (!MicSameRate) RtlZeroMemory(...); return;`

Implication: **AO only works end-to-end when BOTH speaker and mic clients negotiate the
exact pipe rate**. Any client that negotiates a different rate produces a one-sided
silence / drop. Phone Link in particular is known (empirically and from the memory rule
about Phone Link being a "communications device") to pick narrowband / 16 kHz mono on
some codecs. Whenever it does, AO immediately degrades to silence or drop. VB-Cable does
not, because it SRCs inline.

This is the **single strongest explanation for AO being "garbled / silent" on real Phone
Link calls while clean on the 48k/16/2 lab sine**.

### 2.2 Float / int pipe value collision across sides is theoretically catastrophic

If one side registers as 32-bit float and the other as 16-bit PCM:
- Writer stores raw IEEE-754 bit patterns into the INT32 ring via `FpNormFloat`.
- Reader pulls those INT32 values and applies `FpDenorm16(v) = (INT16)(v >> 3)`.
- Result: the float bit pattern gets right-shifted 3 and truncated to INT16. This is
  **pure noise** — the sign, exponent, and high mantissa bits end up in the 16-bit
  output. Pops, clicks, white noise, saturation.

The same class of corruption happens for:
- Speaker 32-bit float → mic 16-bit PCM: noise.
- Speaker 16-bit PCM → mic 32-bit float: reader reinterprets `s<<3` as IEEE-754 bits →
  denormals and infinities at the float consumer.
- Speaker 32-bit PCM → mic 16-bit PCM: reader drops the top 13 bits, clipping hard.
- Speaker 16-bit PCM → mic 32-bit PCM: reader outputs values ≤ 2¹⁹ as 32-bit samples →
  extreme silence (~-85 dB) in the 32-bit native range.
- Speaker 24-bit PCM → mic 16-bit PCM: writer applies `>> 5`, reader applies `>> 3`.
  Net `>> 8` from the original 24-bit → 16 MSBs of the 24-bit sample, which happens to
  be the correct musical truncation. **This one is coincidentally correct.**
- Speaker 24-bit PCM → mic 32-bit float: reader reinterprets the 19-bit normalized INT
  as float bits → noise.

**The pipe has no bit-depth tag on the stored samples.** Correctness depends entirely on
writer and reader having picked matching or coincidentally-compatible format branches.
No code path verifies this compatibility. It is a silent data-contract assumption.

This is the **second strongest candidate** for garble: not in the simple lab condition
(both sides 16/16) but in any real scenario where Phone Link and the test harness
negotiate different wave-format SubFormats for speaker vs mic.

### 2.3 Channel mapping is symmetric only for "first-N" mapping

Both paths use `copyChannels = min(spkChannels-or-pipeChannels, the-other)` and always
copy starting at channel 0. They never look at `dwChannelMask` or KSAUDIO channel
position metadata.

Implication:
- 2ch source → 8ch pipe: `[L, R, 0, 0, 0, 0, 0, 0]`. Reader 2ch pulls `[L, R]`. OK.
- 6ch (5.1) source → 8ch pipe: `[L, R, C, LFE, Ls, Rs, 0, 0]`. Reader 2ch pulls `[L, R]`
  — **drops center, LFE, and surrounds**. User hears an incomplete stereo downmix
  instead of the intended front-L/R-with-everything. Not technically garble, but
  very thin / weird audio.
- 8ch source → 8ch pipe → 2ch mic: reader pulls `[ch0, ch1]` which is assumed to be
  front L/R by channel index. If the source used a different layout (e.g., WMA's
  non-standard `[L, R, C, LFE, Lb, Rb, Ls, Rs]`) the mic still sees `[L, R]` correctly
  for index 0/1, but any non-stereo source is silently collapsed.

This is **not a root cause by itself** for the Phone Link garble (Phone Link is 2ch or
1ch), but it is a structural weakness.

### 2.4 The "SameRate=1 lab" vs "real call" delta is the likely garble origin

Lab condition (G4 B1 log): `rate=48000 bps=16 ch=2 SameRate=1`. Clean throughput, clean
audio on Cable A/B render both in our sine and in the OpenAI TTS playback.

Real call condition: unknown exact format, but the subjective garble persisted even
though the driver-internal metrics (throughput, underrun) are clean. Given §2.1 and §2.2,
the most plausible subjective symptom "garble, not silence" maps to:
- A bit-depth or float mismatch across speaker and mic registrations, **or**
- A rate mismatch on one side producing silence while the other side still carries real
  audio.

We have not instrumented the real-call RegisterFormat events yet; that is the next
evidence gap (see § 4).

---

## § 3. Rejected (can be ruled out by the code read alone)

### 3.1 16-bit normalization bug

`FpNorm16(s) << FpDenorm16 = s` exactly. No bug. Any claim that the 16-bit conversion
loses precision or skews values is incorrect.

### 3.2 Channel mapping asymmetry for 2ch lab path

The 2→8→2 mapping round-trips exactly as demonstrated in § 1.5. Zero-fill of unused
pipe channels and first-N copy on both sides match. There is no sign flip, no channel
swap, no LR inversion in the code.

### 3.3 PipeChannels dynamically changing

`PipeChannels` is set once in `FramePipeInit` and never re-assigned. No runtime
re-configuration. Any theory depending on "PipeChannels grew/shrunk between write and
read" is false.

### 3.4 B1 prefill corrupting format semantics

B1 writes zero INT32 values, which are valid silence in every supported denormalization
path (`FpDenorm16(0) = 0`, `FpDenorm24(0) → 00 00 00`, `FpDenormFloat(0) = 0.0f`,
`FpDenorm32i(0) = 0`). B1 cannot be the source of garble.

### 3.5 Scratch-buffer aliasing between Speaker and Mic DPCs

Writer uses `pPipe->ScratchSpk`, reader uses `pPipe->ScratchMic`
([loopback.cpp:1969/2121](../Source/Utilities/loopback.cpp#L1969)). Different buffers.
No cross-DPC aliasing possible.

### 3.6 Ring capacity / ring wrap handling

The ring write/read routines handle wrap correctly at the frame level (verified against
[loopback.cpp:1467-1495](../Source/Utilities/loopback.cpp#L1467-L1495) and the read
mirror). G4 B1 measurement shows dDrop = 0 and fill drift is minor — no evidence of
wrap corruption in the steady state.

---

## § 4. Open questions (what G5 cannot answer alone)

1. **What WAVEFORMATEX does Phone Link actually negotiate on Cable A speaker and Cable B
   mic during a real call?** We have lab logs for 48k/16/2 but zero logs for a real
   call session. Without that we cannot tell whether the garble is rate mismatch
   (silence branch), bit-depth mismatch (garble branch), or something else.
2. **Does Phone Link switch format mid-call?** Some communication paths change sample
   rate on codec negotiation (NB → WB). If Phone Link transitions from 16k to 48k, the
   driver's `SameRate` flag would flip mid-stream. Unknown whether `FramePipeRegisterFormat`
   is re-called on format renegotiation or only on KSSTATE transitions.
3. **What is the current registry value of `InternalChannels`?** G4 B1 log shows 8 but
   this is configurable via `AoReadRegistryConfig`. If something wrote a non-default
   value, behavior changes. Read-only check on the registry would settle this but is
   outside the file scope.
4. **Does VB-Cable register 8-channel pipe internally, or match client channel count?**
   The note VBcable `internal=...` observations in the G4 log were for the AO driver's
   IOCTL; we did not capture equivalent state from VB. This matters for the question
   "is fixed 8ch the right default". The user explicitly said not to assume VB matches
   client format, so this stays open.
5. **Are there any Windows audio engine "zero-pad" frames inserted on cable streams
   with 8-channel KSDATARANGE when the client asks for 2?** If the audio engine itself
   silently upmixes client 2ch to 8ch before handing it to our DMA, then our writer's
   `copyChannels = min(2, 8) = 2` is wrong because `spkChannels` would actually be 8.
   Unknown without reading the KS filter code.
6. **Does the 32-bit float path's raw bit-cast survive any audio engine tandem
   conversion?** If the engine puts values outside the normalized 19-bit range into a
   float register, the bit pattern stored in the pipe is not in "pipe native" units and
   mixing between streams would explode. Phase 2 comment admits this caveat.
7. **Is there a format-renegotiation event (KSEVENT_TYPE_ENABLE or portcls format
   change) that we do not catch?** `FramePipeRegisterFormat` is only called from
   `SetState(KSSTATE_RUN)`. If a running stream changes its format without going through
   RUN, the pipe holds stale `SpeakerBitsPerSample` / `SpeakerIsFloat` and subsequent
   writes use the wrong `FpNorm*`. This would produce **intermittent garble** that
   matches the "not clean" real-call symptom.

---

## § 5. Answers to the G5 target questions

**Q1. Is AO's 16-bit path semantically equivalent to VB-Cable's 16-bit path?**
→ Yes for the normalize/denormalize shifts (both use `<< 3` / `>> 3`). **No** at the
whole-path level because AO lacks SRC and will drop/silence on rate mismatch whereas VB
SRCs inline.

**Q2. 2→8 write and 8→2 read — mathematically symmetric AND correct on the actual path?**
→ Mathematically symmetric (§ 1.5). Correct on the actual path *only when* speaker and
mic agree on bit depth and float/int and rate. The pipe stores no format tag, so any
cross-side mismatch is structurally undefined and produces garble (§ 2.2).

**Q3. Does fixed 8ch pipe create a meaning mismatch on Phone Link 2ch routes?**
→ For pure 2ch client on both sides, no — `[L, R, 0×6]` round trips exactly. For any
client with > 2 semantic channels (5.1 music, multi-mic capture) the unused tail channels
are silently dropped. For 2ch Phone Link specifically this is **not** the root cause.

**Q4. Does some branch inject different assumptions than the registered format?**
→ One real risk: the writer/reader choose their `FpNorm*`/`FpDenorm*` branch based on
`SpeakerBitsPerSample + SpeakerIsFloat` and `MicBitsPerSample + MicIsFloat` **separately**.
These two sets are populated by separate `FramePipeRegisterFormat` calls. **Nothing
enforces that they agree.** If speaker registers 16-bit PCM and mic registers 32-bit
float (or vice versa), the pipe silently corrupts every sample. The driver does not log
or reject this condition.

---

## § 6. Leading hypothesis after G5

> **Phone Link garble is structural and originates in format-path policy, not in the
> value conversion primitives themselves. The two plausible mechanisms are (a) rate
> mismatch on one direction causing silent-drop / silent-read, and (b) bit-depth or
> int/float mismatch between the Speaker and Mic registrations for the same cable pipe,
> causing raw-bit reinterpretation on read. AO's write/read primitives are individually
> correct for each supported format; the bug class is in the *pairing assumption*. —
> strongly supported, structural.**

Phrasing discipline:
- "strongly supported, structural" not "confirmed" — we have no real-call RegisterFormat
  capture yet.
- This frames the next instrumentation step: capture RegisterFormat / SameRate / bits /
  float on BOTH sides during a real Phone Link call, before any code fix.

---

## § 7. Next gate candidates (NOT a decision)

Listed for the next session to pick from. Phase 6 stays BLOCKED.

- **G6 (instrumentation-only)** — add a one-liner DbgPrint inside `FramePipeWriteFromDma`
  / `FramePipeReadToDma` dropping the resolved `bps / isFloat / channels / SameRate /
  copyChannels` at state transition, so a real Phone Link call produces a log of the
  exact format pair. Uncommitted, like G2/B1.
- **G7** — read KSFilter side (`minwavert.cpp`, range descriptor) to confirm whether
  the audio engine's 2ch client is presented to our DMA as 2ch or as 8ch pre-upmixed.
- **Render format fix branch (only after G6 confirms the mechanism)** — SRC-on-
  mismatch (VB parity) OR format-pair-reject-with-log. Not decided.
- **wider scope** — look at whether `InternalChannels` should track the first-registered
  speaker format rather than be a global registry constant. Requires G7 answer first.

**Phase 6 BLOCKED.** B1 / G2 / B1-prefill instrumentation stays uncommitted. No code
edits from G5.
