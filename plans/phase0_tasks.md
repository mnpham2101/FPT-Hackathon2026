# Phase 0 — Contract Freeze (R1–R6): Full Task Breakdown

> **Authority & context:**
> - **Phase content:** [milestone1.md § Phase 0](milestone1.md#phase-0--freeze-the-contracts-r1r6) — its four acceptance checkboxes are the phase output.
> - **Design:** [phase0-contract-freeze-hld.md](doc/phase0-contract-freeze-hld.md) (commits `d807c37` + `70796c0`) — every target path below is cited verbatim from its §5 folder map; design decisions D1–D4, seam shape §7, conventions §4.
> - **Requirements:** [m1-cooperative-awareness.md §2](../requirements/m1-cooperative-awareness.md) R1–R6 — referenced by number, never restated.
> - **Smoke-test spec:** [baseline-connectivity-smoke-test.md](doc/research_notes/baseline-connectivity-smoke-test.md) — objective, pass criteria C1–C5, tool contents §4, manual steps M1–M12 + node config §6, IVI hop §7, troubleshooting §8, open items O1–O4. Group 0.8 derives from it; nothing is restated.
> - **R1 profile skeleton:** [scenario-player-v2x-callflow-messages.md](../Scenario_Player/doc/research_notes/scenario-player-v2x-callflow-messages.md) §4 + findings F1–F9.
> - **Rules:** [task-planning-conventions.md](../.claude/rules/task-planning-conventions.md) (`X.Y.Z.W`; subtask discipline is every subtask's definition of done, restated once in § Subtask discipline below).
>
> **Task ID legend:** `X.0.Z.W` — X = requirement served · 0 = this phase · Z = task group · W = subtask. IDs are stable; never renumber.

## Phase 0 overview

**Objective.** Freeze the cross-track contracts R1–R6 before dependent work: R1 profile + golden vectors through the Vanetza codec seam; R2/R3/R4 schemas + per-language bindings + round-trip tests + the R4 additive-version test; R5/R6 proven live by the baseline-blueprint connectivity smoke test.

**Input (must exist before start — all present as of 2026-07-31):**

- Requirement definitions R1–R6 in the report §2; Phase 0 HLD committed (`d807c37`, `70796c0`).
- Baseline blueprint documented and saved on CarSky as `trial2_minh` ([carsky-4-node-blueprint.md](../requirements/car-sky-guide/carsky-4-node-blueprint.md) + [blueprint-m1-cooperative-awareness.json](../requirements/car-sky-guide/blueprint-m1-cooperative-awareness.json)) — Phase 0 adds no topology design.
- Existing `IVI_ECU/` Gradle project with interim `model/R3Snapshot.kt` and `model/SceneGeometry.kt` (kotlinx.serialization + JUnit4 configured).

**Output (phase acceptance = the four milestone boxes):**

- [ ] R1 profile document committed; golden-vector CPMs encode/decode through the Vanetza codec seam.
- [ ] R2, R3, R4 schemas committed; round-trip tests pass in each consumer language (C++ / Python / Kotlin).
- [ ] The R4 additive-version test is defined (unknown `warningType` degrades gracefully).
- [ ] Blueprint topology documented (pre-existing guides) + validated: smoke-test C1–C5 green on `trial2_minh`.

Traceability of every subtask to these boxes: § Acceptance traceability.

**Suggested branch (suggestion only — creation is the user's call):** `feat/phase0-contract-freeze`

### Execution split legend

| Label | Meaning |
|---|---|
| *agent* | implemented by a spawned implementation subagent (default) |
| *car-sky* | executed by the [[car-sky]] agent (deploy preflight → build/push/deploy/verify); planner keeps the ID and done-tracking |
| *USER-MANUAL* | Nydus UI steps performed by the user; the plan tracks them, no agent performs them; the evidence-record commit is made by the orchestrating session after the user confirms |

### Subtask discipline (applies to every subtask below)

Per [task-planning-conventions.md § Subtask discipline](../.claude/rules/task-planning-conventions.md#subtask-discipline-non-negotiable): single objective, no out-of-scope code, exactly one atomic commit with the stated message, build passes, unit tests pass, brief is self-contained. Implementation subagents inherit this as their definition of done. C++ builds run on Linux — Vanetza and the pinned FetchContent toolchain are Linux-targeted per [solution-selection-criteria.md](../.claude/rules/solution-selection-criteria.md); the dev host has no Docker/WSL (verified 2026-07-31), so Linux verification runs on GitHub Actions CI (subtask 1.0.7.2), where the C++ lanes (groups 0.2–0.4) verify once their toolchains land.

**Status tracking:** as execution proceeds each subtask gains a `**Status:**` line (appended in that subtask's own atomic commit) recording done/blocked plus the verification evidence; a subtask without a status line is not started.

### Per-node build commands (cited in acceptance below)

| Node / area | Build + test command |
|---|---|
| `V2X_ECU/` | `cmake -S V2X_ECU -B V2X_ECU/build && cmake --build V2X_ECU/build -j && ctest --test-dir V2X_ECU/build --output-on-failure` |
| `ADA_ECU/` core | `cmake -S ADA_ECU -B ADA_ECU/build && cmake --build ADA_ECU/build -j && ctest --test-dir ADA_ECU/build --output-on-failure` |
| `ADA_ECU/detector/` | `pip install -r ADA_ECU/detector/requirements-dev.txt && python -m pytest ADA_ECU/detector/tests` |
| `Scenario_Player/` | `pip install -r Scenario_Player/requirements-dev.txt && python -m pytest Scenario_Player/tests` |
| `IVI_ECU/` | from `IVI_ECU/`: `./gradlew :app:testDebugUnitTest` (`gradlew.bat` on Windows) |
| `contracts/` gate | `python contracts/check_sync.py` → exit 0 |
| `tools/netcheck/` | `docker build -t m1-netcheck:latest tools/netcheck/` |

---

## Task Group 0.1 — Contract source of truth: R1 profile + R1–R4 schemas + shared samples (`contracts/`)

> Authors every artifact of the user-approved top-level `contracts/` folder (HLD D1) except the golden vectors (generated in group 0.2) and the sync gate (group 0.7). All contract JSON uses JSON Schema draft 2020-12 (HLD §8).

### `1.0.1.1` — Author the R1 CPM profile document *(agent)*

**Objective:** write `contracts/r1-cpm-profile.md`, the versioned normative R1 profile.

**Scope:**

- Structure and field/unit/encoding table from the callflow note [§4.1–§4.2](../Scenario_Player/doc/research_notes/scenario-player-v2x-callflow-messages.md) (2 containers, 1 perceived object; ASN.1 paths, types, units, ranges).
- Freeze the eight conventions exactly per [HLD §4](doc/phase0-contract-freeze-hld.md): F1 (`sender.speed` nullable, derived at V2X ECU), F2 (`vanetza::asn1::r2::Cpm` only), F5 (raw UPER, one PDU per UDP datagram, no GN/BTP), F6 (confidence conversions; `101 → null`; position confidence in metres), F7 (`object.distance = hypot(x, y)`, derived in R9, never transmitted), F8 (default 10 Hz via `cpm_rate_hz` config, never a literal), F9 (`measurementDeltaTime` ±2047 ms; bench validates pre-encode, R9 rejects + counts), velocity in the sender (B) cartesian frame.
- Exchange call flow: callflow note §2 — § B is the sole live flow; unidirectional (R10 deferred).
- Fix the golden-vector corpus list (HLD D3, 6 cases): `nominal` · `mdt-max` · `mdt-min` · `conf-unavailable` · `gate-boundary` · `coord-large`.
- Sample values use the F7-derived distance (`hypot(25.0, 1.2) = 25.03`), not the report's `25.4` — report erratum stays flagged to [[project-researcher]] (HLD §11 items 3–4), never absorbed here.

**Acceptance:** file committed at `contracts/r1-cpm-profile.md`; all 8 conventions + the 6-case corpus list present; [markdown-writing-style](../.claude/skills/markdown-writing-style/SKILL.md) followed. Doc-only — no build/test target.

**Dependencies:** none — starts immediately. **Commit:** `[1.0.1.1] docs: author R1 CPM profile document with frozen conventions`

**Status:** done 2026-07-31 — committed; 8 conventions (F1/F2/F5/F6/F7/F8/F9/VF) + 6-case corpus with per-case parameters present; markdown style followed (no hard wrap, bullets/tables).

### `1.0.1.2` — Author the CpmContent logical-content schema *(agent)*

**Objective:** write `contracts/r1-cpm-content.schema.json` — JSON Schema mirroring the codec-seam struct `CpmContent` 1:1, wire-native integer units (the golden vectors' JSON side).

**Scope:** field set = the profile doc's table from 1.0.1.1 (callflow note §4.2): stationId; referenceTime; reference position lat/lon (10⁻⁷ °); orientationAngle (0,1 °); objectId; measurementDeltaTime (integer, −2048..2047); object position x/y (0,01 m) + coordinate confidence (1..4096); velocity x/y (0,01 m/s); classification + ConfidenceLevel (1..101). No SI floats, no derived fields (seam rule, HLD D3).

**Acceptance:** schema committed; `$schema` declares draft 2020-12; parses (`python -m json.tool`); bounds above encoded as JSON Schema constraints. Instance-validation lands with 1.0.5.1.

**Dependencies:** after 1.0.1.1. **Commit:** `[1.0.1.2] feat: add CpmContent JSON Schema mirroring the R1 codec seam`

**Status:** done 2026-07-31 — parses (python -m json.tool); Draft202012Validator.check_schema passes and the profile §4 nominal instance validates (jsonschema 4.26, Python 3.14); wire-native bounds encoded as constraints.

### `2.0.1.3` — Freeze the R2 schema + shared sample *(agent)*

**Objective:** write `contracts/r2-v2x-object.schema.json` and `contracts/samples/r2-object.json`.

**Scope:** fields per report §2 R2 (`schemaVersion`, `type: "v2x_object"`, `stationId`, `rxTime`, `sender{lat, lon, heading, speed}`, `object{objectId, timeOfMeasurement, distance, position{x, y, confidence}, speed, classification, confidence}`) with two frozen deviations: `sender.speed` nullable (F1); sample `object.distance = 25.03` (F7-derived), not the report's `25.4` (erratum flagged, HLD §11 item 3).

**Acceptance:** schema + sample committed; both parse; schema-validation of the sample lands with 3.0.4.5.

**Dependencies:** after 1.0.1.1 (F1/F7 conventions frozen first). **Commit:** `[2.0.1.3] feat: freeze R2 v2x-object schema and shared sample`

**Status:** done 2026-07-31 — schema+sample parse; check_schema passes; sample (and its sender.speed=null variant) validates against the schema (jsonschema 4.26, Python 3.14); distance 25.03 = hypot(25.0, 1.2) (F7), sender.speed nullable (F1).

### `3.0.1.4` — Freeze the R3 schema + shared sample *(agent)*

**Objective:** write `contracts/r3-tracked-object.schema.json` and `contracts/samples/r3-tracked-object.json`.

**Scope:** fields per report §2 R3: `id`, `class`, `source` (`own_sensor` | `v2x_relayed`), `position{x, y}` (ego frame, m), `distance`, `speed`, `confidence` (0–1), `state` (`not_tracked` | `tentative` | `tracked`), `timestamps` (measured / received / last-updated). Note for the Kotlin consumer: `class` and `timestamps` are the two fields the interim `IVI_ECU` models lack (§ IVI reconciliation).

**Acceptance:** schema + sample committed; both parse; sample carries `source: "v2x_relayed"` (the ghost-C shape).

**Dependencies:** none — parallel with the R1/R2 chain. **Commit:** `[3.0.1.4] feat: freeze R3 TrackedObject schema and shared sample`

**Status:** done 2026-07-31 — schema+sample parse; check_schema passes; sample validates and carries source v2x_relayed (ghost-C shape), distance 55.03 = hypot(55.0, 1.7) coherent with the R2 sample (jsonschema 4.26, Python 3.14).

### `4.0.1.5` — Freeze the R4 schema + nominal shared samples *(agent)*

**Objective:** write `contracts/r4-ada-ivi.schema.json`, `contracts/samples/r4-warning.json`, `contracts/samples/r4-state.json`.

**Scope:** per report §2 R4 — warning event (`schemaVersion`, `type: "warning"`, `warningType` (M1: `nlos_obstruction`), `riskState`, `object` = R3 snapshot as `$ref` to `r3-tracked-object.schema.json` (HLD §5), `geometry` = ego/B/C relative positions) + optional state message (`schemaVersion`, `type: "state"`, `seq`, `vehicles`). `geometry` field names must match the existing `IVI_ECU` `SceneGeometry.kt` (`ego`, `vehicleB`, `vehicleC` nullable — § IVI reconciliation).

**Acceptance:** schema + 2 samples committed; all parse; the warning sample's `object.source` is `v2x_relayed`.

**Dependencies:** after 3.0.1.4 (`$ref`). **Commit:** `[4.0.1.5] feat: freeze R4 ADA-IVI schema and shared samples`

**Status:** done 2026-07-31 — schema+2 samples parse; check_schema passes; both samples validate with the R3 $ref resolved via a referencing registry (jsonschema 4.26, Python 3.14); warning object.source = v2x_relayed and equals the shared R3 sample; geometry names ego/vehicleB/vehicleC match SceneGeometry.kt.

### `4.0.1.6` — Author the shared R4 additive-version fixture *(agent)*

**Objective:** write `contracts/samples/r4-unknown-warning.json` per HLD D4 — one fixture both consumers test.

**Scope:** a valid warning event with a **higher** `schemaVersion` than 4.0.1.5's, an unknown `warningType` string, and exactly one unknown extra field.

**Acceptance:** fixture committed; parses; differs from `r4-warning.json` only in the three D4 aspects.

**Dependencies:** after 4.0.1.5. **Commit:** `[4.0.1.6] feat: add shared R4 additive-version fixture`

**Status:** done 2026-07-31 — fixture parses and still validates against the R4 schema; programmatic diff proves exactly the three D4 deltas vs r4-warning.json (schemaVersion 2, warningType slippery_road, extra field hazardDetail).

---

## Task Group 0.2 — V2X ECU: toolchain, R1 codec seam, golden vectors (serves R1)

> Delivers the single R1 codec source behind the seam (HLD D3, §7) and the committed golden-vector corpus. All paths inside `V2X_ECU/`; build command per § Per-node build commands.

### `1.0.2.1` — V2X_ECU CMake toolchain bring-up *(agent)*

**Objective:** create `V2X_ECU/CMakeLists.txt` — C++17; nlohmann/json + Vanetza **ASN.1-only** targets + GoogleTest, all via **pinned** FetchContent (exact tags/commits, no floating branches); CTest wired.

**Scope:** no GN/BTP full-stack pull-in (HLD §8); one sanity test target proving GoogleTest runs and `vanetza::asn1::r2::Cpm` compiles. No product code.

**Acceptance:** V2X build command green; `ctest` runs the sanity test; pins are exact.

**Dependencies:** none — starts immediately. **Commit:** `[1.0.2.1] chore: bring up V2X_ECU C++17 toolchain with Vanetza ASN.1 targets`

### `1.0.2.2` — CpmContent + ICpmCodec seam *(agent)*

**Objective:** create `V2X_ECU/src/codec/cpm_codec.hpp` with `CpmContent`, `DecodeError`, and `ICpmCodec` **exactly** per the frozen shape in [HLD §7](doc/phase0-contract-freeze-hld.md#7-codec-seam-interface-frozen-shape), plus nlohmann `to_json`/`from_json` for `CpmContent`.

**Scope:** `CpmContent` fields mirror `contracts/r1-cpm-content.schema.json` 1:1 (wire-native integer units). Seam rule: pure representation transform — no unit conversion, no derivation (those are R9, above the seam). Land the synced copy `V2X_ECU/contracts/r1-cpm-content.schema.json` (byte-identical to source). Unit test: `CpmContent` ⇄ JSON ⇄ `CpmContent` equality.

**Acceptance:** V2X build + ctest green; interface text matches HLD §7; copy byte-identical.

**Dependencies:** after 1.0.1.2 + 1.0.2.1. **Commit:** `[1.0.2.2] feat: define CpmContent and ICpmCodec seam with JSON binding`

### `1.0.2.3` — VanetzaCpmCodec implementation *(agent)*

**Objective:** implement `V2X_ECU/src/codec/vanetza_cpm_codec.{hpp,cpp}` — the sole `ICpmCodec` implementation over `vanetza::asn1::r2::Cpm`.

**Scope:** bare `asn1::Cpm` is banned everywhere under `V2X_ECU/src/` (F2 — the 1.0.7.1 gate greps for it); `encode` throws on F9 bounds violation (|`measurementDeltaTime`| > 2047); `decode` returns `DecodeError` on malformed bytes, never crashes. Unit tests: encode→decode identity on a nominal in-code `CpmContent`; F9 violation throws; garbage bytes → `DecodeError`.

**Acceptance:** V2X build + ctest green; no unqualified `asn1::Cpm` token in the new sources.

**Dependencies:** after 1.0.2.2. **Commit:** `[1.0.2.3] feat: implement VanetzaCpmCodec over r2::Cpm`

### `1.0.2.4` — gv_tool + golden-vector corpus generation *(agent)*

**Objective:** create `V2X_ECU/tools/golden_vectors/main.cpp` (CMake target `gv_tool`, build-only, never shipped in a node image) and generate + commit the 6-case corpus into `contracts/golden-vectors/`.

**Scope:** `gv_tool` encodes CpmContent JSON → `.uper`, decodes back, asserts identity, writes `<case>.json` + `<case>.uper` pairs. Corpus per HLD D3 and the profile doc: `nominal` (F7-corrected R2 sample values from 2.0.1.3) · `mdt-max`/`mdt-min` (±2047 ms) · `conf-unavailable` (ConfidenceLevel 101) · `gate-boundary` (object at 30 m — the R13 admission seam) · `coord-large` (near CartesianCoordinateLarge bounds).

**Acceptance:** V2X build green; running `gv_tool` regenerates byte-identical vectors (determinism check: run twice, diff empty); 6 `.json`+`.uper` pairs committed under `contracts/golden-vectors/`.

**Dependencies:** after 1.0.2.3 + 1.0.1.1 (corpus list) + 2.0.1.3 (nominal values). **Commit:** `[1.0.2.4] feat: add gv_tool and generate the R1 golden-vector corpus`

### `1.0.2.5` — Golden-vector codec test *(agent)*

**Objective:** create `V2X_ECU/tests/codec/test_cpm_golden_vectors.cpp` + the synced pair copies under `V2X_ECU/tests/fixtures/golden/`.

**Scope:** for every corpus case: `decode(<case>.uper)` equals the `<case>.json` content, and `encode` of that content reproduces the `.uper` bytes. Copies byte-identical to `contracts/golden-vectors/`.

**Acceptance:** V2X build + ctest green over all 6 cases — this closes the milestone box "golden-vector CPMs encode/decode through the Vanetza codec seam".

**Dependencies:** after 1.0.2.4. **Commit:** `[1.0.2.5] test: verify golden vectors through the Vanetza codec seam`

---

## Task Group 0.3 — R2 bindings + round-trip tests, both C++ ends (serves R2)

> One handwritten binding per node (HLD D1/D2) — no cross-node source imports; wire compatibility is enforced by the byte-synced shared sample both tests parse.

### `2.0.3.1` — V2X-side R2 binding + round-trip test *(agent)*

**Objective:** create `V2X_ECU/src/contracts/r2_message.{hpp,cpp}` (nlohmann binding, producer side) + `V2X_ECU/tests/contracts/test_r2_roundtrip.cpp`.

**Scope:** fields per 2.0.1.3's schema (incl. nullable `sender.speed`, F1). Land synced copies: `V2X_ECU/contracts/r2-v2x-object.schema.json`, `V2X_ECU/tests/fixtures/samples/r2-object.json`. Test: shared sample → struct → JSON → struct equality; null `sender.speed` round-trips.

**Acceptance:** V2X build + ctest green; copies byte-identical; pure model code — no transport, no framework deps (HLD §9).

**Dependencies:** after 2.0.1.3 + 1.0.2.1. Parallel with the 1.0.2.x codec chain. **Commit:** `[2.0.3.1] feat: add V2X-side R2 binding with round-trip test`

### `2.0.3.2` — ADA-side R2 binding + round-trip test *(agent)*

**Objective:** create `ADA_ECU/src/contracts/r2_message.{hpp,cpp}` (nlohmann binding, consumer side) + `ADA_ECU/tests/contracts/test_r2_roundtrip.cpp`.

**Scope:** same contract fields as 2.0.3.1, handwritten independently (no import from `V2X_ECU/`). Land synced copies: `ADA_ECU/contracts/r2-v2x-object.schema.json`, `ADA_ECU/tests/fixtures/samples/r2-object.json`. Same test shape as 2.0.3.1.

**Acceptance:** ADA build + ctest green; copies byte-identical.

**Dependencies:** after 2.0.1.3 + 3.0.4.1 (ADA toolchain). Parallel with 2.0.3.1. **Commit:** `[2.0.3.2] feat: add ADA-side R2 binding with round-trip test`

**Status:** done 2026-07-31 — `ada::contracts::R2Message` consumer binding (nullable `sender.speed`/`object.confidence` via std::optional, F1/F6) + round-trip and null-speed tests; copies cmp-identical; verified green on CI run 30602159929. **Commit anomaly:** implementation landed in `dc0d424`, mis-tagged `[5.0.8.2]` — a concurrent-session `git commit` swept this subtask's already-staged files into its commit (index collision, recorded honestly; history not rewritten since pushed).

---

## Task Group 0.4 — ADA ECU contract layer: toolchain, R3/R4 bindings, additive test, detector Python (serves R3, R4)

> Phase 0 lands only the contract layer of `ADA_ECU/` (HLD §5) — the Phase 2 HLD extends the tree later.

### `3.0.4.1` — ADA_ECU CMake toolchain bring-up *(agent)*

**Objective:** create `ADA_ECU/CMakeLists.txt` — C++17; nlohmann/json + GoogleTest via pinned FetchContent; CTest; one sanity test. No Vanetza (ADA never touches UPER).

**Acceptance:** ADA build command green; ctest runs the sanity test; pins exact.

**Dependencies:** none — starts immediately. **Commit:** `[3.0.4.1] chore: bring up ADA_ECU C++17 toolchain`

**Status:** done 2026-07-31 — toolchain + sanity test committed; `ada-core-build` job (Configure/Build/Test steps) added to `phase0-ci.yml`; verified green on CI run 30591588639 (no local cmake on this host — CI is the Linux verification authority).

### `3.0.4.2` — R3 C++ binding + round-trip test *(agent)*

**Objective:** create `ADA_ECU/src/contracts/tracked_object.{hpp,cpp}` + `ADA_ECU/tests/contracts/test_r3_roundtrip.cpp`.

**Scope:** fields per 3.0.1.4's schema (incl. `class`, `source` enum, `state` enum, `timestamps`). Land synced copies: `ADA_ECU/contracts/r3-tracked-object.schema.json`, `ADA_ECU/tests/fixtures/samples/r3-tracked-object.json`. Test: shared sample round-trip equality.

**Acceptance:** ADA build + ctest green; copies byte-identical; pure model code.

**Dependencies:** after 3.0.1.4 + 3.0.4.1. **Commit:** `[3.0.4.2] feat: add ADA R3 TrackedObject binding with round-trip test`

**Status:** done 2026-07-31 — `ada::contracts::TrackedObject` binding (`object_class` ↔ `"class"`, enum wire strings via NLOHMANN_JSON_SERIALIZE_ENUM) + round-trip test; copies cmp-identical; verified green on CI run 30602040565 (`ada-core-build` — no local cmake, CI is the authority).

### `4.0.4.3` — R4 C++ binding + round-trip test *(agent)*

**Objective:** create `ADA_ECU/src/contracts/r4_message.{hpp,cpp}` (producer side: warning event + state message) + `ADA_ECU/tests/contracts/test_r4_roundtrip.cpp`.

**Scope:** fields per 4.0.1.5's schema; the embedded `object` snapshot reuses the 3.0.4.2 R3 binding. Land synced copies: `ADA_ECU/contracts/r4-ada-ivi.schema.json`, `ADA_ECU/tests/fixtures/samples/r4-warning.json`, `ADA_ECU/tests/fixtures/samples/r4-state.json`. Test: both shared samples round-trip.

**Acceptance:** ADA build + ctest green; copies byte-identical.

**Dependencies:** after 4.0.1.5 + 3.0.4.2. **Commit:** `[4.0.4.3] feat: add ADA R4 binding with round-trip test`

**Status:** implemented 2026-07-31, awaiting CI — `ada::contracts::R4WarningEvent`/`R4StateMessage` producer binding (embedded `object` reuses the R3 `TrackedObject` binding; shared `R4VehicleSet` with nullable-or-absent `vehicleC`) + round-trip tests on both shared samples; copies cmp-identical; `ada-core-build` CI job is the verification authority (no local cmake).

### `4.0.4.4` — ADA-side R4 additive-version test *(agent)*

**Objective:** create `ADA_ECU/tests/contracts/test_r4_additive_version.cpp` on the shared D4 fixture.

**Scope:** land the synced copy `ADA_ECU/tests/fixtures/samples/r4-unknown-warning.json`. Test per HLD D4: the 4.0.4.3 binding parses the fixture without error, preserves the unknown `warningType` string, ignores the unknown field (guards ADA-side R4 consumption in R18 tooling).

**Acceptance:** ADA build + ctest green; copy byte-identical.

**Dependencies:** after 4.0.1.6 + 4.0.4.3. **Commit:** `[4.0.4.4] test: add ADA-side R4 additive-version test`

**Status:** implemented 2026-07-31, awaiting CI — D4 fixture parses through the unmodified 4.0.4.3 binding (unknown `warningType` preserved, `schemaVersion` 2 carried, `hazardDetail` ignored and absent from re-emit); copy cmp-identical; `ada-core-build` CI job is the verification authority (no local cmake).

### `3.0.4.5` — Detector Python R3 binding + Python-side fixture validation *(agent)*

**Objective:** create `ADA_ECU/detector/contracts/tracked_object.py` (R3 dataclass + JSONL encode/decode — the R12 subprocess wire shape), `ADA_ECU/detector/tests/test_r3_roundtrip.py`, `ADA_ECU/detector/requirements-dev.txt` (pytest, jsonschema — test-only; runtime deps come with Phase 3).

**Scope:** stdlib `json` + dataclasses only in the binding. Test: (a) JSONL round-trip of the shared R3 sample; (b) `jsonschema` validation of all five ADA-local samples (`r2-object`, `r3-tracked-object`, `r4-warning`, `r4-state`, `r4-unknown-warning`) against the three ADA-local schemas — the one-language-validates-for-all rule (HLD D2) covering R2/R3/R4 fixtures for every consumer.

**Acceptance:** detector pytest command green; no new fixture/schema copies (reads the node-local ones landed by 2.0.3.2 / 3.0.4.2 / 4.0.4.3 / 4.0.4.4).

**Dependencies:** after 2.0.3.2 + 3.0.4.2 + 4.0.4.3 + 4.0.4.4. **Commit:** `[3.0.4.5] feat: add detector R3 JSONL binding and Python fixture validation`

**Status:** implemented 2026-07-31, locally green — stdlib dataclass+JSONL binding (`object_class` ↔ `"class"`); pytest passes locally (Python 3.14, jsonschema 4.26): JSONL round-trip + all five node-local samples validate against the three schemas (r4 `$ref` via referencing.Registry); awaiting CI `python-tests` confirmation.

---

## Task Group 0.5 — Scenario Player contract layer (serves R1)

### `1.0.5.1` — Bench CpmContent dataclass + golden round-trip test *(agent)*

**Objective:** create `Scenario_Player/player/contracts/cpm_content.py` (CpmContent Python dataclass, wire-native units — the bench side of the codec seam), `Scenario_Player/tests/test_cpm_content_roundtrip.py`, `Scenario_Player/requirements-dev.txt` (pytest, jsonschema).

**Scope:** land synced copies: `Scenario_Player/contracts/r1-cpm-content.schema.json` + `Scenario_Player/tests/fixtures/golden/*.json` (**`.json` side only** — `.uper` is unused until the R11 codec path is decided). Test: dataclass ⇄ JSON round-trip against every golden `.json`, plus `jsonschema` validation of each golden `.json` against the schema. **Hard constraint:** no UPER encode/decode from Python — the bench→codec path is the open F3 item, decided in the R11 HLD ([node-code-layout.md § Scenario_Player specifics](../.claude/rules/node-code-layout.md#scenario_player-specifics-r11)); improvising one fails this subtask.

**Acceptance:** Scenario_Player pytest command green; copies byte-identical; stdlib-only binding.

**Dependencies:** after 1.0.1.2 + 1.0.2.4 (golden `.json` fixtures exist). **Commit:** `[1.0.5.1] feat: add bench CpmContent dataclass with golden-vector round-trip test`

---

## Task Group 0.6 — IVI Kotlin R4/R3 binding (serves R4) — reconciled with phase5

### `4.0.6.1` — Freeze the IVI R4 sealed binding + finalize interim R3 models + round-trip test *(agent)*

**Objective:** create `IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/model/R4Message.kt` (sealed: `R4WarningEvent` | `R4StateMessage` per the frozen 4.0.1.5 schema) and finalize the interim models against the frozen R3/R4 schemas, with `IVI_ECU/app/src/test/java/com/hackathon/v2x/ivi/model/R4RoundTripTest.kt`.

**Scope:**

- `R3Snapshot.kt`: keep all existing field names (`id`, `source`, `position`, `distance`, `speed`, `confidence`, `state`); **add** the two missing frozen-R3 fields — object class as a Kotlin-safe property with `@SerialName("class")`, and `timestamps` per 3.0.1.4.
- `SceneGeometry.kt`: keep field names (`ego`, `vehicleB`, `vehicleC` nullable); add `@Serializable` + any distance fields the frozen R4 `geometry` carries.
- Land synced copies: `IVI_ECU/contracts/r3-tracked-object.schema.json`, `IVI_ECU/contracts/r4-ada-ivi.schema.json` (reference anchors — kotlinx consumes code, not JSON Schema), and `IVI_ECU/app/src/test/resources/contracts/samples/` `r3-tracked-object.json` · `r4-warning.json` · `r4-state.json`.
- `R4RoundTripTest.kt`: both shared samples decode → encode → decode to equal objects (kotlinx.serialization; JUnit4).
- Pure Kotlin model package — no Android/UI imports (HLD §9).

**Acceptance:** IVI test command green; copies byte-identical; existing model field names unchanged.

**Dependencies:** after 4.0.1.5. **Commit:** `[4.0.6.1] feat: freeze IVI R4 Kotlin binding and finalize R3 snapshot models`

**Status:** done 2026-07-31 — sealed `R4Message` (`warning`|`state`, discriminator `type`) + finalized `R3Snapshot` (`objectClass` @SerialName class, `timestamps`) + `@Serializable SceneGeometry`; copies cmp-identical; preview call site updated; verified green on CI run 30602040565 (`ivi-unit-tests` — local JDK 25 exceeds Gradle 8.13's range, CI is the authority).

### `4.0.6.2` — IVI-side R4 additive-version test *(agent)*

**Objective:** create `IVI_ECU/app/src/test/java/com/hackathon/v2x/ivi/model/R4AdditiveVersionTest.kt` on the shared D4 fixture.

**Scope:** land the synced copy `IVI_ECU/app/src/test/resources/contracts/samples/r4-unknown-warning.json`. Test per HLD D4: `Json { ignoreUnknownKeys = true }` parses the fixture; the unknown `warningType` classifies as a generic/unknown warning — never a parse failure (the binding-level contract Phase 5's UI behavior builds on).

**Acceptance:** IVI test command green; copy byte-identical.

**Dependencies:** after 4.0.6.1 + 4.0.1.6. **Commit:** `[4.0.6.2] test: add IVI-side R4 additive-version test`

### IVI reconciliation with phase5 (HLD §11 item 5 — resolved here)

Ownership split so no deliverable is decomposed twice; [phase5_tasks.md](phase5_tasks.md) IDs stay stable (never renumbered):

| Deliverable | Owner | Effect on phase5 |
|---|---|---|
| `R4Message.kt` sealed models + finalized `R3Snapshot.kt`/`SceneGeometry.kt` + binding round-trip test | **`4.0.6.1` (this plan)** | `4.5.1.1` is **superseded — do not implement**; at Phase 5 execution it closes by verifying `4.0.6.1`'s artifacts exist and its models are consumed unchanged |
| Binding-level additive-version behavior (lenient parse, unknown `warningType` no-crash) + `R4AdditiveVersionTest.kt` | **`4.0.6.2` (this plan)** | `4.5.1.2` **keeps** `R4Deserializer.kt` (Result wrapper, error taxonomy, WARN logging, malformed-JSON handling) built **on** the frozen binding; its unknown-`warningType` test cases consume the shared `r4-unknown-warning.json` fixture instead of inventing payloads |
| `SceneGeometry`/`R3Snapshot` field-name compatibility | frozen by `3.0.1.4`/`4.0.1.5` schemas | interim field names were kept deliberately; downstream phase5 UI subtasks are unaffected |

Flag: this run commits only `plans/phase0_tasks.md`, so the supersede note is recorded here; annotating `4.5.1.1`/`4.5.1.2` inside `phase5_tasks.md` itself is a follow-up docs edit for the main session (blocked only by this run's single-file commit scope).

---

## Task Group 0.7 — Contract integrity gate + CI Linux verification (serves R1–R4; IDs anchored to R1, which also owns the F2 ban)

### `1.0.7.1` — sync-manifest + byte-identity gate *(agent)*

**Objective:** create `contracts/sync-manifest.json` (source → node-local copy map, exactly the [HLD §5 sync map](doc/phase0-contract-freeze-hld.md#sync-map-sync-manifestjson-content)) and `contracts/check_sync.py` (Python 3 stdlib), and run it green over the completed tree.

**Scope:** `check_sync.py` walks the manifest and exits 1 on any byte difference (D1 — copies must never drift), and additionally greps `V2X_ECU/src/` for the banned bare `asn1::Cpm` token (F2), exit 1 on hit. This is the local + CI contract-integrity gate.

**Acceptance:** `python contracts/check_sync.py` exits 0 on the repo as committed; deliberately corrupting one copy (unstaged) makes it exit 1; the F2 grep catches a planted `asn1::Cpm` token (unstaged).

**Dependencies:** sequential, after every copy-landing subtask — 1.0.2.2, 1.0.2.5, 2.0.3.1, 2.0.3.2, 3.0.4.2, 4.0.4.3, 4.0.4.4, 1.0.5.1, 4.0.6.1, 4.0.6.2. **Commit:** `[1.0.7.1] feat: add contract sync manifest and byte-identity gate`

### `1.0.7.2` — GitHub Actions CI: Linux verification for Phase 0 *(agent)*

**Objective:** create `.github/workflows/phase0-ci.yml` — Linux (`ubuntu-latest`) verification on every push and on PRs to `main`: contracts sync gate, Python unit tests, IVI Gradle unit tests.

**Scope:**

- Rationale: the dev host has no Docker/WSL (user decision 2026-07-31) — GitHub Actions (`origin` = `mnpham2101/FPT-Hackathon2026`) is the project's Linux verification path until then.
- Job `contracts-gate`: runs `python contracts/check_sync.py` only if that file exists — the guard keeps CI green until 1.0.7.1 lands (it is sequenced after all copy-landing subtasks).
- Job `python-tests`: for each of `Scenario_Player/` and `ADA_ECU/detector/`, install `requirements-dev.txt` and run pytest only where that lane's tests exist (green before 1.0.5.1 / 3.0.4.5 land).
- Job `ivi-unit-tests`: `chmod +x gradlew && ./gradlew :app:testDebugUnitTest` in `IVI_ECU/` (temurin JDK 17, Gradle cache) — the per-node build command above; the runner image carries the Android SDK.
- YAML comments mark where groups 0.2/0.4 add C++ build / docker-build jobs when their toolchains and Dockerfiles land — no empty placeholder jobs.
- **No registry-push job** — the CarSky Zot API key is not yet a repo secret; push-to-registry is a later subtask added once the user stores the credential (flag, don't absorb).

**Acceptance:** workflow committed and YAML-valid; on the current tree every job passes (guards skip not-yet-landed lanes); full CI-green evidence is recordable only after the user pushes the branch — local JDK 25 exceeds Gradle 8.13's supported range, so the IVI job is CI-only by design.

**Dependencies:** none — lands immediately; later lanes plug into the existing guards (no workflow rewrite). **Commit:** `[1.0.7.2] chore: add Phase 0 Linux-verification CI workflow`

**Status:** done 2026-07-31 — first `phase0-ci` Actions run on `feat/phase0-contract-freeze` completed `success` (run 30590952652, branch pushed by the user's session; poll evidence in-session). YAML parse-validated locally (PyYAML); every job guarded green-by-design on the current tree; no local Docker/WSL and local JDK 25 exceeds Gradle 8.13's range — CI is the Linux path.

---

## Task Group 0.8 — R5/R6 baseline connectivity smoke test (blueprint `trial2_minh`)

> Procedure adopted wholesale from the [smoke-test note](doc/research_notes/baseline-connectivity-smoke-test.md) (HLD §6) — subtasks below reference its sections, never restate them. **Startup self-run guarantee (HLD §6, user requirement 2026-07-31) is acceptance on every run subtask:** node start ⇒ `entrypoint.sh` self-runs `capture.sh` (background) + `netcheck.py` (foreground), roles/ports wired purely by node-config env ⇒ C1–C5 observable in each node's View Log with **no manual invocation — a run needing a manual exec fails**. Run evidence accumulates in `plans/doc/phase0-smoke-test-run.md` (created by 5.0.8.2).

### `6.0.8.1` — Author the netcheck tool *(agent)*

**Objective:** create `tools/netcheck/Dockerfile`, `entrypoint.sh`, `capture.sh`, `netcheck.py` with **exactly** the contents of the note [§4.2–§4.5](doc/research_notes/baseline-connectivity-smoke-test.md#4-tool-implementation) — do not redesign (closes step M1).

**Scope:** the structural half of the self-run guarantee is already in those contents (`CMD ["./entrypoint.sh"]`; capture background, netcheck foreground; env-only role wiring) — preserve it verbatim.

**Acceptance:** 4 files match the note §4; `sh -n tools/netcheck/entrypoint.sh tools/netcheck/capture.sh` and `python -m py_compile tools/netcheck/netcheck.py` pass; `docker build -t m1-netcheck:latest tools/netcheck/` succeeds where Docker is available (otherwise the build check transfers to 5.0.8.2/M3).

**Dependencies:** none — fully parallel with groups 0.1–0.7. **Commit:** `[6.0.8.1] feat: add netcheck baseline connectivity smoke-test tool`

**Status:** done 2026-07-31 — 4 files verbatim from the note §4.2–§4.5 (compared against the note); sh -n + bash -n and py_compile pass; LF endings verified (i/lf); docker build transfers to 5.0.8.2/M3 per acceptance (no Docker on this host).

### `5.0.8.2` — Build & push the netcheck image (M2–M4) *(car-sky — executed after this planning run, once 6.0.8.1 exists)*

**Objective:** [[car-sky]] runs its deploy-preflight (confirm blueprint `trial2_minh`, target Container nodes, credential), then executes note §6 steps M2–M4: confirm + log in to the correct registry host (**closes O1** — `registry.carsky.io` 502 vs `registry.hackathon-2.carsky.io` 401-auth), `docker build`, `tag`, `push` `m1-netcheck:latest`.

**Scope:** one image for all three Container nodes (note §5) — never three images. Not executed in this planning run; planner keeps done-tracking.

**Acceptance:** push accepted by the registry; `plans/doc/phase0-smoke-test-run.md` created recording the confirmed registry host (O1 answer) and pushed tag.

**Dependencies:** after 6.0.8.1. **Commit:** `[5.0.8.2] docs: record netcheck image push and confirmed registry host`

**Status:** in progress 2026-07-31 — M2–M4 wired as the CI job `netcheck-image` (no Docker on the dev host, so CI replaces the local car-sky build path): build runs on every push; push runs only once `CARSKY_ZOT_API_KEY` exists as a repo secret. **O1 closed:** live probe shows `registry.hackathon-2.carsky.io` answering (405 to HEAD /v2/) while `registry.carsky.io` returns 502 — CI and all M7 image refs must use the hackathon-2 host. Flips done when a CI run reports the push and `plans/doc/phase0-smoke-test-run.md` records it.

### `5.0.8.3` — USER-MANUAL: blueprint node config + deploy → C1 (M5–M9)

**Objective:** the user performs note §6 steps M5–M9 in the Nydus UI: open/clone the blueprint (M5), verify the four `ethernet` pins + edges (M6), set `image`/`command`/`capabilities: ["NET_RAW"]`/env per the note §6.1 table on bench `.10` / V2X `.11` / ADA `.12` (M7 — note: ADA **adds** `LISTEN_PORT=47200` alongside `V2X_LISTEN_PORT`, no renaming), keep the IVI AAOS artifact (M8), New Deployment → every node `Running`, restart count 0 = **C1** (M9).

**Scope:** no agent performs these steps; the plan tracks them. **Self-run acceptance:** the deploy alone must start the scripts — if any node needs a manual exec/interactive session to produce logs, the run fails and goes to the note §8 troubleshooting.

**Acceptance:** C1 evidence (all nodes Running, restart 0) recorded in `plans/doc/phase0-smoke-test-run.md`; O2 (`capabilities` honored) provisionally observable. Evidence commit made by the orchestrating session after user confirmation.

**Dependencies:** after 5.0.8.2. **Commit:** `[5.0.8.3] docs: record smoke-test deployment and C1 node-Running evidence`

### `6.0.8.4` — USER-MANUAL: View Log verification C2–C5 + IVI hop (M10–M12)

**Objective:** the user performs note §6 steps M10–M12: read each node's View Log against the note §6.2 expected logs — **C2** (zero `[ERR]`), **C3** (live per-node logs), **C4** (`[CAP]` tcpdump lines; `/proc` counter fallback acceptable if O2 is negative), **C5** (the `|v2x|ada`-stamped token at the last hop — the chain proof); record the IVI hop-3 evidence using one of the note [§7 options](doc/research_notes/baseline-connectivity-smoke-test.md#7-the-ivi-hop) (ADB Shell `nc -u -l`, ADA-side `[TX]`+`[CAP]` evidence, or the real R4 listener) **and which option was used**; optional M11 MTU probe (`PAD=1400` — closes O3, feeds the CPM size budget); M12 delete the deployment (2-deployment quota).

**Scope:** answers to O2/O3/O4 recorded as observed. **Self-run acceptance:** all C2–C5 evidence must come from logs of self-started scripts — any manual invocation fails the run.

**Acceptance:** C2–C5 + IVI-hop option + O2/O3/O4 answers recorded in `plans/doc/phase0-smoke-test-run.md` — closes the milestone box "blueprint topology documented + validated". Evidence commit by the orchestrating session after user confirmation.

**Dependencies:** after 5.0.8.3. **Commit:** `[6.0.8.4] docs: record C2-C5 smoke-test evidence and IVI hop option`

---

## Execution order & parallelism

Dependencies are real (files, contract artifacts, fixtures) — not default assumptions. Independent start points: `1.0.1.1`, `1.0.2.1`, `3.0.4.1`, `6.0.8.1`, `1.0.7.2` (plus `3.0.1.4`).

```
Lane A  contracts/:      1.0.1.1 ──► 1.0.1.2                 3.0.1.4 ──► 4.0.1.5 ──► 4.0.1.6
                              └─────► 2.0.1.3                (A2 parallel with A1)

Lane B  V2X_ECU:         1.0.2.1 ──► 1.0.2.2 ──► 1.0.2.3 ──► 1.0.2.4 ──► 1.0.2.5
                         (needs 1.0.1.2)        (needs 1.0.1.1 + 2.0.1.3)
                         1.0.2.1 + 2.0.1.3 ──► 2.0.3.1        (parallel with codec chain)

Lane C  ADA_ECU:         3.0.4.1 ──► { 2.0.3.2 ∥ 3.0.4.2 } ──► 4.0.4.3 ──► 4.0.4.4 ──► 3.0.4.5
                         (2.0.3.2 needs 2.0.1.3; 3.0.4.2 needs 3.0.1.4; 4.0.4.x need 4.0.1.5/6)

Lane D  Scenario_Player: 1.0.1.2 + 1.0.2.4 ──► 1.0.5.1

Lane E  IVI_ECU:         4.0.1.5 ──► 4.0.6.1 ──► 4.0.6.2 (also needs 4.0.1.6)

Gate    contracts/:      all copy-landing subtasks ──► 1.0.7.1

CI      .github/:        1.0.7.2 (fully parallel — guarded jobs go live as lanes A–E and the gate land)

Lane F  smoke test:      6.0.8.1 ──► 5.0.8.2 (car-sky) ──► 5.0.8.3 (USER) ──► 6.0.8.4 (USER)
                         (fully parallel with lanes A–E and the gate)
```

- **Parallel:** lanes B, C, E, F against each other once their lane-A inputs exist; within lane C, `2.0.3.2 ∥ 3.0.4.2`; `2.0.3.1` parallel with `1.0.2.2–1.0.2.5`.
- **Sequential:** every arrow above; `1.0.7.1` is last of the contract work; lane F is internally strictly sequential.
- **Blocked-until-spawnable:** all *agent* subtasks are ready to hand to implementation subagents (design final in the Phase 0 HLD); `5.0.8.2` goes to [[car-sky]] only after `6.0.8.1` is committed; `5.0.8.3`/`6.0.8.4` wait on the user.

## Acceptance traceability

| Milestone Phase 0 box | Closed by |
|---|---|
| R1 profile committed; golden vectors encode/decode through the Vanetza seam | 1.0.1.1 · 1.0.1.2 · 1.0.2.1–1.0.2.5 |
| R2/R3/R4 schemas committed; round-trip tests pass in C++ / Python / Kotlin | 2.0.1.3 · 3.0.1.4 · 4.0.1.5 · 2.0.3.1 · 2.0.3.2 · 3.0.4.1 · 3.0.4.2 · 4.0.4.3 · 3.0.4.5 · 1.0.5.1 · 4.0.6.1 · (integrity: 1.0.7.1 · CI: 1.0.7.2) |
| R4 additive-version test defined | 4.0.1.6 · 4.0.4.4 · 4.0.6.2 |
| Blueprint topology documented + validated (nodes, `ethernet` pins, edges) | pre-existing guides (HLD §1) + 6.0.8.1 · 5.0.8.2 · 5.0.8.3 · 6.0.8.4 (C1–C5 on `trial2_minh`) |

## Open items carried, not decided (no Phase 0 subtask may close them)

| Item | Owner / closes at |
|---|---|
| Bench Python → R1 codec path (F3) — `1.0.5.1` explicitly must not improvise it | [[project-architecture]], R11 HLD |
| Smoke-test O1–O4 | their M-steps: O1 → 5.0.8.2 · O2 → 5.0.8.3/6.0.8.4 · O3 → 6.0.8.4 (M11) · O4 → 6.0.8.4 (§7 option 1) |
| Report errata: R2 sample `distance 25.4` vs derived `25.03` (F7); `sender.speed` source wording (F1) — HLD §11 items 3–4 | [[project-researcher]] — contracts use the derived/nullable values meanwhile |
| Annotating `4.5.1.1`/`4.5.1.2` in `phase5_tasks.md` per § IVI reconciliation | main session, follow-up docs edit (this run's commit scope is this file only) |

---

*Created 2026-07-31 by project-planner from the Phase 0 HLD (`d807c37` + `70796c0`) and [milestone1.md § Phase 0](milestone1.md#phase-0--freeze-the-contracts-r1r6). 8 task groups, 27 subtasks: 24 agent-implemented (incl. the 1.0.7.1 gate and the 1.0.7.2 CI workflow, added 2026-07-31), 1 car-sky-executed, 2 user-manual.*
