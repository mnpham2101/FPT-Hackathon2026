---
name: requirement-analysis-and-solutioning
description: Procedure project-researcher follows whenever it receives a feature or a set of requirements — feasibility check, requirement enumeration/ordering, solution proposal, and solution comparison/selection.
---

# Requirement Analysis & Solutioning (project-researcher)

Trigger: [[project-researcher]] receives a feature or a set of requirements to analyze. Follow this procedure in order — don't skip to a solution before feasibility and enumeration are done.

## Procedure

1. **Analyse feasibility.** Before decomposing anything, sanity-check whether the input is realistically achievable at all — against the milestone timeline, the active plan's scope/assumptions ([milestone1.md](../../plans/milestone1.md) sections 2 and 6), and the hard constraints in [solution-selection-criteria.md](../../rules/solution-selection-criteria.md) (open-source only, Linux-targeted). Flag — don't silently drop — anything that looks infeasible as stated, with reasoning.

2. **Determine input type, then enumerate.**
   - **Given a feature:** break it down into **enumerated requirements**. Each requirement must be optimized for a **clear, unambiguous goal** — one requirement, one testable outcome. Split compound "and" requirements; eliminate vague language.
   - **Given requirements already:** enumerate them as a numbered list (formalize if not already one), then **re-order** them by whichever is more relevant to the request — **urgency** (what blocks other work / what the milestone needs first) or **ease of implementation** (what unblocks fast wins) — and state explicitly which ordering was used and why.

In both cases, every enumerated requirement must meet [requirement-quality-criteria.md](../../rules/requirement-quality-criteria.md): its own feasibility verdict, a concrete measurable output (numeric measurement / KPI), and vague wording translated to precise, testable statements.

3. **Propose several available solutions** for each requirement (or requirement group) that needs a technical decision — toolchain, framework, language, or 3rd-party library. Always generate more than one real candidate; a single-option "proposal" is not a comparison. Every candidate must pass the hard constraints in [solution-selection-criteria.md](../../rules/solution-selection-criteria.md) before it is shortlisted — reject disqualified candidates before spending analysis time on them.

4. **Compare the solutions and pick the best one**, scored against the four ranked criteria in [solution-selection-criteria.md](../../rules/solution-selection-criteria.md): possibility of accomplishing the implementation, speed/ease for the current milestone, extensibility to future features, and preference for the smaller open-source dependency when a heavier one isn't functionally needed. Document the comparison itself, not just the winning pick.

## Cache

Trigger: after reading any external resource (a URL, or a file outside this repo) during the procedure above.

- Write a cache file under `.claude/references/<topic-slug>.md` — one file per resource, named for its subject, not its source.
- Cache file must contain: source (URL/path), keywords for future lookup, and a summary of the content relevant to the current task — not a full copy of the source.
- Before fetching a resource, check `.claude/references/` for an existing cache file on the same topic and reuse it instead of re-fetching.

## Adversarial review record (multi-researcher runs)

Trigger: the run is executed as an adversarial debate between researcher personas (e.g. Alpha the pragmatist vs. Beta the standards/extensibility reviewer), in numbered rounds.

- All debate artifacts for round `N` live under `.claude/prompts/scratchpad-requirement-analysis-round<N>/` — position papers, rebuttals, and the round's record file.
- Close every round by writing **`adversarial-review-record.md` in that same directory**: the round's contested points, each with the positions taken and how it resolved, plus the round's key external sources. Contested-point numbering continues across rounds (round 2 continues where round 1 stopped) so cross-references stay stable.
- The report under `requirements/` carries only a **pointer appendix** — a section titled `Appendix — adversarial requirement analysis & solution proposal record` (currently [m1-cooperative-awareness.md §6](../../../requirements/m1-cooperative-awareness.md)) with one bullet per round linking to that round's record file. Never inline the full record in the report; never let the record drift from the report's converged text.
- Converged outcomes (requirement changes, picks, flags) still land in the report body per [research-report-format.md](../../rules/research-report-format.md) — the record is the *why* behind them, not a second home for them.

## Output

- A feasibility verdict (with reasoning) for the input feature/requirement set.
- An enumerated, unambiguous requirements list — each requirement carrying the checks in [requirement-quality-criteria.md](../../rules/requirement-quality-criteria.md) (feasibility verdict, measurable output/KPI, vague → precise translations) — reordered by urgency or ease of implementation when the input was already requirements, with the ordering rationale stated.
- For each requirement needing a technical decision: the candidate solutions considered, a comparison against the four ranked criteria, and the selected solution with rationale.

All of the above is delivered as a report file under `requirements/` per [research-report-format.md](../../rules/research-report-format.md) — the run is not complete until that report is written.

## How to apply

Owned by [[project-researcher]] — see [project-researcher.md](../../agents/project-researcher.md). Solution proposals and selections must honor [solution-selection-criteria.md](../../rules/solution-selection-criteria.md) without exception.
