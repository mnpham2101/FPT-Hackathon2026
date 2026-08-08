---
name: carsky-deployment-guide
description: Procedure project-architecture follows when asked to investigate/set up the virtual development environment on CarSky, or to record how a node deploys — read the platform doc and persist the platform/node reference under requirements/car-sky-guide/ for reuse.
---

# CarSky Platform & Node Reference (project-architecture)

Trigger: [[project-architecture]] is asked to investigate or set up the virtual development environment on CarSky, or to record what a node needs in order to deploy — distinct from [ecu-implementation-scoping](../ecu-implementation-scoping/SKILL.md) (which resolves *what* to design) and [high-level-design-procedure](../high-level-design-procedure/SKILL.md) (which designs it): this skill records how the platform itself works, not a design artifact.

**Scope boundary — reference, not procedure.** This skill produces the files that state what the platform and each node *are*: `node-*.md`, the blueprint and REST references. The document a human *follows* end to end — build → export → deploy → install → verify — is a `*-walkthrough.md`, owned by [[project-researcher]] via [walkthrough-authoring](../walkthrough-authoring/SKILL.md). When a request asks for that, hand it over rather than writing a second procedure here; the two would drift.

## Procedure

1. **Read** [Car-Sky-Platform.html](../../../requirements/development-platform-doc/Car-Sky-Platform.html) (and [BTC_phan_hoi_V2X_team.pdf](../../../requirements/development-platform-doc/BTC_phan_hoi_V2X_team.pdf) if the advisory is relevant) for the blueprint/node/pin model, node types, and deploy flow that answer the specific ask.
2. **If the ask is code deployment for a specific ECU/phase** (not a bare environment setup), first resolve the target ECU folder(s) via [ecu-implementation-scoping](../ecu-implementation-scoping/SKILL.md) — know what artifact/image is being deployed and its current state before writing steps for it.
3. **Record the platform facts the deploy depends on** — node type and its config block, pin kinds and wiring, image/artifact identifiers, the env set, and the platform limits that bite (what REST cannot create, what an import drops). State each fact once, in the file that owns it. Cite the acceptance criteria it serves (R5/R6, or the phase's criteria in [milestone1_high_level_plan.md](../../../documents/Plan-Proposal/milestone1_high_level_plan.md)).
4. **Persist** under `requirements/car-sky-guide/<topic-slug>.md` — one file per node or platform topic. On a later re-run of the same topic, update that file in place rather than creating a duplicate ([markdown-writing-style](../markdown-writing-style/SKILL.md) rule 4).
5. **Commit:** `[X] docs: <subject>` when tied to a specific requirement (typically R5/R6, or the ECU's own); plain `docs: <subject>` when it is generic environment setup.

## Output

- A platform/node reference at `requirements/car-sky-guide/<topic-slug>.md`, carrying the node's facts and the platform's limits.

## How to apply

Owned by [[project-architecture]]. These files are what a `*-walkthrough.md` links to for facts, and what [[project-planner]] reads when a phase actually has deployment work. Produces no task IDs, no procedures, and spawns no implementation.
