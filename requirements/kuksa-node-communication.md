# KUKSA Node Communication — Design Note

Companion note to [car-sky-guide/carsky-kuksa-blueprint-guide.md](car-sky-guide/carsky-kuksa-blueprint-guide.md), which owns the full pin-type comparison, feasibility verdict, and step-by-step import procedure. This note covers the communication model and the design choice at a glance.

## 1. Introduction — communication between nodes using KUKSA

- [Eclipse KUKSA](https://github.com/eclipse-kuksa) is an open-source, VSS-schema-based signal broker (gRPC pub/sub) from the COVESA vehicle-signal standard.
- In this trial variant, every M1 node (Bench, V2X ECU, ADA ECU, IVI ECU) wires one `kuksa` pin to a single `kuksa-databroker` node — a star topology, broker in the center, replacing the baseline's `eth-bridge` node.
- Each node publishes/subscribes to named signal paths on the broker instead of opening direct sockets to its peers. The existing R1/R2/R4 contract payloads travel unchanged, carried verbatim inside custom signal values (§3) — this is not a re-encoding of those contracts as "real" vehicle signals.
- Full topology and node configs: [blueprint-m1-cooperative-awareness-kuksa.json](car-sky-guide/blueprint-m1-cooperative-awareness-kuksa.json).

## 2. KUKSA vs. Ethernet Bridge

| | Ethernet Bridge (baseline, R6, frozen) | KUKSA Databroker (this trial variant) |
|---|---|---|
| What it is | Raw L2/L3 network segment (`eth-bridge` node, Linux bridge mode) | VSS-schema signal broker (`kuksa-databroker` node) |
| How nodes talk | Direct point-to-point UDP/TCP, custom binary/JSON framing | Pub/sub against named, typed signals in a VSS tree |
| Topology | Star via a bridge node; each pair still opens its own socket/port | Star via the broker; every node has exactly one `kuksa` pin |
| Scriptable via CarSky REST/import | No — `ETHERNET` isn't in the `addPin` enum; wired manually in the Nydus UI | Yes — `KUKSA` is REST/import-creatable; the whole topology can be built with zero manual UI steps |
| Payload fit | Natural — just bytes on a socket, no schema to satisfy | Awkward — KUKSA expects real vehicle signals; here it carries opaque payloads via 3 custom `string` signals instead |
| Project status | Committed baseline — matches every node guide's design rationale | At-risk trial variant, not adopted — conflicts with each node guide's "why not kuksa" rationale and would re-freeze R6 project-wide |

## 3. KUKSA requires a VSS artifact

- The `kuksa-databroker` node config (`config.kuksa.vss.artifactId` / `versionId`) points at a **VSS artifact** — a CarSky artifact (category VSS) holding the signal schema the broker serves.
- This project's 3 custom signal paths (bench→V2X CPM, V2X→ADA object, ADA→IVI warning) aren't in any standard VSS catalog, so a custom VSS tree is required: [vss-m1-custom-signals.json](car-sky-guide/vss-m1-custom-signals.json).
- That file must be uploaded as a new VSS artifact through the CarSky UI (Artifacts → New Artifact → category VSS → Add Version) — CarSky's REST API has no file-upload route, only artifact/version metadata registration. The resulting artifactId/versionId then fill the `kuksa-broker` node's placeholders in the blueprint JSON.

## 4. Conclusion — design choice

- **Why KUKSA was tried:** it's the only CarSky pin type that is both REST/import-creatable and available on Container *and* Skycraft nodes directly, with an existing broker node type (`kuksa-databroker`) to star-wire into — the structural analogue of `eth-bridge`. Adopting it would make blueprint creation fully scriptable (no manual ethernet-pin wiring step).
- **Why it isn't the adopted baseline:** it overrides the ratified "why not kuksa" rationale already recorded in each node guide, and R6 ("Ethernet-bridge network") is a frozen contract — swapping transport means re-freezing R6 across every consumer, for a payoff that is purely operational (skipping one manual UI step).
- **Status:** studied alternative only. The concrete topology, node configs, and env wiring for this variant live in [blueprint-m1-cooperative-awareness-kuksa.json](car-sky-guide/blueprint-m1-cooperative-awareness-kuksa.json); the full feasibility verdict and open items are in [carsky-kuksa-blueprint-guide.md](car-sky-guide/carsky-kuksa-blueprint-guide.md).
