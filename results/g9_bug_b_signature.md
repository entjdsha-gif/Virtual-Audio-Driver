# G9 — Bug B Signature (Stale 20ms Buffer Replay in First ~4 Seconds)

**Branch:** `feature/ao-fixed-pipe-rewrite` @ 2c733f1 + uncommitted Option 2 edit
**Date:** 2026-04-14
**Status:** Signature capture only. No proposal, no fix attempt yet.
**Phase 6:** BLOCKED.

## § 1. What Bug B is

During a `loopback_rec.py` recording of Chrome/YouTube played through AO Cable A
(Windows default playback → AO Cable A Input speaker → AO Cable A Output mic),
the **first ~3.35 seconds of audio are a repeating 20 ms buffer chunk**, and then at
approximately **4.0 seconds the stream abruptly transitions to clean, fresh audio**
for the remainder of the recording.

Perceptually: user describes the 0-4 s region as "트랜스포머 소리 — 연속적인 음성 위에
1-2 ms 드롭아웃이 빽빽하게 끼어드는 상태" / "트랜스포머 기계들끼리 대화하는 소리" — a
robotic chopped-voice quality. After 4 s it sounds normal.

This is **distinct from Bug A** (silence drops from ms-quantized per-tick byte
displacement math). Bug A was fixed by Option 2 (hns-precision byte displacement).
Bug B is not silence drops; the audio is non-zero throughout, just repeated.

## § 2. Measurement signature

### 2.1 Autocorrelation at 20 ms lag (all AO wav files vs VB reference)

All tests used Chrome/YouTube via Windows default playback, captured through
`loopback_rec.py`. Autocorrelation computed on a 500 ms window right after audio
starts (start + 200 ms) and again at (start + 2000 ms).

| File | Audio start | AC[20 ms] start+200 ms | AC[20 ms] start+2 s |
|---|---|---|---|
| `ao_loopback_a.wav` (pump + B1 + G2 + G6 uncommitted) | 2.057 s | **0.792** | **0.800** |
| `ao_loopback_a_legacy.wav` (Phase 5 legacy rollback) | 1.011 s | **0.753** | **0.796** |
| `ao_loopback_a_clean.wav` (Phase 5 CLOSED clean) | 1.430 s | 0.450 | **0.811** |
| `ao_loopback_a_baseline.wav` (b856d94 built from git worktree) | 1.982 s | **0.734** | **0.784** |
| `ao_loopback_a_optionA.wav` (Option 2 + legacy rollback) | 0.650 s | **0.784** | **0.771** |
| `vb_loopback_a.wav` (VB-Cable reference, same Chrome/YouTube path) | 0.030 s | **−0.180** | **0.059** |

A 20 ms AC peak of 0.75-0.81 corresponds to near-identical sample sequences
spaced 20 ms apart — i.e. real buffer replay, not music coincidence. VB's random
music autocorrelation at 20 ms is near zero (−0.18 / 0.06), ruling out
"20 ms is a natural musical period in this content" as an explanation.

### 2.2 Direct sample equality check

At `t = 1.00 s`, `1.02 s`, `1.04 s` in `ao_loopback_a_optionA.wav`, 20-ms chunks
compared sample-by-sample:

- 1.00 s vs 1.02 s: **max sample difference = 2** (int16 noise floor) → essentially
  identical
- 1.00 s vs 1.04 s: **max sample difference = 2** → essentially identical

For reference, the first 20 samples of each chunk:

```
1.00s  : [-1476, -1532, -1571, -1591, -1564, -1485, -1376, -1250, -1121, -1011, ...]
1.02s  : [-1476, -1533, -1572, -1591, -1564, -1485, -1375, -1251, -1121, -1010, ...]
1.04s  : [-1476, -1532, -1571, -1591, -1564, -1485, -1375, -1250, -1121, -1010, ...]
```

Differences are in the low unit digits — consistent with WASAPI float→int16
quantization noise, not real signal variation. **The same 20 ms audio chunk is
being emitted three times in a row.**

### 2.3 4.0 s transition

Exactly one silence run of 1.85 ms is observed at `t = 3997.5 ms` in Option 2,
Phase 5 clean, and baseline wav files. Immediately after this point the 20 ms
autocorrelation signature disappears and the audio becomes ordinary music.
The transition is **abrupt**, not gradual.

User confirmed subjectively: "4초부터 갑자기 좋아짐".

Nature of this 4 s marker is unknown. Candidates (none verified):

- WASAPI audio-engine glitch detection timeout and stream recovery
- Chrome / MediaFoundation buffer re-negotiation
- Driver-side state machine transition (WaveRT notification interval boundary,
  shadow pump window roll, DMA wrap, etc.)
- Client-side retry after seeing stuck position reports

## § 3. What Bug B is NOT

- **Not Bug A**: Bug A = silence drops from jittery per-tick byte count, fixed by
  Option 2 (14% → 0.20% body silence). Bug B persists through Option 2.
- **Not a silence detection miss**: the sample values in 0-4 s are non-zero and
  sound like real audio content (just the same 20 ms slice repeated).
- **Not format conversion**: VB uses the same `<< 3` / `>> 3` 16-bit normalization
  and does not exhibit Bug B.
- **Not timer drift correction alone**: VB has drift-corrected timer (decompile
  § 2.2), but removing drift correction does not explain why a specific 20 ms
  chunk would repeat.
- **Not pipe ring policy**: FramePipeReadFrames does partial-read + tail zero-fill,
  which would produce silence on underrun, not repeated real data.
- **Not leading silence**: the 650-2000 ms leading silence is a separate startup
  latency phenomenon present in all AO tests but not in VB (30 ms leading).

## § 4. Not yet confirmed

### 4.1 Introduction point

User's direct statement:

> 원래 이번에 구조변경하기전에는 이런게 없었어. 통화시에는 안좋았고 유투브
> 녹음할때는 처음부터 좋았었어. 그런데 vb처럼 바꾸려고 하면서부터 이렇게된거야.

Translation: before the current restructuring work, Bug B did not exist. Phone
Link calls were already bad (separate issue), but YouTube loopback recording was
clean from the start. Bug B appeared when the "VB-style rewrite" began.

However, today's measurement on `b856d94` (branch main stable baseline per
CLAUDE.md) **also shows** Bug B signature (`AC[20 ms] = 0.734 / 0.784`). This
contradicts the user's memory that pre-restructuring was clean.

**Therefore the introduction point of Bug B is not yet pinpointed.** Two
possibilities remain:

1. **`b856d94` is already within the restructuring range.** It is described as
   "stable baseline" in CLAUDE.md but it may be a main-branch merge commit
   containing some pre-V2 restructuring work. The truly clean pre-restructure
   commit is older.
2. **`b856d94` had Bug B even in its era, but the user's original test path
   differed** (e.g., WASAPI exclusive mode instead of shared, a different player,
   different default-device configuration) and did not exercise the code path
   that exposes Bug B.

**Framing rule:** user evidence strongly suggests Bug B was introduced after the
pre-V2 clean state and before or within the tested baseline range. "Confirmed
V2 regression" is **not** the right wording until bisect resolves this.

### 4.2 Mechanism

Hypotheses listed bluntly, none verified:

- **H1 (position stuck)**: driver's position reporting (`m_ullLinearPosition`,
  `m_ullWritePosition`, `linearPositionOfAvailablePacket`) is stuck or lagging
  for first ~4 s, causing WASAPI audio engine to re-deliver the same 20 ms
  buffer repeatedly until some timeout triggers recovery.
- **H2 (DMA wrap delay)**: the WaveRT DMA buffer is large enough that for first
  ~4 s the write pointer lags behind the read pointer's view, so the mic DPC
  reads stale data from a fixed region.
- **H3 (ring persisted state)**: FRAME_PIPE ring or LOOPBACK_BUFFER persists
  across session stop/run. Previous session's last 20 ms chunk is what plays
  until writer overwrites past that region. At ~4 s the writer catches up.
- **H4 (WaveRT packet reporting lag)**: `GetReadPacket` / `linearPositionOfAvailablePacket`
  computation (minwavertstream.cpp § 984 area) reports the same packet number
  for multiple ticks, causing audio engine to re-deliver.
- **H5 (MicSink direct-push stale state in baseline)**: baseline's MicSink
  direct-push path (if accidentally active) could be copying stale data.
  Does not explain V2 where MicSink is removed.

### 4.3 4 s transition origin

Why 4.0 s specifically? Not yet investigated. Candidates from § 2.3 are all
guesses.

## § 5. Operating state at session end

- **Option 2 edit** (hns-precision byte displacement in
  `Source/Main/minwavertstream.cpp` UpdatePosition) is **uncommitted** in working
  tree. This is the Bug A fix. Proven effective on Bug A, does not affect Bug B.
- **Phase 5 is in legacy rollback** (IOCTL-set via
  `tests/phase5_set_flag.py legacy`). Next driver reload or `SetState(RUN)`
  re-arms pump-owned default.
- Installed driver: Option 2 Phase 5 CLOSED build.
- B1 / G2 / G6 / INF changes: **all restored** (not in working tree).
- No commits created in this session.
- Phase 6: BLOCKED.

## § 6. Next session entry point

**First action**: fix a deterministic repro path. Do not rely on ad-hoc Chrome
tab state.

- Player: Chrome with a specific YouTube URL (record which one)
- Mode: WASAPI shared (Windows default playback routing)
- Cable path: AO Cable A Input = default playback; loopback_rec.py captures from
  AO Cable A Output mic
- Pre-state: `SoundVolumeView` "reset all app volumes to default" to avoid
  per-app routing residue (we hit this trap once in this session)

**Second action**: older-commit spot-check and bisect on Bug B.

1. Enumerate commits between the earliest main commit that CLAUDE.md documents
   as stable and `b856d94`. Plus any "V2 / FRAME_PIPE / pump / pipeline
   rewrite" introduction commits.
2. For each candidate commit, `git worktree add` → build → install → run the
   deterministic repro → compute `AC[20 ms]` on the captured wav.
3. Narrow to the commit where `AC[20 ms]` first crosses from ≤0.2 (VB-like) to
   ≥0.5 (Bug B present).
4. Read the diff of that commit in full. That diff contains the bug.

**Third action** (conditional on bisect success): work out mechanism from the
identified diff, then propose a read-only analysis gate (not a fix) to verify
mechanism before any edit.

## § 7. Wording discipline

- Bug B introduction point is **user-evidence-supported but unconfirmed**.
  "Introduced by V2 restructuring" is NOT to be written as confirmed. Acceptable
  phrasings: "strongly suggested to have been introduced after the pre-V2 clean
  state", "user reports a clean pre-V2 YouTube loopback path not reproduced in
  today's baseline test", "introduction point pending bisect".
- The 20 ms stale replay itself is "**strongly supported, structural**" — the
  autocorrelation and direct sample equality data cross a credibility threshold.
- The 4 s transition is "**observed, mechanism unknown**".

## § 8. Open questions carried forward

1. Where was `b856d94`'s "stable baseline" designation set, and does that match
   a truly pre-restructure state? Need git log + CLAUDE.md history.
2. Was the user's original YouTube loopback test using WASAPI shared or
   exclusive mode? Bug B may be mode-specific.
3. Does Bug B reproduce under `loopback_rec.py` if we use a non-Chrome player
   (Windows Media Player, ffplay with WASAPI shared, exclusive-mode test tone)?
4. Does Bug B reproduce on Cable B the same way, or is it Cable A-specific?
5. What is the exact WaveRT packet notification interval for the Cable A
   stream during these tests? (we saw 20 ms AC, which matches the Windows
   default shared period, but never verified it directly for this cable)
