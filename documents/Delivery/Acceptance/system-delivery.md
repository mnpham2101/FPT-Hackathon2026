# M1 System Delivery — Acceptance Evidence

> The system-level delivery report for Milestone 1: what was deployed, on what resources, and the evidence that the full chain works. Abridged presentation: [phase6-system-delivery-deck.md](../../../presentation/phase6-systemIntegration/phase6-system-delivery-deck.md). How to reproduce every evidence item: [Test-Guides](../Test-Guides/README.md).

## Introduction

The system makes a vehicle aware of a hazard it cannot see by relaying another vehicle's perception. Milestone 1 implements one scenario, entirely on the CarSky cloud platform: three vehicles drive in a collinear convoy — A follows B follows C. A's view of C is blocked by B, so A's own camera can never detect C. B detects C and broadcasts that perception over V2X. A renders C as a ghost object with its relative position, sourced from the relay alone. M1 builds only vehicle A; B and C are simulated by the bench. Full scope and requirements: [m1-cooperative-awareness.md](../../Requirements/m1-cooperative-awareness.md).

| Field | Value |
|---|---|
| Team ID | KIS |
| Lead | Pham Ngoc Minh — mnpham1986@gmail.com |
| Solution name | Cooperative Awareness System |
| Reported version | Milestone 1 |
| Evidence folder | `documents/Delivery/Acceptance/` |

The evidence folder is read in two steps:

1. **System evidence is reported in this page** — the screenshots in this folder and the § System test evidence sections that explain each one.
2. **Supporting guides to reproduce the evidence and collaborate** — [Test-Guides](../Test-Guides/README.md): [apk-deploy.md](../Test-Guides/apk-deploy.md) installs the IVI app, [testing-guide.md](../Test-Guides/testing-guide.md) drives a run and collects its logs, screen captures and pcaps.

## Design

The system is a complete five-node blueprint — three ECUs, a supporting Scenario Player bench, and the Ethernet bridge joining them. The bench generates V2X messages as if received from another vehicle *in the same lane*, *directly in front*.

![The m1_system_test blueprint on the CarSky canvas: Bench — Scenario Player, V2X ECU, ADA ECU and IVI ECU, each wired to Ethernet Bridge 1, with the deployment Running 5/5](m1_system_blueprint.png)

| Node | Responsibility | Input | Output |
|---|---|---|---|
| Bench — Scenario Player | Emulates the V2X modem's connection point; plays the scenario that describes vehicle C | Scenario configuration file — nothing on the wire | Mocked CPM messages (UDP `:47100`) |
| V2X-ECU | Receives CPM messages, decodes, validates and dedupes them, forwards the result | CPM messages from the bench | Decoded CPM information (JSON, UDP `:47200`) |
| ADA-ECU | Fuses decoded CPM information with own-sensor detections of B, assesses collision risk, warns the IVI | Decoded CPM information; its own saved 10 s ego video, looped | Warning from ADA (JSON, UDP `:47300`) |
| IVI-ECU | Renders the driver warning: the God View with ego A, occluder B and ghost C | Warning from ADA | Warning screen — nothing on the wire |

Design details: [System Design](../../Design/SYSTEM-DESIGN/system-design.md) for the blueprint as a whole, [Module Design](../../Design/MODULE-DESIGN/README.md) for each node's HLD.

![Data flow across the four nodes: mocked CPM messages from the bench to the V2X-ECU in amber, decoded CPM information to the ADA-ECU and the warning to the IVI-ECU in navy, and an amber loop arrow on the ADA-ECU for its own looped saved video](m1-system-dataflow.svg)

Two inputs are mocked, drawn amber above:

- **The CPM messages** — there is no vehicle sending them; the bench generates the stream that a real V2X radio would receive.
- **The video feed** — there is no live camera; the ADA-ECU reads a saved video stored inside its own image and loops it.

The real messages under test are **V2X-ECU → ADA-ECU** and **ADA-ECU → IVI-ECU**.

## Resources

### Baseline resources

The CarSky platform resources the deployment stands on — provided by the organizers, not built by the team.

| Resource | What it is |
|---|---|
| KIS device | The team's device entry in the CarSky **Devices** panel; hosts the widgets bound to a deployment |
| IVI Screen widget | Screen-type widget rendering the AAOS guest's framebuffer live — the warning-screen evidence surface, with built-in recording |
| IVI ADB widget | ADB-type widget; its **Local ADB** tunnel is the path for APK install and `logcat` collection |
| AAOS image | The starter-pack `ANDROID_IMAGE` artifact, version `0.0.1`, arch `aarch64` — the guest the IVI app runs on |
| Skycraft node | CarSky VM node type; runs the AAOS guest (IVI-ECU) |
| Container node | CarSky container node type; runs the registry images (bench, V2X-ECU, ADA-ECU) |
| Ethernet bridge | Node type `eth-bridge`, `bridgeMode: linux`, subnet `10.99.0.0/24`; every node's `eth` pin wires to it in a star |

### Libraries

Third-party libraries in the delivered images, per node. Languages: Python 3.11 (bench, ADA detector), C++17 (V2X-ECU, ADA-ECU core), Kotlin 2.2.20 (IVI-ECU).

| Node | Library | License | Usage |
|---|---|---|---|
| Scenario Player | PyYAML 6.0.2 | MIT | Scenario YAML loading |
| Scenario Player | Vanetza ITS2 v26.06 | LGPLv3 (dynamically linked) | ASN.1 UPER encode of the CPM in the `cpm_encode` helper |
| Scenario Player | nlohmann/json 3.11.3 | MIT | JSON binding at the codec seam |
| Scenario Player | pytest ≥ 8 · jsonschema ≥ 4.18 | MIT | Unit tests, schema validation |
| V2X-ECU | Vanetza ITS2 v26.06 | LGPLv3 (dynamically linked) | ASN.1 UPER decode of inbound CPM |
| V2X-ECU | nlohmann/json 3.11.3 | MIT | Outbound object-message JSON and `[EVT]` log lines |
| V2X-ECU | Boost (transitive via Vanetza) | BSL-1.0 | Vanetza runtime dependency |
| V2X-ECU | GoogleTest 1.14.0 | BSD-3-Clause | Unit tests |
| V2X-ECU | tcpdump | BSD | In-container capture for the Wireshark evidence |
| ADA-ECU | nlohmann/json 3.11.3 | MIT | Contract bindings — object message in, tracked-object store, warning out |
| ADA-ECU | ONNX Runtime (CPU) 1.28.0 | MIT | YOLO11n inference session |
| ADA-ECU | YOLO11n (Ultralytics export) | AGPL-3.0 | Object-detection model, committed as ONNX |
| ADA-ECU | opencv-python-headless 5.0.0.93 | Apache-2.0 | Saved-clip frame decode |
| ADA-ECU | numpy 2.4.6 | BSD-3-Clause | Detector pre/post-processing math |
| ADA-ECU | GoogleTest 1.14.0 · pytest ≥ 8 | BSD-3-Clause / MIT | Core and detector unit tests |
| ADA-ECU | tcpdump | BSD | In-container capture for the Wireshark evidence |
| IVI-ECU | Jetpack Compose (BOM 2024.09.03) + Material3 | Apache-2.0 | The HMI layout and the God View canvas |
| IVI-ECU | kotlinx.serialization-json 1.9.0 | Apache-2.0 | Warning-message parsing into the typed model |
| IVI-ECU | kotlinx-coroutines 1.9.0 | Apache-2.0 | Receive loop and state propagation |
| IVI-ECU | Dagger Hilt 2.58 (+ Guava 33.4.0) | Apache-2.0 | Dependency injection |
| IVI-ECU | AndroidX Lifecycle 2.8.6 · activity-compose 1.9.2 | Apache-2.0 | ViewModel, service and Compose integration |
| IVI-ECU | JUnit4 · Robolectric 4.13 · MockK · Turbine | EPL-1.0 / Apache-2.0 / MIT | Unit tests |

## System test evidence

### Types of evidence

No single surface is sufficient: the screen does not prove where the data came from, the log does not prove anything rendered, and neither proves what crossed the wire. Collection procedure per type: [testing-guide.md § Step 3](../Test-Guides/testing-guide.md#step-3--evidence-collection).

| Type | What it proves | Surface |
|---|---|---|
| Warning screen | The driver-facing rendering happened | IVI Screen widget — screenshot or recording |
| Internal log | Each node produced and consumed what it should (`[TX]`, `[RX]`, `[EVT]` counters) | Node **View Log** (containers) · `adb logcat` (IVI app) |
| Wireshark capture | What actually crossed the wire, byte-exact | pcap exported from the V2X-ECU and ADA-ECU internal logs |

### Tools

Automation used to deploy the APK, collect logs, check them against expected results, and export the Wireshark captures. Usage details are in the [Test-Guides README](../Test-Guides/README.md) and the guides it links — each tool row is documented there, not here.

| Tool | Purpose |
|---|---|
| `INSTALL-IVI-APK.cmd` | Install the APK onto the AAOS guest and own the ADB tunnel |
| `COLLECT-LOGS.cmd` | One pass over a deployed Room: every node's log, the guest logcat, and a pass/fail summary against expected results |
| `EXTRACT-PCAP.cmd` | Turn the base64 capture blocks inside a saved node log into `.pcap` files Wireshark opens |
| `capture.sh` (inside the V2X/ADA images) | Run tcpdump in-container; emit `[CAP]` lines and the rotating pcap into the node log |
| `check_v2x_log.py` | Assert the V2X receive chain on a saved `[EVT]` stream |
| `adb logcat` | Read the IVI app's `[RX]` lines — the only surface carrying them |

### Expected evidence per node

| Node | Warning screen | Internal log | Wireshark capture |
|---|---|---|---|
| Bench — Scenario Player | — | `[TX]` lines only | — |
| V2X-ECU | — | `[EVT]` decode/forward counters, `[CAP]` lines | Exported from its log; fits the expected call flow — CPM in on `:47100`, decoded object out on `:47200` |
| ADA-ECU | — | `[EVT]` fusion/risk counters, `[CAP]` lines | Exported from its log; fits the expected call flow — object in on `:47200`, warning out on `:47300` |
| IVI-ECU | God View with ghost C | `[RX]` warning lines in the app's logcat | — |

### Evidence 1 — Warning screen and IVI log

![The IVI Screen widget showing the NLOS God View warning, with the IVI ECU logcat below it and the riskState transitions highlighted](Evidence1_WarningScreen_IVILog_RiskLabel.png)

- The banner reads **NLOS OBSTRUCTION — Vehicle C ahead (relayed via V2X)**.
- Ego A and occluder B are drawn solid; **C is drawn dashed as a ghost** with a risk glow, badge `[V2X] C · 31.3 m · RISK: MEDIUM`, and connectors `d_AB`, `d_AC`.
- The legend states C's provenance: `source: v2x_relayed — never seen by A's sensors`.
- Status bar: `MODE: WARNING`, `V2X LINK: BOUND :47300`.
- The logcat below shows `[RX] R4 message received: R4WarningEvent(…, warningType=nlos_obstruction, riskState=…)` with **riskState stepping low → medium → high** (highlighted).
- The view is drawn from warning messages only — the ego never detects C directly.

### Evidence 2 — ADA-ECU internal log

![The ADA ECU node log, stream user, with the own_sensor_ingest counter highlighted in the EVT counter lines](Evidence2_ADAInternalLog_OwnSensor.png)

- `[EVT]` counter lines show the fusion inputs side by side: `own_sensor_ingest` (highlighted, climbing 26316 → 26318) and `r2_ingest` — decoded CPM information from the V2X-ECU.
- `own_sensor_ingest` counts detections of **vehicle B from the ADA-ECU's own sensor path** — the detector running on its stored ego video.
- The ADA-ECU **loops its 10 s saved video** to keep B detected for the whole demo; no live camera is used.
- `r4_tx` and `risk_transition` count the warnings emitted toward the IVI-ECU.
- `[CAP]` tcpdump lines show the inbound wire traffic `10.99.0.11 → 10.99.0.12:47200`.

### Evidence 3 — V2X-ECU log

![The V2X ECU node log, stream user, showing EVT decode counters climbing and CAP lines for traffic in on 47100 and out on 47200](Evidence3_IVILog.png)

- `[EVT]` counters climb in lockstep: `rx_datagram` → `decode_ok` → `r2_forwarded`, with `decode_reject: 0`, `validate_reject: 0`, `dedupe_drop: 0`.
- Every received CPM is decoded and forwarded — the receive chain the milestone depends on.
- `[CAP]` lines show both wire sides: CPM in `10.99.0.10 → 10.99.0.11:47100` (58 bytes) and decoded object out `10.99.0.11 → 10.99.0.12:47200` (339 bytes).

### Evidence 4 — Bench log

![The Bench Scenario Player node log, stream user, showing TX lines with climbing sequence numbers and the scenario time advancing](Evidence4_Bench.png)

- `[TX]` lines carry a monotonically climbing `seq` and the `scenario_time_s` position, 58 bytes per CPM datagram.
- `scenario_time_s` advances through the 10 s scenario; the **bench resends the scenario cyclically**, so the message stream — and the demo — runs continuously.
- The bench is the only mocked wire source: these are the CPM messages no real vehicle exists to send.
