# Phase 1 — Comms Bring-up (V2X ECU + Scenario Player): Full Task Breakdown

> **Authority & context:**
> - **Phase content:** [milestone1_high_level_plan.md § Phase 1](../documents/Plan/milestone1_high_level_plan.md#phase-1--comms-bring-up-v2x-ecu--scenario-player-r5r9-r11--r10-moved-to-the-future-plan) — its eight acceptance checkboxes are the phase output.
> - **Design (V2X ECU):** [v2x-ecu-hld.md](../documents/Design/V2X-ECU/v2x-ecu-hld.md) — §4 folder structure, §6 components and env table, §11 build and CI, and the [decision record](../documents/Design/V2X-ECU/v2x-ecu-design-decisions.md) D1–D8 (D4 payload-carrying events; D7 the bench↔V2X comms check). Every V2X_ECU path below is cited from its §4; the D7 script pair lives at repo-root `tools/comms_check/`, sanctioned by [node-code-layout.md § tools/](../.claude/rules/node-code-layout.md#tools--test-equipment-and-ecu-mocks) and V2X D7.
> - **Design (Scenario Player):** [scenario-player-hld.md](../documents/Design/SCENARIO-PLAYER/scenario-player-hld.md) — §4 folder structure, §6 components and configuration, §12 test strategy, and the [decision record](../documents/Design/SCENARIO-PLAYER/scenario-player-design-decisions.md) D1–D7. Every Scenario_Player path below is cited from its §4.
> - **Run timing:** [m1-run-timing-and-event-triggering.md §7](../documents/Requirements/m1-run-timing-and-event-triggering.md) R20 and R22 — the bench's paced scenario clock (group 1.6) and the R22 demo cycle it emits (group 1.13).
> - **Requirements:** [m1-cooperative-awareness.md §2](../documents/Requirements/m1-cooperative-awareness.md) R2, R5–R9, R11, R18 — referenced by number, never restated. **R10 is deferred**: the R7 seam declares `send`, nothing calls it, no subtask implements it.
> - **Phase 0 baseline (do not re-plan):** [phase0_tasks.md § Phase 0 overview](phase0_tasks.md#phase-0-overview) — contracts frozen, codec seam + R2 binding + golden vectors + `check_sync.py` landed, smoke test C1–C5 green on a `baseline_m1` clone, CI lanes live.
> - **Deploy procedure:** [deploy-walkthrough-netcheck.md](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md) is the subject walkthrough for group 1.10; every subtask there cites the section governing its step and takes its acceptance from [§6](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#6-expected-outputs-and-acceptance).
> - **Node guides:** [node-v2x-ecu.md](../requirements/car-sky-guide/node-v2x-ecu.md) and [traffic-capture-wireshark.md](../requirements/car-sky-guide/traffic-capture-wireshark.md) already carry the Phase 1 shape; [node-scenario-player.md](../requirements/car-sky-guide/node-scenario-player.md) is unchanged by design ([SP HLD §11](../documents/Design/SCENARIO-PLAYER/scenario-player-hld.md#11-tech-stack-build-and-ci)).
> - **Rules:** [task-planning-conventions.md](../.claude/rules/task-planning-conventions.md) (`X.Y.Z.W`; subtask discipline restated once in § Subtask discipline below) and [walkthrough-driven-delivery.md](../.claude/rules/walkthrough-driven-delivery.md) (group 1.10 decomposes from the walkthrough, never from the platform).
>
> **Task ID legend:** `X.1.Z.W` — X = requirement served · 1 = this phase · Z = task group · W = subtask position within the group. IDs are stable; never renumber.

## Phase 1 overview

**Objective.** Bench CPMs reach the deployed Room and decode into R2 messages at the ADA ECU — receive-only, ego broadcasts nothing. Deliverables: the V2X ECU application (R7 adapter seam, R8 modem-stub FSM + fault injection, R9 Rx pipeline, R18 JSONL event stream, R6 tcpdump capture), the Scenario Player application (R11 scenario-configurable CPM generation over the D1 `cpm_encode` helper), both node images (R5), the D7 bench↔V2X comms-check script pair + CI lane, and the live deploy + verification with the netcheck sink on the ADA node (D6).

**Input (must exist before start):**

- Phase 0 complete 4/4 ([phase0_tasks.md § Phase 0 overview](phase0_tasks.md#phase-0-overview)): `contracts/` frozen with golden vectors + `sync-manifest.json` + `check_sync.py`; `V2X_ECU/` codec seam `src/codec/`, R2 binding `src/contracts/`, `CMakeLists.txt` baseline; `Scenario_Player/player/contracts/cpm_content.py` + golden `.json` fixtures; `tools/netcheck/` deployed-proven.
- Both Phase 1 HLDs committed; the bench codec path is fixed by SP D1.
- `.github/workflows/phase0-ci.yml` lanes: `contracts-gate`, `python-tests`, `v2x-core-build` (Vanetza `_deps` cached), `ada-core-build`, `ivi-unit-tests`, `netcheck-image` (arm64 build, push gated on `CARSKY_ZOT_API_KEY`).

**Output (phase acceptance = the eight milestone boxes):**

- [ ] Blueprint deploys to a Room; Deployment Viewer shows every node Running; the team APK launches on the AAOS node (R5).
- [ ] UDP reachability between every communicating pair; traffic captured on the bridge network (R6).
- [x] CI import check passes — no direct transport imports above the seam; telux parity notes + port plan committed (R7).
- [x] The full scripted call flow is acked and logged; each injected fault produces a defined, logged recovery (R8).
- [x] Golden-vector CPMs decode correctly; the malformed-input corpus is fully rejected with zero crashes (R9).
- [ ] Different bench scenario configurations produce observably different message streams (R11).
- [ ] R2 messages observed at the ADA ECU carrying decoded bench-scenario values, not constants (R2).
- [ ] **Demo:** Wireshark capture of V2X PDUs correctly sent/received at the V2X ECU interface.

| Box | Met by | Outstanding |
|---|---|---|
| R5 | both node images build for `linux/arm64` — `5.1.5.4`, `5.1.7.3`, `5.1.8.2` (CI run 30698630956) | registry push `5.1.10.1`; deploy and per-node `Running` — `5.1.10.2` + `5.1.10.6`; the APK clause, § Open items item 2 |
| R6 | reachability by the Phase 0 smoke test (C1–C5); the capture pair `6.1.5.2`/`6.1.5.3` syntax- and round-trip-verified | no capture has run on a bridge — `6.1.10.5` |
| R7 | `7.1.3.5` (gate green, CI run 30697863324) and `7.1.3.6` (signatures character-identical to the frozen seam) | — |
| R8 | `8.1.3.2`/`8.1.3.3` prove every ack, illegal-order rejection and D2 recovery (`v2x-core-build`, run 30697863324); the `v2x-comms-check` lane runs the real `init → configure → subscribeRx` bring-up | optional supplementary live evidence — `8.1.10.8` |
| R9 | golden decode and zero crashes CI-proven (Phase 0 `1.0.2.5` + `9.1.4.4`); `9.1.4.5` drives the ten-case corpus through the real Vanetza codec, each case asserting its exact disposition, green in run 30700052056 | — |
| R11 | model level `11.1.6.4`; wire level `11.1.7.2` (`sp-codec-helper` green in run 30697863324) | the live config swap — `11.1.10.9` then `11.1.10.4` |
| R2 | the `v2x-comms-check` lane observes R2 JSON carrying decoded golden-vector values at a loopback sink standing in for the ADA node | the same at the deployed ADA address — `2.1.10.3` |
| Demo | — | `6.1.10.5` on a deployed Room |

**Plan-tracked check beyond the eight boxes (HLD D7)** — evidence supporting the R6, R9 and R2 boxes, not a milestone acceptance criterion:

- [x] Scripted send/capture between bench and V2X ECU passes; V2X `[EVT]` logs demonstrate message receive (`rx_datagram`), event raised (`decode_ok` with decoded CpmContent JSON), and CPM deserialized to JSON (`r2_forwarded` with the R2 body). — **closed** by the `v2x-comms-check` lane in CI run 30697863324: ≥ 6 datagrams received and the full chain asserted, `v2x_ecu` exiting 0 on SIGTERM. `check_v2x_log.py` discriminates: removing the `r2_forwarded` event fails at the forward link and stripping `decode_ok`'s `cpm` payload fails at the decode link (exit 1 each), while the intact chain exits 0. The on-platform half of the same check is `9.1.10.7`.

**Suggested branches (suggestion only — creation is the orchestrator/user's call):** `feat/phase1-comms-bringup` for groups 1.1–1.12, and `feat/phase1-bench-run-timing` for group 1.13, branched from `main`. Docs-only subtasks (this plan file, `5.1.11.1`, group 1.10 evidence records) follow the repo convention of committing straight to `main`.

**Phase 1 acceptance state: 3 of the 8 boxes closed (R7, R8, R9), 5 open (R5, R6, R11, R2, Demo).** Every open clause needs a live Room — § Remaining work.

### Remaining work

**41 of 54 subtasks are closed.** The 13 open subtasks are group 1.10's deploy and verification work, the four R11 scenario-clock subtasks of group 1.6, and group 1.13's `22.1.13.4`. None of the last five closes a Phase 1 box or blocks group 1.10.

Three CI runs verify the closed work: **`30697863324` on `16b8674`** (8 lanes green, the phase's code), **`30698630956` on `7a02fb5`** (10 lanes green, adding both node-image lanes — both images build for `linux/arm64` and push), and **`30700052056` on `31d0347`** (10 lanes green, asserting `9.1.4.5`'s ten dispositions).

| Subtask | Executor | What it still needs |
|---|---|---|
| `5.1.10.2` | Human | Clone `baseline_phase1`, apply the § Task Group 1.10 node config, deploy, and tear the Room down at the end of the group. |
| `5.1.10.6` | AI | Read the deployed blueprint back and poll node phases until every node reads `Running` with restart 0 — the per-node badges, not the summary header, which can read `Pending — 0/0 nodes ready` while traffic flows normally. |
| `2.1.10.3` | AI | Read the ADA sink's `[RX]` bodies for R2 values changing over time. |
| `9.1.10.7` | AI | Run `tools/comms_check/check_v2x_log.py` in stream mode over a saved V2X View Log export. |
| `8.1.10.8` | AI | Capture the `[EVT] stub_transition` bring-up sequence, which prints once at node start. |
| `11.1.10.9` | Human | Swap the bench `SCENARIO_CONFIG` to `c-out-of-range.yaml` and redeploy. |
| `11.1.10.4` | AI | Compare the two log sets against the `default.yaml` baseline in [phase1-comms-run.md](doc/phase1-comms-run.md). |
| `6.1.10.5` | AI + Human | Save a View Log containing a `[PCAP-BEGIN]` block (the Room must have run at least one `CAPTURE_ROTATE_S` period), run `V2X_ECU/tools/extract_pcap.sh`, open the `.pcap` in Wireshark. |
| `11.1.6.9` – `11.1.6.12` | agent | The SP D5 scenario clock: the two scenario keys, the epoch stamp, deadline pacing, and `mono_ms`. |

**Sequencing:** `2.1.10.3`, `9.1.10.7`, `8.1.10.8` and `6.1.10.5` all read the V2X node's log, so one restart plus one sufficiently long log download serves all four.

**Mutable tags:** all three image tags are mutable, so the most recent push defines what the tag resolves to — identify a deployed image at deploy time, never from an old run log.

### Execution split legend

Group 1.10's labels are the walkthrough's own vocabulary, taken from [deploy-walkthrough-netcheck.md §5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human). A subtask whose steps that table assigns to different executors is split, so a label names the whole subtask; `6.1.10.5` is the exception, because its Wireshark step is governed by [traffic-capture-wireshark.md](../requirements/car-sky-guide/traffic-capture-wireshark.md) rather than by that table.

| Label | Meaning |
|---|---|
| *agent* | implemented by a spawned implementation subagent (default) |
| *AI* | a row that walkthrough's work-division table assigns to AI; briefed to [[car-sky]], which holds the credential the row needs |
| *Human* | a row that walkthrough's work-division table assigns to Human; the plan tracks it, and the evidence-record commit is made by the orchestrating session after the user confirms |

**Implementation-subagent specification** (inherited by every *agent* subtask): general-purpose agent; tools Read/Grep/Glob/Write/Edit/Bash; writes ONLY inside the node folder its subtask names (plus its own `**Status:**` line in this file and, where the subtask explicitly says so, `contracts/`, `tools/netcheck/`, `tools/comms_check/`, or `.github/workflows/`); reads the target folder's `doc/` first; inherits § Subtask discipline as its definition of done; makes the atomic commit itself with the exact commit message from the brief plus trailer `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`; never pushes — the orchestrator pushes and watches CI. Language best practice is part of done: C++17 core guidelines / RAII / no raw owning pointers; Python type hints + dataclasses + no globals; tests deterministic.

### Subtask discipline (applies to every subtask below)

Per [task-planning-conventions.md § Subtask discipline](../.claude/rules/task-planning-conventions.md#subtask-discipline-non-negotiable): single objective, no out-of-scope code, exactly one atomic commit with the stated message, build passes, unit tests pass, brief self-contained. Hard execution constraints baked in:

- **Dev host is Windows with no Docker/WSL.** C++ verification (V2X_ECU app, `codec_helper`) and image builds run on GitHub Actions — a C++ subtask's build/tests acceptance = **CI green on the pushed branch** (same model as Phase 0). Python subtasks verify locally with pytest **and** on CI (`python-tests`).
- **Local tests never require CI-only artifacts:** `test_encoder_golden.py` skips (`pytest.mark.skipif`) when the `cpm_encode` binary is absent locally; CI builds the helper then runs it unskipped.
- **Confirming a CI run is human work** unless the session has an authenticated `gh` CLI ([deploy-walkthrough-netcheck.md §5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human), row M4). Every subtask whose acceptance is "CI green" therefore depends on that human step.
- **Sequential execution at run time:** all implementation subagents share one working tree, so subtasks execute one at a time in dependency order. The parallel/sequential marks below are the logical dependency structure (what could parallelize with more workers), not the runtime mode.
- **Status tracking:** each subtask gains a `**Status:**` line (appended in that subtask's own atomic commit) recording done/blocked + verification evidence; no status line = not started.

### Per-node build commands (cited in acceptance below)

| Node / area | Build + test command | Verified |
|---|---|---|
| `V2X_ECU/` | `cmake -S V2X_ECU -B V2X_ECU/build && cmake --build V2X_ECU/build -j $(nproc) && ctest --test-dir V2X_ECU/build --output-on-failure` | CI `v2x-core-build` |
| `Scenario_Player/` (Python) | `pip install -r Scenario_Player/requirements-dev.txt && python -m pytest Scenario_Player/tests` | local **and** CI `python-tests` |
| `Scenario_Player/codec_helper/` | `cmake -S Scenario_Player/codec_helper -B Scenario_Player/codec_helper/build && cmake --build Scenario_Player/codec_helper/build -j $(nproc)` | CI `sp-codec-helper` (11.1.8.1) |
| `contracts/` gate | `python contracts/check_sync.py` → exit 0 | local + CI `contracts-gate` |
| R7 import gate | `python V2X_ECU/tools/check_transport_imports.py` → exit 0 | local + CI `contracts-gate` step (7.1.3.5) |
| Node images | `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -t <tag> <folder>/` | CI `v2x-ecu-image` / `scenario-player-image` (5.1.8.2) |
| `tools/netcheck/` | `python -m py_compile tools/netcheck/netcheck.py` | local |
| `tools/comms_check/` | `python -m py_compile tools/comms_check/send_cpm.py tools/comms_check/check_v2x_log.py` | local + CI `v2x-comms-check` (9.1.12.3) |

---

## Task Group 1.1 — Shared Vanetza pin + sync-manifest extension (serves R11 D2; `contracts/` + both C++ build contexts)

> One codec source, two build contexts (SP HLD D2). The fragment is the single home of the Vanetza tag + ASN.1-only option set; the manifest extension is the drift gate over the new copies and **runs after every copy-landing subtask**, like Phase 0's `1.0.7.1`.

### [x] `11.1.1.1` — Extract `contracts/vanetza-pin.cmake` and switch V2X_ECU onto it *(agent)*

**Objective:** one shared Vanetza pin fragment, master at `contracts/vanetza-pin.cmake`, consumed by `V2X_ECU/CMakeLists.txt` via the byte-synced copy `V2X_ECU/cmake/vanetza-pin.cmake`.

**Scope:**

- Author `contracts/vanetza-pin.cmake` by extracting from `V2X_ECU/CMakeLists.txt` (lines it currently holds inline): the `FetchContent_Declare(vanetza … GIT_TAG fb6c551030dcc12b924299bf401e35e5fe814713 … EXCLUDE_FROM_ALL)` block and the five forced `VANETZA_*`/`BUILD_TESTS` option `set(… CACHE BOOL "" FORCE)` lines, plus a `VANETZA_ASN1_TARGETS` variable naming `Vanetza::asn1;Vanetza::asn1_its_r2`. The fragment declares only — consumers call `FetchContent_MakeAvailable`.
- Copy byte-identical to `V2X_ECU/cmake/vanetza-pin.cmake`; edit `V2X_ECU/CMakeLists.txt` to `include(cmake/vanetza-pin.cmake)` in place of the extracted block. No other CMake change.
- `.github/workflows/phase0-ci.yml` (explicitly in write scope for this subtask): the `v2x-core-build` cache key currently hashes only `V2X_ECU/CMakeLists.txt` — extend `hashFiles(…)` to also hash `V2X_ECU/cmake/vanetza-pin.cmake` so a re-pin invalidates the `_deps` cache.

**Acceptance:** V2X build command green on CI (`v2x-core-build`, all existing tests unchanged); the two fragment files byte-identical (`cmp`); cache key carries both hashes.

**Dependencies:** none — starts immediately. **Commit:** `[11.1.1.1] chore: extract shared vanetza-pin.cmake fragment and switch V2X_ECU to it`

**Status:** implemented 2026-08-01 — cmp identical (`contracts/vanetza-pin.cmake` ≡ `V2X_ECU/cmake/vanetza-pin.cmake`), extraction lossless by diff (declare block + 5 forced options moved verbatim, replaced by one `include()`), cache key hashes both files, check_sync + import gate exit 0; CI verification pending the next push to the phase branch. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `11.1.1.2` — Extend `contracts/sync-manifest.json` with the D2 entries *(agent — runs after every copy-landing subtask)*

**Objective:** the sync gate covers every Phase 1 copy: pin fragment, codec-seam sources, and the Scenario Player `.uper` fixtures.

**Scope — add to the existing manifest (36 copies → 47), exactly the SP HLD D2 table:**

- `contracts/vanetza-pin.cmake` → `V2X_ECU/cmake/vanetza-pin.cmake` · `Scenario_Player/codec_helper/cmake/vanetza-pin.cmake`.
- `V2X_ECU/src/codec/cpm_codec.hpp` / `vanetza_cpm_codec.hpp` / `vanetza_cpm_codec.cpp` (masters, normative home) → `Scenario_Player/codec_helper/src/codec/` same-name copies.
- Each of the six `contracts/golden-vectors/<case>.uper` entries gains the target `Scenario_Player/tests/fixtures/golden/<case>.uper`.
- No `check_sync.py` code change expected (it walks the manifest generically); `f2BannedToken`/`f2Scope` unchanged.

**Acceptance:** `python contracts/check_sync.py` exits 0 over the committed tree; deliberately corrupting one new copy (unstaged) flips it to exit 1; CI `contracts-gate` green.

**Dependencies:** after 11.1.1.1 + 11.1.7.1 + 11.1.7.2 (all new copies must exist first — the gate fails on missing targets). **Commit:** `[11.1.1.2] chore: extend sync manifest with codec-source and uper syncs`

**Status:** done 2026-08-01 — manifest extended to 47 copies (pin fragment ×2, codec-seam sources ×3, golden `.uper` ×6); `check_sync.py` exits 0 on the committed tree and exit 1 naming the pair when a new text copy and a new binary copy are each corrupted unstaged (both restored, gate re-green); no script change needed. Closed: CI run 30697863324 green (`contracts-gate`) over all 47 copies.

---

## Task Group 1.2 — V2X ECU foundation: config, socket, event log, forwarder (serves R8, R7, R18, R2)

> The transport-blind foundation modules of [V2X HLD §4](../documents/Design/V2X-ECU/v2x-ecu-hld.md#4-folder-structure). All paths inside `V2X_ECU/`, test files included — the HLD's §4 tree designates each one. Build/test = V2X row of § Per-node build commands (CI `v2x-core-build`).

### [x] `8.1.2.1` — Env config loader `src/config/config.{hpp,cpp}` *(agent)*

**Objective:** the app's **only env reader** (HLD §4): load + validate the §6 app-consumed env set into an immutable `Config` struct.

**Scope:**

- Fields + defaults exactly per [HLD §6](../documents/Design/V2X-ECU/v2x-ecu-hld.md#6-internal-components): `LISTEN_PORT` (47100), `ADA_ECU_HOST`/`ADA_ECU_PORT` (10.99.0.12/47200), `FAULT_PLAN` (`none·init_fail·configure_reject·subscription_drop`, default `none`), `INIT_RETRY_MAX` (3), `RETRY_BACKOFF_MS` (500), `DEDUPE_WINDOW_MS` (1500), `EVENT_LOG_PATH` (empty = stdout only). The three *(proposal)* defaults proceed as proposed — user ratification stays open (§ Open items item 1). `CAPTURE_*`/`PCAP_DIR` are consumed by `capture.sh` directly, not by this loader.
- Validation: ports 1–65535, non-empty host, `FAULT_PLAN` enum, non-negative retry ceiling, positive backoff/window; invalid value → descriptive error (exception), caller exits non-zero. Env read via an injectable getter so tests never mutate process env.
- Test `tests/config/test_config.cpp`: defaults when unset; each override parsed; each rejection case.

**Acceptance:** V2X build + ctest green on CI; no literal tunable outside the defaults table in this one file.

**Dependencies:** none. **Commit:** `[8.1.2.1] feat: add V2X ECU env config loader`

**Status:** implemented 2026-08-01 — `src/config/config.{hpp,cpp}` + `tests/config/test_config.cpp` added, `v2x_config` static lib + `v2x_config_test` registered in `V2X_ECU/CMakeLists.txt`, transport-import and contract-sync gates exit 0; CI verification pending the next push to the phase branch. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `7.1.2.2` — Sole socket holder `src/net/udp_socket.{hpp,cpp}` *(agent)*

**Objective:** `net::UdpSocket` — the **only** V2X_ECU code allowed to include socket headers (HLD D1).

**Scope:** RAII fd ownership (move-only, no raw owning handles); bind(port) + blocking `recvFrom(buffer)` + `sendTo(host, port, bytes)`; errors surface as typed results/exceptions, never `errno` leaks to callers. Test `tests/net/test_udp_socket.cpp`: loopback send→receive round-trip on an ephemeral port; bind-conflict error surfaces cleanly.

**Acceptance:** V2X build + ctest green on CI; socket headers (`<sys/socket.h>`, `<netinet/*>`, `<arpa/*>`) appear only under `src/net/` (the 7.1.3.5 gate will enforce this permanently).

**Dependencies:** none — parallel with 8.1.2.1. **Commit:** `[7.1.2.2] feat: add UdpSocket sole transport holder`

**Status:** implemented 2026-08-01 — `src/net/udp_socket.{hpp,cpp}` + `tests/net/test_udp_socket.cpp` added (`v2x_net` static lib + `v2x_udp_socket_test` registered), RAII move-only fd with socket includes confined to the .cpp (header POSIX-free), transport-import and contract-sync gates exit 0; CI verification pending the next push to the phase branch. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `18.1.2.3` — R18 JSONL event log `src/log/event_log.{hpp,cpp}` *(agent)*

**Objective:** the R18 evidence-stream writer (HLD D4) — one JSONL line per event, `[EVT]`-prefixed.

**Scope:**

- Event vocabulary exactly D4: `rx_datagram`, `decode_ok`, `decode_reject`, `validate_reject`, `dedupe_drop`, `r2_forwarded`, `stub_transition`, `fault_injected`, `recovery` — each line carries event name, monotonic + epoch timestamps, and the current per-stage counters.
- **Payload-carrying events (D4, consumed by D7):** `decode_ok` embeds the decoded `CpmContent` as JSON; `r2_forwarded` embeds the forwarded R2 JSON body — the `[EVT]` stream alone demonstrates receive → event raised → CPM deserialized to JSON. `check_v2x_log.py` (9.1.12.2) parses these fields, so their names freeze here.
- Sink: stdout always (flushed per line — CarSky View Log is the live window); additionally append to `EVENT_LOG_PATH` when non-empty. nlohmann/json for serialization (Phase 0 pin).
- Test `tests/log/test_event_log.cpp`: line shape (parseable JSON after the `[EVT]` prefix), counter accumulation, embedded `decode_ok`/`r2_forwarded` payloads present and parseable, file sink writes when path set (temp dir).

**Acceptance:** V2X build + ctest green on CI.

**Dependencies:** none — parallel with 8.1.2.1/7.1.2.2. **Commit:** `[18.1.2.3] feat: add R18 JSONL event log writer`

**Status:** implemented 2026-08-01 — `src/log/event_log.{hpp,cpp}` + `tests/log/test_event_log.cpp` added (`v2x_event_log` static lib + `v2x_event_log_test` registered); frozen field names `event`/`mono_ms`/`epoch_ms`/`counters` (+ `cpm` on decode_ok, `r2` on r2_forwarded) recorded in the header comment; transport-import and contract-sync gates exit 0; CI verification pending the next push to the phase branch. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `2.1.2.4` — ADA forwarder `src/forward/ada_forwarder.{hpp,cpp}` *(agent)*

**Objective:** the intra-ego R2 edge (HLD D1 — deliberately **not** under the R7 seam): serialize the Phase 0 `v2x::contracts::R2Message` to JSON and UDP-send to `ADA_ECU_HOST:ADA_ECU_PORT`.

**Scope:** consumes `net::UdpSocket` only (no socket headers here); one datagram per R2 message; send failure logged, never throws into the pipeline. Test `tests/forward/test_ada_forwarder.cpp`: loopback listener receives the JSON of the node-local sample `V2X_ECU/tests/fixtures/samples/r2-object.json` intact.

**Acceptance:** V2X build + ctest green on CI.

**Dependencies:** after 7.1.2.2. **Commit:** `[2.1.2.4] feat: add ADA forwarder for R2 JSON`

**Status:** implemented 2026-08-01 — `src/forward/ada_forwarder.{hpp,cpp}` + `tests/forward/test_ada_forwarder.cpp` added (`v2x_forward` static lib + `v2x_ada_forwarder_test` registered); consumes `net::UdpSocket` only, never-throws `send()` with bool result, one compact-JSON datagram per R2 message; transport-import and contract-sync gates exit 0; CI verification pending the next push to the phase branch. Closed: CI run 30697863324 green (`v2x-core-build`).

---

## Task Group 1.3 — R7 adapter seam + R8 modem stub (serves R7, R8)

> The seam-and-stub pair of HLD D1/D2. The seam mirrors the telux radio surface only; `send` is declared and returns `NotSupported` — R10-deferred, nothing calls it.

### [x] `7.1.3.1` — Freeze the seam: `src/adapter/i_radio_adapter.hpp` *(agent)*

**Objective:** the frozen R7 interface — `init() · configure(RadioConfig) · subscribeRx(RxCallback) · send(bytes)` with typed result codes (HLD D2).

**Scope:** header-only: `RadioConfig` (at minimum the Rx port), `RxCallback = std::function<void(const std::vector<uint8_t>&)>`, a result-code enum incl. `Ok` and `NotSupported`, and the pure-virtual `IRadioAdapter`. Names and call order must match what `doc/telux-parity-and-port-plan.md` (7.1.3.6) will document. Test `tests/adapter/test_i_radio_adapter.cpp`: a minimal fake implements the interface and a scripted `init→configure→subscribeRx` sequence compiles and runs — proves implementability, freezes signatures.

**Acceptance:** V2X build + ctest green on CI; interface text stable (later subtasks may not alter it without re-freezing).

**Dependencies:** none. **Commit:** `[7.1.3.1] feat: freeze IRadioAdapter seam interface`

**Status:** implemented 2026-08-01 — frozen header-only seam `src/adapter/i_radio_adapter.hpp` (`RadioConfig`/`RxCallback`/`RadioResult` + pure-virtual `IRadioAdapter`, `v2x_adapter_seam` INTERFACE target) with fake-impl test `tests/adapter/test_i_radio_adapter.cpp` registered as `v2x_i_radio_adapter_test`; transport-import and contract-sync gates both exit 0; CI verification pending the next push to the phase branch. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `8.1.3.2` — Modem stub FSM happy path `src/stub/modem_stub.{hpp,cpp}` *(agent)*

**Objective:** the R8 FSM `idle → initialized → configured → rx-subscribed`, acking each call (HLD D2) — happy path only.

**Scope:** pure logic, no sockets in this subtask; each transition acked with a typed result and reported to an injectable transition observer (main will wire it to `event_log` as `stub_transition`); illegal call order rejected (e.g. `configure` before `init`). Constructor takes plain params (fault plan enum + retry values), never reads env. Test `tests/stub/test_modem_stub_fsm.cpp`: full scripted call flow acked in order; every illegal-order rejection.

**Acceptance:** V2X build + ctest green on CI.

**Dependencies:** after 7.1.3.1 (uses `RadioConfig` + result codes). **Commit:** `[8.1.3.2] feat: implement modem stub FSM happy path`

**Status:** implemented 2026-08-01 — `v2x::stub::ModemStub` (`src/stub/modem_stub.{hpp,cpp}`, `v2x_stub` lib) with `tests/stub/test_modem_stub_fsm.cpp` (`v2x_modem_stub_fsm_test`) covering the happy path acked in order plus all 9 illegal-order rejections (failure code, state unchanged, observer notified); transport-import and contract-sync gates both exit 0; CI verification pending the next push to the phase branch. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `8.1.3.3` — Fault injection + defined recoveries in the stub *(agent)*

**Objective:** config-driven fault injection with the D2 recovery table — each injected fault produces a defined, observable recovery.

**Scope:**

- `FAULT_PLAN` values per HLD D2: `init_fail`/`configure_reject` → fail the call; recovery = retry with `RETRY_BACKOFF_MS` backoff up to `INIT_RETRY_MAX`, then report terminal failure (caller exits non-zero — container restart is the logged last-resort recovery). `subscription_drop` → drop the subscription after establishment; recovery = automatic re-`subscribeRx`, same backoff, unbounded. Drop + resubscribe + every retry surface through the transition observer as `fault_injected`/`recovery` events.
- Backoff waits use an injectable sleep/clock so tests run instantly and deterministically.
- Extend `tests/stub/test_modem_stub_fsm.cpp`: all four plans; retry-then-succeed; retry-exhaustion terminal path; unbounded resubscribe after drop.

**Acceptance:** V2X build + ctest green on CI — this is the unit-level closure of the R8 milestone box.

**Dependencies:** after 8.1.3.2. **Commit:** `[8.1.3.3] feat: add fault injection and recovery to modem stub`

**Status:** implemented 2026-08-01 — D2 recovery table in `src/stub/modem_stub.{hpp,cpp}` with the `EventKind` observer surface (Ack/Reject/FaultInjected/Recovery), injectable `Sleeper`, and `fault_fail_count` knob; `test_modem_stub_fsm.cpp` extended with all four plans, retry-then-succeed, retry-exhaustion terminal path, and unbounded resubscribe after drop; transport-import and contract-sync gates both exit 0; CI verification pending the next push to the phase branch. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `7.1.3.4` — Seam implementation `src/adapter/stub_radio_adapter.{hpp,cpp}` *(agent)*

**Objective:** `StubRadioAdapter : IRadioAdapter` over the modem stub, with the live Rx path: on `rx-subscribed` the stub side opens the `LISTEN_PORT` UDP socket (via `net::UdpSocket`) on a dedicated Rx thread and delivers each datagram to the subscribed callback (HLD D2).

**Scope:** thread lifetime RAII-managed (joined on destruction, no detached threads); `send` returns `NotSupported` and logs — R10-deferred, seam unchanged; the socket and the Rx thread live in `adapter/stub_radio_adapter`, and the FSM and every recovery stay in `stub/modem_stub` (HLD D2). Test `tests/adapter/test_stub_radio_adapter.cpp`: loopback datagram sent to the bound port reaches the callback with identical bytes; `send` returns `NotSupported`; clean shutdown with no leak/hang (ephemeral port).

**Acceptance:** V2X build + ctest green on CI; no socket headers outside `src/net/`.

**Dependencies:** after 7.1.3.1 + 8.1.3.3 + 7.1.2.2. **Commit:** `[7.1.3.4] feat: implement StubRadioAdapter over the modem stub`

**Status:** implemented 2026-08-01 — `src/adapter/stub_radio_adapter.{hpp,cpp}` implements the frozen seam by delegating `init`/`configure`/`subscribeRx` to `ModemStub` verbatim (no adapter-side retry) and adds the live Rx path: one RAII-owned thread + `std::optional<net::UdpSocket>` bound to `stub.config().rx_port`, shutdown by atomic flag + join within the 200 ms `kRxPollTimeout` poll (no detach, no self-pipe), throwing consumers caught and logged, `send` → `NotSupported` with one logged line; `tests/adapter/test_stub_radio_adapter.cpp` covers loopback byte-identical delivery, repeat-subscribe rejection, throwing callback survival, prompt/idempotent `stop()` + port rebindable after destruction, and `InitFail`/`ConfigureReject` passthrough with no thread started; new `v2x_adapter` target (+`find_package(Threads)`); `check_transport_imports.py` and `contracts/check_sync.py` both exit 0; CI verification pending the next push to the phase branch. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `7.1.3.5` — R7 transport-import gate `tools/check_transport_imports.py` + CI step *(agent)*

**Objective:** the R7 acceptance check, made permanent (HLD D1): no direct transport imports above the seam.

**Scope:**

- Python 3 stdlib script: scan `V2X_ECU/src/**` excluding `src/net/`; exit 1 naming the file on any include of `<sys/socket.h>`, `<netinet/…>`, `<arpa/…>`, or `<asio…>`/`<boost/asio…>`; re-assert the F2 grep ban (bare `asn1::Cpm` under `V2X_ECU/src/`, same rule as `contracts/check_sync.py`).
- `.github/workflows/phase0-ci.yml` (explicitly in write scope — `contracts-gate` lives there): add one guarded step to the existing `contracts-gate` job running the script when it exists, same guard style as that job's `check_sync.py` step.

**Acceptance:** script exits 0 on the committed tree from repo root; a planted violation outside `src/net/` (unstaged) flips exit 1; CI `contracts-gate` green — closes the "CI import check passes" half of the R7 box.

**Dependencies:** none (passes trivially before 7.1.2.2 lands; binding once it does). **Commit:** `[7.1.3.5] feat: add transport-import CI gate`

**Status:** implemented 2026-08-01 — script exits 0 from repo root and an unrelated cwd; probes behaved (`<sys/socket.h>` include → exit 1 naming file:line, bare `asn1::Cpm` → exit 1, `vanetza::asn1::r2::Cpm` → exit 0), probe deleted and tree back to exit 0; guarded step added to CI `contracts-gate`; CI verification pending the next push to the phase branch. Closed: CI run 30697863324 green (`contracts-gate`, import-gate step).

### [x] `7.1.3.6` — Telux parity notes + port plan `doc/telux-parity-and-port-plan.md` *(agent)*

**Objective:** the committed R7 doc deliverable (HLD §8, §12): why the seam's names/call order mirror telux, and what porting to real modem hardware changes.

**Scope:** markdown per [markdown-writing-style](../.claude/skills/markdown-writing-style/SKILL.md): mapping table `IRadioAdapter` methods ↔ telux `Cv2x` radio API calls (init/configure/subscribe/send parity, call-order constraints); port plan — replace `StubRadioAdapter` with a telux-backed implementation, config/threading deltas, everything above the seam unchanged (the node's focus goal); `send` row marked R10-deferred. References the frozen header, never restates it.

**Acceptance:** doc committed at `documents/Design/V2X-ECU/telux-parity-and-port-plan.md`; method names match `i_radio_adapter.hpp` exactly. Doc-only — no build target.

**Dependencies:** after 7.1.3.1. **Commit:** `[7.1.3.6] docs: author telux parity notes and port plan`

**Status:** done 2026-08-01 — doc committed; all four seam signatures verified character-identical to the frozen `i_radio_adapter.hpp`, `send` row marked R10-deferred; telux symbol names marked unconfirmed pending SDK headers (no API invented); links resolve. Closed — doc-only acceptance met (`64abd97`).

---

## Task Group 1.4 — R9 Rx pipeline: decode → validate → dedupe → forward (serves R9, with the R2 build stage)

> HLD D3 — four stages, each a unit-testable class; the pipeline runs synchronously on the Rx thread and is transport-blind (emits R2 via an injected sink callback; main wires the forwarder). Field/unit authority for the derivations: `contracts/r1-cpm-profile.md` (F1/F6/F7/F9) + the node-local synced schemas `V2X_ECU/contracts/r1-cpm-content.schema.json` and `V2X_ECU/contracts/r2-v2x-object.schema.json`.

### [x] `9.1.4.1` — Profile validator `src/pipeline/validator.{hpp,cpp}` *(agent)*

**Objective:** stage 2 — profile-range validation of a decoded `CpmContent` and F9, reject + count by reason. Mandatory-field presence is structural and is caught by the seam ([HLD §6](../documents/Design/V2X-ECU/v2x-ecu-hld.md#6-internal-components)).

**Scope:** range set = the wire-native bounds of `V2X_ECU/contracts/r1-cpm-content.schema.json` (stationId, lat/lon, orientation, coordinates, confidences 1..101) plus F9 `|measurementDeltaTime| ≤ 2047` — note the wire legally carries −2048 but F9 bans it (profile rule; decode alone won't reject it). Violations return a typed reject reason (enum) for counting; no logging inside the class. Bounds sourced from named constants mirroring the schema — contract bounds, not tunables. Test `tests/pipeline/test_validator.cpp`: nominal golden content passes; one case per reject reason incl. mdt = −2048.

**Acceptance:** V2X build + ctest green on CI.

**Dependencies:** none (Phase 0 seam suffices). **Commit:** `[9.1.4.1] feat: implement CPM profile validator`

**Status:** implemented 2026-08-01 — 10 reject reasons mirroring the schema bounds as named constants (stationId type-tight, out-of-range unconstructible post-decode), F9 −2048 rejected as `MdtF9Range`, both gates (`check_transport_imports.py`, `check_sync.py`) exit 0; CI verification pending the next push to the phase branch. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `9.1.4.2` — Deduper `src/pipeline/deduper.{hpp,cpp}` *(agent)*

**Objective:** stage 3 — duplicate drop over key `(stationId, objectId, referenceTime + measurementDeltaTime)` within a sliding window.

**Scope:** window length injected (ms — main passes `Config::DEDUPE_WINDOW_MS`, default 1500 *(proposal)*); injectable clock for deterministic tests; expired entries pruned. Test `tests/pipeline/test_deduper.cpp`: same key inside window drops; outside window passes; differing objectId/stationId/timestamp passes; pruning bounded.

**Acceptance:** V2X build + ctest green on CI; no literal window value outside tests.

**Dependencies:** none — parallel with 9.1.4.1. **Commit:** `[9.1.4.2] feat: implement Rx deduper`

**Status:** implemented 2026-08-01 — window + clock injected (no literal window outside tests; default stays in config.cpp), signed-sum key semantics tested (refTime 1000+5 collides with 995+10), pruning bounded via once-per-window sweep asserted by `size()`, transport-import + contract-sync gates exit 0; CI verification pending the next push to the phase branch. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `9.1.4.3` — R2 builder `src/pipeline/r2_builder.{hpp,cpp}` *(agent)*

**Objective:** stage 4a — map `CpmContent` (wire-native integers) → the Phase 0 `v2x::contracts::R2Message` (SI), owning **every** derivation the codec seam excludes (HLD D3).

**Scope:**

- F7: `object.distance = hypot(x, y)` in metres from the 0.01 m wire units — derived here, never transmitted.
- F6: confidence conversions per the profile doc — `ConfidenceLevel 101 → null` (`std::optional`), coordinate confidence to metres.
- F1: `sender.speed` derived from consecutive `referencePosition`/`referenceTime` deltas per `stationId` — nullable until the 2nd message; per-station state lives in the builder; document the planar small-delta approximation in code.
- Unit conversions: lat/lon 10⁻⁷ ° → °, orientation 0.1 ° → °, positions/velocities 0.01 → SI; `rxTime` stamped from a value passed in by the pipeline (receipt time); `object.timeOfMeasurement` carried through from `CpmContent.measurementDeltaTime` unchanged ([V2X HLD §10.2](../documents/Design/V2X-ECU/v2x-ecu-hld.md#102-r2--the-message-set-this-node-produces-for-the-ada-ecu)) — the frozen R2 schema bounds it at −2048..2047 as a delta against the CPM reference time, so an absolute timestamp is schema-invalid there.
- Test `tests/pipeline/test_r2_builder.cpp`: nominal golden content → values cross-checked against the node-local sample `tests/fixtures/samples/r2-object.json` (distance 25.03 = hypot(25.0, 1.2)); F6 101→null; F1 null-then-derived across two messages.

**Acceptance:** V2X build + ctest green on CI.

**Dependencies:** none — parallel with 9.1.4.1/9.1.4.2 (uses only Phase 0 seam + binding). **Commit:** `[9.1.4.3] feat: implement R2 builder with F1/F6/F7 derivations`

**Status:** implemented 2026-08-01 — F1 (per-station equirectangular speed, null until 2nd msg, Δt ≤ 0 guard) / F6 (101→null, /100 clamped, coord confidence ×0.01 m) / F7 (exact hypot, no rounding) per the profile; nominal golden → field-by-field cross-check against `samples/r2-object.json` in `v2x_r2_builder_test`; transport-import + contract-sync gates exit 0; CI verification pending the next push to the phase branch. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `9.1.4.4` — Pipeline composition `src/pipeline/rx_pipeline.{hpp,cpp}` *(agent)*

**Objective:** the four-stage synchronous pipeline: datagram bytes → `ICpmCodec::decode` → validator → deduper → r2_builder → injected R2 sink callback; every stage outcome emitted to `event_log`.

**Scope:** constructor injects `ICpmCodec&`, the three stage objects, `EventLog&`, and `std::function<void(const R2Message&)>` sink (transport-blind, HLD D1); emits D4 events `rx_datagram`/`decode_ok`/`decode_reject`/`validate_reject`/`dedupe_drop`/`r2_forwarded` with running counters; `DecodeError` → reject + count, never crash/throw out. Test `tests/pipeline/test_rx_pipeline.cpp`: each golden `.uper` fixture from `tests/fixtures/golden/` flows through to the sink with correct R2 values; a duplicated datagram increments `dedupe_drop`; counters match.

**Acceptance:** V2X build + ctest green on CI.

**Dependencies:** after 9.1.4.1 + 9.1.4.2 + 9.1.4.3 + 18.1.2.3. **Commit:** `[9.1.4.4] feat: compose the four-stage Rx pipeline`

**Status:** implemented 2026-08-01 — `noexcept` four-stage composition (`onDatagram`) over caller-owned injected collaborators + `R2Sink`, whole-body `catch(...)` with documented single-reject attribution, counting left to `EventLog`; `v2x_rx_pipeline_test` drives a fake `ICpmCodec` through all 6 golden contents (F6/F7 spot checks), decode/validate short-circuits, in-window duplicate, and a throwing sink; transport-import + contract-sync gates exit 0; CI verification pending the next push to the phase branch. Test uses a fake `ICpmCodec` per coordinator amendment; real-codec proof lands with 9.1.4.5 + the comms-check lane. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `9.1.4.5` — Malformed-input corpus + rejection test *(agent)*

**Objective:** the R9 acceptance corpus: `tests/fixtures/malformed/` fully rejected, zero crashes, counters correct.

**Scope:** commit the ten-case corpus at `tests/fixtures/malformed/`, one file per kind named in [V2X HLD §6](../documents/Design/V2X-ECU/v2x-ecu-hld.md#6-internal-components) — `empty.uper`, `truncated-nominal.uper`, `truncated-mid-object.uper`, `random.uper`, `bit-flipped-payload.uper`, `trailing-garbage.uper`, `oversized.uper`, `wrong-message-id.uper`, `wrong-protocol-version.uper`, `r1-variant.uper`. Provenance of each documented in test comments, generation deterministic. Local fixtures, **not** synced contracts (HLD §4). Test `tests/pipeline/test_rx_pipeline_malformed.cpp`: one parameterized suite over the whole corpus, each case asserting its expected disposition — `Reject` (sink never called, `decode_reject` or `validate_reject` counted) or `ToleratedControl` (decoded and forwarded, as profile §3's ignore-on-decode rule mandates) — with no either-outcome branch, and the process never crashing.

**Acceptance:** V2X build + ctest green on CI — closes the malformed half of the R9 box (golden decode half closed by Phase 0 `1.0.2.5` + 9.1.4.4's test).

**Dependencies:** after 9.1.4.4. **Commit:** `[9.1.4.5] test: reject the malformed-input corpus with zero crashes`

**Status:** implemented; the dispositions follow the architecture ruling in § Open items item 8.

- Corpus committed as ten binary fixtures (`*.uper binary` in `.gitattributes`); each case's sha256 and byte-level provenance sit in the test comments, so every fixture is regenerable from them alone.
- `v2x_rx_pipeline_malformed_test` drives the **real** `VanetzaCpmCodec` through all ten in one parameterized suite, plus a whole-corpus run proving the pipeline is not wedged (golden `nominal.uper` still decodes afterwards) and asserting no `unexpected_exception:` attribution.
- A per-case `Disposition` table (`kReject` / `kToleratedControl`) is the suite's single source of truth: each case asserts that exact disposition with no either-outcome branch, and the whole-corpus counter totals derive from the table under `static_assert`, so a relabel cannot desync them.
- Split: 6 `Reject` (`empty`, `truncated-nominal`, `truncated-mid-object`, `bit-flipped-payload`, `random`, `oversized`) and 4 `ToleratedControl` (`wrong-message-id`, `wrong-protocol-version`, `r1-variant`, `trailing-garbage`). The three header edits assert the ignore-on-decode rule profile §3 mandates; `trailing-garbage` is tolerated because `uper_decode_complete` does not check unconsumed trailing octets.
- **Predicted, not measured at authoring time:** the authoring host has no C++ toolchain, and the repo has no Python ASN.1 decoder, so the three added dispositions were reasoned predictions the test asserts strictly. A wrong prediction turns the lane red, and that case is relabelled in a follow-up commit.
- Gates on the amendment: `contracts/check_sync.py` exit 0 over 47 copies — the malformed corpus is correctly absent from the manifest, being local fixtures per HLD §4 — and `V2X_ECU/tools/check_transport_imports.py` exit 0; `Scenario_Player` pytest unchanged at 116 passed / 7 skipped.
- Closed: `v2x-core-build` green in CI run 30697863324 (real-Vanetza corpus, zero crashes) and in run 30700052056 on `31d0347`, which measures all ten dispositions as asserted.

---

## Task Group 1.5 — V2X app assembly + capture + image (serves R8, R6, R5)

### [x] `8.1.5.1` — Composition root `src/main.cpp` + `v2x_ecu` executable *(agent)*

**Objective:** assemble Config → EventLog → stub/adapter → pipeline → forwarder and run the scripted call flow (HLD §3, §6: main is controller only — no business logic).

**Scope:** load `Config` (exit non-zero with message on invalid env); construct `EventLog`, `VanetzaCpmCodec`, stages, `RxPipeline` with `AdaForwarder` as sink, `StubRadioAdapter` with the stub's transition observer wired to `stub_transition`/`fault_injected`/`recovery` events; drive `init → configure → subscribeRx` honoring the D2 recovery rules (terminal failure → exit non-zero); then block (Rx thread serves traffic). Add `add_executable(v2x_ecu src/main.cpp …)` to `V2X_ECU/CMakeLists.txt`. No new test file — acceptance is the full existing suite + link of the complete chain; live call-flow evidence lands at group 1.10.

**Acceptance:** V2X build + ctest green on CI; `v2x_ecu` target builds.

**Dependencies:** after 8.1.2.1 + 2.1.2.4 + 7.1.3.4 + 9.1.4.4. **Commit:** `[8.1.5.1] feat: add v2x_ecu composition root`

**Status:** implemented 2026-08-01 — full chain wired (Config→EventLog→codec→stages→pipeline→forwarder sink, stub observer→D4 `stub_transition`/`fault_injected`/`recovery`), documented exit codes 0/2/3/4/5, SIGTERM/SIGINT stop → `adapter.stop()`; both gates exit 0 (`check_transport_imports` 25 files clean, `check_sync` 36 copies identical); CI verification pending the next push to the phase branch. Closed: CI run 30697863324 green — `v2x-core-build` builds and links `v2x_ecu`, and the `v2x-comms-check` lane runs the binary end-to-end (exits 0 on SIGTERM).

### [x] `6.1.5.2` — Capture script `capture.sh` *(agent)*

**Objective:** the D5 in-container capture: live `[CAP]` text + rotating pcap + base64 export through View Log.

**Scope:** at `V2X_ECU/capture.sh`; reads `CAPTURE_FILTER` (default `udp`), `PCAP_DIR` (default `/data/capture`, mkdir -p), `CAPTURE_ROTATE_S` (default 60) from env — no literals; two tcpdump processes per D5: (a) `tcpdump -i any -n -l -tttt $CAPTURE_FILTER` prefixed `[CAP]` to stdout; (b) `tcpdump -w` rotating every `CAPTURE_ROTATE_S` (e.g. `-G` + `-z` post-rotate), each closed file base64-emitted to stdout between `[PCAP-BEGIN <name>]` / `[PCAP-END]` marker lines — the exact format 6.1.5.3 parses. Degrade gracefully when NET_RAW is unhonored (log and stay alive — O2 fallback posture).

**Acceptance:** `sh -n V2X_ECU/capture.sh` and `bash -n` pass; LF line endings; marker format exactly as stated. (tcpdump runs only on-platform — runtime evidence at 6.1.10.5.)

**Dependencies:** none. **Commit:** `[6.1.5.2] feat: add tcpdump capture script with pcap export`

**Status:** implemented 2026-08-01 — sh -n + bash -n clean; --export-one round-trip base64-decodes byte-identically (the format 6.1.5.3 parses); LF enforced via .gitattributes, staged blob CR-free; runtime tcpdump evidence lands at 6.1.10.5. Closed — the script's `sh -n`/`bash -n` + marker-round-trip acceptance is met; on-platform tcpdump evidence stays with 6.1.10.5.

### [x] `6.1.5.3` — Host-side extraction `tools/extract_pcap.sh` *(agent)*

**Objective:** the "automatic tool" retrieval path of D5: saved View Log in → `.pcap` files out ([usage contract](../requirements/car-sky-guide/traffic-capture-wireshark.md)).

**Scope:** at `V2X_ECU/tools/extract_pcap.sh` (host tool — never shipped in the image); for each `[PCAP-BEGIN <name>]`…`[PCAP-END]` block: strip markers, base64-decode, write `<name>.pcap` next to the input log; multiple blocks per log; non-zero exit + message when no block found. Runs in Git Bash locally. Self-check acceptance: a synthetic log round-trips byte-identically under `cmp`, with the evidence in the Status line; the method is the implementer's.

**Acceptance:** `bash -n` passes; synthetic round-trip byte-identical.

**Dependencies:** none (marker format is HLD-frozen; logically pairs with 6.1.5.2). **Commit:** `[6.1.5.3] feat: add host-side pcap extraction script`

**Status:** implemented 2026-08-01 — bash -n clean; real round-trip through the landed capture.sh --export-one producer extracted 2 blocks byte-identically (cmp clean, binary bytes incl. CR/LF/NUL); no-block exit 1, truncated/corrupt block non-zero with partial extraction, path-escape name sanitized, CRLF-saved log handled. Closed — the script's `bash -n` + byte-identical round-trip acceptance is met.

### [x] `5.1.5.4` — `V2X_ECU/Dockerfile` + `entrypoint.sh` *(agent)*

**Objective:** the deployable V2X ECU image — local tag `v2x-ecu:latest`, pushed as `registry.hackathon-2.carsky.io/m1-v2x-ecu:latest` ([HLD §4](../documents/Design/V2X-ECU/v2x-ecu-hld.md#4-folder-structure) designates the `Dockerfile` path, [§11](../documents/Design/V2X-ECU/v2x-ecu-hld.md#11-tech-stack-build-and-ci) the build): multi-stage — cmake build stage → runtime stage with tcpdump.

**Scope:**

- `V2X_ECU/entrypoint.sh`: `./capture.sh &` then `exec ./v2x_ecu` (blueprint `command: ["./entrypoint.sh"]` per [node-v2x-ecu.md](../requirements/car-sky-guide/node-v2x-ecu.md)).
- `V2X_ECU/Dockerfile`: build stage installs cmake/g++/libboost-dev/libboost-date-time-dev and builds `v2x_ecu` (tests excluded from the image build to keep it lean); runtime stage on **the same pinned `debian:trixie-slim` tag as the build stage** (HLD §11 — one tag in both stages, since bookworm's CMake is 3.25) plus tcpdump + coreutils(base64) + `v2x_ecu`, `entrypoint.sh`, `capture.sh` at workdir `/app`. LGPLv3 posture (report §4, V2X HLD §11): link Vanetza dynamically (`-DBUILD_SHARED_LIBS=ON` at the image's configure) and copy `libvanetza_asn1*.so` into the runtime stage. `tools/`, `tests/`, `doc/` never enter the runtime stage. Build context is `V2X_ECU/` alone.
- arm64 build-time risk under QEMU flagged — § Open items item 3; use the CI lane's buildx cache.

**Acceptance:** CI `v2x-ecu-image` lane (5.1.8.2) green — `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -t m1-v2x-ecu:latest V2X_ECU/` succeeds.

**Dependencies:** after 8.1.5.1 + 6.1.5.2 + 11.1.1.1 + 5.1.8.2 (lane must exist to verify). **Commit:** `[5.1.5.4] feat: add V2X ECU multi-stage Dockerfile and entrypoint`

**Status:** implemented 2026-08-01 — entrypoint `sh -n`/`bash -n` clean, LF + exec bit set; Dockerfile self-reviewed (multi-stage, `BUILD_SHARED_LIBS=ON` + staged `libvanetza_asn1*.so` with a fail-loud guard, tcpdump/coreutils runtime, no ENV shadowing HLD §6, target-scoped build excludes tests). No local Docker — image build verification lands with the 5.1.8.2 CI lane. **Closed: CI run 30698630956 — the `v2x-ecu-image` lane is green,** so the stated acceptance is met: `docker buildx build --platform linux/arm64` of `V2X_ECU/` completes inside the timeout, with Vanetza compiled under QEMU aarch64 emulation, `libvanetza_asn1*.so` staged, and the tcpdump runtime in place.

---

## Task Group 1.6 — Scenario Player application (serves R11)

> All paths from [SP HLD §4](../documents/Design/SCENARIO-PLAYER/scenario-player-hld.md#4-folder-structure); build/test = Scenario_Player Python row of § Per-node build commands (local pytest **and** CI `python-tests`). Wire-native conversion authority: the callflow note §4.2 mapping ([scenario-player-v2x-callflow-messages.md](../documents/Design/SCENARIO-PLAYER/scenario-player-v2x-callflow-messages.md)); content dataclass: `Scenario_Player/player/contracts/cpm_content.py` (Phase 0). Test files are designated by the same §4 tree.

### [x] `11.1.6.1` — Config loader `player/config.py` + runtime manifest *(agent)*

**Objective:** env + scenario-YAML loading/validation — the bench's **only env reader** ([SP HLD §6](../documents/Design/SCENARIO-PLAYER/scenario-player-hld.md#6-internal-components), D3).

**Scope:**

- Env per SP HLD §6: `SCENARIO_CONFIG` (default `/app/scenarios/default.yaml`), `V2X_ECU_HOST`/`V2X_ECU_PORT`, `ENCODER_PATH` (default `/app/cpm_encode`); injectable env getter.
- YAML shape per D3, validated into frozen dataclasses: `name`, `cpm_rate_hz` (default 10, F8), `duration_s`, `loop`, `sender {station_id, lat, lon, heading_deg}`, `object {object_id, initial_distance_m, closing_speed_mps, lateral_offset_m, classification, confidence}`. Missing/mistyped/non-positive-rate → descriptive `ValueError`.
- Create `Scenario_Player/requirements.txt` (runtime: pinned PyYAML) and make `requirements-dev.txt` start with `-r requirements.txt` so local + CI test installs carry PyYAML.
- Test `tests/test_config.py`: valid YAML loads; each rejection case; env defaults + overrides.

**Acceptance:** Scenario_Player pytest green locally and on CI `python-tests`.

**Dependencies:** none. **Commit:** `[11.1.6.1] feat: add scenario config loader and validation`

**Status:** done 2026-08-01 — pytest 69 passed locally (config loader + env defaults + rejections); requirements.txt created with -r include from dev. Closed: CI run 30697863324 green (`python-tests`).

**Follow-up (not this subtask's scope, do not reopen it):** [SP HLD §6](../documents/Design/SCENARIO-PLAYER/scenario-player-hld.md#6-internal-components) and D5 add two scenario keys this loader validates — `start_delay_s` (default `0.0`) and `reference_time_epoch` (default `its`). Both land in `11.1.6.9`.

### [x] `11.1.6.2` — Committed scenario variants `scenarios/*.yaml` *(agent)*

**Objective:** the two R11-acceptance scenario files (SP HLD D3) — observably different by construction.

**Scope:** `scenarios/default.yaml` — C approaching, 70.0 m closing at 5.0 m/s to 20.5 m across a 10.0 s looping cycle, crossing the 30 m admission gate 8.0 s in ([SP D7](../documents/Design/SCENARIO-PLAYER/scenario-player-design-decisions.md#d7--the-demo-cycle-is-one-clip-length-and-its-geometry-is-solved-backwards-from-the-first-warning)); `scenarios/c-out-of-range.yaml` — C static beyond the 35 m exit gate (value deliberately > `gate_exit`; the pairing with the R13 gate constants is a property of the data, per SP HLD D3). All tunables live in the YAML — new variants are new files, never code. Extend `tests/test_config.py`: both committed files load through `player.config` and differ in the D3 kinematic fields.

**Acceptance:** pytest green locally + CI.

**Dependencies:** after 11.1.6.1. **Commit:** `[11.1.6.2] feat: add the two committed scenario variants`

**Status:** done 2026-08-01 — pytest 72 passed locally; default 60→~10 m approach, c-out-of-range static at 60.0 m > 35 m exit gate. Closed: CI run 30697863324 green (`python-tests`).

### [x] `11.1.6.3` — Kinematic model `player/scenario.py` *(agent)*

**Objective:** the single constant-velocity model (SP HLD D3): `sample(t)` → `CpmContent` in wire-native units.

**Scope:** sender B static WGS84 pose + heading from config (lat/lon → 10⁻⁷ °, heading → 0.1 °); object C relative state along x: `x(t) = initial_distance_m − closing_speed_mps·t` (→ 0.01 m), fixed `lateral_offset_m` (→ 0.01 m), velocity x = −closing_speed_mps (→ 0.01 m/s, B's cartesian frame); conversion table = callflow note §4.2; F9 asserted before return (|mdt| ≤ 2047). One model — scenario differences come only from config values. Test `tests/test_scenario_kinematics.py`: sampled values vs hand-computed positions at t = 0/mid/late; wire-unit conversions exact; F9 bound enforced.

**Acceptance:** pytest green locally + CI.

**Dependencies:** after 11.1.6.1. **Commit:** `[11.1.6.3] feat: implement constant-velocity scenario kinematics`

**Status:** done 2026-08-01 — pytest 85 passed locally; hand-computed t=0/10/20 wire values exact, F9 ±2048 rejected. Closed: CI run 30697863324 green (`python-tests`).

**Two conformance defects in the shipped model** ([m1-run-timing-and-event-triggering.md §6.5](../documents/Requirements/m1-run-timing-and-event-triggering.md)): **(b)** `referenceTime` is populated with Unix epoch ms (`int(time.time() * 1000)`, the generator's `now_ms` default) while the frozen R1 profile defines it as `TimestampIts`, ms since 2004-01-01 TAI. It passes the schema bound and changes no M1 behaviour, because the V2X ECU uses `referenceTime` only as a difference for the F1 speed derivation and never forwards it, but it is non-conformant to a frozen profile — `11.1.6.10` owns the fix. **(c)** `measurementDeltaTime` is always 0 on the wire, because the generator never passes the third argument to `Scenario.sample`; harmless for M1, and §6.2's `measured` rule stays correct when it changes — § Open items item 10. Neither is a contract change.

### [x] `11.1.6.4` — Model-level stream-difference test *(agent)*

**Objective:** `tests/test_streams_differ.py` — the two committed YAMLs yield differing `CpmContent` sequences (the model-level half of the R11 box; the live half is 11.1.10.4).

**Scope:** sample both scenarios over the same time grid; assert object-position sequences differ materially (approaching vs static) and each matches its YAML's kinematics; test-only subtask, no product code.

**Acceptance:** pytest green locally + CI.

**Dependencies:** after 11.1.6.2 + 11.1.6.3. **Commit:** `[11.1.6.4] test: prove committed scenarios yield differing streams`

**Status:** done 2026-08-01 — pytest 92 passed locally; approaching-vs-static sequences differ at every t>0, non-kinematic fields identical. Closed: CI run 30697863324 green (`python-tests`).

### [x] `11.1.6.5` — Encoder client `player/encoder_client.py` *(agent)*

**Objective:** the persistent `cpm_encode` subprocess client (SP HLD D1/D4): one `CpmContent` JSON per stdin line → one base64 UPER payload per stdout line.

**Scope:** spawn `ENCODER_PATH --stream`; JSONL both ways; an `{"error": …}` reply logged + surfaced as a typed failure, never kills the stream; subprocess death → logged restart with backoff (injectable sleep). Test `tests/test_encoder_client.py`: drive against a committed fake encoder script (test fixture standing in via `ENCODER_PATH` override) covering echo-success, error-line, and death-restart — no real binary needed locally.

**Acceptance:** pytest green locally + CI.

**Dependencies:** none — parallel with 11.1.6.1–4 (D1 protocol is HLD-frozen). **Commit:** `[11.1.6.5] feat: add persistent cpm_encode subprocess client`

**Status:** done 2026-08-01 — pytest 98 passed locally; echo/error/death-restart covered against the committed fake helper. Closed: CI run 30697863324 green (`python-tests`).

### [x] `11.1.6.6` — UDP sender `player/sender.py` *(agent)*

**Objective:** stdlib-socket UDP tx of encoded payload bytes to `V2X_ECU_HOST:V2X_ECU_PORT` (SP HLD §3/§4).

**Scope:** one datagram per payload; returns byte length for logging; send errors logged, sender stays alive (bench must stay observable). Test `tests/test_sender.py`: loopback listener receives exact bytes.

**Acceptance:** pytest green locally + CI.

**Dependencies:** none — parallel. **Commit:** `[11.1.6.6] feat: add UDP sender to the V2X ECU`

**Status:** done 2026-08-01 — pytest 102 passed locally; loopback byte-identical delivery, never-raises error path. Closed: CI run 30697863324 green (`python-tests`).

### [x] `11.1.6.7` — Rate-loop generator `player/generator.py` *(agent)*

**Objective:** the `cpm_rate_hz` loop with scenario clock, `duration_s`/`loop` handling, and the `[TX]` JSONL line per datagram (SP HLD D4).

**Scope:** injectable clock/sleep (deterministic tests); per tick: `scenario.sample(t)` → encode fn → send fn → print `[TX]` JSONL (scenario time, seq, byte length) flushed to stdout; `loop: true` restarts scenario time at `duration_s`, `loop: false` exits cleanly; encode-failure tick logged and skipped, loop continues. Test `tests/test_generator.py`: tick count/cadence against a fake clock; loop restart; duration exit; `[TX]` line shape.

**Acceptance:** pytest green locally + CI.

**Dependencies:** after 11.1.6.3. **Commit:** `[11.1.6.7] feat: add rate-loop generator`

**Status:** done 2026-08-01 — pytest 107 passed locally; cadence/loop-restart/duration-exit/[TX]-shape/encode-skip covered with fake clock. Closed: CI run 30697863324 green (`python-tests`).

**Follow-up — this loop is the bench half of the SP D5 scenario clock, and of R20:** as shipped, scenario time is a tick counter (`t = cycle_tick * period`) advanced by a fixed `sleep(period)` with no drift correction, so it diverges from wall time by the per-tick work cost, ~1 % accumulating and unbounded over a run; and the `[TX]` line carries no timestamp, so [SP HLD §12](../documents/Design/SCENARIO-PLAYER/scenario-player-hld.md#12-test-strategy)'s `scenario_time_s` within ±1 % of `mono_ms` — R20's K5 — has nothing to read. D5 mandates both, so `11.1.6.11` replaces the fixed sleep with deadline scheduling and `11.1.6.12` adds `mono_ms`.

### [x] `11.1.6.8` — Entrypoint `main.py` *(agent)*

**Objective:** the blueprint-fixed entrypoint at the folder root (`command: ["python", "main.py"]`, workdir `/app`): load env + YAML → spawn encoder → run the generator (SP HLD D4) — controller only, no business logic.

**Scope:** wire `config` → `scenario` → `encoder_client` → `sender` → `generator`; startup + fatal errors logged to stdout (View Log). Test `tests/test_main.py`: end-to-end short run — a bounded scenario (small `duration_s`, `loop: false`), the fake encoder from 11.1.6.5, a loopback UDP listener; assert datagrams received + `[TX]` lines emitted.

**Acceptance:** pytest green locally + CI.

**Dependencies:** after 11.1.6.1 + 11.1.6.3 + 11.1.6.5 + 11.1.6.6 + 11.1.6.7. **Commit:** `[11.1.6.8] feat: add scenario player entrypoint main.py`

**Status:** done 2026-08-01 — pytest 110 passed locally; end-to-end smoke (fake encoder → loopback UDP) received datagrams with [TX] lines; [FATAL] path returns 1. Closed: CI run 30697863324 green (`python-tests`).

### [ ] `11.1.6.9` — Validate the two D5 scenario keys in `player/config.py` *(agent)*

**Objective:** `player/config.py` loads and validates the two scenario keys [SP HLD §6](../documents/Design/SCENARIO-PLAYER/scenario-player-hld.md#6-internal-components) designates and D5 mandates — nothing else changes.

**Scope:**

- Extend the frozen `ScenarioConfig` dataclass in `Scenario_Player/player/config.py` with `start_delay_s` (float, default `0.0`, must be ≥ 0) and `reference_time_epoch` (string, default `its`, permitted values `its` and `unix`).
- Rejection cases raise the same descriptive `ValueError` the existing keys use, naming the offending key.
- Neither key is read by any other module in this subtask — application lands in `11.1.6.10` and `11.1.6.11`.
- Extend `Scenario_Player/tests/test_config.py`: both defaults when the keys are absent; both overrides parsed; a negative `start_delay_s` and an unknown `reference_time_epoch` each rejected. Both committed `scenarios/*.yaml` still load unchanged.

**Acceptance:** Scenario_Player pytest green locally and on CI `python-tests`; no literal for either default outside the config module.

**Dependencies:** after 11.1.6.1. Parallel with the rest of group 1.6. **Commit:** `[11.1.6.9] feat: validate the start_delay_s and reference_time_epoch scenario keys`

### [ ] `11.1.6.10` — Stamp `referenceTime` against `reference_time_epoch` in `player/scenario.py` *(agent)*

**Objective:** `referenceTime` leaves the bench as `TimestampIts` — milliseconds since 2004-01-01T00:00:00.000 TAI — with the epoch taken from configuration ([SP HLD §10](../documents/Design/SCENARIO-PLAYER/scenario-player-hld.md#10-the-contract--r1-the-message-set-this-node-produces), D5).

**Scope:**

- `Scenario_Player/player/scenario.py` applies `ScenarioConfig.reference_time_epoch` when it stamps `CpmContent.referenceTime`: `its` subtracts the 2004-01-01 TAI epoch offset from the wall-clock milliseconds, `unix` keeps Unix epoch milliseconds.
- The epoch offset is a named module constant, not a literal in the stamping expression, and the epoch choice is never read from env.
- No kinematics change; `measurementDeltaTime` handling is untouched (§ Open items item 10).
- Extend `Scenario_Player/tests/test_scenario_kinematics.py`: under `its` a known wall-clock instant produces the expected `TimestampIts` value, cross-checked against the golden vector's `716084805123`; under `unix` the same instant produces Unix epoch ms; the value stays inside the schema bound in both.

**Acceptance:** pytest green locally + CI `python-tests`.

**Dependencies:** after 11.1.6.9. **Commit:** `[11.1.6.10] fix: stamp referenceTime against the configured epoch`

### [ ] `11.1.6.11` — Deadline-schedule the generator's scenario clock *(agent)*

**Objective:** scenario time advances at 1.0× wall time against `CLOCK_MONOTONIC` deadlines, offset by `start_delay_s` (SP HLD D5) — replacing the fixed per-tick sleep.

**Scope:**

- `Scenario_Player/player/generator.py`: tick *n* is due at `t0 + start_delay_s + n × period` on the injected monotonic clock, and the loop sleeps until that instant. The deadline is computed from `t0`, never accumulated by adding `period` to the previous wake-up, so the per-tick work cost does not enter scenario time.
- A tick whose deadline has already passed runs immediately rather than sleeping a negative interval; the sequence number and scenario time still come from the deadline, not from the wake-up.
- `start_delay_s` is the grace before the first CPM and is read from `ScenarioConfig` alone; `duration_s` and `loop` semantics are unchanged.
- Extend `Scenario_Player/tests/test_generator.py` against the existing fake clock: deadlines land at exact multiples of `period` regardless of injected per-tick work; the first tick is delayed by `start_delay_s`; a late tick does not shift later deadlines; `loop: true` restart and `loop: false` exit still hold.

**Acceptance:** pytest green locally + CI `python-tests`; no `sleep(period)` literal remains in the loop.

**Dependencies:** after 11.1.6.9 + 11.1.6.7. **Commit:** `[11.1.6.11] fix: schedule the generator against CLOCK_MONOTONIC deadlines`

### [ ] `11.1.6.12` — Add `mono_ms` to the `[TX]` line *(agent)*

**Objective:** the `[TX]` JSONL line carries `mono_ms`, so `scenario_time_s` can be regressed against elapsed time ([SP HLD §12](../documents/Design/SCENARIO-PLAYER/scenario-player-hld.md#12-test-strategy), D5).

**Scope:**

- `Scenario_Player/player/generator.py`: the per-datagram line becomes `[TX] {"seq":…,"scenario_time_s":…,"bytes":…,"mono_ms":…}`, exactly the field set and order SP HLD §12 fixes. `mono_ms` is the injected monotonic clock in integer milliseconds.
- Field names freeze here — the line is the bench's only observability surface.
- Extend `Scenario_Player/tests/test_generator.py`: the emitted line parses as JSON after the `[TX]` prefix and carries all four keys; `mono_ms` advances monotonically across ticks; over a fake-clock run of ≥ 60 s of scenario time, `scenario_time_s` tracks `mono_ms` within ±1 %.

**Acceptance:** pytest green locally + CI `python-tests`.

**Dependencies:** after 11.1.6.11. **Commit:** `[11.1.6.12] feat: add mono_ms to the bench [TX] line`

---

## Task Group 1.7 — Bench codec path: `cpm_encode` helper + encoder goldens + image (serves R11 D1/D2, R5)

### [x] `11.1.7.1` — `codec_helper/` — build `cpm_encode` from the synced codec seam *(agent)*

**Objective:** the D1 helper CLI, built inside `Scenario_Player/` from byte-synced copies of the V2X codec-seam sources — no cross-folder reads.

**Scope:**

- Land the synced copies (byte-identical to masters, **never edited here**): `codec_helper/src/codec/{cpm_codec.hpp, vanetza_cpm_codec.hpp, vanetza_cpm_codec.cpp}` from `V2X_ECU/src/codec/`; `codec_helper/cmake/vanetza-pin.cmake` from `contracts/vanetza-pin.cmake`.
- `codec_helper/CMakeLists.txt`: C++17 + C, includes `cmake/vanetza-pin.cmake`, FetchContent nlohmann (same v3.11.3 pin) + Vanetza, builds the `cpm_encode` executable.
- `codec_helper/src/main.cpp`: `--stream` — read one `CpmContent` JSON per stdin line, write one base64 UPER line per success or one `{"error": "<reason>"}` line per failure, never exit on bad input; `--encode <file>` — one-shot for tests, base64 to stdout, non-zero exit on failure.

**Acceptance:** CI `sp-codec-helper` lane (11.1.8.1) green: helper builds and `--encode` on the six synced golden `.json` files succeeds; copies `cmp`-identical to masters.

**Dependencies:** after 11.1.1.1 (fragment master) + 11.1.8.1 (lane exists to verify). **Commit:** `[11.1.7.1] feat: build cpm_encode helper from synced codec seam`

**Status:** implemented 2026-08-01 — 4 synced copies blob-hash-identical to their masters (git hash-object verified); cpm_encode CLI (--stream JSONL loop, --encode one-shot) + second-build-context CMakeLists landed; inline RFC-4648 base64 hand-verified; both gates exit 0. Build + --encode verification lands with the 11.1.8.1 CI lane. Closed: CI run 30697863324 green (`sp-codec-helper`) — the helper builds and `--encode` succeeds over the six synced goldens.

### [x] `11.1.7.2` — Encoder-golden test + `.uper` fixture sync *(agent)*

**Objective:** wire-truth proof of D2: `cpm_encode(golden .json) == golden .uper`, byte-for-byte.

**Scope:** land the six synced copies `Scenario_Player/tests/fixtures/golden/<case>.uper` (byte-identical to `contracts/golden-vectors/`); `tests/test_encoder_golden.py`: for each case, run the helper (`--encode`), base64-decode, compare to the `.uper` bytes; binary located via `ENCODER_PATH` env or the local build path `codec_helper/build/cpm_encode`; **`pytest.mark.skipif` when the binary is absent** — local runs skip, CI builds then runs unskipped (§ Subtask discipline).

**Acceptance:** pytest green locally (skipped) and on CI `sp-codec-helper` (executed, 6/6); copies `cmp`-identical.

**Dependencies:** after 11.1.7.1. **Commit:** `[11.1.7.2] test: verify cpm_encode against golden vectors`

**Status:** implemented 2026-08-01 — six .uper copies blob-hash-identical to contracts/golden-vectors/; test skips cleanly without the binary (suite 116 passed / 7 skipped) and, proven against a stand-in binary, passes 6/6 on matching bytes and fails with a first-differing-offset report on drift; both gates exit 0. Real-binary execution lands with the 11.1.8.1 CI lane. Closed: CI run 30697863324 green (`sp-codec-helper`) — the test ran **unskipped** against the built binary, so `cpm_encode(golden .json) == golden .uper` byte-for-byte is CI-proven 6/6.

### [x] `5.1.7.3` — `Scenario_Player/Dockerfile` *(agent)*

**Objective:** the deployable Scenario Player image — local tag `scenario-player:latest`, pushed as `registry.hackathon-2.carsky.io/m1-scenario-player:latest` ([SP HLD §11](../documents/Design/SCENARIO-PLAYER/scenario-player-hld.md#11-tech-stack-build-and-ci), D4): stage 1 cmake-builds `codec_helper` → stage 2 python runtime.

**Scope:** **both stages resolve the same base image, `python:3.11-slim`, through one `ARG`** (SP D4), so `cpm_encode` links against exactly the glibc and libstdc++ it runs on. CMake comes from a pip wheel pinned `cmake>=3.28,<4`, because this base's apt CMake is below the 3.28 floor the helper needs. Stage 1 builds `cpm_encode`; stage 2 carries `pip install -r requirements.txt`, `main.py`, `player/`, `scenarios/`, and `cpm_encode` at `/app/cpm_encode` (workdir `/app`). LGPLv3 dynamic posture (report §4, SP HLD §11 + D4): copy `libvanetza_asn1*.so` into stage 2, or configure shared as 5.1.5.4 does. `codec_helper/src`, `tests/`, `doc/` never enter stage 2. Blueprint config unchanged ([node-scenario-player.md](../requirements/car-sky-guide/node-scenario-player.md)). arm64/QEMU build-time risk: § Open items item 3.

**Acceptance:** CI `scenario-player-image` lane (5.1.8.2) green — `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -t m1-scenario-player:latest Scenario_Player/` succeeds.

**Dependencies:** after 11.1.7.1 + 11.1.6.8 + 5.1.8.2. **Commit:** `[5.1.7.3] feat: add Scenario Player multi-stage Dockerfile`

**Status:** done, commit `e02fb7c`.

- Both Docker stages resolve one base image, `python:3.11-slim`, with the CMake floor met by pip `cmake>=3.28,<4` (SP D4), so no glibc or GLIBCXX skew is possible.
- The runtime stage carries `cpm_encode` and the staged `libvanetza_asn1*.so`; only `main.py`, `player/`, `scenarios/` and `requirements.txt` reach it, and `CMD` matches the blueprint command.
- Closed by CI run 30698630956: the `scenario-player-image` lane builds `Scenario_Player/` for `linux/arm64`, covering `e02fb7c`.

---

## Task Group 1.8 — CI lanes (workflow-file edits; serves R11, R5)

> Both add jobs to `.github/workflows/phase1-ci.yml` (explicitly in write scope), guarded on file existence like the existing jobs — they land **before** their consumers so those subtasks have CI acceptance from day one.

### [x] `11.1.8.1` — Lane: `sp-codec-helper` build + encoder-golden pytest *(agent)*

**Objective:** the Linux verification lane for group 1.7.

**Scope:** new job `sp-codec-helper` in `.github/workflows/phase1-ci.yml`: checkout; install libboost-dev + libboost-date-time-dev; `actions/cache` over `Scenario_Player/codec_helper/build/_deps` keyed on the Vanetza pin + `hashFiles` of `codec_helper/CMakeLists.txt` + `codec_helper/cmake/vanetza-pin.cmake`; configure/build per the codec_helper row of § Per-node build commands (`-j $(nproc)` — bounded, same OOM rationale as `v2x-core-build`); then `--encode` smoke over the six golden `.json` and `python -m pytest Scenario_Player/tests` with `ENCODER_PATH` pointing at the built binary. Entire job guarded: skip-with-notice while `codec_helper/CMakeLists.txt` is absent.

**Acceptance:** workflow YAML valid; lane green on the current tree (guard branch) — goes live when 11.1.7.1 lands.

**Dependencies:** none — lands immediately. **Commit:** `[11.1.8.1] chore: add codec-helper build and encoder-golden CI lane`

**Status:** implemented 2026-08-01 — YAML valid (8 jobs), run-blocks bash -n clean, cache key carries the same Vanetza pin as v2x-core-build; lane builds cpm_encode, smoke-tests --encode over the six synced golden .json, and runs the SP suite with ENCODER_PATH so test_encoder_golden executes unskipped (expected 123 passed / 0 skipped vs 116/7 locally). CI verification pending the next push to the phase branch. Closed: lane green in CI run 30697863324 with `test_encoder_golden.py` executing unskipped.

### [x] `5.1.8.2` — Lanes: `v2x-ecu-image` + `scenario-player-image` docker builds *(agent)*

**Objective:** the two arm64 node-image lanes — build and gated push for both node images, with `netcheck-image` as the template.

**Scope:** two jobs in `.github/workflows/phase1-ci.yml`, mirroring `netcheck-image` verbatim in shape: qemu + buildx setup; `docker buildx build --platform linux/arm64 --provenance=false --sbom=false`; registry tags `registry.hackathon-2.carsky.io/m1-v2x-ecu:latest` and `registry.hackathon-2.carsky.io/m1-scenario-player:latest`; push only when the `CARSKY_ZOT_API_KEY` secret exists (same login step + notice-and-exit-0 guard); buildx layer cache (`type=gha`) against the QEMU build-time risk; each job guarded skip-with-notice while its `Dockerfile` is absent; raised `timeout-minutes` for the C++ stages.

**Acceptance:** workflow YAML valid; both lanes green on the current tree (guard branch) — go live as the Dockerfiles land.

**Dependencies:** none — lands immediately, parallel with 11.1.8.1. **Commit:** `[5.1.8.2] chore: add node-image docker build-push CI lanes`

**Status:** implemented 2026-08-01, landed in `df90774` — YAML valid (10 jobs), run-blocks `bash -n` clean; both lanes build arm64 single-platform with `--provenance`/`--sbom=false` and per-image `type=gha` cache scopes, push gated on `CARSKY_ZOT_API_KEY` (missing secret ⇒ green build-only). Added beyond the brief: an `actions/github-script@v7` step per lane re-exporting `ACTIONS_CACHE_URL`/`ACTIONS_RESULTS_URL`/`ACTIONS_RUNTIME_TOKEN` into `GITHUB_ENV`, without which `type=gha` cannot reach the Actions cache from a plain `run:` step, and `ignore-error=true` on `--cache-to` so a cache-service fault cannot fail an otherwise-successful multi-hour build. **Closed: both lanes green in CI run 30698630956** on `7a02fb5`, which carries both Dockerfiles and both lanes. The arm64 QEMU builds complete within `timeout-minutes: 360`, and the per-image cache scopes leave the `actions/cache` `_deps` entries intact — `v2x-core-build`, `v2x-comms-check` and `sp-codec-helper` stayed green in the same 10-lane run.

---

## Task Group 1.9 — ADA-side R2 sink update (serves R2; D6)

### [x] `2.1.9.1` — Parameterize netcheck `BODY_PREVIEW` *(agent)*

**Objective:** the D6 netcheck sink made real ([V2X HLD D6](../documents/Design/V2X-ECU/v2x-ecu-design-decisions.md), §7 test-equipment row): the `[RX]` body-preview length becomes the `BODY_PREVIEW` env var so the ADA sink can show whole R2 JSON bodies.

**Scope:** `tools/netcheck/netcheck.py` only (explicitly in write scope): replace the literal `96` in the `[RX]` log line with `BODY_PREVIEW` read from env, **default 96** (spec-preserving); add it to the module docstring's env list. No other behavior change; `Dockerfile`/`entrypoint.sh`/`capture.sh` untouched. The sink deployment sets `BODY_PREVIEW=512` (5.1.10.2); the image re-push rides the existing `netcheck-image` lane (5.1.10.1).

**Acceptance:** `python -m py_compile tools/netcheck/netcheck.py` passes; default path produces byte-identical log behavior to today.

**Dependencies:** none. **Commit:** `[2.1.9.1] feat: parameterize netcheck body preview length`

**Status:** done 2026-08-01 — py_compile passes; diff is exactly the env constant + docstring + slice; default 96 preserves today's behavior byte-identically. Closed — the stated local acceptance (`py_compile`, byte-identical default behavior) is fully met; no CI artifact is required.

---

## Task Group 1.10 — Deploy & live verification (serves R5, R2, R11, R6 + the Demo box)

> The subject walkthrough is [deploy-walkthrough-netcheck.md](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md), whose M1–M12 are the clone, configure, deploy, read-logs and teardown sequence this group performs with different images and env. Every subtask below cites the section governing its step, takes its executor from [§5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human), and takes its acceptance from [§6](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#6-expected-outputs-and-acceptance); the commands stay in the walkthrough. Registry push runs in the image CI lanes; the remaining Nydus UI work is Human, because no REST route updates an existing node's config ([§5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human), row M7). Evidence accumulates in `plans/doc/phase1-comms-run.md` (created by 5.1.10.1). Standing requirement: images single-platform `linux/arm64` ([phase0-smoke-test-run.md](doc/phase0-smoke-test-run.md)).

> **Acceptance for an ECU Room.** [§6](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#6-expected-outputs-and-acceptance)'s C1–C4 apply unchanged: every node `Running` with restart 0, no `[ERR]` line, a live readable log per node, and a `[CAP]` line on the capturing node. C5's accumulated `|bench|v2x` stamp is netcheck's payload; the equivalent per-node observables for these images are [V2X HLD §12](../documents/Design/V2X-ECU/v2x-ecu-hld.md#12-test-strategy) and [SP HLD §12](../documents/Design/SCENARIO-PLAYER/scenario-player-hld.md#12-test-strategy), which each subtask names.

### [x] `5.1.10.1` — Push the three images to the CarSky registry *(AI — executed by the image CI lanes)*

**Objective:** `registry.hackathon-2.carsky.io` holds current `m1-v2x-ecu:latest`, `m1-scenario-player:latest`, and `m1-netcheck:latest` (rebuilt with 2.1.9.1's `BODY_PREVIEW`).

**Human precondition:** `CARSKY_ZOT_API_KEY` is stored as a GitHub repository secret — [M2](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m2--store-the-registry-credential-as-a-github-secret), a Human row.

**Scope — steps, with the executor per step:**

1. Push a commit so the image lanes run — [M3 + M4](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m3--m4--build-and-push-automatic). *(AI)*
2. Confirm the run passed in the Actions web UI — [M4](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m3--m4--build-and-push-automatic). *(Human — an agent session holds no GitHub token)*
3. Confirm each tag reached the registry over the catalog and tag-list routes — [M4](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m3--m4--build-and-push-automatic). Registry host per [zot-registry-api-key.md § Registry host caveat](../requirements/car-sky-guide/zot-registry-api-key.md#registry-host-caveat-open-item-o1). *(AI)*
4. Create `plans/doc/phase1-comms-run.md` recording the pushed tags and the lane that pushed each. *(AI)*

**Acceptance:** all three tags present in the registry catalog; the run record created. No digests are recorded — the tags are mutable, and [phase1-comms-run.md](doc/phase1-comms-run.md) states why.

**Dependencies:** after 5.1.5.4 + 5.1.7.3 + 2.1.9.1. **Commit:** `[5.1.10.1] docs: record phase1 image pushes to the CarSky registry`

**Status:** closed 2026-08-01, executed by the image CI lanes. `CARSKY_ZOT_API_KEY` is a repo secret, so the gated push step in each lane ran: run `30698630956` logged `Notice: pushed registry.hackathon-2.carsky.io/m1-v2x-ecu:latest (linux/arm64)` and the same for `m1-scenario-player:latest`, each a single manifest rather than an index; record in [phase1-comms-run.md](doc/phase1-comms-run.md). Presence in the registry is further confirmed by deployment — every node on the Room `phase1_Minh_test-deploy` pulled and ran, and the sink's full 339-character `[RX]` bodies show `m1-netcheck:latest` carries `2.1.9.1`'s `BODY_PREVIEW` rather than the hardcoded-96 build. **Mutable tags:** `main` pushes the same tags from pre-`2.1.9.1` code, so a `main` commit landing before the branch merges replaces the registry copy — re-check the `[RX]` body length after `main` activity, and identify a deployed image at deploy time rather than from a run log.

### [ ] `5.1.10.2` — Human: blueprint node config, deploy and teardown

**Objective:** a Room deployed from a clone of `baseline_phase1`, carrying the Phase 1 node config.

`baseline_phase1` is the clone source for every Room after the smoke test ([carsky-4-node-blueprint.md §8](../requirements/car-sky-guide/carsky-4-node-blueprint.md#8-the-blueprints-on-carsky)). It already carries the bench and V2X images, so on a current clone the config below is a confirmation rather than a first entry. Work on the clone; never edit `baseline_phase1` itself, and never edit the `<name>-deploy` snapshot.

**Node config — cited, not restated:**

- Bench `.10` per [node-scenario-player.md § Blueprint node config](../requirements/car-sky-guide/node-scenario-player.md#blueprint-node-config), with `SCENARIO_CONFIG=/app/scenarios/default.yaml`.
- V2X `.11` per [node-v2x-ecu.md § Blueprint node config](../requirements/car-sky-guide/node-v2x-ecu.md#blueprint-node-config), with `FAULT_PLAN=none`.
- ADA `.12` is the D6 sink and is in no node guide: image `registry.hackathon-2.carsky.io/m1-netcheck:latest`, `command: ["./entrypoint.sh"]`, `capabilities: ["NET_RAW"]`, env `ROLE=ada-sink`, `LISTEN_PORT=47200`, `BODY_PREVIEW=512`, and **no** `NEXT_HOP_*`.
- IVI keeps the provided AAOS artifact; no APK is installed at this point.

**Scope — steps, all Human per [§5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human):**

1. Clone `baseline_phase1` and rename the clone — [M5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m5--choose-the-blueprint).
2. Apply the node config above to the three container nodes — [M7](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m7--configure-the-three-container-nodes).
3. Deploy the clone, picking an existing Device — [M9](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m9--deploy--criterion-c1). The platform allows two concurrent deployments.
4. Optional, for `8.1.10.8`'s supplementary evidence: set `FAULT_PLAN=init_fail` on the V2X node, redeploy, then restore `none` — [M7](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m7--configure-the-three-container-nodes) + [M9](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m9--deploy--criterion-c1).
5. Delete the Deployment once every subtask of this group holds its evidence — [M12](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m12--tear-down).

**Acceptance:** the Room exists and the clone's stored config matches the list above, as read back by `5.1.10.6`. Evidence recorded in `plans/doc/phase1-comms-run.md`; the evidence commit is made by the orchestrating session after the user confirms.

**Dependencies:** after 5.1.10.1. **Commit:** `[5.1.10.2] docs: record phase1 blueprint config and deployment`

### [ ] `5.1.10.6` — AI: blueprint read-back and node-phase poll → every node Running

**Objective:** the R5 box's Running clause, proven from the platform rather than from the Inspector — the deployed blueprint's stored config read back, and every node's phase polled to `Running` with restart 0 (walkthrough criterion C1).

**Scope — steps, all AI per [§5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human):**

1. Read the clone back and confirm each of the four role nodes has one `ethernet` pin wired to the bridge at `10.99.0.10`–`.13` — [M6](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m6--check-the-wiring). A missing pin is a Human canvas fix, reported back to `5.1.10.2`.
2. Read each node's stored `image`, `command`, `env` and `capabilities` back and compare against `5.1.10.2`'s list — [M7](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m7--configure-the-three-container-nodes).
3. Confirm the IVI node carries its VM image artifact — [M8](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m8--leave-the-ivi-node-alone).
4. Poll node phases until every node reads `Running`, recording each `nodeKey` — [M9](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m9--deploy--criterion-c1). Read the per-node badges, not the summary header, which read `Pending — 0/0 nodes ready` on the Room `phase1_Minh_test-deploy` while traffic flowed normally.

**Acceptance:** criterion C1 of [§6](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#6-expected-outputs-and-acceptance) — every node `Running`, restart count 0 — with the read-back and the phase list recorded in `plans/doc/phase1-comms-run.md`. The APK clause of the R5 box is not closed here; § Open items item 2.

**Dependencies:** after 5.1.10.2. **Commit:** `[5.1.10.6] docs: record phase1 blueprint read-back and node Running evidence`

### [ ] `2.1.10.3` — AI: R2 observed at the deployed ADA sink

**Objective:** the R2 box live — the ADA sink's `[RX]` lines carry R2 JSON with decoded bench-scenario values, not constants.

**Scope — steps, all AI per [§5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human), row [M10](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m10--read-the-logs--criteria-c2c5):**

1. Read the ADA node's log over the logs route.
2. Extract the `[RX]` bodies at the 512-character preview `2.1.9.1` enables.
3. Confirm `object.distance` changes across successive messages, as `default.yaml`'s approach kinematics require — the observable [V2X HLD §12](../documents/Design/V2X-ECU/v2x-ecu-hld.md#12-test-strategy) names for this box.

**Acceptance:** criteria C2 and C3 of [§6](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#6-expected-outputs-and-acceptance) on the ADA node — no `[ERR]` line, a live readable log — plus the `[RX]` excerpts showing a changing `object.distance`, recorded in `plans/doc/phase1-comms-run.md`.

**Dependencies:** after 5.1.10.6. **Commit:** `[2.1.10.3] docs: record R2-at-ADA live evidence`

### [ ] `9.1.10.7` — AI: on-platform `[EVT]`-chain check in stream mode

**Objective:** the on-platform half of the D7 check — the deployed V2X node's `[EVT]` stream passes `check_v2x_log.py`, with the live Scenario Player as the sender.

**Scope — steps, all AI per row [M10](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m10--read-the-logs--criteria-c2c5):**

1. Save the V2X node's log to a local file over the logs route.
2. Run `python tools/comms_check/check_v2x_log.py <saved.log>` in stream mode against it.
3. Record the exit status and the checker's output.

Exit 0 asserts `rx_datagram` → `decode_ok` (CpmContent JSON) → `r2_forwarded` (R2 JSON) per received message, which is the chain [V2X HLD §12](../documents/Design/V2X-ECU/v2x-ecu-hld.md#12-test-strategy) lists as this node's deployed observable. The same checker in expected-vector mode already closed the CI half in `9.1.12.3`.

**Acceptance:** `check_v2x_log.py` exits 0 on the saved export, with its output recorded in `plans/doc/phase1-comms-run.md`; criteria C2 and C3 of [§6](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#6-expected-outputs-and-acceptance) hold on the V2X node.

**Dependencies:** after 5.1.10.6 + 9.1.12.2. **Commit:** `[9.1.10.7] docs: record on-platform EVT-chain check evidence`

### [ ] `8.1.10.8` — AI: the R8 scripted call flow on the deployed node

**Objective:** R8's live evidence — the three `[EVT] stub_transition` lines in the D2 order, from the deployed V2X node.

**Scope — steps, all AI per row [M10](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m10--read-the-logs--criteria-c2c5):**

1. Read the V2X node's log from the first seconds after node start; the bring-up sequence prints once and does not repeat.
2. Extract the three `[EVT] stub_transition` lines and the two `[BOOT]` lines, the observables [V2X HLD §12](../documents/Design/V2X-ECU/v2x-ecu-hld.md#12-test-strategy) names for R8's scripted call flow.
3. Where `5.1.10.2` step 4 supplied a `FAULT_PLAN=init_fail` Room, extract the `fault_injected`, `recovery` and retry lines from it as supplementary evidence.

Step 3 is optional: R8's box is closed at unit and loopback level by `8.1.3.2`/`8.1.3.3`, and this subtask does not gate it.

**Acceptance:** the three `stub_transition` lines in the D2 order recorded in `plans/doc/phase1-comms-run.md`.

**Dependencies:** after 5.1.10.6. **Commit:** `[8.1.10.8] docs: record R8 live bring-up evidence`

### [ ] `11.1.10.9` — Human: swap the bench scenario and redeploy

**Objective:** a second Room state differing from `2.1.10.3`'s by one configuration value — no rebuild, config only (SP HLD D3).

**Scope — steps, both Human per [§5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human):**

1. Set the bench node's `SCENARIO_CONFIG` to `/app/scenarios/c-out-of-range.yaml` — [M7](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m7--configure-the-three-container-nodes).
2. Redeploy — [M9](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m9--deploy--criterion-c1).

**Acceptance:** criterion C1 of [§6](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#6-expected-outputs-and-acceptance) on the redeployed Room, and the bench node's stored `SCENARIO_CONFIG` reading `c-out-of-range.yaml` on read-back.

**Dependencies:** after 2.1.10.3 (the `default.yaml` baseline must be captured first). **Commit:** `[11.1.10.9] docs: record the bench scenario swap`

### [ ] `11.1.10.4` — AI: scenario swap → observably different streams

**Objective:** the live half of the R11 box; the model half is `11.1.6.4`.

**Scope — steps, all AI per row [M10](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m10--read-the-logs--criteria-c2c5):**

1. Read the V2X `[EVT]` and ADA `[RX]` logs from the `11.1.10.9` Room.
2. Compare them against the `default.yaml` baseline captured in `2.1.10.3`: a static beyond-gate distance instead of the approaching sequence.
3. Record the paired before/after excerpts.

This is R11's acceptance at Room level, which [SP HLD §12](../documents/Design/SCENARIO-PLAYER/scenario-player-hld.md#12-test-strategy) places on the consumer's log.

**Acceptance:** paired before/after log excerpts recorded in `plans/doc/phase1-comms-run.md`, differing in the object's distance sequence and in nothing else.

**Dependencies:** after 11.1.10.9. **Commit:** `[11.1.10.4] docs: record scenario-swap stream difference evidence`

### [ ] `6.1.10.5` — Capture retrieval → Wireshark (R6 + Demo)

**Objective:** the R6 box's captured-traffic clause and the Demo box; reachability is already proven by the Phase 0 smoke test.

Retrieval procedure: [traffic-capture-wireshark.md § Retrieving a .pcap (user steps)](../requirements/car-sky-guide/traffic-capture-wireshark.md#retrieving-a-pcap-user-steps). What this subtask adds is the two flows to verify and where the evidence lands.

**Scope — steps, with the executor per step:**

1. Save the V2X node's log to a local file, choosing a window that contains a `[PCAP-BEGIN]`…`[PCAP-END]` block; the Room must have run at least one `CAPTURE_ROTATE_S` period — [M10](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m10--read-the-logs--criteria-c2c5). *(AI)*
2. Run `V2X_ECU/tools/extract_pcap.sh <saved.log>`. *(AI)*
3. Open the produced `.pcap` in Wireshark and filter `udp.port == 47100 || udp.port == 47200`. *(Human — visual judgement, and the walkthrough's §5 preamble puts a browser and an operator's eyes outside what an agent reaches)*
4. Verify both flows at the single capture point: bench→V2X UDP/47100 payloads matching the golden-vector and `[EVT]` sizes and timestamps, and V2X→ADA UDP/47200 R2 JSON. *(Human)*
5. Archive the `.pcap` and record the finding in `plans/doc/phase1-comms-run.md`. *(AI)*

Dissection caveat (D5): raw UPER without GN/BTP shows as UDP data, so the evidence is payload-byte correlation rather than an ITS protocol tree.

**Acceptance:** criterion C4 of [§6](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#6-expected-outputs-and-acceptance) — a `[CAP]` line on the capturing node — plus the archived `.pcap` and the two-flow finding recorded in `plans/doc/phase1-comms-run.md`.

**Dependencies:** after 5.1.10.6, parallel with 2.1.10.3 / 9.1.10.7 / 8.1.10.8. **Commit:** `[6.1.10.5] docs: record capture retrieval and Wireshark evidence`

---

## Task Group 1.11 — Plan maintenance (docs)

### [x] `5.1.11.1` — Reconcile milestone1.md Phase 0 acceptance *(agent — docs, commits on `main`)*

**Objective:** `plans/milestone1.md` § Phase 0 agrees with [phase0_tasks.md § Phase 0 overview](phase0_tasks.md#phase-0-overview) — all four acceptance boxes closed, smoke test C1–C5 green.

**Scope:** flip box 4 to `[x]`; replace the blocker paragraph with a one-line closed statement citing [phase0-smoke-test-run.md](doc/phase0-smoke-test-run.md). No other milestone1.md change.

**Acceptance:** milestone1.md § Phase 0 shows 4/4 closed, consistent with phase0_tasks.md; [markdown style](../.claude/skills/markdown-writing-style/SKILL.md) held.

**Dependencies:** none — anytime. **Commit:** `[5.1.11.1] docs: reconcile milestone1 Phase 0 acceptance with phase0_tasks`

**Status:** done 2026-08-01 — landed on `main` as commit `68ef5f5`. [milestone1_high_level_plan.md](../documents/Plan/milestone1_high_level_plan.md) § Phase 0 shows 4/4 closed in this tree; the note about the branch copy lagging `main` is stale following this plan update.

---

## Task Group 1.12 — Bench↔V2X comms check (serves R6, R9; HLD D7)

> The D7 script pair + CI lane — scripted acceptance that messages sent between bench and V2X ECU are received, raise events, and deserialize to JSON. Location `tools/comms_check/` at the repo root is sanctioned by [node-code-layout.md § tools/](../.claude/rules/node-code-layout.md#tools--test-equipment-and-ecu-mocks) and V2X D7 — the scripts span the bench and the V2X ECU, so they belong to neither node folder — and is explicitly in these subtasks' write scope. Test equipment only — never shipped in a node image.

### [x] `6.1.12.1` — Golden-vector UDP sender `tools/comms_check/send_cpm.py` *(agent)*

**Objective:** the bench-side send stand-in for local/CI runs (D7): send each golden-vector `.uper` payload as one UDP datagram to a target `host:port`.

**Scope:** Python 3 stdlib only; target host/port and corpus directory (default `contracts/golden-vectors/`) from CLI args/env — no hardcoded peers (governing principle 5); deterministic case order + configurable inter-send delay; one stdout line per sent vector (case name, byte length) for downstream correlation. On-platform the live sender is the Scenario Player — this script never deploys.

**Acceptance:** `python -m py_compile tools/comms_check/send_cpm.py` passes; loopback self-check — a scratch UDP listener receives all six vectors byte-identical (stdlib-only, runs on the Windows host; evidence in the Status line).

**Dependencies:** none. **Commit:** `[6.1.12.1] feat: add golden-vector UDP sender for the comms check`

**Status:** done 2026-08-01 — py_compile passes; loopback self-check delivered all 6 golden .uper byte-identical (sha256 match), no-args exit 2, empty-corpus exit 1, cwd-independent corpus default. Closed — local acceptance met, and the sender is exercised for real by the green `v2x-comms-check` lane in CI run 30697863324.

### [x] `9.1.12.2` — `[EVT]`-stream assertion `tools/comms_check/check_v2x_log.py` *(agent)*

**Objective:** assert the D7 receive-evidence chain from a V2X ECU `[EVT]` stream: per message, `rx_datagram` (received) → `decode_ok` carrying the decoded `CpmContent` JSON (event raised, deserialization shown) → `r2_forwarded` carrying the R2 JSON body; non-zero exit naming the first missing link.

**Scope:** Python 3 stdlib; input = file path or stdin — accepts CI-captured stdout **or** a saved View Log export (the smoke-test View-Log-as-retrieval model), tolerating interleaved `[CAP]`/non-`[EVT]` lines; two modes: **expected-vector mode** (CI — given the golden corpus dir, asserts the chain per sent case and cross-checks the embedded `decode_ok` content against the golden `.json`) and **stream mode** (on-platform — every observed `rx_datagram` must complete the chain; minimum-count threshold arg). Line shape = 18.1.2.3's D4 output.

**Acceptance:** `python -m py_compile tools/comms_check/check_v2x_log.py` passes; demonstrated exit 0 on a synthetic conforming log and non-zero on logs missing each link kind, both modes (evidence in the Status line).

**Dependencies:** after 18.1.2.3 (the embedded-payload field names freeze there). **Commit:** `[9.1.12.2] feat: add EVT-stream assertion script for the comms check`

**Status:** done 2026-08-01 — py_compile passes; synthetic conforming log (6 golden cases, `[CAP]`/`[BOOT]`/blank noise interleaved) exits 0 in both modes, plus a `--repeat 2` corpus in expected-vector mode; 10 negative fixtures (missing rx/decode/forward link, unmatched cpm, `decode_reject` non-zero, unknown event, non-monotonic counters, lost-line counter gap, malformed `[EVT]` JSON, no `[EVT]` lines) each exit 1 naming the failing link — the cpm cross-check and `decode_reject == 0` are expected-vector-mode assertions by design, so those two fixtures pass in stream mode; stdin input, `MIN_MESSAGES` env and 9 exit-2 invocation cases checked; contract-sync and transport-import gates exit 0. Closed — local acceptance met, exercised by the green `v2x-comms-check` lane in CI run 30697863324, and independently shown to discriminate: dropping the `r2_forwarded` event fails at the forward link and stripping `decode_ok`'s `cpm` payload fails at the decode link (both exit 1), while the complete chain exits 0.

### [x] `9.1.12.3` — CI lane `v2x-comms-check` *(agent)*

**Objective:** the CI-side closure of the D7 check: build `v2x_ecu`, run it loopback, send golden vectors, assert the `[EVT]` chain.

**Scope:** `.github/workflows/phase1-ci.yml` (explicitly in write scope): job `v2x-comms-check` — boost install + the `v2x-core-build` `_deps` cache (same key incl. the 11.1.1.1 fragment hash); build the `v2x_ecu` target; start a stdlib UDP sink standing in for ADA; run `v2x_ecu` in the background with env `LISTEN_PORT`/`ADA_ECU_HOST=127.0.0.1`/`ADA_ECU_PORT`/`FAULT_PLAN=none`, stdout captured to a file; `send_cpm.py` against the listen port; stop the app; `check_v2x_log.py` in expected-vector mode over the captured stdout — the job fails on any non-zero exit.

**Acceptance:** lane green on the pushed branch.

**Dependencies:** after 8.1.5.1 + 6.1.12.1 + 9.1.12.2. **Commit:** `[9.1.12.3] chore: add v2x-comms-check CI lane`

**Status:** implemented 2026-08-01 — YAML valid, run-block bash -n clean, cache key identical to v2x-core-build; lane injects DEDUPE_WINDOW_MS=1 + --delay-ms 100 because four golden vectors share the dedupe key; CI verification pending the next push to the phase branch. Closed: lane green in CI run 30697863324 — ≥ 6 datagrams observed at the ADA sink stand-in with the full chain asserted.

---

## Task Group 1.13 — Bench run timing: paced scenario clock and the R22 demo cycle (serves R20, R22, R11)

> **The bench half of R20 and R22.** Design of record: [SP HLD §6](../documents/Design/SCENARIO-PLAYER/scenario-player-hld.md#6-internal-components) (the scenario-key table), [§12](../documents/Design/SCENARIO-PLAYER/scenario-player-hld.md#12-test-strategy) (the `[TX]` observable), **[D5](../documents/Design/SCENARIO-PLAYER/scenario-player-design-decisions.md#d5--the-scenario-clock-is-deadline-scheduled-and-every-offset-is-configuration)** (the deadline-scheduled clock and its configuration) and **[D7](../documents/Design/SCENARIO-PLAYER/scenario-player-design-decisions.md#d7--the-demo-cycle-is-one-clip-length-and-its-geometry-is-solved-backwards-from-the-first-warning)** (the demo cycle). Requirements: [m1-run-timing-and-event-triggering.md §7](../documents/Requirements/m1-run-timing-and-event-triggering.md) R20 and R22, §6.1's key table, §6.6's geometry.
>
> `scenarios/default.yaml` already carries the R22 geometry — `duration_s` 10.0, `initial_distance_m` 70.0, `closing_speed_mps` 5.0, `cpm_rate_hz` 10, `loop` true. **This group does not re-derive those values**, and it does not repeat group 1.6: the scenario keys, the deadline-scheduled clock and `mono_ms` are `11.1.6.9`–`11.1.6.12`. What is left is the test the retimed file broke, and the one value only a deployed measurement can supply.
>
> **Suggested branch (suggestion only — creation is the user's call):** `feat/phase1-bench-run-timing`, branched from `main`. The two subtasks below commit onto it; the phase's original `feat/phase1-comms-bringup` is merged and is not reused.
>
> Build + test = the Scenario_Player Python row of § Per-node build commands — `python -m pytest Scenario_Player/tests` locally **and** CI `python-tests`.

### [x] `11.1.13.1` — Retune the R11 acceptance tests to the R22 `default.yaml` *(agent — sequential-first in this group)*

**Objective:** the R11 model-level acceptance tests assert the kinematics the committed scenario actually produces. Test-only — **no product code changes in this subtask.**

**Scope — three test files under `Scenario_Player/tests/`:**

- **The sampling grid** in `test_streams_differ.py`. `TIME_GRID` is `t = 0..20 s step 1 s`, which samples past `default.yaml`'s `duration_s = 10.0` into negative object ranges the generator never emits. Replace it with a grid inside **both** committed scenarios' `duration_s` — `t = 0.0 .. 9.0 s`, step 1.0 s — and take the bound from the loaded `ScenarioConfig`s rather than a literal, so a later retune cannot silently re-break it.
- **`test_default_x_matches_yaml_kinematics`** asserts `round((60.0 - 2.5 * t) * 100)`. The committed file is `initial_distance_m: 70.0`, `closing_speed_mps: 5.0`, so the expression is `round((70.0 - 5.0 * t) * 100)`. Derive it from the loaded `ObjectConfig` fields, not from re-typed numbers.
- **`test_velocity_matches_each_yaml`** asserts `default.object.velocity.x == -250`. At 5.0 m/s the wire value is **`-500`** (0.01 m/s units). Derive it from `closing_speed_mps` too.
- **`test_x_sequences_differ_at_every_positive_t`** no longer states a property the two scenarios have. Default sweeps *through* the static variant's constant range — at `t = 2.0 s` it is exactly 60.0 m, so both read `6000` — and pointwise difference is therefore false at one grid point whatever the guard. Assert the invariant that does hold: the two sequences are not the same sequence, and a strictly decreasing stream meets a constant one **at most once**.
- **`test_config.py`'s `TestCommittedScenarioVariants`** asserts `initial_distance_m == 60.0` and a final distance ≈ 10 m. Retune it to the R22 geometry, and make it the **one** place the geometry appears as literals: it pins intent (including the 30 m gate crossing landing at 8.0 s, inside R22's open interval), while `test_streams_differ.py` derives from the same files and checks only the wiring from data to wire. Without both halves the derived tests cannot catch a wrong YAML value.
- **`test_scenario_kinematics.py`'s `_default_config()`** mirrors `default.yaml` in code and is documented as doing so. It is green but stale — 60.0 / 2.5 / 20.0 with hand-computed `6000/3500/1000` at `t = 0/10/20`. Update the values, the sample points and the docstring so the mirror is true.
- **The module docstring** of `test_streams_differ.py` states the 20 s / 60 m grid. Restate it against the committed files, and cite [SP D7](../documents/Design/SCENARIO-PLAYER/scenario-player-design-decisions.md#d7--the-demo-cycle-is-one-clip-length-and-its-geometry-is-solved-backwards-from-the-first-warning) rather than repeating the derivation.
- **Do not touch** `test_out_of_range_x_constant` or `test_non_kinematic_fields_identical_at_equal_t`. `test_out_of_range_x_static_beyond_exit_gate` keeps its `c-out-of-range.yaml` pairing (unchanged at 60.0 m static) but takes its expected value from the loaded config like the rest.

**Acceptance:** `python -m pytest Scenario_Player/tests` green locally and on CI `python-tests` **and** `sp-codec-helper` — the two lanes the retiming reddened. In `test_streams_differ.py` no assertion carries a re-typed kinematic constant; every expected value is computed from the loaded `ScenarioConfig`.

**Dependencies:** none — the committed `default.yaml` is its only input. **Parallel** with everything in group 1.6. **Commit:** `[11.1.13.1] test: retune the stream-difference assertions to the R22 scenario geometry`

**Status:** done 2026-08-04 — commit `9521d76` on `feat/phase1-bench-run-timing`. Suite 116 passed, 7 skipped locally (the count [phase1-ci.yml](../.github/workflows/phase1-ci.yml) documents without `ENCODER_PATH`). The intent/derivation split is mutation-verified: editing `initial_distance_m` in the YAML fails `test_config.py`'s geometry test and nothing else.

### [ ] `22.1.13.4` — Set `start_delay_s` in `scenarios/default.yaml` from the measured warm-up *(agent)*

**Objective:** cancel the ADA detector's warm-up so bench scenario time equals clip time — the one value R22's alignment budget is written against ([D7](../documents/Design/SCENARIO-PLAYER/scenario-player-design-decisions.md#d7--the-demo-cycle-is-one-clip-length-and-its-geometry-is-solved-backwards-from-the-first-warning); [§6.6(g)](../documents/Requirements/m1-run-timing-and-event-triggering.md)).

**Scope — one key in one data file. No code, no other scenario:**

- `Scenario_Player/scenarios/default.yaml` gains `start_delay_s: <W>`, where `<W>` is the **deployed** detector warm-up in seconds that Phase 3 `22.3.6.3` recorded in `plans/doc/phase3-ada-detector-run.md`. `DETECTOR_START_DELAY_S` stays `0.0`, so `start_delay_s = W` with no further term.
- The header comment's line stating the key "arrives with R20 and is not read by `player/config.py`" is replaced by the value's source: the run doc it came from and the −0.5 / +1.1 s band it must hold within.
- **`c-out-of-range.yaml` is not touched.** It admits no track, so no choreography instant exists in it to align.
- **Do not invent the number.** If `22.3.6.3` has not produced it, this subtask is blocked, not estimated — a wrong `start_delay_s` is wrong for every cycle of the run, and matched bench/clip periods make the offset constant rather than self-correcting.
- Extend `Scenario_Player/tests/test_config.py`: `default.yaml` loads with `start_delay_s` equal to the committed value and inside `[0, duration_s)`.

**Acceptance:** pytest green locally and on CI `python-tests`; the committed value equals `22.3.6.3`'s recorded `W` to one decimal, and the run doc it came from is named in the commit body.

**Dependencies:** after `11.1.6.9` (the loader must accept the key) **and Phase 3 `22.3.6.3`** (the measurement). Sequential-last in this group. **Commit:** `[22.1.13.4] feat: set the bench start delay to the measured detector warm-up`

**Retired IDs in this group, never reused:** `20.1.13.2` and `20.1.13.3`. The scenario keys and the deadline-scheduled `mono_ms` loop they covered are `11.1.6.9`, `11.1.6.11` and `11.1.6.12`, in the group that owns those files.

---

## Execution order & parallelism

Dependencies are real — files, frozen interfaces, CI lanes — and never a default assumption. At run time everything executes sequentially in one working tree (§ Subtask discipline); the structure below is the logical one. The two node folders are parallel tracks, sharing only the frozen contracts and the synced codec sources; `milestone1.md §3` names them tracks too.

```
Docs      5.1.11.1                                    (anytime, main)
CI-first  11.1.8.1 ∥ 5.1.8.2 ∥ 7.1.3.5               (guarded jobs/gate — land before their consumers)
Shared    11.1.1.1 ──────────────────────────────────► 11.1.1.2 (after every copy-landing subtask, so after 11.1.7.2)

Track V (V2X_ECU)
  foundation:  8.1.2.1 ∥ 7.1.2.2 ∥ 18.1.2.3 ; 2.1.2.4 after 7.1.2.2
  seam/stub:   7.1.3.1 ──► 8.1.3.2 ──► 8.1.3.3 ──► 7.1.3.4 (also needs 7.1.2.2) ; 7.1.3.6 after 7.1.3.1
  pipeline:    9.1.4.1 ∥ 9.1.4.2 ∥ 9.1.4.3 ──► 9.1.4.4 (needs 18.1.2.3) ──► 9.1.4.5
  assembly:    8.1.5.1 (needs 8.1.2.1 + 2.1.2.4 + 7.1.3.4 + 9.1.4.4) ; 6.1.5.2 ∥ 6.1.5.3 anytime
  image:       5.1.5.4 (needs 8.1.5.1 + 6.1.5.2 + 11.1.1.1 + 5.1.8.2)

Track P (Scenario_Player)
  app:         11.1.6.1 ──► 11.1.6.2 ∥ 11.1.6.3 ──► 11.1.6.4 ; 11.1.6.5 ∥ 11.1.6.6 anytime ; 11.1.6.7 after 11.1.6.3
               11.1.6.8 after 11.1.6.1/3/5/6/7
  clock (D5):  11.1.6.9 (after 11.1.6.1) ──► 11.1.6.10 ; 11.1.6.9 + 11.1.6.7 ──► 11.1.6.11 ──► 11.1.6.12
  codec path:  11.1.7.1 (needs 11.1.1.1 + 11.1.8.1) ──► 11.1.7.2
  image:       5.1.7.3 (needs 11.1.7.1 + 11.1.6.8 + 5.1.8.2)

Sink       2.1.9.1                                    (anytime before 5.1.10.1)
D7 check   6.1.12.1 (anytime) ∥ 9.1.12.2 (after 18.1.2.3) ──► 9.1.12.3 (after 8.1.5.1 + both scripts)

Track D (deploy — never blocks code)
  5.1.10.1 (needs 5.1.5.4 + 5.1.7.3 + 2.1.9.1) ──► 5.1.10.2 (Human) ──► 5.1.10.6 (AI)
    5.1.10.6 ──► 2.1.10.3 ──► 11.1.10.9 (Human) ──► 11.1.10.4
    5.1.10.6 ──► 9.1.10.7 (also needs 9.1.12.2) ∥ 8.1.10.8 ∥ 6.1.10.5

Track T (R22 demo cycle — group 1.13, independent of tracks V, D)
  11.1.13.1 (no dependency — the committed default.yaml is its only input)
  22.1.13.4 (after 11.1.6.9 + phase-3 22.3.6.3 — blocked on a measurement, not on code)
```

**Recommended runtime order (single tree):** 5.1.11.1 → 11.1.8.1 → 5.1.8.2 → 7.1.3.5 → 11.1.1.1 → 8.1.2.1 → 7.1.2.2 → 18.1.2.3 → 2.1.2.4 → 7.1.3.1 → 7.1.3.6 → 8.1.3.2 → 8.1.3.3 → 7.1.3.4 → 9.1.4.1 → 9.1.4.2 → 9.1.4.3 → 9.1.4.4 → 9.1.4.5 → 8.1.5.1 → 6.1.12.1 → 9.1.12.2 → 9.1.12.3 → 6.1.5.2 → 6.1.5.3 → 5.1.5.4 → 11.1.6.1 → 11.1.6.2 → 11.1.6.3 → 11.1.6.4 → 11.1.6.5 → 11.1.6.6 → 11.1.6.7 → 11.1.6.8 → 11.1.13.1 → 11.1.6.9 → 11.1.6.10 → 11.1.6.11 → 11.1.6.12 → 11.1.7.1 → 11.1.7.2 → 5.1.7.3 → 11.1.1.2 → 2.1.9.1 → group 1.10 when unblocked → 22.1.13.4 when Phase 3 `22.3.6.3` has the number.

## Acceptance traceability

| Milestone Phase 1 box | Closed by |
|---|---|
| Blueprint deploys; every node Running; team APK launches (R5) | 5.1.5.4 · 5.1.7.3 · 5.1.8.2 · 5.1.10.1 · 5.1.10.2 · 5.1.10.6 — APK clause open, § Open items item 2 |
| UDP reachability + traffic captured on the bridge (R6) | reachability: Phase 0 smoke test (C1–C5); capture: 6.1.5.2 · 6.1.5.3 · 6.1.10.5 |
| CI import check; telux parity notes + port plan (R7) | 7.1.3.5 (gate) · 7.1.3.6 (doc) · 7.1.3.1 · 7.1.2.2 · 7.1.3.4 (seam substance) |
| Scripted call flow acked/logged; faults → defined logged recovery (R8) | 8.1.3.2 · 8.1.3.3 (unit closure) · 8.1.2.1 · 8.1.5.1 · live evidence in 8.1.10.8 |
| Golden vectors decode; malformed corpus rejected, zero crashes (R9) | golden: Phase 0 `1.0.2.5` + 9.1.4.4; malformed: 9.1.4.5; stages: 9.1.4.1–3 |
| Different scenario configs → observably different streams (R11) | 11.1.6.2 · 11.1.6.4 (model) · 11.1.10.9 · 11.1.10.4 (live) — via codec path 11.1.7.1/11.1.7.2 |
| R2 at the ADA ECU with decoded bench values (R2) | 9.1.4.3 · 2.1.2.4 · 2.1.9.1 · 2.1.10.3 |
| **Demo:** Wireshark capture at the V2X interface | 6.1.5.2 · 6.1.5.3 · 6.1.10.5 (D5 dissection caveat noted) |

Four plan-tracked deliverables carry no milestone box of their own:

| Deliverable | Delivered by |
|---|---|
| HLD D7 — scripted send/capture bench↔V2X; logs demonstrate receive → event → CPM-to-JSON | 6.1.12.1 · 9.1.12.2 · 9.1.12.3 (CI) · 18.1.2.3 (payload-carrying events) · 9.1.10.7 (on-platform) |
| R18 evidence stream starts | 18.1.2.3 · `[EVT]` emission 9.1.4.4/8.1.5.1 · bench `[TX]` 11.1.6.7 · `mono_ms` 11.1.6.12 |
| R20's bench half — scenario time at 1.0× wall time, K5 within ±1 % | 11.1.6.11 (deadline scheduling) · 11.1.6.12 (`mono_ms`) · 11.1.6.9 (its keys) — verified by phase-4 `21.4.3.4` |
| R22's bench half — the 10.0 s cycle aligned to the clip | the committed `scenarios/default.yaml` geometry · 22.1.13.4 (`start_delay_s` from the measured warm-up) · 11.1.13.1 (its model-level test) |

## Open items & flags (no Phase 1 subtask may silently close them)

| # | Item | Owner / closes at |
|---|---|---|
| 1 | Three defaults await user ratification: `INIT_RETRY_MAX=3`, `RETRY_BACKOFF_MS=500`, `DEDUPE_WINDOW_MS=1500` ([V2X HLD §6](../documents/Design/V2X-ECU/v2x-ecu-hld.md#6-internal-components) states them as design values). They are externalized as env either way, and the `v2x-comms-check` lane overrides `DEDUPE_WINDOW_MS=1` for its back-to-back golden sends, which proves the externalization rather than the values. The HLD carries no "unratified" marker for them, so architecture is asked to add one or to record the values as ratified | **user** ratifies; [[project-architecture]] records which |
| 2 | R5 box clause "the team APK launches on the AAOS node": neither Phase 1 HLD covers IVI work and the deploy keeps the provided AAOS artifact — needs a ruling (build+install the Phase 0 IVI skeleton APK manually, or defer the clause to Phase 5) | user / [[project-architecture]] |
| 3 | arm64 image builds compile Vanetza under QEMU emulation in both C++-bearing image lanes. They complete within `timeout-minutes: 360`, so no cross-compilation is needed; the buildx `gha` cache scopes are per-image and leave the `actions/cache` `_deps` entries intact | **closed** by CI run `30698630956` |
| 4 | The phase's registry push is executed by the image CI lanes' secret-gated push step | **closed** by CI run `30698630956` |
| 5 | ~~Planner-designated test paths beyond the HLDs' explicit test lists~~ — closed: both HLD §4 trees now designate every test file this phase creates ([V2X HLD §4](../documents/Design/V2X-ECU/v2x-ecu-hld.md#4-folder-structure), [SP HLD §4](../documents/Design/SCENARIO-PLAYER/scenario-player-hld.md#4-folder-structure)) | **closed** — no planner-designated path remains |
| 6 | The registry host is environment-scoped and is re-derived per round, not assumed to be `hackathon-2` | **closed** — [[project-architecture]] |
| 7 | Smoke-test O3 (bridge MTU headroom) is open — the nominal CPM is 58 bytes, so it carries no Phase 1 risk; an optional `PAD` probe may ride a group 1.10 run ([M11](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m11--optional-mtu-headroom)) | group 1.10 (optional) |
| 8 | **R9 "malformed" scope.** Under the frozen [contracts/r1-cpm-profile.md](../contracts/r1-cpm-profile.md) §3, `protocolVersion` and `messageId` are ignored on decode and `CpmContent` carries no header field. `wrong-message-id`, `wrong-protocol-version` and `r1-variant` are therefore valid-but-different-header inputs rather than malformed ones, and R9's "fully rejected" governs structurally invalid input. No frozen contract changes and no requirement is re-worded. `9.1.4.5` asserts an explicit expected `Disposition` for each of its ten fixtures | **closed** by the [[project-architecture]] ruling and CI run 30700052056, which measures all 10 dispositions |
| 9 | The SP D5 scenario clock is planned work, not an open decision: [scenario-player-design-decisions.md D5](../documents/Design/SCENARIO-PLAYER/scenario-player-design-decisions.md) and [SP HLD §6](../documents/Design/SCENARIO-PLAYER/scenario-player-hld.md#6-internal-components), [§10](../documents/Design/SCENARIO-PLAYER/scenario-player-hld.md#10-the-contract--r1-the-message-set-this-node-produces) and [§12](../documents/Design/SCENARIO-PLAYER/scenario-player-hld.md#12-test-strategy) mandate the `start_delay_s` and `reference_time_epoch` keys, the `TimestampIts` epoch stamp, `CLOCK_MONOTONIC` deadline pacing, and `mono_ms` on the `[TX]` line. The HLD outranks the plan, so all four are scheduled | `11.1.6.9` · `11.1.6.10` · `11.1.6.11` · `11.1.6.12` |
| 10 | `measurementDeltaTime` is always 0 on the wire, because the generator never passes the third argument to `Scenario.sample` ([m1-run-timing-and-event-triggering.md §6.5(c)](../documents/Requirements/m1-run-timing-and-event-triggering.md)). It is inside the F9 bound and changes no M1 behaviour, and no HLD or frozen contract requires a non-zero value, so it stays a user decision | **user** |
| 11 | **The `[EVT] ready` line of the §4.2 B-1 readiness pick is unscheduled, on every node.** [§4.2](../documents/Requirements/m1-run-timing-and-event-triggering.md)'s selected mechanism is R5's Deployment-Viewer check **plus one `[EVT] ready` line per node** plus the bench `start_delay_s`. Two of the three are covered — R5's check by group 1.10, `start_delay_s` by `11.1.6.9` and `22.1.13.4` — and the ready line is written by no node. It gates no acceptance box, because the operator reads the Deployment Viewer rather than the log line. **Visible and unscheduled, not absorbed** | **user** |

---

*This plan decomposes [milestone1_high_level_plan.md § Phase 1](../documents/Plan/milestone1_high_level_plan.md#phase-1--comms-bring-up-v2x-ecu--scenario-player-r5r9-r11--r10-moved-to-the-future-plan) against the two Phase 1 HLDs, with group 1.13 decomposed from [m1-run-timing-and-event-triggering.md §6.6](../documents/Requirements/m1-run-timing-and-event-triggering.md) and [SP D7](../documents/Design/SCENARIO-PLAYER/scenario-player-design-decisions.md#d7--the-demo-cycle-is-one-clip-length-and-its-geometry-is-solved-backwards-from-the-first-warning). 13 task groups, 54 subtasks: 45 are agent-implemented, and group 1.10's 9 carry the walkthrough's AI and Human labels. 40 are closed against three CI runs — `30697863324` on `16b8674`, `30698630956` on `7a02fb5`, and `30700052056` on `31d0347`. The 14 open are group 1.10's deploy and verification work, group 1.6's scenario-clock subtasks and group 1.13's two — § Remaining work. Retired IDs, never reused: `20.1.13.2`, `20.1.13.3`.*
