---
name: implementation-step-proposal
description: Procedure project-researcher follows when invoked with already-analysed requirements to propose implementation — enumerated implementation steps per requirement, saved as plans/Rx_[Phase].md for project-planner to decompose into tasks.
---

# Implementation Step Proposal (project-researcher)

Trigger: [[project-researcher]] is asked to study **already-analysed** requirements AND propose their implementation. The requirements are final — perform **no requirement analysis**: do not run [requirement-analysis-and-solutioning](../requirement-analysis-and-solutioning/SKILL.md), and do not apply [requirement-quality-criteria.md](../../rules/requirement-quality-criteria.md), [solution-selection-criteria.md](../../rules/solution-selection-criteria.md), or [research-report-format.md](../../rules/research-report-format.md) — the solutions are already selected in the report.

## Reference documentation

Consult **all** documents listed in [CLAUDE.md § Document authority](../../../CLAUDE.md), in authority order:

1. [m1-cooperative-awareness.md](../../../requirements/m1-cooperative-awareness.md) — the requirement's definition, dependencies, acceptance, and tech stack (§2); technical solutions (§3); standing decisions (§4).
2. [m1-proposal-deck.md](../../../presentation/m1-proposal-deck.md) — second authority; the report wins on conflict.
3. [Car-Sky-Platform.html](../../../requirements/development-platform-doc/Car-Sky-Platform.html) — platform mechanics (blueprint/node/pin, deploy flow).
4. [BTC_phan_hoi_V2X_team.pdf](../../../requirements/development-platform-doc/BTC_phan_hoi_V2X_team.pdf) — organizers' advisory.

Plus the active plan [milestone1.md](../../../plans/milestone1.md) for the requirement's phase, phase objective, and phase acceptance criteria.

## Procedure

1. **Locate the requirement.** Find Rx in the report §2 and the phase that delivers it in [milestone1.md](../../../plans/milestone1.md) §5 — the phase number is the `[Phase]` segment of the output file name.
2. **Extract the implementation constraints.** From the reference documentation: what the requirement must satisfy (definition + acceptance), the selected tech stack, dependent contracts (R1–R6), and platform facts that shape the implementation (node type, pins, deploy flow).
3. **Enumerate the implementation steps.** A numbered list in dependency order. Each step: concise, one action, self-contained enough for an implementing agent to act without re-reading the wider codebase — name the component/interface touched, what to build or change, and the check that proves the step done.
4. **Save one file per requirement** in [plans/](../../../plans/) (same folder as milestone1.md), named `Rx_[Phase].md` — e.g. `plans/R7_1.md` for R7 delivered in Phase 1.
5. **Hand off to [[project-planner]]**, which decomposes the steps into `X.Y.Z.W` tasks/subtasks per [task-planning-conventions.md](../../rules/task-planning-conventions.md) and orchestrates implementation. The steps are planner input — this skill produces no task IDs and spawns no implementation.

## File format (`plans/Rx_[Phase].md`)

- **Title:** `# Rx — <requirement name> (Phase <n>)`.
- **References:** links to Rx in the report and to its phase in milestone1.md; any other authority documents the steps rely on.
- **Implementation steps:** the enumerated list from step 3 — the core of the file.
- **Acceptance mapping:** which steps satisfy which of Rx's acceptance items — a step list that leaves an acceptance item uncovered is incomplete.

Write per [markdown-writing-style](../markdown-writing-style/SKILL.md). Commit with the pre-decomposition X-only tag from [task-planning-conventions.md](../../rules/task-planning-conventions.md): `[<x>] docs: propose implementation steps for R<x>`.

## How to apply

[[project-researcher]] runs this instead of [requirement-analysis-and-solutioning](../requirement-analysis-and-solutioning/SKILL.md) whenever the request states the requirements are already analysed and asks for implementation guidance. If a requirement turns out to be ambiguous or contradicts the report during step 2, flag it back to the user — do not slide into re-analysis.
