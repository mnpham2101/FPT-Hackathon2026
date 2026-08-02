# Phase 5 mini-blueprint — ADA ECU + IVI ECU + Ethernet Bridge

Research note for the Phase 5 display track: the smallest deployable CarSky topology that exercises the R4 hop (ADA → IVI) on its own, so IVI work never waits on the comms track.

- **Serves:** R4, R5, R6, R16 acceptance ("the HMI runs on the AAOS node", "an R4 warning brings the warning view up").
- **Owns:** why a reduced topology, what it contains, how to create it, how it is verified.
- **Does not own:** the platform mechanics (blueprint canvas, deploy dialog, log reading) — [deploy-walkthrough-netcheck.md](../../../requirements/car-sky-guide/deploy-walkthrough-netcheck.md) is the worked procedure; the full 5-node design is [carsky-4-node-blueprint.md](../../../requirements/car-sky-guide/carsky-4-node-blueprint.md).

## 1. Why reduce the topology at all

The baseline blueprint carries 5 nodes (bench, V2X, ADA, IVI, bridge). For Phase 5 the bench and V2X nodes contribute nothing: the display track is defined as mock-driven from the start ([milestone1.md](../../../plans/milestone1.md) § Phase 5), and its only input is an R4 datagram on `10.99.0.13:47300`.

| Reason | Effect |
|---|---|
| Two concurrent deployments per account ([deploy-walkthrough §2.5](../../../requirements/car-sky-guide/deploy-walkthrough-netcheck.md)) | A 3-node Room leaves the comms track's Room untouched while Phase 5 iterates |
| Bench/V2X images are Phase 1 deliverables | A 5-node deploy fails on any image that is not yet pushed — three container nodes hang in `Provisioning` (walkthrough §4 mistake #1) |
| Fewer nodes = shorter deploy, fewer variables | The AAOS guest is the slowest node to reach `Running`; nothing else should compete with it |
| Phase 6 convergence must be a swap, not a rebuild | Keeping addresses and ports identical to the baseline means the IVI node config is already correct when the real ADA image replaces the simulator |

## 2. Composition — three nodes, one edge each

| Node | CarSky type | Address | Runs in Phase 5 |
|---|---|---|---|
| Ethernet Bridge | Ethernet Bridge Node | `10.99.0.1`, subnet `10.99.0.0/24`, `bridgeMode: "linux"` | The R6 virtual L2 network |
| ADA ECU | Container Node | `10.99.0.12` | The **R4 simulator image**, not the real ADA image — see [phase5-r4-simulator.md](phase5-r4-simulator.md) |
| IVI ECU | Skycraft Node (AAOS guest) | `10.99.0.13` | The AAOS artifact + the team APK installed post-deploy |

Addresses, the `47300` port, and the pin shapes are unchanged from the baseline ([carsky-4-node-blueprint.md §1](../../../requirements/car-sky-guide/carsky-4-node-blueprint.md#1-topology)) — deliberately, so nothing about the IVI node has to be re-learned at Phase 6. The ADA node keeps its identity (address, pin, env var *names*) and changes only which image it pulls.

**Slot the simulator into the ADA node rather than adding a fourth "test" node.** A separate node would need its own `ethernet` pin, and pins cannot be created by REST or by JSON import (§3) — the ADA node already has one.

## 3. Creation route — clone, then delete

**Clone the baseline and remove two nodes.** This is the only route that preserves `ethernet` pins.

The platform limit that forces it: *neither the REST API nor Nydus "Import from File" can create `ethernet` pins or bridge edges* — import silently drops them, and hand-authored JSON with pins fails with `source or target pin not found` ([carsky-4-node-blueprint.md](../../../requirements/car-sky-guide/carsky-4-node-blueprint.md), [carsky-rest-api-blueprint.md](../../../requirements/car-sky-guide/carsky-rest-api-blueprint.md)). A blueprint built from scratch by script has no network at all.

Steps, in order:

1. **Clone** the known-good baseline — `POST /api/v1/blueprints/{id}/clone`, or Export Selected → Import from File in the UI. Source: `trial2_minh_netcheck`, the blueprint that passed C1–C5 ([phase0-smoke-test-run.md](../../../plans/doc/phase0-smoke-test-run.md)).
2. **Rename** it so the trial is identifiable from the Blueprint list (e.g. `trial3_minh_ivi`) — the differentiator goes in Name/Description, never in code.
3. **Delete the Bench and V2X ECU nodes** on the canvas. Deleting a node removes its pin and its edge; the ADA and IVI pins are untouched.
4. **Verify the two surviving edges by reading the config back** — `GET /api/v1/blueprints/{id}` returns each node's stored config verbatim, which the Inspector's truncated fields do not. Expect one `ETHERNET` / `OUTPUT` pin per role node, `properties.address` = `10.99.0.12` and `10.99.0.13`, both wired to the bridge's single `INPUT` pin.
5. **Reconfigure the ADA node** to the simulator image (§4).
6. **Leave the IVI node's `image` block alone** — artifact `AAOS`, `artifactId x9oqgIwzTp1m26SWIQqJt`, `versionId xSU_Q7YJZUxxUgDr4Ugcp`, `0.0.1`, `aarch64` ([node-ivi-ecu.md](../../../requirements/car-sky-guide/node-ivi-ecu.md)). Without it the deploy is rejected: `skycraft requires 'image' config with VM image artifact details`.

If a clone ever comes back without pins, they must be re-drawn by hand on the canvas — there is no scripted repair.

## 4. ADA node config for Phase 5

```json
{
  "container": {
    "image": "registry.hackathon-2.carsky.io/m1-r4-sim:latest",
    "command": ["./entrypoint.sh"],
    "capabilities": ["NET_RAW"],
    "env": {
      "IVI_ECU_HOST": "10.99.0.13",
      "IVI_ECU_PORT": "47300",
      "R4_SCENARIO": "/app/scenarios/approach.json",
      "R4_RATE_HZ": "1",
      "START_DELAY_S": "20"
    }
  }
}
```

- **Env var names match the real ADA node** (`IVI_ECU_HOST` / `IVI_ECU_PORT`, [node-ada-ecu.md](../../../requirements/car-sky-guide/node-ada-ecu.md)) so the Phase 6 image swap needs no config edit.
- **Image must be single-platform `linux/arm64`**, built with `--provenance=false --sbom=false`; a multi-platform manifest index fails to pull on this cluster ([phase0-smoke-test-run.md § Standing requirement](../../../plans/doc/phase0-smoke-test-run.md)).
- **`command` is relative** — `./entrypoint.sh`, not `/entrypoint.sh`; it resolves against the image workdir `/app` (walkthrough §4 mistake #5).
- **`NET_RAW`** only if the run should also produce a tcpdump `[CAP]` line as wire evidence; the simulator works without it.
- **`START_DELAY_S`** exists so the APK is listening before the first datagram — the AAOS guest boots slower than a container.

## 5. Verification ladder

| Check | Evidence | Closes |
|---|---|---|
| Both nodes + bridge reach `Running`, restart count 0 | Deployment Viewer badges | R5 |
| Simulator emits | ADA node **View Log**: `[TX] … → 10.99.0.13:47300` | — |
| Datagram on the wire | ADA node `[CAP]` line (needs `NET_RAW`) | R6 |
| **IVI receives** | `adb logcat -s IVI_V2X` on the guest shows the parsed R4 event | R6 hop 3, smoke-test **O4** |
| Warning view comes up | Screenshot / recording of the Display Area | R16, R17 |

The fourth row is the one this blueprint exists for. Phase 0 could only verify hop 3 *indirectly* (ADA-side `[TX]` + `[CAP]`), because the AAOS guest had no listener and the platform's VM shell route answers 502 ([deploy-walkthrough §3 M10](../../../requirements/car-sky-guide/deploy-walkthrough-netcheck.md), open item O4). The R4 listener in the APK is what retires that gap — as the walkthrough itself predicts.

## 6. Open items and risks

| Item | Status | Impact if it bites |
|---|---|---|
| **ADB reach to the Skycraft guest** | Unverified on this deployment. Route is the Rework device panel or the CarSky Gateway ADB tunnel ([node-ivi-ecu.md](../../../requirements/car-sky-guide/node-ivi-ecu.md)); the REST VM-shell route is known-502 | No `adb install` ⇒ no APK on the node ⇒ every in-Room criterion falls back to emulator evidence. **Verify this before the APK is finished, not after** |
| **AAOS guest Android version** | Unknown; APK targets `minSdk 29 / targetSdk 33` | A guest below API 29 rejects the APK outright |
| **MTU headroom (O3)** | Open; bridge is a tunnelled fabric, 1500 B not guaranteed | An R4 warning is ~450 B, so headroom is ample — non-issue for this hop |
| **Deployment budget** | 2 concurrent Rooms | Tear down the Phase 5 Room (Delete Deployment) before the comms track needs a second |
