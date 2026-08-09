---
name: high-level-design-procedure
description: Procedure project-architecture follows when given a feature set + chosen (1st-choice) solution, or when invoked as a subagent by project-researcher — MVC-separated design, folder structure analysis/creation, reading the requirement documents in full, research-note sourcing, HLD authoring, committing the design, and handing off to project-planner for task decomposition.
---

# High-Level Design Procedure (project-architecture)

Trigger: [[project-architecture]] receives (a) a set of features and a chosen solution — the 1st-choice solution produced by [[project-researcher]]'s [requirement-analysis-and-solutioning](../requirement-analysis-and-solutioning/SKILL.md) — or (b) is invoked directly as a subagent by [[project-researcher]] once that solution is picked. If instead the request is a bare phase or requirement with no folder-scoped feature yet, run [ecu-implementation-scoping](../ecu-implementation-scoping/SKILL.md) first to resolve the target ECU folder(s), then continue here.

## Procedure

1. **Follow MVC architecture.** Keep UI, data, UI logic, and business logic in separate, independently-replaceable layers:
   - **Data** — persistence, schemas, stores (e.g. the TrackedObject store).
   - **Business logic** — domain rules and transformations (e.g. distance estimation, the gate state machine, message composition) — independent of how data is stored or displayed.
   - **UI logic** — the controller/view-model layer mediating between business logic and presentation.
   - **UI** — the rendering/display surface (e.g. camera overlay, BEV). No design may collapse two of these layers into one module for convenience.

2. **Analyse the current folder structure.** Read the existing repo layout — for M1, one top-level folder per R5 node (`Scenario_Player/`, `V2X_ECU/`, `ADA_ECU/`, `IVI_ECU/`), mapped in [CLAUDE.md § Repository layout](../../../CLAUDE.md) and already resolved by [ecu-implementation-scoping](../ecu-implementation-scoping/SKILL.md) if this run started from a phase/requirement — before proposing anything new. Design the module structure *inside* the resolved folder, honoring the node build rules (self-contained folder, no cross-node imports, no hardcoded tunables). If the target folder or its structure doesn't exist yet, create it as part of this step — don't defer structure creation to a later, unspecified point.

3. **Read the requirement documents in full — before any design decision.** Every requirement the node serves, read whole: definition, dependency, acceptance and tech stack, plus the figures those entries embed, the frozen contract files they name, the report's §4 decision record, and any later report under [requirements/](../../../requirements/) that adds requirement numbers or defers work touching this node. The requirements decide what the node must do; the design only decides how. **Skimming for requirement numbers is what produces a design that meets the headline and contradicts an acceptance clause** — an acceptance criterion or a standing decision missed here surfaces as rework after implementation. What was read, and what each document fixed for the node, becomes §2 of the HLD per [hld-content-and-commit-format.md](../../rules/hld-content-and-commit-format.md).

4. **Source existing research notes.** Check [documents/KnowledgeBase/](../../../documents/KnowledgeBase/), the resolved node's folder under [documents/Design/](../../../documents/Design/), and `plans/doc/research_notes/` — the locations defined in [CLAUDE.md § Repository layout](../../../CLAUDE.md) — for notes already covering part of the feature (e.g. [baseline-connectivity-smoke-test.md](../../../documents/Delivery/Test-Guides/baseline-connectivity-smoke-test.md) for the Phase 0 smoke test). Reference them from the HLD — link plus a one-line statement of what is adopted — instead of re-deriving or restating their content. Notes are non-authoritative scratch: on any conflict, the CLAUDE.md document-authority order wins.

5. **Make the high-level design (HLD)** for the feature(s) against the chosen solution — module boundaries, data flow, and how the MVC layers connect. **One HLD per R5 node, at `documents/Design/<NODE>/<node-slug>-hld.md`**, with its decision record and diagram sources beside it — a later phase extends that document rather than adding a second one. The node's own folder holds no design ([CLAUDE.md § Repository layout](../../../CLAUDE.md)). Its mandatory section list and order, the per-section content, the diagram and writing rules are governed by [hld-content-and-commit-format.md](../../rules/hld-content-and-commit-format.md), whose worked example is [ivi-ecu-hld.md](../../../documents/Design/IVI-ECU/ivi-ecu-hld.md) — outline against that section list before writing, and check the document against it again before step 6.

6. **Propose the HLD, then commit it.** Present the HLD for review — to the user, or to [[project-researcher]] if invoked as its subagent — then commit the design document as an atomic commit, tagged per [hld-content-and-commit-format.md](../../rules/hld-content-and-commit-format.md). Pause for that reviewer's explicit approval before committing when the design designates a file location outside the sanctioned locations ([CLAUDE.md § Repository layout](../../../CLAUDE.md)) or overrides a decision in the report's §4 decision record ([m1-cooperative-awareness.md §4](../../../documents/Requirements/m1-cooperative-awareness.md)); in all other cases the pause is optional — pause when uncertain, otherwise commit directly.

7. **Hand off to [[project-planner]].** Invoke [[project-planner]] (Agent tool) with a self-contained brief — HLD file path + commit hash, requirement numbers served, resolved node folder(s), file-location designations, referenced research notes, and open items — so it decomposes the design into tasks/subtasks per [task-planning-conventions.md](../../rules/task-planning-conventions.md). When running as a subagent that cannot invoke another agent, instead return that same brief to the invoker with the explicit instruction to pass it to [[project-planner]]. Either delivery completes the procedure; without one, the procedure is incomplete.

## Output

- Folder structure (created if missing, otherwise confirmed/extended).
- An HLD document with clearly separated MVC layers, referencing (not restating) any sourced research notes.
- A committed HLD, `[X] design: ...` tagged per the project commit format.
- A handoff brief delivered by invoking [[project-planner]] (step 7).

## How to apply

Owned by [[project-architecture]] — see [project-architecture.md](../../agents/project-architecture.md). HLD content shape and the commit message format are governed by [hld-content-and-commit-format.md](../../rules/hld-content-and-commit-format.md) — do not restate those rules here.
