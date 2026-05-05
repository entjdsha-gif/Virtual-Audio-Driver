# VB-Cable Path Analysis for AO Telephony V1

## Purpose

This note captures what the VB-Cable reference manual implies about audio-path
quality, how that differs from AO's current loopback model, and why
`AO Telephony Passthrough V1` is the correct narrow first fix for Phone Link
call quality.

VB internals are still proprietary. This document does not claim to reverse
engineer VB-Cable. It only compares:

- documented VB-Cable behavior
- observable AO behavior
- actual connected-call results (`VB clean / AO bad`)

## What the VB manual says

### 1. VB frames the cable as a simple transfer path

The manual describes VB-Cable as a simple virtual audio cable that transfers the
signal from input to output, while handling format conversion only when needed.

Reference:
- [page 3](./VBCABLE_ReferenceManual_pages/VBCABLE_ReferenceManual_page-0003.jpg)

### 2. VB treats user-facing format and internal sample-rate as tunable path parameters

The manual exposes default audio format, cable format, internal sample rate, and
latency as separate operational concepts. This suggests the path is designed
around the connected stream's practical format rather than around one permanently
fixed oversized internal representation.

References:
- [page 7](./VBCABLE_ReferenceManual_pages/VBCABLE_ReferenceManual_page-0007.jpg)
- [page 9](./VBCABLE_ReferenceManual_pages/VBCABLE_ReferenceManual_page-0009.jpg)

### 3. VB explicitly states that format match means no conversion and best quality

The most important design statement in the manual is that if input/output sample
rate matches the internal sample rate, the signal passes through without
conversion, and that this gives the best audio quality.

Reference:
- [page 11](./VBCABLE_ReferenceManual_pages/VBCABLE_ReferenceManual_page-0011.jpg)

This is the clearest documented justification for a `passthrough-first`
philosophy:

- when formats already align, do not add extra driver-side conversion
- when formats do not align, convert only as much as necessary

### 4. VB documents latency and mismatch cost explicitly

The manual documents buffer-driven latency behavior, including:

- `Max Latency = 3 x Max Buffer Size`
- scaling behavior when internal sample rate differs from output sample rate

This matters because mismatch is not only a "conversion quality" issue. It also
changes buffering behavior and therefore affects continuity risk in real-time
paths.

References:
- [page 14](./VBCABLE_ReferenceManual_pages/VBCABLE_ReferenceManual_page-0014.jpg)
- [page 15](./VBCABLE_ReferenceManual_pages/VBCABLE_ReferenceManual_page-0015.jpg)
- [page 17](./VBCABLE_ReferenceManual_pages/VBCABLE_ReferenceManual_page-0017.jpg)

## What AO currently does

AO's current loopback architecture is different.

### Fixed internal format

AO keeps a fixed internal loopback model centered on:

- `48kHz`
- `24-bit`
- `8-channel` or `16-channel`

Relevant code:
- [loopback.h](../Source/Utilities/loopback.h)
- [loopback.cpp](../Source/Utilities/loopback.cpp)

### FormatMatch is stricter than stream-to-stream match

AO's generic raw path is gated by `FormatMatch`, which currently means:

- speaker format matches mic format
- and that format also matches AO's fixed internal format

That is a very different rule from "speaker and mic already match each other, so
just pass bytes through."

### Why Phone Link is a structural mismatch today

Observed live call format:

- Phone Link render/capture stream: `48kHz / 16-bit / 2ch / PCM`

AO generic internal path:

- internal loopback model: `48kHz / 24-bit / 8ch`

So even when Phone Link opens the same format on both ends, AO still falls into
the converted path because:

- `16-bit != 24-bit`
- `2ch != 8ch`

That means AO performs extra bit-depth and channel conversion inside the driver
on the most common telephony path.

## Why this explains the current real-call gap

Connected-call isolation already showed:

- internal PCM dumps are clean for both AO and VB
- VB remains clean on the actual Phone Link call
- AO becomes choppy / noisy / hollow on the actual Phone Link call

That combination strongly suggests the real gap is not upstream TTS generation
or Python PCM handling. It is the AO path behavior under the live Windows audio
endpoint / routing / cable path.

The most plausible structural explanation is:

- VB's documented philosophy prefers no conversion when formats already align
- AO's current generic path still forces conversion because the internal model is
  larger than the stream

This does not prove every VB internal detail. It does show that AO is currently
doing extra work on a path where VB explicitly aims not to.

## Design implication for V1

`AO Telephony Passthrough V1` is intentionally narrow.

It does **not** redesign AO's whole internal model. Instead, it applies the VB
manual's best-quality principle to the exact real-world path that is failing:

- `48kHz / 16-bit / 2ch / PCM`
- speaker and mic same-format
- single render stream

For that path only, V1 removes extra AO-side bit-depth/channel conversion and
uses a dedicated raw telephony ring.

V1 keeps the existing public contract unchanged:

- `InternalRate`
- `InternalChannels`
- `LB_INTERNAL_BITS`
- `AO_CONFIG`
- current `8/16` public channel model

## Direction after V1

V1 is not the end-state architecture.

The long-term target is:

- support a wider format range than VB
- but inside that supported range, prefer raw passthrough when the two sides
  already agree
- convert only on mismatch

In short:

- V1 = telephony-specific first slice
- V2 = full passthrough-first generalization

## Important non-conclusions

- This document does **not** prove that VB tracks every field of every stream
  dynamically.
- This document does **not** claim HFP limits are irrelevant. HFP still defines
  a quality ceiling.
- It does show that HFP is not the explanation for AO-specific distortion,
  because VB stays clean on the same Phone Link/HFP path.
