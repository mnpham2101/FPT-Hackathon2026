---
marp: true
theme: default
paginate: true
title: "M1 System Design"
description: "Design deck — the five-node blueprint, the message and video data paths, the protocol stack, and the track-admission state machine of the Cooperative Awareness System"
deck: "M1 System Design · Team KIS · FPT Hackathon 2026"
---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-title-city.jpg)

# M1 System Design

## The blueprint, the data paths, and the algorithm

**Milestone 1 · Team KIS — FPT Hackathon 2026**

Full document: [system-design.md](../../documents/Design/SYSTEM-DESIGN/system-design.md) · per-node HLDs: [Module Design](../../documents/Design/MODULE-DESIGN/README.md)

---

# Table of contents

1. **Introduction** — the NLOS relay mission
2. **Blueprint design** — five nodes on one Ethernet bridge
3. **Data path** — messages, golden vectors, and the video path
4. **Protocol stack** — one transport, two encodings, four contracts
5. **Algorithm** — track admission as the risk gate

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 01 · Introduction

---

# The NLOS relay mission

- Three vehicles in a collinear convoy: **A follows B follows C**; B blocks A's view of C.
- **B's perception of C reaches A over V2X**; A composes `d_AC ≈ d_AB + d_BC` and renders C as a ghost object from the relay alone.
- Milestone 1 builds **only vehicle A**, entirely on the CarSky cloud platform; B and C are simulated by the bench.
- One blueprint is **one car** — the ego and the bench equipment around it.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 02 · Blueprint design

---

# Five nodes on one Ethernet bridge

![h:340 The Milestone 1 blueprint: bench, V2X-ECU, ADA-ECU, IVI-ECU and the Ethernet Bridge, with addresses and message flows](../assets/phase2-4-blueprint-5-nodes.svg)

- A star on one bridge: every role node declares exactly one `ethernet` pin; the three flows are UDP ports on the same NIC.

---

# Ethernet as the Milestone 1 network

- **Chosen for ease of implementation.**
- The bridge is a platform-native node type.
- One L2 segment, static IPv4 — no routing, no gateway, no broker.
- Every hop is a direct UDP datagram to a known peer.
- **The code is not bound to the transport.**
- Addresses and ports come from blueprint env, never hardcoded.
- Each node holds its socket in one component behind a seam.
- A new protocol replaces that component — **not the message logic**.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 03 · Data path

---

# The message path

| Message | Direction | Purpose |
| --- | --- | --- |
| **R1 — CPM** | Bench → V2X-ECU (`:47100`) | The mocked stream a real radio would deliver — perceived vehicle C |
| **R2 — decoded CPM information** | V2X-ECU → ADA-ECU (`:47200`) | One perceived-object update, after decode · validate · dedupe |
| **R3 — tracked object** | ADA-ECU internal | Feeds store, admission and risk; on the wire only inside R4 |
| **R4 — warning from ADA** | ADA-ECU → IVI-ECU (`:47300`) | The scene geometry and risk state the HMI renders |

- One UDP datagram per wire message.
- R1 is the only mocked wire source.

---

# Golden vectors

- Each vector is a **pair of two files**: `<case>.json` the content, `<case>.uper` the frozen wire bytes.
- Encoder and decoder test against the **same pairs** — the R1 wire format cannot drift.

| Vector | Pair | Exercises |
| --- | --- | --- |
| `nominal` | `.json` · `.uper` | The committed happy path |
| `gate-boundary` | `.json` · `.uper` | The admission-gate boundary — exactly 30.00 m |
| `coord-large` | `.json` · `.uper` | Coordinate bounds, datagram size budget |
| `mdt-min` / `mdt-max` | `.json` · `.uper` each | Measurement-delta-time bounds |
| `conf-unavailable` | `.json` · `.uper` | The unavailable-confidence mapping |

- Embedded in each CPM: sender reference position and time, station id, one perceived object.

---

# Video path — no live feed

![h:280 Data flow: the mocked CPM stream and the ADA-ECU's looped saved video in amber, the real messages in navy](../assets/m1-system-dataflow.svg)

- Live camera bring-up is a **frozen scope boundary**: the ego video is a saved 10 s clip **built into the ADA image**, looped at runtime.
- Video never crosses the network — the amber loop is a file read inside the node.

---

# Tracked objects on the wire

![h:330 The M1 blueprint with the video path: clip baked in at build time, tracked-object information forwarded over Ethernet](../assets/m1-video-source-topology.svg)

- Obstructed-object information travels as **tracked-object data inside messages over Ethernet** — the object snapshot embedded in each warning.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 04 · Protocol stack

---

# One transport, two encodings

![h:330 The protocol stack: Ethernet bridge, IPv4, UDP, then UPER for the CPM and JSON for every other contract](../assets/m1-protocol-stack.svg)

- Below the encoding row everything is shared: UDP one-message-per-datagram over static same-subnet IPv4 on one bridged L2 segment.
- The CPM is the only binary encoding (ASN.1 UPER, ETSI TS 103 324) and the only third-party codec; every other contract is versioned JSON against a committed schema.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 05 · Algorithm

---

# Track admission as the risk gate

![h:300 The admission machine: not_tracked, tentative, tracked — distance gates with hysteresis and a silence timeout](../assets/phase2-4-ada-admission.svg)

- One machine for both perception sources — relayed C and own-sensor B — parameterized only by what counts as an update.
- Distance gates with **hysteresis** (enter 30 m, exit 35 m) and a **monotonic silence timeout**; only a `tracked` entry is published to the risk assessment.
- **Vehicle variable speed is not used now** — the criterion is fixed distance; speed scaling is a registered future development.

---

<!-- _class: lead -->

![bg](../assets/bg-fpt-tower.jpg)

# Thank you

Team KIS · Cooperative Awareness System · full document: system-design.md
