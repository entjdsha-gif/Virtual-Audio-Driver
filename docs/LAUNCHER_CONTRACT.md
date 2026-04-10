# AO Virtual Cable - Launcher Integration Contract

## Overview

External launchers can manage AO Virtual Cable lifecycle by calling
`install-core.ps1` with standardized actions, flags, and exit codes.

All interactions are through a single entry point:
```
powershell.exe -NoProfile -ExecutionPolicy Bypass -File install-core.ps1 -Action <action> [-Silent] [-JsonOutput]
```

---

## Actions

| Action | Description | Requires Admin | Modifies System |
|--------|-------------|---------------|-----------------|
| `health-check` | Report installation status | No | No |
| `install` | Fresh install (auto-upgrades if already installed) | Yes | Yes |
| `upgrade` | Remove existing + install new | Yes | Yes |
| `uninstall` | Remove all components | Yes | Yes |
| `repair` | Reinstall over existing (keeps settings) | Yes | Yes |

---

## Flags

| Flag | Description |
|------|-------------|
| `-Silent` | No interactive prompts. Reboot auto-triggers on failure paths. |
| `-JsonOutput` | Emit a single JSON object to stdout as the last line. For `health-check`, this is the only meaningful output. For other actions, JSON summary is appended after human-readable log. |

---

## Exit Codes

| Code | Meaning | Launcher Response |
|------|---------|-------------------|
| 0 | Success | Done |
| 1 | General failure | Show error log |
| 10 | Healthy (health-check: installed and working) | No action needed |
| 20 | Not installed (health-check) | Trigger install |
| 21 | Upgrade available (health-check: version mismatch) | Trigger upgrade |
| 22 | Degraded (health-check: partial install) | Trigger repair |
| 30 | Reboot required (install/upgrade deferred) | Schedule reboot |
| 40 | Admin privileges required | Re-launch elevated |
| 41 | Blocked (Secure Boot / test-signing issue) | Show instructions |
| 50 | Install failed | Show error, retry later |

---

## health-check JSON Schema

```json
{
  "status": "healthy|not_installed|degraded|upgrade_available",
  "exitCode": 10,
  "devices": {
    "AOCableA": { "present": true, "status": "OK", "service": "Running" },
    "AOCableB": { "present": true, "status": "OK", "service": "Running" }
  },
  "controlDevices": {
    "AOCableA": true,
    "AOCableB": true
  },
  "driverStore": {
    "activePackages": ["oem84.inf", "oem85.inf"],
    "stalePackages": []
  },
  "installedVersion": {
    "aocablea.sys": { "sha256": "...", "size": 97256, "modified": "2026-04-10 15:07:55" },
    "aocableb.sys": { "sha256": "...", "size": 97256, "modified": "2026-04-10 15:08:13" }
  },
  "bundledVersion": {
    "aocablea.sys": { "sha256": "...", "size": 97256 },
    "aocableb.sys": { "sha256": "...", "size": 97256 }
  },
  "versionMatch": true,
  "testSigning": true,
  "secureBoot": false
}
```

### Field notes

- **testSigning**: `true` | `false` | `null`
  - `true`: confirmed via bcdedit (admin) or indirect signal (AO services running with test-signed driver)
  - `false`: confirmed off via bcdedit
  - `null`: could not determine (non-admin, no AO services running). Launcher should treat as unknown and re-check elevated if needed.
- **bundledVersion**: may be `{}` when health-check is executed outside the packaged installer directory (e.g., from repo root or a non-package path). Launcher should always execute health-check from the packaged installer root so bundled hashes are available for version comparison. When empty, `versionMatch` is always `false`.
- **versionMatch**: `true` only when both installed and bundled hashes are available and match. `false` when either is missing or hashes differ.

---

## Launcher Call Sequence

### On login / periodic check

```
1. install-core.ps1 -Action health-check -Silent -JsonOutput
   |
   +-- exit 10 (healthy, version match)    -> done
   +-- exit 20 (not installed)             -> go to step 2
   +-- exit 21 (upgrade available)         -> go to step 3
   +-- exit 22 (degraded)                  -> go to step 4
   +-- exit 40 (need admin)                -> prompt elevation, retry
   +-- exit 41 (blocked)                   -> show instructions

2. install-core.ps1 -Action install -Silent -JsonOutput
   |
   +-- exit 0  (success)                   -> done
   +-- exit 30 (reboot required)           -> schedule reboot
   +-- exit 50 (failed)                    -> log error, retry later

3. install-core.ps1 -Action upgrade -Silent -JsonOutput
   |
   +-- exit 0  (success)                   -> done
   +-- exit 30 (reboot required)           -> schedule reboot
   +-- exit 50 (failed)                    -> log error, retry later

4. install-core.ps1 -Action repair -Silent -JsonOutput
   (same as upgrade but preserves registry settings)
```

### On user request (uninstall)

```
install-core.ps1 -Action uninstall -Silent -JsonOutput
   |
   +-- exit 0  (success)                   -> done
   +-- exit 30 (reboot required)           -> schedule reboot
```

---

## Version Comparison

Launcher determines upgrade-needed by comparing:
- `installedVersion.aocablea.sys.sha256` vs `bundledVersion.aocablea.sys.sha256`
- If hashes differ: exit 21 (upgrade available)
- If installed but hashes match: exit 10 (healthy)
- If not installed: exit 20

Bundled version is read from `drivers/CableA/aocablea.sys` in the package.
Installed version is read from `%SystemRoot%\System32\drivers\aocablea.sys`.

---

## Notes

- `health-check` does NOT require admin and does NOT modify the system
- All modifying actions (`install`, `upgrade`, `uninstall`, `repair`) require admin
- `-JsonOutput` JSON is always the last line of stdout (parseable via `| Select-Object -Last 1 | ConvertFrom-Json`)
- Human-readable log goes to stdout before JSON (launcher can ignore or capture)
- `-Silent` suppresses Read-Host prompts; auto-reboots on failure paths
