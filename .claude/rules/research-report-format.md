# Research Report Format

Governs the artifact [[project-researcher]] produces whenever it runs [requirement-analysis-and-solutioning](../skills/requirement-analysis-and-solutioning/SKILL.md). Every analysis run ends in a written report file — conversation output alone is not a deliverable.

## Location & naming

- Reports live in [requirements/](../../requirements/) at the repo root, one file per analysed feature or requirement set: `requirements/<feature-slug>.md`.
- Requirement numbers are **project-global and stable**: a new report continues the numbering, and published numbers are never reused or renumbered — they are the `X` segment of task IDs `X.Y.Z.W` ([task-planning-conventions.md](task-planning-conventions.md#task-id-scheme)).

## Required content

1. **Enumerated requirements** — numbered list; each requirement a precise, testable statement meeting [requirement-quality-criteria.md](requirement-quality-criteria.md) (measurable output / KPI, vague → precise translations recorded).
2. **Feasibility study result** — the whole-input verdict plus each requirement's achievable / at-risk / infeasible verdict, with reasoning.
3. **Technical solution analysis** — for each requirement (or group) needing a technical decision: the candidate solutions considered (always more than one), their comparison against the ranked criteria in [solution-selection-criteria.md](solution-selection-criteria.md), and the proposed best solution stating which criteria drove the pick.

## Commit format

Report commits are the one exception to the mandatory `[<taskID>]` prefix in [task-planning-conventions.md](task-planning-conventions.md#commit-message-format): the report is what *defines* the requirement numbers, so no `X` exists at commit time. Omit the taskID and commit as:

```
docs: <subject>
```

e.g. `docs: add requirement analysis & solution report for the comms track`.

## Downstream use

The report is the traceability anchor for everything after it: [[project-planner]]'s plan, tasks, and subtasks — and [[project-architecture]]'s HLDs — refer to requirements by number from these files instead of restating them.

## How to apply

[[project-researcher]] writes the report as the final step of every [requirement-analysis-and-solutioning](../skills/requirement-analysis-and-solutioning/SKILL.md) run, before handing off to [[project-architecture]] or [[project-planner]]. No analysis is complete until its report exists under `requirements/`.
