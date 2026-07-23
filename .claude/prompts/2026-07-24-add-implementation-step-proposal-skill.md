# Prompt — Add implementation-step-proposal skill for project-researcher

**Date:** 2026-07-24 **Requested by:** mnpham1986@gmail.com

## Prompt text

> When invoked to study new a analysed requirements AND propose implementation, Project-researcher's reference documentation should includes all document listed in Document authority ion in Claude.md to propose implementation steps and then provide project-planner the information to create tasks and implement the selected solution.
>
> Store the information in a file, 1 file per requirement, in folder `plans` (same as milestone1.md). Naming convention follows `Rx_[Phase]`. The steps must be enummerated. The description for each step be concise, enough for other agent to implement.
>
> The requirements are already analysed, don't perform requirement analysis. Don't need to use SKILL in requirement-analysis-and-solutioning, requirement-quality-criteria.md, solution-selection-criteria.md, research-report-format.md. Instead just do the above request to provide guide for implementation.
>
> Fix either project-researcher.md or add the corresponding SKILL file to guide the agent.
>
> Save this prompt.

## Outcome

Created [.claude/skills/implementation-step-proposal/SKILL.md](../skills/implementation-step-proposal/SKILL.md) — the procedure project-researcher follows when requirements are already analysed and the ask is implementation guidance:

- **Reference documentation** — all four CLAUDE.md Document-authority documents in authority order, plus [plans/milestone1.md](../../plans/milestone1.md) for the requirement's phase and acceptance criteria.
- **Procedure** — locate Rx + its phase, extract implementation constraints, enumerate dependency-ordered steps (one action each, self-contained enough for an implementing agent), save as `plans/Rx_[Phase].md` (e.g. `plans/R7_1.md`), hand off to project-planner for `X.Y.Z.W` decomposition.
- **Explicit skips** — no requirement-analysis-and-solutioning run, no requirement-quality-criteria / solution-selection-criteria / research-report-format pass; flag ambiguities back instead of re-analysing.
- **File format** — title, references, enumerated implementation steps, acceptance mapping (uncovered acceptance item = incomplete step list).

Wired the mode into [project-researcher](../agents/project-researcher.md): frontmatter description, a Scope-of-work bullet routing already-analysed-requirement requests to the new skill, and the Outputs section listing the `plans/Rx_[Phase].md` deliverable.
