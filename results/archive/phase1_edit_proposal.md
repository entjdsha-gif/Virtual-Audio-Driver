# Phase 1 — Edit Proposal (pre-implementation)

Date: 2026-04-13
Author: Claude
Status: **PROPOSAL — open items resolved, awaiting edit session**
Scope: Phase 1 of the `feature/ao-fixed-pipe-rewrite` fixed-pipe rewrite.

## Approval record (2026-04-13)

User reviewed the proposal and approved the five open items in §7 at their defaults. The proposal is now locked against further design changes and ready to be executed in a subsequent session. Reaffirmed guardrails:

- Phase 1 = diagnostics + rollout scaffolding only.
- No transport ownership change.
- No observable behavior change.
- Every new counter must read **zero** at idle immediately after install.

Decision log (full rationale in §7):

| Open item | Decision |
|---|---|
| (1) `AO_V2_DIAG` field naming | **compact** (`A_R_GatedSkipCount` style) |
| (2) IOCTL surface | **extend** existing `IOCTL_AO_GET_STREAM_STATUS` via output-buffer-length gating — no new IOCTL code |
| (3) `FeatureFlags` pipe slots | **land in Phase 1** as declared and zero-initialized; Phase 1 never reads them or branches on them |
| (4) per-session vs monotonic split | **as proposed** — `GatedSkip*` / `OverJump*` per-session; `FramesProcessedTotal`, `PumpInvocationCount`, `PumpShadowDivergenceCount`, `FeatureFlags` monotonic |
| (5) `test_stream_monitor.py` version policy | **V2-only** — replace speculative V2 parser; no V1/V2 dual mode |

This approval block exists so a future session (possibly with context amnesia) can treat §3 edit blocks as authoritative without reopening the naming / IOCTL / flag-slot debates.

Plan sources (operating-rules order):
- Codex baseline: `docs/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CODEX.md`, **Phase 1** (lines ~784–808)
- Claude execution notebook: `results/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CLAUDE.md`, **Phase 1** (lines ~1293–1542)
- Operating rules: `docs/VB_CABLE_DUAL_PLAN_OPERATING_RULES.md`
- Phase 0 closure: `results/phase0_findings.md` and commit `852ca16`

This document exists to:
- reload Codex's Phase 1 scope and exit criteria before any edits,
- translate Claude's per-file notes into concrete line-numbered edit targets against the **current** `feature/ao-fixed-pipe-rewrite` tip,
- surface every point where the plan and the current code drift, so that the drift is resolved in the proposal — not silently in the editor,
- make the idle-state expectations of each new field explicit so the Exit Criteria are measurable, and
- keep the diagnostic data contract (ioctl.h ⇄ adapter.cpp ⇄ test_stream_monitor.py) synchronized in one step, per `CLAUDE.md` Diagnostics Rule.

---

## 1. Scope and non-goals

### 1.1 Goal (from Codex Phase 1)
> "create the visibility and rollback controls needed for a safe rewrite"

### 1.2 In scope
- Add stream-level query-pump state + counters on `CMiniportWaveRTStream`.
- Add pipe-level render/capture counter pairs on `FRAME_PIPE`.
- Add a new `AO_V2_DIAG` IOCTL-returned struct that carries the Phase 1 counters.
- Extend `IOCTL_AO_GET_STREAM_STATUS` so the extended `AO_STREAM_STATUS + AO_V2_DIAG` payload is written when the output buffer is large enough. V1 clients that pass a `sizeof(AO_STREAM_STATUS)`-sized buffer continue to work unchanged.
- Update `adapter.cpp` IOCTL handler to populate the new fields from `g_CableAPipe` / `g_CableBPipe` (the `FRAME_PIPE` globals, **not** the legacy `g_CableA/BLoopback` LOOPBACK_BUFFER globals).
- Update `test_stream_monitor.py` so its V2 unpack matches the new struct shape the driver actually ships.
- Log the change in `docs/PIPELINE_V2_CHANGELOG.md` per `CLAUDE.md` Changelog Rule.

### 1.3 Out of scope (Phase 1 must NOT)
- Move any transport ownership (no cable render/capture rebinding).
- Change any behavior observable by audio clients.
- Introduce a query-driven pump helper (that is Phase 3).
- Start incrementing the new counters from any execution path. Phase 1 lands the fields at **zero** and leaves them at zero.
- Wire the `FeatureFlags` bits into any decision. Phase 1 declares the constants and stores zero; Phase 3 is the first phase to read them.
- Change the test signing cert, INF Target OS, Windows version coverage, or any install path. Those are settled and orthogonal.

### 1.4 Exit criteria (from Codex)
1. AO builds (`build-verify.ps1 -Config Release` green, driver .sys produced).
2. Diagnostics remain **sane and zero at idle** when the new path is not active.
3. No transport behavior change is observable.

### 1.5 Idle expectations (measurable form of 1.4 item 2)
- After a fresh `install.ps1 -Action upgrade` and no user audio activity, `python test_stream_monitor.py --once` must print every new Phase 1 counter as `0`.
- After a live Phone Link call that goes through AO, the Phase 1 counters **also** remain `0`. Phase 1 does not increment them; Phase 3 is the first caller.
- `python test_ioctl_diag.py` still reports `ALL PASSED` on both cables (Rate/Latency/Bits/Channels roundtrip intact).
- KDNET attach — optional — shows no new DPCs or spinlock activity from Phase 1 code paths. Phase 1 adds declarations only.

---

## 2. Plan ⇄ current code drift (reconcile before editing)

The Claude plan was written against an assumed repository state. The current tip of `feature/ao-fixed-pipe-rewrite` differs on three points that matter for Phase 1. None of them require replanning — they are line-number corrections — but they must be folded into the per-file edits below or the edits will land in the wrong places.

### 2.1 `minwavertstream.h` protected block boundary
- Plan said: "Add around line 108, in the protected: block".
- Reality: line 108 is `m_byteDisplacementCarryForward`, which is a member **inside** the protected block (lines 86–132). The natural append point is **after line 131 (`m_ulFadeInRemaining`) and before line 133 (`public:`)**.
- Action: §3.1 edits target line 131 → 132 as the insertion anchor, not "around line 108".

### 2.2 `loopback.h` FRAME_PIPE diagnostic anchor
- Plan said: "after `ActiveRenderCount`, around line 258".
- Reality: `ActiveRenderCount` is at **line 256**, and line 258 is the `Scratch Buffers` comment. The correct insertion anchor is **after line 256 and before line 258**.
- Action: §3.2 edits target line 257 as the insertion anchor.

### 2.3 `ioctl.h` has no existing diagnostic struct and `test_stream_monitor.py` already parses a **speculative** V2 layout
- Plan said: "Find the existing diagnostic struct, probably `AO_PIPE_STATUS` or similar".
- Reality: `ioctl.h` only defines `AO_ENDPOINT_STATUS`, `AO_STREAM_STATUS`, and `AO_CONFIG`. There is no `AO_PIPE_STATUS`, no `AO_V2_DIAG`, no `AO_STREAM_DIAG`.
- Meanwhile `test_stream_monitor.py` **already contains a V2 unpack block** (lines 86–107) that assumes a driver-side `AO_V2_DIAG` with fields like `Passthrough`, `RenderCount`, `PushLoss`, `PullLoss`, `ConvOF`, `ConvSF`, `PipeFrames`, `PipeFill`, `SpkWrite`, `MicRead`, `MaxDpcUs`, `PosJump`. These are **speculative** — they belong to a prior V2 diagnostic attempt that never landed on the driver side. The driver does not produce this struct today, and none of the Phase 1 counters (`GatedSkipCount`, `OverJumpCount`, `PumpInvocationCount`, `PumpShadowDivergenceCount`, `PumpFramesProcessedTotal`, `PumpFeatureFlags`) overlap with that speculative set.
- Action: Phase 1 introduces a **real** `AO_V2_DIAG` struct that carries the Phase 1 counter set described in the Claude plan. The speculative V2 unpack in `test_stream_monitor.py` is replaced, not extended. §3.3 defines the new struct and §3.6 rewrites the Python parser to match.
- Safety: V1 clients (anything calling `IOCTL_AO_GET_STREAM_STATUS` with a `sizeof(AO_STREAM_STATUS)` buffer) keep working because the new struct is **appended after** the existing `AO_STREAM_STATUS` in the same IOCTL response, gated by output buffer length.

---

## 3. Per-file edits

Each subsection below names the file, the insertion location on the current branch tip, what is added, why, and the data-contract sibling files touched in the same commit.

### 3.1 `Source/Main/minwavertstream.h`

**Insertion anchor:** end of the second `protected:` block, after `m_ulFadeInRemaining` (line 131), before `public:` (line 133).

**Add — stream-level pump state:**
```cpp
    // Phase 1: query-driven pump state (no behavior change; Phase 3 starts
    // writing these, Phase 5/6 start reading them). All fields init to zero
    // in Init() and stay zero through Phase 1 and Phase 2.
    ULONGLONG   m_ullPumpBaselineHns;               // elapsed-100ns baseline for pump math
    ULONG       m_ulPumpProcessedFrames;            // already-accounted frames in this run
    BOOLEAN     m_bPumpInitialized;                 // first-call-after-RUN latch
    ULONG       m_ulPumpLastBufferOffset;           // pump's own buffer-offset view (decoupled from m_ullLinearPosition)

    // Phase 1: stream-level diagnostic counters (copied into FRAME_PIPE
    // render/capture slots when the pump runs; Phase 3 first increment)
    ULONG       m_ulPumpInvocationCount;            // total pump calls (any outcome)
    ULONG       m_ulPumpGatedSkipCount;             // 8-frame gate fires
    ULONG       m_ulPumpOverJumpCount;              // DMA over-jump guard fires
    ULONG       m_ulPumpShadowDivergenceCount;      // Phase 3 shadow-window divergence hits
    ULONGLONG   m_ullPumpFramesProcessed;           // monotonic run-total of pump-driven frames

    // Phase 1: runtime feature flags (AO_PUMP_FLAG_* bits). Stored as ULONG,
    // never read during Phase 1. Phase 3 is the first reader.
    ULONG       m_ulPumpFeatureFlags;

    // Phase 1: Phase 3 shadow-window accumulators (declared now so Phase 3
    // can populate them without touching this header again).
    ULONGLONG   m_ullPumpShadowWindowPumpFrames;
    ULONGLONG   m_ullPumpShadowWindowLegacyBytes;
    ULONG       m_ulPumpShadowWindowCallCount;
    ULONG       m_ulLastUpdatePositionByteDisplacement;
```

**Add — flag constants (near the top of the file, after the `#include` block and before the class body):**
```cpp
//
// Phase 1: runtime feature flags for the query-driven pump helper.
//
// Default at CMiniportWaveRTStream::Init: 0 (all flags clear).
// Phase 3 entry:  ENABLE | SHADOW_ONLY on cable endpoints in SetState RUN.
// Phase 5 entry:  DISABLE_LEGACY_RENDER  set after render-side pump parity.
// Phase 6 entry:  DISABLE_LEGACY_CAPTURE set after capture-side pump parity.
// IOCTL rollback: clear DISABLE_LEGACY_* to fall back to UpdatePosition byte-per-ms transport.
//
#define AO_PUMP_FLAG_ENABLE                  0x00000001u
#define AO_PUMP_FLAG_SHADOW_ONLY             0x00000002u
#define AO_PUMP_FLAG_DISABLE_LEGACY_RENDER   0x00000004u
#define AO_PUMP_FLAG_DISABLE_LEGACY_CAPTURE  0x00000008u
```

**Why:**
- The stream fields are the authoritative source for pump accounting; the pipe-side counters are just a mirror for the IOCTL to read without walking stream instances.
- Declaring the flag constants now (Phase 1) lets Phase 3 land a one-liner `SetState` change that sets `ENABLE | SHADOW_ONLY` without touching this header again, which keeps phase boundaries tidy.
- Declaring the shadow-window accumulators now means Phase 3 only needs `.cpp` edits for the comparison logic — the struct layout is fixed from Phase 1 onward.

**Data-contract siblings touched in the same commit:**
- `minwavertstream.cpp` (§3.4) — initialize each new field to zero/`FALSE` in `Init`.

**Idle expected values (after install, before any audio activity):**
- Every new stream field = `0` / `FALSE`.

### 3.2 `Source/Utilities/loopback.h`

**Insertion anchor:** inside `FRAME_PIPE`, **after line 256 (`volatile ULONG ActiveRenderCount;`) and before line 258** (the `Scratch Buffers` comment block).

**Add — per-direction pipe counters:**
```cpp
    // ─── Phase 1: per-direction pump counter pairs (rev 2.4 split) ───
    // Speaker and Mic streams share one pipe. A single counter would race.
    // Render-side (populated by Speaker stream's pump helper in Phase 3+).
    volatile ULONG      RenderGatedSkipCount;
    volatile ULONG      RenderOverJumpCount;
    volatile ULONGLONG  RenderFramesProcessedTotal;
    volatile ULONG      RenderPumpInvocationCount;
    volatile ULONG      RenderPumpShadowDivergenceCount;

    // Capture-side (populated by Mic stream's pump helper in Phase 3+).
    volatile ULONG      CaptureGatedSkipCount;
    volatile ULONG      CaptureOverJumpCount;
    volatile ULONGLONG  CaptureFramesProcessedTotal;
    volatile ULONG      CapturePumpInvocationCount;
    volatile ULONG      CapturePumpShadowDivergenceCount;

    // Per-direction feature flag snapshots. Written by the stream at
    // SetState and on IOCTL-driven flag flip; read by the IOCTL handler so
    // the status tool can observe the active pump configuration without
    // reaching into the stream instance. Phase 1 keeps both at zero.
    ULONG               RenderPumpFeatureFlags;
    ULONG               CapturePumpFeatureFlags;
```

**Why:**
- Per-direction split is the rev 2.4 fix for the Speaker-vs-Mic race the plan flagged explicitly. The `=`-assignment pattern (pump helper copies stream counter into the matching direction's slot) has no race because only one stream ever writes a given slot.
- The `volatile` qualifier on the counters matches the existing `DropCount`/`UnderrunCount`/`ActiveRenderCount` pattern on lines 254–256 and tells the compiler not to cache reads in the IOCTL handler's tight snapshot loop.
- `FeatureFlags` slots are non-volatile because they are read rarely and written through the stream's setter path; the flag flip is a one-shot event, not a hot-path update. Also keeping them non-volatile lets the IOCTL handler sample them atomically via a single ULONG read without a fence.

**Data-contract siblings touched in the same commit:**
- `loopback.cpp` (§3.5) — initialize new fields in `FramePipeInit`; decide per-session vs monotonic reset behavior in `FramePipeReset`.
- `adapter.cpp` (§3.4) — copy these fields into the new `AO_V2_DIAG` output.

**Idle expected values:**
- Every new pipe field = `0`.
- On per-session reset (`FramePipeReset`): `RenderGatedSkipCount`, `RenderOverJumpCount`, `CaptureGatedSkipCount`, `CaptureOverJumpCount` return to `0`.
- On per-session reset: `Render*FramesProcessedTotal`, `Render*PumpInvocationCount`, `Render*PumpShadowDivergenceCount`, `CaptureFramesProcessedTotal`, `CapturePumpInvocationCount`, `CapturePumpShadowDivergenceCount` **preserve** their value. The monotonic run-totals must survive a ring reset so Phase 3's shadow-window divergence ratio stays measurable across RUN→PAUSE→RUN cycles.
- Feature-flag snapshots are also preserved across reset (they reflect configured state, not runtime counters).

### 3.3 `Source/Main/ioctl.h`

**Insertion anchor:** after `AO_CONFIG` (line 57), before the registry-value-name `#define`s (line 59).

**Add — version-tagged diagnostic payload struct:**
```c
// Phase 1: AO_V2_DIAG — extended diagnostic payload returned by
// IOCTL_AO_GET_STREAM_STATUS after the AO_STREAM_STATUS block when the
// caller's output buffer is large enough.
//
// Layout contract:
//   IOCTL_AO_GET_STREAM_STATUS output =
//     [ AO_STREAM_STATUS ]                         // V1, always present
//     [ AO_V2_DIAG        ]                        // V2, present iff OutBufLen >= V1 + V2
//
// Callers:
//   - V1 clients pass sizeof(AO_STREAM_STATUS) and never see AO_V2_DIAG.
//   - V2 clients (test_stream_monitor.py) pass sizeof(AO_STREAM_STATUS) +
//     sizeof(AO_V2_DIAG) and unpack both.
//
// First field is StructSize so a V2 client can detect the exact layout it
// is talking to even if AO_V2_DIAG grows in later phases.
typedef struct _AO_V2_DIAG {
    ULONG   StructSize;            // sizeof(AO_V2_DIAG) as the driver was built

    // Per-cable-per-direction Phase 1 counters.
    // A = Cable A, B = Cable B. R = Render (Speaker), C = Capture (Mic).
    // Naming: <Cable><Direction><Field>.

    // Cable A Render
    ULONG     A_R_GatedSkipCount;
    ULONG     A_R_OverJumpCount;
    ULONG     A_R_FramesProcessedLow;
    ULONG     A_R_FramesProcessedHigh;
    ULONG     A_R_PumpInvocationCount;
    ULONG     A_R_PumpShadowDivergenceCount;
    ULONG     A_R_PumpFeatureFlags;

    // Cable A Capture
    ULONG     A_C_GatedSkipCount;
    ULONG     A_C_OverJumpCount;
    ULONG     A_C_FramesProcessedLow;
    ULONG     A_C_FramesProcessedHigh;
    ULONG     A_C_PumpInvocationCount;
    ULONG     A_C_PumpShadowDivergenceCount;
    ULONG     A_C_PumpFeatureFlags;

    // Cable B Render
    ULONG     B_R_GatedSkipCount;
    ULONG     B_R_OverJumpCount;
    ULONG     B_R_FramesProcessedLow;
    ULONG     B_R_FramesProcessedHigh;
    ULONG     B_R_PumpInvocationCount;
    ULONG     B_R_PumpShadowDivergenceCount;
    ULONG     B_R_PumpFeatureFlags;

    // Cable B Capture
    ULONG     B_C_GatedSkipCount;
    ULONG     B_C_OverJumpCount;
    ULONG     B_C_FramesProcessedLow;
    ULONG     B_C_FramesProcessedHigh;
    ULONG     B_C_PumpInvocationCount;
    ULONG     B_C_PumpShadowDivergenceCount;
    ULONG     B_C_PumpFeatureFlags;
} AO_V2_DIAG;

// Compile-time shape guard. Bump this C_ASSERT together with any future
// AO_V2_DIAG field addition. Current shape: ULONG StructSize + 4 blocks *
// 7 ULONGs each = 1 + 28 = 29 ULONGs = 116 bytes.
C_ASSERT(sizeof(AO_V2_DIAG) == 4 + 4 * 7 * sizeof(ULONG));
```

**Why:**
- A separate struct (not extending `AO_STREAM_STATUS`) preserves V1 binary compatibility. `test_ioctl_diag.py` still passes because it only cares about V1.
- The `StructSize` first field is the versioning handle the rev 2.4 plan (and the current `test_stream_monitor.py` stub) already expects. Every future phase that grows `AO_V2_DIAG` bumps the size and updates the C_ASSERT in the same commit.
- Per-direction split at the cable level (A_R / A_C / B_R / B_C) keeps the Speaker-vs-Mic race fix from §3.2 visible in the IOCTL shape itself.
- `FeatureFlags` is per-direction because Phase 5 flips render flags before Phase 6 flips capture flags; the tool needs to see them independently.

**Data-contract siblings touched in the same commit:**
- `adapter.cpp` (§3.4) — IOCTL handler writes this struct.
- `test_stream_monitor.py` (§3.6) — replaces the speculative V2 unpack with the matching `struct.unpack_from`.
- `minwavertstream.cpp` (§3.4) and `loopback.cpp` (§3.5) — provide the source fields.

**Idle expected values:**
- Every ULONG counter = `0`.
- `StructSize = sizeof(AO_V2_DIAG)` always (not a counter — this is a version tag).
- `A_R_PumpFeatureFlags` / `A_C_PumpFeatureFlags` / `B_R_PumpFeatureFlags` / `B_C_PumpFeatureFlags` = `0` in Phase 1. In Phase 3 these will show `ENABLE | SHADOW_ONLY = 0x3` for active cable endpoints.

### 3.4 `Source/Main/adapter.cpp`

**Insertion anchor:** the `IOCTL_AO_GET_STREAM_STATUS` handler, currently lines 1671–1706. The V1 block at 1682–1700 stays untouched. Extend the handler after line 1701 (after the `#endif` for Cable B V1 population) and before line 1703 (`bytesReturned = sizeof(AO_STREAM_STATUS);`).

**Add — V2 extension path, gated by output buffer length:**
```cpp
    // Phase 1: V2 diagnostic extension. V1 clients pass
    // sizeof(AO_STREAM_STATUS); they get only the V1 block and bytesReturned
    // stays at sizeof(AO_STREAM_STATUS). V2 clients pass
    // sizeof(AO_STREAM_STATUS) + sizeof(AO_V2_DIAG) and get both.
    ULONG v2Offset = sizeof(AO_STREAM_STATUS);
    if (irpSp->Parameters.DeviceIoControl.OutputBufferLength >=
        v2Offset + sizeof(AO_V2_DIAG))
    {
        AO_V2_DIAG* pDiag = (AO_V2_DIAG*)((BYTE*)pStatus + v2Offset);
        RtlZeroMemory(pDiag, sizeof(AO_V2_DIAG));
        pDiag->StructSize = sizeof(AO_V2_DIAG);

#if defined(CABLE_A) || !defined(CABLE_B)
        // Cable A Render snapshot
        pDiag->A_R_GatedSkipCount            = g_CableAPipe.RenderGatedSkipCount;
        pDiag->A_R_OverJumpCount             = g_CableAPipe.RenderOverJumpCount;
        pDiag->A_R_FramesProcessedLow        = (ULONG)(g_CableAPipe.RenderFramesProcessedTotal & 0xFFFFFFFF);
        pDiag->A_R_FramesProcessedHigh       = (ULONG)(g_CableAPipe.RenderFramesProcessedTotal >> 32);
        pDiag->A_R_PumpInvocationCount       = g_CableAPipe.RenderPumpInvocationCount;
        pDiag->A_R_PumpShadowDivergenceCount = g_CableAPipe.RenderPumpShadowDivergenceCount;
        pDiag->A_R_PumpFeatureFlags          = g_CableAPipe.RenderPumpFeatureFlags;

        // Cable A Capture snapshot
        pDiag->A_C_GatedSkipCount            = g_CableAPipe.CaptureGatedSkipCount;
        pDiag->A_C_OverJumpCount             = g_CableAPipe.CaptureOverJumpCount;
        pDiag->A_C_FramesProcessedLow        = (ULONG)(g_CableAPipe.CaptureFramesProcessedTotal & 0xFFFFFFFF);
        pDiag->A_C_FramesProcessedHigh       = (ULONG)(g_CableAPipe.CaptureFramesProcessedTotal >> 32);
        pDiag->A_C_PumpInvocationCount       = g_CableAPipe.CapturePumpInvocationCount;
        pDiag->A_C_PumpShadowDivergenceCount = g_CableAPipe.CapturePumpShadowDivergenceCount;
        pDiag->A_C_PumpFeatureFlags          = g_CableAPipe.CapturePumpFeatureFlags;
#endif

#if defined(CABLE_B) || !defined(CABLE_A)
        // Cable B Render snapshot
        pDiag->B_R_GatedSkipCount            = g_CableBPipe.RenderGatedSkipCount;
        pDiag->B_R_OverJumpCount             = g_CableBPipe.RenderOverJumpCount;
        pDiag->B_R_FramesProcessedLow        = (ULONG)(g_CableBPipe.RenderFramesProcessedTotal & 0xFFFFFFFF);
        pDiag->B_R_FramesProcessedHigh       = (ULONG)(g_CableBPipe.RenderFramesProcessedTotal >> 32);
        pDiag->B_R_PumpInvocationCount       = g_CableBPipe.RenderPumpInvocationCount;
        pDiag->B_R_PumpShadowDivergenceCount = g_CableBPipe.RenderPumpShadowDivergenceCount;
        pDiag->B_R_PumpFeatureFlags          = g_CableBPipe.RenderPumpFeatureFlags;

        // Cable B Capture snapshot
        pDiag->B_C_GatedSkipCount            = g_CableBPipe.CaptureGatedSkipCount;
        pDiag->B_C_OverJumpCount             = g_CableBPipe.CaptureOverJumpCount;
        pDiag->B_C_FramesProcessedLow        = (ULONG)(g_CableBPipe.CaptureFramesProcessedTotal & 0xFFFFFFFF);
        pDiag->B_C_FramesProcessedHigh       = (ULONG)(g_CableBPipe.CaptureFramesProcessedTotal >> 32);
        pDiag->B_C_PumpInvocationCount       = g_CableBPipe.CapturePumpInvocationCount;
        pDiag->B_C_PumpShadowDivergenceCount = g_CableBPipe.CapturePumpShadowDivergenceCount;
        pDiag->B_C_PumpFeatureFlags          = g_CableBPipe.CapturePumpFeatureFlags;
#endif

        bytesReturned = v2Offset + sizeof(AO_V2_DIAG);
    }
    else
    {
        bytesReturned = sizeof(AO_STREAM_STATUS);
    }
```

**Important:** delete the existing `bytesReturned = sizeof(AO_STREAM_STATUS);` on line 1703 because the new block above now sets `bytesReturned` in both the V1 and V2 paths explicitly.

**Why:**
- Single IOCTL with extended payload avoids allocating a new IOCTL code and minimizes tool surface. V1 callers keep working without code change.
- Per-direction split mirrors the per-direction pipe slots in §3.2 and the per-direction ioctl shape in §3.3 — one mental model for Speaker vs Mic race safety.
- `g_CableAPipe` / `g_CableBPipe` are the correct source, not the legacy `g_CableALoopback` / `g_CableBLoopback`. The V1 path still reads from `g_CableALoopback` for historical format fields — that is left untouched in Phase 1 and is cleaned up in a later phase.
- Placed inside the existing `#if defined(CABLE_A)` guards so the per-cable build variants compile cleanly.

**Optional (recommended) add — FRAME_PIPE size assertion block near lines 29–34:**
```cpp
C_ASSERT(sizeof(AO_V2_DIAG) == 116);
```
This mirrors the existing `LOOPBACK_BUFFER` size asserts and catches accidental `AO_V2_DIAG` layout changes at compile time. It is belt-and-suspenders on top of the assert already in `ioctl.h`.

**Data-contract siblings touched in the same commit:** all 7 other Phase 1 files.

**Idle expected values:**
- V1 client: same bytes as before Phase 1.
- V2 client: `AO_V2_DIAG` entirely zeroed except `StructSize == sizeof(AO_V2_DIAG)`.

### 3.5 `Source/Main/minwavertstream.cpp`

**Insertion anchor:** `CMiniportWaveRTStream::Init`, after `m_ulFadeInRemaining = 0;` (line 237) and before the platform-specific `#if !defined(CABLE_A) && !defined(CABLE_B)` block on line 239.

**Add — zero every new field declared in §3.1:**
```cpp
    // Phase 1: pump state zeros (matches new fields in minwavertstream.h).
    m_ullPumpBaselineHns                  = 0;
    m_ulPumpProcessedFrames               = 0;
    m_bPumpInitialized                    = FALSE;
    m_ulPumpLastBufferOffset              = 0;
    m_ulPumpInvocationCount               = 0;
    m_ulPumpGatedSkipCount                = 0;
    m_ulPumpOverJumpCount                 = 0;
    m_ulPumpShadowDivergenceCount         = 0;
    m_ullPumpFramesProcessed              = 0;
    m_ulPumpFeatureFlags                  = 0;
    m_ullPumpShadowWindowPumpFrames       = 0;
    m_ullPumpShadowWindowLegacyBytes      = 0;
    m_ulPumpShadowWindowCallCount         = 0;
    m_ulLastUpdatePositionByteDisplacement = 0;
```

**Why:** zeroing at Init guarantees the Exit-Criteria "idle = 0" invariant the Phase 1 gate measures. Phase 3 is the first phase that writes any non-zero value.

**Data-contract siblings:** `minwavertstream.h` §3.1.

**Idle expected values:** all fields `0` / `FALSE` immediately after Init returns.

### 3.6 `Source/Utilities/loopback.cpp`

**Insertion anchor 1 — `FramePipeInit` (lines 1256–1352), specifically after `pPipe->ActiveRenderCount = 0;` on line 1342:**
```cpp
    // Phase 1: per-direction pump counters — all zero at init.
    pPipe->RenderGatedSkipCount             = 0;
    pPipe->RenderOverJumpCount              = 0;
    pPipe->RenderFramesProcessedTotal       = 0;
    pPipe->RenderPumpInvocationCount        = 0;
    pPipe->RenderPumpShadowDivergenceCount  = 0;
    pPipe->RenderPumpFeatureFlags           = 0;

    pPipe->CaptureGatedSkipCount            = 0;
    pPipe->CaptureOverJumpCount             = 0;
    pPipe->CaptureFramesProcessedTotal      = 0;
    pPipe->CapturePumpInvocationCount       = 0;
    pPipe->CapturePumpShadowDivergenceCount = 0;
    pPipe->CapturePumpFeatureFlags          = 0;
```

**Insertion anchor 2 — `FramePipeReset` (lines 1558–1584), after `pPipe->UnderrunCount = 0;` on line 1573:**
```cpp
    // Phase 1: per-session pump counters reset on ring reset.
    // Matches VB FUN_1400039ac (per-session fields cleared) while
    // preserving monotonic run-totals so Phase 3 shadow divergence
    // ratio survives RUN -> PAUSE -> RUN cycles.
    pPipe->RenderGatedSkipCount  = 0;
    pPipe->RenderOverJumpCount   = 0;
    pPipe->CaptureGatedSkipCount = 0;
    pPipe->CaptureOverJumpCount  = 0;

    // Do NOT reset on session boundary (monotonic across Phase 3 windows):
    //   RenderFramesProcessedTotal,       CaptureFramesProcessedTotal
    //   RenderPumpInvocationCount,        CapturePumpInvocationCount
    //   RenderPumpShadowDivergenceCount,  CapturePumpShadowDivergenceCount
    //   RenderPumpFeatureFlags,           CapturePumpFeatureFlags
```

**Why — per-session vs monotonic split:**
- `GatedSkip*` / `OverJump*` are **per-session** diagnostics: they describe the current RUN's frame-level decisions. Resetting them at session boundary matches VB's reset pattern and gives the user a clean view when a stream starts.
- `FramesProcessedTotal`, `PumpInvocationCount`, `PumpShadowDivergenceCount` are **monotonic run-totals**. Phase 3's Exit Criterion is "shadow divergence ratio < threshold across many invocations." If these reset on RUN→PAUSE→RUN, the ratio becomes noisy and unmeasurable in a real-world call that transitions through PAUSE.
- `FeatureFlags` reflect configured state, not per-session runtime — they survive reset by design.

**Data-contract siblings:** `loopback.h` §3.2, `adapter.cpp` §3.4.

**Idle expected values:**
- After `FramePipeInit`: all new fields `0`.
- After `FramePipeReset`: `RenderGatedSkipCount`, `RenderOverJumpCount`, `CaptureGatedSkipCount`, `CaptureOverJumpCount` return to `0`. Other new fields retain their pre-reset value (still `0` in Phase 1 because nothing ever writes them).

### 3.7 `test_stream_monitor.py`

**Edits:**

1. **Remove** the speculative V2 unpack block (lines ~86–107) that references fields the driver never shipped (`Passthrough`, `PushLoss`, `PullLoss`, `ConvOF`, `ConvSF`, `PipeFrames`, `PipeFill`, `SpkWrite`, `MicRead`, `MaxDpcUs`, `PosJump`). These were placeholders for a different V2 design that Phase 1 supersedes.

2. **Replace** with the new `AO_V2_DIAG` parser, matching §3.3 byte-for-byte:

   ```python
   AO_V2_DIAG_SIZE = 4 + 4 * 7 * 4  # StructSize + 4 * 7 ULONGs = 116 bytes
   V2_FIELDS_PER_BLOCK = 7  # per cable per direction

   def parse_v2_diag(buf, offset):
       """Parse AO_V2_DIAG at the given byte offset. Returns dict or None."""
       if len(buf) < offset + 4:
           return None
       struct_size = struct.unpack_from('<I', buf, offset)[0]
       if struct_size != AO_V2_DIAG_SIZE:
           return None  # version mismatch, bail
       if len(buf) < offset + struct_size:
           return None
       cursor = offset + 4
       def read_block():
           nonlocal cursor
           fields = struct.unpack_from('<IIIIIII', buf, cursor)
           cursor += 7 * 4
           return {
               'GatedSkipCount':           fields[0],
               'OverJumpCount':            fields[1],
               'FramesProcessedLow':       fields[2],
               'FramesProcessedHigh':      fields[3],
               'PumpInvocationCount':      fields[4],
               'PumpShadowDivergenceCount':fields[5],
               'PumpFeatureFlags':         fields[6],
               'FramesProcessedTotal':     fields[2] | (fields[3] << 32),
           }
       return {
           'StructSize': struct_size,
           'CableA_Render':  read_block(),
           'CableA_Capture': read_block(),
           'CableB_Render':  read_block(),
           'CableB_Capture': read_block(),
       }
   ```

3. **Caller change:** request the extended buffer size when opening the device for status reads:

   ```python
   V1_STATUS_SIZE = 64  # sizeof(AO_STREAM_STATUS)
   V2_BUF_SIZE    = V1_STATUS_SIZE + AO_V2_DIAG_SIZE
   ```

   And pass `V2_BUF_SIZE` (instead of `V1_STATUS_SIZE`) as the `nOutBufferSize` to `DeviceIoControl`. The driver's new `else` branch in §3.4 still writes only V1 bytes if we under-size, so this is a cooperative opt-in, not a break for other tools.

4. **Display:** print one line per cable per direction with:
   - `GatedSkipCount` / `OverJumpCount` — raw
   - `FramesProcessedTotal` — 64-bit recombined
   - `PumpInvocationCount`
   - `PumpShadowDivergenceCount` with the derived `divergence / max(1, invocations) * 100` percentage — this is the metric that will close Phase 3's exit criterion
   - `PumpFeatureFlags` as hex

**Why:**
- The existing speculative V2 parser targets a set of fields that will never ship. Keeping it would guarantee silent parse failures once Phase 1 lands.
- Updating the parser in the same commit as the driver-side struct satisfies the Diagnostics Rule.
- Exposing the divergence ratio is the **whole point** of Phase 1 infrastructure — it is the number Phase 3's exit criterion reads.

**Data-contract siblings:** `ioctl.h` §3.3, `adapter.cpp` §3.4.

**Idle expected values:**
- For each of the 4 blocks (CableA_Render, CableA_Capture, CableB_Render, CableB_Capture):
  - every counter `0`
  - `FramesProcessedTotal` `0`
  - `PumpFeatureFlags` `0x00000000`
  - derived divergence percentage `0.00%`

### 3.8 `docs/PIPELINE_V2_CHANGELOG.md`

**Edit:** prepend a new top-level entry above the existing `2026-04-13 — Driver INF target OS widening` block:

```markdown
## 2026-04-13 — Phase 1: Diagnostic counters and rollout scaffolding

**Files changed:**
- `Source/Main/minwavertstream.h` — +14 pump-state / counter fields on `CMiniportWaveRTStream`; +4 `AO_PUMP_FLAG_*` bit constants.
- `Source/Utilities/loopback.h` — +12 per-direction counter / flag fields on `FRAME_PIPE` (Render + Capture pairs).
- `Source/Main/ioctl.h` — +new `AO_V2_DIAG` struct returned via existing `IOCTL_AO_GET_STREAM_STATUS` when the caller's buffer is large enough; V1 callers unchanged.
- `Source/Main/adapter.cpp` — IOCTL handler extended to populate `AO_V2_DIAG` from `g_CableAPipe`/`g_CableBPipe`; `bytesReturned` now set explicitly in both V1 and V2 paths. Optional new `C_ASSERT(sizeof(AO_V2_DIAG) == 116)`.
- `Source/Main/minwavertstream.cpp` — `Init()` zeros every new field from the header.
- `Source/Utilities/loopback.cpp` — `FramePipeInit` zeros new fields; `FramePipeReset` clears per-session fields only and preserves monotonic run-totals.
- `test_stream_monitor.py` — speculative V2 parser replaced with a real `AO_V2_DIAG` parser matching the driver-side struct; opens the device with an extended output buffer; prints per-cable-per-direction counter rows and a shadow divergence ratio.

**What:** Phase 1 lands the diagnostic data contract and feature-flag skeleton that Phase 3 (pump helper), Phase 5 (render transport rebind), and Phase 6 (capture transport rebind) will read and write. No execution path in Phase 1 ever writes a non-zero value into these fields or reads a feature flag for a decision. Phase 1 is pure declaration plus zero initialization plus an IOCTL surface extension.

**Why:** Codex Phase 1 scope — "we should not start moving transport without first making it observable." Runtime visibility must land before the transport-ownership changes in Phase 3/5/6, or debugging those phases becomes blind.

**Verification:**
- `.\build-verify.ps1 -Config Release` green.
- `.\install.ps1 -Action upgrade` green on host; `test_ioctl_diag.py ALL PASSED` on both cables.
- `python test_stream_monitor.py --once` reports every new counter as `0` at idle.
- After a Phone Link AO call on the target, the new counters remain `0` (Phase 1 does not increment them — Phase 3 is the first writer).
- `feature/ao-fixed-pipe-rewrite` WinDbg bp from Phase 0 (commit 852ca16) can be re-attached via the existing KDNET target key without reconfiguring.

**Exit criteria (Codex):** AO builds, diagnostics sane and zero at idle, no transport behavior change — all met.
```

**Why:** `CLAUDE.md` Changelog Rule mandates an entry before or immediately after any code edit, with date, files, what, why. This block matches that shape.

---

## 4. Sequencing within the Phase 1 commit

All 8 files above must land in **one commit** for two reasons:
1. The Diagnostics Rule in `CLAUDE.md` requires `ioctl.h`, `adapter.cpp`, and `test_stream_monitor.py` to stay in lock-step.
2. Cutting a commit between adding a field and initializing it would leave a window where `Init()` does not zero the new memory — kernel memory contains whatever the Executor scratched there.

Recommended edit order during implementation (same commit at the end):
1. `ioctl.h` — define `AO_V2_DIAG` first. Gives every other file a stable shape to reference.
2. `loopback.h` — add new `FRAME_PIPE` fields.
3. `loopback.cpp` — zero new fields in `FramePipeInit`, set per-session reset policy in `FramePipeReset`.
4. `minwavertstream.h` — add new stream fields and flag constants.
5. `minwavertstream.cpp` — zero new stream fields in `Init()`.
6. `adapter.cpp` — add V2 extension path to `IOCTL_AO_GET_STREAM_STATUS`; optional `C_ASSERT(sizeof(AO_V2_DIAG) == 116)`.
7. `test_stream_monitor.py` — replace speculative V2 parser; bump requested buffer size; print new fields.
8. `docs/PIPELINE_V2_CHANGELOG.md` — changelog entry.

---

## 5. Build and verification plan

After all 8 files edited locally, before commit:

1. `.\build-verify.ps1 -Config Release`
   - Must pass 17/17 as before.
   - If Phase 1 field names collide with anything, `C_ASSERT(sizeof(AO_V2_DIAG) == 116)` or `FIELD_OFFSET` fails — resolve before running install.

2. `.\install.ps1 -Action upgrade`
   - Must reach `INSTALL_EXIT=0` with the no-reboot quiesce path (same as Phase 0).

3. `python test_ioctl_diag.py`
   - Both cables report `ALL PASSED`. Confirms V1 path is not broken by the new `bytesReturned` logic.

4. `python test_stream_monitor.py --once`
   - Prints every new counter as `0`. Prints `StructSize == 116`. Prints `divergence: 0.00%` for every block.
   - **This is the Phase 1 gate measurement.**

5. (Optional, nice-to-have) With the Phone Link AO target still available:
   - Re-attach WinDbg with `-k net:port=50000,key=...` (same key as Phase 0, still in target BCD).
   - Place a short Phone Link AO call.
   - Run `test_stream_monitor.py --once` on host right after the call ends.
   - Every new counter must **still** be `0`. Phase 1 is not supposed to have a path that increments them; observing non-zero here would mean a latent write or uninitialized memory.
   - This is the direct answer to Codex Exit Criterion #3 ("no transport behavior change is observable"): we confirm that adding the infrastructure did not inadvertently start counting anything.

---

## 6. Rollback

Single-commit phase with no runtime state changes. Rollback is a `git revert <phase1-commit>` — no device cleanup, no driver reinstall, no registry touch required beyond a normal `install.ps1 -Action upgrade` to land the reverted binaries.

If mid-edit, uncommitted: `git restore` on the 8 files.

If committed and uninstalled: land revert, rebuild, re-install.

No Phase 1 change touches the test-signing cert, INF Target OS, KDNET BCD settings, or target state.

---

## 7. Open items — RESOLVED

All five items were reviewed and approved at the proposal defaults (see Approval record near the top of this document). This section is kept for audit trail — do not reopen these questions in the edit session unless the user explicitly asks.

1. **`AO_V2_DIAG` field naming — DECIDED: compact.** Pattern is `<Cable>_<Direction>_<Field>`, e.g. `A_R_GatedSkipCount`, `B_C_PumpFeatureFlags`. The per-cable, per-direction prefix loses a bit of in-source readability but keeps `test_stream_monitor.py` output rows narrow enough to scan and avoids line-wrap in the IOCTL handler. The struct layout in §3.3 already uses this naming.

2. **IOCTL surface — DECIDED: extend existing.** `IOCTL_AO_GET_STREAM_STATUS` grows a length-gated V2 payload appended after `AO_STREAM_STATUS`. V1 callers (anything passing `sizeof(AO_STREAM_STATUS)` as the output buffer size) are unaffected. No new IOCTL code number is allocated in Phase 1. A future phase is free to split the diagnostic surface into a dedicated IOCTL if the extended payload grows large enough to make the combined handler awkward.

3. **`FeatureFlags` pipe slots — DECIDED: land in Phase 1.** `RenderPumpFeatureFlags` and `CapturePumpFeatureFlags` on `FRAME_PIPE` are declared, zero-initialized, exposed via `AO_V2_DIAG`, and **never read** during Phase 1. Phase 3 is the first phase that writes to them (at `SetState RUN` on cable endpoints) and the first that branches on their value. Landing the slots now means Phase 3 does not need another `FRAME_PIPE` ABI bump.

4. **Per-session vs monotonic counter policy — DECIDED: split as proposed.**
   - **Per-session** (reset to zero in `FramePipeReset`): `RenderGatedSkipCount`, `RenderOverJumpCount`, `CaptureGatedSkipCount`, `CaptureOverJumpCount`. These describe the current run's frame-gate decisions and should present a clean slate to the user on each RUN.
   - **Monotonic** (preserved across `FramePipeReset`): `RenderFramesProcessedTotal`, `CaptureFramesProcessedTotal`, `RenderPumpInvocationCount`, `CapturePumpInvocationCount`, `RenderPumpShadowDivergenceCount`, `CapturePumpShadowDivergenceCount`, `RenderPumpFeatureFlags`, `CapturePumpFeatureFlags`. These either feed Phase 3's shadow-window divergence ratio (needs to survive RUN→PAUSE→RUN to be meaningful) or reflect configured state rather than per-run counters.
   - `minwavertstream.cpp` `Init()` zeros everything stream-side — a fresh stream never inherits a previous instance's counters. The split only governs the pipe-side reset path.

5. **`test_stream_monitor.py` version policy — DECIDED: V2-only.** The current speculative V2 parser (Passthrough / PushLoss / PullLoss / ConvOF / ConvSF / PipeFrames / PipeFill / SpkWrite / MicRead / MaxDpcUs / PosJump) is **removed** and replaced with the parser in §3.7 matching the real `AO_V2_DIAG`. The monitor opens the device with `sizeof(AO_STREAM_STATUS) + sizeof(AO_V2_DIAG)` as the output buffer size. Comparative testing against pre-Phase-1 binaries is not in scope; if it is ever needed later, branch a copy of the monitor tagged with the target binary version.

---

## 8. What this proposal is NOT

- It is not a pre-approval to edit code. Per user directive, Phase 1 code editing starts only after this document is reviewed and the five open items in §7 are answered.
- It is not a replacement for the Codex plan. Codex still owns architecture and exit criteria. This document translates those into file-level actions against the current code state.
- It is not binding on Phase 3 design choices beyond the flag-bit constants and shadow-window field names declared here. Phase 3 may rename helpers, move logic into new files, or change the pump's internal math freely.

---

## 9. Next actions — next session

The proposal is approved. The next session starts directly at edit execution.

**Pre-edit checklist (first thing in the next session):**
1. Confirm `git status --short` is clean and `git log --oneline -1` shows `852ca16 Close Phase 0 gate` (or later, if the proposal itself has been committed).
2. Re-read §3 edit blocks. If any target file has been modified between approval and edit start, re-survey the drift (§2) before touching that file.
3. Re-read the Approval record at the top of this document to reload the five decisions.

**Edit → build → verify → commit sequence:**
1. Land all 8 files in the order specified in §4 (ioctl.h first, changelog last).
2. Run the verification steps in §5, starting with `build-verify.ps1 -Config Release`.
3. If verification passes, commit with message shaped like:
   > `Phase 1: diagnostic counters and rollout scaffolding (no behavior change)`
4. If any verification step fails, do not commit. Diagnose, fix, re-run §5 from step 1. Rollback per §6 is always available.

**Out of scope for the edit session:**
- No transport ownership changes.
- No Phase 2/3 peeking ("while I'm in minwavertstream.cpp, let me just also...").
- No test signing, INF, or install infrastructure edits.
- No new files beyond the 8 listed.

**When the edit session ends:**
- `results/phase0_findings.md` does not need to change.
- `results/phase1_edit_proposal.md` stays as the authoritative record; optionally add a footer section "10. Landed — see commit `<sha>`" after the commit lands.
- Phase 2 planning does not start until the Phase 1 commit is green on both host and target.
