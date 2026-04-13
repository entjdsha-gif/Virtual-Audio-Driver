# Phase 0 Findings

Date: 2026-04-13
Branch: feature/ao-fixed-pipe-rewrite @ f2fe231
Plan refs:
- docs/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CODEX.md (Phase 0: Freeze evidence and naming)
- results/VB_CABLE_AO_REIMPLEMENTATION_PLAN_CLAUDE.md (Phase 0: Pre-flight)

## Step 1 — Build sanity: PASS (with noted warning)

- Toolchain blocker resolved: installed `Microsoft.VisualStudio.Component.VC.14.44.17.14.x86.x64.Spectre` (previously missing; MSB8040 on all kernel-mode projects).
- MSBuild Release x64 produced and signed `aocablea.sys`, `aocableb.sys`, `AOControlPanel.exe`.
- `build-verify.ps1 -Config Release`: 17 PASS / 0 FAIL.
- Known warning: Package project `inf2cat` fails with 22.9.1 copylist/source-media mismatch (`aocablea.sys`/`aocableb.sys` not found in `package\` staging dir). Not an install-path blocker: `install.ps1` runs its own staged packaging via `New-StagedDriverPackage`, which invokes `stampinf`/`inf2cat`/`sign` independently.

## Step 2 — Baseline install: PASS

- `install.ps1 -Action upgrade` (elevated, no reboot).
- In-session quiesce via PREPARE_UNLOAD succeeded for both cables; devcon removal + driver store cleanup completed without reboot.
- Staged packaging via `New-StagedDriverPackage` succeeded for Cable A/B (confirms MSBuild Package-project `inf2cat` failure does NOT affect the install path).
- `pnputil` install succeeded; staged == installed hash match; `install-manifest.json` written.
- `INSTALL_EXIT=0`.
- Non-blocking warning: `C:\WINDOWS\System32\drivers\virtualaudiodriver.sys` (V1 leftover) could not be removed (access denied); script treated as stale.

## Step 3 — IOCTL smoke: PASS

- `python test_ioctl_diag.py`.
- Cable A: GET_CONFIG returned Rate=48000, Latency=20, Bits=24, Channels=8. SET_INTERNAL_RATE=96000 / SET_MAX_LATENCY=10 roundtrip OK. Registry Parameters verified.
- Cable B: identical, roundtrip OK.
- Result: ALL PASSED.

## Step 4 — Position-query entrypoint runtime verification: NOT RUN (blocked)

### Intent (per plan)
Verify that `CMiniportWaveRTStream::GetPosition` and/or `GetPositions` fire during a Phone Link AO-profile live call. The exit criterion is ≥100 combined hits across 5 seconds, with the dominant function recorded as the Phase 3 hook target.

### Why runtime evidence is required
Static review alone is insufficient per Codex plan v3 — OQ3 was elevated to a blocking Phase 0 gate precisely because portcls may dispatch position queries through a different entrypoint than the current AO `GetPosition`/`GetPositions` methods. Phase 3 wiring depends on knowing which entrypoint is actually hot on this AO build.

### Constraints that rule out source-modification paths
- `ioctl.h` currently exposes only `AO_STREAM_STATUS` + `AO_CONFIG`; no position-query counters.
- `test_stream_monitor.py` comment acknowledges `AO_V2_DIAG` is planned but not yet implemented in the driver.
- Phase 0 is "freeze evidence and naming" — adding counters now would land Phase 1 work before Phase 0 gate closes, which violates the operating-rules phase workflow (Rule 3).

### Why same-PC local kernel debug cannot close this gate
`kd -kl` (local kernel debug) is read-only by platform design. Microsoft documents the following commands as unavailable in local KD:
- Setting breakpoints (`bp`, `bu`, `bm`, `ba`)
- Stepping (`p`, `t`, `pa`, `ta`, ...)
- Viewing processor registers
- User-mode thread stack traces

The structural reason: conventional kernel debugging requires two separate execution contexts (debugger + target kernel). Local KD runs inside a single OS instance as a read-only inspector with no CPU trap handler to service software breakpoints. Counter-only `bu ... "r @$t0=@$t0+1; gc"` cannot fire because there is no debugger context to execute the command.

**Conclusion: same-PC local kd breakpoint verification is unsupported by platform design.** Local KD failure demo was explicitly skipped per user direction; this document is the record.

### Remaining viable path
Hyper-V VM (or separate physical target) with KDNET. The VM runs AO under test signing; the host WinDbg attaches over KDNET and sets real `bu` breakpoints. Same physical machine is acceptable — Hyper-V provides the second execution context required.

Step 4 runtime evidence is **deferred until a KDNET target is available**. No Phase 1 / Phase 3 implementation work starts until Step 4 closes with a recorded hit count and a named Phase 3 hook target.

### Environment state at time of findings
- Host build/install: green (Steps 1-3 above).
- `tests/live_call/.env`: `AUDIO_CABLE_PROFILE=ao`, `PHONE_LINK_DIAL_MODE=hidden_uri_only`, `AMD_ENABLED=false`, `PHONE_LINK_DEVICE_ID` set.
- Live call baseline quality (VB vs AO) already known from prior work: VB clean, AO distorted. This is the "before" reference for the rewrite; Step 5 re-measurement will happen together with Step 4 on the KDNET target.

## Open items

- [ ] Spin up Hyper-V VM (or equivalent) with KDNET + test signing; install AO on target; run Step 4 + Step 5 together (5-second Phone Link AO call with `bu`-based counters on `aocablea`/`aocableb` `GetPosition`/`GetPositions`).
- [ ] Once Step 4 closes, update this file with: counter values, dominant hit function, baseline quality (clean/garbled/silent/mixed), and confirmed Phase 3 hook target.
- [ ] Until then, no Phase 1 or Phase 3 code changes.
