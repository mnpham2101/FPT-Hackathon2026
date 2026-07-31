# CarSky Blueprint via REST API (scripted alternative to the UI)

Companion to [carsky-4-node-blueprint.md](carsky-4-node-blueprint.md). That guide builds the blueprint by hand on the Nydus canvas; this one drives the REST API for the parts that can be scripted.

The platform's own general reference is [Car-Sky-Platform.html](../development-platform-doc/Car-Sky-Platform.html)'s "API & MCP Tools" module (§4–§10) and "Get CarSky API Key" module — read those for the full endpoint catalog, the MCP tool set, and the officially documented auth flow. **This doc is the narrower, live-verified ground truth for this specific hackathon-2 deployment** — where the two disagree, trust this doc's live results, and treat the platform doc as the idealized/general design intent. A concrete example: the platform doc's endpoint reference states the granular pin routes as `/api/v1/blueprints/nodes/:nodeId/pins` etc.; the live `GET /api/v1/openapi.json` on this deployment instead declares `/api/v1/nodes/{nodeId}/pins` (no `/blueprints/` prefix); and empirically **neither shape works** — see the API reference table below. Always verify a single-item route live before relying on it, regardless of which doc it came from.

## Key finding: what REST can and cannot do

| Task | REST API | Notes |
|---|---|---|
| Create blueprint | ✅ `POST /api/v1/blueprints` | `{name, description}` |
| Add nodes (any type + config) | ✅ `POST /api/v1/blueprints/{id}/batch` `addNode` | `container`, `skycraft`, `eth-bridge`, etc. |
| Add CAN/LIN/KUKSA/GPIO/VHAL/VIDEO/GENERIC pins | ✅ `addPin` | enum-restricted (below) |
| **Add `ETHERNET` pins** | ❌ **rejected** | `pinType` enum is `VHAL, KUKSA, CAN, LIN, VIDEO, GPIO, GENERIC` — **no `ETHERNET`**. Confirmed live: `addPin` with `pinType: "ETHERNET"`, `nodeId` targeting a node from an earlier (already-existing) blueprint, returns 400 `"Invalid option: expected one of \"VHAL\"\|\"KUKSA\"\|\"CAN\"\|\"LIN\"\|\"VIDEO\"\|\"GPIO\"\|\"GENERIC\""`. |
| **Wire the Ethernet Bridge** | ❌ blocked | edges need the ethernet pins that REST can't create |
| Validate | ✅ `POST /api/v1/blueprints/{id}/validate` | fails until every node has ≥1 pin |
| **Import blueprint JSON** (`POST /api/v1/blueprints/import` and Nydus UI **Import from File**) | ⚠️ nodes only | Same `ETHERNET` limitation as `addPin` — `ethernet` pins in the imported file are silently dropped during node creation. An imported JSON must carry its nodes with full `config` and empty `pins`/`edges`; add and wire `ethernet` pins manually in the UI after import. |

**Consequence:** the R6 Ethernet Bridge network (every node's `ethernet` pin wired to the bridge) **cannot be built over REST, nor via JSON import** — those pins and edges must be added in the Nydus UI canvas regardless of how the nodes themselves were created. The UI creates ETHERNET pins through its Zero-sync path, which the public REST/OpenAPI schema (and the import path built on top of it) does not expose. So the practical split is:

- **Scriptable (REST):** create the blueprint + all nodes with their full config (image, env, command). Done once, reproducibly, no clicking.
- **Manual (UI):** add one `ethernet` pin per node, wire each to the Ethernet Bridge (guide §4 step 5), then deploy.

## Authentication

- The REST API requires an API key credential on every request.
- Send it as `Authorization: Bearer <API_KEY>` or `X-API-Key: <API_KEY>` — both are accepted.
- The key format is `a8k_<prefix>_<secret>`. It is a machine-to-machine (m2m) API key, minted in the UI (**Settings → Credentials → New credential**) and displayed only at the moment of creation.
- The Credentials **list view** shows a display ID styled `m2m-<uuid>-<credential-name>`. This ID is not the secret — using it as a credential is rejected with `{"error":"UNAUTHORIZED","message":"Unrecognized credential format"}`. Full explanation: [carsky-credential-verify](../../.claude/skills/carsky-credential-verify/SKILL.md).
- A CarSky account's email and password is a separate credential (Keycloak login), not the API key.
- When no API key exists yet, a Keycloak login session can bootstrap one without using the UI. Step-by-step procedure: [carsky-login](../../.claude/skills/carsky-login/SKILL.md).

```
export CS=https://hackathon-2.carsky.io
export KEY=<your_api_key>
curl -H "Authorization: Bearer $KEY" $CS/api/v1/blueprints         # list
```

## Node config is flat, not wrapped

The platform doc's Container Node example shows `{"container": {"image": …}}`, but the **stored** node config is flat — `image`, `command`, `env` sit directly in `config`:

```json
{"op":"addNode","data":{
  "label":"V2X ECU","nodeType":"container",
  "config":{"image":"registry.carsky.io/m1-v2x-ecu:latest","command":["./v2x_ecu"],
            "env":{"LISTEN_PORT":"47100","ADA_ECU_HOST":"10.99.0.12","ADA_ECU_PORT":"47200"}},
  "positionX":-450,"positionY":-40}}
```

Node types seen in real blueprints: `container`, `skycraft`, `eth-bridge`, `script-node`, `can-bus`, `lin-bus`, `kuksa-databroker`, `gpio-panel`.

## Finish in the UI

After creating the nodes over REST, open the blueprint in Nydus and complete [carsky-4-node-blueprint.md §4](carsky-4-node-blueprint.md) from step 5 (wire the pins): add each node's `ethernet` pin, wire it to the Ethernet Bridge, then `validate` passes and the blueprint can deploy. Until the ethernet pins exist, `validate` returns 422 `Node "…" has no pins`.

## Deploy & test a Room

Once the blueprint's nodes exist (REST) and its `ethernet` pins are wired (manual UI step above), the rest of the blueprint-to-testing workflow is REST-reachable and documented in [Car-Sky-Platform.html](../development-platform-doc/Car-Sky-Platform.html)'s "API & MCP Tools" module §5.5–§5.7 (not independently re-verified live here — cross-check before depending on exact shapes):

| Step | Endpoint | Notes |
|---|---|---|
| Deploy | `POST /api/v1/deployments` `{blueprintId, roomId, name}` | `roomId` is the target device's id (§2 Device vs Node in [carsky-4-node-blueprint.md](carsky-4-node-blueprint.md) — the K8s resource pool, not an ECU) |
| Poll status | `GET /api/v1/deployments/{roomId}/status` (or `.../nodes/watch` for SSE) | Poll until every node reports `Running` — this is R5's acceptance check |
| List nodes | `GET /api/v1/deployments/{roomId}/nodes` | Resolves each node's `nodeKey` for the calls below |
| Screenshot (IVI) | `GET /api/v1/vms/{roomId}/{nodeKey}/screenshot` | PNG; useful to verify the R16/R17 warning view renders, without a manual ADB session |
| UI tree / find text | `GET /api/v1/vms/{roomId}/{nodeKey}/accessibility` | Structured check for expected UI elements (e.g. ghost-C warning visible) |
| ADB shell | `POST /api/v1/vms/{roomId}/{nodeKey}/shell` | One-shot `adb shell` command, no tunnel needed for simple checks |
| Read/write signals | `POST /api/v1/signals/{roomId}/{nodeKey}/values` / `.../actuate` | For nodes with CAN/LIN/GPIO/KUKSA pins — not used by the four M1 ECU/bench nodes (they only carry `ethernet`) |
| Logs | `GET /api/v1/deployments/{roomId}/logs/{nodeKey}?container=user\|sidecar` (`.../search` for historical) | **`container` is mandatory** (verified live 2026-07-31) — every Room pod runs `user` (team image) + `sidecar` (Nydus shim); omitting it returns 500 whose message lists the two. Failure text is itself diagnostic (e.g. `waiting to start: trying and failing to pull image`). Procedure: [carsky-room-diagnostics](../../.claude/skills/carsky-room-diagnostics/SKILL.md) |
| Node phases | `GET /api/v1/deployments/{roomId}/nodes` | `{displayName, name, nodeType, phase, message}` per node — `name` is the `nodeKey` the logs/screenshot/shell routes need |
| ~~List deployments~~ | ~~`GET /api/v1/deployments`~~ | **not live** — 404 `Route not found` (2026-07-31). Deploying also creates a snapshot blueprint `<name>-deploy`; edit the original and redeploy |
| Find deployment | `GET /api/v1/deployments/find?device=<name\|id>` or `?blueprint=<name>` | **live** (verified 2026-07-31) — resolves the active deployment without the dead list route. `blueprint` matches by **name, not id**, and appears to match only the exact deployed name; `[]` on a name is not proof nothing is deployed — cross-check via the device list below |
| List devices (Rooms) | `GET /api/v1/devices?limit=…` | **live** (verified 2026-07-31) — `{id, name, ownerId, operational}` per device; `id` is the `roomId`, `operational: BUSY` marks a device with an active deployment. Filter by your `ownerId` (embedded in the m2m key's display ID) to find your Room |
| Teardown | `DELETE /api/v1/deployments/{roomId}` | Stops the Room; the blueprint itself is untouched and redeployable |

An MCP server also wraps this same REST API into named tools (`deploy`, `screenshot`, `tap`, `get_signal_values`, `pod_logs`, etc. — see the platform doc's §6–§7 for the full 42-tool catalog). It requires running CarSky's own `mcp/` server package (`node mcp/dist/index.js`) against this API, which is not part of this repo — not used for M1; stick to the REST calls above unless that package becomes available.

## API reference (blueprint group)

| Method | Path | Purpose |
|---|---|---|
| GET | `/api/v1/blueprints` | list (paginate, `?name=` filter) |
| POST | `/api/v1/blueprints` | create empty |
| GET | `/api/v1/blueprints/{id}` | full topology (nodes, pins, edges) |
| POST | `/api/v1/blueprints/{id}/batch` | add nodes/pins/edges (atomic) |
| POST | `/api/v1/blueprints/{id}/validate` | check topology |
| POST | `/api/v1/blueprints/{id}/clone` | duplicate (used for trial variants — [§7](carsky-4-node-blueprint.md#7-multiple-blueprints-trial-variants)) |
| GET | `/api/v1/blueprints/{id}/export` | export JSON |
| POST | `/api/v1/blueprints/import` | import exported JSON (same `ETHERNET` limitation) |
| POST | `/api/v1/nodes/{nodeId}/pins` | **declared in the spec but not live** — confirmed live 2026-07-29: 404 `Route not found`, even with a valid `pinType` (`GPIO`). Not nested under `/blueprints/{id}` despite what an earlier version of this doc assumed. Use the `/batch` endpoint's `addPin` op (with `nodeId` set to target an existing node) instead — it's the only pin-creation path that actually works. |
| PATCH/DELETE | `/api/v1/nodes/{nodeId}` · `/api/v1/pins/{pinId}` · `/api/v1/edges/{edgeId}` | **also declared but not live** (same 404 `Route not found`) — confirmed live 2026-07-29 trying to `DELETE` a test pin. **There is currently no working delete/remove op anywhere in this API** — `/batch`'s `operations` enum is only `addNode` \| `addPin` \| `addEdge`. Once a pin or node is added via `/batch`, it can only be removed by hand in the Nydus UI canvas. |

Full spec: `GET /api/v1/openapi.json` (OpenAPI 3.1) or Swagger UI at `/api/v1/docs` — but verify live before relying on any single-item route it lists; the granular node/pin/edge family above is present in the spec yet unreachable on `hackathon-2.carsky.io` as of 2026-07-29.
