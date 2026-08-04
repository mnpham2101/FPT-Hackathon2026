# Phase 2 — ADA Scaffolding (store + R13 admission + R14 abstraction, no detector): Full Task Breakdown

> **Authority & context:**
> - **Phase content:** [milestone1.md § Phase 2](milestone1.md#phase-2--ada-scaffolding-store--state-machine-no-detector-r3-r13) — its six acceptance checkboxes are the phase output.
> - **Design:** [ada-ecu-hld.md](../ADA_ECU/doc/ada-ecu-hld.md) — §4 folder structure, §6 components, env tables and § Startup validation, §12 test strategy, and the [decision record](../ADA_ECU/doc/ada-ecu-design-decisions.md) D1–D11. Every `ADA_ECU/` path below is cited from its §4; diagrams [components](../ADA_ECU/doc/phase2-4-ada-ecu-components.puml) · [call flow](../ADA_ECU/doc/phase2-4-ada-ecu-callflow.puml) · [admission](../ADA_ECU/doc/phase2-4-ada-ecu-admission.puml).
> - **Requirements:** [m1-cooperative-awareness.md §2](../requirements/m1-cooperative-awareness.md) R2, R3, R5, R6, R12–R14, R18 — referenced by number, never restated.
> - **Run timing:** [m1-run-timing-and-event-triggering.md](../requirements/m1-run-timing-and-event-triggering.md) — §6.2's clock-domain ruling, which `2.2.3.1` and `13.2.4.3` implement; §6.1's env values and §6.6's R22 choreography, which `13.2.2.1` loads (HLD [D10](../ADA_ECU/doc/ada-ecu-design-decisions.md#d10--clock-domains-and-stimulus-paced-against-clock_monotonic), [D11](../ADA_ECU/doc/ada-ecu-design-decisions.md#d11--r22-run-choreography-the-run-origin-the-paced-clip-and-the-risk-band-pair)).
> - **Phase 0 baseline (present on `main`, do not re-plan):** `contracts/` frozen + `sync-manifest.json` + `check_sync.py`; `ADA_ECU/contracts/` synced schema copies; `ADA_ECU/src/contracts/{tracked_object,r2_message,r4_message}.{hpp,cpp}` bindings; `ADA_ECU/detector/contracts/tracked_object.py`; `ADA_ECU/tests/contracts/` round-trip tests; `ADA_ECU/CMakeLists.txt` with the `ada_add_test()` helper.
> - **Deploy guide:** [node-ada-ecu.md](../requirements/car-sky-guide/node-ada-ecu.md) — its § Blueprint node config carries `command: ["./entrypoint.sh"]`, `capabilities: ["NET_RAW"]` and the `registry.hackathon-2.carsky.io` host; `5.2.9.4` adds only the §6 env rows.
> - **Rules:** [task-planning-conventions.md](../.claude/rules/task-planning-conventions.md) (`X.Y.Z.W`; subtask discipline restated once in § Subtask discipline below); [node-code-layout.md](../.claude/rules/node-code-layout.md).
>
> **Task ID legend:** `X.2.Z.W` — X = requirement served · 2 = this phase · Z = task group · W = subtask position within the group. IDs are stable; never renumber, never reuse a retired one.

## Phase 2 overview

**Objective.** Stand up the ADA ECU skeleton inside `ADA_ECU/`: the C++17 core (config, socket, event log, observers, parsers), the R3 track store, the R13 admission state machine, and the R14 Collision Risk Assessment abstraction with its committed database schema. Every one of those runs on mock input — a JSONL fixture through the real detector-reader, and mock R2 traffic through the real socket — so the pipeline works before any ML. The phase also lands the deployable image, the ADA CI lanes, and the R12 clip preflight Phase 3 depends on.

**Input (must exist before start).** All present on `main`:

- The Phase 0 contract layer listed under § Authority above.
- [ada-ecu-hld.md](../ADA_ECU/doc/ada-ecu-hld.md) and [video-source-for-r12.md](../ADA_ECU/doc/research_notes/video-source-for-r12.md) committed.
- Phase 1's frozen R2 producer shape — `V2X_ECU/src/forward/ada_forwarder` sends one compact-JSON `R2Message` datagram per perceived-object update to `ADA_ECU_HOST:47200`. Phase 2 consumes that shape from `tools/mock_v2x_sender.py`; **no live Phase 1 node is required.**

Nothing in this phase waits on a person before it can start.

**Output (phase acceptance = the six milestone boxes):**

- [ ] The store exposes all R3 fields; detector-shaped and relayed-shaped entries enter through the identical interface (R3) — closed by `3.2.4.1` + `2.2.3.1` + `3.2.3.2` (both parsers call the same `upsert`).
- [ ] Mock-driven state transitions are observable in logs and match the R13 diagram; toggling the mock off yields no tracks — closed by `13.2.4.2` + `13.2.4.3` + `13.2.8.2` (loopback lane, `DETECTOR_ENABLED=false` arm).
- [ ] Mock C is admitted only within `gate_enter` and dropped only beyond `gate_exit` or after `miss_limit` — no add/remove flicker — closed by `13.2.4.3` boundary cases at 30 m / 35 m plus `TRACK_TIMEOUT_MS` expiry. **`miss_limit` is realized as wall-clock, flagged not absorbed** — § Open items item 1.
- [ ] Gate constants are read from configuration — no literals — closed by `13.2.2.1` (the node's only env reader).
- [ ] CRA database schema committed; video-input proposal sent to FPT-Mentor — closed by `14.2.5.2` (schema) + `12.2.9.2` (the send).
- [ ] **Demo:** build + CI round-trip tests green on the frozen contracts (golden vectors) — closed by `ada-core-build` staying green across every subtask plus `5.2.8.1` / `13.2.8.2`.

**Suggested branch (suggestion only — creation is the user's call):** `feat/phase2-ada-scaffold`, branched from `main`. One branch for the whole phase; implementation subtasks commit onto it. Docs-only subtasks and evidence records commit straight to `main`.

**This plan is written against `main`**, where `ADA_ECU/` carries nothing beyond the Phase 0 contract layer listed above. That is the only baseline; no other branch is one.

### Execution labels

Every subtask carries exactly one. The label is who performs the work, not who tracks it — the planner keeps the ID and the done-tracking in every case. The vocabulary is the `AI` / `Human` split of [deploy-ada-ecu-walkthrough.md §7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#7-work-division-between-ai-and-human).

| Label | Who does it |
|---|---|
| *AI* | A spawned implementation subagent. The default for code, tests and CI. A step touching the live platform is [[car-sky]] instead, and is still *AI*. |
| *AI — orchestrator* | The orchestrating session. It pushes the phase branch, watches the lane, and records the run id in the subtask's `**Status:**` line. It also makes the evidence-record commit for a *Human* subtask once the person confirms. |
| *Human* | A person, outside any tool an agent holds. No agent performs these. |

**Creating the phase branch is a Human step; pushing onto it afterwards is not.** The user creates the branch named in § Phase 2 overview. The orchestrator then pushes each subtask's commit and reads the lane result. No subtask's acceptance waits on a person for a push.

**Phase 2 is 25 *AI* subtasks and 1 *Human* subtask.** The single human row is `12.2.9.2`. No subtask here is performed by [[car-sky]] — nothing in this phase touches the platform.

**Implementation-subagent specification** (inherited by every *AI* subtask): general-purpose agent; tools Read/Grep/Glob/Write/Edit/Bash; writes ONLY inside `ADA_ECU/` (plus its own `**Status:**` line in this file and, where the subtask explicitly says so, `.github/workflows/` or `requirements/car-sky-guide/`); reads [ADA_ECU/doc/](../ADA_ECU/doc/) first; inherits § Subtask discipline as its definition of done; makes the atomic commit itself with the exact commit message from the brief; never pushes — that is the orchestrator's row above.

Language best practice is part of the definition of done:

- C++17 core guidelines, RAII ownership, no raw owning pointers.
- No socket header outside `src/net/`.
- Python type hints and dataclasses, no module-level mutable globals.
- Tests deterministic — no sleep used as synchronization.

### Subtask discipline (applies to every subtask below)

Per [task-planning-conventions.md § Subtask discipline](../.claude/rules/task-planning-conventions.md#subtask-discipline-non-negotiable): single objective, no out-of-scope code, exactly one atomic commit with the stated message, build passes, unit tests pass, brief self-contained. Hard execution constraints:

- **Dev host is Windows-on-ARM with no Docker/WSL.** C++ verification and image builds run on GitHub Actions — a C++ subtask's build/tests acceptance = **CI green on the pushed branch** (the Phase 0/1 model). Python verification follows § Per-node build commands: a `detector/` subtask verifies locally **and** on CI `python-tests`, a `tools/` subtask verifies locally, since no lane collects `ADA_ECU/tools/tests/`.
- **No hardcoded tunables** (CLAUDE.md principle 5): every constant in [HLD §6](../ADA_ECU/doc/ada-ecu-hld.md#6-internal-components) comes from `src/config/config.cpp` (core) or `detector/config.py` (detector, Phase 3). A literal outside those two files is a defect.
- **Sequential execution at run time:** all implementation subagents share one working tree, so subtasks execute one at a time in dependency order. The parallel/sequential marks below are the logical dependency structure.
- **Status tracking:** each subtask gains a `**Status:**` line (appended in that subtask's own atomic commit) recording done/blocked + verification evidence; no status line = not started. **Nothing in this file is started.**

### Per-node build commands (cited in acceptance below)

| Node / area | Build + test command | Verified |
|---|---|---|
| `ADA_ECU/` (C++ core) | `cmake -S ADA_ECU -B ADA_ECU/build && cmake --build ADA_ECU/build -j $(nproc) && ctest --test-dir ADA_ECU/build --output-on-failure` | CI `ada-core-build` (phase0-ci.yml) |
| `ADA_ECU/detector/` (Python) | `pip install -r ADA_ECU/detector/requirements-dev.txt && python -m pytest ADA_ECU/detector/tests` | local **and** CI `python-tests` |
| `ADA_ECU/tools/` (Python) | `python -m py_compile ADA_ECU/tools/<script>.py` | local |
| `contracts/` gate | `python contracts/check_sync.py` → exit 0 | local + CI `contracts-gate` |
| ADA image | `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -t m1-ada-ecu:latest ADA_ECU/` | CI `ada-ecu-image` (`5.2.8.1`) |

**Image tags, stated separately.** The local build tag is `m1-ada-ecu:latest` ([HLD D9](../ADA_ECU/doc/ada-ecu-design-decisions.md#d9--deployment-shape)). The registry tag is `registry.hackathon-2.carsky.io/m1-ada-ecu:latest` ([node-ada-ecu.md § Blueprint node config](../requirements/car-sky-guide/node-ada-ecu.md), which is also the blueprint `image` value).

### CI ruling — extend or add?

**`ada-core-build` in [phase0-ci.yml](../.github/workflows/phase0-ci.yml) needs no edit.** It runs `cmake -S ADA_ECU -B ADA_ECU/build`, builds, and runs `ctest` over the whole project — every module library and test target this phase registers in `ADA_ECU/CMakeLists.txt` is covered the moment it is registered. The same holds for `python-tests`, which carries a guarded `ADA_ECU/detector` pytest step.

**`python-tests` collects two folders and no others:** `Scenario_Player/tests` and `ADA_ECU/detector/tests`, each guarded on its own `requirements-dev.txt`. `ADA_ECU/tools/tests/` is outside both guards, so a `tools/` test's acceptance is local — the `ADA_ECU/tools/` row of § Per-node build commands.

**Phase 2 creates two lanes in two files.** `ada-loopback-check` (the mock-driven admission run) goes in a new `.github/workflows/phase2-ci.yml`, per the convention stated in [phase1-ci.yml](../.github/workflows/phase1-ci.yml)'s header — *a lane belongs to the phase that created it, not to the phase that last touched it*. `ada-ecu-image` (arm64 build + gated push) goes in `.github/workflows/phase4-ci.yml`: the image built from `ADA_ECU/` carries the Phase 2 scaffold, the Phase 3 detector and the Phase 4 fusion, so its lane is filed with the node artifact rather than any single phase ([HLD §11](../ADA_ECU/doc/ada-ecu-hld.md#11-tech-stack-build-and-ci); [phase4_tasks.md § CI ruling](phase4_tasks.md) — whichever subtask lands first creates that file with the standard header, and the others add only their job). Phase 3 and Phase 4 create their own files for their own lanes.

---

## Task Group 2.1 — Node orientation (serves R5)

### [x] `5.2.1.3` — Point `ADA_ECU/README.md` at the HLD *(AI)*

**Objective:** the node README states what the folder is and links the design of record ([HLD §4](../ADA_ECU/doc/ada-ecu-hld.md#4-folder-structure), the `README.md` row).

**Scope:** `ADA_ECU/README.md` only — one-screen orientation: node identity (Container Node, R3/R12–R15), the two-process shape (D2), links to [ada-ecu-hld.md](../ADA_ECU/doc/ada-ecu-hld.md), [node-ada-ecu.md](../requirements/car-sky-guide/node-ada-ecu.md), and the § Per-node build commands rows. It also carries the clip attribution string [ADA_ECU/media/ego-b-occluding-c.source.md § Attribution](../ADA_ECU/media/ego-b-occluding-c.source.md) names, since the clip ships inside every pushed image. References, never restates ([markdown-writing-style](../.claude/skills/markdown-writing-style/SKILL.md) rule 4).

**Acceptance:** links resolve; no requirement text restated; the attribution line matches the sidecar character for character. Doc-only — no build target.

**Dependencies:** none. **Commit:** `[5.2.1.3] docs: point the ADA README at the phase 2-4 HLD`

**Status:** done — README rewritten as one-screen orientation; every link target verified present, both build commands byte-match § Per-node build commands, attribution line diffs identical against the sidecar.

---

## Task Group 2.2 — Core foundation: config, socket, event log, input queue (serves R13, R6, R18, R3)

> The four transport- and rule-blind modules every later group depends on. Paths from [HLD §4](../ADA_ECU/doc/ada-ecu-hld.md#4-folder-structure); build/test = the ADA C++ row of § Per-node build commands. Each new module registers a static library plus a test target in `ADA_ECU/CMakeLists.txt`, following the existing `ada_contracts` / `ada_add_test()` pattern.

### [x] `13.2.2.1` — Env config loader `src/config/config.{hpp,cpp}` *(AI)*

**Objective:** the node's **only** env reader ([HLD §6](../ADA_ECU/doc/ada-ecu-hld.md#6-internal-components), Data table, the `config/config` row): load + validate the core-consumed env set into an immutable `Config` struct.

**Scope:**

- Fields + defaults exactly per the **Env — core** table of [HLD §6](../ADA_ECU/doc/ada-ecu-hld.md#6-internal-components): `V2X_LISTEN_PORT` (47200) · `V2X_LISTEN_HOST` (`0.0.0.0`) · `IVI_ECU_HOST` (`10.99.0.13`) · `IVI_ECU_PORT` (47300) · `GATE_ENTER_M` (30) · `GATE_EXIT_M` (35) · `CONFIRM_HITS` (3) · `TRACK_TIMEOUT_MS` (1000) · `FUSION_TICK_MS` (100) · `DETECTOR_ENABLED` (true) · `DETECTOR_CMD` (`python3 /app/detector/main.py`) · `DETECTOR_RESTART_MAX` (5) · `CRA_ENABLED` (`nlos_obstruction`) · `RISK_NEAR_M` (60) · `RISK_CRITICAL_M` (30) · `RISK_TTC_WARN_S` (6) · `RISK_TTC_CRITICAL_S` (3) · `RISK_DWELL_MS` (300) · `STATE_RATE_HZ` (0) · `EVENT_LOG_PATH` (empty) · `ASSESS_LOG_EVERY_MS` (1000). The Phase 4 risk values are loaded now and unused until then — one loader, one table, no second env read later.
- **Not read here:** `VIDEO_CLIP_PATH`, `DETECTOR_FRAME_STRIDE`, `DETECTOR_LOOP`, `DETECTOR_REALTIME_PACING`, `DETECTOR_CLIP_FPS`, `DETECTOR_START_DELAY_S`, `MODEL_PATH`, `CONF_THRESHOLD`, `IOU_THRESHOLD`, `TRACK_IOU_MIN`, `VEHICLE_WIDTH_M`, `CAMERA_HFOV_DEG` — the **Env — detector** table of HLD §6, read by `detector/config.py` in Phase 3. Each key is read in exactly one place, which that section states as a rule. Also not read here: `CAPTURE_FILTER`, `PCAP_DIR` and `CAPTURE_ROTATE_S`, consumed by `capture.sh` directly in Phase 4.
- Validation: exactly the rules of the **Startup validation** table of [HLD §6](../ADA_ECU/doc/ada-ecu-hld.md#6-internal-components) and nothing beyond them — ports 1–65535; non-empty hosts; `CRA_ENABLED` naming registered plugins only; `0 < GATE_ENTER_M < GATE_EXIT_M`; `0 < RISK_CRITICAL_M < RISK_NEAR_M`; `0 < RISK_TTC_CRITICAL_S < RISK_TTC_WARN_S`; `CONFIRM_HITS ≥ 1`; positive `TRACK_TIMEOUT_MS`/`FUSION_TICK_MS`; `RISK_DWELL_MS ≥ 0`; non-negative `STATE_RATE_HZ`; `DETECTOR_RESTART_MAX ≥ 0`. Invalid value → descriptive exception naming the offending key, caller exits non-zero. Env read through an injectable getter so tests never mutate process env.
- **No assertion may relate a risk threshold to a gate threshold.** `RISK_NEAR_M`/`RISK_CRITICAL_M` are thresholds on the composed range `d_AC`; `GATE_ENTER_M`/`GATE_EXIT_M` are thresholds on one source's own range (`d_BC` relayed, `d_AB` estimated). Comparing them orders two different quantities, and the pair R22 requires — 60 / 30 — is above `GATE_ENTER_M` by design ([D5](../ADA_ECU/doc/ada-ecu-design-decisions.md#d5--risk-vocabulary-and-edge-triggered-emission), [D11](../ADA_ECU/doc/ada-ecu-design-decisions.md#d11--r22-run-choreography-the-run-origin-the-paced-clip-and-the-risk-band-pair)). A loader that rejects the pair exits the node at startup, and a node that has exited emits nothing.
- Test `tests/config/test_config.cpp`: defaults when unset; each override parsed; each rejection case, including the three same-quantity ordering rules — gate, risk range, risk TTC — and a positive case proving `RISK_NEAR_M = 60` with `GATE_ENTER_M = 30` loads without error.

**Acceptance:** ADA build + ctest green on CI (`ada-core-build`); no tunable literal anywhere outside this file's defaults table.

**Dependencies:** none — starts immediately. **Commit:** `[13.2.2.1] feat: add ADA ECU env config loader`

**Status:** done — loader + injectable-getter tests cover defaults, every override, all rejection rules incl. the three same-quantity orderings, and the 60/30 cross-quantity positive case; build/tests deferred to CI ada-core-build.

### [x] `6.2.2.2` — Sole socket holder `src/net/udp_socket.{hpp,cpp}` *(AI — parallel with 13.2.2.1)*

**Objective:** `net::UdpSocket` — the **only** `ADA_ECU/src` code allowed to include socket headers ([HLD §6](../ADA_ECU/doc/ada-ecu-hld.md#6-internal-components), controller table, the `net/udp_socket` row).

**Scope:** RAII fd ownership (move-only, no raw owning handles); `bind(host, port)`; blocking `recvFrom(buffer)` with a poll timeout so a stopping thread is not wedged; `sendTo(host, port, bytes)`; transient errors are **counted rather than thrown** and the count is returned to the caller, which logs it (HLD §6, the same row). POSIX headers confined to the `.cpp`; the header stays POSIX-free. Test `tests/net/test_udp_socket.cpp`: loopback send → receive round-trip on an ephemeral port; bind-conflict surfaces cleanly; receive timeout returns empty rather than blocking forever.

**Acceptance:** ADA build + ctest green on CI; `<sys/socket.h>`, `<netinet/*>`, `<arpa/*>` appear only under `src/net/`.

**Dependencies:** none. **Commit:** `[6.2.2.2] feat: add ADA UdpSocket sole transport holder`

**Status:** done — RAII move-only socket with poll-timeout receive and counted transient errors; loopback round-trip, bind-conflict, timeout and move tests on ephemeral ports; POSIX headers confined to the .cpp; build/tests deferred to CI ada-core-build.

### [x] `18.2.2.3` — R18 `[EVT]` JSONL event log `src/log/event_log.{hpp,cpp}` *(AI — parallel)*

**Objective:** the ADA half of the R18 evidence stream (HLD D8) — one JSONL line per event, `[EVT]`-prefixed, same line shape as the V2X ECU so one offline reader reconstructs both nodes.

**Scope:**

- Event vocabulary exactly [HLD D8](../ADA_ECU/doc/ada-ecu-design-decisions.md#d8--r18-the-ada-half-of-the-evidence-stream): `detector_spawn`, `detector_eof`, `detector_restart`, `own_sensor_ingest`, `r2_ingest`, `parse_reject`, `track_transition`, `track_expire`, `assessment`, `assess_skipped_b_unknown`, `risk_transition`, `r4_tx`. Phase 2 emits the first eight; the last four are declared here and first written in Phase 4.
- Line fields match the V2X ECU's frozen shape: `event`, `mono_ms`, `epoch_ms`, `counters`, plus a per-event `payload`. Payload-carrying events: `r2_ingest` embeds the received R2 body, `own_sensor_ingest` the parsed R3 object, `track_transition` the id/source/from/to/distance/reason, `r4_tx` the emitted R4 body.
- `mono_ms` is `CLOCK_MONOTONIC`, `epoch_ms` is `CLOCK_REALTIME` — [m1-run-timing-and-event-triggering.md §6.2](../requirements/m1-run-timing-and-event-triggering.md). Both are written on every line, never one derived from the other.
- Serialization through nlohmann, never string concatenation — a payload carrying quotes or newlines must escape correctly. Sink: stdout always, flushed per line (CarSky View Log is the live window); additionally append to `EVENT_LOG_PATH` when non-empty.
- Test `tests/log/test_event_log.cpp`: line parses as JSON after the `[EVT]` prefix; counters accumulate; embedded payloads present and parseable; a payload string containing quotes/newlines round-trips; file sink writes when the path is set (temp dir).

**Acceptance:** ADA build + ctest green on CI. The field names freeze here — `18.2.6.5`'s log checker parses them, and so do Phase 4's `event_report.py` and `check_evt_log.py --fusion`.

**Dependencies:** none. **Commit:** `[18.2.2.3] feat: add ADA R18 JSONL event log writer`

**Status:** done — nlohmann-only writer with both clock stamps, cumulative counters, injectable sink/clocks, stdout + optional EVENT_LOG_PATH file sink; all twelve D8 event names declared; tests cover parse-after-prefix, counter accumulation, quote/newline round-trip and the temp-dir file sink; build/tests deferred to CI ada-core-build.

### [x] `3.2.2.4` — Bounded input queue `src/observer/input_queue.hpp` *(AI — parallel)*

**Objective:** the D2 single-consumer queue — two producer threads (V2X rx, detector reader), one consumer (the main thread), so the store has exactly one writer.

**Scope:** header-only bounded queue of a small tagged struct `InputItem { Source source; std::string line; int64_t rxEpochMs; }` where `Source ∈ {V2xR2, DetectorR3}`; blocking `pop(timeout)`; `push` drops the **oldest** item and increments a dropped counter when full (a stalled consumer must never block a socket thread); `close()` wakes the consumer for clean shutdown. No parsing, no I/O, no logging inside. Test `tests/observer/test_input_queue.cpp`: FIFO order per producer; two concurrent producers deliver every item or count the drop; bounded capacity honoured; `pop` returns after `close()`.

**Acceptance:** ADA build + ctest green on CI; deterministic under repeat runs (no sleeps as synchronization).

**Dependencies:** none. **Commit:** `[3.2.2.4] feat: add bounded input queue for the two observers`

**Status:** done — header-only bounded queue (INTERFACE lib) with drop-oldest-and-count push, timeout pop, close()-wakes-consumer; tests cover per-producer FIFO, two concurrent producers (delivered + dropped accounting), capacity bound and pop-after-close, synchronized by join not sleeps; build/tests deferred to CI ada-core-build.

---

## Task Group 2.3 — Data parsers: wire → frozen R3 model (serves R2, R3)

> The Data Parser block of the [component map](../ADA_ECU/doc/phase2-4-ada-ecu-components.puml). Both parsers produce the **same** `contracts::TrackedObject` and both hand it to the **same** `upsert` — that identity is the R3 acceptance box. Field authority: the synced `ADA_ECU/contracts/r2-v2x-object.schema.json` and `r3-tracked-object.schema.json`; models: the Phase 0 bindings in `ADA_ECU/src/contracts/`.

### [x] `2.2.3.1` — R2 parser `src/parser/r2_parser.{hpp,cpp}` *(AI)*

**Objective:** map one received R2 JSON datagram to a `TrackedObject` with `source = v2x_relayed` ([HLD §6](../ADA_ECU/doc/ada-ecu-hld.md#6-internal-components), controller table, the `parser/r2_parser` row; [HLD §10.1](../ADA_ECU/doc/ada-ecu-hld.md#101-r2--the-message-set-from-the-v2x-ecu-consumed)).

**Scope:**

- Parse **through the frozen `contracts::R2Message` binding**, never by raw JSON probing.
- Mapping: `id = "v2x:" + stationId + ":" + object.objectId` · `class = object.classification` · `source = v2x_relayed` · `position = object.position{x,y}` · `distance = object.distance` · `speed = object.speed`.
- `confidence = object.confidence`, **or `0.0` when that field is null** — the obligation row in [HLD §10.1](../ADA_ECU/doc/ada-ecu-hld.md#101-r2--the-message-set-from-the-v2x-ecu-consumed). Frozen R2 types the field `["number","null"]` and frozen R3 requires `confidence` present in 0–1, so an unmapped null produces a schema-invalid track. The received value is carried out-of-band on the parse result, beside `position.confidence`.
- **Timestamps, per [HLD §10.2](../ADA_ECU/doc/ada-ecu-hld.md#102-r3--the-object-model-of-the-store-owned) and [m1-run-timing-and-event-triggering.md §6.2](../requirements/m1-run-timing-and-event-triggering.md):**
  - `timestamps.measured` = `rxTime + object.timeOfMeasurement`. Frozen `contracts/r2-v2x-object.schema.json` bounds `object.timeOfMeasurement` at −2048..2047 ms as an offset against the CPM reference time, so it is a delta and never an epoch. Both operands arrive in the same R2 message, so the sum stays inside one clock domain.
  - `timestamps.received` = the message's `rxTime` field, which is the V2X ECU's clock. A recorded value, never an operand of anything but the line above.
  - The ADA-side receive stamp does not enter the R3 object. It rides on `InputItem.rxEpochMs` for the `r2_ingest` event only.
  - `timestamps.lastUpdated` = **written by the store**, always ADA's own `CLOCK_REALTIME` at store write — never a foreign node's value.
  - **Arithmetic mixing two nodes' stamps is forbidden** (D10). No expression may combine a value that originated on another node with one that originated here.
- `state` is **not** set here — the store is the sole writer of `state` (D3). Emit `not_tracked` as the placeholder the store overwrites.
- `position.confidence` (metres, R2 field F6) has no R3 home: carry it out-of-band on the parse result so `18.2.2.3`'s `r2_ingest` payload can record it ([HLD §10.1](../ADA_ECU/doc/ada-ecu-hld.md#101-r2--the-message-set-from-the-v2x-ecu-consumed), the `position.confidence` obligation row).
- Failures return a typed reject reason (enum) for counting — no logging inside the class, no throw into the pipeline.
- Test `tests/parser/test_r2_parser.cpp`: the node-local sample `tests/fixtures/samples/r2-object.json` maps field-by-field to the expected `TrackedObject`; id convention exact; `measured` equals `1789000000123 + (−50)` = `1789000000073`, the sample's `rxTime` plus its `timeOfMeasurement`; `received` equals the sample's `rxTime`; a null `object.confidence` maps to `0.0` and the received null is still visible on the out-of-band result; one case per reject reason.

**Acceptance:** ADA build + ctest green on CI.

**Dependencies:** none (Phase 0 binding suffices). **Commit:** `[2.2.3.1] feat: add R2 to TrackedObject parser`

**Status:** done — parses through the frozen R2Message binding only; id `v2x:<stationId>:<objectId>`, null confidence → 0.0 with the wire null and position.confidence out-of-band on the result, measured = rxTime + timeOfMeasurement, lastUpdated = 0 placeholder; five typed reject reasons each counted and tested; build/tests deferred to CI ada-core-build.

### [x] `3.2.3.2` — R3 JSONL parser `src/parser/r3_parser.{hpp,cpp}` *(AI — parallel with 2.2.3.1)*

**Objective:** map one detector JSONL line to a `TrackedObject` with `source = own_sensor`.

**Scope:** parse through the frozen `contracts::TrackedObject` binding; **the incoming `state` field is ignored** (D3 — the store owns `state`); a line whose `source` is not `own_sensor` is rejected with a typed reason (a detector cannot mint relayed entries — the structural half of the zero-C argument, D6); malformed JSON and schema-invalid lines are rejected and counted, never fatal. Test `tests/parser/test_r3_parser.cpp`: `tests/fixtures/samples/r3-tracked-object.json` maps intact; an incoming `state: "tracked"` is discarded; a `source: "v2x_relayed"` line is rejected; malformed line rejected.

**Acceptance:** ADA build + ctest green on CI.

**Dependencies:** none. **Commit:** `[3.2.3.2] feat: add detector JSONL to TrackedObject parser`

**Status:** done — parses through the frozen TrackedObject binding only; incoming state discarded for the not_tracked placeholder (D3), non-own_sensor source rejected with a typed reason (D6), malformed/missing/wrong-type/out-of-range lines rejected and counted, never fatal; the frozen sample carries source v2x_relayed, so the maps-intact case flips it to own_sensor and the verbatim sample is the source-rejection case; build/tests deferred to CI ada-core-build.

### [x] `3.2.3.3` — Parse-reject corpus `tests/fixtures/malformed/` + counted-rejection test *(AI)*

**Objective:** prove both parsers reject a structurally invalid corpus with zero crashes and correct counters (HLD §4 — a local fixture, **not** a synced contract).

**Scope:** commit the corpus with per-case provenance in the test comments — R2 side: empty line, truncated JSON, wrong `type`, missing `object`, `object.distance` absent, `distance` non-numeric, unknown extra field (**must be tolerated**, R2 additive evolution); R3 side: empty line, truncated JSON, missing required R3 field, out-of-range `confidence`, `source: v2x_relayed`, unknown extra field (**tolerated**). Test `tests/parser/test_parse_reject_corpus.cpp` (planner-designated path, § Open items item 2): one parameterized suite asserting each case's expected disposition (`Reject` or `ToleratedAdditive`) with no either-outcome branch, plus a whole-corpus run proving the parsers still accept the valid sample afterwards.

**Acceptance:** ADA build + ctest green on CI; every case's disposition asserted explicitly.

**Dependencies:** after `2.2.3.1` + `3.2.3.2`. **Commit:** `[3.2.3.3] test: reject the malformed parse corpus with zero crashes`

**Status:** done — thirteen one-defect corpus files (`r2-<case>.json` / `r3-<case>.json`, empty-line cases as zero-byte files) with provenance in the test header; one parameterized suite asserts each case's exact disposition and reject reason with no either-outcome branch, both unknown-extra-field cases tolerated; the whole-corpus run proves one parser instance per side survives all cases with exact counters and still accepts the valid samples; build/tests deferred to CI ada-core-build.

---

## Task Group 2.4 — R3 store + R13 admission state machine (serves R3, R13)

> The "Current Input" block. The state machine realizes [phase2-4-ada-ecu-admission.puml](../ADA_ECU/doc/phase2-4-ada-ecu-admission.puml) exactly — **one machine, both sources**, parameterized only by what counts as an update (D3). `not_tracked` means **absent from the store**: a drop erases the entry, it does not leave one flagged.

### [x] `3.2.4.1` — Track store `src/store/track_store.{hpp,cpp}` *(AI)*

**Objective:** the R3 store — an `id → TrackedObject` map with `upsert / get / all / nearest / erase`, single-writer, exposing every R3 field.

**Scope:**

- Surface: `upsert(TrackedObject)` (the **identical** entry point for both parsers — the R3 acceptance box), `get(id)`, `all()`, `allBySource(Source)`, `nearest(Source)` (smallest `distance`), `erase(id)`. `allBySource` is a planner addition, § Open items item 2; the rest are [HLD §6](../ADA_ECU/doc/ada-ecu-hld.md#6-internal-components)'s `store/track_store` row. No admission logic here — that is `store/admission`, a separate component in the same section (D3) — and no I/O.
- The store owns `state`: `upsert` preserves the stored `state` and never takes it from the incoming object. It also stamps `timestamps.lastUpdated` from `CLOCK_REALTIME` at write, discarding whatever the parser left there.
- Test `tests/store/test_track_store.cpp`: an object round-trips with **all nine R3 fields** intact (`id`, `class`, `source`, `position`, `distance`, `speed`, `confidence`, `state`, `timestamps`); a detector-shaped and a relayed-shaped object both enter through the same `upsert` and are indistinguishable to the store except by `source`; `nearest` picks the smallest distance per source; `erase` removes.

**Acceptance:** ADA build + ctest green on CI — this test is the R3 acceptance box's unit-level closure.

**Dependencies:** none (uses the Phase 0 binding). **Commit:** `[3.2.4.1] feat: add the R3 track store`

**Status:** done — upsert/get/all/allBySource/nearest/erase with injectable CLOCK_REALTIME stamp; store owns `state` (not_tracked on first insert, preserved on refresh) and restamps `lastUpdated`; 7-case test covers the nine-field round-trip, identical-upsert for both source shapes, per-source nearest with tie case, erase, and the restamp; build/tests deferred to CI ada-core-build.

### [x] `13.2.4.2` — Admission state machine `src/store/admission.{hpp,cpp}` *(AI)*

**Objective:** the R13 machine as a **pure** function of (current state, hits, distance, elapsed) → (next state, action) — no I/O, no store access, no clock read inside.

**Scope:**

- Exactly the diagram's edges: `not_tracked → tentative` on `distance ≤ GATE_ENTER_M` (create, hits = 1) · `tentative → tentative` on `distance ≤ GATE_ENTER_M` (hits += 1) · `tentative → tracked` at `hits ≥ CONFIRM_HITS` · `tracked → tracked` on `distance ≤ GATE_EXIT_M` (refresh) · `tentative → not_tracked` on `distance > GATE_ENTER_M` **or** `now − lastUpdated > TRACK_TIMEOUT_MS` (erase, hits = 0) · `tracked → not_tracked` on `distance > GATE_EXIT_M` **or** timeout (erase, hits = 0).
- Hysteresis is one Schmitt band: `GATE_ENTER_M` admits and holds while `tentative`; only once `tracked` does the wider `GATE_EXIT_M` hold. The hits counter resets to 0 on every entry to `not_tracked`.
- Thresholds are constructor parameters (from `Config`), never literals; `now` is a parameter, never `steady_clock::now()` inside.
- Test `tests/store/test_admission_own_sensor.cpp`: the full own-sensor lifecycle in both directions; promotion exactly at `CONFIRM_HITS`, not before; counter reset proven by re-entering `tentative` after a drop and needing the full N again; timeout expiry from both `tentative` and `tracked`.

**Acceptance:** ADA build + ctest green on CI; every edge in the `.puml` has at least one covering case.

**Dependencies:** after `13.2.2.1` (threshold types). **Commit:** `[13.2.4.2] feat: implement the R13 admission state machine`

**Status:** done — pure step() machine realizing all eight diagram edges, thresholds as ctor params from Config, timeout evaluated first, hits reset on every erase; edge names aligned to the 13.2.4.3 reason vocabulary (self-loops distinct as counted/refreshed); test covers lifecycle both directions, promotion boundary, counter reset, timeout precedence, Schmitt band, CONFIRM_HITS==1 degenerate; build/tests deferred to CI ada-core-build.

### [ ] `13.2.4.3` — Store ↔ admission integration, expiry, transition events *(AI)*

**Objective:** wire the machine into the store — every ingest and every tick runs admission, every edge writes one `track_transition`, every expiry writes `track_expire`.

**Scope:**

- `TrackStore::apply(update)` runs admission on ingest and `TrackStore::expire(now)` runs the timeout edge for **every** track on the fusion tick, so a track can expire on silence alone (D2). Erased tracks are removed from the map, not flagged.
- **Expiry is an interval, so it runs on `CLOCK_MONOTONIC`** (`steady_clock`) — [m1-run-timing-and-event-triggering.md §6.2](../requirements/m1-run-timing-and-event-triggering.md). Keep a parallel monotonic stamp per track and compare `now − thatStamp > TRACK_TIMEOUT_MS` against it. `timestamps.lastUpdated` stays `CLOCK_REALTIME` — it is a log/wire value, not an interval operand. Reason: a host NTP step on the shared wall clock would otherwise expire every track at once mid-demo. `13.2.4.2` is unaffected — `now` is a parameter there; what this subtask fixes is which clock the caller reads it from.
- Each edge emits one `track_transition` event (id, source, from, to, distance, reason ∈ `gate_enter|confirmed|gate_exit|out_of_gate|timeout`) through the injected `EventLog&`; expiry additionally emits `track_expire`.
- Test `tests/store/test_admission_relayed.cpp`: relayed-C boundary cases at the exact gate values — 29.9 / 30.0 / 30.1 m admitting, 34.9 / 35.0 / 35.1 m holding-vs-dropping once `tracked`; **no flicker**: an oscillating 30–34 m sequence yields exactly one admit transition and zero drops; expiry after `TRACK_TIMEOUT_MS` of silence; the emitted `track_transition` sequence matches the diagram edge for edge.
- Test `tests/store/test_expiry_monotonic.cpp`, the fourth file [HLD §4](../ADA_ECU/doc/ada-ecu-hld.md#4-folder-structure) designates under `tests/store/`: expiry fires on the monotonic stamp alone. One case steps `timestamps.lastUpdated` forward and backward with the monotonic stamp held, and asserts no track expires; a second advances the injected monotonic time past `TRACK_TIMEOUT_MS` with `lastUpdated` untouched, and asserts the track is erased. That pair is the failure D10 exists to prevent.

**Acceptance:** ADA build + ctest green on CI — closes the "admitted only within `gate_enter`, dropped only beyond `gate_exit` or after `miss_limit`, no flicker" box at unit level.

**Dependencies:** after `3.2.4.1` + `13.2.4.2` + `18.2.2.3`. **Commit:** `[13.2.4.3] feat: run admission and expiry inside the track store`

---

## Task Group 2.5 — R14 CRA abstraction, database schema, registry (serves R14)

> R14's acceptance is **the code plus the database schema** — the interface, the registry, and a committed schema the assessment reads and writes. Phase 2 lands all three empty of rules; Phase 4's `chained_collision` is the first plugin and the proof that adding one is *one new file plus one line* (D4).

### [x] `14.2.5.2` — CRA assessment-record schema `schema/cra-assessment-record.schema.json` *(AI — first in the group)*

**Objective:** the committed R14 database schema — one of the two artifacts R14's acceptance names, and a Phase 2 acceptance box on its own.

**Scope:**

- JSON Schema (draft 2020-12) at `ADA_ECU/schema/cra-assessment-record.schema.json` — **node-local**, deliberately *not* under `ADA_ECU/contracts/` (that folder holds only byte-synced copies of frozen cross-node contracts) and **not** added to `contracts/sync-manifest.json`.
- Record fields exactly the [HLD D4 table](../ADA_ECU/doc/ada-ecu-design-decisions.md#d4--r14-the-collision-risk-assessment-interface-registry-and-database): `trackId`, `warningType`, `riskState`, `riskStateEnteredMs`, `firstSeenMs`, `lastUpdatedMs`, `distanceM`, `previousDistanceM`, `closingRateMps`, `ttcS` (nullable), `lastSnapshot` (an R3 object, `$ref` to the synced `contracts/r3-tracked-object.schema.json`), `lastKnownB` (nullable position), `emittedCount`, `rationale`. Keyed by `trackId` + `warningType`, which is what D4's field table calls the key; D4's accessor line words it differently, and § Open items item 5 carries the reconciliation.
- One committed sample record at `tests/fixtures/cra-assessment-record.json` that validates against it. It sits **outside** `tests/fixtures/samples/`, which [HLD §4](../ADA_ECU/doc/ada-ecu-hld.md#4-folder-structure) reserves for byte-synced contract samples; this record is node-local (D1). Path per § Open items item 2.

**Acceptance:** schema is valid JSON Schema and the sample validates (checked by `14.2.5.3`'s test); `contracts/check_sync.py` still exits 0.

**Dependencies:** none. **Commit:** `[14.2.5.2] feat: commit the CRA assessment-record schema`

**Status:** done — draft 2020-12 schema with all fourteen D4 fields required, `ttcS`/`lastKnownB` nullable, `lastSnapshot` `$ref`ing the synced R3 schema by relative path; sample validates (jsonschema Draft202012Validator, zero errors); `contracts/check_sync.py` exit 0 (47 copies byte-identical).

### [ ] `14.2.5.1` — Freeze the CRA interface `src/cra/i_collision_risk_assessment.hpp` *(AI)*

**Objective:** the R14 seam, header-only and frozen — the text is in [HLD D4](../ADA_ECU/doc/ada-ecu-design-decisions.md#d4--r14-the-collision-risk-assessment-interface-registry-and-database) and is transcribed, not redesigned.

**Scope:** `struct RiskContext { const TrackStore& store; AssessmentDb& db; std::int64_t now_ms; }` · `struct RiskFinding { std::string warningType; std::string riskState; std::optional<contracts::TrackedObject> trigger; std::string rationale; }` · `class ICollisionRiskAssessment` with `virtual std::string name() const = 0` and `virtual RiskFinding assess(RiskContext&) = 0`. **The plugin never emits** — it returns a finding; the output stage decides transport.

**`AssessmentDb` is forward-declared here, not included** — that is what lets this header land before `14.2.5.3` and removes the circular dependency; the definition arrives with the accessor. The forward declaration carries no accessor signature, so § Open items item 5's reconciliation lands in `14.2.5.3` alone and does not reopen this header. Test `tests/cra/test_cra_interface.cpp` (planner-designated path, § Open items item 2): a minimal fake plugin implements the interface, is called through a base pointer, and returns a finding — proves implementability and freezes the signatures.

**Acceptance:** ADA build + ctest green on CI; interface text stable — later subtasks may not alter it without re-freezing.

**Dependencies:** after `3.2.4.1` (references `TrackStore`). **Commit:** `[14.2.5.1] feat: freeze the collision risk assessment interface`

### [ ] `14.2.5.3` — Assessment database accessor `src/cra/assessment_db.{hpp,cpp}` *(AI)*

**Objective:** the typed in-process accessor over the D4 schema — the seam a future milestone swaps for real persistence.

**Scope:** `struct AssessmentRecord` mirroring `14.2.5.2`'s fields; `get(trackId, warningType) -> optional<Record>`, `upsert(Record)`, `erase(trackId)`, `all()`; every write also appended to the `[EVT]` stream as an `assessment` line so the table is reconstructible offline (D4). The composite `get` key follows D4's field table; § Open items item 5 carries the divergence from D4's accessor line, and a resolution there changes this signature and nothing else. **No database engine** — the rationale is recorded in HLD D4; this subtask does not revisit it. Test `tests/cra/test_assessment_db.cpp`: a record round-trips through `upsert`/`get`; the serialized record **validates against `schema/cra-assessment-record.schema.json`** (loaded from disk, not restated in the test); `erase` removes; the sample at `tests/fixtures/cra-assessment-record.json` deserializes.

**Acceptance:** ADA build + ctest green on CI; the schema is loaded and enforced by the test, not duplicated.

**Dependencies:** after `14.2.5.2` + `14.2.5.1` + `18.2.2.3`. **Commit:** `[14.2.5.3] feat: add the CRA assessment database accessor`

### [ ] `14.2.5.4` — Plugin registry `src/cra/registry.{hpp,cpp}` + `src/cra/builtin_plugins.cpp` *(AI)*

**Objective:** explicit registration and lookup by `warningType`, plus the one file an added plugin edits.

**Scope:**

- `registry.add(std::unique_ptr<ICollisionRiskAssessment>)`, `get(name)`, `enabled(const std::vector<std::string>&)` selecting the subset named by `CRA_ENABLED`; an unknown name in `CRA_ENABLED` is a startup error naming it.
- `builtin_plugins.cpp` holds `registerBuiltinPlugins(Registry&, const Config&)` — **empty in Phase 2**, one `registry.add(std::make_unique<ChainedCollision>(cfg))` line in Phase 4. **Registration is explicit, not static-init** (D4: a static library's linker discards unreferenced registrar objects).
- Test `tests/cra/test_registry.cpp`: two fake plugins register and are retrievable by name; duplicate name rejected; `CRA_ENABLED` selects a subset; an unknown name errors.

**Acceptance:** ADA build + ctest green on CI.

**Dependencies:** after `14.2.5.1` + `13.2.2.1`. **Commit:** `[14.2.5.4] feat: add the CRA plugin registry and builtin registration point`

---

## Task Group 2.6 — Observers, composition root, mock drive (serves R2, R12, R13, R18)

> The input edges and the controller. **No mock branch exists inside `src/`** (D2): Phase 2's mock own-sensor input is the *real* detector-reader pointed at a JSONL fixture via `DETECTOR_CMD`, and mock R2 traffic is a real datagram on the real socket. "Toggling the mock off" is `DETECTOR_ENABLED=false` plus no bench traffic.

### [ ] `2.2.6.1` — V2X listener `src/observer/v2x_listener.{hpp,cpp}` *(AI)*

**Objective:** the R2 ingress thread — bind `V2X_LISTEN_HOST:V2X_LISTEN_PORT`, receive datagrams, push onto the input queue.

**Scope:** owns one `net::UdpSocket`; one RAII-managed thread joined on destruction (no detached threads); each datagram becomes one `InputItem{V2xR2, body, rxEpochMs}` where `rxEpochMs` is `CLOCK_REALTIME` at receive; receive errors are counted and logged, never fatal; `stop()` is prompt and idempotent. No parsing here. Test `tests/observer/test_v2x_listener.cpp` (planner-designated path): a loopback datagram on an ephemeral port arrives on the queue byte-identical; clean shutdown with no hang; a queue-full drop is counted.

**Acceptance:** ADA build + ctest green on CI; no socket headers outside `src/net/`.

**Dependencies:** after `6.2.2.2` + `3.2.2.4`. **Commit:** `[2.2.6.1] feat: add the R2 UDP listener thread`

### [ ] `12.2.6.2` — Detector reader `src/observer/detector_reader.{hpp,cpp}` *(AI — parallel with 2.2.6.1)*

**Objective:** spawn `DETECTOR_CMD` and read its stdout as R3 JSONL — the ego side of the D2 process contract (argv + exit codes + JSONL over stdout, no FFI, no RPC).

**Scope:**

- `fork`/`exec` the configured command with a stdout pipe; one reader thread doing `getline`; each line becomes one `InputItem{DetectorR3, line, rxEpochMs}`.
- Lifecycle, one rule per step (D2):
  1. A clean EOF on the child's stdout emits `detector_eof` and respawns `DETECTOR_CMD`.
  2. The clip-level loop is the detector's own, applied by `FileFrameSource` from `DETECTOR_LOOP` (D6); this class reads no detector env key and holds no loop flag.
  3. A non-zero exit emits `detector_restart(reason, attempt)` and respawns.
  4. Restarts after a non-zero exit are bounded by `DETECTOR_RESTART_MAX`; past the bound the class stops respawning and logs terminal failure.
  5. Each spawn emits `detector_spawn`.
  6. `DETECTOR_ENABLED=false` means no spawn at all, logged once.
- The committed clip is 10 s, and `DETECTOR_LOOP` defaults to true, so a run longer than the clip is served inside the detector rather than by a respawn.
- **Phase 2 drive:** `DETECTOR_CMD="cat /app/tests/fixtures/own_sensor_mock.jsonl"` — the real reader, a fixture producer. There is no fixture-mode branch in this class.
- Test `tests/observer/test_detector_reader.cpp` (planner-designated path): a `printf`/`cat`-style child's lines all arrive; a clean EOF respawns the child (bounded assertion, injectable sleep); non-zero exit restarts up to the max then stops; `DETECTOR_ENABLED=false` spawns nothing; child is reaped on destruction (no zombies).

**Acceptance:** ADA build + ctest green on CI; no orphaned child process after the suite.

**Dependencies:** after `13.2.2.1` + `3.2.2.4` + `18.2.2.3`. **Commit:** `[12.2.6.2] feat: add the detector subprocess reader`

### [ ] `3.2.6.3` — Mock drive equipment: `tests/fixtures/own_sensor_mock.jsonl` + `tools/mock_v2x_sender.py` *(AI)*

**Objective:** the two Phase 2 stimulus sources — both outside `src/`.

**Scope:**

- `ADA_ECU/tests/fixtures/own_sensor_mock.jsonl`: a hand-written R3 JSONL stream for one own-sensor track `own:1` at ~5 Hz timestamps, distance walking 40 → 8 m and back out past 35 m, every line validating against the synced `contracts/r3-tracked-object.schema.json`, `source: own_sensor`, `state: not_tracked`. Content chosen to traverse the full R13 lifecycle in both directions.
- `ADA_ECU/tools/mock_v2x_sender.py`: Python 3 stdlib only; sends R2 JSON datagrams to a target `host:port` from CLI args/env (**no hardcoded peer**); a `--profile approaching|out-of-range` selecting a distance ramp 70.0 → 20.5 m at 5.0 m/s or a static 60.0 m, mirroring the bench's two committed scenarios (`Scenario_Player/scenarios/default.yaml`, `c-out-of-range.yaml`) — the approach values are the R22 demo geometry those files carry ([SP D7](../Scenario_Player/doc/scenario-player-design-decisions.md#d7--the-demo-cycle-is-one-clip-length-and-its-geometry-is-solved-backwards-from-the-first-warning)), read from the YAML rather than re-derived; configurable rate and count; one stdout line per datagram for correlation. Bodies validate against the synced `contracts/r2-v2x-object.schema.json`.
- Test equipment only — never enters the image (`.dockerignore`, `5.2.7.1`).

**Acceptance:** `python -m py_compile ADA_ECU/tools/mock_v2x_sender.py` passes; the script's `--validate` self-check reports every fixture line valid against the synced R3 schema and every generated body valid against the synced R2 schema (both loaded from `ADA_ECU/contracts/`, never restated); a loopback self-check receives every datagram byte-identical — evidence in the Status line.

**Dependencies:** none. **Commit:** `[3.2.6.3] feat: add the Phase 2 mock own-sensor fixture and R2 sender`

### [ ] `13.2.6.4` — Composition root `src/main.cpp` + `ada_ecu` executable *(AI)*

**Objective:** assemble config → event log → registry → observers → queue → parsers → store, and run the fusion tick — controller only, no rules (HLD §8 MVC mapping).

**Scope:**

- Load `Config` (exit non-zero with a message on invalid env); construct `EventLog`, `TrackStore`, `AssessmentDb`, `Registry` + `registerBuiltinPlugins` (empty in Phase 2), `V2xListener`, `DetectorReader`.
- Main loop: `pop` from the queue → route by source to `r2_parser` / `r3_parser` → `parse_reject` on failure (counted, never fatal) → `store.apply(update)` → `r2_ingest` / `own_sensor_ingest` event. Every `FUSION_TICK_MS`, call `store.expire(now)`; the CRA assessment call is added in Phase 4 (`15.4.2.3`).
- **The main thread is the single writer** of the store (D2). SIGTERM/SIGINT → stop observers, join threads, flush the log, exit 0. Documented exit codes.
- `add_executable(ada_ecu src/main.cpp …)` in `ADA_ECU/CMakeLists.txt`. No new unit-test file — acceptance is the full existing suite plus the link of the complete chain; run-level evidence is `13.2.8.2`.

**Acceptance:** ADA build + ctest green on CI; the `ada_ecu` target builds and links.

**Dependencies:** after `13.2.2.1` + `2.2.6.1` + `12.2.6.2` + `2.2.3.1` + `3.2.3.2` + `13.2.4.3` + `14.2.5.4`. **Commit:** `[13.2.6.4] feat: add the ada_ecu composition root`

### [ ] `18.2.6.5` — `[EVT]`-stream checker `tools/check_evt_log.py` *(AI)*

**Objective:** the scripted assertion that turns a saved `[EVT]` stream into a pass/fail — the ADA counterpart of `tools/comms_check/check_v2x_log.py`, reused on-platform and in CI.

**Scope:**

- Python 3 stdlib; input = file path or stdin; tolerates interleaved `[CAP]` and non-`[EVT]` lines (View Log exports carry both).
- **Phase 2 mode `--admission`:** every `r2_ingest` / `own_sensor_ingest` is followed by store state consistent with the diagram; the observed `track_transition` sequence per track id is a legal path through [the state machine](../ADA_ECU/doc/phase2-4-ada-ecu-admission.puml) (no `not_tracked → tracked` jump, no promotion before `CONFIRM_HITS` in-gate updates, no drop inside the hysteresis band); at least one full `not_tracked → tentative → tracked → not_tracked` cycle observed per named source; non-zero exit naming the first illegal edge.
- **Mode `--expect-no-tracks`:** exit non-zero if any `track_transition` appears — the "mock off yields no tracks" arm.
- **An empty input is never a pass** — zero `[EVT]` lines exits non-zero. Phase 4 extends this script with the risk/emission chain (`18.4.3.3`); the modes are additive.

**Acceptance:** `python -m py_compile` passes; demonstrated exit 0 on a synthetic conforming log and non-zero on each of: an illegal edge, an early promotion, a mid-band drop, and a log with zero `[EVT]` lines — evidence in the Status line.

**Dependencies:** after `18.2.2.3` (field names freeze there). **Commit:** `[18.2.6.5] feat: add the ADA EVT-stream admission checker`

---

## Task Group 2.7 — Image and entrypoint (serves R5; HLD D9)

### [ ] `5.2.7.1` — `ADA_ECU/Dockerfile` + `entrypoint.sh` + `.dockerignore` *(AI)*

**Objective:** the deployable `m1-ada-ecu:latest` image — two stages, **one base**, single-platform `linux/arm64` (D9).

**Scope:**

- Both stages on `python:3.11-slim`, per [HLD D9](../ADA_ECU/doc/ada-ecu-design-decisions.md#d9--deployment-shape) — it is the report's Python 3.11 for the detector *and* the C++ build base, so the binary links against the glibc and libstdc++ it runs on by construction. No Vanetza on this node, so apt's cmake (3.22+) suffices.
- The two stages, step by step:
  1. Build stage: apt-install cmake, g++ and git.
  2. Build stage: configure and build the `ada_ecu` target only, with the test targets excluded from the image build.
  3. Runtime stage: apt-install `tcpdump` and `coreutils` (for base64), which the Phase 4 capture needs.
  4. Runtime stage: copy in `/app/ada_ecu` and `/app/entrypoint.sh`, and set workdir `/app`.
  5. Write no `COPY` line for `media/`, `models/` or `detector/`.
  6. Keep `tools/`, `tests/`, `doc/` and `schema/` out of the runtime stage.

  Step 5 is a layer-ordering constraint, not an omission: `media/` is Phase 3 `12.3.7.2`'s single line, and `models/` plus `detector/` are `5.3.6.1`'s, in that order, so the rarely-changing blobs sit above the code layer.
- `entrypoint.sh`: `[ -x ./capture.sh ] && ./capture.sh &` then `exec ./ada_ecu` — the guard exists because `capture.sh` lands in Phase 4 (`6.4.4.1`); the blueprint `command` is `["./entrypoint.sh"]` from now on, so the node config does not change again when capture arrives.
- `.dockerignore` keeps `doc/`, `tests/`, `tools/`, `schema/`, `build/`, `detector/requirements-dev.txt` and `media/source/` out of the build context — and **must not exclude `media/` itself**.
- No `ENV` lines shadowing [HLD §6](../ADA_ECU/doc/ada-ecu-hld.md#6-internal-components) defaults — the blueprint injects env.

**Acceptance:** `sh -n` and `bash -n` clean on `entrypoint.sh`, LF line endings, exec bit set; CI `ada-ecu-image` lane (`5.2.8.1`) green — `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -t m1-ada-ecu:latest ADA_ECU/` succeeds. `--platform` and the disabled attestations are a standing requirement: a Container Node rejects a multi-platform manifest index and hangs in Provisioning ([phase0-smoke-test-run.md](doc/phase0-smoke-test-run.md)).

**Dependencies:** after `13.2.6.4` + `5.2.8.1` (the lane must exist to verify). **Commit:** `[5.2.7.1] feat: add the ADA ECU Dockerfile and entrypoint`

---

## Task Group 2.8 — CI lanes (serves R5, R13; workflow-file edits)

> Two files, per § CI ruling: `5.2.8.1` writes `.github/workflows/phase4-ci.yml` (the node-artifact lane), `13.2.8.2` creates `.github/workflows/phase2-ci.yml`. Both lanes land **before** their consumers, guarded on file existence like the Phase 0/1 jobs, so consuming subtasks have CI acceptance from day one. `.github/workflows/` is explicitly in these two subtasks' write scope and no other's in this phase.

### [x] `5.2.8.1` — Lane `ada-ecu-image` in `phase4-ci.yml` *(AI)*

**Objective:** arm64 image build and gated push for the ADA node image; `v2x-ecu-image` in [phase1-ci.yml](../.github/workflows/phase1-ci.yml) is the template. This lane builds `m1-ada-ecu:latest`, the image Phase 4's isolated Room deploys. No other route builds it.

**Scope — create `.github/workflows/phase4-ci.yml` (if absent) and one job in it:**

1. Create the file with the same `on:` and `concurrency:` blocks as phase1-ci.yml.
2. Add a header comment naming what the file carries and why, following the Phase 1 file's convention.
3. Add one job `ada-ecu-image`, mirroring `v2x-ecu-image` in shape.
4. Set up qemu and buildx in that job.
5. Build with `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -t registry.hackathon-2.carsky.io/m1-ada-ecu:latest ADA_ECU/` — the registry tag of § Per-node build commands, whose local form is `m1-ada-ecu:latest`.
6. Push only when `CARSKY_ZOT_API_KEY` exists, using the same login step and notice-and-exit-0 guard.
7. Give the job its own buildx `type=gha` cache scope.
8. Verify the pushed artifact through the existing `.github/actions/verify-arm64-image` composite.
9. Guard the job to skip with a notice while `ADA_ECU/Dockerfile` is absent.
10. Set `timeout-minutes: 360`, matching the existing arm64 image lanes in phase1-ci.yml.

**D9's in-image detector check is not this subtask's.** [D9](../ADA_ECU/doc/ada-ecu-design-decisions.md#d9--deployment-shape) ends the image lane by starting `detector/main.py --synthetic` inside the pulled `linux/arm64` image and observing R3 JSONL on stdout. The detector first enters the image in Phase 3 `5.3.6.1`, and that step lands with it.

**Acceptance:** workflow YAML valid; run-blocks `bash -n` clean; lane green on the current tree (guard branch) — goes live when `5.2.7.1` lands.

**Dependencies:** none — lands immediately. **Commit:** `[5.2.8.1] chore: add the ADA ECU image build-push CI lane`

**Status:** done — `.github/workflows/phase4-ci.yml` created (job `ada-ecu-image`, guard branch active while `ADA_ECU/Dockerfile` is absent); YAML parses via `yaml.safe_load`, both bash `run:` blocks `bash -n` clean, LF endings (zero CR bytes).

### [ ] `13.2.8.2` — Lane `ada-loopback-check` *(AI)*

**Objective:** the repeatable form of the Phase 2 "mock-driven transitions observable in logs; mock off yields no tracks" box.

**Scope — create `.github/workflows/phase2-ci.yml` (same `on:`/`concurrency:`/header convention as phase1-ci.yml) with one job `ada-loopback-check`, its steps in this order:**

1. Build the `ada_ecu` target.
2. Run A: start `ada_ecu` with `DETECTOR_ENABLED=true`, `DETECTOR_CMD="cat ADA_ECU/tests/fixtures/own_sensor_mock.jsonl"` and `IVI_ECU_HOST=127.0.0.1`, capturing stdout to a file.
3. Run A: drive `python ADA_ECU/tools/mock_v2x_sender.py --profile approaching` at the node's listen port.
4. Run A: send SIGTERM and wait for the process to exit.
5. Run A: assert `python ADA_ECU/tools/check_evt_log.py --admission` exits 0 over the captured stdout, passing a minimum transition count so the check cannot pass vacuously.
6. Run B: start the same binary with `DETECTOR_ENABLED=false` and start no sender, capturing stdout.
7. Run B: assert `python ADA_ECU/tools/check_evt_log.py --expect-no-tracks` exits 0 over that capture.
8. Fail the job on any non-zero exit from steps 1–7.

**Acceptance:** lane green on the pushed branch; run A observes at least one complete `tentative → tracked → not_tracked` cycle per source.

**Dependencies:** after `13.2.6.4` + `3.2.6.3` + `18.2.6.5`. **Commit:** `[13.2.8.2] chore: add the ADA loopback admission CI lane`

---

## Task Group 2.9 — Video input: preflight, spec delivery, node guide (serves R12, R5)

> The clip is an input, not a deliverable of this group: `ADA_ECU/media/ego-b-occluding-c.mp4` with its provenance sidecar, committed by Phase 3 `12.3.7.1`. This group owns the machine-checkable preflight over it, the delivery of the spec proposal that closes a milestone box, and the node guide's env rows.

### [ ] `12.2.9.1` — Clip preflight `tools/check_clip_spec.py` *(AI)*

**Objective:** the [research note KPI 1](../ADA_ECU/doc/research_notes/video-source-for-r12.md#measurable-checks-kpis) made executable — reject a non-conforming clip naming the failing attribute, and pass on the committed one.

**Scope:**

- Python 3 at `ADA_ECU/tools/check_clip_spec.py`; reads a video path and checks the machine-checkable rows of the [§3 spec table](../ADA_ECU/doc/research_notes/video-source-for-r12.md#3-video-input-spec-to-build-phase-3-against): container MP4, codec H.264, resolution 1280×720, constant frame rate 20 fps, file ≤ 60 MB, no audio track — plus a decode pass proving OpenCV reads ≥ 99% of the declared frame count with zero errors (KPI 2).
- **Duration default is 10–120 s, an interim value.** The committed clip is 10.0 s / 200 frames. B is the lead vehicle only between t≈6 s and t≈16 s of the source. A run longer than the clip comes from looping (`DETECTOR_LOOP=true`) rather than from different footage — reasoning in [the sidecar § The remaining deviation](../ADA_ECU/media/ego-b-occluding-c.source.md). The research note's §3 duration row reads 60–120 s, and § Open items item 6 carries that reconciliation to [[project-researcher]].
- Every expected value comes from CLI flags/env with those defaults — **no literals**.
- Probe via `ffprobe` when present, falling back to OpenCV properties with a clear notice; exit 1 listing every failing attribute with actual-vs-expected; exit 0 with a one-line summary otherwise.
- **Out of scope — the content rows.** "B occludes the lane at 10–40 m in ≥ 90% of frames" and "no vehicle ahead of B in the ego lane" are human judgements recorded in the sidecar's § Content verdict; this script must not claim to verify them.
- Test `ADA_ECU/tools/tests/test_check_clip_spec.py` (planner-designated path, § Open items item 2): synthesize small conforming and non-conforming clips with OpenCV; assert exit codes and the named failing attribute.

**Acceptance:** `python -m py_compile` passes; the test passes **locally**, per the `ADA_ECU/tools/` row of § Per-node build commands; **and the script exits 0 on `ADA_ECU/media/ego-b-occluding-c.mp4`** with its summary recorded in the Status line. No CI lane covers this test — `python-tests` collects `Scenario_Player/tests` and `ADA_ECU/detector/tests` only, and installs neither the OpenCV this test needs to synthesize clips.

**Dependencies:** none. **Commit:** `[12.2.9.1] feat: add the R12 clip preflight checker`

### [ ] `12.2.9.2` — Send the video-input proposal to FPT-Mentor *(Human)*

**Objective:** the milestone acceptance clause "video-input proposal sent to FPT-Mentor".

**Scope — four steps, all performed by the person:**

1. Send [video-source-for-r12.md §3](../ADA_ECU/doc/research_notes/video-source-for-r12.md#3-video-input-spec-to-build-phase-3-against) to FPT-Mentor — the format / frame rate / data rate table with its `assume` markers, and the KPI list beneath it.
2. Ask for confirmation or correction of the proposed values.
3. State in the message that the committed clip is 10.0 s and that a longer run comes from looping, so any correction is against what exists.
4. Record the send in `plans/doc/phase2-ada-scaffold-run.md` — created by this subtask — with the date and any reply.

Nothing new is authored; the note is the artifact.

**This requests confirmation of a spec, not footage, and blocks nothing.** The clip is sourced, encoded, licence-cleared and committed ([sidecar](../ADA_ECU/media/ego-b-occluding-c.source.md)), so every Phase 3 subtask proceeds without waiting for a reply.

**Acceptance:** step 4's record exists in `plans/doc/phase2-ada-scaffold-run.md`, carrying the date and any reply. The evidence commit is the orchestrator's, made once the person confirms the send.

**Dependencies:** none — send it early; it has the longest external latency and no dependant. **Commit:** `[12.2.9.2] docs: record the video-input proposal sent to FPT-Mentor`

### [ ] `5.2.9.4` — Add the §6 env rows to `node-ada-ecu.md` *(AI — writes in `requirements/car-sky-guide/`)*

**Objective:** the guide's § Blueprint node config carries the full HLD §6 env set, so a human configures the node once from one table.

**Scope — [node-ada-ecu.md](../requirements/car-sky-guide/node-ada-ecu.md) § Blueprint node config only, additive:**

- Env rows added to the existing five: `V2X_LISTEN_HOST`, `CONFIRM_HITS`, `TRACK_TIMEOUT_MS`, `FUSION_TICK_MS`, `DETECTOR_ENABLED`, `DETECTOR_CMD`, `DETECTOR_LOOP`, `DETECTOR_RESTART_MAX`, `VIDEO_CLIP_PATH` (`/app/media/ego-b-occluding-c.mp4`), `DETECTOR_FRAME_STRIDE` (`4`), `DETECTOR_REALTIME_PACING` (`true`), `DETECTOR_CLIP_FPS` (`20.0`), `DETECTOR_START_DELAY_S` (`0.0`), `MODEL_PATH`, `CONF_THRESHOLD`, `IOU_THRESHOLD`, `TRACK_IOU_MIN`, `VEHICLE_WIDTH_M`, `CAMERA_HFOV_DEG`, `CRA_ENABLED`, `RISK_NEAR_M`, `RISK_CRITICAL_M`, `RISK_TTC_WARN_S`, `RISK_TTC_CRITICAL_S`, `RISK_DWELL_MS`, `STATE_RATE_HZ`, `EVENT_LOG_PATH`, `ASSESS_LOG_EVERY_MS`, `CAPTURE_FILTER`, `PCAP_DIR`, `CAPTURE_ROTATE_S`. Values = the [HLD §6](../ADA_ECU/doc/ada-ecu-hld.md#6-internal-components) defaults; the table there is the authority and is referenced, not duplicated in prose.
- **Change nothing else.** The guide carries `command: ["./entrypoint.sh"]`, `capabilities: ["NET_RAW"]` and the `registry.hackathon-2.carsky.io` host. Leave them unchanged. Pins unchanged: exactly one `ethernet` `OUTPUT` pin at `10.99.0.12`, **no `video` pin** (the clip is baked into the image; [research note §1](../ADA_ECU/doc/research_notes/video-source-for-r12.md#1-platform-finding--carsky-serves-no-camera-content)). No frozen contract moves.

**This subtask blocks nothing.** The env values a human types into the isolated Room come from `5.4.9.1`'s `blueprint-ada-isolated.json` and are diffed by `5.4.10.6`, not from this guide.

**Acceptance:** the JSON block is valid JSON; every env name matches `src/config/config.cpp` and `detector/config.py` character for character; links resolve. Doc-only — no build target.

**Dependencies:** after `13.2.2.1` (core env names freeze there) + Phase 3 `12.3.2.1` (detector env names). **Commit:** `[5.2.9.4] docs: add the phase 2-4 env rows to the ADA node guide`

---

## Execution order & parallelism

Dependencies are real (files, frozen interfaces, CI lanes) — not default assumptions. At run time everything executes sequentially in one working tree (§ Subtask discipline); the lanes below are the logical structure.

```
Human       12.2.9.2 (longest external latency)                 (no dependant - schedule first)
Docs        5.2.1.3                                             (anytime)
CI-first    5.2.8.1                                             (guarded lane - lands before 5.2.7.1)

foundation  13.2.2.1 ∥ 6.2.2.2 ∥ 18.2.2.3 ∥ 3.2.2.4
parsers     2.2.3.1 ∥ 3.2.3.2 ──► 3.2.3.3
store       3.2.4.1 ; 13.2.4.2 (after 13.2.2.1) ──► 13.2.4.3 (also needs 3.2.4.1 + 18.2.2.3)
CRA         14.2.5.2 ──► 14.2.5.1 (after 3.2.4.1) ──► 14.2.5.3 ──► 14.2.5.4
observers   2.2.6.1 (after 6.2.2.2 + 3.2.2.4) ∥ 12.2.6.2 (after 13.2.2.1 + 3.2.2.4 + 18.2.2.3)
equipment   3.2.6.3 ∥ 18.2.6.5 (after 18.2.2.3) ∥ 12.2.9.1      (anytime)
assembly    13.2.6.4 (after 13.2.2.1 + 2.2.6.1 + 12.2.6.2 + 2.2.3.1 + 3.2.3.2 + 13.2.4.3 + 14.2.5.4)
verify      13.2.8.2 (after 13.2.6.4 + 3.2.6.3 + 18.2.6.5)
image       5.2.7.1 (after 13.2.6.4 + 5.2.8.1)
guide       5.2.9.4 (after 13.2.2.1 + phase-3 12.3.2.1)
```

**Recommended runtime order (single tree):** 12.2.9.2 *(Human)* → 5.2.8.1 → 5.2.1.3 → 13.2.2.1 → 6.2.2.2 → 18.2.2.3 → 3.2.2.4 → 2.2.3.1 → 3.2.3.2 → 3.2.3.3 → 3.2.4.1 → 13.2.4.2 → 13.2.4.3 → 14.2.5.2 → 14.2.5.1 → 14.2.5.3 → 14.2.5.4 → 2.2.6.1 → 12.2.6.2 → 3.2.6.3 → 18.2.6.5 → 13.2.6.4 → 13.2.8.2 → 5.2.7.1 → 12.2.9.1 → 5.2.9.4.

**Phase 3 and Phase 4 relative to this phase.** Both run in parallel after Phase 2 ([milestone1.md §3](milestone1.md#3-development-plan--order-of-implementation)), and neither needs the other. Phase 4 needs groups 2.2–2.6 (store, admission, CRA seam, main loop) plus `5.2.7.1`; Phase 3 needs only the frozen `detector/contracts/tracked_object.py` from Phase 0 and the `DETECTOR_CMD` contract from `12.2.6.2`.

## Acceptance traceability

| Milestone Phase 2 box | Closed by |
|---|---|
| Store exposes all R3 fields; both entry shapes use the identical interface (R3) | `3.2.4.1` (nine-field round-trip + same-`upsert` case) · `2.2.3.1` · `3.2.3.2` |
| Mock-driven transitions observable and matching the diagram; mock off ⇒ no tracks | `13.2.4.2` · `13.2.4.3` · `18.2.2.3` (`track_transition`) · `18.2.6.5` · `13.2.8.2` (both arms) |
| C admitted only within `gate_enter`, dropped only beyond `gate_exit` or after `miss_limit`, no flicker | `13.2.4.3` boundary + oscillation + timeout cases |
| Gate constants from configuration, no literals | `13.2.2.1` (sole env reader) · `5.2.9.4` (blueprint injection) |
| CRA database schema committed | `14.2.5.2` (schema) · `14.2.5.3` (accessor + schema-enforcing test) |
| Video-input proposal sent to FPT-Mentor | `12.2.9.2` — the artifact is the committed research note §3 |
| **Demo:** build + CI round-trip tests green on the frozen contracts | `ada-core-build` green per subtask · `5.2.8.1` · `13.2.8.2` · Phase 0 `tests/contracts/` unchanged |
| *(phase task, no box)* R14 abstraction stood up | `14.2.5.1` · `14.2.5.4` — the plugin itself is Phase 4 (`14.4.1.2`) |
| *(phase task, no box)* ADA half of the R18 stream starts | `18.2.2.3` · emission in `13.2.4.3` / `13.2.6.4` |

**Every Phase 2 box closes off-platform.** No box here needs a Room, a registry or a person beyond `12.2.9.2`'s send.

## Open items & flags (no Phase 2 subtask may silently close them)

| # | Item | Owner / closes at |
|---|---|---|
| 1 | **`miss_limit` semantics — the user's re-ratification.** [milestone1.md §4](milestone1.md#track-admission-gate-r13) words M as "consecutive missed updates (proposed 5)". [HLD D3](../ADA_ECU/doc/ada-ecu-design-decisions.md#d3--r13-admission-one-state-machine-both-sources) realizes it as wall-clock `TRACK_TIMEOUT_MS = 1000 ms` and carries the argument for the change of form. Implementation proceeds on the wall-clock form. **Trigger:** the question goes to the user at the same point in the schedule as `12.2.9.2`, before the first subtask starts — the phase's two outbound actions leave together. An answer arriving after `13.2.4.3` is done is taken up as new work under a new ID | **user** |
| 2 | **Planner-designated paths and surfaces beyond the HLD's explicit lists**, named per the folder's own conventions: `tests/config/`, `tests/net/`, `tests/log/`, `tests/observer/test_input_queue.cpp`, `tests/observer/test_v2x_listener.cpp`, `tests/observer/test_detector_reader.cpp`, `tests/parser/test_parse_reject_corpus.cpp`, `tests/cra/test_cra_interface.cpp`, `tests/fixtures/cra-assessment-record.json`, `tools/check_evt_log.py`, `tools/check_clip_spec.py`, `tools/tests/test_check_clip_spec.py`, and the `TrackStore::allBySource(Source)` accessor. Required by subtask discipline and by the research note's KPI 1. Flagged as HLD-consistent additions, not new design | [[project-architecture]] (ack) |
| 3 | **`(proposal)` defaults proceed as proposed** — `CONFIRM_HITS=3`, `TRACK_TIMEOUT_MS=1000`, `FUSION_TICK_MS=100`, `DETECTOR_RESTART_MAX=5`, `DETECTOR_LOOP=true` ([HLD §6](../ADA_ECU/doc/ada-ecu-hld.md#6-internal-components)). Externalized either way, so a ratification change is a node-config edit, not a code change. **Trigger:** answered with item 1, in the same message | user |
| 4 | **The relayed `measured` timestamp — mapped, nothing outstanding.** `2.2.3.1` maps `timestamps.measured = rxTime + object.timeOfMeasurement`, per [HLD §10.2](../ADA_ECU/doc/ada-ecu-hld.md#102-r3--the-object-model-of-the-store-owned) and [m1-run-timing-and-event-triggering.md §6.2](../requirements/m1-run-timing-and-event-triggering.md). Frozen `contracts/r2-v2x-object.schema.json` bounds `object.timeOfMeasurement` as a −2048..2047 ms delta and `V2X_ECU/src/pipeline/r2_builder.cpp` emits it in that form, so the sum is the only reading the contract admits. No contract change and no HLD amendment is outstanding. The rest of §6.2 — realtime for stamps, monotonic for intervals including expiry, no arithmetic across two nodes' clocks — is implemented at `2.2.3.1`, `18.2.2.3`, `3.2.4.1` and `13.2.4.3` | none — recorded for traceability |
| 5 | **D4's accessor signature against its own field table.** [D4](../ADA_ECU/doc/ada-ecu-design-decisions.md#d4--r14-the-collision-risk-assessment-interface-registry-and-database) words the accessor `get(trackId)` and calls the table "keyed by track id"; its field table names `trackId, warningType` as the key. `14.2.5.2` and `14.2.5.3` build the composite form. **Trigger:** reconcile before Phase 4's first plugin compiles against the seam (`14.4.1.2`) | [[project-architecture]] |
| 6 | **Research-note §3's duration row reads 60–120 s.** The committed clip is 10.0 s and the run length comes from `DETECTOR_LOOP`, so `12.2.9.1` defaults to 10–120 s in the interim. **Trigger:** reconcile §3 before `12.2.9.2` sends it | [[project-researcher]] |
| 7 | **Which component honours `DETECTOR_LOOP`.** [D2](../ADA_ECU/doc/ada-ecu-design-decisions.md#d2--process-thread-and-mock-model) gates the core's EOF respawn on `DETECTOR_LOOP=true`, while [HLD §6](../ADA_ECU/doc/ada-ecu-hld.md#6-internal-components) places the key in the **Env — detector** table under the rule that each key is read in exactly one place, and D6 gives it to `FileFrameSource`. `12.2.6.2` follows §6: it respawns on any clean EOF and reads no detector key. **Trigger:** reconcile D2's wording with §6's placement before Phase 3 `12.3.2.1` writes `detector/config.py` | [[project-architecture]] |

---

*Phase 2 = 9 task groups, 26 subtasks — 25 *AI* (2 docs-only), 1 *Human*. Nothing started. Decomposed from [ada-ecu-hld.md](../ADA_ECU/doc/ada-ecu-hld.md), [video-source-for-r12.md](../ADA_ECU/doc/research_notes/video-source-for-r12.md) and [milestone1.md § Phase 2](milestone1.md#phase-2--ada-scaffolding-store--state-machine-no-detector-r3-r13).*
