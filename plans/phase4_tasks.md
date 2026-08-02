# Phase 4 — Obscured-object Fusion: relayed C + risk + warning (R13–R15): Full Task Breakdown

> **Authority & context:**
> - **Phase content:** [milestone1.md § Phase 4](milestone1.md#phase-4--obscured-object-fusion-relayed-c--risk--warning-r13r15--runs--with-phase-3) — its acceptance checkboxes are the phase output, plus the output-evidence box this plan adds (§ Phase 4 output acceptance).
> - **Design:** [phase2-4-ada-ecu-hld.md](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md) (commit `093f6d6`) — **D4** (CRA interface, registry, database), **D5** (risk vocabulary, thresholds, edge-triggered emission, composition), **D7** (output stage), **D8** (evidence stream), **D9** (capture on this node); §4 folder map; §6 env table.
> - **Requirements:** [m1-cooperative-awareness.md §2](../requirements/m1-cooperative-awareness.md) R2, R4, R5, R6, R13, R14, R15, R18 — referenced by number, never restated.
> - **Phase 2 baseline (do not re-plan):** [phase2_tasks.md § Output](phase2_tasks.md#phase-2-overview) — store, R13 machine, `ICollisionRiskAssessment`, `registry` + `builtin_plugins.cpp`, `assessment_db` + its schema, `event_log`, `udp_socket`, `main.cpp` fusion tick, `tools/check_evt_log.py`, the image and `entrypoint.sh`'s capture hook.
> - **Capture prior art (reuse, do not reinvent):** [traffic-capture-wireshark.md](../requirements/car-sky-guide/traffic-capture-wireshark.md) and the Phase 1 pair `V2X_ECU/capture.sh` (`[6.1.5.2]`) + `V2X_ECU/tools/extract_pcap.sh` (`[6.1.5.3]`) on `main`.
> - **Rules:** [task-planning-conventions.md](../.claude/rules/task-planning-conventions.md); [node-code-layout.md](../.claude/rules/node-code-layout.md).
>
> **Task ID legend:** `X.4.Z.W` — X = requirement served · 4 = this phase · Z = task group · W = subtask position. IDs are stable; never renumber.
>
> **Runs in parallel with Phase 3.** This phase consumes the store, not the detector. Where a real own-sensor B track is needed before Phase 3 lands, the Phase 2 fixture (`tests/fixtures/own_sensor_mock.jsonl`) supplies it.

## Phase 4 overview

**Objective.** Turn live R2 traffic into a tracked ghost C, compose the scene (`d_AC = d_AB + d_BC`), assess NLOS collision risk through the R14 abstraction with the first plugin, and emit R4 warning events to the IVI on every risk transition — with the evidence to prove it, on both the `[EVT]` log and the wire.

**Input (must exist before start):**

- Phase 2 complete (list above). In particular the CRA seam is frozen: this phase writes the **first** plugin and proves the D4 claim that adding one is one new file plus one line.
- A live or mocked R2 source: `ADA_ECU/tools/mock_v2x_sender.py` (`3.2.6.3`) for CI and loopback; the deployed Phase 1 chain (bench → V2X ECU) for the live evidence group.
- **Not required:** Phase 3. The detector's own-sensor B track is interchangeable with the Phase 2 fixture for everything except the live deployed run in group 4.6.

**Output (phase acceptance):**

- [ ] With bench scenarios live, C's track appears with `source = v2x_relayed` only and follows the full R13 lifecycle — closed by Phase 2 `2.2.3.1`/`13.2.4.3` at unit level and by `13.4.6.3` live.
- [ ] The NLOS plugin registers through the CRA interface; the abstraction + database schema are the committed artifacts (R14) — closed by `14.4.1.2` (one new file + one line in `builtin_plugins.cpp`) over Phase 2 `14.2.5.1`/`14.2.5.2`/`14.2.5.4`.
- [ ] At least one R4 warning event per scenario run, carrying the risk state and the composed geometry (R15) — closed by `15.4.5.1` (CI, repeatable) and `15.4.6.5` (live, on the wire).
- [ ] The event list reconstructs a full run offline (R18) — closed by `18.4.3.2` + `18.4.6.4`.
- [ ] **Demo:** ADA logs — collision-risk event list — closed by `18.4.3.2` (`tools/event_report.py`).
- [ ] **Output check (new — § Phase 4 output acceptance):** the TrackedObjects of vehicle **B** and vehicle **C** are both shown to reach the IVI path, evidenced from the ADA `[EVT]` log **and** from a Wireshark/pcap capture of the ADA→IVI Ethernet traffic — closed by `18.4.6.4` (log path) + `15.4.6.5` (wire path), with the contract caveat below.

**Suggested branch (suggestion only — creation is the user's call):** `feat/phase4-ada-fusion-warning`. One branch for the whole phase. It branches from Phase 2's branch (or `main` after Phase 2 merges); it does **not** need Phase 3's branch.

### Execution split legend, subagent spec, subtask discipline

Identical to [phase2_tasks.md § Execution split legend](phase2_tasks.md#execution-split-legend) and § Subtask discipline — not restated. Build commands: the [phase2_tasks.md § Per-node build commands](phase2_tasks.md#per-node-build-commands-cited-in-acceptance-below) table applies unchanged; every C++ subtask's build/tests acceptance is **CI `ada-core-build` green on the pushed branch**.

### CI ruling for this phase

New lane in a new `.github/workflows/phase4-ci.yml` — *a lane belongs to the phase that created it*. One job: `ada-e2e-loopback` (`15.4.5.1`). `ada-core-build` (phase0-ci.yml), `ada-ecu-image` (phase2-ci.yml) and the Phase 3 lanes are reused, never duplicated.

## Phase 4 output acceptance — what "B and C reach the IVI" means, precisely

The user's requirement: *the TrackedObjects of vehicle B and vehicle C are sent to the IVI ECU, checkable from logs **or** from Wireshark captures of the Ethernet message.* This plan delivers **both** paths. One contract fact must be stated before the acceptance is written, because it changes what each path can prove.

**What the frozen [`contracts/r4-ada-ivi.schema.json`](../contracts/r4-ada-ivi.schema.json) warning event actually carries:**

| Element | What it is | Proves |
|---|---|---|
| `object` | the **full R3 TrackedObject** of the *triggering* track — C | C's TrackedObject, on the wire |
| `geometry.vehicleB` | `{x, y}` position only | B's **position**, on the wire |
| `geometry.vehicleC` | `{x, y}` position, null when C's track has been erased | C's composed position |
| `geometry.ego` | `{0, 0}` | the frame origin |

**There is no full R3 TrackedObject for B in the frozen R4 message.** The wire therefore proves *C's TrackedObject and B's position*; B's full TrackedObject is proven from the ADA `[EVT]` stream (`own_sensor_ingest` and `track_transition` carry it).

### Decision: the plan commits to option (i) — accept the frozen R4 as the evidence

**Recommendation and reasoning:**

1. **The user's requirement is disjunctive** — "from logs **or** from Wireshark". Option (i) satisfies it twice over: the `[EVT]` log carries **both** full TrackedObjects, and the pcap independently carries C's full R3 object plus B's position. Nothing is missing from the evidence; only the *transport* of B's full object differs.
2. **Option (ii) changes a frozen contract on the critical path.** CLAUDE.md governing principle 1 forbids changing a frozen contract without re-freezing across every consumer — here that is the ADA `r4_message` binding, the ADA emitter, the golden samples, the synced copies in `ADA_ECU/contracts/` and `IVI_ECU/contracts/`, the IVI Kotlin binding, and the round-trip tests in both languages. That is a Phase 5 dependency added to a Phase 4 deliverable, six days from the 2026-08-08 deadline.
3. **The IVI does not need it.** R17's warning view draws the God view of three vehicles from **positions**; `geometry` already carries all three. The only consumer that ever wanted a `trackedObjects` array is the branch's superseded `R4WarningMessage.kt` — an artifact of the design HLD D1 deletes, superseded by `main`'s `R4Message.kt` ([HLD §11 item 4](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#11-open-items-and-flags)). Re-freezing R4 to satisfy a superseded consumer inverts the dependency.
4. **R15's own acceptance is already met by (i)** — "a pcap on the R6 network showing at least one R4 warning event during a scenario run, which triggers the IVI warning view".

**The Phase 4 output-acceptance box is therefore worded as:**

> With a scenario run live, **(a)** the ADA `[EVT]` log shows a `tracked` `own_sensor` TrackedObject for B **and** a `tracked` `v2x_relayed` TrackedObject for C, both with full R3 fields, plus at least one `r4_tx` carrying the emitted R4 body; **and (b)** a pcap of ADA→IVI UDP traffic on the R6 network decodes to the same R4 body — C's full R3 TrackedObject in `object`, B's position in `geometry.vehicleB`, C's composed position in `geometry.vehicleC` — correlated to the log by timestamp and byte length.

**Option (ii) is planned but not started** — group 4.7, gated on the user's explicit ratification. If the user wants both vehicles as full R3 objects on the wire, that group's four subtasks are the re-freeze, and they must complete **before** Phase 5's IVI binding work starts, not after.

---

## Task Group 4.1 — Scene composition and the NLOS plugin (serves R15, R14)

> The business-logic half. `scene_composer` is geometry, `chained_collision` is rules; neither opens a socket, reads env, or formats a wire message ([HLD §8](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#8-mvc-mapping)).

### [ ] `15.4.1.1` — Scene composer `src/fusion/scene_composer.{hpp,cpp}` *(agent)*

**Objective:** compose the ego-frame scene from the store — the D5 geometry, salvaged as formula from the superseded `warning_builder` and rebuilt against the frozen types.

**Scope:**

- `vehicleB = (d_AB, y_B)` from the **nearest `own_sensor` track**; `vehicleC = (d_AB + d_BC, y_B + y_BC)` — longitudinal sum, lateral component-wise, valid for the near-collinear convoy ([milestone1.md §2](milestone1.md#2-scope--assumptions)). `ego = (0, 0)` always.
- Returns a `SceneGeometry { ego, vehicleB, optional<vehicleC> }`; `vehicleC` is `nullopt` exactly when C's track has been erased — the case the frozen schema allows null for.
- **No B ⇒ no composition:** with no own-sensor track and no remembered `lastKnownB`, the function returns "not composable"; the caller decides what to do (D5's `b_unknown` path is `14.4.1.2`'s).
- Pure: takes a `const TrackStore&` and the values, reads no env, writes no log.
- Test `tests/fusion/test_scene_composer.cpp`: composed values against hand-computed numbers for at least three (d_AB, y_B, d_BC, y_BC) sets; the null-C case; the not-composable case; nearest-B selection when two own-sensor tracks exist.

**Acceptance:** ADA build + ctest green on CI.

**Dependencies:** after Phase 2 `3.2.4.1`. **Commit:** `[15.4.1.1] feat: add ego-frame scene composition`

### [ ] `14.4.1.2` — NLOS plugin `src/cra/plugins/chained_collision.{hpp,cpp}` + registration *(agent)*

**Objective:** the M1 plugin — the SVG's "Chained Collision", registering under the frozen R4 registry key `nlos_obstruction` (D4 naming reconciliation: one concept, three existing names, no new term).

**Scope:**

- Implements `ICollisionRiskAssessment`: `name() == "nlos_obstruction"`; `assess(RiskContext&)` reads the store and the `AssessmentDb`, returns a `RiskFinding`. **The plugin never emits** — the output stage decides transport.
- Band table exactly [D5](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#d5--risk-vocabulary-and-edge-triggered-emission): `high` when C is `tracked` and (`d_AC ≤ RISK_CRITICAL_M` **or** `ttc ≤ RISK_TTC_CRITICAL_S`); `medium` when C is `tracked`, `d_AC ≤ RISK_NEAR_M`, and not `high`; `low` otherwise — including no `tracked` C at all.
- Derived values: `closingRateMps = -(d_AC(t) − d_AC(t−Δ)) / Δ` from the record's `previousDistanceM`/`lastUpdatedMs`; `ttcS = d_AC / closingRate` when the rate is positive, otherwise **null**.
- **`b_unknown` path:** with no own-sensor B and no `lastKnownB`, `d_AC` does not exist — return `low` with rationale `b_unknown` and log `assess_skipped_b_unknown`. Consequence to preserve: no `medium`/`high` is ever entered without a known B, so a clearing event can always fill the required `geometry.vehicleB` from `lastKnownB`.
- DB writes on every assessment: `riskState`, `distanceM`/`previousDistanceM`, `closingRateMps`, `ttcS`, `lastSnapshot` (C's R3 snapshot, carried past its erasure), `lastKnownB`, `lastUpdatedMs`, `rationale`.
- **Risk thresholds are separate constants from the R13 gate and must never alias it** (D5 — that aliasing is what collapsed R14 into R13 in the superseded implementation). `13.2.2.1`'s validator already enforces `RISK_CRITICAL_M < RISK_NEAR_M < GATE_ENTER_M`.
- **Registration: one line** — `registry.add(std::make_unique<ChainedCollision>(cfg))` in `src/cra/builtin_plugins.cpp`. No edit to the interface, the store, the emitter, or any other plugin. That diff *is* R14's acceptance evidence and should be visible in the commit.
- Test `tests/cra/test_chained_collision.cpp` (band-table half): every row of the D5 table including both `high` triggers separately; `ttc` null when not closing; `b_unknown` returns `low` with the rationale; the record round-trips through the DB with the composed values.

**Acceptance:** ADA build + ctest green on CI; the commit touches exactly one new module plus one line of `builtin_plugins.cpp`.

**Dependencies:** after `15.4.1.1` + Phase 2 `14.2.5.1` + `14.2.5.3` + `14.2.5.4`. **Commit:** `[14.4.1.2] feat: add the NLOS chained-collision risk plugin`

### [ ] `14.4.1.3` — Dwell debounce and edge-triggered transitions *(agent)*

**Objective:** exactly one committed transition per real risk change, in **both** directions (D5).

**Scope:**

- A candidate level must hold for `RISK_DWELL_MS` (default 300) before it commits — one debounce covering all three thresholds, **independent** of the R13 gate hysteresis, which protects track identity rather than risk level.
- **Every** change of committed `riskState` for a `(warningType, trackId)` produces exactly one `risk_transition` event and exactly one downstream emission — steady state produces nothing. Downgrades and the return to `low` are transitions too: R4 carries no separate "clear" message and the periodic state stream is optional, so the transition back is the only way the IVI learns to stop warning.
- `assessment` events: on every committed change plus an `ASSESS_LOG_EVERY_MS` heartbeat (a per-tick line would bury the transitions the demo table asks for, D8). Payload carries `d_AC`, `ttc`, rationale.
- Test extends `tests/cra/test_chained_collision.cpp`: a level flapping shorter than the dwell commits nothing; a level held past the dwell commits once and only once; the full `low → medium → high → medium → low` sequence produces five `risk_transition`s and no duplicates; the clearing transition still carries a `lastSnapshot` after C's track was erased.

**Acceptance:** ADA build + ctest green on CI; no test relies on wall-clock sleeps (injectable clock).

**Dependencies:** after `14.4.1.2` + Phase 2 `18.2.2.3`. **Commit:** `[14.4.1.3] feat: debounce and edge-trigger risk transitions`

---

## Task Group 4.2 — R15 output stage (serves R15)

> Controller layer (D7/§8): model → the view model the IVI consumes. `warning_builder` is **the only R4 producer in the node**, so the wire shape cannot drift from the schema the Phase 0 round-trip tests already cover.

### [ ] `15.4.2.1` — Warning builder `src/output/warning_builder.{hpp,cpp}` *(agent)*

**Objective:** map `RiskFinding` + `SceneGeometry` onto the frozen `contracts::R4WarningEvent` and serialize through its binding.

**Scope:**

- Field mapping: `schemaVersion` = the frozen version · `type: "warning"` · `warningType` = the finding's (== the plugin name == the R4 registry key) · `riskState` = the finding's (`low|medium|high`) · `object` = the finding's `trigger` (C's R3 snapshot, from `lastSnapshot` when C's track was erased) · `geometry` = `15.4.1.1`'s composition, `vehicleC` null when C is gone.
- Serialization **only** through `src/contracts/r4_message.hpp` — no hand-built JSON anywhere (the superseded implementation's failure mode).
- Test `tests/output/test_warning_builder.cpp`: the emitted object **validates against the synced `ADA_ECU/contracts/r4-ada-ivi.schema.json`** (loaded from disk) for a `medium` case, a `high` case, and the **null-`vehicleC`** clearing case; `geometry.vehicleB` is always present and non-null (the `b_unknown` invariant); `object` carries all nine R3 fields.

**Acceptance:** ADA build + ctest green on CI; every emitted shape schema-validated in-test.

**Dependencies:** after `15.4.1.1` + `14.4.1.2`. **Commit:** `[15.4.2.1] feat: build R4 warning events from risk findings`

### [ ] `15.4.2.2` — IVI sender `src/output/ivi_sender.{hpp,cpp}` *(agent)*

**Objective:** one UDP datagram per R4 event to `IVI_ECU_HOST:IVI_ECU_PORT`, with the payload-carrying `r4_tx` event.

**Scope:** consumes `net::UdpSocket` only (no socket headers here); one datagram per event; send failure logged and counted, never thrown into the pipeline; logs `r4_tx` carrying the **full R4 body** (the V2X ECU's payload-carrying convention, D8) — this is what makes `18.4.6.4`'s log evidence self-sufficient. Test `tests/output/test_ivi_sender.cpp` (planner-designated path, § Open items item 3): a loopback listener receives the exact JSON of a built warning event; a send to an unreachable host is counted, not fatal; the `r4_tx` event's embedded body parses back to an equal event.

**Acceptance:** ADA build + ctest green on CI.

**Dependencies:** after `15.4.2.1` + Phase 2 `6.2.2.2` + `18.2.2.3`. **Commit:** `[15.4.2.2] feat: add the R4 UDP sender to the IVI`

### [ ] `15.4.2.3` — Wire assessment and emission into the fusion tick *(agent)*

**Objective:** complete the D2 loop in `src/main.cpp` — expire → assess → compose → build → send, on the main thread only.

**Scope:** extend Phase 2's `13.2.6.4` loop: after `store.expire(now)`, for each plugin enabled by `CRA_ENABLED`, call `assess(RiskContext{store, db, now})`; on a committed transition, compose geometry, build the R4 event, send it, and emit `risk_transition` + `r4_tx`. Registration list unchanged (`builtin_plugins.cpp` already carries the plugin from `14.4.1.2`). **Still the single-writer main thread** — no new threads, no locks. No new unit-test file; acceptance is the full suite plus `15.4.5.1`'s lane.

**Acceptance:** ADA build + ctest green on CI; `ada_ecu` links; `15.4.5.1`'s lane observes at least one `r4_tx`.

**Dependencies:** after `14.4.1.3` + `15.4.2.2`. **Commit:** `[15.4.2.3] feat: assess and emit on the fusion tick`

### [ ] `15.4.2.4` — OPTIONAL: periodic awareness state (`STATE_RATE_HZ`) *(agent — build only if time permits)*

**Objective:** R15's optional periodic `R4StateMessage` stream — explicitly deferrable ([milestone1.md § Phase 4](milestone1.md#phase-4--obscured-object-fusion-relayed-c--risk--warning-r13r15--runs--with-phase-3): "the periodic awareness state only if time permits").

**Scope:** when `STATE_RATE_HZ > 0`, a last-value-wins state tick builds an `R4StateMessage` (`type: "state"`, monotonic `seq`, `vehicles{ego, vehicleB, vehicleC|null}`) through the same frozen binding and the same sender. Default `0` — off. The builder and binding already support it (D7), so this is wiring plus a rate timer. Test extends `tests/output/test_warning_builder.cpp` with a state-message schema validation and `seq` monotonicity.

**Acceptance:** ADA build + ctest green on CI; with `STATE_RATE_HZ=0` (the default) **not one state datagram is sent** — asserted, so the option cannot leak into the committed path.

**Dependencies:** after `15.4.2.3`. **OPTIONAL — skip without penalty; the warning event alone triggers and renders the M1 demo.** **Commit:** `[15.4.2.4] feat: add the optional periodic awareness state stream`

---

## Task Group 4.3 — R18 evidence tooling (serves R18)

### [ ] `18.4.3.1` — R4 loopback sink `tools/mock_ivi_receiver.py` *(agent)*

**Objective:** the IVI stand-in for loopback and CI runs — receive R4 datagrams and make them checkable.

**Scope:** `ADA_ECU/tools/mock_ivi_receiver.py`, Python 3 stdlib; binds a host/port from CLI args/env (**no hardcoded peer**); prints one line per datagram (sequence, byte length, `type`, `warningType`, `riskState`) and appends the full body to an optional JSONL output file; `--validate` mode additionally validates each body against the synced `ADA_ECU/contracts/r4-ada-ivi.schema.json`; `--expect-min N` exits non-zero when fewer than N warning events arrived. Test equipment only — never enters the image.

**Acceptance:** `python -m py_compile` passes; a loopback self-check receives a sample R4 body byte-identical and validates it — evidence in the Status line.

**Dependencies:** none. **Commit:** `[18.4.3.1] feat: add the R4 loopback receiver`

### [ ] `18.4.3.2` — Collision-risk event list `tools/event_report.py` *(agent)*

**Objective:** the §1 demo-table artifact (D8) — render an `[EVT]` stream as the collision-risk event list a human reads.

**Scope:** `ADA_ECU/tools/event_report.py`, Python 3 stdlib; input = a saved `[EVT]` log (file or stdin), tolerating interleaved `[CAP]` lines; selects the `track_transition` + `risk_transition` + `r4_tx` subset and renders a chronological table — time, track id, source, state change, `d_AC`, `ttc`, risk level, whether an R4 was emitted; a `--summary` footer counting tracks admitted, transitions per level, and R4 events sent. **This is the "event list reconstructs a full run offline" acceptance made concrete**, so it must work on a log alone with no live process.

**Acceptance:** `python -m py_compile` passes; rendering a synthetic full-run log produces a table whose row count and R4 count match the log's — evidence in the Status line.

**Dependencies:** after `14.4.1.3` + `15.4.2.2` (event vocabulary complete). **Commit:** `[18.4.3.2] feat: add the collision-risk event report tool`

### [ ] `18.4.3.3` — Extend `tools/check_evt_log.py` with the Phase 4 chain *(agent)*

**Objective:** scripted assertion of the full ADA chain, including the **both-tracks** check that the phase's output acceptance rests on.

**Scope — additive modes on Phase 2's script:**

- `--fusion`: per relayed track, `r2_ingest → track_transition → assessment → risk_transition → r4_tx` is complete; every `risk_transition` has exactly one matching `r4_tx`; no `r4_tx` without a preceding `risk_transition` (edge-triggered, D5); a `b_unknown` run produces `assess_skipped_b_unknown` and **no** `r4_tx`.
- `--both-tracks`: exits 0 only when the log contains **a `tracked` `own_sensor` TrackedObject (B) with all nine R3 fields and a `tracked` `v2x_relayed` TrackedObject (C) with all nine R3 fields**, plus at least one `r4_tx` whose embedded body carries C's R3 object in `object` and a non-null `geometry.vehicleB`. Non-zero exit naming which of the four is missing.
- `--r4-schema <path>`: validate every embedded `r4_tx` body against the synced R4 schema.

**Acceptance:** `python -m py_compile` passes; demonstrated exit 0 on a synthetic complete log and non-zero on each of: a missing `r4_tx`, an `r4_tx` with no preceding transition, a log with only B, a log with only C, and an `r4_tx` with null `geometry.vehicleB` — evidence in the Status line.

**Dependencies:** after `15.4.2.3` + Phase 2 `18.2.6.5`. **Commit:** `[18.4.3.3] feat: assert the fusion chain and both-tracks evidence in the EVT checker`

---

## Task Group 4.4 — ADA→IVI traffic capture (serves R6, R15, R19; HLD D9)

> The V2X ECU's capture point cannot see this hop, so this node carries its own capture. The scripts are **duplicated per folder, not shared** — self-contained build contexts, no cross-node imports ([node-code-layout.md](../.claude/rules/node-code-layout.md)); the host-side procedure is the shared [traffic-capture-wireshark.md](../requirements/car-sky-guide/traffic-capture-wireshark.md).

### [ ] `6.4.4.1` — In-container capture `ADA_ECU/capture.sh` *(agent)*

**Objective:** live `[CAP]` text plus a rotating pcap exported through View Log — the ADA copy of the proven `V2X_ECU/capture.sh` (`[6.1.5.2]`).

**Scope:** port `V2X_ECU/capture.sh` to `ADA_ECU/capture.sh` with **no format changes** — the `[PCAP-BEGIN <name>]` / `[PCAP-END]` base64 markers must stay byte-compatible with the extraction tooling. Env, no literals: `CAPTURE_FILTER` (default `udp`), `PCAP_DIR` (`/data/capture`, `mkdir -p`), `CAPTURE_ROTATE_S` (60). Two tcpdump processes: `-i any -n -l -tttt $CAPTURE_FILTER` prefixed `[CAP]` to stdout, and `-w` rotating with each closed file base64-emitted between the markers. Degrades gracefully when `NET_RAW` is not honoured (log and stay alive). Phase 2's `entrypoint.sh` already starts it when present — no entrypoint change needed.

**Acceptance:** `sh -n` and `bash -n` clean; LF line endings; exec bit set; `--export-one` round-trip base64-decodes byte-identically. Runtime tcpdump evidence lands at `15.4.6.5`.

**Dependencies:** after Phase 2 `5.2.7.1`. **Commit:** `[6.4.4.1] feat: add the ADA to IVI tcpdump capture script`

### [ ] `6.4.4.2` — Host-side extraction `ADA_ECU/tools/extract_pcap.sh` *(agent — parallel)*

**Objective:** saved View Log in → `.pcap` files out, for the ADA node — the ADA copy of `V2X_ECU/tools/extract_pcap.sh` (`[6.1.5.3]`).

**Scope:** port verbatim; host tool, never shipped in the image (`.dockerignore` already excludes `tools/`); for each marker block: strip, base64-decode, write `<name>.pcap` beside the input log; multiple blocks per log; non-zero exit with a message when no block is found; sanitize the block name against path escape.

**Acceptance:** `bash -n` clean; a round-trip through `6.4.4.1`'s `--export-one` producer extracts byte-identically (`cmp` clean) — evidence in the Status line.

**Dependencies:** none (marker format is frozen by the Phase 1 pair). **Commit:** `[6.4.4.2] feat: add the host-side ADA pcap extraction script`

---

## Task Group 4.5 — End-to-end loopback CI lane (serves R15, R18)

### [ ] `15.4.5.1` — `phase4-ci.yml` + lane `ada-e2e-loopback` *(agent)*

**Objective:** the repeatable, machine-checked form of the phase's output acceptance — everything except the deployed Room.

**Scope:** create `.github/workflows/phase4-ci.yml` (same `on:`/`concurrency:` block and header-comment convention as [phase1-ci.yml](../.github/workflows/phase1-ci.yml)) with one job `ada-e2e-loopback`:

1. Build the `ada_ecu` target (reusing the `ada-core-build` configure step's shape).
2. Start `tools/mock_ivi_receiver.py --validate --expect-min 1` on a loopback port.
3. Start `ada_ecu` with `IVI_ECU_HOST=127.0.0.1`, `IVI_ECU_PORT=<that port>`, `DETECTOR_ENABLED=true`, `DETECTOR_CMD="cat ADA_ECU/tests/fixtures/own_sensor_mock.jsonl"`, the §6 defaults otherwise, stdout captured.
4. Drive `tools/mock_v2x_sender.py --profile approaching` — C closing from 60 m through the gate and the risk bands.
5. SIGTERM; then assert **all** of: `check_evt_log.py --admission --fusion --both-tracks --r4-schema ADA_ECU/contracts/r4-ada-ivi.schema.json` exit 0 · the receiver saw ≥ 1 schema-valid warning event · `event_report.py` renders a non-empty event list.
6. Second arm with `--profile out-of-range`: C stays beyond the exit gate, so the run must produce **zero** `r4_tx` — the negative control that stops the lane passing on any traffic at all.

**Acceptance:** lane green on the pushed branch, both arms; the approaching arm observes a `low → medium → high` progression (the D5 defaults are chosen so `default.yaml`'s approach produces exactly that).

**Dependencies:** after `15.4.2.3` + `18.4.3.1` + `18.4.3.2` + `18.4.3.3`. **Commit:** `[15.4.5.1] chore: add the ADA end-to-end loopback CI lane`

---

## Task Group 4.6 — Deploy and live evidence (serves R5, R13, R15, R18)

> Split per [HLD §9](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#9-deployment-shape-r5r6): image build/push and node-config **values** are [[car-sky]]-executable; blueprint edits and deploy/verify clicks are user Nydus UI steps (REST cannot edit node config — the Phase 1 finding). Evidence accumulates in `plans/doc/phase4-ada-fusion-run.md`.

### [ ] `5.4.6.1` — Build and push `m1-ada-ecu:latest` *(car-sky, or the CI push step)*

**Objective:** the registry holds a current ADA image built from the phase's code.

**Scope:** [[car-sky]] runs [carsky-deploy-preflight](../.claude/skills/carsky-deploy-preflight/SKILL.md) — which blueprint, which node, which credential — then ensures `registry.hackathon-2.carsky.io/m1-ada-ecu:latest` is current. In practice the push happens in the `ada-ecu-image` lane whenever `CARSKY_ZOT_API_KEY` is present (the Phase 1 precedent: the agent was never needed there). Create `plans/doc/phase4-ada-fusion-run.md` recording the push.

**Acceptance:** the tag is pullable; the record exists. **Standing hazard:** the tag is mutable and every branch push re-pushes it — identify the deployed image at deploy time, never from an old run log.

**Dependencies:** after `15.4.2.3` + `6.4.4.1` + Phase 3 `5.3.6.1` (so the deployed image carries the detector too). **Commit:** `[5.4.6.1] docs: record the phase 4 ADA image push`

### [ ] `5.4.6.2` — USER-MANUAL: node config + deploy → ADA node Running *(user, Nydus UI)*

**Objective:** the ADA node runs in the Room with the D9 configuration, alongside the Phase 1 bench and V2X nodes.

**Scope:** ADA node `.12` per [node-ada-ecu.md § Blueprint node config](../requirements/car-sky-guide/node-ada-ecu.md) as updated by `5.2.9.4` — image `…/m1-ada-ecu:latest`, `command: ["./entrypoint.sh"]`, `capabilities: ["NET_RAW"]`, the §6 env set with `V2X_LISTEN_PORT=47200` and `IVI_ECU_HOST=10.99.0.13`/`IVI_ECU_PORT=47300`. The ADA node replaces the Phase 1 netcheck sink at `.12`. Bench + V2X nodes keep their Phase 1 config. New Deployment → Deployment Viewer shows every node Running, restart 0; mind the 2-deployment quota.

**Acceptance:** per-node Running badges + restart 0 recorded in `plans/doc/phase4-ada-fusion-run.md` (the Deployment Viewer summary header is unreliable — the Phase 1 finding); evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `5.4.6.1`. **Commit:** `[5.4.6.2] docs: record the phase 4 ADA node config and Running evidence`

### [ ] `13.4.6.3` — USER-MANUAL: live R13 lifecycle of relayed C across both scenarios *(user, Nydus UI)*

**Objective:** the first Phase 4 box, live — C's track appears with `source = v2x_relayed` only and follows the full R13 lifecycle, and a scenario swap changes it.

**Scope:** with `SCENARIO_CONFIG=/app/scenarios/default.yaml` on the bench, save the ADA node View Log and run `python ADA_ECU/tools/check_evt_log.py --admission --fusion <saved.log>` (exit 0) plus `event_report.py`; then swap the bench to `c-out-of-range.yaml`, redeploy (config only, no rebuild), and repeat — the second run must show C never admitted and **zero** `r4_tx`. Confirm by inspection that every C track carries `source: v2x_relayed`, never `own_sensor`.

**Acceptance:** both log sets and both tool outputs recorded in `plans/doc/phase4-ada-fusion-run.md`; evidence commit by the orchestrating session.

**Dependencies:** after `5.4.6.2`. **Commit:** `[13.4.6.3] docs: record the live relayed-C admission evidence`

### [ ] `18.4.6.4` — USER-MANUAL: **evidence path A** — both TrackedObjects in the `[EVT]` log *(user, Nydus UI)*

**Objective:** the log half of § Phase 4 output acceptance, and the R18 "event list reconstructs a full run offline" box.

**Scope:**

- Save the ADA node View Log over a full `default.yaml` run.
- Run `python ADA_ECU/tools/check_evt_log.py --both-tracks --r4-schema ADA_ECU/contracts/r4-ada-ivi.schema.json <saved.log>` → **exit 0**, which asserts: a `tracked` `own_sensor` TrackedObject for **B** with all nine R3 fields; a `tracked` `v2x_relayed` TrackedObject for **C** with all nine R3 fields; ≥ 1 `r4_tx` whose embedded body carries C's R3 object in `object` and a non-null `geometry.vehicleB`.
- Run `python ADA_ECU/tools/event_report.py <saved.log>` and record the rendered collision-risk event list — this is the §1 demo-table artifact.
- Record the two excerpts (B's and C's TrackedObject lines) verbatim in the run doc.

**Acceptance:** both tool outputs and the two excerpts recorded in `plans/doc/phase4-ada-fusion-run.md`; evidence commit by the orchestrating session.

**Dependencies:** after `5.4.6.2` + `18.4.3.2` + `18.4.3.3`. **Commit:** `[18.4.6.4] docs: record the both-tracks EVT log evidence`

### [ ] `15.4.6.5` — USER-MANUAL: **evidence path B** — ADA→IVI pcap in Wireshark *(user, Nydus UI)*

**Objective:** the wire half of § Phase 4 output acceptance, R15's own acceptance (a pcap showing ≥ 1 R4 warning event), and the ADA half of R19's corroborating capture.

**Scope — per [traffic-capture-wireshark.md](../requirements/car-sky-guide/traffic-capture-wireshark.md), retargeted to this node:**

- The Room must run at least one `CAPTURE_ROTATE_S` period so a `[PCAP-BEGIN]` block exists; save the ADA node View Log.
- Run `ADA_ECU/tools/extract_pcap.sh <saved.log>`; open the `.pcap` in Wireshark; filter `udp.port == 47300`.
- **R4 payloads are plain JSON and read directly in the packet-bytes pane** — no dissector caveat applies here (unlike the V2X hop's raw UPER). Record, from the capture itself: `warningType`, `riskState`, the **full `object`** (C's R3 TrackedObject), `geometry.vehicleB`, `geometry.vehicleC`.
- Correlate to `18.4.6.4`'s log by timestamp and datagram length — the same event on both paths.
- **What this proves and what it does not:** C's full TrackedObject and B's *position* are on the wire; **B's full TrackedObject is proven by path A, not by the capture** — the frozen R4 does not carry it (§ Phase 4 output acceptance). If the user requires B's full object on the wire, that is group 4.7, and it must be ratified first.

**Acceptance:** the `.pcap` archived and the decoded fields recorded in `plans/doc/phase4-ada-fusion-run.md`, showing ≥ 1 R4 warning event; evidence commit by the orchestrating session.

**Dependencies:** after `5.4.6.2` + `6.4.4.1` + `6.4.4.2` (parallel with `18.4.6.4`). **Commit:** `[15.4.6.5] docs: record the ADA to IVI pcap evidence`

---

## Task Group 4.7 — OPTION (ii): additive `trackedObjects` on R4 — **NOT STARTED, gated on user ratification** (serves R4)

> Planned in full so the cost is visible, and **blocked** so no dependent work starts by accident. This group changes a **frozen contract** and therefore requires a re-freeze across every consumer (CLAUDE.md governing principle 1). The planner's recommendation is **not** to run it — see § Phase 4 output acceptance. If the user ratifies it, all four subtasks run as one wave **before** Phase 5's IVI binding work, and `18.4.3.3`/`15.4.6.5` are amended to assert B's full object on the wire.
>
> **User ratification:** *(not given — group is not started)*

### [ ] `4.4.7.1` — Add `trackedObjects` to `contracts/r4-ada-ivi.schema.json` and re-sync *(agent — gated)*

**Objective:** the contract change itself, additive and optional.

**Scope:** add an **optional** `trackedObjects` array of `$ref: r3-tracked-object.schema.json` to `warningEvent` (not in `required` — an omitted array must stay valid, so every existing golden sample keeps validating); document that it carries every `tracked` object in the store, including B and C, and that `object` remains the triggering track; bump the `schemaVersion` guidance in the description; update `contracts/samples/r4-warning.json` to carry the array; re-sync the byte-identical copies into `ADA_ECU/contracts/` and `IVI_ECU/contracts/` per `contracts/sync-manifest.json`.

**Acceptance:** `python contracts/check_sync.py` exits 0; the existing samples still validate (proving the change is genuinely additive); CI `contracts-gate` green.

**Dependencies:** user ratification. **Commit:** `[4.4.7.1] feat: add the optional trackedObjects array to R4`

### [ ] `4.4.7.2` — ADA binding + emitter + round-trip tests *(agent — gated)*

**Objective:** the producer side of the re-freeze.

**Scope:** extend `ADA_ECU/src/contracts/r4_message.{hpp,cpp}` with the optional array; extend `src/output/warning_builder` to populate it from the store's `tracked` objects; extend `tests/contracts/test_r4_roundtrip.cpp` and `tests/output/test_warning_builder.cpp` to cover present-and-absent arrays and to schema-validate both. The additive-version test (`test_r4_additive_version.cpp`) must still pass unchanged — that is the check that the change did not break tolerance.

**Acceptance:** ADA build + ctest green on CI, with the additive-version test untouched and passing.

**Dependencies:** after `4.4.7.1`. **Commit:** `[4.4.7.2] feat: emit trackedObjects in the ADA R4 warning event`

### [ ] `4.4.7.3` — IVI Kotlin binding + round-trip *(agent — gated; hands to Phase 5)*

**Objective:** the consumer side of the re-freeze — without it the contract is changed on one side only, which is the exact failure CLAUDE.md principle 1 forbids.

**Scope:** extend `main`'s `IVI_ECU/.../model/R4Message.kt` (**not** the branch's superseded `R4WarningMessage.kt`, [HLD §11 item 4](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#11-open-items-and-flags)) with the optional list and its `@SerialName`s; extend the Kotlin round-trip test to decode a body **with** and **without** the array; CI `ivi-unit-tests` green. Rendering work stays Phase 5's.

**Acceptance:** `ivi-unit-tests` green; a body without the array still decodes.

**Dependencies:** after `4.4.7.1`. **Commit:** `[4.4.7.3] feat: decode trackedObjects in the IVI R4 binding`

### [ ] `4.4.7.4` — Amend the evidence checks to assert B on the wire *(agent — gated)*

**Objective:** make the new field actually load-bearing for acceptance, rather than shipping an unused array.

**Scope:** extend `tools/check_evt_log.py --both-tracks` to require B's **full** R3 object inside the `r4_tx` body's `trackedObjects`; amend `15.4.6.5`'s recorded fields to include it; amend § Phase 4 output acceptance's wording from option (i) to option (ii).

**Acceptance:** `15.4.5.1`'s lane green with the stricter assertion; the amended wording committed.

**Dependencies:** after `4.4.7.2` + `4.4.7.3`. **Commit:** `[4.4.7.4] test: assert both tracked objects on the R4 wire`

---

## Execution order & parallelism

```
tooling    18.4.3.1                                    (anytime)
capture    6.4.4.1 (after phase-2 5.2.7.1) ∥ 6.4.4.2   (anytime)
rules      15.4.1.1 ──► 14.4.1.2 ──► 14.4.1.3
output     15.4.2.1 (after 15.4.1.1 + 14.4.1.2) ──► 15.4.2.2 ──► 15.4.2.3 (after 14.4.1.3)
                                                       └──► 15.4.2.4 (OPTIONAL)
evidence   18.4.3.2 (after 14.4.1.3 + 15.4.2.2) ∥ 18.4.3.3 (after 15.4.2.3)
CI         15.4.5.1 (after 15.4.2.3 + 18.4.3.1 + 18.4.3.2 + 18.4.3.3)

Lane D (deploy - never blocks code)
  5.4.6.1 (needs 15.4.2.3 + 6.4.4.1 + phase-3 5.3.6.1) ──► 5.4.6.2 (USER) ──► 13.4.6.3 (USER)
                                                                        ├──► 18.4.6.4 (USER)
                                                                        └──► 15.4.6.5 (USER, ∥ with 18.4.6.4)

Group 4.7  BLOCKED on user ratification; if ratified: 4.4.7.1 ──► 4.4.7.2 ∥ 4.4.7.3 ──► 4.4.7.4
```

**Recommended runtime order (single tree):** 18.4.3.1 → 6.4.4.1 → 6.4.4.2 → 15.4.1.1 → 14.4.1.2 → 14.4.1.3 → 15.4.2.1 → 15.4.2.2 → 15.4.2.3 → 18.4.3.2 → 18.4.3.3 → 15.4.5.1 → 15.4.2.4 *(if time)* → group 4.6 when a Room is available.

**Shared-file sequencing with Phase 3:** `ADA_ECU/CMakeLists.txt` (this phase adds targets, Phase 3 adds none) and `ADA_ECU/Dockerfile` (Phase 3 adds `COPY detector/ models/ media/`, this phase adds nothing — `capture.sh` is picked up by Phase 2's existing entrypoint guard, but the runtime stage must `COPY capture.sh`, which is a one-line edit to sequence against Phase 3's).

## Acceptance traceability

| Milestone Phase 4 box | Closed by |
|---|---|
| C tracked with `source = v2x_relayed` only, full R13 lifecycle | Phase 2 `2.2.3.1` (source set at parse) + `13.2.4.3` (lifecycle) · live: 13.4.6.3 |
| NLOS plugin registers through the CRA interface; abstraction + DB schema are the artifacts (R14) | 14.4.1.2 (one file + one line) over Phase 2 `14.2.5.1`/`14.2.5.2`/`14.2.5.3`/`14.2.5.4` |
| ≥ 1 R4 warning per scenario run with risk state and composed geometry (R15) | 15.4.1.1 · 15.4.2.1 · 15.4.2.2 · 15.4.2.3 · CI 15.4.5.1 · live 15.4.6.5 |
| The event list reconstructs a full run offline (R18) | 18.4.3.2 · 18.4.3.3 · 18.4.6.4 · payload-carrying events from Phase 2 `18.2.2.3` |
| **Demo:** ADA logs — collision-risk event list | 18.4.3.2 · 18.4.6.4 |
| **Output check:** B's and C's TrackedObjects reach the IVI path, evidenced by log **and** pcap | **path A** 18.4.6.4 (both full R3 objects + `r4_tx`) · **path B** 15.4.6.5 (C's full R3 object + B's position on the wire) · caveat and decision in § Phase 4 output acceptance |
| R15/R19 — pcap of ADA→IVI traffic | 6.4.4.1 · 6.4.4.2 · 15.4.6.5 |

## Open items & flags (no Phase 4 subtask may silently close them)

| # | Item | Owner / closes at |
|---|---|---|
| 1 | **Contract tension on the Phase 4 output evidence — decided as option (i), flagged for the user.** The frozen R4 carries C's full R3 TrackedObject and B's *position*, not B's TrackedObject. This plan accepts that and proves B's full object from the `[EVT]` log. **Option (ii)** — an additive `trackedObjects` array — is planned in full as group 4.7 and **not started**; it is a frozen-contract change requiring a re-freeze across the ADA binding, the ADA emitter, the golden samples, both synced copy sets, the IVI Kotlin binding and both languages' round-trip tests. Recommendation: **do not run it** — reasoning in § Phase 4 output acceptance | **user** (ratification), group 4.7 |
| 2 | **`(proposal)` risk defaults proceed as proposed** — `RISK_NEAR_M=25`, `RISK_CRITICAL_M=15`, `RISK_TTC_WARN_S=6`, `RISK_TTC_CRITICAL_S=3`, `RISK_DWELL_MS=300`, `ASSESS_LOG_EVERY_MS=1000` ([HLD §6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#6-configuration--no-hardcoded-tunables)). Chosen so `default.yaml`'s approach produces a visible `low → medium → high` progression and `c-out-of-range.yaml` never leaves `low`. Externalized, so ratification is a node-config edit | user |
| 3 | **Planner-designated test/tool paths beyond the HLD's list**: `tests/output/test_ivi_sender.cpp`; the `--fusion` / `--both-tracks` / `--r4-schema` modes of `tools/check_evt_log.py`. Flagged to [[project-architecture]] as HLD-consistent additions | [[project-architecture]] (ack) |
| 4 | **Cross-phase dependency, not this phase's work:** the branch's `IVI_ECU/app/.../model/R4WarningMessage.kt` cannot decode this design's output — no `@SerialName` on `R4Geometry(ego, b, c)`, and it requires a `trackedObjects` array this design does not emit. **`main`'s `R4Message.kt` (sealed `R4WarningEvent`/`R4StateMessage` + `SceneGeometry.kt`) is the binding the IVI uses; the branch's parallel model is superseded.** Fixing it is **Phase 5's** first task ([HLD §11 item 4](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#11-open-items-and-flags)) — recorded here so it is not lost, planned nowhere in this file | [[project-planner]] → Phase 5 |
| 5 | **`15.4.2.4` (periodic awareness state) is optional and may be dropped** without affecting any acceptance box — R15 words it optional and the warning event alone renders the M1 demo. It must stay off by default (`STATE_RATE_HZ=0`), asserted in test | Phase 4, if time permits |
| 6 | **Phase 6 inherits this phase's live evidence.** `15.4.6.5`'s pcap is the ADA half of R19's corroborating capture; the V2X half is Phase 1's `6.1.10.5`. Phase 6 re-records both in one continuous run — it does not re-derive them | [[project-planner]] → Phase 6 |

---

*Created 2026-08-02 by project-planner from [phase2-4-ada-ecu-hld.md](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md) D4/D5/D7/D8/D9 and [milestone1.md § Phase 4](milestone1.md#phase-4--obscured-object-fusion-relayed-c--risk--warning-r13r15--runs--with-phase-3). 7 task groups, 22 subtasks: 13 agent-implemented (1 of them optional), 1 car-sky, 4 user-manual, 4 gated on user ratification (group 4.7, not started). Planned from zero.*
