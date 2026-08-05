---
marp: true
theme: default
paginate: true
title: Phases 2-4 — ADA ECU Design
description: Design deck — the terminology, the blueprint slice the ADA ECU occupies, the three contracts it consumes, owns and produces plus its node-local record schema, its protocol stack and libraries, the image each blueprint node runs with the ADA node's architecture and call flows, how the node is tested, and what Phase 5 inherits
deck: Phases 2-4 — ADA ECU Design · FPT Hackathon 2026
---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-title-city.jpg)

# Phases 2-4 — ADA ECU Design

## Ego's perception and fusion node, module by module

**Milestone 1 · Cooperative Vehicle Awareness — FPT Hackathon 2026**

Deck A of two — the design. How that design was planned and executed is Deck B: [phase2-4-ada-ecu-task-execution-deck.html](phase2-4-ada-ecu-task-execution-deck.html).

Sources: [ada-ecu-hld.md](../../ADA_ECU/doc/ada-ecu-hld.md) · [ada-ecu-design-decisions.md](../../ADA_ECU/doc/ada-ecu-design-decisions.md) · [contracts/](../../contracts/) · [m1-cooperative-awareness.md](../../requirements/m1-cooperative-awareness.md)

---

# Table of contents

1. **Terminology** — every term this deck uses, before it is used
2. **The blueprint** — five nodes, and the one these three phases build
3. **The contracts** — R2 in, R3 owned, R4 out, and one node-local schema
4. **Protocol stack and libraries** — three channels, and the third parties that serve them
5. **The blueprint nodes** — the image each runs, the ADA node's architecture, and its call flows
6. **Testing** — the configurations that exercise the node, and the equipment they need
7. **Handoff** — what Phase 5 and the demo run inherit

*Task groups, subtasks, execution order and CI runs are Deck B's subject.*

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 01 · Terminology

---

# Platform vocabulary

CarSky's own words. Every later slide uses them without restating them.

| Term | Definition |
| ---- | ---------- |
| **Blueprint** | The design of one vehicle: its nodes and their wiring. Nothing executes. |
| **Node** | One ECU within a blueprint. A **Container Node** runs a container image; a **Skycraft Node** runs an Android guest. |
| **Pin** | A node's connection point. This node declares exactly one, an `ethernet` pin. |
| **Ethernet Bridge** | A node whose only function is to join the other nodes' pins into one network. |
| **Room** | The running instance of a deployed blueprint. |
| **View Log** | The platform's window onto a node's standard output — this node's only observation surface. |

- **A Container Node has no volume and no bind mount.** A file reaches it by being inside its image, which is why the demo clip is an image layer.

---

# Project vocabulary

Terms this project defined or narrowed. Each carries a specific meaning here.

| Term | Definition |
| ---- | ---------- |
| **Contract** | A frozen, versioned message definition agreed between two nodes, committed to the repository as a JSON Schema. |
| **Frozen** | Changed only by re-freezing across every consumer at once. A contract edited by one node alone is broken, not updated. |
| **Seam** | A boundary positioned so one side can be replaced without modifying the other. |
| **Ego** | The vehicle the system runs in — vehicle A, whose driver is warned. |
| **B** | The occluder directly ahead of ego, and the only vehicle ego's own camera sees. |
| **C** | The vehicle beyond B, which ego's camera can never see and which reaches ego only over the V2X relay. |
| **Bench** | Sanctioned test equipment sharing the Room network, standing in for the outside world. |
| **Isolated Room** | A reduced Room in which bench roles stand at the neighbouring nodes' addresses, so this node runs alone. |

- **The god view is the display's overhead drawing of the three vehicles**, and "ghost C" is its name for the relayed one. This node composes C's position; drawing it is the IVI's work.

---

# The requirement numbers this node serves

An `R`-number is an identifier from the requirements report, never a name.

| | What it is |
| --- | ---------- |
| **R3** | The `TrackedObject` schema — the shape every ego-side track obeys, whatever produced it |
| **R12** | Object detection from provided video files, with a distance estimate and zero detections labelled C |
| **R13** | The track store and its admission state machine, with a proximity gate on the reported distance |
| **R14** | The Collision Risk Assessment abstraction — risk logic behind one interface, plus the record schema it reads and writes |
| **R15** | The warning output to the IVI ECU, edge-triggered on each risk transition |
| **R18** | Structured event logs that reconstruct a full run offline |
| **R19** | The end-to-end recorded run — the milestone's definition of done |
| **R20-R22** | Real-time paced stimulus, run alignment, and the choreography that places the first warning after the eighth second |

- **R1, R2 and R4 are contracts rather than behaviour this node owns.** R1 is the V2X message on the air — a Collective Perception Message, or CPM, encoded with the ASN.1 packing rules called UPER — and this node never sees one; R2 and R4 are the JSON messages it consumes and produces.
- **R5 and R6 are the platform and the network**: the node deployment, and the one Ethernet-bridge segment every node sits on.

---

# The node's own vocabulary

Coined or narrowed by this design. Every diagram label below is one of these.

| Term | Definition |
| ---- | ---------- |
| **Track** | One entry in the store: an identity, a position, a range, a provenance and a state. |
| **Update** | One admission-relevant observation of a track — one relayed message, or one detector line. |
| **Gate** | The proximity threshold pair that admits and drops a track, with hysteresis between them. |
| **Composed range** | `d_AC = d_AB + d_BC` — ego's own estimate of B plus B's reported range to C. |
| **Risk band** | A level — `low`, `medium`, `high` — that a total, ordered condition table assigns to the composed range and its rate of change. |
| **Plugin** | One risk rule realizing the assessment interface, found by its registry key. |
| **Dwell** | The interval a risk change must hold before it is committed and emitted. |
| **`[EVT]` stream** | One JSON line per event on standard output, carrying both a monotonic and a wall-clock stamp. |

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 02 · The blueprint

---

# Five nodes, one bridge

One blueprint is one vehicle — ego. Four nodes carry a role; the fifth joins them into a network.

![h:420 Milestone 1 blueprint: five nodes, three contract-labelled UDP flows, one Ethernet Bridge](../assets/m1-blueprint-5-nodes.svg)

- **Addresses are static by design**, injected through node configuration, never written as literals in source.

---

# The ADA slice these three phases build

![h:520 The ADA node at 10.99.0.12, its two network edges, and the real node or bench stand-in that can sit at each](../assets/phase2-4-ada-slice.svg)

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 03 · The contracts

---

# R2 — the object message consumed

What the V2X ECU passes inward once it has decoded the air message.

| | |
| --- | --- |
| **Direction** | V2X ECU → ADA ECU, one way, UDP to `10.99.0.12:47200`, one message per datagram |
| **Encoding** | UTF-8 JSON, `type: "v2x_object"`, one message per perceived-object update |
| **Required fields** | `schemaVersion` · `type` · `stationId` · `rxTime` · `sender` (lat, lon, heading, speed) · `object` (objectId, timeOfMeasurement, distance, position, speed, classification, confidence) |
| **Normative file** | `contracts/r2-v2x-object.schema.json` — **frozen** |
| **Node copy** | `ADA_ECU/contracts/r2-v2x-object.schema.json`, byte-identical, bound by `src/contracts/r2_message` |

- **`object.distance` is the admission input, and the V2X ECU derives it.** This node reads the field as received and never recomputes it.
- **Two fields are nullable and need a rule.** A null class confidence maps to the lowest confidence, because the R3 field is required; a null sender speed is read for evidence only, and no rule depends on it.
- **`position.confidence` is a position accuracy in metres, not a probability**, and has no R3 home — it is carried in the ingest event and dropped from the track.

---

# R3 — the track schema owned

Not a datagram. The one shape every ego-side object obeys, whatever produced it.

| | |
| --- | --- |
| **Role** | Perception sources are interchangeable because they conform to one schema |
| **Crossing points** | The detector's standard output, and the `object` field of every R4 warning |
| **Required fields** | `id` · `class` · `source` · `position` · `distance` · `speed` · `confidence` · `state` · `timestamps` (measured, received, lastUpdated) |
| **Normative file** | `contracts/r3-tracked-object.schema.json` — **frozen** |
| **Node copies** | `ADA_ECU/contracts/r3-tracked-object.schema.json`, bound twice: `src/contracts/tracked_object` in the core and `detector/contracts/tracked_object` in the detector |

- **`source` is an enumeration of exactly two values** — `own_sensor` and `v2x_relayed` — and it is what the whole cooperative-awareness claim rests on.
- **The store is the sole writer of `state`.** An arriving object carries `not_tracked` by convention and that value is discarded.
- **Track identity is namespaced by source:** `own:<n>` from the detector's own association, `v2x:<stationId>:<objectId>` from the relay. Neither side can mint the other's.
- **Expiry does not read `lastUpdated`.** The wire stamps are wall-clock; expiry compares a monotonic stamp the store keeps beside each entry.

---

# R4 — the warning message produced

The warning, and everything the display needs to draw the scene.

| | |
| --- | --- |
| **Direction** | ADA ECU → IVI ECU, one way, UDP to `10.99.0.13:47300`, no framing header |
| **Encoding** | UTF-8 JSON; two message kinds discriminated by `type`, the schema being a `oneOf` |
| **Warning event** | `schemaVersion` · `type` · `warningType` · `riskState` · `object` — a whole R3 snapshot of C — · `geometry` with `ego`, `vehicleB` and a nullable `vehicleC` |
| **Awareness state** | `schemaVersion` · `type` · `seq` · `vehicles`, last-value-wins by sequence number. Optional in R15 |
| **Normative file** | `contracts/r4-ada-ivi.schema.json` — **frozen**, embedding the R3 schema by reference |
| **Node copy** | `ADA_ECU/contracts/r4-ada-ivi.schema.json`; `output/warning_builder` is the node's only producer |

- **`object.source` is `v2x_relayed` on every warning this node emits**, because only a relayed track can be C. The display's provenance guard renders the ghost on that value alone.
- **`warningType` and `riskState` are plain strings in the schema**, so an unknown value degrades at the consumer rather than being rejected. The `low` / `medium` / `high` vocabulary is fixed by the schema's description and by the record schema's enumeration, not by an enumeration in R4.
- **`geometry.vehicleB` is required and never null;** `geometry.vehicleC` is nullable — and the exact null rule is a flagged contradiction, § Open items.

---

# The CRA assessment record — node-local, not a contract

R14's acceptance names the schema the assessment reads and writes as a committed artifact. This is that artifact.

| | |
| --- | --- |
| **File** | `ADA_ECU/schema/cra-assessment-record.schema.json` |
| **Status** | **Node-local and deliberately outside `contracts/`** — it crosses no node boundary, so the sync manifest does not govern it |
| **Shape** | One record per assessed track: 14 required fields, keyed by `trackId` plus `warningType` |
| **What it carries** | The last committed risk state and when it was entered · the assessment's lifetime · the last and prior composed range · the derived closing rate and time to collision · the last R3 snapshot · the last known B position · an emitted count and a rationale |
| **Where it lives at run time** | In process, behind a typed accessor. Every write is also appended to the `[EVT]` stream, so the table is reconstructible offline |

- **Two of its fields exist to survive an erasure.** The last snapshot and the last known B position are what let a *clearing* warning still carry a required `object` and a required `vehicleB` after the track is gone.
- **A database engine was rejected.** SQLite adds a dependency and a file lifecycle to a node with no volume, and buys nothing over an append that already leaves the node through its only egress.

---

# Where the contracts live

One authority at the repository root, and a byte-identical working copy inside each node that uses it.

| Location | Holds |
| -------- | ----- |
| **`contracts/`** | The frozen originals — all four schemas, the R1 profile, the reference vectors and the samples |
| **`ADA_ECU/contracts/`** | R2, R3, R4 |
| **`ADA_ECU/schema/`** | The assessment record — node-local, not part of the manifest |
| **`ADA_ECU/tests/fixtures/samples/`** | The shared samples, synced the same way |

- **The copies are not forks.** `contracts/sync-manifest.json` lists every file that must match and `contracts/check_sync.py` fails as soon as one diverges — 47 copies across the repository.
- **The reason for copying:** each node folder must build independently with no cross-folder reads, which a shared directory would breach.
- **A node-local fork would be a second, unversioned contract** — tightening a field, promoting one to required — that keeps passing after the real one changes. That is what the gate exists to prevent.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 04 · Protocol stack and libraries

---

# The node's protocol stack

![h:520 The ADA node's protocol stack: three channels — R2 in, the detector's R3 over standard output, R4 out — with the encoding, library, transport, network and link layers each rests on](../assets/phase2-4-ada-protocol-stack.svg)

---

# The libraries and their licences

Nine third-party dependencies, each performing one function. All open source and Linux-compatible.

| Library | Version | Licence | What it serves |
| ------- | ------- | ------- | -------------- |
| **nlohmann/json** | v3.11.3 | MIT | R2 in, R3 in the store, R4 out — every JSON binding in the core |
| **ONNX Runtime** | 1.28.0 | MIT | The detector's inference session, CPU execution provider only |
| **OpenCV** (`opencv-python-headless`) | 5.0.0.93 | Apache-2.0 | Video decode behind the frame-source seam |
| **NumPy** | 2.4.6 | BSD-3-Clause | The detector's array arithmetic |
| **YOLO11n weights** | — | AGPL-3.0 | The detection model, exported once to ONNX and committed |
| **Python standard-library `json`** | 3.11 | PSF | The detector's R3 output lines |
| **GoogleTest** | v1.14.0 | BSD-3-Clause | The core's unit suites, driven by CTest |
| **pytest** · **jsonschema** | ≥ 8 · ≥ 4.18 | MIT | The detector's suites, and schema validation in test |

- **None of these is a framework.** Each is a library the code calls; none owns the program's control flow.
- **Deliberately absent: an ASN.1 codec**, because this node never sees a CPM, and **any database engine**, because the assessment table is in process.
- **The three detector wheels are pinned** to versions proven to install as prebuilt `aarch64` wheels, so an image build never falls back to compiling under emulation.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 05 · The blueprint nodes

---

# Delivered images

One row per node of the blueprint. Only the third is built by these phases; the bench below it is test equipment rather than a node.

| Node | Image | Role | Messages | State after Phases 2-4 |
| ---- | ----- | ---- | -------- | ---------------------- |
| **Scenario Player** | `m1-scenario-player:latest` | Bench — one object update every 100 ms | Produces R1 | Phase 1's node, unchanged. Absent from the isolated Room |
| **V2X ECU** | `m1-v2x-ecu:latest` | Relay — decode, validate, deduplicate, forward | Consumes R1, produces R2 | Phase 1's node, unchanged. A bench role stands at its address in the isolated Room |
| **ADA ECU** | `m1-ada-ecu:latest` | Perception and fusion — the node these phases build | Consumes R2, holds R3, produces R4 | One image, two processes: the C++17 core and the Python detector subprocess |
| **IVI ECU** | None — an APK on the Skycraft node | Display — the god view | Consumes R4 | **Not built here.** Phase 5's work; a bench role binds its port in the isolated Room |
| **Ethernet Bridge** | None — a platform node type with no image | Joins every node's `ethernet` pin into one L2 segment | Carries all three flows | Unchanged since Phase 0 |
| *ADA bench* | `m1-ada-bench:latest`, from `tools/ada-bench/` | Test equipment — two roles selected by `ROLE` | Emits R2 as `v2x_mock`, receives R4 as `ivi_mock` | Built by these phases. **Not a node of the blueprint** — it stands in for two |

- **The two stand-in roles are one image**, and nothing on this node's side changes when a stand-in replaces a neighbour.

---

# Component architecture — the data path

![h:475 The ADA node's data path: the V2X ECU interface, the detector subprocess, the observer and parsers, the R3 store, the assessment subsystem and the output stage reaching the IVI interface](../assets/phase2-4-ada-arch-a.svg)

---

# Component architecture — shared edges, artifacts and evidence

![h:390 The rest of the node: the composition root, the sole socket holder, the configuration reader and the event writer; the capture pair; the artifacts baked into the image; and the host-side checks and the View Log outside it](../assets/phase2-4-ada-arch-b.svg)

---

# Reading the component diagrams

![h:470 The legend of the component diagrams: the fill colours naming a component's role, and the notation for dependency, realization, node frames and test equipment](../assets/phase1-des-arch-legend.svg)

- **Both halves above are one diagram**, cropped from the design's own component map. The copy in `ADA_ECU/doc/` is the authority.

---

# MVC separation, and the layer rule

Each component sits in one layer, held there by the rule in the right-hand column.

| Layer | Components | The rule that keeps it separate |
| ----- | ---------- | ------------------------------- |
| **Data** | the frozen bindings, the track store, the assessment table with its schema, the configuration reader, the event log | Models and stores hold no rules: the store keeps, refreshes and erases entries but never decides what a distance means |
| **Business logic** | admission, the chained-collision plugin, the scene composer, and the detector's inference, distance and association | Pure functions of their inputs. None opens a socket, reads the environment or formats a wire message |
| **Controller** | the composition root, both observers, both parsers, the warning builder, the sender, and the detector's entry point, pacer and emitter | The controller owns the clock, the threads, the subprocess and the sockets, and holds no rules |
| **View** | none — the node is headless | The rendering surface is the IVI's god view; the local observation surface is the event stream |

- **No layer is collapsed:** admission cannot reach a socket, a plugin cannot send a datagram, and the detector cannot reach the store — its only route in is one line through a parser.
- **Four named components sit in no row above** — the socket holder, the plugin registry, the frame-source seam and the detector's configuration reader. The component diagram colours the first three; the table is what is incomplete. § Open items.

---

# Abridged folder structure

```
ADA_ECU/
├── Dockerfile · entrypoint.sh · capture.sh   two stages on one base image, single-platform arm64
├── CMakeLists.txt                            the executable, one library per module, the test targets
├── contracts/                                byte-synced R2 · R3 · R4 schema copies
├── schema/                                   the assessment-record schema — node-local
├── src/
│   ├── main.cpp                              the composition root
│   ├── config/ · net/ · log/                 sole environment reader · sole socket holder · the event stream
│   ├── observer/                             the two readers and the one bounded queue
│   ├── parser/                               wire and line to the frozen object
│   ├── store/                                the R3 store · the admission machine
│   ├── cra/                                  the interface · registry · record table · plugins/
│   ├── fusion/ · output/                     the composed geometry · the warning builder and sender
│   └── contracts/                            the frozen R2 / R3 / R4 bindings
├── detector/                                 the Python pipeline, its bindings, its pinned wheels, its tests
├── models/ · media/                          the exported model · the demo clip and its provenance record
├── tools/                                    host-side readers of this node's own output — no image, no data path
├── tests/                                    GoogleTest suites mirroring src/, plus the malformed corpus
└── doc/                                      the design, the decision record, the diagrams, research notes
```

- **The image workdir mirrors this folder**, so a path here is a path inside the container.
- **Test directories mirror source directories**, so the suite covering a module is always at the matching path.

---

# Phase 2 — the mock-driven scaffold

The whole pipeline standing up before any machine learning, driven by external stimulus.

| What Phase 2 made | Why it is shaped that way |
| ----------------- | ------------------------- |
| The core's holders — one configuration reader, one socket holder, one event writer, one bounded queue | Each concern has exactly one place it can be changed, and the socket header appears in one translation unit |
| Both parsers, and a corpus of one-defect malformed inputs | Every rejection is counted by reason and none is fatal; a conforming producer must yield a zero reject count |
| The R3 store and the R13 admission machine | The machine is pure and source-agnostic, parameterized only by what counts as an update |
| The assessment interface, its registry and its record schema | Frozen in Phase 2 so Phase 4's plugin proves the claim that adding one is one new file plus one line |
| The composition root, the image and the entry point | One startup path; any failure becomes one logged line and a non-zero exit |

- **The mocks are stimulus, never a branch.** A recorded own-sensor stream reaches the core through the *real* reader, and mock relayed traffic arrives as real datagrams on the real socket — so there is no mock code path anywhere in `src/`.
- **"No input yields no tracks" is a configuration**, not a code path: the detector disabled and no traffic arriving.

---

# Phase 3 — the clip, and the range it yields

The detector's only input is a file inside the image. That is a platform finding, not a preference.

| | |
| --- | --- |
| **The clip** | `ADA_ECU/media/ego-b-occluding-c.mp4` — 1280×720, 20 fps constant, 200 frames, 10.0 s, ~5 MB |
| **Provenance** | Openly-licensed dashcam footage, Pexels License — commercial use and modification permitted; credited in `media/ego-b-occluding-c.source.md` |
| **Content** | B is the frontmost in-lane vehicle in every sampled frame, closing from roughly 60 m to 10 m. C is never in the footage |
| **How it reaches the node** | One image layer. A Container Node has no volume, no bind mount and no file-injection field |
| **Sampling** | Every fourth decoded frame — 5 Hz effective, against a budget of 200 ms per sampled frame |

- **The `video` pin was rejected**, and its cost is the reason: no C++ client, an undocumented frame header, and 590 Mbit/s of raw pixels for content a local file serves at 0.5 MB/s — plus an R5/R6 re-freeze.
- **The range estimate is calibrated, not assumed.** The two camera constants ship at 2.6 m and 34.4° after a retune against this clip; the default car width put B inside the gate from the first frame. The gate itself was not moved, and never is.
- **A 10 s clip means every long-run behaviour depends on looping**, and the gap across a wrap must stay under the track timeout or ego's own B track expires between cycles.

---

# Phase 4 — fusion and emission

Live relayed traffic becomes a tracked C, an assessed risk, and one warning on the wire.

| Component | Its single responsibility |
| --------- | ------------------------- |
| **Scene composer** | B from the nearest own-sensor track, C at `d_AB + d_BC` longitudinally and offset laterally component-wise, ego at the origin |
| **Chained-collision plugin** | The M1 non-line-of-sight rules: the composed range, its closing rate and time to collision against three bands, debounced |
| **Assessment record table** | The prior record every edge trigger and closing rate needs, and the snapshots a clearing warning needs |
| **Warning builder** | The node's only R4 producer — a risk finding plus the composed geometry onto the frozen binding |
| **IVI sender** | The node's only outbound socket — one datagram per event, and one event line carrying the body |

- **The plugin never emits.** It returns a finding; the output stage decides transport. That is what keeps a rule replaceable without touching the wire.
- **`d_AC` needs `d_AB`, so with no own-sensor B there is no chain.** The assessment returns `low` with an explicit rationale and logs that it was skipped — which is also why a *clearing* warning can always fill the required `vehicleB` from the last known position.
- **Adding a future plugin is one new file plus one line** in `src/cra/builtin_plugins.cpp`, with no edit to the interface, the store, the emitter or any existing plugin. Registration is explicit rather than static, because a linker discards unreferenced registrars in a static library.

---

# The risk bands and the emission rule

The table is total and ordered — the first matching row wins, so no state matches two rows and none matches none.

| | Band | Condition |
| --- | ---- | --------- |
| **1** | `high` | C tracked **and** (composed range ≤ 30 m **or** time to collision ≤ 3 s) |
| **2** | `medium` | C tracked **and** (composed range ≤ 60 m **or** time to collision ≤ 6 s) |
| **3** | `low` | everything else — no tracked C, no known B, or a tracked C neither near nor closing fast |

- **The bands threshold the composed range; the gate thresholds the relayed range alone.** They measure different quantities, so no ordering holds between them and the loader asserts none — a check relating the two would collapse the risk level into track identity.
- **60 m and 30 m put the first warning on a comparison rather than a derivative.** The composed range at C's admission is about 47 m, so the range clause commits the transition on its own with 13 m of margin, tolerating a range-estimate bias up to 1.7×. At 25 m the transition would ride entirely on a time to collision derived from the noisiest quantity in the node.
- **Every committed change emits exactly one warning, in both directions**, and steady state emits nothing. R4 carries no clear message, so the transition back down is the only way the display learns to stop warning.
- **These five risk values are architecture proposals awaiting ratification.** They are externalized, so ratifying or retuning is a node-configuration edit.

---

# Configuration — every value the deployment injects

No tunable appears as a literal. The core reads its values in one place, the detector in one other.

| Core | Default | Meaning |
| ---- | ------- | ------- |
| `V2X_LISTEN_HOST` · `V2X_LISTEN_PORT` | `0.0.0.0` · `47200` | The relayed-message ingress |
| `IVI_ECU_HOST` · `IVI_ECU_PORT` | `10.99.0.13` · `47300` | The warning egress |
| `GATE_ENTER_M` · `GATE_EXIT_M` · `CONFIRM_HITS` | `30` · `35` · `3` | The admission band and the confirmation count |
| `TRACK_TIMEOUT_MS` · `FUSION_TICK_MS` | `1000` · `100` | Silence before erasure · the tick period |
| `RISK_NEAR_M` · `RISK_CRITICAL_M` · `RISK_DWELL_MS` | `60` · `30` · `300` | The two range bands and the debounce |
| `DETECTOR_CMD` · `DETECTOR_ENABLED` | the detector entry point · `true` | The subprocess, and how a fixture replaces it |
| `VIDEO_CLIP_PATH` · `DETECTOR_FRAME_STRIDE` | the baked clip · `4` | The frame source and the decimation |
| `DETECTOR_CLIP_FPS` · `VEHICLE_WIDTH_M` · `CAMERA_HFOV_DEG` | `20.0` · `2.6` · `34.4` | The rate pacing targets, and the two pinhole constants |

- **Only the addresses, the ports and the two gate constants have a committed source.** Every other default above is this design's proposal.
- **A value the blueprint omits falls through to the application's own default**, never to one baked into the image. Startup validation then orders values of one quantity, bounds single values, and does nothing more.

---

# Internal call flow, part 1 — bring-up

![h:520 Bring-up: the node configuration reaches the composition root, configuration is read once, the plugins are registered, the socket is bound, and the detector subprocess is spawned](../assets/phase2-4-ada-callflow-1.svg)

---

# Internal call flow, part 2 — two perception paths, one store

![h:520 Ingest: a detector line and a relayed datagram arrive on separate threads, share one bounded queue, are parsed into the same frozen object, and meet at a single writer](../assets/phase2-4-ada-callflow-2.svg)

---

# The R13 admission machine

![h:520 The admission machine: absent from the store, tentative and tracked, with the enter gate, the confirm count, the wider exit gate, and the silence timeout that erases a track from either state](../assets/phase2-4-ada-admission.svg)

---

# Internal call flow, part 3 — the fusion tick and the warning

![h:520 The fusion tick: expiry, then the assessment reading the store and the record table, composing the range and the band, and the output stage building and sending one warning datagram](../assets/phase2-4-ada-callflow-3.svg)

---

# Internal call flow — inside the detector subprocess

![h:520 The detector: the committed clip or the synthetic fixture behind one frame-source seam, then pacing, inference, range estimation and association, emitting R3 lines on standard output](../assets/phase2-4-ada-detector.svg)

---

# The flow between the nodes

![h:520 This phase's fidelity: the CPM reaches the V2X ECU, the object message reaches this node, and one warning leaves for the display](../assets/phase2-4-ada-callflow-nodes.svg)

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 06 · Testing

---

# The four configurations that exercise the node

The same node binary and image in all four; only the neighbours change.

| Configuration | Upstream of R2 | Downstream of R4 | What it proves |
| ------------- | -------------- | ---------------- | -------------- |
| **Unit** | injected values and fixtures | assertions | Gate boundaries, the band table, the composed geometry and the pinhole arithmetic against hand-computed values |
| **Fixture-driven core** | a recorded own-sensor stream through the real reader, plus real datagrams on the real socket | a loopback receiver | The whole core runs with no model and no clip |
| **Isolated Room** | bench emitter at `10.99.0.11` | bench sink at `10.99.0.13` | The node deployed, on the platform's network, with both neighbours mocked |
| **Full blueprint** | the real Scenario Player and V2X ECU | the IVI Skycraft node | The same observables with nothing mocked |

- **Expected output is identical in the last two**, because the node's image, command, capabilities and environment are unchanged between them. A difference is a finding about a neighbour.
- **Only the first two have been exercised.** The deployed Room and the full blueprint are configurations this design is built for, not results it can report.
- **A fake detector cannot prove the real one detects**, which is why the clip runs against the actual model rather than against the synthetic fixture.

---

# What a run must show

Each line is one component's output, and together they are the acceptance evidence.

| Observable | Produced by |
| ---------- | ----------- |
| `detector_spawn`, then one own-sensor ingest line per detection per sampled frame | the detector reader, and the detector's emitter |
| `r2_ingest` carrying the received body, at 90 % or more of the producer's send count | the listener, the R2 parser and the event writer |
| a `parse_reject` count of zero against a conforming producer | both parsers |
| a transition to `tentative` then `tracked`, once for the relayed C and once for the own-sensor B | the admission machine |
| zero own-sensor entries claiming a relayed source, and zero relayed entries claiming an own-sensor identity | structural — neither namespace can mint the other's |
| `assessment` carrying the composed range, the time to collision and the rationale | the chained-collision plugin |
| `r4_tx` carrying the emitted warning body, with `object.source` `v2x_relayed` | the warning builder and the sender |
| a capture line for `10.99.0.12` → `10.99.0.13:47300` | the capture script |
| with C held beyond the gate: ingest still counting up, and no relayed transition at all | the gate — the negative case |

- **`r4_tx` is the strongest single line.** It proves both tracks existed at the same instant, which is Phase 4's output acceptance.
- **Six further checks read a finished run offline** — the two tracks' relative ages, both stimulus rates, and the first warning's onset.

---

# Test equipment, and the observation surface

None of the equipment below ships in the node image, and none of it is on the data path.

| | What it is |
| --- | --- |
| **`ADA_ECU/tests/fixtures/own_sensor_mock.jsonl`** | A recorded own-sensor stream the real reader replays, so the core runs with no model and no clip |
| **`ADA_ECU/tools/mock_v2x_sender.py`** | A relayed-message emitter driving the socket from the two committed bench scenarios |
| **`tools/ada-bench/`** at the repository root | One image, two roles: the emitter standing in at `10.99.0.11`, the checking sink standing in at `10.99.0.13` |
| **`ADA_ECU/tools/make_sample_video.py`** | A synthetic clip proving the decode path; it cannot produce detection evidence |
| **`ADA_ECU/tools/` readers** | Host-side readers of this node's own output: the zero-C check, the run-alignment check, the collision-risk event list, the capture extractor |

- **A mock of another node does not live in that node's folder**, and the bench sits outside every node folder so it can change without rebuilding what it tests.
- **The observable surface is the node's own standard output:** one `[EVT]` line per event, with a payload where the payload *is* the proof — the received message body, the emitted warning body, the distance and reason on a transition, the composed range and rationale on an assessment.
- **The node captures its own egress.** Traffic toward the display is evidence the V2X ECU's capture point cannot see, so the entry point starts a capture and then replaces itself with the binary — which is why the capture capability is unconditional in this node's configuration.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 07 · Handoff

---

# What Phase 5 and the demo run inherit

Written as the next stage's input list.

| | What may now be assumed |
| --- | --- |
| **The message it consumes** | R4 is frozen and this node is its only producer. Every warning carries a whole R3 snapshot of C with `source` `v2x_relayed`, a required numeric B position, and a nullable C position |
| **When a warning arrives** | Edge-triggered on a committed risk change, in both directions, after the debounce. Steady state is silent, and there is no clear message — the downgrade is the clear |
| **The vocabulary it renders** | Three risk levels and one warning type, `nlos_obstruction`; an unknown value must degrade rather than be rejected |
| **The address it binds** | UDP `47300` at `10.99.0.13`, one message per datagram, no framing header |
| **The evidence it joins** | The `[EVT]` stream reconstructs a run offline, and this node captures its own traffic toward the display |
| **The onset it can rely on** | No warning before the eighth second of a cycle is a property of configuration and bench scenario data — no contract, no code, and no orchestrator carries it |
| **The tooling it inherits** | The bench sink as a stand-in for its own node, and the host-side readers of this node's output |

- **Nothing carries forward from the Phase 2 mocks.** The recorded stream and the relayed-message emitter are retired by the detector and by a real upstream.

---

# Open items, and flagged contradictions

Reported rather than resolved. Each needs a decision, not an implementation choice.

| | Item |
| --- | --- |
| **Contract versus design** | The frozen R4 schema says `geometry.vehicleC` is null *until C is first tracked*; this node's design says null *exactly when C's track has been erased*. Contract-first means the schema wins, and both readings are legitimate nulls — the one-sided design wording is the defect |
| **Awaiting ratification** | The five risk values, and the two admission counters, whose intent the requirement fixes but whose form this design chose |
| **A count became a timeout** | The plan's missed-update count is realized as a wall-clock timeout, because "its messages stop" is a time condition a counter cannot express. Intent unchanged, form changed — flagged, not absorbed |
| **Event vocabulary drift** | A thirteenth event name exists for the disabled-detector case, and the machine emits a transition only on a state *change* rather than on every edge as the design's diagram note states |
| **Payload not designated** | The expiry event's payload shape is implementation-chosen; the design names the event but not its fields |
| **Paths beyond the tree** | Four host-side scripts and the tests directories that hold them exist outside the designated folder tree, as design-consistent additions |
| **Placement gaps** | Four components the design names sit in no row of its MVC table. Its component diagram places three of them, so the two halves of the design disagree about where they belong |
| **Requirement numbers** | R20, R21 and R22 are enumerated in a later report and not yet ratified; the pacer is built to this design's designation of it |

---

# What deliberately did not ship

Stated plainly, so nothing is assumed present that is not.

- **The periodic awareness state.** The binding carries the message and the rate key is validated, but no path emits it and the default leaves it off. R15 words it optional and no acceptance criterion depends on it.
- **The run-alignment checker.** The design designates a host-side checker for the six timing checks; it is not written, because two of its inputs — the bench's monotonic stamp and the detector's line shape — are other phases' to land first.
- **The deployed measurements.** The detector's warm-up interval and its inference rate on the platform's own processor are unmeasured, and the warm-up is the one value the demo choreography cannot be aligned without.
- **The deployed Room and the full blueprint.** Both are designed for and neither has been run, so no observable on the platform is reported here.
- **The annotated video export** with per-event risk labels, optional in the plan and built by nothing.
- **Every part of the display's dashcam view** — no clip serving, no exposed port, no media player and no video pin. A worked design for it exists; reading it is not permission to build it.
- **The radar and live-camera inputs** drawn in the requirement's own figure. There is no radar and no camera bring-up in this milestone, and frame acquisition sits behind a seam so a live source arrives later as one more implementation.

---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-fpt-tower.jpg)

# Thank you

**Phases 2-4 — ADA ECU Design** · Milestone 1 · FPT Hackathon 2026
