# Phase 1 HLD — V2X ECU Comms Bring-up (R7–R9, R18 start; R5/R6 execution)

> High-level design for the V2X ECU's share of [milestone1.md § Phase 1](../../plans/milestone1.md#phase-1--comms-bring-up-v2x-ecu--scenario-player-r5r9-r11--r10-moved-to-the-future-plan), per [hld-content-and-commit-format.md](../../.claude/rules/hld-content-and-commit-format.md). Requirement definitions and tech stacks: [m1-cooperative-awareness.md](../../requirements/m1-cooperative-awareness.md) §2 R5–R9, §3(a)/(b)/(d) — referenced, never restated. Companion: [Scenario Player Phase 1 HLD](../../Scenario_Player/doc/phase1-scenario-player-hld.md). Call-flow source: [phase1-v2x-ecu-callflow.puml](phase1-v2x-ecu-callflow.puml).
>
> **Receive-only node** — R10 is deferred (report §4, 2026-07-30): the R7 seam declares `send`, nothing calls it.

## 1. Scope

- V2X ECU application: R7 radio adapter seam, R8 modem stub FSM with fault injection, R9 Rx pipeline (decode → validate → dedupe → forward R2), and the start of the R18 JSONL evidence stream.
- Deployment shape of this node for R5/R6: image layout, entrypoint, blueprint node-config changes, and the R6 **traffic capture** design (tcpdump, per user directive 2026-07-30).
- Phase 1 interim observation point at the ADA node (its real code is Phase 2) so the "R2 observed at the ADA ECU" acceptance is checkable.
- **Prerequisite:** the Phase 0 contract layer ([phase0-contract-freeze-hld.md](../../requirements/phase0-contract-freeze-hld.md)) — codec seam `src/codec/`, R2 binding `src/contracts/`, golden vectors, `CMakeLists.txt` baseline. Phase 1 extends that tree; it does not redefine it.

## 2. Sourced research notes

| Note | Adopted |
|---|---|
| [scenario-player-v2x-callflow-messages.md](../../Scenario_Player/doc/research_notes/scenario-player-v2x-callflow-messages.md) | §2 call flow (§A bring-up produces no wire traffic; §B is the only live flow; wire is unidirectional) — this HLD's runtime flow is that note made concrete. Conventions F1/F6/F7 (derivations in R9, above the codec seam) and F9 (reject + count) placed per [Phase 0 HLD §4](../../requirements/phase0-contract-freeze-hld.md). |
| [baseline-connectivity-smoke-test.md](../../plans/doc/research_notes/baseline-connectivity-smoke-test.md) | Capture technique (`[CAP]`-prefixed tcpdump in-container, `NET_RAW` via flat `capabilities`, `/proc/net/dev` fallback), View-Log-as-retrieval model, and the `tools/netcheck/` image reused as the Phase 1 ADA-side R2 sink (D6). Open items O1–O3 inherited (§11). |

## 3. Design decisions

### D1 — Module layering; the seam made checkable

- Single socket-API holder: **`src/net/udp_socket.{hpp,cpp}`** is the only code allowed to include socket headers. The R7 adapter implementation and the ADA forwarder both consume `net::UdpSocket`; pipeline, stub logic, codec, contracts, and config stay transport-blind.
- The ADA-facing link (R2 forwarding) is **not** under the R7 seam — the seam mirrors the radio (telux) surface only; the intra-ego link stays UDP on real hardware. Hence two distinct edges: `adapter/` (radio, seam-governed) and `forward/` (intra-ego).
- **CI import check (R7 acceptance), made precise:** `tools/check_transport_imports.py` fails if any file outside `src/net/` includes `<sys/socket.h>`, `<netinet/*>`, `<arpa/*>`, or `<asio*>`; it also re-asserts the F2 grep ban (bare `asn1::Cpm`). Run locally + as the CI gate alongside `check_sync.py`.

### D2 — R7 adapter + R8 stub: in-process pair behind the seam

- `adapter/i_radio_adapter.hpp` freezes the seam: `init() · configure(RadioConfig) · subscribeRx(RxCallback) · send(bytes)` with typed result codes — names and call order mirror the telux parity notes ([doc designated below](#5-folder-structure-map--file-location-designations); authored as an R7 implementation deliverable).
- `StubRadioAdapter` implements the seam against `stub/modem_stub.{hpp,cpp}` — the R8 FSM `idle → initialized → configured → rx-subscribed`, acking each call. On `rx-subscribed` the stub opens the `LISTEN_PORT` UDP socket (via `net::`) on a dedicated Rx thread and delivers each datagram to the subscribed callback. `send` returns `NotSupported` and logs — R10-deferred behavior, seam unchanged.
- **Fault injection is config-driven** (`FAULT_PLAN` env: `none | init_fail | configure_reject | subscription_drop`), and each fault has a **defined, logged recovery** (R8 acceptance):

| Fault | Defined recovery |
|---|---|
| `init_fail` / `configure_reject` | retry with backoff (`RETRY_BACKOFF_MS`, up to `INIT_RETRY_MAX`), then exit non-zero — container restart is the logged last-resort recovery |
| `subscription_drop` | automatic re-`subscribeRx` with the same backoff, unbounded; drop + resubscribe both logged |

### D3 — R9 Rx pipeline: four stages, each a unit-testable class

`pipeline/rx_pipeline.{hpp,cpp}` runs synchronously on the Rx thread (10 Hz load — no queueing needed in M1):

1. **decode** — `ICpmCodec::decode` (Phase 0 seam, `r2::Cpm` only). `DecodeError` → reject, count, log — never crash (malformed corpus: `tests/fixtures/malformed/` — empty, truncated golden vector, random bytes, wrong `messageId`/`protocolVersion`, r1-variant CPM, oversized).
2. **validate** — `pipeline/validator.{hpp,cpp}`: mandatory-field presence and profile ranges per `r1-cpm-profile.md`, incl. F9 `|measurementDeltaTime| ≤ 2047`. Violations reject + count by reason.
3. **dedupe** — `pipeline/deduper.{hpp,cpp}`: key `(stationId, objectId, referenceTime + measurementDeltaTime)`, sliding window `DEDUPE_WINDOW_MS`. Duplicates drop + count.
4. **build + forward** — `pipeline/r2_builder.{hpp,cpp}` maps `CpmContent` → the Phase 0 R2 binding, owning every derivation the codec seam excludes: F7 `object.distance = hypot(x, y)`, F6 confidence conversions, F1 `sender.speed` derived from consecutive `referencePosition`/`referenceTime` deltas per `stationId` (nullable until the 2nd message), `rxTime` stamped at datagram receipt. `forward/ada_forwarder.{hpp,cpp}` sends the JSON to `ADA_ECU_HOST:ADA_ECU_PORT`.

### D4 — R18 evidence stream starts here

- `log/event_log.{hpp,cpp}` writes one JSONL line per event: `rx_datagram`, `decode_ok`, `decode_reject`, `validate_reject`, `dedupe_drop`, `r2_forwarded`, `stub_transition`, `fault_injected`, `recovery` — each with monotonic + epoch timestamps and stage counters.
- Sink: stdout always (CarSky View Log is the live window); additionally to `EVENT_LOG_PATH` when set. Event lines are prefixed `[EVT]` so they interleave cleanly with `[CAP]` capture lines.

### D5 — R6 traffic capture (user directive 2026-07-30: tcpdump; saved + read by script or Wireshark)

- The V2X ECU interface sees **both** live Phase 1 flows (bench→V2X R1, V2X→ADA R2), so this node is the single capture point; `capture.sh` + tcpdump ship in the image, started by `entrypoint.sh` alongside the app. Requires `"capabilities": ["NET_RAW"]` flat in the node config (verified shape per the smoke-test note; `/proc/net/dev` counter fallback if unhonored — O2).
- Two tcpdump processes, two consumers:
  - **Live text** — `tcpdump -i any -n -l -tttt $CAPTURE_FILTER` → stdout as `[CAP]` lines: the live, human-readable check in View Log.
  - **Saved pcap** — `tcpdump -w` into `$PCAP_DIR`, rotating every `CAPTURE_ROTATE_S`; on each rotation the closed file is base64-emitted to stdout between `[PCAP-BEGIN <name>]` / `[PCAP-END]` markers — a byte-perfect export channel through the only egress the platform offers (View Log; no platform file/pcap endpoint exists per the smoke-test note).
- **Retrieval** (procedure + exact commands: [traffic-capture-wireshark.md](../../requirements/car-sky-guide/traffic-capture-wireshark.md)): user saves View Log → `tools/extract_pcap.sh <saved.log>` cuts the marker block, base64-decodes, and writes `<name>.pcap` → opens in Wireshark. The script is the "automatic tool" path; manual grep/base64 one-liners are documented as fallback.
- **Wireshark dissection caveat, stated up front:** the wire format is raw UPER `CollectivePerceptionMessage` per UDP datagram, no GN/BTP (F5) — Wireshark's ITS dissector keys on GN/BTP framing, so payloads display as UDP data. The demo evidence is the capture itself with payload bytes matching the golden vectors / `[EVT]` decode log (correlate by timestamp + length), not an ITS protocol tree.

### D6 — Phase 1 R2 observation at the ADA node

- ADA's real code is Phase 2; Phase 1 deploys the **`tools/netcheck/` image** (Phase 0 smoke-test equipment, user-endorsed location) on the ADA node as a pure sink: `ROLE=ada-sink, LISTEN_PORT=47200`, no `NEXT_HOP_*` → its `[RX]` log lines show the R2 JSON body — "R2 messages observed at the ADA ECU carrying decoded bench-scenario values".
- Netcheck implementation note (flagged to the planner, lands with the Phase 0 netcheck subtask): parameterize the body preview length as `BODY_PREVIEW` env (spec'd literal 96 truncates R2 JSON); the sink sets `BODY_PREVIEW=512`.

## 4. Folder structure map — file-location designations

Phase 1 additions on the Phase 0-designated tree (unmarked = Phase 0, listed for context only):

```
V2X_ECU/
├── Dockerfile                          # P1: multi-stage — cmake build stage → slim runtime (+ tcpdump)
├── entrypoint.sh                       # P1: capture.sh & → exec ./v2x_ecu
├── capture.sh                          # P1: D5 — live-text tcpdump + rotating pcap + base64 export
├── CMakeLists.txt                      # P0 baseline; P1 adds v2x_ecu executable + new test targets
├── contracts/                          # P0 synced schemas
├── src/
│   ├── main.cpp                        # P1: composition root — Config → stub/adapter → pipeline → run
│   ├── config/config.{hpp,cpp}         # P1: env loading/validation (table §6); the only env reader
│   ├── net/udp_socket.{hpp,cpp}        # P1: sole socket-API holder (D1)
│   ├── adapter/i_radio_adapter.hpp     # P1: the frozen R7 seam
│   ├── adapter/stub_radio_adapter.{hpp,cpp}   # P1: seam impl over the modem stub
│   ├── stub/modem_stub.{hpp,cpp}       # P1: R8 FSM + fault injection
│   ├── codec/                          # P0 seam (cpm_codec.hpp, vanetza_cpm_codec.{hpp,cpp})
│   ├── contracts/r2_message.{hpp,cpp}  # P0 R2 binding
│   ├── pipeline/{rx_pipeline,validator,deduper,r2_builder}.{hpp,cpp}   # P1: D3
│   ├── forward/ada_forwarder.{hpp,cpp} # P1: R2 UDP sender (intra-ego edge)
│   └── log/event_log.{hpp,cpp}         # P1: R18 JSONL writer (D4)
├── tools/
│   ├── golden_vectors/                 # P0 gv_tool
│   ├── check_transport_imports.py      # P1: R7 CI import check (D1)
│   └── extract_pcap.sh                 # P1: host-side log → .pcap extraction (D5; not in the image)
├── tests/
│   ├── codec/                          # P0 golden-vector tests
│   ├── contracts/                      # P0 round-trip
│   ├── stub/test_modem_stub_fsm.cpp    # P1: FSM transitions + all fault plans + recoveries
│   ├── pipeline/test_validator.cpp · test_deduper.cpp · test_r2_builder.cpp   # P1: incl. F1/F6/F7 cases
│   ├── pipeline/test_rx_pipeline_malformed.cpp   # P1: full corpus rejected, zero crashes, counters correct
│   └── fixtures/
│       ├── golden/ · samples/          # P0 synced
│       └── malformed/                  # P1: R9 corpus (local fixture, not a synced contract)
└── doc/
    ├── phase1-v2x-ecu-comms-hld.md     # this document
    ├── phase1-v2x-ecu-callflow.puml    # P1 call flow (§7)
    └── telux-parity-and-port-plan.md   # P1 deliverable, authored by the R7 implementation subtask
```

## 5. Tech stack

| Area | Stack | Trace |
|---|---|---|
| Application | C++17, single process + Rx thread | report §3(d) |
| Codec | Phase 0 seam — Vanetza ITS2 `r2::Cpm` (LGPLv3, ASN.1-only targets, pinned FetchContent) | report §3(a); Phase 0 D3 |
| R2 message | nlohmann/json via the Phase 0 binding | report R2 |
| Transport | POSIX UDP via `src/net/` | report R6/§3(b) |
| Build/test | CMake ≥ 3.22 + GoogleTest + CTest (Phase 0 toolchain) | Phase 0 §8 |
| Capture | tcpdump (image-installed), base64/coreutils; Wireshark host-side | R6/R19 tech stack; user directive |
| Image | Docker multi-stage (build → slim runtime) | R5 |

## 6. Configuration (no hardcoded tunables — every value env-injected by the blueprint)

| Env | Default | Meaning |
|---|---|---|
| `LISTEN_PORT` | `47100` (blueprint) | R7 stub Rx port (bench-facing) |
| `ADA_ECU_HOST` / `ADA_ECU_PORT` | `10.99.0.12` / `47200` (blueprint) | R2 forward target |
| `FAULT_PLAN` | `none` | R8 injection: `none·init_fail·configure_reject·subscription_drop` |
| `INIT_RETRY_MAX` | `3` *(proposal)* | init/configure retry ceiling (D2) |
| `RETRY_BACKOFF_MS` | `500` *(proposal)* | backoff base for D2 recoveries |
| `DEDUPE_WINDOW_MS` | `1500` *(proposal)* | R9 dedupe sliding window |
| `EVENT_LOG_PATH` | *(empty = stdout only)* | R18 JSONL file sink |
| `CAPTURE_FILTER` | `udp` | tcpdump BPF filter |
| `PCAP_DIR` | `/data/capture` | rotating pcap directory |
| `CAPTURE_ROTATE_S` | `60` | pcap rotation + base64-export period |

*(proposal)* values are architecture proposals to confirm — no committed acceptance criterion fixes them.

## 7. Call flow

[phase1-v2x-ecu-callflow.puml](phase1-v2x-ecu-callflow.puml) — PlantUML sequence: bring-up FSM (with the fault/recovery `alt`), then the live loop bench datagram → decode → validate → dedupe → R2 build → forward → ADA sink, with `[EVT]`/`[CAP]` emission points marked.

## 8. MVC mapping

- **Data** — `config/`, `contracts/` bindings, `log/event_log` (persistence of the evidence stream), fixtures.
- **Business logic** — `pipeline/*` (validation, dedupe, R2 construction with F1/F6/F7 derivations), `stub/modem_stub` FSM, the codec seam.
- **Controller** — `main.cpp` composition root + `adapter/` (mediates between the transport edge and the pipeline).
- **UI** — none on this headless node; observability is the `[EVT]`/`[CAP]` stream in View Log (the R18/R6 surfaces).

## 9. Deployment shape (R5/R6)

- Image `v2x-ecu:latest`, built from `V2X_ECU/` alone (self-contained context), pushed per [node-v2x-ecu.md](../../requirements/car-sky-guide/node-v2x-ecu.md) — that guide is updated by this design: `command` becomes `["./entrypoint.sh"]`, `capabilities: ["NET_RAW"]` added, env table extended per §6.
- ADA node runs the netcheck sink for Phase 1 only (D6); the bench node config is unchanged ([companion HLD §9](../../Scenario_Player/doc/phase1-scenario-player-hld.md)).
- Execution split: image build/push + node-config values are [[car-sky]]-executable; blueprint edits and deploy/verify clicks are user Nydus UI steps (REST cannot edit node config — smoke-test note M7).

## 10. Acceptance traceability (Phase 1 criteria this HLD closes the design for)

| [Phase 1 acceptance](../../plans/milestone1.md#phase-1--comms-bring-up-v2x-ecu--scenario-player-r5r9-r11--r10-moved-to-the-future-plan) | Closed by |
|---|---|
| Blueprint deploys, nodes Running (R5) | §9 image + guide updates; deploy execution per node guides |
| UDP reachability + traffic captured (R6) | Phase 0 smoke test (reachability) + D5 capture on the live flows |
| CI import check; telux parity notes + port plan (R7) | D1 check + seam interface; `doc/telux-parity-and-port-plan.md` |
| Scripted call flow acked/logged; faults → defined logged recovery (R8) | D2 FSM + recovery table + `test_modem_stub_fsm` |
| Golden vectors decode; malformed corpus rejected, zero crashes (R9) | D3 stage 1 + corpus + `test_rx_pipeline_malformed` |
| R2 at the ADA ECU with decoded bench values (R2) | D3 stage 4 + D6 sink log |
| **Demo:** Wireshark capture of V2X PDUs at the V2X ECU interface | D5 pcap export + [capture guide](../../requirements/car-sky-guide/traffic-capture-wireshark.md) (dissection caveat noted) |

## 11. Open items & flags

| # | Item | Owner / closes at |
|---|---|---|
| 1 | Phase 0 implementation is a hard prerequisite (codec seam, R2 binding, golden vectors, netcheck tool) — Phase 1 subtasks touching them are sequential after Phase 0 lands. | project-planner sequencing |
| 2 | Inherited smoke-test opens: O1 registry host, O2 `capabilities` honored (capture falls back to counters), O3 bridge MTU (feeds CPM size budget). | first deploy (M-steps) |
| 3 | `BODY_PREVIEW` parameterization folded into the netcheck implementation subtask (D6). | project-planner |
| 4 | Blueprint `command`/`capabilities` change for this node is a manual Nydus UI edit — flag in the deploy task, not automatable. | deploy task / user |
| 5 | *(proposal)* defaults in §6 to confirm with the user; externalized either way. | user ratification |
