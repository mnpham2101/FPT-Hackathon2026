# Phase 0 HLD — Contract Freeze (R1–R6)

> High-level design for [milestone1.md § Phase 0](../milestone1.md#phase-0--freeze-the-contracts-r1r6), per [hld-content-and-commit-format.md](../../.claude/rules/hld-content-and-commit-format.md). Requirement definitions, field tables, and tech stacks live in [m1-cooperative-awareness.md](../../requirements/m1-cooperative-awareness.md) §2 Contracts and §3 — referenced, never restated. Call-flow source: [phase0-contract-freeze-call-flow.puml](phase0-contract-freeze-call-flow.puml).
>
> **Location note:** this HLD is cross-cutting (all four nodes), so it lives in `plans/doc/` per [node-code-layout.md § Per-folder doc/](../../.claude/rules/node-code-layout.md#per-folder-doc); later node-scoped HLDs (R11 bench, Phase 2 ADA) land in their own node's `doc/`.
>
> **Approval status:** all designations below are inside sanctioned locations (the four node folders, `plans/doc/`, the user-endorsed `tools/netcheck/`) except D1's contract source-of-truth folder `contracts/` — **approved by the user 2026-07-30** (top-level `contracts/`, the primary proposal; §3 D1, §11 item 0).

## 1. Scope

- Freeze the cross-track contracts before dependent work: R1 CPM profile document + golden vectors through the Vanetza codec seam; R2/R3/R4 schema files + per-language bindings + round-trip tests + the R4 additive-version test; R5/R6 validated live by the baseline-blueprint connectivity smoke test.
- R5/R6 topology is **already documented and live** — [carsky-4-node-blueprint.md](../../requirements/car-sky-guide/carsky-4-node-blueprint.md) + [blueprint-m1-cooperative-awareness.json](../../requirements/car-sky-guide/blueprint-m1-cooperative-awareness.json), saved on CarSky as `trial2_minh`. Phase 0 adds no topology design; it proves the deployed shape (§6).

## 2. Sourced research notes

| Note | Adopted |
|---|---|
| [baseline-connectivity-smoke-test.md](research_notes/baseline-connectivity-smoke-test.md) | **Wholesale, as the Phase 0 smoke-test procedure**: objective, pass criteria C1–C5, `tools/netcheck/` implementation (4 files, contents included), manual steps M1–M12 + node config, troubleshooting, open items O1–O4. Its `tools/netcheck/` repo-root designation is user-endorsed (2026-07-30) — recorded here as an approved location. Nothing re-derived in this HLD. |
| [scenario-player-v2x-callflow-messages.md](../../Scenario_Player/doc/research_notes/scenario-player-v2x-callflow-messages.md) | §4 CPM structure + R1→ASN.1 field mapping as the skeleton of the R1 profile document; findings F1–F2 and F5–F9 + the velocity-frame ambiguity resolved as profile conventions (§4 below). F3 (bench Python→codec path) stays open for the R11 HLD. |

Notes are non-authoritative scratch; on any conflict the CLAUDE.md document-authority order wins.

## 3. Design decisions

### D1 — Contract source of truth: top-level `contracts/` — **approved by user 2026-07-30**

- All shared contract artifacts (R1 profile doc, R1 logical-content schema, R2/R3/R4 JSON Schemas, shared sample messages, golden vectors, sync manifest + check script) live in one folder that freezes and re-freezes as a unit: **`contracts/` at the repo root** (full tree in §5) — user-approved 2026-07-30 over the `requirements/contracts/` alternative.
- **Why no sanctioned location fits:** these are machine-consumed artifacts shared by all four nodes — `doc/` subfolders are documentation-only ([node-code-layout.md](../../.claude/rules/node-code-layout.md#per-folder-doc)), no single node folder may own them (self-contained folders, no cross-node reads), and `plans/` holds plans. A shared artifact needed by two nodes is a *contract deliverable* (node-code-layout § Build rules) — this folder is where contract deliverables live.
- **Why top-level `contracts/`:** makes the frozen set maximally visible (contract-first, CLAUDE.md governing principle 1), keeps executable check code out of `requirements/`, and gives any future re-freeze one location + one commit. **Alternative if declined:** `requirements/contracts/` — precedent for JSON artifacts exists there (`car-sky-guide/*.json`), at the cost of placing `check_sync.py` (code) in the authority tree.
- **Access model — copies + sync check, never cross-folder reads.** Each node folder is a self-contained Docker build context, so no node build or test reads `contracts/` at build time. Every consumer node checks in a **verbatim copy** of the schemas/fixtures it consumes (copy map in §5); `contracts/check_sync.py` walks `sync-manifest.json` and fails on any byte difference — run locally and as the CI contract-integrity gate.
- **Binding code is per-node and handwritten**, not a synced artifact — the schema is the shared contract deliverable; wire compatibility across per-node bindings is enforced by the byte-synced shared sample fixtures parsed in every consumer's round-trip test.

### D2 — Per-language bindings + round-trip tests

- One binding per consumer language, placed in the owning node folder (paths in §5): C++ nlohmann bindings in `V2X_ECU/` (R2 producer side + CpmContent) and `ADA_ECU/` (R2/R3/R4), Python in `Scenario_Player/` (CpmContent) and `ADA_ECU/detector/` (R3 JSONL), Kotlin kotlinx.serialization in `IVI_ECU/` (R4 with embedded R3 snapshot).
- Bindings are pure model code — no transport, no UI, no framework dependencies (MVC data layer, §9).
- Round-trip tests live next to each binding and consume the synced sample fixtures: struct ⇄ JSON ⇄ struct equality, plus `jsonschema` validation of the shared samples on the Python side (one language validating the shared fixtures against the schemas covers all, since every language parses the same bytes).
- IVI note: the interim `IVI_ECU/.../model/R3Snapshot.kt` and `SceneGeometry.kt` stay field-name compatible; the frozen R3 schema adds `class` (Kotlin: `@SerialName("class")`) and `timestamps`, finalized at the existing phase5 subtask 4.5.1.1 — coordination flag in §11.

### D3 — Golden vectors + the R1 codec seam

- **Codec seam** (interface in §7): `V2X_ECU/src/codec/cpm_codec.hpp` defines `CpmContent` (the profiled logical CPM, wire-native integer units, mirrored 1:1 by `r1-cpm-content.schema.json`) and `ICpmCodec` with `encode(CpmContent) → UPER bytes` / `decode(bytes) → CpmContent`. Sole implementation `VanetzaCpmCodec` uses `vanetza::asn1::r2::Cpm` **explicitly** (F2 — bare `asn1::Cpm` is banned, enforced by the integrity gate).
- **Seam rule:** the codec is a pure representation transform — no unit conversion, no derivation. R2's SI floats, the `object.distance = hypot(x, y)` derivation (F7), and sender-speed derivation (F1) happen in the R9 pipeline above the seam. Wire-native units make encode/decode lossless and golden vectors deterministic (no float drift).
- **Generator:** `V2X_ECU/tools/golden_vectors/` builds a CLI (`gv_tool`, CMake target, not shipped in the node image) that encodes CpmContent JSON → `.uper`, decodes back, asserts identity, and writes the pair into `contracts/golden-vectors/`.
- **Corpus** (final list fixed in the profile doc): `nominal` (F7-corrected R2 sample values) · `mdt-max`/`mdt-min` (±2047 ms, F9) · `conf-unavailable` (ConfidenceLevel 101, F6) · `gate-boundary` (object at 30 m — the R13 admission seam) · `coord-large` (near CartesianCoordinateLarge bounds).
- **Reuse:** R9 (Phase 1) reuses `decode`; R11 reuses `encode` via the **open** bench Python→codec path (§11); R10 (deferred) would reuse `encode` — the seam is why R10 returns without a redesign.

### D4 — R4 additive-version test placement

- One shared fixture `contracts/samples/r4-unknown-warning.json` (higher `schemaVersion`, unknown `warningType`, one unknown extra field) — byte-synced into both consumers, so both test the same future message.
- **ADA C++ side:** `ADA_ECU/tests/contracts/test_r4_additive_version.cpp` — the nlohmann R4 binding parses it without error, preserves the unknown `warningType` string, ignores unknown fields (guards ADA-side R4 consumption: log replay/loopback in R18 tooling).
- **IVI Kotlin side:** `IVI_ECU/app/src/test/java/com/hackathon/v2x/ivi/model/R4AdditiveVersionTest.kt` — kotlinx `Json { ignoreUnknownKeys = true }` parses it; the unknown `warningType` classifies as a generic/unknown warning, never a parse failure (Phase 5's UI behavior builds on this binding-level contract).

## 4. R1 profile conventions (adopted from the callflow note's findings)

The R1 profile document `contracts/r1-cpm-profile.md` fixes these conventions — one line each here; full reasoning in the [note §6](../../Scenario_Player/doc/research_notes/scenario-player-v2x-callflow-messages.md):

| Finding | Convention frozen in the profile doc |
|---|---|
| F1 | CPM r2 carries no sender speed → R2 `sender.speed` is nullable, derived at the V2X ECU from consecutive reference position/time deltas. |
| F2 | `vanetza::asn1::r2::Cpm` only; bare `asn1::Cpm` (r1 alias) banned under `V2X_ECU/src/`. |
| F5 | Wire format = raw UPER `CollectivePerceptionMessage`, one PDU per UDP datagram, no GN/BTP envelope. |
| F6 | Confidence conversion: `ConfidenceLevel/100` clamped, `101 → null`; position confidence converts to metres, not to a probability. |
| F7 | `R2 object.distance` is derived: `hypot(position.x, position.y)`, computed in R9 — never transmitted. |
| F8 | Default message rate 10 Hz as `cpm_rate_hz` scenario config — never a literal. |
| F9 | `measurementDeltaTime` bounded ±2047 ms: bench validates before encode; R9 rejects + counts violations. |
| — | `PerceivedObject.velocity` is expressed in the sender (B) cartesian frame — one convention, both ends. |

## 5. Folder structure map — file-location designations

Every Phase 0 deliverable and its target path. No implementer picks a path ad hoc.

### Contract source of truth (approved location — D1, user 2026-07-30)

```
contracts/
├── r1-cpm-profile.md               # R1 profile document: fields/units/encoding (skeleton: callflow note §4), §4 conventions, exchange call flow, golden-vector corpus list
├── r1-cpm-content.schema.json      # JSON Schema of CpmContent (codec-seam struct, wire-native units) — the golden vectors' JSON side
├── r2-v2x-object.schema.json       # R2 schema (source of truth); sender.speed nullable per F1
├── r3-tracked-object.schema.json   # R3 schema (source of truth)
├── r4-ada-ivi.schema.json          # R4 schema: warning event + optional state message; embedded object = R3 $ref
├── samples/                        # shared cross-language fixtures: r2-object.json · r3-tracked-object.json · r4-warning.json · r4-state.json · r4-unknown-warning.json
├── golden-vectors/                 # R1 fixtures, written by gv_tool: <case>.json + <case>.uper pairs (corpus in D3)
├── sync-manifest.json              # source → node-local copy map (table below)
└── check_sync.py                   # integrity gate: byte-identity per manifest + F2 grep ban; exit 1 on drift (Python 3 stdlib)
```

### Node folders (sanctioned)

```
V2X_ECU/                                        # C++17
├── CMakeLists.txt                              # toolchain: C++17, nlohmann/json + Vanetza (ASN.1-only targets) + GoogleTest via pinned FetchContent, CTest
├── contracts/                                  # synced copies: r1-cpm-content.schema.json · r2-v2x-object.schema.json
├── src/codec/cpm_codec.hpp                     # the R1 codec seam: CpmContent + ICpmCodec (§7)
├── src/codec/vanetza_cpm_codec.{hpp,cpp}       # VanetzaCpmCodec — r2::Cpm only (F2)
├── src/contracts/r2_message.{hpp,cpp}          # R2 nlohmann binding (producer side)
├── tools/golden_vectors/main.cpp               # gv_tool CLI (D3) — build-only target, not in the node image
└── tests/
    ├── codec/test_cpm_golden_vectors.cpp       # decode(<case>.uper) == <case>.json, encode round-trip
    ├── contracts/test_r2_roundtrip.cpp
    └── fixtures/                               # synced copies: golden/ (vectors) · samples/r2-object.json

ADA_ECU/                                        # C++17 core + Python detector; the Phase 2 HLD (per requirements/ada-ecu.svg) extends this tree — Phase 0 lands only the contract layer
├── CMakeLists.txt                              # core toolchain: C++17, nlohmann/json + GoogleTest via pinned FetchContent, CTest
├── contracts/                                  # synced copies: r2 · r3 · r4 schemas
├── src/contracts/r2_message.{hpp,cpp}          # R2 nlohmann binding (consumer side)
├── src/contracts/tracked_object.{hpp,cpp}      # R3 nlohmann binding
├── src/contracts/r4_message.{hpp,cpp}          # R4 nlohmann binding (producer side)
├── tests/
│   ├── contracts/test_r2_roundtrip.cpp · test_r3_roundtrip.cpp · test_r4_roundtrip.cpp · test_r4_additive_version.cpp   # (D4)
│   └── fixtures/samples/                       # synced copies: all five samples
└── detector/
    ├── contracts/tracked_object.py             # R3 Python dataclass + JSONL encode/decode (the R12 subprocess wire shape)
    ├── tests/test_r3_roundtrip.py              # round-trip + jsonschema validation of shared samples
    └── requirements-dev.txt                    # pytest · jsonschema (test-only; runtime deps come with Phase 3)

Scenario_Player/                                # Python; runtime structure comes with the R11 HLD — Phase 0 lands only the contract layer
├── contracts/r1-cpm-content.schema.json        # synced copy
├── player/contracts/cpm_content.py             # CpmContent Python dataclass (wire-native units) — the bench side of the codec seam
├── tests/
│   ├── test_cpm_content_roundtrip.py           # round-trip vs golden-vectors JSON side + jsonschema validation
│   └── fixtures/golden/                        # synced copies: golden-vectors *.json only (.uper unused until the R11 codec path is decided)
└── requirements-dev.txt                        # pytest · jsonschema

IVI_ECU/                                        # Kotlin — existing Gradle project; kotlinx.serialization + JUnit4 already configured
├── contracts/                                  # synced reference copies: r3 · r4 schemas (sync-check anchors; kotlinx consumes code, not JSON Schema)
├── app/src/main/java/com/hackathon/v2x/ivi/model/R4Message.kt   # sealed R4 binding: R4WarningEvent | R4StateMessage; finalizes interim R3Snapshot.kt/SceneGeometry.kt (D2)
├── app/src/test/java/com/hackathon/v2x/ivi/model/R4RoundTripTest.kt
├── app/src/test/java/com/hackathon/v2x/ivi/model/R4AdditiveVersionTest.kt   # (D4)
└── app/src/test/resources/contracts/samples/   # synced copies: r3 + r4 samples
```

### Smoke-test tool (approved location — user-endorsed 2026-07-30)

```
tools/netcheck/                                 # exactly the 4 files specified in the smoke-test note §4 — contents there, not restated here
├── Dockerfile · entrypoint.sh · capture.sh · netcheck.py
```

### Sync map (`sync-manifest.json` content)

| Source (`contracts/`) | Node-local copies |
|---|---|
| `r1-cpm-content.schema.json` | `V2X_ECU/contracts/` · `Scenario_Player/contracts/` |
| `r2-v2x-object.schema.json` | `V2X_ECU/contracts/` · `ADA_ECU/contracts/` |
| `r3-tracked-object.schema.json` | `ADA_ECU/contracts/` · `IVI_ECU/contracts/` |
| `r4-ada-ivi.schema.json` | `ADA_ECU/contracts/` · `IVI_ECU/contracts/` |
| `samples/r2-object.json` | `V2X_ECU/tests/fixtures/samples/` · `ADA_ECU/tests/fixtures/samples/` |
| `samples/r3-tracked-object.json` | `ADA_ECU/tests/fixtures/samples/` · `IVI_ECU/app/src/test/resources/contracts/samples/` |
| `samples/r4-*.json` (3 files) | `ADA_ECU/tests/fixtures/samples/` · `IVI_ECU/app/src/test/resources/contracts/samples/` |
| `golden-vectors/*` | `V2X_ECU/tests/fixtures/golden/` (pairs) · `Scenario_Player/tests/fixtures/golden/` (`.json` only) |

## 6. R5/R6 smoke test

- Procedure, pass criteria (C1–C5), tool implementation, node config, manual steps (M1–M12), and troubleshooting: **adopted unchanged from [baseline-connectivity-smoke-test.md](research_notes/baseline-connectivity-smoke-test.md)** — no additional design in this HLD, and no separate diagram (the note is complete).
- **Startup self-run guarantee (user requirement, 2026-07-31):** on each Linux container node (bench, V2X, ADA), the image entrypoint invokes the test programs automatically at node start — `entrypoint.sh` launches `capture.sh` in the background and `netcheck.py` in the foreground, roles/ports wired purely by node-config env (note §4–§6). No manual exec, no interactive session: **node start ⇒ scripts self-run ⇒ C1–C5 observable in each node's View Log** — a Room deploy alone yields the pass evidence that traffic flows smoothly between the nodes. This is a testable acceptance statement, not an implication: any smoke-test run that needs a manual script invocation fails it.
- **IVI hop coverage:** the AAOS Skycraft node cannot run the scripts (note §7); hop 3 (ADA → IVI) evidence uses one of the note §7 options — ADB Shell `nc -u -l` listener, ADA-side `[TX]` + `[CAP]` capture evidence, or the real R4 listener once built — and the run records which option was used.
- Acceptance mapping: C1–C5 green on blueprint `trial2_minh` closes Phase 0's "blueprint topology documented + validated" box; the topology documents themselves pre-exist (§1).
- Execution split for the planner: writing `tools/netcheck/` files and the docker build/push (M1–M4) are agent-executable (deployment execution belongs to [[car-sky]]); M5–M12 are **user-manual Nydus UI steps**.

## 7. Codec seam interface (frozen shape)

```cpp
// V2X_ECU/src/codec/cpm_codec.hpp — the single R1 codec seam (report §3(a): one codec source behind one seam)
struct CpmContent;                    // profiled logical CPM, wire-native integer units; ⇄ JSON via nlohmann; mirrored by r1-cpm-content.schema.json
struct DecodeError { std::string reason; };

class ICpmCodec {
public:
    virtual ~ICpmCodec() = default;
    virtual std::vector<std::uint8_t> encode(const CpmContent&) const = 0;                  // → ASN.1 UPER; throws on un-encodable content (F9 bounds)
    virtual std::variant<CpmContent, DecodeError> decode(const std::uint8_t* data, std::size_t len) const = 0;
};
// Sole implementation: VanetzaCpmCodec (vanetza::asn1::r2::Cpm only — F2). Consumers: gv_tool (Phase 0), R9 decode (Phase 1), R11 encode (path open, §11), R10 encode (deferred).
```

`CpmContent` fields follow the [callflow note §4.2 mapping](../../Scenario_Player/doc/research_notes/scenario-player-v2x-callflow-messages.md) exactly — the profile doc is their normative home.

## 8. Tech stack

Traceable to the report §3 (per-track stacks) and per-requirement tech-stack lines; new picks are toolchain configuration owned by this HLD.

| Area | Stack | Trace / rationale |
|---|---|---|
| Schemas | JSON Schema draft 2020-12 | report §3 stack summary "Contracts / CI" |
| R1 codec | Vanetza ITS2 `r2::Cpm` (LGPLv3, dynamic link), ASN.1-only CMake targets, pinned FetchContent | R1 tech stack; §3(a) — no GN/BTP pull-in |
| C++ bindings | nlohmann/json (MIT), C++17 | R2/R3/R4 tech stack; §3(d) |
| C++ build/test | CMake ≥ 3.22 + GoogleTest (pinned FetchContent) + CTest | new pick: standard CTest integration and gmock available for the R7 seam tests (criteria 1, 3 over the smaller doctest, criterion 4) |
| Python | stdlib `json` + dataclasses; pytest + `jsonschema` (dev-only) | R3 tech stack; §3(d); jsonschema validates the shared fixtures once for all languages |
| Kotlin | kotlinx.serialization 1.9 + JUnit4 (already configured in `IVI_ECU/`) | R4 tech stack; §3(e) |
| Contract integrity | `check_sync.py`, Python 3 stdlib | D1 — copies must not drift; also hosts the F2 grep ban |
| Smoke test | alpine + python3 + tcpdump per the note | R5/R6; note §4.5 |

## 9. MVC mapping

- **Data layer** — everything in Phase 0 except the codec: schemas, per-language bindings, fixtures, the sync manifest. Bindings are pure models: no transport, no UI, no framework imports (the IVI model package stays pure Kotlin, as its existing files already do).
- **Business logic** — the codec seam only: a representation transform CpmContent ⇄ UPER, with all unit conversion/derivation explicitly pushed above it into R9 (D3 seam rule).
- **UI logic / UI** — none in Phase 0; the R4 binding is the data layer Phase 5's view-model and views already build against.

## 10. Call flow

[phase0-contract-freeze-call-flow.puml](phase0-contract-freeze-call-flow.puml) — PlantUML sequence: author contracts → generate/verify golden vectors through the Vanetza seam → sync copies → per-language round-trip + additive-version tests → freeze. Header note maps the runtime hops these contracts govern (bench —R1→ V2X —R2→ ADA(R3) —R4→ IVI).

## 11. Open items & flags

| # | Item | Owner / closes at |
|---|---|---|
| 0 | **D1 location approval — closed 2026-07-30:** the user approved top-level `contracts/` (primary proposal; `requirements/contracts/` alternative declined). Kept for traceability. | closed (user) |
| 1 | **Bench Python → R1 codec path** (binding, helper subprocess, or pre-encoded vectors) — standing open item in [node-code-layout.md](../../.claude/rules/node-code-layout.md#scenario_player-specifics-r11); candidates ranked in callflow note F3. Restated open — **decided in the R11 HLD, not here.** | project-architecture, R11 HLD |
| 2 | Smoke-test open items **O1–O4** (registry host · `capabilities` honored · bridge MTU → feeds the CPM size budget · AAOS `nc`) | the note's M-steps; O3 feeds `coord-large`/size checks |
| 3 | **Report erratum flag (not absorbed):** R2 sample `distance: 25.4` ≠ `hypot(25.0, 1.2) = 25.03` (F7). Profile doc + samples use the derived value; report patch is researcher's. | project-researcher |
| 4 | **Report erratum flag:** R2 `sender.speed` has no CPM r2 source field (F1) — schema marks it nullable + derived; report wording patch is researcher's. | project-researcher |
| 5 | **Coordination:** the IVI R4 binding + tests overlap Phase 5's IVI contract layer. Planner reconciles ownership — no duplicate decomposition; the split is [phase0_tasks.md § IVI deliverable ownership split](../phase0_tasks.md#ivi-deliverable-ownership-split-hld-11-item-5). | project-planner |

## 12. Phase 0 acceptance traceability

| [Phase 0 acceptance](../milestone1.md#phase-0--freeze-the-contracts-r1r6) | Closed by |
|---|---|
| R1 profile committed; golden vectors encode/decode through the Vanetza seam | `r1-cpm-profile.md` + `gv_tool` + `test_cpm_golden_vectors.cpp` (D3) |
| R2/R3/R4 schemas committed; round-trip tests pass in C++ / Python / Kotlin | schemas (D1) + the six round-trip test suites (D2, §5 paths) |
| R4 additive-version test defined | `test_r4_additive_version.cpp` + `R4AdditiveVersionTest.kt` on the shared fixture (D4) |
| Blueprint topology documented (nodes, `ethernet` pins, edges) | pre-existing guides (§1) + smoke test C1–C5 on `trial2_minh` (§6) |
