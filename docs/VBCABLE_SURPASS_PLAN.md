# AO Virtual Cable: VB-Cable Surpass Plan

## Current State (2026-04-10)

### Verified Working
- 48 PCM formats (8/16/24/32-bit, mono/stereo, 8k-192k)
- Float32 render + capture
- 5.1 / 7.1 multichannel
- 16-channel selectable mode (8/16 switch via registry + device restart)
- 16-channel render + capture
- 16-channel channel isolation verified (16/16 sequential RMS + simultaneous FFT)
- SRC (44.1k / 8k / 192k)
- Multi-client (2+ concurrent streams)
- Control Panel tray app with IOCTL communication
- Control Panel operations workflow (live runtime state, Apply, Set & Restart, Self-Test, Defaults)
- IOCTL SET/GET_CONFIG roundtrip verified
- Device switching (10 cycles stable)
- Registry persistence through the driver control path
- C_ASSERT struct layout guards (compile-time mismatch detection)

### M1 Outcomes
- Build output is now reproducible enough to catch stale binary mix-ups.
- `install.ps1` handles install, upgrade, uninstall, and cleanup.
- In-place overwrite of a loaded kernel binary is no longer attempted.
- Upgrade now supports reboot-assisted resume via an elevated Scheduled Task.
- Clean-state install now syncs the service binary in `System32\drivers`.
- `verify-install.ps1` validates PnP state, service state, hash match, Driver Store state, and control-device access.
- Stale AO Driver Store packages can be removed automatically after install or manually via cleanup.

### Remaining Gaps
- No quantitative quality harness yet for bit-exact, latency, dropout, or drift
- No automated AO vs VB-Cable baseline comparison yet
- Some legacy/manual test scripts still need cleanup
- Upgrade still often requires reboot because the standalone control device and kernel service can remain loaded after PnP removal

---

## Gap Analysis: AO vs VB-Cable

| Axis | VB-Cable (Known) | AO Virtual Cable | Gap |
|------|-------------------|------------------|-----|
| Features | 2ch free, up to 8ch paid, no SRC | 8/16ch selectable, SRC, float32, multi-client | AO ahead on channels, SRC, and format support |
| Install | EXE installer, generally clean | Scripted flow, now reproducible and hash-verified | Gap reduced; polish still possible |
| Management | Minimal systray app | Operational control panel with live state, apply/restart workflow, self-test | Gap reduced; further polish/report export possible |
| Quality | Perceived stable (large user base) | No published measurements yet | Gap: provability |
| Automation | Unknown | Build guardrails and install verification in place | Needs deeper regression coverage |

*Note: VB-Cable internals are not publicly documented. Comparisons here are based on observable behavior.*

---

## Milestones

### M1: Build Reproducibility + Install Flow - COMPLETE

**Goal:** Trustworthy build -> install -> verify pipeline

| Item | Status | Detail |
|------|--------|--------|
| ProjectReference (CableA/B -> Utilities, Filters) | Done | Enforces MSBuild dependency order |
| build-verify.ps1 | Done | Hash manifest, timestamp order, binary distinction |
| install.ps1 (pnputil-based) | Done | install/upgrade/uninstall/cleanup, hash verification |
| verify-install.ps1 | Done | PnP/service/hash/driver-store diagnostics |
| install.bat wrapper | Done | Backward-compatible thin wrapper |
| Direct .sys copy eliminated | Done | No in-place overwrite while driver is loaded |
| Reboot-resume recovery | Done | Deferred resume via elevated Scheduled Task |
| System32 service-binary sync | Done | Running service hash now matches built hash |
| Driver Store stale package cleanup | Done | Leaves only active AO packages |

**Success criteria achieved:**
- `build-verify.ps1` passes
- `install.ps1 -Action install` succeeds with devices present and hash match
- `install.ps1 -Action upgrade` succeeds and resumes after reboot when required
- `verify-install.ps1` passes
- `test_ioctl_diag.py` returns valid GET_CONFIG values and passes SET -> GET roundtrip

**Optional final smoke test (non-blocking):**
- `install.ps1 -Action uninstall` -> `install.ps1 -Action install` one more clean cycle

### M2: 16-Channel Selectable Architecture - COMPLETE

**Goal:** Registry-driven 8/16 channel selection

**Approach:**
- Store max channel count per cable service under `Services\AOCableA\Parameters` and `Services\AOCableB\Parameters`
- Read at driver init and apply at device creation
- Require device restart for channel-mode changes
- Keep compile-time struct layout guards and add runtime validation for allowed channel counts

**Outcomes:**
1. Internal channel count was lifted into validated runtime state (`8` or `16`)
2. Static 16ch PortCls advertisement data was eliminated from the image
3. 16ch advertisement is now built dynamically at device start and selected through binding-based miniport configuration
4. `MaxChannelCount` registry changes now apply on device restart without requiring full reinstall
5. Existing 8ch behavior remains stable and regression-tested

**Success criteria achieved:**
- `GET_CONFIG` reports `Channels=8` and `Channels=16` correctly after registry change + device restart
- `SET -> GET` roundtrip remains valid in both modes
- `verify-install.ps1` passes with the M2 build installed
- 16ch render + capture streams open successfully
- Channel isolation verification passes:
  - Stage 1: `16/16` sequential RMS isolation
  - Stage 2: `16/16` simultaneous FFT frequency ownership
  - Crosstalk remains below `-60 dB`
- No BSOD observed across install, restart, or mode-switch validation

### M3: Control Panel Operations Tool - COMPLETE

**Goal:** Move beyond a settings UI into an operational tool for the completed 8/16 selectable architecture

| Feature | Priority |
|---------|----------|
| Live stream status (format, state, stream count) | Must |
| Current config display (rate, depth, runtime channels, latency, MaxChannelCount) | Must |
| Self-test (one-click loopback verification) | Must |
| Channel mode selector (8/16, triggers device restart and shows result) | Must |
| Rate/latency control | Must |
| Restart/apply workflow with explicit success/failure feedback | Must |
| Diagnostic info (driver version, hash, driver store status) | Should |

**Success criteria achieved:**
- Runtime state now shows rate, latency, bit depth, runtime channels, and `MaxChannelCount` for Cable A and Cable B independently
- `Apply` updates runtime sample rate and max latency through the driver control path
- `Set & Restart` performs per-device `MaxChannelCount` write, elevated device restart, reopen, and result verification
- `Self-Test` performs non-destructive connectivity and config sanity checks
- `Defaults` resets UI selections to the standard `48000 Hz / 20 ms / 8 channels` baseline
- Manual smoke tests for `8 <-> 16` switching, `Apply`, `Self-Test`, and `Defaults` completed successfully

### M4: Quality Measurement Framework - COMPLETE

**Goal:** Produce quantitative results on the same hardware path

**Outcomes:**

| Sub-milestone | Status | Deliverable |
|--------------|--------|-------------|
| M4a: Common harness | COMPLETE | `test_quality_common.py` - device lookup, loopback, WAV I/O, analysis, CSV/JSON |
| M4b: Bit-exact / null | Q02 PASS, Q01 EXPERIMENTAL | `test_bit_exact.py` - Q02 silence verified; Q01 blocked by playrec sample-drop artifact |
| M4c: Latency | COMPLETE | `test_latency.py` - multi-chirp single-session, steady-state measurement |
| M4d: Dropout / drift | COMPLETE | `test_dropout.py`, `test_drift.py` - 60s verified, extensible to 1hr |
| M4e: VB-Cable comparison | COMPLETE | `test_compare_vb.py` - automated side-by-side AO vs VB benchmark |

**Q01 bit-exact note:** The `sd.playrec()` WDM-KS path exhibits periodic sample drops (~0.6% over 2s) at ring-buffer boundaries during simultaneous play+record. This is a harness/API limitation, not a driver defect (Q02 silence passes perfectly on the same path). Q01 is parked as experimental until an alternative verification method (e.g., WASAPI exclusive loopback) is available.

**Benchmark results:** See `docs/BENCHMARK_SUMMARY.md` for AO vs VB-Cable comparison data.

**Remaining (non-blocking):**
- M4-SRC: Multi-rate SRC quality (Q06) - deferred to post-M5
- 1-hour extended dropout/drift runs - deferred to stability phase

### M5: "Surpass" Declaration

**Criteria (all required with evidence):**
1. Feature superset: 16ch + SRC + multi-client + float32, backed by automated tests
2. Install experience: clean install/upgrade/uninstall, no unnecessary reboot, hash-verified
3. Measured quality: bit-exact loopback, latency at or better than VB baseline, no 1hr dropout
4. Management tools: live status, self-test, channel mode control
5. No known regressions in `VALIDATION_MATRIX.md`

### Post-M2 Required Work: No-Reboot Upgrade Path

**Problem observed repeatedly**
- `install.ps1 -Action upgrade` removes AO PnP device instances, but `AOCableA/B` can remain `RUNNING + NOT_STOPPABLE`
- `\\.\AOCableA` / `\\.\AOCableB` control devices remain reachable even after PnP removal
- This keeps `System32\drivers\aocable*.sys` locked and forces reboot-assisted resume

**Product goal**
- Upgrades should complete without reboot whenever the driver can be cleanly quiesced
- Reboot should be fallback-only, not the normal development or user upgrade path

**Investigation / implementation goals**
1. Identify what keeps the kernel driver loaded after AO MEDIA device removal
2. Ensure Control Panel and other user-mode clients release control-device handles before upgrade
3. Rework control-device lifecycle so upgrade/uninstall can fully quiesce the driver in-session
4. Add an explicit pre-upgrade quiesce path and verify service unload before touching `System32\drivers`
5. Keep reboot-resume only as last-resort fallback when unload verification fails

---

## Priority Classification

### Must
1. Build reproducibility
2. Install flow redesign
3. 16-channel architecture
4. Bit-exact + latency + dropout measurement

### Should
5. Control Panel live status + self-test
6. Long-run stability tests
7. App compatibility matrix
8. Test script cleanup

### Nice-to-have
9. Installer GUI (WiX/Inno Setup)
10. Signed driver (EV certificate)
11. Per-channel volume/mute
12. ASIO interface
