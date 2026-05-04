# G7 — Routing vs Endpoint Property Read-Only Notes

**Branch:** `feature/ao-fixed-pipe-rewrite` @ 2c733f1 + uncommitted B1 + G2 + G6
**Date:** 2026-04-14
**Scope:** Read-only comparison of AO Cable vs VB-Cable endpoint declarations and
runtime attachment evidence. No code changes, no INF changes, no proposals beyond
§ 6. Triggered by the G6 finding that AO Cable pipes had "producer without consumer
and consumer without producer" during a live Phone Link call — strong signal that
Phone Link did not attach to AO endpoints at all.

---

## § 1. Intended routing (from harness source)

Reading [tests/live_call/audio_router.py](../tests/live_call/audio_router.py) and
[tests/live_call/run_test_call.py](../tests/live_call/run_test_call.py):

- AO profile intended wiring (per CLAUDE.md + audio_router docstring):
  - System default **Playback** → AO Cable A (render side)
  - System default **Recording** → AO Cable B (capture side)
  - Phone Link specifically routed via per-app to match the profile
- `audio_router._set_default_audio_device()` calls `IPolicyConfig::SetDefaultEndpoint`
  for **all three roles: eConsole (0), eMultimedia (1), eCommunications (2)**
  ([audio_router.py:150-151](../tests/live_call/audio_router.py#L150-L151)). So the
  harness DOES explicitly try to make AO the communications default.
- SoundVolumeView background loop reapplies per-app routing to Phone Link processes
  (`PhoneExperienceHost.exe`, `CrossDeviceService.exe`, `CrossDeviceResume.exe`,
  `YourPhoneAppProxy.exe`).

**Takeaway:** harness intent is correct. If the OS honored the
`SetDefaultEndpoint` call for the `eCommunications` role, and if Phone Link used
the communications default, the wiring would close. The broken link is somewhere
between "harness asks Windows to make AO the default" and "Phone Link actually
uses AO".

---

## § 2. Observed runtime attachment (from G6 log)

Source: [results/g6_runtime/g6_pairing.log](g6_runtime/g6_pairing.log).

During the ~27 s active call phase (t = 4.76 – 27.02):

| Cable | Speaker (WR) state | Mic (RD) state |
|---|---|---|
| A | **Not active** until t=62.31 (post-call) | `RdF/s ≈ 48000`, `dUnderrun ≈ 48000/s`, `Fill=0` — reader drains an empty pipe |
| B | `WrF/s ≈ 47000`, `Fill` climbs to `191952 / 192000`, `dDrop = 51801` at t=24.66 — writer fills pipe, nobody reads | **Not active** until t=84.01 (post-call) |

Format-pair probe (G6 on-change) showed clean `48k/16/2/SameRate=1/copyCh=2/PipeCh=8`
on every slot that was exercised. Zero mid-call transitions.

**Interpretation:** neither cable had a complete producer-consumer pair during the
call. Only the harness's own AI playback (Cable B SPK) and its own capture (Cable A
MIC) touched AO endpoints. Phone Link's call audio (remote speech → cable → harness,
and harness AI → cable → call mic input) never hit AO at all. Phone Link chose some
other device — the system's prior communications default, a physical endpoint, or
whatever Windows returned when asked for `eCommunications` default.

This reframes "garbled / silent real call" as **harness-only audio trapped in a
half-open pipe**, not as driver-side distortion. G6 eliminates format-pair mismatch
and rate mismatch as root causes.

---

## § 3. AO INF / endpoint property observations

Source: [Source/Main/aocablea.inx](../Source/Main/aocablea.inx),
[Source/Main/aocableb.inx](../Source/Main/aocableb.inx).

Both files declare on every endpoint (Speaker Topology AND Mic Topology) :

```inf
HKR,EP\0,%PKEY_AudioEndpoint_Association%,,%KSNODETYPE_ANY%
HKR,EP\0,%PKEY_AudioEndpoint_Supports_EventDriven_Mode%,0x00010001,0x1
HKR,EP\0,%PKEY_AudioDevice_NeverSetAsDefaultEndpoint%,0x00010001,0x00000003
```

String definition:
```inf
PKEY_AudioDevice_NeverSetAsDefaultEndpoint = "{F3E80BEF-1723-4FF2-BCC4-7F83DC5E46D4},3"
```

### 3.1 The `NeverSetAsDefaultEndpoint` flag

The property identifier `{F3E80BEF-1723-4FF2-BCC4-7F83DC5E46D4},3` is the
Windows-documented property **`PKEY_AudioDevice_NeverSetAsDefaultEndpoint`**.
Its value is a bitmask with (per Microsoft's audio endpoint property documentation):

- bit 0 (`0x1`) = do not auto-select for `eConsole` / `eMultimedia` role
- bit 1 (`0x2`) = do not auto-select for `eCommunications` role

**The AO declaration sets this to `0x00000003` — both bits set — on EVERY endpoint
(Cable A Speaker, Cable A Mic, Cable B Speaker, Cable B Mic)**. Total of four
endpoints, all flagged "never auto-select for any role".

Hits:
- [aocablea.inx:70](../Source/Main/aocablea.inx#L70) — Speaker Topology
- [aocablea.inx:86](../Source/Main/aocablea.inx#L86) — Mic Topology
- [aocableb.inx:70](../Source/Main/aocableb.inx#L70) — Speaker Topology
- [aocableb.inx:86](../Source/Main/aocableb.inx#L86) — Mic Topology

### 3.2 Other AO endpoint properties set on EP\0

On the topology filter's EP\0 only (not wave filter), these three properties:
- `PKEY_AudioEndpoint_Association = KSNODETYPE_ANY` — generic association (benign)
- `PKEY_AudioEndpoint_Supports_EventDriven_Mode = 1` — event-driven WASAPI supported
- `PKEY_AudioDevice_NeverSetAsDefaultEndpoint = 3` — **the smoking gun**

No `PKEY_AudioEngine_OEMFormat`, no `PKEY_AudioEndpoint_FormFactor`, no
`PKEY_AudioEndpoint_JackSubType` declarations.

---

## § 4. AO vs VB differences

Source for VB: [C:\Windows\INF\oem55.inf](file:///C:/Windows/INF/oem55.inf) (VB Cable A,
installed copy of `vbaudio_cable_a.inx` v3.3.1.9). Confirmed by `pnputil /enum-drivers`.

### 4.1 Driver registration class

| Field | AO | VB |
|---|---|---|
| Class | MEDIA | MEDIA |
| Provider (signer) | `AO Audio Test` (test-signed) | `Microsoft Windows Hardware Compatibility Publisher` (WHQL attested) |
| Driver attribute | `Legacy` | `Universal, Attested` |
| INF version | `1.0.0.1` (2016) | `3.3.1.9` (2025-02-25) |
| PnpLockdown | 1 | 1 |

Both are MEDIA class. The `Universal + Attested` status for VB makes it a first-class
modern audio driver. `Legacy + Test-Signed` for AO is expected for an in-development
driver and is NOT itself a blocker — test-signing mode still loads the driver and the
G6 log confirms all four endpoints are fully functional as audio devices. The user
experience difference is not "driver refuses to load" but "Phone Link refuses to
attach to it".

### 4.2 Endpoint property declarations (core of the finding)

| Property on EP\0 | AO Cable A/B | VB Cable A |
|---|---|---|
| `PKEY_AudioEndpoint_Association` | `KSNODETYPE_ANY` | `KSNODETYPE_ANY` (same) |
| `PKEY_AudioEndpoint_Supports_EventDriven_Mode` | `1` | *(not declared)* |
| **`PKEY_AudioDevice_NeverSetAsDefaultEndpoint`** | **`0x3`** (both roles blocked) | ***NOT DECLARED*** |
| `PKEY_AudioEngine_OEMFormat` | *(not declared)* | **48 kHz / 24-bit / stereo default wave format** |

Also in VB and NOT in AO:
- `OEMSettingsOverride.AddReg` applied to both render and capture topologies
- Two Topology registrations: `EP\0` and `EP\1`, each with `PKEY_AudioEngine_OEMFormat`
  (16299 NTamd64 + 16299 NTARM64 paths)

And in AO and NOT in VB:
- `PKEY_AudioDevice_NeverSetAsDefaultEndpoint = 3` (explicitly blocks default auto-select)
- `PKEY_AudioEndpoint_Supports_EventDriven_Mode = 1` (informational, not a blocker)
- `DeviceType = 0x0000001D` under the AudioHw subkey (legacy driver hardware type)
- DRM `[SignatureAttributes]` section with `DRMLevel=1300` and `PETrust=true`

### 4.3 KS category registration

AO [aocablea.inx:103-112](../Source/Main/aocablea.inx#L103-L112):
```
AddInterface=KSCATEGORY_AUDIO,    WaveCableASpeaker,     …
AddInterface=KSCATEGORY_RENDER,   WaveCableASpeaker,     …
AddInterface=KSCATEGORY_REALTIME, WaveCableASpeaker,     …
AddInterface=KSCATEGORY_AUDIO,    TopologyCableASpeaker, …
AddInterface=KSCATEGORY_TOPOLOGY, TopologyCableASpeaker, …
AddInterface=KSCATEGORY_AUDIO,    WaveCableAMic,         …
AddInterface=KSCATEGORY_REALTIME, WaveCableAMic,         …
AddInterface=KSCATEGORY_CAPTURE,  WaveCableAMic,         …
AddInterface=KSCATEGORY_AUDIO,    TopologyCableAMic,     …
AddInterface=KSCATEGORY_TOPOLOGY, TopologyCableAMic,     …
```

VB [oem55.inf:165-179](file:///C:/Windows/INF/oem55.inf):
```
AddInterface=KSCATEGORY_AUDIO,    WaveRender1,     …
AddInterface=KSCATEGORY_REALTIME, WaveRender1,     …
AddInterface=KSCATEGORY_RENDER,   WaveRender1,     …
AddInterface=KSCATEGORY_AUDIO,    TopoRender1,     …
AddInterface=KSCATEGORY_TOPOLOGY, TopoRender1,     …
AddInterface=KSCATEGORY_AUDIO,    WaveCapture1,    …
AddInterface=KSCATEGORY_REALTIME, WaveCapture1,    …
AddInterface=KSCATEGORY_CAPTURE,  WaveCapture1,    …
AddInterface=KSCATEGORY_AUDIO,    TopoCapture1,    …
AddInterface=KSCATEGORY_TOPOLOGY, TopoCapture1,    …
```

**Identical category set on both sides.** AO and VB both register
AUDIO + REALTIME + RENDER (or CAPTURE) + TOPOLOGY for every endpoint. There is no
missing KS category on AO.

### 4.4 Service install path

- AO uses `ROOT\AOCableA` hardware ID with `AddService=AOCableA` → KMDF driver.
- VB uses `VBAudioVACAWDM` hardware ID on `NTamd64.10.0...16299` with
  `KmdfService=VBAudioVACAMME`.

Both ultimately KMDF kernel drivers that implement KS audio filters. Both are valid
WDM audio endpoints from the OS's enumeration perspective. G6 already proved AO's
endpoints are fully opened when someone (even just the harness) asks for them.

### 4.5 Encoding

AO `.inx` files are **UTF-16 LE**. VB installed `.inf` is plain ASCII. This is a
stylistic difference and does not affect registration semantics — the Windows
`pnputil` has successfully ingested AO's INF as evidenced by the G4 B1 install run.

---

## § 5. Strongest supported hypothesis

> **AO Cable is actively opted out of being the default Communications endpoint by
> its own INF, via `PKEY_AudioDevice_NeverSetAsDefaultEndpoint = 0x3`, on all four
> endpoints (both cables, Speaker + Mic). VB-Cable does not set this property. Phone
> Link uses the Windows Communications default endpoint for call audio. Because
> AO's endpoints declare "never auto-select me for Communications role", Windows's
> automatic selection skips AO, and Phone Link falls back to whatever device Windows
> considers the valid Communications default (typically a physical device). This
> explains every G6 observation: AO cables are touched only by the test harness's
> own streams, never by Phone Link; both pipes sit half-open; the "garbled" call
> quality the user reports is in fact the remote party hearing silence (no mic
> input from the harness reaching Phone Link) and the harness hearing silence (no
> remote audio from Phone Link reaching it). — strongly supported, structural.**

Framing rules applied:
- **Confirmed** (direct source evidence): AO INX files contain the property on all
  four endpoints; VB installed INF does not contain the property on any endpoint.
- **Strongly supported, structural** (inferred mechanism): Phone Link's selection
  of Communications default is influenced by this property, producing the
  "half-open pipe" runtime signature we observed.
- **Not confirmed**: we have not traced Phone Link's source / telemetry to prove
  the exact API call chain by which the flag filters AO out. But the correlation
  between (flag set) ↔ (AO not attached at runtime) ↔ (audio broken) is total, and
  the reverse for VB (no flag) ↔ (VB attached) ↔ (audio clean) is also total.

Auxiliary observations (not root cause but may matter):
- AO's `Legacy + Test-Signed` status could additionally bias some comms-aware
  apps (e.g., Teams, Zoom) to deprioritize it, even after the flag is removed.
  This is a secondary concern — fix the flag first, see if it closes the gap.
- AO's missing `PKEY_AudioEngine_OEMFormat` means Windows chooses a default wave
  format without an OEM hint. This can cause format negotiation surprises but
  G6 already showed the active format is clean `48k/16/2`, so this is not the
  current pain point.

---

## § 6. Next action

**Scope for the next gate:** INF-only fix, zero driver source-code change.

Concrete plan (proposal to be written next session, not in this notes doc):

1. Remove (or set to `0x0`) line 70 and line 86 of `aocablea.inx`.
2. Same in `aocableb.inx`.
3. Rebuild, reinstall via `install.ps1 -Action upgrade`, verify with build-verify.
4. Confirm with PowerShell / pycaw that after install, Windows Sound Panel shows
   AO cables under both **"Playback"** and **"Recording"** tabs, AND that they
   can be selected as the **default communications device** by the system rather
   than only manually.
5. Re-run `run_test_call.py` AO profile. Expected outcomes:
   - G6 log should show all four slots active during the call: both cables, both
     directions. Not just the harness's half.
   - User should perceive clean audio on the phone.
6. If the call is clean, **do not commit** the INF change yet. First reason:
   the working tree still carries B1 + G2 + G6 instrumentation. Second reason:
   the INF fix should be committed on its own branch or isolated from the
   instrumentation. Sequence: land INF fix → remove instrumentation → commit
   both as separate commits.

**Do not add** `PKEY_AudioEngine_OEMFormat`, `FormFactor`, or any other VB-style
extras in the first pass. Keep the delta to the minimal change. One property
removal per file, four lines total across two files. Smallest possible fix for
the strongest supported hypothesis.

**Phase 6 stays BLOCKED.** B1 / G2 / G6 stay uncommitted. Driver code is not
touched in this branch of work. The unblock condition is: after the INF fix,
re-run G6 measurement and confirm both cables show proper producer-consumer
pairing during a Phone Link call.

---

## § 7. Open questions (carried into the next gate)

1. **Does `NeverSetAsDefaultEndpoint` affect manual `SetDefaultEndpoint` calls?**
   The audio_router.py harness calls `IPolicyConfig::SetDefaultEndpoint` for all
   three roles. If the flag also suppresses explicit `SetDefaultEndpoint` success,
   the fix requires flag removal. If the flag only suppresses *automatic*
   selection but not explicit calls, there may be a second layer (e.g., Phone
   Link re-enumerating devices on its own).
2. **Does Phone Link cache the Communications endpoint at startup, or per-call?**
   If cached at startup, the post-INF-fix test must restart Phone Link so it
   re-queries the new default.
3. **Does removing the flag alone make AO appear under Sound Panel "Communications"
   tab?** The communications device dropdown in Windows 10/11 is distinct from
   the regular playback/recording dropdown. If AO still doesn't appear there
   after the flag is removed, there may be an additional filter (e.g.,
   `PKEY_Device_DevType` or a missing communications-specific category).
4. **Is the Test-Signed + Legacy status of AO itself a communications filter
   blocker in some Windows builds?** Not expected on Test Signing enabled systems,
   but worth noting. If the INF fix doesn't close the gap, the next hypothesis is
   "driver signature level filters out AO from Phone Link's allowed device list".
5. **Do Teams / Discord / Zoom behave the same way as Phone Link on AO?** If yes,
   the fix is general. If no, Phone Link has an additional filter beyond the
   documented property.

---

## § 8. Operating state

- `results/g7_routing_vs_endpoint_notes.md` = this file, read-only output of G7.
- No code changes in this gate.
- No INF changes in this gate.
- No commits.
- B1 / G2 / G6 instrumentation remains uncommitted in the working tree.
- Phase 6 stays BLOCKED until the next gate (INF fix + re-measurement) resolves.
- Wording of results stays at "strongly supported, structural" for the mechanism
  and "confirmed" only for the direct source-evidence observations (flag present
  in AO, absent in VB).
