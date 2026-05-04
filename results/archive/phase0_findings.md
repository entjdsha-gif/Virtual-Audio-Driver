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

## Step 4 — Position-query entrypoint runtime verification: PASS

Date closed: 2026-04-13
Transport: KDNET two-box, separate physical target (host 192.168.0.2, target 192.168.0.9, Windows 10 22H2 build 19045).

### Method
- Host: WinDbg classic 10.0.26100.7705, attached via `-k net:port=50000,key=...`.
- Break-in via `Debug -> Break` once target booted into Windows desktop.
- `.sympath` set to MS symbol server + local `x64\Release\Cable{A,B}`; `.reload /f` completed successfully for both AO modules with `(private pdb symbols)` status.
- Three position-query entry points found on both cables (not two as the plan initially assumed):
  - `CMiniportWaveRTStream::GetPosition (KSAUDIO_POSITION*)`
  - `CMiniportWaveRTStream::GetPositions (uint64*, uint64*, LARGE_INTEGER*)`
  - `CMiniportWaveRTStream::GetPositionRegister (KSRTAUDIO_HWREGISTER*)`
- Six counter breakpoints installed via `bu ... "r @$tN = @$tN + 1; gc"`.
- Windows default audio device set to `AO Virtual Cable A` (render + capture) on the target before the call.
- Phone Link outbound call placed from target; call ran for ~5 seconds while the remote party spoke several phrases. Target did not route a live microphone signal into AO capture, so the target->remote direction was silent.

### Counter result (5-second window)

| Counter | Symbol | Value |
|---|---|---|
| `@$t0` | `aocablea!CMiniportWaveRTStream::GetPosition` | 0 |
| `@$t1` | `aocablea!CMiniportWaveRTStream::GetPositions` | **120** |
| `@$t2` | `aocablea!CMiniportWaveRTStream::GetPositionRegister` | 0 |
| `@$t3` | `aocableb!CMiniportWaveRTStream::GetPosition` | 0 |
| `@$t4` | `aocableb!CMiniportWaveRTStream::GetPositions` | 0 |
| `@$t5` | `aocableb!CMiniportWaveRTStream::GetPositionRegister` | 0 |
| | **sum (run-time queries, t0+t1+t3+t4)** | **120** |

### ICF observation
`GetPositionRegister` on both cables is folded by the linker onto the same address as `CMiniportTopologyVirtualAudioDriver::DataRangeIntersection`. This is Identical COMDAT Folding of unimplemented stubs — `GetPositionRegister` is not a real implementation; portcls that calls it receives the shared stub return. Counters `@$t2` / `@$t5` remained at 0 throughout, which additionally confirms that neither the real register-path nor the folded DataRangeIntersection call site fires during the active-call window.

### Gate decision
- Hit-count threshold: 120 >= 100. **PASS.**
- Dominant entry point: `CMiniportWaveRTStream::GetPositions` (plural), exclusively. The singular `GetPosition` fires zero times.
- Cable B counters all zero: expected, because only Cable A was selected as the Windows default audio device during the measurement. Cable B coverage is deferred to a later measurement once each cable is exercised independently.
- Call rate: 120 hits / ~5 s ≈ 24 Hz on the active render stream, which is consistent with portcls's typical position-query cadence and aligns with the VB-Cable behavior recorded in `results/vbcable_runtime_claude.md`.

### OQ3 closure
OQ3 (whether portcls dispatches through a different entry point than `GetPosition`/`GetPositions`) is now closed. `GetPositions` is the single hot path on this AO build. **Phase 3 must hook the query-driven pump into `CMiniportWaveRTStream::GetPositions`**, not `GetPosition`. The singular form can be treated as cold code for the Phase 3 redesign, and `GetPositionRegister` can be left as the ICF-folded stub it already is.

## Step 5 — Live call quality baseline: mixed

Baseline recorded from the same call that supplied Step 4 counter data. Remote->target direction carried the remote party's speech audibly, target->remote direction was silent because no live microphone source was routed into AO capture during this measurement (Windows default capture was set to AO Cable A but nothing was writing samples into it). Using the 3-valued scale agreed for Phase 0 bring-up:

- `mixed` — one direction active with audio flowing, other direction empty by test-setup choice, not by driver fault.

This is the Phase 0 "before" reference. It does not attempt to characterize render-side distortion at the sample level — that is deferred until an instrumented pump (Phase 1 counters and Phase 3 pump helper) exists and a proper bidirectional harness is run. The historical "AO Cable: heavy distortion, VB Cable: clean" note from `CLAUDE.md` remains the qualitative reference for future A/B comparisons.

## Phase 0 gate: CLOSED

- Steps 1-3: PASS.
- Step 4: PASS with explicit hook target recorded (`GetPositions`).
- Step 5: baseline recorded as `mixed` (see above).
- No source-side behavior changes landed during Phase 0. The only code change was the installability-only INF target-OS widening (commit 4da70ad), which does not touch runtime driver behavior and is independent of the Phase 0 gate criteria.
- KDNET setup is intentionally left attached (target `bcdedit /debug on` preserved, key unchanged) so Phase 1 / Phase 3 can re-attach via the same `-k net:port=50000,key=...` invocation without reconfiguring the target.
- Phase 1 and Phase 3 implementation work is now **unblocked**.

## Step 4 — (historical) deferred rationale

The section below is preserved as the record of why Step 4 was initially blocked before the KDNET target was available. It is kept for traceability and is no longer the current gate state.



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

## Open items (historical, closed by the KDNET run above)

- [x] Spin up Hyper-V VM (or equivalent) with KDNET + test signing; install AO on target; run Step 4 + Step 5 together (5-second Phone Link AO call with `bu`-based counters on `aocablea`/`aocableb` `GetPosition`/`GetPositions`). Resolved by a separate physical Windows 10 22H2 target (192.168.0.9) over KDNET; Hyper-V was not used.
- [x] Once Step 4 closes, update this file with: counter values, dominant hit function, baseline quality, and confirmed Phase 3 hook target.
- [x] Until then, no Phase 1 or Phase 3 code changes. Constraint released now that the gate is closed; Phase 1 may start.
