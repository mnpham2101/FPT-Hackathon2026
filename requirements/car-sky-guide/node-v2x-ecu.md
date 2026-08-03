# Node: V2X ECU

- **CarSky node type:** Container Node
- **Virtualization level:** App-level (Linux container) — developed in part, only certain layers (report §1 node table)
- **Focus goal:** hardware portability (R7)
- **Part of:** [carsky-4-node-blueprint.md](carsky-4-node-blueprint.md)

## Responsibility

- Configures the modem and interfaces with it — simulated only.
- Connects to the V2X network — simulated (UDP toward the bench, below the R7 adapter seam).
- Rx: receives CPM payloads, decodes/validates/dedupes, forwards R2 JSON to the ADA ECU informing it of an obstruction outside ego's line of sight (R9). **This is the whole M1 V2X data path — receive-only.**
- ~~Tx: receives ADA store data, constructs CPMs, broadcasts them over the V2X network (R10).~~ — **R10 moved to the future plan** (2026-07-30); not implemented in M1.

## Tech stack

C++17; Vanetza ITS2 ASN.1 codec (LGPLv3, dynamic linking) behind the R7 adapter seam; nlohmann/json for R2 — report §3(a)/(d).

## Build & push the image

```
docker login registry.hackathon-2.carsky.io -u <your_carsky_username>
docker tag v2x-ecu:latest registry.hackathon-2.carsky.io/m1-v2x-ecu:latest
docker push registry.hackathon-2.carsky.io/m1-v2x-ecu:latest
```

- **Registry host is `registry.hackathon-2.carsky.io`** — the host that actually serves Zot; `registry.carsky.io` does not ([zot-registry-api-key.md § Registry host caveat](zot-registry-api-key.md#registry-host-caveat-open-item-o1)). The same host must appear in the login, the tag and the `image` field below; a mismatch is the "push succeeded, node cannot pull" failure.

## Blueprint node config

```json
{
  "container": {
    "image": "registry.hackathon-2.carsky.io/m1-v2x-ecu:latest",
    "command": ["./entrypoint.sh"],
    "capabilities": ["NET_RAW"],
    "env": {
      "LISTEN_PORT": "47100",
      "ADA_ECU_HOST": "10.99.0.12",
      "ADA_ECU_PORT": "47200",
      "FAULT_PLAN": "none"
    }
  }
}
```

`command` is the entrypoint that starts the tcpdump capture sidecar in the background and the ECU app in the foreground; `capabilities: ["NET_RAW"]` (flat in `config`, verified shape per [blueprint-KIS.json](../development-platform-doc/blueprint-KIS.json)) lets tcpdump open raw sockets — capture procedure and retrieval: [traffic-capture-wireshark.md](traffic-capture-wireshark.md).

`LISTEN_PORT` is where the R7 modem-stub UDP channel accepts bench-originated CPMs; `ADA_ECU_HOST`/`ADA_ECU_PORT` target the ADA ECU's static address for R2 forwarding; `FAULT_PLAN` selects the R8 fault-injection scenario (`none · init_fail · configure_reject · subscription_drop`). Optional overrides (defaults + meanings in the [HLD §6](../../V2X_ECU/doc/v2x-ecu-hld.md#6-internal-components)): `INIT_RETRY_MAX`, `RETRY_BACKOFF_MS`, `DEDUPE_WINDOW_MS`, `EVENT_LOG_PATH`, `CAPTURE_FILTER`, `PCAP_DIR`, `CAPTURE_ROTATE_S`. All read from env, never hardcoded (CLAUDE.md governing principle 5).

## Pins

Container Node available pin kinds: `can`, `lin`, `kuksa`, `gpio`, `ethernet`, `video`, `usb`, `tunnel` ([full table](carsky-4-node-blueprint.md#pin-kinds-by-node-type)).

**M1 wires exactly one — `ethernet`, `OUTPUT`, targeting the Ethernet Bridge node** — carries both the bench-facing (R1) and ADA-facing (R2) UDP traffic on separate ports over the same NIC. Added manually in the Nydus UI canvas after import — [blueprint-m1-cooperative-awareness.json](blueprint-m1-cooperative-awareness.json) creates this node via Import from File but carries no pins (JSON import silently drops `ethernet` pins, same limitation as the REST `addPin` endpoint — see [carsky-rest-api-blueprint.md](carsky-rest-api-blueprint.md)). Verified pin shape, per the real platform export [blueprint-KIS.json](../development-platform-doc/blueprint-KIS.json) — `id` is platform-assigned when the pin is created, and there is no `prefixLen` field; only `address` (the eth-bridge node's own `subnet` config supplies the mask):

```json
{
  "id": "<platform-assigned>",
  "name": "eth",
  "pinType": "ETHERNET",
  "direction": "OUTPUT",
  "side": null,
  "port": null,
  "properties": {
    "address": "10.99.0.11"
  }
}
```

This pin's edge targets the Ethernet Bridge node's single `ETHERNET`/`INPUT` pin (star topology).

**Why nothing else:**
- No `kuksa`/`can`/`lin` — the V2X protocol stack ships in the modem and stays out of scope for the whole project (report § Input constraints); this node exchanges already-decoded CPM objects over ethernet/UDP, not vehicle-bus or VSS signals.
- No `gpio`/`video`/`usb`/`tunnel` — nothing in R7–R9 needs them.

## Verification (feeds R7–R9 acceptance)

- CI import check: no direct transport imports above the R7 adapter seam.
- Golden-vector CPMs decode correctly; malformed-input corpus fully rejected, zero crashes (R9).
- R2 messages observed at the ADA ECU carrying decoded bench-scenario values, not constants.
- ~~Broadcast frames captured on the bridge with fields populated from live store data (R10).~~ — **R10 moved to the future plan.**
- Wireshark capture of V2X PDUs at this node's interface (report §1 demo table, committed method).
