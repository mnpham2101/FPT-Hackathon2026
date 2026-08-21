# ADA-ECU final-round technical defense

Status: presentation-ready engineering study, verified against the source on 2026-08-19.

This note answers four questions likely to arise at the final: how ADA-ECU scales, what happens when only VehicleB exists, what latency means at different closing speeds, and what happens under an input storm or several simultaneous hazards. It deliberately separates **implemented**, **verified**, and **next-scale design** so the team does not claim future work as delivered behavior.

## Executive answer

| Question | Defensible answer |
|---|---|
| Can ADA-ECU accept more sensors and use cases? | **Architecturally yes, contractually not plug-and-play yet.** Inputs are normalized into one `TrackedObject` model and risk logic is behind a plugin interface. Today the frozen provenance enum contains only `own_sensor` and `v2x_relayed`; distinct camera/lidar/radar provenance needs an additive R3 contract revision or a sensor adapter that emits `own_sensor`. |
| What if there is B but no C? | B is admitted and retained. The NLOS plugin returns `low` with rationale `no_tracked_c`; it creates no C assessment and emits no R4 warning. This is already covered by deterministic unit tests. A CarSky B-only run is still useful as visual evidence. |
| Does latency remain safe at different speeds? | The recorded system run measured **101 ms from hazard-data arrival to IVI receive/display evidence**. Risk classification also uses closing rate and TTC, so it reacts earlier when relative speed is higher. However, admission gates remain fixed and no production latency SLA or speed sweep has been benchmarked yet. |
| What happens in a message storm or with hazards in many directions? | Memory is bounded: a 1,024-entry queue drops the oldest item instead of blocking producers. Admission hysteresis, confirmation, dwell and edge-triggered R4 output suppress noise. Today there is no source-aware queue fairness, drop telemetry, global top-K arbitration or direction-aware conflict ranking; the M1 plugin assesses only the nearest tracked relayed C. |

## 1. Scale-up and architecture

### Current processing shape

```text
V2X UDP R2 ─┐
            ├─ bounded InputQueue ─ parser/admission ─ TrackStore ─ CRA plugins ─ R4 UDP ─ IVI
Detector R3 ┘                                      └─ AssessmentDb + EVT evidence
```

- V2X and detector readers are independent producers; the main thread is the single writer to tracking, assessment and output state.
- Both paths become the same typed `TrackedObject`; risk plugins do not depend on OpenCV, YOLO, UDP or a specific sensor SDK.
- `TrackStore` is keyed by object id. `AssessmentDb` is keyed by `(trackId, warningType)`, so several risk plugins can assess the same track independently.
- `ICollisionRiskAssessment` is the extension seam for another use case. `FrameSource` is the detector seam for file, synthetic stream or a future live camera.

### SOLID assessment

| Principle | Current evidence | Verdict |
|---|---|---|
| Single responsibility | Transport, parsing, admission, store, fusion, assessment and output are separate modules. | Strong |
| Open/closed | A risk case is added as a plugin plus explicit registry entry; frame acquisition has implementations behind an interface. | Strong at the two intended seams |
| Liskov substitution | CRA and frame-source implementations are invoked through small base interfaces and have substitute/fake tests. | Satisfied within current implementations |
| Interface segregation | Interfaces expose only assessment context or frame acquisition, not a “god object” API. | Strong |
| Dependency inversion | Risk logic depends on the CRA abstraction and typed context; the controller wires concrete components. `RiskContext` still exposes concrete `TrackStore` and `AssessmentDb`. | Aligned, not fully abstracted |

The accurate presentation phrase is **“SOLID-aligned modular design”**, not “perfect SOLID”. The system has clear extension seams, while persistence and store access remain concrete by deliberate M1 scope.

### “Uniform database” — what exists today

There are two uniform internal models:

1. `TrackStore`: the latest normalized object records, irrespective of whether they came from own sensing or relayed V2X.
2. `AssessmentDb`: typed risk state per `(trackId, warningType)`, mirrored to `[EVT]` JSONL so a run can be reconstructed offline.

`AssessmentDb` is an **in-process table, not an external database and not persistent across restart**. A scale-up path is to place a repository interface in front of it and retain the current memory implementation for deterministic real-time operation, while exporting events to a time-series/event store for analytics. A database must not be placed synchronously in the safety-critical tick path.

### Adding camera, lidar, radar or another V2X case

| Addition | Reusable unchanged | Required work |
|---|---|---|
| New camera/model | detector tracking, R3 parser, admission, store, CRA, R4 | implement/configure a `FrameSource` or inference backend that emits valid R3 |
| Lidar/radar normalized as ego sensing | admission, store, CRA, output | adapter converts sensor coordinates, timestamp, confidence and id into `TrackedObject`; it may temporarily use `own_sensor` |
| Distinct sensor provenance | downstream architecture | version R3 source/provenance fields and update schema bindings in every producer/consumer |
| New hazard type | input/store/output framework | implement one CRA plugin, register it and define its warning contract/IVI presentation |
| Intersection or multi-direction case | plugin framework | world-frame transform, ego pose/heading, trajectory prediction and conflict-point association; the current longitudinal sum is not sufficient |

The present `scene_composer` assumes the near-collinear A–B–C convoy. That assumption must be replaced by a common coordinate frame and time-aligned object fusion before claiming 360-degree use cases.

## 2. VehicleB exists and VehicleC does not

### Implemented behavior

1. Detector R3 observations for B pass through normal admission: `not_tracked → tentative → tracked` after `CONFIRM_HITS=3` in-gate updates.
2. The NLOS plugin searches for a `tracked` `v2x_relayed` C.
3. If none exists, it returns `riskState=low`, `rationale=no_tracked_c`, and no trigger.
4. Because the committed state remains low, the controller's edge detector emits neither `risk_transition` nor `r4_tx`.
5. IVI therefore receives no false NLOS warning. B can remain visible in ADA tracking evidence.

### Existing deterministic evidence

| Test | Assertion |
|---|---|
| `ChainedCollisionBandTable.NoTrackedCReturnsLowWithoutDbWrites` | B-only returns low, rationale `no_tracked_c`, no trigger, no assessment row/event |
| `SceneComposer.NullCBeforeCIsFirstAdmitted` | geometry contains numeric B and nullable C |
| input/admission tests | own-sensor observations still promote B independently of V2X traffic |

Recommended final-round CarSky evidence:

```text
Profile: detector enabled; Bench/V2X C sender disabled
Duration: at least TRACK_TIMEOUT_MS + 3 detector observations + 2 s
PASS: own_sensor_ingest > 0
PASS: own: track_transition to tracked exists
PASS: v2x_relayed tracked transitions = 0
PASS: r4_tx = 0 and IVI shows no NLOS warning
```

This is a **negative safety test**: no output is the correct output. Capture the ADA counters/log and IVI idle screen together so “nothing happened” is measurable rather than anecdotal.

## 3. Latency and different speeds

### What has been measured

The recorded end-to-end system evidence reports:

| Segment | Recorded result |
|---|---:|
| V2X decode and forward | under 1 ms |
| Hazard message arrival → IVI receive/display evidence | **101 ms** |

That 101 ms is one observed run, not yet a percentile SLA. The next study must report at least sample count, p50, p95, p99 and maximum under normal and storm load.

### Where response time comes from

For a newly appearing object, response time is approximately:

```text
source sampling + network + queue wait + confirmation + next fusion tick
+ risk dwell + R4 network + IVI parse/render
```

Current timing controls are `FUSION_TICK_MS=100`, `RISK_DWELL_MS=300`, `CONFIRM_HITS=3`, and `TRACK_TIMEOUT_MS=1000`. The 300 ms dwell intentionally trades a little reaction time for stability. A previously tracked hazard avoids the cold admission cost; a new track needs three valid observations.

### Closing-speed sensitivity

The implemented risk rule is already partly speed-aware because it derives closing rate from the change in composed A–C distance and calculates TTC. With default thresholds:

| Relative closing speed | TTC at 60 m | TTC at 30 m | Current interpretation |
|---:|---:|---:|---|
| 5 m/s (18 km/h) | 12 s | 6 s | medium at 60 m by range; high at 30 m by range |
| 10 m/s (36 km/h) | 6 s | 3 s | medium/high boundary aligns with TTC thresholds |
| 20 m/s (72 km/h) | 3 s | 1.5 s | high can trigger at 60 m by TTC |
| 30 m/s (108 km/h) | 2 s | 1 s | high by TTC before the 30 m range boundary |

These are deterministic `distance / relative-speed` examples, not braking-distance claims. The node currently lacks ego CAN/GNSS speed and road-friction/braking inputs. Therefore:

- implemented: range + relative closing-rate/TTC classification;
- not implemented: speed-scaled admission gates, braking-envelope model, road-condition model;
- risk to test: noisy distance differentiation can make TTC jitter; dwell limits flicker but cannot replace sensor uncertainty modelling.

Recommended speed sweep: replay identical geometry at closing speeds 0, 5, 10, 20 and 30 m/s; assert first committed risk, trigger time, output latency and no regression in provenance. Repeat at least 30 cycles per profile and publish p50/p95/p99/max.

## 4. Message storm and simultaneous hazards

### Protections already implemented

| Protection | Effect |
|---|---|
| bounded queue, capacity 1,024 | memory cannot grow without bound |
| drop-oldest, non-blocking producers | newest observations survive overload; receiver threads do not deadlock |
| one writer | deterministic store/assessment state, no data races in core state |
| confirmation + hysteresis + timeout | rejects one-frame noise and track-boundary chatter |
| risk dwell | suppresses rapid band flapping |
| edge-triggered R4 | steady medium/high does not produce one warning every 100 ms tick |
| malformed-message rejection | bad traffic is counted/rejected rather than terminating the node |

The concurrent queue tests prove accounting (`delivered + dropped = pushed`) and FIFO order per producer. They do not establish a production throughput limit.

### Current limits under “4 phương 8 hướng”

- Queue shedding is global, not source-aware. A camera burst can evict V2X messages, or vice versa.
- Queue drops are test-readable but are not yet a first-class runtime event/counter in the presentation evidence.
- Duplicate updates are not coalesced in the queue; only the later store upsert replaces a track by id.
- Every enabled plugin runs sequentially on each tick. Per-tick work is approximately `O(P × N)` for `P` plugins and `N` tracks, plus map operations.
- The M1 NLOS plugin evaluates only the nearest tracked relayed C. That is a simple filter for the convoy, not a general priority model. It can ignore a farther object with a more dangerous trajectory.
- There is no global arbiter that ranks findings across warning types, directions and severities; no top-K output budget and no per-source fairness.

### Scale-up design

```text
per-source bounded ingress
        ↓
validate + deduplicate/coalesce by (source, object-id, time-window)
        ↓
time-align + transform to a common world/ego frame
        ↓
spatial index / conflict-zone association
        ↓
parallel or budgeted CRA evaluation
        ↓
global arbiter: severity → TTC → confidence → freshness
        ↓
top-K + per-hazard cooldown/rate limit → IVI
```

Recommended deterministic priority tuple:

```text
(risk severity descending,
 TTC ascending with null last,
 confidence descending,
 observation age ascending,
 object id ascending for stable ties)
```

Recommended storm acceptance profiles:

| Profile | Input | Pass criteria |
|---|---|---|
| normal | expected camera rate + 10 Hz V2X | zero drops; p99 output latency within agreed budget |
| burst | 10× expected rate for 5 s | memory bounded; process alive; drop metric increases; latest critical object survives |
| sustained overload | 2× capacity for 60 s | bounded RSS; no deadlock; documented degradation and recovery time |
| multi-hazard | at least 16 objects from four directions | deterministic top-K ordering; no warning flood; highest-risk conflict always selected |
| adversarial source | one source floods invalid/duplicate messages | other source retains reserved capacity; rejects and drops observable |

## Presentation wording

Use this 45-second answer:

> “ADA-ECU is built around one normalized object model, a single-writer track store and pluggable risk-assessment modules. That lets us reuse admission, evidence and IVI output when we add a sensor or a hazard case. Today we have proven two sources and one near-collinear NLOS case; distinct lidar/radar provenance and 360-degree association are the next contract and world-model step. If only B exists, B is tracked but no C assessment or warning is emitted. Our recorded hazard-to-IVI latency is 101 ms, while the risk rule also uses TTC so higher closing speed escalates earlier. Under load, the queue is bounded and warnings are debounced and edge-triggered. For production scale, we add per-source fairness, drop telemetry, coalescing and a global severity/TTC top-K arbiter.”

## Source anchors

- Architecture authority: `documents/Design/MODULE-DESIGN/ADA-ECU/ada-ecu-hld.md`
- Source enum/model: `ADA_ECU/src/contracts/tracked_object.hpp`
- Bounded queue: `ADA_ECU/src/observer/input_queue.hpp`
- Single-writer loop and edge-triggered output: `ADA_ECU/src/main.cpp`
- Plugin seam/registry: `ADA_ECU/src/cra/i_collision_risk_assessment.hpp`, `registry.*`, `builtin_plugins.cpp`
- Current NLOS/TTC behavior: `ADA_ECU/src/cra/plugins/chained_collision.cpp`
- Uniform assessment table: `ADA_ECU/src/cra/assessment_db.hpp`
- B-only tests: `ADA_ECU/tests/cra/test_chained_collision.cpp`, `ADA_ECU/tests/fusion/test_scene_composer.cpp`
- Queue overload tests: `ADA_ECU/tests/observer/test_input_queue.cpp`
- Recorded timeline: `documents/Delivery/Acceptance/m1-delivery-timeline.svg`

