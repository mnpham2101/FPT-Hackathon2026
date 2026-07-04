---
name: task-planning-conventions
description: Task ID scheme, phase/task/subtask structure, subtask discipline (single objective, atomic commit, build/tests), and parallel/sequential grouping rules governing how the implementation plan must be built.
---

# Task Planning Conventions

Authoritative process rule for how [[project-planner]] must structure the implementation plan.
This governs *how* planning is done, independent of which milestone is active — phase *content*
itself lives in [.claude/plans/](../plans/) (currently
[milestone1.md](../plans/milestone1.md)).

## Task ID scheme

Every task and subtask is identified as `X.Y.Z.W`:

| Segment | Meaning |
|---|---|
| `X` | Requirement — the enumerated requirement (from [[project-researcher]]'s output) this work serves |
| `Y` | Phase — the phase number in the active plan (e.g. 1–6 in milestone1.md) |
| `Z` | Task — a group of related subtasks delivering one feature or technical solution within the phase |
| `W` | Subtask — the atomic unit of work |

- IDs are assigned once and stay stable — never renumber completed work, even if scope shifts
  later.
- Every task/subtask must map to a requirement (`X`) or a phase (`Y`). No orphan tasks with no
  traceable origin.

## Plan structure: phases → tasks → subtasks

- The plan is organized as **phases**, each phase carrying an explicit **input** (what must exist
  before the phase can start) and **output**, i.e. its **acceptance criteria** — the check that
  proves the phase is complete.
- Each phase decomposes into **tasks**, and each task into **subtasks**.
- **Grouping:** subtasks are grouped into a task when they jointly deliver one feature or one
  technical solution — don't scatter a single coherent piece of work across unrelated tasks, and
  don't bundle unrelated work into one task for convenience.

## Subtask discipline (non-negotiable)

Every subtask must:

- Have a **strict single objective**, stated unambiguously.
- Contain **no out-of-scope code** — anything not required by that single objective belongs in a
  different subtask.
- Produce exactly **one atomic commit**.
- **Pass build** before being considered complete.
- **Pass unit tests** before being considered complete.
- Be **self-contained**: carry all information an implementing subagent needs (file paths,
  relevant contract fields, acceptance criteria) so the subagent should not need to re-read the
  wider codebase — only re-read on hitting an actual implementation issue.

A subtask is marked **done** only when all of the above hold *and* the commit has been made.

## Commit message format

Every commit in this project — HLD/design commits included, not just implementation subtasks —
follows:

```
[<taskID>] <type>: <subject>
```

- `<taskID>` is the **most specific ID known at commit time**:
  - **Omitted entirely** (`<type>: <subject>`, no bracket) for research-report commits — the
    report is what defines the requirement numbers, so no `X` exists yet (see
    [research-report-format.md](research-report-format.md#commit-format)).
  - `X` alone (requirement only) for pre-decomposition commits — e.g. an HLD/design commit made
    before phase/task/subtask breakdown exists for that requirement.
  - Full `X.Y.Z.W` for implementation commits made against a decomposed subtask.
- `<type>` is one of: `design` (HLD/architecture artifacts), `feat` (new implementation), `fix`,
  `refactor`, `test`, `docs`, `chore`.
- `<subject>` is a short, imperative description of the change.

Examples:
- `docs: add requirement analysis & solution report for the comms track` — research-report
  commit, made before any requirement numbers exist.
- `[2] design: define perception-track MVC module boundaries` — requirement-2 HLD, committed
  before task/subtask IDs exist for that requirement.
- `[2.3.1.2] feat: implement gate hysteresis in track manager` — post-decomposition implementation
  subtask.

## Parallel vs. sequential execution

- Tasks and subtasks may run **in parallel** or **sequentially** — the planner must explicitly
  mark which, based on real dependencies (shared files, data contracts, ordering constraints), not
  by default assumption.
- Tracks that only share a frozen contract (not each other's internals) default to parallel;
  a subtask that consumes another subtask's output is sequential after it.

## How to apply

- [[project-planner]] owns and enforces this document when building and updating the plan.
  Subagents spawned to implement a subtask inherit the subtask-discipline rules above as their
  definition of done — the planner's brief to a subagent must make that explicit, not assume it.
- Commit tagging: [[project-researcher]] omits the taskID for report commits (see
  [research-report-format.md](research-report-format.md#commit-format));
  [[project-architecture]] uses the `X`-only form for HLD commits (see
  [hld-content-and-commit-format.md](hld-content-and-commit-format.md)); [[project-planner]] and
  the subagents it spawns use the full `X.Y.Z.W` form once tasks/subtasks are decomposed.
