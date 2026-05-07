# Phase 1 Step 6.1 -- Diagnostics Counter Exposure (Code Evidence)

Status: complete (code-level)
Date: 2026-05-08
Branch: `phase/1-int32-ring`
Auditor: Claude (execution agent)
Review: pending Codex

This document records the **code-level** deliverable for Phase 1 Step 6.
The **runtime acceptance** (step6.md criteria #2-7, which require an
installed driver and live overflow/underrun scenarios) is split off as
Step 6.2 per pre-implementation plan agreement; this evidence is
limited to schema + C_ASSERT + build-clean + Python regression.

## 1. Scope

Per [step6.md](step6.md), expose Phase 1 ring counters and the
underrun-recovery flag through `IOCTL_AO_GET_STREAM_STATUS` so user-
mode can observe them: `OverflowCounter`, `UnderrunCounter`,
`UnderrunFlag` (drained boolean), current ring fill in frames, and
current `WrapBound`.

REVIEW_POLICY § 7 mandates that
[`Source/Main/ioctl.h`](../../Source/Main/ioctl.h),
[`Source/Main/adapter.cpp`](../../Source/Main/adapter.cpp), and
[`test_stream_monitor.py`](../../test_stream_monitor.py) are updated
together. This commit honors that contract.

## 2. Schema Change

`AO_V2_DIAG` grows from **132 -> 172 bytes** (+40, +10 ULONG-equivalents).

### 2.1 New fields (append-only)

```c
// Phase 1 Step 6: per-cable canonical FRAME_PIPE diagnostics.
ULONG  A_OverflowCount;
ULONG  A_UnderrunCount;
ULONG  A_RingFillFrames;
ULONG  A_WrapBoundFrames;
UCHAR  A_UnderrunFlag;          // 0 = normal, 1 = drained-recovery
UCHAR  A_Reserved0[3];          // align next ULONG

ULONG  B_OverflowCount;
ULONG  B_UnderrunCount;
ULONG  B_RingFillFrames;
ULONG  B_WrapBoundFrames;
UCHAR  B_UnderrunFlag;
UCHAR  B_Reserved1[3];
```

`UnderrunFlag` is `UCHAR` per step6.md ("UCHAR; 0 = normal, 1 =
drained-recovery"). Each cable block totals 20 bytes (4 ULONGs + 1
UCHAR + 3 pad). Per-cable total is a multiple of `sizeof(ULONG)`,
preserving append-only alignment for any future tail extension.

### 2.2 C_ASSERT update (twin-mirror)

Both copies of the shape guard updated to the new size:

- [`Source/Main/ioctl.h:191`](../../Source/Main/ioctl.h)
  ```c
  C_ASSERT(sizeof(AO_V2_DIAG) == (1 + 4 * 7 + 4 + 2 * 5) * sizeof(ULONG));
  ```
- [`Source/Main/adapter.cpp:44`](../../Source/Main/adapter.cpp)
  ```c
  C_ASSERT(sizeof(AO_V2_DIAG) == 172);
  ```

The mirror is the BLOCKER-level safeguard against schema drift between
ioctl.h and adapter.cpp (REVIEW_POLICY § 7 motivation): if either edit
moves without the other, the build fails.

### 2.3 Wire-format size constants

`AO_STREAM_STATUS` (V1 block) is **unchanged** at 64 bytes. Total
output buffer for V2 callers grows from 196 -> 236 bytes.

## 3. Handler Change (`adapter.cpp`)

### 3.1 New static helper `AoSnapshotFramePipeDiag`

Single contained file-local helper that takes `pipe->Lock` once per
cable and returns the consistent five-tuple
`(OverflowCount, UnderrunCount, UnderrunFlag, RingFillFrames,
WrapBoundFrames)`.

- Located at [`adapter.cpp:1591..1641`](../../Source/Main/adapter.cpp).
- `#pragma code_seg()` (nonpaged) so the spinlock-bracketed section
  never resides in a pageable page when executing at DISPATCH_LEVEL.
  Caller (`AoDeviceControlHandler`, `code_seg("PAGE")`) returns to
  PASSIVE before resuming pageable execution.
- Defensive guards: if `pipe == NULL || pipe->Data == NULL ||
  pipe->WrapBound <= 0`, returns all-zero outputs without acquiring
  the lock. This matches the pattern used by `AoRingAvailableFrames`
  (loopback.cpp:1275).

### 3.2 Step 4 audit invariant note

[Step 4 audit § 2.4](step4-audit.md) recorded that no file outside
`loopback.cpp` accessed FRAME_PIPE fields directly. Step 6 deliberately
softens that invariant by introducing one new file-local helper in
adapter.cpp that reads FRAME_PIPE fields under the canonical lock.
This is the minimum-blast-radius way to expose the diagnostics
five-tuple atomically; the alternative (an exported helper in
loopback.cpp) was considered and rejected per pre-implementation plan
discussion to keep the REVIEW_POLICY § 7 three-file sync rule intact.

The exception is contained: only `AoSnapshotFramePipeDiag` accesses
FRAME_PIPE fields outside loopback.cpp; no other adapter.cpp code does.

### 3.3 IOCTL handler fill block

Inside `case IOCTL_AO_GET_STREAM_STATUS` ([adapter.cpp:1761..1864](../../Source/Main/adapter.cpp)),
after the existing Phase 1 / Phase 5 zero-fill block (preserved
verbatim), the new fill calls `AoSnapshotFramePipeDiag` for the active
cable(s) per the same `CABLE_A` / `CABLE_B` / merged-build conditional
guards used by the V1 status block.

`UCHAR ufA` / `UCHAR ufB` are declared inside their respective
`#if defined(CABLE_A) ...` / `#if defined(CABLE_B) ...` blocks so a
single-cable build does not warn `C4189: 'ufB': local variable
initialized but not referenced` (warnings-as-errors active).

`A_Reserved0[3]` / `B_Reserved1[3]` are not assigned explicitly; they
are zeroed by the `RtlZeroMemory(pDiag, sizeof(AO_V2_DIAG))` call at
the top of the V2 fill block.

## 4. User-Mode Consumer (`test_stream_monitor.py`)

### 4.1 New layout constants

```python
V2_DIAG_SIZE_P1  = 4 + 4 * 7 * 4                 # 116 (Phase 1)
V2_DIAG_SIZE_P5  = V2_DIAG_SIZE_P1 + 4 * 4       # 132 (Phase 5)
V2_DIAG_SIZE_P6  = V2_DIAG_SIZE_P5 + 2 * 5 * 4   # 172 (Phase 6 = current)
V2_DIAG_SIZE     = V2_DIAG_SIZE_P6
RING_DIAG_BLOCK_BYTES = 5 * 4                    # per-cable Phase 6 ring diag
```

Backward compat: the parser accepts any of P1 / P5 / P6 by reading
`StructSize` self-describe and dispatching layout decoding from there.

### 4.2 New parsing

Per-cable Phase 6 ring diag block decoded via
`struct.unpack_from('<IIIIB3x', buf.raw, cursor)` -- four 32-bit
unsigned + one UCHAR + three pad bytes, total 20 bytes per cable.
Results are stored under `result['CableA_Ring']` /
`result['CableB_Ring']`.

### 4.3 New display

`print_status` adds a per-cable line when the Phase 6 ring block is
present:

```
    Ring   : Overflow=N Underrun=N Flag=0(ok) Fill=NNNN WrapBound=7168
```

`Flag` shows the raw UCHAR value plus a tag (`ok` / `RECOVER`) so the
recovery state is immediately legible alongside the counters.

## 5. Stale-Consumer Updates (`tests/phase3_*` / `tests/phase5_*`)

Three other Python scripts parse `AO_V2_DIAG` (REVIEW_POLICY § 7
names only `test_stream_monitor.py`, but these are in-tree consumers
that would silently break otherwise):

- [`tests/phase3_live_call_shadow.py`](../../tests/phase3_live_call_shadow.py)
- [`tests/phase3_shadow_active.py`](../../tests/phase3_shadow_active.py)
- [`tests/phase5_rollback.py`](../../tests/phase5_rollback.py)

Each gets a one-line minimal update: add `V2_DIAG_SIZE_P6 = 172` and
extend the accepted-`struct_size` set to include 172. Their internal
parsing of Phase 1/Phase 5 blocks is untouched -- offsets are unchanged
because the new fields land at the V2 tail.

## 6. Verification

### 6.1 Build clean (C_ASSERT shape gate)

```text
MSBuild Source\Main\CableA.vcxproj /p:Configuration=Release /p:Platform=x64
  -> 0 warning / 0 error C++ compilation
  -> aocablea.sys signed (98 KB)

MSBuild Source\Main\CableB.vcxproj /p:Configuration=Release /p:Platform=x64
  -> 0 warning / 0 error C++ compilation
  -> aocableb.sys signed (98 KB)

build-verify.ps1 -Config Release
  -> 17 PASS / 0 FAIL
```

The CableA / CableB rebuilds carried fresh timestamps relative to the
prior build (`03:53:47` vs `03:08:49`), confirming the C_ASSERT(172)
twin-mirror was actually exercised, not skipped by incremental build.

INF verification reports a separate `x86\InfVerif.dll` machine-WDK
issue that is orthogonal to Step 6 (same pre-existing failure path
recorded in Steps 3-5). `aocablea.sys` / `aocableb.sys` are produced
and signed.

### 6.2 Python user-mode harness regression

| Test | Result |
|---|---|
| `python -m py_compile test_stream_monitor.py` | OK |
| `python -m py_compile tests/phase3_live_call_shadow.py` | OK |
| `python -m py_compile tests/phase3_shadow_active.py` | OK |
| `python -m py_compile tests/phase5_rollback.py` | OK |
| `tests/phase1-runtime/ring_write_test.py` | 6 PASS / 0 FAIL |
| `tests/phase1-runtime/ring_read_test.py` | 8 PASS / 0 FAIL |
| `tests/phase1-runtime/ring_round_trip_test.py` | 5 PASS / 0 FAIL |
| `tests/phase1-runtime/underrun_hysteresis_test.py` | 3 PASS / 0 FAIL |

Phase 1 user-mode harness regression: **22 PASS / 0 FAIL**.

### 6.3 Encoding hygiene

After the C4819 build error caused by an em-dash (`—`, U+2014) inserted
by the initial edit, `Source/Main/ioctl.h` was rewritten ASCII-clean
(0 non-ASCII bytes) to match its existing no-BOM CP949-compatible
encoding. `Source/Main/adapter.cpp` keeps its pre-existing UTF-8 BOM
and pre-existing non-ASCII bytes (untouched by this commit).

### 6.4 Working tree

`git diff --check`: **clean** (no whitespace / conflict markers).

`git diff --name-only` (committable):
```
Source/Main/adapter.cpp
Source/Main/ioctl.h
test_stream_monitor.py
tests/phase3_live_call_shadow.py
tests/phase3_shadow_active.py
tests/phase5_rollback.py
```

Plus this evidence file (will be added separately) and the phase-index
mark commit. Excluded: `build-manifest.json` (build artifact),
`tests/phase1-runtime/` (untracked per GIT_POLICY § 9).

## 7. Acceptance Criteria Trace (step6.md)

| # | Criterion | 6.1 status | Evidence |
|---|---|---|---|
| 1 | Build clean | PASS | § 6.1 build output. |
| 2 | `test_stream_monitor.py` shows 0/0 for Overflow/Underrun in steady state | DEFERRED -> 6.2 | Requires installed driver. Code path produces zero counters when ring is healthy; verification is runtime. |
| 3 | `WrapBoundFrames == TargetLatencyFrames` | DEFERRED -> 6.2 | Requires installed driver + reconcile_wrapbound_to_target settle observation. |
| 4 | `RingFillFrames` in small live-latency band | DEFERRED -> 6.2 | Requires installed driver + active stream. |
| 5 | `UnderrunFlag == 0` in steady state | DEFERRED -> 6.2 | Requires installed driver. |
| 6 | Forced overflow scenario from Step 2 increments `<Cable>_OverflowCount` visible in monitor | DEFERRED -> 6.2 | Requires kernel-mode forced overflow setup against installed driver. |
| 7 | Forced underrun scenario from Step 5 increments `<Cable>_UnderrunCount` AND sets `<Cable>_UnderrunFlag` to 1 | DEFERRED -> 6.2 | Requires kernel-mode forced underrun setup against installed driver. Flag clear after refill is the strong invariant. |

Step 6.1 PASS gates only criterion #1. Criteria #2-7 are explicitly
the runtime acceptance gate that Step 6.2 will satisfy after the
driver is installed and a live monitor capture is recorded.

## 8. Residual Risks

### 8.1 RR-1 -- Step 4 audit invariant softened

Step 4 audit said no file outside `loopback.cpp` reads FRAME_PIPE
fields. Step 6 introduces one file-local helper in `adapter.cpp`
(`AoSnapshotFramePipeDiag`) that does. This is documented in § 3.2 and
contained to one helper. Phase 6 cleanup will retire the legacy
`A_R/A_C/B_R/B_C_*` zero-fill block in adapter.cpp; at that point we
can revisit whether to keep this helper file-local or hoist it into
`loopback.cpp` for cleaner ownership.

### 8.2 RR-2 -- Runtime acceptance not yet observed

step6.md acceptance #2-7 are runtime gates. Step 6.1 commit cannot
satisfy them without an installed driver. Driver install ceremony
(per `feedback_quiesce_issue` / `feedback_test_signing_reboot` memory
notes) is non-trivial. Step 6.2 will execute the install, run
`test_stream_monitor.py` at idle + during forced overflow + during
forced underrun, and capture the monitor output as runtime evidence.

### 8.3 RR-3 -- Stale phase3_* / phase5_* test scripts

Three V2 consumers updated minimally to accept the new size. They
were already not exercised in Phase 1 ring-rewrite scope; their
internal expectations of the legacy pump counters remain untouched.
Phase 6 cleanup will retire either the scripts or the legacy schema
fields they parse.

## 9. Files Touched (Step 6.1)

| Path | Tracked? | Change |
|---|---|---|
| `Source/Main/ioctl.h` | tracked | extend AO_V2_DIAG (+10 ULONG-equivs); update C_ASSERT |
| `Source/Main/adapter.cpp` | tracked | add `AoSnapshotFramePipeDiag` helper; new V2 fill block; update mirror C_ASSERT |
| `test_stream_monitor.py` | tracked | new size constants; parse + display per-cable Ring block |
| `tests/phase3_live_call_shadow.py` | tracked | accept 172 in struct_size set |
| `tests/phase3_shadow_active.py` | tracked | accept 172 in struct_size set |
| `tests/phase5_rollback.py` | tracked | accept 172 in struct_size set |
| `phases/1-int32-ring/step6-code-evidence.md` | tracked | **new** (this document) |
| `phases/1-int32-ring/index.json` | tracked | mark step 6 completed (separate commit, after 6.2 if user prefers) |

Source-code edits (.h + .cpp): 2 files. Schema-coupling files
(REVIEW_POLICY § 7 three): 3. Total: 6 + this document.

## 10. Auditor Self-Assessment

Schema extends append-only. Twin-mirror C_ASSERT compiles. Build
clean. User-mode parser updated symmetrically. Step 4 invariant
intentionally and minimally relaxed; documented. Step 6.1 PASS for
code; Step 6.2 runtime acceptance is the next gate.

Awaiting Codex review.
