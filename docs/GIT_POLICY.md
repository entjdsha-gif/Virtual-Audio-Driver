# AO Cable V1 Git Policy

Status: active
Date: 2026-04-25

## 1. Principle

`main` is the verified release baseline. `feature/ao-fixed-pipe-rewrite` is the
current single active development branch.

AO Cable V1 uses a **single-branch + commit-prefix** model rather than V2's
phase-branch model, because:

- the project recently consolidated multiple worktrees and parallel branches
  back into one to remove coordination overhead;
- per-phase isolation is achieved through `phases/<N>-name/` directories and
  per-step commits, not through git branches;
- merging back to `main` is reserved for shipping milestones (M1/M2/M6),
  not for every phase exit.

## 2. Branches

### Active

- `main` — release baseline. Shipping commits only.
- `feature/ao-fixed-pipe-rewrite` — current implementation branch. All phase
  work happens here.

### Frozen reference

- `feature/ao-pipeline-v2` — V2 SessionPassthrough research baseline. Do not
  modify. Reference only.
- `feature/ao-telephony-passthrough-v1` — V1 telephony history. Reference
  only.

### Short-lived fix branches

A short-lived fix branch may be created when a non-trivial change must be
isolated for review, e.g.:

```text
fix/ring-overflow-counter
fix/install-resume-task-race
```

These are merged with `--no-ff` back to `feature/ao-fixed-pipe-rewrite` and
then deleted locally.

### Forbidden

- Do not commit directly to `main` for non-shipping work.
- Do not rewrite history on shared branches (`main`, `feature/ao-fixed-pipe-rewrite`).
- Do not force-push to `main` ever; warn the user if asked.
- Do not skip git hooks (`--no-verify`) or signing (`--no-gpg-sign`) unless
  the user explicitly authorizes.

## 3. Commits

### Commit message format

Use a phase / step prefix when the change belongs to a phase:

```text
phase1/step0: introduce INT32 frame-indexed FRAME_PIPE struct
phase1/step1: ring write hard-reject + overflow counter
phase1/exit: phase 1 closeout — runtime evidence + status flip
phase2/step0: add AoRingReadToScratch with linear-interp SRC
phase4/fix: correct fade envelope counter clamp
docs: clarify ADR-005 hysteresis half-WrapBound rule
review: codex review feedback fixes (phase3/step1)
```

For non-phase work (operational, install, signing, docs, build):

```text
docs: ...
build: ...
install: ...
signing: ...
ci: ...
```

### Commit discipline

- Commit only after review passes (see `docs/REVIEW_POLICY.md`).
- Workflow per step:
  1. Implement the step (only the planned scope).
  2. Run `/review` (or request reviewer pass).
  3. If `BLOCKER`: fix, re-review. **Do not commit the fix before re-review passes.**
  4. Review passes: commit (single commit per step is preferred).
  5. Mark the step `completed` via `python scripts/execute.py mark <phase> <N> completed --message "..."`.
- Do not split one approved test attempt into separate design / runtime-result / analysis commits unless the user explicitly asks.
- A failed test may still be committed (so the failure is recorded), but the commit message and step file must say `FAIL` clearly.

### Forbidden commits

Never commit:

- Build artifacts: `*.obj`, `*.pdb`, `*.sys`, `*.cat`, `*.tlog`, `Source/**/Debug/`, `Source/**/Release/`.
- Generated INF files (only `.inx` source is tracked).
- Test capture artifacts above ~10 MB without user approval (`.wav`, `.npy`, `.etl`).
- `.env` files, API keys, signing key material.
- Local WDK workaround root files (`Directory.Build.props`, `Directory.Build.targets` at repo root).
- Personal IDE state: `.vs/`, `.idea/`, `*.user`.

The `.gitignore` enforces most of these.

## 4. Phase Lifecycle Through Git

A phase has the following git footprint:

```text
phase<N>/step0:  initial step in the phase (tracked, single commit ideally)
phase<N>/step1:  next step
...
phase<N>/exit:   phase closeout — flip phases/index.json + phases/<N>/exit.md
```

No branches per phase. The `phases/<N>-name/` directory plus the commit
prefix is the phase identifier.

After `phase<N>/exit` commits, the phase status in `phases/index.json` flips
from `in_progress` (or `planned`) to `completed`.

## 5. Merge to main (shipping)

Merging `feature/ao-fixed-pipe-rewrite` to `main` is a separate, deliberate
operation, gated on:

```text
1. All in-scope phases for the shipping milestone (M1, M2, M6, ...) are
   completed in phases/index.json.
2. Build/install/sign verification passes on a clean machine.
3. Live-call quality reaches the target (per docs/PRD.md success criteria).
4. The user explicitly approves the merge.
```

Use `git merge --no-ff` to preserve the development history.

## 6. Cherry-picks

Cherry-picks from frozen reference branches (`feature/ao-pipeline-v2`,
`feature/ao-telephony-passthrough-v1`) are allowed for narrow fixes (e.g.
no-reboot upgrade fix), but only after:

- the commit being cherry-picked is documented in the commit message,
- the change passes review under `docs/REVIEW_POLICY.md`.

## 7. Local WDK / Signing Workarounds

Local-only WDK or signing workaround files must never be committed. If a
build needs them locally, document the reason in a personal note, not in
the repo. The repo's build must work on a fresh clone with full WDK package
verification and proper signing.

## 8. Runtime artifacts under `tests/`

`tests/` runtime outputs (`tests/<phaseN-runtime>/...`, capture wavs, etl
traces) remain **untracked** by default. Promote to tracked location only
when the user explicitly approves a specific artifact as permanent evidence
(e.g. a representative live-call log for a milestone).

## 9. Reset / discard rules

Destructive operations (`git reset --hard`, `git push --force`,
`git checkout -- <file>` discarding work) must be:

- Confirmed with the user before running.
- Justified by the situation (e.g. local mistake to undo).
- Never used on `main`.
- Never used to "make a problem disappear" without diagnosing root cause.

## 10. Commit ownership

Every commit produced through this workflow must include the appropriate
co-author trailer when an AI agent contributed:

```text
Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
```

(or the equivalent for whichever assistant is in use). This makes
attribution and audit traceable.
