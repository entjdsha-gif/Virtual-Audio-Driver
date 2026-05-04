# Phase 6 Exit: clean V1 codebase

## Exit Gate

- [ ] Steps 0..4 marked completed.
- [ ] `grep` audits pass:
      - no `AO_PUMP_FLAG` / `m_ulPump*` / `PumpToCurrentPosition` /
        `IOCTL_AO_SET_PUMP_FEATURE_FLAGS` in `Source/`.
      - no cable-side sinc / 2048-coefficient code.
      - no cable-side packed 24-bit math.
      - no `MicSink` symbols in the cable path.
      - no stale Phase 5/6 comments.
- [ ] Build clean.
- [ ] Live-call regression test (Phase 5 Step 2 procedure) still
      passes.
- [ ] No regression in non-cable streams.

## Outcome

Phase 6 closes the V1 codebase to a clean state matching
`docs/AO_CABLE_V1_ARCHITECTURE.md` and `docs/AO_CABLE_V1_DESIGN.md`.
The retired experimentation (Phase 5 pump, Step 3-4 timer-owned
transport, Y attempts) is gone from the source tree.

## Phase 6 → Phase 7 Handoff

Phase 7 is quality polish and product feature work: multi-channel
support, broader format acceptance, telephony category metadata
(`KSNODETYPE_MICROPHONE`), Control Panel polish, benchmark suite
extension, signing pipeline finalization, M6 shipping prep.

After Phase 7 reaches the M6 milestone, the user may decide to
merge `feature/ao-fixed-pipe-rewrite` into `main` per
`docs/GIT_POLICY.md` § 5.
