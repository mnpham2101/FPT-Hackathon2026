---
name: dev-environment-research
description: Procedure project-researcher follows when asked about the development/working environment, platform capabilities, toolchain, or library support (e.g. "how does tool X support Y", "what environment does Phase N need") — source ingestion, tool-support research, requirement mapping, and a diagrammed research note.
---

# Development Environment & Toolchain Research (project-researcher)

Trigger: [[project-researcher]] receives a question about the working environment, platform
(e.g. CARSKY), toolchain, or a specific library/tool — narrower than a full
[requirement-analysis-and-solutioning](../requirement-analysis-and-solutioning/SKILL.md) run:
it clarifies *how existing requirements will be met in a concrete environment*, and defines
**no new requirement numbers**.

## Procedure

1. **Ingest the given sources first** — platform docs (PDF/slides), mentor/organizer Q&A,
   the active plan ([milestone1.md](../../../plans/milestone1.md)), and existing reports under
   `requirements/`. Extract the environment model as explicit facts (what the platform provides,
   what the team must build, what is out of scope).
2. **Research tool/library support** (WebSearch/WebFetch) for each open question. Candidates
   must pass the hard constraints in
   [solution-selection-criteria.md](../../rules/solution-selection-criteria.md) (open-source,
   Linux); note version/feature caveats (e.g. a library implementing an older spec revision)
   rather than assuming from memory.
3. **Map findings to existing requirement numbers** from the reports in `requirements/`.
   Flag — never silently absorb — any conflict with a frozen contract, the plan's
   scope/assumptions, or a published requirement; name the decision owner.
4. **Write the research note** under `requirements/` (e.g. `m1-<topic>.md`), clearly marked as
   a researcher artifact, **not an HLD**: environment model, module/role mapping with
   requirement numbers, tool-support findings, open items, and **sources** (links).
5. **Diagram the environment and flow** as `.puml` files next to the note — typically a
   component diagram (working environment / nodes / modules) and a swimlane activity diagram
   (end-to-end flow). Validate the PlantUML renders before delivering; keep activity labels on
   single lines so boxes use full width.

## Output

- A research note + `.puml` diagrams under `requirements/`, cross-referenced from the plan doc
  where relevant. Conversation output alone is not a deliverable.
- Commit (when asked) as `docs: <subject>` — no taskID, per
  [research-report-format.md](../../rules/research-report-format.md).

## How to apply

Owned by [[project-researcher]]. If the research uncovers a genuinely *new* requirement or a
solution decision needing formal comparison, escalate to
[requirement-analysis-and-solutioning](../requirement-analysis-and-solutioning/SKILL.md) instead
of extending this note. Design consequences hand off to [[project-architecture]].
