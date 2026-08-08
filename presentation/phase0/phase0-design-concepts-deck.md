---
marp: true
theme: default
paginate: true
title: Phase 0 — Design Concepts
description: Design deck — the terminology, the five-node blueprint, the four frozen contracts and their locations, the protocol stack and its libraries, the image the blueprint nodes ran and the state Phase 0 left them in, the contract gate and the smoke test that closed the phase, and what Phase 1 inherits
deck: Phase 0 — Design Concepts · FPT Hackathon 2026
---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-title-city.jpg)

# Phase 0 — Design Concepts

## The architecture the contracts describe

**Milestone 1 · Cooperative Vehicle Awareness — FPT Hackathon 2026**

Deck A of two — the design. The planning and execution of that design is Deck B: [phase0-task-execution-deck.html](phase0-task-execution-deck.html).

Sources: [phase0_tasks.md](../../plans/phase0_tasks.md) · [milestone1_high_level_plan.md](../../documents/Plan/milestone1_high_level_plan.md) · [phase0-contract-freeze-hld.md](../../deprecated/phase0-contract-freeze-hld.md) · [contracts/](../../contracts/)

---

# Table of contents

1. **Terminology** — every defined term, before it is used
2. **The blueprint** — five nodes, one bridge, three flows
3. **The contracts** — R1, R2, R3 and R4 in turn, and the location of each
4. **Protocol stack and libraries** — one transport, two encodings, four third parties
5. **The blueprint nodes** — the image each ran, and the state Phase 0 left them in
6. **Testing** — what the contract gate proved, and what the smoke test proved
7. **Handoff** — what Phase 1 inherits, and what it adds to both nodes

*Tracks, lanes and task groups — the planning terminology — are the subject of Deck B.*

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 01 · Terminology

---

# Platform terminology

The CarSky platform's own vocabulary. Every later slide uses these terms without restating them.

| Term | Definition |
| ---- | ---------- |
| **Blueprint** | The design of one vehicle: its nodes and their wiring. One blueprint is one vehicle. Nothing executes. |
| **Node** | One ECU within a blueprint. Its type determines how it executes: a **Container Node** runs an OCI image; a **Skycraft Node** runs an Android guest. |
| **Pin** | A node's connection point. Milestone 1 uses exactly one kind, the `ethernet` pin, and exactly one per node. |
| **Ethernet Bridge** | A node whose sole function is to join the other nodes' pins into one network. |
| **Room** | The running instance of a deployed blueprint. The blueprint is the design; the Room is the constructed system. |
| **Deployment** | The act of converting a blueprint into a Room. Only a deployment starts a container. |

- **Blueprint to Room is the complete model.** Editing a blueprint changes nothing that is running; only a new deployment applies a change.

---

# Project terminology

Terms this project defined or narrowed. Each carries a specific meaning here that it does not carry generally.

| Term | Definition |
| ---- | ---------- |
| **Contract** | A frozen, versioned message definition agreed between two nodes — a JSON Schema or an encoding profile, committed to the repository. It is not documentation about code; it is the authority against which code is measured. |
| **R-number** | The requirement identifier from the requirements report. `R1`–`R4` are the four contracts; `R5`–`R6` the platform and network; `R7` onward, per-node behaviour. |
| **Seam** | A boundary positioned so that one side can be replaced without modifying the other — the codec seam, the radio-adapter seam. |
| **Frozen** | Changed only by re-freezing across every consumer simultaneously. A contract edited by one node alone is broken, not updated. |
| **Ego** | The vehicle the system runs in — vehicle A, whose driver receives the warning. |
| **Bench** | Sanctioned test equipment sharing the Room network: here the Scenario Player, which stands in for the modem and the external world. |

- **"Contract" is the load-bearing term in this deck.** Phase 0 produced no running feature; it produced contracts.

---

# The four contracts

The identifiers labelling every flow on the following section's diagram. Each is given a full slide in section 03.

| | Contract | Between | Content |
| --- | -------- | ------- | ------- |
| **R1** | CPM profile | bench → V2X ECU | The V2X message as it exists on the air |
| **R2** | `v2x_object` | V2X ECU → ADA ECU | One perceived object, passed inward |
| **R3** | `TrackedObject` | *within* the ADA ECU | The structure every tracked object obeys, whatever its origin |
| **R4** | ADA → IVI warning | ADA ECU → IVI ECU | The warning, and the scene to be displayed for it |

- **Three of the four are messages between nodes; R3 is not.** It is a schema used inside one node, which is why it has no port and no flow of its own.
- **The numbers are not a sequence of steps.** They are identifiers. R1 through R4 happen to fall in flow order, which is a convenience rather than a rule.

---

# Contract terminology

The terms the contract artifacts themselves carry. They appear in the schemas, in the CI lanes and in Deck B.

| Term | Definition |
| ---- | ---------- |
| **UPER** | *Unaligned Packed Encoding Rules* — the ASN.1 rules that pack one R1 message into 58 bytes. The only non-JSON encoding in Milestone 1. |
| **Octet** | One byte. ASN.1 terminology, retained because the profile document uses it. |
| **Test vector** | One case: a message content paired with the exact bytes it must encode to — `nominal.json` with `nominal.uper`. |
| **Golden** | Those bytes are committed and frozen. A test encodes the `.json` and compares byte for byte against the stored `.uper`, rather than recomputing the expected result. |
| **Corpus** | The complete set of vectors — six here, selected to cover the boundary cases rather than a single nominal path. |

- **The six cases:** `nominal` · `mdt-max` and `mdt-min` (±2047 ms) · `conf-unavailable` (the 101 sentinel) · `gate-boundary` (an object at exactly 30 m) · `coord-large` (near the large-coordinate bound).
- **Their purpose:** the same message is encoded by C++ in the V2X ECU and decoded by a separate helper on the bench. The corpus is the single reference against which both are measured.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 02 · The blueprint

---

# Five nodes, not four

One blueprint is one vehicle — vehicle A, the ego. Four of its nodes are ECU or bench roles; the fifth is the Ethernet Bridge that joins them into a network.

| Node | Node type | Address | Serves |
| ---- | --------- | ------- | ------ |
| **Scenario Player (bench)** | Container Node | `10.99.0.10` | R11 |
| **V2X ECU** | Container Node | `10.99.0.11` | R7–R9 |
| **ADA ECU** | Container Node | `10.99.0.12` | R12–R15 |
| **IVI ECU** | Skycraft Node (AAOS guest) | `10.99.0.13` | R16–R17 |
| **Ethernet Bridge** | Ethernet Bridge Node | `10.99.0.1` | R6 |

- **The bridge owns no pin of its own.** Each role node declares exactly one `ethernet` pin wired to it — a star topology on the single subnet `10.99.0.0/24`.
- **The bench is a node, not a module.** It shares the Room network as test equipment; its code never enters the V2X ECU's folder.
- **Addresses are static by design**, so every UDP target remains deterministic across redeployments. Each address arrives through node configuration, never as a literal in source.

---

# Every flow labelled by its contract

![h:450 Milestone 1 blueprint: five nodes, three contract-labelled UDP flows, one Ethernet Bridge](../assets/m1-blueprint-5-nodes.svg)

---

# The routing

<div class="chain">
<div class="link"><span>Scenario Player</span><small>bench</small></div>
<div class="arr">→</div>
<div class="link hot"><span>V2X ECU</span><small>R1 in · R2 out</small></div>
<div class="arr">→</div>
<div class="link"><span>ADA ECU</span><small>R3 store · R4 out</small></div>
<div class="arr">→</div>
<div class="link"><span>IVI ECU</span><small>renders ghost C</small></div>
</div>

- **Every hop is same-subnet UDP.** No routing, no gateway, no broker and no middleware — three ports on one interface, not three pins.
- **One message per datagram**, on ports `47100` (R1), `47200` (R2) and `47300` (R4). A message never spans two datagrams.
- **Every hop is unidirectional** in Milestone 1. Nothing flows back from the IVI toward the bench.
- **R3 has no hop of its own.** It exists within the ADA ECU and reaches the network only when embedded in an R4 message.

*The content of each contract is the subject of the next section.*

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 03 · The contracts

---

# R1 — the CPM profile

The V2X message as it exists on the air. The only contract that is not JSON.

| | |
| --- | --- |
| **Defines** | The ETSI CPM fields Milestone 1 uses, with their units and bounds — a *profile*, narrowing a large standard to the demonstration's requirements |
| **Standard** | ETSI TS 103 324, release 2 |
| **Encoding** | ASN.1 UPER — 58 bytes for the nominal case |
| **Artifacts** | `r1-cpm-content.schema.json` — the content structure · `r1-cpm-profile.md` — the field-by-field profile · `golden-vectors/` — six `.json` and `.uper` pairs |
| **Copies held in** | `V2X_ECU/contracts/` · `Scenario_Player/contracts/` |

- **Two independent implementations must agree on it** — the V2X ECU's C++ codec and the bench's encoder helper — which is the reason the reference vectors exist.
- **CI generated the vectors, not a person**, and the same run proved regeneration is deterministic: generated twice, with an empty difference.

---

# R2 — `v2x_object`

What the V2X ECU passes inward once it has decoded the air message.

| | |
| --- | --- |
| **Defines** | One perceived-object update: identity, position, kinematics, confidence, and the timestamp at which it was valid |
| **Direction** | V2X ECU → ADA ECU, UDP `47200` |
| **Encoding** | JSON, versioned |
| **Artifacts** | `r2-v2x-object.schema.json` · `samples/r2-object.json` |
| **Copies held in** | `V2X_ECU/contracts/` · `ADA_ECU/contracts/` |

- **`object.distance` is derived here and never transmitted.** The air message carries a relative position; the distance is computed on arrival, so the two cannot disagree.
- **One message per object update**, not a batch: a CPM describing three objects becomes three R2 messages.

---

# R3 — `TrackedObject`

Not a message. The single structure every ego-side object obeys, whatever produced it.

| | |
| --- | --- |
| **Defines** | The track record: identity, state, geometry, provenance, and the confidence carried with it |
| **Direction** | None — internal to the ADA ECU |
| **Encoding** | JSON, versioned |
| **Artifacts** | `r3-tracked-object.schema.json` · `samples/r3-tracked-object.json` |
| **Copies held in** | `ADA_ECU/contracts/` · `IVI_ECU/contracts/` |

- **Its purpose is to remove the distinction between sources.** An object detected by the camera and an object received over the relay become the same kind of record on entry to the store.
- **`provenance` is what survives that flattening.** It records how the object was learned, which is what allows the demonstration to prove that C was never observed directly.
- **The IVI holds a copy** because R3 arrives embedded in every R4 message; the IVI must decode it although it never receives one alone.

---

# R4 — the ADA to IVI warning

The warning, and everything required to display it. The IVI renders from this message alone.

| | |
| --- | --- |
| **Defines** | A versioned warning event: the nature of the risk, the object that raised it, and the composed scene geometry to display |
| **Direction** | ADA ECU → IVI ECU, UDP `47300` |
| **Encoding** | JSON, versioned |
| **Artifacts** | `r4-ada-ivi.schema.json` · `samples/r4-warning.json` · `r4-state.json` · `r4-unknown-warning.json` |
| **Copies held in** | `ADA_ECU/contracts/` · `IVI_ECU/contracts/` |

- **The IVI performs no fusion and holds no world model.** Everything it displays comes from the message in hand, which is what keeps the display verifiable.
- **`r4-unknown-warning.json` is a deliberate sample** — a warning type the IVI does not recognise, so that forward compatibility is tested rather than assumed.

---

# Where the contracts are held

One authority at the repository root, and a working copy inside each node that consumes it.

| Location | Holds |
| -------- | ----- |
| **`contracts/`** | The frozen originals: all four schemas, the R1 profile, the reference vectors, the samples |
| **`V2X_ECU/contracts/`** | R1, R2 |
| **`ADA_ECU/contracts/`** | R2, R3, R4 |
| **`IVI_ECU/contracts/`** | R3, R4 |
| **`Scenario_Player/contracts/`** | R1 |

- **The copies are not forks.** `contracts/sync-manifest.json` lists every file that must match, and `contracts/check_sync.py` fails CI as soon as one diverges.
- **The reason for copying:** each node folder must build independently, with no cross-folder imports. A node reading from a shared directory would breach that rule.
- **A node holds only the contracts it uses.** The bench holds R1 and nothing further, because R1 is the only contract it touches.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 04 · Protocol stack and libraries

---

# One transport, two encodings, four third parties

![h:505 Protocol stack: message, encoding and library per contract, over a shared UDP, IPv4 and Ethernet-Bridge base](../assets/m1-protocol-stack.svg)

---

# The function of each library

Four third-party dependencies, each performing one function. All are open source and Linux-compatible.

| Library | Licence | Serves | Function |
| ------- | ------- | ------ | -------- |
| **Vanetza** | LGPLv3 | R1 | ETSI ITS release-2 ASN.1 codec — the only component that handles UPER |
| **nlohmann/json** | MIT | R2, R3, R4 | JSON binding on both C++ nodes, the V2X ECU and the ADA ECU |
| **Python standard-library `json`** | PSF | R1, R3 | The bench's scenario data, and the detector's R3 output stream |
| **kotlinx.serialization** | Apache-2.0 | R3, R4 | JSON binding on the IVI, the only Kotlin node |

- **Vanetza sits behind a single codec seam**, so no application code imports it directly; the standard can change without the application changing with it.
- **None of the four is a framework.** Each is a library the code invokes; none owns the program's control flow.
- **The transport requires no library.** UDP sockets are provided by each language's standard library.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 05 · The blueprint nodes

---

# Delivered images

Neither node had its production implementation yet. Phase 0 needed only to prove the connection between them, so both executed the same temporary container.

| | Scenario Player (bench) | V2X ECU |
| --- | --- | --- |
| **Phase 0 image** | `m1-netcheck:latest` | `m1-netcheck:latest` — the same image |
| **Difference between them** | Environment variables only: `ROLE`, `NEXT_HOP_HOST` and `NEXT_HOP_PORT` | Environment variables only, plus `LISTEN_PORT`, which is what makes it a relay |
| **Phase 0 role** | Source: emits one datagram per second | Relay: receives, stamps, forwards |
| **Contract used** | None | None |

- **One image, three roles, no code branches.** Whether a node sends, relays or receives is determined entirely by the variables set — the same discipline the production ECUs follow.
- **This is a deliberate placeholder, not a prototype.** No part of `netcheck.py` becomes production code; it is scaffolding that proved the topology and was then set aside.

---

# Node architecture

The image delivers one script that sends, stamps and forwards UDP datagrams. No node architecture exists yet, and none of the four nodes has an internal call flow of its own.

| | Phase 0 | Where the architecture arrives |
| --- | --- | --- |
| **Scenario Player** | One script, one role | Phase 1 — the module design, the kinematic model and the encoder path |
| **V2X ECU** | The same script, relaying | Phase 1 — the radio seam, the simulated modem and the receive pipeline |
| **ADA ECU** | The same script, receiving | Phases 2–4 — the track store, the detector and fusion |
| **IVI ECU** | The supplied artifact, untouched | Phase 5 — the display application |

- **No design decision is embedded in `netcheck.py`.** It proves the network carries datagrams between the addresses; every later node is designed against the contracts, not against it.

---

# Overall callflows between nodes

![h:495 Phase 0 netcheck call flow: the bench sends a stamped ASCII payload over UDP; the V2X node appends its role and forwards](../assets/phase0-netcheck-callflow.svg)

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 06 · Testing

---

# What the contract gate proves

Phase 0 shipped no running feature, so its verification is of the contracts themselves. One CI lane carries all of it.

| Check | What it proves |
| ----- | -------------- |
| **Schema validation** | Every sample validates against the schema it claims, so a committed sample cannot drift from its contract |
| **Round-trip tests in C++, Python and Kotlin** | Each node's language binding writes and reads its contracts identically — three implementations, one definition |
| **Reference-vector generation** | The six R1 message pairs are generated by a build-only tool, and the same run proves regeneration is byte-stable |
| **Copy synchronisation** | `check_sync.py` maps the originals onto all 36 node-local copies and exits non-zero on any byte difference |

- **The gate is what makes "frozen" enforceable.** A contract edited in one node alone fails the lane rather than reaching the next phase unnoticed.
- **`contracts-gate` was green over all 36 copies** — the evidence for the schema half of Phase 0's acceptance. The lane inventory and the run links are in [Deck B](phase0-task-execution-deck.html).

---

# Proving the network before the ECUs existed

Phase 0's fourth acceptance criterion was closed by a connectivity smoke test on blueprint `trial2_minh`. Its method, tooling, division of labour and evidence form a deck of their own; this slide states only its conclusions.

- **All five pass criteria met, 2026-07-31.** Every node running · zero errors · live per-node logs · traffic captured on the network · the chain proven by the payload's accumulated stamps.
- **It closed the final Phase 0 acceptance criterion** — blueprint topology documented and validated — which is what unblocked Phase 1.
- **It established two facts every later image inherits:** the registry host that responds, and the requirement that node images be single-platform `linux/arm64`.
- **It left two items open** — network maximum-packet-size headroom, and whether the AAOS guest can host a listener. Neither is blocking.
- **Full report:** [phase0-smoke-test-deck.html](phase0-smoke-test-deck.html) — method, tested object, testing agent, manual steps and results. None of its content is repeated here.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 07 · Handoff

---

# What Phase 1 inherits

Phase 1 implements the bench and the V2X ECU. The following are Phase 0 deliverables it builds on rather than re-establishes.

- **Four frozen contracts on disk**, with the six R1 reference messages and the samples — the definitions both nodes are written against, not a format still under discussion.
- **A gate that keeps them frozen.** `sync-manifest.json` and `check_sync.py` over 36 copies; Phase 1 extends the manifest rather than re-deriving it.
- **A validated topology.** Five nodes on `10.99.0.0/24` with static addresses and one bridge, deployed and observed running — so a Phase 1 node knows the address it transmits to before it is written.
- **Two platform facts every later image obeys:** the registry host that responds, and the requirement that node images be single-platform `linux/arm64`.
- **Six CI lanes** already running on every push, which Phase 1 extends with four more in the same shape.
- **The netcheck image**, reusable as a listener wherever a node has no implementation yet.

---

# What Phase 1 adds to both nodes

Phase 0 established whether the nodes can communicate. Phase 1 establishes what they say, and is where the production designs arrive.

| | Arriving in Phase 1 |
| --- | --- |
| **Scenario Player** | Scenario configurations as data, the kinematic model, and the encoder helper that converts a scenario into real R1 bytes |
| **V2X ECU** | The radio-adapter seam, the simulated modem behind it, and the receive pipeline: decode, validate, deduplicate, forward as R2 |
| **Both** | A high-level design, a per-node call flow, and unit tests against the reference vectors |

- **The netcheck image is retired as soon as either production image lands.** Nothing carries forward from it except the confirmation that the network functions.
- **The per-node call flows belong to Phase 1**, and are drawn in its deck: [phase1-design-deck.html](../phase1/phase1-design-deck.html).

---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-fpt-tower.jpg)

# Thank you

**Phase 0 — Design Concepts** · FPT Hackathon 2026
