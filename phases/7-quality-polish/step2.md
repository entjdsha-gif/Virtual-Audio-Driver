# Phase 7 Step 2: Multi-channel support (≥ 2 channels)

## Goal

V1 ships with stereo (2-channel) for both render and capture (per
`docs/PRD.md` § 6 / ADR-008). The internal ring is already 16-channel
(per ADR-003 / DESIGN § 2). This step lifts the KSDATARANGE channel
limit to allow up to 8-channel input/output and validates the SRC
correctly handles multi-channel layouts.

## Planned Files

- `Source/Filters/cablewavtable.h` — KSDATARANGE channel range.
- `Source/Utilities/loopback.cpp` — confirm SRC channel-mapping is
  correct for >2 channels (per-channel state arrays already sized to
  16).

## Acceptance Criteria

- [ ] Build clean.
- [ ] 5.1 (6-channel) render → capture round trip preserves channel
      mapping (left ≠ right ≠ center etc.).
- [ ] 7.1 (8-channel) round trip preserves mapping.
- [ ] Stereo and mono regression tests still pass.

## Completion

```powershell
python scripts/execute.py mark 7-quality-polish 2 completed --message "Multi-channel up to 8-ch validated."
```
