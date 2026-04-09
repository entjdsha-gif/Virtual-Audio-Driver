# AO Virtual Cable - Validation Matrix

## 1. Feature Verification

| ID | Test | Method | Pass Criteria | Status |
|----|------|--------|---------------|--------|
| F01 | PCM 16-bit stereo 48kHz render | PyAudio stream open+write | No exception | PASS |
| F02 | PCM 16-bit stereo 48kHz capture | PyAudio stream open+read | Data received | PASS |
| F03 | PCM 8/16/24/32-bit formats (48 combos) | `test_formats.py` | All open+write | PASS |
| F04 | Float32 render | PyAudio `paFloat32` | No exception | PASS |
| F05 | Float32 capture | PyAudio `paFloat32` | Data received | PASS |
| F06 | 5.1 (6ch) render + capture | `test_multichannel.py` | Stream open + bytes received | PASS (stream-level) |
| F07 | 7.1 (8ch) render + capture | `test_multichannel.py` | Stream open + bytes received | PASS (stream-level) |
| F08 | SRC 44.1kHz -> internal | PyAudio 44100Hz stream | No exception | PASS |
| F09 | SRC 8kHz -> internal | PyAudio 8000Hz stream | No exception | PASS |
| F10 | SRC 192kHz -> internal | PyAudio 192000Hz stream | No exception | PASS |
| F11 | Multi-client (2 streams) | Concurrent render | Both succeed | PASS |
| F12 | 16ch render + capture | `test_16ch_isolation.py` | Loopback verified | PASS |
| F13 | 8/16 channel switch via registry | Registry + device restart + `test_ioctl_diag.py` | Both modes work | PASS |
| F14 | Mono render (channel upmix) | PyAudio 1ch | No exception | PASS |

## 2. IOCTL / Control Panel Verification

| ID | Test | Method | Pass Criteria | Status |
|----|------|--------|---------------|--------|
| I01 | Device open (`\\.\AOCableA`) | `CreateFileW` | Handle valid | PASS |
| I02 | Device open (`\\.\AOCableB`) | `CreateFileW` | Handle valid | PASS |
| I03 | GET_CONFIG initial values | `DeviceIoControl` | Rate > 0, Latency > 0 | PASS |
| I04 | SET_INTERNAL_RATE(96000) | `DeviceIoControl` | Success | PASS |
| I05 | SET_MAX_LATENCY(5) | `DeviceIoControl` | Success | PASS |
| I06 | GET_CONFIG after SET | `DeviceIoControl` | `(96000,5,24,8)` | PASS |
| I07 | GET_STREAM_STATUS | `DeviceIoControl` | Valid status struct | PASS |
| I08 | Registry persistence after SET | `winreg` read | Values match SET | PASS |
| I09 | GET_CONFIG after reboot | `DeviceIoControl` | Values match registry | TODO |
| I10 | Control Panel Settings dialog | Manual | Opens, shows values | PASS |

## 3. Install / Update Verification

| ID | Test | Method | Pass Criteria | Status |
|----|------|--------|---------------|--------|
| N01 | Clean install | `install.ps1 -Action install` | Devices appear, hash match | PASS |
| N02 | Upgrade | `install.ps1 -Action upgrade` | New driver loaded, hash verified | PASS |
| N03 | Uninstall | `install.ps1 -Action uninstall` | All components removed | TODO (recommended final smoke) |
| N04 | Uninstall + reinstall | Sequential N03 -> N01 | Clean state restored | TODO (recommended final smoke) |
| N05 | Double uninstall | Uninstall when not installed | Graceful no-op | TODO |
| N06 | No admin error | Run install without elevation | Clear error message | TODO |
| N07 | Hash verification | `verify-install.ps1` | built == installed | PASS |
| N08 | Stale `.sys` detection | `verify-install.ps1` | Flags mismatch correctly | PASS |
| N09 | Build order enforcement | `build-verify.ps1` | lib timestamp <= sys | PASS |
| N10 | No unnecessary reboot | pnputil install flow | Reboot only when old loaded driver blocks replacement | PASS |
| N11 | Driver Store cleanup | `install.ps1 -Action cleanup` | Only active AO packages remain | PASS |
| N12 | In-session upgrade without reboot | Upgrade while AO is installed and idle | Driver unloads, new package activates, no reboot required | TODO |
| N13 | Upgrade fallback behavior | Upgrade while driver cannot unload | Clear reboot-required message and clean auto-resume | PASS |

## 4. Stability Verification

| ID | Test | Method | Pass Criteria | Status |
|----|------|--------|---------------|--------|
| S01 | Device switching (10 cycles) | `Set-AudioDevice` loop | All succeed | PASS |
| S02 | Device switching (100 cycles) | Extended loop | All succeed, no BSOD | TODO |
| S03 | 1hr continuous render | PyAudio write loop | No dropout, no crash | TODO |
| S04 | 1hr continuous loopback | Render + capture loop | No drift, no dropout | TODO |
| S05 | Sleep/resume recovery | S3 suspend + resume | Streams resume | TODO |
| S06 | App restart (10 cycles) | Open/close stream loop | All succeed | TODO |
| S07 | Multi-client 3+ streams | 3 concurrent renders | All succeed | TODO |
| S08 | No BSOD in 24hr normal use | System event log | No bugcheck events | TODO |

## 5. Quality / Performance Measurement

| ID | Metric | Method | Pass Criteria | Status |
|----|--------|--------|---------------|--------|
| Q01 | Bit-exact loopback (matching format) | Render/capture/diff | Zero residual | TODO |
| Q02 | Null test (silence in, check output) | Render silence + capture | All zeros | TODO |
| Q03 | Round-trip latency | Impulse cross-correlation | Measured (ms) | TODO |
| Q04 | Dropout count (1hr) | Gap detection in capture | 0 dropouts | TODO |
| Q05 | Sample drift (1hr) | Render vs capture count | <= 1 sample/hr | TODO |
| Q06 | Multi-rate SRC quality | Sweep/capture/compare | Visual/numeric report | TODO |
| Q07 | AO vs VB-Cable latency | Same hardware, same test | AO <= VB + 1ms | TODO |
| Q08 | AO vs VB-Cable dropout | Same hardware, 1hr each | AO <= VB | TODO |

## 6. Compatibility Matrix

| ID | Application | Test | Pass Criteria | Status |
|----|-------------|------|---------------|--------|
| C01 | Windows Sound Settings | Set as default I/O | Device selectable | PASS |
| C02 | Audacity | Record via AO Cable | Audio captured | TODO |
| C03 | OBS Studio | Audio source: AO Cable | Audio captured in recording | TODO |
| C04 | Discord | Input device: AO Cable | Voice transmitted | TODO |
| C05 | Chrome/Edge | Play to AO Cable output | Audio rendered | TODO |
| C06 | WASAPI exclusive mode | Exclusive stream open | Stream opens | TODO |
| C07 | WASAPI shared mode | Shared stream open | Coexists with others | TODO |

---

## Test Script Reference

| Script | Tests Covered | Reliability |
|--------|---------------|-------------|
| `test_formats.py` | F01-F05, F14 | High |
| `test_multichannel.py` | F06, F07 | Medium (stream-level only) |
| `test_16ch_isolation.py` | F12, F13 | High |
| `test_ioctl_diag.py` | I01-I08 | High |
| `test_complete.ps1` | Mixed | Medium |
| `test_peak.ps1` | Manual loopback check | Manual only |
| `test_full.ps1` | Mixed | Stale - needs update |
| `build-verify.ps1` | N09 | High |
| `verify-install.ps1` | N07, N08, N11 | High |
