# CarSky Blueprint Setup — M1 4-Node Topology

Step-by-step guide to build the Milestone 1 blueprint in Nydus: one node per ECU/bench role (V2X ECU, ADA ECU, IVI ECU, Bench Scenario Player), wired through one Ethernet Bridge node. Serves report §1 "Cloud development constraints", R5 (node deployment), R6 (network).

Per-node detail (image prep, pin config, env vars) lives in its own file — this guide covers the blueprint-level steps: canvas, wiring, deploy, verify.

- [node-scenario-player.md](node-scenario-player.md) — Bench Scenario Player
- [node-v2x-ecu.md](node-v2x-ecu.md) — V2X ECU
- [node-ada-ecu.md](node-ada-ecu.md) — ADA ECU
- [node-ivi-ecu.md](node-ivi-ecu.md) — IVI ECU

> **Scripted alternative:** [carsky-rest-api-blueprint.md](carsky-rest-api-blueprint.md) creates the blueprint + all 5 nodes over the REST API in one call (verified live); [blueprint-m1-cooperative-awareness.json](blueprint-m1-cooperative-awareness.json) does the same 5 nodes via Nydus **Import from File**. Note a hard platform limit found in both paths: **neither the REST API nor JSON import can create `ethernet` pins or wire the bridge** (import silently drops them, so a hand-authored JSON with `ethernet` pins/edges fails with "source or target pin not found" once the edges reference pins that were never created) — steps 5's pin-wiring must be done in the UI canvas regardless of how the nodes were created.

## 1. Topology

One blueprint = one car (report §1). It contains 4 role nodes plus the bridge that connects them — 5 nodes total in the Room.

| Node | CarSky node type | Role | Ethernet pin address (proposed) |
|---|---|---|---|
| Ethernet Bridge | Ethernet Bridge Node | R6 virtual L2 network; every other node's ethernet pin targets this | `10.99.0.1` (bridge itself, subnet `10.99.0.0/24`) |
| Bench — Scenario Player | Container Node | Emulates the Quectel modem's connection point toward the V2X ECU; generates CPMs (R11) | `10.99.0.10` |
| V2X ECU | Container Node | Decodes/encodes CPM, Rx pipeline to ADA, ego Tx (R7–R10) | `10.99.0.11` |
| ADA ECU | Container Node | Detection, track store, risk assessment, warning emission (R12–R15) | `10.99.0.12` |
| IVI ECU | Skycraft Node (AAOS guest) | God-view HMI (R16, R17) | `10.99.0.13` |

Static addresses are recommended over auto-assignment so every node's UDP send target is deterministic across redeploys — read them from each node's env config, never hardcode them in source (CLAUDE.md governing principle 5). All four role nodes declare exactly **one** `ethernet` pin each, wired to the bridge; the different message flows (bench↔V2X, V2X→ADA, ADA→IVI) are separate UDP ports on that same NIC, not separate pins.

Proposed UDP ports (externalize as config, confirm with team before freezing):

| Flow | Contract | Proposed port |
|---|---|---|
| Bench → V2X ECU | R1 CPM (ASN.1 UPER) over the R7 modem-stub UDP channel | `47100` |
| V2X ECU → ADA ECU | R2 JSON | `47200` |
| ADA ECU → IVI ECU | R4 JSON | `47300` |

### Pin kinds by node type

Each per-node file states which of these it uses in M1 (always `ethernet` only) and why the rest don't apply — this table is the shared reference, not repeated per file.

| CarSky node type | Available pin kinds |
|---|---|
| Container Node | `can`, `lin`, `kuksa`, `gpio`, `ethernet`, `video`, `usb`, `tunnel` |
| Skycraft Node | `vhal`, `kuksa`, `ethernet`, `video`, `usb` |
| Ethernet Bridge Node | not a pin owner itself — it's the common target every other node's `ethernet` pin wires to |

## 2. Terminology: Device vs. Node

CarSky's own docs use "Device" for two unrelated things — worth disambiguating before §4 step 7 (**New Deployment**) asks you to pick one:

- **Device (Mental Model / Devices panel):** real external hardware (a hub, ECU, phone) or a VM connecting into a Room from *outside* the cluster, through a Proxy/Outpost node. Not used in M1 — development runs entirely on the CarSky cloud platform (report §1 "Cloud development constraints"), so there's no physical bench hardware to bridge in.
- **Device (New Deployment dialog):** the K8s cluster/resource pool the Room's pods get scheduled onto — an infrastructure target, unrelated to which ECU is simulated.

**Neither meaning is one of our 4 ECU/bench roles.** V2X ECU, ADA ECU, IVI ECU, and the Bench Scenario Player are all **Nodes** inside the blueprint (3× Container Node, 1× Skycraft Node) — none of them is a Device. The only Device this guide touches is the deployment-target cluster in step 7, and it plays no part in which ECU is simulated.

## 3. Prerequisites

- CarSky account with Registry access; a Zot API key for `docker login` (see [Log In to Zot Registry & Get API Key](../development-platform-doc/Car-Sky-Platform.html) in-platform guide).
- OCI images built and ready to push for the 3 team-built nodes: Bench, V2X ECU, ADA ECU (per-node files below give the build/push commands).
- The starter-pack AAOS artifact (image `.zip` + `cvd-host_package.tar.gz`) uploaded to **Artifacts → New Artifact → Category: ANDROID IMAGE**, tagged `image` / `host_package` roles.
- The team IVI APK ready for ADB install after deploy (not baked into the VM image).

## 4. Steps

1. **Push the 3 team images to the registry.**
   `docker login registry.carsky.io -u <username>` (password = Zot API key), then tag and push each of the Bench, V2X ECU, and ADA ECU images — commands are in each node's file.
2. **Create the blueprint.** Nydus → Blueprint list → **New Blueprint**. Name it (e.g. `m1-cooperative-awareness`).
3. **Open the canvas** and drag one **Ethernet Bridge Node** onto it. In its Inspector, set `bridgeMode: "linux"`, `subnet: "10.99.0.0/24"`.
4. **Add the 4 role nodes** — drag from the node library:
   - 3× **Container Node** for Bench, V2X ECU, ADA ECU.
   - 1× **Skycraft Node** for the IVI ECU.
   For each, open its per-node guide and apply the `container`/`image` config, add its single `ethernet` pin, and set the pin's target to the Ethernet Bridge node with the static address from §1.
5. **Wire the pins.** Drag from each role node's `ethernet` pin connector to the Ethernet Bridge node's connector — same-type wiring (`ethernet ↔ ethernet`) is the only valid pairing here. Four edges total, all terminating at the bridge (a star topology, not a chain — R6's "every ECU node declares an `ethernet` pin wired to the bridge").
6. **Blueprint-level config.** Click empty canvas space → Inspector shows blueprint config. Set Name/Description; leave Locked off until the topology is stable.
7. **New Deployment.** Inspector → **New Deployment** → select or create a Device (§2 — the K8s resource pool, not an ECU) → set a Deployment name → **Deploy**.
8. **Verify.** Open the **Deployment Viewer**; wait for the header to read `5/5` nodes ready and every node badge `Running` (R5 acceptance).
9. **Install the IVI APK.** Use the Skycraft node's ADB access (Rework device panel or CarSky Gateway ADB tunnel) to `adb install` the team APK (R5 acceptance: "the team APK launches on the AAOS node").
10. **Verify network reachability (R6).** From each Container node's shell (or a debug pin), confirm UDP reachability to its peer(s): Bench → V2X ECU, V2X ECU → ADA ECU, ADA ECU → IVI ECU. Capture traffic on the bridge (tcpdump inside a container, or the platform's packet-capture facility if available) for the R18/R19 evidence trail.

## 5. Acceptance mapping

- **R5:** blueprint with the 4 nodes + bridge deploys to a Room; Deployment Viewer reports every node Running; team APK launches on the AAOS node.
- **R6:** wiring matches the communication topology (bench ↔ V2X ECU, V2X ECU → ADA, ADA → IVI); UDP reachability demonstrated between every communicating pair; traffic captured on the bridge network.

## 6. Teardown / redeploy

Deployment Viewer → **Delete Deployment** removes the Room and frees K8s resources; the blueprint itself is kept and can be redeployed. Use **Redeploy** instead of delete+recreate when only pushing an updated image or config to an already-running Room.

## 7. Multiple blueprints (trial variants)

Yes — the Blueprint list is a list of independently named designs, not a single slot. Create as many as needed, e.g. `trial1`, `trial2`, to try different node configs, scenario sets, or topology tweaks side by side without touching a known-good baseline.

- **New, from scratch:** Blueprint list → **New Blueprint** → give it a distinct name (`trial1`) and description. Repeat with another name (`trial2`) for a second, fully independent design.
- **New, from an existing baseline (recommended for trials):** rather than rebuilding the 4-node topology by hand each time, start from a working blueprint and branch it:
  - **Clone via API:** `POST /api/v1/blueprints/{id}/clone` duplicates an existing blueprint (nodes, pins, edges, config) into a new one you then rename.
  - **Export / Import via UI:** select the baseline blueprint → **Export Selected** (downloads the topology JSON) → **Import from File** → the imported copy becomes a new, separately-named blueprint you can then rename/re-tag.
- **Each blueprint deploys independently.** A blueprint and a Deployment are different things (§ concepts in [Nydus → Canvas & Node Library](../development-platform-doc/Car-Sky-Platform.html)): one blueprint can itself be deployed multiple times into independent Rooms with independent IPs/ports. Use **that** (repeated **New Deployment** on the *same* blueprint) when you just want several concurrent runs of the identical topology — reserve separate blueprints (`trial1`/`trial2`) for when the topology, node images, or config actually differ between trials.
- **Keep trials distinguishable:** put the differentiator in the blueprint's Name/Description (Inspector, empty-canvas view) and in each Container node's env (e.g. `SCENARIO_CONFIG=/app/scenarios/trial2.yaml`) — not in code, per CLAUDE.md governing principle 5.
- Deleting a blueprint requires deleting every one of its running Deployments first (§4 steps 7–8, New Deployment / Deployment Viewer) — trial blueprints you're done with need their Deployments torn down before the blueprint itself can be removed.
