# Phase 2–4 HLD — ADA ECU: store, admission, risk assessment, warning output (R3, R12–R15, R18 ADA side)

> High-level design for the ADA ECU's share of [milestone1.md](../../../plans/milestone1.md) §5 Phases 2, 3 and 4, per [hld-content-and-commit-format.md](../../../.claude/rules/hld-content-and-commit-format.md). Requirement definitions, field tables and tech stacks live in [m1-cooperative-awareness.md](../../../requirements/m1-cooperative-awareness.md) §2 R3/R12–R15/R18 and §3(d)/(g) — referenced, never restated.
>
> Authoritative design inputs this document realizes rather than reinterprets: [ada-ecu.svg](../../../requirements/ada-ecu.svg) (module shape, R14) and [vehicleC_track_admission_state_machine.png](../../../requirements/vehicleC_track_admission_state_machine.png) (R13 lifecycle).
>
> Diagram sources: [components](../../../documents/Design/ADA-ECU/phase2-4-ada-ecu-components.puml) · [call flow](../../../documents/Design/ADA-ECU/phase2-4-ada-ecu-callflow.puml) · [admission state machine](../../../documents/Design/ADA-ECU/phase2-4-ada-ecu-admission.puml).
>
> **This HLD resolves the standing consolidation question** raised in [video-source-for-r12.md](../../../documents/KnowledgeBase/video-source-for-r12.md) §5 ("two ADA folders exist") — decision D1.

## 1. Scope

- The ADA ECU application across three phases: the C++17 core skeleton, R3 track store and R13 admission state machine (Phase 2); the R12 Python detector subprocess (Phase 3); relayed-C fusion, the R14 Collision Risk Assessment abstraction with its database, and R15 R4 emission (Phase 4). The ADA half of the R18 evidence stream runs through all three.
- Deployment shape of this node for R5/R6: image layout, entrypoint, blueprint node-config additions, and the ADA→IVI traffic capture that R15/R19 acceptance needs.
- **Prerequisite, not redesigned here:** the Phase 0 contract layer already in this folder ([phase0-contract-freeze-hld.md](../../../plans/doc/phase0-contract-freeze-hld.md)) — `contracts/` synced schemas, `src/contracts/` R2/R3/R4 bindings, `detector/contracts/tracked_object.py`, `tests/contracts/`, `CMakeLists.txt`. Phases 2–4 extend that tree.
- **Out of scope here:** the IVI-side R4 consumption and rendering (Phase 5, R16/R17) — §11 item 4 flags one defect found there without designing it; the V2X ECU (Phase 1) and the bench (R11).

## 2. Sourced research notes and prior designs

| Source | Adopted |
|---|---|
| [video-source-for-r12.md](../../../documents/KnowledgeBase/video-source-for-r12.md) | Wholesale as the R12 input decision: no CarSky video content exists, the clip is user-supplied and baked into the image at `media/ego-b-occluding-c.mp4`, reached via `VIDEO_CLIP_PATH` / `DETECTOR_FRAME_STRIDE` (§3 spec, §4 deliverable). Its §2 (a′) rejection rests on frame acquisition sitting behind a seam — designed as D6. `make_sample_video.py` kept as a CI fixture only. |
| [Phase 0 HLD](../../../plans/doc/phase0-contract-freeze-hld.md) | D1 access model (byte-synced schema copies, no cross-folder reads), D2 bindings as pure model code, D4 additive-version tolerance. The frozen R2/R3/R4 bindings in `src/contracts/` are the only message models this design uses. |
| [V2X ECU Phase 1 HLD](../../../V2X_ECU/doc/phase1-v2x-ecu-comms-hld.md) | D1 sole-socket-holder rule, D4 `[EVT]` JSONL line shape and payload-carrying events (so one offline reader reconstructs both nodes), D5 tcpdump capture pattern reused for the ADA→IVI hop (D9). |
| [Scenario Player Phase 1 HLD](../../../Scenario_Player/doc/phase1-scenario-player-hld.md) | Bench cadence `cpm_rate_hz` (10 Hz default) as the relayed-update rate this design sizes timeouts against; the two committed scenarios (`default.yaml` approaching, `c-out-of-range.yaml` beyond the exit gate) as the R13/R14 exercise inputs; §10 item 2's ratified one-base-image Docker pattern reused in D9. |

Notes are non-authoritative scratch; on any conflict the CLAUDE.md document-authority order wins.

## 3. Design decisions

### D1 — Consolidation: `ada-ecu/` is deleted; nothing is moved wholesale

`ADA_ECU/` is the canonical node folder ([node-code-layout.md](../../../.claude/rules/node-code-layout.md)). The branch's lowercase `ada-ecu/` is a parallel implementation built outside the frozen contracts, and it cannot be merged file-by-file: every source file there depends on `ada-ecu/include/ada/types.hpp`, a **second** `TrackedObject`/`Source`/`TrackState` model that duplicates and contradicts the frozen binding in `src/contracts/tracked_object.hpp`. Copying any `.cpp` drags the duplicate model in.

**Verdict: `ada-ecu/` is removed in one commit. Salvage is by rewrite against the frozen types — the algorithm and file shape are reused, the code is not.** Two folders would also mean two Dockerfiles and two build contexts for one CarSky node.

Salvaged (shape reused, rewritten on the frozen bindings):

| `ada-ecu/` source | Lands as | Kept / changed |
|---|---|---|
| `src/udp_r2_receiver.cpp`, `src/udp_r4_sender.cpp` | `src/net/udp_socket.{hpp,cpp}` | Keep the bind/`select`/`recvfrom`/`sendto` shape. Changed: one file becomes the **sole socket-API holder** (V2X HLD D1); transient errors are counted and logged, not thrown. |
| `src/event_logger.cpp` | `src/log/event_log.{hpp,cpp}` | Keep the `{ts, event, payload}` JSONL line. Changed: built with nlohmann (the concatenated version cannot escape strings), `[EVT]`-prefixed to stdout always plus an optional file sink — the V2X ECU's shape (D8). |
| `src/config.cpp` | `src/config/config.{hpp,cpp}` | Keep the env-override mechanism. Changed: env is the **only** source — the `.conf` file is dropped (two config sources is two sources of truth, and the blueprint injects env); names corrected to the sanctioned set (§6). |
| `src/track_store.cpp` | `src/store/track_store.{hpp,cpp}` | Keep the `id → TrackedObject` map and the `upsert/get/all/nearest` surface. Changed: admission logic moves out to `store/admission` and is rewritten (D3); drops are erased, not left behind. |
| `src/r2_mapper.cpp` | `src/parser/r2_parser.{hpp,cpp}` | Keep the R2→TrackedObject mapping and the `v2x:<stationId>:<objectId>` id convention. Changed: parses through the frozen `R2Message` binding instead of raw JSON probing; `position.confidence` (metres, F6) has no R3 home and goes to the `r2_ingest` event payload. |
| `src/detector_jsonl_ingest.cpp`, `src/r3_mapper.cpp` | `src/observer/detector_reader.{hpp,cpp}` + `src/parser/r3_parser.{hpp,cpp}` | Keep line-oriented JSONL ingest. Changed: reads the live subprocess stdout pipe; the file path stays only as a test fixture mode. |
| `tools/mock_v2x_sender.py`, `tools/mock_ivi_receiver.py`, `tools/make_sample_video.py` | `tools/` (same names) | Loopback test equipment and the CI video fixture; retargeted to the sanctioned ports. |
| `tests/track_store_tests.cpp` | `tests/store/` | The **case list** is reused as a checklist; assertions are rewritten because the state machine changes (D3). |

Discarded, with the reason:

| Discarded | Why |
|---|---|
| `schemas/*.json` (r2, r3, r4) | Forks, not copies, of the frozen contracts: `additionalProperties: false` breaks the R4 additive-version acceptance; R3 makes `position.confidence` required (that field belongs to R2); R4 requires a non-null `geometry.vehicleC`, adds an undeclared `trackedObjects` array and drops the `stateMessage` variant. `contracts/` (repo root) is the only schema source; `ADA_ECU/contracts/` holds its byte-synced copies. |
| `src/warning_builder.cpp` | Emits hand-concatenated JSON with the three divergences above. The **geometry formula** (`d_AB + d_BC`, lateral component-wise) is salvaged into `fusion/scene_composer`; the emitter is rebuilt on the frozen `R4WarningEvent` binding. |
| `src/risk_assessor.cpp` | A concrete class with a binary `Warning|Clear` vocabulary mapped to `high|low`, thresholded on `gate_enter_m` — no interface, no registry, no database. It collapses R14 into R13 and never emits `medium`. R14 is designed fresh (D4, D5). |
| `src/main.cpp` | A test harness (`--mock-distances`, `--max-r2`, in-process sender thread, repo-relative default paths), not a node entrypoint. Its loopback ideas move to `tools/`. |
| `Dockerfile` | Pins the build stage to `--platform=$BUILDPLATFORM` with no cross-toolchain while the runtime stage follows `TARGETARCH`; CarSky needs a single-platform `linux/arm64` image (D9). |
| `config/ada-ecu.conf` | File-based config whose defaults (`46002`, `46004`, `IVI_HOST`/`IVI_PORT`) contradict the blueprint's `47200`/`47300`/`IVI_ECU_HOST`/`IVI_ECU_PORT`. |
| `tools/video_detector.py` | `--backend placeholder` only — no YOLO11n, no ONNX Runtime, fabricated distance; it also emits `state: "tentative"` (the store owns state) and `position.confidence` (not in frozen R3). Its `FrameInput` / `DetectionBackend` protocol shape is salvaged as the D6 seams. |
| `docs/*.md`, `README.md` | Narrate the design being replaced. This HLD is the design of record for the node; two conflicting design documents is the failure mode to avoid. |

R12 and R14 were never implemented in `ada-ecu/`, and R13 was implemented incompletely (relayed tracks skipped `tentative`; the hit counter was never reset; expired tracks were marked, never erased) — so the discarded volume is not working functionality being thrown away.

### D2 — Process, thread and mock model

- **One image, two processes** (report §3(d)): the C++17 `ada_ecu` core and the Python 3.11 detector, joined **only** by argv + exit codes + R3 JSONL over stdout. No FFI, no RPC, no shared file.
- **Core threads:** a V2X rx thread (blocking receive on `V2X_LISTEN_PORT`), a detector-reader thread (`getline` on the subprocess stdout pipe), and the main thread. Both readers push onto one bounded queue; **the main thread is the single writer** of the store, the assessment database and the R4 output. Consequence: no locks beyond the queue, deterministic event ordering, and an `[EVT]` stream that replays a run exactly as it happened (R18 acceptance).
- **Fusion tick** every `FUSION_TICK_MS` drives expiry and assessment even when nothing arrives — a track must be able to expire, and risk to fall, on silence alone.
- **Detector lifecycle:** clean EOF with `DETECTOR_LOOP=true` (default) respawns from frame 0, so B stays present for a continuous R19 run that outlasts a 60–120 s clip; a non-zero exit triggers a logged, bounded restart (`DETECTOR_RESTART_MAX`). Looping the same detection path is not a scripted shortcut — no ego-path data is synthesized.
- **Mocks live outside `src/`.** Phase 2's mock own-sensor input is the real detector-reader pointed at a fixture — `DETECTOR_CMD="cat /app/tests/fixtures/own_sensor_mock.jsonl"`; mock R2 traffic is `tools/mock_v2x_sender.py` on the real socket. There is **no mock branch anywhere in the core**, and Phase 2's "toggling the mock off yields no tracks" is `DETECTOR_ENABLED=false` plus no bench traffic.

### D3 — R13 admission: one state machine, both sources

Realizes [vehicleC_track_admission_state_machine.png](../../../requirements/vehicleC_track_admission_state_machine.png) exactly, as [phase2-4-ada-ecu-admission.puml](../../../documents/Design/ADA-ECU/phase2-4-ada-ecu-admission.puml). The diagram is source-agnostic, so there is **one** implementation in `store/admission.{hpp,cpp}`, parameterized only by what counts as an update.

| Term | Definition fixed here |
|---|---|
| update | one admission-relevant observation for a track id — for `v2x_relayed` one R2 message (its `object.distance`), for `own_sensor` one detector JSONL line (its `distance`) |
| `not_tracked` | **absent from the store**, per the diagram's "absent from store" — a drop erases the entry rather than leaving one flagged `not_tracked` |
| hysteresis | `distance ≤ GATE_ENTER_M` admits and holds a `tentative` track; only once `tracked` does the wider `distance ≤ GATE_EXIT_M` hold it. One Schmitt band, exactly the diagram's `≤ 30 m` / `> 35 m` labels |
| N — `CONFIRM_HITS` | consecutive in-gate updates required in `tentative` before promotion; the counter resets to 0 on every entry to `not_tracked` |
| M — `TRACK_TIMEOUT_MS` | **wall-clock silence**, not a message count (decided below) |

**`miss_limit` is a wall-clock timeout, not a count of consecutive misses.** Two reasons, both structural:

- "Its messages stop" (R13's own wording) is a time condition. A count cannot express silence — nothing arrives to increment it, which is precisely why the branch implementation needed a clock inside its `expire()` anyway.
- The two sources run at independently configured cadences — relayed updates at the bench's `cpm_rate_hz` (10 Hz default), own-sensor at `DETECTOR_FRAME_STRIDE`-reduced 5 Hz. One count M would mean two different real timeouts, and re-tuning *another node's* config would silently change ADA behavior.

Default `TRACK_TIMEOUT_MS = 1000` is the wall-clock form of [milestone1.md](../../../plans/milestone1.md) §4's proposed M = 5, taken at the slower of the two sources (5 × 200 ms), so neither source expires early. The plan's wording is flagged for re-ratification (§11 item 1) rather than silently overridden.

Further rules:

- **The store is the sole writer of `state`.** The `state` field on incoming detector lines and R2-derived objects is ignored; the detector emits `not_tracked` by convention because it holds no store.
- Track ids: `v2x:<stationId>:<objectId>` (relayed), `own:<n>` (own-sensor, assigned by the detector's frame-to-frame association, D6).
- Every edge writes one `track_transition` event (id, source, from, to, distance, reason) — Phase 2's "transitions observable in logs and matching the diagram".
- A track's last snapshot survives its erasure in the assessment database (D4), not in the store — that is what lets a cleared warning still carry an R3 `object`.

### D4 — R14: the Collision Risk Assessment interface, registry and database

Realizes the «interface» block of [ada-ecu.svg](../../../requirements/ada-ecu.svg). Naming reconciliation, stated once: the SVG's realization **`Chained Collision`** is the report's "M1 NLOS plugin" and registers under the frozen R4 registry key **`nlos_obstruction`** — one concept, three existing names, no new term coined.

```cpp
// src/cra/i_collision_risk_assessment.hpp
struct RiskContext { const TrackStore& store; AssessmentDb& db; std::int64_t now_ms; };
struct RiskFinding {
  std::string warningType;                        // R4 registry key == plugin name
  std::string riskState;                          // low | medium | high (frozen R4)
  std::optional<contracts::TrackedObject> trigger;// R3 snapshot for the R4 `object` field
  std::string rationale;                          // evidence text for the DB and the [EVT] stream
};
class ICollisionRiskAssessment {
 public:
  virtual ~ICollisionRiskAssessment() = default;
  virtual std::string name() const = 0;           // registry key
  virtual RiskFinding assess(RiskContext&) = 0;   // reads the store, reads/writes the db
};
```

- **Registration is explicit, not static-init.** `src/cra/builtin_plugins.cpp` holds one `registry.add(std::make_unique<ChainedCollision>(cfg))` line per plugin, and `main.cpp` enables the subset named by `CRA_ENABLED` (default `nlos_obstruction`). Adding a future plugin (intersection hazard, curve blind spot, priority-vehicle preemption, speed-scaled risk) is **one new file plus one line in that file** — no edit to the interface, the store, the emitter, or any existing plugin, which is R14's acceptance. Self-registering static registrars were rejected: in a static library the linker discards the unreferenced objects unless the whole archive is force-linked, which is a build-system trap for a milestone with no time to debug one.
- **The plugin never emits.** It returns a `RiskFinding`; the output stage decides transport. That keeps rules (business logic) replaceable independently of emission (controller).

**The database — what it concretely is.** R14's acceptance names "the database schema the assessment reads and writes" as a committed artifact. Here it is an **append-and-upsert record table keyed by track id, defined by a committed JSON Schema and held in process**, with every write also appended to the `[EVT]` stream so the table is reconstructible offline:

- Schema: `ADA_ECU/schema/cra-assessment-record.schema.json` — a **node-local** schema, deliberately *not* in `ADA_ECU/contracts/` (that folder holds only byte-synced copies of the frozen cross-node contracts under `sync-manifest.json`).
- Accessor: `src/cra/assessment_db.{hpp,cpp}` — typed `get(trackId)` / `upsert(record)` / `erase(trackId)`, the seam a future milestone swaps for real persistence.

| Record field | Meaning |
|---|---|
| `trackId`, `warningType` | key: which track, assessed by which plugin |
| `riskState`, `riskStateEnteredMs` | last committed level and when it was entered — the edge-trigger and dwell input |
| `firstSeenMs`, `lastUpdatedMs` | assessment lifetime |
| `distanceM`, `previousDistanceM` | last and prior composed `d_AC` — the closing-rate input |
| `closingRateMps`, `ttcS` | derived; `ttcS` null when not closing |
| `lastSnapshot` | R3 snapshot of the track, carried past its erasure so a cleared warning still has an `object` |
| `lastKnownB` | last own-sensor B position, so a clear can still fill the required `geometry.vehicleB` |
| `emittedCount`, `rationale` | evidence: how many R4 events this track produced, and why the last level was chosen |

Why not SQLite: it adds a dependency and a file lifecycle to a Container Node that has **no volume** (research note §1 — a file dies with the pod), buys nothing over a JSONL append that also *leaves* the node through its only egress, and loses on criteria C1/C2/C4. A committed JSON Schema plus a typed accessor satisfies the acceptance literally — Phase 2 commits the schema, Phase 4's plugin reads and writes it.

### D5 — Risk vocabulary and edge-triggered emission

Frozen R4 fixes `riskState ∈ {low, medium, high}`. For `nlos_obstruction`, with ego speed unknown in M1 (no GNSS, no Cortex-M — report §1), the inputs are the composed range `d_AC`, its change over time from the database, and C's relayed speed.

| `riskState` | Meaning for the chained-collision plugin | Condition |
|---|---|---|
| `low` | C is not a present threat: unknown, out of the tracking gate, or tracked but neither near nor closing fast | no `tracked` C, **or** `d_AC > RISK_NEAR_M` and (`ttc` null or `ttc > RISK_TTC_WARN_S`) |
| `medium` | C is tracked and relevant — inside the awareness band, closing at a normal rate | C `tracked`, `d_AC ≤ RISK_NEAR_M`, and not `high` |
| `high` | C is tracked and imminent | C `tracked` and (`d_AC ≤ RISK_CRITICAL_M` **or** `ttc ≤ RISK_TTC_CRITICAL_S`) |

- `closingRateMps = -(d_AC(t) − d_AC(t−Δ)) / Δ`; `ttcS = d_AC / closingRate` when the rate is positive, otherwise null.
- **Risk thresholds are separate constants from the R13 gate and must never alias it** — that aliasing is what collapsed R14 into R13 in the branch. Defaults `RISK_NEAR_M = 25` and `RISK_CRITICAL_M = 15` sit strictly inside `GATE_ENTER_M = 30`, so the committed `default.yaml` approach scenario produces a visible `low → medium → high` progression and `c-out-of-range.yaml` never leaves `low`.
- **Every change of `riskState` for a `(warningType, trackId)` emits exactly one R4 warning event — both directions.** Steady state emits nothing. Downgrades and the return to `low` are emitted too: R4 carries no separate "clear" message and the periodic state stream is optional, so the transition back is the only way the IVI learns to stop warning.
- A transition commits only after it holds for `RISK_DWELL_MS` (default 300 ms) — one debounce covering all three thresholds, independent of the R13 gate hysteresis that protects track identity.
- **No B, no chain.** `d_AC = d_AB + d_BC` needs `d_AB`; with no own-sensor B track the composed range does not exist, so the plugin returns `low` with rationale `b_unknown` and logs `assess_skipped_b_unknown`. It follows that no `medium`/`high` was ever entered without a known B, so the clearing event can always fill the required `geometry.vehicleB` from `lastKnownB`. `geometry.vehicleC` is `null` exactly when C's track has been erased — which is why the frozen schema allows null there.
- Composition (`fusion/scene_composer`): `vehicleB = (d_AB, y_B)` from the nearest `own_sensor` track, `vehicleC = (d_AB + d_BC, y_B + y_BC)` — longitudinal sum, lateral component-wise, valid for the near-collinear convoy ([milestone1.md](../../../plans/milestone1.md) §2).

### D6 — R12 detector: frame-source seam, inference, distance, zero-C evidence

- **Frame-source seam (mandatory, per the research note's rejection of the video-pin option).** `detector/frame_source.py` defines `FrameSource.iter_frames() -> Iterator[Frame]` with `Frame(index, timestamp_ms, image, width, height)`. Implementations: `FileFrameSource` (OpenCV `VideoCapture` on `VIDEO_CLIP_PATH`, honoring `DETECTOR_FRAME_STRIDE` and `DETECTOR_LOOP`) and `SyntheticFrameSource` (CI, no clip needed). A future `video` pin arrives as one new implementation — no change to inference, distance, tracking or emission.
- **Inference** (`detector/inference.py`): ONNX Runtime CPU session on `MODEL_PATH`, letterbox to 640×640, NMS at `IOU_THRESHOLD`, score floor `CONF_THRESHOLD`; COCO classes `car|truck|bus|motorcycle` collapse to the R3 `class: "vehicle"`. `Detector` is a protocol, so the model is swappable without touching the pipeline.
- **Distance estimation — pinhole known-width.** `f_px = (frame_w / 2) / tan(CAMERA_HFOV_DEG / 2)`; `d = VEHICLE_WIDTH_M × f_px / bbox_w_px`; lateral `y = (bbox_u_center − frame_w / 2) × d / f_px`. Inputs, all externalized: bounding-box pixel width, frame width, `VEHICLE_WIDTH_M` (1.8 m), `CAMERA_HFOV_DEG` (60°). Chosen because a user-supplied clip carries no intrinsics and no ground-plane calibration, and a monocular-depth network is a second model on a CPU-only node (criteria C1/C2/C4); the ground-plane bbox-bottom alternative needs camera height *and* pitch — more unknowns, not fewer. Absolute accuracy is unvalidated without a calibration target (§11 item 3); a gate at 30/35 m over a clip with B at 10–40 m needs monotonic, consistently-biased range, which this delivers.
- **Frame-to-frame association** (`detector/tracker.py`): greedy IoU matching against the previous sampled frame, id held while IoU ≥ `TRACK_IOU_MIN`. Enough for one dominant occluder; no tracking library is pulled in.
- **Emission** (`detector/emit.py`): one R3 JSONL line per detection per sampled frame through the frozen `detector/contracts/tracked_object.py` binding — `source: own_sensor`, `id: own:<n>`, `state: not_tracked` (D3), `speed` from Δdistance/Δt (0 on a track's first frame), `confidence` from the detection score.
- **Zero detections labeled C — enforcement and evidence.** Enforcement is structural, not a filter: the detector's only input is ego's clip, in which C is never visible (research note §3, the one binding content row); it has no access to relayed data and cannot mint a `v2x:` id or a `v2x_relayed` source. Evidence is `tools/check_zero_c.py`, which fails the run if any own-sensor entry claims a relayed source or id namespace, or if any own-sensor track sits within `ZERO_C_RADIUS_M` of the relayed C position at the same timestamp. A pass is the expected trivial result — the check exists to make the R19 claim falsifiable, which is what R19 asks for.
- **Model provenance:** `tools/export_yolo11n.py` performs the one-off Ultralytics → ONNX export (AGPL-3.0 accepted, report §4) and the resulting `models/yolo11n.onnx` is committed, so the image build stays offline-reproducible and CI never depends on the Ultralytics package. Binary size is flagged (§11 item 5).

### D7 — R15 output stage

- `output/warning_builder` maps `RiskFinding` + composed geometry onto the frozen `contracts::R4WarningEvent` struct and serializes through its binding — the only R4 producer in the node, so the wire shape cannot drift from the schema the round-trip tests already cover.
- `output/ivi_sender` sends one UDP datagram per event to `IVI_ECU_HOST:IVI_ECU_PORT` via `net/udp_socket`, and logs `r4_tx` carrying the full R4 body (the V2X ECU's payload-carrying-event convention).
- The **periodic awareness state** (`R4StateMessage`) stays optional per R15: the builder and the binding already support it; a `STATE_RATE_HZ > 0` enables a last-value-wins state tick. Default `0` — off, built only if Phase 4 has time.

### D8 — R18: the ADA half of the evidence stream

- Same `[EVT]` JSONL line shape as the V2X ECU, so one offline reader reconstructs both nodes: `detector_spawn`, `detector_eof`, `detector_restart`, `own_sensor_ingest`, `r2_ingest`, `parse_reject`, `track_transition`, `track_expire`, `assessment`, `assess_skipped_b_unknown`, `risk_transition`, `r4_tx`.
- Payload-carrying where it proves something: `r2_ingest` embeds the received R2 body, `r4_tx` the emitted R4 body, `track_transition` the distance and reason, `assessment` the `d_AC`/`ttc`/rationale.
- `assessment` is written on every committed change plus an `ASSESS_LOG_EVERY_MS` heartbeat — a per-tick line would bury the transitions the demo table asks for.
- Sink: stdout always (CarSky View Log is the live window), plus `EVENT_LOG_PATH` when set. `tools/event_report.py` renders the `track_transition` + `risk_transition` + `r4_tx` subset as the **collision-risk event list** of report §1's demo table.

### D9 — Deployment shape

- **Single-platform `linux/arm64`, one base image for both stages.** Base `python:3.11-slim` — it is the report's Python 3.11 for the detector *and* the C++ build base, so the binary links against the glibc/libstdc++ it runs on by construction (the Scenario Player's ratified F1 pattern). No Vanetza on this node (`CMakeLists.txt` says so), so the ASN.1 build cost and the CMake 3.28 floor do not apply — apt's cmake clears the existing 3.22 minimum.

  ```
  docker buildx build --platform linux/arm64 --provenance=false --sbom=false \
    -t m1-ada-ecu:latest ADA_ECU/
  ```

  `--platform` and the disabled attestations are a standing requirement, not a preference: a Container Node rejects a multi-platform manifest index and hangs in Provisioning ([phase0-smoke-test-run.md](../../../plans/doc/phase0-smoke-test-run.md)).
- **Layout in the image:** `/app/ada_ecu` (binary), `/app/detector/` (Python package + `requirements.txt` installed at build), `/app/models/yolo11n.onnx` from `COPY models/`, `/app/media/ego-b-occluding-c.mp4` from `COPY media/` — the only way a file reaches a Container Node, since no volume or bind-mount field exists (research note §1).
- **Capture on this node.** R15 and R19 require a pcap of ADA→IVI traffic, which the V2X ECU's capture point cannot see. This node therefore carries the same `entrypoint.sh` + `capture.sh` + tcpdump pattern (V2X HLD D5), which changes the blueprint `command` to `["./entrypoint.sh"]` and requires `"capabilities": ["NET_RAW"]`. The scripts are duplicated per folder rather than shared — self-contained build contexts, no cross-node source imports; the host-side extraction procedure is the shared [traffic-capture-wireshark.md](../../../requirements/car-sky-guide/traffic-capture-wireshark.md).
- **Node guide update is a deliverable of this design:** [node-ada-ecu.md](../../../requirements/car-sky-guide/node-ada-ecu.md) gains the `command`/`capabilities` change and the additive env rows of §6. Additive only — no frozen contract moves, R6 keeps one `ethernet` pin and no `video` pin.

## 4. Folder structure map — file-location designations

Every deliverable and its target path; no implementer picks a path ad hoc. `P0` = exists (Phase 0), listed for context; `P2`/`P3`/`P4` = the phase that lands it. Everything is inside the node folder, so every designation is a sanctioned location ([node-code-layout.md](../../../.claude/rules/node-code-layout.md#per-folder-doc)) — no approval pause applies.

```
ADA_ECU/
├── Dockerfile                                   # P2: two stages, one base (python:3.11-slim), single-platform arm64 (D9)
├── entrypoint.sh                                # P2: capture.sh & -> exec ./ada_ecu (D9)
├── capture.sh                                   # P4: ADA->IVI live [CAP] text + rotating pcap (D9)
├── .dockerignore                                # P2: keeps doc/, tests/, tools/, requirements-dev.txt out of the context
├── CMakeLists.txt                               # P0 baseline; P2 adds the ada_ecu executable, module libraries and new test targets
├── README.md                                    # P0; P2 updates it to point at this HLD
├── contracts/                                   # P0 synced schema copies (r2, r3, r4) - sync-manifest owned, never edited here
├── schema/
│   └── cra-assessment-record.schema.json        # P2: R14 assessment-database schema (D4) - node-local, NOT a synced contract
├── src/
│   ├── main.cpp                                 # P2: composition root - config -> observers -> parsers -> store -> CRA -> output (D2)
│   ├── config/config.{hpp,cpp}                  # P2: the only env reader (§6)
│   ├── net/udp_socket.{hpp,cpp}                 # P2: sole socket-API holder (D1)
│   ├── observer/input_queue.hpp                 # P2: bounded queue, two producers, one consumer (D2)
│   ├── observer/v2x_listener.{hpp,cpp}          # P2: R2 UDP rx thread
│   ├── observer/detector_reader.{hpp,cpp}       # P2: subprocess spawn + stdout JSONL rx thread (fixture-driven until P3)
│   ├── parser/r2_parser.{hpp,cpp}               # P2: R2Message -> TrackedObject (v2x_relayed)
│   ├── parser/r3_parser.{hpp,cpp}               # P2: detector JSONL line -> TrackedObject (own_sensor)
│   ├── store/track_store.{hpp,cpp}              # P2: the R3 store ("Current Input"), single writer
│   ├── store/admission.{hpp,cpp}                # P2: R13 state machine (D3) - pure, no I/O
│   ├── cra/i_collision_risk_assessment.hpp      # P2: the R14 interface (D4)
│   ├── cra/registry.{hpp,cpp}                   # P2: plugin registry, lookup by warningType
│   ├── cra/builtin_plugins.cpp                  # P2: the one file edited when a plugin is added
│   ├── cra/assessment_db.{hpp,cpp}              # P2: typed accessor over the D4 schema
│   ├── cra/plugins/chained_collision.{hpp,cpp}  # P4: the M1 plugin (SVG "Chained Collision", warningType nlos_obstruction)
│   ├── fusion/scene_composer.{hpp,cpp}          # P4: d_AC = d_AB + d_BC, lateral component-wise (D5)
│   ├── output/warning_builder.{hpp,cpp}         # P4: RiskFinding + geometry -> R4WarningEvent (D7)
│   ├── output/ivi_sender.{hpp,cpp}              # P4: R4 UDP tx to the IVI (D7)
│   ├── log/event_log.{hpp,cpp}                  # P2: R18 [EVT] JSONL writer (D8)
│   └── contracts/{r2_message,tracked_object,r4_message}.{hpp,cpp}   # P0 frozen bindings
├── detector/                                    # P3 Python subprocess (R12); P0 already holds contracts/ + tests/ + requirements-dev.txt
│   ├── main.py                                  # P3: argv, wiring, exit codes - the process contract's ego side
│   ├── config.py                                # P3: the detector's only env reader
│   ├── frame_source.py                          # P3: the frame-source seam + File/Synthetic implementations (D6)
│   ├── inference.py                             # P3: YOLO11n ONNX session, letterbox, NMS, class mapping
│   ├── distance.py                              # P3: pinhole known-width range + lateral offset
│   ├── tracker.py                               # P3: greedy IoU association -> stable own:<n> ids
│   ├── emit.py                                  # P3: R3 JSONL on stdout via the P0 binding
│   ├── requirements.txt                         # P3 runtime: onnxruntime · opencv-python-headless · numpy
│   ├── requirements-dev.txt                     # P0: pytest · jsonschema
│   ├── contracts/tracked_object.py              # P0 R3 binding
│   └── tests/
│       ├── test_r3_roundtrip.py                 # P0
│       ├── test_frame_source.py                 # P3: stride, loop, EOF, synthetic source
│       ├── test_distance.py                     # P3: pinhole maths against hand-computed values
│       ├── test_tracker.py                      # P3: id stability across frames, id change below TRACK_IOU_MIN
│       └── test_emit_contract.py                # P3: emitted lines validate against the synced R3 schema
├── models/yolo11n.onnx                          # P3: exported once by tools/export_yolo11n.py, committed (D6)
├── media/ego-b-occluding-c.mp4                  # P3: the user-supplied clip (research note §4)
├── tools/
│   ├── export_yolo11n.py                        # P3: one-off Ultralytics -> ONNX export (never runs in CI or the image)
│   ├── make_sample_video.py                     # P3: CI smoke fixture only - never the demo source
│   ├── check_zero_c.py                          # P3: the R12/R19 zero-C check (D6)
│   ├── event_report.py                          # P4: [EVT] -> collision-risk event list (D8)
│   ├── mock_v2x_sender.py                       # P2: R2 loopback sender (salvaged)
│   ├── mock_ivi_receiver.py                     # P4: R4 loopback sink (salvaged)
│   └── extract_pcap.sh                          # P4: host-side log -> .pcap (D9; not in the image)
├── tests/                                       # GoogleTest + CTest
│   ├── test_sanity.cpp · contracts/             # P0
│   ├── store/test_admission_own_sensor.cpp      # P2: full lifecycle, both directions, counter reset
│   ├── store/test_admission_relayed.cpp         # P2: gate/hysteresis boundaries at 30 m and 35 m, timeout expiry
│   ├── store/test_track_store.cpp               # P2: upsert/nearest/erase; R3 field completeness
│   ├── parser/test_r2_parser.cpp                # P2: mapping, id convention, malformed rejection counted
│   ├── parser/test_r3_parser.cpp                # P2: JSONL mapping, incoming `state` ignored
│   ├── cra/test_registry.cpp                    # P2: registration, lookup, CRA_ENABLED selection
│   ├── cra/test_assessment_db.cpp               # P2: record round-trip against the D4 schema
│   ├── cra/test_chained_collision.cpp           # P4: the D5 band table, dwell debounce, b_unknown path
│   ├── fusion/test_scene_composer.cpp           # P4: composed geometry vs hand-computed values
│   ├── output/test_warning_builder.cpp          # P4: emitted R4 validates against the synced schema; null vehicleC case
│   └── fixtures/
│       ├── samples/                             # P0 synced
│       ├── own_sensor_mock.jsonl                # P2: the Phase 2 mock own-sensor stream (D2)
│       └── malformed/                           # P2: R2/R3 rejection corpus (local fixture, not a synced contract)
└── doc/
    ├── phase2-4-ada-ecu-hld.md                  # this document
    ├── phase2-4-ada-ecu-components.puml         # §7
    ├── phase2-4-ada-ecu-callflow.puml           # §7
    ├── phase2-4-ada-ecu-admission.puml          # §7
    └── research_notes/                          # existing (video-source study + its diagrams)
```

Removed by this design: the entire `ada-ecu/` folder at the repo root (D1).

## 5. Tech stack

Traceable to report §3(d)/(g); no candidate is re-litigated here.

| Area | Stack | Trace |
|---|---|---|
| Core | C++17, one process, two rx threads + a single-writer main loop | report §3(d) |
| Messages | nlohmann/json via the frozen Phase 0 bindings (R2 in, R3 store, R4 out) | R2/R3/R4 tech stack |
| Transport | POSIX UDP through `src/net/` | R6, report §3(f) |
| Detector | Python 3.11; YOLO11n exported to ONNX on **ONNX Runtime CPU**; OpenCV (`opencv-python-headless`) decode; numpy | report §3(g), R12 |
| Process boundary | argv + exit codes + R3 JSONL over stdout — no FFI, no RPC | report §3(d) |
| Build/test | CMake ≥ 3.22 + GoogleTest + CTest (C++); pytest + jsonschema (Python) | Phase 0 toolchain |
| Evidence | JSONL `[EVT]` stream; tcpdump for the ADA→IVI pcap | R18, R15/R19 |
| Image | Docker multi-stage, one base `python:3.11-slim`, single-platform `linux/arm64` | R5, D9 |

Deliberately absent: Vanetza (this node never touches UPER), any database engine (D4), any tracking or monocular-depth library (D6).

## 6. Configuration — no hardcoded tunables

Every value is env-injected by the blueprint or falls through to `src/config/config.cpp` (core) / `detector/config.py` (detector). `(proposal)` marks an architecture proposal no committed acceptance criterion fixes.

| Env | Default | Meaning |
|---|---|---|
| `V2X_LISTEN_PORT` · `V2X_LISTEN_HOST` | `47200` (blueprint) · `0.0.0.0` | R2 ingress |
| `IVI_ECU_HOST` · `IVI_ECU_PORT` | `10.99.0.13` · `47300` (blueprint) | R4 egress |
| `GATE_ENTER_M` · `GATE_EXIT_M` | `30` · `35` (blueprint) | R13 admission / drop gate |
| `CONFIRM_HITS` | `3` *(proposal — plan §4 N)* | in-gate updates before `tracked` |
| `TRACK_TIMEOUT_MS` | `1000` *(proposal — realizes plan §4 M)* | silence before a track is erased (D3) |
| `FUSION_TICK_MS` | `100` *(proposal)* | expiry + assessment tick |
| `DETECTOR_ENABLED` · `DETECTOR_CMD` | `true` · `python3 /app/detector/main.py` | detector spawn; the fixture override is how Phase 2 mocks (D2) |
| `DETECTOR_RESTART_MAX` · `DETECTOR_LOOP` | `5` *(proposal)* · `true` *(proposal)* | bounded restarts; replay the clip at EOF |
| `VIDEO_CLIP_PATH` · `DETECTOR_FRAME_STRIDE` | `/app/media/ego-b-occluding-c.mp4` · `4` | research note §4 |
| `MODEL_PATH` | `/app/models/yolo11n.onnx` | ONNX session |
| `CONF_THRESHOLD` · `IOU_THRESHOLD` · `TRACK_IOU_MIN` | `0.35` · `0.45` · `0.3` *(proposal)* | detection and association |
| `VEHICLE_WIDTH_M` · `CAMERA_HFOV_DEG` | `1.8` · `60` *(proposal)* | pinhole distance inputs (D6) |
| `CRA_ENABLED` | `nlos_obstruction` | active plugin list (D4) |
| `RISK_NEAR_M` · `RISK_CRITICAL_M` | `25` · `15` *(proposal)* | `medium` / `high` range thresholds (D5) |
| `RISK_TTC_WARN_S` · `RISK_TTC_CRITICAL_S` | `6` · `3` *(proposal)* | TTC thresholds (D5) |
| `RISK_DWELL_MS` | `300` *(proposal)* | debounce before a transition commits |
| `STATE_RATE_HZ` | `0` (off) | optional R15 periodic awareness state |
| `ZERO_C_RADIUS_M` | `5` *(proposal)* | zero-C check tolerance (D6) |
| `EVENT_LOG_PATH` · `ASSESS_LOG_EVERY_MS` | *(empty = stdout only)* · `1000` *(proposal)* | R18 sink and heartbeat |
| `CAPTURE_FILTER` · `PCAP_DIR` · `CAPTURE_ROTATE_S` | `udp` · `/data/capture` · `60` | D9 capture, V2X ECU values |

## 7. Diagrams

- **Call flow** — [phase2-4-ada-ecu-callflow.puml](../../../documents/Design/ADA-ECU/phase2-4-ada-ecu-callflow.puml): bring-up, B detected in the clip → R3 JSONL → store, relayed C → store, then the fusion tick expiry → R13 admission → R14 assessment → R15 R4 warning → IVI, with the parse-reject, `b_unknown` and detector-EOF branches.
- **Component map** — [phase2-4-ada-ecu-components.puml](../../../documents/Design/ADA-ECU/phase2-4-ada-ecu-components.puml): the [ada-ecu.svg](../../../requirements/ada-ecu.svg) blocks as modules and their dependencies.
- **Admission state machine** — [phase2-4-ada-ecu-admission.puml](../../../documents/Design/ADA-ECU/phase2-4-ada-ecu-admission.puml): D3 made unambiguous, one machine for both sources.

## 8. MVC mapping

No module spans two layers.

- **Data** — `src/contracts/` frozen bindings, `src/store/track_store` (current perception), `src/cra/assessment_db` + `schema/cra-assessment-record.schema.json` (assessment memory), `src/config/`, `src/log/event_log` (evidence persistence), fixtures.
- **Business logic** — `src/store/admission` (R13 rules, pure), `src/cra/plugins/chained_collision` (R14 rules), `src/fusion/scene_composer` (geometry), and the detector's `inference` + `distance` + `tracker` (perception rules). None of these open a socket, read env, or format a wire message.
- **UI logic (controller)** — `src/main.cpp` composition root, `src/observer/` (input edges), `src/parser/` (wire → model), `src/output/warning_builder` + `ivi_sender` (model → the view model the IVI consumes). This layer is what makes the R4 message; it holds no rules.
- **UI** — none on this headless node. The rendering surface is the IVI's Phase 5 view (R16/R17), which consumes the R4 message as its view model; the local observability surface is the `[EVT]` stream in CarSky View Log.

## 9. Deployment shape (R5/R6)

- Image `ada-ecu:latest` → `registry.carsky.io/m1-ada-ecu:latest`, built from `ADA_ECU/` alone (self-contained context), per [node-ada-ecu.md](../../../requirements/car-sky-guide/node-ada-ecu.md) with the D9 changes to `command` and `capabilities` plus the §6 env rows.
- Pins unchanged: exactly one `ethernet` `OUTPUT` pin at `10.99.0.12` into the bridge. No `video` pin — the clip is baked in (research note §1).
- Execution split for the planner: image build/push and node-config values are [[car-sky]]-executable; blueprint edits and deploy/verify clicks stay user Nydus UI steps.

## 10. Acceptance traceability

| Acceptance ([milestone1.md](../../../plans/milestone1.md) §5) | Closed by |
|---|---|
| Phase 2 — store exposes all R3 fields; detector-shaped and relayed-shaped entries enter through the identical interface | frozen R3 binding + `store/track_store` with both parsers writing the same `upsert` (D2, D3); `tests/store/test_track_store.cpp` |
| Phase 2 — mock-driven transitions observable and matching the R13 diagram; mock off ⇒ no tracks | D3 machine + `track_transition` events (D8); `DETECTOR_ENABLED=false` (D2) |
| Phase 2 — C admitted only within `gate_enter`, dropped only beyond `gate_exit` or after `miss_limit`, no flicker | D3 hysteresis band + `TRACK_TIMEOUT_MS`; `tests/store/test_admission_relayed.cpp` boundary cases |
| Phase 2 — gate constants from configuration, no literals | §6 env table; `config/config.cpp` as the only env reader |
| Phase 2 — CRA database schema committed | `schema/cra-assessment-record.schema.json` + `cra/assessment_db` (D4) |
| Phase 2 — video-input proposal sent to FPT-Mentor | already produced: research note §3/§4 (delivery is a user action) |
| Phase 3 — detection log with per-frame objects and distance estimates | D6 detector + `[EVT]` `own_sensor_ingest` payloads |
| Phase 3 — entries enter through the same R3 interface, `source = own_sensor` | D6 emission via the frozen binding → `r3_parser` → the same `upsert` |
| Phase 3 — **zero detections labeled C** | D6 structural argument + `tools/check_zero_c.py` |
| Phase 3 — CPU-only, offline pace acceptable | ONNX Runtime CPU at `DETECTOR_FRAME_STRIDE` 4 ⇒ 5 Hz effective (research note §3) |
| Phase 4 — C tracked with `source = v2x_relayed` only, full R13 lifecycle | `r2_parser` sets the source; the store is the sole `state` writer (D3) |
| Phase 4 — the NLOS plugin registers through the CRA interface; abstraction + DB schema are the artifacts | D4 interface + registry + `builtin_plugins.cpp`; `tests/cra/test_registry.cpp` |
| Phase 4 — at least one R4 warning per scenario run with risk state and composed geometry | D5 band table over `default.yaml` (a `low → medium → high` progression) + D7 emission |
| Phase 4 — the event list reconstructs a full run offline | D8 stream + `tools/event_report.py` |
| R15/R19 — pcap of ADA→IVI traffic | D9 capture on this node |

## 11. Open items and flags

| # | Item | Owner / closes at |
|---|---|---|
| 1 | **`miss_limit` semantics changed in form, not intent** — [milestone1.md](../../../plans/milestone1.md) §4 words M as "consecutive missed updates (proposed 5)"; D3 realizes it as `TRACK_TIMEOUT_MS` wall-clock (default 1000 ms = 5 periods at the slower source). Flagged for re-ratification, not absorbed silently. | user / project-planner (plan §4 wording) |
| 2 | **Deleting `ada-ecu/` removes ~1,900 lines of committed work** (D1). The rule, not this design, makes `ADA_ECU/` canonical, but the deletion should carry the user's explicit go-ahead in the subtask that performs it. | user, at planning |
| 3 | **Distance accuracy is unvalidated** — the pinhole known-width estimate (D6) has no calibration target in a user-supplied clip. `VEHICLE_WIDTH_M` / `CAMERA_HFOV_DEG` are proposals; if the delivered clip's geometry makes B's estimated range disagree with the scenario, the two constants are retuned, never the gate. | Phase 3, on the real clip |
| 4 | **IVI-side defect found, not designed here (Phase 5's work):** `IVI_ECU/app/.../model/R4WarningMessage.kt` on this branch declares `R4Geometry(ego, b, c)` with no `@SerialName`, so it cannot decode this node's `geometry.vehicleB`/`vehicleC`, and it requires a `trackedObjects` array this design does not emit. **`R4Message.kt` on main (sealed `R4WarningEvent`/`R4StateMessage` + `SceneGeometry.kt`) is the binding the IVI uses**; the branch's parallel model is superseded. | project-planner → Phase 5 |
| 5 | **Two committed binaries** — `models/yolo11n.onnx` (~10 MB) and `media/ego-b-occluding-c.mp4` (≤ 60 MB). Both must be in the build context because a Container Node has no volume. Alternative (build-time download) costs reproducibility and network at build. Flagged for the user's call on repo size. | user |
| 6 | **Detector wheel availability on `linux/arm64`** — `onnxruntime` and `opencv-python-headless` are installed at image build for aarch64; a missing wheel means a source build inside QEMU. To be proven by the first ADA image CI lane, not assumed. | Phase 3 CI lane |
| 7 | **`presentation/ada/ada-phase2-3-4-deck.*` on this branch describes the superseded implementation** (it documents the `ada-ecu/` design). Deck ownership is out of this run's scope; refreshing or dropping it is a separate decision. | user / project-planner |
| 8 | The clip itself is still outstanding — the user must supply `media/ego-b-occluding-c.mp4` per research note §4 before Phase 3 can produce R12 evidence; `tools/make_sample_video.py` is a CI fixture and forfeits that evidence. | user |
