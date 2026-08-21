# V2X-ECU test evidence

Extends [testing-guide.md](testing-guide.md): how to collect and read the evidence that the V2X ECU's Rx pipeline (R7–R9) actually processed the CPMs the bench sent. This applies identically whether the node was exercised in the isolated V2X test (`phase1_smoked_test`) or inside the full system test (`m1_system_test`) — the node produces the same `[EVT]` stream and the same capture either way; only what else the Room contains differs.

## Prerequisites

- A Room deployed and its images installed. [testing-guide.md](testing-guide.md) covers choosing a blueprint (`v2x-isolated-test`, `-Test 4`, or `system-test`, `-Test 1` — § Available blueprints), deploying it (§ Step 2), and installing the APK where the path includes the IVI node ([apk-deploy.md](apk-deploy.md)) — none of which this document restates.
- A CarSky REST API key at `secrets\carsky-api-key.txt`, the same file `COLLECT-LOGS.cmd` reads for every other test.
- Nothing else. Unlike the IVI evidence in [testing-guide.md § Step 3 · 2](testing-guide.md#2--logs), this node's evidence is entirely node-side — its log comes over REST and its traffic capture rides inside that same log — so no ADB tunnel is needed to read it.

## Log collection step

Two commands, run from the repo root, in order:

```powershell
.\tools\logs-collector\COLLECT-LOGS.cmd -Test system-test -Tail 50000
.\tools\pcap-extract\EXTRACT-PCAP.cmd .\test-report\system-test
```

The first pulls every node's log, including `node-v2x-ecu.txt`, into `test-report\system-test\`. Use a large `-Tail` — this node is high-volume, and the default 5000 lines can miss a whole pcap rotation. The second turns that log's pcap blocks into `.pcap` files named after it. Swap `-Test system-test` for `-Test v2x-isolated-test` to run the isolated blueprint instead.

Flags and failure modes: [logs-collector](../../../tools/logs-collector/README.md), [pcap-extract](../../../tools/pcap-extract/README.md).

## Log analysis

### 1 · The node's own call flow, from the HLD

[V2X-ECU HLD §9](../../Design/MODULE-DESIGN/V2X-ECU/v2x-ecu-hld.md#9-call-flow) — source: [phase1-v2x-ecu-callflow.puml](../../Design/MODULE-DESIGN/V2X-ECU/phase1-v2x-ecu-callflow.puml). The same rendered diagram is embedded in the HLD itself; the copy below is that image, not a redraw:

![V2X ECU call flow: capture starts, the in-node bring-up FSM (section A), then the live loop (section B, autonumbered 1-14) — datagram in, decode, validate, dedupe, build, forward, [EVT] — with the decode-reject, decode-ok, validate-reject, dedupe-drop, bounded-retry and subscription-drop branches marked at their emission points](../../Design/MODULE-DESIGN/V2X-ECU/phase1-v2x-ecu-callflow.svg)

Its `== § B Live loop ==` is what a captured log actually shows: bring-up (`§ A`) runs once at process start and falls outside a tail-limited log fetch on a node that has been running for hours, so it is not numbered. `§ B` is autonumbered `1`–`14` in the source, and the table below uses those same numbers — reading the diagram and reading this table point at the same step:

| # | Step | Component |
|---|---|---|
| 1 | Bench sends one UDP datagram (UPER CPM) to the bound `47100` | Bench → `net/udp_socket` |
| — | Concurrently, tcpdump sees it — a `[CAP]` line, and a byte into the rotating pcap (drawn as a note beside step 1, not its own numbered step) | `capture.sh` |
| 2 | The Rx thread hands the bytes to the pipeline via the subscribed callback | `adapter/stub_radio_adapter` → `pipeline/rx_pipeline` |
| 3 | Logs `rx_datagram` | `rx_pipeline` → `log/event_log` |
| 4 | Calls the codec to decode the UPER bytes | `pipeline/rx_pipeline` → `codec/vanetza_cpm_codec` |
| 5 | *(branch)* `DecodeError` → logs `decode_reject` and stops — never a crash (R9) | `codec/vanetza_cpm_codec` |
| 6 | Decode succeeds — logs `decode_ok` with the whole `cpm` object (D4) | `codec/vanetza_cpm_codec` → `log/event_log` |
| 7 | Checks the profile ranges (F9: `\|measurementDeltaTime\| ≤ 2047`) | `pipeline/validator` |
| 8 | *(branch)* Invalid → logs `validate_reject` and stops | `pipeline/validator` |
| 9 | Checks `(stationId, objectId, referenceTime + measurementDeltaTime)` against the sliding window | `pipeline/deduper` |
| 10 | *(branch)* Duplicate → logs `dedupe_drop` and stops | `pipeline/deduper` |
| 11 | Derives `distance = hypot(x, y)` (F7), the two confidence conversions (F6), and sender speed from consecutive positions (F1) | `pipeline/r2_builder` |
| 12 | Sends the R2 JSON to the forwarder | `pipeline/rx_pipeline` → `forward/ada_forwarder` |
| 13 | Sends the R2 JSON datagram to `10.99.0.12:47200` | `forward/ada_forwarder` |
| 14 | Logs `r2_forwarded` | `rx_pipeline` → `log/event_log` |

Rows marked *(branch)* are alternates — a fresh, valid, decodable datagram takes `1 → 2 → 3 → 4 → 6 → 7 → 9 → 11 → 12 → 13 → 14`, skipping `5`, `8` and `10`.

### 2 · Correlating the flow with a real `[EVT]` triplet

One datagram through steps 3, 6 and 11–14 above — from `node-v2x-ecu.txt` in the evidence collected for this document (§ below), one line per event, joined by a shared `mono_ms`:

| Event (step) | Key fields | What it shows |
|---|---|---|
| `rx_datagram` (3) | `bytes: 58`, `mono_ms: 3086663739` | The datagram arrived and step 2 handed it off |
| `decode_ok` (6) | `cpm.stationId: 1201`, `cpm.object.objectId: 7`, `cpm.object.position: {x: 4750, y: 120}` | Decoded successfully — no `decode_reject` (step 5) at this `mono_ms` |
| `r2_forwarded` (14) | `r2.object.distance: 47.515`, `r2.object.position: {x: 47.5, y: 1.2}` | Reached step 11's derivation and step 13's send with no `validate_reject` (step 8) or `dedupe_drop` (step 10) in between |

The three lines share `mono_ms` (and `epoch_ms`), which is what identifies them as one pass rather than three unrelated messages. `r2.object.distance = 47.515` is step 11 acting on this exact datagram, not a constant: `cpm.object.position` is wire-native centimetres (`CpmContent` carries the ASN.1 units unconverted, HLD §6), so `{4750, 120}` → `{47.50 m, 1.20 m}`, and `hypot(47.5, 1.2) ≈ 47.515` — the value the `r2_forwarded` line actually carries.

### 3 · The Bench ↔ V2X-ECU exchange, from the researched call flow

[scenario-player-v2x-callflow.puml § B](../../Design/MODULE-DESIGN/SCENARIO-PLAYER/scenario-player-v2x-callflow.puml) is the wire-level view of the same exchange — a research note, not an HLD, but the only diagram drawn from the bench's side of this wire. Reused here rather than redrawn:

![Scenario Player and V2X ECU call flow, section B autonumbered: the bench builds and UPER-encodes a CPM, sends it to the V2X ECU, which decodes/validates/dedupes/derives through the R9 pipeline and forwards R2 JSON to the ADA ECU](../../Design/MODULE-DESIGN/SCENARIO-PLAYER/scenario-player-v2x-callflow.svg)

Its `§ B` loop, autonumbered in the source:

| # | Step |
|---|---|
| 1 | Scenario Player advances the A/B/C trajectory one tick |
| 2 | Builds a CPM — header + management container + `OriginatingVehicleContainer` + one `PerceivedObjectContainer` (vehicle C) |
| 3 | UPER-encodes it with the shared R1 codec |
| 4 | Sends the datagram to `10.99.0.11:47100` |
| 5 | `StubRadioAdapter` delivers it via `onRx` — § 1 step 2 above |
| 6 | The R9 pipeline decodes, validates, dedupes and derives — § 1 steps 4, 6, 7, 9, 11 |
| 7 | The application sends the R2 JSON to the ADA ECU at `10.99.0.12:47200` — § 1 steps 12–14 |

Repeats at `cpm_rate_hz` (10 Hz / 100 ms, finding F8) for the scenario's duration — [scenario-player-v2x-callflow-messages.md](../../Design/MODULE-DESIGN/SCENARIO-PLAYER/scenario-player-v2x-callflow-messages.md) is the companion note behind this diagram and behind the field mapping in § 4 below.

### 4 · Correlating the exchange with the captured pcap

The same `stationId=1201` datagram as § 2, taken from `node-v2x-ecu.pcap` in the evidence collected for this document: a 58-byte UDP payload, `10.99.0.10:56287 → 10.99.0.11:47100`, captured roughly 2.3 ms before the `[EVT]` triplet in § 2 logged it — consistent with capture happening on the wire at step 1 and the `[EVT] rx_datagram` timestamp being stamped once `rx_pipeline` processes the callback, two steps later at step 3.

**Wireshark will not dissect any of this as ITS.** The CPM travels as raw UPER with no GeoNetworking/BTP envelope (finding F5), and the ITS dissector keys on that envelope to recognise a message at all — expected, not a capture defect; [testing-guide.md § 3 · Wireshark](testing-guide.md#3--wireshark) covers the same caveat for the other two ports on this wire. What follows is a manual UPER decode instead, checked field by field against the same `decode_ok` line's ground truth.

#### What the message contains

![CPM message structure: CollectivePerceptionMessage split into header (protocolVersion, messageId, stationId) and payload; payload split into managementContainer (referenceTime, referencePosition, unused segmentationInfo/messageRateRange) and cpmContainers holding containerId 1 OriginatingVehicleContainer (orientationAngle, unused pitch/roll/trailer) and containerId 5 PerceivedObjectContainer wrapping one PerceivedObject (objectId, measurementDeltaTime, position, velocity, classification, confidence); side panels show the wire path, the M1 profile summary, what the V2X ECU derives rather than receives, and why DENM is not CPM](../../Design/MODULE-DESIGN/SCENARIO-PLAYER/cpm-message-structure.svg)

Reused from [scenario-player-v2x-callflow-messages.md §4](../../Design/MODULE-DESIGN/SCENARIO-PLAYER/scenario-player-v2x-callflow-messages.md#4-cpm-message-structure), not redrawn.

| Field | Meaning | ASN.1 type | Unit / range |
|---|---|---|---|
| `stationId` | Which vehicle sent this — B's station identifier | `StationId` | `0..4294967295` |
| `referenceTime` | When B's pose below was measured | `TimestampIts` | ms since 2004-01-01 TAI |
| `referencePosition.latitude` / `.longitude` | Where B is (WGS84) | `Latitude` / `Longitude` | `10⁻⁷ °` |
| `orientationAngle` | Which way B is facing | `Wgs84AngleValue` | `0.1° · 0..3601` |
| `object.objectId` | B's own tracking ID for the object it perceived (vehicle C) | `Identifier2B` | `0..65535` |
| `object.measurementDeltaTime` | Offset between `referenceTime` and when C was actually measured | `DeltaTimeMilliSecondSigned` | `ms · ±2047` (F9) |
| `object.position.x` / `.y` | Where C is, relative to B (longitudinal / lateral) | `CartesianCoordinateLarge` | `0.01 m` |
| `object.position.xConfidence` / `.yConfidence` | How accurate B's measurement of C's position is | `CoordinateConfidence` | `0.01 m · 1..4096` |
| `object.velocity.x` / `.y` | How fast C is moving, relative to B | `VelocityComponentValue` | `0.01 m/s` |
| `object.classification` | What kind of object C is | `TrafficParticipantType` | M1 always sends `passengerCar(5)` |
| `object.classConfidence` | How sure B is of that classification | `ConfidenceLevel` | `1..101` (101 = unavailable) |

Full profile, including the mandatory-but-unpopulated ASN.1 fields (`positionConfidenceEllipse`, `altitude`, velocity `confidence`) that this project's `CpmContent` binding does not carry: [`r1-cpm-profile.md` §3](../../../contracts/r1-cpm-profile.md).

#### How far a manual decode actually gets

```
02 0e 00 00 04 b1 06 80 60 79 fc f2 11 6c d9 b5 52 d2 d5 57 ff ff ff 08 …
```

UPER packs fields as a continuous bitstream, MSB-first, with **no padding to byte boundaries** — a field's start depends on the exact bit-width of every field before it, which depends on the exact ASN.1 constraint of each. Verified against the same `decode_ok` line's values (source: `CPM-PDU-Descriptions.asn`, `v2.1.1`):

| Bits | Width | Field | Wire value |
|---|---|---|---|
| 0–7 | 8 | `header.protocolVersion` | `2` |
| 8–15 | 8 | `header.messageId` | `14` |
| 16–47 | 32 | `header.stationId` | `1201` |
| 48 | 1 | `CpmPayload`'s extension marker (`...`) | `0` — unused |
| 49 | 1 | `ManagementContainer`'s extension marker (`...`) | `0` — unused |
| 50 | 1 | `segmentationInfo` presence bit | `0` — absent |
| 51 | 1 | `messageRateRange` presence bit | `0` — absent |
| 52–93 | 42 | `referenceTime` | `1787111046972` |
| 94–124 | 31 | `referencePosition.latitude` | `210285110` |
| 125–156 | 32 | `referencePosition.longitude` | `1058048170` |

The header (bits 0–47) is byte-aligned only by coincidence — `8 + 8 + 32 = 48` happens to be a whole number of bytes. Bit 48 onward never is again. The 4-bit gap at 48–51 is not padding: it's `CpmPayload` and `ManagementContainer` each spending one bit on their ASN.1 extensibility marker (`...` in the grammar above), plus one presence bit for each of `ManagementContainer`'s two `OPTIONAL` fields — all zero here because M1 uses none of them. Skip that gap and `referenceTime` reads as `111694440435` instead of the `1787111046972` `decode_ok` actually reports — a wrong-but-plausible-looking number, indistinguishable from a correct one without cross-checking against the decoder's own ground truth.

**This is also where a reliable manual decode stops.** Past `referencePosition`, the message enters `cpmContainers` — a `SEQUENCE (SIZE 1..8) OF WrappedCpmContainer`, where each entry is `{ containerId INTEGER(1..16), containerData <the type that id identifies> }`. `containerData` is an ASN.1 *open type*: its own length is carried on the wire, and correctly stepping over it needs the exact CDD encoding rule for that construct, not just this profile's field list. Nested inside container id 5 is `PerceivedObject`, which the profile's own §2 (and § 3 above) notes carries **14 further `OPTIONAL` fields M1 never sends** — meaning a presence bitmap of a length only the full `PerceivedObject` grammar fixes sits before `objectId` even starts. And every field of interest past that point (`objectId`, position, velocity, the confidences) is small enough that searching the bitstream for its known value the way `referenceTime`/`latitude`/`longitude` were verified above stops being conclusive: the same search for `orientationAngle`'s known value (`900`) returns 5 candidate bit positions, and for `objectId`'s (`7`) returns 27 — a small integer matches too many unrelated bit windows by coincidence to trust without the exact preceding structure.

That is the real reason `objectId`, position, velocity and the confidences are read off the decoded `[EVT]` line rather than off the pcap by hand in § 2 and § 3 above — not that UPER is somehow undecodable (six fields above are decoded and verified against ground truth, crossing a non-byte-aligned boundary to do it), but that reliably locating anything inside `cpmContainers` needs the container-wrapper's open-type framing and `PerceivedObject`'s full field list from the actual ASN.1 grammar, which is exactly the job `codec/vanetza_cpm_codec` already does and logs the result of.

## Evidence collected for this document

`v2x-ecu-test-evidence/collected-20260819-104859/system-test/`, kept out of the repository (git-ignored): `node-v2x-ecu.txt` (44,544 lines), `node-bench-scenario-player.txt` (50,000 lines), and five `node-v2x-ecu*.pcap` files, one per capture rotation, collected against `m1_system_test-deploy`. The triplet in § 2 and the packet in § 4 are both drawn from this run.
