# Phase 2 Step 0: GCD divisor helper

## Read First

- `docs/ADR.md` ADR-004 (single-pass linear-interp SRC, GCD divisor).
- `docs/AO_CABLE_V1_DESIGN.md` § 2.3 / § 2.4 (write / read SRC).
- `results/vbcable_disasm_analysis.md` § "GCD-based ratio" (VB
  reference: divisors 300, 100, 75).

## Goal

Implement the GCD divisor selector helper that picks the smallest
divisor among `[300, 100, 75]` that divides both source and destination
rates evenly. Returns the resulting `(srcRatio, dstRatio)` pair plus a
status. Used by Phase 2 Steps 1 and 2.

## Planned Files

Edit only:

- `Source/Utilities/loopback.cpp` — add `pickGCDDivisor` static helper.
- `Source/Utilities/loopback.h` — declaration only if exposed beyond
  the TU; otherwise keep static.

## Required Edits

```c
typedef struct _AO_GCD_RATIO {
    ULONG Divisor;     /* 300, 100, 75, or 0 (unsupported) */
    ULONG SrcRatio;    /* srcRate / Divisor */
    ULONG DstRatio;    /* dstRate / Divisor */
} AO_GCD_RATIO;

static
NTSTATUS
PickGCDDivisor(ULONG srcRate, ULONG dstRate, AO_GCD_RATIO* out)
{
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
    out->Divisor = 0;
    return STATUS_NOT_SUPPORTED;
}
```

## Acceptance Criteria

- [ ] Build clean.
- [ ] Helper returns `STATUS_SUCCESS` for the rate pairs listed in
      `docs/AO_CABLE_V1_ARCHITECTURE.md` § 10.3 (8000, 16000, 22050,
      32000, 44100, 48000, 88200, 96000, 176400, 192000 paired against
      typical internal rate 48000):
      - 48000 ↔ 48000 → divisor 300, ratio 160:160 (same-rate fast path).
      - 48000 ↔ 44100 → divisor 300, ratio 147:160.
      - 48000 ↔ 96000 → divisor 100, ratio 480:960.
      - 48000 ↔ 8000  → divisor 100, ratio 480:80.
      - 48000 ↔ 22050 → divisor 75,  ratio 294:640.
- [ ] Helper returns `STATUS_NOT_SUPPORTED` for unsupported rates
      (e.g., 12345 Hz).
- [ ] No allocations, no locks; pure function.

## Completion

```powershell
python scripts/execute.py mark 2-single-pass-src 0 completed --message "GCD divisor helper PASS for all V1 rate pairs."
```
