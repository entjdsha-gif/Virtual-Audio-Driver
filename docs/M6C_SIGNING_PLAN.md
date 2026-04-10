# M6c: Production Signing & Release Flow

## Problem

Current state: test-signed with self-created "AO Audio Test" certificate.
- Requires `bcdedit /set testsigning on` on every target PC
- Shows "publisher warning" dialog during install
- Secure Boot must be disabled
- Not suitable for distribution to end users

## Windows Driver Signing Requirements (2024+)

### Two paths to production signing:

#### Path A: EV Code Signing Certificate + Microsoft Attestation

1. **Purchase EV code signing certificate** (~$200-400/year)
   - DigiCert, Sectigo, GlobalSign, etc.
   - Must be an Extended Validation (EV) certificate
   - Stored on hardware token (USB)

2. **Register for Windows Hardware Developer Center**
   - https://partner.microsoft.com/dashboard/hardware
   - Requires EV certificate for account creation

3. **Submit driver package for attestation signing**
   - Upload .sys + .inf + .cat to Partner Center
   - Microsoft co-signs with their production certificate
   - Result: driver trusted on all Windows 10/11 without test signing

**Pros:** Full production trust, no Secure Boot issues
**Cons:** Annual certificate cost, hardware token dependency, submission turnaround time

#### Path B: Self-Signed with Manual Trust (Development/Internal)

1. **Create a proper self-signed certificate** (current approach, enhanced)
   - Generate a code signing certificate with longer validity
   - Export and distribute .cer file to target PCs

2. **Install certificate in Trusted Publishers store** on target PC
   - `certutil -addstore TrustedPublisher AoAudio.cer`
   - Removes "unknown publisher" warning
   - Still requires test signing mode

**Pros:** No cost, immediate
**Cons:** Still needs test signing, manual trust setup per PC

---

## Recommended Approach

### Phase 1: Enhanced Self-Signing (immediate, no cost)

Improve current flow so the installer experience is cleaner:

1. **Generate a proper certificate** with descriptive subject
   ```powershell
   New-SelfSignedCertificate -Type CodeSigningCert `
       -Subject "CN=AO Audio, O=AO Audio, L=Seoul, C=KR" `
       -CertStoreLocation Cert:\CurrentUser\My `
       -NotAfter (Get-Date).AddYears(5)
   ```

2. **Export .cer and bundle with installer**
   - `installer/AOAudio.cer`

3. **Installer auto-installs certificate** to Trusted Publishers
   ```powershell
   certutil -addstore TrustedPublisher "$scriptDir\AOAudio.cer"
   ```
   - Removes "unknown publisher" dialog
   - One-time per PC, persists across reinstalls

4. **Sign .cat files** (not just .sys)
   - Currently: .sys signed, .cat signing attempted
   - Ensure .cat is always generated and signed

### Phase 2: Microsoft Attestation Signing (production release)

When ready for public distribution:

1. Purchase EV certificate
2. Register Partner Center account
3. Create submission pipeline:
   ```
   build -> sign .sys with EV cert -> generate .cat -> 
   submit to Partner Center -> download Microsoft-signed package ->
   bundle into installer
   ```
4. Remove test signing requirement from installer
5. Works with Secure Boot enabled

---

## Current Signing Flow (test)

```
build.bat
  -> MSBuild compiles aocablea.sys / aocableb.sys
  -> No signing at build time

install.ps1 -Action install
  -> New-StagedDriverPackage
     -> stampinf stamps .inf from .inx
     -> signtool signs .sys with "AO Audio Test" cert
     -> inf2cat generates .cat
     -> signtool signs .cat
  -> pnputil /add-driver /install
  -> bcdedit /set testsigning on (if not already)
```

## Proposed Signing Flow (Phase 1)

```
build-installer.ps1
  -> Collect pre-built .sys, pre-stamped .inf
  -> Sign .sys with enhanced self-signed cert
  -> Generate and sign .cat
  -> Bundle AOAudio.cer in package

installer/install-core.ps1
  -> certutil -addstore TrustedPublisher AOAudio.cer
  -> pnputil /add-driver /install
  -> No "unknown publisher" warning
  -> test signing still required (bcdedit)
```

## Proposed Signing Flow (Phase 2 - Production)

```
build-release.ps1
  -> Collect pre-built .sys, pre-stamped .inf
  -> Sign .sys with EV certificate
  -> Generate and sign .cat with EV certificate
  -> Submit to Microsoft Partner Center
  -> Download Microsoft-attested package
  -> Bundle into installer

installer/install-core.ps1
  -> pnputil /add-driver /install
  -> No publisher warning
  -> No test signing required
  -> Works with Secure Boot
```

---

## Release Checklist

### Pre-release
- [ ] All M1-M5 tests pass
- [ ] verify-install.ps1: 17/17 PASS
- [ ] test_ioctl_diag.py: ALL PASSED
- [ ] Benchmark suite: AO 4/4 PASS
- [ ] No known regressions in VALIDATION_MATRIX.md

### Signing
- [ ] Certificate generated/renewed
- [ ] .sys signed
- [ ] .cat generated and signed
- [ ] Certificate bundled in installer (Phase 1) or Microsoft-attested (Phase 2)

### Packaging
- [ ] build-installer.ps1 succeeds
- [ ] Package contains: Setup.bat, Uninstall.bat, install-core.ps1, drivers/, devgen.exe, devcon.exe
- [ ] manifest.json hashes verified
- [ ] Clean-PC fresh install test: PASS
- [ ] Upgrade test (from previous version): PASS
- [ ] Uninstall test: PASS

### Distribution
- [ ] Package zipped or wrapped in SFX
- [ ] README with prerequisites (test signing for Phase 1)
- [ ] Version tag in git

---

## Signing Prerequisites Summary

| Item | Phase 1 (Self-Signed) | Phase 2 (Production) |
|------|----------------------|---------------------|
| Certificate | Self-signed (free) | EV cert (~$300/yr) |
| Partner Center | Not needed | Required |
| Test signing | Required | Not needed |
| Secure Boot | Must disable | Works |
| Publisher warning | Removed (cert in Trusted Publishers) | Removed (Microsoft signature) |
| Distribution | Internal/dev | Public |
