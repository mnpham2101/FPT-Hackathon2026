# R1 CPM Profile — M1 Cooperative Awareness (v1)

> **Normative status:** the frozen R1 contract. Profile version **1** (matches R2 `schemaVersion: 1`); frozen 2026-07-31 by Phase 0 subtask 1.0.1.1 — changes require a re-freeze across every consumer. Authority: [m1-cooperative-awareness.md §2 R1](../documents/Requirements/m1-cooperative-awareness.md); design: [phase0-contract-freeze-hld.md §4, §7](../deprecated/phase0-contract-freeze-hld.md); field research: [scenario-player-v2x-callflow-messages.md §4](../documents/Design/SCENARIO-PLAYER/scenario-player-v2x-callflow-messages.md).

## 1. Scope & wire format

- One V2X message type in M1: **ETSI CPM, TS 103 324 v2.1.1** (release 2). No CAM, no DENM.
- Wire format (**F5**): one raw **UPER-encoded `CollectivePerceptionMessage`** per UDP datagram — no GeoNetworking/BTP envelope (the GN/BTP stack ships in the modem, out of scope project-wide).
- Codec (**F2**): `vanetza::asn1::r2::Cpm` only; the bare `asn1::Cpm` alias (release 1, wrong wire format) is banned under `V2X_ECU/src/` — enforced by the 1.0.7.1 integrity gate.
- Emission rate (**F8**): default 10 Hz via the `cpm_rate_hz` scenario config key — never a code literal.

## 2. Message structure

Exactly 2 containers, exactly 1 perceived object (C) per message:

```
CollectivePerceptionMessage
├─ header : ItsPduHeader              protocolVersion=2 · messageId=cpm(14) · stationId=B
└─ payload : CpmPayload
   ├─ managementContainer             referenceTime · referencePosition
   └─ cpmContainers (SIZE 1..8)
      ├─ [id=1] OriginatingVehicleContainer    orientationAngle
      └─ [id=5] PerceivedObjectContainer       numberOfPerceivedObjects=1 · perceivedObjects[0] → C
```

Containers 2 (RSU), 3 (SensorInformation), 4 (PerceptionRegion) are unused; `PerceivedObject`'s 14 further OPTIONAL fields are unsent — every one a future extension point needing no profile change.

## 3. Field table

CpmContent field names (left column) are the normative JSON names mirrored by [r1-cpm-content.schema.json](r1-cpm-content.schema.json).

| CpmContent field | ASN.1 path | ASN.1 type | Unit / range | Nominal sample |
|---|---|---|---|---|
| `stationId` | `header.stationId` | `StationId` | 0..4294967295 | `1201` |
| `referenceTime` | `payload.managementContainer.referenceTime` | `TimestampIts` | ms since 2004-01-01T00:00:00.000 TAI · 0..4398046511103 | `716084805123` |
| `referencePosition.latitude` | `…managementContainer.referencePosition.latitude` | `Latitude` | 10⁻⁷ ° · −900000000..900000001 (900000001 = unavailable) | `210285110` |
| `referencePosition.longitude` | `…managementContainer.referencePosition.longitude` | `Longitude` | 10⁻⁷ ° · −1800000000..1800000001 (1800000001 = unavailable) | `1058048170` |
| `orientationAngle` | `…cpmContainers[id=1].orientationAngle.value` | `Wgs84AngleValue` | 0,1 ° · 0..3601 (3601 = unavailable) | `900` |
| `object.objectId` | `…cpmContainers[id=5].perceivedObjects[0].objectId` | `Identifier2B` | 0..65535 | `7` |
| `object.measurementDeltaTime` | `….measurementDeltaTime` | `DeltaTimeMilliSecondSigned` | ms · wire −2048..2047, profile ±2047 (F9) | `-50` |
| `object.position.x` / `.y` | `….position.xCoordinate.value` / `.yCoordinate.value` | `CartesianCoordinateLarge` | 0,01 m · −131072..131071 · sender-B frame | `2500` / `120` |
| `object.position.xConfidence` / `.yConfidence` | `….position.{x,y}Coordinate.confidence` | `CoordinateConfidence` | 0,01 m · 1..4096 (4096 = unavailable) | `90` / `90` |
| `object.velocity.x` / `.y` | `….velocity.cartesianVelocity.{x,y}Velocity.value` | `VelocityComponentValue` | 0,01 m/s · −16383..16383 · sender-B frame | `1520` / `0` |
| `object.classification` | `….classification[0].objectClass.vehicleSubClass` | `TrafficParticipantType` | 0..255 · M1: `passengerCar(5)` | `5` |
| `object.classConfidence` | `….classification[0].confidence` | `ConfidenceLevel` | 1..101 (101 = unavailable → R2 `null`, F6) | `95` |

Encoding note: mandatory ASN.1 fields not carried in CpmContent (`referencePosition.positionConfidenceEllipse`, `referencePosition.altitude`, velocity `confidence` (`SpeedConfidence`), `header.protocolVersion=2`, `header.messageId=cpm(14)`, `numberOfPerceivedObjects=1`) are encoded with their CDD "unavailable"/fixed named values and ignored on decode; the exact numerics are fixed by the codec implementation (1.0.2.3) and locked by the golden vectors.

## 4. CpmContent — the codec-seam logical model

- `CpmContent` (C++ struct in `V2X_ECU/src/codec/cpm_codec.hpp`, subtask 1.0.2.2; Python dataclass in `Scenario_Player/player/contracts/cpm_content.py`, subtask 1.0.5.1) carries exactly the §3 fields in **wire-native integer units** — no SI floats, no derived fields.
- Seam rule (HLD D3): the codec is a pure representation transform CpmContent ⇄ UPER; unit conversion (F6), `distance` derivation (F7), and sender-speed derivation (F1) happen above the seam, in R9.

Nominal CpmContent JSON (the `nominal` golden vector, F7-consistent with the shared R2 sample):

```json
{
  "stationId": 1201,
  "referenceTime": 716084805123,
  "referencePosition": { "latitude": 210285110, "longitude": 1058048170 },
  "orientationAngle": 900,
  "object": {
    "objectId": 7,
    "measurementDeltaTime": -50,
    "position": { "x": 2500, "y": 120, "xConfidence": 90, "yConfidence": 90 },
    "velocity": { "x": 1520, "y": 0 },
    "classification": 5,
    "classConfidence": 95
  }
}
```

## 5. Frozen conventions (8)

| # | Convention |
|---|---|
| F1 | CPM r2 carries no sender speed → R2 `sender.speed` is nullable: derived at the V2X ECU from consecutive `referenceTime`/`referencePosition` deltas, `null` until two CPMs from that station have arrived. |
| F2 | `vanetza::asn1::r2::Cpm` only; bare `asn1::Cpm` banned under `V2X_ECU/src/`. |
| F5 | Raw UPER `CollectivePerceptionMessage`, one PDU per UDP datagram, no GN/BTP. |
| F6 | Confidence conversions in R9: `r2.object.confidence = ConfidenceLevel / 100` clamped to [0,1], `101 → null`; `r2.object.position.confidence = CoordinateConfidence × 0,01` metres — an accuracy, not a probability. |
| F7 | `R2 object.distance = hypot(object.position.x, object.position.y)` in SI metres — derived in R9, never transmitted in the CPM. |
| F8 | Default message rate 10 Hz via `cpm_rate_hz` scenario config — never a literal. |
| F9 | `measurementDeltaTime` valid range ±2047 ms: the bench validates before encode; R9 rejects violations and counts them (−2048 is wire-encodable but profile-invalid). |
| VF | `PerceivedObject.position` and `.velocity` are expressed in the sender (B) cartesian frame — both ends use this one convention. |

## 6. Exchange call flow

- The sole live flow: **B (bench, R11) → A's V2X ECU** — B detects C → encodes a CPM per this profile → sends one UDP datagram (F5) to the V2X ECU (`10.99.0.11:47100` on the R6 bridge) → R9 decodes through the seam, validates (F9), converts (F6), derives (F1, F7) → emits R2 to the ADA ECU.
- Unidirectional: R10 ego-Tx is deferred (decision 2026-07-30) — no reply, no ack, no handshake, no in-band auth. Diagrams: [callflow note §2](../documents/Design/SCENARIO-PLAYER/scenario-player-v2x-callflow-messages.md).

## 7. Golden-vector corpus (6 cases)

`contracts/golden-vectors/<case>.json` + `<case>.uper` pairs, generated by `gv_tool` (subtask 1.0.2.4), each asserted encode/decode-identical. Every case is the §4 nominal CpmContent with only the stated delta:

| Case | Delta vs nominal | Exercises |
|---|---|---|
| `nominal` | none | the committed happy path |
| `mdt-max` | `object.measurementDeltaTime = 2047` | F9 upper bound |
| `mdt-min` | `object.measurementDeltaTime = -2047` | F9 lower bound (profile bound, not wire −2048) |
| `conf-unavailable` | `object.classConfidence = 101` | F6 `101 → null` |
| `gate-boundary` | `object.position.x = 3000`, `object.position.y = 0` | R13 admission seam: derived distance exactly 30,00 m |
| `coord-large` | `object.position.x = 131071`, `object.position.y = -131072` | `CartesianCoordinateLarge` bounds; feeds the O3 MTU/size budget |

## 8. Erratum flags (carried, not absorbed)

| Report text | Profile value | Owner |
|---|---|---|
| R2 sample `object.distance: 25.4` | `25.03 = hypot(25.0, 1.2)` (F7) | [[project-researcher]] patches the report |
| R2 `sender.speed` implied decoded from the CPM | no CPM r2 source field — nullable + derived (F1) | [[project-researcher]] patches the report |

---

*Frozen by subtask 1.0.1.1 (Phase 0). Consumers: bench encoder (R11), V2X decoder (R9), golden vectors (1.0.2.4), CpmContent bindings (1.0.2.2, 1.0.5.1).*
