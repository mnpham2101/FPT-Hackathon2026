# Requirement Quality Criteria

Governs every requirement [[project-researcher]] produces via [requirement-analysis-and-solutioning](../skills/requirement-analysis-and-solutioning/SKILL.md). A requirement failing any check below is not ready to hand to [[project-architecture]] or [[project-planner]].

## Per-requirement feasibility

Every enumerated requirement — not just the input as a whole — gets its own feasibility verdict (**achievable / at-risk / infeasible**) with brief reasoning against the milestone timeline, the active plan's scope/assumptions, and the hard constraints in [solution-selection-criteria.md](solution-selection-criteria.md). Flag at-risk/infeasible items — never silently drop or absorb them.

## Concrete measurable outputs

Every requirement must state a **measurable output**: a numeric measurement, KPI, threshold, or binary observable check that proves it is met — e.g. "relay latency ≤ 200 ms end-to-end", "C rendered on A's display within 2 s of B's first detection". A requirement whose completion cannot be verified by such a check is not done being analysed.

## Vague → precise

Translate vague wording ("fast", "reliable", "robust", "handle errors") into precise, testable statements during enumeration. Record each translation as `original → precise` so the user can veto the interpretation. If no defensible number exists yet, propose one and mark it as an assumption to confirm — never leave the requirement vague.

## How to apply

[[project-researcher]] applies this at steps 1–2 (feasibility, enumeration) of [requirement-analysis-and-solutioning](../skills/requirement-analysis-and-solutioning/SKILL.md). Its enumerated requirements output must show, per requirement: the feasibility verdict, the measurable output, and any vague → precise translations made.
