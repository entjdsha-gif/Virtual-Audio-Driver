# AO Virtual Cable - Release Hardening Plan

## Current Package Contents (installer/out/AOVirtualCable/)

```
AOVirtualCable/
  Setup.bat                  user-facing entry point
  Uninstall.bat              user-facing uninstall
  install-core.ps1           full installer logic (plaintext PowerShell)
  install-manifest.json      installed driver hashes
  manifest.json              package file hashes
  AOControlPanel.exe         tray app
  devgen.exe                 WDK tool (root device creation)
  devcon.exe                 WDK tool (driver binding)
  drivers/
    CableA/
      aocablea.sys            kernel driver
      aocablea.inf            INF file
      aocablea.cat            catalog (signing)
    CableB/
      aocableb.sys
      aocableb.inf
      aocableb.cat
```

**Total: 14 files, ~370 KB**

---

## Exposure Analysis

### What users currently see

| File | Concern | Risk |
|------|---------|------|
| install-core.ps1 | Full installer logic in plaintext. Reveals IOCTL codes, quiesce protocol, registry paths, error handling strategy. | Medium - reverse-engineering aid |
| Setup.bat | Simple batch wrapper. Reveals PowerShell invocation pattern. | Low |
| Uninstall.bat | Simple batch wrapper. | Low |
| manifest.json | Package hashes. | None |
| install-manifest.json | Installed driver hashes. | None |
| devgen.exe / devcon.exe | Microsoft WDK tools. Bundled for functionality. | Low - well-known tools |
| .sys / .inf / .cat | Required for Windows driver install. | None - inherently exposed |
| AOControlPanel.exe | Compiled binary. | Low |

### Primary concern: install-core.ps1

This is ~900 lines of plaintext PowerShell that reveals:
- `IOCTL_AO_PREPARE_UNLOAD` code (0x0022A014)
- Control device symlink names (`\\.\AOCableA`)
- Quiesce protocol sequence
- Registry key paths and parameter names
- Health-check JSON schema and exit code contract
- Service lifecycle management strategy

---

## External Distribution: Do NOT Include

| Category | Files | Reason |
|----------|-------|--------|
| Source code | `Source/` | Proprietary |
| Documentation | `docs/` | Internal architecture, gap analysis, plans |
| Test scripts | `test_*.py`, `run_benchmark_suite.ps1` | Internal tooling |
| Build scripts | `build.bat`, `build-verify.ps1`, `build-manifest.json` | Build infra |
| Dev installer | `install.ps1`, `verify-install.ps1` | Developer-facing |
| Debug symbols | `*.pdb` | Debug info |
| Staging artifacts | `.tmp-stage-a/`, `.tmp-stage-b/` | Build intermediates |
| Results | `results/` | Test data |
| Package builder | `installer/build-installer.ps1` | Build infra |
| Git metadata | `.git/`, `.gitignore` | Source control |

---

## Minimum Distribution Set

The final user-facing package should contain only:

```
AOVirtualCable/
  Setup.exe              <- EXE bootstrapper (wraps Setup.bat + install-core.ps1)
  Uninstall.exe          <- optional separate uninstaller
  AOControlPanel.exe
  devgen.exe
  devcon.exe
  drivers/
    CableA/  aocablea.sys  aocablea.inf  aocablea.cat
    CableB/  aocableb.sys  aocableb.inf  aocableb.cat
```

**Key difference: no .ps1 or .bat files visible to user.**

---

## EXE Bootstrapper Strategy

### Why

- Hides PowerShell source from casual inspection
- Better UX: double-click .exe instead of .bat
- Can embed UAC manifest for automatic elevation
- Can embed icon and version info

### How

**Option A: Self-extracting archive (SFX)**
- 7-Zip SFX or WinRAR SFX
- Extracts to temp, runs Setup.bat, cleans up
- Pros: simple, no compilation
- Cons: still extracts .ps1 to temp briefly

**Option B: Compiled PowerShell wrapper**
- Use `ps2exe` or similar tool to compile install-core.ps1 to .exe
- Pros: single file, no .ps1 visible
- Cons: AV false positives with ps2exe

**Option C: Minimal C/C++ launcher EXE**
- Small native EXE that:
  1. Extracts embedded resource (install-core.ps1) to temp
  2. Launches PowerShell with -File pointing to temp copy
  3. Passes through exit code
  4. Deletes temp on exit
- Pros: clean, no AV issues, full control
- Cons: requires compilation

**Option D: Inno Setup / NSIS installer**
- Full installer framework with GUI
- Embeds all files and runs PowerShell as custom action
- Pros: professional UX, well-tested
- Cons: heavier, another dependency

### Recommendation

**Phase 1 (immediate):** Option A (SFX) for quick wins
**Phase 2 (production):** Option C or D for clean distribution

---

## Hardening Checklist

### Pre-distribution

- [ ] Remove all .ps1 and .bat from user-visible package (wrap in EXE)
- [ ] Remove manifest.json from user package (internal only)
- [ ] Strip debug info from .sys if present
- [ ] Verify no PDB files in package
- [ ] Verify no source/docs/tests in package
- [ ] Verify install-manifest.json is not pre-populated (written at install time)

### Package integrity

- [ ] Sign Setup.exe with code signing certificate
- [ ] All .sys and .cat files signed
- [ ] manifest.json hashes verified during build

### Installer UX

- [ ] Setup.exe auto-elevates (UAC manifest)
- [ ] No PowerShell window visible to user (hidden or minimized)
- [ ] Progress indication during install
- [ ] Clear success/failure message
- [ ] No "Press Enter" prompts on success

### Post-install

- [ ] Control Panel auto-starts
- [ ] verify-install equivalent runs silently
- [ ] Health-check returns exit 10

---

## Milestone Integration

This work maps to a new **M7: Release Hardening** milestone:

| Sub-milestone | Scope |
|--------------|-------|
| M7a: EXE bootstrapper | Wrap .ps1/.bat in native EXE, embed UAC manifest |
| M7b: Package cleanup | Remove internal files, strip debug info |
| M7c: Installer UX polish | Hidden PS window, progress bar, branded messages |

M7 depends on M6c (signing) for production distribution but can be
developed in parallel for the install UX portion.
