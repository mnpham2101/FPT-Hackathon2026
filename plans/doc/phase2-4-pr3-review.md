# PR #3 review — `feat/phase2-ada-scaffold` (ADA Phases 2–4)

Reviewed 2026-08-02 against `origin/main` at merge-base `68ef5f5`. 60 files, +3085/−3. Head commit `0af502d`.

Scope of this document: **findings only** — nothing was fixed, replanned, or committed. Acceptance is judged against [milestone1.md](../milestone1.md) Phases 2–4 and the Phase 0 frozen contracts under [contracts/](../../contracts/).

## Verdict

**Do not merge as-is.** The branch delivers a working single-node ADA skeleton with a real R13 gate, a UDP R2 → store → risk → R4 chain, and JSONL evidence logs. But it builds a **parallel, divergent copy of the frozen R1–R6 contracts** in a **non-sanctioned folder**, and its ADA→IVI wire format does not match the IVI decoder it ships in the same PR. Two defects make the artifact non-functional on the target platform (Linux container, CarSky node), and neither was caught because the test gate is inert.

Nothing here is unrecoverable — the C++ core is decent and mostly needs re-homing and re-pointing at the frozen schemas.

## Blocking defects

### B1 · The ADA→IVI wire format does not match the IVI decoder in this same PR

The C++ emits `geometry.vehicleB` / `geometry.vehicleC` ([warning_builder.cpp:51-55](../../ada-ecu/src/warning_builder.cpp#L51-L55)), matching frozen [r4-ada-ivi.schema.json](../../contracts/r4-ada-ivi.schema.json). The Kotlin decoder added in this PR declares the fields as `b` and `c` ([R4WarningMessage.kt:40-43](../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/model/R4WarningMessage.kt#L40-L43)).

`b` is non-nullable with no default, so kotlinx.serialization throws `MissingFieldException` on every real ADA datagram. The IVI never renders a warning.

The unit test passes only because its fixture was hand-written with `b`/`c` and `riskState: "warning"` — a value ADA never emits ([R4WarningMessageTest.kt](../../IVI_ECU/app/src/test/java/com/hackathon/v2x/ivi/model/R4WarningMessageTest.kt)). This is a false-green test that certifies a broken integration.

### B2 · The container image cannot build on Linux

[track_store_tests.cpp:44](../../ada-ecu/tests/track_store_tests.cpp#L44) opens the event log at `/private/tmp/ada_core_tests.jsonl` — a macOS-only path. On `debian:12-slim` the `EventLogger` constructor throws, the test binary terminates non-zero, and [Dockerfile:18](../../ada-ecu/Dockerfile#L18) runs `ctest` inside the build stage. `docker build` fails.

Compounding it: the Dockerfile builds `-DCMAKE_BUILD_TYPE=Release`, which defines `NDEBUG` and **compiles out every `assert()` in the test file**. Once B2's path is fixed the suite will pass while verifying nothing. The whole test file is `assert()`-based with no framework.

[README.md](../../ada-ecu/README.md) instructs `brew install cmake nlohmann-json`. Linux is a hard constraint ([solution-selection-criteria](../../.claude/rules/solution-selection-criteria.md)); the branch was evidently only ever built on macOS, which [docs/phase2_acceptance.md](../../ada-ecu/docs/phase2_acceptance.md) concedes ("Linux/ARM container build path — Ready, not locally verified").

### B3 · A third, divergent copy of the frozen contracts

`ada-ecu/schemas/` holds hand-written re-derivations of R2/R3/R4 that are **not** in [sync-manifest.json](../../contracts/sync-manifest.json). `contracts/check_sync.py` reports OK because it never looks at them — the integrity gate is bypassed, not broken. The authoritative copies at `ADA_ECU/contracts/` are untouched and unused.

Every divergence below is a behaviour change against a frozen contract:

| Field | Frozen contract | PR copy | Consequence |
|---|---|---|---|
| all objects | unknown fields tolerated (additive evolution) | `additionalProperties: false` | any additively-evolved message is hard-rejected |
| R2 `sender.speed` | `["number","null"]` — null until two CPMs (F1) | `number`, non-null | first messages of every run rejected |
| R2 `object.confidence` | nullable — null when `ConfidenceLevel = 101` (F6) | `number`, non-null | valid unavailable-confidence messages rejected |
| R2 `object.position.confidence` | accuracy **in metres**, no upper bound (F6) | `maximum: 1` | any accuracy worse than 1 m rejected; also mis-typed as a probability |
| R2 `timeOfMeasurement` | −2048…2047 | unbounded | out-of-range values silently accepted |
| R2 `stationId` | max 4294967295 | unbounded here, `get<int>()` in code | see M1 |
| R2/R3 `classification`/`class` | free string | `const: "vehicle"` | forecloses any other class |
| R3 `position` | requires `x`, `y` only | additionally requires `confidence` | a conformant R3 object from the detector or V2X is rejected |
| R4 | `oneOf` warningEvent \| **stateMessage** | warning only | R15 periodic awareness state has no schema and no implementation |
| R4 `geometry.vehicleC` | nullable — null until C is first tracked | required non-null point | cannot express "C not yet relayed" |
| R4 `schemaVersion` | `minimum: 1` | `const: 1` | blocks any version bump |
| R4 `trackedObjects` | absent | new required-shaped array | unratified contract extension (see M4) |

### B4 · The deployed node exits immediately

[main.cpp:112-115](../../ada-ecu/src/main.cpp#L112-L115): with none of `--mock` / `--listen-once` / `--max-r2`, the process prints "scaffold ready" and returns 0. The Dockerfile's `CMD` is exactly that case ([Dockerfile:33](../../ada-ecu/Dockerfile#L33)). A CarSky Container Node deployed from this image terminates on start and never reaches Running.

There is also no long-running mode at all — `--max-r2 <n>` processes n datagrams and exits. The node has no service loop.

## Major issues

### M1 · R2 → R3 mapping loses and inverts data

[r2_mapper.cpp](../../ada-ecu/src/r2_mapper.cpp):

- **Timestamps are swapped** (line 50): `measured` gets local wall-clock, `received` gets the V2X ECU's `rxTime`. `timeOfMeasurement` — the only field that carries measurement time — is never read. R18 offline reconstruction reads these.
- **`stationId` is parsed as `int`** (line 38). The contract allows the full uint32 range; `get<int>()` narrows, so any station ID above 2³¹ produces a negative, wrong track ID.
- **A null `confidence` kills the message** (line 49). `value("confidence", 0.0)` on a JSON `null` throws `type_error.302`, caught at line 52, returning `nullopt` — the whole R2 message is dropped and logged as `invalid_r2_v2x_object`. F6 makes null a normal value.
- **`sender.lat/lon/heading` are never read.** The relayed object's position stays in **B's frame** but is stored into `TrackedObject.position`, which R3 defines as *ego frame*, and B→C range is stored as `distance` = "range from ego". Every downstream consumer — the R13 gate, the risk threshold, the composed geometry — is therefore comparing B-frame quantities against ego-frame thresholds.

### M2 · Scene composition is inconsistent and heading-blind

[warning_builder.cpp:32-38](../../ada-ecu/src/warning_builder.cpp#L32-L38) computes `c_x = d_AB + d_BC` from **scalar distances** but `c_y = b_y + object.position.y` from **components**. The plan specifies `d_AC = d_AB + d_BC` with "lateral offsets component-wise" under a near-collinear assumption, so the x-axis is defensible — but mixing the two makes the result inconsistent whenever B is laterally offset, and `vehicleB.x` is set to the scalar `d_AB` rather than `own_b->position.x`. B's heading is never applied, so the B-frame → ego-frame rotation is simply absent.

### M3 · Hand-rolled JSON serialisation

[warning_builder.cpp:8-24](../../ada-ecu/src/warning_builder.cpp#L8-L24) builds the R4 message with `ostringstream` while `nlohmann::json` is already a linked dependency. Consequences: default 6-significant-digit precision on all doubles; `nan`/`inf` would emit invalid JSON; `id` and `class` are interpolated without escaping. Same pattern in [event_logger.cpp:18](../../ada-ecu/src/event_logger.cpp#L18) and the `track_transition` log strings.

### M4 · Unratified contract extension pushed into the IVI

`trackedObjects` is invented here, added to the PR's R4 schema, emitted by the C++, and consumed by the Kotlin `toSceneGeometry()`. The frozen R4 tolerates unknown fields, so it is *legal* — but it is a cross-track contract change made unilaterally by the ADA branch, and this PR is the one that also edits `IVI_ECU/` to depend on it. That is Phase 5's folder, with three live IVI branches (`feat/phase5-ivi-hmi`, `-dev`, `-complete`) that will conflict. The PR's own deck slide lists "Phase 5 IVI app rendering and listener implementation" as out of scope.

### M5 · The R13 state machine deviates from the plan without a recorded decision

[milestone1.md § Track admission gate](../milestone1.md) fixes `confirm_hits` (N) = proposed 3 and `miss_limit` (M) = proposed 5 **consecutive missed updates**. The implementation uses `tentative_hits = 2` and `miss_limit_ms = 1500` — a wall-clock timeout, not a miss count. The constants are properly externalised, but the semantics changed.

Also in [track_store.cpp:84-93](../../ada-ecu/src/track_store.cpp#L84-L93): `tentative_hits_` only ever increments and is never reset by `expire()`/`expire_source()`. Once a track has been Tracked, a single post-expiry hit re-promotes it straight to Tracked with no re-confirmation — the tentative state is unreachable for the rest of the run.

### M6 · The risk assessor is the admission gate a second time

[main.cpp:109](../../ada-ecu/src/main.cpp#L109) constructs `NlosRiskAssessor(config.gate_enter_m)`, and [risk_assessor.cpp:18](../../ada-ecu/src/risk_assessor.cpp#L18) warns when `state == Tracked && distance <= risk_distance_m_`. Since a v2x track is only Tracked when it is inside the gate, the risk condition is a tautology — risk is not independently assessed. `speed` is never used. `riskState` maps to `"high"`/`"low"` only; `"medium"` is unreachable.

The `CollisionRiskAssessor` interface exists but there is no registry — `NlosRiskAssessor` is constructed directly, so "the NLOS plugin registers through the CRA interface" (Phase 4 AC) is not demonstrated.

### M7 · The event log is corruptible by a malformed datagram

[v2x_r2_ingest.cpp:19](../../ada-ecu/src/v2x_r2_ingest.cpp#L19) writes the raw payload into the JSONL `payload` slot **before** validating it. Any non-JSON datagram on port 46002 produces a syntactically invalid line, breaking offline reconstruction of the run (R18).

### M8 · The IVI listener dies on the first non-warning datagram

[R4UdpListener.kt:24-25](../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/net/R4UdpListener.kt#L24-L25) decodes before checking `type`, so the `type: "state"` messages the frozen R4 contract permits throw `MissingFieldException`. The exception escapes `listen()`, is logged once in `MainActivity`, and the coroutine is never restarted — the app is deaf for the remainder of the session. Contract decision D4 requires graceful degradation on unknown content.

## Minor issues

- **No hostname resolution.** [udp_r4_sender.cpp:25](../../ada-ecu/src/udp_r4_sender.cpp#L25) and the receiver use `inet_pton` only, so `ivi_host` must be an IPv4 literal; a CarSky service name throws.
- **Silent truncation.** The 8192-byte `recvfrom` buffer never checks `MSG_TRUNC`; an oversized datagram is silently cut and then fails to parse.
- **Duplicate env var.** [config.cpp:95-96](../../ada-ecu/src/config.cpp#L95-L96) has both `ADA_LISTEN_PORT` and `V2X_LISTEN_PORT` writing `ada_listen_port`, with the second silently winning. Undocumented.
- **No config validation.** Unknown keys are ignored; a malformed value makes `std::stod`/`stoi` throw out of `load_config`, and `main` has no handler, so the node aborts instead of reporting.
- **`#` strips mid-value.** [config.cpp:55-58](../../ada-ecu/src/config.cpp#L55-L58) truncates at the first `#` anywhere on the line.
- **`errno` used without `<cerrno>`** in [udp_r2_receiver.cpp:39](../../ada-ecu/src/udp_r2_receiver.cpp#L39).
- **Cross-arch Dockerfile bug.** [Dockerfile:1-3](../../ada-ecu/Dockerfile#L1-L3) pins the build stage to `$BUILDPLATFORM` and declares `ARG TARGETARCH` but never uses it, while the runtime stage is target-platform. On a cross-build the image carries a wrong-architecture binary.
- **Test fixtures shipped in the runtime image** ([Dockerfile:30](../../ada-ecu/Dockerfile#L30)), and `main.cpp` defaults point at repo-relative paths (`ada-ecu/testdata/...`) that do not exist inside the image.
- **Stale README.** "replace it with `nlohmann/json` once dependency packaging is finalized" — nlohmann is already the parser.
- **The detector emits `state`.** [video_detector.py:51](../../ada-ecu/tools/video_detector.py#L51) sets `"state": "tentative"`; track state belongs to the R13 machine, not a perception source.

## Undone work against the plan's acceptance criteria

### Phase 2 — ADA scaffolding (R3, R13)

| Criterion | Status |
|---|---|
| Store exposes all R3 fields; both source shapes enter through one interface | **Done** |
| Mock-driven transitions observable in logs, match the R13 diagram | **Partial** — logged, but N/M semantics changed (M5) and the diagram's tentative path is unreachable after first promotion |
| C admitted only within `gate_enter`, dropped beyond `gate_exit` or after `miss_limit`, no flicker | **Partial** — hysteresis correct; `miss_limit` is time-based, not miss-count |
| Gate constants read from configuration | **Done** |
| **CRA database schema committed** | **Not started** — no schema anywhere in the branch |
| **Video-input study; format/frame-rate/data-rate proposal sent to FPT-Mentor** | **Not started** |
| Demo: build + CI round-trip green **on the frozen contracts (golden vectors)** | **Not met** — build fails on Linux (B2); tests use `ada-ecu/testdata/`, never `contracts/golden-vectors/` or the manifest-synced fixtures |

### Phase 3 — Object detection from video (R12)

| Criterion | Status |
|---|---|
| YOLO11n → ONNX on ONNX Runtime CPU; OpenCV decode | **Not started** — `--backend` accepts only `placeholder`; `requirements.txt` has no `onnxruntime` |
| Per-frame detection + distance estimation | **Not started** — [video_detector.py:38](../../ada-ecu/tools/video_detector.py#L38) returns `12.0 + frame_index * 0.05`, ignoring pixels entirely; frames are decoded and discarded |
| R3 JSONL streamed over stdout into the store (subprocess contract) | **Not started** — the C++ reads a **static file once at startup** ([detector_jsonl_ingest.cpp](../../ada-ecu/src/detector_jsonl_ingest.cpp)); no pipe, no streaming. Own-sensor tracks are frozen for the whole run |
| Detection log over the provided clip | **Not started** — no clip acquired; no CarSky dashcam source investigated |
| **Zero detections labeled C** — checked on the log | **Not started** — no check exists (this feeds R19) |

### Phase 4 — Fusion, risk, warning (R13–R15)

| Criterion | Status |
|---|---|
| C admitted per R13 from R2 `distance`, `source = v2x_relayed` only | **Done** |
| CRA abstraction + NLOS plugin **registered through it**, reading/writing the Phase 2 DB schema | **Partial** — interface exists; no registry, no database |
| Scene composition (ego, B, ghost C) | **Partial** — see M2 |
| Edge-triggered R4 warning on risk transitions | **Done** |
| Periodic awareness state (R15, optional) | **Not started** — no `state` message schema or emitter |
| At least one R4 warning per scenario run carrying risk state + composed geometry | **Not verifiable** — no bench scenario has been run against this node |
| Event list reconstructs a full run offline (R18) | **Partial** — logger works but is corruptible (M7) and carries no schema |
| **Confirm output: `trackedObjects` of B and C received at IVI-ECU, evidenced by logs or Wireshark** | **Not met** — B1 means the IVI cannot decode the message |

## Process and convention violations

1. **Wrong folder.** All code is in `ada-ecu/`. [node-code-layout.md](../../.claude/rules/node-code-layout.md) fixes exactly four code folders; the ADA node is `ADA_ECU/`. The branch leaves the real `ADA_ECU/` (with its Phase 0 contract copies and round-trip tests) orphaned and duplicates it.
2. **Wrong doc location.** Design notes are in `ada-ecu/docs/`; the convention is the node's `doc/` subfolder.
3. **Contract copies outside the manifest** — B3.
4. **No task decomposition.** `plans/phase2_tasks.md` does not exist. Work proceeded without a project-planner breakdown, so no subtask has an `X.Y.Z.W` ID.
5. **Malformed commit tags.** `[15.4]`, `[12.3]`, `[3.2]` are two-segment IDs, not `X.Y.Z.W`. Five commits carry no tag at all (`Merge code`, `Implement UDP R2 receiver for ADA`, `detector JSONL input seam cho Phase 3.`, `Create checklist file...`, `Add presentation and report for ada`), against [task-planning-conventions](../../.claude/rules/task-planning-conventions.md).
6. **Non-atomic commits.** `Add presentation and report for ada` bundles a 256-line deck, an 88-line HTML export and a report; `Merge code` mixes merge and content.
7. **Deck placement.** `presentation/ada/` with slug `ada-phase2-3-4-deck`; [deck-authoring-conventions](../../.claude/rules/deck-authoring-conventions.md) requires `presentation/phase<N>/` and a `phase<N>-` slug. The HTML is also not reproducible — `presentation/slide-build-tool/build-slides.py` does not exist anywhere in the repo, on this branch or on main, so the export was hand-authored.
8. **Cross-track edit.** `IVI_ECU/` changes belong to Phase 5 — M4.
9. **`.gitignore` pollution.** `.temp_ag_kit/`, `.agents/.ag-kit/…`, `.temp_backup/` are one contributor's local tooling, unrelated to this project's toolchain.

## Suggested order of repair

1. **B3 + violation 1 together** — move the code into `ADA_ECU/`, delete `ada-ecu/schemas/`, and point the C++ at the manifest-synced `ADA_ECU/contracts/` copies. Everything else is cheaper after this.
2. **B1** — change the Kotlin to `vehicleB`/`vehicleC`, make `vehicleC` nullable, and rebuild the test fixture from `contracts/samples/r4-warning.json` instead of hand-writing it.
3. **B2** — make the test log path a build-time temp dir, move `ctest` out of the `NDEBUG` build or add a `Debug` test stage, and drop the `brew` instructions.
4. **B4** — add a real service loop and make it the container's default mode.
5. **M1/M2** — decide where the B-frame → ego-frame transform lives, then fix the mapper and the composition together; they are one change.
6. Everything else can follow in the Phase 2–4 replan.

## What is genuinely good here

- The R13 enter/exit hysteresis is correct and the constants are properly externalised — no literals in logic.
- The `own_sensor` / `v2x_relayed` split is respected end-to-end; `expire_source` correctly preserves own-sensor tracks on a V2X timeout, which is exactly the R19-relevant behaviour.
- Edge-triggered emission (state-change only, no repeats) matches R15's intent.
- The JSONL event logger is the right shape for R18.
- `tools/mock_v2x_sender.py` and `tools/mock_ivi_receiver.py` are useful bench equipment worth keeping.
