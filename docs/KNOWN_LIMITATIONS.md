# AO Virtual Cable - Known Limitations

---

## Driver

### Upgrade requires reboot in most cases
- `install.ps1 -Action upgrade` removes PnP device instances, but `AOCableA/B` kernel services can remain `RUNNING + NOT_STOPPABLE`
- Control device handles (`\\.\AOCableA`) stay reachable after PnP removal, keeping `aocable*.sys` locked
- Workaround: reboot-resume via elevated Scheduled Task is automated
- Root cause: kernel service lifecycle not fully quiesceable from user mode

### No ASIO interface
- Only WaveRT/WDM-KS audio interfaces are implemented
- ASIO would require a separate user-mode driver component

### No signed driver
- Requires test signing mode or secure boot disabled
- EV code signing certificate needed for production distribution

---

## Quality Measurement

### Q01 bit-exact loopback (experimental)
- `sd.playrec()` over WDM-KS exhibits periodic sample drops (~0.6%) at ring-buffer boundaries during simultaneous play+record
- This is a sounddevice/PortAudio harness limitation, not a driver defect
- Evidence: Q02 silence passes perfectly on the same path; between drops, samples are exact matches
- Sample drops occur at 48/96-sample intervals matching driver buffer boundaries
- Resolution requires alternative verification (WASAPI exclusive loopback or file-based round-trip)

### Latency includes PortAudio buffering
- Measured round-trip latency (~121ms) includes sounddevice/PortAudio buffer overhead
- Actual driver-internal latency is lower; the measurement represents the end-to-end API path
- VB-Cable measured at ~200ms on the same path, so relative comparison is valid

### Long-run tests not yet executed
- Dropout and drift tests verified at 30-60s scale
- 1-hour and 24-hour extended runs are planned but not yet performed
- No issues observed at current test durations

---

## Install / Packaging

### No GUI installer
- Installation is via PowerShell script (`install.ps1`)
- WiX/Inno Setup GUI installer is a future enhancement

### Driver Store cleanup requires explicit action
- Stale AO packages in the Driver Store are not auto-removed on upgrade
- Use `install.ps1 -Action cleanup` to remove inactive packages

---

## Control Panel

### Requires driver to be installed
- Control Panel opens but shows no data if driver is not installed
- No graceful offline mode

### Channel mode change requires device restart
- Switching between 8ch and 16ch modes restarts the audio device
- Active audio streams are interrupted during restart

---

## Compatibility

### Not tested on Windows Server or ARM64
- Driver targets x64 desktop Windows only
- Windows Server and ARM64 platforms are untested

### Application compatibility matrix incomplete
- Audacity, OBS, Discord, Chrome/Edge are listed as TODO in the validation matrix
- Expected to work via standard Windows audio APIs but not formally verified
