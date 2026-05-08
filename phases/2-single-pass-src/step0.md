# Phase 2 Step 0: GCD divisor helper

## Read First

- `docs/ADR.md` ADR-004 (single-pass linear-interp SRC, GCD divisor).
- `docs/AO_CABLE_V1_DESIGN.md` § 2.3 / § 2.4 (write / read SRC).
- `results/vbcable_disasm_analysis.md` § "GCD-based ratio" (VB
  reference: divisors 300, 100, 75).

## Goal

Implement the GCD divisor selector helper that tries the candidate
list `[300, 100, 75]` in **fixed priority order** and returns on the
first candidate that divides both source and destination rates evenly
(ADR-004 Decision step 1 — **first match wins**, NOT "smallest
divisor": `48000/96000` matches at `Divisor=300`, ratio `160:320`,
not at `Divisor=100`, ratio `480:960`). Returns the resulting
`(Divisor, SrcRatio, DstRatio)` triple plus an `NTSTATUS`. Used by
Phase 2 Steps 1 and 2.

## Planned Files

Edit only:

- `Source/Utilities/loopback.cpp` — add `PickGCDDivisor` static helper.
- `Source/Utilities/loopback.h` — declaration only if exposed beyond
  the TU; otherwise keep static.

## Required Edits

```c
typedef struct _AO_GCD_RATIO {
    ULONG Divisor;     /* 300, 100, 75 on success; 0 on failure */
    ULONG SrcRatio;    /* srcRate / Divisor on success; 0 on failure */
    ULONG DstRatio;    /* dstRate / Divisor on success; 0 on failure */
} AO_GCD_RATIO;

static
NTSTATUS
PickGCDDivisor(
    _In_  ULONG srcRate,
    _In_  ULONG dstRate,
    _Out_ AO_GCD_RATIO* out)
{
    /* Caller-bug guard: NULL out cannot be touched. */
    if (out == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    /* Zero output up front so every failure path leaves it clean. */
    out->Divisor  = 0;
    out->SrcRatio = 0;
    out->DstRatio = 0;
    if (srcRate == 0 || dstRate == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    /* First-match across the priority list (ADR-004 Decision step 1).
     * The first candidate that evenly divides BOTH rates wins. This is
     * NOT "smallest divisor": e.g. 48000/96000 matches at 300 (ratio
     * 160:320), not at 100 (480:960). */
    static const ULONG divisors[] = { 300, 100, 75 };
    for (ULONG i = 0; i < ARRAYSIZE(divisors); ++i) {
        ULONG d = divisors[i];
        if ((srcRate % d) == 0 && (dstRate % d) == 0) {
            out->Divisor  = d;
            out->SrcRatio = srcRate / d;
            out->DstRatio = dstRate / d;
            return STATUS_SUCCESS;
        }
    }
    /* In-range PCM rate pair that fails the priority probe.
     * ADR-008 § Consequences requires STATUS_NOT_SUPPORTED here. */
    return STATUS_NOT_SUPPORTED;
}
```

## Acceptance Criteria

- [ ] Build clean.
- [ ] **Divisor policy**: AO Cable V1 adopts ADR-004 Decision step 1
      — **first match across `[300, 100, 75]`** wins. This is NOT
      "smallest divisor". For example, `48000/96000` matches at
      `Divisor=300` (`SrcRatio=160`, `DstRatio=320`), not at
      `Divisor=100` (`480:960`).
- [ ] Helper returns `STATUS_SUCCESS` and fills `Divisor`, `SrcRatio`,
      `DstRatio` for the directional rate pairs below (full V1
      KSDATARANGE rate set in
      `docs/AO_CABLE_V1_ARCHITECTURE.md` § 10.3):

      | srcRate | dstRate | Divisor | SrcRatio | DstRatio |
      |---:|---:|---:|---:|---:|
      | 48000 | 48000 | 300 |  160 |  160 |
      | 48000 | 44100 | 300 |  160 |  147 |
      | 44100 | 48000 | 300 |  147 |  160 |
      | 48000 | 96000 | 300 |  160 |  320 |
      | 96000 | 48000 | 300 |  320 |  160 |
      | 48000 |  8000 | 100 |  480 |   80 |
      |  8000 | 48000 | 100 |   80 |  480 |
      | 48000 | 22050 |  75 |  640 |  294 |
      | 22050 | 48000 |  75 |  294 |  640 |

- [ ] Helper returns `STATUS_NOT_SUPPORTED` (and zeroes all `out`
      fields) for advertised-rate pairs that share no common divisor
      in `[300, 100, 75]`:
      - `srcRate=8000, dstRate=22050` (8000 needs ÷100; 22050 needs
        ÷75 — no candidate divides both). This is the canonical
        ADR-008 § Consequences case the KSDATARANGE intersection
        handler maps to its own `STATUS_NOT_SUPPORTED` tier.
- [ ] Helper also returns `STATUS_NOT_SUPPORTED` for helper-level
      unsupported / no-match inputs whose values divide by no
      candidate. The helper does **not** validate KSDATARANGE
      membership — production callers are filtered at intersection
      time (`STATUS_NO_MATCH`) before reaching this entry point;
      this branch is defensive correctness only:
      - `srcRate=12345, dstRate=48000` (12345 not divisible by any of
        300 / 100 / 75)
- [ ] Helper returns `STATUS_INVALID_PARAMETER` for caller-bug inputs
      (and, when `out != NULL`, zeroes all `out` fields):
      - `srcRate == 0`
      - `dstRate == 0`
      - `out == NULL`
- [ ] No allocations, no locks; pure function.

## Completion

```powershell
python scripts/execute.py mark 2-single-pass-src 0 completed --message "GCD divisor helper PASS for all V1 rate pairs."
```
