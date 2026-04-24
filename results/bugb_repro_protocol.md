# Bug B — Deterministic Repro Protocol (for bisect)

**Purpose:** Fix a single, repeatable test procedure so that per-commit
`AC[20ms]` measurements during Bug B bisect are comparable. Without this
every measurement is a moving target.

**Scope:** Used for bisect across main-branch commits in the range
`f547e4e` (lower anchor) .. `b856d94` (upper anchor, Bug B confirmed).

**Status:** Fixed 2026-04-14. Do not edit mid-bisect. If the protocol must
change, restart bisect from the upper anchor.

---

## 1. Pre-state (each run)

1. Close Chrome completely (no background tab residue).
2. Run `SoundVolumeView` → "reset all app volumes / routings to default"
   to clear per-app routing overrides from previous sessions.
3. Confirm Windows default playback device = **AO Cable A Input (speaker)**.
4. Confirm Windows default recording device is *not* AO Cable A Output
   (we capture via `loopback_rec.py`, not via default recording).
5. Confirm no other audio app is playing (Spotify, Discord, etc.).
6. DebugView running is optional for bisect — only needed if a commit
   result is inconclusive and mechanism analysis is required.

## 2. Player — same deterministic Chrome shared playback source

- **Application:** Google Chrome (stable channel, whichever is installed).
- **Mode:** WASAPI shared (Chrome default — do not force exclusive).
- **Source requirement:** the same deterministic audio source must be used
  for every probe in the bisect. Source selection priority:

  1. **Preferred — reuse prior AO/VB comparison source.** If the exact
     source used in the G9 §2.1 table (`ao_loopback_a*.wav` /
     `vb_loopback_a.wav`) can be identified and re-played, use it.
  2. **Fallback — local audio file played *inside* Chrome.** Drag a local
     `.wav` or `.mp3` into a Chrome tab (`file://` URL) so playback still
     traverses Chrome's WASAPI shared path. This removes network /
     advertising / recommendation-algorithm variability while preserving
     the Chrome shared-playback code path that Bug B is bound to.
  3. **Last resort — fixed YouTube URL.** Only if (1) and (2) are both
     blocked. Must be pinned with a timestamp past any ads, >= 30 s of
     continuous vocal/instrument content. Record the exact URL here before
     the first measurement.

- **Status for this bisect:** the G9 session did not record a specific
  YouTube URL, so path (1) is unavailable. Use path (2) local-in-Chrome
  fallback.
  - Local source file (FROZEN for this bisect):
    `d:\mywork\Virtual-Audio-Driver\results\g6_runtime\vb_loopback_a.wav`
  - Properties: 15 s, 48 kHz, 16-bit, 2ch — deterministic, sufficient
    length for AC[20ms] at start+2s window.
  - Rationale: reproducibility over network variability. Bug B is
    strongly suspected to be bound to the Chrome WASAPI shared playback
    path rather than YouTube branding specifically.

- **Whatever source is picked, it is frozen for the entire bisect.** If
  the source must change mid-bisect, restart from the upper anchor.

## 3. Capture

- Tool: `tests/loopback_rec.py`
- Source: AO Cable A Output mic (the loopback capture endpoint)
- Duration: **10 seconds** minimum, recording starts *before* Chrome
  playback begins
- Output: `results/bugb_runtime/<commit>/cap.wav`
- Sample format: whatever `loopback_rec.py` defaults to — do not change
  per-commit

## 4. Per-commit procedure

```
1. git worktree add ../ao-probe-<commit> <commit>
2. cd ../ao-probe-<commit>
3. build (Release) — same toolchain as main tree
4. install driver (upgrade path, no AutoReboot)
5. reboot if test-signing requires it (per memory feedback_test_signing_reboot)
6. Pre-state steps from § 1
7. Start loopback_rec.py capture (10 s)
8. Within 1 s, start Chrome YouTube URL playback
9. Wait for capture to finish
10. Save wav to results/bugb_runtime/<commit>/cap.wav
11. Compute AC[20ms] at start+2s (see § 5)
12. Record result in results/bugb_runtime/bisect_log.md
13. Uninstall driver or leave installed (next commit will overwrite)
14. Return to main working tree (do not commit anything in probe worktree)
```

## 5. Metric computation

Reuse the G9 §2 method exactly:

- Find audio start (first sample with abs > small threshold)
- Take a **500 ms window starting at (audio_start + 2000 ms)**
- Compute normalized autocorrelation at **20 ms lag** on that window
- Record the scalar value

A second probe window at (audio_start + 200 ms) is optional diagnostic,
not used for PASS/BAD judgment.

## 6. PASS / BAD thresholds (fixed)

Based on the G9 §2.1 table:

| AC[20ms] at start+2s | Classification |
|---|---|
| ≤ 0.20 | **CLEAN** (VB-reference level) |
| ≥ 0.50 | **BUG B PRESENT** |
| 0.20 – 0.50 | **GREY — re-measure** |

Grey zone handling:
- Re-run the full per-commit procedure a second time (new capture).
- If second run is CLEAN or BAD, use that classification.
- If still grey, mark the commit as **INCONCLUSIVE** in the bisect log
  and move to an adjacent commit (do not let a grey result decide a
  bisect branch).

## 7. Bisect decision tree

Anchors:
- Lower: `f547e4e` (Cable A/B loopback endpoints introduced — earliest
  commit on which this test protocol is applicable; *not* assumed clean)
- Upper: `b856d94` (Bug B confirmed in G9)

Probe order:

```
probe 1: 4d4ca17
  ├─ CLEAN → probe 2: 88016e8
  │            ├─ CLEAN → bisect between 88016e8 and b856d94
  │            └─ BAD   → bisect between 4d4ca17 and 88016e8
  └─ BAD   → probe 2: 245f009
               ├─ CLEAN → bisect between 245f009 and 4d4ca17
               └─ BAD   → probe 3: f547e4e
                           ├─ CLEAN → bisect between f547e4e and 245f009
                           └─ BAD   → **TERMINATE bisect**. Bug B is
                                       structurally bound to the Cable A/B
                                       endpoint introduction (f547e4e).
                                       Read f547e4e diff in full and treat
                                       it as the primary cause candidate.
```

## 8. Operating rules (엄수)

- **No commits** in any probe worktree. Ever.
- Feature worktree (`feature/ao-fixed-pipe-rewrite` with Option 2
  uncommitted edit) stays untouched during bisect.
- Option 2 Bug A fix is **not** carried into probe worktrees. We are
  measuring historical state, not fixed state. This means Bug A silence
  will reappear in probe wavs — that is expected and does not affect
  `AC[20ms]` Bug B metric (Bug B is about sample-value repetition, not
  silence).
- If Bug A silence contaminates AC measurement (e.g. measurement window
  lands inside a silence drop), shift the window by 500 ms and retry on
  the same wav. Document the shift in the bisect log.
- Phase 6 stays BLOCKED until bisect concludes and mechanism is
  understood.
- Wording: "Bug B introduction point pending bisect" until a single
  commit is identified. Not "confirmed regression" before that.

## 9. Bisect log format

File: `results/bugb_runtime/bisect_log.md`

Per entry:
```
### <commit short sha>
- Date: YYYY-MM-DD
- Build: <pass/fail + notes>
- Install: <pass/fail + notes>
- Capture: results/bugb_runtime/<commit>/cap.wav
- audio_start: X.XX s
- AC[20ms] at start+2s: 0.XXX
- Classification: CLEAN | BUG B | GREY | INCONCLUSIVE
- Notes: <anything unusual>
```
