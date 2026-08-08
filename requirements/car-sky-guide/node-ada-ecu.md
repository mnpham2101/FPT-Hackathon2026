# Node: ADA ECU

- **CarSky node type:** Container Node
- **Virtualization level:** App-level (Linux container) — developed fully (report §1 node table)
- **Focus goal:** structured, low-latency architecture — not object-detection performance
- **Part of:** [carsky-4-node-blueprint.md](carsky-4-node-blueprint.md)

## Responsibility

- Detects objects (B, the visible occluder) from provided saved video files — no live camera (R12).
- Maintains the R3 track store with R13 admission state machine (own-sensor + `v2x_relayed`).
- Runs the R14 Collision Risk Assessment abstraction with the M1 NLOS plugin.
- Emits R4 warning events (and optionally periodic state) to the IVI ECU (R15).
- Does not receive GNSS or other sensor data from the Cortex-M ECU (that ECU is not built in M1).

## Tech stack

C++17 core (store, CRA, emission, logging) + Python 3.11 detector subprocess; YOLO11n (AGPL-3.0) on ONNX Runtime CPU; OpenCV video decode; the two processes join only through argv + exit codes + R3 JSONL over stdout (report §3(d)/(g)).

## Build & push the image

```
docker login registry.hackathon-2.carsky.io -u <your_carsky_username>
docker tag m1-ada-ecu:latest registry.hackathon-2.carsky.io/m1-ada-ecu:latest
docker push registry.hackathon-2.carsky.io/m1-ada-ecu:latest
```

- **Registry host is `registry.hackathon-2.carsky.io`** — the host that actually serves Zot; `registry.carsky.io` does not ([zot-registry-api-key.md § Registry host caveat](zot-registry-api-key.md#registry-host-caveat-open-item-o1)). The same host must appear in the login, the tag and the `image` field below; a mismatch is the "push succeeded, node cannot pull" failure.
- **Single-platform `linux/arm64`, attestations off** — a Container Node rejects a multi-platform manifest index and hangs in `Provisioning` ([ADA decision D9](../../documents/Design/ADA-ECU/ada-ecu-design-decisions.md#d9--deployment-shape)).
- The image is built and pushed by CI, not by hand; that procedure is [deploy-ada-ecu-walkthrough.md §3](deploy-ada-ecu-walkthrough.md#3-build-the-images-on-ci).

The provided video clip(s) ship inside the image (`COPY` at build time) — no live video pin is used in M1 (per report §1, live camera bring-up is out of scope).

## Blueprint node config

```json
{
  "container": {
    "image": "registry.hackathon-2.carsky.io/m1-ada-ecu:latest",
    "command": ["./entrypoint.sh"],
    "capabilities": ["NET_RAW"],
    "env": {
      "V2X_LISTEN_PORT": "47200",
      "IVI_ECU_HOST": "10.99.0.13",
      "IVI_ECU_PORT": "47300",
      "GATE_ENTER_M": "30",
      "GATE_EXIT_M": "35"
    }
  }
}
```

`command` is `./entrypoint.sh`, never the bare binary — the same shape the [V2X ECU](node-v2x-ecu.md#blueprint-node-config) already ships, and product code in the node image rather than a netcheck or test artifact:

- It launches `capture.sh` in the background (the ADA→IVI traffic evidence R15/R19 needs, which the V2X ECU's capture point cannot see) and then `exec`s the node binary.
- The `exec` is load-bearing: the app becomes PID 1 and receives the platform's SIGTERM directly, which is what makes the clean-shutdown path run and keeps the app's exit code as the container's. Behind a non-exec parent shell the signal lands on the shell and the app is killed on the grace-period timeout instead.
- The path is relative to the image workdir `/app`; `/entrypoint.sh` does not exist and the container dies at start.

`capabilities: ["NET_RAW"]` (flat in `config`, verified shape per [blueprint-KIS.json](../development-platform-doc/blueprint-KIS.json)) is **always part of this node's config, never conditional**: without it `tcpdump` cannot open a capture handle, `capture.sh` degrades to a `/proc/net/dev` packet counter, and the node's traffic evidence weakens to "bytes moved on this interface". Capture procedure and pcap retrieval: [traffic-capture-wireshark.md](traffic-capture-wireshark.md).

**The image must ship both scripts.** `command: ["./entrypoint.sh"]` requires `entrypoint.sh` and `capture.sh` at the image workdir, and a node whose image lacks them dies at start. Their target paths are designated in [ADA HLD §4](../../documents/Design/ADA-ECU/ada-ecu-hld.md#4-folder-structure); the pattern to follow is [V2X_ECU/entrypoint.sh](../../V2X_ECU/entrypoint.sh) and [V2X_ECU/capture.sh](../../V2X_ECU/capture.sh).

`V2X_LISTEN_PORT` receives R2 JSON from the V2X ECU; `IVI_ECU_HOST`/`IVI_ECU_PORT` target the IVI ECU's static address for R4 emission. `GATE_ENTER_M`/`GATE_EXIT_M` are the R13 admission-gate constants — externalized configuration, never literals in code (CLAUDE.md governing principle 5; [milestone1.md](../../plans/milestone1.md) §4). The full env surface — detector, risk, capture and evidence tunables with their defaults — is [ADA HLD §6](../../documents/Design/ADA-ECU/ada-ecu-hld.md#6-internal-components); anything the blueprint omits falls through to the app's own default, never to an `ENV` baked into the image.

## Pins

Container Node available pin kinds: `can`, `lin`, `kuksa`, `gpio`, `ethernet`, `video`, `usb`, `tunnel` ([full table](carsky-4-node-blueprint.md#pin-kinds-by-node-type)).

**M1 wires exactly one — `ethernet`, `OUTPUT`, targeting the Ethernet Bridge node.** Added manually in the Nydus UI canvas after import — [blueprint-m1-cooperative-awareness.json](blueprint-m1-cooperative-awareness.json) creates this node via Import from File but carries no pins (JSON import silently drops `ethernet` pins, same limitation as the REST `addPin` endpoint — see [carsky-rest-api-blueprint.md](carsky-rest-api-blueprint.md)). Verified pin shape, per the real platform export [blueprint-KIS.json](../development-platform-doc/blueprint-KIS.json) — `id` is platform-assigned when the pin is created, and there is no `prefixLen` field; only `address` (the eth-bridge node's own `subnet` config supplies the mask):

```json
{
  "id": "<platform-assigned>",
  "name": "eth",
  "pinType": "ETHERNET",
  "direction": "OUTPUT",
  "side": null,
  "port": null,
  "properties": {
    "address": "10.99.0.12"
  }
}
```

This pin's edge targets the Ethernet Bridge node's single `ETHERNET`/`INPUT` pin (star topology).

**Why nothing else:**
- No `kuksa` — "the ADA ECU does not receive GNSS or other sensor data from the Cortex-M ECU" (report §1); KUKSA/VSS is exactly that vehicle-signal path, so it's intentionally absent in M1.
- No `video` — the provided clip(s) are baked into the image at build time (see § Build & push above), not streamed through a live video pin; no live camera bring-up is a frozen M1 scope boundary.
- No `can`/`lin`/`gpio`/`usb`/`tunnel` — nothing in R12–R15 needs them.

## Verification (feeds R12–R15, R18 acceptance)

What this node must be observed doing. **How** it is deployed and checked — the isolated Room, its bench stand-ins, the log route and the pass criteria — is [deploy-ada-ecu-walkthrough.md](deploy-ada-ecu-walkthrough.md); that procedure is not repeated here.

- The observable surface is the node's own stdout: one `[EVT]`-prefixed JSON line per event (`r2_ingest`, `own_sensor_ingest`, `track_transition`, `parse_reject`, `assessment`, `risk_transition`, `r4_tx`), plus the `[CAP]` lines `capture.sh` emits.
- Detection log over the provided clip with per-frame objects and distance estimates; **zero detections labeled C**.
- R13 state transitions observable in logs, matching the admission state-machine diagram.
- The NLOS plugin registers through the CRA interface (R14).
- At least one R4 warning event per scenario run, carrying risk state and composed geometry, observed at the IVI ECU (R15).
- JSONL event logs reconstruct a full run offline (R18).
