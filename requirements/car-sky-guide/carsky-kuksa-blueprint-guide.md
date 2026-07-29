# CarSky Blueprint — KUKSA-Pin Variant (Ethernet-Free)

**Status: studied alternative, not adopted.** This is a trial variant ([carsky-4-node-blueprint.md §7](carsky-4-node-blueprint.md#7-multiple-blueprints-trial-variants)) of the baseline [blueprint-m1-cooperative-awareness.json](blueprint-m1-cooperative-awareness.json). It does not replace the baseline and does not change [m1-cooperative-awareness.md](../m1-cooperative-awareness.md)'s frozen R6. Adopting it for real requires an explicit decision — see § Feasibility verdict.

## 1. Why this exists

[carsky-rest-api-blueprint.md](carsky-rest-api-blueprint.md) confirms live: the `addPin` REST enum is `VHAL, KUKSA, CAN, LIN, VIDEO, GPIO, GENERIC` — no `ETHERNET`. Neither REST `addPin` nor JSON import can create `ethernet` pins or wire the Ethernet Bridge; that step is UI-manual regardless of how the nodes were created ([carsky-4-node-blueprint.md §4](carsky-4-node-blueprint.md#4-steps) step 5).

This variant asks: is there a pin type in that accepted enum that reaches all four M1 node roles, so the *entire* blueprint — nodes, pins, and edges — becomes scriptable, with zero manual UI steps?

## 2. Pin-type comparison

| REST-creatable pin | Container Node | Skycraft Node | Bridge/broker node exists | Fit for arbitrary byte/JSON payloads |
|---|---|---|---|---|
| `CAN` | ✅ | ❌ (not listed) | `can-bus` | ❌ raw SocketCAN frames (fixed small data field, needs a DBC) |
| `LIN` | ✅ | ❌ | `lin-bus` (implied) | ❌ same frame-size problem as CAN |
| `GPIO` | ✅ | ❌ (not listed) | `gpio-panel` | ❌ discrete input-control widget, not a data pipe |
| `VIDEO` | ✅ | ✅ | none documented | ❌ shared-memory RGBA topic, not text/binary messages |
| `VHAL` | ❌ (not listed) | ✅ | Script Node acting as VehicleServer | ⚠️ Android vehicle-property model, needs a Script Node bridge |
| `KUKSA` | ✅ | ✅ | `kuksa-databroker` | ✅ VSS signal pub/sub; a custom `string` signal carries an opaque payload verbatim |
| `GENERIC` | unknown — **0 mentions** in [Car-Sky-Platform.html](../development-platform-doc/Car-Sky-Platform.html) despite being in the live `addPin` enum | unknown | unknown | unknown — **not verified live**, see § Open items |

`KUKSA` is the only documented pin type that (a) is REST/import-creatable, (b) is available on **both** Container and Skycraft nodes directly (no extra bridge node per leg), and (c) has an existing broker node type (`kuksa-databroker`) to star-wire into — the direct structural analogue of `eth-bridge`. It is the pick.

## 3. Feasibility verdict

**At-risk — flagged, not silently adopted.**

- **Conflicts with a ratified decision.** [node-v2x-ecu.md](node-v2x-ecu.md), [node-ada-ecu.md](node-ada-ecu.md), and [node-scenario-player.md](node-scenario-player.md) each already state *why not* `kuksa`: these links carry decoded CPM/JSON objects over ethernet/UDP, "not vehicle-bus or VSS signals" — and [node-ivi-ecu.md](node-ivi-ecu.md) excludes `kuksa` because report §4 bars GNSS/vehicle-signal injection into the IVI. Adopting this variant overrides that rationale project-wide, which CLAUDE.md governing principle 3 requires flagging, not absorbing.
- **R6 is frozen.** "Ethernet-bridge network" is R6 by name (CLAUDE.md governing principle 1). Swapping the transport means re-freezing R6 across every consumer (V2X_ECU, ADA_ECU, IVI_ECU, Scenario_Player), not a topology edit.
- **Timeline.** Deadline 2026-08-08, 10 days out from today. No node code exists yet beyond IVI_ECU's view-seam UI layer and READMEs — so this is the cheapest possible moment to switch transport, but it still adds a new dependency (a KUKSA gRPC client) in three languages (C++17, Python, Kotlin) plus a custom VSS artifact to author and upload, for a payoff that is purely operational (skip one five-minute manual UI step per blueprint). Against [solution-selection-criteria.md](../../.claude/rules/solution-selection-criteria.md) criteria #1–#2 (highest priority: implementation certainty and milestone speed), the existing Ethernet-plus-one-manual-step path is lower-risk for this milestone.
- **What stays unchanged if adopted.** The payload design here (§4) keeps R1/R2/R4 encodings byte-for-byte identical — only R6's transport binding changes, and only inside the R7 adapter seam's concrete implementation (the seam's `init · configure · subscribeRx · send` interface is unaffected). That containment is what makes this option *possible* to adopt later without touching R1–R4, R9, R10 logic — just their transport binding.

**Recommendation:** keep [blueprint-m1-cooperative-awareness.json](blueprint-m1-cooperative-awareness.json) (Ethernet + one manual UI step) as the default for M1. Use this variant only if the team explicitly decides full REST scriptability is worth re-freezing R6 and absorbing the new client-library work, with enough runway left to de-risk it.

## 4. Design — payload-preserving KUKSA transport

One custom VSS `string` signal per R6 leg, each carrying the existing contract's bytes verbatim (no re-encoding as "real" vehicle signals):

| Leg | Contract | VSS signal path | Value |
|---|---|---|---|
| Bench → V2X ECU | R1 CPM (ASN.1 UPER) | `Vehicle.V2X.M1.BenchToV2xCpmB64` | base64 of the raw CPM bytes |
| V2X ECU → ADA ECU | R2 JSON | `Vehicle.V2X.M1.V2xToAdaObjectJson` | JSON text, unchanged |
| ADA ECU → IVI ECU | R4 JSON | `Vehicle.V2X.M1.AdaToIviWarningJson` | JSON text, unchanged |

Custom VSS tree: [vss-m1-custom-signals.json](vss-m1-custom-signals.json) — a minimal COVESA-VSS-shaped overlay defining just these 3 paths (not the standard VSS catalog). **Not live-verified** — confirm CarSky's VSS artifact validator accepts a non-standard tree before relying on it (§ Open items).

Topology — same star shape as the baseline, broker in place of the bridge:

```
Bench ──kuksa──┐
V2X ECU ──kuksa─┼──▶ KUKSA Databroker (kuksa-databroker node)
ADA ECU ──kuksa─┤
IVI ECU ──kuksa─┘
```

Blueprint file: [blueprint-m1-cooperative-awareness-kuksa.json](blueprint-m1-cooperative-awareness-kuksa.json).

## 5. Prerequisites

1. CarSky account, Registry access, API key ([carsky-credential-verify](../../.claude/skills/carsky-credential-verify/SKILL.md)).
2. The 3 team OCI images (Bench, V2X ECU, ADA ECU) and the AAOS starter-pack artifact — same as the baseline ([carsky-4-node-blueprint.md §3](carsky-4-node-blueprint.md#3-prerequisites)).
3. Each of the 3 container nodes' app code links a KUKSA gRPC client (not yet implemented — flag for project-architecture if this variant is adopted): C++ (V2X_ECU, ADA_ECU core) via a KUKSA C++/gRPC client generated from the `kuksa.val.v1` proto; Python (Scenario_Player, ADA_ECU detector) via the `kuksa-client` PyPI package.
4. IVI_ECU (Kotlin/AAOS) needs a KUKSA client too, or an equivalent gRPC stub generated for Kotlin — **unresolved, open item below**.

## 6. Steps — import, build, deploy, test

1. **Author and upload the VSS artifact.** Artifacts → New Artifact → Category **VSS** → name it (e.g. `m1-custom-vss`) → Add Version → upload [vss-m1-custom-signals.json](vss-m1-custom-signals.json). Note the artifact ID and version ID.
2. **Fill in the blueprint file.** In [blueprint-m1-cooperative-awareness-kuksa.json](blueprint-m1-cooperative-awareness-kuksa.json), replace `REPLACE_WITH_VSS_ARTIFACT_ID` / `REPLACE_WITH_VSS_VERSION_ID` on the `kuksa-broker` node with the values from step 1, and the IVI node's `REPLACE_WITH_ARTIFACT_ID` / `REPLACE_WITH_VERSION_ID` / `REPLACE_WITH_ARTIFACT_VERSION` per [node-ivi-ecu.md](node-ivi-ecu.md) § Prepare the VM artifact.
3. **Push the 3 team images** — same commands as [carsky-4-node-blueprint.md §4](carsky-4-node-blueprint.md#4-steps) step 1, tagged `m1-scenario-player`, `m1-v2x-ecu`, `m1-ada-ecu`.
4. **Import the blueprint.** Nydus → Blueprint list → **Import from File** → select the filled-in JSON. This creates all 5 nodes with config. Node creation via import is proven safe (only `ethernet` pins are dropped on import — KUKSA is a different, non-blocked pinType).
5. **Verify the pins/edges survived import.** `GET /api/v1/blueprints/{id}` and check every node's `pins` array is non-empty and the 4 edges exist. **This is not yet live-verified** — the only confirmed-working ETHERNET-free import behavior is "nodes only, non-ethernet pins untested." If pins/edges came back empty:
   - **Fallback (proven-safe path):** add them via the REST `/batch` endpoint instead, same pattern as [carsky-rest-api-blueprint.md](carsky-rest-api-blueprint.md):
     ```
     POST /api/v1/blueprints/{id}/batch
     {"operations":[
       {"op":"addPin","data":{"nodeId":"<bench-node-id>","name":"kuksa","pinType":"KUKSA","direction":"OUTPUT"}},
       {"op":"addPin","data":{"nodeId":"<broker-node-id>","name":"kuksa","pinType":"KUKSA","direction":"INPUT"}},
       {"op":"addEdge","data":{"sourcePinId":"<bench-pin-id>","targetPinId":"<broker-pin-id>"}}
       /* repeat addPin+addEdge for v2x-ecu, ada-ecu, ivi-ecu */
     ]}
     ```
   - `KUKSA` is confirmed in the live `addPin` enum, so this fallback is expected to work even if bulk import of pins does not.
6. **Validate.** `POST /api/v1/blueprints/{id}/validate` — passes once every node has its `kuksa` pin and every pin has an edge to the broker.
7. **Deploy.** `POST /api/v1/deployments {blueprintId, roomId, name}` — same as [carsky-rest-api-blueprint.md § Deploy & test a Room](carsky-rest-api-blueprint.md#deploy--test-a-room).
8. **Verify nodes Running.** Poll `GET /api/v1/deployments/{roomId}/status` until `5/5` nodes report `Running` (R5 acceptance, unaffected by this variant).
9. **Install the IVI APK.** Same ADB step as [node-ivi-ecu.md § Post-deploy](node-ivi-ecu.md#post-deploy-install-the-team-apk).
10. **Test the signal path.** From a container's shell, use a KUKSA CLI/client (e.g. `kuksa-client` Python REPL) against the intercepted local address (`127.0.0.10:55555` per this variant's `kuksaIntercepts` config) to `get`/`publish` each of the 3 signal paths in § 4 and confirm the value round-trips to the peer node — the KUKSA-transport equivalent of the baseline's UDP-reachability check (R6 acceptance).
11. **Teardown / redeploy** — identical to [carsky-4-node-blueprint.md §6](carsky-4-node-blueprint.md#6-teardown--redeploy).

## 7. Open items — verify live before adopting

- **`GENERIC` pin type is undocumented.** It's in the live `addPin` enum but has zero mentions in [Car-Sky-Platform.html](../development-platform-doc/Car-Sky-Platform.html). If it turns out to be a raw point-to-point byte pipe (rather than KUKSA's signal-broker model), it would be a lower-friction Ethernet substitute — no VSS artifact, no per-node client SDK, closer to what R6 already assumes. Worth one live `addPin`/`addNode` probe (mirroring how [carsky-rest-api-blueprint.md](carsky-rest-api-blueprint.md) verified the `ETHERNET` rejection) before investing further in the KUKSA path.
- **Import behavior for non-ethernet pins/edges is unverified** — § 6 step 5 gives the proven-safe REST-batch fallback.
- **How the Skycraft (IVI) guest resolves the broker's intercepted address is unresolved.** Container nodes get `config.kuksaIntercepts` mapping a pin name to a local `ip:port`; the Skycraft node config block (`image`/`prefix`/`gpuBackend`/`displayWidth`/`displayHeight`) has no equivalent field in the documented schema. Whether the AAOS guest kernel gets an analogous intercepted address automatically, or the Kotlin app needs a different discovery mechanism, needs a project-architecture HLD decision before implementation — do not improvise this in code (same convention as the [node-code-layout.md](../../.claude/rules/node-code-layout.md) Vanetza-binding open item).
- **KUKSA client library choice per language** (C++, Python, Kotlin) is not yet researched to the depth [solution-selection-criteria.md](../../.claude/rules/solution-selection-criteria.md) requires (open-source-only + Linux-only constraints, license check) — out of scope for this blueprint-level note.
