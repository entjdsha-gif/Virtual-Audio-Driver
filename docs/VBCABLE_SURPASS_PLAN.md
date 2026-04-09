# AO Virtual Cable: VB-Cable Surpass Plan

## Current State (2026-04-09)

### Verified Working
- 48 PCM formats (8/16/24/32-bit, mono/stereo, 8k-192k)
- Float32 render + capture
- 5.1 / 7.1 multichannel
- SRC (44.1k / 8k / 192k)
- Multi-client (2+ concurrent streams)
- Control Panel tray app with IOCTL communication
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
- No 16-channel support yet
- No live operational diagnostics in Control Panel
- No quantitative quality harness yet (bit-exact, latency, dropout, drift)
- Some legacy/manual test scripts still need cleanup
- Upgrade still often requires reboot because the standalone control device and kernel service can remain loaded after PnP removal

---

## Gap Analysis: AO vs VB-Cable

| Axis | VB-Cable (Known) | AO Virtual Cable | Gap |
|------|-------------------|------------------|-----|
| Features | 2ch free, up to 8ch paid, no SRC | 8ch, SRC, float32, multi-client | AO ahead on SRC/format. Gap: no 16ch |
| Install | EXE installer, generally clean | Scripted flow, now reproducible and hash-verified | Gap reduced; polish still possible |
| Management | Minimal systray app | IOCTL control panel, config/status | Needs live status and self-test |
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

### M2: 16-Channel Selectable Architecture

**Goal:** Registry-driven 8/16 channel selection

**Approach:**
- Store max channel count per cable service under `Services\AOCableA\Parameters` and `Services\AOCableB\Parameters`
- Read at driver init and apply at device creation
- Require device restart for channel-mode changes
- Keep compile-time struct layout guards and add runtime validation for allowed channel counts

**Steps:**
1. Audit every 8-channel assumption and produce `channel-audit.md`
2. Lift internal channel count into a runtime parameter
3. Implement 8/16 selectable mode
4. Validate all existing tests at 8ch and add new 16ch coverage

### M3: Control Panel Operations Tool

**Goal:** Move beyond a settings UI into an operational tool

| Feature | Priority |
|---------|----------|
| Live stream status (format, state, stream count) | Must |
| Current config display (rate, depth, channels, latency) | Must |
| Self-test (one-click loopback verification) | Must |
| Channel mode selector (8/16, triggers device restart) | Must |
| Rate/latency control | Must |
| Diagnostic info (driver version, hash, driver store status) | Should |

### M4: Quality Measurement Framework

**Goal:** Produce quantitative results on the same hardware path

**Priority order:**
1. Bit-exact / null test
2. Latency
3. Dropout / underrun
4. Drift
5. THD+N (lower priority)

**Approach:**
- Python + sounddevice/PyAudio test harness
- Measure AO and VB-Cable on the same machine and same path
- Set pass criteria relative to the measured VB baseline, not arbitrary absolute numbers

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
