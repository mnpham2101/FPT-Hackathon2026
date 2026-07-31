# CarSky Blueprint via REST API (scripted alternative to the UI)

Companion to [carsky-4-node-blueprint.md](carsky-4-node-blueprint.md). That guide builds the blueprint by hand on the Nydus canvas; this one drives the REST API for the parts that can be scripted.

The platform's own reference is [Car-Sky-Platform.html](../development-platform-doc/Car-Sky-Platform.html)'s "API & MCP Tools" module (§4–§10) — the full endpoint catalog, MCP tool set, and credential flow. **This doc takes precedence for `hackathon-2.carsky.io`**: some routes it or the OpenAPI spec declare don't work here — e.g. the granular pin routes `/api/v1/blueprints/nodes/:nodeId/pins` and `/api/v1/nodes/{nodeId}/pins` both 404; use `/batch` instead (below).

## Key finding: what REST can and cannot do

| Task | REST API | Notes |
|---|---|---|
| Create blueprint | ✅ `POST /api/v1/blueprints` | `{name, description}` |
| Add nodes (any type + config) | ✅ `POST /api/v1/blueprints/{id}/batch` `addNode` | `container`, `skycraft`, `eth-bridge`, etc. |
| Add CAN/LIN/KUKSA/GPIO/VHAL/VIDEO/GENERIC pins | ✅ `addPin` | enum-restricted (below) |
| **Add `ETHERNET` pins** | ❌ rejected | `pinType` enum is `VHAL, KUKSA, CAN, LIN, VIDEO, GPIO, GENERIC` — no `ETHERNET`. Returns 400 `"Invalid option: expected one of \"VHAL\"\|\"KUKSA\"\|\"CAN\"\|\"LIN\"\|\"VIDEO\"\|\"GPIO\"\|\"GENERIC\""`. |
| **Wire the Ethernet Bridge** | ❌ blocked | edges need the ethernet pins that REST can't create |
| Validate | ✅ `POST /api/v1/blueprints/{id}/validate` | fails until every node has ≥1 pin |
| **Import blueprint JSON** (`POST /api/v1/blueprints/import` and Nydus UI **Import from File**) | ⚠️ nodes only | Same `ETHERNET` limitation as `addPin` — `ethernet` pins in the imported file are silently dropped during node creation. An imported JSON must carry its nodes with full `config` and empty `pins`/`edges`; add and wire `ethernet` pins manually in the UI after import. |

**Consequence:** the R6 Ethernet Bridge network cannot be built over REST or via JSON import — the UI creates `ETHERNET` pins through its Zero-sync path, which the public REST/OpenAPI schema doesn't expose. Add and wire them manually in Nydus ([carsky-4-node-blueprint.md §4 step 5](carsky-4-node-blueprint.md)); `validate` returns 422 `Node "…" has no pins` until they exist.

- **Scriptable (REST):** blueprint + all nodes with full config (image, env, command) — done once, reproducibly.
- **Manual (UI):** the `ethernet` pins and edges only.

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
  "config":{"image":"registry.hackathon-2.carsky.io/m1-v2x-ecu:latest","command":["./v2x_ecu"],
            "env":{"LISTEN_PORT":"47100","ADA_ECU_HOST":"10.99.0.12","ADA_ECU_PORT":"47200"}},
  "positionX":-450,"positionY":-40}}
```

Node types seen in real blueprints: `container`, `skycraft`, `eth-bridge`, `script-node`, `can-bus`, `lin-bus`, `kuksa-databroker`, `gpio-panel`.

## Deploy & test a Room

Once the blueprint's nodes exist (REST) and its `ethernet` pins are wired (manual UI step above), deploy and manage the Room with the calls below:

| Step | Endpoint | Notes |
|---|---|---|
| Deploy | `POST /api/v1/deployments` `{blueprintId, roomId, name}` | `roomId` is the target device's id (§2 Device vs Node in [carsky-4-node-blueprint.md](carsky-4-node-blueprint.md) — the K8s resource pool, not an ECU) |
| Poll status | `GET /api/v1/deployments/{roomId}/status` (or `.../nodes/watch` for SSE) | Poll until every node reports `Running` — this is R5's acceptance check |
| List nodes | `GET /api/v1/deployments/{roomId}/nodes` | Resolves each node's `nodeKey` for the calls below |
| Screenshot (IVI) | `GET /api/v1/vms/{roomId}/{nodeKey}/screenshot` | ❌ 502, unavailable on this deployment |
| UI tree / find text | `GET /api/v1/vms/{roomId}/{nodeKey}/accessibility` | ❌ 502, unavailable on this deployment |
| ADB shell | `POST /api/v1/vms/{roomId}/{nodeKey}/shell` `{command}` | ❌ 502, unavailable on this deployment. Request/response shape per the OpenAPI spec: `{command}` in, `{ok, exitCode, stdout}` out |
| Read/write signals | `POST /api/v1/signals/{roomId}/{nodeKey}/values` / `.../actuate` | For nodes with CAN/LIN/GPIO/KUKSA pins — not used by the four M1 ECU/bench nodes (they only carry `ethernet`) |
| Logs | `GET /api/v1/deployments/{roomId}/logs/{nodeKey}?container=user\|sidecar` (`.../search` for historical) | **`container` is mandatory** — every Room pod runs `user` (team image) + `sidecar` (Nydus shim); omitting it returns 500 whose message lists the two. Failure text is itself diagnostic (e.g. `waiting to start: trying and failing to pull image`). Procedure: [carsky-room-diagnostics](../../.claude/skills/carsky-room-diagnostics/SKILL.md) |
| Node phases | `GET /api/v1/deployments/{roomId}/nodes` | `{displayName, name, nodeType, phase, message}` per node — `name` is the `nodeKey` the logs/screenshot/shell routes need |
| ~~List deployments~~ | ~~`GET /api/v1/deployments`~~ | ❌ 404 `Route not found`. Deploying also creates a snapshot blueprint `<name>-deploy`; edit the original and redeploy |
| Find deployment | `GET /api/v1/deployments/find?device=<name\|id>` or `?blueprint=<name>` | ✅ resolves the active deployment without the dead list route. `blueprint` matches by **name, not id**, and only the exact deployed name; `[]` is not proof nothing is deployed — cross-check via the device list below |
| List devices (Rooms) | `GET /api/v1/devices?limit=…` | ✅ `{id, name, ownerId, operational}` per device; `id` is the `roomId`, `operational: BUSY` marks an active deployment. Filter by your `ownerId` (embedded in the m2m key's display ID) to find your Room |
| Pod containers | `GET /api/v1/deployments/{roomId}/pods` | ✅ per-pod `phase` and per-container `{name, state, ready}`; shows at a glance whether `sidecar` runs while `user` waits (the image-pull-failure signature) |
| Restart | `POST /api/v1/deployments/{roomId}/restart` and `.../restart/{node}` | ⚠️ returns 500 `INTERNAL_ERROR` even though pods do get recreated — treat as unreliable, prefer teardown + redeploy |
| Container exec | `POST /api/v1/deployments/{roomId}/container-exec/{nodeKey}` `{command}` | ❌ 502, unavailable on this deployment |
| Teardown | `DELETE /api/v1/deployments/{roomId}` | Stops the Room; the blueprint itself is untouched and redeployable |

**None of the ❌ routes above are needed for M1** — deploy status, node phases, and container logs cover every M1 verification, including the netcheck hop-3 check ([deploy-walkthrough-netcheck.md](deploy-walkthrough-netcheck.md)).

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
| POST | `/api/v1/nodes/{nodeId}/pins` | ❌ declared in the spec but not live — 404 `Route not found` even with a valid `pinType`. Use `/batch`'s `addPin` op (with `nodeId` targeting an existing node) instead — the only working pin-creation path. |
| PATCH/DELETE | `/api/v1/nodes/{nodeId}` · `/api/v1/pins/{pinId}` · `/api/v1/edges/{edgeId}` | ❌ declared but not live — same 404. **No working delete/remove op exists in this API** — `/batch`'s `operations` enum is only `addNode`\|`addPin`\|`addEdge`. Remove a pin/node by hand in the Nydus UI. |

Full spec: `GET /api/v1/openapi.json` (OpenAPI 3.1) or Swagger UI at `/api/v1/docs` — the granular node/pin/edge routes it lists are present but unreachable; use `/batch` instead.
