# Node: Bench — Scenario Player

- **CarSky node type:** Container Node
- **Virtualization level:** Container — team-built, developed fully (report §1 node table)
- **Focus goal:** V2X message generation across different scenarios
- **Part of:** [carsky-4-node-blueprint.md](carsky-4-node-blueprint.md)

## Responsibility

- Emulates the Quectel modem's connection point toward the V2X ECU.
- Generates V2X messages (R1 CPM profile) informing the V2X ECU of vehicle C — distance, relative position, speed, classification, and sender B's pose — across configurable scenarios and message rates (R11).
- Conceptually test equipment outside the car, deployed in the same blueprint so it shares the Room network — not a mock to eliminate (CLAUDE.md governing principle 2).

## Tech stack

Python; the shared R1 codec (Vanetza-based encoder) — report §3(c).

## Build & push the image

```
docker login registry.carsky.io -u <your_carsky_username>
docker tag scenario-player:latest registry.carsky.io/m1-scenario-player:latest
docker push registry.carsky.io/m1-scenario-player:latest
```

## Blueprint node config

```json
{
  "container": {
    "image": "registry.carsky.io/m1-scenario-player:latest",
    "command": ["python", "main.py"],
    "env": {
      "SCENARIO_CONFIG": "/app/scenarios/default.yaml",
      "V2X_ECU_HOST": "10.99.0.11",
      "V2X_ECU_PORT": "47100"
    }
  }
}
```

`V2X_ECU_HOST`/`V2X_ECU_PORT` are read from env, never hardcoded in source (CLAUDE.md governing principle 5) — they target the V2X ECU node's static address from the blueprint topology.

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
    "address": "10.99.0.10"
  }
}
```

This pin's edge targets the Ethernet Bridge node's single `ETHERNET`/`INPUT` pin (every role node's `OUTPUT` pin fans into that one bridge pin — a star topology, not pairwise pins).

**Why nothing else:**
- No `kuksa`/`can`/`lin`/`gpio` — the bench emulates a V2X modem endpoint broadcasting CPMs (R11); it isn't simulating a vehicle bus or vehicle-signal source.
- No `video`/`usb`/`tunnel` — nothing in R11 needs them.

## Verification (feeds R11 acceptance)

- Generated messages are received and decoded by the V2X ECU on connected nodes.
- Different scenario configurations (e.g. C approaching vs C out of range) produce observably different message streams — swap `SCENARIO_CONFIG` and confirm the V2X ECU's Rx log changes accordingly.
