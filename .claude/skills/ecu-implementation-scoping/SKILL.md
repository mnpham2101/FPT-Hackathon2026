---
name: ecu-implementation-scoping
description: Procedure project-architecture follows when asked to design against a phase or requirement rather than an already folder-scoped feature — resolves which node folder(s) (Scenario_Player, V2X_ECU, ADA_ECU, IVI_ECU) the work belongs to, investigates their current state, then hands off into high-level-design-procedure.
---

# ECU Implementation Scoping (project-architecture)

Trigger: [[project-architecture]] is asked to design for a **phase or requirement** (e.g. "design Phase 3", "architect R12") rather than being handed an already folder-scoped feature + solution — the entry point one step before [high-level-design-procedure](../high-level-design-procedure/SKILL.md), which assumes the target folder is already known.

## Procedure

1. **Resolve phase/requirement → node(s).** Cross-reference the requested phase in [milestone1_high_level_plan.md](../../../documents/Plan/milestone1_high_level_plan.md) §5 and the requirement's home node in the report ([m1-cooperative-awareness.md](../../../requirements/m1-cooperative-awareness.md) §2) against the folder → node → requirement map in [node-code-layout.md](../../rules/node-code-layout.md), which also fixes each node's language and build artifact.

   Two traps that table exists to prevent: the bench Scenario Player is its own node and its own folder, so R11 work never lands in `V2X_ECU/`; and multi-node phases (Phase 0 contracts, Phase 1 comms bring-up, Phase 6 convergence) implicate more than one folder — list every folder touched, not just the first match.

2. **Investigate each implicated folder's current state** (Glob/Read) — what exists, what's missing, and whether it matches the report's §3 per-node tech stack (bench: Python; V2X: C++17; ADA: C++17 core + Python detector subprocess; IVI: Kotlin/Jetpack Compose/AndroidX). Flag mismatches back to the user instead of silently reconciling.
3. **Consult [Car-Sky-Platform.html](../../../requirements/development-platform-doc/Car-Sky-Platform.html)** for blueprint/node/pin mechanics whenever the folder's deployment shape (OCI image layout, AAOS module layout) is in question.
4. **Hand off** the resolved folder(s) + current-state findings as the "feature set + chosen solution" input to [high-level-design-procedure](../high-level-design-procedure/SKILL.md) — that skill's own folder-analysis step then operates inside the folder(s) this step resolved, not the repo root.

## Output

- The node folder(s) implicated by the phase/requirement, with current-state findings and any tech-stack mismatches flagged.
- No HLD content and no folder creation here — that's [high-level-design-procedure](../high-level-design-procedure/SKILL.md)'s job once this step hands off.

## How to apply

Owned by [[project-architecture]]. Run only when the request is phase/requirement-driven without a named folder scope; skip straight to [high-level-design-procedure](../high-level-design-procedure/SKILL.md) when a folder-scoped feature + solution is already in hand. Produces folder/module suggestions only — no task IDs, no implementation, no subagent spawning; task/subtask breakdown stays [[project-planner]]'s job.
