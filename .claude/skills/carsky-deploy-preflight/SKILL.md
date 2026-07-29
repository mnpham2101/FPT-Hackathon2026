---
name: carsky-deploy-preflight
description: Procedure the car-sky agent runs before any CarSky deployment — confirm the three required inputs (which blueprint, which ECU/node in it, which login credential) and the platform-access context, prompting the user for anything missing. Gate: never build, push, or deploy until all three are pinned.
---

# CarSky Deploy Preflight (car-sky agent)

Trigger: [[car-sky]] is handed a deployment task (deploy an artifact/image to CarSky) by [[project-architecture]] or [[project-planner]]. Run this **first**, before any build/push/deploy action — it resolves the inputs a deploy cannot proceed without. Distinct from [carsky-deployment-guide](../carsky-deployment-guide/SKILL.md) (which authors runbook docs); this skill gathers and confirms the live deploy inputs.

## The three required inputs

Never start a deploy until all three are pinned. If the spawning agent's brief already supplies one, confirm it back; if any is missing or ambiguous, **ask the user** (do not guess — a wrong blueprint or credential deploys to the wrong place or fails auth).

| # | Input | What to resolve | Where it comes from |
|---|---|---|---|
| 1 | **Which blueprint** | Exact blueprint name + id to deploy into (e.g. `trial1_minh`, id `a071ccc2-…`). New or existing? If new, confirm it should be created first. | Spawning brief, or `GET /api/v1/blueprints?name=<filter>`; prompt if unclear |
| 2 | **Which ECU / node** | Which node(s) in the blueprint this deploy targets — one ECU (`V2X ECU`, `ADA ECU`, `IVI ECU`) or the Bench, or the whole blueprint. Determines which image/artifact to build+push and which node config to update. | Spawning brief (the ECU folder being deployed); confirm against the blueprint's node list |
| 3 | **Which credential** | The API key (or login) to authenticate with, and thus **which account/environment** the deploy lands in. | **Prompt the user** — never assume; see § Platform access |

Restate the three back to the user as a one-line confirmation before proceeding (e.g. "Deploying **ADA ECU** into blueprint **trial1_minh** on **hackathon-2** using the key you provided — proceed?").

## Platform access (context the deploy needs to log in)

- **Environment (M1):** `https://hackathon-2.carsky.io` — the base URL. Environment-specific; confirm it hasn't changed (the organizers may rotate hosts between rounds).
- **Auth:** REST calls use header `Authorization: Bearer <API_KEY>`. The key is minted in the UI: **Settings (⚙) → Credentials → New credential** (shown once). It is a machine-to-machine (m2m) API key — **not** a Keycloak login credential, and not derivable from one.
- **Never** hardcode, echo, log, or commit the API key. Take it from the user at run time (or an env var they set, e.g. `CS_API_KEY`); keep it out of files and out of the transcript where avoidable.
- **Registry:** container images push to the CarSky Zot registry (`docker login registry.carsky.io -u <user>`, API key as password) before a Container node can reference them.
- **Full REST reference + verified request shapes:** [carsky-rest-api-blueprint.md](../../../requirements/car-sky-guide/carsky-rest-api-blueprint.md) — endpoints, the atomic `/batch` node-create payload, `validate`, deploy/verify calls, and the OpenAPI spec at `GET /api/v1/openapi.json`. [Car-Sky-Platform.html](../../../requirements/development-platform-doc/Car-Sky-Platform.html)'s "API & MCP Tools" module is the general platform reference behind it — consult it for anything the guide doesn't cover, but the guide's live-verified results win on conflict.

## Known platform limitation to flag up front

The REST/OpenAPI API **cannot create `ETHERNET` pins or wire the Ethernet Bridge** (`pinType` enum excludes `ETHERNET`; verified — returns 400). So any deploy whose blueprint still needs its R6 ethernet wiring has a **manual UI step** ([carsky-4-node-blueprint.md §4 step 5](../../../requirements/car-sky-guide/carsky-4-node-blueprint.md)). Surface this in the preflight confirmation whenever the target blueprint's nodes lack their ethernet pins/edges — don't let the deploy reach `validate` and fail with "node has no pins" unexpectedly.

## Output

- A confirmed triple (blueprint, ECU/node, credential) + verified base URL, restated to the user.
- A go/no-go: proceed to deploy only when all three are pinned and the user has confirmed; otherwise hold and ask.

## How to apply

Owned by [[car-sky]], run at the start of every deployment task. Produces no task IDs. On completion, hand control back to [[car-sky]]'s own deploy procedure (build → push → author/update blueprint → deploy → verify), using the confirmed inputs.
