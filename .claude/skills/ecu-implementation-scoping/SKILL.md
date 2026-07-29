---
name: ecu-implementation-scoping
description: Procedure project-architecture follows when asked to design against a phase or requirement rather than an already folder-scoped feature — resolves which ECU folder(s) (ADA_ECU, V2X_ECU, IVI_ECU) the work belongs to, investigates their current state, then hands off into high-level-design-procedure.
---

# ECU Implementation Scoping (project-architecture)

Trigger: [[project-architecture]] is asked to design for a **phase or requirement** (e.g. "design Phase 3", "architect R12") rather than being handed an already folder-scoped feature + solution — the entry point one step before [high-level-design-procedure](../high-level-design-procedure/SKILL.md), which assumes the target folder is already known.

## Procedure

1. **Resolve phase/requirement → ECU node(s).** Cross-reference the requested phase in [milestone1.md](../../../plans/milestone1.md) §5 and the requirement's home ECU in the report ([m1-cooperative-awareness.md](../../../requirements/m1-cooperative-awareness.md) §2) against the fixed R5 node map — one top-level repo folder per CarSky node:

   | ECU folder | CarSky node type | Requirements it hosts |
   |---|---|---|
   | `V2X_ECU/` | Container Node | R1, R7–R11 |
   | `ADA_ECU/` | Container Node | R3, R12–R15 |
   | `IVI_ECU/` | Skycraft Node (AAOS) | R4, R16–R17 |

   Multi-ECU phases (e.g. Phase 0 contracts, Phase 6 convergence) implicate more than one folder — list every folder touched, not just the first match.

2. **Investigate each implicated folder's current state** (Glob/Read) — what exists, what's missing, and whether it matches the report's §3 per-ECU tech stack (V2X: C++17; ADA: C++17 core + Python detector subprocess; IVI: Kotlin/Jetpack Compose/AndroidX). Flag mismatches back to the user instead of silently reconciling.
3. **Consult [Car-Sky-Platform.html](../../../requirements/development-platform-doc/Car-Sky-Platform.html)** for blueprint/node/pin mechanics whenever the folder's deployment shape (OCI image layout, AAOS module layout) is in question.
4. **Hand off** the resolved folder(s) + current-state findings as the "feature set + chosen solution" input to [high-level-design-procedure](../high-level-design-procedure/SKILL.md) — that skill's own folder-analysis step then operates inside the folder(s) this step resolved, not the repo root.

## Output

- The ECU folder(s) implicated by the phase/requirement, with current-state findings and any tech-stack mismatches flagged.
- No HLD content and no folder creation here — that's [high-level-design-procedure](../high-level-design-procedure/SKILL.md)'s job once this step hands off.

## How to apply

Owned by [[project-architecture]]. Run only when the request is phase/requirement-driven without a named folder scope; skip straight to [high-level-design-procedure](../high-level-design-procedure/SKILL.md) when a folder-scoped feature + solution is already in hand. Produces folder/module suggestions only — no task IDs, no implementation, no subagent spawning; task/subtask breakdown stays [[project-planner]]'s job.
