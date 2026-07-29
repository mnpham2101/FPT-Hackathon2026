---
name: carsky-deployment-guide
description: Procedure project-architecture follows when asked to investigate/set up the virtual development environment on CarSky, or to produce steps to deploy code — read the platform doc, return a step-by-step guide, and persist it under requirements/car-sky-guide/ for reuse.
---

# CarSky Deployment Guide (project-architecture)

Trigger: [[project-architecture]] is asked to investigate or set up the virtual development environment on CarSky, or to produce steps to deploy code onto it — distinct from [ecu-implementation-scoping](../ecu-implementation-scoping/SKILL.md) (which resolves *what* to design) and [high-level-design-procedure](../high-level-design-procedure/SKILL.md) (which designs it): this skill is operational/runbook guidance for *using the platform itself*, not a design artifact.

## Procedure

1. **Read** [Car-Sky-Platform.html](../../../requirements/development-platform-doc/Car-Sky-Platform.html) (and [BTC_phan_hoi_V2X_team.pdf](../../../requirements/development-platform-doc/BTC_phan_hoi_V2X_team.pdf) if the advisory is relevant) for the blueprint/node/pin model, node types, and deploy flow that answer the specific ask.
2. **If the ask is code deployment for a specific ECU/phase** (not a bare environment setup), first resolve the target ECU folder(s) via [ecu-implementation-scoping](../ecu-implementation-scoping/SKILL.md) — know what artifact/image is being deployed and its current state before writing steps for it.
3. **Translate platform mechanics into a numbered, step-by-step guide** the user can execute manually — one concrete action per step (e.g. build the OCI image, push to the Zot registry, author the blueprint in Nydus with nodes/pins/edges, trigger New Deployment, verify every node Running in the Deployment Viewer, ADB-install the APK on the AAOS node). Cite the acceptance criteria each step serves (R5/R6, or the target phase's acceptance criteria in [milestone1.md](../../../plans/milestone1.md)) so the user can verify success at each step.
4. **Return the guide to the user**, then **persist it** under `requirements/car-sky-guide/<topic-slug>.md` — one file per environment-setup or deployment topic (e.g. `requirements/car-sky-guide/carsky-env-setup.md`, `requirements/car-sky-guide/carsky-deploy-v2x-ecu.md`). On a later re-run of the same topic, update the existing file in place rather than creating a duplicate (per [markdown-writing-style](../markdown-writing-style/SKILL.md) rule 4).
5. **Commit** the guide (when asked): `[X] docs: <subject>` when it's tied to a specific requirement (typically R5/R6, or the ECU's own requirement); plain `docs: <subject>` (no taskID) when it's a generic environment-setup guide not tied to one requirement.

## Output

- A step-by-step CarSky guide returned in-conversation.
- The same guide persisted at `requirements/car-sky-guide/<topic-slug>.md`, discoverable by [[project-planner]] when it plans deployment tasks for a phase.

## How to apply

Owned by [[project-architecture]]. [[project-planner]] reads `requirements/car-sky-guide/` when building a phase's implementation plan (see [[project-planner]]'s spec) — keep guides accurate and up to date, since stale steps would misdirect that planning. Produces no task IDs and spawns no implementation.
