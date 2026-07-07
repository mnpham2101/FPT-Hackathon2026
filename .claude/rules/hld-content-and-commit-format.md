---
name: hld-content-and-commit-format
description: What an HLD document may contain (folder structure map, PlantUML component/activity/sequence diagrams) and how project-architecture commits it, per the project's commit message format.
---

# HLD Content & Commit Format

Governs the artifact [[project-architecture]] produces at the "make the HLD" and "propose HLD, commit design" steps of [high-level-design-procedure](../skills/high-level-design-procedure/SKILL.md).

## HLD content

An HLD document may include all or some of the following, as fits the feature/solution being designed — not every HLD needs every artifact, and none of these are mandatory in isolation:

- **Folder structure map** — the module/directory layout implementing the MVC separation.
- **Component diagram** (PlantUML) — modules/services and their dependencies.
- **Activity diagram** (PlantUML) — control/data flow through a process (e.g. detection → distance → gate → relay).
- **Sequence diagram** (PlantUML) — interaction over time between components (e.g. B detects C → broadcasts → A receives → composes → displays).

Diagrams are written as `.puml` source, not pre-rendered images, so they stay diffable and reviewable in the same commit as the design text.

## Commit format

HLD commits follow the project-wide format defined in [task-planning-conventions.md](task-planning-conventions.md#commit-message-format):

```
[<taskID>] <type>: <subject>
```

using `type = design` and the **requirement-only `X`** form of the taskID — HLD happens before phase/task/subtask decomposition exists for that requirement, so `Y.Z.W` aren't assignable yet.

## How to apply

[[project-architecture]] applies this at every "propose HLD, commit design" step. Do not commit an HLD without its `[X] design: ...` tagged message, and do not invent a different commit message shape for design commits.
