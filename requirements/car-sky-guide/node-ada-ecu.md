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

## Build & push the ARM64 image

```
docker buildx build --platform linux/arm64 --provenance=false --sbom=false \
  -t registry.hackathon-2.carsky.io/m1-ada-ecu:<commit> --load ADA_ECU
docker login registry.hackathon-2.carsky.io -u <your_carsky_username>
docker push registry.hackathon-2.carsky.io/m1-ada-ecu:<commit>
```

The provided video clip(s) ship inside the image (`COPY` at build time) — no live video pin is used in M1 (per report §1, live camera bring-up is out of scope).

## Blueprint node config

```json
{
  "container": {
    "image": "registry.carsky.io/m1-ada-ecu:latest",
    "command": ["--config", "/app/config/ada-ecu.conf"],
    "env": {
      "V2X_LISTEN_PORT": "47200",
      "IVI_HOST": "10.99.0.13",
      "IVI_PORT": "47300",
      "DETECTOR_ENABLED": "true",
      "DETECTOR_CMD": "python3 /app/detector/tools/video_detector.py --video /app/media/ego-b-occluding-c.mp4 --backend yolo-onnx --model /app/models/yolo11n.onnx --every-n-frames 4 --confidence 0.20",
      "CRA_ENABLED": "nlos_obstruction",
      "RISK_NEAR_M": "30",
      "RISK_CRITICAL_M": "15",
      "RISK_DWELL_MS": "300",
      "ENABLE_PCAP": "true",
      "GATE_ENTER_M": "30",
      "GATE_EXIT_M": "35"
    }
  }
}
```

`V2X_LISTEN_PORT` receives R2 JSON from the V2X ECU; `IVI_HOST`/`IVI_PORT` target the IVI ECU's static address for R4 emission. `GATE_ENTER_M`/`GATE_EXIT_M` are the R13 admission-gate constants — externalized configuration, never literals in code (CLAUDE.md governing principle 5; [milestone1.md](../../plans/milestone1.md) §4).

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

- Detection log over the provided clip with per-frame objects and distance estimates; **zero detections labeled C**.
- R13 state transitions observable in logs, matching the admission state-machine diagram.
- The NLOS plugin registers through the CRA interface (R14).
- At least one R4 warning event per scenario run, carrying risk state and composed geometry, observed at the IVI ECU (R15).
- JSONL event logs reconstruct a full run offline (R18).

After one scenario, save the ADA event log and the rotating capture directory from the container.
Validate the event chain with `python3 ADA_ECU/tools/check_evt_log.py <event-log>`. To transport a
single capture through text-only CarSky logs, run `/app/capture.sh --export-one <capture.pcap>` in
the node, save the emitted `[CAP]` line locally, then run
`ADA_ECU/tools/extract_pcap.sh <capture-line.txt> <output.pcap>`. The script checks the SHA-256
before accepting the file. In Wireshark, filter `udp.port == 47300` and inspect the R4 payload for
tracked `own:B` and `v2x:1201:7` objects.
