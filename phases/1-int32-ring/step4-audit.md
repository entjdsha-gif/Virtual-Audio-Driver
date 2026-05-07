# Phase 1 Step 4 — Hard-Reject Overflow Audit Report

Status: complete
Date: 2026-05-08
Branch: `phase/1-int32-ring`
Auditor: Claude (execution agent)
Review: pending Codex

## 1. Scope

Per [step4.md](step4.md), audit `Source/Utilities/loopback.cpp` and confirm
that no remaining code path silently overwrites cable ring data on
overflow. All paths that touch the canonical `FRAME_PIPE` fields
`pipe->Data` or `pipe->WritePos` must either:

1. Go through `AoRingWriteFromScratch` (which hard-rejects per ADR-005), or
2. Be deleted (legacy paths that contradict ADR-005).

Sources of truth:

- [docs/ADR.md § ADR-005](../../docs/ADR.md) — hard-reject ring overflow
  + hysteretic underrun.
- [docs/REVIEW_POLICY.md § 2](../../docs/REVIEW_POLICY.md) — forbidden
  drift list, including "Silently overwrites the ring on overflow (must
  be hard-reject + counter)".
- [docs/AO_CABLE_V1_DESIGN.md § 2.1, § 2.2, § 2.3](../../docs/AO_CABLE_V1_DESIGN.md) —
  `FRAME_PIPE` shape + canonical write API.

Out of scope: read-side paths (audited at Step 3); the legacy
`LOOPBACK_BUFFER` ring used by `LoopbackWrite` / `LoopbackWriteConverted`
(non-cable; see § 6 Residual Risks).

## 2. Methodology

Two complementary searches identify every site that could touch a cable
ring's write cursor or data:

```text
A. WritePos field accesses inside loopback.cpp:
   rg -n '\bWritePos\b' Source/Utilities/loopback.cpp

B. WritePos field accesses anywhere in Source/Main:
   rg -n '\bWritePos\b' Source/Main/

C. Direct member access to canonical pipe fields anywhere in Source:
   rg -in 'pipe->Data|->WritePos|pipe->writepos|Pipe->Write|Pipe->Data' Source/

D. References to cable pipe globals:
   rg -n '\bg_CableAPipe\b|\bg_CableBPipe\b' Source/

E. Symbols that historically wrote the cable ring:
   rg -n 'FramePipeWrite|FramePipeWriteFromDma|AoRingWrite|LoopbackWrite' Source/
```

Raw results were taken from the working tree at HEAD `c61ad9f`
(`phase1/step3: mark completed`).

### 2.1 Search A — `WritePos` in `loopback.cpp`

| Line | Identifier | Context |
|---|---|---|
| 501 | `pLoopback->WritePos` | `LoopbackBufferInit` reset (LOOPBACK_BUFFER, non-cable) |
| 509 | `pLoopback->MicSink.WritePos` | LOOPBACK_BUFFER MicSink reset (non-cable) |
| 594 | `pLoopback->WritePos` | `LoopbackWrite` read (non-cable) |
| 607 | `pLoopback->WritePos` | `LoopbackWrite` advance (non-cable) |
| 622 | `pLoopback->MicSink.WritePos` | `LoopbackWrite` MicSink (non-cable) |
| 635 | `pLoopback->MicSink.WritePos` | `LoopbackWrite` MicSink advance (non-cable) |
| 706 | `pLoopback->WritePos` | reset path (non-cable) |
| 736 | `pLoopback->MicSink.WritePos` | reset path (non-cable) |
| 757 | `pLoopback->MicSink.WritePos` | reset path (non-cable) |
| 812 | `pLoopback->MicSink.WritePos` | format change reset (non-cable) |
| 933 | `pLoopback->WritePos` | `LoopbackWriteConverted` read (non-cable) |
| 946 | `pLoopback->WritePos` | `LoopbackWriteConverted` advance (non-cable) |
| 964 | `pLoopback->MicSink.WritePos` | `LoopbackWriteConverted` MicSink (non-cable) |
| 977 | `pLoopback->MicSink.WritePos` | `LoopbackWriteConverted` MicSink advance (non-cable) |
| 1168 | `pLoopback->WritePos` | reset path (non-cable) |
| 1215 | `pLoopback->WritePos` | reset path (non-cable) |
| **1284** | `pipe->WritePos` | `AoRingAvailableFrames` — read-only fill calc |
| **1392** | `pipe->WritePos` | `FramePipeResetCable` — STOP-path reset to 0 |
| **1417** | `pipe->WritePos` | `AoRingAvailableSpaceFrames_Locked` — read-only |
| **1432** | `pipe->WritePos` | `AoRingAvailableFrames_Locked` — read-only |
| 1529 | comment | `AoRingWriteFromScratch` doc |
| 1533 | comment | `AoRingWriteFromScratch` doc |
| 1571 | comment | `AoRingWriteFromScratch` doc ("WritePos NOT advanced on full ring") |
| **1590** | `pipe->WritePos` | `AoRingWriteFromScratch` slot computation — read |
| **1598** | `pipe->WritePos++` | `AoRingWriteFromScratch` advance — **write** |
| **1599** | `pipe->WritePos` | `AoRingWriteFromScratch` wrap check — read |
| **1600** | `pipe->WritePos = 0` | `AoRingWriteFromScratch` wrap reset — write |

Bold rows are the canonical-pipe sites; non-bold rows are
`pLoopback->WritePos` (LOOPBACK_BUFFER, separate struct) and are not
in scope.

The **only** advance writes to `pipe->WritePos` are inside
`AoRingWriteFromScratch` (lines 1598-1600). The reset write at 1392 is a
STOP-path zero-out gated by `FramePipeResetCable` and is not a
write-overflow concern.

### 2.2 Search B — `WritePos` in `Source/Main/`

```
(no matches)
```

`Source/Main/minwavertstream.cpp`, `adapter.cpp`, etc. never reach into
`pPipe->WritePos` directly. Confirms the Step 0 boundary still holds:
external code only calls API helpers, never field-access.

### 2.3 Search C — `pipe->Data` / variants

Matches are confined to `Source/Utilities/loopback.cpp`:

- `pipe->Data[slotBase + ch] = sample` — write, inside
  `AoRingWriteFromScratch` (Step 2).
- `pipe->Data[slotBase + (LONG)ch]` — read, inside
  `AoRingReadToScratch` (Step 3).
- `pipe->Data` lifecycle — `FramePipeInitCable` (allocate),
  `FramePipeFree` (release).

No file outside `loopback.cpp` accesses `pipe->Data` or any other
`FRAME_PIPE` field directly; external code only calls API helpers.
Combined with Search A and Search B, this confirms the canonical write
cursor and ring storage have a single owner (`AoRingWriteFromScratch`)
and a single read consumer (`AoRingReadToScratch`).

### 2.4 Search D — Cable pipe globals

| Path | Lines | Purpose |
|---|---|---|
| `Source/Utilities/loopback.h` | 494, 495 | `extern` declarations |
| `Source/Utilities/loopback.cpp` | 1266, 1267 | definitions |
| `Source/Main/adapter.cpp` | 254, 257, 261, 262, 600, 606, 612, 618, 660, 663, 667, 668 | `FramePipeInit` / `FramePipeCleanup` only — lifecycle, no field access |
| `Source/Main/adapter.cpp` | 1738-1739 | comment describing FRAME_PIPE counter sourcing |
| `Source/Main/minwavertstream.cpp` | 107, 111, 1373, 1377, 1474, 1478, 1531-1540, 1665, 1669, 1888, 1893, 2135, 2137, 2280, 2282 | pointer assignment (`pFP = &g_CableAPipe`) only — passed to legacy `FramePipe*` shims |

No site outside `loopback.cpp` reads or writes `WritePos`, `ReadPos`, or
`Data` fields directly.

### 2.5 Search E — Historical write symbols

| Path | Line | Symbol | Disposition |
|---|---|---|---|
| `Source/Utilities/loopback.cpp` | 579 | `LoopbackWrite` | non-cable (LOOPBACK_BUFFER) |
| `Source/Utilities/loopback.cpp` | 854 | `LoopbackWriteConverted` | non-cable (LOOPBACK_BUFFER) |
| `Source/Utilities/loopback.cpp` | 866 | `LoopbackWriteConverted` → `LoopbackWrite` call | non-cable |
| `Source/Utilities/loopback.cpp` | 1519 | `AoRingWriteFromScratch` doc-banner | canonical write owner |
| `Source/Utilities/loopback.cpp` | 1540 | `AoRingWriteFromScratch` definition | canonical write owner |
| `Source/Utilities/loopback.cpp` | 1921 | `FramePipeWriteFrames` (no-op stub) | legacy shim (Step 1) |
| `Source/Utilities/loopback.cpp` | 1980 | `FramePipeWriteFromDma` (no-op stub) | legacy shim (Step 1) |
| `Source/Utilities/loopback.cpp` | 1991 | `FramePipeWriteFromDmaEx` (no-op stub) | legacy shim (Step 1) |
| `Source/Utilities/transport_engine.cpp` | 1171 | calls `FramePipeWriteFromDmaEx` | external caller, hits no-op stub |
| `Source/Main/minwavertstream.cpp` | 2297 | calls `FramePipeWriteFromDma` | external caller, hits no-op stub |

## 3. Cable-Ring Write Paths Inventory

The complete inventory of functions that participate in or carry the
name of a cable-ring write path.

| Function | Lines | Body summary | Disposition |
|---|---|---|---|
| `AoRingWriteFromScratch` | 1540-1605 | hard-reject + `OverflowCounter++` + advance | **KEEP — canonical** |
| `NormalizeToInt19` (inline static) | 1444-1502 | bit-depth dispatch (write helper) | **KEEP — canonical** |
| `AoRingAvailableSpaceFrames_Locked` (inline static) | 1413-1421 | writable space behind 2-frame guard | **KEEP — canonical** |
| `FramePipeInitCable` | 1300-1351 | allocate `Data`, init `WritePos`/`ReadPos` to 0 | **KEEP — init, not write-overflow** |
| `FramePipeFree` | 1358-1372 | release `Data` | **KEEP — destructor** |
| `FramePipeResetCable` | 1382-1402 | STOP path: zero `WritePos`/`ReadPos`/SRC state | **KEEP — reset, not write-overflow** |
| `FramePipeWriteFrames` | 1921-1930 | `UNREFERENCED_PARAMETER + return 0` | **LEGACY NO-OP** (Phase 6 cleanup target) |
| `FramePipeWriteFromDma` | 1980-1989 | `UNREFERENCED_PARAMETER + return 0` | **LEGACY NO-OP** (Phase 6 cleanup target) |
| `FramePipeWriteFromDmaEx` | 1991-2002 | `UNREFERENCED_PARAMETER + return 0` | **LEGACY NO-OP** (Phase 6 cleanup target) |
| `FramePipePrefillSilence` | 1943-1946 | `UNREFERENCED_PARAMETER` (no body) | LEGACY NO-OP (write-adjacent, scope-relevant) |
| `FramePipeRegisterFormat` / `FramePipeUnregisterFormat` | 1949-1977 | `UNREFERENCED_PARAMETER` only | LEGACY NO-OP (format-only, no write) |

### 3.1 Disposition rationale

- `AoRingWriteFromScratch` is the single owner of `pipe->Data` and
  `pipe->WritePos` writes. ADR-005 hard-reject is observably
  implemented (Step 2 forced-overflow regression — `ring_write_test.py`
  scenario [2] — passes with `STATUS_INSUFFICIENT_RESOURCES` +
  `OverflowCounter` increment + `WritePos` unchanged).
- `FramePipeResetCable` writes `pipe->WritePos = 0` only on STOP under
  the pipe spinlock. ADR-005 governs steady-state writes (frames
  arriving from the producer); STOP reset is not a write-overflow
  concern.
- `FramePipeInitCable` allocates the ring on PASSIVE_LEVEL during
  device init, before any producer can attach. Not a write-overflow
  concern.
- `FramePipeWriteFrames` / `FramePipeWriteFromDma` /
  `FramePipeWriteFromDmaEx` were the legacy producers. After Phase 1
  Step 1 they each return `0` without touching `pipe->Data` or
  `pipe->WritePos`. They CANNOT silently overwrite because they don't
  write at all. They are kept as link symbols only so external callers
  compile; Phase 6 cleanup removes them entirely after Phase 4-5
  caller migration.

## 4. External Caller Analysis

Two external sites still reference the legacy write API. Both currently
hit no-op stubs.

### 4.1 `Source/Main/minwavertstream.cpp:2297`

```c
FramePipeWriteFromDma(pPipe, m_pDmaBuffer + bufferOffset, runWrite);
```

- Caller: render-side write loop in the WaveRT capture/render path.
- Target: `FramePipeWriteFromDma` (loopback.cpp:1980) — returns 0,
  touches nothing.
- Audio impact: no audio reaches `pipe->Data` through this call site
  in Phase 1.
- Migration target: Phase 4 render coupling will replace this with
  `AoRingWriteFromScratch`. Until then, this site is harmless because
  it cannot silently overwrite — there's nothing to overwrite.

### 4.2 `Source/Utilities/transport_engine.cpp:1171`

```c
ULONG writtenFrames = FramePipeWriteFromDmaEx(...);
```

- Caller: transport-engine ring write helper (DMA wrap-aware).
- Target: `FramePipeWriteFromDmaEx` (loopback.cpp:1991) — returns 0.
- Audio impact: same as 4.1.
- Migration target: same as 4.1.

### 4.3 ADR-005 verdict for external callers

Neither caller violates ADR-005 because no overwrite is happening.
ADR-005 forbids silent overwrite **on overflow**; here there is no
write at all, only a no-op return. The audio cessation that this
implies is expected during Phase 1 (the ring-write rewrite phase) and
is documented as such in [step1.md](step1.md) acceptance:

> Existing transport callers ... compile, even if behavior incorrect.

Phase 4 will restore audio flow by migrating both call sites to
`AoRingWriteFromScratch`. That migration is the audible flip; it is
not in scope for Step 4.

## 5. Conclusion

**PASS.**

After exhaustive search of the source tree:

1. `AoRingWriteFromScratch` is the **only** function that writes
   `pipe->Data[...]` or advances `pipe->WritePos` in the steady-state
   write path.
2. `AoRingWriteFromScratch` implements ADR-005 hard-reject + counter
   correctly (verified at Step 2 + retained as Step 4 regression).
3. All other historically-named cable-ring write functions are Step 1
   no-op stubs that touch nothing and so cannot violate ADR-005.
4. External callers in `minwavertstream.cpp` / `transport_engine.cpp`
   currently target the no-op stubs; they will migrate to
   `AoRingWriteFromScratch` at Phase 4-5.

No code edits were necessary to satisfy Step 4 acceptance — the work
was already done at Phase 1 Step 1 when the legacy writers were
converted to no-op stubs and at Step 2 when the canonical writer
was introduced with hard-reject semantics.

## 6. Residual Risks

These items are NOT step4 BLOCKERs (out of scope: cable-ring path) but
are recorded for future reviewers because they intersect with
[REVIEW_POLICY.md § 2](../../docs/REVIEW_POLICY.md) forbidden-drift
items.

### 6.1 RR-1 — `LoopbackWrite` silent overwrite-oldest on `LOOPBACK_BUFFER`

`Source/Utilities/loopback.cpp:609-615`:

```c
pLoopback->DataCount += Count;
if (pLoopback->DataCount > bufSize)
{
    pLoopback->DataCount = bufSize;
    pLoopback->ReadPos = writePos;   // overwrite-oldest, no counter
}
```

This is a `LOOPBACK_BUFFER` (the legacy mic-side passthrough ring),
NOT the canonical `FRAME_PIPE` cable ring. ADR-005 governs cable rings.
However, REVIEW_POLICY § 2 names "Silently overwrites the ring on
overflow (must be hard-reject + counter)" as a forbidden drift in
absolute terms — `LOOPBACK_BUFFER` is technically a ring even if it is
not the cable ring.

Phase plan: Phase 5 capture coupling or Phase 6 cleanup retires
`LoopbackWrite` along with the `LOOPBACK_BUFFER` struct. Until then,
this site is non-cable, non-V1, and out of scope for Step 4.

### 6.2 RR-2 — `MicSink` dual-write inside `LoopbackWrite`

`Source/Utilities/loopback.cpp:617-637`:

```c
// Direct push to Mic DMA - eliminates async timing gap.
if (pLoopback->MicSink.Active && pLoopback->MicSink.DmaBuffer)
{
    ...
    pLoopback->MicSink.WritePos = micPos;
    pLoopback->MicSink.TotalBytesWritten += Count;
}
```

REVIEW_POLICY § 2 explicitly names "Re-introduces `MicSink` dual-write
(ring + DMA push)". This block dual-writes into both the
`LOOPBACK_BUFFER` ring and the mic DMA buffer. Same rationale as RR-1:
this is `LOOPBACK_BUFFER`-scoped, not cable-ring; out of scope for
Step 4.

Phase plan: same — Phase 5 capture coupling retires this. The
canonical capture path is `AoRingReadToScratch` from cable
`FRAME_PIPE` directly into the capture DMA buffer; no dual-write.

### 6.3 RR-3 — Legacy `FramePipeWrite*` shim symbols still present

`FramePipeWriteFrames`, `FramePipeWriteFromDma`, `FramePipeWriteFromDmaEx`,
`FramePipeReadToDma`, `FramePipePrefillSilence`,
`FramePipeRegisterFormat`, `FramePipeUnregisterFormat` exist as no-op
stubs.

Risk: a future contributor could re-fill these stubs with naive
implementations that bypass `AoRingWriteFromScratch`. This is exactly
the failure mode ADR-005 / REVIEW_POLICY § 2 are designed to prevent.

Mitigation:

- The current stub bodies (`UNREFERENCED_PARAMETER + return 0`) make
  any re-implementation an obvious diff that review will catch.
- Phase 6 cleanup deletes the entire shim layer after Phase 4-5
  audible flip, eliminating this risk surface.
- Until then, REVIEW_POLICY § 2 enforcement covers the case.

## 7. Acceptance Criteria Trace

step4.md acceptance:

| # | Criterion | Status | Evidence |
|---|---|---|---|
| 1 | Audit report committed to `phases/1-int32-ring/step4-audit.md` listing every cable-ring write path and its disposition | PASS | This document — § 3 inventory + § 3.1 disposition rationale. |
| 2 | No remaining cable-ring write path increments `WritePos` without incrementing `OverflowCounter` on overflow | PASS | § 2.1 search shows the only `WritePos++` site is inside `AoRingWriteFromScratch`, gated by the overflow branch (`OverflowCounter++` + `STATUS_INSUFFICIENT_RESOURCES` early return) immediately preceding the `WritePos++` advance. § 5 conclusion. |
| 3 | Build clean | PASS | `build-verify.ps1 -Config Release` → 17 PASS / 0 FAIL (run separately, see commit message / review thread). |
| 4 | Step 2 overflow scenario test still passes | PASS | `ring_write_test.py` scenario [2] forced overflow: `rc=STATUS_INSUFFICIENT_RESOURCES, OverflowCounter 0->1, WritePos unchanged` (regression run at Step 4 verification time). |
| 5 | No non-cable (mic-array / speaker / save-data / tone-generator) regression | PASS (manual) | Step 4 introduces zero source-code edits. The non-cable paths (`LoopbackWrite`/`LoopbackWriteConverted`/`SaveData`/`ToneGenerator`) are byte-identical to Step 3 HEAD. |

## 8. Review Verdict (auditor self-assessment)

`AoRingWriteFromScratch` is the sole canonical owner of the cable-ring
write cursor. ADR-005 is honored. Step 4 PASS, no MINOR, three
recorded RESIDUAL RISKs that belong to later phases.

Awaiting Codex review.
