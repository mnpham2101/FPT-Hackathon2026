# Phase 4 — Obscured-object Fusion: relayed C + risk + warning (R13–R15): Full Task Breakdown

> **Authority & context:**
> - **Phase content:** [milestone1.md § Phase 4](milestone1.md#phase-4--obscured-object-fusion-relayed-c--risk--warning-r13r15--runs--with-phase-3) — its acceptance checkboxes are the phase output, plus the output-evidence box this plan adds (§ Phase 4 output acceptance).
> - **Design:** [phase2-4-ada-ecu-hld.md](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md) (commit `093f6d6`) — **D4** (CRA interface, registry, database), **D5** (risk vocabulary, thresholds, edge-triggered emission, composition), **D7** (output stage), **D8** (evidence stream), **D9** (capture on this node); §4 folder map; §6 env table.
> - **Requirements:** [m1-cooperative-awareness.md §2](../requirements/m1-cooperative-awareness.md) R2, R4, R5, R6, R13, R14, R15, R18 — referenced by number, never restated.
> - **Run timing:** [m1-run-timing-and-event-triggering.md](../requirements/m1-run-timing-and-event-triggering.md) — R20/R21, the §6.4 K1–K5 checks that group 4.8 implements, and the §6.2 clock-domain ruling. Its §8(1) fixes this group's schedule: demo-quality, behind every acceptance box, never in front.
> - **Phase 2 baseline (do not re-plan):** [phase2_tasks.md § Output](phase2_tasks.md#phase-2-overview) — store, R13 machine, `ICollisionRiskAssessment`, `registry` + `builtin_plugins.cpp`, `assessment_db` + its schema, `event_log`, `udp_socket`, `main.cpp` fusion tick, `tools/check_evt_log.py`, the image and `entrypoint.sh`'s capture hook.
> - **Capture prior art (reuse, do not reinvent):** [traffic-capture-wireshark.md](../requirements/car-sky-guide/traffic-capture-wireshark.md) and the Phase 1 pair `V2X_ECU/capture.sh` (`[6.1.5.2]`) + `V2X_ECU/tools/extract_pcap.sh` (`[6.1.5.3]`) on `main`.
> - **Deployment & verification procedure (groups 4.6, 4.9, 4.10, 4.11):** [deploy-ada-ecu-walkthrough.md](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md) — the stage-1 artifact those groups are decomposed from, per [walkthrough-driven-delivery.md](../.claude/rules/walkthrough-driven-delivery.md). Its [§7 work division](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#7-work-division-between-ai-and-human) fixes each subtask's executor and its [§8 expected outputs and acceptance](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance) fixes each check's criteria. **Cite, never restate** — commands stay in the walkthrough.
> - **Rules:** [task-planning-conventions.md](../.claude/rules/task-planning-conventions.md); [node-code-layout.md](../.claude/rules/node-code-layout.md); [walkthrough-driven-delivery.md](../.claude/rules/walkthrough-driven-delivery.md).
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

**Suggested branch (suggestion only — creation, checkout and push are the user's call):** `feat/phase4-ada-fusion-warning`. One branch for the whole phase, per [task-planning-conventions.md § Branch suggestion](../.claude/rules/task-planning-conventions.md#branch-suggestion-per-phase). It branches from Phase 2's branch (or `main` after Phase 2 merges); it does **not** need Phase 3's branch. Groups 4.9–4.11 share no file with the fusion code — they write only under `tools/ada-bench/`, `.github/workflows/`, `requirements/car-sky-guide/` and `plans/doc/` — so if the user prefers to run the bring-up lane separately, `feat/phase4-ada-isolated-room` is a clean alternative that merges without conflict. Plan and run-doc commits go straight to `main` either way.

### Two Rooms, two routes — which group runs where

[deploy-ada-ecu-walkthrough.md §1.4](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#14-blueprints) defines two blueprints, and this phase uses both in order. Nothing about the ADA node's own config differs between them ([§5.6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#56-the-full-blueprint-route)); only the neighbours change.

| Route | Blueprint | Groups | Depends on |
|---|---|---|---|
| **Isolated — run this first** | bridge + V2X bench mock `.11` + ADA `.12` + IVI sink mock `.13` ([§2.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#21-topology)) | **4.9 → 4.10 → 4.11** | Nothing outside `ADA_ECU/` and `tools/ada-bench/`. No V2X ECU, no Scenario Player, no IVI app |
| **Full chain — after it passes** | the 5-node blueprint ([carsky-4-node-blueprint.md](../requirements/car-sky-guide/carsky-4-node-blueprint.md)), per [§5.6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#56-the-full-blueprint-route) | **4.6** | Phase 1's deployed comms chain and, for the consumer half, Phase 5's app |

**The isolated route is the cheaper first attempt and is not optional scaffolding**: it puts every ADA-side failure mode — wrong port, drifted message shape, a detector that never spawns, a risk level that never changes — in a Room where both neighbours are under this phase's own control. Discovering any of them inside the full chain costs a second unknown to eliminate first.

### Execution split legend, subagent spec, subtask discipline

Identical to [phase2_tasks.md § Execution split legend](phase2_tasks.md#execution-split-legend) and § Subtask discipline — not restated. Build commands: the [phase2_tasks.md § Per-node build commands](phase2_tasks.md#per-node-build-commands-cited-in-acceptance-below) table applies unchanged; every C++ subtask's build/tests acceptance is **CI `ada-core-build` green on the pushed branch**.

### CI ruling for this phase

New lane in a new `.github/workflows/phase4-ci.yml` — *a lane belongs to the phase that created it*. Three jobs: `ada-e2e-loopback` (`15.4.5.1`), `ada-bench-image` (`5.4.9.5`) and `ada-bench-selfcheck` (`2.4.9.7`). Whichever of the three lands first creates the file with the standard `on:`/`concurrency:`/header block; **the three edits are sequenced against each other** and are the only shared-file contention inside this phase. `ada-core-build` (phase0-ci.yml), `ada-ecu-image` (phase2-ci.yml — checked against the walkthrough's build table by `5.4.9.6`, never re-created) and the Phase 3 lanes are reused, never duplicated.

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

## Task Group 4.6 — Full-blueprint deploy and live evidence (serves R5, R13, R15, R18)

> **Route corrected 2026-08-03.** This group is the **full-blueprint** run of [deploy-ada-ecu-walkthrough.md §5.6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#56-the-full-blueprint-route) — the ADA node in the 5-node Room beside the Phase 1 bench and V2X ECU. It was planned before the walkthrough existed and read as if that were the only route; it is not. **Groups 4.9–4.11 run first, on the isolated blueprint**, and prove everything this group proves except what only the real neighbours can show. §5.6 is explicit that the mechanics are identical and the ADA node is *not reconfigured* between the two — so nothing below is re-derived for the isolated Room.
>
> **What §5.6 changes for this group, and nothing else does:** the relayed traffic originates in the real V2X ECU driven by a bench scenario, so `STATION_ID`, `OBJECT_ID` and the distance profile come from that scenario and [§5.5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#55-retune-when-no-warning-is-emitted)'s `MIN_DISTANCE_M` lever is unavailable; and there is **no sink log**, because the Android node runs no container — so the `[RX]`/`[CHECK]`/`[SUMMARY]`/`[CAP]` evidence of `15.4.11.4` has no counterpart here and the ADA node's own capture (`6.4.4.1`) stops being optional.
>
> Split per [HLD §9](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#9-deployment-shape-r5r6) and [walkthrough §7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#7-work-division-between-ai-and-human): image build/push, registry confirmation, config read-back, phase polling and log reading are AI rows performed by [[car-sky]]; canvas edits, node-config edits, the deploy and the teardown are Human rows no agent performs. The *USER-MANUAL* subtasks below keep the human half and the record-keeping; their AI halves are the group 4.10/4.11 subtasks, **re-run against this Room under the same briefs** rather than given new IDs. Evidence accumulates in `plans/doc/phase4-ada-fusion-run.md`; the isolated run's evidence is a separate document, `plans/doc/phase4-ada-isolated-room-run.md`.

### [ ] `5.4.6.1` — Build and push `m1-ada-ecu:latest` *(car-sky, or the CI push step)*

**Objective:** the registry holds a current ADA image built from the phase's code.

**Scope:** [[car-sky]] runs [carsky-deploy-preflight](../.claude/skills/carsky-deploy-preflight/SKILL.md) — which blueprint, which node, which credential — then ensures `registry.hackathon-2.carsky.io/m1-ada-ecu:latest` is current. **No image is built by hand, by anyone** ([walkthrough §3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#3-build-the-images-on-ci)): the push happens in the `ada-ecu-image` lane (`5.2.8.1`) on every commit push whenever `CARSKY_ZOT_API_KEY` is present, which is the Phase 1 precedent. The job's context, tag, registry host and platform flags must match the [§3.2 table](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#32-build-and-push-the-images-on-ci) — that check is `5.4.9.5`, not this subtask. Create `plans/doc/phase4-ada-fusion-run.md` recording the push.

**Acceptance:** the tag is pullable; the record exists. The independent registry-side confirmation is [§3.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#33-confirm-the-run-passed-and-the-images-landed) and lands at `5.4.10.1`. **Standing hazard:** the tag is mutable and every branch push re-pushes it — identify the deployed image at deploy time, never from an old run log.

**Dependencies:** after `15.4.2.3` + `6.4.4.1` + Phase 3 `5.3.6.1` (so the deployed image carries the detector too) + `5.2.8.1` (the lane must exist — [§8.1 item 3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these) records that it does not yet). **Commit:** `[5.4.6.1] docs: record the phase 4 ADA image push`

### [ ] `5.4.6.2` — USER-MANUAL: node config + deploy → ADA node Running on the **full** blueprint *(user, Nydus UI)*

**Objective:** the ADA node runs in the 5-node Room with the D9 configuration, alongside the Phase 1 bench and V2X nodes — [walkthrough §5.6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#56-the-full-blueprint-route).

**Scope:** ADA node `.12` per [node-ada-ecu.md § Blueprint node config](../requirements/car-sky-guide/node-ada-ecu.md) as updated by `5.2.9.4` — image `…/m1-ada-ecu:latest`, `command: ["./entrypoint.sh"]`, `capabilities: ["NET_RAW"]`, the §6 env set with `V2X_LISTEN_PORT=47200` and `IVI_ECU_HOST=10.99.0.13`/`IVI_ECU_PORT=47300`. **These are the same values the isolated Room used** — §5.6's "the ADA node is not reconfigured" is what makes that true, and it is why a mismatch here is a transcription error rather than a design difference. The ADA node replaces the Phase 1 netcheck sink at `.12`. Bench + V2X nodes keep their Phase 1 config. **Do not import a hand-authored blueprint JSON in place of the 5-node blueprint** — §5.6: an import arrives without its `ethernet` pins and typically without the Skycraft `image` block, and the deploy is rejected outright. New Deployment → Deployment Viewer shows every node Running, restart 0; mind the 2-deployment quota. The phase poll and `nodeKey` capture are the AI row of [§4.5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#45-deploy) and are performed by [[car-sky]] under `5.4.10.8`'s brief, re-run here.

**Readiness is this check, and nothing else is coming** — [m1-run-timing-and-event-triggering.md §4.2](../requirements/m1-run-timing-and-event-triggering.md) rules out a node-to-node startup handshake: the platform offers no `dependsOn`, no readiness probe and no "deployment started" event into a container, the R5/R6 topology has no reverse path to build acks on, and **R5's Deployment-Viewer check performed here by a human *is* the barrier**, paired with a configured bench `start_delay_s`. Do not plan or assume a barrier message; if a node misses early traffic, the remedy is the delay. The §4.2 B-1 pick's other half — one `[EVT] ready` line per node — is unscheduled (§ Open items item 7).

**Acceptance:** per-node Running badges + restart 0 recorded in `plans/doc/phase4-ada-fusion-run.md` (the Deployment Viewer summary header is unreliable — the Phase 1 finding); evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `5.4.6.1`. **Commit:** `[5.4.6.2] docs: record the phase 4 ADA node config and Running evidence`

### [ ] `13.4.6.3` — USER-MANUAL: live R13 lifecycle of relayed C across both bench scenarios *(user, Nydus UI)*

**Objective:** the first Phase 4 box, live **on the full chain** — C's track appears with `source = v2x_relayed` only and follows the full R13 lifecycle, and a Scenario Player scenario swap changes it.

**Scope:** with `SCENARIO_CONFIG=/app/scenarios/default.yaml` on the bench, save the ADA node View Log and run `python ADA_ECU/tools/check_evt_log.py --admission --fusion <saved.log>` (exit 0) plus `event_report.py`; then swap the bench to `c-out-of-range.yaml`, redeploy (config only, no rebuild), and repeat — the second run must show C never admitted and **zero** `r4_tx`. Confirm by inspection that every C track carries `source: v2x_relayed`, never `own_sensor`.

**This is the full-blueprint form of the negative case, and it is a different lever from the isolated Room's.** [§5.6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#56-the-full-blueprint-route) states that on the full chain the distance profile comes from the Scenario Player's scenario rather than from node env, so `c-out-of-range.yaml` is the swap here; `13.4.11.5` uses `PROFILE=out_of_range` on the bench mock node for the same claim in the isolated Room. Run both — one proves the gate against the real V2X decode path, the other against a controlled emitter. Saving the log is an AI row ([§5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#5-run-the-checks)) performed by [[car-sky]] under `18.4.11.1`'s brief.

**Acceptance:** both log sets and both tool outputs recorded in `plans/doc/phase4-ada-fusion-run.md`; evidence commit by the orchestrating session.

**Dependencies:** after `5.4.6.2`. **Commit:** `[13.4.6.3] docs: record the live relayed-C admission evidence`

### [ ] `18.4.6.4` — USER-MANUAL: **evidence path A** — both TrackedObjects in the `[EVT]` log *(user, Nydus UI)*

**Objective:** the log half of § Phase 4 output acceptance, and the R18 "event list reconstructs a full run offline" box.

**Scope:**

- Save the ADA node View Log over a full `default.yaml` run — an **AI row** ([§5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#5-run-the-checks), "Save the three node logs"), performed by [[car-sky]] over the logs route with the mandatory `?container=user`; this *USER-MANUAL* subtask keeps the judgement and the record-keeping.
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

**Scope note added 2026-08-03 — where this subtask's evidence comes from, and where it does not.** [Walkthrough §5.4](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#54-traffic-evidence-and-wireshark-scope) puts producing a `.pcap` **out of scope of that procedure** and makes it no pass criterion of any of its three checks; the walkthrough's own traffic evidence is the sink node's `[CAP]` text lines, which is `15.4.11.4`'s. This subtask is therefore **not** gated by, and does not gate, the isolated-Room run. It stays in the plan because [milestone1.md § Phase 4](milestone1.md#phase-4--obscured-object-fusion-relayed-c--risk--warning-r13r15--runs--with-phase-3) box (b) and R15's own acceptance require a pcap, and because §5.6 makes the ADA node's own capture **mandatory** on the full blueprint, where no sink log exists. The route is [traffic-capture-wireshark.md](../requirements/car-sky-guide/traffic-capture-wireshark.md), which §5.4 names as the one that does produce a file; §7 marks the browser log download and the extraction script as Human work. It may be run against either Room — the ADA node's capture is identical in both.

**Acceptance:** the `.pcap` archived and the decoded fields recorded in `plans/doc/phase4-ada-fusion-run.md`, showing ≥ 1 R4 warning event; evidence commit by the orchestrating session.

**Dependencies:** after `5.4.6.2` **or** `5.4.10.8` (whichever Room is up), plus `6.4.4.1` + `6.4.4.2` (parallel with `18.4.6.4`). **Commit:** `[15.4.6.5] docs: record the ADA to IVI pcap evidence`

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

## Task Group 4.8 — R21 run-alignment verification (serves R21, R20) — **scheduled last; gates no acceptance box**

> Source: [m1-run-timing-and-event-triggering.md](../requirements/m1-run-timing-and-event-triggering.md) — §6.4 the five checks, §6.3 where the numbers come from, §7 R20/R21.
>
> **Why this phase.** Three of the five checks (K1–K3 — the ones carrying R21's measurable output) read the ADA `[EVT]` vocabulary that only this phase completes: `r4_tx` and the `risk_transition`/`track_transition` sequence. The tool also joins its siblings in `ADA_ECU/tools/` (`check_zero_c.py`, `event_report.py`, `check_evt_log.py`), which is where a check script that reads a node's log belongs ([node-code-layout.md](../.claude/rules/node-code-layout.md)).
>
> **Report §8(1) is binding on the schedule.** R20/R21 are demo-*quality*, not R19-gating: no box in § Output depends on this group, no subtask above waits on it, and it runs only once every Phase 3 and Phase 4 acceptance box is closed. What lands here is the **instrument** plus the one missing check input; the **run that produces a verdict** is Phase 6's continuous run, where all three logs coexist for the first time (§ Open items item 7).

### [ ] `20.4.8.1` — Bench `[TX]` line gains `mono_ms` *(agent — **writes in `Scenario_Player/`**)*

**Objective:** the bench `[TX]` JSONL line carries the monotonic stamp K5 regresses `scenario_time_s` against — the one K-check input that exists nowhere today (report §6.3: the line currently carries `{seq, scenario_time_s, bytes}` and no timestamp at all).

**Write scope — explicit exception to this phase's subagent spec:** this subtask writes in `Scenario_Player/` only, not `ADA_ECU/`. It is planned here rather than in Phase 1 so the enabler stays visible beside the checker that consumes it, and so §8(1)'s "behind Phase 3 and Phase 4 acceptance" scheduling holds for both.

**Scope:**

- `Scenario_Player/player/generator.py` (the `[TX]` printer from `11.1.6.7`): add an injectable `mono_ms: Callable[[], int] | None = None` constructor parameter defaulting to `int(time.monotonic() * 1000)` — the same injectable-callable pattern the file already uses for `now_ms` / `sleep` / `log`. The emitted line becomes `{"seq", "scenario_time_s", "bytes", "mono_ms"}` with `separators=(",", ":")` unchanged; `mono_ms` is read once per emitted datagram, immediately after `send` returns.
- **`CLOCK_MONOTONIC`, never `time.time()`** (report §6.2: intervals use the monotonic clock, wire and log timestamps use the realtime clock). `[TX]` carries no realtime stamp today and gains none here. The existing `_wall_clock_ms` stays exactly as it is — it feeds the sample's `reference_time_ms`, which is a different concern.
- **Out of scope, deliberately:** deadline scheduling of the tick loop, `start_delay_s`, `reference_time_epoch`, and any `[ENC-SKIP]` change. Those are R20's remaining halves and are not planned in this phase — § Open items item 7.
- Test — extend `Scenario_Player/tests/test_generator.py`: against a fake monotonic clock, each `[TX]` line parses as JSON and carries an integer `mono_ms` equal to the injected sequence; values are non-decreasing across a `loop: true` restart; the existing cadence / loop-restart / duration-exit / `[TX]`-shape / encode-skip cases stay green.

**Acceptance:** `pip install -r Scenario_Player/requirements-dev.txt && python -m pytest Scenario_Player/tests` green locally **and** on CI `python-tests`; no new `time.time()` call anywhere in `player/`.

**Dependencies:** none — `11.1.6.7` is done and running live. **Commit:** `[20.4.8.1] feat: stamp the bench TX line with a monotonic timestamp`

### [ ] `21.4.8.2` — Run-alignment checker `tools/check_run_alignment.py` *(agent)*

**Objective:** the post-run verification script report §6.4 specifies — measure K1–K5 from saved logs and exit non-zero when a bound is missed. **Post-run verification only: it is not a trigger, not an orchestrator, and never runs on the ego data path** (report §5 — the tool that should exist is a checker, not a trigger).

**Scope:**

- `ADA_ECU/tools/check_run_alignment.py`, **Python 3 stdlib only** (no numpy, no jsonschema) — the `check_zero_c.py` / `event_report.py` shape. Test equipment only, never in the image (`.dockerignore` already excludes `tools/`).
- **Three optional input logs, at least one required.** Each supplied log enables its own checks; every check reads timestamps produced by **one** clock, so none depends on cross-node agreement (report §6.4).

| Flag | Log | Line shape | Enables |
|---|---|---|---|
| `--evt` | ADA `[EVT]` JSONL | `[EVT] ` prefix + `{event, mono_ms, epoch_ms, counters, payload}` (Phase 2 `18.2.2.3`) | K1, K2, K3 |
| `--detector` | detector R3 JSONL | one `TrackedObject` per line (Phase 3 `12.3.2.6`) | K4 |
| `--tx` | bench `[TX]` JSONL | `[TX] ` prefix + `{seq, scenario_time_s, bytes, mono_ms}` (`mono_ms` from `20.4.8.1`) | K5 |

- Tolerate interleaved `[CAP]` and non-`[EVT]` lines in the `--evt` input — View Log exports carry both, the same tolerance `tools/check_evt_log.py` already implements.
- **The five checks** (report §6.4; bounds are defaults, every one overridable — see the flag table):

| # | Check | Bound | Reconstructed from |
|---|---|---|---|
| K1 | At every `r4_tx`, a `tracked` `own_sensor` entry exists whose `timestamps.lastUpdated` is within `--track-timeout-ms` of that `r4_tx`'s `epoch_ms` | binary pass | `own_sensor_ingest` payloads + `track_transition` (`id`, `source`, `from`, `to`) replayed in order to a per-id state |
| K2 | The first `own_sensor` → `tracked` transition precedes the first `v2x_relayed` → `tracked` transition | binary pass | `track_transition` line order; **absence of either is a fail**, not a skip |
| K3 | `max │lastUpdated(newest tracked own_sensor) − lastUpdated(newest tracked v2x_relayed)│` over all `r4_tx` | ≤ `--max-skew-ms` (1000) | same replay as K1; both values are ADA's own `CLOCK_REALTIME` at store write (report §6.2), so the subtraction stays in one clock domain |
| K4 | Observed sampled-frame rate = `(distinct frames − 1) / Δ(timestamps.received) seconds`, against expected `--clip-fps / --frame-stride` | within `--detector-tolerance-pct` (2 %) over ≥ `--min-window-s` | detector R3 JSONL: `timestamps.measured` is the frame-capture stamp (clip time), `timestamps.received` the emit time (wall) |
| K5 | `Δscenario_time_s / Δ(mono_ms/1000)` | within `--bench-tolerance-pct` (1 %) over ≥ `--min-window-s` | bench `[TX]`; **`loop: true` resets `scenario_time_s` to 0 mid-stream** — accumulate across cycles by detecting the decrease and carrying the previous cycle's maximum |

- **Flags, no literals in the check bodies** (CLAUDE.md principle 5, the `check_clip_spec.py` pattern — every expected value from a CLI flag or the matching env var, with the §6.4 values as defaults): `--track-timeout-ms` (1000) · `--max-skew-ms` (1000) · `--detector-tolerance-pct` (2.0) · `--bench-tolerance-pct` (1.0) · `--min-window-s` (60) · `--clip-fps` · `--frame-stride` · `--require K1,K2,…`. **`--clip-fps` and `--frame-stride` have no defaults** — they are the detector's `DETECTOR_CLIP_FPS` / `DETECTOR_FRAME_STRIDE`, and `DETECTOR_CLIP_FPS` is a §6.1 key that does not exist yet (§ Open items item 7); K4 skips when they are absent rather than guessing.
- **Exit-code semantics** — an empty log is never a pass, the `check_zero_c.py` rule:

| Code | Meaning |
|---|---|
| 0 | every enabled check passed **and** every enabled check examined a non-empty record set |
| 1 | an enabled check missed its bound — output names the check id, the bound, the measured value, and the offending line number |
| 2 | a supplied log yielded zero records relevant to its checks, **or** `--require` names a check whose input log was not supplied |
| 3 | usage error — no log supplied at all, unreadable file, `--require K4` without `--clip-fps`/`--frame-stride` |

- **Output:** one line per check — `K1 PASS`, `K3 FAIL measured=1420ms bound=1000ms at r4_tx line 812`, `K5 SKIP (no --tx log)` — plus a summary naming the records examined per log, so a vacuous pass is visible rather than silent.
- Test `ADA_ECU/tools/tests/test_check_run_alignment.py` (planner-designated path, § Open items item 3), synthetic logs written to `tmp_path`: a conforming three-log set exits 0 with five PASS · K1 fails when an `r4_tx` has no in-timeout `tracked` own-sensor entry · K2 fails when the relayed `tracked` transition comes first · K2 fails when one of the two is absent · K3 fails at a planted 1500 ms skew · K4 fails when a 60 s clip is emitted in 10 s of wall time · K5 fails at a 10× scenario-time advance · K5 **passes** across a `loop: true` restart · an empty `[EVT]` log exits 2 · `--require K5` with no `--tx` exits 2 · no log at all exits 3.

**Acceptance:** `python -m py_compile ADA_ECU/tools/check_run_alignment.py` passes; the test passes locally **and** on CI `python-tests`; every bound traced to a flag or env var, none to a literal inside a check.

**Dependencies:** after `14.4.1.3` + `15.4.2.2` (the `risk_transition` / `r4_tx` vocabulary and payload shapes freeze there) and `20.4.8.1` (K5's input field). K4's line shape comes from Phase 3 `12.3.2.6`. **Runs after every acceptance subtask in this phase and in Phase 3** — report §8(1). No code is shared with `check_evt_log.py`; the tools stay standalone.

**Commit:** `[21.4.8.2] feat: add the R21 run-alignment post-run checker`

---

## Task Group 4.9 — Isolated-Room prerequisites: the bench image, the CI lanes, the blueprint reference (serves R2, R4, R5)

> **Added 2026-08-03**, as stage 2 of [walkthrough-driven-delivery.md](../.claude/rules/walkthrough-driven-delivery.md) over [deploy-ada-ecu-walkthrough.md](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md). Every subtask below cites the section that governs its step; **no brief carries a copy of the procedure**, and no command is restated here.
>
> **This group is the walkthrough's [§8.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these) made into work, and that is why it is scheduled ahead of every Room step.** Items 3, 5 and 8 name three artifacts that do not exist — the two CI jobs, the bench image with its two roles and log shapes, and the blueprint file. Nothing in groups 4.10–4.11 can start until they do, and each is cheap to land off-platform. Items 1, 4, 9, 10, 11 and 12 cannot be retired by authoring anything; they are carried as flags and attached to the subtask where each first bites.
>
> **Where the bench lives, and why it is not a node folder.** `tools/ada-bench/` at the repository root, one image `m1-ada-bench:latest`, two roles selected by `ROLE` — the placement and its four rejected alternatives are [§2.4](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#24-where-the-bench-sources-live-and-why), argued there against [node-code-layout.md](../.claude/rules/node-code-layout.md). [tools/netcheck/](../tools/netcheck/) is the standing precedent: test equipment that deploys as a Container node, self-contained, its own Dockerfile, no imports from any node folder, no hardcoded peer addresses. **No subtask in this group writes inside `V2X_ECU/`, `IVI_ECU/` or `ADA_ECU/`** — the bench must be able to change without rebuilding the thing it tests. Flagged to [[project-architecture]] as § Open items item 9, because `node-code-layout.md` names four code folders and does not yet name `tools/` as the sanctioned home for bench containers.
>
> **Run doc:** `plans/doc/phase4-ada-isolated-room-run.md`, created by the first subtask that records evidence into it and appended by every one after. It is deliberately separate from `plans/doc/phase4-ada-fusion-run.md`, which is group 4.6's full-chain record — two Rooms, two records, so no reader has to work out which run a log came from.

### [ ] `5.4.9.1` — Author `requirements/car-sky-guide/blueprint-ada-isolated.json` *(agent — day one, blocks nothing else in this group)*

**Objective:** land the blueprint definition [§2.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#22-the-blueprint-definition-and-where-it-lives) designates but which [§8.1 item 8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these) records as not yet created.

**Scope:** create the file at exactly that path, beside [blueprint-m1-cooperative-awareness.json](../requirements/car-sky-guide/blueprint-m1-cooperative-awareness.json), whose shape it follows. **Its content is [§2.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#22-the-blueprint-definition-and-where-it-lives)'s JSON block, transcribed byte-for-byte** — four nodes, flat `config` per [carsky-rest-api-blueprint.md § Node config is flat](../requirements/car-sky-guide/carsky-rest-api-blueprint.md#node-config-is-flat-not-wrapped), empty `pins` and `edges`. Nothing is invented, reordered or "improved": this file's whole value is that `5.4.10.3` creates nodes from it and `5.4.10.6` diffs the live blueprint against it, so a divergence here is a divergence in both.

- **`pins` and `edges` stay empty, unlike the IVI mini-blueprint's reference JSON.** REST rejects `ETHERNET` pins and an import silently drops them ([carsky-rest-api-blueprint.md § Key finding](../requirements/car-sky-guide/carsky-rest-api-blueprint.md#key-finding-what-rest-can-and-cannot-do)), and this file *is* a creation source here — §4.1 sanctions both the `/batch` and the Import-from-File routes off it. Declaring pins it cannot deliver would make the read-back diff at `5.4.10.6` fail on fields nobody typed. The addresses the human draws instead are [§4.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#42-wire-the-ethernet-pins)'s table.
- The `description` field already carries the not-importable-pins warning in §2.2's text; keep it verbatim rather than paraphrasing.
- **Do not resolve §2.2's own open question inside this file.** Its two notes flag that `command: ["./entrypoint.sh"]` and `capabilities: ["NET_RAW"]` assume the delivered ADA image ships the capture entrypoint, and [§8.1 item 7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these) asks for that to be reconciled with [node-ada-ecu.md](../requirements/car-sky-guide/node-ada-ecu.md). That reconciliation is Phase 2 `5.2.9.4`'s, and it is a **hard dependency of `5.4.10.5`** — see § Open items item 10. Write §2.2's values here and let `5.2.9.4` make the node guide agree.

**Acceptance:** the file parses as JSON; its four nodes, their `nodeType`s, every `image`, `command`, `capabilities` and env key/value, and the bridge's `bridgeMode`/`subnet` match §2.2 field for field; `pins` and `edges` are empty arrays; the addresses and ports agree with [§9 Quick reference](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#9-quick-reference) (`.11` / `.12` / `.13` on `10.99.0.0/24`, bridge `.1`, ports `47200` and `47300`).

**Dependencies:** none — starts immediately, in parallel with everything. **Commit:** `[5.4.9.1] docs: add the isolated ADA blueprint definition`

### [ ] `2.4.9.2` — Bench emitter `tools/ada-bench/mock_v2x.py` *(agent)*

**Objective:** the `ROLE=v2x_mock` role of [§2.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#23-the-bench-image--one-image-two-roles) — a relayed-object emitter that stands in for the V2X ECU **at its output edge only**, performing no decoding and sending no encoded frames.

**Scope:** Python 3, standard library only (the image is Alpine plus `python3` and `tcpdump`; no pip install).

- One UDP datagram per tick to `TARGET_HOST:TARGET_PORT` at `RATE_HZ`, after `START_DELAY_S` seconds so the ADA node is listening first. **Every one of those is an environment variable and none is a literal** ([§3.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#31-write-the-bench-scripts): nothing about the topology is in the code; [CLAUDE.md](../CLAUDE.md) governing principle 5).
- The datagram body, the `PROFILE` behaviours (`approaching`, `out_of_range`) and the `STATION_ID` / `OBJECT_ID` / `START_DISTANCE_M` / `MIN_DISTANCE_M` / `CLOSING_RATE_MPS` / `LATERAL_M` / `OBJECT_SPEED_MPS` semantics are §2.3's bullet list — implement them from there, do not re-derive them.
- **The message must match [`ADA_ECU/contracts/r2-v2x-object.schema.json`](../ADA_ECU/contracts/r2-v2x-object.schema.json) field for field.** [§2.4](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#24-where-the-bench-sources-live-and-why): keep the field list byte-identical to that copy, or the bench will pass a message the real consumer rejects. SI units, `classification: "vehicle"`, `object.confidence` and `sender.speed` populated so no nullable field is exercised by accident. The failure this prevents is the `parse_reject`-on-every-datagram row of [§6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#6-troubleshooting), whose stated remedy is "fix the emitter, not the node".
- One `[TX]` log line per datagram, in §2.3's exact shape — `2.4.11.2`'s pass criterion counts them and compares against the ADA node's `r2_ingest` count, so the prefix and the `seq`/`objectId`/`distance`/`bytes` fields are load-bearing, not decoration.
- **Out of scope:** the entrypoint, the capture script and the Dockerfile (`5.4.9.4`); anything the sink does (`4.4.9.3`); any encoding, ASN.1 or Vanetza path.

**Acceptance:** `python -m py_compile` passes; a loopback self-check sends one datagram of each profile to a local socket and the received body **validates against `ADA_ECU/contracts/r2-v2x-object.schema.json`** — the same acceptance shape `18.4.3.1` uses, evidence in the Status line. The repeatable CI form is `2.4.9.7`.

**Dependencies:** none. Parallel with `4.4.9.3`. **Commit:** `[2.4.9.2] feat: add the V2X bench relayed-object emitter`

### [ ] `4.4.9.3` — Bench sink `tools/ada-bench/mock_ivi.py` *(agent)*

**Objective:** the `ROLE=ivi_mock` role of [§2.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#23-the-bench-image--one-image-two-roles) — bind `0.0.0.0:LISTEN_PORT`, log and check every warning datagram. It stands in for the Android node and is a Linux container precisely so it can do what the real node cannot: log, check and capture ([§2.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#21-topology)).

**Scope:** Python 3, standard library only.

- Two lines per datagram plus a `[SUMMARY]` every `SUMMARY_EVERY_S` seconds, in §2.3's exact shapes. **`15.4.11.4` greps these strings literally**, so `[RX]`, `[CHECK]`, `[SUMMARY]`, `both_vehicles=yes`, `c_source_relayed=yes` and the field names beside them are the contract of this file.
- **Explicit field checks, not full schema validation** — §2.3 fixes that so the image stays standard-library only. `both_vehicles=yes` requires `geometry.vehicleB` and `geometry.vehicleC` both present with numeric `x` and `y`; `c_source_relayed=yes` requires `object.source` to be exactly `v2x_relayed`; `rejected` counts datagrams that were not valid JSON or whose `type` was neither `warning` nor `state`.
- The field list is taken from [`ADA_ECU/contracts/r4-ada-ivi.schema.json`](../ADA_ECU/contracts/r4-ada-ivi.schema.json) and kept byte-identical to it ([§2.4](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#24-where-the-bench-sources-live-and-why)). **A null `geometry.vehicleC` is legitimate before C is first tracked** and must produce `both_vehicles=no` rather than a rejection — [§5.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#53-check-3--the-warning-reaches-the-ivi-stand-in-carrying-both-vehicles)'s failure note turns on being able to read `no` on the first `seq` and `yes` afterwards.
- **Out of scope:** rendering anything, the capture (`5.4.9.4` owns `capture.sh`), and any dependency on `ADA_ECU/tools/mock_ivi_receiver.py` — that is `18.4.3.1`'s loopback tool for CI inside the node folder and **is not shipped in this image**; the two stay separate per the no-cross-folder-imports rule.

**Acceptance:** `python -m py_compile` passes; a loopback self-check feeds it the committed `contracts/samples/r4-warning.json` body and a null-`vehicleC` variant, and the emitted `[RX]`/`[CHECK]`/`[SUMMARY]` lines match §2.3's shapes with `both_vehicles` reading `yes` then `no` respectively and `rejected=0` — evidence in the Status line.

**Dependencies:** none. Parallel with `2.4.9.2`. **Commit:** `[4.4.9.3] feat: add the IVI bench warning sink and checker`

### [ ] `5.4.9.4` — Bench image `tools/ada-bench/{entrypoint.sh,capture.sh,Dockerfile}` *(agent)*

**Objective:** one image serving both roles — [§2.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#23-the-bench-image--one-image-two-roles)'s five-file table completed, so **a deploy alone produces evidence and no shell session is ever needed** ([§3.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#31-write-the-bench-scripts)).

**Scope:** the three remaining rows of §2.3's file table.

- `entrypoint.sh`: `[BOOT]` line, launch `capture.sh` in the background, then `exec` the script named by `ROLE`. A misspelled `ROLE` must fail loudly at start — the climbing-restart-count row of [§6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#6-troubleshooting) is what that row diagnoses.
- `capture.sh`: `tcpdump -i any -n -l` on `CAPTURE_FILTER`, each line prefixed `[CAP]`, falling back to packet counters without `NET_RAW`. **Port `tools/netcheck/capture.sh` rather than writing a new one** — same category of artifact, and `15.4.11.4`'s `[CAP]` criterion depends on the line shape. It writes no rotating pcap: [§5.4](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#54-traffic-evidence-and-wireshark-scope) puts `.pcap` production out of scope for this procedure, and the `[PCAP-BEGIN]` machinery is `ADA_ECU/capture.sh`'s (`6.4.4.1`), not this one's.
- `Dockerfile`: `FROM alpine:3.20`, `apk add python3 tcpdump`, `WORKDIR /app`, copy the four files, `CMD ["./entrypoint.sh"]`. **`command` is relative to `/app`** — §4.3's closing note: `./entrypoint.sh` works and `/entrypoint.sh` does not exist, so the container dies at start.
- Self-contained context: no file outside `tools/ada-bench/` enters the build, and nothing here imports from a node folder ([node-code-layout.md § Build rules](../.claude/rules/node-code-layout.md#build-rules-all-container-nodes)).

**Acceptance:** `sh -n` and `bash -n` clean on both scripts, LF line endings, exec bit set; `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -t m1-ada-bench:latest tools/ada-bench/` succeeds in the `5.4.9.5` lane; running the built image with `ROLE=v2x_mock` prints `[BOOT]` and then `[TX]` lines, and with `ROLE=ivi_mock` prints `[BOOT]` and binds.

**Dependencies:** after `2.4.9.2` + `4.4.9.3` (the two files it `exec`s must exist). **Commit:** `[5.4.9.4] feat: add the ADA bench image entrypoint, capture and Dockerfile`

### [ ] `5.4.9.5` — CI lane `ada-bench-image` *(agent — closes [§8.1 item 3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these), half)*

**Objective:** the first of the two jobs [§3.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#32-build-and-push-the-images-on-ci) requires and §8.1 item 3 records as absent — **a push currently builds and publishes nothing**.

**Scope:** one job `ada-bench-image` building context `tools/ada-bench/` and pushing `m1-ada-bench:latest`, in the shape of `v2x-ecu-image` in [phase1-ci.yml](../.github/workflows/phase1-ci.yml). Every flag and value comes from §3.2's command block and its four bullets — `--platform linux/arm64`, `--provenance=false --sbom=false`, registry host `registry.hackathon-2.carsky.io` used identically in the login, the tag and the node's `image` field, and the key read only from the `CARSKY_ZOT_API_KEY` repository secret ([zot-registry-api-key.md § CI secret](../requirements/car-sky-guide/zot-registry-api-key.md#ci-secret-carsky_zot_api_key)). Push only when the secret exists, with the same notice-and-exit-0 guard the Phase 1 lanes use; verify the pushed artifact through the existing `.github/actions/verify-arm64-image` composite.

**File placement:** `.github/workflows/phase4-ci.yml` — *a lane belongs to the phase that created it*, this phase's § CI ruling. That file is also `15.4.5.1`'s; **whichever lands first creates it** with the standard `on:`/`concurrency:`/header block, and the other adds only its job. Sequence the two edits — they are the only shared file in this group.

**Acceptance:** workflow YAML valid; run-blocks `bash -n` clean; the lane is green on the pushed branch and the build step completes in roughly a minute (§3.2: Alpine plus two packages under emulation).

**Dependencies:** after `5.4.9.4`. Shares `phase4-ci.yml` with `15.4.5.1` and `2.4.9.7`. **Commit:** `[5.4.9.5] chore: add the ADA bench image build-push CI lane`

### [ ] `5.4.9.6` — Confirm the `ada-ecu-image` lane matches §3.2's table *(agent — closes [§8.1 item 3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these), other half)*

**Objective:** the second job of [§3.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#32-build-and-push-the-images-on-ci) exists and is correct. §3.2 is unambiguous about the order: *"If a job's context, tag, registry host or platform flag does not match the table above, fix the `.yml` first — there is nothing to verify without it."*

**Scope:** the job is **already planned as Phase 2 `5.2.8.1`** (`ada-ecu-image` in `phase2-ci.yml`, context `ADA_ECU/`, tag `registry.hackathon-2.carsky.io/m1-ada-ecu:latest`) and is **not** re-planned here — IDs are never duplicated. This subtask's single objective is the confirmation and, if needed, the correction:

- Check the live job against §3.2's table row for row: job name, build context, image tag, registry host, `--platform linux/arm64`, `--provenance=false --sbom=false`, and the secret's name.
- Check its timeout against §3.2's closing paragraph — the ADA image compiles C++ and installs the detector's Python dependencies under emulation, and the existing image jobs use **360 minutes** for that reason. A shorter cap is the "red after 360 minutes" row of [§6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#6-troubleshooting) waiting to happen.
- Any mismatch is fixed in the `.yml` in this same commit. A clean check produces no code change and the commit is the record in `plans/doc/phase4-ada-isolated-room-run.md` — **this subtask creates that run doc.**

**Acceptance:** every row of §3.2's table confirmed against the live workflow and recorded, with any correction applied and the lane green; the run doc exists and names the two job files (`phase2-ci.yml` for the ADA image, `phase4-ci.yml` for the bench image) so no later reader hunts for them.

**Dependencies:** after Phase 2 `5.2.8.1`. Parallel with `5.4.9.5`. **Commit:** `[5.4.9.6] docs: confirm the ADA ECU image lane against the walkthrough build table`

### [ ] `2.4.9.7` — CI lane `ada-bench-selfcheck` — emitter → sink loopback *(agent)*

**Objective:** prove [§8.1 item 5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these) — *"the bench image, its two roles, its env names and its log-line shapes … not yet written"* — **before a Room slot is spent on them**. An unproven route is proved ahead of the work depending on it ([walkthrough-driven-delivery.md § Stage 2](../.claude/rules/walkthrough-driven-delivery.md)); this is the cheapest place that can happen, and it is the only check in this group that runs without the platform.

**Scope:** a second job in `phase4-ci.yml`. Start `mock_ivi.py` on a loopback port; start `mock_v2x.py` with `TARGET_HOST=127.0.0.1`, that port, a short `START_DELAY_S` and `PROFILE=approaching`; feed the sink a handful of synthetic R4 warning bodies from `contracts/samples/`; then assert **all** of:

- the emitter's datagrams validate against `ADA_ECU/contracts/r2-v2x-object.schema.json` (the `parse_reject` failure mode, caught here rather than in a Room);
- the emitter's `[TX]` lines and the sink's `[RX]`/`[CHECK]`/`[SUMMARY]` lines parse under the exact greps `2.4.11.2` and `15.4.11.4` use — the lane fails if a prefix or field name drifts;
- `PROFILE=out_of_range` holds distance at `START_DISTANCE_M`, which is what makes `13.4.11.5`'s negative case meaningful;
- a malformed datagram increments the sink's `rejected` counter rather than killing it.

**Out of scope:** anything requiring the platform, a registry or a deployed node.

**Acceptance:** lane green on the pushed branch, with a non-zero examined-datagram count asserted so the lane cannot pass vacuously.

**Dependencies:** after `2.4.9.2` + `4.4.9.3` + `5.4.9.5` (same file). **Commit:** `[2.4.9.7] chore: add the ADA bench loopback self-check CI lane`

---

## Task Group 4.10 — Create and deploy the isolated Room (serves R5, R6)

> [Walkthrough §3.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#33-confirm-the-run-passed-and-the-images-landed) through [§4.5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#45-deploy), in the order the document states — **that ordering is binding** where the walkthrough gives one, and it does here: the images must exist before the nodes reference them, the nodes before the pins, the pins before validation passes, and the config read-back before a Room slot is spent.
>
> **This is where execution stops and waits for a person, three times.** [§7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#7-work-division-between-ai-and-human) assigns the canvas, every node-config edit and the deploy to Human, and the reasons are structural, not conservatism: REST has no `ETHERNET` pin type, no update route and no delete operation, and picking the Device spends one of two Room slots. **No agent performs a *USER-MANUAL* row below**; [[car-sky]] is spawned for the *car-sky* rows and halts at the next human one, reporting exactly what is needed.

| Step | Subtask | Owner |
|---|---|---|
| Confirm the two image jobs passed | `5.4.10.1` | **Human** |
| Confirm both images reached the registry | `5.4.10.2` | AI — [[car-sky]] |
| Create the blueprint and its four nodes | `5.4.10.3` | AI — [[car-sky]] |
| Wire and configure the ethernet pins | `6.4.10.4` | **Human** |
| Configure the correct image on each node | `5.4.10.5` | **Human** |
| Read the stored config back and diff it | `5.4.10.6` | AI — [[car-sky]] |
| Deploy the blueprint | `5.4.10.7` | **Human** |
| Poll to `Running`, resolve every `nodeKey` | `5.4.10.8` | AI — [[car-sky]] |

### [ ] `5.4.10.1` — HUMAN TASK: confirm the two image jobs passed *(user — no agent performs this)*

**Objective:** the first half of [§3.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#33-confirm-the-run-passed-and-the-images-landed) — green on both `ada-bench-image` and `ada-ecu-image` in the newest Actions run.

**Scope:** GitHub → Actions → the newest run → the two jobs. §7 assigns this to Human because *an agent session holds no GitHub token*; the same note records that it **flips to AI on a machine with an authenticated `gh` CLI**, in which case §3.3's two `gh` commands replace the browser and this subtask is handed to [[car-sky]] instead. A red push step printing `secret not set` means the credential of [§1.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#12-cloud-platform-access) is missing, not that the code is wrong.

**Acceptance:** both job names and their conclusions recorded in `plans/doc/phase4-ada-isolated-room-run.md`, with the run id. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `5.4.9.5` + `5.4.9.6`, and a commit pushed. **Commit:** `[5.4.10.1] docs: record the image CI run for the isolated ADA Room`

### [ ] `5.4.10.2` — Confirm both images reached the registry *(car-sky)*

**Objective:** the second half of [§3.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#33-confirm-the-run-passed-and-the-images-landed) — the registry's own answer, **independent of what the run reported**.

**Scope:** the three `curl` calls §3.3 gives, against the catalog and both tag lists. The Zot credential is supplied at run time and never stored ([zot-registry-api-key.md](../requirements/car-sky-guide/zot-registry-api-key.md)); §7's closing note applies — every AI row needs its credential handed over, an agent keeps none. Confirm each manifest is a **single-platform `linux/arm64` image, not a manifest index**: an index is what makes a node hang in `Provisioning`, and §3.3's warning is that the failure "appears late and reads like a network fault".

**Acceptance, verbatim from [§3.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#33-confirm-the-run-passed-and-the-images-landed):** both repository names present in the catalog, and `{"name":"m1-ada-ecu","tags":["latest"]}` / `{"name":"m1-ada-bench","tags":["latest"]}` returned by the tag lists. Both digests recorded in the run doc. A name missing here stops the group — fix it now, per §3.3.

**Dependencies:** after `5.4.10.1`. **Commit:** `[5.4.10.2] docs: record the registry confirmation for both isolated-Room images`

### [ ] `5.4.10.3` — Create the blueprint and its four nodes over REST *(car-sky)*

**Objective:** [§4.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#41-create-the-blueprint)'s two calls — a blueprint named `ada-isolated`, then one `/batch` adding the four nodes with their `config` blocks. §7 assigns this to AI; it is the largest scripted step in the procedure and the one that keeps the human's canvas session to pins and corrections.

**Scope:** the endpoints, the payload shape and the flat-`config` rule are §4.1's; the node data is `5.4.9.1`'s file and nothing else. §4.1 names the batch payload `batch-ada-isolated.json` without designating a path — **derive it at run time from `requirements/car-sky-guide/blueprint-ada-isolated.json`, one `{"op":"addNode","data":{…}}` per node, and do not commit a second copy of the node data**; a committed derivative would be a second source of truth for the same fields. Flagged to [[project-researcher]] as § Open items item 11.

- Nydus **Import from File** on the same file produces the same result and is an equally sanctioned route (§4.1); **neither route creates pins**, and neither is better than the other for that.
- If a blueprint already carries `ethernet` pins at these addresses, cloning it is the only route that preserves them (§4.1) and saves `6.4.10.4` most of its work — check before creating from scratch.
- **Never edit the `<name>-deploy` snapshot** a deploy creates: §4.1's blockquote — edits to it appear to save and are ignored by the next deploy.

**Acceptance, verbatim from [§4.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#41-create-the-blueprint):** the created blueprint returns an `id`, and the batch call reports **four nodes created, no pins, no edges**. The `id` and the four node ids recorded in the run doc — every call in groups 4.10 and 4.11 needs the blueprint id.

**Dependencies:** after `5.4.9.1` + `5.4.10.2`. **Commit:** `[5.4.10.3] docs: record the isolated blueprint and its four nodes created over REST`

### [ ] `6.4.10.4` — HUMAN TASK: wire the four ethernet pins and validate *(user, Nydus canvas — no agent performs this)*

**Objective:** [§4.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#42-wire-the-ethernet-pins) — one `ETHERNET`/`INPUT` pin on the bridge, one `ETHERNET`/`OUTPUT` pin per container node at its address, three edges, all terminating at the bridge. A star, not a chain.

**Scope:** §4.2's five numbered steps, its per-node address table, and the pin shape at [node-ada-ecu.md § Pins](../requirements/car-sky-guide/node-ada-ecu.md#pins) — *only the address differs per node*. **This is canvas work with no scripted alternative and it is the reason the whole procedure has a human in it:** REST cannot create `ETHERNET` pins, a JSON import silently drops them, and the API has no delete operation ([carsky-rest-api-blueprint.md § Key finding](../requirements/car-sky-guide/carsky-rest-api-blueprint.md#key-finding-what-rest-can-and-cannot-do)) — so a wrong pin is corrected by hand or not at all. Same-type wiring only.

**Acceptance, verbatim from [§4.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#42-wire-the-ethernet-pins):** `POST /api/v1/blueprints/{id}/validate` returns a **pass**. A 422 naming a node means that node still has no pin — the [§6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#6-troubleshooting) row for it says the same. The validate call itself is a read and may be run by [[car-sky]]; the drawing is not. Recorded in the run doc.

**Dependencies:** after `5.4.10.3`. **Commit:** `[6.4.10.4] docs: record the isolated blueprint pin wiring and validation`

### [ ] `5.4.10.5` — HUMAN TASK: configure each node's image and env in the Inspector *(user, Nydus UI — no agent performs this)*

**Objective:** [§4.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#43-configure-each-nodes-image) — every node's `image`, `command`, `capabilities` and `env` set and confirmed against §2.2.

**Scope:** §4.3's three-row image table (`m1-ada-bench:latest` with `ROLE=v2x_mock` on `.11`, `m1-ada-ecu:latest` on `.12`, `m1-ada-bench:latest` with `ROLE=ivi_mock` on `.13`) and its four-row *"values that decide whether anything works at all"* table. **This step stays a hand edit even though `5.4.10.3` created the nodes with config attached** — §4.3: the API has no update route, so every correction after creation is an Inspector edit. Click the node, edit, click empty canvas to commit.

The four values §4.3 singles out each fail in a way that looks like something else, and their symptoms are that table's, not this brief's. `command` is relative to the image workdir `/app`.

**Blocked on a reconciliation, not on a build:** [§8.1 item 7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these) requires the ADA node's `command` and `capabilities` to be resolved against [node-ada-ecu.md](../requirements/car-sky-guide/node-ada-ecu.md) **before deploying**, and that file today still says `command: ["./ada_ecu"]`, no `capabilities`, and the stale `registry.carsky.io` host. Phase 2 `5.2.9.4` is the fix and must land first — § Open items item 10.

**Acceptance:** every field typed, then proven by `5.4.10.6`'s read-back rather than by the Inspector's truncated fields (§4.4 is explicit that the read-back is what counts). Which values were typed recorded in the run doc.

**Dependencies:** after `6.4.10.4` + Phase 2 `5.2.9.4`. **Commit:** `[5.4.10.5] docs: record the isolated-Room node configuration`

### [ ] `5.4.10.6` — Read the stored config back and diff it against the definition *(car-sky)*

**Objective:** [§4.4](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#44-read-the-stored-config-back) — *"one call catches every row of the two tables above"*. **This is the gate in front of the Room slot**: hand transcription is where drift enters, and a mistyped octet found here costs a re-edit rather than a deploy cycle.

**Scope:** `GET /api/v1/blueprints/{id}`, then a field-for-field diff against `requirements/car-sky-guide/blueprint-ada-isolated.json`, ignoring only platform-assigned `id`s and node positions. Every one of these is a diff line, not a judgement call: each node's `nodeType` and whole `config`; the bench's `TARGET_HOST`/`TARGET_PORT` and `ROLE`; the ADA node's `V2X_LISTEN_PORT`, `IVI_ECU_HOST`/`IVI_ECU_PORT` and every threshold; the sink's `LISTEN_PORT`; the bridge's `bridgeMode` and `subnet`, without which the `10.99.0.x` addresses have no network.

**Acceptance, verbatim from [§4.4](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#44-read-the-stored-config-back):** four nodes; **three `ETHERNET`/`OUTPUT` pins at `10.99.0.11`, `.12`, `.13` plus the bridge's single `INPUT` pin and three edges**; and each container node's `config` carrying the image, command, capabilities and env exactly as typed. A clean diff recorded in the run doc — or every mismatching field named and handed back to `5.4.10.5` for a canvas fix, then re-run. **A deploy does not start on a dirty diff.**

**Dependencies:** after `5.4.10.5`. **Commit:** `[5.4.10.6] docs: record the isolated blueprint read-back diff`

### [ ] `5.4.10.7` — HUMAN TASK: deploy the blueprint *(user, Nydus UI — no agent performs this)*

**Objective:** [§4.5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#45-deploy)'s three clicks — blueprint Inspector → **New Deployment** → pick an **existing Device** → **Deploy**.

**Scope:** §4.5's numbered steps. §7 keeps this Human because picking the Device is the user's call and **consumes one of two Room slots** ([§8.1 item 13](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these); § Open items item 12 tracks the contention against Phase 3's and Phase 5's Rooms). `+ Create new device` is unnecessary and eats into that budget. Deploy the **original** blueprint, never the `<name>-deploy` snapshot.

**Readiness is this check and nothing else is coming** — the same ruling `5.4.6.2` carries: [m1-run-timing-and-event-triggering.md §4.2](../requirements/m1-run-timing-and-event-triggering.md) rules out a node-to-node startup handshake, and the bench's `START_DELAY_S=20` is the remedy for a node that would otherwise miss early traffic. Do not plan or assume a barrier message.

**Acceptance:** the deployment exists and its `roomId` is recorded in the run doc; the phase evidence itself is `5.4.10.8`'s.

**Dependencies:** after `5.4.10.6` (clean diff) and a free Room slot. **Commit:** `[5.4.10.7] docs: record the isolated Room deployment`

### [ ] `5.4.10.8` — Poll every node to `Running` and resolve every `nodeKey` *(car-sky)*

**Objective:** the AI row of [§4.5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#45-deploy) — turn the human's deploy into recorded evidence, and produce the three keys every log call in group 4.11 needs.

**Scope:** poll `GET /api/v1/deployments/{roomId}/nodes` until every node reads `Running` with restart count 0, recording each entry's `name` — the `nodeKey`. §4.5's two timing facts are binding on how this is judged: **the ADA node is the slowest to become useful** (largest image, and the detector loads its model before the first detection), so give it a minute past `Running` before anything reads its log; and *stuck in `Provisioning`* means the image could not be pulled — re-check `5.4.10.2` and the node's `image` field, diagnosing per [carsky-room-diagnostics](../.claude/skills/carsky-room-diagnostics/SKILL.md), never by redeploying blind.

**Acceptance:** 4/4 nodes `Running` with restart count 0, and all three container `nodeKey` values recorded in the run doc. This is the precondition every [§8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance) proof rests on, and it replaces reading node badges off the Deployment Viewer by eye — the Phase 1 finding that its summary header is unreliable stands.

**Dependencies:** after `5.4.10.7`. **Commit:** `[5.4.10.8] docs: record the isolated Room reaching Running`

---

## Task Group 4.11 — The three checks, the negative case, and teardown (serves R2, R13, R14, R15, R18)

> [Walkthrough §5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#5-run-the-checks) through [§5.7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#57-tear-down). **Every acceptance below is [§8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance)'s, quoted, not invented** — [walkthrough-driven-delivery.md § Stage 2](../.claude/rules/walkthrough-driven-delivery.md) forbids the planner writing its own criteria for a walkthrough-governed check, and §8's three rows are exactly the three the user named.
>
> **The three proofs do not substitute for one another:** §8's own framing is that the first two prove what happened *inside* the node and the third proves it *left* the node. A run with checks 1 and 2 green and check 3 silent is a routing failure, not a partial pass.
>
> **The event names are the node's design, not an observed fact yet** — [§8.1 item 4](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these). `r2_ingest`, `own_sensor_ingest`, `track_transition`, `parse_reject`, `assessment`, `risk_transition` and `r4_tx` are literal strings from [HLD D8](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#d8--r18-the-ada-half-of-the-evidence-stream); a check that finds none of them is more likely reading a node that emits different names than a node that did nothing. Compare against the log before concluding a failure.

### [ ] `18.4.11.1` — Save the three node logs and the run's threshold values *(car-sky)*

**Objective:** the AI row *"Save the three node logs"* of [§5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#5-run-the-checks) — one fetch per node into a file, so all three checks read **one window** instead of three.

**Scope:** §5's three `curl` redirections into `ada.log`, `bench.log` and `sink.log`. Two of its rules are load-bearing and are the reason this is its own subtask rather than a step inside each check:

- **`container=user` is mandatory** — omitting it returns 500 listing the two container names, the [§6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#6-troubleshooting) row for it.
- **Let the Room run at least 60 seconds after the ADA node reaches `Running`** before reading anything — the emitter waits `START_DELAY_S` and the detector needs time to load its model. Re-fetching per grep gives three checks three different windows, which §5 says plainly not to do.

Also record, per [§8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance)'s closing instruction: `PROFILE`, `START_DISTANCE_M`, `MIN_DISTANCE_M`, `CLOSING_RATE_MPS`, `GATE_ENTER_M`, `GATE_EXIT_M`, `RISK_NEAR_M`, `RISK_CRITICAL_M` as deployed. **A pass at unknown thresholds proves nothing** — that sentence is §8's, and it is why this subtask exists ahead of the checks rather than beside them.

**Acceptance:** three non-empty log files archived under `plans/doc/`, the eight threshold values recorded beside them in the run doc, and the window (Room start, `Running` time, fetch time) stated.

**Dependencies:** after `5.4.10.8` + 60 s. **Commit:** `[18.4.11.1] docs: record the isolated-Room node logs and their threshold values`

### [ ] `2.4.11.2` — Check 1: the relayed message is received and raises its event *(car-sky)*

**Objective:** [§5.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#51-check-1--the-relayed-message-is-received-and-raises-its-event)'s claim under test — the ADA node receives the bench's datagram and raises the corresponding event. **This is the first of the three checks the user named.**

**Scope:** §5.1's five greps over `ada.log` and `bench.log`, and its two expected-line blocks. Nothing is added to them.

**Acceptance — [§8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance) output 1, verbatim:** `r2_ingest` count ≥ 1 and ≥ 90% of the bench's `[TX]` count · the first payload's `stationId` and `object.objectId` match the bench's configured values · `parse_reject` count is 0 · one `track_transition` to `tentative` and a later one to `tracked`, both `"source":"v2x_relayed"`.

**On failure, §5.1 already says which side is wrong** — `r2_ingest` at 0 while `bench.log` shows `[TX]` is a routing fault (`TARGET_HOST`/`TARGET_PORT`/`V2X_LISTEN_PORT`, back to `5.4.10.5`); a non-zero `parse_reject` means **the emitter is wrong, not the node**, and is fixed against `ADA_ECU/contracts/r2-v2x-object.schema.json` at `2.4.9.2`. Report the verdict and the side; do not improvise a fix on the platform.

**Dependencies:** after `18.4.11.1`. Parallel with `13.4.11.3` and `15.4.11.4` — three reads of saved files, no shared state. **Commit:** `[2.4.11.2] docs: record check 1 — relayed message received and event raised`

### [ ] `13.4.11.3` — Check 2: both vehicles are in the track store *(car-sky)*

**Objective:** [§5.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#52-check-2--both-vehicles-are-in-the-track-store)'s claim under test — the store holds a track for vehicle **B**, produced by ego's own detector from the baked-in clip, and a track for vehicle **C**, present only through the relayed path. **This is the second check the user named**, and it is the one that carries the milestone's whole point.

**Scope:** §5.2's five greps and its three expected-line blocks, over `ada.log`. The single strongest line is the emitted `r4_tx`, **because it proves both tracks existed at the same instant** rather than at two different times — §5.2's own words, and the reason its criterion is written against that payload rather than against two separate transitions.

**Acceptance — [§8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance) output 2, verbatim:** ≥ 1 `track_transition` to `tracked` with `"source":"own_sensor"` · ≥ 1 with `"source":"v2x_relayed"` · ≥ 1 `r4_tx` payload with `object.source` = `v2x_relayed` and numeric `geometry.vehicleB` · **zero** own-sensor entries claiming a relayed source or a `v2x:` id, and **zero** relayed entries claiming an `own:` id.

**Those last three zeros are the zero-C guarantee in text** (§5.2) — nothing ego's detector produced can claim to be C, and nothing relayed can claim to have been seen directly. They are the same claim `ADA_ECU/tools/check_zero_c.py` (`12.3.5.1`) makes structurally, evidenced here on a deployed node.

**Two failures with different owners, per §5.2:** no `own_sensor_ingest` at all is a detector problem — read for `detector_spawn` first, then `VIDEO_CLIP_PATH` / `MODEL_PATH` / `DETECTOR_ENABLED`, the [§6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#6-troubleshooting) rows. `r4_tx` absent though both tracks reached `tracked` is **a tuning problem, not a defect** — the risk level never changed, and the route is `14.4.11.6`, not a code change. [§8.1 item 10](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these) warns this may be the expected outcome rather than the exception.

**Dependencies:** after `18.4.11.1`. Parallel with `2.4.11.2` and `15.4.11.4`. **Commit:** `[13.4.11.3] docs: record check 2 — both vehicles in the track store`

### [ ] `15.4.11.4` — Check 3: the warning reaches the IVI stand-in carrying both vehicles *(car-sky)*

**Objective:** [§5.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#53-check-3--the-warning-reaches-the-ivi-stand-in-carrying-both-vehicles)'s claim under test — the ADA node puts a warning datagram on the wire, addressed to the IVI node, carrying both vehicles. **This is the third check the user named, and the traffic evidence for it.**

**Scope:** §5.3's four greps over `sink.log`, and its expected-output block. `cSource=v2x_relayed` on every warning is what §5.3 calls *the point of the whole Room*.

**Acceptance — [§8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance) output 3, verbatim:** ≥ 1 `[RX]` with `type=warning` from `10.99.0.12` · ≥ 1 `[CHECK] both_vehicles=yes c_source_relayed=yes` · last `[SUMMARY]` with `rejected=0` · ≥ 1 `[CAP] IP 10.99.0.12.<port> > 10.99.0.13.47300: UDP` · the sink's `received` count equals the ADA log's `r4_tx` count.

**Traffic evidence is the sink's `[CAP]` text lines, and producing a `.pcap` is out of scope of this procedure** — [§5.4](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#54-traffic-evidence-and-wireshark-scope), explicitly, with the pcap route named there and marked *"not a pass criterion of any check"*. The plan's own separate pcap box is `15.4.6.5` and does not gate this subtask. **`[CAP]` needs `NET_RAW` on the sink node** ([§8.1 item 12](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these)); without it `capture.sh` falls back to packet counters, the `[RX]` line still stands and the `[CAP]` criterion cannot be met by any route in the document — report that state rather than substituting other evidence for it.

**Two failures, per §5.3:** `r4_tx` present with the sink silent is a routing fault (`IVI_ECU_HOST`/`IVI_ECU_PORT` against `LISTEN_PORT`, back to `5.4.10.5`). `both_vehicles=no` needs the `seq` numbers read before it is called a defect — **`no` on the first datagram and `yes` afterwards is expected** because a null C is legitimate before C is first tracked; `no` throughout is the defect.

**Dependencies:** after `18.4.11.1`. Parallel with `2.4.11.2` and `13.4.11.3`. **Commit:** `[15.4.11.4] docs: record check 3 — the warning on the wire carrying both vehicles`

### [ ] `13.4.11.5` — HUMAN TASK: run the `PROFILE=out_of_range` negative case *(user, Nydus UI — no agent performs this)*

**Objective:** [§8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance)'s first *further observation* — prove the admission gate **rejects** on distance, so admission in the main run was earned rather than automatic.

**Scope:** §5.1's *"Run the negative case too"* paragraph — set `PROFILE=out_of_range` on the V2X bench node and redeploy. §7 marks it Human: it is a node-config edit plus a fresh deployment, and both are Inspector/canvas work. The re-run of the checks over the new logs is [[car-sky]]'s, under `18.4.11.1` and `2.4.11.2`'s briefs.

**Acceptance, verbatim from [§8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance):** with `PROFILE=out_of_range`, `r2_ingest` still counts up and **no** relayed `track_transition` appears. §5.1 states the failure meaning exactly: a track admitted under this profile means the gate is not reading the message's distance.

**Never reach for the gate constants to make a warning appear** — [§5.5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#55-retune-when-no-warning-is-emitted)'s closing rule: moving `GATE_ENTER_M`/`GATE_EXIT_M` to change the alarm makes admission and alarm indistinguishable and **invalidates this very subtask**.

**Dependencies:** after `2.4.11.2` (the positive case must have passed first, or the negative proves nothing). **Commit:** `[13.4.11.5] docs: record the out-of-range negative case`

### [ ] `14.4.11.6` — HUMAN TASK: retune when no warning is emitted *(user, Nydus UI — contingency, run only if `13.4.11.3` finds no `r4_tx`)*

**Objective:** [§5.5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#55-retune-when-no-warning-is-emitted) — get the assessed risk level to **change**, which is what emits a warning, using node config and a redeploy. **No rebuild.**

**Scope:** §5.5's four numbered steps and its three-row lever table (`MIN_DISTANCE_M` down on the bench, `RISK_NEAR_M` up or `RISK_CRITICAL_M` up on the ADA node). §5.5's step 2 carries the fact that makes this likely rather than exotic: **the composed range is ego-to-B plus B-to-C, so it is always larger than the distance the bench emits** — the defaults were chosen against a scenario, not against this clip's actual ego-to-B range, which is [§8.1 item 10](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these)'s warning that §5.5 may be on the critical path rather than a fallback.

- **Pick one lever and change only it** (§5.5 step 3), then re-run `13.4.11.3` and `15.4.11.4`.
- **Never retune the admission gate** — §5.5's closing rule, restated at `13.4.11.5`.
- **Record every value changed.** §5.5: *a run whose thresholds are unknown proves nothing*; the record goes beside `18.4.11.1`'s threshold list, superseding it for the re-run.

**Acceptance:** §8 outputs 2 and 3 met on the re-run, with the changed lever and its before/after values recorded. If no lever produces a transition, that is a finding about the risk defaults ([HLD §6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#6-configuration--no-hardcoded-tunables), § Open items item 2's `(proposal)` values) — report it rather than widening the search.

**Dependencies:** after `13.4.11.3`, only if it found no `r4_tx`. **Commit:** `[14.4.11.6] docs: record the risk-threshold retune and its re-run evidence`

### [ ] `5.4.11.7` — HUMAN TASK: tear the Room down *(user, Nydus UI — no agent performs this)*

**Objective:** [§5.7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#57-tear-down) — **Delete Deployment**, releasing one of the two Room slots. The blueprint is untouched and redeployable.

**Scope:** §5.7, and its one irreversible condition: **save all three log files before deleting — the log route returns nothing once the Room is gone.** `18.4.11.1` is what satisfies that, so this subtask does not run until its files are archived and every check that needs them has run.

**Acceptance:** the deployment deleted and the release recorded in the run doc; `plans/doc/` holds the three logs and every check's verdict. Group 4.6's full-blueprint deploy may then take the slot.

**Dependencies:** after `2.4.11.2` + `13.4.11.3` + `15.4.11.4` + `13.4.11.5`, and after `14.4.11.6` if it ran. **Commit:** `[5.4.11.7] docs: record the isolated Room teardown`

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

Group 4.8  LAST - after every acceptance subtask above and every Phase 3 acceptance box
           20.4.8.1 (Scenario_Player/, no dependency) ──► 21.4.8.2 (also needs 14.4.1.3 + 15.4.2.2)

Lane E (isolated Room - the bring-up lane; touches no file the fusion code touches)
  4.9  5.4.9.1 (day one, no dependency)
       2.4.9.2 ∥ 4.4.9.3 (day one) ──► 5.4.9.4 ──► 5.4.9.5 ──► 2.4.9.7
       5.4.9.6 (after phase-2 5.2.8.1, ∥ everything)
  4.10 5.4.10.1 (USER) ──► 5.4.10.2 ──► 5.4.10.3 ──► 6.4.10.4 (USER) ──► 5.4.10.5 (USER, also needs phase-2 5.2.9.4)
       ──► 5.4.10.6 ──► 5.4.10.7 (USER) ──► 5.4.10.8
  4.11 18.4.11.1 ──► { 2.4.11.2 ∥ 13.4.11.3 ∥ 15.4.11.4 }
       13.4.11.5 (USER, after 2.4.11.2)      14.4.11.6 (USER, only if 13.4.11.3 found no r4_tx)
       5.4.11.7 (USER, last - releases the Room slot)
```

**Recommended runtime order (single tree):** 18.4.3.1 → 6.4.4.1 → 6.4.4.2 → 15.4.1.1 → 14.4.1.2 → 14.4.1.3 → 15.4.2.1 → 15.4.2.2 → 15.4.2.3 → 18.4.3.2 → 18.4.3.3 → 15.4.5.1 → 15.4.2.4 *(if time)* → **group 4.9 → 4.10 → 4.11 (isolated Room)** → group 4.6 (full chain) when a second Room is available.

### Lane E in detail — parallel, sequential, and where it stops for a person

- **Parallel with everything, from day one:** `5.4.9.1` (a JSON file), `2.4.9.2` and `4.4.9.3` (two standalone Python scripts), and `5.4.9.6` (a workflow read). None of the four touches `ADA_ECU/src/`, so the whole of group 4.9 can run beside the fusion code rather than after it — which matters, because [§8.1 item 5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these) makes the bench an unproven dependency of the acceptance run.
- **Sequential, and genuinely so:** every arrow in groups 4.10 and 4.11. The walkthrough's own ordering is binding — the images before the nodes that reference them, the nodes before the pins, the pins before `validate` passes, the read-back before the Room slot, the Room before any log exists, and the logs saved before the teardown deletes them.
- **The three human gates** are `6.4.10.4` (canvas pins), `5.4.10.5` (node config) and `5.4.10.7` (deploy), plus `13.4.11.5` and `5.4.11.7` afterwards. **[[car-sky]] halts at each one, reports exactly what the human must do, and waits** — it does not improvise around a canvas step. The AI rows either side of them are `5.4.10.3`, `5.4.10.6`, `5.4.10.8` and the whole of group 4.11's reading.
- **Group 4.9 has one shared file:** `.github/workflows/phase4-ci.yml`, written by `15.4.5.1`, `5.4.9.5` and `2.4.9.7`. Sequence those three edits; nothing else in the lane collides with anything.
- **Group 4.10's entry condition is not "the code compiles"** — it is [§1.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#13-deliverable-prerequisites)'s seven deliverable rows present in the image, every one of which [§8.1 item 2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these) records as unwritten today. They are Phase 2, 3 and 4 work already planned above; group 4.10 consumes them and plans none of them.

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

### Walkthrough acceptance traceability (isolated Room)

Each row of [deploy-ada-ecu-walkthrough.md §8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance) maps to exactly one subtask, and no subtask invents a criterion §8 does not state.

| §8 proof | Log surface | Closed by |
|---|---|---|
| 1 — the relayed message is received and raises its event | ADA node log | `2.4.11.2` |
| 2 — both vehicles are in the track store | ADA node log | `13.4.11.3` |
| 3 — the warning reaches the IVI stand-in carrying both vehicles | IVI sink log | `15.4.11.4` |
| Observation — `PROFILE=out_of_range` admits nothing while `r2_ingest` still counts up | ADA node log | `13.4.11.5` |
| Observation — every warning in the run carries `cSource=v2x_relayed` | IVI sink log | `15.4.11.4` |
| §8's threshold record ("a pass at unknown thresholds proves nothing") | run doc | `18.4.11.1`, superseded by `14.4.11.6` if it runs |

**How the isolated Room relates to the milestone boxes above.** It closes none of them on its own and is not planned as if it did: the boxes are worded against *bench scenarios live*, which on the full blueprint means the real Scenario Player. What it does is retire every ADA-side unknown before that Room is booked — which is why groups 4.9–4.11 sit in front of group 4.6 rather than replacing it.

## Open items & flags (no Phase 4 subtask may silently close them)

| # | Item | Owner / closes at |
|---|---|---|
| 1 | **Contract tension on the Phase 4 output evidence — decided as option (i), flagged for the user.** The frozen R4 carries C's full R3 TrackedObject and B's *position*, not B's TrackedObject. This plan accepts that and proves B's full object from the `[EVT]` log. **Option (ii)** — an additive `trackedObjects` array — is planned in full as group 4.7 and **not started**; it is a frozen-contract change requiring a re-freeze across the ADA binding, the ADA emitter, the golden samples, both synced copy sets, the IVI Kotlin binding and both languages' round-trip tests. Recommendation: **do not run it** — reasoning in § Phase 4 output acceptance | **user** (ratification), group 4.7 |
| 2 | **`(proposal)` risk defaults proceed as proposed** — `RISK_NEAR_M=25`, `RISK_CRITICAL_M=15`, `RISK_TTC_WARN_S=6`, `RISK_TTC_CRITICAL_S=3`, `RISK_DWELL_MS=300`, `ASSESS_LOG_EVERY_MS=1000` ([HLD §6](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#6-configuration--no-hardcoded-tunables)). Chosen so `default.yaml`'s approach produces a visible `low → medium → high` progression and `c-out-of-range.yaml` never leaves `low`. Externalized, so ratification is a node-config edit | user |
| 3 | **Planner-designated test/tool paths beyond the HLD's list**: `tests/output/test_ivi_sender.cpp`; the `--fusion` / `--both-tracks` / `--r4-schema` modes of `tools/check_evt_log.py`; `tools/check_run_alignment.py` + `tools/tests/test_check_run_alignment.py` (group 4.8). Flagged to [[project-architecture]] as HLD-consistent additions | [[project-architecture]] (ack) |
| 4 | **Cross-phase dependency, not this phase's work:** the branch's `IVI_ECU/app/.../model/R4WarningMessage.kt` cannot decode this design's output — no `@SerialName` on `R4Geometry(ego, b, c)`, and it requires a `trackedObjects` array this design does not emit. **`main`'s `R4Message.kt` (sealed `R4WarningEvent`/`R4StateMessage` + `SceneGeometry.kt`) is the binding the IVI uses; the branch's parallel model is superseded.** Fixing it is **Phase 5's** first task ([HLD §11 item 4](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#11-open-items-and-flags)) — recorded here so it is not lost, planned nowhere in this file | [[project-planner]] → Phase 5 |
| 5 | **`15.4.2.4` (periodic awareness state) is optional and may be dropped** without affecting any acceptance box — R15 words it optional and the warning event alone renders the M1 demo. It must stay off by default (`STATE_RATE_HZ=0`), asserted in test | Phase 4, if time permits |
| 6 | **Phase 6 inherits this phase's live evidence.** `15.4.6.5`'s pcap is the ADA half of R19's corroborating capture; the V2X half is Phase 1's `6.1.10.5`. Phase 6 re-records both in one continuous run — it does not re-derive them | [[project-planner]] → Phase 6 |
| 7 | **R20/R21 are only partly planned, deliberately** ([report §7](../requirements/m1-run-timing-and-event-triggering.md)). Group 4.8 delivers the instrument (`21.4.8.2`) and §8(2)'s cheap half (`20.4.8.1`). **Not planned anywhere, and no subtask may assume it exists:** (a) **bench deadline scheduling** — `player/generator.py` still sleeps a fixed `period` with no drift correction (§2(d)), so a red K5 is a *measurement*, not a tool defect; (b) **detector real-time pacing and its §6.1 keys** `DETECTOR_REALTIME_PACING` / `DETECTOR_CLIP_FPS` / `DETECTOR_START_DELAY_S` — R20's detector half, ~2 h *inside* Phase 3 work that has not started, hence K4's `--clip-fps`/`--frame-stride` come from the command line and K4 skips without them (annotated at Phase 3 `12.3.2.1`); (c) bench `start_delay_s` / `reference_time_epoch` (§6.1) and the one `[EVT] ready` line per node of the §4.2 B-1 readiness pick; (d) **running the checker on a real run** — all three logs coexist only in Phase 6's continuous run, so this phase delivers the instrument and Phase 6 produces the verdict | **user** (accept/reject R20/R21 per §8(1)) → [[project-planner]] → Phase 6 |
| 8 | **§6.2's clock-domain ruling is a design decision the ADA HLD does not carry** — `CLOCK_REALTIME` for wire and log stamps, `CLOCK_MONOTONIC` for intervals **including track expiry**, cross-node timestamp arithmetic forbidden. It lands on Phase 2 `13.2.4.3` (expiry) and `2.2.3.1` (the R3 mapping, which is also PR-review defect M1) — annotated there, and carried as [phase2_tasks.md § Open items item 9](phase2_tasks.md#open-items--flags-no-phase-2-subtask-may-silently-close-them). No Phase 4 subtask may implement it silently | [[project-architecture]] (HLD amendment), Phase 2 |
| 9 | **`tools/ada-bench/` is implementation code outside the four node folders.** [node-code-layout.md](../.claude/rules/node-code-layout.md) opens with "The repo has exactly four code folders … **no implementation code lives outside them**", and [tools/netcheck/](../tools/netcheck/) already contradicts that sentence in practice. [Walkthrough §2.4](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#24-where-the-bench-sources-live-and-why) argues the placement against that rule and rejects all four alternatives, and this plan follows it — but the *rule* is [[project-architecture]]'s and does not yet name `tools/` as the sanctioned home for bench containers. **Requested: one paragraph in `node-code-layout.md` sanctioning it, with the same build rules the four folders carry.** No subtask here may edit that rule | [[project-architecture]] |
| 10 | **[node-ada-ecu.md](../requirements/car-sky-guide/node-ada-ecu.md) states three facts the walkthrough contradicts, and the deploy is blocked on the reconciliation.** Its § Blueprint node config still carries `command: ["./ada_ecu"]` and no `capabilities`, and its § Build & push still uses the stale registry host `registry.carsky.io`; [walkthrough §2.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#22-the-blueprint-definition-and-where-it-lives) and [§9](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#9-quick-reference) use `["./entrypoint.sh"]`, `["NET_RAW"]` and `registry.hackathon-2.carsky.io`. [§8.1 item 7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these) requires this resolved with the file's owner **before deploying**. **The fix is already planned as Phase 2 `5.2.9.4` and is not re-planned here**; it is promoted to a hard dependency of `5.4.10.5`. Reported to [[project-architecture]] as a node-fact discrepancy rather than patched in a subtask brief | [[project-architecture]] · Phase 2 `5.2.9.4` |
| 11 | **[Walkthrough §4.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#41-create-the-blueprint) names a batch payload file `batch-ada-isolated.json` with no designated path and no content spec.** Every other artifact in the document has a path. This plan resolves it by **deriving the payload at run time** from `blueprint-ada-isolated.json` rather than committing a second copy of the node data (`5.4.10.3`), which is the choice that keeps one source of truth — but the walkthrough should say so. Reported to [[project-researcher]] as a missing step detail; **not filled in a subtask brief** | [[project-researcher]] |
| 12 | **Room quota is now contended four ways, not three.** [§8.1 item 13](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these) and [ada-ivi-plan.md open item 5](ada-ivi-plan.md): two concurrent deployments across the whole account, against Phase 3 `5.3.6.2`, this phase's group 4.10 **and** group 4.6, and Phase 5 groups 5.8/5.9. Serialize the deploys and tear each Room down — `5.4.11.7` and `4.5.9.4` both end with one. The isolated Room holds a slot for its full duration | [[project-planner]] (scheduling) |
| 13 | **[§8.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these) items that authoring cannot retire, and where each first bites.** Item 1 (the whole route is unexercised) — the lane as a whole. Item 2 (none of the [§1.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#13-deliverable-prerequisites) ADA deliverables written) — group 4.10's entry condition; they are Phases 2–4 work planned above. Item 4 (the `[EVT]` names are design, not observation) — `2.4.11.2`, `13.4.11.3`. Item 6 (arm64 wheels for the detector) — Phase 3 `12.3.1.1`, and `5.4.9.6`'s 360-minute timeout check. Item 9 (a `/batch`-created 4-node blueprint accepting hand-drawn pins and then validating) — `6.4.10.4`, whose `validate` pass is the first proof of it. Item 10 (the defaults may produce no risk transition) — `13.4.11.3`, contingency `14.4.11.6`. Item 11 (detector frame rate on the Room's CPU) — Phase 3 `5.3.6.2`. Item 12 (`NET_RAW` granted) — `15.4.11.4`'s `[CAP]` criterion. **None is filled in by guessing; each is reported from the subtask that hits it** | per subtask |

---

*Created 2026-08-02 by project-planner from [phase2-4-ada-ecu-hld.md](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md) D4/D5/D7/D8/D9 and [milestone1.md § Phase 4](milestone1.md#phase-4--obscured-object-fusion-relayed-c--risk--warning-r13r15--runs--with-phase-3). 8 task groups, 24 subtasks: 15 agent-implemented (1 of them optional), 1 car-sky, 4 user-manual, 4 gated on user ratification (group 4.7, not started). Planned from zero; group 4.8 added the same day from [m1-run-timing-and-event-triggering.md](../requirements/m1-run-timing-and-event-triggering.md) §6.4.*

*Updated 2026-08-03 by project-planner, running stage 2 of [walkthrough-driven-delivery.md](../.claude/rules/walkthrough-driven-delivery.md) over [deploy-ada-ecu-walkthrough.md](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md). **Added groups 4.9, 4.10 and 4.11 — 22 subtasks** (7 agent, 8 [[car-sky]], 7 human), decomposed from that document and taking their acceptance from its §8; **corrected group 4.6 in place** — it is the full-blueprint route of §5.6, not the only route, its AI rows belong to [[car-sky]] per §7, and `15.4.6.5`'s pcap is out of the walkthrough's scope while remaining a milestone box. Five new open items (9–13) carry what could not be filled in without their owner. Nothing renumbered; 11 task groups, 46 subtasks.*
