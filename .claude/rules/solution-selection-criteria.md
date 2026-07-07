---
name: solution-selection-criteria
description: Hard constraints (open-source only, Linux-targeted) and the ranked criteria for picking the best toolchain/framework/language/library solution. Governs project-researcher's solutioning steps.
---

# Solution Selection Criteria

Governs every solution [[project-researcher]] proposes or picks via [requirement-analysis-and-solutioning](../skills/requirement-analysis-and-solutioning/SKILL.md). Applies uniformly to toolchain, framework, programming-language, and 3rd-party-library choices.

## Hard constraints (disqualifying — apply before comparison)

- **Open-source only.** Commercial or paid libraries/tools must not be used, proposed, or shortlisted — not even as a "better but rejected" option in a written comparison.
- **Linux environment.** Every solution must target and run on Linux. Do not propose Windows-only or otherwise platform-locked tooling.

A candidate failing either constraint is disqualified before it reaches the comparison step — it is not a valid "available solution" under [requirement-analysis-and-solutioning](../skills/requirement-analysis-and-solutioning/SKILL.md).

## Best-solution criteria (ranked)

Once candidates pass the hard constraints, pick the best by weighing, in this priority order:

1. **High possibility of accomplishing the implementation.** Prefer the solution most likely to actually work end-to-end over one that is theoretically superior but riskier or unproven for this use case.
2. **Fastest, easiest implementation of the current milestone.** Prefer the option that gets the current milestone's acceptance criteria met with the least implementation effort and time.
3. **Possibility of implementing future features.** Prefer the option that doesn't foreclose likely next-milestone work (see the deferred-scope section of the active plan doc). This criterion ranks below #1 and #2 — don't over-engineer for hypothetical future needs at the cost of the current milestone.
4. **Smaller open-source library preferred.** Among functionally-equivalent options, prefer the smaller / narrower-scope library over a larger one whose extra surface isn't needed. Example: use standalone `asio` instead of `boost` when only Asio's API is needed — pulling in all of Boost for one component is disfavored.

Earlier-numbered criteria take priority when they conflict; criterion 4 is a tie-breaker among otherwise-comparable options, not a veto over 1–3.

## How to apply

[[project-researcher]] applies this at the "propose solutions" and "compare & pick" steps of [requirement-analysis-and-solutioning](../skills/requirement-analysis-and-solutioning/SKILL.md). Any written solution comparison must state which of the four criteria drove the pick, not just name the winner.
