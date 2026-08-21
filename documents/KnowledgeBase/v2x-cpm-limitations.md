# The limits of V2X CPM for cooperative-awareness warnings

CPM (Collective Perception Message, ETSI TS 103 324) is the message this project relies on to let vehicle A learn about vehicle C without ever seeing it — B perceives C and broadcasts it, A receives and warns. CPM makes that relay possible, but it does not make the warning decision itself correct, timely, or attributable to the right vehicle. Four gaps below are protocol-level limits, not defects specific to this codebase, and this project's own implementation illustrates each one concretely.

## 1. CPM standardizes fields, not risk policy

ETSI TS 103 324 defines what a Perceived Object Container carries — position, classification, confidence, kinematics — and nothing that scores how dangerous an object is. Standardization "has focused on the transmitting side and left the receiving side to be implementation specific" (ETSI TR 103 439): the wire format is fixed, the decision of what to do with it is not.

This project's own contract mirrors that split exactly. The R1 CPM profile and the R2 V2X→ADA object message ([contracts/r1-cpm-profile.md](../../contracts/r1-cpm-profile.md), [contracts/samples/r2-object.json](../../contracts/samples/r2-object.json)) carry only `id`, `class`, `source`, `position`, `distance`, `speed`, `confidence`, `state`, `timestamps` — no risk field anywhere on the wire. `riskState` is computed entirely downstream, by ADA-ECU's own ordered threshold table ([ada-ecu-design-decisions.md D5](../Design/MODULE-DESIGN/ADA-ECU/ada-ecu-design-decisions.md), implemented in `ADA_ECU/src/cra/plugins/chained_collision.cpp:89-103`).

Because no standard hands down a threshold, every deployment invents its own — and there is nothing to converge on: real forward-collision-warning systems use time-based or speed-scaled thresholds rather than a fixed distance, because a gap that is safe at 30 km/h is not safe at 120 km/h. Road rules, driving culture and typical speeds all vary by region too, so "how close is too close" has no universal answer.

This project's own threshold (`RISK_NEAR_M=60`, `RISK_CRITICAL_M=30`) does trace back to a real study, done at proposal stage — vehicle speed, expected stopping distance, and expected driver response for a vehicle over 75 km/h ([documents/Proposals/README.md](../Proposals/README.md), lines 43 and 57). But that derivation is never referenced by the later, more detailed design-decision record that also justifies the same two numbers — `ada-ecu-design-decisions.md` D11 and the R22 run-choreography study ([requirements/deprecated/m1-run-timing-and-event-triggering.md §6.6](../../requirements/deprecated/m1-run-timing-and-event-triggering.md)) pick `60`/`30` purely because they satisfy the demo's timing window against the bench's fixed convoy geometry, with no mention of the 75 km/h stopping-distance origin. The two rationales land on the same numbers, but nothing in the repository connects them — a documentation-continuity gap, not a computation error.

## 2. Fixed thresholds, not scaled by speed

Even where a project picks a principled threshold, a single fixed distance stops being appropriate the moment closing speed changes. This is why real forward-collision-warning systems key off time-to-collision or a speed-scaled distance rather than a constant.

This project's threshold is fixed regardless of ego speed — speed-scaled admission/risk criteria are explicitly registered future work, not yet built ([m1-future-features-register.md](../Requirements/future/m1-future-features-register.md)).

## 3. Broadcast-storm and congestion control

CPM emission at scale — every ITS station broadcasting its own perceived objects — causes real channel congestion, which is why ETSI mandates Decentralized Congestion Control (DCC) as part of the ITS-G5 stack: transmitters throttle their own send rate based on measured Channel Busy Rate rather than sending on a fixed schedule regardless of load.

This project has no such mechanism. V2X-ECU's dedup stage only collapses literal repeat messages from the same station/object/measurement-time within a time window (`V2X_ECU/src/pipeline/deduper.cpp`) — that is deduplication, not congestion control. R1 emission runs at a flat, unconditional rate with no adaptive throttling. Both single-message aggregation and broadcast-storm mitigation are explicitly listed as deferred ([m1-future-features-register.md](../Requirements/future/m1-future-features-register.md)).

## 4. Sender identity is not the same as lane relevance

CPM's station ID uniquely identifies which station sent a given message — that solves *attribution*. It does not solve *relevance*: nothing in the protocol says that the station sending a CPM is the specific vehicle directly ahead of you, as opposed to some other nearby vehicle in an adjacent or oncoming lane that also happens to be broadcasting. A station ID is typically an opaque, privacy-rotated identifier; it carries no positional or lane semantics. Resolving "is this sender actually the vehicle I'm tracking" requires an application-layer association step — matching the CPM's own reference position, or the relayed object's geometry, against what the ego vehicle already perceives directly. This is an open, actively studied problem in cooperative perception, not a solved one (see e.g. track-to-track association research for collective perception).

This project's own code shows the gap directly: B (the own-sensor track) and C (the v2x-relayed track) are each picked independently by nearest distance, with no cross-check between them —

- B: `store.nearest(Source::own_sensor)`, `ADA_ECU/src/fusion/scene_composer.cpp:11-12`
- C: nearest `tracked` `v2x_relayed` track, `ADA_ECU/src/cra/plugins/chained_collision.cpp:18-28`

`ADA_ECU/src/parser/r2_parser.cpp:59` stamps every relayed object `v2x:<stationId>:<objectId>` and admits it through the R13 distance gate with no plausibility or identity check against B. The ambiguity does not surface in the M1 demo only because the bench simulates exactly one broadcasting station — `Scenario_Player/scenarios/default.yaml:19` and `c-out-of-range.yaml:14` both hardcode a single `station_id: 1201`. With more than one nearby broadcaster, there is currently no mechanism to tell which CPM is geometrically relevant.

## Related

- The wire fields these limits sit on top of: [contracts/r1-cpm-profile.md](../../contracts/r1-cpm-profile.md), [contracts/r2-v2x-object.schema.json](../../ADA_ECU/contracts/r2-v2x-object.schema.json)
- The risk-threshold decision these limits bear on: [ada-ecu-design-decisions.md](../Design/MODULE-DESIGN/ADA-ECU/ada-ecu-design-decisions.md) D5, D11
