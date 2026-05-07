# Phase 1 Exit: INT32 frame-indexed ring ready

## Exit Gate

All of the following are true:

- [ ] Step 0 (cross-TU access removal), Step 1 (struct/API shape),
      Step 2 (write same-rate), Step 3 (read same-rate), Step 4
      (overflow audit), Step 5 (underrun hysteresis), Step 6
      (diagnostics) all marked completed.
- [ ] Each step's commit has a passing review per
      `docs/REVIEW_POLICY.md`.
- [ ] `build-verify.ps1 -Config Release` succeeds clean (no new
      warnings).
- [ ] `tests/phase1-runtime/` contains the underrun hysteresis test
      and its passing trace (untracked).
- [ ] Diagnostics IOCTL exposes the Phase 6 `AO_V2_DIAG` schema
      (172 bytes) with steady-state counters readable via
      `test_stream_monitor.py`. **PASS_WITH_CAVEATS**:
      - PASS: live AO_V2_DIAG schema/readback, steady-state
        `Overflow=0 Underrun=0 Flag=0(ok) Fill=0 WrapBound=96000`
        per cable (see `step6-runtime-evidence.md` § 3, § 7 #2-#5).
      - DEFERRED to Phase 4/5: forced-overflow / forced-underrun
        live counter increments. Per `step4-audit.md` § 4 the
        canonical FRAME_PIPE is dormant in Phase 1 -- external
        callers (`minwavertstream.cpp:2297`,
        `transport_engine.cpp:1171`) hit no-op stubs, so live
        WASAPI traffic does not exercise `AoRingWriteFromScratch` /
        `AoRingReadToScratch` until Phase 4 (render flip) and
        Phase 5 (capture flip). The increment / hysteresis
        algorithms themselves are verified by the Step 2 / 3 / 5
        equivalent-logic harness (`ring_write_test.py` [2],
        `ring_read_test.py` R4-R6, `underrun_hysteresis_test.py`
        H2 / H3).
- [ ] No regression in non-cable paths (mic-array / speaker / save-
      data / tone-generator) -- **not claimed**. Step 6.2 install +
      live monitor confirmed basic driver install, service load,
      and IOCTL readback only; a full non-cable regression sweep is
      not part of Phase 1 evidence.

## Outcome

**Phase 1 classification: PASS_WITH_CAVEATS.**

After Phase 1, the cable `FRAME_PIPE` matches `docs/AO_CABLE_V1_DESIGN.md`
§ 2 exactly. Same-rate write/read works correctly with hard-reject
overflow and hysteretic underrun (verified by the equivalent-logic
harness and exposed via `IOCTL_AO_GET_STREAM_STATUS`). SRC is still
missing (Phase 2). The canonical advance helper is still missing
(Phase 3). Cable transport ownership has not yet flipped (Phases 4 /
5) -- until then the canonical FRAME_PIPE remains dormant for live
audio while the legacy `LOOPBACK_BUFFER` carries WASAPI traffic.

## Known Caveats / Non-claims

Phase 1 deliberately does NOT claim the following; later phases
address each:

- **Canonical FRAME_PIPE is initialized and observable.**
  `g_CableAPipe` / `g_CableBPipe` are allocated at adapter init,
  initialized to a known shape (`WrapBound = 96000`,
  `TargetLatencyFrames = 96000`), and their state is correctly
  reported through the new diagnostics IOCTL. The data path has
  just not yet flipped onto them.
- **`AoRingWriteFromScratch` / `AoRingReadToScratch` algorithms
  verified by Step 2 / 3 / 5 harness.**
  `tests/phase1-runtime/ring_helpers.py` re-implements the canonical
  helpers byte-equivalently (cross-checked by
  `ring_round_trip_test.py` RT1..RT5 in Step 3). Counter movements
  are demonstrated end-to-end in this harness
  (`ring_write_test.py` [2] forced overflow,
  `ring_read_test.py` R4-R6 underrun + hysteresis,
  `underrun_hysteresis_test.py` H2 single-event + H3 multi-event
  count). Phase 1 PASS depends on this equivalent-logic chain;
  Phase 4 / 5 will replace it with live observation.
- **Live WASAPI traffic still uses legacy `LOOPBACK_BUFFER`.**
  The legacy ring carries audio between Speaker and Mic endpoints
  in Phase 1. Step 6.2 captured 144000 frames (~66k non-zero
  samples) through the Mic side during a 3 s capture, with
  `LOOPBACK_BUFFER` as the source. The `LoopbackWrite` silent
  overwrite and `MicSink` dual-write recorded in
  `step4-audit.md` § 6 (RR-1, RR-2) remain in the legacy code path
  and are out of Phase 1 scope; they retire at Phase 5 / 6 with
  the audible flip and cleanup respectively.
- **Live forced-counter increments are not claimed.** Live
  WASAPI traffic does not move `A_OverflowCount` /
  `A_UnderrunCount` / `A_UnderrunFlag` in Phase 1, because the
  canonical FRAME_PIPE is the **target** shape, not the active
  data path. Phase 4 render coupling and Phase 5 capture coupling
  will migrate audible callers onto the canonical helpers, at
  which point the now-exposed counters will move under live
  traffic.

## Forbidden Carry-Over Into Phase 2

- Do not let Phase 2 weaken the hard-reject by adding a "skip if
  overflow imminent" path on the caller side. Hard-reject is the
  single source of truth.
- Do not let Phase 2 introduce a second SRC path. Single function per
  direction (per ADR-004).

## Phase 1 -> Phase 2 Handoff

Phase 2 implements the SRC path: GCD divisor 300/100/75, linear-
interpolation accumulator, per-channel state. Same atomic call,
same hard-reject on the write side, same hysteretic recovery on the
read side.
