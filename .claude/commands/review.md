# AO Cable V1 Review Command

Review the most recent step or change against AO Cable V1 rules.

## Read First

- `CLAUDE.md`
- `AGENTS.md`
- `docs/PRD.md`
- `docs/ADR.md`
- `docs/AO_CABLE_V1_ARCHITECTURE.md`
- `docs/AO_CABLE_V1_DESIGN.md`
- `docs/REVIEW_POLICY.md`

## Forbidden Drift Checklist

Reject any change that:

- Re-introduces packed 24-bit ring storage.
- Re-introduces the 4-stage `ConvertToInternal -> SrcConvert -> ConvertFromInternal -> LoopbackWrite` pipeline.
- Re-introduces sinc SRC with the 2048-coefficient table for cable streams.
- Re-introduces `MicSink` dual-write (ring + DMA push at the same time).
- Re-introduces FormatMatch enforcement that requires Speaker == Mic == Internal.
- Adds a second cable transport owner outside `AoCableAdvanceByQpc`.
- Allows query callbacks (`GetPosition`, `GetPositions`) to advance audio without going through the canonical helper.
- Allows the shared timer to advance audio independently of the canonical helper.
- Silently overwrites the ring on overflow (must be hard-reject + counter).
- Hides underrun, overflow, or DMA overrun-guard hits as success.
- Treats Phone Link end-to-end audio quality as proof of driver-internal correctness.
- Returns stale ring data to a new capture session after Stop/Start.
- Stores `ms` as runtime state in cable transport math (frames are authoritative).

## Required Validation

For non-trivial changes, the review must check:

1. Build/static validation (`build-verify.ps1 -Config Release`).
2. Forbidden-symbol absence (no `MicSink`, no sinc table re-introduced, etc.).
3. Design-value match against `docs/ADR.md` and `docs/AO_CABLE_V1_DESIGN.md`.
4. PortCls/WaveRT API sequence parity against installed WDK headers / Microsoft samples.
5. Create / register / unregister pairing for every PortCls / KS object the change touches.
6. Runtime observable proof for the phase goal (live loopback / Phone Link call / `test_stream_monitor.py` counters as appropriate).
7. Failure-path, ownership, lifetime, and unwind behavior — including `KeFlushQueuedDpcs` on Pause/Stop and ref-count discipline on `AO_STREAM_RT`.
8. INF / registry / interface state when applicable.
9. Diagnostics consistency: `Source/Main/ioctl.h`, `Source/Main/adapter.cpp`, and `test_stream_monitor.py` updated together when `AO_V2_DIAG` schema changes.

## Output Format

Report findings first, ordered by severity:

- `BLOCKER` — must be fixed before commit.
- `MINOR` — should be fixed but does not block commit if explicitly accepted.
- `RESIDUAL RISK` — known risk that survives this review and is recorded.

Use `file:line` references where possible.

If no findings, say so explicitly and list residual risks (do not pretend zero risk).

## What this command does NOT do

- Does not auto-fix findings.
- Does not commit.
- Does not mark a step `completed`. Only `/harness` after explicit user approval marks step state.
