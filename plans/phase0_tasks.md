# Phase 0 — Contract Freeze (R1–R6): Full Task Breakdown

> **Authority & context:**
> - **Phase content:** [milestone1_high_level_plan.md § Phase 0](../documents/Plan%20and%20Proposal/milestone1_high_level_plan.md#phase-0--freeze-the-contracts-r1r6) — its four acceptance checkboxes are the phase output.
> - **Design:** [phase0-contract-freeze-hld.md](../deprecated/phase0-contract-freeze-hld.md) (commits `d807c37` + `70796c0`) — every target path below is cited verbatim from its §5 folder map; design decisions D1–D4, seam shape §7, conventions §4.
> - **Requirements:** [m1-cooperative-awareness.md §2](../requirements/m1-cooperative-awareness.md) R1–R6 — referenced by number, never restated.
> - **Smoke-test procedure:** [deploy-walkthrough-netcheck.md](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md) — steps M1–M12, the AI/Human work division [§5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human), and acceptance [§6](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#6-expected-outputs-and-acceptance). Group 0.8 decomposes from it per [walkthrough-driven-delivery.md § Stage 2](../.claude/rules/walkthrough-driven-delivery.md); nothing is restated.
> - **Smoke-test design:** [baseline-connectivity-smoke-test.md](doc/research_notes/baseline-connectivity-smoke-test.md) — the objective, the [pass criteria C1–C5](doc/research_notes/baseline-connectivity-smoke-test.md#2-pass-criteria) every mention below refers to, and open items O1–O4.
> - **R1 profile skeleton:** [scenario-player-v2x-callflow-messages.md](../documents/Design/SCENARIO-PLAYER/scenario-player-v2x-callflow-messages.md) §4 + findings F1–F9.
> - **Rules:** [task-planning-conventions.md](../.claude/rules/task-planning-conventions.md) (`X.Y.Z.W`; subtask discipline is every subtask's definition of done, restated once in § Subtask discipline below).
>
> **Task ID legend:** `X.0.Z.W` — X = requirement served · 0 = this phase · Z = task group · W = subtask. IDs are stable; never renumber.

## Phase 0 overview

**Objective.** Freeze the cross-track contracts R1–R6 before dependent work: R1 profile + golden vectors through the Vanetza codec seam; R2/R3/R4 schemas + per-language bindings + round-trip tests + the R4 additive-version test; R5/R6 proven live by the baseline-blueprint connectivity smoke test.

**Input (must exist before start):**

- Requirement definitions R1–R6 in the report §2; Phase 0 HLD committed (`d807c37`, `70796c0`).
- Baseline blueprint documented and saved on CarSky as `baseline_m1` ([carsky-4-node-blueprint.md § 8](../requirements/car-sky-guide/carsky-4-node-blueprint.md#8-the-blueprints-on-carsky) + [blueprint-m1-cooperative-awareness.json](../requirements/car-sky-guide/blueprint-m1-cooperative-awareness.json)) — Phase 0 adds no topology design.
- Existing `IVI_ECU/` Gradle project with interim `model/R3Snapshot.kt` and `model/SceneGeometry.kt` (kotlinx.serialization + JUnit4 configured).

**Output (phase acceptance = the four milestone boxes):**

- [x] R1 profile document committed; golden-vector CPMs encode/decode through the Vanetza codec seam. — closed 2026-07-31; the 6-case corpus decodes to its `.json` content and re-encodes to the exact `.uper` octets (CI run 30608005574).
- [x] R2, R3, R4 schemas committed; round-trip tests pass in each consumer language (C++ / Python / Kotlin). — closed 2026-07-31; integrity gate green over 36 copies (CI run 30608202261).
- [x] The R4 additive-version test is defined (unknown `warningType` degrades gracefully). — closed 2026-07-31 on the shared D4 fixture, in both the ADA and the IVI consumer.
- [x] Blueprint topology documented (pre-existing guides) + validated: smoke-test C1–C5 green on a `baseline_m1` clone. — closed 2026-07-31; 5/5 nodes `Running`, C1–C5 all met. Evidence: [phase0-smoke-test-run.md](doc/phase0-smoke-test-run.md).

Which subtasks close each box: [§ Acceptance traceability](#acceptance-traceability).

**Phase 0 acceptance state 2026-07-31: 4 of 4 boxes closed — phase complete.** This state predates the group 0.8 re-decomposition against [deploy-walkthrough-netcheck.md § 5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human), which adds `6.0.8.7`, `6.0.8.8` and `5.0.8.9` — the walkthrough's optional M11 probe and its M12 teardown — as open subtasks. None of the three closes an acceptance box, so the four boxes stand.

**Suggested branch (suggestion only — creation is the user's call):** `feat/phase0-contract-freeze`

### Execution split legend

| Label | Meaning |
|---|---|
| *agent* | implemented by a spawned implementation subagent (default) |
| *car-sky* | a row the walkthrough's work-division table assigns to **AI**; executed by the [[car-sky]] agent, planner keeps the ID and done-tracking |
| *Human* | a row the subject walkthrough's work-division table assigns to **Human** — [deploy-walkthrough-netcheck.md § 5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human) for group 0.8; no agent performs it, and [[project-planner]] makes the evidence-record commit after the user confirms |

### Subtask discipline (applies to every subtask below)

Per [task-planning-conventions.md § Subtask discipline](../.claude/rules/task-planning-conventions.md#subtask-discipline-non-negotiable): single objective, no out-of-scope code, exactly one atomic commit with the stated message, build passes, unit tests pass, brief is self-contained. Implementation subagents inherit this as their definition of done. C++ builds target Linux, as [solution-selection-criteria.md § Hard constraints](../.claude/rules/solution-selection-criteria.md#hard-constraints-disqualifying--apply-before-comparison) requires of every solution. Linux verification runs on GitHub Actions CI (subtask `1.0.7.2`); the development host provides no Linux container runtime.

**Status tracking:** as execution proceeds each subtask gains a `**Status:**` line (appended in that subtask's own atomic commit) recording done/blocked plus the verification evidence; a subtask without a status line is not started.

### Per-node build commands (cited in acceptance below)

| Node / area | Build + test command |
|---|---|
| `V2X_ECU/` | Boost is a Vanetza hard dependency and is installed first: `sudo apt-get update && sudo apt-get install -y libboost-dev libboost-date-time-dev`. Then `cmake -S V2X_ECU -B V2X_ECU/build && cmake --build V2X_ECU/build -j && ctest --test-dir V2X_ECU/build --output-on-failure` |
| `ADA_ECU/` core | `cmake -S ADA_ECU -B ADA_ECU/build && cmake --build ADA_ECU/build -j && ctest --test-dir ADA_ECU/build --output-on-failure` |
| `ADA_ECU/detector/` | `pip install -r ADA_ECU/detector/requirements-dev.txt && python -m pytest ADA_ECU/detector/tests` |
| `Scenario_Player/` | `pip install -r Scenario_Player/requirements-dev.txt && python -m pytest Scenario_Player/tests` |
| `IVI_ECU/` | from `IVI_ECU/`: `./gradlew :app:testDebugUnitTest` (`gradlew.bat` on Windows) — the pre-`4.5.1.4` module layout; [phase5_minh_tasks.md § Build & verification commands](phase5_minh_tasks.md) states the command set that supersedes it |
| `contracts/` gate | `python contracts/check_sync.py` → exit 0 |
| `tools/netcheck/` | built by the `netcheck-image` CI job, no local Docker required. Local tag `m1-netcheck:latest`; registry tag `registry.hackathon-2.carsky.io/m1-netcheck:latest` |

---

## Task Group 0.1 — Contract source of truth: R1 profile + R1–R4 schemas + shared samples (`contracts/`)

> Authors the R1 profile, the R1–R4 schemas and the shared samples in the user-approved top-level `contracts/` folder (HLD D1); the golden vectors come from group 0.2 and the sync gate from group 0.7. All contract JSON uses JSON Schema draft 2020-12 (HLD §8).

### [x] `1.0.1.1` — Author the R1 CPM profile document *(agent)*

**Objective:** write `contracts/r1-cpm-profile.md`, the versioned normative R1 profile.

**Scope:**

- Structure and field/unit/encoding table from the callflow note [§4.1–§4.2](../documents/Design/SCENARIO-PLAYER/scenario-player-v2x-callflow-messages.md) (2 containers, 1 perceived object; ASN.1 paths, types, units, ranges).
- Freeze the eight conventions exactly per [HLD §4](../deprecated/phase0-contract-freeze-hld.md), in the wording of [`contracts/r1-cpm-profile.md` § 5](../contracts/r1-cpm-profile.md): F1, F2, F5, F6, F7, F8, F9, VF. VF covers `PerceivedObject.position` **and** `.velocity` in the sender (B) cartesian frame.
- Exchange call flow: callflow note §2 — § B is the sole live flow; unidirectional (R10 deferred).
- Fix the golden-vector corpus list (HLD D3, 6 cases): `nominal` · `mdt-max` · `mdt-min` · `conf-unavailable` · `gate-boundary` · `coord-large`.
- Sample values use the F7-derived distance (`hypot(25.0, 1.2) = 25.03`), not the report's `25.4` — report erratum stays flagged to [[project-researcher]] (HLD §11 items 3–4), never absorbed here.

**Acceptance:** file committed at `contracts/r1-cpm-profile.md`; all 8 conventions + the 6-case corpus list present; [markdown-writing-style](../.claude/skills/markdown-writing-style/SKILL.md) followed. Doc-only — no build/test target.

**Dependencies:** none — starts immediately. **Commit:** `[1.0.1.1] docs: author R1 CPM profile document with frozen conventions`

**Status:** done 2026-07-31 — committed; 8 conventions (F1/F2/F5/F6/F7/F8/F9/VF) + 6-case corpus with per-case parameters present; markdown style followed (no hard wrap, bullets/tables).

### [x] `1.0.1.2` — Author the CpmContent logical-content schema *(agent)*

**Objective:** write `contracts/r1-cpm-content.schema.json` — JSON Schema mirroring the codec-seam struct `CpmContent` 1:1, wire-native integer units (the golden vectors' JSON side).

**Scope:** field set = the profile doc's table from `1.0.1.1` (callflow note §4.2), in wire-native integer units. The committed schema [`contracts/r1-cpm-content.schema.json`](../contracts/r1-cpm-content.schema.json) is the field list. No SI floats, no derived fields (seam rule, HLD D3).

**Acceptance:** schema committed; `$schema` declares draft 2020-12; parses (`python -m json.tool`); bounds above encoded as JSON Schema constraints. Instance-validation lands with 1.0.5.1.

**Dependencies:** after 1.0.1.1. **Commit:** `[1.0.1.2] feat: add CpmContent JSON Schema mirroring the R1 codec seam`

**Status:** done 2026-07-31 — parses (python -m json.tool); Draft202012Validator.check_schema passes and the profile §4 nominal instance validates (jsonschema 4.26, Python 3.14); wire-native bounds encoded as constraints.

### [x] `2.0.1.3` — Freeze the R2 schema + shared sample *(agent)*

**Objective:** write `contracts/r2-v2x-object.schema.json` and `contracts/samples/r2-object.json`.

**Scope:** fields per report §2 R2 (`schemaVersion`, `type: "v2x_object"`, `stationId`, `rxTime`, `sender{lat, lon, heading, speed}`, `object{objectId, timeOfMeasurement, distance, position{x, y, confidence}, speed, classification, confidence}`) with two frozen deviations: `sender.speed` nullable (F1); sample `object.distance = 25.03` (F7-derived), not the report's `25.4` (erratum flagged, HLD §11 item 3).

**Acceptance:** schema + sample committed; both parse; schema-validation of the sample lands with 3.0.4.5.

**Dependencies:** after 1.0.1.1 (F1/F7 conventions frozen first). **Commit:** `[2.0.1.3] feat: freeze R2 v2x-object schema and shared sample`

**Status:** done 2026-07-31 — schema+sample parse; check_schema passes; sample (and its sender.speed=null variant) validates against the schema (jsonschema 4.26, Python 3.14); distance 25.03 = hypot(25.0, 1.2) (F7), sender.speed nullable (F1).

### [x] `3.0.1.4` — Freeze the R3 schema + shared sample *(agent)*

**Objective:** write `contracts/r3-tracked-object.schema.json` and `contracts/samples/r3-tracked-object.json`.

**Scope:** fields per report §2 R3: `id`, `class`, `source` (`own_sensor` | `v2x_relayed`), `position{x, y}` (ego frame, m), `distance`, `speed`, `confidence` (0–1), `state` (`not_tracked` | `tentative` | `tracked`), `timestamps` (measured / received / last-updated). Note for the Kotlin consumer: `class` and `timestamps` are the two fields the interim `IVI_ECU` models lack ([§ IVI deliverable ownership split](#ivi-deliverable-ownership-split-hld-11-item-5)).

**Acceptance:** schema + sample committed; both parse; sample carries `source: "v2x_relayed"` (the ghost-C shape).

**Dependencies:** none — parallel with the R1/R2 chain. **Commit:** `[3.0.1.4] feat: freeze R3 TrackedObject schema and shared sample`

**Status:** done 2026-07-31 — schema+sample parse; check_schema passes; sample validates and carries source v2x_relayed (ghost-C shape), distance 55.03 = hypot(55.0, 1.7) coherent with the R2 sample (jsonschema 4.26, Python 3.14).

### [x] `4.0.1.5` — Freeze the R4 schema + nominal shared samples *(agent)*

**Objective:** write `contracts/r4-ada-ivi.schema.json`, `contracts/samples/r4-warning.json`, `contracts/samples/r4-state.json`.

**Scope:** per report §2 R4 — warning event (`schemaVersion`, `type: "warning"`, `warningType` (M1: `nlos_obstruction`), `riskState`, `object` = R3 snapshot as `$ref` to `r3-tracked-object.schema.json` (HLD §5), `geometry` = ego/B/C relative positions) + optional state message (`schemaVersion`, `type: "state"`, `seq`, `vehicles`). `geometry` field names must match the existing `IVI_ECU` `SceneGeometry.kt` (`ego`, `vehicleB`, `vehicleC` nullable — [§ IVI deliverable ownership split](#ivi-deliverable-ownership-split-hld-11-item-5)).

**Acceptance:** schema + 2 samples committed; all parse; the warning sample's `object.source` is `v2x_relayed`.

**Dependencies:** after 3.0.1.4 (`$ref`). **Commit:** `[4.0.1.5] feat: freeze R4 ADA-IVI schema and shared samples`

**Status:** done 2026-07-31 — schema+2 samples parse; check_schema passes; both samples validate with the R3 $ref resolved via a referencing registry (jsonschema 4.26, Python 3.14); warning object.source = v2x_relayed and equals the shared R3 sample; geometry names ego/vehicleB/vehicleC match SceneGeometry.kt.

### [x] `4.0.1.6` — Author the shared R4 additive-version fixture *(agent)*

**Objective:** write `contracts/samples/r4-unknown-warning.json` per HLD D4 — one fixture both consumers test.

**Scope:** a valid warning event with a **higher** `schemaVersion` than 4.0.1.5's, an unknown `warningType` string, and exactly one unknown extra field.

**Acceptance:** fixture committed; parses; differs from `r4-warning.json` only in the three D4 aspects.

**Dependencies:** after 4.0.1.5. **Commit:** `[4.0.1.6] feat: add shared R4 additive-version fixture`

**Status:** done 2026-07-31 — fixture parses and still validates against the R4 schema; programmatic diff proves exactly the three D4 deltas vs r4-warning.json (schemaVersion 2, warningType slippery_road, extra field hazardDetail).

---

## Task Group 0.2 — V2X ECU: toolchain, R1 codec seam, golden vectors (serves R1)

> Delivers the single R1 codec source behind the seam (HLD D3, §7) and the committed golden-vector corpus. All paths inside `V2X_ECU/`; build command per § Per-node build commands.

### [x] `1.0.2.1` — V2X_ECU CMake toolchain bring-up *(agent)*

**Objective:** create `V2X_ECU/CMakeLists.txt` — C++17; nlohmann/json + Vanetza **ASN.1-only** targets + GoogleTest, all via **pinned** FetchContent (exact tags/commits, no floating branches); CTest wired.

**Scope:** no GN/BTP full-stack pull-in (HLD §8); one sanity test target proving GoogleTest runs and `vanetza::asn1::r2::Cpm` compiles. No product code.

**Acceptance:** V2X build command green; `ctest` runs the sanity test; pins are exact.

**Dependencies:** none — starts immediately. **Commit:** `[1.0.2.1] chore: bring up V2X_ECU C++17 toolchain with Vanetza ASN.1 targets`

**Status:** done 2026-07-31 — commit `34fccba`; verified green on CI run 30603467579 (new job `v2x-core-build`, separate Configure/Build/Test steps). Pins: nlohmann `v3.11.3`, googletest `v1.14.0`, Vanetza commit `fb6c551030dcc12b924299bf401e35e5fe814713` (tag v26.06) with `EXCLUDE_FROM_ALL` so only the ASN.1 targets build; sanity test links `Vanetza::asn1` + `Vanetza::asn1_its_r2` and constructs `vanetza::asn1::r2::Cpm`. Two recorded deviations: CMake floor is **3.28** (not ADA's 3.22) because `FetchContent_Declare(... EXCLUDE_FROM_ALL)` needs 3.28 — without it Vanetza's root adds all 13 components to `all`; and the CI Build step uses `-j $(nproc)` rather than a bare `-j`, which make expands to *unlimited* jobs (~1400 asn1c TUs would OOM the runner). `actions/cache` over `V2X_ECU/build/_deps` is keyed on the exact Vanetza pin, so later lanes do not repay the ASN.1 compile.

### [x] `1.0.2.2` — CpmContent + ICpmCodec seam *(agent)*

**Objective:** create `V2X_ECU/src/codec/cpm_codec.hpp` with `CpmContent`, `DecodeError`, and `ICpmCodec` **exactly** per the frozen shape in [HLD §7](../deprecated/phase0-contract-freeze-hld.md#7-codec-seam-interface-frozen-shape), plus nlohmann `to_json`/`from_json` for `CpmContent`.

**Scope:** `CpmContent` fields mirror `contracts/r1-cpm-content.schema.json` 1:1 (wire-native integer units). Seam rule: pure representation transform — no unit conversion, no derivation (those are R9, above the seam). Land the synced copy `V2X_ECU/contracts/r1-cpm-content.schema.json` (byte-identical to source). Unit test: `CpmContent` ⇄ JSON ⇄ `CpmContent` equality.

**Acceptance:** V2X build + ctest green; interface text matches HLD §7; copy byte-identical.

**Dependencies:** after 1.0.1.2 + 1.0.2.1. **Commit:** `[1.0.2.2] feat: define CpmContent and ICpmCodec seam with JSON binding`

**Status:** done 2026-07-31 — commit `96a2043`; verified green on CI run 30603927615 (`v2x-core-build`; that run's tree also carries 2.0.3.1). Seam block machine-diffed against HLD §7 — identical signatures/const-ness/return types; namespace `v2x::codec`; header-only (inline nlohmann binding) exposed by the new `v2x_codec_seam` INTERFACE target. All 19 schema property paths bound 1:1 in wire-native integer types; copy `cmp`-identical; tests `StructToJsonToStructEquality` · `WireShapeMatchesSchemaKeys` · `RejectsMissingRequiredKey`.

### [x] `1.0.2.3` — VanetzaCpmCodec implementation *(agent)*

**Objective:** implement `V2X_ECU/src/codec/vanetza_cpm_codec.{hpp,cpp}` — the sole `ICpmCodec` implementation over `vanetza::asn1::r2::Cpm`.

**Scope:** bare `asn1::Cpm` is banned everywhere under `V2X_ECU/src/` (F2 — the 1.0.7.1 gate greps for it); `encode` throws on F9 bounds violation (|`measurementDeltaTime`| > 2047); `decode` returns `DecodeError` on malformed bytes, never crashes. Unit tests: encode→decode identity on a nominal in-code `CpmContent`; F9 violation throws; garbage bytes → `DecodeError`.

**Acceptance:** V2X build + ctest green; no unqualified `asn1::Cpm` token in the new sources.

**Dependencies:** after 1.0.2.2. **Commit:** `[1.0.2.3] feat: implement VanetzaCpmCodec over r2::Cpm`

**Status:** done 2026-07-31 — commit `0c99a38`.

- **Shipped:** the sole `ICpmCodec` implementation over `vanetza::asn1::r2::Cpm`. F9 violations (`|measurementDeltaTime| > 2047`, including the wire-legal −2048) throw `std::out_of_range`; `decode` guards null/empty input and returns `DecodeError`. Unsent-mandatory fields use the CDD named `*_unavailable` values, never literals.
- **Verifying run:** CI run 30605356736 (`v2x-core-build`, Configure/Build/Test all success). F2 grep over `V2X_ECU/src/` clean.
- **Deviation:** none against the brief. Five ASN.1 member shapes read off the pinned headers (`fb6c5510`, tag v26.06) differ from the naive assumption, and are locked by the golden vectors: `PerceivedObject.velocity` is a CHOICE `Velocity3dWithConfidence*` (cartesian arm selected); `PerceivedObject.classification` is an `ObjectClassDescription*` SEQUENCE OF; `ReferencePosition.positionConfidenceEllipse` is a `PosConfidenceEllipse`; `objectId` is an OPTIONAL pointer; `TimestampIts` is `INTEGER_t`, so `referenceTime` goes through `asn_uint642INTEGER`/`asn_INTEGER2uint64`. Relocating these to `V2X_ECU/doc/` is an open item for [[project-architecture]].

### [x] `1.0.2.4` — gv_tool + golden-vector corpus generation *(agent)*

**Objective:** create `V2X_ECU/tools/golden_vectors/main.cpp` (CMake target `gv_tool`, build-only, never shipped in a node image) and generate + commit the 6-case corpus into `contracts/golden-vectors/`.

**Scope:** `gv_tool` encodes CpmContent JSON → `.uper`, decodes back, asserts identity, writes `<case>.json` + `<case>.uper` pairs. Corpus per HLD D3 and the profile doc: `nominal` (F7-corrected R2 sample values from 2.0.1.3) · `mdt-max`/`mdt-min` (±2047 ms) · `conf-unavailable` (ConfidenceLevel 101) · `gate-boundary` (object at 30 m — the R13 admission seam) · `coord-large` (near CartesianCoordinateLarge bounds).

**Acceptance:** V2X build green; running `gv_tool` regenerates byte-identical vectors (determinism check: run twice, diff empty); 6 `.json`+`.uper` pairs committed under `contracts/golden-vectors/`.

**Dependencies:** after 1.0.2.3 + 1.0.1.1 (corpus list) + 2.0.1.3 (nominal values). **Commit:** `[1.0.2.4] feat: add gv_tool and generate the R1 golden-vector corpus`

**Status:** done 2026-07-31.

- **Shipped:** `gv_tool`, its CMake target and 4 CI steps; the 6-case corpus under `contracts/golden-vectors/`. The bytes are CI-generated and determinism-checked, never hand-authored.
- **Verifying runs:** CI run **30605811757** proved determinism (`Generate golden vectors (run 1)`, `(run 2)` and `Golden-vector determinism check` all success, `diff -r` empty). That run also exercises `1.0.2.3`: all six cases encode and decode to an equal `CpmContent`, `coord-large` (CartesianCoordinateLarge bounds) included. CI run **30607280047** published the corpus through the guarded `Publish golden-vector corpus` step.
- **Deviation — three commits, not one**, against [§ Subtask discipline](#subtask-discipline-applies-to-every-subtask-below): `7d99b08` (tool), `17526b8` (`fix: grant contents:write` so the publish step could push) and `9e0f43d` (corpus, authored by CI). A re-plan of this group splits "author `gv_tool`" from "generate and commit the corpus".

### [x] `1.0.2.5` — Golden-vector codec test *(agent)*

**Objective:** create `V2X_ECU/tests/codec/test_cpm_golden_vectors.cpp` + the synced pair copies under `V2X_ECU/tests/fixtures/golden/`.

**Scope:** for every corpus case: `decode(<case>.uper)` equals the `<case>.json` content, and `encode` of that content reproduces the `.uper` bytes. Copies byte-identical to `contracts/golden-vectors/`.

**Acceptance:** V2X build + ctest green over all 6 cases — this closes the milestone box "golden-vector CPMs encode/decode through the Vanetza codec seam".

**Dependencies:** after 1.0.2.4. **Commit:** `[1.0.2.5] test: verify golden vectors through the Vanetza codec seam`

**Status:** done 2026-07-31 — commit `1f81ed1`; verified green on CI run **30608005574** (`v2x-core-build`, Configure/Build/Test all success). Value-parameterized `CpmGoldenVectorTest` over the 6 case names: `DecodeYieldsTheFrozenContent` (decode of `<case>.uper` holds a `CpmContent` equal to the `<case>.json` content, printing `DecodeError::reason` on failure) and `EncodeReproducesTheFrozenOctets` (size assert then per-octet compare, so a regression names the first differing offset). Hyphenated case names are transliterated to underscores by a name generator — gtest suffixes must be alphanumeric. All 12 fixtures `cmp`-identical **and** same-blob as `contracts/golden-vectors/`; the `.uper` files each carry NUL bytes, so Git's binary detection keeps `core.autocrlf=true` on this Windows host from mangling them (`Bin 0 -> 58 bytes` in the diffstat). Closes the milestone box *"golden-vector CPMs encode/decode through the Vanetza codec seam"*. The binary-fixture caveat this raised for later fixtures is carried as an open item for [[project-architecture]] to record in `V2X_ECU/doc/`.

---

## Task Group 0.3 — R2 bindings + round-trip tests, both C++ ends (serves R2)

> One handwritten binding per node (HLD D1/D2) — no cross-node source imports; wire compatibility is enforced by the byte-synced shared sample both tests parse.

### [x] `2.0.3.1` — V2X-side R2 binding + round-trip test *(agent)*

**Objective:** create `V2X_ECU/src/contracts/r2_message.{hpp,cpp}` (nlohmann binding, producer side) + `V2X_ECU/tests/contracts/test_r2_roundtrip.cpp`.

**Scope:** fields per 2.0.1.3's schema (incl. nullable `sender.speed`, F1). Land synced copies: `V2X_ECU/contracts/r2-v2x-object.schema.json`, `V2X_ECU/tests/fixtures/samples/r2-object.json`. Test: shared sample → struct → JSON → struct equality; null `sender.speed` round-trips.

**Acceptance:** V2X build + ctest green; copies byte-identical; pure model code — no transport, no framework deps (HLD §9).

**Dependencies:** after 2.0.1.3 + 1.0.2.1. Parallel with the 1.0.2.x codec chain. **Commit:** `[2.0.3.1] feat: add V2X-side R2 binding with round-trip test`

**Status:** done 2026-07-31 — commit `f702cda`; verified green on CI run 30603927615 (`v2x-core-build`). `v2x::contracts::R2Message` producer binding, handwritten independently of the ADA sibling (no cross-node include) but mirroring its CI-green `std::optional<double>` idiom: `sender.speed` (F1) and `object.confidence` (F6) always emit the key, `null` when `nullopt`. Both copies `cmp`-identical; tests `StructAndWireEquality` · `FieldSpotChecks` (distance 25.03 = F7 hypot) · `NullSenderSpeedRoundTrips`.

### [x] `2.0.3.2` — ADA-side R2 binding + round-trip test *(agent)*

**Objective:** create `ADA_ECU/src/contracts/r2_message.{hpp,cpp}` (nlohmann binding, consumer side) + `ADA_ECU/tests/contracts/test_r2_roundtrip.cpp`.

**Scope:** same contract fields as 2.0.3.1, handwritten independently (no import from `V2X_ECU/`). Land synced copies: `ADA_ECU/contracts/r2-v2x-object.schema.json`, `ADA_ECU/tests/fixtures/samples/r2-object.json`. Same test shape as 2.0.3.1.

**Acceptance:** ADA build + ctest green; copies byte-identical.

**Dependencies:** after 2.0.1.3 + 3.0.4.1 (ADA toolchain). Parallel with 2.0.3.1. **Commit:** `[2.0.3.2] feat: add ADA-side R2 binding with round-trip test`

**Status:** done 2026-07-31 — `ada::contracts::R2Message` consumer binding (nullable `sender.speed`/`object.confidence` via std::optional, F1/F6) + round-trip and null-speed tests; copies cmp-identical; verified green on CI run 30602159929. **Commit anomaly:** the implementation landed in `dc0d424`, mis-tagged `[5.0.8.2]`. The tag stands because the commit is pushed.

---

## Task Group 0.4 — ADA ECU contract layer: toolchain, R3/R4 bindings, additive test, detector Python (serves R3, R4)

> Phase 0 lands only the contract layer of `ADA_ECU/` (HLD §5) — the Phase 2 HLD extends the tree later.

### [x] `3.0.4.1` — ADA_ECU CMake toolchain bring-up *(agent)*

**Objective:** create `ADA_ECU/CMakeLists.txt` — C++17; nlohmann/json + GoogleTest via pinned FetchContent; CTest; one sanity test. No Vanetza (ADA never touches UPER).

**Acceptance:** ADA build command green; ctest runs the sanity test; pins exact.

**Dependencies:** none — starts immediately. **Commit:** `[3.0.4.1] chore: bring up ADA_ECU C++17 toolchain`

**Status:** done 2026-07-31 — toolchain + sanity test committed; `ada-core-build` job (Configure/Build/Test steps) added to `phase0-ci.yml`; verified green on CI run 30591588639 (no local cmake on this host — CI is the Linux verification authority).

### [x] `3.0.4.2` — R3 C++ binding + round-trip test *(agent)*

**Objective:** create `ADA_ECU/src/contracts/tracked_object.{hpp,cpp}` + `ADA_ECU/tests/contracts/test_r3_roundtrip.cpp`.

**Scope:** fields per 3.0.1.4's schema (incl. `class`, `source` enum, `state` enum, `timestamps`). Land synced copies: `ADA_ECU/contracts/r3-tracked-object.schema.json`, `ADA_ECU/tests/fixtures/samples/r3-tracked-object.json`. Test: shared sample round-trip equality.

**Acceptance:** ADA build + ctest green; copies byte-identical; pure model code.

**Dependencies:** after 3.0.1.4 + 3.0.4.1. **Commit:** `[3.0.4.2] feat: add ADA R3 TrackedObject binding with round-trip test`

**Status:** done 2026-07-31 — `ada::contracts::TrackedObject` binding (`object_class` ↔ `"class"`, enum wire strings via NLOHMANN_JSON_SERIALIZE_ENUM) + round-trip test; copies cmp-identical; verified green on CI run 30602040565 (`ada-core-build` — no local cmake, CI is the authority).

### [x] `4.0.4.3` — R4 C++ binding + round-trip test *(agent)*

**Objective:** create `ADA_ECU/src/contracts/r4_message.{hpp,cpp}` (producer side: warning event + state message) + `ADA_ECU/tests/contracts/test_r4_roundtrip.cpp`.

**Scope:** fields per 4.0.1.5's schema; the embedded `object` snapshot reuses the 3.0.4.2 R3 binding. Land synced copies: `ADA_ECU/contracts/r4-ada-ivi.schema.json`, `ADA_ECU/tests/fixtures/samples/r4-warning.json`, `ADA_ECU/tests/fixtures/samples/r4-state.json`. Test: both shared samples round-trip.

**Acceptance:** ADA build + ctest green; copies byte-identical.

**Dependencies:** after 4.0.1.5 + 3.0.4.2. **Commit:** `[4.0.4.3] feat: add ADA R4 binding with round-trip test`

**Status:** done 2026-07-31 — `ada::contracts::R4WarningEvent`/`R4StateMessage` producer binding (embedded `object` reuses the R3 `TrackedObject` binding; shared `R4VehicleSet` with nullable-or-absent `vehicleC`) + round-trip tests on both shared samples; copies cmp-identical; verified green on CI run 30602717230 (`ada-core-build`).

### [x] `4.0.4.4` — ADA-side R4 additive-version test *(agent)*

**Objective:** create `ADA_ECU/tests/contracts/test_r4_additive_version.cpp` on the shared D4 fixture.

**Scope:** land the synced copy `ADA_ECU/tests/fixtures/samples/r4-unknown-warning.json`. Test per HLD D4: the 4.0.4.3 binding parses the fixture without error, preserves the unknown `warningType` string, ignores the unknown field (guards ADA-side R4 consumption in R18 tooling).

**Acceptance:** ADA build + ctest green; copy byte-identical.

**Dependencies:** after 4.0.1.6 + 4.0.4.3. **Commit:** `[4.0.4.4] test: add ADA-side R4 additive-version test`

**Status:** done 2026-07-31 — D4 fixture parses through the unmodified 4.0.4.3 binding (unknown `warningType` preserved, `schemaVersion` 2 carried, `hazardDetail` ignored and absent from re-emit); copy cmp-identical; verified green on CI run 30602717230 (`ada-core-build`).

### [x] `3.0.4.5` — Detector Python R3 binding + Python-side fixture validation *(agent)*

**Objective:** create `ADA_ECU/detector/contracts/tracked_object.py` (R3 dataclass + JSONL encode/decode — the R12 subprocess wire shape), `ADA_ECU/detector/tests/test_r3_roundtrip.py`, `ADA_ECU/detector/requirements-dev.txt` (pytest, jsonschema — test-only; runtime deps come with Phase 3).

**Scope:** stdlib `json` + dataclasses only in the binding. Test: (a) JSONL round-trip of the shared R3 sample; (b) `jsonschema` validation of all five ADA-local samples (`r2-object`, `r3-tracked-object`, `r4-warning`, `r4-state`, `r4-unknown-warning`) against the three ADA-local schemas — the one-language-validates-for-all rule (HLD D2) covering R2/R3/R4 fixtures for every consumer.

**Acceptance:** detector pytest command green; no new fixture/schema copies (reads the node-local ones landed by 2.0.3.2 / 3.0.4.2 / 4.0.4.3 / 4.0.4.4).

**Dependencies:** after 2.0.3.2 + 3.0.4.2 + 4.0.4.3 + 4.0.4.4. **Commit:** `[3.0.4.5] feat: add detector R3 JSONL binding and Python fixture validation`

**Status:** done 2026-07-31 — stdlib dataclass+JSONL binding (`object_class` ↔ `"class"`); pytest green locally (Python 3.14, jsonschema 4.26: JSONL round-trip + all five node-local samples validate against the three schemas, r4 `$ref` via referencing.Registry) and on CI run 30602717230 (`python-tests`).

---

## Task Group 0.5 — Scenario Player contract layer (serves R1)

### [x] `1.0.5.1` — Bench CpmContent dataclass + golden round-trip test *(agent)*

**Objective:** create `Scenario_Player/player/contracts/cpm_content.py` (CpmContent Python dataclass, wire-native units — the bench side of the codec seam), `Scenario_Player/tests/test_cpm_content_roundtrip.py`, `Scenario_Player/requirements-dev.txt` (pytest, jsonschema).

**Scope:** land synced copies: `Scenario_Player/contracts/r1-cpm-content.schema.json` + `Scenario_Player/tests/fixtures/golden/*.json` — this subtask lands the `.json` side only. The matching `.uper` copies are landed by the Scenario Player design, [scenario-player-design-decisions.md D2](../documents/Design/SCENARIO-PLAYER/scenario-player-design-decisions.md#d2--one-codec-source-two-build-contexts-joined-by-the-sync-manifest), and are listed in `contracts/sync-manifest.json`. Test: dataclass ⇄ JSON round-trip against every golden `.json`, plus `jsonschema` validation of each golden `.json` against the schema. **Hard constraint:** no UPER encode/decode from Python — the bench→codec path is fixed by [node-code-layout.md § Scenario_Player specifics](../.claude/rules/node-code-layout.md#scenario_player-specifics-r11); a Python UPER implementation is out of scope and fails acceptance.

**Acceptance:** Scenario_Player pytest command green; copies byte-identical; stdlib-only binding.

**Dependencies:** after 1.0.1.2 + 1.0.2.4 (golden `.json` fixtures exist). **Commit:** `[1.0.5.1] feat: add bench CpmContent dataclass with golden-vector round-trip test`

**Status:** done 2026-07-31 — commit `9878555`.

- **Shipped:** stdlib-only nested dataclasses (`CpmContent`/`ReferencePosition`/`PerceivedObject`/`ObjectPosition`/`ObjectVelocity`) with `to_dict`/`from_dict`/`to_json`/`from_json`, in a namespace-package layout mirroring `ADA_ECU/detector/`. All 7 synced copies blob-SHA-identical to their `contracts/` sources. Zero UPER or ASN.1 code in Python, per the subtask's hard constraint.
- **Verifying run:** CI run **30607692500** (`python-tests` → step `Scenario_Player unit tests` success), covering 21 tests — 6 dict round-trips, 6 JSON-text round-trips, 6 Draft-2020-12 validations, `check_schema`, a wire-native edge spot-check (mdt ±2047, classConfidence 101, gate-boundary x=3000, coord-large x=131071/y=−131072) and unknown-extra-keys tolerance.
- **Deviation:** none.

---

## Task Group 0.6 — IVI Kotlin R4/R3 binding (serves R4)

### [x] `4.0.6.1` — Freeze the IVI R4 sealed binding + finalize interim R3 models + round-trip test *(agent)*

**Objective:** create `IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/model/R4Message.kt` (sealed: `R4WarningEvent` | `R4StateMessage` per the frozen 4.0.1.5 schema) and finalize the interim models against the frozen R3/R4 schemas, with `IVI_ECU/app/src/test/java/com/hackathon/v2x/ivi/model/R4RoundTripTest.kt`.

**Scope:**

- `R3Snapshot.kt`: keep all existing field names (`id`, `source`, `position`, `distance`, `speed`, `confidence`, `state`); **add** the two missing frozen-R3 fields — object class as a Kotlin-safe property with `@SerialName("class")`, and `timestamps` per 3.0.1.4.
- `SceneGeometry.kt`: keep field names (`ego`, `vehicleB`, `vehicleC` nullable); add `@Serializable` + any distance fields the frozen R4 `geometry` carries.
- Land synced copies: `IVI_ECU/contracts/r3-tracked-object.schema.json`, `IVI_ECU/contracts/r4-ada-ivi.schema.json` (reference anchors — kotlinx consumes code, not JSON Schema), and `IVI_ECU/app/src/test/resources/contracts/samples/` `r3-tracked-object.json` · `r4-warning.json` · `r4-state.json`.
- `R4RoundTripTest.kt`: both shared samples decode → encode → decode to equal objects (kotlinx.serialization; JUnit4).
- Pure Kotlin model package — no Android/UI imports (HLD §9).

**Acceptance:** IVI test command green; copies byte-identical; existing model field names unchanged.

**Dependencies:** after 4.0.1.5. **Commit:** `[4.0.6.1] feat: freeze IVI R4 Kotlin binding and finalize R3 snapshot models`

**Status:** done 2026-07-31 — sealed `R4Message` (`warning`|`state`, discriminator `type`) + finalized `R3Snapshot` (`objectClass` @SerialName class, `timestamps`) + `@Serializable SceneGeometry`; copies cmp-identical; preview call site updated; verified green on CI run 30602040565 (`ivi-unit-tests` — local JDK 25 exceeds Gradle 8.13's range, CI is the authority). **Deviation:** the subtask carries three deliverables — the new sealed binding, the two model finalizations, and the round-trip test — against the single-objective rule of [§ Subtask discipline](#subtask-discipline-applies-to-every-subtask-below); a re-plan of this group splits them.

### [x] `4.0.6.2` — IVI-side R4 additive-version test *(agent)*

**Objective:** create `IVI_ECU/app/src/test/java/com/hackathon/v2x/ivi/model/R4AdditiveVersionTest.kt` on the shared D4 fixture.

**Scope:** land the synced copy `IVI_ECU/app/src/test/resources/contracts/samples/r4-unknown-warning.json`. Test per HLD D4: `Json { ignoreUnknownKeys = true }` parses the fixture; the unknown `warningType` classifies as a generic/unknown warning — never a parse failure (the binding-level contract Phase 5's UI behavior builds on).

**Acceptance:** IVI test command green; copy byte-identical.

**Dependencies:** after 4.0.6.1 + 4.0.1.6. **Commit:** `[4.0.6.2] test: add IVI-side R4 additive-version test`

**Status:** done 2026-07-31 — D4 fixture decodes through the unmodified 4.0.6.1 binding (`ignoreUnknownKeys` lenient parse; unknown `warningType` preserved as a usable warning, `schemaVersion` 2 carried, `hazardDetail` ignored); copy cmp-identical; verified green on CI run 30602717230 (`ivi-unit-tests`).

### IVI deliverable ownership split (HLD §11 item 5)

Ownership split so no deliverable is decomposed twice. The Phase 5 breakdown is [phase5_minh_tasks.md](phase5_minh_tasks.md); its subtask `4.5.1.4` performs the relocation named below.

| Deliverable | Owner | What Phase 5 does with it |
|---|---|---|
| `R4Message.kt` sealed models + finalized `R3Snapshot.kt`/`SceneGeometry.kt` + binding round-trip test | **`4.0.6.1` (this plan)** | Phase 5 does not rebuild them — `4.5.1.4` relocates them into its `:contract` module and consumes the models unchanged |
| Binding-level additive-version behavior (lenient parse, unknown `warningType` no-crash) + `R4AdditiveVersionTest.kt` | **`4.0.6.2` (this plan)** | Phase 5's `R4Deserializer` (Result wrapper, error taxonomy, malformed-JSON handling) is built **on** the frozen binding, and its unknown-`warningType` cases consume the shared `r4-unknown-warning.json` fixture instead of inventing payloads |
| `SceneGeometry`/`R3Snapshot` field-name compatibility | frozen by `3.0.1.4`/`4.0.1.5` schemas | field names are frozen by the `3.0.1.4` and `4.0.1.5` schemas; Phase 5 consumes them unchanged |

---

## Task Group 0.7 — Contract integrity gate + CI Linux verification (serves R1–R4; IDs anchored to R1, which also owns the F2 ban)

### [x] `1.0.7.1` — sync-manifest + byte-identity gate *(agent)*

**Objective:** create `contracts/sync-manifest.json` (source → node-local copy map, exactly the [HLD §5 sync map](../deprecated/phase0-contract-freeze-hld.md#sync-map-sync-manifestjson-content)) and `contracts/check_sync.py` (Python 3 stdlib), and run it green over the completed tree.

**Scope:** `check_sync.py` walks the manifest and exits 1 on any byte difference (D1 — copies must never drift), and additionally greps `V2X_ECU/src/` for the banned bare `asn1::Cpm` token (F2), exit 1 on hit. This is the local + CI contract-integrity gate.

**Acceptance:**

- `python contracts/check_sync.py` exits 0 on the repo as committed.
- Corrupting one copy (unstaged) makes it exit 1.
- A planted bare `asn1::Cpm` token under `V2X_ECU/src/` (unstaged) makes the F2 grep exit 1.

**Dependencies:** sequential, after every subtask that lands a synced copy listed in `contracts/sync-manifest.json` — 1.0.2.2, 1.0.2.5, 2.0.3.1, 2.0.3.2, 3.0.4.2, 4.0.4.3, 4.0.4.4, 1.0.5.1, 4.0.6.1, 4.0.6.2. **Commit:** `[1.0.7.1] feat: add contract sync manifest and byte-identity gate`

**Status:** done 2026-07-31 — commit `4a1dedb`; ran after all ten predecessors.

- **Shipped:** `contracts/sync-manifest.json` at **21 sources → 36 copies** at close, the [HLD §5 sync map](../deprecated/phase0-contract-freeze-hld.md#sync-map-sync-manifestjson-content) expanded to concrete paths; later phases extend the manifest. `contracts/check_sync.py` is Python-3 stdlib-only, reads the path map and the banned token plus its scope from the manifest, compares `read_bytes()` because the `.uper` fixtures are binary, and prints every failure before exiting 1.
- **Verifying run:** CI run **30608202261** (`contracts-gate` taking the real branch of its existence guard), proving byte-identity survives a Linux checkout. All three acceptance checks demonstrated: exit 0 from the repo root, `contracts/` and an unrelated directory; a one-byte corruption of `ADA_ECU/contracts/r3-tracked-object.schema.json` exits 1 naming the pair; an F2 probe exits 0 on `vanetza::asn1::r2::Cpm` and 1 on the bare token.
- **Deviation:** none.

### [x] `1.0.7.2` — GitHub Actions CI: Linux verification for Phase 0 *(agent)*

**Objective:** create `.github/workflows/phase0-ci.yml` — Linux (`ubuntu-latest`) verification on every push and on PRs to `main`: contracts sync gate, Python unit tests, IVI Gradle unit tests.

**Scope:**

- GitHub Actions (`origin` = `mnpham2101/FPT-Hackathon2026`) is the project's Linux verification path; the development host provides no Linux container runtime.
- Job `contracts-gate`: runs `python contracts/check_sync.py` only if that file exists — the guard keeps CI green until `1.0.7.1` lands, which is sequenced after every subtask that lands a synced copy.
- Job `python-tests`: for each of `Scenario_Player/` and `ADA_ECU/detector/`, install `requirements-dev.txt` and run pytest only where that lane's tests exist (green before 1.0.5.1 / 3.0.4.5 land).
- Job `ivi-unit-tests`: `chmod +x gradlew && ./gradlew :app:testDebugUnitTest` in `IVI_ECU/` (temurin JDK 17, Gradle cache) — the per-node build command above; the runner image carries the Android SDK.
- YAML comments mark where groups 0.2/0.4 add C++ build / docker-build jobs when their toolchains and Dockerfiles land — no empty placeholder jobs.
- Job `netcheck-image`: builds `m1-netcheck:latest` and pushes it to `registry.hackathon-2.carsky.io` when `CARSKY_ZOT_API_KEY` is set, per [node-code-layout.md § Build rules](../.claude/rules/node-code-layout.md#build-rules-all-container-nodes). Storing that secret is the Human step `5.0.8.5`; the missing credential is flagged to the user, not worked around.

**Acceptance:** workflow committed and YAML-valid; every job's existence guard skips the lanes that have not landed, so the workflow passes; CI-green evidence is recorded once the branch reaches `origin` — local JDK 25 exceeds Gradle 8.13's supported range, so the IVI job is CI-only by design.

**Dependencies:** none — lands immediately; later lanes plug into the existing guards (no workflow rewrite). **Commit:** `[1.0.7.2] chore: add Phase 0 Linux-verification CI workflow`

**Status:** done 2026-07-31 — first `phase0-ci` Actions run on `feat/phase0-contract-freeze` completed `success` (run 30590952652). YAML parse-validated locally (PyYAML); every job's existence guard skipped the lanes that had not landed, so the workflow passed; local JDK 25 exceeds Gradle 8.13's range, so CI is the Linux path.

---

## Task Group 0.8 — R5/R6 baseline connectivity smoke test (blueprint `baseline_m1`)

> Decomposed from [deploy-walkthrough-netcheck.md](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md) per [walkthrough-driven-delivery.md § Stage 2](../.claude/rules/walkthrough-driven-delivery.md) — each subtask below links the section that governs its step and restates none of it. The executor of every subtask comes from that walkthrough's [§ 5 work-division table](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human); acceptance comes from its [§ 6](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#6-expected-outputs-and-acceptance). The blueprint is `baseline_m1` ([carsky-4-node-blueprint.md § 8](../requirements/car-sky-guide/carsky-4-node-blueprint.md#8-the-blueprints-on-carsky)), cloned rather than edited in place.
>
> **Startup self-run guarantee (HLD §6, user requirement) is acceptance on every run subtask:** node start ⇒ `entrypoint.sh` self-runs `capture.sh` (background) + `netcheck.py` (foreground), roles and ports wired purely by node-config env ⇒ C1–C5 observable in each node's log with no manual invocation. A run needing a manual exec fails. Run evidence accumulates in `plans/doc/phase0-smoke-test-run.md` (created by `5.0.8.2`).

### [x] `6.0.8.1` — Author the netcheck tool *(agent)*

**Objective:** create `tools/netcheck/Dockerfile`, `entrypoint.sh`, `capture.sh`, `netcheck.py` — the four sources [M1](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m1--write-the-application-code) consumes. The walkthrough's § 5 assigns M1 to neither column: it is a development deliverable the procedure starts from.

**Scope:** the structural half of the self-run guarantee is part of the tool — `CMD ["./entrypoint.sh"]`, capture in the background and netcheck in the foreground, role wiring by environment variable only. The design behind each check is the [smoke-test note § 4](doc/research_notes/baseline-connectivity-smoke-test.md#4-tool-implementation).

**Acceptance:**

- The four files build the image the smoke test runs.
- `sh -n tools/netcheck/entrypoint.sh tools/netcheck/capture.sh` passes.
- `python -m py_compile tools/netcheck/netcheck.py` passes.
- The image build is verified by the `netcheck-image` CI job; no local Docker is required.

**Dependencies:** none — fully parallel with groups 0.1–0.7. **Commit:** `[6.0.8.1] feat: add netcheck baseline connectivity smoke-test tool`

**Status:** done 2026-07-31 — `sh -n`, `bash -n` and `py_compile` pass; LF endings verified. **Deviation — two commits, not one**, against [§ Subtask discipline](#subtask-discipline-applies-to-every-subtask-below): `c17b488` (the four files) and `1fe007a` (boot-stage and hang-location logs added to `netcheck.py`). The second commit put the shipped tool ahead of the smoke-test note's § 4.3 listing; folding it back is an open item for [[project-researcher]].

### [x] `5.0.8.5` — Store the registry credential and confirm the push run *(Human)*

**Objective:** supply the credential the `netcheck-image` job needs, and confirm the run that uses it passed. Both rows are Human in [§ 5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human).

**Steps:**

1. The user stores the Zot API key as the GitHub repository secret `CARSKY_ZOT_API_KEY`, per [M2](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m2--store-the-registry-credential-as-a-github-secret).
2. The user opens the newest `phase0-ci` run in the Actions web UI and confirms job `netcheck-image` passed, per [M4](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m3--m4--build-and-push-automatic). An agent session holds no GitHub token, so this row has no AI route.

**Acceptance:** the *Push to CarSky Zot registry* step prints `pushed …/m1-netcheck:latest` rather than `secret not set`.

**Dependencies:** before `5.0.8.2`'s push can succeed. **Commit:** no product change; the evidence lands in `5.0.8.2`'s run record.

**Status:** done 2026-07-31 — closed inside the `5.0.8.2` run, which pushed successfully and therefore proves the secret was stored and the run confirmed. This subtask carries no commit of its own. It was added by this plan update, so the mark records the earlier evidence rather than a separate close.

### [x] `5.0.8.2` — Trigger the image build and push, and confirm the registry tag *(car-sky)*

**Objective:** get the netcheck image into the registry through the `netcheck-image` CI job, and confirm the tag arrived. These are the AI rows of [M3 + M4](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m3--m4--build-and-push-automatic).

**Steps:**

1. [[car-sky]] runs its deploy preflight over the target Room and the credentials it was handed.
2. [[car-sky]] pushes a commit, which runs the `netcheck-image` job; the build and push are automatic and need no local Docker.
3. [[car-sky]] confirms the image is in the registry over the catalog and tag-list routes.
4. [[car-sky]] records the confirmed registry host and the pushed tag in `plans/doc/phase0-smoke-test-run.md`, creating that file.

**Scope:** one image for all three Container nodes — never three. Local tag `m1-netcheck:latest`; registry tag `registry.hackathon-2.carsky.io/m1-netcheck:latest`. Confirming the host **closes O1** (`registry.carsky.io` answers 502; `registry.hackathon-2.carsky.io` answers).

**Acceptance:** the catalog lists the registry tag, and the run record names the confirmed host and tag.

**Dependencies:** after `6.0.8.1` and `5.0.8.5`. **Commit:** `[5.0.8.2] docs: record netcheck image push and confirmed registry host`

**Status:** done 2026-07-31 — pushed as `registry.hackathon-2.carsky.io/m1-netcheck:latest`, single-platform `linux/arm64` (commit `5e75920`; closes O1). Pulls and runs on the three container nodes of the deployed Room `trial2_minh_netcheck` — that is the Room name as deployed, not a blueprint to clone. Container images must be single-platform `linux/arm64` — see [phase0-smoke-test-run.md § Standing requirement](doc/phase0-smoke-test-run.md). **Deviation — nine `[5.0.8.2]` commits, not one**, against [§ Subtask discipline](#subtask-discipline-applies-to-every-subtask-below); they include the CI wiring, which belongs to `1.0.7.2`'s workflow rather than to a deploy subtask, and two image-format fixes.

### [x] `5.0.8.3` — Blueprint clone, node config and deploy *(Human)*

**Objective:** perform the Human rows of M5–M9 in the Nydus UI, ending in a deployed Room. [§ 5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human) assigns each of these five rows to Human; the read-back and polling rows of the same steps are `5.0.8.6`.

**Steps:**

1. The user opens `baseline_m1` and works on a clone of it, per [M5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m5--choose-the-blueprint).
2. The user draws any missing `ethernet` pin or edge on the canvas, per [M6](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m6--check-the-wiring).
3. The user sets `image`, `command`, `capabilities` and the environment variables on the bench, V2X and ADA nodes in the Inspector, per [M7](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m7--configure-the-three-container-nodes). The ADA `LISTEN_PORT` trap is [§ 7 row 7](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#7-mistakes-already-made--check-these-first).
4. The user attaches the IVI node's VM artifact if the read-back shows it missing, per [M8](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m8--leave-the-ivi-node-alone).
5. The user starts **New Deployment** and picks the Device, per [M9](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m9--deploy--criterion-c1).

**Scope:** no agent performs these steps; the plan tracks them. **Self-run acceptance:** the deploy alone must start the scripts — a node needing a manual exec to produce logs fails the run.

**Acceptance:** **C1** — every node `Running`, restart count 0 — recorded in `plans/doc/phase0-smoke-test-run.md`. [[project-planner]] makes the evidence-record commit after the user confirms.

**Dependencies:** after `5.0.8.2`. **Commit:** `[5.0.8.3] docs: record smoke-test deployment and C1 node-Running evidence`

**Status:** done 2026-07-31 — deployed as `trial2_minh_netcheck` on room `27gs83k3oeju2mbywu1j8`; that is the deployed Room name, not a blueprint. 5/5 nodes `Running`, restart count 0, stable across a 10-minute window (C1). Deploy alone started every script — no manual exec used, self-run guarantee met. Evidence: [phase0-smoke-test-run.md § M5–M9](doc/phase0-smoke-test-run.md).

### [x] `5.0.8.6` — Read the blueprint and deployment state back over REST *(car-sky)*

**Objective:** perform the AI rows of M6–M9, which verify what `5.0.8.3` configured.

**Steps:**

1. [[car-sky]] reads every pin, edge and address back, per [M6](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m6--check-the-wiring).
2. [[car-sky]] reads each node's stored `image`, `command`, env and capabilities back, per [M7](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m7--configure-the-three-container-nodes).
3. [[car-sky]] confirms the IVI node's VM artifact is attached, per [M8](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m8--leave-the-ivi-node-alone).
4. [[car-sky]] polls node phases until every node reads `Running`, per [M9](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m9--deploy--criterion-c1).

**Scope:** read-only. No REST route updates an existing node's config, so a wrong value goes back to `5.0.8.3` rather than being corrected here.

**Acceptance:** the read-back matches the intended config, and the node-phase poll shows every node `Running` — the C1 evidence `5.0.8.3` records.

**Dependencies:** interleaved with `5.0.8.3`, each row after the Human row it verifies. **Commit:** no product change; the evidence lands in `5.0.8.3`'s run record.

**Status:** done 2026-07-31 — the node-phase poll and the config read-back are the source of the 5/5 `Running` evidence in `69c7542`. This subtask carries no commit of its own. It was added by this plan update, so the mark records the earlier evidence rather than a separate close.

### [x] `6.0.8.4` — Read the node logs and record C2–C5 and the hop-3 evidence *(car-sky)*

**Objective:** perform the AI rows of M10 and produce the acceptance evidence [§ 6](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#6-expected-outputs-and-acceptance) names.

**Steps:**

1. [[car-sky]] reads every node's log over the logs route, per [M10](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m10--read-the-logs--criteria-c2c5).
2. [[car-sky]] records **C2**, **C3**, **C4** and **C5** from those logs, against the expected lines in that section.
3. [[car-sky]] records the hop-3 evidence and which of the two methods produced it, per [§ Checking IVI RX traffic (hop 3)](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#checking-ivi-rx-traffic-hop-3).
4. [[car-sky]] appends all of it to `plans/doc/phase0-smoke-test-run.md`.

**Scope:** log reading only. The MTU probe is `6.0.8.7`/`6.0.8.8` and the teardown is `5.0.8.9`. **Self-run acceptance:** all C2–C5 evidence must come from logs of self-started scripts — any manual invocation fails the run.

**Acceptance:** C2–C5 and the hop-3 method recorded in `plans/doc/phase0-smoke-test-run.md`, and O2 answered — this closes the milestone box "blueprint topology documented + validated". O3 and O4 are carried in [§ Open items](#open-items-carried-not-decided-no-phase-0-subtask-may-close-them) and close no part of this subtask.

**Dependencies:** after `5.0.8.3` and `5.0.8.6`. **Commit:** `69c7542`, made under the `[5.0.8.3]` tag — the evidence for both subtasks landed in one commit, and no `[6.0.8.4]`-tagged commit exists.

**Status:** done 2026-07-31 — C2–C5 all met (0 `[ERR]` lines, live logs on all 5 nodes, `[CAP]` capture on the wire, accumulated stamp `seq=…|bench|v2x` at ADA). O2 closed: `NET_RAW` honored, so the capture is real tcpdump output rather than the counter fallback. Hop-3 used the indirect ADA-side check (`[TX]` plus `[CAP]`), the only method available while the VM has no listener. Evidence: [phase0-smoke-test-run.md § M10](doc/phase0-smoke-test-run.md). **Deviations:** the M10 rows were performed by the user in this run, though [§ 5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human) assigns them to AI; and the commit line above replaces a `[6.0.8.4]` tag that was recorded but never existed.

### [ ] `6.0.8.7` — Set `PAD` on the bench node and redeploy *(Human)*

**Objective:** run the optional MTU-headroom probe's Human row, per [M11](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m11--optional-mtu-headroom).

**Steps:**

1. The user sets `PAD=1400` on the bench node in the Inspector.
2. The user starts a fresh deployment of the same clone.

**Scope:** a node config edit and a redeploy. Reading the result is `6.0.8.8`.

**Acceptance:** the Room redeploys with `PAD=1400` visible in the bench node's stored config.

**Dependencies:** after `6.0.8.4`. **Commit:** no product change; the evidence lands in the run record with `6.0.8.8`.

**Status:** not started — M11 is optional in the walkthrough and was not run; O3 stays open in [§ Open items](#open-items-carried-not-decided-no-phase-0-subtask-may-close-them).

### [ ] `6.0.8.8` — Read the logs for the MTU ceiling *(car-sky)*

**Objective:** run the AI row of [M11](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m11--optional-mtu-headroom) and answer O3.

**Steps:**

1. [[car-sky]] compares arrivals across `PAD` values on the logs route.
2. [[car-sky]] bisects `PAD` to the ceiling if large datagrams do not arrive while small ones do.
3. [[car-sky]] records the ceiling in `plans/doc/phase0-smoke-test-run.md` as the CPM message-size budget input.

**Acceptance:** the run record states the MTU ceiling, closing O3.

**Dependencies:** after `6.0.8.7`. **Commit:** `[6.0.8.8] docs: record the smoke-test MTU ceiling and the CPM size budget`

**Status:** not started — depends on `6.0.8.7`.

### [ ] `5.0.8.9` — Tear down the deployment *(Human)*

**Objective:** release the Room slot, per [M12](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m12--tear-down).

**Steps:**

1. The user deletes the deployment; the blueprint is untouched and redeployable.

**Acceptance:** the deployment is gone and one of the two concurrent Room slots is free.

**Dependencies:** last in group 0.8, after `6.0.8.8`. **Commit:** no product change.

**Status:** not started — the run record carries no teardown evidence. This subtask closes no acceptance box.

---

## Execution order & parallelism

Independent start points: `1.0.1.1`, `1.0.2.1`, `3.0.4.1`, `6.0.8.1`, `1.0.7.2` (plus `3.0.1.4`).

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

Gate    contracts/:      every subtask landing a synced copy ──► 1.0.7.1

CI      .github/:        1.0.7.2 (fully parallel — guarded jobs go live as lanes A–E and the gate land)

Lane F  smoke test:      6.0.8.1 (agent) ──┐
                         5.0.8.5 (Human) ──┴──► 5.0.8.2 (car-sky) ──► 5.0.8.3 (Human) ∥ 5.0.8.6 (car-sky)
                              ──► 6.0.8.4 (car-sky) ──► 6.0.8.7 (Human) ──► 6.0.8.8 (car-sky) ──► 5.0.8.9 (Human)
                         (fully parallel with lanes A–E and the gate)
```

- **Parallel:** lanes B, C, E, F against each other once their lane-A inputs exist; within lane C, `2.0.3.2 ∥ 3.0.4.2`; `2.0.3.1` parallel with `1.0.2.2–1.0.2.5`.
- **Sequential:** every arrow above; `1.0.7.1` is last of the contract work; lane F follows the walkthrough's own M1–M12 ordering, which is binding.
- **Interleaved:** `5.0.8.6`'s read-back rows each run after the `5.0.8.3` row they verify, not after the whole subtask.

## Acceptance traceability

| Milestone Phase 0 box | Closed by |
|---|---|
| R1 profile committed; golden vectors encode/decode through the Vanetza seam | 1.0.1.1 · 1.0.1.2 · 1.0.2.1–1.0.2.5 |
| R2/R3/R4 schemas committed; round-trip tests pass in C++ / Python / Kotlin | 2.0.1.3 · 3.0.1.4 · 4.0.1.5 · 2.0.3.1 · 2.0.3.2 · 3.0.4.1 · 3.0.4.2 · 4.0.4.3 · 3.0.4.5 · 1.0.5.1 · 4.0.6.1 · (integrity: 1.0.7.1 · CI: 1.0.7.2) |
| R4 additive-version test defined | 4.0.1.6 · 4.0.4.4 · 4.0.6.2 |
| Blueprint topology documented + validated (nodes, `ethernet` pins, edges) | pre-existing guides (HLD §1) + 6.0.8.1 · 5.0.8.5 · 5.0.8.2 · 5.0.8.3 · 5.0.8.6 · 6.0.8.4 (C1–C5 on a `baseline_m1` clone) |

## Open items carried, not decided (no Phase 0 subtask may close them)

| Item | Owner / closes at | Trigger |
|---|---|---|
| Bench Python → R1 codec path (F3) — **closed**: fixed as the `cpm_encode` helper subprocess under `Scenario_Player/codec_helper/` by [node-code-layout.md § Scenario_Player specifics](../.claude/rules/node-code-layout.md#scenario_player-specifics-r11) and [scenario-player-design-decisions.md D1](../documents/Design/SCENARIO-PLAYER/scenario-player-design-decisions.md) | [[project-architecture]] | closed |
| Smoke-test O3, O4 | O3 closes at `6.0.8.8`; O4 stays with [[project-researcher]] | O1 and O2 are closed by `5.0.8.2` and `6.0.8.4`. O3 (MTU headroom) — the M11 probe was not run. O4 (AAOS `nc` availability) — the direct listener check is unavailable on this deployment ([§ Checking IVI RX traffic (hop 3)](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#checking-ivi-rx-traffic-hop-3)), so only the indirect check ran. Neither blocks Phase 0 |
| Report errata: R2 sample `distance 25.4` vs derived `25.03` (F7); `sender.speed` source wording (F1) — HLD §11 items 3–4 | [[project-researcher]] | contracts use the derived and nullable values meanwhile |
| The shipped `tools/netcheck/netcheck.py` carries boot-stage and hang-location logging (commit `1fe007a`) that the smoke-test note's [§ 4.3](doc/research_notes/baseline-connectivity-smoke-test.md#4-tool-implementation) listing does not. Two documents describe the same tool | [[project-researcher]] | fold the change into the note, so the walkthrough and the note agree with the tree |
| HLD §8 fixes the C++ floor at CMake ≥ 3.22; `V2X_ECU/CMakeLists.txt` requires 3.28 because `FetchContent_Declare(... EXCLUDE_FROM_ALL)` needs it. The HLD and the tree disagree | [[project-architecture]] | correct HLD §8, or record the per-node floor there |
| The ASN.1 member shapes recorded in `1.0.2.3`'s status, and the binary-fixture caveat in `1.0.2.5`'s, are node design facts sitting in a plan file | [[project-architecture]] | relocate them into `V2X_ECU/doc/`, and cite them from here |
| `6.0.8.7`, `6.0.8.8` and `5.0.8.9` add the walkthrough's M11 and M12 rows to a phase whose four acceptance boxes are already closed | user | decide whether the optional MTU probe and the teardown are run at all, or dropped from the plan |
