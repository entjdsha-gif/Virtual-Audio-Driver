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

Per `docs/GIT_POLICY.md` § 6 (V1 ship merge — ADR-014):

```powershell
git checkout main
git merge --no-ff feature/ao-fixed-pipe-rewrite
```

The merge commit message must include the ADR-014 verified block:

```text
V1 ship merge (M6)

V1 classification: <PASS / PASS_WITH_CAVEATS>

Verified:
- build-verify.ps1 -Config Release: <hash> built clean
- install.ps1 -Action upgrade: <date>, machine <id>
- live-call parity with VB: tests/phase5-runtime/<run>/judgment
- benchmark suite PASS: phases/7-quality-polish/step4 artifacts
- M6 checklist: phases/7-quality-polish/step5 fully checked
- Phase 1-7 each merged with verified blocks (see git log
  --first-parent feature/ao-fixed-pipe-rewrite)

Known blockers:
- <any documented residual risk> | none

Non-claims:
- this merge does NOT replace V2 (separate ACX track).
- this merge does NOT promise zero-drift parity with VB binary
  (only behavioral parity per the design).

Co-Authored-By: <agent identity>
```

## Beyond V1

V1 ships when this exit gate is met. V2 (ACX/KMDF clean rewrite,
in `ao-cable-v2-step2b-merge`) is a separate product track and is
not gated on V1.

Future V1 maintenance work (bug fixes, security patches) goes through
a similar phase model: open a new phase directory under `phases/`,
follow `docs/REVIEW_POLICY.md` and `docs/GIT_POLICY.md`.
