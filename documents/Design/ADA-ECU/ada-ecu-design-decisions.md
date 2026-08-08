# ADA ECU — design decisions

> The decision record for [ada-ecu-hld.md](ada-ecu-hld.md), which cites these by number. Binding on implementation: a decision is revisited by changing its entry here, never by an implementation that departs from it.

## D1 — One folder, one image, one object model

`ADA_ECU/` is this node's only build context, its only image and its only design authority. Everything the node ships is built from this folder, and the folder reads nothing outside it ([node-code-layout § Build rules](../../../.claude/rules/node-code-layout.md#build-rules-all-container-nodes)).

- **`src/contracts/` is the node's only object model.** `TrackedObject`, `Source`, `TrackState`, `R2Message` and `R4WarningEvent` are declared once, in the bindings written against the frozen schemas. A second declaration of any of them — a node-local `types.hpp`, a detector-side redefinition, a tool's private field list — is a defect. Two models for one message means two answers to what a field is called, and the wire settles the argument at run time.
- **`contracts/` holds byte-synced copies of the frozen schemas and nothing else.** A node-local fork — tightening `additionalProperties`, promoting a field to required, adding an array the producer never emits — is a second, unversioned contract that keeps passing after the real one changes.
- **Node-local schemas that are not cross-node contracts live in `schema/`**, which the sync manifest does not govern. The R14 assessment record is the one such file (D4).
- Rejected alternative: per-module private structs mapped at each boundary. It buys decoupling between modules of one process, at the cost of reintroducing exactly the drift the frozen contracts exist to prevent.

## D2 — Process, thread and mock model

- **One image, two processes** (report §3(d)): the C++17 `ada_ecu` core and the Python 3.11 detector, joined only by argv, exit codes and R3 JSONL over stdout. No FFI, no RPC, no shared file.
- **Three threads in the core.** A V2X receive thread blocks on `V2X_LISTEN_PORT`; a detector-reader thread reads lines from the subprocess stdout pipe; the main thread runs the fusion tick. Both readers push onto one bounded queue, and **the main thread is the single writer** of the store, the assessment database and the R4 output. No lock is needed beyond the queue, event ordering is deterministic, and the `[EVT]` stream replays a run exactly as it happened.
- **The fusion tick runs every `FUSION_TICK_MS` whether or not anything arrived.** A track must be able to expire, and risk to fall, on silence alone.
- **Detector lifecycle.** A clean EOF with `DETECTOR_LOOP=true` respawns the clip from frame 0, so B stays present for a run longer than the clip. A non-zero exit triggers a logged restart, bounded by `DETECTOR_RESTART_MAX`. Replaying the same detection path synthesizes no ego data — every frame is still inferred.
- **Mocks live outside `src/`.** A recorded own-sensor stream reaches the core through the real reader (`DETECTOR_CMD="cat /app/tests/fixtures/own_sensor_mock.jsonl"`); relayed traffic reaches it as real datagrams on the real socket. **There is no mock branch anywhere in the core**, and "no input yields no tracks" is `DETECTOR_ENABLED=false` plus no traffic.

## D3 — R13 admission: one state machine, both sources

Realizes [vehicleC_track_admission_state_machine.png](../../../requirements/vehicleC_track_admission_state_machine.png) as [phase2-4-ada-ecu-admission.puml](phase2-4-ada-ecu-admission.puml). The diagram is source-agnostic, so `store/admission` holds **one** implementation, parameterized only by what counts as an update.

| Term | Definition fixed here |
|---|---|
| update | one admission-relevant observation for a track id — for `v2x_relayed` one R2 message and its `object.distance`, for `own_sensor` one detector JSONL line and its `distance` |
| `not_tracked` | **absent from the store**, per the diagram's "absent from store". A drop erases the entry; it never leaves one flagged `not_tracked` |
| hysteresis | `distance ≤ GATE_ENTER_M` admits and holds a `tentative` track; only once `tracked` does the wider `distance ≤ GATE_EXIT_M` hold it. One Schmitt band, exactly the diagram's `≤ 30 m` / `> 35 m` labels |
| N — `CONFIRM_HITS` | consecutive in-gate updates required in `tentative` before promotion; the counter resets to 0 on every entry to `not_tracked` |
| M — `TRACK_TIMEOUT_MS` | silence measured on `CLOCK_MONOTONIC` (D10), not a count of missed messages |

**M is a timeout, not a count of consecutive misses.** Two reasons, both structural:

- "Its messages stop" — R13's own wording — is a time condition. A count cannot express silence, because nothing arrives to increment it.
- The two sources run at independently configured cadences: relayed updates at the bench's `cpm_rate_hz`, own-sensor updates at the detector's paced rate. One count M would mean two different real timeouts, and retuning *another node's* config would silently change this node's behaviour.

`TRACK_TIMEOUT_MS = 1000` is the time form of [milestone1_high_level_plan.md §4](../../Plan-Proposal/milestone1_high_level_plan.md#track-admission-gate-r13)'s M = 5, taken at the slower of the two sources — 5 × 200 ms — so neither source expires early.

Further rules:

- **The store is the sole writer of `state`.** The `state` field on an incoming detector line or an R2-derived object is ignored; the detector emits `not_tracked` by convention because it holds no store.
- Track ids are `v2x:<stationId>:<objectId>` for relayed entries and `own:<n>` for own-sensor entries, assigned by the detector's frame-to-frame association (D6). Neither namespace can be minted by the other side.
- Every edge writes one `track_transition` event carrying id, source, from, to, distance and reason.
- A track's last snapshot survives its erasure in the assessment database (D4), never in the store — that is what lets a cleared warning still carry an R3 `object`.

## D4 — R14: the Collision Risk Assessment interface, registry and database

Realizes the «interface» block of [ada-ecu.svg](../../../requirements/ada-ecu.svg). Naming reconciliation, stated once: the figure's realization **Chained Collision** is the report's "M1 NLOS plugin" and registers under the frozen R4 registry key **`nlos_obstruction`** — one concept, three existing names, no new term coined.

```cpp
// src/cra/i_collision_risk_assessment.hpp
struct RiskContext { const TrackStore& store; AssessmentDb& db; std::int64_t now_ms; };
struct RiskFinding {
  std::string warningType;                        // R4 registry key == plugin name
  std::string riskState;                          // low | medium | high (frozen R4)
  std::optional<contracts::TrackedObject> trigger;// R3 snapshot for the R4 `object` field
  std::string rationale;                          // evidence text for the database and the [EVT] stream
};
class ICollisionRiskAssessment {
 public:
  virtual ~ICollisionRiskAssessment() = default;
  virtual std::string name() const = 0;           // registry key
  virtual RiskFinding assess(RiskContext&) = 0;   // reads the store, reads and writes the database
};
```

- **Registration is explicit, not static-init.** `src/cra/builtin_plugins.cpp` holds one `registry.add(std::make_unique<ChainedCollision>(cfg))` line per plugin, and `main.cpp` enables the subset named by `CRA_ENABLED`. Adding a future plugin — intersection hazard, curve blind spot, priority-vehicle preemption, speed-scaled risk — is one new file plus one line in that file, with no edit to the interface, the store, the emitter or any existing plugin. That is R14's acceptance. Self-registering static registrars were rejected: in a static library the linker discards the unreferenced objects unless the whole archive is force-linked, which is a build-system trap.
- **The plugin never emits.** It returns a `RiskFinding` and the output stage decides transport, which keeps rules replaceable independently of emission.

**The database is an append-and-upsert record table keyed by track id**, defined by a committed JSON Schema and held in process, with every write also appended to the `[EVT]` stream so the table is reconstructible offline. R14's acceptance names the schema the assessment reads and writes as a committed artifact; this is that artifact.

- Schema: `ADA_ECU/schema/cra-assessment-record.schema.json` — node-local, deliberately outside `contracts/` (D1).
- Accessor: `src/cra/assessment_db.{hpp,cpp}` — typed `get(trackId)` / `upsert(record)` / `erase(trackId)`, the seam a later milestone swaps for real persistence.

| Record field | Meaning |
|---|---|
| `trackId`, `warningType` | the key: which track, assessed by which plugin |
| `riskState`, `riskStateEnteredMs` | the last committed level and when it was entered — the edge-trigger and dwell input |
| `firstSeenMs`, `lastUpdatedMs` | the assessment's lifetime |
| `distanceM`, `previousDistanceM` | the last and prior composed `d_AC` — the closing-rate input |
| `closingRateMps`, `ttcS` | derived; `ttcS` is null when the track is not closing |
| `lastSnapshot` | the R3 snapshot of the track, carried past its erasure so a cleared warning still has an `object` |
| `lastKnownB` | the last own-sensor B position, so a clear can still fill the required `geometry.vehicleB` |
| `emittedCount`, `rationale` | evidence: how many R4 events this track produced, and why the last level was chosen |

Rejected alternative: SQLite. It adds a dependency and a file lifecycle to a Container Node that has no volume, and buys nothing over a JSONL append that also *leaves* the node through its only egress.

## D5 — Risk vocabulary and edge-triggered emission

Frozen R4 fixes `riskState ∈ {low, medium, high}`. For `nlos_obstruction`, with ego speed unknown in M1 — no GNSS and no Cortex-M ECU, report §1 — the inputs are the composed range `d_AC`, its change over time from the database, and C's relayed speed.

**The band table is total and ordered.** The first row whose condition holds wins, so no state matches two rows and none matches none.

| # | `riskState` | Meaning for the chained-collision plugin | Condition |
|---|---|---|---|
| 1 | `high` | C is tracked and imminent | C `tracked` and (`d_AC ≤ RISK_CRITICAL_M` **or** `ttc ≤ RISK_TTC_CRITICAL_S`) |
| 2 | `medium` | C is tracked and relevant — inside the awareness band, or closing fast enough to matter at any range | C `tracked` and (`d_AC ≤ RISK_NEAR_M` **or** `ttc ≤ RISK_TTC_WARN_S`) |
| 3 | `low` | C is not a present threat: unknown, outside the tracking gate, or tracked but neither near nor closing fast | every other state — no `tracked` C, `b_unknown`, or C `tracked` with `d_AC > RISK_NEAR_M` and (`ttc` null or `ttc > RISK_TTC_WARN_S`) |

- `RISK_TTC_WARN_S` is the second clause of row 2, and that is its whole effect: a track closing fast enough is `medium` before its range reaches `RISK_NEAR_M`.
- `closingRateMps = -(d_AC(t) − d_AC(t−Δ)) / Δ`; `ttcS = d_AC / closingRate` when the rate is positive, and null otherwise.
- **Risk thresholds are separate constants from the R13 gate and never alias it.** Aliasing them collapses R14 into R13 and makes track identity and alarm level indistinguishable.
- **The bands and the gate measure different quantities, so no ordering holds between them.** `RISK_NEAR_M` and `RISK_CRITICAL_M` are thresholds on the composed range `d_AC = d_AB + d_BC`. `GATE_ENTER_M` is a threshold on the relayed range `d_BC` alone. C is admitted at `d_BC ≈ GATE_ENTER_M`, so `d_AC` at that instant is `d_AB` above the gate value, and 40 m or more for every `d_AB` the ego clip produces. A `RISK_NEAR_M` at or below `GATE_ENTER_M` therefore sits below every composed range the node can observe, and row 2's range clause never fires. The orderings the loader asserts are the ones lying within a single quantity ([HLD §6](ada-ecu-hld.md#6-internal-components)).
- Rejected alternative: asserting `RISK_CRITICAL_M < RISK_NEAR_M < GATE_ENTER_M` at startup, with `RISK_NEAR_M` 25 and `RISK_CRITICAL_M` 15. Its right-hand comparison relates two different quantities. The values it admits leave row 2 reachable through its TTC clause alone, which is what D11 rejects.
- **Every change of `riskState` for a `(warningType, trackId)` emits exactly one R4 warning event, in both directions.** Steady state emits nothing. Downgrades and the return to `low` are emitted too: R4 carries no separate clear message and the periodic state stream is optional, so the transition back is the only way the IVI learns to stop warning.
- A transition commits only after it holds for `RISK_DWELL_MS` — one debounce across all three thresholds, independent of the R13 gate hysteresis that protects track identity.
- **No B, no chain.** `d_AC = d_AB + d_BC` needs `d_AB`, so with no own-sensor B track the composed range does not exist. The plugin returns `low` with rationale `b_unknown` and logs `assess_skipped_b_unknown`. It follows that no `medium` or `high` was ever entered without a known B, so a clearing event can always fill the required `geometry.vehicleB` from `lastKnownB`. `geometry.vehicleC` is null exactly when C's track has been erased, which is why the frozen schema allows null there.
- Composition, in `fusion/scene_composer`: `vehicleB = (d_AB, y_B)` from the nearest `own_sensor` track, and `vehicleC = (d_AB + d_BC, y_B + y_BC)` — longitudinal sum, lateral component-wise, valid for the near-collinear convoy ([milestone1_high_level_plan.md §2](../../Plan-Proposal/milestone1_high_level_plan.md#2-scope--assumptions)).

## D6 — R12 detector: frame-source seam, inference, distance, zero-C evidence

- **The frame source is a seam.** `detector/frame_source.py` defines `FrameSource.iter_frames() -> Iterator[Frame]` with `Frame(index, timestamp_ms, image, width, height)`. `FileFrameSource` decodes `VIDEO_CLIP_PATH` with OpenCV `VideoCapture`, honouring `DETECTOR_FRAME_STRIDE` and `DETECTOR_LOOP`; `SyntheticFrameSource` needs no clip and serves CI. A live source arrives later as one more implementation, with no change to inference, distance, tracking or emission.
- **The clip reaches the container as an image layer and by no other route.** A Container Node has no volume, no bind mount and no declarative file injection, so `COPY media/ /app/media/` is the transfer and `docker push` is the upload ([m1-video-source-and-ivi-dashcam.md §5](../../../requirements/m1-video-source-and-ivi-dashcam.md)). Rejected alternatives: a `video` pin, which has no C++ client, an undocumented frame header and 590 Mbit/s of raw RGBA at 720p20; a fetch at container start, which trades a build-time cost for a demo-time network dependency.
- **The IVI dashcam view is deferred and no part of it is built here** ([milestone1_high_level_plan.md §6](../../Plan-Proposal/milestone1_high_level_plan.md#6-deferred-to-later-milestones)). That means no HTTP clip server in `entrypoint.sh`, no `CLIP_HTTP_ENABLED` or `CLIP_HTTP_PORT`, no `exposedPorts` entry and its gateway route, and no `video` pin. A worked design for it exists; reading it is not permission to build it.
- **Inference** (`detector/inference.py`): an ONNX Runtime CPU session on `MODEL_PATH`, letterboxing to 640×640, NMS at `IOU_THRESHOLD` and a score floor of `CONF_THRESHOLD`. The COCO classes `car`, `truck`, `bus` and `motorcycle` collapse to the R3 `class: "vehicle"`. `Detector` is a protocol, so the model swaps without touching the pipeline.
- **Distance is a pinhole known-width estimate.** `f_px = (frame_w / 2) / tan(CAMERA_HFOV_DEG / 2)`; `d = VEHICLE_WIDTH_M × f_px / bbox_w_px`; lateral `y = (bbox_u_center − frame_w / 2) × d / f_px`. Chosen because a user-supplied clip carries no intrinsics and no ground-plane calibration, and a monocular-depth network is a second model on a CPU-only node. The ground-plane bbox-bottom alternative needs camera height *and* pitch — more unknowns, not fewer. What the gate needs is a monotonic, consistently biased range over B at roughly 10–40 m, which this delivers.
- **The risk this carries, and the only sanctioned response.** Absolute accuracy rests on two constants that a user-supplied clip cannot calibrate. If B's estimated range disagrees with the scenario, `VEHICLE_WIDTH_M` and `CAMERA_HFOV_DEG` are retuned. **The R13 gate is never moved to compensate** — the gate decides track identity and the risk thresholds decide alarm level, and moving one to fix the other destroys both ([walkthrough §5.5](../../../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#55-retune-when-no-warning-is-emitted)).
- **Association** (`detector/tracker.py`): greedy IoU matching against the previous sampled frame, holding an id while IoU ≥ `TRACK_IOU_MIN`. Enough for one dominant occluder, and no tracking library is pulled in.
- **Emission** (`detector/emit.py`): one R3 JSONL line per detection per sampled frame, through the frozen `detector/contracts/tracked_object.py` binding — `source: own_sensor`, `id: own:<n>`, `state: not_tracked` (D3), `confidence` from the detection score. **`speed` is the magnitude of the range rate**, `|Δdistance| / Δt`, and 0 on a track's first sampled frame: frozen R3 bounds `speed` at ≥ 0, so a signed closing rate is not representable there. The sign lives in the `distance` series the store keeps.
- **Zero detections labelled C is structural, not a filter.** The detector's only input is ego's clip, in which C is never visible; it has no access to relayed data and cannot mint a `v2x:` id or a `v2x_relayed` source. `tools/check_zero_c.py` makes the claim falsifiable: it fails the run if any own-sensor entry claims a relayed source or id namespace, or if any own-sensor track sits within its radius argument of the relayed C position at the same timestamp. A pass is the expected trivial result, which is what R19 asks for.
- **Model provenance.** `tools/export_yolo11n.py` performs the one-off Ultralytics → ONNX export (AGPL-3.0, report §4) and `models/yolo11n.onnx` is committed, so the image build stays offline-reproducible and CI never depends on the Ultralytics package. Accepted cost: the model and the clip are committed binaries and enter every build context, because a Container Node reaches a file no other way.

## D7 — R15 output stage

- `output/warning_builder` maps a `RiskFinding` plus the composed geometry onto the frozen `contracts::R4WarningEvent` struct and serializes through its binding. It is the node's only R4 producer, so the wire shape cannot drift from the schema the round-trip tests cover.
- `output/ivi_sender` sends one UDP datagram per event to `IVI_ECU_HOST:IVI_ECU_PORT` through `net/udp_socket`, and logs `r4_tx` carrying the full R4 body.
- **The periodic awareness state is optional per R15.** The builder and the binding support `R4StateMessage`; `STATE_RATE_HZ > 0` enables a last-value-wins state tick, and the default `0` leaves it off. No acceptance criterion depends on it.

## D8 — R18: the ADA half of the evidence stream

- **One line per event, flushed per line:** `[EVT] {"event":…,"mono_ms":…,"epoch_ms":…,"payload":{…}}`. The `[EVT]` prefix, the per-line flush, the `event` name and the two clock stamps are the V2X ECU's, so one offline reader walks both nodes' streams; the stamps are the clock pair of D10. This node nests its payload under one `payload` key rather than a per-event key, which is the shape the walkthrough's checks read.
- Event vocabulary: `detector_spawn`, `detector_eof`, `detector_restart`, `own_sensor_ingest`, `r2_ingest`, `parse_reject`, `track_transition`, `track_expire`, `assessment`, `assess_skipped_b_unknown`, `risk_transition`, `r4_tx`.
- **Events carry payloads where the payload is the proof.** `r2_ingest` embeds the received R2 body, `r4_tx` the emitted R4 body, `track_transition` the distance and reason, `assessment` the `d_AC`, the TTC and the rationale.
- `assessment` is written on every committed change plus an `ASSESS_LOG_EVERY_MS` heartbeat. A line per fusion tick would bury the transitions the demo table asks for.
- Sink: stdout always, because the CarSky View Log is the live window, plus `EVENT_LOG_PATH` when it is set. `tools/event_report.py` renders the `track_transition`, `risk_transition` and `r4_tx` subset as the collision-risk event list of report §1's demo table.

## D9 — Deployment shape

- **Single-platform `linux/arm64`, one base image for both build stages.** The base is `python:3.11-slim` — the report's Python 3.11 for the detector *and* the C++ build base — so the core binary links against the glibc and libstdc++ it runs on by construction. This node carries no Vanetza, so neither the ASN.1 build cost nor the CMake 3.28 floor applies and apt's CMake clears the 3.22 minimum.

  ```
  docker buildx build --platform linux/arm64 --provenance=false --sbom=false \
    -t m1-ada-ecu:latest ADA_ECU/
  ```

  `--platform` and the disabled attestations are a standing requirement: a Container Node rejects a manifest index and hangs in `Provisioning`.
- **Layout inside the image:** `/app/ada_ecu` (the core binary), `/app/detector/` (the Python package with its `requirements.txt` installed at build), `/app/models/yolo11n.onnx`, `/app/media/ego-b-occluding-c.mp4`, and `entrypoint.sh` with `capture.sh` at the workdir root.
- **The node captures its own egress.** R15 and R19 need a record of ADA→IVI traffic, which the V2X ECU's capture point cannot see. `entrypoint.sh` starts `capture.sh` in the background and then `exec`s the binary, which is why the blueprint `command` is `["./entrypoint.sh"]` and `capabilities` is `["NET_RAW"]` ([node-ada-ecu.md](../../../requirements/car-sky-guide/node-ada-ecu.md#blueprint-node-config)). The `exec` is load-bearing: the app becomes PID 1 and receives SIGTERM directly.
- **The scripts are duplicated per folder rather than shared.** Build contexts are self-contained and no folder imports another's sources; the host-side extraction procedure is the shared [traffic-capture-wireshark.md](../../../requirements/car-sky-guide/traffic-capture-wireshark.md).
- **The image lane proves the detector's wheels resolve on `aarch64`, rather than assuming it.** `onnxruntime` and `opencv-python-headless` are installed at image build under emulation, and a missing wheel means a source build inside QEMU that can exhaust the job's timeout. The lane therefore ends by starting `detector/main.py --synthetic` inside the pulled `linux/arm64` image and observing R3 JSONL on stdout: a build that links and a detector that imports are two different claims, and only the second one closes R12's runtime path.

## D10 — Clock domains, and stimulus paced against `CLOCK_MONOTONIC`

Realizes [m1-run-timing-and-event-triggering.md §6.1–§6.2](../../../requirements/m1-run-timing-and-event-triggering.md) for this node. No clock is synchronised across nodes, and no component performs arithmetic mixing two nodes' timestamps.

| Purpose | Clock |
|---|---|
| Wire and log stamps — R3 `timestamps.*`, `[EVT] epoch_ms` | `CLOCK_REALTIME` (`system_clock` / `time.time()`) |
| Intervals — the fusion tick, track expiry, detector pacing, `[EVT] mono_ms` | `CLOCK_MONOTONIC` (`steady_clock` / `time.monotonic()`) |
| Arithmetic mixing two nodes' stamps | forbidden |

Which clock fills which R3 field, per source, is [§10.2](ada-ecu-hld.md#102-r3--the-object-model-of-the-store-owned).

- **Expiry compares a monotonic stamp, never `lastUpdated`.** `track_store` keeps a `monoMs` beside each entry and `admission` tests `now_mono − monoMs > TRACK_TIMEOUT_MS`. `lastUpdated` is `CLOCK_REALTIME` because it reaches the wire; the host's own NTP daemon stepping the shared wall clock mid-run would otherwise expire every track at once.
- **The detector paces sampled frames to wall-clock rate.** With `DETECTOR_REALTIME_PACING` true, `detector/pacer.py` holds each sampled frame until its deadline `t0 + n × DETECTOR_FRAME_STRIDE / DETECTOR_CLIP_FPS` on `time.monotonic()`. **The deadline is computed, not accumulated** — a fixed sleep per frame folds the inference cost into scenario time and drifts unbounded over a run. `DETECTOR_CLIP_FPS` defaults to the clip's declared rate and is overridable; `DETECTOR_START_DELAY_S` is the grace from spawn to the first emitted frame.
- **The run start is the operator restarting a node.** There is no orchestrator, no trigger message and no clock exchange: each stimulus source self-schedules from its own process start against its own configured delay.
- **`tools/check_run_alignment.py` is a checker, not a trigger.** It reads logs after a run, is never on the ego data path, and is sanctioned test equipment rather than product code. Its checks are [§12](ada-ecu-hld.md#12-test-strategy).
- **Pacing serves R20, not the deferred dashcam view.** The deferred item forbids real-time pacing *as its own requirement* in service of that view; R20 makes it a requirement on its own footing, so building it pulls no deferred surface in (D6).

## D11 — R22 run choreography: the run origin, the paced clip, and the risk band pair

Realizes [m1-run-timing-and-event-triggering.md §6.6](../../../requirements/m1-run-timing-and-event-triggering.md) for this node. R22 places the first R4 of a cycle in the open interval (`T0` + 8.0 s, `T0` + 10.0 s). Below is what this node owes that placement. Every lever is node configuration or bench scenario data, and no frozen contract carries any of it.

- **`T0` is this node's own first emitted `own_sensor` R3 line**, visible in the `[EVT]` stream. The run origin is therefore read from one clock, which is what keeps K6 inside D10's single-domain rule.
- **`DETECTOR_REALTIME_PACING = true` is load-bearing, not a preference.** With pacing off, clip time is not run time: the clip is consumed at whatever rate the CPU allows, and no instant in the choreography is expressible. A demo run with it off has no defined warning onset.
- **Alignment is made on the bench, not here.** `DETECTOR_START_DELAY_S` stays 0, so the detector's warm-up `W` — ONNX session load plus `VideoCapture` open — is the only unknown, and the bench's `start_delay_s` cancels it ([SP D7](../SCENARIO-PLAYER/scenario-player-design-decisions.md)).
- **The bench cycle period and the clip length are one matched pair**, 10.0 s. A longer bench cycle leaves C tracked across a clip wrap at which B has been dropped and not yet re-admitted. The assessment then falls to `low` on the `b_unknown` path of D5, the warning disappears mid-run, and K1 fails for that interval. The constraint binds the two values together, and neither is free to be retuned alone.
- **`RISK_NEAR_M` 60 with `RISK_CRITICAL_M` 30 moves the trigger off a derivative and onto a direct comparison.** `d_AC` at C's admission is ≈ 47 m, so the range clause of D5's row 2 commits the transition on its own, with a 13 m margin at the detector's nominal range estimate. It tolerates a range-estimate bias up to `k ≤ 1.70` before the transition leaves R22's window.

| Rejected alternative | Why |
|---|---|
| 60 / 40 | Adds a `high` state ≈ 0.7 s after the `medium` onset, spending the warning window on a second transition rather than a margin |
| 60 / 50 | Opens at `high`, and its bias tolerance falls to `k ≤ 1.16` |
| 25 / 15 | `medium` at 8.3 s through row 2's TTC clause only, with no range-clause margin: `d_AC` never falls to 25 m inside a cycle, so the onset rides entirely on `ttc` |
| Leave the bands and drive the transition from TTC alone | `ttc` derives from a numerical derivative of the *estimated* `d_AB`, the noisiest quantity in the node, and the transition would have to survive that jitter through the `RISK_DWELL_MS` debounce |
