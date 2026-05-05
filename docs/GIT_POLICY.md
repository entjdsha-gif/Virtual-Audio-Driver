# AO Cable V1 Git Policy

Status: active
Authority: ADR-014 (supersedes ADR-012)

## 1. Principle

`main` is the **pre-rewrite shipping reference**. It is unchanged
during Phase 1-6 and only receives the V1 ship merge at Phase 7 exit.
Do not commit directly to `main`.

`feature/ao-fixed-pipe-rewrite` is **V1's integration branch** — it
plays the role V2's `master` plays in V2's git policy. All phase
branches merge here with `--no-ff`. Direct commits to this branch are
reserved for short-lived `docs/...` / `fix/...` branch merges and the
final V1 ship merge prep.

All Phase 1+ implementation work happens on **per-phase branches**
(`phase/<N>-name`). Phase branches merge to `feature/ao-fixed-pipe-rewrite`
only after Codex review passes and the phase exit gate is met.

Do not hide blockers. If a local tool workaround is used, record exactly
what it does and which validation remains blocked.

## 2. Branches

### Active

- `main` — pre-rewrite shipping baseline. **Untouched during Phase 1-6.**
  Receives one `--no-ff` merge from `feature/ao-fixed-pipe-rewrite` at
  V1 ship event (Phase 7 exit).
- `feature/ao-fixed-pipe-rewrite` — V1's integration branch. Phase
  branches merge here. Holds the seven-round planning baseline (commits
  through `aaf585a`) plus the ADR-014 policy switch commit (this commit).

### Phase branches

```text
phase/1-int32-ring
phase/2-single-pass-src
phase/3-canonical-helper-shadow
phase/4-render-coupling
phase/5-capture-coupling
phase/6-cleanup
phase/7-quality-polish
```

Branch name = `phase/<N>-<phases/N-name directory>`. The mapping is
1:1 with `phases/<N>-name/`.

A phase branch is created from `feature/ao-fixed-pipe-rewrite` HEAD at
phase entry, lives for the duration of phase implementation, and is
merged back at phase exit. After successful merge the phase branch may
be deleted locally; the merge commit on `feature/ao-fixed-pipe-rewrite`
preserves the history.

V2's "branch name must not collide with design phase name" rule does
not apply to V1 — V1's `phases/N-name` directory naming is unambiguous.

### Frozen reference (never modified)

- `feature/ao-pipeline-v2` — V2 SessionPassthrough research baseline.
- `feature/ao-telephony-passthrough-v1` — V1 telephony history.

### Short-lived support branches

```text
docs/<topic>     # cross-phase doc-only fix (e.g. docs/git-policy)
fix/<scope>      # bug fix outside a phase (e.g. fix/install-resume-task)
```

These branch off `feature/ao-fixed-pipe-rewrite`, merge back with
`--no-ff`, and are deleted after merge.

### Forbidden

- Direct commits to `main` of any kind during Phase 1-6.
- Direct commits to `feature/ao-fixed-pipe-rewrite` for phase
  implementation work. Phase work goes on `phase/N-name`.
- Squash merges (loses per-step bisect granularity).
- Fast-forward merges of phase branches (loses the `Verified:` block
  attached to the merge commit).
- Force-push to `main` ever.
- Force-push to `feature/ao-fixed-pipe-rewrite` ever.
- Skipping git hooks (`--no-verify`) or signing (`--no-gpg-sign`)
  unless the user explicitly authorizes for a specific commit.
- Rewriting history on shared branches.

## 3. Commits (within a phase branch)

### Commit message prefix

```text
phaseN/stepM: <imperative summary>
phaseN/fix: <imperative summary>             (post-review fix; see § 4)
phaseN: close <classification>               (final closeout commit)
```

`<classification>` is a short status token chosen at phase close:
`PASS`, `PASS_WITH_CAVEATS`, `BLOCKED`, or a phase-specific token
(e.g. V2 used `CONVERSION_CLEAN` for phase 11).

For non-phase work on `feature/ao-fixed-pipe-rewrite` (via `docs/...`
or `fix/...` branches):

```text
docs: <imperative summary>
fix: <imperative summary>
build: <imperative summary>
install: <imperative summary>
signing: <imperative summary>
review: <imperative summary>                 (cross-phase review fix bundle)
policy: <imperative summary>                 (ADR / GIT_POLICY change)
```

### Commit discipline

- Commit only after Codex review passes for the step (see
  `docs/REVIEW_POLICY.md`). Compile success and design-value matching
  alone are not a review pass — API sequence, register/unregister
  pairing, runtime observable proof, failure-path / lifetime, and INF
  state must all be checked.
- Do not commit before review.
- Do not mark a step `completed` before commit.
- A failed runtime test may be committed so the failure is recorded.
  Commit message and step file must contain `FAIL` clearly.
- One step → one commit (preferred). A post-review fix may be a
  separate `phaseN/fix:` commit if the original step commit already
  landed; if the step commit hasn't landed yet, fold the fix in
  before committing.

### Forbidden commits (always)

- Build artifacts: `*.obj`, `*.pdb`, `*.sys`, `*.cat`, `*.tlog`,
  `Source/**/Debug/`, `Source/**/Release/`, `build/`.
- Generated INF (only `.inx` source is tracked).
- Test capture artifacts > ~10 MB without explicit user approval.
- `.env`, API keys, signing key material.
- Local WDK workaround root files (`Directory.Build.props`,
  `Directory.Build.targets` at repo root).
- Personal IDE state: `.vs/`, `.idea/`, `*.user`.

The `.gitignore` enforces most of these.

## 4. Workflow per step

```text
1. Implement the step on phase/N-name.
2. Self-check: build, IOCTL probe, acceptance criteria from step file.
3. Request Codex review.
4. Cross-verify each finding against WDK headers, design docs, RE evidence.
   Disagree-with-evidence is allowed; do not blindly apply incorrect
   findings.
5. If BLOCKER found and verified correct: fix on the same phase branch,
   request re-review. **Do not commit the fix before re-review passes.**
6. Review passes: commit with phaseN/stepM: prefix.
7. python scripts/execute.py mark <phase-dir> <step> completed
       --message "..."
```

If a step's commit has already landed when a review finding arrives
(e.g. cross-doc audit catches drift later), the fix is a separate
`phaseN/fix:` commit on the same phase branch.

## 5. Phase merge (phase branch → integration)

At phase exit, after the phase exit gate is met and the phase has a
`phaseN: close <classification>` commit on its phase branch:

```powershell
git checkout feature/ao-fixed-pipe-rewrite
git merge --no-ff phase/N-name
```

`--no-ff` is mandatory. Squash is forbidden.

The merge commit message **must** include the V2-style verified block.
Use this template (Heredoc into `git commit`):

```text
Merge phase/N: <one-line description> (<CLASSIFICATION>)

Phase N classification: <CLASSIFICATION>

Verified:
- build clean (build-verify.ps1 -Config Release): <commit hash> built OK
- install clean (install.ps1 -Action upgrade): <date>, machine <id>
- IOCTL_AO_GET_STREAM_STATUS new fields readable: <test_stream_monitor.py output ref>
- live-call evidence (where applicable): <run dir>
- forbidden-symbol grep: clean
- per-phase exit.md acceptance: each box checked with evidence pointer

Known blockers:
- <item> | none

Non-claims:
- this merge does NOT prove <whatever's out of scope>
- ...

Co-Authored-By: <agent identity>
```

`Verified:` lines must be specific enough that a reviewer six months
later can re-run them. Vague entries like "looks good" are rejected.

`Non-claims:` is the safety rail against "merged ⇒ everything works"
misreading. List what the merge intentionally does NOT prove
(e.g. "this Phase 4 merge does not prove capture-side audible flip —
that is Phase 5").

After the merge commit lands, the phase branch may be deleted locally:

```powershell
git branch -d phase/N-name
```

The merge commit and per-step commits remain on
`feature/ao-fixed-pipe-rewrite` history.

## 6. V1 ship merge (Phase 7 exit only)

Merging `feature/ao-fixed-pipe-rewrite` → `main` is a separate event,
gated on Phase 7 step 5 (M6 shipping checklist):

```text
1. All Phase 1-7 step files marked completed in phases/index.json.
2. Each phase has a successful `Merge phase/N` commit on
   feature/ao-fixed-pipe-rewrite.
3. Build / install / sign verification passes on a clean machine.
4. Live-call quality reaches the target (docs/PRD.md success criteria).
5. The user explicitly approves the merge.
```

Use `git merge --no-ff` and the same `Verified` / `Known blockers` /
`Non-claims` block, scoped to the V1 ship gate.

## 7. Cherry-picks

Cherry-picks from frozen reference branches (`feature/ao-pipeline-v2`,
`feature/ao-telephony-passthrough-v1`) are allowed for narrow fixes
(e.g. no-reboot upgrade fix), but only after:

- the cherry-picked commit is documented in the new commit message
  (original SHA + branch + reason);
- the change passes review under `docs/REVIEW_POLICY.md`;
- the change is brought in via a `fix/<scope>` short-lived branch,
  not directly committed to `feature/ao-fixed-pipe-rewrite`.

## 8. Local WDK / signing workarounds

Local-only WDK or signing workaround files must never be committed.
If a build needs them locally, document the reason in a personal note,
not in the repo. The repo's build must work on a fresh clone with full
WDK package verification and proper signing.

Builds that use local workarounds count as compile/link validation
only. They do not prove install/load readiness.

## 9. Runtime artifacts under `tests/`

`tests/` runtime outputs (`tests/<phaseN-runtime>/...`, capture wavs,
ETL traces, dbgview logs) remain **untracked** by default. Promote to
tracked location only when the user explicitly approves a specific
artifact as permanent evidence (e.g. a representative live-call log
for a milestone). Even when promoted, prefer storing in `results/`
rather than `tests/`.

## 10. Reset / discard rules

Destructive operations (`git reset --hard`, `git push --force`,
`git checkout -- <file>` discarding work, `git branch -D`) must be:

- Confirmed with the user before running.
- Justified by the situation (e.g. local mistake to undo).
- Never used on `main` or `feature/ao-fixed-pipe-rewrite`.
- Never used on a phase branch that has unmerged work without an
  explicit "throw it away" instruction from the user.
- Never used to "make a problem disappear" without diagnosing root cause.

## 11. Commit ownership

Every commit produced through this workflow must include the
appropriate co-author trailer when an AI agent contributed:

```text
Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
```

(Or the equivalent for whichever assistant is in use.) Attribution and
audit traceable.

## 12. Quick reference

```text
phase entry:
  git checkout feature/ao-fixed-pipe-rewrite
  git pull          # if remote is set up
  git checkout -b phase/N-name

phase work (per step):
  <implement, review, fix, re-review>
  git add <files>
  git commit -m "phaseN/stepM: <msg>"
  python scripts/execute.py mark <phase-dir> <step> completed --message "..."

phase exit:
  <ensure exit.md acceptance checked>
  git commit -m "phaseN: close <CLASSIFICATION>"     # closeout commit
  git checkout feature/ao-fixed-pipe-rewrite
  git merge --no-ff phase/N-name                     # with Verified block
  git branch -d phase/N-name                         # optional cleanup

V1 ship (Phase 7 exit only, user-approved):
  git checkout main
  git merge --no-ff feature/ao-fixed-pipe-rewrite    # with Verified block
```
