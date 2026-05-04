# Phase 7 Step 5: M6 shipping checklist

## Goal

Final pre-merge checklist before `feature/ao-fixed-pipe-rewrite`
merges to `main` per `docs/GIT_POLICY.md` § 5.

## Planned Files

No source edits. Cross-check / produce a release manifest.

## Checklist

- [ ] Build on clean machine: `build-verify.ps1 -Config Release`
      produces `aocablea.sys` + `aocableb.sys` with valid WHQL-style
      signing pipeline (`docs/M6C_SIGNING_PLAN.md`).
- [ ] Install on clean machine: `install.ps1 -Action upgrade` finishes
      without reboot, both cables registered, default device
      switching works.
- [ ] Live-call parity with VB confirmed (Phase 5 Step 2 procedure).
- [ ] Multi-channel test passes (Phase 7 Step 2).
- [ ] Benchmark suite roll-up: PASS against `docs/PRD.md` § 8 success
      criteria.
- [ ] Control Panel diagnostics show steady-state counters at 0.
- [ ] No `BLOCKER` open in any phase exit document.
- [ ] Release artifacts ready: `.sys`, `.cat`, `.inf`, signed installer
      package.
- [ ] `docs/RELEASE_CANDIDATE.md` and `docs/RELEASE_HARDENING.md`
      reviewed and current.
- [ ] User explicitly approves the merge to `main`.

## Completion

```powershell
python scripts/execute.py mark 7-quality-polish 5 completed --message "M6 shipping checklist passed; ready for main merge."
```
