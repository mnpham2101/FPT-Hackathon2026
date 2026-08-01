---
name: carsky-room-diagnostics
description: Procedure the car-sky agent follows to inspect a deployed CarSky Room — deployment status, per-node phases, container logs — and to diagnose why nodes are not Running. Use whenever the task is to check logs on nodes, report deployment status, or troubleshoot a deploy that failed, hung, or misbehaves at runtime.
---

# CarSky Room Diagnostics (car-sky)

Trigger: [[car-sky]] is asked to check a deployment's status, read node logs, or troubleshoot a Room that will not come up. Read-only by default — diagnosis never mutates the platform; fixes are proposed, and applied only when the brief says so. Endpoint catalog and auth: [carsky-rest-api-blueprint.md](../../../requirements/car-sky-guide/carsky-rest-api-blueprint.md); this skill is the runtime-inspection procedure, not a second endpoint list.

## 0. Pin the Room

There is **no `GET /api/v1/deployments` list route** (404 `Route not found`, verified live 2026-07-31) — the `roomId` must come from one of:

- the `POST /api/v1/deployments` response when the deploy was made,
- the Nydus UI deployment entry,
- any node's sidecar log line `[sidecar] connecting to room <roomId>` — the reliable path when the user pastes a log,
- the user, who can read it from the Deployment Viewer.

Deploying also creates a **snapshot blueprint named `<blueprint>-deploy`**; it is what actually ran. Edits belong in the original blueprint followed by a redeploy — editing the snapshot fixes nothing.

## 1. Status → phases → logs, in that order

| Step | Call | Read |
|---|---|---|
| Room status | `GET /api/v1/deployments/{roomId}/status` | `{status, message, namespace}` — `DEPLOYING` vs ready; `message` carries deploy-time rejections |
| Per-node phases | `GET /api/v1/deployments/{roomId}/nodes` | array of `{displayName, name, nodeType, phase, message}` — `name` is the **nodeKey** every later call needs; `phase` is `Running` / `Provisioning` / … |
| Node log | `GET /api/v1/deployments/{roomId}/logs/{nodeKey}?container=user` | the team application's own output |

**`container` is mandatory on the logs route.** Every Room pod runs two containers — `user` (the team image) and `sidecar` (the Nydus networking shim). Omitting the parameter returns `500 UPSTREAM_ERROR` whose message names the choices: `a container name must be specified for pod …, choose one of: [user sidecar]`.

## 2. Read the error responses as evidence

The 500 wrapper is not a dead end — the upstream Kubernetes message inside it is often the whole diagnosis. Real example: requesting `?container=user` on a stuck node returned `container "user" … is waiting to start: trying and failing to pull image`, which identified an image-pull failure without any log line existing yet.

So: when a log call fails, **quote the upstream message** rather than reporting "logs unavailable".

## 3. Split the two containers

| Symptom | Reading |
|---|---|
| `sidecar` healthy (TAP created + configured, `connected to bridge`) but `user` has no logs | Application-level problem — image pull, crash-loop, or bad command. Network is fine; do not chase it. |
| `sidecar` `connect … Connection refused (os error 111) — retry 500ms` early in startup | **Normal.** The bridge may not accept yet; the sidecar retries and later logs `connected`. Only a *persistently* refusing connection is a fault. |
| `sidecar` never reaches `room state: fullyConnected` | Platform/network fault — escalate with the sidecar log. |
| `No device connected` | Emitted by the log/shell viewer, not the application — not evidence of a fault. |

## 4. Node stuck in `Provisioning`

Nearly always the image, in this order:

1. **Confirm what the node actually requests:** `GET /api/v1/blueprints/{id}` → each node's flat `config.image` (also `command`, `env`, `capabilities`).
2. **Confirm that image exists in the registry:** `GET /v2/_catalog` and `GET /v2/<repo>/tags/list` on the registry host with basic auth ([zot-registry-api-key.md](../../../requirements/car-sky-guide/zot-registry-api-key.md)). A repository absent from the catalog can never be pulled.
3. **Confirm the host:** references to a dead registry host fail identically to a missing image — the live host is recorded in [zot-registry-api-key.md § Registry host caveat](../../../requirements/car-sky-guide/zot-registry-api-key.md).
4. **Confirm the config was actually applied.** Node config is a **UI-only edit** (no live REST route mutates it), so a plan step that says "set the image" is easy to skip — a Room deployed from an unedited blueprint pulls whatever the baseline referenced, typically images that do not exist yet.

## 5. Report

State, in this order: Room status · per-node phase table · the failing node's decisive evidence (quoted upstream/log line) · root cause · the concrete fix and who performs it (UI-only steps belong to the user) · whether anything was mutated (normally nothing).

Never present a hypothesis as a finding — if the evidence is a pull failure, say the image failed to pull and show which reference was requested, rather than asserting why it is missing.

## How to apply

Owned by [[car-sky]] ([car-sky.md](../../agents/car-sky.md)). Credentials come from the run's brief per [carsky-credential-verify](../carsky-credential-verify/SKILL.md) — never echoed, stored, or committed. Findings that correct a committed guide (a dead route, a changed host) are reported so the orchestrating session updates `requirements/car-sky-guide/`.
