# Phase 7 Step 1: Telephony category metadata for capture endpoints

## Goal

VB-Cable declares `KSNODETYPE_ANY` for endpoints. AO Cable V1 inherits
that. Phone Link and similar communications apps may auto-prefer
endpoints declared as `KSNODETYPE_MICROPHONE` /
`KSNODETYPE_HEADSET_MICROPHONE` or with the
`PKEY_AudioEndpoint_FormFactor` set to a microphone form factor.

This step adds telephony-friendly metadata to the cable capture
endpoints **without** changing the architecture (still a virtual
cable, not a real microphone). Goal: improve auto-selection by Phone
Link / Teams / Discord without breaking VB-equivalent semantics.

## Planned Files

- `Source/Main/aocablea.inx` — capture endpoint AddReg block.
- `Source/Main/aocableb.inx` — same.

## Required Edits

Add to capture topology endpoint `AddReg` blocks:

```inf
HKR,EP\\0, %PKEY_AudioEndpoint_Association%,, %KSNODETYPE_MICROPHONE%
HKR,EP\\0, %PKEY_AudioEndpoint_FormFactor%, 0x00010001, 0x00000004 ; Microphone
```

(Exact key GUIDs and value types per Microsoft Learn audio endpoint
property reference.)

## Acceptance Criteria

- [ ] Build clean. INF re-stamps successfully.
- [ ] Install on a clean machine; AO Cable A capture endpoint shows up
      with the microphone icon in Sound Settings (visual sanity).
- [ ] Phone Link auto-selection: when AO Cable A capture is set as
      default communications microphone, Phone Link respects it.
- [ ] No regression in render endpoints.

## Completion

```powershell
python scripts/execute.py mark 7-quality-polish 1 completed --message "Capture endpoints declare microphone form factor."
```
