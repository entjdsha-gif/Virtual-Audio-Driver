# AO Cable V1 PRD

Status: accepted
Date: 2026-04-25

## 1. Product Summary

**AO Cable V1** is a Windows virtual audio cable kernel driver. Same product
category as VB-Cable: a transparent pipe that lets one application's audio
output become another application's microphone input on the same PC.

It exposes two independent cable pairs:

```text
AO Cable A Render  ->  AO Cable A Capture
AO Cable B Render  ->  AO Cable B Capture
```

Each pair is a render endpoint plus a capture endpoint that share an internal
ring. Frames written to the render endpoint come out of the capture endpoint
in the same order, with format conversion when the requested format does not
match the internal ring format.

## 2. Identity

AO Cable V1 follows the **VB-Cable identity**: simple, stable, low-overhead,
no hidden processing, predictable on long live calls.

What that means concretely:

- single producer / single consumer per cable
- one canonical transport owner per cable
- no mixing, no APO/DSP/AGC/EQ/limiter/noise suppression
- no volume / mute control inside the driver
- internal sample-rate conversion only when client format does not match the
  internal ring; otherwise pass-through
- frame counts are the unit of truth (`ms` is comments / UI / logs only)
- failure modes (overflow, underrun, late tick) are counted, not hidden

What AO Cable V1 adds over VB-Cable in the same identity space:

- two cable pairs (A and B), enabling split-routing scenarios
- official code signing path
- repeatable build / install / no-reboot upgrade tooling
- integrated diagnostics IOCTL for observability
- live-call quality measurement harness (Phone Link + OpenAI Realtime)

## 3. Problem

Two practical use cases drove the rewrite:

1. **Phone Link uplink** — route PC audio (TTS, AI voice, music, anything)
   into Phone Link as the call microphone. The audio that the remote party
   hears must be clean, not garbled.
2. **App-to-app audio routing** — let one PC application's output become
   another PC application's microphone input without a physical loopback
   cable, an external mixer, or VB-Cable.

The previous AO Cable design (telephony-V1, then pipeline-V2) failed both
cases because of structural problems in the cable transport core: packed
24-bit ring, 4-stage conversion pipeline, MicSink dual-write, FormatMatch
enforcement, and timer-vs-query split ownership during Phase 5/6. Live-call
quality on AO was garbled while VB-Cable on the same path was clean.

V1 (this rewrite) replaces the cable transport core with the verified
VB-Cable pattern: INT32 frame-indexed ring, single-pass linear-interp SRC,
hard-reject overflow, canonical advance helper.

## 4. Goal

AO Cable V1 must:

1. Accept render-side PCM (8/16/24/32-bit, 8000..192000 Hz, mono/stereo) and
   deliver it through the capture endpoint with the same frame ordering and
   no avoidable distortion.
2. Match VB-Cable's live-call quality on the Phone Link + OpenAI Realtime
   path — measured by user judgment (clean / garbled / silent) and STT
   transcription accuracy.
3. Behave predictably for long live calls (10+ minutes) without cumulative
   timing drift or progressive degradation.
4. Coexist with VB-Cable on the same machine without conflict.

This is a **driver-internal** guarantee. Windows Audio Engine, Phone Link,
the phone OS, Bluetooth, cellular/VoIP codecs, and the remote network path
may still apply processing outside AO Cable V1.

## 5. Target User Outcome

A user who installs AO Cable V1 can:

- route PC application audio into Phone Link as a call microphone, with
  remote-party audio quality at least matching what VB-Cable produces in the
  same scenario;
- route one PC app's output to another PC app's input without an external
  loopback;
- run the included live-call test harness to verify their installation
  produces clean call audio;
- read driver-side counters (ring fill, overflow, underrun, drop) through
  the Control Panel or `test_stream_monitor.py` to diagnose any quality issue
  themselves.

## 6. V1 Scope

V1 includes:

- PortCls / WaveRT framework (continuation; not a greenfield ACX rewrite).
- Two cable pairs: Cable A (render+capture), Cable B (render+capture).
- INF-advertised default OEM format: 48 kHz / 24-bit / Stereo.
- KSDATARANGE accepting practical formats (see ADR-008).
- INT32 frame-indexed cable ring with hard-reject overflow.
- Single-pass linear-interpolation SRC per direction (write SRC, read SRC),
  GCD divisor 300/100/75.
- Canonical cable advance helper (`AoCableAdvanceByQpc`) that owns transport,
  accounting, and position freshness; called by query path, shared timer,
  and packet surface.
- Position recalculated to current QPC inside `GetPosition` /
  `GetPositions`.
- 8-frame minimum gate, 63/64 phase-corrected timer, DMA overrun guard.
- Fade-in envelope for click suppression at packet boundaries.
- Diagnostics IOCTL (`IOCTL_AO_GET_STREAM_STATUS`) with fill / overflow /
  underrun / drop counters per cable per direction.
- Build and install tooling: `build-verify.ps1`, `install.ps1 -Action upgrade`
  (no-reboot quiesce path).
- Live-call test harness: `tests/live_call/run_test_call.py`.

## 7. Non-Goals (V1)

V1 does not include:

- Audio Class Extensions (ACX) / KMDF rewrite — that is a separate product
  track (see `feature/ao-pipeline-v2` reference; not this project).
- More than two cable pairs.
- Per-app routing UI inside the driver (handled by Windows Sound Settings or
  third-party tools like SoundVolumeView/svcl).
- Volume / mute / mixer.
- APO / DSP / AGC / echo cancellation / equalization / limiter / noise
  suppression.
- Built-in ASIO driver (works through ASIO4ALL bridge — same as VB-Cable).
- Built-in Bluetooth HFP forwarding (Phone Link does its own HFP routing
  externally).
- Phone Link internal control. The driver presents a microphone-class
  endpoint; whether Phone Link auto-selects it as the communications
  microphone is a Windows endpoint-policy concern, not driver scope.
- Full-path bit-perfect guarantee after the driver. Driver-internal ring is
  ~19-bit-normalized INT32 by design (matches VB-Cable's headroom strategy).

## 8. Success Criteria

V1 is successful when all of these are true on a clean Windows 11 machine:

1. The driver builds via `build-verify.ps1 -Config Release` without warnings
   regressing.
2. The driver installs via `install.ps1 -Action upgrade` without reboot.
3. Sound Settings shows four endpoints: AO Cable A render, AO Cable A
   capture, AO Cable B render, AO Cable B capture.
4. WASAPI shared-mode open at 48 kHz / 24-bit / Stereo succeeds on all four
   endpoints; unsupported formats are rejected cleanly.
5. Local sine-tone loopback on each cable preserves frame order and produces
   no audible glitches in steady state.
6. Live-call test harness on AO Cable A (or B) produces user-judged clean
   audio, matching VB-Cable on the same harness.
7. STT transcription accuracy on OpenAI Realtime through AO Cable matches
   transcription accuracy through VB-Cable.
8. `test_stream_monitor.py` shows zero ring overflow and zero underrun
   during steady-state speech for at least one full call.
9. RUN -> STOP -> RUN does not replay stale audio into a fresh capture
   session.
10. Cable A and Cable B are independent — heavy traffic on one does not
    affect the other.

## 9. Product Risks

Key risks:

- **PortCls/WaveRT API misuse during rewrite** — mitigated by REVIEW_POLICY
  § 3-4 (API sequence validation against installed WDK headers).
- **Phase regression** — Phase 5/6 history shows partial rewrites that
  introduced split ownership; mitigated by ADR-006 (one canonical owner)
  and per-phase exit gates.
- **Long-call timing drift** — mitigated by ADR-007 (63/64 phase correction
  + 8-frame gate + DMA overrun guard).
- **Phone Link compatibility variance** — Phone Link's endpoint selection
  policy is OS-side; we expose a microphone-class endpoint with the
  expected metadata, but cannot force selection. Acceptable as long as the
  user-driven path (set as default communications microphone, then call)
  works.
- **Coexistence with VB-Cable** — both drivers register similar
  KSCATEGORY interfaces on different hardware IDs. Mitigated by
  AO-specific service names, hardware IDs, and INF identity (see
  `Source/Main/aocablea.inx` / `aocableb.inx`).

## 10. Source Of Truth

Detailed technical design:

```text
docs/AO_CABLE_V1_DESIGN.md
```

Architecture overview:

```text
docs/AO_CABLE_V1_ARCHITECTURE.md
```

Architecture decisions:

```text
docs/ADR.md
```

AI / agent working rules:

```text
CLAUDE.md
AGENTS.md
```
