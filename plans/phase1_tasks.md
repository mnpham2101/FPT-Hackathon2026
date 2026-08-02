# Phase 1 — Comms Bring-up (V2X ECU + Scenario Player): Full Task Breakdown

> **Authority & context:**
> - **Phase content:** [milestone1.md § Phase 1](milestone1.md#phase-1--comms-bring-up-v2x-ecu--scenario-player-r5r9-r11--r10-moved-to-the-future-plan) — its eight acceptance checkboxes are the phase output.
> - **Design (V2X ECU):** [phase1-v2x-ecu-comms-hld.md](../V2X_ECU/doc/phase1-v2x-ecu-comms-hld.md) (commits `3d0c655` + `f823d08` + `dda1566`) — decisions D1–D7 (D4 amended: payload-carrying events; D7: bench↔V2X comms check), §4 folder map, §6 env table, §9 deployment shape. Every V2X_ECU path below is cited from its §4; the D7 script pair lives at repo-root `tools/comms_check/` (user-mandated location, netcheck precedent).
> - **Design (Scenario Player):** [phase1-scenario-player-hld.md](../Scenario_Player/doc/phase1-scenario-player-hld.md) (commits `dc16c81` + `03805dc`) — decisions D1–D4, §3 folder map, §5 config. Every Scenario_Player path below is cited from its §3.
> - **Requirements:** [m1-cooperative-awareness.md §2](../requirements/m1-cooperative-awareness.md) R2, R5–R9, R11, R18 — referenced by number, never restated. **R10 is deferred**: the R7 seam declares `send`, nothing calls it, no subtask implements it.
> - **Phase 0 baseline (do not re-plan):** [phase0_tasks.md](phase0_tasks.md) § Output — contracts frozen, codec seam + R2 binding + golden vectors + `check_sync.py` landed, smoke test C1–C5 green on `trial2_minh`, CI lanes live.
> - **Deploy guides:** [node-v2x-ecu.md](../requirements/car-sky-guide/node-v2x-ecu.md) and [traffic-capture-wireshark.md](../requirements/car-sky-guide/traffic-capture-wireshark.md) already carry the Phase 1 shape; [node-scenario-player.md](../requirements/car-sky-guide/node-scenario-player.md) is unchanged by design (SP HLD §8).
> - **Rules:** [task-planning-conventions.md](../.claude/rules/task-planning-conventions.md) (`X.Y.Z.W`; subtask discipline restated once in § Subtask discipline below).
>
> **Task ID legend:** `X.1.Z.W` — X = requirement served · 1 = this phase · Z = task group · W = subtask position within the group. IDs are stable; never renumber.

## Phase 1 overview

**Objective.** Bench CPMs reach the deployed Room and decode into R2 messages at the ADA ECU — receive-only, ego broadcasts nothing. Deliverables: the V2X ECU application (R7 adapter seam, R8 modem-stub FSM + fault injection, R9 Rx pipeline, R18 JSONL event stream, R6 tcpdump capture), the Scenario Player application (R11 scenario-configurable CPM generation over the D1 `cpm_encode` helper), both node images (R5), the D7 bench↔V2X comms-check script pair + CI lane, and the live deploy + verification with the netcheck sink on the ADA node (D6).

**Input (must exist before start — all present as of 2026-08-01):**

- Phase 0 complete 4/4 ([phase0_tasks.md § Output](phase0_tasks.md#phase-0-overview)): `contracts/` frozen with golden vectors + `sync-manifest.json` + `check_sync.py`; `V2X_ECU/` codec seam `src/codec/`, R2 binding `src/contracts/`, `CMakeLists.txt` baseline; `Scenario_Player/player/contracts/cpm_content.py` + golden `.json` fixtures; `tools/netcheck/` deployed-proven.
- Both Phase 1 HLDs committed (header above); the SP HLD closes the bench→codec open item (D1).
- `.github/workflows/phase0-ci.yml` lanes: `contracts-gate`, `python-tests`, `v2x-core-build` (Vanetza `_deps` cached), `ada-core-build`, `ivi-unit-tests`, `netcheck-image` (arm64 build, push gated on `CARSKY_ZOT_API_KEY`).

**Output (phase acceptance = the eight milestone boxes):**

- [ ] Blueprint deploys to a Room; Deployment Viewer shows every node Running; the team APK launches on the AAOS node (R5) — **partly:** the image-build half is met — both node images build for `linux/arm64` (CI run 30698630956, `v2x-ecu-image` + `scenario-player-image` green), closing `5.1.5.4`/`5.1.7.3`/`5.1.8.2`. A build is not a deploy, so the box stays open on its own wording: it still needs the registry push (`5.1.10.1`), the blueprint deploy with the Deployment Viewer showing every node Running (`5.1.10.2`), and a ruling on the APK clause. *APK clause still unowned, § Open items item 2.*
- [ ] UDP reachability between every communicating pair; traffic captured on the bridge network (R6). — **partly:** reachability closed by the Phase 0 smoke test (C1–C5); the capture pair `6.1.5.2`/`6.1.5.3` is syntax- and round-trip-verified, but **no capture has run on a bridge** — needs `6.1.10.5`.
- [x] CI import check passes — no direct transport imports above the seam; telux parity notes + port plan committed (R7). — **closed** by `7.1.3.5` (gate green in CI run 30697863324, `contracts-gate`) + `7.1.3.6` (doc committed, signatures character-identical to the frozen seam).
- [x] The full scripted call flow is acked and logged; each injected fault produces a defined, logged recovery (R8). — **closed at unit + loopback level:** `8.1.3.2`/`8.1.3.3` prove every ack, illegal-order rejection, and D2 recovery on CI (`v2x-core-build`, run 30697863324), and the `v2x-comms-check` lane runs the real `init → configure → subscribeRx` bring-up inside `v2x_ecu`. **Not proven:** fault injection against a deployed node — optional supplementary evidence in `2.1.10.3`.
- [x] Golden-vector CPMs decode correctly; the malformed-input corpus is fully rejected with zero crashes (R9). — **closed:** golden decode and zero crashes are **CI-proven** (Phase 0 `1.0.2.5` + `9.1.4.4`; `9.1.4.5` drives the corpus through the **real** Vanetza codec, green in run 30697863324). *Malformed* is settled by the architecture ruling of 2026-08-01 (§ Open items item 8) to mean **structurally invalid** input, and the amended `9.1.4.5` corpus is categorised accordingly: 6 structurally-malformed cases dispositioned `Reject` (`empty`, `truncated-nominal`, `truncated-mid-object`, `bit-flipped-payload`, `random`, `oversized`) and 4 valid-but-different inputs dispositioned `ToleratedControl` — the 3 header edits, which profile §3 freezes as ignored on decode ([contracts/r1-cpm-profile.md](../contracts/r1-cpm-profile.md) §3), plus `trailing-garbage`. Those 4 are **correctly-tolerated negative controls, not failures**. **Closed 2026-08-01:** the predicted dispositions were confirmed empirically — `v2x-core-build` green in run **30700052056** on `31d0347` runs the amended suite, which asserts each case's exact disposition (no "either outcome passes"), so a green lane means all 6 `Reject` cases rejected and all 4 controls decoded as the profile mandates, with zero crashes and no `unexpected_exception:` in the log.
- [ ] Different bench scenario configurations produce observably different message streams (R11). — **partly:** the model half is closed by `11.1.6.4` and the byte-level codec path by `11.1.7.2` (`sp-codec-helper` green in run 30697863324); the live config-swap half needs `11.1.10.4`.
- [ ] R2 messages observed at the ADA ECU carrying decoded bench-scenario values, not constants (R2). — **partly:** the `v2x-comms-check` lane observes R2 JSON carrying real decoded golden-vector values, but at a **loopback UDP sink standing in for the ADA node**, not the deployed ADA ECU — that needs `2.1.10.3`.
- [ ] **Demo:** Wireshark capture of V2X PDUs correctly sent/received at the V2X ECU interface. — **open:** needs `6.1.10.5` on a deployed Room.
- [x] Scripted send/capture between bench and V2X ECU passes; V2X `[EVT]` logs demonstrate message receive (`rx_datagram`), event raised (`decode_ok` with decoded CpmContent JSON), and CPM deserialized to JSON (`r2_forwarded` with the R2 body) — per HLD D7. — **closed** by the `v2x-comms-check` lane in CI run 30697863324: ≥ 6 datagrams received and the full `rx_datagram` → `decode_ok` (CpmContent JSON) → `r2_forwarded` (R2 JSON) chain asserted, `v2x_ecu` exiting 0 on SIGTERM. Not vacuous: `check_v2x_log.py` discriminates — removing the `r2_forwarded` event fails at the forward link and stripping `decode_ok`'s `cpm` payload fails at the decode link (exit 1 each), while the intact chain exits 0.

**Suggested branch (suggestion only — creation is the orchestrator/user's call):** `feat/phase1-comms-bringup` — one branch for the whole phase; implementation subtasks commit onto it. Docs-only subtasks (this plan file, `5.1.11.1`, group 1.10 evidence records) follow the repo convention of committing straight to `main`.

**Phase 1 acceptance state 2026-08-01: 3 of the 9 boxes closed (R7, R8, D7), 5 partly closed (R5, R6, R9, R11, R2), 1 open (Demo).** Every unclosed clause now needs a live Room — § Remaining work.

### Remaining work

**40 of 44 subtasks are closed. The 4 below are all USER-MANUAL — nothing remaining is agent work, CI work, or new code.**

Three CI runs verify everything else: **`30697863324` on `16b8674`** (8 lanes green, the phase's code), **`30698630956` on `7a02fb5`** (10 lanes green, adding both node-image lanes — both images build for `linux/arm64` and push), and **`30700052056` on `31d0347`** (10 lanes green, confirming the amended `9.1.4.5` dispositions — the last CI gap, now closed). `5.1.10.1` closed too, executed by the CI push step rather than [[car-sky]].

| Subtask | Performed by | What it still needs |
|---|---|---|
| `5.1.10.2` | **user** (Nydus UI) | Confirm **per-node** `Running` badges + restart 0 in the Deployment Viewer — not the summary header, which read `Pending — 0/0 nodes ready` while traffic flowed normally. Node config per § Task Group 1.10 is already applied on `phase1_Minh_test-deploy`. |
| `2.1.10.3` | **user** (Nydus UI) | Two leftovers only — the R2-at-ADA half is closed live: capture the `[EVT] stub_transition` bring-up sequence (prints once at node start, scrolled away), and run `tools/comms_check/check_v2x_log.py` in stream mode over a saved View Log export. |
| `11.1.10.4` | **user** (Nydus UI) | Swap the bench `SCENARIO_CONFIG` to `c-out-of-range.yaml`, redeploy, and compare the two log sets against the `default.yaml` baseline in [phase1-comms-run.md](doc/phase1-comms-run.md). |
| `6.1.10.5` | **user** (Nydus UI) | Save a View Log containing a `[PCAP-BEGIN]` block (the Room must have run at least one `CAPTURE_ROTATE_S` period), run `V2X_ECU/tools/extract_pcap.sh`, open the `.pcap` in Wireshark. |

**Sequencing:** the last three all read the V2X node's log, so **one restart plus one sufficiently long log download serves all three**.

**Standing hazard, not a residual:** all three image tags are mutable, so whichever branch pushes last wins the tag — identify a deployed image at deploy time, never from an old run log.

### Execution split legend

| Label | Meaning |
|---|---|
| *agent* | implemented by a spawned implementation subagent (default) |
| *car-sky* | planned for the [[car-sky]] agent (deploy preflight → build/push/deploy/verify); planner keeps the ID and done-tracking. **The agent was never spawnable; the phase's one *car-sky* subtask (`5.1.10.1`) was executed by the CI push step instead — § Open items item 4** |
| *USER-MANUAL* | Nydus UI steps performed by the user; the plan tracks them; the evidence-record commit is made by the orchestrating session after the user confirms |

**Implementation-subagent specification** (inherited by every *agent* subtask): general-purpose agent; tools Read/Grep/Glob/Write/Edit/Bash; writes ONLY inside the node folder its subtask names (plus its own `**Status:**` line in this file and, where the subtask explicitly says so, `contracts/`, `tools/netcheck/`, `tools/comms_check/`, or `.github/workflows/`); reads the target folder's `doc/` first; inherits § Subtask discipline as its definition of done; makes the atomic commit itself with the exact commit message from the brief plus trailer `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`; never pushes — the orchestrator pushes and watches CI. Language best practice is part of done: C++17 core guidelines / RAII / no raw owning pointers; Python type hints + dataclasses + no globals; tests deterministic.

### Subtask discipline (applies to every subtask below)

Per [task-planning-conventions.md § Subtask discipline](../.claude/rules/task-planning-conventions.md#subtask-discipline-non-negotiable): single objective, no out-of-scope code, exactly one atomic commit with the stated message, build passes, unit tests pass, brief self-contained. Hard execution constraints baked in:

- **Dev host is Windows with no Docker/WSL.** C++ verification (V2X_ECU app, `codec_helper`) and image builds run on GitHub Actions — a C++ subtask's build/tests acceptance = **CI green on the pushed branch** (same model as Phase 0). Python subtasks verify locally with pytest **and** on CI (`python-tests`).
- **Local tests never require CI-only artifacts:** `test_encoder_golden.py` skips (`pytest.mark.skipif`) when the `cpm_encode` binary is absent locally; CI builds the helper then runs it unskipped.
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

> One codec source, two build contexts (SP HLD D2). The fragment is the single home of the Vanetza tag + ASN.1-only option set; the manifest extension is the drift gate over the new copies and runs **sequential-last** like Phase 0's `1.0.7.1`.

### [x] `11.1.1.1` — Extract `contracts/vanetza-pin.cmake` and switch V2X_ECU onto it *(agent)*

**Objective:** one shared Vanetza pin fragment, master at `contracts/vanetza-pin.cmake`, consumed by `V2X_ECU/CMakeLists.txt` via the byte-synced copy `V2X_ECU/cmake/vanetza-pin.cmake`.

**Scope:**

- Author `contracts/vanetza-pin.cmake` by extracting from `V2X_ECU/CMakeLists.txt` (lines it currently holds inline): the `FetchContent_Declare(vanetza … GIT_TAG fb6c551030dcc12b924299bf401e35e5fe814713 … EXCLUDE_FROM_ALL)` block and the five forced `VANETZA_*`/`BUILD_TESTS` option `set(… CACHE BOOL "" FORCE)` lines, plus a `VANETZA_ASN1_TARGETS` variable naming `Vanetza::asn1;Vanetza::asn1_its_r2`. The fragment declares only — consumers call `FetchContent_MakeAvailable`.
- Copy byte-identical to `V2X_ECU/cmake/vanetza-pin.cmake`; edit `V2X_ECU/CMakeLists.txt` to `include(cmake/vanetza-pin.cmake)` in place of the extracted block. No other CMake change.
- `.github/workflows/phase0-ci.yml` (explicitly in write scope for this subtask): the `v2x-core-build` cache key currently hashes only `V2X_ECU/CMakeLists.txt` — extend `hashFiles(…)` to also hash `V2X_ECU/cmake/vanetza-pin.cmake` so a re-pin invalidates the `_deps` cache.

**Acceptance:** V2X build command green on CI (`v2x-core-build`, all existing tests unchanged); the two fragment files byte-identical (`cmp`); cache key carries both hashes.

**Dependencies:** none — starts immediately. **Commit:** `[11.1.1.1] chore: extract shared vanetza-pin.cmake fragment and switch V2X_ECU to it`

**Status:** implemented 2026-08-01 — cmp identical (`contracts/vanetza-pin.cmake` ≡ `V2X_ECU/cmake/vanetza-pin.cmake`), extraction lossless by diff (declare block + 5 forced options moved verbatim, replaced by one `include()`), cache key hashes both files, check_sync + import gate exit 0; CI verification pending wave push. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `11.1.1.2` — Extend `contracts/sync-manifest.json` with the D2 entries *(agent — sequential-last of the contract work)*

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

> The transport-blind foundation modules of [V2X HLD §4](../V2X_ECU/doc/phase1-v2x-ecu-comms-hld.md#4-folder-structure-map--file-location-designations). All paths inside `V2X_ECU/`; build/test = V2X row of § Per-node build commands (CI `v2x-core-build`). Test-file paths here beyond the HLD's list are planner-designated per the HLD's `tests/<module>/` pattern (§ Open items item 5).

### [x] `8.1.2.1` — Env config loader `src/config/config.{hpp,cpp}` *(agent)*

**Objective:** the app's **only env reader** (HLD §4): load + validate the §6 app-consumed env set into an immutable `Config` struct.

**Scope:**

- Fields + defaults exactly per [HLD §6](../V2X_ECU/doc/phase1-v2x-ecu-comms-hld.md#6-configuration-no-hardcoded-tunables--every-value-env-injected-by-the-blueprint): `LISTEN_PORT` (47100), `ADA_ECU_HOST`/`ADA_ECU_PORT` (10.99.0.12/47200), `FAULT_PLAN` (`none·init_fail·configure_reject·subscription_drop`, default `none`), `INIT_RETRY_MAX` (3), `RETRY_BACKOFF_MS` (500), `DEDUPE_WINDOW_MS` (1500), `EVENT_LOG_PATH` (empty = stdout only). The three *(proposal)* defaults proceed as proposed — user ratification stays open (§ Open items item 1). `CAPTURE_*`/`PCAP_DIR` are consumed by `capture.sh` directly, not by this loader.
- Validation: ports 1–65535, non-empty host, `FAULT_PLAN` enum, non-negative retry ceiling, positive backoff/window; invalid value → descriptive error (exception), caller exits non-zero. Env read via an injectable getter so tests never mutate process env.
- Test `tests/config/test_config.cpp`: defaults when unset; each override parsed; each rejection case.

**Acceptance:** V2X build + ctest green on CI; no literal tunable outside the defaults table in this one file.

**Dependencies:** none. **Commit:** `[8.1.2.1] feat: add V2X ECU env config loader`

**Status:** implemented 2026-08-01 — `src/config/config.{hpp,cpp}` + `tests/config/test_config.cpp` added, `v2x_config` static lib + `v2x_config_test` registered in `V2X_ECU/CMakeLists.txt`, transport-import and contract-sync gates exit 0; CI verification pending wave push. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `7.1.2.2` — Sole socket holder `src/net/udp_socket.{hpp,cpp}` *(agent)*

**Objective:** `net::UdpSocket` — the **only** V2X_ECU code allowed to include socket headers (HLD D1).

**Scope:** RAII fd ownership (move-only, no raw owning handles); bind(port) + blocking `recvFrom(buffer)` + `sendTo(host, port, bytes)`; errors surface as typed results/exceptions, never `errno` leaks to callers. Test `tests/net/test_udp_socket.cpp`: loopback send→receive round-trip on an ephemeral port; bind-conflict error surfaces cleanly.

**Acceptance:** V2X build + ctest green on CI; socket headers (`<sys/socket.h>`, `<netinet/*>`, `<arpa/*>`) appear only under `src/net/` (the 7.1.3.5 gate will enforce this permanently).

**Dependencies:** none — parallel with 8.1.2.1. **Commit:** `[7.1.2.2] feat: add UdpSocket sole transport holder`

**Status:** implemented 2026-08-01 — `src/net/udp_socket.{hpp,cpp}` + `tests/net/test_udp_socket.cpp` added (`v2x_net` static lib + `v2x_udp_socket_test` registered), RAII move-only fd with socket includes confined to the .cpp (header POSIX-free), transport-import and contract-sync gates exit 0; CI verification pending wave push. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `18.1.2.3` — R18 JSONL event log `src/log/event_log.{hpp,cpp}` *(agent)*

**Objective:** the R18 evidence-stream writer (HLD D4) — one JSONL line per event, `[EVT]`-prefixed.

**Scope:**

- Event vocabulary exactly D4: `rx_datagram`, `decode_ok`, `decode_reject`, `validate_reject`, `dedupe_drop`, `r2_forwarded`, `stub_transition`, `fault_injected`, `recovery` — each line carries event name, monotonic + epoch timestamps, and the current per-stage counters.
- **Payload-carrying events (D4 as amended 2026-08-01, consumed by D7):** `decode_ok` embeds the decoded `CpmContent` as JSON; `r2_forwarded` embeds the forwarded R2 JSON body — the `[EVT]` stream alone demonstrates receive → event raised → CPM deserialized to JSON. `check_v2x_log.py` (9.1.12.2) parses these fields, so their names freeze here.
- Sink: stdout always (flushed per line — CarSky View Log is the live window); additionally append to `EVENT_LOG_PATH` when non-empty. nlohmann/json for serialization (Phase 0 pin).
- Test `tests/log/test_event_log.cpp`: line shape (parseable JSON after the `[EVT]` prefix), counter accumulation, embedded `decode_ok`/`r2_forwarded` payloads present and parseable, file sink writes when path set (temp dir).

**Acceptance:** V2X build + ctest green on CI.

**Dependencies:** none — parallel with 8.1.2.1/7.1.2.2. **Commit:** `[18.1.2.3] feat: add R18 JSONL event log writer`

**Status:** implemented 2026-08-01 — `src/log/event_log.{hpp,cpp}` + `tests/log/test_event_log.cpp` added (`v2x_event_log` static lib + `v2x_event_log_test` registered); frozen field names `event`/`mono_ms`/`epoch_ms`/`counters` (+ `cpm` on decode_ok, `r2` on r2_forwarded) recorded in the header comment; transport-import and contract-sync gates exit 0; CI verification pending wave push. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `2.1.2.4` — ADA forwarder `src/forward/ada_forwarder.{hpp,cpp}` *(agent)*

**Objective:** the intra-ego R2 edge (HLD D1 — deliberately **not** under the R7 seam): serialize the Phase 0 `v2x::contracts::R2Message` to JSON and UDP-send to `ADA_ECU_HOST:ADA_ECU_PORT`.

**Scope:** consumes `net::UdpSocket` only (no socket headers here); one datagram per R2 message; send failure logged, never throws into the pipeline. Test `tests/forward/test_ada_forwarder.cpp`: loopback listener receives the JSON of the node-local sample `V2X_ECU/tests/fixtures/samples/r2-object.json` intact.

**Acceptance:** V2X build + ctest green on CI.

**Dependencies:** after 7.1.2.2. **Commit:** `[2.1.2.4] feat: add ADA forwarder for R2 JSON`

**Status:** implemented 2026-08-01 — `src/forward/ada_forwarder.{hpp,cpp}` + `tests/forward/test_ada_forwarder.cpp` added (`v2x_forward` static lib + `v2x_ada_forwarder_test` registered); consumes `net::UdpSocket` only, never-throws `send()` with bool result, one compact-JSON datagram per R2 message; transport-import and contract-sync gates exit 0; CI verification pending wave push. Closed: CI run 30697863324 green (`v2x-core-build`).

---

## Task Group 1.3 — R7 adapter seam + R8 modem stub (serves R7, R8)

> The seam-and-stub pair of HLD D1/D2. The seam mirrors the telux radio surface only; `send` is declared and returns `NotSupported` — R10-deferred, nothing calls it.

### [x] `7.1.3.1` — Freeze the seam: `src/adapter/i_radio_adapter.hpp` *(agent)*

**Objective:** the frozen R7 interface — `init() · configure(RadioConfig) · subscribeRx(RxCallback) · send(bytes)` with typed result codes (HLD D2).

**Scope:** header-only: `RadioConfig` (at minimum the Rx port), `RxCallback = std::function<void(const std::vector<uint8_t>&)>`, a result-code enum incl. `Ok` and `NotSupported`, and the pure-virtual `IRadioAdapter`. Names and call order must match what `doc/telux-parity-and-port-plan.md` (7.1.3.6) will document. Test `tests/adapter/test_i_radio_adapter.cpp`: a minimal fake implements the interface and a scripted `init→configure→subscribeRx` sequence compiles and runs — proves implementability, freezes signatures.

**Acceptance:** V2X build + ctest green on CI; interface text stable (later subtasks may not alter it without re-freezing).

**Dependencies:** none. **Commit:** `[7.1.3.1] feat: freeze IRadioAdapter seam interface`

**Status:** implemented 2026-08-01 — frozen header-only seam `src/adapter/i_radio_adapter.hpp` (`RadioConfig`/`RxCallback`/`RadioResult` + pure-virtual `IRadioAdapter`, `v2x_adapter_seam` INTERFACE target) with fake-impl test `tests/adapter/test_i_radio_adapter.cpp` registered as `v2x_i_radio_adapter_test`; transport-import and contract-sync gates both exit 0; CI verification pending wave push. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `8.1.3.2` — Modem stub FSM happy path `src/stub/modem_stub.{hpp,cpp}` *(agent)*

**Objective:** the R8 FSM `idle → initialized → configured → rx-subscribed`, acking each call (HLD D2) — happy path only.

**Scope:** pure logic, no sockets in this subtask; each transition acked with a typed result and reported to an injectable transition observer (main will wire it to `event_log` as `stub_transition`); illegal call order rejected (e.g. `configure` before `init`). Constructor takes plain params (fault plan enum + retry values), never reads env. Test `tests/stub/test_modem_stub_fsm.cpp`: full scripted call flow acked in order; every illegal-order rejection.

**Acceptance:** V2X build + ctest green on CI.

**Dependencies:** after 7.1.3.1 (uses `RadioConfig` + result codes). **Commit:** `[8.1.3.2] feat: implement modem stub FSM happy path`

**Status:** implemented 2026-08-01 — `v2x::stub::ModemStub` (`src/stub/modem_stub.{hpp,cpp}`, `v2x_stub` lib) with `tests/stub/test_modem_stub_fsm.cpp` (`v2x_modem_stub_fsm_test`) covering the happy path acked in order plus all 9 illegal-order rejections (failure code, state unchanged, observer notified); transport-import and contract-sync gates both exit 0; CI verification pending wave push. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `8.1.3.3` — Fault injection + defined recoveries in the stub *(agent)*

**Objective:** config-driven fault injection with the D2 recovery table — each injected fault produces a defined, observable recovery.

**Scope:**

- `FAULT_PLAN` values per HLD D2: `init_fail`/`configure_reject` → fail the call; recovery = retry with `RETRY_BACKOFF_MS` backoff up to `INIT_RETRY_MAX`, then report terminal failure (caller exits non-zero — container restart is the logged last-resort recovery). `subscription_drop` → drop the subscription after establishment; recovery = automatic re-`subscribeRx`, same backoff, unbounded. Drop + resubscribe + every retry surface through the transition observer as `fault_injected`/`recovery` events.
- Backoff waits use an injectable sleep/clock so tests run instantly and deterministically.
- Extend `tests/stub/test_modem_stub_fsm.cpp`: all four plans; retry-then-succeed; retry-exhaustion terminal path; unbounded resubscribe after drop.

**Acceptance:** V2X build + ctest green on CI — this is the unit-level closure of the R8 milestone box.

**Dependencies:** after 8.1.3.2. **Commit:** `[8.1.3.3] feat: add fault injection and recovery to modem stub`

**Status:** implemented 2026-08-01 — D2 recovery table in `src/stub/modem_stub.{hpp,cpp}` with the `EventKind` observer surface (Ack/Reject/FaultInjected/Recovery), injectable `Sleeper`, and `fault_fail_count` knob; `test_modem_stub_fsm.cpp` extended with all four plans, retry-then-succeed, retry-exhaustion terminal path, and unbounded resubscribe after drop; transport-import and contract-sync gates both exit 0; CI verification pending wave push. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `7.1.3.4` — Seam implementation `src/adapter/stub_radio_adapter.{hpp,cpp}` *(agent)*

**Objective:** `StubRadioAdapter : IRadioAdapter` over the modem stub, with the live Rx path: on `rx-subscribed` the stub side opens the `LISTEN_PORT` UDP socket (via `net::UdpSocket`) on a dedicated Rx thread and delivers each datagram to the subscribed callback (HLD D2).

**Scope:** thread lifetime RAII-managed (joined on destruction, no detached threads); `send` returns `NotSupported` and logs — R10-deferred, seam unchanged; where the socket/thread code lands between the two designated modules (`adapter/` vs `stub/`) is the implementer's call within HLD D2's wording. Test `tests/adapter/test_stub_radio_adapter.cpp`: loopback datagram sent to the bound port reaches the callback with identical bytes; `send` returns `NotSupported`; clean shutdown with no leak/hang (ephemeral port).

**Acceptance:** V2X build + ctest green on CI; no socket headers outside `src/net/`.

**Dependencies:** after 7.1.3.1 + 8.1.3.3 + 7.1.2.2. **Commit:** `[7.1.3.4] feat: implement StubRadioAdapter over the modem stub`

**Status:** implemented 2026-08-01 — `src/adapter/stub_radio_adapter.{hpp,cpp}` implements the frozen seam by delegating `init`/`configure`/`subscribeRx` to `ModemStub` verbatim (no adapter-side retry) and adds the live Rx path: one RAII-owned thread + `std::optional<net::UdpSocket>` bound to `stub.config().rx_port`, shutdown by atomic flag + join within the 200 ms `kRxPollTimeout` poll (no detach, no self-pipe), throwing consumers caught and logged, `send` → `NotSupported` with one logged line; `tests/adapter/test_stub_radio_adapter.cpp` covers loopback byte-identical delivery, repeat-subscribe rejection, throwing callback survival, prompt/idempotent `stop()` + port rebindable after destruction, and `InitFail`/`ConfigureReject` passthrough with no thread started; new `v2x_adapter` target (+`find_package(Threads)`); `check_transport_imports.py` and `contracts/check_sync.py` both exit 0; CI verification pending wave push. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `7.1.3.5` — R7 transport-import gate `tools/check_transport_imports.py` + CI step *(agent)*

**Objective:** the R7 acceptance check, made permanent (HLD D1): no direct transport imports above the seam.

**Scope:**

- Python 3 stdlib script: scan `V2X_ECU/src/**` excluding `src/net/`; exit 1 naming the file on any include of `<sys/socket.h>`, `<netinet/…>`, `<arpa/…>`, or `<asio…>`/`<boost/asio…>`; re-assert the F2 grep ban (bare `asn1::Cpm` under `V2X_ECU/src/`, same rule as `contracts/check_sync.py`).
- `.github/workflows/phase0-ci.yml` (explicitly in write scope): add one guarded step to the existing `contracts-gate` job running the script when it exists — same guard style as that job's `check_sync.py` step.

**Acceptance:** script exits 0 on the committed tree from repo root; a planted violation outside `src/net/` (unstaged) flips exit 1; CI `contracts-gate` green — closes the "CI import check passes" half of the R7 box.

**Dependencies:** none (passes trivially before 7.1.2.2 lands; binding once it does). **Commit:** `[7.1.3.5] feat: add transport-import CI gate`

**Status:** implemented 2026-08-01 — script exits 0 from repo root and an unrelated cwd; probes behaved (`<sys/socket.h>` include → exit 1 naming file:line, bare `asn1::Cpm` → exit 1, `vanetza::asn1::r2::Cpm` → exit 0), probe deleted and tree back to exit 0; guarded step added to CI `contracts-gate`; CI verification pending wave push. Closed: CI run 30697863324 green (`contracts-gate`, import-gate step).

### [x] `7.1.3.6` — Telux parity notes + port plan `doc/telux-parity-and-port-plan.md` *(agent)*

**Objective:** the committed R7 doc deliverable (HLD §4/§10): why the seam's names/call order mirror telux, and what porting to real modem hardware changes.

**Scope:** markdown per [markdown-writing-style](../.claude/skills/markdown-writing-style/SKILL.md): mapping table `IRadioAdapter` methods ↔ telux `Cv2x` radio API calls (init/configure/subscribe/send parity, call-order constraints); port plan — replace `StubRadioAdapter` with a telux-backed implementation, config/threading deltas, everything above the seam unchanged (the node's focus goal); `send` row marked R10-deferred. References the frozen header, never restates it.

**Acceptance:** doc committed at `V2X_ECU/doc/telux-parity-and-port-plan.md`; method names match `i_radio_adapter.hpp` exactly. Doc-only — no build target.

**Dependencies:** after 7.1.3.1. **Commit:** `[7.1.3.6] docs: author telux parity notes and port plan`

**Status:** done 2026-08-01 — doc committed; all four seam signatures verified character-identical to the frozen `i_radio_adapter.hpp`, `send` row marked R10-deferred; telux symbol names marked unconfirmed pending SDK headers (no API invented); links resolve. Closed — doc-only acceptance met (`64abd97`).

---

## Task Group 1.4 — R9 Rx pipeline: decode → validate → dedupe → forward (serves R9, with the R2 build stage)

> HLD D3 — four stages, each a unit-testable class; the pipeline runs synchronously on the Rx thread and is transport-blind (emits R2 via an injected sink callback; main wires the forwarder). Field/unit authority for the derivations: `contracts/r1-cpm-profile.md` (F1/F6/F7/F9) + the node-local synced schemas `V2X_ECU/contracts/r1-cpm-content.schema.json` and `V2X_ECU/contracts/r2-v2x-object.schema.json`.

### [x] `9.1.4.1` — Profile validator `src/pipeline/validator.{hpp,cpp}` *(agent)*

**Objective:** stage 2 — mandatory-field presence + profile-range validation of a decoded `CpmContent`, reject + count by reason.

**Scope:** range set = the wire-native bounds of `V2X_ECU/contracts/r1-cpm-content.schema.json` (stationId, lat/lon, orientation, coordinates, confidences 1..101) plus F9 `|measurementDeltaTime| ≤ 2047` — note the wire legally carries −2048 but F9 bans it (profile rule; decode alone won't reject it). Violations return a typed reject reason (enum) for counting; no logging inside the class. Bounds sourced from named constants mirroring the schema — contract bounds, not tunables. Test `tests/pipeline/test_validator.cpp`: nominal golden content passes; one case per reject reason incl. mdt = −2048.

**Acceptance:** V2X build + ctest green on CI.

**Dependencies:** none (Phase 0 seam suffices). **Commit:** `[9.1.4.1] feat: implement CPM profile validator`

**Status:** implemented 2026-08-01 — 10 reject reasons mirroring the schema bounds as named constants (stationId type-tight, out-of-range unconstructible post-decode), F9 −2048 rejected as `MdtF9Range`, both gates (`check_transport_imports.py`, `check_sync.py`) exit 0; CI verification pending wave push. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `9.1.4.2` — Deduper `src/pipeline/deduper.{hpp,cpp}` *(agent)*

**Objective:** stage 3 — duplicate drop over key `(stationId, objectId, referenceTime + measurementDeltaTime)` within a sliding window.

**Scope:** window length injected (ms — main passes `Config::DEDUPE_WINDOW_MS`, default 1500 *(proposal)*); injectable clock for deterministic tests; expired entries pruned. Test `tests/pipeline/test_deduper.cpp`: same key inside window drops; outside window passes; differing objectId/stationId/timestamp passes; pruning bounded.

**Acceptance:** V2X build + ctest green on CI; no literal window value outside tests.

**Dependencies:** none — parallel with 9.1.4.1. **Commit:** `[9.1.4.2] feat: implement Rx deduper`

**Status:** implemented 2026-08-01 — window + clock injected (no literal window outside tests; default stays in config.cpp), signed-sum key semantics tested (refTime 1000+5 collides with 995+10), pruning bounded via once-per-window sweep asserted by `size()`, transport-import + contract-sync gates exit 0; CI verification pending wave push. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `9.1.4.3` — R2 builder `src/pipeline/r2_builder.{hpp,cpp}` *(agent)*

**Objective:** stage 4a — map `CpmContent` (wire-native integers) → the Phase 0 `v2x::contracts::R2Message` (SI), owning **every** derivation the codec seam excludes (HLD D3).

**Scope:**

- F7: `object.distance = hypot(x, y)` in metres from the 0.01 m wire units — derived here, never transmitted.
- F6: confidence conversions per the profile doc — `ConfidenceLevel 101 → null` (`std::optional`), coordinate confidence to metres.
- F1: `sender.speed` derived from consecutive `referencePosition`/`referenceTime` deltas per `stationId` — nullable until the 2nd message; per-station state lives in the builder; document the planar small-delta approximation in code.
- Unit conversions: lat/lon 10⁻⁷ ° → °, orientation 0.1 ° → °, positions/velocities 0.01 → SI; `rxTime` stamped from a value passed in by the pipeline (receipt time), `object.timeOfMeasurement` from `referenceTime + measurementDeltaTime`.
- Test `tests/pipeline/test_r2_builder.cpp`: nominal golden content → values cross-checked against the node-local sample `tests/fixtures/samples/r2-object.json` (distance 25.03 = hypot(25.0, 1.2)); F6 101→null; F1 null-then-derived across two messages.

**Acceptance:** V2X build + ctest green on CI.

**Dependencies:** none — parallel with 9.1.4.1/9.1.4.2 (uses only Phase 0 seam + binding). **Commit:** `[9.1.4.3] feat: implement R2 builder with F1/F6/F7 derivations`

**Status:** implemented 2026-08-01 — F1 (per-station equirectangular speed, null until 2nd msg, Δt ≤ 0 guard) / F6 (101→null, /100 clamped, coord confidence ×0.01 m) / F7 (exact hypot, no rounding) per the profile; nominal golden → field-by-field cross-check against `samples/r2-object.json` in `v2x_r2_builder_test`; transport-import + contract-sync gates exit 0; CI verification pending wave push. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `9.1.4.4` — Pipeline composition `src/pipeline/rx_pipeline.{hpp,cpp}` *(agent)*

**Objective:** the four-stage synchronous pipeline: datagram bytes → `ICpmCodec::decode` → validator → deduper → r2_builder → injected R2 sink callback; every stage outcome emitted to `event_log`.

**Scope:** constructor injects `ICpmCodec&`, the three stage objects, `EventLog&`, and `std::function<void(const R2Message&)>` sink (transport-blind, HLD D1); emits D4 events `rx_datagram`/`decode_ok`/`decode_reject`/`validate_reject`/`dedupe_drop`/`r2_forwarded` with running counters; `DecodeError` → reject + count, never crash/throw out. Test `tests/pipeline/test_rx_pipeline.cpp` (planner-designated path): each golden `.uper` fixture from `tests/fixtures/golden/` flows through to the sink with correct R2 values; a duplicated datagram increments `dedupe_drop`; counters match.

**Acceptance:** V2X build + ctest green on CI.

**Dependencies:** after 9.1.4.1 + 9.1.4.2 + 9.1.4.3 + 18.1.2.3. **Commit:** `[9.1.4.4] feat: compose the four-stage Rx pipeline`

**Status:** implemented 2026-08-01 — `noexcept` four-stage composition (`onDatagram`) over caller-owned injected collaborators + `R2Sink`, whole-body `catch(...)` with documented single-reject attribution, counting left to `EventLog`; `v2x_rx_pipeline_test` drives a fake `ICpmCodec` through all 6 golden contents (F6/F7 spot checks), decode/validate short-circuits, in-window duplicate, and a throwing sink; transport-import + contract-sync gates exit 0; CI verification pending wave push. Test uses a fake `ICpmCodec` per coordinator amendment; real-codec proof lands with 9.1.4.5 + the comms-check lane. Closed: CI run 30697863324 green (`v2x-core-build`).

### [x] `9.1.4.5` — Malformed-input corpus + rejection test *(agent)*

**Objective:** the R9 acceptance corpus: `tests/fixtures/malformed/` fully rejected, zero crashes, counters correct.

**Scope:** commit the corpus per HLD D3 — `empty.uper` (0 bytes), `truncated-nominal.uper` (prefix of the golden `nominal.uper`), `random.uper` (fixed-seed bytes), `wrong-message-id.uper` + `wrong-protocol-version.uper` (byte-edited golden header), `r1-variant.uper` (header edited to the release-1 shape), `oversized.uper` (> any legal CPM, e.g. 4 KiB) — provenance of each documented in test comments, generation deterministic. Local fixtures, **not** synced contracts (HLD §4). Test `tests/pipeline/test_rx_pipeline_malformed.cpp`: every file → sink never called, `decode_reject` (or `validate_reject`) counted, process never crashes; whole corpus in one parameterized suite.

**Acceptance:** V2X build + ctest green on CI — closes the malformed half of the R9 box (golden decode half closed by Phase 0 `1.0.2.5` + 9.1.4.4's test).

**Dependencies:** after 9.1.4.4. **Commit:** `[9.1.4.5] test: reject the malformed-input corpus with zero crashes`

**Status:** implemented 2026-08-01 — 7-case corpus committed as binary fixtures (`*.uper binary` `.gitattributes`) with byte-level provenance + regeneration recipe in the test comments; `v2x_rx_pipeline_malformed_test` drives the **real** `VanetzaCpmCodec` through all 7 in a parameterized suite plus a whole-corpus/not-wedged run (golden `nominal.uper` still decodes afterwards) and asserts no `unexpected_exception:` attribution anywhere; transport-import + contract-sync gates exit 0; CI verification pending wave push. **Deviation flagged:** the 3 header-edit cases (`wrong-message-id`, `wrong-protocol-version`, `r1-variant`) are **not** rejectable — profile §3 freezes `protocolVersion`/`messageId` as *ignored on decode*, every octet value is in ASN.1 range, and `CpmContent` carries no header field for the validator; they are asserted as profile-tolerated negative controls (4 rejected / 3 forwarded). Closing that gap needs a header-conformance check in the R1 codec + a profile §3 re-freeze — out of this subtask's scope. Closed on its stated acceptance: CI run 30697863324 green (`v2x-core-build`) — the real-Vanetza corpus suite passes with zero crashes. The 4-reject/3-tolerated deviation above is **not** closed by it and is carried as a contract-level finding for the architecture owner in § Open items item 8; the R9 milestone box stays partly closed. **Amended 2026-08-01 per the architecture owner's ruling (§ Open items item 8):** the deviation is adjudicated a **mis-categorisation in the corpus, not a defect in R9's wording** — no frozen-contract change, no requirement re-wording. The three header-edit fixtures are kept but **relabelled** explicit *profile-tolerated negative controls*, asserting the behaviour profile §3 mandates (they decode, they forward, and their `CpmContent` is identical to the golden nominal — the ignore-on-decode rule itself under test). Three fixtures added for the genuinely-structural class: `truncated-mid-object.uper` (nominal[0:52] — cut inside `velocity.yVelocity.value`, bits 414–428, with the PerceivedObject presence bitmap already committing to velocity *and* classification and bits 436–452 absent entirely), `bit-flipped-payload.uper` (octet 36 inverted, `0x80` → `0x7f` — bits 288–295 lie strictly inside the bits 255–300 span, which holds *only* structural elements: the container identifier, the `containerData` CHOICE index, `numberOfPerceivedObjects`, the `perceivedObjects` length determinant and the PerceivedObject OPTIONAL-presence bitmap, with no plain value INTEGER to absorb the flip as a legal-but-different value), and `trailing-garbage.uper` (a complete nominal followed by `de ad be ef de ad be ef`, predicted **tolerated** rather than rejected — `uper_decode_complete` never checks unconsumed trailing octets, and the verified bit map shows nominal.uper *already* carries a whole spare zero octet past its last content bit at 452 and still decodes green). The suite now carries a per-case **`Disposition` table** (`kReject` / `kToleratedControl`) as its single source of truth: it asserts that exact disposition with **no either-outcome branch**, derives the whole-corpus counter totals from the table (`kRejectCases` / `kToleratedCases`, `static_assert`-checked) so a relabel cannot desync them, and prints a relabel-ready diagnostic on mismatch (case, expected disposition, decoded?/forwarded?, which reject counter fired, the reject `detail`, the full `[EVT]` log). Corpus is now **10 cases: 6 `Reject` / 4 `ToleratedControl`**, with a measured byte-level UPER bit map of `nominal.uper` and each new fixture's sha256 recorded in the test comments so every case is regenerable from them alone. **HONEST LIMIT — predicted, not measured:** the authoring host has no C++ toolchain and convention F3 bans an ASN.1 decoder in Python, so the three new dispositions are *reasoned predictions that the test asserts strictly*; the first `v2x-core-build` run is the empirical oracle. If a prediction is wrong the lane goes RED and that one case gets relabelled in a follow-up commit — the intended loop, not a failure of the design. Gates re-run on the amendment: `contracts/check_sync.py` exit 0 (47 copies; the malformed corpus is correctly *absent* from the sync manifest — local fixtures per HLD §4) and `V2X_ECU/tools/check_transport_imports.py` exit 0; `Scenario_Player` pytest unchanged at 116 passed / 7 skipped (the bench is untouched).

---

## Task Group 1.5 — V2X app assembly + capture + image (serves R8, R6, R5)

### [x] `8.1.5.1` — Composition root `src/main.cpp` + `v2x_ecu` executable *(agent)*

**Objective:** assemble Config → EventLog → stub/adapter → pipeline → forwarder and run the scripted call flow (HLD §4/§8: main is controller only — no business logic).

**Scope:** load `Config` (exit non-zero with message on invalid env); construct `EventLog`, `VanetzaCpmCodec`, stages, `RxPipeline` with `AdaForwarder` as sink, `StubRadioAdapter` with the stub's transition observer wired to `stub_transition`/`fault_injected`/`recovery` events; drive `init → configure → subscribeRx` honoring the D2 recovery rules (terminal failure → exit non-zero); then block (Rx thread serves traffic). Add `add_executable(v2x_ecu src/main.cpp …)` to `V2X_ECU/CMakeLists.txt`. No new test file — acceptance is the full existing suite + link of the complete chain; live call-flow evidence lands at group 1.10.

**Acceptance:** V2X build + ctest green on CI; `v2x_ecu` target builds.

**Dependencies:** after 8.1.2.1 + 2.1.2.4 + 7.1.3.4 + 9.1.4.4. **Commit:** `[8.1.5.1] feat: add v2x_ecu composition root`

**Status:** implemented 2026-08-01 — full chain wired (Config→EventLog→codec→stages→pipeline→forwarder sink, stub observer→D4 `stub_transition`/`fault_injected`/`recovery`), documented exit codes 0/2/3/4/5, SIGTERM/SIGINT stop → `adapter.stop()`; both gates exit 0 (`check_transport_imports` 25 files clean, `check_sync` 36 copies identical); CI verification pending wave push. Closed: CI run 30697863324 green — `v2x-core-build` builds and links `v2x_ecu`, and the `v2x-comms-check` lane runs the binary end-to-end (exits 0 on SIGTERM).

### [x] `6.1.5.2` — Capture script `capture.sh` *(agent)*

**Objective:** the D5 in-container capture: live `[CAP]` text + rotating pcap + base64 export through View Log.

**Scope:** at `V2X_ECU/capture.sh`; reads `CAPTURE_FILTER` (default `udp`), `PCAP_DIR` (default `/data/capture`, mkdir -p), `CAPTURE_ROTATE_S` (default 60) from env — no literals; two tcpdump processes per D5: (a) `tcpdump -i any -n -l -tttt $CAPTURE_FILTER` prefixed `[CAP]` to stdout; (b) `tcpdump -w` rotating every `CAPTURE_ROTATE_S` (e.g. `-G` + `-z` post-rotate), each closed file base64-emitted to stdout between `[PCAP-BEGIN <name>]` / `[PCAP-END]` marker lines — the exact format 6.1.5.3 parses. Degrade gracefully when NET_RAW is unhonored (log and stay alive — O2 fallback posture).

**Acceptance:** `sh -n V2X_ECU/capture.sh` and `bash -n` pass; LF line endings; marker format exactly as stated. (tcpdump runs only on-platform — runtime evidence at 6.1.10.5.)

**Dependencies:** none. **Commit:** `[6.1.5.2] feat: add tcpdump capture script with pcap export`

**Status:** implemented 2026-08-01 — sh -n + bash -n clean; --export-one round-trip base64-decodes byte-identically (the format 6.1.5.3 parses); LF enforced via .gitattributes, staged blob CR-free; runtime tcpdump evidence lands at 6.1.10.5. Closed — the script's `sh -n`/`bash -n` + marker-round-trip acceptance is met; on-platform tcpdump evidence stays with 6.1.10.5.

### [x] `6.1.5.3` — Host-side extraction `tools/extract_pcap.sh` *(agent)*

**Objective:** the "automatic tool" retrieval path of D5: saved View Log in → `.pcap` files out ([usage contract](../requirements/car-sky-guide/traffic-capture-wireshark.md)).

**Scope:** at `V2X_ECU/tools/extract_pcap.sh` (host tool — never shipped in the image); for each `[PCAP-BEGIN <name>]`…`[PCAP-END]` block: strip markers, base64-decode, write `<name>.pcap` next to the input log; multiple blocks per log; non-zero exit + message when no block found. Self-check: demonstrate a round-trip on a synthetic log (known bytes → base64 block → script → `cmp`) — evidence in the Status line; runs in Git Bash locally.

**Acceptance:** `bash -n` passes; synthetic round-trip byte-identical.

**Dependencies:** none (marker format is HLD-frozen; logically pairs with 6.1.5.2). **Commit:** `[6.1.5.3] feat: add host-side pcap extraction script`

**Status:** implemented 2026-08-01 — bash -n clean; real round-trip through the landed capture.sh --export-one producer extracted 2 blocks byte-identically (cmp clean, binary bytes incl. CR/LF/NUL); no-block exit 1, truncated/corrupt block non-zero with partial extraction, path-escape name sanitized, CRLF-saved log handled. Closed — the script's `bash -n` + byte-identical round-trip acceptance is met.

### [x] `5.1.5.4` — `V2X_ECU/Dockerfile` + `entrypoint.sh` *(agent)*

**Objective:** the deployable `v2x-ecu:latest` image (HLD §9): multi-stage — cmake build stage → slim runtime with tcpdump.

**Scope:**

- `V2X_ECU/entrypoint.sh`: `./capture.sh &` then `exec ./v2x_ecu` (blueprint `command: ["./entrypoint.sh"]` per [node-v2x-ecu.md](../requirements/car-sky-guide/node-v2x-ecu.md)).
- `V2X_ECU/Dockerfile`: build stage installs cmake/g++/libboost-dev/libboost-date-time-dev and builds `v2x_ecu` (tests excluded from the image build to keep it lean); runtime stage = slim Debian-family base + tcpdump + coreutils(base64) + `v2x_ecu`, `entrypoint.sh`, `capture.sh` at workdir `/app`. LGPLv3 posture (report §4, SP HLD §10 item 2): link Vanetza dynamically (`-DBUILD_SHARED_LIBS=ON` at the image's configure) and copy `libvanetza_asn1*.so` into the runtime stage. `tools/`, `tests/`, `doc/` never enter the runtime stage. Build context is `V2X_ECU/` alone.
- arm64 build-time risk under QEMU flagged — § Open items item 3; use the CI lane's buildx cache.

**Acceptance:** CI `v2x-ecu-image` lane (5.1.8.2) green — `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -t m1-v2x-ecu:latest V2X_ECU/` succeeds.

**Dependencies:** after 8.1.5.1 + 6.1.5.2 + 11.1.1.1 + 5.1.8.2 (lane must exist to verify). **Commit:** `[5.1.5.4] feat: add V2X ECU multi-stage Dockerfile and entrypoint`

**Status:** implemented 2026-08-01 — entrypoint `sh -n`/`bash -n` clean, LF + exec bit set; Dockerfile self-reviewed (multi-stage, `BUILD_SHARED_LIBS=ON` + staged `libvanetza_asn1*.so` with a fail-loud guard, tcpdump/coreutils runtime, no ENV shadowing HLD §6, target-scoped build excludes tests). No local Docker — image build verification lands with the 5.1.8.2 CI lane (Wave D). **Open** — acceptance is the `v2x-ecu-image` lane, which has never run: no `v2x-ecu` image has ever been built (§ Remaining work). **Closed: CI run 30698630956 — the `v2x-ecu-image` lane is green,** so the stated acceptance ("CI `v2x-ecu-image` lane green") is met: `docker buildx build --platform linux/arm64` of `V2X_ECU/` completed inside the timeout, so the multi-stage image builds for real — Vanetza compiled under QEMU aarch64 emulation, `libvanetza_asn1*.so` staged, tcpdump runtime.

---

## Task Group 1.6 — Scenario Player application (serves R11)

> All paths from [SP HLD §3](../Scenario_Player/doc/phase1-scenario-player-hld.md#3-folder-structure-map--file-location-designations); build/test = Scenario_Player Python row of § Per-node build commands (local pytest **and** CI `python-tests`). Wire-native conversion authority: the callflow note §4.2 mapping ([scenario-player-v2x-callflow-messages.md](../Scenario_Player/doc/research_notes/scenario-player-v2x-callflow-messages.md)); content dataclass: `Scenario_Player/player/contracts/cpm_content.py` (Phase 0). Test paths beyond the HLD's list are planner-designated per the folder's `tests/test_<module>.py` convention (§ Open items item 5).

### [x] `11.1.6.1` — Config loader `player/config.py` + runtime manifest *(agent)*

**Objective:** env + scenario-YAML loading/validation — the bench's **only env reader** (SP HLD D3/§5).

**Scope:**

- Env per HLD §5: `SCENARIO_CONFIG` (default `/app/scenarios/default.yaml`), `V2X_ECU_HOST`/`V2X_ECU_PORT`, `ENCODER_PATH` (default `/app/cpm_encode`); injectable env getter.
- YAML shape per D3, validated into frozen dataclasses: `name`, `cpm_rate_hz` (default 10, F8), `duration_s`, `loop`, `sender {station_id, lat, lon, heading_deg}`, `object {object_id, initial_distance_m, closing_speed_mps, lateral_offset_m, classification, confidence}`. Missing/mistyped/non-positive-rate → descriptive `ValueError`.
- Create `Scenario_Player/requirements.txt` (runtime: pinned PyYAML) and make `requirements-dev.txt` start with `-r requirements.txt` so local + CI test installs carry PyYAML.
- Test `tests/test_config.py`: valid YAML loads; each rejection case; env defaults + overrides.

**Acceptance:** Scenario_Player pytest green locally and on CI `python-tests`.

**Dependencies:** none. **Commit:** `[11.1.6.1] feat: add scenario config loader and validation`

**Status:** done 2026-08-01 — pytest 69 passed locally (config loader + env defaults + rejections); requirements.txt created with -r include from dev. Closed: CI run 30697863324 green (`python-tests`).

### [x] `11.1.6.2` — Committed scenario variants `scenarios/*.yaml` *(agent)*

**Objective:** the two R11-acceptance scenario files (SP HLD D3) — observably different by construction.

**Scope:** `scenarios/default.yaml` — C approaching, 60 m closing to ~10 m over the run; `scenarios/c-out-of-range.yaml` — C static beyond the 35 m exit gate (value deliberately > `gate_exit`; pairing re-checked at Phase 2 per SP HLD §10 item 3). All tunables live in the YAML — new variants are new files, never code. Extend `tests/test_config.py`: both committed files load through `player.config` and differ in the D3 kinematic fields.

**Acceptance:** pytest green locally + CI.

**Dependencies:** after 11.1.6.1. **Commit:** `[11.1.6.2] feat: add the two committed scenario variants`

**Status:** done 2026-08-01 — pytest 72 passed locally; default 60→~10 m approach, c-out-of-range static at 60.0 m > 35 m exit gate. Closed: CI run 30697863324 green (`python-tests`).

### [x] `11.1.6.3` — Kinematic model `player/scenario.py` *(agent)*

**Objective:** the single constant-velocity model (SP HLD D3): `sample(t)` → `CpmContent` in wire-native units.

**Scope:** sender B static WGS84 pose + heading from config (lat/lon → 10⁻⁷ °, heading → 0.1 °); object C relative state along x: `x(t) = initial_distance_m − closing_speed_mps·t` (→ 0.01 m), fixed `lateral_offset_m` (→ 0.01 m), velocity x = −closing_speed_mps (→ 0.01 m/s, B's cartesian frame); conversion table = callflow note §4.2; F9 asserted before return (|mdt| ≤ 2047). One model — scenario differences come only from config values. Test `tests/test_scenario_kinematics.py`: sampled values vs hand-computed positions at t = 0/mid/late; wire-unit conversions exact; F9 bound enforced.

**Acceptance:** pytest green locally + CI.

**Dependencies:** after 11.1.6.1. **Commit:** `[11.1.6.3] feat: implement constant-velocity scenario kinematics`

**Status:** done 2026-08-01 — pytest 85 passed locally; hand-computed t=0/10/20 wire values exact, F9 ±2048 rejected. Closed: CI run 30697863324 green (`python-tests`).

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

### [x] `11.1.6.8` — Entrypoint `main.py` *(agent)*

**Objective:** the blueprint-fixed entrypoint at the folder root (`command: ["python", "main.py"]`, workdir `/app`): load env + YAML → spawn encoder → run the generator (SP HLD D4) — controller only, no business logic.

**Scope:** wire `config` → `scenario` → `encoder_client` → `sender` → `generator`; startup + fatal errors logged to stdout (View Log). Test `tests/test_main.py`: end-to-end short run — a bounded scenario (small `duration_s`, `loop: false`), the fake encoder from 11.1.6.5, a loopback UDP listener; assert datagrams received + `[TX]` lines emitted.

**Acceptance:** pytest green locally + CI.

**Dependencies:** after 11.1.6.1 + 11.1.6.3 + 11.1.6.5 + 11.1.6.6 + 11.1.6.7. **Commit:** `[11.1.6.8] feat: add scenario player entrypoint main.py`

**Status:** done 2026-08-01 — pytest 110 passed locally; end-to-end smoke (fake encoder → loopback UDP) received datagrams with [TX] lines; [FATAL] path returns 1. Closed: CI run 30697863324 green (`python-tests`).

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

**Objective:** the deployable `scenario-player:latest` image (SP HLD §3/§8): stage 1 cmake-builds `codec_helper` → stage 2 python-slim runtime.

**Scope:** stage 1: cmake/g++/libboost-dev + build `cpm_encode`; stage 2: `python:3.11-slim` + `pip install -r requirements.txt` + `main.py`, `player/`, `scenarios/`, and `cpm_encode` at `/app/cpm_encode` (workdir `/app`); LGPLv3 dynamic posture per SP HLD §10 item 2 — copy `libvanetza_asn1*.so` into stage 2 (or configure shared as 5.1.5.4 does); `codec_helper/src`, `tests/`, `doc/` never enter stage 2. Blueprint config unchanged ([node-scenario-player.md](../requirements/car-sky-guide/node-scenario-player.md)). arm64/QEMU build-time risk: § Open items item 3.

**Acceptance:** CI `scenario-player-image` lane (5.1.8.2) green — `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -t m1-scenario-player:latest Scenario_Player/` succeeds.

**Dependencies:** after 11.1.7.1 + 11.1.6.8 + 5.1.8.2. **Commit:** `[5.1.7.3] feat: add Scenario Player multi-stage Dockerfile`

**Status:** implemented 2026-08-01 — self-reviewed multi-stage image (trixie build stage for cmake ≥3.28 → python:3.11-slim runtime carrying cpm_encode + staged libvanetza_asn1*.so with fail-loud guards; only main.py/player/scenarios/requirements.txt reach stage 2; no ENV shadowing HLD §5; CMD matches the blueprint command); .dockerignore cross-checked against every COPY; glibc trixie→bookworm and arm64/QEMU risks flagged for the 5.1.8.2 lane. No local Docker — image build verification lands with 5.1.8.2. Amended 2026-08-01: F1 ratified by architecture — builder and runtime now share one base image (`python:3.11-slim`) with the CMake floor met by pip `cmake>=3.28,<4`, closing the glibc/GLIBCXX skew risk by construction (SP HLD § Open items item 2); arm64 build proof still pending the 5.1.8.2 lane. **Open** — acceptance is the `scenario-player-image` lane, which has never run: no `scenario-player` image has ever been built, and no CI run covers `e02fb7c` (§ Remaining work). **Closed: CI run 30698630956 — the `scenario-player-image` lane is green,** so `docker buildx build --platform linux/arm64` of `Scenario_Player/` succeeds and the arm64 build proof the amendment above was waiting on now exists (that run covers `e02fb7c`). It also **empirically validates F1**: the shared `python:3.11-slim` base with pip `cmake>=3.28,<4` built clean on the first attempt — the predicted glibc/GLIBCXX skew did not materialise.

---

## Task Group 1.8 — CI lanes (workflow-file edits, one objective each; serves R11, R5)

> Both extend `.github/workflows/phase0-ci.yml` (explicitly in write scope), guarded on file existence like the existing jobs — they land **before** their consumers so those subtasks have CI acceptance from day one.

### [x] `11.1.8.1` — Lane: `sp-codec-helper` build + encoder-golden pytest *(agent)*

**Objective:** the Linux verification lane for group 1.7.

**Scope:** new job `sp-codec-helper`: checkout; install libboost-dev + libboost-date-time-dev; `actions/cache` over `Scenario_Player/codec_helper/build/_deps` keyed on the Vanetza pin + `hashFiles` of `codec_helper/CMakeLists.txt` + `codec_helper/cmake/vanetza-pin.cmake`; configure/build per the codec_helper row of § Per-node build commands (`-j $(nproc)` — bounded, same OOM rationale as `v2x-core-build`); then `--encode` smoke over the six golden `.json` and `python -m pytest Scenario_Player/tests` with `ENCODER_PATH` pointing at the built binary. Entire job guarded: skip-with-notice while `codec_helper/CMakeLists.txt` is absent.

**Acceptance:** workflow YAML valid; lane green on the current tree (guard branch) — goes live when 11.1.7.1 lands.

**Dependencies:** none — lands immediately. **Commit:** `[11.1.8.1] chore: add codec-helper build and encoder-golden CI lane`

**Status:** implemented 2026-08-01 — YAML valid (8 jobs), run-blocks bash -n clean, cache key carries the same Vanetza pin as v2x-core-build; lane builds cpm_encode, smoke-tests --encode over the six synced golden .json, and runs the SP suite with ENCODER_PATH so test_encoder_golden executes unskipped (expected 123 passed / 0 skipped vs 116/7 locally). CI verification pending wave push. Closed: lane green in CI run 30697863324 with `test_encoder_golden.py` executing unskipped.

### [x] `5.1.8.2` — Lanes: `v2x-ecu-image` + `scenario-player-image` docker builds *(agent)*

**Objective:** arm64 image build (+ gated push) for both node images — `netcheck-image` is the template.

**Scope:** two jobs mirroring `netcheck-image` verbatim in shape: qemu + buildx setup; `docker buildx build --platform linux/arm64 --provenance=false --sbom=false`; tags `m1-v2x-ecu:latest` / `m1-scenario-player:latest` on `registry.hackathon-2.carsky.io`; push only when the `CARSKY_ZOT_API_KEY` secret exists (same login step + notice-and-exit-0 guard); buildx layer cache (`type=gha`) against the QEMU build-time risk; each job guarded skip-with-notice while its `Dockerfile` is absent; raised `timeout-minutes` for the C++ stages.

**Acceptance:** workflow YAML valid; both lanes green on the current tree (guard branch) — go live as the Dockerfiles land.

**Dependencies:** none — lands immediately, parallel with 11.1.8.1. **Commit:** `[5.1.8.2] chore: add node-image docker build-push CI lanes`

**Status:** implemented 2026-08-01 — YAML valid (10 jobs), run-blocks bash -n clean; both lanes build arm64 single-platform with --provenance/--sbom=false and per-image type=gha cache scopes, push gated on CARSKY_ZOT_API_KEY (missing secret ⇒ green build-only). Expect these lanes to be SLOW (Vanetza's ~1400 ASN.1 TUs under QEMU emulation, timeout-minutes 360) — they are the first real proof either node image builds. CI verification pending wave push. Added beyond the brief: an `actions/github-script@v7` step per lane re-exporting `ACTIONS_CACHE_URL`/`ACTIONS_RESULTS_URL`/`ACTIONS_RUNTIME_TOKEN` into `GITHUB_ENV`, without which `type=gha` cannot reach the Actions cache from a plain `run:` step (those vars are given to actions, not run steps) — and `ignore-error=true` on `--cache-to` so a cache-service fault cannot fail an otherwise-successful multi-hour build. **Open** — the lanes' own acceptance is both green on the current tree; they landed in `df90774`, after the last CI run, and have never executed (§ Remaining work). **Closed: both lanes green in CI run 30698630956** (on `7a02fb5`, which carries both Dockerfiles and both lanes) — the stated acceptance is met and the lanes are no longer unexecuted. Observed facts that supersede the pre-run cautions above: the arm64 QEMU builds completed **within** `timeout-minutes: 360`, and the per-image `type=gha` cache scopes did **not** evict the `actions/cache` `_deps` entries — `v2x-core-build`, `v2x-comms-check` and `sp-codec-helper` all stayed green in the same 10-lane run.

---

## Task Group 1.9 — ADA-side R2 sink update (serves R2; D6)

### [x] `2.1.9.1` — Parameterize netcheck `BODY_PREVIEW` *(agent)*

**Objective:** the D6 netcheck note made real (V2X HLD §11 item 3): the `[RX]` body-preview length becomes the `BODY_PREVIEW` env var so the ADA sink can show whole R2 JSON bodies.

**Scope:** `tools/netcheck/netcheck.py` only (explicitly in write scope): replace the literal `96` in the `[RX]` log line with `BODY_PREVIEW` read from env, **default 96** (spec-preserving); add it to the module docstring's env list. No other behavior change; `Dockerfile`/`entrypoint.sh`/`capture.sh` untouched. The sink deployment sets `BODY_PREVIEW=512` (5.1.10.2); the image re-push rides the existing `netcheck-image` lane (5.1.10.1).

**Acceptance:** `python -m py_compile tools/netcheck/netcheck.py` passes; default path produces byte-identical log behavior to today.

**Dependencies:** none. **Commit:** `[2.1.9.1] feat: parameterize netcheck body preview length`

**Status:** done 2026-08-01 — py_compile passes; diff is exactly the env constant + docstring + slice; default 96 preserves today's behavior byte-identically. Closed — the stated local acceptance (`py_compile`, byte-identical default behavior) is fully met; no CI artifact is required.

---

## Task Group 1.10 — Deploy & live verification (serves R5, R2, R11, R6 + the Demo box)

> Split per Phase 0 group 0.8: registry work was planned for [[car-sky]] and executed by CI instead; USER-MANUAL for Nydus UI (blueprint `command`/`capabilities`/env edits are manual — V2X HLD §11 item 4; REST cannot edit node config). Evidence accumulates in `plans/doc/phase1-comms-run.md` (created by 5.1.10.1). Standing requirement: images single-platform `linux/arm64` ([phase0-smoke-test-run.md](doc/phase0-smoke-test-run.md)).

### [x] `5.1.10.1` — Push the three images to the CarSky registry *(planned car-sky — executed by CI)*

**Objective:** [[car-sky]] runs deploy-preflight (blueprint `trial2_minh`, target nodes, credential), then ensures `registry.hackathon-2.carsky.io` holds current `m1-v2x-ecu:latest`, `m1-scenario-player:latest`, and `m1-netcheck:latest` (rebuilt with 2.1.9.1's `BODY_PREVIEW`).

**Scope:** push path = the CI lanes (5.1.8.2 + existing `netcheck-image`) once `CARSKY_ZOT_API_KEY` is a repo secret, or direct build/push; verify tags present via the registry API; create `plans/doc/phase1-comms-run.md` recording pushed digests. Guides' `registry.carsky.io` login lines are stale (O1) — use the hackathon-2 host (§ Open items item 6).

**Acceptance:** all three tags pull-able from the registry; evidence doc created.

**Dependencies:** after 5.1.5.4 + 5.1.7.3 + 2.1.9.1. **Commit:** `[5.1.10.1] docs: record phase1 image pushes to the CarSky registry`

**Status:** **partly closed 2026-08-01 — executed by CI, not [[car-sky]].** `CARSKY_ZOT_API_KEY` turned out to be present, so the secret-gated push step in each image lane ran: run `30698630956` logged `Notice: pushed registry.hackathon-2.carsky.io/m1-v2x-ecu:latest (linux/arm64)` and the same for `m1-scenario-player:latest`, each a single manifest (not an index); record in [phase1-comms-run.md](doc/phase1-comms-run.md). This retires the "needs the car-sky agent" framing of § Open items item 4 for good. **Closed:** the acceptance's "all three tags pull-able" is met by deployment itself — every node on `phase1_Minh_test-deploy` pulled and ran, and the sink's full 339-char `[RX]` bodies prove `m1-netcheck:latest` carries `2.1.9.1`'s `BODY_PREVIEW` rather than the hardcoded-96 build. **Standing hazard, not a residual:** `main` pushes that same tag from pre-`2.1.9.1` code, so any `main` commit before the branch merges silently reverts the registry copy — re-check the `[RX]` body length after `main` activity. No digests are recorded: all three tags are mutable and every branch push re-pushes them, so identify the deployed image at deploy time.

### [ ] `5.1.10.2` — USER-MANUAL: blueprint node config + deploy → all nodes Running

**Objective:** the user edits `trial2_minh` (or a clone) in the Nydus UI and deploys; every node Running, restart 0 — the R5 box (APK clause aside, § Open items item 2).

**Scope — node config per the guides + D6:**

- Bench `.10`: per [node-scenario-player.md § Blueprint node config](../requirements/car-sky-guide/node-scenario-player.md) — image `registry.hackathon-2.carsky.io/m1-scenario-player:latest`, `command: ["python", "main.py"]`, env `SCENARIO_CONFIG=/app/scenarios/default.yaml`, `V2X_ECU_HOST=10.99.0.11`, `V2X_ECU_PORT=47100`.
- V2X `.11`: per [node-v2x-ecu.md § Blueprint node config](../requirements/car-sky-guide/node-v2x-ecu.md) — image `…/m1-v2x-ecu:latest`, `command: ["./entrypoint.sh"]`, `capabilities: ["NET_RAW"]`, env `LISTEN_PORT=47100`, `ADA_ECU_HOST=10.99.0.12`, `ADA_ECU_PORT=47200`, `FAULT_PLAN=none`.
- ADA `.12` (D6 sink): image `…/m1-netcheck:latest`, `command: ["./entrypoint.sh"]`, `capabilities: ["NET_RAW"]`, env `ROLE=ada-sink`, `LISTEN_PORT=47200`, `BODY_PREVIEW=512` — **no** `NEXT_HOP_*`.
- IVI: keep the provided AAOS artifact. Ethernet pins/edges already exist on `trial2_minh` (Phase 0). New Deployment → Deployment Viewer all Running, restart 0; mind the 2-deployment quota.

**Acceptance:** Running evidence (screenshot/notes) recorded in `plans/doc/phase1-comms-run.md`; evidence commit by the orchestrating session after user confirmation.

**Dependencies:** after 5.1.10.1. **Commit:** `[5.1.10.2] docs: record phase1 blueprint config and Running evidence`

### [ ] `2.1.10.3` — USER-MANUAL: R2 observed at the ADA ECU + scripted `[EVT]`-chain check

**Objective:** close the R2 box live and the on-platform half of the D7 box: the ADA sink's `[RX]` lines show R2 JSON carrying decoded bench-scenario values, and the V2X `[EVT]` stream passes `check_v2x_log.py`.

**Scope:** save the V2X node View Log export and run `python tools/comms_check/check_v2x_log.py <saved.log>` in stream mode (D7 on-platform — bench = the live Scenario Player): exit 0 proves `rx_datagram` → `decode_ok` (CpmContent JSON) → `r2_forwarded` (R2 JSON) per received message — **replaces the manual eyeball check of the Rx chain**. Still read directly: the `[EVT]` `stub_transition` bring-up sequence (the R8 scripted-call-flow live evidence) and the ADA node View Log `[RX]` bodies (512-char preview) showing `object.distance` **changing over time** per `default.yaml`'s approach kinematics. Optional supplementary R8 evidence: redeploy V2X with `FAULT_PLAN=init_fail`, observe `fault_injected`/`recovery`/retry lines, restore `none`.

**Acceptance:** `check_v2x_log.py` exit 0 on the saved export with its output recorded, plus the bring-up and ADA `[RX]` excerpts, in `plans/doc/phase1-comms-run.md`; evidence commit by the orchestrating session.

**Dependencies:** after 5.1.10.2 + 9.1.12.2. **Commit:** `[2.1.10.3] docs: record R2-at-ADA live evidence`

### [ ] `11.1.10.4` — USER-MANUAL: scenario swap → observably different streams

**Objective:** the live half of the R11 box (model half = 11.1.6.4).

**Scope:** edit the bench node's `SCENARIO_CONFIG` to `/app/scenarios/c-out-of-range.yaml`, redeploy, compare V2X `[EVT]` + ADA `[RX]` logs against the 2.1.10.3 run: static beyond-gate distance vs approaching sequence — no rebuild, config-only (SP HLD §8).

**Acceptance:** paired before/after log excerpts recorded in `plans/doc/phase1-comms-run.md`; evidence commit by the orchestrating session.

**Dependencies:** after 2.1.10.3. **Commit:** `[11.1.10.4] docs: record scenario-swap stream difference evidence`

### [ ] `6.1.10.5` — USER-MANUAL: capture retrieval → Wireshark (R6 + Demo)

**Objective:** close the R6 box (traffic captured on the bridge; reachability already proven by the Phase 0 smoke test) and the Demo box.

**Scope:** per [traffic-capture-wireshark.md](../requirements/car-sky-guide/traffic-capture-wireshark.md): save the V2X node View Log; run `V2X_ECU/tools/extract_pcap.sh <saved.log>`; open the `.pcap` in Wireshark; verify **both** flows at the single capture point — bench→V2X UDP/47100 payloads matching golden-vector/`[EVT]` sizes+timestamps, V2X→ADA UDP/47200 R2 JSON. Dissection caveat stands (D5): raw UPER without GN/BTP shows as UDP data, evidence = payload-byte correlation, not an ITS protocol tree.

**Acceptance:** `.pcap` archived + findings recorded in `plans/doc/phase1-comms-run.md`; evidence commit by the orchestrating session.

**Dependencies:** after 5.1.10.2 (parallel with 2.1.10.3/11.1.10.4). **Commit:** `[6.1.10.5] docs: record capture retrieval and Wireshark evidence`

---

## Task Group 1.11 — Plan maintenance (docs)

### [x] `5.1.11.1` — Reconcile milestone1.md Phase 0 acceptance *(agent — docs, commits on `main`)*

**Objective:** `plans/milestone1.md` § Phase 0 still shows box 4 open with a blocker note, contradicting [phase0_tasks.md § Output](phase0_tasks.md#phase-0-overview) (closed, smoke test C1–C5 green).

**Scope:** flip box 4 to `[x]`; replace the stale blocker paragraph with a one-line closed statement citing [phase0-smoke-test-run.md](doc/phase0-smoke-test-run.md). No other milestone1.md change.

**Acceptance:** milestone1.md § Phase 0 shows 4/4 closed, consistent with phase0_tasks.md; [markdown style](../.claude/skills/markdown-writing-style/SKILL.md) held.

**Dependencies:** none — anytime. **Commit:** `[5.1.11.1] docs: reconcile milestone1 Phase 0 acceptance with phase0_tasks`

**Status:** done 2026-08-01 — landed on `main` as commit `68ef5f5`; [milestone1.md](milestone1.md) § Phase 0 there shows 4/4 closed. This branch's copy still carries the stale box until `main` merges in.

---

## Task Group 1.12 — Bench↔V2X comms check (serves R6, R9; HLD D7)

> The D7 script pair + CI lane — scripted acceptance that messages sent between bench and V2X ECU are received, raise events, and deserialize to JSON. Location `tools/comms_check/` at the repo root is user-mandated (cross-node test equipment spanning bench and V2X ECU — the `tools/netcheck/` precedent) and explicitly in these subtasks' write scope. Test equipment only — never shipped in a node image.

### [x] `6.1.12.1` — Golden-vector UDP sender `tools/comms_check/send_cpm.py` *(agent)*

**Objective:** the bench-side send stand-in for local/CI runs (D7): send each golden-vector `.uper` payload as one UDP datagram to a target `host:port`.

**Scope:** Python 3 stdlib only; target host/port and corpus directory (default `contracts/golden-vectors/`) from CLI args/env — no hardcoded peers (governing principle 5); deterministic case order + configurable inter-send delay; one stdout line per sent vector (case name, byte length) for downstream correlation. On-platform the live sender is the Scenario Player — this script never deploys.

**Acceptance:** `python -m py_compile tools/comms_check/send_cpm.py` passes; loopback self-check — a scratch UDP listener receives all six vectors byte-identical (stdlib-only, runs on the Windows host; evidence in the Status line).

**Dependencies:** none. **Commit:** `[6.1.12.1] feat: add golden-vector UDP sender for the comms check`

**Status:** done 2026-08-01 — py_compile passes; loopback self-check delivered all 6 golden .uper byte-identical (sha256 match), no-args exit 2, empty-corpus exit 1, cwd-independent corpus default. Closed — local acceptance met, and the sender is exercised for real by the green `v2x-comms-check` lane in CI run 30697863324.

### [x] `9.1.12.2` — `[EVT]`-stream assertion `tools/comms_check/check_v2x_log.py` *(agent)*

**Objective:** assert the D7 receive-evidence chain from a V2X ECU `[EVT]` stream: per message, `rx_datagram` (received) → `decode_ok` carrying the decoded `CpmContent` JSON (event raised, deserialization shown) → `r2_forwarded` carrying the R2 JSON body; non-zero exit naming the first missing link.

**Scope:** Python 3 stdlib; input = file path or stdin — accepts CI-captured stdout **or** a saved View Log export (the smoke-test View-Log-as-retrieval model), tolerating interleaved `[CAP]`/non-`[EVT]` lines; two modes: **expected-vector mode** (CI — given the golden corpus dir, asserts the chain per sent case and cross-checks the embedded `decode_ok` content against the golden `.json`) and **stream mode** (on-platform — every observed `rx_datagram` must complete the chain; minimum-count threshold arg). Line shape = 18.1.2.3's amended-D4 output.

**Acceptance:** `python -m py_compile tools/comms_check/check_v2x_log.py` passes; demonstrated exit 0 on a synthetic conforming log and non-zero on logs missing each link kind, both modes (evidence in the Status line).

**Dependencies:** after 18.1.2.3 (the embedded-payload field names freeze there). **Commit:** `[9.1.12.2] feat: add EVT-stream assertion script for the comms check`

**Status:** done 2026-08-01 — py_compile passes; synthetic conforming log (6 golden cases, `[CAP]`/`[BOOT]`/blank noise interleaved) exits 0 in both modes, plus a `--repeat 2` corpus in expected-vector mode; 10 negative fixtures (missing rx/decode/forward link, unmatched cpm, `decode_reject` non-zero, unknown event, non-monotonic counters, lost-line counter gap, malformed `[EVT]` JSON, no `[EVT]` lines) each exit 1 naming the failing link — the cpm cross-check and `decode_reject == 0` are expected-vector-mode assertions by design, so those two fixtures pass in stream mode; stdin input, `MIN_MESSAGES` env and 9 exit-2 invocation cases checked; contract-sync and transport-import gates exit 0. Closed — local acceptance met, exercised by the green `v2x-comms-check` lane in CI run 30697863324, and independently shown to discriminate: dropping the `r2_forwarded` event fails at the forward link and stripping `decode_ok`'s `cpm` payload fails at the decode link (both exit 1), while the complete chain exits 0.

### [x] `9.1.12.3` — CI lane `v2x-comms-check` *(agent)*

**Objective:** the CI-side closure of the D7 acceptance box: build `v2x_ecu`, run it loopback, send golden vectors, assert the `[EVT]` chain.

**Scope:** `.github/workflows/phase0-ci.yml` (explicitly in write scope): job `v2x-comms-check` — boost install + the `v2x-core-build` `_deps` cache (same key incl. the 11.1.1.1 fragment hash); build the `v2x_ecu` target; start a stdlib UDP sink standing in for ADA; run `v2x_ecu` in the background with env `LISTEN_PORT`/`ADA_ECU_HOST=127.0.0.1`/`ADA_ECU_PORT`/`FAULT_PLAN=none`, stdout captured to a file; `send_cpm.py` against the listen port; stop the app; `check_v2x_log.py` in expected-vector mode over the captured stdout — the job fails on any non-zero exit.

**Acceptance:** lane green on the pushed branch.

**Dependencies:** after 8.1.5.1 + 6.1.12.1 + 9.1.12.2. **Commit:** `[9.1.12.3] chore: add v2x-comms-check CI lane`

**Status:** implemented 2026-08-01 — YAML valid, run-block bash -n clean, cache key identical to v2x-core-build; lane injects DEDUPE_WINDOW_MS=1 + --delay-ms 100 because four golden vectors share the dedupe key; CI verification pending wave push. Closed: lane green in CI run 30697863324 — ≥ 6 datagrams observed at the ADA sink stand-in with the full chain asserted.

---

## Execution order & parallelism

Dependencies are real (files, frozen interfaces, CI lanes) — not default assumptions. At run time everything executes sequentially in one working tree (§ Subtask discipline); the lanes below are the logical structure. The two node folders are **logically parallel tracks** — they share only the frozen contracts and the synced codec sources.

```
Docs      5.1.11.1                                    (anytime, main)
CI-first  11.1.8.1 ∥ 5.1.8.2 ∥ 7.1.3.5               (guarded lanes/gate — land before their consumers)
Shared    11.1.1.1 ──────────────────────────────────► 11.1.1.2 (sequential-LAST, after 11.1.7.2)

Lane V (V2X_ECU)
  foundation:  8.1.2.1 ∥ 7.1.2.2 ∥ 18.1.2.3 ; 2.1.2.4 after 7.1.2.2
  seam/stub:   7.1.3.1 ──► 8.1.3.2 ──► 8.1.3.3 ──► 7.1.3.4 (also needs 7.1.2.2) ; 7.1.3.6 after 7.1.3.1
  pipeline:    9.1.4.1 ∥ 9.1.4.2 ∥ 9.1.4.3 ──► 9.1.4.4 (needs 18.1.2.3) ──► 9.1.4.5
  assembly:    8.1.5.1 (needs 8.1.2.1 + 2.1.2.4 + 7.1.3.4 + 9.1.4.4) ; 6.1.5.2 ∥ 6.1.5.3 anytime
  image:       5.1.5.4 (needs 8.1.5.1 + 6.1.5.2 + 11.1.1.1 + 5.1.8.2)

Lane P (Scenario_Player)
  app:         11.1.6.1 ──► 11.1.6.2 ∥ 11.1.6.3 ──► 11.1.6.4 ; 11.1.6.5 ∥ 11.1.6.6 anytime ; 11.1.6.7 after 11.1.6.3
               11.1.6.8 after 11.1.6.1/3/5/6/7
  codec path:  11.1.7.1 (needs 11.1.1.1 + 11.1.8.1) ──► 11.1.7.2
  image:       5.1.7.3 (needs 11.1.7.1 + 11.1.6.8 + 5.1.8.2)

Sink       2.1.9.1                                    (anytime before 5.1.10.1)
D7 check   6.1.12.1 (anytime) ∥ 9.1.12.2 (after 18.1.2.3) ──► 9.1.12.3 (after 8.1.5.1 + both scripts)

Lane D (deploy — deferred, never blocks code)
  5.1.10.1 (car-sky; needs 5.1.5.4 + 5.1.7.3 + 2.1.9.1) ──► 5.1.10.2 (USER) ──► 2.1.10.3 (also needs 9.1.12.2) ──► 11.1.10.4
                                                                        └─────► 6.1.10.5 (∥ with 2.1.10.3)
```

**Recommended runtime order (single tree):** 5.1.11.1 → 11.1.8.1 → 5.1.8.2 → 7.1.3.5 → 11.1.1.1 → 8.1.2.1 → 7.1.2.2 → 18.1.2.3 → 2.1.2.4 → 7.1.3.1 → 7.1.3.6 → 8.1.3.2 → 8.1.3.3 → 7.1.3.4 → 9.1.4.1 → 9.1.4.2 → 9.1.4.3 → 9.1.4.4 → 9.1.4.5 → 8.1.5.1 → 6.1.12.1 → 9.1.12.2 → 9.1.12.3 → 6.1.5.2 → 6.1.5.3 → 5.1.5.4 → 11.1.6.1 → 11.1.6.2 → 11.1.6.3 → 11.1.6.4 → 11.1.6.5 → 11.1.6.6 → 11.1.6.7 → 11.1.6.8 → 11.1.7.1 → 11.1.7.2 → 5.1.7.3 → 11.1.1.2 → 2.1.9.1 → group 1.10 when unblocked.

## Acceptance traceability

| Milestone Phase 1 box | Closed by |
|---|---|
| Blueprint deploys; every node Running; team APK launches (R5) | 5.1.5.4 · 5.1.7.3 · 5.1.8.2 · 5.1.10.1 · 5.1.10.2 — APK clause open, § Open items item 2 |
| UDP reachability + traffic captured on the bridge (R6) | reachability: Phase 0 smoke test (C1–C5); capture: 6.1.5.2 · 6.1.5.3 · 6.1.10.5 |
| CI import check; telux parity notes + port plan (R7) | 7.1.3.5 (gate) · 7.1.3.6 (doc) · 7.1.3.1 · 7.1.2.2 · 7.1.3.4 (seam substance) |
| Scripted call flow acked/logged; faults → defined logged recovery (R8) | 8.1.3.2 · 8.1.3.3 (unit closure) · 8.1.2.1 · 8.1.5.1 · live evidence in 2.1.10.3 |
| Golden vectors decode; malformed corpus rejected, zero crashes (R9) | golden: Phase 0 `1.0.2.5` + 9.1.4.4; malformed: 9.1.4.5; stages: 9.1.4.1–3 |
| Different scenario configs → observably different streams (R11) | 11.1.6.2 · 11.1.6.4 (model) · 11.1.10.4 (live) — via codec path 11.1.7.1/11.1.7.2 |
| R2 at the ADA ECU with decoded bench values (R2) | 9.1.4.3 · 2.1.2.4 · 2.1.9.1 · 2.1.10.3 |
| **Demo:** Wireshark capture at the V2X interface | 6.1.5.2 · 6.1.5.3 · 6.1.10.5 (D5 dissection caveat noted) |
| Scripted send/capture bench↔V2X; logs demonstrate receive → event → CPM-to-JSON (D7) | 6.1.12.1 · 9.1.12.2 · 9.1.12.3 (CI) · 18.1.2.3 (payload-carrying events) · 2.1.10.3 (on-platform) |
| *(phase task, no box)* R18 evidence stream starts | 18.1.2.3 · `[EVT]` emission 9.1.4.4/8.1.5.1 · bench `[TX]` 11.1.6.7 |

## Open items & flags (no Phase 1 subtask may silently close them)

| # | Item | Owner / closes at |
|---|---|---|
| 1 | *(proposal)* defaults proceed as proposed: `INIT_RETRY_MAX=3`, `RETRY_BACKOFF_MS=500`, `DEDUPE_WINDOW_MS=1500` — externalized as env either way; ratification pending (V2X HLD §11 item 5). Now in service and exercised by the `v2x-comms-check` lane, which overrides `DEDUPE_WINDOW_MS=1` for its back-to-back golden sends — proving the externalization, not the values | user |
| 2 | R5 box clause "the team APK launches on the AAOS node": neither Phase 1 HLD covers IVI work and the deploy keeps the provided AAOS artifact — needs a ruling (build+install the Phase 0 IVI skeleton APK manually, or defer the clause to Phase 5) | user / [[project-architecture]] |
| 3 | arm64 image builds compile Vanetza under QEMU emulation (both C++-bearing images) — build-time risk; mitigated by buildx `gha` cache + raised timeouts in 5.1.8.2; if a lane exceeds its timeout, escalate before hand-rolling cross-compilation. **RESOLVED 2026-08-01 by CI run `30698630956`:** both lanes ran and both images built for `linux/arm64` **within** `timeout-minutes: 360`, first attempt — no cross-compilation escalation needed. The paired cache-eviction concern also did not materialise: the per-image `type=gha` scopes left the `actions/cache` `_deps` entries intact and `v2x-core-build`, `v2x-comms-check` and `sp-codec-helper` stayed green in the same run. The library-skew half of [SP HLD § Open items](../Scenario_Player/doc/phase1-scenario-player-hld.md#10-open-items--flags) item 2 was already closed by the F1 ratification (`e02fb7c`, one shared `python:3.11-slim` base) and is now empirically validated too; nothing residual | closed by CI run `30698630956` |
| 4 | **Retired for Phase 1.** [[car-sky]] was never spawnable in any session, but the agent turned out not to be needed: `CARSKY_ZOT_API_KEY` was already a repo secret, so the gated push step in each image lane ran by itself and closed `5.1.10.1` (run `30698630956`). Nothing in the phase's remaining scope depends on the agent — the 4 open subtasks are Nydus UI work only | closed by CI run `30698630956` |
| 5 | Planner-designated test paths beyond the HLDs' explicit test lists, named per each folder's existing convention (`V2X_ECU/tests/<module>/test_*.cpp`: config, net, log, forward, adapter ×2, `test_rx_pipeline.cpp`; `Scenario_Player/tests/test_*.py`: encoder_client, sender, generator, main) — required by subtask discipline (unit tests per module); flagged to [[project-architecture]] as HLD-consistent additions, not new design | [[project-architecture]] (ack) |
| 6 | `node-v2x-ecu.md` / `node-scenario-player.md` "Build & push" sections still cite `registry.carsky.io`; live host is `registry.hackathon-2.carsky.io` (Phase 0 O1) — guide touch-up owned by architecture; 5.1.10.1 uses the live host meanwhile | [[project-architecture]] |
| 7 | Smoke-test O3 (bridge MTU headroom) still open — nominal CPM is 58 bytes so no Phase 1 risk; optional `PAD` probe may ride a group 1.10 run | group 1.10 (optional) |
| 8 | **R9 "malformed" scope — RESOLVED BY RULING** ([[project-architecture]] owner, 2026-08-01): the 4-reject/3-tolerated finding is a **mis-categorisation in the corpus, not a defect in R9's wording**. Under the frozen [contracts/r1-cpm-profile.md](../contracts/r1-cpm-profile.md) §3, `protocolVersion`/`messageId` are ignored on decode and `CpmContent` carries no header field, so `wrong-message-id`, `wrong-protocol-version` and `r1-variant` are **not malformed inputs at all** — they are valid-but-different-header inputs, and R9's "fully rejected" governs **structurally invalid** input. Consequence: **no frozen-contract change and no requirement re-wording** — neither the R1 codec nor profile §3 nor R9's text is touched. `9.1.4.5` was amended to match: the three are relabelled profile-tolerated negative controls, two genuinely structural fixtures were added (`truncated-mid-object`, `bit-flipped-payload`) along with the honest `trailing-garbage` control (predicted tolerated, because `uper_decode_complete` ignores unconsumed octets), and every case now asserts an explicit expected `Disposition`. **Residual: CI confirmation only** — the new dispositions are predicted, never measured locally, and CI run `30698630956` does **not** confirm them: it ran on `7a02fb5`, the parent of the amendment commit `3202c72`, so the three new fixtures did not exist in the tree it built | **closed** by the ruling + CI run 30700052056 (all 10 dispositions confirmed) |

---

*Created 2026-08-01 by project-planner from the two Phase 1 HLDs and [milestone1.md § Phase 1](milestone1.md#phase-1--comms-bring-up-v2x-ecu--scenario-player-r5r9-r11--r10-moved-to-the-future-plan); amended same day per HLD D7 (`dda1566`): group 1.12 comms check, ninth acceptance box, payload-carrying `[EVT]` events. 12 task groups, 44 subtasks: 39 agent-implemented, 1 car-sky-executed, 4 user-manual. Closeout 2026-08-01: **40 of 44 closed** — every agent-implemented subtask, the image chain (`5.1.5.4`, `5.1.7.3`, `5.1.8.2`) included, plus `5.1.10.1`, which the CI push step executed instead of [[car-sky]] — against three CI runs: `30697863324` on `16b8674` (8 lanes green, the phase's code), `30698630956` on `7a02fb5` (10 lanes green, adding both node images) and `30700052056` on `31d0347` (10 lanes green, confirming the amended R9 dispositions). The 4 remaining are the deploy group's USER-MANUAL items — § Remaining work.*
