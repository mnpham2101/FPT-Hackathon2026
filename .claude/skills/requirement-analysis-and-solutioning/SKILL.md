---
name: requirement-analysis-and-solutioning
description: Procedure project-researcher follows whenever it receives a feature or a set of requirements — feasibility check, requirement enumeration/ordering, solution proposal, and solution comparison/selection.
---

# Requirement Analysis & Solutioning (project-researcher)

Trigger: [[project-researcher]] receives a feature or a set of requirements to analyze. Follow
this procedure in order — don't skip to a solution before feasibility and enumeration are done.

## Procedure

1. **Analyse feasibility.**
   Before decomposing anything, sanity-check whether the input is realistically achievable at all
   — against the milestone timeline, the active plan's scope/assumptions
   ([milestone1.md](../../plans/milestone1.md) sections 2 and 6), and the hard constraints in
   [solution-selection-criteria.md](../../rules/solution-selection-criteria.md) (open-source only,
   Linux-targeted). Flag — don't silently drop — anything that looks infeasible as stated, with
   reasoning.

2. **Determine input type, then enumerate.**
   - **Given a feature:** break it down into **enumerated requirements**. Each requirement must be
     optimized for a **clear, unambiguous goal** — one requirement, one testable outcome. Split
     compound "and" requirements; eliminate vague language.
   - **Given requirements already:** enumerate them as a numbered list (formalize if not already
     one), then **re-order** them by whichever is more relevant to the request — **urgency**
     (what blocks other work / what the milestone needs first) or **ease of implementation**
     (what unblocks fast wins) — and state explicitly which ordering was used and why.

3. **Propose several available solutions** for each requirement (or requirement group) that needs
   a technical decision — toolchain, framework, language, or 3rd-party library. Always generate
   more than one real candidate; a single-option "proposal" is not a comparison. Every candidate
   must pass the hard constraints in
   [solution-selection-criteria.md](../../rules/solution-selection-criteria.md) before it is
   shortlisted — reject disqualified candidates before spending analysis time on them.

4. **Compare the solutions and pick the best one**, scored against the four ranked criteria in
   [solution-selection-criteria.md](../../rules/solution-selection-criteria.md): possibility of
   accomplishing the implementation, speed/ease for the current milestone, extensibility to future
   features, and preference for the smaller open-source dependency when a heavier one isn't
   functionally needed. Document the comparison itself, not just the winning pick.

## Output

- A feasibility verdict (with reasoning) for the input feature/requirement set.
- An enumerated, unambiguous requirements list — reordered by urgency or ease of implementation
  when the input was already requirements, with the ordering rationale stated.
- For each requirement needing a technical decision: the candidate solutions considered, a
  comparison against the four ranked criteria, and the selected solution with rationale.

## How to apply

Owned by [[project-researcher]] — see
[project-researcher.md](../../agents/project-researcher.md). Solution proposals and selections
must honor [solution-selection-criteria.md](../../rules/solution-selection-criteria.md) without
exception.
