# Phase 7 Exit: V1 ready to ship

## Exit Gate

- [ ] Steps 0..5 marked completed.
- [ ] M6 shipping checklist (Step 5) is fully checked.
- [ ] User has explicitly approved a merge of
      `feature/ao-fixed-pipe-rewrite` into `main`.

## Outcome

After Phase 7 exits, AO Cable V1 is ready to merge to `main` and
ship. Cable transport is VB-equivalent, diagnostics are user-
visible, multi-channel is supported, signing pipeline is functional,
benchmark suite confirms PRD success criteria.

## Phase 7 → main merge

Per `docs/GIT_POLICY.md` § 5:

```powershell
git checkout main
git merge --no-ff feature/ao-fixed-pipe-rewrite
```

The merge commit message must include:

```text
Verified:
- build-verify.ps1 -Config Release
- install.ps1 -Action upgrade
- live-call parity with VB
- benchmark suite PASS

Known blockers:
- (any documented residual risk)
```

## Beyond V1

V1 ships when this exit gate is met. V2 (ACX/KMDF clean rewrite,
in `ao-cable-v2-step2b-merge`) is a separate product track and is
not gated on V1.

Future V1 maintenance work (bug fixes, security patches) goes through
a similar phase model: open a new phase directory under `phases/`,
follow `docs/REVIEW_POLICY.md` and `docs/GIT_POLICY.md`.
