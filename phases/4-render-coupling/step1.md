# Phase 4 Step 1: Fade-in envelope on packet boundaries

## Read First

- `docs/AO_CABLE_V1_DESIGN.md` § 4.3 (fade envelope reference in
  helper render branch).
- `results/vbcable_pipeline_analysis.md` § 7.3 (VB fade envelope
  reference).

## Goal

Implement the 96-entry fade-in envelope (with max usable index 95)
that suppresses packet-boundary clicks, matching the VB pattern.

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

## Acceptance Criteria

- [ ] Build clean.
- [ ] Forced packet-boundary discontinuity test (manually inject a
      step-discontinuous waveform at a boundary) shows the envelope
      smoothing the discontinuity over ~96 samples.
- [ ] Steady-state speech is unaffected (counter is at 0, no envelope).
- [ ] `AoResetFadeCounter` correctly arms a fresh fade at RUN.

## Completion

```powershell
python scripts/execute.py mark 4-render-coupling 1 completed --message "Fade envelope active at packet boundaries."
```
