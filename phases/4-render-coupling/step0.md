# Phase 4 Step 0: Fade-in envelope helper

## Read First

- `docs/AO_CABLE_V1_DESIGN.md` § 4.3 (fade envelope reference in
  helper render branch).
- `results/vbcable_pipeline_analysis.md` § 7.3 (VB fade envelope
  reference).

## Goal

Define and implement the 96-entry fade-in envelope that suppresses
packet-boundary clicks, matching the VB pattern. This step **only**
introduces the helper functions; it does not invoke them yet.

The envelope must be in place before Step 1's atomic flip, because
Step 1 calls `AoApplyFadeEnvelope` from the helper render branch
during the legacy → helper audible-ownership transition.

## Planned Files

Edit only:

- `Source/Utilities/transport_engine.cpp` — define
  `g_aoFadeEnvelope[96]` (or 95-entry table with explicit bounds),
  implement `AoApplyFadeEnvelope` and `AoResetFadeCounter`.
- `Source/Utilities/transport_engine.h` — declarations.

## Required Edits

```c
static const SHORT g_aoFadeEnvelope[96] = { /* per VB ... */ };

VOID
AoApplyFadeEnvelope(LONG* samples, ULONG sampleCount, LONG* perStreamCounter)
{
    LONG counter = *perStreamCounter;
    for (ULONG i = 0; i < sampleCount; ++i) {
        if (counter >= 0) {
            /* fully ramped; no envelope */
        } else {
            ULONG idx = (ULONG)(counter + 96);
            if (idx < ARRAYSIZE(g_aoFadeEnvelope)) {
                samples[i] = (samples[i] * g_aoFadeEnvelope[idx]) >> 7;
            }
            counter++;
        }
    }
    *perStreamCounter = counter;
}

VOID
AoResetFadeCounter(PAO_STREAM_RT rt)
{
    rt->FadeSampleCounter = -96;  /* pre-silence prefix */
}
```

`AoResetFadeCounter` is invoked at packet-boundary transitions —
specifically, on RUN entry and on any deliberate position
reinitialization.

## Rules

- Tell the user before editing.
- Do not yet wire `AoApplyFadeEnvelope` into the helper render branch.
  Step 1 does that as part of the atomic flip.
- The envelope must operate on the **scratch buffer before** the ring
  write; Step 1 enforces this ordering. (Review #15 of 8afa59a:
  applying the fade after the ring write affects nothing audible.)

## Acceptance Criteria

- [ ] Build clean.
- [ ] Forced-discontinuity unit test (call `AoApplyFadeEnvelope` on a
      step-discontinuous synthetic input) shows the envelope smoothing
      the discontinuity over ~96 samples.
- [ ] Steady-state speech is unaffected (counter at 0, envelope is a
      no-op).
- [ ] `AoResetFadeCounter` arms the counter to -96.

## What This Step Does NOT Do

- Does not call `AoApplyFadeEnvelope` from the helper. Step 1 does
  that.
- Does not flip render audible ownership. Step 1 does that.

## Completion

```powershell
python scripts/execute.py mark 4-render-coupling 0 completed --message "Fade envelope helper defined; not yet wired into helper."
```
