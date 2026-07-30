---
name: high-level-design-procedure
description: Procedure project-architecture follows when given a feature set + chosen (1st-choice) solution, or when invoked as a subagent by project-researcher — research-note sourcing, MVC-separated design, folder structure analysis/creation, HLD authoring, committing the design, and handing off to project-planner for task decomposition.
---

# High-Level Design Procedure (project-architecture)

Trigger: [[project-architecture]] receives (a) a set of features and a chosen solution — the 1st-choice solution produced by [[project-researcher]]'s [requirement-analysis-and-solutioning](../requirement-analysis-and-solutioning/SKILL.md) — or (b) is invoked directly as a subagent by [[project-researcher]] once that solution is picked. If instead the request is a bare phase or requirement with no folder-scoped feature yet, run [ecu-implementation-scoping](../ecu-implementation-scoping/SKILL.md) first to resolve the target ECU folder(s), then continue here.

## Procedure

1. **Follow MVC architecture.** Keep UI, data, UI logic, and business logic in separate, independently-replaceable layers:
   - **Data** — persistence, schemas, stores (e.g. the TrackedObject store).
   - **Business logic** — domain rules and transformations (e.g. distance estimation, the gate state machine, message composition) — independent of how data is stored or displayed.
   - **UI logic** — the controller/view-model layer mediating between business logic and presentation.
   - **UI** — the rendering/display surface (e.g. camera overlay, BEV). No design may collapse two of these layers into one module for convenience.

2. **Analyse the current folder structure.** Read the existing repo layout — for M1, one top-level folder per R5 node (`Scenario_Player/`, `V2X_ECU/`, `ADA_ECU/`, `IVI_ECU/`), mapped in [node-code-layout.md](../../rules/node-code-layout.md) and already resolved by [ecu-implementation-scoping](../ecu-implementation-scoping/SKILL.md) if this run started from a phase/requirement — before proposing anything new. Design the module structure *inside* the resolved folder, honoring that rule's build rules (self-contained folder, no cross-node imports, no hardcoded tunables). If the target folder or its structure doesn't exist yet, create it as part of this step — don't defer structure creation to a later, unspecified point.

3. **Source existing research notes.** Check `doc/research_notes/` under `plans/` and the node folders (`Scenario_Player/`, `V2X_ECU/`, `ADA_ECU/`, `IVI_ECU/`) for notes already covering part of the feature (e.g. [baseline-connectivity-smoke-test.md](../../../plans/doc/research_notes/baseline-connectivity-smoke-test.md) for the Phase 0 smoke test). Reference them from the HLD — link plus a one-line statement of what is adopted — instead of re-deriving or restating their content. Notes are non-authoritative scratch: on any conflict, the CLAUDE.md document-authority order wins.

4. **Make the high-level design (HLD)** for the feature(s) against the chosen solution — module boundaries, data flow, and how the MVC layers connect. Required content (folder map with file-location designations, tech stack, call-flow diagram) and optional content is governed by [hld-content-and-commit-format.md](../../rules/hld-content-and-commit-format.md).

5. **Propose the HLD, then commit it.** Present the HLD for review — to the user, or to [[project-researcher]] if invoked as its subagent — then commit the design document as an atomic commit, tagged per [hld-content-and-commit-format.md](../../rules/hld-content-and-commit-format.md). May pause for explicit user approval before committing — do so when the design designates a file location outside the four node folders or overrides a recorded decision; otherwise commit directly.

6. **Hand off to [[project-planner]].** Invoke [[project-planner]] (Agent tool) with a self-contained brief — HLD file path + commit hash, requirement numbers served, resolved node folder(s), file-location designations, referenced research notes, and open items — so it decomposes the design into tasks/subtasks per [task-planning-conventions.md](../../rules/task-planning-conventions.md). The procedure is not complete until this handoff is made.

## Output

- Folder structure (created if missing, otherwise confirmed/extended).
- An HLD document with clearly separated MVC layers, referencing (not restating) any sourced research notes.
- A committed HLD, `[X] design: ...` tagged per the project commit format.
- A handoff brief delivered by invoking [[project-planner]] (step 6).

## How to apply

Owned by [[project-architecture]] — see [project-architecture.md](../../agents/project-architecture.md). HLD content shape and the commit message format are governed by [hld-content-and-commit-format.md](../../rules/hld-content-and-commit-format.md) — do not restate those rules here.
