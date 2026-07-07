# Prompt — Add requirement quality criteria rule for project-researcher

**Date:** 2026-07-04 **Requested by:** mnpham1986@gmail.com

## Prompt text

> Add rules to require project-researcher to analyse all requirements for feasibility and concrete measurable outputs such as numeric measurements, KPIs. Translate vague requirements to precise requirements. Make it succint
>
> save this prompt to prompts

## Outcome

Created [.claude/rules/requirement-quality-criteria.md](../rules/requirement-quality-criteria.md) with three checks every requirement must pass before handoff to project-architecture / project-planner:

- **Per-requirement feasibility** — each enumerated requirement (not just the input as a whole) gets its own achievable / at-risk / infeasible verdict with reasoning; at-risk/infeasible items are flagged, never silently dropped.
- **Concrete measurable outputs** — every requirement states a numeric measurement, KPI, threshold, or binary observable check that proves it is met.
- **Vague → precise** — vague wording is translated into testable statements during enumeration, recorded as `original → precise` so the user can veto the interpretation; proposed numbers without a defensible basis are marked as assumptions to confirm.

Wired the rule into [project-researcher](../agents/project-researcher.md) and steps 1–2 of [requirement-analysis-and-solutioning](../skills/requirement-analysis-and-solutioning/SKILL.md), and referenced it from [research-report-format.md](../rules/research-report-format.md) so report requirements must meet it.
