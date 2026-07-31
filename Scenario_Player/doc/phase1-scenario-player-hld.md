# Phase 1 HLD — Scenario Player CPM Generation (R11; bench side of R5/R6)

> High-level design for the bench's share of [milestone1.md § Phase 1](../../plans/milestone1.md#phase-1--comms-bring-up-v2x-ecu--scenario-player-r5r9-r11--r10-moved-to-the-future-plan), per [hld-content-and-commit-format.md](../../.claude/rules/hld-content-and-commit-format.md). Requirement definition and stack: [m1-cooperative-awareness.md](../../requirements/m1-cooperative-awareness.md) R11, §3(c)/(d). Companion: [V2X ECU Phase 1 HLD](../../V2X_ECU/doc/phase1-v2x-ecu-comms-hld.md). Call-flow source: [phase1-scenario-player-callflow.puml](phase1-scenario-player-callflow.puml).
>
> **This HLD closes the standing open item** "how Python reaches the R1 encoder" ([node-code-layout.md § Scenario_Player specifics](../../.claude/rules/node-code-layout.md#scenario_player-specifics-r11)) — decision D1.

## 1. Sourced research notes

| Note | Adopted |
|---|---|
| [scenario-player-v2x-callflow-messages.md](research_notes/scenario-player-v2x-callflow-messages.md) | §2 wire model (bench is the sole talker, unidirectional), §4 M1 CPM profile as the content the generator must produce, F3's ranked codec-path candidates (D1 picks #1), F8 (10 Hz default as `cpm_rate_hz` config), F9 (bench-side bound validation before encode). |
| [Phase 0 HLD](../../requirements/phase0-contract-freeze-hld.md) | D3 codec seam (`CpmContent` + `ICpmCodec`, wire-native units) as the exact source this bench reuses; D1 sync mechanism (copies + `check_sync.py`) extended to the codec source (D2 below); `player/contracts/cpm_content.py` + golden `.json` fixtures as the Phase 0-designated bench contract layer. |

## 2. Design decisions

### D1 — Bench → R1 codec path: **`cpm_encode` helper subprocess** (resolves F3)

- **Pick: candidate 1** — a small C++ CLI `cpm_encode`, built from the same Vanetza-based codec seam sources as the V2X ECU, invoked by Python as a **persistent subprocess**: one `CpmContent` JSON per stdin line → one base64 UPER payload per stdout line (JSONL both ways; encode failure returns an `{"error": …}` line, never kills the stream).
- Against [solution-selection-criteria](../../.claude/rules/solution-selection-criteria.md): **C1** — reuses the exact frozen codec, byte-verifiable against the Phase 0 golden vectors, and the subprocess-over-stdio pattern is already sanctioned in this repo (the R12 detector boundary); **C2** — one extra CMake target and ~100 lines of CLI, no new toolchain; **C4** — nothing new pulled in. Rejected: **pybind11 binding** (new toolchain layer, cross-compile friction inside the image build — fails C2), **pre-encoded vectors + byte patching** (drifts from the codec and fails R11's different-configs-different-streams acceptance — fails C1).
- Encoding stays behind the R1 codec seam: when R10 returns, the V2X ECU calls the same `ICpmCodec::encode` in-process — no contract change (callflow note §2.4).

### D2 — One codec source, two build contexts: sync-manifest extension

Node folders are self-contained build contexts with no cross-folder reads, so the codec seam reaches this folder the same way schemas do — **byte-synced copies** under Phase 0's `sync-manifest.json` + `check_sync.py` gate:

| Master (normative home) | Synced copy here |
|---|---|
| `V2X_ECU/src/codec/cpm_codec.hpp` · `vanetza_cpm_codec.{hpp,cpp}` | `codec_helper/src/codec/` |
| `requirements/contracts/vanetza-pin.cmake` *(new shared fragment: Vanetza git tag + asn1-only target list)* | `V2X_ECU/cmake/` · `codec_helper/cmake/` |
| `requirements/contracts/golden-vectors/*.uper` | `tests/fixtures/golden/` *(Phase 0 synced `.json` only — the "until the R11 codec path is decided" condition is now resolved, so `.uper` syncs too)* |

Drift is caught twice: byte-identity by `check_sync.py`, and wire-truth by `test_encoder_golden.py` — `cpm_encode(golden .json) == golden .uper`, byte-for-byte.

### D3 — Scenarios are declarative YAML; kinematics is one model, not code branches

- `player/scenario.py` implements a single constant-velocity kinematic model in B's frame: given `t`, it yields sender B's pose (static WGS84 pose + heading per config) and object C's relative state (`initial_distance_m` + `closing_speed_mps` along x, fixed `lateral_offset_m`), emitted as `CpmContent` in wire-native units (conversion table = the callflow note §4.2 mapping; F9 bound asserted before encode).
- Scenario YAML shape (validated by `player/config.py`; every tunable lives here or in env — no literals): `name`, `cpm_rate_hz` (default 10, F8), `duration_s`, `loop`, `sender {station_id, lat, lon, heading_deg}`, `object {object_id, initial_distance_m, closing_speed_mps, lateral_offset_m, classification, confidence}`.
- Committed variants (R11 acceptance — observably different streams): `scenarios/default.yaml` (C approaching: 60 m closing to ~10 m) and `scenarios/c-out-of-range.yaml` (C static beyond the 35 m exit gate) — chosen to drive the R13 admission/drop lifecycle in later phases. New variants are new YAML files, never code.
- Selected at runtime by `SCENARIO_CONFIG` (blueprint-fixed default `/app/scenarios/default.yaml`).

### D4 — Runtime composition

`main.py` (blueprint-fixed entrypoint) → load env + YAML → spawn `cpm_encode --stream` → rate loop: `scenario.sample(t)` → encode via `player/encoder_client.py` → `player/sender.py` UDP to `V2X_ECU_HOST:V2X_ECU_PORT` → `[TX]` JSONL log line per datagram (scenario time, seq, byte length). `loop: true` restarts scenario time; encoder-subprocess death → logged restart with backoff (bench is test equipment — it must stay alive and observable in View Log).

## 3. Folder structure map — file-location designations

P0 = Phase 0-designated, listed for context; everything else is Phase 1:

```
Scenario_Player/
├── Dockerfile                          # multi-stage: stage 1 cmake-builds codec_helper → stage 2 python-slim + cpm_encode binary + player/
├── main.py                             # entrypoint at /app (blueprint command: ["python", "main.py"])
├── requirements.txt                    # runtime: PyYAML
├── requirements-dev.txt                # P0: pytest · jsonschema
├── player/
│   ├── __init__.py
│   ├── config.py                       # env + scenario-YAML loading/validation (D3); the only env reader
│   ├── scenario.py                     # kinematic model → CpmContent over time (business logic)
│   ├── generator.py                    # cpm_rate_hz loop, scenario clock, loop/duration handling
│   ├── encoder_client.py               # persistent cpm_encode subprocess client (JSONL ↔ base64)
│   ├── sender.py                       # UDP tx to V2X_ECU_HOST:V2X_ECU_PORT
│   └── contracts/cpm_content.py        # P0: CpmContent dataclass (wire-native units)
├── codec_helper/
│   ├── CMakeLists.txt                  # builds cpm_encode; Vanetza via synced vanetza-pin.cmake + nlohmann
│   ├── cmake/vanetza-pin.cmake         # synced copy (D2)
│   └── src/
│       ├── main.cpp                    # cpm_encode CLI: --stream (JSONL loop) · --encode <file> (one-shot, used by tests)
│       └── codec/                      # synced copies of the V2X_ECU codec seam (D2) — never edited here
├── scenarios/
│   ├── default.yaml                    # C approaching (D3)
│   └── c-out-of-range.yaml             # C beyond exit gate (D3)
├── tests/
│   ├── test_cpm_content_roundtrip.py   # P0
│   ├── test_config.py                  # YAML validation incl. rejection cases
│   ├── test_scenario_kinematics.py     # sampled CpmContent vs hand-computed positions; F9 bound
│   ├── test_streams_differ.py          # the two committed YAMLs yield differing CpmContent sequences (R11 acceptance, model level)
│   ├── test_encoder_golden.py          # cpm_encode(golden .json) == golden .uper bytes (D2; needs built helper — CI builds it)
│   └── fixtures/golden/                # P0 .json + P1 synced .uper
└── doc/
    ├── research_notes/                 # existing
    ├── phase1-scenario-player-hld.md   # this document
    └── phase1-scenario-player-callflow.puml
```

## 4. Tech stack

| Area | Stack | Trace |
|---|---|---|
| Generator | Python 3.11, stdlib + PyYAML | report §3(c)/(d) |
| Encoder | `cpm_encode` C++17 CLI over the shared Vanetza ITS2 `r2::Cpm` seam (LGPLv3, dynamic link) | report R11 tech stack "shared R1 codec"; D1 |
| Transport | stdlib `socket` UDP | report R6 |
| Tests | pytest + jsonschema (P0 toolchain); CMake+CTest for the helper | Phase 0 §8 |
| Image | Docker multi-stage (cmake builder → python-slim runtime) | R5 |

## 5. Configuration

| Env | Default | Meaning |
|---|---|---|
| `SCENARIO_CONFIG` | `/app/scenarios/default.yaml` (blueprint) | active scenario file |
| `V2X_ECU_HOST` / `V2X_ECU_PORT` | `10.99.0.11` / `47100` (blueprint) | target peer |
| `ENCODER_PATH` | `/app/cpm_encode` | helper binary location (test override) |

All scenario-content tunables (rate, kinematics, IDs) live in the YAML (D3), never in code or env.

## 6. Call flow

[phase1-scenario-player-callflow.puml](phase1-scenario-player-callflow.puml) — PlantUML sequence: config load → helper spawn → rate loop (sample → encode → send → log) with the encode-error and helper-restart paths.

## 7. MVC mapping

- **Data** — `scenarios/*.yaml`, `player/contracts/cpm_content.py`, golden fixtures.
- **Business logic** — `player/scenario.py` (kinematics → CpmContent), the codec (in the helper).
- **Controller** — `main.py` + `player/generator.py` / `encoder_client.py` / `sender.py` (orchestration and I/O edges).
- **UI** — none; observability is the `[TX]` JSONL stream in View Log.

## 8. Deployment shape (R5/R6)

- Image `scenario-player:latest` built from `Scenario_Player/` alone (the helper compiles inside stage 1 of this folder's Dockerfile — self-contained context preserved); push + node config per [node-scenario-player.md](../../requirements/car-sky-guide/node-scenario-player.md) — **unchanged**: command `["python", "main.py"]`, env as designated there. No capture on this node — the V2X ECU interface captures both live flows ([companion HLD D5](../../V2X_ECU/doc/phase1-v2x-ecu-comms-hld.md)).
- Scenario switching (R11 acceptance) = edit `SCENARIO_CONFIG` in the node config + redeploy — a user Nydus UI step, no rebuild.

## 9. Acceptance traceability

| [Phase 1 acceptance](../../plans/milestone1.md#phase-1--comms-bring-up-v2x-ecu--scenario-player-r5r9-r11--r10-moved-to-the-future-plan) | Closed by |
|---|---|
| Different scenario configs → observably different streams (R11) | D3 variants + `test_streams_differ.py` (model level) + differing V2X `[EVT]` Rx logs on config swap (live level) |
| Generated messages received and decoded by the V2X ECU (R11) | D1 shared-codec path + D2 golden byte-equality + companion R9 pipeline |
| Bench side of R5/R6 (image, node Running, wiring) | §8 + node guide |

## 10. Open items & flags

| # | Item | Owner / closes at |
|---|---|---|
| 1 | Phase 0 prerequisite: codec seam sources + golden vectors + `sync-manifest.json` must exist before `codec_helper/` and `test_encoder_golden.py` can land; manifest gains the D2 entries (incl. the new `vanetza-pin.cmake`) as a Phase 1 subtask. | project-planner sequencing |
| 2 | Builder-stage base image / architecture must match the cluster (`--platform`, smoke-test note M3); helper binary is linked against stage-1 libs — stage 2 must carry the matching runtime (or the helper is built static where Vanetza allows: LGPLv3 requires the dynamic-link posture already accepted in the report §4 — keep dynamic, copy `libvanetza_asn1*.so` into stage 2). | implementation subtask |
| 3 | `c-out-of-range.yaml` distance value intentionally exceeds `gate_exit` (35 m) — constants stay in the YAML, but the pairing with R13 gate config should be re-checked when Phase 2 freezes gate values. | project-planner (Phase 2 cross-check) |
