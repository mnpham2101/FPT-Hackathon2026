---
name: hld-content-and-commit-format
description: What an HLD markdown document must and may contain (folder structure, tech stack, PlantUML call-flow/component/activity diagrams) and how project-architecture commits it, per the project's commit message format.
---

# HLD Content & Commit Format

Governs the artifact [[project-architecture]] produces at the "make the HLD" and "propose HLD, commit design" steps of [high-level-design-procedure](../skills/high-level-design-procedure/SKILL.md).

## HLD content

Every HLD is a markdown document. It must include:

- **Folder structure map** — the module/directory layout implementing the MVC separation, scoped to the ECU folder(s) the design touches (`ADA_ECU/`, `V2X_ECU/`, `IVI_ECU/` — resolved by [ecu-implementation-scoping](../skills/ecu-implementation-scoping/SKILL.md) when the design started from a phase/requirement rather than a named folder).
- **Tech stack** — the languages/libraries/frameworks the design uses, traceable to the report's §3 per-ECU tech stack.
- **Call-flow diagram** (PlantUML sequence diagram) — interaction over time between components (e.g. B detects C → broadcasts → A receives → composes → displays).

Include the following only when they clarify the design further — not mandatory in isolation:

- **Component diagram** (PlantUML) — modules/services and their dependencies.
- **Activity diagram** (PlantUML) — control/data flow through a process (e.g. detection → distance → gate → relay).

Diagrams are written as `.puml` source, not pre-rendered images, so they stay diffable and reviewable in the same commit as the design text.

## Commit format

HLD commits follow the project-wide format defined in [task-planning-conventions.md](task-planning-conventions.md#commit-message-format):

```
[<taskID>] <type>: <subject>
```

using `type = design` and the **requirement-only `X`** form of the taskID — HLD happens before phase/task/subtask decomposition exists for that requirement, so `Y.Z.W` aren't assignable yet.

## How to apply

[[project-architecture]] applies this at every "propose HLD, commit design" step. Do not commit an HLD without its `[X] design: ...` tagged message, and do not invent a different commit message shape for design commits.
