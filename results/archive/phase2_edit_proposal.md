# Phase 2 — Edit Proposal (pre-implementation)

Date: 2026-04-13
Author: Claude
Status: **PROPOSAL — approved, ready to execute**
Scope: Phase 2 of the `feature/ao-fixed-pipe-rewrite` fixed-pipe rewrite.

## Approval record (2026-04-13)

User reviewed the proposal and approved the one open scope item in §5.1 at the recommended default.

Decision log:

| Open item | Decision |
|---|---|
| (5.1) G11 (8-bit) — defer vs implement | **Defer.** Phase 2 ships G9 + G10 only. 8-bit is a path-absent gap, not a wrong-value bug, so implementing it is a new code path outside the "low-risk format parity fix" framing Codex requires for Phase 2. A later dedicated phase will add `FpNorm8` / `FpDenorm8`, the 8-bit branches in `FramePipeWriteFromDma` / `FramePipeReadToDma`, and its own verification. |

Reaffirmed guardrails:
- **Parity-first principle**: no new functionality lands until every parity phase is closed. First make AO behave identically to VB-Cable on the paths AO already implements; only after all parity phases are closed may we consider additions / improvements / extensions. This principle governs Phase 2 and every phase until parity closure.
- Phase 2 = low-risk format parity fixes only (G9, G10).
- No transport ownership change.
- No state machine change.
- No pump helper introduction (that is Phase 3).
- Diagnostic contract from Phase 1 is untouched.
- `docs/VB_CABLE_AO_COMPARISON.md` must continue to mark the 8-bit rows as `❌ 미구현` / "deferred — parity-first" after Phase 2 lands.
- The Phase 2 changelog entry must explicitly record the 8-bit deferral reason as "deferred because parity-first, no new functionality before parity closure."

Plan sources (operating-rules order):
- Codex baseline: `docs/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CODEX.md`, **Phase 2** (lines ~810–834)
- Claude execution notebook: `results/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CLAUDE.md`, **Phase 2** (lines ~1546–1625)
- Evidence: `docs/VB_CABLE_AO_COMPARISON.md` §1-3 (Normalization) and §3-2 (Denormalization)
- Operating rules: `docs/VB_CABLE_DUAL_PLAN_OPERATING_RULES.md`
- Phase 1 closure: commit `06751aa`

This document:
- reloads Codex's Phase 2 scope and exit criteria before any edits,
- translates Claude's per-function notes into concrete line-numbered edit targets against the **current** `feature/ao-fixed-pipe-rewrite` tip (post-Phase-1),
- surfaces one significant plan-to-code drift (8-bit is completely absent, not "wrong fill value"),
- records the safety analysis for the 32-bit direct-copy change on the cable-only path,
- lists the one open decision that requires explicit user direction before editing.

---

## 1. Scope and non-goals

### 1.1 Goal (from Codex Phase 2)
> "apply independently-understood format behavior fixes before the architectural rewrite starts muddying root-cause analysis"

### 1.2 In scope (G9 + G10, format parity fixes)
- **G9** — 32-bit PCM direct copy. AO's `FpNorm32i: s >> 13` and `FpDenorm32i: v << 13` change to direct copy to match VB-Cable's observed behavior.
- **G10** — 32-bit float direct bit copy. AO's `FpNormFloat: FloatBitsToInt24(bits) >> 5` and `FpDenormFloat: Int24ToFloatBits(v << 5)` change to direct bit cast.
- Update of `docs/VB_CABLE_AO_COMPARISON.md` §1-3 and §3-2 tables so the "Match" column reflects post-Phase-2 state for 32-bit int and 32-bit float.
- `docs/PIPELINE_V2_CHANGELOG.md` entry per `CLAUDE.md` Changelog Rule.

### 1.3 Out of scope (Phase 2 must NOT)
- Move any transport ownership (no cable render/capture rebinding).
- Change any state-machine or pump-ownership behavior.
- Introduce the query-driven pump helper (Phase 3).
- Touch `FloatBitsToInt24` / `Int24ToFloatBits` themselves — these remain used by the LOOPBACK_BUFFER legacy path at `ReadSample` / `WriteSample` (`Source/Utilities/loopback.cpp` lines 175 and 196). The Phase 2 edit only updates the `FpNormFloat` / `FpDenormFloat` wrappers to bypass them.
- Implement 8-bit format support (see §2.1 below).
- Touch 16-bit or 24-bit conversion helpers — these already match VB-Cable per `docs/VB_CABLE_AO_COMPARISON.md` §1-3 / §3-2 ("동일").
- Change any diagnostic counter, IOCTL shape, or test tool. Phase 1's diagnostic contract is untouched.

### 1.4 Exit criteria (from Codex)
1. AO builds (`build-verify.ps1 -Config Release` 17 PASS / 0 FAIL, all three driver .sys files produced).
2. Format-focused tests stay green.
3. If a regression appears, it is clearly attributable to format logic rather than timing/state ownership changes.

### 1.5 Verification plan
1. `build-verify.ps1 -Config Release` — green.
2. `install.ps1 -Action upgrade` — `INSTALL_EXIT=0`.
3. `test_ioctl_diag.py` — `ALL PASSED` on both cables (V1 IOCTL path intact).
4. `test_stream_monitor.py --once` — Phase 1 counters stay at 0, `StructSize == 116`, no new divergence.
5. **Format-specific bit-exact** (where a harness exists):
   - `test_bit_exact.py` is available at the repo root as a fidelity / null-test harness (Q01 chirp fidelity, Q02 silence). Run it with AO Cable A input/output if a target or host setup allows it. Expected result: Phase 2 should make the 32-bit paths bit-exact rather than truncated. If no bit-exact regression is visible, that is also acceptable — Phase 2 is about restoring match, not proving bit-exactness.
6. **16-bit / 24-bit sanity** — implicit. Phase 2 does not touch these paths, but a smoke-level 16-bit playback via AO should continue to work. A regression in 16-bit would indicate accidental spillover into the 24-bit >>5 path and must be hunted down before commit.

### 1.6 Rollback
Single-file phase (`Source/Utilities/loopback.cpp`) plus the documentation touch-ups. Rollback is `git revert <phase2-commit>`, rebuild, reinstall. No registry, no cert, no INF, no IOCTL shape change.

---

## 2. Plan ⇄ current code drift (reconcile before editing)

### 2.1 G11 (8-bit) is not "wrong fill value" — 8-bit is completely absent

**Plan assumed:** 8-bit format is implemented in AO with the underrun fill value hard-coded to `0x00` instead of VB's `0x80` center.

**Reality (verified against `Source/Utilities/loopback.cpp`):**
- No `FpNorm8` or `FpDenorm8` helper functions exist.
- `FramePipeWriteFromDma` (the speaker-side DMA ingest path) has no 8-bit branch.
- `FramePipeReadToDma` (the mic-side DMA egress path) has only 16 / 24 / 32-int / 32-float branches at lines 1995–2032; no 8-bit.
- Comment block at lines 1643–1646 documents the shift strategy for 16 / 24 / 32 and does not mention 8-bit.
- `docs/VB_CABLE_AO_COMPARISON.md` §1-3 confirms this with the cell `8-bit | (byte - 0x80) * 0x800 (<<11) | 미구현 | ❌`. The `미구현` ("not implemented") marker is authoritative.

**Consequence:** The Claude plan's §G11 fix (`RtlFillMemory(dstPos, n, 0x80)` conditional on `MicBitsPerSample == 8`) is not applicable — there is no code path to patch; the feature does not exist.

**Recommendation:** Defer 8-bit support to a later, dedicated phase. Implementing 8-bit requires:
- A new `FpNorm8(BYTE b)` function producing INT32 (probably `((INT32)(b - 0x80)) << 11` per VB).
- A new `FpDenorm8(INT32 v)` function producing `BYTE` (probably `(BYTE)((v >> 11) + 0x80)`).
- New 8-bit branches in `FramePipeWriteFromDma` and `FramePipeReadToDma`.
- Format-registration path validation that 8-bit is advertised in the stream's `KSDATAFORMAT_WAVEFORMATEX` and accepted.
- New bit-exact test for 8-bit round-trip.

That is not a "low-risk format parity fix." Codex Phase 2 explicitly excludes new transport work. Implementing an absent format is closer to Phase 3/6 scope in spirit — it adds a new code path.

**Decision required:** confirm deferral of G11. Default: **defer, Phase 2 ships G9 + G10 only**. The Phase 2 changelog and findings will note the 8-bit gap explicitly.

### 2.2 Helper functions stay — only the wrappers change

The `FloatBitsToInt24` (lines 73–122) and `Int24ToFloatBits` (lines 124–167) helpers are used by **two** call sites in `Source/Utilities/loopback.cpp`:
1. `FpNormFloat` / `FpDenormFloat` — these are the Phase 2 edit targets.
2. `ReadSample` / `WriteSample` at lines 175 and 196 — part of the legacy `LOOPBACK_BUFFER` path.

After Phase 2 lands, the `FpNormFloat` / `FpDenormFloat` wrappers will no longer call these helpers, but the helpers remain used by `ReadSample` / `WriteSample`. **Do not delete `FloatBitsToInt24` / `Int24ToFloatBits`.** Deleting them would cause a missing-symbol build error in the legacy path. A later phase that removes the LOOPBACK_BUFFER code entirely can re-evaluate.

### 2.3 FRAME_PIPE semantic shift on 32-bit paths (cable-only safety)

**Before Phase 2:** 32-bit PCM is shifted right by 13 on the way into the pipe and left by 13 on the way out. 32-bit float is converted to ~19-bit INT24 domain (the "normalized" representation the pipe uses internally). This gives mixing headroom if heterogeneous streams were to ever share a pipe.

**After Phase 2:** 32-bit PCM and 32-bit float pipe samples carry the full application bit pattern unchanged. For a cable that the application uses as 32-bit, the INT32 in the pipe is the original INT32 sample (PCM) or the original float bit pattern reinterpreted as INT32 (float). No scaling.

**Why this is safe on the cable-only path:**
- Each cable is single-writer (exactly one speaker stream active at a time per cable) and single-reader (exactly one mic stream). No mixing happens.
- `SpeakerSameRate` / `MicSameRate` path selection ensures the producer and consumer agree on the sample-rate interpretation. Bit-depth agreement is enforced by the existing format registration path (`FramePipeRegisterFormat`), which sets `SpeakerBitsPerSample` / `MicBitsPerSample` and `SpeakerIsFloat` / `MicIsFloat` before transport starts.
- The speaker side writes samples in whatever format the app gave it; the mic side reads them and denormalizes with the matching helper. If Speaker is 32-bit PCM, Mic's 32-bit PCM branch in `FramePipeReadToDma` reads them via `FpDenorm32i` which after Phase 2 is also a direct copy. Round-trip is bit-exact.
- If Speaker is 32-bit float, Mic's 32-bit float branch uses `FpDenormFloat` which after Phase 2 is also a direct bit cast. Round-trip is bit-exact.
- The risk is **heterogeneous format between Speaker and Mic on the same cable** — e.g. Speaker writes 32-bit float, Mic reads as 32-bit PCM. In that case Mic would reinterpret the float's bit pattern as PCM integer. But this scenario already fails before Phase 2 (the `>>5` / `>>13` path also breaks on a float-to-PCM mismatch), and `SpeakerSameRate` / `MicSameRate` logic does not cover cross-type conversion. This is not a new hazard — Phase 2 preserves the existing assumption that Speaker and Mic formats match.
- For 16-bit and 24-bit, the normalized ~19-bit pipe representation is **unchanged** and is still the lossless width that matches VB-Cable.

**Phase 3/4/5/6 implications:** If a later phase introduces mixing of 32-bit streams on a single pipe, the direct-copy behavior must be re-examined. Phase 2 is a temporary semantic and will need a headroom strategy (e.g. shift-and-saturate on write, expand on read) before mixing lands. This is flagged in the changelog so the risk is not lost.

### 2.4 `docs/VB_CABLE_AO_COMPARISON.md` table needs updating

Both tables in §1-3 and §3-2 currently mark the 32-bit rows as `❌`. Phase 2 flips these to `✅` for the relevant rows. The 8-bit rows stay `❌ 미구현` because deferred.

This is part of the Phase 2 commit because (a) the comparison doc is the primary cited evidence for the change, and (b) leaving it stale creates a silent contradiction for any future session re-reading Phase 2 rationale.

---

## 3. Per-function edits

Single file: `Source/Utilities/loopback.cpp`. All edits cluster around lines 1673–1692.

### 3.1 `FpNorm32i` (lines 1673–1676)

**Current:**
```cpp
static __forceinline INT32 FpNorm32i(INT32 s)
{
    return s >> 13;
}
```

**Proposed:**
```cpp
// Phase 2 (G9): direct copy matches VB-Cable's observed 32-bit PCM
// behavior. The pipe carries the application's raw 32-bit samples.
// Safe on cable-only single-writer / single-reader path; see Phase 2
// proposal section 2.3 for the headroom caveat if mixing is ever added.
static __forceinline INT32 FpNorm32i(INT32 s)
{
    return s;
}
```

### 3.2 `FpDenorm32i` (lines 1678–1681)

**Current:**
```cpp
static __forceinline INT32 FpDenorm32i(INT32 v)
{
    return v << 13;
}
```

**Proposed:**
```cpp
// Phase 2 (G9 mirror): direct copy, symmetric with FpNorm32i.
static __forceinline INT32 FpDenorm32i(INT32 v)
{
    return v;
}
```

### 3.3 `FpNormFloat` (lines 1684–1687)

**Current:**
```cpp
// Float uses existing FloatBitsToInt24/Int24ToFloatBits → shift by 5
static __forceinline INT32 FpNormFloat(UINT32 bits)
{
    return FloatBitsToInt24(bits) >> 5;
}
```

**Proposed:**
```cpp
// Phase 2 (G10): direct bit cast matches VB-Cable's observed 32-bit
// float behavior. The pipe carries the raw IEEE-754 bit pattern
// reinterpreted as INT32. Consumer (FpDenormFloat) must cast it back.
// Safe on single-writer / single-reader paired cable; see Phase 2
// proposal section 2.3.
static __forceinline INT32 FpNormFloat(UINT32 bits)
{
    return (INT32)bits;
}
```

### 3.4 `FpDenormFloat` (lines 1689–1692)

**Current:**
```cpp
static __forceinline UINT32 FpDenormFloat(INT32 v)
{
    return Int24ToFloatBits(v << 5);
}
```

**Proposed:**
```cpp
// Phase 2 (G10 mirror): direct bit cast, symmetric with FpNormFloat.
static __forceinline UINT32 FpDenormFloat(INT32 v)
{
    return (UINT32)v;
}
```

### 3.5 (Optional) Comment block around lines 1643–1646

If the file has a header-style comment enumerating the normalization strategies per bit depth, update the 32-bit entries to say "direct copy (G9)" and "direct cast (G10)". This is a documentation-only touch-up, skipped if the comment does not exist in an easily-editable form.

### 3.6 `docs/VB_CABLE_AO_COMPARISON.md` table updates

In §1-3 table:
- `32-bit int` row, "AO" column: `FpNorm32i: s >> 13` → `FpNorm32i: direct copy (Phase 2 / G9)`.
- `32-bit int` row, "Match" column: `❌` → `✅`.
- `32-bit float` row, "AO" column: `FpNormFloat: FloatBitsToInt24() >> 5` → `FpNormFloat: direct bit cast (Phase 2 / G10)`.
- `32-bit float` row, "Match" column: `❌` → `✅`.

In §3-2 table:
- `32-bit int` row: same shape, mirror for `FpDenorm32i`.
- `32-bit float` row: same shape, mirror for `FpDenormFloat`.

8-bit rows stay unchanged (still `❌ 미구현`). The table header or a small note can be added mentioning that the 8-bit discrepancy is deferred to a later phase.

### 3.7 `docs/PIPELINE_V2_CHANGELOG.md` entry

Prepend above the existing top entry:

```markdown
## 2026-04-13 — Phase 2: Format parity fixes for 32-bit PCM and 32-bit float (G9, G10)

**Files changed:**
- `Source/Utilities/loopback.cpp` — `FpNorm32i`, `FpDenorm32i`, `FpNormFloat`, `FpDenormFloat` now do direct copy / direct bit cast, matching VB-Cable's observed behavior on 32-bit PCM and 32-bit float paths.
- `docs/VB_CABLE_AO_COMPARISON.md` — §1-3 and §3-2 tables updated to reflect the new post-fix state; 32-bit int and 32-bit float rows flip from ❌ to ✅.

**What:** The Phase 1 FRAME_PIPE kept AO's pre-rewrite normalization strategy where 32-bit int samples were right-shifted by 13 on the way in and 32-bit float samples went through `FloatBitsToInt24(...) >> 5`. Phase 2 replaces both with direct copy so the cable preserves the application's original 32-bit bit pattern bit-for-bit.

**Why:** Documented in `docs/VB_CABLE_AO_COMPARISON.md` §1-3 / §3-2. VB-Cable passes 32-bit PCM and 32-bit float through unchanged. AO's pre-Phase-2 behavior truncated 13 bits of INT32 dynamic range and converted float via a 24-bit intermediate, losing ~5 bits of float mantissa precision through the round-trip. The mismatch was identified as G9 (int) and G10 (float) in the Phase 2 target list.

**8-bit (G11) deferred — parity-first:** Inspection during Phase 2 planning found that 8-bit format is not implemented at all in AO — neither `FpNorm8` / `FpDenorm8` nor 8-bit branches exist in `FramePipeWriteFromDma` / `FramePipeReadToDma`. The Claude plan's G11 item was written assuming 8-bit was implemented-but-wrong, which is not the current state. Implementing 8-bit is therefore a new code path, not a parity correction. Deferred because parity-first, no new functionality before parity closure. `docs/VB_CABLE_AO_COMPARISON.md` continues to mark the 8-bit rows as `❌ 미구현`.

**32-bit headroom note:** Pipe samples on 32-bit paths now carry the application's raw bit pattern. This is safe on the cable-only single-writer / single-reader transport but eliminates the prior mixing headroom. If a future phase introduces mixing of 32-bit streams on a single pipe, the headroom strategy must be re-evaluated before or alongside that phase. 16-bit and 24-bit paths are unchanged and retain the normalized ~19-bit pipe representation that matches VB-Cable.

**Verification:**
- `build-verify.ps1 -Config Release` — 17 PASS / 0 FAIL.
- `install.ps1 -Action upgrade` — `INSTALL_EXIT=0`.
- `test_ioctl_diag.py` — `ALL PASSED` on both cables (V1 IOCTL path intact).
- `test_stream_monitor.py --once` — Phase 1 counters stay at 0, no new divergence.
- Smoke: play a 16-bit source through Cable A (basic sanity that the other paths are unaffected).

**Exit criteria (Codex Phase 2):** AO builds, format-focused tests stay green, no transport ownership change.

**Rollback:** `git revert <this commit>`, rebuild, reinstall.
```

---

## 4. Sequencing within the Phase 2 commit

Phase 2 is small — one source file and two documentation files. Single commit. Recommended edit order during implementation:

1. `Source/Utilities/loopback.cpp` — the four helper function bodies (§3.1–3.4). Verify after each pair of edits that the file still parses (no stray braces) before moving on.
2. `docs/VB_CABLE_AO_COMPARISON.md` — table updates (§3.6).
3. `docs/PIPELINE_V2_CHANGELOG.md` — entry (§3.7).
4. Build, verify, commit.

Edits are all in ASCII-safe ranges; no BOM concerns.

---

## 5. Open decisions — RESOLVED

One item was reviewed and decided. Full rationale above remains for audit; the decision is locked.

### 5.1 G11 (8-bit) — DECIDED: defer (parity-first).

Phase 2 ships G9 + G10 only. G11 is not a parity fix — 8-bit is path-absent in AO, so closing it requires new code paths (`FpNorm8` / `FpDenorm8` helpers, new 8-bit branches in `FramePipeWriteFromDma` / `FramePipeReadToDma`, format-registration validation, a new bit-exact test). That is feature addition, not parity. Under the parity-first principle, no new functionality lands until every parity phase is closed, so G11 is deferred unconditionally.

The Phase 2 changelog entry records the deferral reason verbatim as "deferred because parity-first, no new functionality before parity closure." `docs/VB_CABLE_AO_COMPARISON.md` §1-3 and §3-2 continue to mark the 8-bit rows as `❌ 미구현` after Phase 2 lands; only the 32-bit int and 32-bit float rows flip.

The 8-bit gap is tracked exclusively via the changelog entry and the unchanged `❌` marker in the comparison doc. No TODO is added to code, no placeholder is dropped, no scheduling commitment is made. If and when parity closure happens, the question of whether to implement 8-bit can be revisited then.

---

## 6. What this proposal is NOT

- It is not a pre-approval to edit code. Edit starts only after §5 is answered.
- It is not a replacement for the Codex plan. Codex still owns architecture; this doc only translates Phase 2 to file-level actions.
- It does not change any Phase 1 diagnostic contract.
- It does not touch the KDNET target, INF target OS, or install path.

---

## 7. Next actions

1. Answer §5.1 (defer or implement 8-bit).
2. Approve §3 edit blocks as written (or adjust).
3. Authorize edit session start; edit order is the four helpers in `loopback.cpp`, then the comparison doc, then the changelog, then build → verify → commit.
