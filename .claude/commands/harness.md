# AO Cable V1 Harness Command

Run an AO Cable V1 phase step manually and safely.

## Rules

- Read `CLAUDE.md` and `AGENTS.md` first.
- Do not edit files before telling the user what will change.
- Do not use `--dangerously-skip-permissions`.
- Do not auto-commit.
- Do not auto-push.
- Do not continue when behavior of a Windows audio API (PortCls, KS, WaveRT, IRP path) is unknown — verify against installed WDK headers, Microsoft Learn, or the local PDF first.
- If blocked, report the blocker instead of guessing.

## Workflow

1. Read in this order:
   - `CLAUDE.md`
   - `AGENTS.md`
   - `docs/PRD.md`
   - `docs/ADR.md`
   - `docs/AO_CABLE_V1_ARCHITECTURE.md`
   - `docs/AO_CABLE_V1_DESIGN.md`
2. Inspect the target phase status:

```powershell
python scripts\execute.py status 1-int32-ring
```

3. Print the next step prompt:

```powershell
python scripts\execute.py next 1-int32-ring
```

4. Before editing, tell the user:
   - What will change.
   - Which files will be touched.
   - Why the change is needed.
   - Whether the change matches the step's planned files exactly, or expands scope.

5. After user approval, perform only that step. Do not bundle two steps into one edit.

6. Run the step's acceptance criteria as specified in `phases/<phase>/step<N>.md`.

7. Mark the step explicitly as completed, error, or blocked:

```powershell
python scripts\execute.py mark 1-int32-ring 0 completed --message "..."
```

## What this command does NOT do

- It does not start a new branch (single-branch policy: see `docs/GIT_POLICY.md`).
- It does not commit on success — the user or `/review` decides.
- It does not skip the review step. A step is not "done" until `/review` passes and the change is committed.
