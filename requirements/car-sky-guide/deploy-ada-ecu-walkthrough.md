# ADA ECU Bring-Up — Isolated Room, Deploy to Verified Fusion

**The authoritative procedure for exercising the ADA ECU on its own**, in a Room containing only the ADA node, a stand-in for the V2X ECU that feeds it, a stand-in for the IVI ECU that consumes its output, and the Ethernet Bridge that joins them.

- **Companion to** [deploy-walkthrough-netcheck.md](deploy-walkthrough-netcheck.md) — the template for container nodes (build → push to Zot → node pulls). Read it once for the platform model (blueprint, node, pin, Device, Room) and the credential table; none of that is repeated here.
- **Companion to** [deploy-ivi-hmi-walkthrough.md](deploy-ivi-hmi-walkthrough.md) — the same chain seen from the consumer end, on the Android node. It owns everything about the IVI app's build, install, launch and verification.
- **Node facts** — image tag, blueprint config block, pin shape, address — live in [node-ada-ecu.md](node-ada-ecu.md) and are linked, never copied. That file owns the node's *facts*; this one owns the *doing*.

**What this Room proves, and what it does not.** It proves the ADA node's whole internal chain: a relayed object arrives on the wire, an event is raised, a track is admitted, ego's own detector produces a second track from the baked-in clip, and a warning carrying both vehicles leaves the node on the wire. It does not prove anything about the real V2X ECU or the real IVI app — both are replaced by bench containers here. The full-chain variant is [§5.6](#56-the-full-blueprint-route).

```
bench + ADA sources  ──push──▶  GitHub Actions  ──push──▶  Zot registry  ──pull──▶  Room nodes
    (git repo)                    (two jobs)                                         (§4.5)
```

```
relayed-object datagrams  ──▶  ADA ECU  ──▶  warning datagrams
   V2X bench (mock)            10.99.0.12        IVI sink (mock)
     10.99.0.11                                    10.99.0.13
      UDP 47200                                     UDP 47300
                          ego's own detector
                          on the baked-in clip
```

**Most of this route has never been run.** The ADA ECU container image, the bench image of [§2.3](#23-the-bench-image--one-image-two-roles), the two CI jobs that build them, and this topology are all unexercised. Read [§8.1](#81-confirm-before-relying-on-these) before scheduling work against any step below.

---

## 1. Prerequisites

### 1.1 Toolchain on the build machine

**Nothing is built by hand.** Both images come off GitHub Actions ([§3](#3-build-the-images-on-ci)), so the machine following this guide needs no Docker, no compiler and no Python.

| Need | Value | Used at |
|---|---|---|
| A clone of this repository, with push access | any | [§3.1](#31-write-the-bench-scripts), [§3.2](#32-build-and-push-the-images-on-ci) |
| `curl` | any | Every registry and platform call below |
| A browser | any | The Nydus canvas and the Actions run page |

Optionally the [`gh` CLI](https://cli.github.com/), authenticated with `gh auth login` — it replaces the browser half of [§3.3](#33-confirm-the-run-passed-and-the-images-landed).

### 1.2 Cloud platform access

| Credential | Format | Used for |
|---|---|---|
| Keycloak login | email + password | Signing in to the Nydus and Zot web UIs |
| CarSky API key | `a8k_…` | Every REST call below — blueprint creation, config read-back, node phases, logs |
| Zot API key | `zak_…` | Stored as the `CARSKY_ZOT_API_KEY` repository secret; the CI jobs' `docker login` password ([zot-registry-api-key.md](zot-registry-api-key.md)) |

Verifying a key before it is needed: [carsky-deploy-preflight](../../.claude/skills/carsky-deploy-preflight/SKILL.md).

One Room slot must be free. The account allows two concurrent deployments; this procedure consumes one for its whole duration.

### 1.3 Deliverable prerequisites

The ADA ECU's own software must exist before the procedure is run. Each row is product code the procedure consumes, not a step of this guide — the bench scripts beside it *are* a step, [§3.1](#31-write-the-bench-scripts). Check every row is present before starting; item 2 of [§8.1](#81-confirm-before-relying-on-these) is why.

| Deliverable | What it must provide |
|---|---|
| `ADA_ECU/Dockerfile` | A single-platform `linux/arm64` image carrying the core binary at `/app/ada_ecu`, the Python detector under `/app/detector/`, the model at `/app/models/yolo11n.onnx` and the clip at `/app/media/ego-b-occluding-c.mp4` |
| The ADA core binary | Binds the relayed-object port, spawns the detector, maintains the track store, runs the risk assessment, and sends warning datagrams to the configured IVI address |
| The event stream | One `[EVT]`-prefixed JSON line per event on stdout, with the event names and payload fields [§5](#5-run-the-checks) greps for |
| The Python detector | Reads the baked-in clip, emits one tracked-object JSON line per detection on stdout, and never mints a relayed source or a `v2x:` track id |
| `ADA_ECU/models/yolo11n.onnx` | The committed detection model the detector loads |
| `ADA_ECU/media/ego-b-occluding-c.mp4` | The clip ego's detector runs on, containing vehicle B and never vehicle C |
| `ADA_ECU/entrypoint.sh` and `capture.sh` | Start the capture alongside the core, so the node's own log carries `[CAP]` lines. **Required, not optional:** the node's `command` is `./entrypoint.sh`, so an image without it dies at start |

### 1.4 Blueprints

Both Rooms this guide can run against are **a clone of `baseline_phase1`** — the five-node baseline kept on the platform, and the clone source for every Room after the smoke test ([carsky-4-node-blueprint.md § The blueprints on CarSky](carsky-4-node-blueprint.md#8-the-blueprints-on-carsky)). They differ in what the clone is reduced to:

| Room | Made from `baseline_phase1` by | Use when |
|---|---|---|
| **The isolated blueprint** — Ethernet Bridge + V2X bench (mock) at `.11` + ADA ECU at `.12` + IVI sink (mock) at `.13` | deleting the Scenario Player and the IVI Skycraft node, adding one Container node for the sink, and repointing images | ADA work alone: every neighbour under your control, and a sink that can log, check and capture |
| **The full blueprint** — bench Scenario Player, V2X ECU, ADA ECU, IVI Skycraft node, Ethernet Bridge | cloning and renaming; nothing removed | Full-chain work — [§5.6](#56-the-full-blueprint-route) |

**The isolated blueprint is what this guide runs against.** Its composition and every node's config is [§2](#2-the-isolated-blueprint); its creation is [§4.1](#41-create-the-blueprint).

**Clone; never build either one from scratch and never import one.** A clone keeps its `ethernet` pins, and the platform can create them by no other route — neither REST nor a JSON import can make them, and an import silently drops them. Whatever the isolated Room still needs drawn by hand is [§4.2](#42-wire-the-ethernet-pins). The platform limitation and its consequence: [carsky-rest-api-blueprint.md § Key finding](carsky-rest-api-blueprint.md#key-finding-what-rest-can-and-cannot-do) and [carsky-4-node-blueprint.md § Steps](carsky-4-node-blueprint.md#4-steps).

---

## 2. The isolated blueprint

### 2.1 Topology

Four nodes. Every address and port is the value the full blueprint uses, so the ADA node's own config is identical in both — switching between them is a change to the neighbours alone.

| Node | Type | Address | Role in this Room |
|---|---|---|---|
| Ethernet Bridge | Ethernet Bridge Node | `10.99.0.1`, subnet `10.99.0.0/24` | The virtual L2 network; every other node's `ethernet` pin targets it |
| V2X Bench (mock) | Container Node | `10.99.0.11` | Emits relayed-object datagrams to the ADA node on UDP `47200`, describing vehicle C as seen by vehicle B |
| ADA ECU | Container Node | `10.99.0.12` | The node under test |
| IVI Sink (mock) | Container Node | `10.99.0.13` | Binds UDP `47300`, logs and checks every warning datagram, and captures the traffic |

- The bench emitter stands in for the V2X ECU **only at its output edge**. It performs no decoding and sends no encoded frames; it produces exactly the JSON the ADA node expects to receive, at the cadence the real node would.
- The sink stands in for the Android node. It is a Linux container, so it can log, check and capture — none of which the real Android node does.
- Vehicle C is never visible to ego's detector. That is a property of the clip, not of this Room: the clip contains only vehicle B, so C can reach the store through the relayed path alone.

### 2.2 The blueprint definition and where it lives

**Designated path: `requirements/car-sky-guide/blueprint-ada-isolated.json`**, beside [blueprint-m1-cooperative-awareness.json](blueprint-m1-cooperative-awareness.json), which it follows in shape. The content below is what belongs in that file.

**It is the config specification, not an import payload.** The Room is created by cloning `baseline_phase1` ([§4.1](#41-create-the-blueprint)), so this file is never imported and never `POST`ed — importing it would produce a pinless blueprint. It is what each node's Inspector is edited *to* ([§4.3](#43-configure-each-nodes-image)) and what the read-back is diffed *against* ([§4.4](#44-read-the-stored-config-back)): one place holding the values, so a typo on the canvas is a diff line rather than a mystery.

```json
{
  "version": 1,
  "blueprint": {
    "name": "ada-isolated",
    "description": "ADA ECU exercised alone: a bench emitter stands in for the V2X ECU, a bench sink stands in for the IVI ECU, both from one image selected by ROLE. Addresses and ports match the full vehicle blueprint, so the ADA node's config is unchanged between the two. Config specification only - the Room is made by cloning baseline_phase1 and editing it on the canvas, because ETHERNET pins are rejected on import and over REST.",
    "zones": [],
    "nodes": [
      {
        "id": "eth-bridge",
        "label": "Ethernet Bridge",
        "nodeType": "eth-bridge",
        "config": { "bridgeMode": "linux", "subnet": "10.99.0.0/24" },
        "positionX": 0,
        "positionY": -40,
        "pins": []
      },
      {
        "id": "v2x-bench-mock",
        "label": "V2X Bench (mock)",
        "nodeType": "container",
        "config": {
          "image": "registry.hackathon-2.carsky.io/m1-ada-bench:latest",
          "command": ["./entrypoint.sh"],
          "capabilities": ["NET_RAW"],
          "env": {
            "ROLE": "v2x_mock",
            "TARGET_HOST": "10.99.0.12",
            "TARGET_PORT": "47200",
            "PROFILE": "approaching",
            "RATE_HZ": "10",
            "START_DELAY_S": "20",
            "STATION_ID": "1201",
            "OBJECT_ID": "7",
            "START_DISTANCE_M": "45",
            "MIN_DISTANCE_M": "5",
            "CLOSING_RATE_MPS": "5",
            "LATERAL_M": "1.2",
            "OBJECT_SPEED_MPS": "15.2",
            "CAPTURE_FILTER": "udp port 47200"
          }
        },
        "positionX": -450,
        "positionY": -40,
        "pins": []
      },
      {
        "id": "ada-ecu",
        "label": "ADA ECU",
        "nodeType": "container",
        "config": {
          "image": "registry.hackathon-2.carsky.io/m1-ada-ecu:latest",
          "command": ["./entrypoint.sh"],
          "capabilities": ["NET_RAW"],
          "env": {
            "V2X_LISTEN_HOST": "0.0.0.0",
            "V2X_LISTEN_PORT": "47200",
            "IVI_ECU_HOST": "10.99.0.13",
            "IVI_ECU_PORT": "47300",
            "GATE_ENTER_M": "30",
            "GATE_EXIT_M": "35",
            "CONFIRM_HITS": "3",
            "TRACK_TIMEOUT_MS": "1000",
            "FUSION_TICK_MS": "100",
            "DETECTOR_ENABLED": "true",
            "DETECTOR_LOOP": "true",
            "VIDEO_CLIP_PATH": "/app/media/ego-b-occluding-c.mp4",
            "DETECTOR_FRAME_STRIDE": "4",
            "MODEL_PATH": "/app/models/yolo11n.onnx",
            "CRA_ENABLED": "nlos_obstruction",
            "RISK_NEAR_M": "60",
            "RISK_CRITICAL_M": "30",
            "CAPTURE_FILTER": "udp port 47300"
          }
        },
        "positionX": 0,
        "positionY": 200,
        "pins": []
      },
      {
        "id": "ivi-sink-mock",
        "label": "IVI Sink (mock)",
        "nodeType": "container",
        "config": {
          "image": "registry.hackathon-2.carsky.io/m1-ada-bench:latest",
          "command": ["./entrypoint.sh"],
          "capabilities": ["NET_RAW"],
          "env": {
            "ROLE": "ivi_mock",
            "LISTEN_PORT": "47300",
            "SUMMARY_EVERY_S": "10",
            "CAPTURE_FILTER": "udp port 47300"
          }
        },
        "positionX": 450,
        "positionY": -40,
        "pins": []
      }
    ],
    "edges": []
  }
}
```

Two notes on the ADA node's block:

- The env set above is the subset this Room needs. The node's full configuration surface, with defaults and meanings, is in [the node's design document](../../ADA_ECU/doc/ada-ecu-hld.md); the node's own config block is owned by [node-ada-ecu.md § Blueprint node config](node-ada-ecu.md#blueprint-node-config).
- **`command: ["./entrypoint.sh"]` and `capabilities: ["NET_RAW"]` are unconditional** — they are part of the ADA ECU node config in every blueprint, isolated or full, and [node-ada-ecu.md § Blueprint node config](node-ada-ecu.md#blueprint-node-config) is the authority for both. There is no variant that omits them: the image must ship `entrypoint.sh` and `capture.sh` ([§1.3](#13-deliverable-prerequisites)), and a node whose image lacks them dies at start rather than falling back.

### 2.3 The bench image — one image, two roles

**One image, `m1-ada-bench:latest`, serving both bench nodes**, with the role picked by the `ROLE` environment variable. One image means one build, one push, one tag to keep straight — the pattern [tools/netcheck/](../../tools/netcheck/) already uses for three roles. Two images reach this Room in total: this one and `m1-ada-ecu:latest`.

The image is Alpine plus `python3` and `tcpdump`. Its entrypoint starts `capture.sh` in the background and the role script in the foreground, so the pod's lifetime is the role script's lifetime and a deploy alone produces evidence. No shell session is ever needed.

| File in `tools/ada-bench/` | Purpose |
|---|---|
| `entrypoint.sh` | Prints a `[BOOT]` line, launches `capture.sh`, then `exec`s the script named by `ROLE` |
| `capture.sh` | `tcpdump -i any -n -l` on `CAPTURE_FILTER`, each line prefixed `[CAP]`; falls back to packet counters without `NET_RAW` |
| `mock_v2x.py` | The `v2x_mock` role — the relayed-object emitter |
| `mock_ivi.py` | The `ivi_mock` role — the warning sink and checker |
| `Dockerfile` | `FROM alpine:3.20`, `apk add python3 tcpdump`, `WORKDIR /app`, copy the four files, `CMD ["./entrypoint.sh"]` |

**`ROLE=v2x_mock` — the emitter.** Sends one UDP datagram per tick to `TARGET_HOST:TARGET_PORT` at `RATE_HZ`, after `START_DELAY_S` seconds so the ADA node is listening first. Each datagram is one relayed-object message, matching [r2-v2x-object.schema.json](../../ADA_ECU/contracts/r2-v2x-object.schema.json) field for field:

- `stationId` is `STATION_ID` — the station that saw C, that is vehicle B.
- `object.objectId` is `OBJECT_ID` — vehicle C, as B labelled it.
- `object.distance` and `object.position.x` walk from `START_DISTANCE_M` down to `MIN_DISTANCE_M` at `CLOSING_RATE_MPS`, then hold. `object.position.y` is `LATERAL_M`.
- `PROFILE=approaching` uses that walk. `PROFILE=out_of_range` holds the distance at `START_DISTANCE_M` — beyond the drop gate, so no track is ever admitted. Use it to prove the gate rejects, not only that it admits.
- Every message uses SI units and `classification: "vehicle"`, with `object.confidence` and `sender.speed` populated so no nullable field is exercised by accident.

One log line per datagram:

```
[TX] seq=42 objectId=7 distance=30.5 bytes=318 -> 10.99.0.12:47200
```

**`ROLE=ivi_mock` — the sink.** Binds `0.0.0.0:LISTEN_PORT`, parses every datagram as JSON, and checks it against the shape in [r4-ada-ivi.schema.json](../../ADA_ECU/contracts/r4-ada-ivi.schema.json). It performs explicit field checks rather than full schema validation, so the image stays Python-standard-library only. Two lines per datagram, plus a summary every `SUMMARY_EVERY_S` seconds:

```
[RX] seq=3 from=10.99.0.12:51044 bytes=486 type=warning warningType=nlos_obstruction risk=medium cSource=v2x_relayed cPos=(41.2,1.7) bPos=(11.0,0.4)
[CHECK] seq=3 both_vehicles=yes c_source_relayed=yes
[SUMMARY] received=31 warnings=31 both_vehicles=31 rejected=0
```

- `both_vehicles=yes` requires `geometry.vehicleB` and `geometry.vehicleC` both present with numeric `x` and `y`.
- `c_source_relayed=yes` requires the message's `object.source` to be exactly `v2x_relayed`.
- `rejected` counts datagrams that were not valid JSON, or whose `type` was neither `warning` nor `state`.

### 2.4 Where the bench sources live, and why

**Decision: both bench roles build from `tools/ada-bench/` at the repository root, and the image is `m1-ada-bench:latest`.** They are not placed in any node folder. The reasoning, against [node-code-layout.md](../../.claude/rules/node-code-layout.md):

| Candidate location | Rejected because |
|---|---|
| `V2X_ECU/` | That folder is one self-contained build context producing the real V2X ECU image. A stand-in for that node is not its implementation; putting a second Dockerfile and stand-in sources in the context breaks the one-folder-one-image rule and risks shipping bench code in the real image |
| `IVI_ECU/` | That folder builds an Android APK for the Skycraft node. A Linux container has no build path there at all |
| `ADA_ECU/` | The folder root Dockerfile is the node under test. Its build context should stay minimal, and the bench must be able to change without rebuilding the thing it is testing |
| A fifth top-level node folder | The four top-level code folders are one per node in the full blueprint. These two containers are not nodes of that blueprint; they replace nodes that are |

`tools/` is where this repository already keeps test equipment that deploys as a Container node: [tools/netcheck/](../../tools/netcheck/) builds `m1-netcheck:latest` and runs as three nodes. `tools/ada-bench/` is the same category of artifact, held to the same rules — self-contained, its own Dockerfile, no imports from any node folder, no hardcoded peer addresses.

**The bench mirrors two contracts and must not fork them.** The emitter's message shape and the sink's field checks follow the schema copies under `ADA_ECU/contracts/`. Keep the bench's copy of any field list byte-identical to those files, or the bench will pass a message the real consumer rejects.

---

## 3. Build the images on CI

Both images are built and pushed by GitHub Actions on a Linux runner. **No Docker is needed on the developer machine, and no image is ever built by hand.** Change the code → push → the lane rebuilds and republishes.

### 3.1 Write the bench scripts

The five files of [§2.3](#23-the-bench-image--one-image-two-roles), under `tools/ada-bench/`. Two rules make one image serve both roles:

- **Nothing about the topology is in the code** — addresses, ports, cadence, profile and role come from environment variables.
- **The container starts its role itself**, so a deploy alone produces evidence; no shell session is ever needed.

Keep the emitter's field list and the sink's checks aligned with the contract copies, per [§2.4](#24-where-the-bench-sources-live-and-why).

### 3.2 Build and push the images on CI

Two jobs, one per image, following the shape of the existing image jobs in [.github/workflows/phase1-ci.yml](../../.github/workflows/phase1-ci.yml):

| Job | Image | Build context |
|---|---|---|
| `ada-bench-image` | `m1-ada-bench:latest` | `tools/ada-bench/` |
| `ada-ecu-image` | `m1-ada-ecu:latest` | `ADA_ECU/` |

Each job runs the same push, and both flags are mandatory:

```
docker login <registry-host> -u <account> --password-stdin   # key supplied from the secret
docker buildx build --platform linux/arm64 --provenance=false --sbom=false \
  -t <registry-host>/<image> --push <context>
```

- `--platform linux/arm64` — a Container Node rejects a multi-platform manifest index and hangs in `Provisioning`.
- `--provenance=false --sbom=false` — attestations turn the result into a manifest index, with the same effect.
- Registry host `registry.hackathon-2.carsky.io`. Use the same host in the login, the tag, and the node's `image` field; a mismatch is the "push succeeded but the node cannot pull" failure ([zot-registry-api-key.md § Registry host caveat](zot-registry-api-key.md#registry-host-caveat-open-item-o1)).
- The key lives only in the `CARSKY_ZOT_API_KEY` repository secret and is never written to the repository ([zot-registry-api-key.md § CI secret](zot-registry-api-key.md#ci-secret-carsky_zot_api_key)).

**Requires both workflow jobs to exist.** Neither is present; add them before expecting a push to produce anything. If a job's context, tag, registry host or platform flag does not match the table above, fix the `.yml` first — there is nothing to verify without it.

**Trigger:** push a commit. Both lanes run on every push, matching the existing image lanes.

The runner is x86_64, so the `linux/arm64` build runs under emulation. The bench image is Alpine plus two packages and finishes in about a minute. The ADA image compiles the C++ core and installs the detector's Python dependencies under that emulation — give its job a long timeout; the existing image jobs use 360 minutes for the same reason.

### 3.3 Confirm the run passed and the images landed

**The run.** GitHub → **Actions** → the newest run → the two jobs above. Green on both means the images were built and pushed. A red push step printing `secret not set` means the credential of [§1.2](#12-cloud-platform-access) is missing.

With `gh` authenticated, the same check without a browser:

```bash
gh run list --limit 5
gh run view <run-id>
```

**The registry**, independent of what the run reported:

```bash
curl -u <registry-account>:<zak-key> https://registry.hackathon-2.carsky.io/v2/_catalog
curl -u <registry-account>:<zak-key> https://registry.hackathon-2.carsky.io/v2/m1-ada-ecu/tags/list
curl -u <registry-account>:<zak-key> https://registry.hackathon-2.carsky.io/v2/m1-ada-bench/tags/list
```

Expected: both repository names in the catalog, and `{"name":"m1-ada-ecu","tags":["latest"]}` / `{"name":"m1-ada-bench","tags":["latest"]}`.

A name missing here means the node will hang in `Provisioning` later. Fix it now — that failure appears late and reads like a network fault.

---

## 4. Create and deploy the Room

### 4.1 Create the blueprint

**Clone `baseline_phase1` and reduce it** ([§1.4](#14-blueprints)). Cloning is the only route that preserves `ethernet` pins, and the isolated Room's addresses are the baseline's addresses — so three of the four pins it needs arrive already drawn and wired.

```bash
export CS=https://hackathon-2.carsky.io
curl -X POST -H "Authorization: Bearer $KEY" $CS/api/v1/blueprints/{baselineId}/clone
```

Expected: a new blueprint carrying every node, pin and edge of the baseline, with the `id` every call below needs. Nydus → **Export Selected** → **Import from File** is *not* an equivalent route here: an import arrives with no pins at all.

Then, on the Nydus canvas, four edits turn the clone into [§2.1](#21-topology)'s topology. The API performs none of them — it has no update route and no delete operation, so all four are Inspector and canvas work:

| # | Edit | What it costs |
|---|---|---|
| 1 | **Rename** the clone, in the blueprint Inspector | The name is the only place the differentiator goes |
| 2 | **Delete the bench Scenario Player node** (`10.99.0.10`) | Its pin and edge go with it; nothing else is touched |
| 3 | **Delete the IVI Skycraft node** (`10.99.0.13`) and **add a Container node** in its place | A node type cannot be changed, so the sink is a new node — and the one pin this procedure has to draw by hand ([§4.2](#42-wire-the-ethernet-pins)) |
| 4 | **Repoint the V2X ECU node** (`10.99.0.11`) at the bench image, with the emitter's env | Config only; its pin and edge survive untouched |

The ADA node at `10.99.0.12` needs no topology edit at all — only the image and env of [§4.3](#43-configure-each-nodes-image).

> **Do not edit the `<name>-deploy` snapshot.** Deploying creates a copy under that name. Edits to it appear to save and are ignored by the next deploy. Always edit the clone itself.

> **Do not edit `baseline_phase1`.** It is the source every other Room is cloned from; a Room-specific edit made in it propagates to every clone taken afterwards.

### 4.2 Wire the ethernet pins

Canvas work, with no scripted alternative: **REST cannot create `ETHERNET` pins, a JSON import silently drops them, and the API has no delete operation.** `POST /api/v1/blueprints/{id}/validate` returns 422 `Node "…" has no pins` until every node has one.

**The clone of [§4.1](#41-create-the-blueprint) supplies most of them already.** Confirm what survived, then draw only what is missing:

| Node | `properties.address` | State after the clone |
|---|---|---|
| Ethernet Bridge | `10.99.0.1` | Present — the single `ETHERNET` / `INPUT` pin every other node's pin targets |
| V2X Bench (mock) | `10.99.0.11` | Present, inherited from the V2X ECU node the clone repurposes |
| ADA ECU | `10.99.0.12` | Present, untouched |
| IVI Sink (mock) | `10.99.0.13` | **Missing — draw it.** The Skycraft node that held this address was deleted, and the Container node replacing it is new |

To draw the missing one: add an `ethernet` pin to the sink node, `direction: OUTPUT`, `properties.address` `10.99.0.13`. The pin shape is the one in [node-ada-ecu.md § Pins](node-ada-ecu.md#pins); only the address differs per node. Then drag from that pin to the bridge's connector — same-type wiring only (`ethernet ↔ ethernet`).

The finished Room has three edges, all terminating at the bridge — a star, not a chain. Should a clone ever arrive with pins missing elsewhere, the same two steps redraw them; there is no scripted repair.

Validate:

```bash
curl -X POST -H "Authorization: Bearer $KEY" $CS/api/v1/blueprints/{id}/validate
```

Expected: a pass. A 422 naming a node means that node still has no pin.

### 4.3 Configure each node's image

Click a node, edit in the Inspector, click empty canvas to commit. Set and confirm each node's `image` against the table below, and its `command`, `capabilities` and `env` against [§2.2](#22-the-blueprint-definition-and-where-it-lives).

| Node | Image | Distinguishing env |
|---|---|---|
| V2X Bench (mock) | `registry.hackathon-2.carsky.io/m1-ada-bench:latest` | `ROLE=v2x_mock` |
| ADA ECU | `registry.hackathon-2.carsky.io/m1-ada-ecu:latest` | — |
| IVI Sink (mock) | `registry.hackathon-2.carsky.io/m1-ada-bench:latest` | `ROLE=ivi_mock` |

All three nodes take `command: ["./entrypoint.sh"]` and `capabilities: ["NET_RAW"]`. **Every value here is typed by hand**, and there is no route that avoids it: the clone arrives carrying the baseline's images and env, and **the API has no update route**, so changing any of it is an Inspector edit. [§4.4](#44-read-the-stored-config-back) is what catches the typos.

Four values decide whether anything works at all, and each fails in a way that looks like something else:

| Value | Node | What a wrong value looks like |
|---|---|---|
| `TARGET_HOST=10.99.0.12` | V2X Bench | The emitter logs `[TX]` normally and the ADA node's log is silent. A single wrong digit is the usual cause |
| `TARGET_PORT=47200` equals `V2X_LISTEN_PORT` | V2X Bench and ADA | Same symptom: traffic on the wire, nothing received |
| `IVI_ECU_HOST=10.99.0.13`, `IVI_ECU_PORT=47300` equals the sink's `LISTEN_PORT` | ADA and sink | The ADA log shows warnings sent and the sink log shows nothing received |
| `ROLE` spelled exactly `v2x_mock` / `ivi_mock` | both bench nodes | The container exits at start; restart count climbs |

`command` is relative to the image workdir `/app`. `./entrypoint.sh` works; `/entrypoint.sh` does not exist and the container dies at start.

### 4.4 Read the stored config back

Read the config back rather than trusting the Inspector's truncated fields. This one call catches every row of the two tables above.

```bash
curl -H "Authorization: Bearer $KEY" $CS/api/v1/blueprints/{id}
```

Expected: four nodes; three `ETHERNET` / `OUTPUT` pins at `10.99.0.11`, `.12`, `.13` plus the bridge's single `INPUT` pin and three edges; and each container node's `config` carrying the image, command, capabilities and env exactly as typed.

### 4.5 Deploy

1. Click empty canvas → the blueprint Inspector → **New Deployment**.
2. Pick an **existing Device** from the dropdown — the Kubernetes resource pool, not an ECU. `+ Create new device` is unnecessary and eats into the two-Room budget.
3. **Deploy**.

Poll until every node reads `Running` with restart count 0:

```bash
curl -H "Authorization: Bearer $KEY" $CS/api/v1/deployments/{roomId}/nodes
```

Each entry carries `{displayName, name, nodeType, phase, message}`. `name` is the **`nodeKey`** every log call below needs — record all three.

The ADA node is the slowest to become useful: the image is the largest, and the detector loads its model before the first detection appears. Give it a minute past `Running` before reading its log.

*Stuck in `Provisioning`* means the image could not be pulled. Re-check [§3.3](#33-confirm-the-run-passed-and-the-images-landed) and the node's `image` field. Diagnosis procedure: [carsky-room-diagnostics](../../.claude/skills/carsky-room-diagnostics/SKILL.md).

---

## 5. Run the checks

All three checks read node logs. The route is the same for every node, and **`container` is mandatory** — omitting it returns 500:

```bash
curl -H "Authorization: Bearer $KEY" \
  "$CS/api/v1/deployments/{roomId}/logs/{nodeKey}?container=user"
```

The browser equivalent is Deployment Viewer → the node → **View Log**.

**Let the Room run for at least 60 seconds after the ADA node reaches `Running`** before reading anything. The emitter waits `START_DELAY_S` (20 s) before its first datagram, and the detector needs a few seconds to load its model and produce its first line.

Save each node's log to a file first, then grep the file. Re-fetching per grep wastes time and gives three checks three different windows:

```bash
curl -sH "Authorization: Bearer $KEY" "$CS/api/v1/deployments/{roomId}/logs/<ada-nodeKey>?container=user"   > ada.log
curl -sH "Authorization: Bearer $KEY" "$CS/api/v1/deployments/{roomId}/logs/<bench-nodeKey>?container=user" > bench.log
curl -sH "Authorization: Bearer $KEY" "$CS/api/v1/deployments/{roomId}/logs/<sink-nodeKey>?container=user"  > sink.log
```

The ADA node writes one `[EVT]`-prefixed JSON line per event. The names quoted in the greps below — `r2_ingest`, `own_sensor_ingest`, `track_transition`, `parse_reject`, `risk_transition`, `r4_tx` — are literal strings in that stream; type them exactly.

### 5.1 Check 1 — the relayed message is received and raises its event

**Claim under test:** the ADA node receives the bench's datagram and raises the corresponding event.

```bash
grep -c '"event":"r2_ingest"'     ada.log
grep -c '"event":"parse_reject"'  ada.log
grep    '"event":"r2_ingest"'     ada.log | head -1
grep    '"event":"track_transition"' ada.log | grep '"source":"v2x_relayed"'
grep -c '\[TX\]' bench.log
```

Expected first `r2_ingest` line, with the received body carried in the payload:

```
[EVT] {"ts":1789000000123,"event":"r2_ingest","payload":{"schemaVersion":1,"type":"v2x_object","stationId":1201,"rxTime":1789000000123,"sender":{...},"object":{"objectId":7,"distance":30.5,...}}}
```

Expected transitions for the relayed track, in this order:

```
[EVT] {"ts":...,"event":"track_transition","payload":{"id":"v2x:1201:7","source":"v2x_relayed","from":"not_tracked","to":"tentative","distance":29.5,"reason":"in_gate"}}
[EVT] {"ts":...,"event":"track_transition","payload":{"id":"v2x:1201:7","source":"v2x_relayed","from":"tentative","to":"tracked","distance":28.0,"reason":"confirm_hits"}}
```

| Pass criterion | Value |
|---|---|
| `r2_ingest` count | ≥ 1, and ≥ 90% of the bench's `[TX]` count over the same window |
| The first `r2_ingest` payload | `type` is `v2x_object`, `stationId` equals `STATION_ID`, `object.objectId` equals `OBJECT_ID` |
| `parse_reject` count | exactly **0** |
| Relayed transitions | one `to":"tentative"` and one later `to":"tracked"`, both with `"source":"v2x_relayed"` and id `v2x:1201:7` |

**Fails when** `r2_ingest` is 0 while `bench.log` shows `[TX]` lines — the datagram is not arriving. Re-check `TARGET_HOST`, `TARGET_PORT` and `V2X_LISTEN_PORT` in [§4.3](#43-configure-each-nodes-image).

**Fails when** `parse_reject` is non-zero — the emitter and the ADA node disagree on the message shape. The emitter is wrong, not the node: fix the bench against [r2-v2x-object.schema.json](../../ADA_ECU/contracts/r2-v2x-object.schema.json).

**Run the negative case too.** Set `PROFILE=out_of_range` on the bench node and redeploy. `r2_ingest` must still count up, and **no** relayed `track_transition` may appear at all — the object stays beyond the drop gate. A track admitted under this profile means the gate is not reading the message's distance.

### 5.2 Check 2 — both vehicles are in the track store

**Claim under test:** the store holds a track for vehicle B, produced by ego's own detector from the clip, and a track for vehicle C, present only through the relayed path.

```bash
grep    '"event":"own_sensor_ingest"' ada.log | head -3
grep    '"event":"track_transition"'  ada.log | grep '"to":"tracked"'
grep -c '"event":"own_sensor_ingest".*"source":"v2x_relayed"' ada.log
grep -c '"event":"own_sensor_ingest".*"id":"v2x:'             ada.log
grep    '"event":"r4_tx"' ada.log | head -1
```

Expected own-sensor ingest, one per detection per sampled frame:

```
[EVT] {"ts":...,"event":"own_sensor_ingest","payload":{"id":"own:1","class":"vehicle","source":"own_sensor","position":{"x":11.0,"y":0.4},"distance":11.0,"speed":0.0,"confidence":0.81,"state":"not_tracked","timestamps":{...}}}
```

Expected `to":"tracked"` transitions — one per source, both present in the same log:

```
[EVT] {"ts":...,"event":"track_transition","payload":{"id":"own:1","source":"own_sensor","from":"tentative","to":"tracked",...}}
[EVT] {"ts":...,"event":"track_transition","payload":{"id":"v2x:1201:7","source":"v2x_relayed","from":"tentative","to":"tracked",...}}
```

The single strongest line is the emitted warning, because it proves both tracks existed **at the same instant** rather than at two different times:

```
[EVT] {"ts":...,"event":"r4_tx","payload":{"schemaVersion":1,"type":"warning","warningType":"nlos_obstruction","riskState":"medium","object":{"id":"v2x:1201:7","source":"v2x_relayed","state":"tracked",...},"geometry":{"ego":{"x":0.0,"y":0.0},"vehicleB":{"x":11.0,"y":0.4},"vehicleC":{"x":41.2,"y":1.7}}}}
```

| Pass criterion | Value |
|---|---|
| Own-sensor tracks | ≥ 1 `own_sensor_ingest` line, and ≥ 1 `track_transition` with `"source":"own_sensor"` and `"to":"tracked"` |
| Relayed tracks | ≥ 1 `track_transition` with `"source":"v2x_relayed"` and `"to":"tracked"` |
| Both at once | ≥ 1 `r4_tx` payload whose `object.source` is `v2x_relayed` **and** whose `geometry.vehicleB` has numeric `x` and `y` |
| Own-sensor entries claiming a relayed source | exactly **0** |
| Own-sensor entries carrying a `v2x:` id | exactly **0** |
| Relayed entries carrying an `own:` id | exactly **0** |

The last three are the zero-C guarantee in text: nothing ego's detector produced can claim to be C, and nothing relayed can claim to have been seen directly.

**Fails when** there are no `own_sensor_ingest` lines. Read the detector's own lines first — a `detector_spawn` event with no ingest after it means the detector started and produced nothing, so check `VIDEO_CLIP_PATH` and `MODEL_PATH` against the paths inside the image. No `detector_spawn` at all means `DETECTOR_ENABLED` is not `true`.

**Fails when** `r4_tx` is absent though both tracks reached `tracked` — the risk level never changed, so no warning was ever edge-triggered. That is a tuning problem, not a defect: go to [§5.5](#55-retune-when-no-warning-is-emitted).

### 5.3 Check 3 — the warning reaches the IVI stand-in carrying both vehicles

**Claim under test:** the ADA node puts a warning datagram on the wire, addressed to the IVI node, carrying both vehicles.

```bash
grep    '\[RX\]'      sink.log | head -3
grep -c '\[CHECK\].*both_vehicles=yes c_source_relayed=yes' sink.log
grep    '\[SUMMARY\]' sink.log | tail -1
grep    '\[CAP\].*10.99.0.13.47300' sink.log | head -3
```

Expected on the sink:

```
[RX] seq=1 from=10.99.0.12:51044 bytes=486 type=warning warningType=nlos_obstruction risk=medium cSource=v2x_relayed cPos=(41.2,1.7) bPos=(11.0,0.4)
[CHECK] seq=1 both_vehicles=yes c_source_relayed=yes
[SUMMARY] received=31 warnings=31 both_vehicles=31 rejected=0
[CAP] IP 10.99.0.12.51044 > 10.99.0.13.47300: UDP, length 486
```

| Pass criterion | Value |
|---|---|
| Warning datagrams received | ≥ 1 `[RX]` line with `type=warning` and `from=10.99.0.12:` |
| Both vehicles carried | ≥ 1 `[CHECK]` line with `both_vehicles=yes` **and** `c_source_relayed=yes` |
| Summary | the last `[SUMMARY]` shows `both_vehicles` ≥ 1 and `rejected=0` |
| On the wire | ≥ 1 `[CAP]` line matching `IP 10.99.0.12.<port> > 10.99.0.13.47300: UDP` |
| Sent equals received | the sink's `received` count equals the ADA log's `r4_tx` count |

`cSource=v2x_relayed` on every warning is the point of the whole Room: the ghost vehicle in the message came from the relayed path and from nowhere else.

**Fails when** the ADA log shows `r4_tx` lines and the sink shows no `[RX]` — the datagram is not arriving. Check `IVI_ECU_HOST`, `IVI_ECU_PORT` and the sink's `LISTEN_PORT`; the `[CAP]` lines tell you whether the packet reached the node's interface at all.

**Fails when** `[CHECK]` reports `both_vehicles=no` — the ADA node emitted a warning with a null `geometry.vehicleC` or a missing `geometry.vehicleB`. A null C is legitimate before C is first tracked, so read the `seq` numbers: `no` on the first datagram and `yes` afterwards is expected; `no` throughout is a defect.

### 5.4 Traffic evidence and Wireshark scope

**In scope here: the `[CAP]` lines in the node logs.** Every container node runs `tcpdump` alongside its program, so each datagram appears as one text line naming source address, destination address, port and byte length. The sink's `[CAP]` line, paired with the `[RX]` line beside it, is this procedure's traffic evidence, and it is what [§5.3](#53-check-3--the-warning-reaches-the-ivi-stand-in-carrying-both-vehicles) accepts; the ADA node's own capture is the same datagram seen at the sending end.

**Out of scope here: producing a `.pcap` file.** This procedure writes no capture file and yields nothing to open in Wireshark. The route that does — a rotating `tcpdump -w` whose closed files leave the node base64-encoded in the log, and the host-side script that decodes them — is described in [traffic-capture-wireshark.md](traffic-capture-wireshark.md). Use it when a `.pcap` is wanted as supporting evidence; it is not a pass criterion of any check above.

If a `.pcap` is produced, the warning payload is plain JSON and reads directly in Wireshark's packet-bytes pane. Filter `udp.port == 47300`.

Every `[CAP]` line depends on the platform actually granting the `NET_RAW` the node config requests. Where it is not granted, `capture.sh` falls back to packet counters and the wire evidence weakens to "bytes moved on this interface" — the `[RX]` line still stands, and the `[CAP]` criterion of [§5.3](#53-check-3--the-warning-reaches-the-ivi-stand-in-carrying-both-vehicles) does not.

### 5.5 Retune when no warning is emitted

A warning is emitted when the assessed risk level **changes**. If both tracks are healthy and no `r4_tx` line ever appears, the level never left its initial value. Retune with node config and a redeploy — no rebuild:

1. Read the observed geometry off the ADA log: `grep '"event":"assessment"' ada.log | tail -5`. Each payload carries the composed ego-to-C range and the time-to-collision estimate.
2. Compare the smallest composed range to `RISK_NEAR_M` (60). The composed range is ego-to-B plus B-to-C, so it is always larger than the distance the bench emits.
3. Pick one lever and change only it:

| Lever | Where | Effect |
|---|---|---|
| `MIN_DISTANCE_M` down | V2X Bench node | The relayed object closes further, shrinking the composed range |
| `RISK_NEAR_M` up | ADA node | The `medium` band starts further out |
| `RISK_CRITICAL_M` up | ADA node | The `high` band starts further out |

4. Redeploy and re-run [§5.2](#52-check-2--both-vehicles-are-in-the-track-store) and [§5.3](#53-check-3--the-warning-reaches-the-ivi-stand-in-carrying-both-vehicles).

**Never retune the admission gate to force a warning.** `GATE_ENTER_M` and `GATE_EXIT_M` decide track identity; the risk thresholds decide alarm level. Moving the gate to change the alarm makes the two indistinguishable and invalidates [§5.1](#51-check-1--the-relayed-message-is-received-and-raises-its-event)'s negative case.

Record every value you changed. A run whose thresholds are unknown proves nothing.

### 5.6 The full-blueprint route

For full-chain work, the 5-node blueprint replaces both bench containers with the real nodes: the bench Scenario Player feeding the real V2X ECU on one side, the Android Skycraft node on the other. **The mechanics are exactly [§4.1](#41-create-the-blueprint) through [§5.5](#55-retune-when-no-warning-is-emitted); only the composition differs**, so nothing above is repeated here.

- **Creation route: clone `baseline_phase1` and rename it — that is the whole reduction.** None of [§4.1](#41-create-the-blueprint)'s four canvas edits applies, and [§4.2](#42-wire-the-ethernet-pins) has nothing to draw: this Room *is* the baseline's composition. Set the ADA node's image and env ([§4.3](#43-configure-each-nodes-image)), read the config back ([§4.4](#44-read-the-stored-config-back)), deploy.
- **Composition and per-node detail:** [carsky-4-node-blueprint.md](carsky-4-node-blueprint.md), whose per-node files carry each neighbour's image, config and pin.
- **Do not import** a hand-authored blueprint JSON in place of it: an import arrives without its `ethernet` pins, and typically without the Skycraft `image` block, which gets the deploy rejected outright.
- **The IVI APK is installed after the deploy, not carried by the blueprint** — over ADB into the running guest, by [deploy-ivi-hmi-walkthrough.md § How the APK reaches the IVI ECU node](deploy-ivi-hmi-walkthrough.md#41-how-the-apk-reaches-the-ivi-ecu-node).
- **The ADA node is not reconfigured.** Its image, `command`, `capabilities`, env, address and port are the values of [§2.2](#22-the-blueprint-definition-and-where-it-lives) unchanged — that is why the isolated blueprint uses the full blueprint's addresses. Switching between the two touches the neighbours only.
- **Checks 1 and 2 are unchanged**, and read the same ADA log lines. What changes is where the relayed traffic originates: the real V2X ECU, driven by the bench scenario, in place of the emitter — so `STATION_ID`, `OBJECT_ID` and the distance profile come from that scenario rather than from node env, and [§5.5](#55-retune-when-no-warning-is-emitted)'s `MIN_DISTANCE_M` lever is not available.
- **Check 3 has no sink log.** The Android node runs no container and produces none of the `[RX]`, `[CHECK]`, `[SUMMARY]` or `[CAP]` lines. Consumer-side evidence moves to the guest's own log, and that route belongs to [deploy-ivi-hmi-walkthrough.md § Verify the HMI and the logging](deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging).
- **The ADA node's own capture becomes the only wire evidence** for the outgoing warning, because there is no sink node to capture on. Its `[CAP]` lines carry the whole traffic claim on this blueprint.
- **Room budget:** the full blueprint is one deployment like any other, and only two run at once.

### 5.7 Tear down

**Delete Deployment** when the logs are saved. The blueprint is untouched and redeployable. Only two Rooms may run at once, so releasing one matters.

Save all three log files before deleting — the log route returns nothing once the Room is gone.

---

## 6. Troubleshooting

| Symptom | Meaning | Action |
|---|---|---|
| A push produces no image job in Actions | The workflow jobs do not exist on that branch | [§3.2](#32-build-and-push-the-images-on-ci) |
| Image job red at the push step, printing `secret not set` | The registry credential is not configured | [§1.2](#12-cloud-platform-access) |
| Image job red after 360 minutes | The emulated `linux/arm64` build hit the timeout cap | Almost always a Python dependency building from source; re-run, then raise the cap |
| A container node stuck in `Provisioning` | The image could not be pulled | [§3.3](#33-confirm-the-run-passed-and-the-images-landed), then the node's `image` field. [carsky-room-diagnostics](../../.claude/skills/carsky-room-diagnostics/SKILL.md) |
| `validate` returns 422 `Node "…" has no pins` | The pins are not drawn yet | [§4.2](#42-wire-the-ethernet-pins) |
| Log call returns 500 listing two container names | `container` was omitted from the query | Add `?container=user` |
| A node's restart count climbs, no `[BOOT]` line | The image ships no `entrypoint.sh`, or `command` is an absolute path | The image must carry it ([§1.3](#13-deliverable-prerequisites)); use `./entrypoint.sh`, not `/entrypoint.sh` |
| A bench node's restart count climbs after `[BOOT]` | `ROLE` is misspelled | `v2x_mock` / `ivi_mock` exactly |
| Bench logs `[TX]`, ADA log is empty | Wrong target address or port | One digit in `TARGET_HOST` is the usual cause |
| ADA log shows `parse_reject` on every datagram | The emitter's message shape has drifted from the contract | Fix the emitter against [r2-v2x-object.schema.json](../../ADA_ECU/contracts/r2-v2x-object.schema.json) |
| `detector_spawn` present, no `own_sensor_ingest` | The detector started and produced nothing | Check `VIDEO_CLIP_PATH` and `MODEL_PATH` resolve inside the image |
| No `detector_spawn` at all | The detector was never started | `DETECTOR_ENABLED` must be `true` |
| Own-sensor tracks appear, then stop and never return | The clip ended and did not replay | `DETECTOR_LOOP=true` |
| Both tracks `tracked`, no `r4_tx` | The risk level never changed | [§5.5](#55-retune-when-no-warning-is-emitted) |
| `r4_tx` present, sink silent | The datagram is not reaching the sink | Check `IVI_ECU_HOST` / `IVI_ECU_PORT` against the sink's `LISTEN_PORT`; read the sink's `[CAP]` lines |
| Sink reports `rejected` climbing | Datagrams that are not valid JSON are arriving on `47300` | Another sender is on that port, or the ADA node is emitting malformed output — read one rejected payload |
| `[CAP] no NET_RAW` in place of capture lines | The node's config is missing `capabilities`, or the platform did not grant it | Confirm `"capabilities": ["NET_RAW"]` in the read-back of [§4.4](#44-read-the-stored-config-back); if it is there, the platform refused it — item 12 of [§8.1](#81-confirm-before-relying-on-these) |
| Edits to the blueprint have no effect on the next deploy | The `<name>-deploy` snapshot was edited | Edit the original blueprint |

---

## 7. Work division between AI and human

The split follows from what an agent can reach. An agent writes code, runs CLI tools and makes authenticated REST calls; it cannot use the Nydus canvas, a browser download, or its own eyes.

| Action | AI / Human | Description |
|---|---|---|
| [Write the ADA ECU deliverables](#13-deliverable-prerequisites) | Neither | Product code the procedure consumes; it exists before the procedure starts |
| [Write the bench scripts](#31-write-the-bench-scripts) | AI | The emitter, the sink, the capture script, the entrypoint and the Dockerfile under `tools/ada-bench/` |
| [Add the two image jobs](#32-build-and-push-the-images-on-ci) | AI | The workflow jobs that build and push both images |
| [Push, and let the jobs build and push the images](#32-build-and-push-the-images-on-ci) | AI | A commit push is the whole trigger; no local Docker anywhere |
| [Confirm the run passed](#33-confirm-the-run-passed-and-the-images-landed) | Human | Actions web UI; an agent session holds no GitHub token |
| [Confirm both images reached the registry](#33-confirm-the-run-passed-and-the-images-landed) | AI | Registry catalog and tag lists over `curl` |
| [Clone `baseline_phase1`](#41-create-the-blueprint) | AI | `POST /api/v1/blueprints/{id}/clone`; the one scripted step of the creation route |
| [Rename it, delete two nodes, add the sink node](#41-create-the-blueprint) | Human | Nydus canvas; the API has no delete operation and cannot change a node's type |
| [Draw and wire the sink's ethernet pin](#42-wire-the-ethernet-pins) | Human | Nydus canvas; REST cannot create `ETHERNET` pins or the edges joining them |
| [Configure each node's image](#43-configure-each-nodes-image) | Human | Node Inspector; the API has no update route, so every correction is a UI edit |
| [Read the stored config back](#44-read-the-stored-config-back) | AI | `GET /api/v1/blueprints/{id}` returns every pin, address, image and env as stored |
| [Deploy the blueprint](#45-deploy) | Human | **New Deployment** dialog; picking the Device is the user's call and consumes a Room slot |
| [Poll node phases until Running](#45-deploy) | AI | `GET /api/v1/deployments/{roomId}/nodes`; also yields each `nodeKey` |
| [Save the three node logs](#5-run-the-checks) | AI | Logs route with `container=user` |
| [Check 1 — reception and its event](#51-check-1--the-relayed-message-is-received-and-raises-its-event) | AI | Text in the ADA log |
| [Check 2 — both vehicles in the store](#52-check-2--both-vehicles-are-in-the-track-store) | AI | Text in the ADA log |
| [Check 3 — the warning on the wire](#53-check-3--the-warning-reaches-the-ivi-stand-in-carrying-both-vehicles) | AI | Text in the sink log, including its `[CAP]` lines |
| [Run the out-of-range negative case](#51-check-1--the-relayed-message-is-received-and-raises-its-event) | Human | A bench node config edit, then a fresh deployment |
| [Retune the profile or the risk thresholds](#55-retune-when-no-warning-is-emitted) | Human | Node Inspector edits, then a fresh deployment |
| [Export a `.pcap` for Wireshark](#54-traffic-evidence-and-wireshark-scope) | Human | Browser log download, then the extraction script; optional, not a pass criterion |
| [Switch to the full blueprint](#56-the-full-blueprint-route) | Human | Canvas composition and a fresh deploy |
| [Tear the Room down](#57-tear-down) | Human | **Delete Deployment**; releases one of the two Room slots |

Five notes on the rows above:

- **The first row belongs to neither column.** No agent writes the node's product code here and no human writes it at bring-up time — those sources are consumed, not authored, by this procedure. The bench scripts of the second row are different: they are this procedure's own test equipment, and writing them is a step of it.
- **No image is built by hand, by anyone.** The build rows are AI because the work is a commit and a push, not a `docker build`.
- **Confirming the run flips to AI** on a machine with an authenticated `gh` CLI. Without it, the Actions web UI is the only route.
- **Every node-config and topology edit is human.** The API clones a blueprint and adds nodes; it has no update and no delete operation, and it cannot create a pin — so reducing the clone, and every config value on it, is Inspector and canvas work, read back over REST.
- **Every AI row needs a credential supplied at run time** — the CarSky API key for the REST rows, the registry account and key for the registry check. An agent stores neither.

---

## 8. Expected outputs and acceptance

Three outputs, all text, read from two log surfaces. None substitutes for another: the first two prove what happened inside the node, the third proves it left the node.

| # | Proof | Where it appears | Accepted when |
|---|---|---|---|
| 1 | **The relayed message is received and raises its event** | ADA node log | `r2_ingest` count ≥ 1 and ≥ 90% of the bench's `[TX]` count · the first payload's `stationId` and `object.objectId` match the bench's configured values · `parse_reject` count is 0 · one `track_transition` to `tentative` and a later one to `tracked`, both `"source":"v2x_relayed"` |
| 2 | **Both vehicles are in the track store** | ADA node log | ≥ 1 `track_transition` to `tracked` with `"source":"own_sensor"` · ≥ 1 with `"source":"v2x_relayed"` · ≥ 1 `r4_tx` payload with `object.source` = `v2x_relayed` and numeric `geometry.vehicleB` · **zero** own-sensor entries claiming a relayed source or a `v2x:` id, and **zero** relayed entries claiming an `own:` id |
| 3 | **The warning reaches the IVI stand-in carrying both vehicles** | IVI sink log | ≥ 1 `[RX]` with `type=warning` from `10.99.0.12` · ≥ 1 `[CHECK] both_vehicles=yes c_source_relayed=yes` · last `[SUMMARY]` with `rejected=0` · ≥ 1 `[CAP] IP 10.99.0.12.<port> > 10.99.0.13.47300: UDP` · the sink's `received` count equals the ADA log's `r4_tx` count |

Two further observations complete the set rather than repeating the three above:

| Observation | What it settles |
|---|---|
| With `PROFILE=out_of_range`, `r2_ingest` still counts up and **no** relayed `track_transition` appears | The admission gate rejects on distance, so admission in the main run was earned rather than automatic |
| Every warning in the run carries `cSource=v2x_relayed` | The ghost vehicle came from the relayed path and from nowhere else — nothing ego saw directly was ever labelled as it |

Record the values of `PROFILE`, `START_DISTANCE_M`, `MIN_DISTANCE_M`, `CLOSING_RATE_MPS`, `GATE_ENTER_M`, `GATE_EXIT_M`, `RISK_NEAR_M` and `RISK_CRITICAL_M` alongside the logs. A pass at unknown thresholds proves nothing.

On the full blueprint, outputs 1 and 2 are accepted identically and output 3 is replaced by the guest-side evidence of [§5.6](#56-the-full-blueprint-route).

### 8.1 Confirm before relying on these

Every point below can make a step fail without this guide being wrong. Confirm each at the step named before scheduling work that depends on it. Numbers are stable: a point that stops needing confirmation stays in place, marked settled, rather than renumbering the rest.

| # | Point | Where it bites |
|---|---|---|
| 1 | **The whole route is unexercised.** No Room has ever carried this topology, and no step below has been run end to end | The entire document |
| 2 | Every deliverable in [§1.3](#13-deliverable-prerequisites) other than the clip — the ADA `Dockerfile`, the core binary, the detector, the model file, the entrypoint and capture scripts. None of them has been written | [§1.3](#13-deliverable-prerequisites), [§3.2](#32-build-and-push-the-images-on-ci) |
| 3 | The two CI jobs `ada-bench-image` and `ada-ecu-image`. Neither exists, so a push currently builds and publishes nothing | [§3.2](#32-build-and-push-the-images-on-ci) |
| 4 | The `[EVT]` event names and payload fields the checks grep for. They are what the node's design specifies, not what a run has produced | [§5.1](#51-check-1--the-relayed-message-is-received-and-raises-its-event), [§5.2](#52-check-2--both-vehicles-are-in-the-track-store) |
| 5 | The bench image, its two roles, its env names and its log-line shapes — specified in [§2.3](#23-the-bench-image--one-image-two-roles), not yet written | [§3.1](#31-write-the-bench-scripts), [§5.3](#53-check-3--the-warning-reaches-the-ivi-stand-in-carrying-both-vehicles) |
| 6 | `linux/arm64` wheel availability for the detector's Python dependencies. A missing wheel means a source build under the runner's emulation, which can exhaust the job timeout or fail outright | [§3.2](#32-build-and-push-the-images-on-ci) |
| 7 | **Settled — nothing to confirm.** The ADA node's `command` is `["./entrypoint.sh"]` and its `capabilities` are `["NET_RAW"]`, unconditionally, and [node-ada-ecu.md § Blueprint node config](node-ada-ecu.md#blueprint-node-config) states both. Whether the platform honours the capability at run time is item 12 | — |
| 8 | The blueprint file at `requirements/car-sky-guide/blueprint-ada-isolated.json`, which has not been created | [§2.2](#22-the-blueprint-definition-and-where-it-lives) |
| 9 | **The clone-and-reduce route as a whole.** That `/clone` returns a blueprint with every pin and edge intact; that deleting a node on the canvas leaves the survivors' pins alone; and that a hand-drawn pin on a newly added Container node wires to the bridge and validates | [§4.1](#41-create-the-blueprint), [§4.2](#42-wire-the-ethernet-pins) — a clone that loses its pins turns three inherited edges into three hand-drawn ones |
| 10 | That the relayed profile defaults produce a risk-level change against the clip's actual ego-to-B range. They may not, in which case no warning is emitted and [§5.5](#55-retune-when-no-warning-is-emitted) is on the critical path rather than a fallback | [§5.2](#52-check-2--both-vehicles-are-in-the-track-store), [§5.5](#55-retune-when-no-warning-is-emitted) |
| 11 | The detector's frame rate on the Room's CPU allocation. If it falls far below the configured stride, ego's own track may expire between updates and the composed geometry disappears | [§5.2](#52-check-2--both-vehicles-are-in-the-track-store) |
| 12 | **That the platform grants `NET_RAW` at run time.** Every node requests it unconditionally, but no deployment has yet shown a `[CAP]` line proving the request was honoured. Where it is refused, the `[CAP]` criterion of [§5.3](#53-check-3--the-warning-reaches-the-ivi-stand-in-carrying-both-vehicles) cannot be met by any route in this document | [§5.4](#54-traffic-evidence-and-wireshark-scope) |
| 13 | A free Room slot. Two deployments run at once across the whole account, and this Room holds one for its full duration | [§4.5](#45-deploy) |

---

## 9. Quick reference

| Thing | Value |
|---|---|
| Registry host | `registry.hackathon-2.carsky.io` |
| ADA image | `registry.hackathon-2.carsky.io/m1-ada-ecu:latest`, single-platform `linux/arm64`, built from `ADA_ECU/` by job `ada-ecu-image` |
| Bench image | `registry.hackathon-2.carsky.io/m1-ada-bench:latest`, single-platform `linux/arm64`, built from `tools/ada-bench/` by job `ada-bench-image` |
| CI secret | `CARSKY_ZOT_API_KEY` (`zak_…`) |
| Clone source | `baseline_phase1` ([carsky-4-node-blueprint.md § The blueprints on CarSky](carsky-4-node-blueprint.md#8-the-blueprints-on-carsky)) |
| Blueprint definition | `requirements/car-sky-guide/blueprint-ada-isolated.json` — config specification, never imported |
| Node addresses | V2X bench `.11` · ADA `.12` · IVI sink `.13`, on `10.99.0.0/24` with the bridge at `.1` |
| Ports | bench → ADA `47200` · ADA → sink `47300` |
| Every container node | `command: ["./entrypoint.sh"]` · `capabilities: ["NET_RAW"]` |
| Role values | `ROLE=v2x_mock` on `.11` · `ROLE=ivi_mock` on `.13` |
| Log route | `GET /api/v1/deployments/{roomId}/logs/{nodeKey}?container=user` — `container` is mandatory |
| Event names greped | `r2_ingest` · `own_sensor_ingest` · `track_transition` · `parse_reject` · `assessment` · `risk_transition` · `r4_tx` |
| Sink log lines | `[RX]` · `[CHECK]` · `[SUMMARY]` · `[CAP]` |
