# Prompt — CarSky deployment-guide skills, planner deployment tasks, always-detailed thinking

**Date:** 2026-07-29 **Requested by:** mnpham1986@gmail.com

## Prompt text

> add role or skill for project-architecture:
>
> when asked to investigate set up virtual development environment on Car-Sky, or deploy code:
> * read Car-Sky-Platform, and return guides step by step for me to do so.
> * document the guide for future use.
>
> add role for project-planner:
> When ask to make implementation plan for a phase,
> * always read the guide produced by project-architecture and makes tasks or subtasks for deployment on Car Sky
>
> Change claude setting such that all agent produce detail thinking when being prompt to do anything.
>
> Save this prompts

## Outcome

- New skill `.claude/skills/carsky-deployment-guide/SKILL.md`, owned by [[project-architecture]]: triggered when asked to investigate/set up the CarSky virtual dev environment or produce deployment steps. Reads [Car-Sky-Platform.html](../../requirements/development-platform-doc/Car-Sky-Platform.html) (+ the BTC PDF when relevant), resolves the target ECU folder via `ecu-implementation-scoping` when the ask is code-specific, returns a numbered step-by-step guide, and persists it at `requirements/car-sky-guide/<topic-slug>.md` (one file per environment/deployment topic, updated in place on re-runs rather than duplicated — originally drafted as a new top-level `guides/` folder, then relocated under `requirements/` per follow-up correction).
- `project-architecture.md` updated: new skill added to Scope of work, Inputs, and Outputs.
- `project-planner.md` updated: new Scope-of-work bullet — when asked to plan a phase, always read the `requirements/car-sky-guide/` deployment guide(s) for that phase's ECU(s) and include CarSky deployment tasks/subtasks alongside feature tasks; flag back if no guide exists yet rather than guessing steps. Added `requirements/car-sky-guide/` to Inputs.
- `.claude/settings.json`: added `"effortLevel": "high"`. `alwaysThinkingEnabled`/`showThinkingSummaries` were already `true` at the project-settings level, which is process-wide and already covers subagents spawned via the Agent/Task tool (Claude Code has no separate per-agent thinking toggle in its settings schema or in this repo's agent frontmatter) — the missing lever for "detailed thinking" was reasoning effort, not the thinking toggle itself.
