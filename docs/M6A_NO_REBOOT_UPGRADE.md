# M6a: No-Reboot Upgrade Path - Design Document

## Problem Statement

`install.ps1 -Action upgrade` usually requires a reboot because the kernel driver cannot be fully unloaded in the current boot session. The service reports `RUNNING + NOT_STOPPABLE`, which blocks binary replacement in `System32\drivers`.

---

## Root Cause Analysis

### The Unload Chain

For the driver to fully unload, ALL of the following must happen in order:

```
1. All user-mode handles closed     (Control Panel, apps)
       |
2. PnP devices removed              (pnputil /remove-device)
       |
3. Audio stack releases references   (wdmaudio.drv, AudioSrv)
       |
4. Control device deleted            (IoDeleteDevice)
       |
5. DriverUnload called               (kernel frees module)
       |
6. Service stops                     (sc.exe stop succeeds)
       |
7. .sys file unlocked                (can be replaced)
```

### Where It Breaks: Step 1 and Step 4

**Problem 1: Control device outlives PnP devices**

The control device (`\\.\AOCableA`) is created at `DriverEntry` (adapter.cpp:472) and deleted only at `DriverUnload` (adapter.cpp:192). PnP REMOVE_DEVICE (adapter.cpp:1846) does NOT touch the control device.

This means: after `pnputil /remove-device` removes audio endpoints, the control device still exists. The driver module stays loaded because it still owns a device object.

**Problem 2: Control Panel holds open handles**

`Source/ControlPanel/device.cpp:14-16` opens handles via `CreateFileW(L"\\\\.\\AOCableA")`. While the Control Panel is running, these handles keep the device referenced, preventing `IoDeleteDevice` from completing even if called.

**Problem 3: Audio service holds references**

Windows AudioSrv and wdmaudio.drv may hold device references through the PortCls miniport chain. These are released asynchronously after PnP removal and may take seconds.

### Current Flow (Requires Reboot)

```
install.ps1 upgrade:
  1. Remove-AllAODevices()
     - pnputil /remove-device (PnP endpoints gone)
     - sc.exe stop AOCableA   -> FAILS (NOT_STOPPABLE, control device still alive)
     - sc.exe delete AOCableA -> marks for deletion (pending reboot)
  2. Remove-Item aocablea.sys -> FAILS (file locked by loaded driver)
  3. Register-ResumeInstall   -> scheduled task for post-reboot
  4. Exit 3010 (reboot required)
```

---

## Proposed Solution: Pre-Upgrade Quiesce Protocol

### Phase 1: User-Mode Cleanup (install.ps1)

```powershell
# Step 1: Kill Control Panel (releases \\.\AOCableA handles)
Stop-Process -Name AOControlPanel -Force

# Step 2: Signal any other AO clients to close
# (future: broadcast a custom WM_AO_QUIESCE message)

# Step 3: Wait for handles to drain (poll with handle count check)
Wait-ForHandlesDrained -Device "\\.\AOCableA" -TimeoutSec 5
```

### Phase 2: Driver-Side Quiesce (kernel change)

Add a new IOCTL: `IOCTL_AO_PREPARE_UNLOAD`

When received, the driver:
1. Sets a global `g_PrepareUnload = TRUE` flag
2. Rejects new IRP_MJ_CREATE on the control device (returns STATUS_DEVICE_NOT_READY)
3. Waits for existing handle count to reach 0 (or the install script polls)
4. Calls `AoDeleteControlDevice()` proactively (without waiting for DriverUnload)

```c
// adapter.cpp - new IOCTL handler
case IOCTL_AO_PREPARE_UNLOAD:
    InterlockedExchange(&g_PrepareUnload, TRUE);
    // Reject new opens
    // Delete control device once refcount = 0
    AoDeleteControlDevice();
    status = STATUS_SUCCESS;
    break;
```

After this, the control device is gone. The driver module is only held by PnP device references.

### Phase 3: PnP Removal (install.ps1)

```powershell
# Step 4: Remove PnP devices
pnputil /remove-device $instanceId

# Step 5: Wait for AudioSrv to release (up to 10s)
Wait-ForServiceStop -Name AOCableA -TimeoutSec 10

# Step 6: Verify .sys is unlocked
Test-FileUnlocked "$env:SystemRoot\System32\drivers\aocablea.sys"
```

### Phase 4: Install New Version

```powershell
# Step 7: If unlocked, proceed with in-session install
Remove-Item aocablea.sys -Force
pnputil /add-driver aocablea.inf /install
# No reboot needed!
```

### Fallback: Reboot-Resume (unchanged)

If steps 4-6 fail (timeout, AudioSrv holds references), fall back to existing reboot-resume path. This becomes the exception, not the default.

---

## Implementation Plan

### Kernel Changes (adapter.cpp)

| Change | File | Lines | Complexity |
|--------|------|-------|------------|
| Add `g_PrepareUnload` flag | adapter.cpp | new global | Low |
| Add `IOCTL_AO_PREPARE_UNLOAD` handler | adapter.cpp | ~1730 | Low |
| Guard `AoControlCreate` with `g_PrepareUnload` | adapter.cpp | ~1685 | Low |
| Add open handle refcount tracking | adapter.cpp | CREATE/CLOSE handlers | Medium |
| Call `AoDeleteControlDevice()` when refcount hits 0 and `g_PrepareUnload` is set | adapter.cpp | CLOSE handler | Medium |

### Install Script Changes (install.ps1)

| Change | Complexity |
|--------|------------|
| Kill Control Panel before removal | Already done (line 470) |
| Send `IOCTL_AO_PREPARE_UNLOAD` via PowerShell | Medium (DeviceIoControl P/Invoke) |
| Poll for service stop with timeout | Low (extend existing `Wait-For-ServiceStop`) |
| Test file lock before attempting delete | Low |
| Conditional: skip reboot if in-session unload succeeds | Low |

### Control Panel Changes (ControlPanel)

| Change | Complexity |
|--------|------------|
| Handle `STATUS_DEVICE_NOT_READY` gracefully on open | Low |
| (Future) Listen for `WM_AO_QUIESCE` broadcast | Nice-to-have |

---

## Verification Steps

1. **Happy path**: Upgrade while Control Panel is running -> script kills CP, sends PREPARE_UNLOAD, PnP remove, service stops, binary replaced, new driver loads. No reboot.
2. **AudioSrv holds**: Upgrade while audio stream is active -> script waits 10s, AudioSrv releases, succeeds. No reboot.
3. **Timeout fallback**: External app holds handle -> 10s timeout expires -> reboot-resume path (existing behavior, no regression).
4. **Clean install**: No existing driver -> skip quiesce, install directly (existing behavior).

---

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| `AoDeleteControlDevice()` while IRP in flight | Check refcount before delete; IoDeleteDevice already drains pending IRPs |
| Race between PREPARE_UNLOAD and new CREATE | `g_PrepareUnload` flag checked in CREATE under lock; returns STATUS_DEVICE_NOT_READY |
| AudioSrv doesn't release in time | Timeout + existing reboot fallback; no regression |
| P/Invoke for DeviceIoControl in PowerShell | Well-documented pattern; can also use a small helper EXE |

---

## Estimated Effort

| Component | Estimate |
|-----------|----------|
| Kernel IOCTL + refcount | 2-3 hours |
| Install script changes | 1-2 hours |
| Testing (upgrade scenarios) | 2-3 hours |
| **Total** | **5-8 hours** |

---

## Success Criteria

- `install.ps1 -Action upgrade` completes without reboot when no external app holds AO handles
- Reboot-resume path still works as fallback
- No regression in clean install or uninstall flows
- Control Panel re-launches normally after upgrade
