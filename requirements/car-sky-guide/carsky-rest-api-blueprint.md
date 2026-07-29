# CarSky Blueprint via REST API (scripted alternative to the UI)

Companion to [carsky-4-node-blueprint.md](carsky-4-node-blueprint.md). That guide builds the blueprint by hand on the Nydus canvas; this one drives the REST API for the parts that can be scripted. **Verified live on 2026-07-29** against `https://hackathon-2.carsky.io` by creating the `trial1_minh` blueprint (id `a071ccc2-c5fe-4d81-ac0e-16bef8d22173`).

## Key finding: what REST can and cannot do

| Task | REST API | Notes |
|---|---|---|
| Create blueprint | ✅ `POST /api/v1/blueprints` | `{name, description}` |
| Add nodes (any type + config) | ✅ `POST /api/v1/blueprints/{id}/batch` `addNode` | `container`, `skycraft`, `eth-bridge`, etc. |
| Add CAN/LIN/KUKSA/GPIO/VHAL/VIDEO/GENERIC pins | ✅ `addPin` | enum-restricted (below) |
| **Add `ETHERNET` pins** | ❌ **rejected** | `pinType` enum is `VHAL, KUKSA, CAN, LIN, VIDEO, GPIO, GENERIC` — **no `ETHERNET`**. 400 `VALIDATION_ERROR`. |
| **Wire the Ethernet Bridge** | ❌ blocked | edges need the ethernet pins that REST can't create |
| Validate | ✅ `POST /api/v1/blueprints/{id}/validate` | fails until every node has ≥1 pin |

**Consequence:** the R6 Ethernet Bridge network (every node's `ethernet` pin wired to the bridge) **cannot be built over REST** — those pins and edges must be added in the Nydus UI canvas. The UI creates ETHERNET pins through its Zero-sync path, which the public REST/OpenAPI schema does not expose. So the practical split is:

- **Scriptable (REST):** create the blueprint + all nodes with their full config (image, env, command). Done once, reproducibly, no clicking.
- **Manual (UI):** add one `ethernet` pin per node, wire each to the Ethernet Bridge (guide §4 step 5), then deploy.

## Authentication

REST calls use `Authorization: Bearer <API_KEY>`. Mint the key in the UI: **Settings (⚙) → Credentials → New credential** — shown once, store it securely. (The key is an OIDC m2m credential; it is **not** a Keycloak user password and cannot be derived from one.)

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

## Reproduce the trial1_minh node set

One atomic batch creates all 5 nodes (bridge + 3 containers + skycraft). `POST $CS/api/v1/blueprints/{id}/batch` with `operations`:

```json
{"operations":[
 {"op":"addNode","data":{"label":"Ethernet Bridge","nodeType":"eth-bridge","config":{"bridgeMode":"linux","subnet":"10.99.0.0/24"},"positionX":0,"positionY":0}},
 {"op":"addNode","data":{"label":"Bench - Scenario Player","nodeType":"container","config":{"image":"registry.carsky.io/m1-scenario-player:latest","command":["python","main.py"],"env":{"SCENARIO_CONFIG":"/app/scenarios/default.yaml","V2X_ECU_HOST":"10.99.0.11","V2X_ECU_PORT":"47100"}},"positionX":-450,"positionY":-260}},
 {"op":"addNode","data":{"label":"V2X ECU","nodeType":"container","config":{"image":"registry.carsky.io/m1-v2x-ecu:latest","command":["./v2x_ecu"],"env":{"LISTEN_PORT":"47100","ADA_ECU_HOST":"10.99.0.12","ADA_ECU_PORT":"47200"}},"positionX":-450,"positionY":-40}},
 {"op":"addNode","data":{"label":"ADA ECU","nodeType":"container","config":{"image":"registry.carsky.io/m1-ada-ecu:latest","command":["./ada_ecu"],"env":{"V2X_LISTEN_PORT":"47200","IVI_ECU_HOST":"10.99.0.13","IVI_ECU_PORT":"47300","GATE_ENTER_M":"30","GATE_EXIT_M":"35"}},"positionX":-450,"positionY":180}},
 {"op":"addNode","data":{"label":"IVI ECU","nodeType":"skycraft","config":{"prefix":"ivi","gpuBackend":"virglrenderer","displayWidth":1920,"displayHeight":1080},"positionX":450,"positionY":-40}}
]}
```

Batch is atomic — if any op fails (e.g. an `ETHERNET` `addPin`), the whole batch rolls back and no partial nodes remain. `addPin` ops reference a node by `nodeRef` (its index among the batch's `addNode` ops); `addEdge` ops reference pins by `sourcePinRef`/`targetPinRef`.

## Finish in the UI

After the batch, open `trial1_minh` in Nydus and complete [carsky-4-node-blueprint.md §4](carsky-4-node-blueprint.md) from step 5 (wire the pins): add each node's `ethernet` pin, wire to the Ethernet Bridge, then `validate` passes and the blueprint can deploy. Until the ethernet pins exist, `validate` returns 422 `Node "…" has no pins`.

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
| POST | `/api/v1/blueprints/{id}/nodes` · `/nodes/{nodeId}/pins` · `/{id}/edges` | granular single-item variants |

Full spec: `GET /api/v1/openapi.json` (OpenAPI 3.1) or Swagger UI at `/api/v1/docs`.
