# Phase 0 Step 4/5 — KDNET Two-Machine Runbook

Date: 2026-04-13
Depends on: `results/phase0_findings.md` (Step 1-3 PASS, Step 4 deferred)
Target topology:

- **Host**: this PC (repo, build, WinDbg, analysis). Windows 11 Pro. Build artifacts live at `D:\mywork\Virtual-Audio-Driver\x64\Release\`. WinDbg classic at `C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\windbg.exe`. `kdnet.exe` present at same dir.
- **Target**: separate physical PC. Will run AO + Phone Link + paired phone + real call. Hyper-V path is **suspended** by decision; same-PC local kd is **discarded** by platform limitation.

The gate: record runtime hit counts for `CMiniportWaveRTStream::GetPosition` / `::GetPositions` on `aocablea` + `aocableb` during a 5-second Phone Link AO-profile call on the target. Exit criterion: **≥100 combined hits and a single dominant entrypoint recorded as the Phase 3 hook target**. Step 5 (live call baseline quality) runs as the same call — clean / garbled / silent / mixed.

---

## Stage 1. Host KDNET preparation

On the host (this PC), once per machine:

### 1.1. Pick a transport
KDNET over Ethernet is the simplest two-box transport. Prerequisites:

- Target has a KDNET-supported NIC. Vast majority of modern Intel/Realtek/Broadcom onboard Ethernet works; USB3 Ethernet adapters are also viable with the USB3 debug transport (different config path, not covered here).
- Both machines reachable on the same L2 segment (same switch / crossover / direct Ethernet). Routing through a WiFi NAT is possible but pinning a static host IP on the wired interface is cleaner.
- Firewall: inbound UDP on the chosen KDNET port (default suggestion: **50000**). Host listens; target connects out to host.

### 1.2. Pin host IP for the KDNET interface
KDNET key binds to the host IP the target was configured against. If the host IP changes, the link breaks. Options:

- Static IPv4 on the wired NIC used for debugging, e.g. `10.0.10.1/24`.
- Or document the current DHCP lease and refresh `kdnet` on the target when it changes.

### 1.3. Open firewall port on host
Elevated PowerShell:
```powershell
New-NetFirewallRule -DisplayName "AO KDNET Inbound" `
    -Direction Inbound -Protocol UDP -LocalPort 50000 -Action Allow
```

### 1.4. Symbol setup (host side)
Host WinDbg symbol search path should include:

- Microsoft symbol server: `srv*C:\Symbols*https://msdl.microsoft.com/download/symbols`
- Local AO build PDBs: `D:\mywork\Virtual-Audio-Driver\x64\Release\CableA;D:\mywork\Virtual-Audio-Driver\x64\Release\CableB`

Exact PDB paths confirmed on this host:
- `x64/Release/CableA/aocablea.pdb`
- `x64/Release/CableB/aocableb.pdb`

Command to set for one session:
```
.sympath srv*C:\Symbols*https://msdl.microsoft.com/download/symbols;D:\mywork\Virtual-Audio-Driver\x64\Release\CableA;D:\mywork\Virtual-Audio-Driver\x64\Release\CableB
.symfix+ C:\Symbols
.reload /f
```

### 1.5. Verify kdnet availability
```powershell
& "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\kdnet.exe" -verbose
```
Run it with no args first on the **target** (Stage 3.3) to enumerate supported NICs. On the host, `kdnet.exe` is only needed as a reference for the tooling; the host side does not run it for attach.

---

## Stage 2. Target artifacts to copy from host

Package these into one folder (e.g. a USB stick, shared SMB drop, or `scp`-equivalent) and move to the target:

```
<DROP>/
├── install.ps1                             # from repo root
├── build-verify.ps1                        # from repo root (sanity only on target)
├── verify-install.ps1                      # from repo root (if present; optional)
├── x64/
│   └── Release/
│       ├── CableA/
│       │   ├── aocablea.sys
│       │   ├── aocablea.inf
│       │   └── aocablea.pdb
│       └── CableB/
│           ├── aocableb.sys
│           ├── aocableb.inf
│           └── aocableb.pdb
├── Source/
│   └── Main/x64/Release/VirtualAudioDriver.sys   # main driver (referenced by some install paths)
└── test_ioctl_diag.py                     # for on-target IOCTL smoke
```

Notes:

- **PDBs are mandatory** for the host to resolve `CMiniportWaveRTStream::GetPosition` / `::GetPositions` symbols. PDBs go to the target as part of the drop, but the host reads them from the host copy via `.sympath` — so the critical copy is that **both host and target carry matching sys+pdb pairs** produced from the same build. Easiest: ship one drop and keep the host's own `x64/Release/` untouched — they already match.
- `install.ps1` auto-finds `devcon.exe` under `C:\Program Files (x86)\Windows Kits\10\Tools\...`. If the target is a fresh machine without WDK Tools, devcon is optional — the script falls back to `pnputil`. Confirmed via `install.ps1:381-387` and `if ($devcon)` guards.
- `tests/live_call/` (Phone Link harness) is **not** needed on the target for Step 4/5 if the target is where the phone is paired and you dial manually from the phone. If you want to use the automated harness on the target, copy `tests/live_call/` along with a configured `.env`.

---

## Stage 3. Target-side install + KDNET config

Order matters: install AO **before** enabling kernel debug so the normal install path is proven on the target first.

### 3.1. Prerequisites on target
- Windows 10/11 x64, test signing capable.
- Admin account.
- Phone Link installed, phone paired, Bluetooth HFP working. Validated by making a normal call from phone or PC side before touching anything.
- Copy the drop folder from Stage 2.

### 3.2. Enable test signing and install AO
Elevated PowerShell on target:
```powershell
bcdedit /set testsigning on
# Reboot once; after reboot verify "Test Mode" watermark on desktop
shutdown /r /t 0
```
After reboot, elevated PowerShell again in the drop folder:
```powershell
cd <DROP>
.\install.ps1 -Action upgrade
python test_ioctl_diag.py
```
Expected: install completes (`INSTALL_EXIT=0`), IOCTL diagnostic passes for both cables. This mirrors host Steps 2-3.

### 3.3. Enumerate NICs for KDNET
Elevated cmd:
```
"C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\kdnet.exe"
```
(No args). Output lists NICs and marks which are supported. Pick the wired Ethernet NIC that shares the debug segment with the host.

### 3.4. Configure KDNET against host
Elevated cmd:
```
"C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\kdnet.exe" <HOST_IP> 50000
```
Replace `<HOST_IP>` with the host's pinned IP (Stage 1.2). Output will look like:
```
Key=1.2.3.4
Debugger must connect with:
    -k net:port=50000,key=1.2.3.4
```
**Copy the full `-k net:...` line. This is the only piece the host needs.** Do not paste the key into chat unless the target is dedicated test hardware.

Then:
```
bcdedit /debug on
bcdedit /dbgsettings
shutdown /r /t 0
```
`bcdedit /dbgsettings` should confirm `Net` transport and the port/key just set.

### 3.5. Target behavior on next boot
Target stops very early during boot until the host's WinDbg attaches. This is expected — the target is waiting. Proceed to Stage 4 immediately.

---

## Stage 4. Host WinDbg attach

On host, launch WinDbg with the exact `-k` string produced by Stage 3.4:

```powershell
& "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\windbg.exe" -k net:port=50000,key=<KEY_FROM_TARGET>
```

Initial state in the Command window:
```
.sympath srv*C:\Symbols*https://msdl.microsoft.com/download/symbols;D:\mywork\Virtual-Audio-Driver\x64\Release\CableA;D:\mywork\Virtual-Audio-Driver\x64\Release\CableB
.symfix+ C:\Symbols
.reload /f
g
```
`g` releases the target from the boot-time debugger stop; target finishes booting into Windows.

Verification before the call:
```
lm m aocable*
```
Must show both `aocablea` and `aocableb` loaded with symbols (not `(deferred)`). If modules appear without symbols, re-run `.reload /f aocablea.sys` / `.reload /f aocableb.sys`. If symbols still fail, stop here — PDB/sys mismatch between host and target. Rebuild/recopy.

Symbol probe:
```
x aocablea!*GetPosition*
x aocablea!*GetPositions*
x aocableb!*GetPosition*
x aocableb!*GetPositions*
```
At least one `CMiniportWaveRTStream::GetPosition` (and separately `::GetPositions`) must resolve per cable. Record the exact mangled/demangled symbol names here for the next step.

---

## Stage 5. Step 4 — counters + breakpoints

With target booted and symbols resolved:

```
.bpcmds     ; optional: show nothing before we start
bc *        ; clear any stale bp
r @$t0 = 0
r @$t1 = 0
r @$t2 = 0
r @$t3 = 0

bu aocablea!CMiniportWaveRTStream::GetPosition  "r @$t0 = @$t0 + 1; gc"
bu aocablea!CMiniportWaveRTStream::GetPositions "r @$t1 = @$t1 + 1; gc"
bu aocableb!CMiniportWaveRTStream::GetPosition  "r @$t2 = @$t2 + 1; gc"
bu aocableb!CMiniportWaveRTStream::GetPositions "r @$t3 = @$t3 + 1; gc"

bl
```
`bl` must show four breakpoints, all `e` (enabled) — not `u` (unresolved). If any are `u`, the symbol resolved at `x` time but not now → re-reload with `.reload /f` and repeat.

Release the target:
```
g
```

**Rule**: do not break in during the call. Only break after the call ends. Per user directive — counters only, no stack dumps, no printf noise.

---

## Stage 6. Step 5 — same call is the live-quality evidence

On the target:
1. Confirm default audio device mapping sends call output to the AO cable (same as host `tests/live_call/.env` AO profile).
2. Place a real Phone Link call from the target (or from the paired phone to the target — whichever the test harness normally uses). Let the call connect and run audio for **at least 5 seconds**; longer is fine.
3. End the call normally.
4. User notes the subjective quality on the target side: **clean / garbled / silent / mixed**. This becomes the Step 5 baseline entry.

Host stays hands-off during the call. The target's kernel is running freely under the attached debugger; `bu` with `gc` accumulates counters without stopping execution.

---

## Stage 7. Break in + read counters

After the call ends, from the host WinDbg command window, Ctrl+Break (or Debug → Break).

```
? @$t0
? @$t1
? @$t2
? @$t3
```

Record all four values exactly. Also record:
- Wall-clock duration of the call segment where audio was actually flowing (needed to translate counts → rate).
- Which of the four counters dominates (this determines the Phase 3 hook target).

Clear state:
```
bc *
g
```
`g` returns the target to normal running state. Detach or leave attached for further experiments as you like.

---

## Stage 8. Gate evaluation

Apply the Codex Phase 0 exit criterion to the four counters:

| Condition | Result | Next action |
|---|---|---|
| At least one counter ≥ ~100 over ~5 seconds of audio | **PASS** | Record dominant function as Phase 3 hook target. Unblock Phase 1. |
| Combined < 100 but > 0 | **PASS with caution** | Record rate. If rate indicates < 20 Hz per stream, flag as "low frequency — plan for Phase 3 tolerance"; still unblock. |
| All four = 0 | **FAIL** | portcls is calling a different entrypoint. **Stop.** Re-check `lm`/`x` on `CMiniportWaveRTStream::GetPositionRegister` / `::GetPresentationPosition` and any other vtable slot candidates. Update `phase0_findings.md` and Codex plan Phase 3 wiring before proceeding. |
| Only one cable non-zero | **Partial** | Investigate whether the active cable during the call was the one observed. Re-run with both cables explicitly exercised (render + capture). |

Update `results/phase0_findings.md` Step 4 section with:
- Transport: KDNET two-box (target hostname/MAC optional, no secrets).
- Duration of measured window.
- Four counter values.
- Dominant entrypoint (module + full symbol).
- Baseline quality for Step 5.
- Decision: Phase 0 gate **CLOSED / FAILED**.

Commit the updated findings as the gate-close checkpoint.

---

## Appendix A. Safety rules (do not skip)

- No `bp`. Always `bu` so the breakpoint re-binds across `.reload`.
- No `.printf`, no `kv`, no `!process`, no stack dumps inside the bp command — those serialize the kernel at DPC/IRQL and will distort the very timing we are measuring.
- Never leave the target broken-in during a live call. Break in only after the call ends.
- Do not set a bp on `UpdatePosition()` or any inner helper in this round — Step 4's entire purpose is to observe the **public portcls entrypoint** the rewrite will hook into. Inner-helper instrumentation is Phase 1 work.
- If the target crashes under the debugger, capture `!analyze -v` output and save to `results/phase0_target_bugcheck.md` before resetting.

## Appendix B. If KDNET refuses to connect

- First boot after `kdnet` config: target hangs at a black screen with "Waiting for debugger" text visible only if COM debugging is on (net transport is silent). Assume it is waiting and attach from host.
- Verify host firewall accepted the inbound UDP rule (Stage 1.3).
- Verify the key string matches exactly — one character wrong and the host sees no traffic.
- `ping` from host to target and from target to host on the debug interface. If ping fails, KDNET will also fail.
- NIC mismatch: rerun `kdnet.exe` on target, this time with `-i <nic_index>` to force the right interface if auto-pick chose a disconnected one.

## Appendix C. Phone Link on target — known prerequisites

- Target must already have Phone Link set up and the phone paired. Step 4/5 is not the place to debug Phone Link pairing.
- Bluetooth HFP should route cleanly before AO is loaded. Reference quality: unchanged vs. VB-Cable (which we know is clean).
- `AUDIO_CABLE_PROFILE=ao` must apply on the target — either via the `tests/live_call/.env` if the harness is copied, or by setting Windows default devices manually to the AO cable.
