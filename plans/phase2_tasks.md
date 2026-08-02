# Phase 2 — ADA Scaffolding (store + R13 admission + R14 abstraction, no detector): Full Task Breakdown

> **Authority & context:**
> - **Phase content:** [milestone1.md § Phase 2](milestone1.md#phase-2--ada-scaffolding-store--state-machine-no-detector-r3-r13) — its six acceptance checkboxes are the phase output.
> - **Design:** [phase2-4-ada-ecu-hld.md](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md) (commit `093f6d6`) — decisions D1–D9, §4 folder map, §6 env table, §10 acceptance traceability, §11 open items. Every `ADA_ECU/` path below is cited from its §4; diagrams [components](../ADA_ECU/doc/phase2-4-ada-ecu-components.puml) · [call flow](../ADA_ECU/doc/phase2-4-ada-ecu-callflow.puml) · [admission](../ADA_ECU/doc/phase2-4-ada-ecu-admission.puml).
> - **Video source:** [video-source-for-r12.md](../ADA_ECU/doc/research_notes/video-source-for-r12.md) (commit `e4d64e7`) — §3 the spec, §4 what the user must provide, §6 the decision for the planner.
> - **Requirements:** [m1-cooperative-awareness.md §2](../requirements/m1-cooperative-awareness.md) R2, R3, R5, R6, R12–R14, R18 — referenced by number, never restated.
> - **Phase 0 baseline (do not re-plan):** [phase0_tasks.md § Output](phase0_tasks.md) — `contracts/` frozen + `sync-manifest.json` + `check_sync.py`; `ADA_ECU/contracts/` synced schema copies; `ADA_ECU/src/contracts/{tracked_object,r2_message,r4_message}.{hpp,cpp}` bindings; `ADA_ECU/detector/contracts/tracked_object.py`; `ADA_ECU/tests/contracts/` round-trip tests; `ADA_ECU/CMakeLists.txt` with the `ada_add_test()` helper.
> - **Deploy guide:** [node-ada-ecu.md](../requirements/car-sky-guide/node-ada-ecu.md) — updated by `5.2.9.4` with the D9 `command`/`capabilities` change and the §6 env rows.
> - **Rules:** [task-planning-conventions.md](../.claude/rules/task-planning-conventions.md) (`X.Y.Z.W`; subtask discipline restated once in § Subtask discipline below); [node-code-layout.md](../.claude/rules/node-code-layout.md).
>
> **Task ID legend:** `X.2.Z.W` — X = requirement served · 2 = this phase · Z = task group · W = subtask position within the group. IDs are stable; never renumber.
>
> **Planning baseline:** this plan is written **from zero**. The branch's lowercase `ada-ecu/` implementation is ruled superseded by HLD D1 and is deleted, not extended — nothing in it counts as work already done.

## Phase 2 overview

**Objective.** Stand up the ADA ECU skeleton inside `ADA_ECU/`: the C++17 core (config, socket, event log, observers, parsers), the R3 track store, the R13 admission state machine, and the R14 Collision Risk Assessment abstraction with its committed database schema — all driven by mock input (a JSONL fixture through the real detector-reader, mock R2 traffic through the real socket), so the pipeline works before any ML. Also lands the deployable image, the ADA CI lanes, and the R12 video-input intake path that Phase 3 depends on.

**Input (must exist before start):**

- Phase 0 complete: the contract layer listed under § Authority above.
- [phase2-4-ada-ecu-hld.md](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md) and [video-source-for-r12.md](../ADA_ECU/doc/research_notes/video-source-for-r12.md) committed.
- Phase 1's frozen R2 producer shape — `V2X_ECU/src/forward/ada_forwarder` sends one compact-JSON `R2Message` datagram per perceived-object update to `ADA_ECU_HOST:47200`. Phase 2 consumes that shape from `tools/mock_v2x_sender.py`; no live Phase 1 node is required.
- **User go-ahead on `5.2.1.1`** (deleting `ada-ecu/`) before that subtask runs. Nothing else in the phase blocks on it.

**Output (phase acceptance = the six milestone boxes):**

- [ ] The store exposes all R3 fields; detector-shaped and relayed-shaped entries enter through the identical interface (R3) — closed by `3.2.4.1` + `2.2.3.1` + `3.2.3.2` (both parsers call the same `upsert`).
- [ ] Mock-driven state transitions are observable in logs and match the R13 diagram; toggling the mock off yields no tracks — closed by `13.2.4.2` + `13.2.4.3` + `13.2.8.2` (loopback lane, `DETECTOR_ENABLED=false` arm).
- [ ] Mock C is admitted only within `gate_enter` and dropped only beyond `gate_exit` or after `miss_limit` — no add/remove flicker — closed by `13.2.4.3` boundary cases at 30 m / 35 m plus `TRACK_TIMEOUT_MS` expiry. **`miss_limit` semantics change is flagged, not absorbed** — § Open items item 1.
- [ ] Gate constants are read from configuration — no literals — closed by `13.2.2.1` (the node's only env reader).
- [ ] CRA database schema committed; video-input proposal sent to FPT-Mentor — closed by `14.2.5.2` (schema) + `12.2.9.2` (USER-MANUAL delivery of the [research note §3 spec](../ADA_ECU/doc/research_notes/video-source-for-r12.md#3-video-input-spec-to-build-phase-3-against)).
- [ ] **Demo:** build + CI round-trip tests green on the frozen contracts (golden vectors) — closed by `ada-core-build` staying green across every subtask plus `5.2.8.1`/`13.2.8.2`.

**Suggested branch (suggestion only — creation is the user's call):** `feat/phase2-ada-scaffold` — the branch this plan is written on. One branch for the whole phase; implementation subtasks commit onto it. Docs-only subtasks (`13.2.10.1`, this plan file, evidence records) follow the repo convention of committing straight to `main`.

### Execution split legend

| Label | Meaning |
|---|---|
| *agent* | implemented by a spawned implementation subagent (default) |
| *car-sky* | planned for the [[car-sky]] agent (deploy preflight → build/push/deploy/verify); planner keeps the ID and done-tracking |
| *USER-MANUAL* | performed by the user (Nydus UI, external delivery); the plan tracks it, the evidence commit is made by the orchestrating session after the user confirms |
| *USER-GATED* | agent-executable, but only after the user records an explicit go-ahead in this file |

**Implementation-subagent specification** (inherited by every *agent* subtask): general-purpose agent; tools Read/Grep/Glob/Write/Edit/Bash; writes ONLY inside `ADA_ECU/` (plus its own `**Status:**` line in this file and, where the subtask explicitly says so, `.github/workflows/`, `requirements/car-sky-guide/`, or the repo-root path the subtask names); reads [ADA_ECU/doc/](../ADA_ECU/doc/) first; inherits § Subtask discipline as its definition of done; makes the atomic commit itself with the exact commit message from the brief; never pushes — the orchestrator pushes and watches CI. Language best practice is part of done: C++17 core guidelines / RAII / no raw owning pointers / no socket headers outside `src/net/`; Python type hints + dataclasses + no globals; tests deterministic.

### Subtask discipline (applies to every subtask below)

Per [task-planning-conventions.md § Subtask discipline](../.claude/rules/task-planning-conventions.md#subtask-discipline-non-negotiable): single objective, no out-of-scope code, exactly one atomic commit with the stated message, build passes, unit tests pass, brief self-contained. Hard execution constraints:

- **Dev host is Windows-on-ARM with no Docker/WSL.** C++ verification and image builds run on GitHub Actions — a C++ subtask's build/tests acceptance = **CI green on the pushed branch** (the Phase 0/1 model). Python subtasks verify locally with pytest **and** on CI.
- **No hardcoded tunables** (CLAUDE.md principle 5): every constant in [HLD §6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#6-configuration--no-hardcoded-tunables) comes from `src/config/config.cpp` (core) or `detector/config.py` (detector). A literal outside those two files is a defect.
- **Sequential execution at run time:** all implementation subagents share one working tree, so subtasks execute one at a time in dependency order. The parallel/sequential marks below are the logical dependency structure.
- **Status tracking:** each subtask gains a `**Status:**` line (appended in that subtask's own atomic commit) recording done/blocked + verification evidence; no status line = not started.

### Per-node build commands (cited in acceptance below)

| Node / area | Build + test command | Verified |
|---|---|---|
| `ADA_ECU/` (C++ core) | `cmake -S ADA_ECU -B ADA_ECU/build && cmake --build ADA_ECU/build -j $(nproc) && ctest --test-dir ADA_ECU/build --output-on-failure` | CI `ada-core-build` (phase0-ci.yml) |
| `ADA_ECU/detector/` (Python) | `pip install -r ADA_ECU/detector/requirements-dev.txt && python -m pytest ADA_ECU/detector/tests` | local **and** CI `python-tests` |
| `ADA_ECU/tools/` (Python) | `python -m py_compile ADA_ECU/tools/<script>.py` | local |
| `contracts/` gate | `python contracts/check_sync.py` → exit 0 | local + CI `contracts-gate` |
| ADA image | `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -t m1-ada-ecu:latest ADA_ECU/` | CI `ada-ecu-image` (5.2.8.1) |

### CI ruling — extend or add?

**`ada-core-build` in [phase0-ci.yml](../.github/workflows/phase0-ci.yml) needs no edit at all.** It runs `cmake -S ADA_ECU -B ADA_ECU/build`, builds, and runs `ctest` over the whole project — every module library and test target this phase registers in `ADA_ECU/CMakeLists.txt` is covered the moment it is registered. The same holds for `python-tests`, which already carries a guarded `ADA_ECU/detector` pytest step.

**New lanes go in a new `phase2-ci.yml`**, per the convention stated in [phase1-ci.yml](../.github/workflows/phase1-ci.yml)'s header — *a lane belongs to the phase that created it, not to the phase that last touched it*. Phase 2 creates two: `ada-ecu-image` (arm64 build + gated push) and `ada-loopback-check` (the mock-driven admission run). Phase 3 and Phase 4 create their own files for their own lanes.

---

## Task Group 2.1 — Consolidation and repo hygiene (serves R5; HLD D1, §11 items 2 and 7)

> One CarSky node has one build context. `ADA_ECU/` is canonical ([node-code-layout.md](../.claude/rules/node-code-layout.md)); the parallel lowercase folder and the deck describing it are retired before new code lands, so no implementer can read the superseded model by accident.

### [ ] `5.2.1.1` — Delete the superseded `ada-ecu/` folder *(agent — **USER-GATED**)*

**Objective:** remove the repo-root `ada-ecu/` tree in one commit, per HLD D1.

**Scope:**

- Delete `ada-ecu/` entirely (34 files, ~1,900 lines): `src/`, `include/`, `schemas/`, `config/`, `docs/`, `tests/`, `tools/`, `testdata/`, `CMakeLists.txt`, `Dockerfile`, `README.md`, `requirements.txt`.
- Nothing is moved. Salvage is by rewrite against the frozen bindings in later subtasks — the HLD D1 salvage table names which shape lands where; an implementer of a later subtask may read the deleted file from git history for shape, never copy it.
- No other file changes. No CMake reference exists to update (`ada-ecu/CMakeLists.txt` is standalone and no CI lane references it).

**Acceptance:** `ada-ecu/` absent from the tree; `python contracts/check_sync.py` exits 0 (the folder's forked `schemas/*.json` were never manifest targets); `ada-core-build` green — `ADA_ECU/` is untouched by this deletion.

**USER GATE — this subtask must not run until the user records a go-ahead here.** ~1,900 lines of a teammate's committed work are removed. The rationale is HLD D1: every file there depends on a second `TrackedObject`/`Source`/`TrackState` model in `ada-ecu/include/ada/types.hpp` that contradicts the frozen `ADA_ECU/src/contracts/tracked_object.hpp`, so no file can be merged without dragging the duplicate model in; the folder's `schemas/*.json` are forks of the frozen contracts (they break the R4 additive-version acceptance and require fields R3 does not have). **User go-ahead:** *(not yet given)*.

**Dependencies:** none — first, once gated. **Commit:** `[5.2.1.1] chore: remove the superseded ada-ecu folder`

### [ ] `5.2.1.2` — Retire `presentation/ada/ada-phase2-3-4-deck.*` *(agent — **USER-GATED**, docs, commits on `main`)*

**Objective:** close [HLD §11 item 7](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#11-open-items-and-flags) — the deck documents the design deleted by `5.2.1.1`.

**Scope:** delete `presentation/ada/ada-phase2-3-4-deck.md` and `presentation/ada/ada-phase2-3-4-deck.html`. No replacement deck is authored in this run; a Phase 2/3/4 task-planning deck, when requested, is produced under `presentation/phase<N>/` per [task-planning-presentation](../.claude/skills/task-planning-presentation/SKILL.md) from this file.

**Acceptance:** both files absent; no link elsewhere in the repo resolves to them (grep clean).

**Planner recommendation:** delete. The deck's content is the `ada-ecu/` design, so keeping it leaves two conflicting design narratives — the exact failure mode HLD D1 removes. **User go-ahead:** *(not yet given)*. Alternative if the user prefers: keep the file and prepend a one-line "superseded by [phase2-4-ada-ecu-hld.md]" banner — cheaper, but the stale slides stay reachable.

**Dependencies:** none — parallel with `5.2.1.1`. **Commit:** `[5.2.1.2] docs: retire the superseded ADA phase 2-4 deck`

### [ ] `5.2.1.3` — Point `ADA_ECU/README.md` at the HLD *(agent)*

**Objective:** the node README states what the folder is and links the design of record (HLD §4 marks it a P2 update).

**Scope:** `ADA_ECU/README.md` only — one-screen orientation: node identity (Container Node, R3/R12–R15), the two-process shape (D2), links to [phase2-4-ada-ecu-hld.md](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md), [node-ada-ecu.md](../requirements/car-sky-guide/node-ada-ecu.md), and the § Per-node build commands rows. References, never restates ([markdown-writing-style](../.claude/skills/markdown-writing-style/SKILL.md) rule 4).

**Acceptance:** links resolve; no requirement text restated. Doc-only — no build target.

**Dependencies:** none. **Commit:** `[5.2.1.3] docs: point the ADA README at the phase 2-4 HLD`

---

## Task Group 2.2 — Core foundation: config, socket, event log, input queue (serves R13, R6, R18, R3)

> The four transport- and rule-blind modules every later group depends on. Paths from [HLD §4](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#4-folder-structure-map--file-location-designations); build/test = the ADA C++ row of § Per-node build commands. Each new module registers a static library plus a test target in `ADA_ECU/CMakeLists.txt`, following the existing `ada_contracts` / `ada_add_test()` pattern.

### [ ] `13.2.2.1` — Env config loader `src/config/config.{hpp,cpp}` *(agent)*

**Objective:** the node's **only** env reader (HLD D1 salvage row, §6): load + validate the core-consumed env set into an immutable `Config` struct.

**Scope:**

- Fields + defaults exactly per [HLD §6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#6-configuration--no-hardcoded-tunables), core half: `V2X_LISTEN_PORT` (47200) · `V2X_LISTEN_HOST` (`0.0.0.0`) · `IVI_ECU_HOST` (`10.99.0.13`) · `IVI_ECU_PORT` (47300) · `GATE_ENTER_M` (30) · `GATE_EXIT_M` (35) · `CONFIRM_HITS` (3) · `TRACK_TIMEOUT_MS` (1000) · `FUSION_TICK_MS` (100) · `DETECTOR_ENABLED` (true) · `DETECTOR_CMD` (`python3 /app/detector/main.py`) · `DETECTOR_RESTART_MAX` (5) · `DETECTOR_LOOP` (true) · `CRA_ENABLED` (`nlos_obstruction`) · `RISK_NEAR_M` (25) · `RISK_CRITICAL_M` (15) · `RISK_TTC_WARN_S` (6) · `RISK_TTC_CRITICAL_S` (3) · `RISK_DWELL_MS` (300) · `STATE_RATE_HZ` (0) · `EVENT_LOG_PATH` (empty) · `ASSESS_LOG_EVERY_MS` (1000). The Phase 4 risk values are loaded now and unused until then — one loader, one table, no second env read later.
- **Not read here:** `VIDEO_CLIP_PATH`, `DETECTOR_FRAME_STRIDE`, `MODEL_PATH`, `CONF_THRESHOLD`, `IOU_THRESHOLD`, `TRACK_IOU_MIN`, `VEHICLE_WIDTH_M`, `CAMERA_HFOV_DEG`, `ZERO_C_RADIUS_M` (detector-side, `detector/config.py`, Phase 3) and `CAPTURE_FILTER`/`PCAP_DIR`/`CAPTURE_ROTATE_S` (consumed by `capture.sh` directly, Phase 4).
- Validation: ports 1–65535; non-empty hosts; `GATE_EXIT_M > GATE_ENTER_M`; `CONFIRM_HITS ≥ 1`; positive `TRACK_TIMEOUT_MS`/`FUSION_TICK_MS`; `RISK_CRITICAL_M < RISK_NEAR_M < GATE_ENTER_M` (D5 — the risk band must never alias the R13 gate); non-negative `STATE_RATE_HZ`. Invalid value → descriptive exception, caller exits non-zero. Env read through an injectable getter so tests never mutate process env.
- Test `tests/config/test_config.cpp`: defaults when unset; each override parsed; each rejection case, including the two ordering rules.

**Acceptance:** ADA build + ctest green on CI (`ada-core-build`); no tunable literal anywhere outside this file's defaults table.

**Dependencies:** none — starts immediately. **Commit:** `[13.2.2.1] feat: add ADA ECU env config loader`

### [ ] `6.2.2.2` — Sole socket holder `src/net/udp_socket.{hpp,cpp}` *(agent — parallel with 13.2.2.1)*

**Objective:** `net::UdpSocket` — the **only** `ADA_ECU/src` code allowed to include socket headers (HLD D1, mirroring the V2X ECU's ruling).

**Scope:** RAII fd ownership (move-only, no raw owning handles); `bind(host, port)`; blocking `recvFrom(buffer)` with a poll timeout so a stopping thread is not wedged; `sendTo(host, port, bytes)`; transient errors are **counted and returned, never thrown** (HLD D1 salvage row). POSIX headers confined to the `.cpp`; the header stays POSIX-free. Test `tests/net/test_udp_socket.cpp`: loopback send → receive round-trip on an ephemeral port; bind-conflict surfaces cleanly; receive timeout returns empty rather than blocking forever.

**Acceptance:** ADA build + ctest green on CI; `<sys/socket.h>`, `<netinet/*>`, `<arpa/*>` appear only under `src/net/`.

**Dependencies:** none. **Commit:** `[6.2.2.2] feat: add ADA UdpSocket sole transport holder`

### [ ] `18.2.2.3` — R18 `[EVT]` JSONL event log `src/log/event_log.{hpp,cpp}` *(agent — parallel)*

**Objective:** the ADA half of the R18 evidence stream (HLD D8) — one JSONL line per event, `[EVT]`-prefixed, same line shape as the V2X ECU so one offline reader reconstructs both nodes.

**Scope:**

- Event vocabulary exactly [HLD D8](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#d8--r18-the-ada-half-of-the-evidence-stream): `detector_spawn`, `detector_eof`, `detector_restart`, `own_sensor_ingest`, `r2_ingest`, `parse_reject`, `track_transition`, `track_expire`, `assessment`, `assess_skipped_b_unknown`, `risk_transition`, `r4_tx`. Phase 2 emits the first eight; the last four are declared here and first written in Phase 4.
- Line fields match the V2X ECU's frozen shape: `event`, `mono_ms`, `epoch_ms`, `counters`, plus a per-event `payload`. Payload-carrying events: `r2_ingest` embeds the received R2 body, `own_sensor_ingest` the parsed R3 object, `track_transition` the id/source/from/to/distance/reason, `r4_tx` the emitted R4 body.
- Serialization through nlohmann (never string concatenation — the superseded implementation could not escape strings). Sink: stdout always, flushed per line (CarSky View Log is the live window); additionally append to `EVENT_LOG_PATH` when non-empty.
- Test `tests/log/test_event_log.cpp`: line parses as JSON after the `[EVT]` prefix; counters accumulate; embedded payloads present and parseable; a payload string containing quotes/newlines round-trips; file sink writes when the path is set (temp dir).

**Acceptance:** ADA build + ctest green on CI. The field names freeze here — `13.2.6.5`'s log checker parses them.

**Dependencies:** none. **Commit:** `[18.2.2.3] feat: add ADA R18 JSONL event log writer`

### [ ] `3.2.2.4` — Bounded input queue `src/observer/input_queue.hpp` *(agent — parallel)*

**Objective:** the D2 single-consumer queue — two producer threads (V2X rx, detector reader), one consumer (the main thread), so the store has exactly one writer.

**Scope:** header-only bounded queue of a small tagged struct `InputItem { Source source; std::string line; int64_t rxEpochMs; }` where `Source ∈ {V2xR2, DetectorR3}`; blocking `pop(timeout)`; `push` drops the **oldest** item and increments a dropped counter when full (a stalled consumer must never block a socket thread); `close()` wakes the consumer for clean shutdown. No parsing, no I/O, no logging inside. Test `tests/observer/test_input_queue.cpp`: FIFO order per producer; two concurrent producers deliver every item or count the drop; bounded capacity honoured; `pop` returns after `close()`.

**Acceptance:** ADA build + ctest green on CI; deterministic under repeat runs (no sleeps as synchronization).

**Dependencies:** none. **Commit:** `[3.2.2.4] feat: add bounded input queue for the two observers`

---

## Task Group 2.3 — Data parsers: wire → frozen R3 model (serves R2, R3)

> The Data Parser block of the [component map](../ADA_ECU/doc/phase2-4-ada-ecu-components.puml). Both parsers produce the **same** `contracts::TrackedObject` and both hand it to the **same** `upsert` — that identity is the R3 acceptance box. Field authority: the synced `ADA_ECU/contracts/r2-v2x-object.schema.json` and `r3-tracked-object.schema.json`; models: the Phase 0 bindings in `ADA_ECU/src/contracts/`.

### [ ] `2.2.3.1` — R2 parser `src/parser/r2_parser.{hpp,cpp}` *(agent)*

**Objective:** map one received R2 JSON datagram to a `TrackedObject` with `source = v2x_relayed` (HLD D1 salvage row).

**Scope:**

- Parse **through the frozen `contracts::R2Message` binding**, never by raw JSON probing.
- Mapping: `id = "v2x:" + stationId + ":" + object.objectId` · `class = object.classification` · `source = v2x_relayed` · `position = object.position{x,y}` · `distance = object.distance` · `speed = object.speed` · `confidence = object.confidence` · `timestamps.measured = object.timeOfMeasurement` resolved to ms epoch · `timestamps.received = rxTime` · `timestamps.lastUpdated` = ingest time.
- `state` is **not** set here — the store is the sole writer of `state` (D3). Emit `not_tracked` as the placeholder the store overwrites.
- `position.confidence` (metres, R2 field F6) has no R3 home: carry it out-of-band on the parse result so `18.2.2.3`'s `r2_ingest` payload can record it (HLD D1 salvage row).
- Failures return a typed reject reason (enum) for counting — no logging inside the class, no throw into the pipeline.
- Test `tests/parser/test_r2_parser.cpp`: the node-local sample `tests/fixtures/samples/r2-object.json` maps field-by-field to the expected `TrackedObject`; id convention exact; one case per reject reason.

**Acceptance:** ADA build + ctest green on CI.

**Dependencies:** none (Phase 0 binding suffices). **Commit:** `[2.2.3.1] feat: add R2 to TrackedObject parser`

### [ ] `3.2.3.2` — R3 JSONL parser `src/parser/r3_parser.{hpp,cpp}` *(agent — parallel with 2.2.3.1)*

**Objective:** map one detector JSONL line to a `TrackedObject` with `source = own_sensor`.

**Scope:** parse through the frozen `contracts::TrackedObject` binding; **the incoming `state` field is ignored** (D3 — the store owns `state`); a line whose `source` is not `own_sensor` is rejected with a typed reason (a detector cannot mint relayed entries — the structural half of the zero-C argument, D6); malformed JSON and schema-invalid lines are rejected and counted, never fatal. Test `tests/parser/test_r3_parser.cpp`: `tests/fixtures/samples/r3-tracked-object.json` maps intact; an incoming `state: "tracked"` is discarded; a `source: "v2x_relayed"` line is rejected; malformed line rejected.

**Acceptance:** ADA build + ctest green on CI.

**Dependencies:** none. **Commit:** `[3.2.3.2] feat: add detector JSONL to TrackedObject parser`

### [ ] `3.2.3.3` — Parse-reject corpus `tests/fixtures/malformed/` + counted-rejection test *(agent)*

**Objective:** prove both parsers reject a structurally invalid corpus with zero crashes and correct counters (HLD §4 — a local fixture, **not** a synced contract).

**Scope:** commit the corpus with per-case provenance in the test comments — R2 side: empty line, truncated JSON, wrong `type`, missing `object`, `object.distance` absent, `distance` non-numeric, unknown extra field (**must be tolerated**, R2 additive evolution); R3 side: empty line, truncated JSON, missing required R3 field, out-of-range `confidence`, `source: v2x_relayed`, unknown extra field (**tolerated**). Test `tests/parser/test_parse_reject_corpus.cpp` (planner-designated path, § Open items item 4): one parameterized suite asserting each case's expected disposition (`Reject` or `ToleratedAdditive`) with no either-outcome branch, plus a whole-corpus run proving the parsers still accept the valid sample afterwards.

**Acceptance:** ADA build + ctest green on CI; every case's disposition asserted explicitly.

**Dependencies:** after `2.2.3.1` + `3.2.3.2`. **Commit:** `[3.2.3.3] test: reject the malformed parse corpus with zero crashes`

---

## Task Group 2.4 — R3 store + R13 admission state machine (serves R3, R13)

> The "Current Input" block. The state machine realizes [phase2-4-ada-ecu-admission.puml](../ADA_ECU/doc/phase2-4-ada-ecu-admission.puml) exactly — **one machine, both sources**, parameterized only by what counts as an update (D3). `not_tracked` means **absent from the store**: a drop erases the entry, it does not leave one flagged.

### [ ] `3.2.4.1` — Track store `src/store/track_store.{hpp,cpp}` *(agent)*

**Objective:** the R3 store — an `id → TrackedObject` map with `upsert / get / all / nearest / erase`, single-writer, exposing every R3 field.

**Scope:**

- Surface: `upsert(TrackedObject)` (the **identical** entry point for both parsers — the R3 acceptance box), `get(id)`, `all()`, `allBySource(Source)`, `nearest(Source)` (smallest `distance`), `erase(id)`. No admission logic here (that is `store/admission`, D1 salvage row) and no I/O.
- The store owns `state`: `upsert` preserves the stored `state` and never takes it from the incoming object.
- Test `tests/store/test_track_store.cpp`: an object round-trips with **all nine R3 fields** intact (`id`, `class`, `source`, `position`, `distance`, `speed`, `confidence`, `state`, `timestamps`); a detector-shaped and a relayed-shaped object both enter through the same `upsert` and are indistinguishable to the store except by `source`; `nearest` picks the smallest distance per source; `erase` removes.

**Acceptance:** ADA build + ctest green on CI — this test is the R3 acceptance box's unit-level closure.

**Dependencies:** none (uses the Phase 0 binding). **Commit:** `[3.2.4.1] feat: add the R3 track store`

### [ ] `13.2.4.2` — Admission state machine `src/store/admission.{hpp,cpp}` *(agent)*

**Objective:** the R13 machine as a **pure** function of (current state, hits, distance, elapsed) → (next state, action) — no I/O, no store access, no clock read inside.

**Scope:**

- Exactly the diagram's edges: `not_tracked → tentative` on `distance ≤ GATE_ENTER_M` (create, hits = 1) · `tentative → tentative` on `distance ≤ GATE_ENTER_M` (hits += 1) · `tentative → tracked` at `hits ≥ CONFIRM_HITS` · `tracked → tracked` on `distance ≤ GATE_EXIT_M` (refresh) · `tentative → not_tracked` on `distance > GATE_ENTER_M` **or** `now − lastUpdated > TRACK_TIMEOUT_MS` (erase, hits = 0) · `tracked → not_tracked` on `distance > GATE_EXIT_M` **or** timeout (erase, hits = 0).
- Hysteresis is one Schmitt band: `GATE_ENTER_M` admits and holds while `tentative`; only once `tracked` does the wider `GATE_EXIT_M` hold. The hits counter resets to 0 on every entry to `not_tracked`.
- Thresholds are constructor parameters (from `Config`), never literals; `now` is a parameter, never `steady_clock::now()` inside.
- Test `tests/store/test_admission_own_sensor.cpp`: the full own-sensor lifecycle in both directions; promotion exactly at `CONFIRM_HITS`, not before; counter reset proven by re-entering `tentative` after a drop and needing the full N again; timeout expiry from both `tentative` and `tracked`.

**Acceptance:** ADA build + ctest green on CI; every edge in the `.puml` has at least one covering case.

**Dependencies:** after `13.2.2.1` (threshold types). **Commit:** `[13.2.4.2] feat: implement the R13 admission state machine`

### [ ] `13.2.4.3` — Store ↔ admission integration, expiry, transition events *(agent)*

**Objective:** wire the machine into the store — every ingest and every tick runs admission, every edge writes one `track_transition`, every expiry writes `track_expire`.

**Scope:**

- `TrackStore::apply(update)` runs admission on ingest and `TrackStore::expire(now)` runs the timeout edge for **every** track on the fusion tick, so a track can expire on silence alone (D2). Erased tracks are removed from the map, not flagged.
- Each edge emits one `track_transition` event (id, source, from, to, distance, reason ∈ `gate_enter|confirmed|gate_exit|out_of_gate|timeout`) through the injected `EventLog&`; expiry additionally emits `track_expire`.
- Test `tests/store/test_admission_relayed.cpp`: relayed-C boundary cases at the exact gate values — 29.9 / 30.0 / 30.1 m admitting, 34.9 / 35.0 / 35.1 m holding-vs-dropping once `tracked`; **no flicker**: an oscillating 30–34 m sequence yields exactly one admit transition and zero drops; expiry after `TRACK_TIMEOUT_MS` of silence; the emitted `track_transition` sequence matches the diagram edge for edge.

**Acceptance:** ADA build + ctest green on CI — closes the "admitted only within `gate_enter`, dropped only beyond `gate_exit` or after `miss_limit`, no flicker" box at unit level.

**Dependencies:** after `3.2.4.1` + `13.2.4.2` + `18.2.2.3`. **Commit:** `[13.2.4.3] feat: run admission and expiry inside the track store`

---

## Task Group 2.5 — R14 CRA abstraction, database schema, registry (serves R14)

> R14's acceptance is **the code plus the database schema** — the interface, the registry, and a committed schema the assessment reads and writes. Phase 2 lands all three empty of rules; Phase 4's `chained_collision` is the first plugin and the proof that adding one is *one new file plus one line* (D4).

### [ ] `14.2.5.1` — Freeze the CRA interface `src/cra/i_collision_risk_assessment.hpp` *(agent)*

**Objective:** the R14 seam, header-only and frozen — the text is in [HLD D4](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#d4--r14-the-collision-risk-assessment-interface-registry-and-database) and is transcribed, not redesigned.

**Scope:** `struct RiskContext { const TrackStore& store; AssessmentDb& db; std::int64_t now_ms; }` · `struct RiskFinding { std::string warningType; std::string riskState; std::optional<contracts::TrackedObject> trigger; std::string rationale; }` · `class ICollisionRiskAssessment` with `virtual std::string name() const = 0` and `virtual RiskFinding assess(RiskContext&) = 0`. **The plugin never emits** — it returns a finding; the output stage decides transport. Test `tests/cra/test_cra_interface.cpp` (planner-designated path, § Open items item 4): a minimal fake plugin implements the interface, is called through a base pointer, and returns a finding — proves implementability and freezes the signatures.

**Acceptance:** ADA build + ctest green on CI; interface text stable — later subtasks may not alter it without re-freezing.

**Dependencies:** after `3.2.4.1` (references `TrackStore`) + `14.2.5.3` for the `AssessmentDb` type — land the DB accessor first, or forward-declare. Implementer's call; forward declaration is the cheaper order.

**Commit:** `[14.2.5.1] feat: freeze the collision risk assessment interface`

### [ ] `14.2.5.2` — CRA assessment-record schema `schema/cra-assessment-record.schema.json` *(agent)*

**Objective:** the committed R14 database schema — one of the two artifacts R14's acceptance names, and a Phase 2 acceptance box on its own.

**Scope:**

- JSON Schema (draft 2020-12) at `ADA_ECU/schema/cra-assessment-record.schema.json` — **node-local**, deliberately *not* under `ADA_ECU/contracts/` (that folder holds only byte-synced copies of frozen cross-node contracts) and **not** added to `contracts/sync-manifest.json`.
- Record fields exactly the [HLD D4 table](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#d4--r14-the-collision-risk-assessment-interface-registry-and-database): `trackId`, `warningType`, `riskState`, `riskStateEnteredMs`, `firstSeenMs`, `lastUpdatedMs`, `distanceM`, `previousDistanceM`, `closingRateMps`, `ttcS` (nullable), `lastSnapshot` (an R3 object, `$ref` to the synced `contracts/r3-tracked-object.schema.json`), `lastKnownB` (nullable position), `emittedCount`, `rationale`. Keyed by `trackId` + `warningType`.
- One committed sample record at `tests/fixtures/samples/cra-assessment-record.json` that validates against it.

**Acceptance:** schema is valid JSON Schema and the sample validates (checked by `14.2.5.3`'s test); `contracts/check_sync.py` still exits 0 (the file is correctly absent from the manifest).

**Dependencies:** none. **Commit:** `[14.2.5.2] feat: commit the CRA assessment-record schema`

### [ ] `14.2.5.3` — Assessment database accessor `src/cra/assessment_db.{hpp,cpp}` *(agent)*

**Objective:** the typed in-process accessor over the D4 schema — the seam a future milestone swaps for real persistence.

**Scope:** `struct AssessmentRecord` mirroring `14.2.5.2`'s fields; `get(trackId, warningType) -> optional<Record>`, `upsert(Record)`, `erase(trackId)`, `all()`; every write also appended to the `[EVT]` stream as an `assessment` line so the table is reconstructible offline (D4). **No database engine** — the rationale (a Container Node has no volume, a file dies with the pod) is recorded in HLD D4 and must not be re-litigated. Test `tests/cra/test_assessment_db.cpp`: a record round-trips through `upsert`/`get`; the serialized record **validates against `schema/cra-assessment-record.schema.json`** (loaded from disk, not restated in the test); `erase` removes; the committed sample deserializes.

**Acceptance:** ADA build + ctest green on CI; the schema is loaded and enforced by the test, not duplicated.

**Dependencies:** after `14.2.5.2` + `18.2.2.3`. **Commit:** `[14.2.5.3] feat: add the CRA assessment database accessor`

### [ ] `14.2.5.4` — Plugin registry `src/cra/registry.{hpp,cpp}` + `src/cra/builtin_plugins.cpp` *(agent)*

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

### [ ] `2.2.6.1` — V2X listener `src/observer/v2x_listener.{hpp,cpp}` *(agent)*

**Objective:** the R2 ingress thread — bind `V2X_LISTEN_HOST:V2X_LISTEN_PORT`, receive datagrams, push onto the input queue.

**Scope:** owns one `net::UdpSocket`; one RAII-managed thread joined on destruction (no detached threads); each datagram becomes one `InputItem{V2xR2, body, rxEpochMs}`; receive errors are counted and logged, never fatal; `stop()` is prompt and idempotent. No parsing here. Test `tests/observer/test_v2x_listener.cpp` (planner-designated path): a loopback datagram on an ephemeral port arrives on the queue byte-identical; clean shutdown with no hang; a queue-full drop is counted.

**Acceptance:** ADA build + ctest green on CI; no socket headers outside `src/net/`.

**Dependencies:** after `6.2.2.2` + `3.2.2.4`. **Commit:** `[2.2.6.1] feat: add the R2 UDP listener thread`

### [ ] `12.2.6.2` — Detector reader `src/observer/detector_reader.{hpp,cpp}` *(agent — parallel with 2.2.6.1)*

**Objective:** spawn `DETECTOR_CMD` and read its stdout as R3 JSONL — the ego side of the D2 process contract (argv + exit codes + JSONL over stdout, no FFI, no RPC).

**Scope:**

- `fork`/`exec` the configured command with a stdout pipe; one reader thread doing `getline`; each line becomes one `InputItem{DetectorR3, line, rxEpochMs}`.
- Lifecycle per D2: clean EOF with `DETECTOR_LOOP=true` respawns from frame 0 (so B stays present for a run longer than the clip); a non-zero exit triggers a logged bounded restart up to `DETECTOR_RESTART_MAX`, then stops trying and logs terminal failure. Events `detector_spawn`, `detector_eof`, `detector_restart(reason, attempt)`.
- `DETECTOR_ENABLED=false` means no spawn at all, logged once.
- **Phase 2 drive:** `DETECTOR_CMD="cat /app/tests/fixtures/own_sensor_mock.jsonl"` — the real reader, a fixture producer. There is no fixture-mode branch in this class.
- Test `tests/observer/test_detector_reader.cpp` (planner-designated path): a `printf`/`cat`-style child's lines all arrive; EOF with loop respawns (bounded assertion, injectable sleep); non-zero exit restarts up to the max then stops; `DETECTOR_ENABLED=false` spawns nothing; child is reaped on destruction (no zombies).

**Acceptance:** ADA build + ctest green on CI; no orphaned child process after the suite.

**Dependencies:** after `13.2.2.1` + `3.2.2.4` + `18.2.2.3`. **Commit:** `[12.2.6.2] feat: add the detector subprocess reader`

### [ ] `3.2.6.3` — Mock drive equipment: `tests/fixtures/own_sensor_mock.jsonl` + `tools/mock_v2x_sender.py` *(agent)*

**Objective:** the two Phase 2 stimulus sources — both outside `src/`.

**Scope:**

- `ADA_ECU/tests/fixtures/own_sensor_mock.jsonl`: a hand-written R3 JSONL stream for one own-sensor track `own:1` at ~5 Hz timestamps, distance walking 40 → 8 m and back out past 35 m, every line validating against the synced `contracts/r3-tracked-object.schema.json`, `source: own_sensor`, `state: not_tracked`. Content chosen to traverse the full R13 lifecycle in both directions.
- `ADA_ECU/tools/mock_v2x_sender.py`: Python 3 stdlib only; sends R2 JSON datagrams to a target `host:port` from CLI args/env (**no hardcoded peer**); a `--profile approaching|out-of-range` selecting a distance ramp 60 → 10 m or a static 60 m, mirroring the bench's two committed scenarios (`Scenario_Player/scenarios/default.yaml`, `c-out-of-range.yaml`); configurable rate and count; one stdout line per datagram for correlation. Bodies validate against the synced `contracts/r2-v2x-object.schema.json`.
- Test equipment only — never enters the image (`.dockerignore`, `5.2.7.1`).

**Acceptance:** `python -m py_compile ADA_ECU/tools/mock_v2x_sender.py` passes; the script's `--validate` self-check reports every fixture line valid against the synced R3 schema and every generated body valid against the synced R2 schema (both loaded from `ADA_ECU/contracts/`, never restated); a loopback self-check receives every datagram byte-identical — evidence in the Status line.

**Dependencies:** none. **Commit:** `[3.2.6.3] feat: add the Phase 2 mock own-sensor fixture and R2 sender`

### [ ] `13.2.6.4` — Composition root `src/main.cpp` + `ada_ecu` executable *(agent)*

**Objective:** assemble config → event log → registry → observers → queue → parsers → store, and run the fusion tick — controller only, no rules (HLD §8 MVC mapping).

**Scope:**

- Load `Config` (exit non-zero with a message on invalid env); construct `EventLog`, `TrackStore`, `AssessmentDb`, `Registry` + `registerBuiltinPlugins` (empty in Phase 2), `V2xListener`, `DetectorReader`.
- Main loop: `pop` from the queue → route by source to `r2_parser` / `r3_parser` → `parse_reject` on failure (counted, never fatal) → `store.apply(update)` → `r2_ingest` / `own_sensor_ingest` event. Every `FUSION_TICK_MS`, call `store.expire(now)`; the CRA assessment call is added in Phase 4 (`15.4.2.3`).
- **The main thread is the single writer** of the store (D2). SIGTERM/SIGINT → stop observers, join threads, flush the log, exit 0. Documented exit codes.
- `add_executable(ada_ecu src/main.cpp …)` in `ADA_ECU/CMakeLists.txt`. No new unit-test file — acceptance is the full existing suite plus the link of the complete chain; run-level evidence is `13.2.8.2`.

**Acceptance:** ADA build + ctest green on CI; the `ada_ecu` target builds and links.

**Dependencies:** after `13.2.2.1` + `2.2.6.1` + `12.2.6.2` + `2.2.3.1` + `3.2.3.2` + `13.2.4.3` + `14.2.5.4`. **Commit:** `[13.2.6.4] feat: add the ada_ecu composition root`

### [ ] `18.2.6.5` — `[EVT]`-stream checker `tools/check_evt_log.py` *(agent)*

**Objective:** the scripted assertion that turns a saved `[EVT]` stream into a pass/fail — the ADA counterpart of `tools/comms_check/check_v2x_log.py`, reused on-platform and in CI.

**Scope:**

- Python 3 stdlib; input = file path or stdin; tolerates interleaved `[CAP]` and non-`[EVT]` lines (View Log exports carry both).
- **Phase 2 mode `--admission`:** every `r2_ingest` / `own_sensor_ingest` is followed by store state consistent with the diagram; the observed `track_transition` sequence per track id is a legal path through [the state machine](../ADA_ECU/doc/phase2-4-ada-ecu-admission.puml) (no `not_tracked → tracked` jump, no promotion before `CONFIRM_HITS` in-gate updates, no drop inside the hysteresis band); at least one full `not_tracked → tentative → tracked → not_tracked` cycle observed per named source; non-zero exit naming the first illegal edge.
- **Mode `--expect-no-tracks`:** exit non-zero if any `track_transition` appears — the "mock off yields no tracks" arm.
- Phase 4 extends this script with the risk/emission chain (`18.4.3.3`); the modes are additive.

**Acceptance:** `python -m py_compile` passes; demonstrated exit 0 on a synthetic conforming log and non-zero on each of: an illegal edge, an early promotion, a mid-band drop, and a log with zero `[EVT]` lines — evidence in the Status line.

**Dependencies:** after `18.2.2.3` (field names freeze there). **Commit:** `[18.2.6.5] feat: add the ADA EVT-stream admission checker`

---

## Task Group 2.7 — Image and entrypoint (serves R5; HLD D9)

### [ ] `5.2.7.1` — `ADA_ECU/Dockerfile` + `entrypoint.sh` + `.dockerignore` *(agent)*

**Objective:** the deployable `ada-ecu:latest` image — two stages, **one base**, single-platform `linux/arm64` (D9).

**Scope:**

- Both stages on `python:3.11-slim` — it is the report's Python 3.11 for the detector *and* the C++ build base, so the binary links against the glibc/libstdc++ it runs on by construction (the Scenario Player's ratified F1 pattern). No Vanetza on this node, so apt's cmake (3.22+) suffices.
- Build stage: apt cmake/g++/git, configure and build the `ada_ecu` target only (tests excluded from the image build). Runtime stage: `/app/ada_ecu`, `/app/entrypoint.sh`, workdir `/app`, plus `tcpdump` + `coreutils` (base64) for the Phase 4 capture. `detector/`, `models/`, `media/` COPY lines land in Phase 3 (`5.3.6.1`); `tools/`, `tests/`, `doc/`, `schema/` never enter the runtime stage.
- `entrypoint.sh`: `[ -x ./capture.sh ] && ./capture.sh &` then `exec ./ada_ecu` — the guard exists because `capture.sh` lands in Phase 4 (`6.4.4.1`); the blueprint `command` is `["./entrypoint.sh"]` from now on, so the node config does not change again when capture arrives.
- `.dockerignore` keeps `doc/`, `tests/`, `tools/`, `schema/`, `build/`, `detector/requirements-dev.txt` out of the build context.
- No `ENV` lines shadowing [HLD §6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#6-configuration--no-hardcoded-tunables) defaults — the blueprint injects env.

**Acceptance:** `sh -n` and `bash -n` clean on `entrypoint.sh`, LF line endings, exec bit set; CI `ada-ecu-image` lane (`5.2.8.1`) green — `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -t m1-ada-ecu:latest ADA_ECU/` succeeds. `--platform` and the disabled attestations are a standing requirement: a Container Node rejects a multi-platform manifest index and hangs in Provisioning ([phase0-smoke-test-run.md](doc/phase0-smoke-test-run.md)).

**Dependencies:** after `13.2.6.4` + `5.2.8.1` (the lane must exist to verify). **Commit:** `[5.2.7.1] feat: add the ADA ECU Dockerfile and entrypoint`

---

## Task Group 2.8 — CI lanes (serves R5, R13; workflow-file edits)

> New file `.github/workflows/phase2-ci.yml`, per § CI ruling. Both lands **before** their consumers, guarded on file existence like the Phase 0/1 jobs, so consuming subtasks have CI acceptance from day one. `.github/workflows/` is explicitly in these two subtasks' write scope and no other's.

### [ ] `5.2.8.1` — `phase2-ci.yml` + lane `ada-ecu-image` *(agent)*

**Objective:** arm64 image build and gated push for the ADA node image; `v2x-ecu-image` in [phase1-ci.yml](../.github/workflows/phase1-ci.yml) is the template.

**Scope:** create `.github/workflows/phase2-ci.yml` with the same `on:`/`concurrency:` block as phase1-ci.yml and a header comment naming what the file carries and why (the Phase 1 file's convention). One job `ada-ecu-image` mirroring `v2x-ecu-image` in shape: qemu + buildx setup; `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -t registry.hackathon-2.carsky.io/m1-ada-ecu:latest ADA_ECU/`; push only when `CARSKY_ZOT_API_KEY` exists (same login step + notice-and-exit-0 guard); buildx `type=gha` cache with its own scope; verification of the pushed artifact through the existing `.github/actions/verify-arm64-image` composite; job guarded skip-with-notice while `ADA_ECU/Dockerfile` is absent.

**Acceptance:** workflow YAML valid; run-blocks `bash -n` clean; lane green on the current tree (guard branch) — goes live when `5.2.7.1` lands.

**Dependencies:** none — lands immediately. **Commit:** `[5.2.8.1] chore: add the ADA ECU image build-push CI lane`

### [ ] `13.2.8.2` — Lane `ada-loopback-check` *(agent)*

**Objective:** the repeatable form of the Phase 2 "mock-driven transitions observable in logs; mock off yields no tracks" box.

**Scope:** second job in `phase2-ci.yml`: build the `ada_ecu` target; **run A** — start `ada_ecu` with `DETECTOR_ENABLED=true`, `DETECTOR_CMD="cat ADA_ECU/tests/fixtures/own_sensor_mock.jsonl"`, `IVI_ECU_HOST=127.0.0.1`, stdout captured; drive `tools/mock_v2x_sender.py --profile approaching` at the listen port; SIGTERM; assert `python ADA_ECU/tools/check_evt_log.py --admission` exit 0 over the captured stdout, with a minimum transition count so the check cannot pass vacuously. **Run B** — same binary with `DETECTOR_ENABLED=false` and no sender; assert `check_evt_log.py --expect-no-tracks` exit 0. Job fails on any non-zero exit.

**Acceptance:** lane green on the pushed branch; run A observes at least one complete `tentative → tracked → not_tracked` cycle per source.

**Dependencies:** after `13.2.6.4` + `3.2.6.3` + `18.2.6.5` + `5.2.8.1` (same file). **Commit:** `[13.2.8.2] chore: add the ADA loopback admission CI lane`

---

## Task Group 2.9 — Video input: spec delivery, clip intake, config wiring (serves R12, R5)

> The Phase 2 "video-input study" deliverable. The **study itself is already produced** — [video-source-for-r12.md](../ADA_ECU/doc/research_notes/video-source-for-r12.md) §3 is the spec and §4 is the ask. What remains is delivering it, building the preflight that checks a returned clip, wiring the two env vars, and **obtaining the clip** — which no agent can do.

### [ ] `12.2.9.1` — Clip preflight `tools/check_clip_spec.py` *(agent)*

**Objective:** the [research note KPI 1](../ADA_ECU/doc/research_notes/video-source-for-r12.md#measurable-checks-kpis) made executable — reject a non-conforming clip naming the failing attribute.

**Scope:**

- Python 3 at `ADA_ECU/tools/check_clip_spec.py`; reads a video path and checks the machine-checkable rows of the [§3 spec table](../ADA_ECU/doc/research_notes/video-source-for-r12.md#3-video-input-spec-to-build-phase-3-against): container MP4, codec H.264, resolution 1280×720, constant frame rate 20 fps, duration 60–120 s, file ≤ 60 MB, no audio track — plus a decode pass proving OpenCV reads ≥ 99% of the declared frame count with zero errors (KPI 2).
- Every expected value comes from CLI flags/env with the §3 values as defaults — **no literals** (the numbers are proposals awaiting FPT-Mentor confirmation and may change).
- Probe via `ffprobe` when present, falling back to OpenCV properties with a clear notice; exit 1 listing every failing attribute with actual-vs-expected; exit 0 with a one-line summary otherwise.
- **Out of scope — the content rows.** "B occludes the lane at 10–40 m in ≥ 90% of frames" and "C never visible" are human judgements at intake (`12.2.9.3`) plus the R12 evidence checks in Phase 3 (`12.3.5.1`, `12.3.5.2`); this script must not claim to verify them.
- Test `ADA_ECU/tools/tests/test_check_clip_spec.py` (planner-designated path, § Open items item 4): synthesize small conforming and non-conforming clips with OpenCV; assert exit codes and the named failing attribute.

**Acceptance:** `python -m py_compile` passes; the test passes locally and on CI `python-tests`.

**Dependencies:** none. **Commit:** `[12.2.9.1] feat: add the R12 clip preflight checker`

### [ ] `12.2.9.2` — USER-MANUAL: send the video-input proposal to FPT-Mentor *(user)*

**Objective:** the Phase 2 acceptance clause "video-input proposal sent to FPT-Mentor".

**Scope:** send [video-source-for-r12.md §3](../ADA_ECU/doc/research_notes/video-source-for-r12.md#3-video-input-spec-to-build-phase-3-against) — the format / frame rate / data rate table with its `assume` markers, the KPI list, and the §4 single deliverable — asking for (a) confirmation or correction of the proposed values and (b) whether FPT supplies footage. Nothing new is authored; the note is the artifact.

**Acceptance:** the send is recorded in `plans/doc/phase2-ada-scaffold-run.md` (created by this subtask) with the date and any reply; evidence commit by the orchestrating session after the user confirms.

**Dependencies:** none — do this first in the phase; it has the longest external latency. **Commit:** `[12.2.9.2] docs: record the video-input proposal sent to FPT-Mentor`

### [ ] `12.2.9.3` — HUMAN TASK: supply the demo clip — **blocks Phase 3** *(user)*

**Objective:** the one artifact no agent can produce. Phase 3's R12 acceptance ("detection log over the provided clip with per-frame objects and distance estimates") has no input without it.

**What the user must hand over — exactly one file:**

| Item | Value |
|---|---|
| File | one ego-POV clip named `ego-b-occluding-c.mp4` |
| Repo path | `ADA_ECU/media/ego-b-occluding-c.mp4` |
| Format | MP4 / H.264, 1280×720, 20 fps constant, 60–120 s, ≤ 60 MB, no audio |
| **Content — B** | vehicle B visible and occluding the lane directly ahead in **≥ 90% of frames**, apparent range roughly **10–40 m** |
| **Content — C** | vehicle C **never visible in any frame** — binding, the premise of the whole use case |
| Viewpoint | ego forward-facing camera, roughly fixed, near-collinear same-heading convoy |

**Sourcing, in preference order** ([research note §4](../ADA_ECU/doc/research_notes/video-source-for-r12.md#4-what-the-user-must-provide)): trim a real dashcam recording of a car directly ahead in the same lane · use an openly-licensed driving-POV clip · render an ego POV from a road sim (costs GPU-class load, discouraged on a shared server). Off-spec **format** is fixable in one `ffmpeg` command; off-spec **content** is not fixable at all.

**When it is needed.** Phase 3's clip-dependent group (`12.3.4.*`, `12.3.5.2`, and the R12 acceptance box) cannot start without it. Phase 3's other 15 subtasks — every detector module, the model export, the CI lanes — are clip-independent and proceed against `SyntheticFrameSource` and `tools/make_sample_video.py`. **The practical deadline is the day Phase 3's group 3.4 would otherwise start; against the 2026-08-08 milestone deadline that means the clip is needed within the first third of Phase 3.** Every day past that is a day of slip on the R12 evidence box and, through it, on R19 — the definition of done.

**If no clip arrives — the fallback, and its cost.** Phase 3 develops and demonstrates against `SyntheticFrameSource` and `tools/make_sample_video.py`. That is a **decoder-and-contract smoke fixture, not a demo source**: it writes flat grey rectangles labelled "B", and a pretrained COCO detector will not classify a labelled rectangle as `car`. The concrete cost:

| Still met with the synthetic fixture | Forfeited without a real clip |
|---|---|
| The frame-source seam, stride/loop behaviour, R3 JSONL emission, the store integration, the zero-C check's *structural* argument | R12's acceptance evidence — a detection log with **per-frame objects and distance estimates** |
| Every unit test, every CI lane, the image build | The distance-constant validation (HLD §11 item 3) — nothing to calibrate against |
| The Phase 4 fusion chain (B's range can be injected through the fixture) | The ≥ 5 Hz effective-inference KPI on real decode + inference load |
| — | R19's "zero direct C detections" claim as *measured* evidence rather than an argument from construction |

**A synthetic-fixture run cannot close R12's acceptance box and therefore cannot close R19.** This is the single highest-risk open input in Phases 2–4.

**Acceptance:** the file exists at the stated path, passes `12.2.9.1`'s preflight, and the user confirms the two content rows by eye. Intake and commit happen in Phase 3 (`12.3.4.1`, `12.3.4.2`).

**Dependencies:** after `12.2.9.2` if FPT supplies the footage; otherwise independent and can start today. **Commit:** *(none — the commit is `12.3.4.2`)*

### [ ] `5.2.9.4` — Update `node-ada-ecu.md` with the D9 node config *(agent — writes in `requirements/car-sky-guide/`)*

**Objective:** the guide deliverable HLD D9 names — the ADA node's blueprint config gains the entrypoint/capabilities change and the full §6 env set, so the user configures the node once.

**Scope — [node-ada-ecu.md](../requirements/car-sky-guide/node-ada-ecu.md) § Blueprint node config only:**

- `"command": ["./entrypoint.sh"]` (was `["./ada_ecu"]`) and `"capabilities": ["NET_RAW"]` — required by the in-container capture (D9).
- Env rows added to the existing five: `V2X_LISTEN_HOST`, `CONFIRM_HITS`, `TRACK_TIMEOUT_MS`, `FUSION_TICK_MS`, `DETECTOR_ENABLED`, `DETECTOR_CMD`, `DETECTOR_LOOP`, `DETECTOR_RESTART_MAX`, `VIDEO_CLIP_PATH` (`/app/media/ego-b-occluding-c.mp4`), `DETECTOR_FRAME_STRIDE` (`4`), `MODEL_PATH`, `CONF_THRESHOLD`, `IOU_THRESHOLD`, `TRACK_IOU_MIN`, `VEHICLE_WIDTH_M`, `CAMERA_HFOV_DEG`, `CRA_ENABLED`, `RISK_NEAR_M`, `RISK_CRITICAL_M`, `RISK_TTC_WARN_S`, `RISK_TTC_CRITICAL_S`, `RISK_DWELL_MS`, `STATE_RATE_HZ`, `EVENT_LOG_PATH`, `ASSESS_LOG_EVERY_MS`, `CAPTURE_FILTER`, `PCAP_DIR`, `CAPTURE_ROTATE_S`. Values = the [HLD §6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#6-configuration--no-hardcoded-tunables) defaults; the table there is the authority and is referenced, not duplicated in prose.
- Registry host corrected to `registry.hackathon-2.carsky.io` in § Build & push (the `registry.carsky.io` lines are stale — Phase 0 O1, [phase1_tasks.md § Open items item 6](phase1_tasks.md#open-items--flags-no-phase-1-subtask-may-silently-close-them)).
- **Additive only** — pins unchanged: exactly one `ethernet` `OUTPUT` pin at `10.99.0.12`, **no `video` pin** (the clip is baked into the image; [research note §1](../ADA_ECU/doc/research_notes/video-source-for-r12.md#1-platform-finding--carsky-serves-no-camera-content)). No frozen contract moves.

**Acceptance:** the JSON block is valid JSON; every env name matches `src/config/config.cpp` and `detector/config.py` character for character; links resolve. Doc-only — no build target.

**Dependencies:** after `13.2.2.1` (core env names freeze there); the detector names are HLD-frozen already. **Commit:** `[5.2.9.4] docs: update the ADA node guide with the phase 2-4 node config`

---

## Task Group 2.10 — Plan maintenance (docs, commits on `main`)

### [ ] `13.2.10.1` — Reconcile `milestone1.md` with the Phase 2–4 HLD *(agent — docs)*

**Objective:** the plan of record must not contradict the design of record.

**Scope — [milestone1.md](milestone1.md) only, four edits:**

- §4 § Track admission gate: annotate `miss_limit` (M) — realized as wall-clock `TRACK_TIMEOUT_MS = 1000 ms` per [HLD D3](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#d3--r13-admission-one-state-machine-both-sources), **flagged for the user's re-ratification, not silently replaced** (§ Open items item 1).
- Phase 2 § Tasks: point at the Phase 2–4 HLD alongside `ada-ecu.svg`; state that the video-input study is the committed research note and that the clip is a user deliverable.
- Phase 3 § Tasks/Acceptance: name the clip dependency and `tools/check_zero_c.py` as the zero-C check's instrument.
- Phase 4 § Acceptance: add the output-evidence box (§ Phase 4 output acceptance in [phase4_tasks.md](phase4_tasks.md)).

**Acceptance:** no remaining contradiction between milestone1.md §4/Phases 2–4 and the HLD; [markdown style](../.claude/skills/markdown-writing-style/SKILL.md) held; changes listed in the commit body.

**Dependencies:** none — anytime. **Commit:** `[13.2.10.1] docs: reconcile milestone1 phases 2-4 with the ADA HLD`

---

## Execution order & parallelism

Dependencies are real (files, frozen interfaces, CI lanes) — not default assumptions. At run time everything executes sequentially in one working tree (§ Subtask discipline); the lanes below are the logical structure.

```
Docs        13.2.10.1 · 5.2.1.3                       (anytime)
User-first  12.2.9.2 (longest external latency) ──► 12.2.9.3 (HUMAN, blocks Phase 3)
Gated       5.2.1.1 ∥ 5.2.1.2                          (after the user's go-ahead; anytime)
CI-first    5.2.8.1                                    (guarded lane — lands before 5.2.7.1)

foundation  13.2.2.1 ∥ 6.2.2.2 ∥ 18.2.2.3 ∥ 3.2.2.4
parsers     2.2.3.1 ∥ 3.2.3.2 ──► 3.2.3.3
store       3.2.4.1 ; 13.2.4.2 (after 13.2.2.1) ──► 13.2.4.3 (also needs 3.2.4.1 + 18.2.2.3)
CRA         14.2.5.2 ──► 14.2.5.3 ──► 14.2.5.1 ──► 14.2.5.4
observers   2.2.6.1 (after 6.2.2.2 + 3.2.2.4) ∥ 12.2.6.2 (after 13.2.2.1 + 3.2.2.4 + 18.2.2.3)
equipment   3.2.6.3 ∥ 18.2.6.5 (after 18.2.2.3) ∥ 12.2.9.1     (anytime)
assembly    13.2.6.4 (after 13.2.2.1 + 2.2.6.1 + 12.2.6.2 + 2.2.3.1 + 3.2.3.2 + 13.2.4.3 + 14.2.5.4)
verify      13.2.8.2 (after 13.2.6.4 + 3.2.6.3 + 18.2.6.5 + 5.2.8.1)
image       5.2.7.1 (after 13.2.6.4 + 5.2.8.1)
guide       5.2.9.4 (after 13.2.2.1)
```

**Recommended runtime order (single tree):** 12.2.9.2 → 13.2.10.1 → 5.2.8.1 → 5.2.1.1 → 5.2.1.2 → 5.2.1.3 → 13.2.2.1 → 6.2.2.2 → 18.2.2.3 → 3.2.2.4 → 2.2.3.1 → 3.2.3.2 → 3.2.3.3 → 3.2.4.1 → 13.2.4.2 → 13.2.4.3 → 14.2.5.2 → 14.2.5.3 → 14.2.5.1 → 14.2.5.4 → 2.2.6.1 → 12.2.6.2 → 3.2.6.3 → 18.2.6.5 → 13.2.6.4 → 13.2.8.2 → 5.2.7.1 → 12.2.9.1 → 5.2.9.4.

**Phase 3 and Phase 4 relative to this phase.** [milestone1.md §3](milestone1.md#3-development-plan--order-of-implementation) runs Phases 3 and 4 in parallel after Phase 2. That holds here with one correction: Phase 4 needs only groups 2.2–2.6 (store, admission, CRA seam, main loop); Phase 3 needs only the frozen `detector/contracts/tracked_object.py` from Phase 0 and the `DETECTOR_CMD` contract from `12.2.6.2`. Neither needs the other.

## Acceptance traceability

| Milestone Phase 2 box | Closed by |
|---|---|
| Store exposes all R3 fields; both entry shapes use the identical interface (R3) | 3.2.4.1 (nine-field round-trip + same-`upsert` case) · 2.2.3.1 · 3.2.3.2 |
| Mock-driven transitions observable and matching the diagram; mock off ⇒ no tracks | 13.2.4.2 · 13.2.4.3 · 18.2.2.3 (`track_transition`) · 18.2.6.5 · 13.2.8.2 (both arms) |
| C admitted only within `gate_enter`, dropped only beyond `gate_exit` or after `miss_limit`, no flicker | 13.2.4.3 boundary + oscillation + timeout cases |
| Gate constants from configuration, no literals | 13.2.2.1 (sole env reader) · 5.2.9.4 (blueprint injection) |
| CRA database schema committed | 14.2.5.2 (schema) · 14.2.5.3 (accessor + schema-enforcing test) |
| Video-input proposal sent to FPT-Mentor | 12.2.9.2 (delivery) — the artifact is the committed research note §3 |
| **Demo:** build + CI round-trip tests green on the frozen contracts | `ada-core-build` green per subtask · 5.2.8.1 · 13.2.8.2 · Phase 0 `tests/contracts/` unchanged |
| *(phase task, no box)* R14 abstraction stood up | 14.2.5.1 · 14.2.5.4 — the plugin itself is Phase 4 (`14.4.1.2`) |
| *(phase task, no box)* ADA half of the R18 stream starts | 18.2.2.3 · emission in 13.2.4.3 / 13.2.6.4 |

## Open items & flags (no Phase 2 subtask may silently close them)

| # | Item | Owner / closes at |
|---|---|---|
| 1 | **`miss_limit` semantics — needs the user's re-ratification.** [milestone1.md §4](milestone1.md#track-admission-gate-r13) words M as "consecutive missed updates (proposed 5)"; [HLD D3](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#d3--r13-admission-one-state-machine-both-sources) realizes it as wall-clock `TRACK_TIMEOUT_MS = 1000 ms` (5 periods at the slower of the two sources). The design reason is structural — "its messages stop" is a time condition, and one count would mean two different real timeouts across two independently configured cadences. Implementation proceeds on the wall-clock form (it is the only implementable reading); `13.2.10.1` records the change as flagged, and a user "no" costs a rewrite of `13.2.4.2`/`13.2.4.3` only | **user** |
| 2 | **Deleting `ada-ecu/` (`5.2.1.1`) needs the user's explicit go-ahead** — ~1,900 lines of a teammate's committed work. Gate text and rationale are in the subtask; nothing else in the phase blocks on it | **user**, at `5.2.1.1` |
| 3 | **Deck disposition (`5.2.1.2`)** — `presentation/ada/ada-phase2-3-4-deck.*` documents the superseded design. Recommendation: delete; alternative: banner it | **user**, at `5.2.1.2` |
| 4 | **Planner-designated test and tool paths beyond the HLD's explicit lists**, named per the folder's own conventions: `tests/config/`, `tests/net/`, `tests/log/`, `tests/observer/test_input_queue.cpp`, `tests/observer/test_v2x_listener.cpp`, `tests/observer/test_detector_reader.cpp`, `tests/parser/test_parse_reject_corpus.cpp`, `tests/cra/test_cra_interface.cpp`, `tools/check_evt_log.py`, `tools/check_clip_spec.py`, `tools/tests/test_check_clip_spec.py`. Required by subtask discipline (unit tests per module) and by the research note's KPI 1 (preflight). Flagged to [[project-architecture]] as HLD-consistent additions, not new design | [[project-architecture]] (ack) |
| 5 | **`(proposal)` defaults proceed as proposed** — `CONFIRM_HITS=3`, `TRACK_TIMEOUT_MS=1000`, `FUSION_TICK_MS=100`, `DETECTOR_RESTART_MAX=5`, `DETECTOR_LOOP=true` ([HLD §6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#6-configuration--no-hardcoded-tunables)). Externalized either way, so a ratification change is a node-config edit, not a code change | user |
| 6 | **The clip (`12.2.9.3`) is the phase's one output the project cannot manufacture** — see that subtask for the fallback and its cost. Tracked here because it blocks *Phase 3*, not Phase 2 | **user** |
| 7 | **Repo size** — Phase 3 commits `models/yolo11n.onnx` (~10 MB) and `media/ego-b-occluding-c.mp4` (≤ 60 MB) into the build context, because a Container Node has **no volume** and a file reaches it only inside the image. The alternative (download at image build) costs offline reproducibility and network at build time. Recommendation: commit both; ~70 MB is within normal Git limits and both are write-once. Decision needed before `12.3.3.1` | **user**, by Phase 3 group 3.3 |
| 8 | **Cross-phase, not this plan's work:** `IVI_ECU/app/.../model/R4WarningMessage.kt` on this branch cannot decode this design's R4 output (no `@SerialName`, and it requires a `trackedObjects` array this design does not emit). `R4Message.kt` on `main` is the binding the IVI uses. Recorded as a **Phase 5 input**, per [HLD §11 item 4](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#11-open-items-and-flags) | [[project-planner]] → Phase 5 |

---

*Created 2026-08-02 by project-planner from [phase2-4-ada-ecu-hld.md](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md), [video-source-for-r12.md](../ADA_ECU/doc/research_notes/video-source-for-r12.md) and [milestone1.md § Phase 2](milestone1.md#phase-2--ada-scaffolding-store--state-machine-no-detector-r3-r13). 10 task groups, 30 subtasks: 28 agent-implemented (2 of them user-gated, 3 docs-only), 1 user-manual, 1 human deliverable. Planned from zero — the branch's `ada-ecu/` implementation is superseded by HLD D1 and counts as no work done.*
