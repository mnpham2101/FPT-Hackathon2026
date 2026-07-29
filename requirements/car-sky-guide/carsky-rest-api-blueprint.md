# CarSky Blueprint via REST API (scripted alternative to the UI)

Companion to [carsky-4-node-blueprint.md](carsky-4-node-blueprint.md). That guide builds the blueprint by hand on the Nydus canvas; this one drives the REST API for the parts that can be scripted. **Verified live on 2026-07-29** against `https://hackathon-2.carsky.io` by creating the `trial1_minh` blueprint (id `a071ccc2-c5fe-4d81-ac0e-16bef8d22173`).

## Key finding: what REST can and cannot do

| Task | REST API | Notes |
|---|---|---|
| Create blueprint | ✅ `POST /api/v1/blueprints` | `{name, description}` |
| Add nodes (any type + config) | ✅ `POST /api/v1/blueprints/{id}/batch` `addNode` | `container`, `skycraft`, `eth-bridge`, etc. |
| Add CAN/LIN/KUKSA/GPIO/VHAL/VIDEO/GENERIC pins | ✅ `addPin` | enum-restricted (below) |
| **Add `ETHERNET` pins** | ❌ **rejected** | `pinType` enum is `VHAL, KUKSA, CAN, LIN, VIDEO, GPIO, GENERIC` — **no `ETHERNET`**. Confirmed live: `addPin` with `pinType: "ETHERNET"`, `nodeId` targeting a node from an earlier (already-existing) blueprint, returns 400 `"Invalid option: expected one of \"VHAL\"\|\"KUKSA\"\|\"CAN\"\|\"LIN\"\|\"VIDEO\"\|\"GPIO\"\|\"GENERIC\""`. |
| **Wire the Ethernet Bridge** | ❌ blocked | edges need the ethernet pins that REST can't create |
| Validate | ✅ `POST /api/v1/blueprints/{id}/validate` | fails until every node has ≥1 pin |
| **Import blueprint JSON** (`POST /api/v1/blueprints/import` and Nydus UI **Import from File**) | ⚠️ nodes only | Same `ETHERNET` limitation as `addPin` — `ethernet` pins in the imported file are silently dropped during node creation. **Confirmed live** 2026-07-29 importing [blueprint-m1-cooperative-awareness.json](blueprint-m1-cooperative-awareness.json): a first version with `ethernet` pins + edges failed with `addEdge: source or target pin not found`, because by the time the edges are applied, the referenced ethernet pin IDs were never created. Fix: the importable JSON carries the 5 nodes with full `config` and empty `pins`/`edges`; add + wire ethernet pins manually in the UI after import. |

**Consequence:** the R6 Ethernet Bridge network (every node's `ethernet` pin wired to the bridge) **cannot be built over REST, nor via JSON import** — those pins and edges must be added in the Nydus UI canvas regardless of how the nodes themselves were created. The UI creates ETHERNET pins through its Zero-sync path, which the public REST/OpenAPI schema (and the import path built on top of it) does not expose. So the practical split is:

- **Scriptable (REST):** create the blueprint + all nodes with their full config (image, env, command). Done once, reproducibly, no clicking.
- **Manual (UI):** add one `ethernet` pin per node, wire each to the Ethernet Bridge (guide §4 step 5), then deploy.

## Authentication

REST calls use `Authorization: Bearer <API_KEY>` (`X-API-Key: <API_KEY>` also accepted — both declared in `GET /api/v1/openapi.json`'s `components.securitySchemes.ApiKeyAuth`). Mint the key in the UI: **Settings (⚙) → Credentials → New credential** — shown once, store it securely. The real key always starts `a8k_` (`a8k_<prefix>_<secret>`); the Credentials **list view**'s display ID (`m2m-<uuid>-<credential-name>`) looks similar but is **not** the secret and is rejected (`{"error":"UNAUTHORIZED","message":"Unrecognized credential format"}`) — full gotcha writeup in [carsky-credential-verify](../../.claude/skills/carsky-credential-verify/SKILL.md).

It is an OIDC m2m credential, not a Keycloak user password — but a Keycloak login session **can bootstrap one headlessly** when no key exists yet. Confirmed live 2026-07-29: direct password-grant against client `rework` is rejected (`unauthorized_client`, Direct Access Grants disabled) and the `admin-cli` client's direct-grant JWT is rejected by this API as `Invalid JWT`, but a scripted browser-style login (Keycloak form POST → Envoy OAuth2-proxy callback) succeeds and sets session cookies that authorize `/internal/*` and `/api/*` (no `v1`) — notably `POST /internal/credentials`, which mints a fresh `a8k_...` key. Full step-by-step: [carsky-login](../../.claude/skills/carsky-login/SKILL.md). Note `/api/v1/*` (everything in this doc) stays behind the `a8k_...` key only — the login-session cookie never authorizes it directly.

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
| POST | `/api/v1/nodes/{nodeId}/pins` | **declared in the spec but not live** — confirmed live 2026-07-29: 404 `Route not found`, even with a valid `pinType` (`GPIO`). Not nested under `/blueprints/{id}` despite what an earlier version of this doc assumed. Use the `/batch` endpoint's `addPin` op (with `nodeId` set to target an existing node) instead — it's the only pin-creation path that actually works. |
| PATCH/DELETE | `/api/v1/nodes/{nodeId}` · `/api/v1/pins/{pinId}` · `/api/v1/edges/{edgeId}` | **also declared but not live** (same 404 `Route not found`) — confirmed live 2026-07-29 trying to `DELETE` a test pin. **There is currently no working delete/remove op anywhere in this API** — `/batch`'s `operations` enum is only `addNode` \| `addPin` \| `addEdge`. Once a pin or node is added via `/batch`, it can only be removed by hand in the Nydus UI canvas. |

Full spec: `GET /api/v1/openapi.json` (OpenAPI 3.1) or Swagger UI at `/api/v1/docs` — but verify live before relying on any single-item route it lists; the granular node/pin/edge family above is present in the spec yet unreachable on `hackathon-2.carsky.io` as of 2026-07-29.
