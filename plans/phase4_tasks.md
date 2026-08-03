# Phase 4 — Obscured-object Fusion: relayed C + risk + warning (R13–R15): Full Task Breakdown

> **Authority & context:**
> - **Phase content:** [milestone1.md § Phase 4](milestone1.md#phase-4--obscured-object-fusion-relayed-c--risk--warning-r13r15--runs--with-phase-3) — its acceptance checkboxes are the phase output, plus the output-evidence box this plan sharpens (§ Phase 4 output acceptance).
> - **Design:** [ada-ecu-hld.md](../ADA_ECU/doc/ada-ecu-hld.md) — §4 folder structure, §6 env tables; and in the [decision record](../ADA_ECU/doc/ada-ecu-design-decisions.md) **D4** (CRA interface, registry, database), **D5** (risk vocabulary, thresholds, edge-triggered emission, composition), **D7** (output stage), **D8** (evidence stream), **D9** (capture on this node).
> - **Requirements:** [m1-cooperative-awareness.md §2](../requirements/m1-cooperative-awareness.md) R2, R4, R5, R6, R13, R14, R15, R18 — referenced by number, never restated.
> - **Deployment & verification procedure (groups 4.9–4.11):** [deploy-ada-ecu-walkthrough.md](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md) — the stage-1 artifact those groups are decomposed from, per [walkthrough-driven-delivery.md](../.claude/rules/walkthrough-driven-delivery.md). Its [§7 work division](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#7-work-division-between-ai-and-human) fixes each subtask's executor and its [§8 expected outputs and acceptance](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance) fixes each check's criteria. **Cite, never restate** — commands stay in the walkthrough.
> - **Capture (port, do not reinvent):** [traffic-capture-wireshark.md](../requirements/car-sky-guide/traffic-capture-wireshark.md) is the host-side procedure; `V2X_ECU/capture.sh` and `V2X_ECU/tools/extract_pcap.sh` on `main` are the working pair this phase's ADA copies are ported from, and their `[PCAP-BEGIN]` marker format is frozen.
> - **Phase 2 baseline (do not re-plan):** [phase2_tasks.md § Output](phase2_tasks.md#phase-2-overview) — store, R13 machine, `ICollisionRiskAssessment`, `registry` + `builtin_plugins.cpp`, `assessment_db` + its schema, `event_log`, `udp_socket`, `main.cpp` fusion tick, `tools/check_evt_log.py`, the image and `entrypoint.sh`'s capture hook.
> - **Rules:** [task-planning-conventions.md](../.claude/rules/task-planning-conventions.md); [node-code-layout.md](../.claude/rules/node-code-layout.md); [walkthrough-driven-delivery.md](../.claude/rules/walkthrough-driven-delivery.md).
>
> **Task ID legend:** `X.4.Z.W` — X = requirement served · 4 = this phase · Z = task group · W = subtask position. IDs are stable; never renumber, never reuse a retired one.
>
> **Runs in parallel with Phase 3.** This phase consumes the store, not the detector. Where a real own-sensor B track is needed before Phase 3 lands, the Phase 2 fixture (`tests/fixtures/own_sensor_mock.jsonl`) supplies it — except in the deployed Room, where the real detector must be in the image.

## Phase 4 overview

**Objective.** Turn live R2 traffic into a tracked ghost C, compose the scene (`d_AC = d_AB + d_BC`), assess NLOS collision risk through the R14 abstraction with the first plugin, and emit R4 warning events to the IVI on every risk transition — with the evidence to prove it, on the `[EVT]` log and on the wire.

**Input (must exist before start):**

- Phase 2 complete. In particular the CRA seam is frozen: this phase writes the **first** plugin and proves the D4 claim that adding one is one new file plus one line.
- An R2 source: `ADA_ECU/tools/mock_v2x_sender.py` (`3.2.6.3`) for CI and loopback; the bench mock of `2.4.9.2` for the isolated Room.
- **Phase 3 is required for the deployed Room only** — groups 4.10–4.11 need a real `own_sensor` B track from the detector inside the image. Everything in groups 4.1–4.5 runs against the Phase 2 fixture.

**This phase does not run the system test, and cannot** — that is the 5-node blueprint with the real IVI app *rendering* the warning, which needs the IVI ECU finished. It is planned once, in [phase5_minh_tasks.md § Task Group 5.10](phase5_minh_tasks.md#task-group-510--system-verification-test-serves-r4-r5-r6-r16-r17-r18-r19), and it is Phase 5's to run.

**What this phase runs is two Rooms, and the second is what makes its boxes live** — the [isolated ADA Room](#task-group-410--isolated-ada-test-create-and-deploy-the-room-serves-r5-r6) with both neighbours mocked, then the [alternative test](#task-group-412--alternative-test-real-bench-and-v2x-ecu-upstream-serves-r13-r15-r18) with the real bench and V2X ECU upstream. **No IVI app is needed for either**; only the God view is. What each rung newly exercises, and which box closes where: § Where each Phase 4 box actually closes.

**Output (phase acceptance):**

- [ ] C's track appears with `source = v2x_relayed` only and follows the full R13 lifecycle — Phase 2 `2.2.3.1`/`13.2.4.3` at unit level, `15.4.5.1` in CI, `13.4.11.3` + `13.4.11.5` on a deployed node against the bench mock, and **`13.4.12.1` against the real Scenario Player and V2X ECU**, which is the box's *"with bench scenarios live"* wording met in full.
- [ ] The NLOS plugin registers through the CRA interface; the abstraction + database schema are the committed artifacts (R14) — closed by `14.4.1.2` (one new file + one line in `builtin_plugins.cpp`) over Phase 2 `14.2.5.1`/`14.2.5.2`/`14.2.5.4`. **Closes entirely off-platform.**
- [ ] At least one R4 warning event per scenario run, carrying the risk state and the composed geometry (R15) — closed by `15.4.5.1` (CI, repeatable) and `15.4.11.4` (deployed, on the wire).
- [ ] The event list reconstructs a full run offline (R18) — closed by `18.4.3.2` (the tool) + `13.4.11.3` (its run over a deployed node's log).
- [ ] **Demo:** ADA logs — collision-risk event list — closed by `18.4.3.2` + `13.4.11.3`.
- [ ] **Output check:** the TrackedObjects of vehicle **B** and vehicle **C** are both shown to reach the IVI path, evidenced from the ADA `[EVT]` log **and** from a pcap of the ADA→IVI Ethernet traffic — closed by `13.4.11.3` (log path) + `15.4.6.5` (wire path), with the contract caveat in § Phase 4 output acceptance.

**Suggested branch (suggestion only — creation, checkout and push are the user's call):** `feat/phase4-ada-fusion-warning`. One branch for the whole phase. It branches from Phase 2's branch (or `main` after Phase 2 merges); it does **not** need Phase 3's branch. Groups 4.9–4.11 share no file with the fusion code — they write only under `tools/ada-bench/`, `.github/workflows/`, `requirements/car-sky-guide/` and `plans/doc/` — so `feat/phase4-ada-isolated-room` is a clean alternative if the user prefers to run the bring-up lane separately. Plan and run-doc commits go straight to `main` either way.

### Execution labels and the AI / Human split

Identical to [phase2_tasks.md § Execution labels](phase2_tasks.md#execution-labels) — *agent* · *car-sky* · *Human*. The split is not a preference: it comes from [walkthrough §7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#7-work-division-between-ai-and-human), and the reasons are structural. REST has no `ETHERNET` pin type, no update route for an existing node's config, and no delete operation; picking a Device spends one of two Room slots; and an agent session holds no browser and no GitHub token.

**Phase 4 is 20 *agent*, 9 *car-sky*, 8 *Human* subtasks.** Every human row is in groups 4.10, 4.11 and 4.12 — the whole of groups 4.1–4.6 and 4.9 is agent work.

| Where | AI does | A person does |
|---|---|---|
| Groups 4.1–4.5 (fusion code, tools, CI) | everything — 13 *agent* subtasks | nothing |
| Group 4.6 (pcap evidence) | extract and decode the capture (*car-sky*) | nothing required; opening the pcap in Wireshark is optional confirmation |
| Group 4.9 (bench image, lanes, blueprint file) | everything — 7 *agent* subtasks | nothing |
| Group 4.10 (create and deploy the Room) | registry confirmation, cloning `baseline_phase1` over REST, config read-back diff, phase polling (*car-sky* ×4) | confirm the CI jobs went green · reduce the clone and draw the sink's ethernet pin on the canvas · type each node's image and env in the Inspector · click Deploy (*Human* ×4) |
| Group 4.11 (the three checks, negative case, teardown) | save the logs, run all three checks and the evidence tools (*car-sky* ×4) | flip `PROFILE` and redeploy for the negative case · retune a risk threshold if no warning appeared · delete the deployment (*Human* ×3) |
| Group 4.12 (alternative test — real upstream) | nothing new; groups 4.10–4.11's *car-sky* subtasks re-run against this Room under their existing briefs | clone the blueprint, add the real bench and V2X ECU, deploy, swap the scenario, redeploy (*Human* ×1) |

**[[car-sky]] halts at every *Human* row**, reports exactly what the person must do, and waits. It never improvises around a canvas step, a browser step, or a deploy click.

**Implementation-subagent specification** for *agent* subtasks: as [phase2_tasks.md](phase2_tasks.md#execution-labels), with the write scope of each subtask being the folder its brief names — `ADA_ECU/` for groups 4.1–4.4, `tools/ada-bench/` for group 4.9, `.github/workflows/` and `requirements/car-sky-guide/` only where the brief says so.

### Subtask discipline and build commands

Identical to [phase2_tasks.md § Subtask discipline](phase2_tasks.md#subtask-discipline-applies-to-every-subtask-below); the [§ Per-node build commands](phase2_tasks.md#per-node-build-commands-cited-in-acceptance-below) table applies unchanged. Every C++ subtask's build/tests acceptance is **CI `ada-core-build` green on the pushed branch**. **Nothing in this file is started.**

### CI ruling for this phase

New lane in a new `.github/workflows/phase4-ci.yml` — *a lane belongs to the phase that created it*. Three jobs: `ada-e2e-loopback` (`15.4.5.1`), `ada-bench-image` (`5.4.9.5`) and `ada-bench-selfcheck` (`2.4.9.7`). Whichever of the three lands first creates the file with the standard `on:`/`concurrency:`/header block; **the three edits are sequenced against each other** and are the only shared-file contention inside this phase. `ada-core-build` (phase0-ci.yml), `ada-ecu-image` (phase2-ci.yml — checked against the walkthrough's build table by `5.4.9.6`, never re-created) and the Phase 3 lanes are reused, never duplicated.

### Every image the isolated Room runs is built and pushed by GitHub Actions

**Three nodes run containers. Two images serve them. Both are built and pushed by CI — nothing is built by hand, by anyone, on any machine** ([walkthrough §3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#3-build-the-images-on-ci)). A developer changes code and pushes; the lane rebuilds and republishes. **There is no local `docker build` step anywhere in this phase, and a brief that implies one is wrong.**

| Image | Node it runs as | `ROLE` | Build context | CI job | Job file | Planned by |
|---|---|---|---|---|---|---|
| `m1-ada-bench:latest` | **V2X Bench mock**, `10.99.0.11` | `ROLE=v2x_mock` | `tools/ada-bench/` | `ada-bench-image` | `phase4-ci.yml` | `5.4.9.4` (image), `5.4.9.5` (lane) |
| `m1-ada-bench:latest` | **IVI Sink mock**, `10.99.0.13` | `ROLE=ivi_mock` | `tools/ada-bench/` | `ada-bench-image` — *the same job, the same tag, pushed once* | `phase4-ci.yml` | as above |
| `m1-ada-ecu:latest` | **ADA ECU**, `10.99.0.12` — the node under test | — | `ADA_ECU/` | `ada-ecu-image` | `phase2-ci.yml` | Phase 2 `5.2.8.1` (lane); contents by Phase 2 `5.2.7.1` + this phase's `6.4.4.1` + Phase 3 `12.3.7.2` / `5.3.6.1` |

**Two images, three nodes — the two mocks share one image and differ only by `ROLE`** ([§2.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#23-the-bench-image--one-image-two-roles)). The bench image is **deployed twice**, under two different `ROLE` values, from one push of one tag. That is deliberate packaging and it is also the single most misconfigurable thing in the Room: two nodes carrying the *identical* `image` string, told apart by one environment variable. `5.4.10.5` is where a human types it, and where that risk is called out.

**Neither job substitutes for the other:** `5.4.9.5` creates the bench lane, `5.4.9.6` confirms the ADA lane, `5.4.10.1` confirms both went green, `5.4.10.2` confirms **both tags** reached the registry. A green run with one tag missing leaves a node hanging in `Provisioning`.

**`capabilities: ["NET_RAW"]` is unconditional on the ADA node.** Not contingent on a capture being wanted for a given run, not a property of one blueprint: every ADA node config in this plan carries it. `capture.sh` degrading to packet counters when the capability is not honoured is a robustness property of the script, never a reason to omit the capability.

## Phase 4 output acceptance — what "B and C reach the IVI" means, precisely

The requirement: *the TrackedObjects of vehicle B and vehicle C are sent to the IVI ECU, checkable from logs **or** from Wireshark captures of the Ethernet message.* This plan delivers **both** paths. One contract fact changes what each path can prove.

**What the frozen [`contracts/r4-ada-ivi.schema.json`](../contracts/r4-ada-ivi.schema.json) warning event carries:**

| Element | What it is | Proves |
|---|---|---|
| `object` | the **full R3 TrackedObject** of the *triggering* track — C | C's TrackedObject, on the wire |
| `geometry.vehicleB` | `{x, y}` position only | B's **position**, on the wire |
| `geometry.vehicleC` | `{x, y}` position, null when C's track has been erased | C's composed position |
| `geometry.ego` | `{0, 0}` | the frame origin |

**There is no full R3 TrackedObject for B in the frozen R4 message.** The wire proves *C's TrackedObject and B's position*; B's full TrackedObject is proven from the ADA `[EVT]` stream, where `own_sensor_ingest` and `track_transition` carry it.

**The plan accepts the frozen R4 as the evidence, and does not change it.** The reasoning, so it is not re-litigated:

1. **The requirement is disjunctive** — "from logs **or** from Wireshark". This satisfies it twice over: the `[EVT]` log carries **both** full TrackedObjects, and the pcap independently carries C's full R3 object plus B's position. Nothing is missing from the evidence; only the *transport* of B's full object differs.
2. **The alternative changes a frozen contract on the critical path.** CLAUDE.md governing principle 1 forbids changing a frozen contract without re-freezing across every consumer — the ADA `r4_message` binding, the ADA emitter, the golden samples, the synced copies in `ADA_ECU/contracts/` and `IVI_ECU/contracts/`, the IVI Kotlin binding, and the round-trip tests in both languages. That is a Phase 5 dependency added to a Phase 4 deliverable, days from the 2026-08-08 deadline.
3. **The IVI does not need it.** R17's warning view draws the God view of three vehicles from **positions**; `geometry` already carries all three.
4. **R15's own acceptance is already met** — "a pcap on the R6 network showing at least one R4 warning event during a scenario run".

**The output-acceptance box is therefore:**

> With a run live, **(a)** the ADA `[EVT]` log shows a `tracked` `own_sensor` TrackedObject for B **and** a `tracked` `v2x_relayed` TrackedObject for C, both with full R3 fields, plus at least one `r4_tx` carrying the emitted R4 body; **and (b)** a pcap of ADA→IVI UDP traffic decodes to the same R4 body — C's full R3 TrackedObject in `object`, B's position in `geometry.vehicleB`, C's composed position in `geometry.vehicleC` — correlated to the log by timestamp and byte length.

**Adding a `trackedObjects` array to R4 is out of scope** and no subtask in this plan implements it — § Open items item 1.

## Where each Phase 4 box actually closes

Stated once, so no subtask claims more than it can deliver.

| Closes | Boxes |
|---|---|
| **Off-platform** — unit tests + the `ada-e2e-loopback` CI lane | R14 plugin registration; the R18 event-report tool; the edge-triggered emission chain; the negative control (out-of-range ⇒ zero `r4_tx`) |
| **Isolated ADA Room** (groups 4.10–4.11) — real node, both neighbours mocked | R13 lifecycle of relayed C on a deployed node; ≥ 1 R4 warning on the wire; both TrackedObjects in the log; the pcap; the gate's negative case |
| **Alternative test** (group 4.12) — real bench + real V2X ECU upstream, sink mock downstream | the *"with bench scenarios live"* wording of every box above: relayed C originating in a real bench scenario, encoded as an R1 CPM and decoded by the real V2X ECU, and a scenario **swap** changing what the ADA node tracks |
| **System test** — [phase5_minh_tasks.md group 5.10](phase5_minh_tasks.md#task-group-510--system-verification-test-serves-r4-r5-r6-r16-r17-r18-r19), which needs the finished IVI | nothing this phase owns. It adds the **rendering** half — the God view drawing ghost C — which is R16/R17 and R19, not R13–R15 |

**The three are a ladder, not alternatives to pick between.** Each keeps the ADA node's own config byte-identical and changes only what surrounds it ([§5.6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#56-the-full-blueprint-route)), so a failure that appears at one rung is attributable to what that rung added:

1. **Isolated** — both neighbours mocked. Every ADA-side failure mode (wrong port, drifted message shape, a detector that never spawns, a risk level that never changes) surfaces where both sides are under this phase's control. Discovering one of these further up costs a second unknown to eliminate first.
2. **Alternative** — the upstream mock is replaced by the real relay. What it newly exercises is the R1 CPM encode/decode path and the real R2 producer shape; a regression here is Phase 1's, not the ADA node's.
3. **System test** — the downstream mock is replaced by the real IVI. What it newly exercises is rendering.

Run them in that order. Rung 2 is where this phase's acceptance boxes are closed at their full wording, and it is the last rung that needs no IVI app.

---

## Task Group 4.1 — Scene composition and the NLOS plugin (serves R15, R14)

> The business-logic half. `scene_composer` is geometry, `chained_collision` is rules; neither opens a socket, reads env, or formats a wire message ([HLD §8](../ADA_ECU/doc/ada-ecu-hld.md#mvc-separation)).

### [ ] `15.4.1.1` — Scene composer `src/fusion/scene_composer.{hpp,cpp}` *(agent)*

**Objective:** compose the ego-frame scene from the store — the D5 geometry, rebuilt against the frozen types.

**Scope:**

- `vehicleB = (d_AB, y_B)` from the **nearest `own_sensor` track**; `vehicleC = (d_AB + d_BC, y_B + y_BC)` — longitudinal sum, lateral component-wise, valid for the near-collinear convoy ([milestone1.md §2](milestone1.md#2-scope--assumptions)). `ego = (0, 0)` always.
- **"Nearest own-sensor track" is a real selection, not a formality** — the clip carries adjacent-lane and oncoming traffic, so several `own_sensor` tracks coexist. Nearest-by-`distance` is the rule; it is what `3.2.4.1`'s `nearest(Source)` implements, and it is not re-derived here.
- Returns a `SceneGeometry { ego, vehicleB, optional<vehicleC> }`; `vehicleC` is `nullopt` exactly when C's track has been erased — the case the frozen schema allows null for.
- **No B ⇒ no composition:** with no own-sensor track and no remembered `lastKnownB`, the function returns "not composable"; the caller decides what to do (D5's `b_unknown` path is `14.4.1.2`'s).
- Pure: takes a `const TrackStore&` and the values, reads no env, writes no log.
- Test `tests/fusion/test_scene_composer.cpp`: composed values against hand-computed numbers for at least three (d_AB, y_B, d_BC, y_BC) sets; the null-C case; the not-composable case; nearest-B selection when three own-sensor tracks exist.

**Acceptance:** ADA build + ctest green on CI.

**Dependencies:** after Phase 2 `3.2.4.1`. **Commit:** `[15.4.1.1] feat: add ego-frame scene composition`

### [ ] `14.4.1.2` — NLOS plugin `src/cra/plugins/chained_collision.{hpp,cpp}` + registration *(agent)*

**Objective:** the M1 plugin — the SVG's "Chained Collision", registering under the frozen R4 registry key `nlos_obstruction` (D4 naming reconciliation: one concept, three existing names, no new term).

**Scope:**

- Implements `ICollisionRiskAssessment`: `name() == "nlos_obstruction"`; `assess(RiskContext&)` reads the store and the `AssessmentDb`, returns a `RiskFinding`. **The plugin never emits** — the output stage decides transport.
- Band table exactly [D5](../ADA_ECU/doc/ada-ecu-design-decisions.md#d5--risk-vocabulary-and-edge-triggered-emission): `high` when C is `tracked` and (`d_AC ≤ RISK_CRITICAL_M` **or** `ttc ≤ RISK_TTC_CRITICAL_S`); `medium` when C is `tracked`, `d_AC ≤ RISK_NEAR_M`, and not `high`; `low` otherwise — including no `tracked` C at all.
- Derived values: `closingRateMps = -(d_AC(t) − d_AC(t−Δ)) / Δ` from the record's `previousDistanceM`/`lastUpdatedMs`; `ttcS = d_AC / closingRate` when the rate is positive, otherwise **null**.
- **`b_unknown` path:** with no own-sensor B and no `lastKnownB`, `d_AC` does not exist — return `low` with rationale `b_unknown` and log `assess_skipped_b_unknown`. Consequence to preserve: no `medium`/`high` is ever entered without a known B, so a clearing event can always fill the required `geometry.vehicleB` from `lastKnownB`.
- DB writes on every assessment: `riskState`, `distanceM`/`previousDistanceM`, `closingRateMps`, `ttcS`, `lastSnapshot` (C's R3 snapshot, carried past its erasure), `lastKnownB`, `lastUpdatedMs`, `rationale`.
- **Risk thresholds are separate constants from the R13 gate and must never alias it** (D5) — aliasing them collapses R14 into R13, leaving the assessment with nothing of its own to decide. `13.2.2.1`'s validator already enforces `RISK_CRITICAL_M < RISK_NEAR_M < GATE_ENTER_M`.
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
- Serialization **only** through `src/contracts/r4_message.hpp` — no hand-built JSON anywhere.
- Test `tests/output/test_warning_builder.cpp`: the emitted object **validates against the synced `ADA_ECU/contracts/r4-ada-ivi.schema.json`** (loaded from disk) for a `medium` case, a `high` case, and the **null-`vehicleC`** clearing case; `geometry.vehicleB` is always present and non-null (the `b_unknown` invariant); `object` carries all nine R3 fields.

**Acceptance:** ADA build + ctest green on CI; every emitted shape schema-validated in-test.

**Dependencies:** after `15.4.1.1` + `14.4.1.2`. **Commit:** `[15.4.2.1] feat: build R4 warning events from risk findings`

### [ ] `15.4.2.2` — IVI sender `src/output/ivi_sender.{hpp,cpp}` *(agent)*

**Objective:** one UDP datagram per R4 event to `IVI_ECU_HOST:IVI_ECU_PORT`, with the payload-carrying `r4_tx` event.

**Scope:** consumes `net::UdpSocket` only (no socket headers here); one datagram per event; send failure logged and counted, never thrown into the pipeline; logs `r4_tx` carrying the **full R4 body** (the V2X ECU's payload-carrying convention, D8) — this is what makes the log evidence self-sufficient. Test `tests/output/test_ivi_sender.cpp` (planner-designated path, § Open items item 3): a loopback listener receives the exact JSON of a built warning event; a send to an unreachable host is counted, not fatal; the `r4_tx` event's embedded body parses back to an equal event.

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

**Dependencies:** after `15.4.2.3`. **OPTIONAL — skip without penalty; it closes no box and the warning event alone triggers and renders the M1 demo.** **Commit:** `[15.4.2.4] feat: add the optional periodic awareness state stream`

---

## Task Group 4.3 — R18 evidence tooling (serves R18)

### [ ] `18.4.3.1` — R4 loopback sink `tools/mock_ivi_receiver.py` *(agent)*

**Objective:** the IVI stand-in for loopback and CI runs — receive R4 datagrams and make them checkable.

**Scope:** `ADA_ECU/tools/mock_ivi_receiver.py`, Python 3 stdlib; binds a host/port from CLI args/env (**no hardcoded peer**); prints one line per datagram (sequence, byte length, `type`, `warningType`, `riskState`) and appends the full body to an optional JSONL output file; `--validate` mode additionally validates each body against the synced `ADA_ECU/contracts/r4-ada-ivi.schema.json`; `--expect-min N` exits non-zero when fewer than N warning events arrived. Test equipment only — never enters the image.

**This is the CI-side sink and is distinct from `tools/ada-bench/mock_ivi.py` (`4.4.9.3`), which is the deployed one.** Neither imports the other; the no-cross-folder-imports rule keeps them separate, and each has its own reason to exist — this one validates against a schema with `jsonschema` available, the deployed one is standard-library-only because its image is Alpine.

**Acceptance:** `python -m py_compile` passes; a loopback self-check receives a sample R4 body byte-identical and validates it — evidence in the Status line.

**Dependencies:** none. **Commit:** `[18.4.3.1] feat: add the R4 loopback receiver`

### [ ] `18.4.3.2` — Collision-risk event list `tools/event_report.py` *(agent)*

**Objective:** the §1 demo-table artifact (D8) — render an `[EVT]` stream as the collision-risk event list a human reads.

**Scope:** `ADA_ECU/tools/event_report.py`, Python 3 stdlib; input = a saved `[EVT]` log (file or stdin), tolerating interleaved `[CAP]` lines; selects the `track_transition` + `risk_transition` + `r4_tx` subset and renders a chronological table — time, track id, source, state change, `d_AC`, `ttc`, risk level, whether an R4 was emitted; a `--summary` footer counting tracks admitted, transitions per level, and R4 events sent. **This is the "event list reconstructs a full run offline" acceptance made concrete**, so it must work on a log alone with no live process.

**Acceptance:** `python -m py_compile` passes; rendering a synthetic full-run log produces a table whose row count and R4 count match the log's — evidence in the Status line.

**Dependencies:** after `14.4.1.3` + `15.4.2.2` (event vocabulary complete). **Commit:** `[18.4.3.2] feat: add the collision-risk event report tool`

### [ ] `18.4.3.3` — Extend `tools/check_evt_log.py` with the Phase 4 chain *(agent)*

**Objective:** scripted assertion of the full ADA chain, including the **both-tracks** check the phase's output acceptance rests on.

**Scope — additive modes on Phase 2's script:**

- `--fusion`: per relayed track, `r2_ingest → track_transition → assessment → risk_transition → r4_tx` is complete; every `risk_transition` has exactly one matching `r4_tx`; no `r4_tx` without a preceding `risk_transition` (edge-triggered, D5); a `b_unknown` run produces `assess_skipped_b_unknown` and **no** `r4_tx`.
- `--both-tracks`: exits 0 only when the log contains **a `tracked` `own_sensor` TrackedObject (B) with all nine R3 fields and a `tracked` `v2x_relayed` TrackedObject (C) with all nine R3 fields**, plus at least one `r4_tx` whose embedded body carries C's R3 object in `object` and a non-null `geometry.vehicleB`. Non-zero exit naming which of the four is missing.
- `--r4-schema <path>`: validate every embedded `r4_tx` body against the synced R4 schema.
- An empty input stays a non-zero exit, as Phase 2 established.

**Acceptance:** `python -m py_compile` passes; demonstrated exit 0 on a synthetic complete log and non-zero on each of: a missing `r4_tx`, an `r4_tx` with no preceding transition, a log with only B, a log with only C, and an `r4_tx` with null `geometry.vehicleB` — evidence in the Status line.

**Dependencies:** after `15.4.2.3` + Phase 2 `18.2.6.5`. **Commit:** `[18.4.3.3] feat: assert the fusion chain and both-tracks evidence in the EVT checker`

---

## Task Group 4.4 — ADA→IVI traffic capture (serves R6, R15, R19; HLD D9)

> The V2X ECU's capture point cannot see this hop, so this node carries its own capture. The scripts are **duplicated per folder, not shared** — self-contained build contexts, no cross-node imports ([node-code-layout.md](../.claude/rules/node-code-layout.md)); the host-side procedure is the shared [traffic-capture-wireshark.md](../requirements/car-sky-guide/traffic-capture-wireshark.md).

### [ ] `6.4.4.1` — In-container capture `ADA_ECU/capture.sh` *(agent)*

**Objective:** live `[CAP]` text plus a rotating pcap exported through View Log — the ADA copy of the proven `V2X_ECU/capture.sh` (`[6.1.5.2]`).

**Scope:** port `V2X_ECU/capture.sh` to `ADA_ECU/capture.sh` with **no format changes** — the `[PCAP-BEGIN <name>]` / `[PCAP-END]` base64 markers must stay byte-compatible with the extraction tooling. Env, no literals: `CAPTURE_FILTER` (default `udp`), `PCAP_DIR` (`/data/capture`, `mkdir -p`), `CAPTURE_ROTATE_S` (60). Two tcpdump processes: `-i any -n -l -tttt $CAPTURE_FILTER` prefixed `[CAP]` to stdout, and `-w` rotating with each closed file base64-emitted between the markers. Degrades gracefully when `NET_RAW` is not honoured (log and stay alive) — **that fallback is robustness, not a licence to omit the capability**. Phase 2's `entrypoint.sh` (`5.2.7.1`) already starts it when present. **Also add the one-line `COPY capture.sh /app/` to the Dockerfile's runtime stage** — that is this subtask's only Dockerfile edit, and it is sequenced against Phase 3's three `COPY` lines.

**Acceptance:** `sh -n` and `bash -n` clean; LF line endings; exec bit set; `--export-one` round-trip base64-decodes byte-identically; `ada-ecu-image` lane green with `capture.sh` present in the built image. Runtime tcpdump evidence lands at `15.4.6.5`.

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

**Acceptance:** lane green on the pushed branch, both arms; the approaching arm observes a `low → medium → high` progression (the D5 defaults are chosen so an approach produces exactly that).

**Dependencies:** after `15.4.2.3` + `18.4.3.1` + `18.4.3.2` + `18.4.3.3`. **Commit:** `[15.4.5.1] chore: add the ADA end-to-end loopback CI lane`

---

## Task Group 4.6 — ADA→IVI wire evidence: the pcap (serves R6, R15, R19)

> **This group is one subtask** — the pcap, and nothing else. The 5-node system test needs the finished IVI ECU and is planned once, in [phase5_minh_tasks.md group 5.10](phase5_minh_tasks.md#task-group-510--system-verification-test-serves-r4-r5-r6-r16-r17-r18-r19); every node-level claim it would otherwise carry is already owned here — the image push by CI (`5.4.9.5` / `5.4.9.6`), the deploy and Running evidence by `5.4.10.5`–`5.4.10.8`, the relayed-C lifecycle by `13.4.11.3` + `13.4.11.5`, and the both-tracks log evidence by `13.4.11.3`.

### [ ] `15.4.6.5` — Extract and decode the ADA→IVI pcap *(car-sky)*

**Objective:** the wire half of § Phase 4 output acceptance, R15's own acceptance (a pcap showing ≥ 1 R4 warning event), and the ADA half of R19's corroborating capture.

**Which Room:** whichever is up. The ADA node's capture is identical in every composition, so this runs against the **isolated Room** of groups 4.10–4.11 — it does not book one, and it does not wait for the system test.

**Scope — per [traffic-capture-wireshark.md](../requirements/car-sky-guide/traffic-capture-wireshark.md), retargeted to this node:**

- The Room must run at least one `CAPTURE_ROTATE_S` period so a `[PCAP-BEGIN]` block exists; the ADA node log is already saved by `18.4.11.1`.
- Run `ADA_ECU/tools/extract_pcap.sh <saved.log>`; decode the resulting `.pcap` filtered to `udp.port == 47300`.
- **R4 payloads are plain JSON and read directly in the packet bytes** — no dissector caveat applies here, unlike the V2X hop's raw UPER. Record, from the capture itself: `warningType`, `riskState`, the **full `object`** (C's R3 TrackedObject), `geometry.vehicleB`, `geometry.vehicleC`.
- Correlate to `13.4.11.3`'s log evidence by timestamp and datagram length — the same event on both paths.
- **What this proves and what it does not:** C's full TrackedObject and B's *position* are on the wire; **B's full TrackedObject is proven from the log, not from the capture** — the frozen R4 does not carry it (§ Phase 4 output acceptance).

**Opening the `.pcap` in the Wireshark GUI is optional confirmation, not a pass criterion** — the decoded fields recorded here are the evidence, and they are obtainable without a GUI. [Walkthrough §5.4](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#54-traffic-evidence-and-wireshark-scope) puts producing a `.pcap` out of scope of *that* procedure and makes it no pass criterion of its three checks; this subtask exists because [milestone1.md § Phase 4](milestone1.md#phase-4--obscured-object-fusion-relayed-c--risk--warning-r13r15--runs--with-phase-3) box (b) and R15's own acceptance require one anyway.

**Acceptance:** the `.pcap` archived under `plans/doc/` and the decoded fields recorded in `plans/doc/phase4-ada-isolated-room-run.md`, showing ≥ 1 R4 warning event correlated to the log.

**Dependencies:** after `6.4.4.1` + `6.4.4.2` + `18.4.11.1`. **Commit:** `[15.4.6.5] docs: record the ADA to IVI pcap evidence`

---

## Task Group 4.9 — Isolated-Room prerequisites: the bench image, the CI lanes, the blueprint reference (serves R2, R4, R5)

> Stage 2 of [walkthrough-driven-delivery.md](../.claude/rules/walkthrough-driven-delivery.md) over [deploy-ada-ecu-walkthrough.md](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md). Every subtask below cites the section that governs its step; **no brief carries a copy of the procedure**, and no command is restated here.
>
> **This group is the walkthrough's [§8.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these) made into work, which is why it is scheduled ahead of every Room step.** Items 3, 5 and 8 name three artifacts that do not exist — the two CI jobs, the bench image with its two roles and log shapes, and the blueprint file. Nothing in groups 4.10–4.11 can start until they do, and each is cheap to land off-platform. **All seven subtasks are *agent* work and none touches the platform.**
>
> **Where the bench lives.** `tools/ada-bench/` at the repository root, one image `m1-ada-bench:latest`, two roles selected by `ROLE` — sanctioned by [node-code-layout.md § `tools/`](../.claude/rules/node-code-layout.md#tools--test-equipment-and-ecu-mocks), which names it beside `tools/netcheck/` and `tools/comms_check/` and applies the four node folders' build rules to it. **No subtask in this group writes inside `V2X_ECU/`, `IVI_ECU/` or `ADA_ECU/`** — the bench must be able to change without rebuilding the thing it tests.
>
> **Run doc:** `plans/doc/phase4-ada-isolated-room-run.md`, created by `5.4.9.6` and appended by every subtask after it. It is the single record for this phase's deployed evidence.

### [ ] `5.4.9.1` — Author `requirements/car-sky-guide/blueprint-ada-isolated.json` *(agent — day one, blocks nothing else in this group)*

**Objective:** land the blueprint definition [§2.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#22-the-blueprint-definition-and-where-it-lives) designates but which [§8.1 item 8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these) records as not yet created.

**Scope:** create the file at exactly that path, beside [blueprint-m1-cooperative-awareness.json](../requirements/car-sky-guide/blueprint-m1-cooperative-awareness.json), whose shape it follows. **Its content is [§2.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#22-the-blueprint-definition-and-where-it-lives)'s JSON block, transcribed byte-for-byte** — four nodes, flat `config` per [carsky-rest-api-blueprint.md § Node config is flat](../requirements/car-sky-guide/carsky-rest-api-blueprint.md#node-config-is-flat-not-wrapped), empty `pins` and `edges`. Nothing is invented, reordered or "improved": `5.4.10.5` types the Inspector fields from it and `5.4.10.6` diffs the live blueprint against it, so a divergence here is a divergence in both. **The file is a config specification, never an import payload** — the Room is made by cloning `baseline_phase1` ([§2.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#22-the-blueprint-definition-and-where-it-lives)), and importing this file would produce a pinless blueprint.

- **`pins` and `edges` stay empty.** The clone supplies three of the four pins and `6.4.10.4` draws the fourth by hand, so no pin in the live blueprint originates in this file. Declaring pins here would make `5.4.10.6`'s read-back diff compare against fields this file cannot be the source of. The addresses are [§4.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#42-wire-the-ethernet-pins)'s table.
- The `description` field already carries the not-importable-pins warning in §2.2's text; keep it verbatim rather than paraphrasing.
- **Transcribe the ADA node's `command: ["./entrypoint.sh"]` and `capabilities: ["NET_RAW"]` exactly, and treat neither as conditional.** `entrypoint.sh` is product code in the shipped image (the `V2X_ECU/entrypoint.sh` pattern, written by Phase 2 `5.2.7.1`): it backgrounds `capture.sh` and `exec`s the binary so the node is PID 1 and takes SIGTERM directly. [node-ada-ecu.md](../requirements/car-sky-guide/node-ada-ecu.md) already agrees on both, and on the registry host.

**Acceptance:** the file parses as JSON; its four nodes, their `nodeType`s, every `image`, `command`, `capabilities` and env key/value, and the bridge's `bridgeMode`/`subnet` match §2.2 field for field; `pins` and `edges` are empty arrays; the addresses and ports agree with [§9 Quick reference](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#9-quick-reference) (`.11` / `.12` / `.13` on `10.99.0.0/24`, bridge `.1`, ports `47200` and `47300`).

**Dependencies:** none — starts immediately. **Commit:** `[5.4.9.1] docs: add the isolated ADA blueprint definition`

### [ ] `2.4.9.2` — Bench emitter `tools/ada-bench/mock_v2x.py` *(agent)*

**Objective:** the `ROLE=v2x_mock` role of [§2.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#23-the-bench-image--one-image-two-roles) — a relayed-object emitter standing in for the V2X ECU **at its output edge only**, performing no decoding and sending no encoded frames.

**Scope:** Python 3, standard library only (the image is Alpine plus `python3` and `tcpdump`; no pip install).

- One UDP datagram per tick to `TARGET_HOST:TARGET_PORT` at `RATE_HZ`, after `START_DELAY_S` seconds so the ADA node is listening first. **Every one of those is an environment variable and none is a literal** ([§3.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#31-write-the-bench-scripts); [CLAUDE.md](../CLAUDE.md) governing principle 5).
- The datagram body, the `PROFILE` behaviours (`approaching`, `out_of_range`) and the `STATION_ID` / `OBJECT_ID` / `START_DISTANCE_M` / `MIN_DISTANCE_M` / `CLOSING_RATE_MPS` / `LATERAL_M` / `OBJECT_SPEED_MPS` semantics are §2.3's bullet list — implement them from there, do not re-derive them.
- **The message must match [`ADA_ECU/contracts/r2-v2x-object.schema.json`](../ADA_ECU/contracts/r2-v2x-object.schema.json) field for field.** Keep the field list byte-identical to that copy, or the bench will pass a message the real consumer rejects. SI units, `classification: "vehicle"`, `object.confidence` and `sender.speed` populated so no nullable field is exercised by accident. The failure this prevents is the `parse_reject`-on-every-datagram row of [§6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#6-troubleshooting), whose stated remedy is "fix the emitter, not the node".
- **C's asserted distance must stay beyond B's at every instant** — that is what makes C genuinely occluded and keeps `check_zero_c.py` rule 3 meaningful ([clip sidecar § What this obliges downstream](../ADA_ECU/media/ego-b-occluding-c.source.md#what-this-obliges-downstream)). The clip's B closes from ~60 m to ~10 m, so an `approaching` profile starting below that band would place C in front of B. Choose `START_DISTANCE_M` and `CLOSING_RATE_MPS` against those numbers and record the choice in the module docstring.
- One `[TX]` log line per datagram, in §2.3's exact shape — `2.4.11.2`'s pass criterion counts them against the ADA node's `r2_ingest` count, so the prefix and the `seq`/`objectId`/`distance`/`bytes` fields are load-bearing, not decoration.
- **Out of scope:** the entrypoint, the capture script and the Dockerfile (`5.4.9.4`); anything the sink does (`4.4.9.3`); any encoding, ASN.1 or Vanetza path.

**Acceptance:** `python -m py_compile` passes; a loopback self-check sends one datagram of each profile to a local socket and the received body **validates against `ADA_ECU/contracts/r2-v2x-object.schema.json`** — evidence in the Status line. The repeatable CI form is `2.4.9.7`.

**Dependencies:** none. Parallel with `4.4.9.3`. **Commit:** `[2.4.9.2] feat: add the V2X bench relayed-object emitter`

### [ ] `4.4.9.3` — Bench sink `tools/ada-bench/mock_ivi.py` *(agent)*

**Objective:** the `ROLE=ivi_mock` role of [§2.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#23-the-bench-image--one-image-two-roles) — bind `0.0.0.0:LISTEN_PORT`, log and check every warning datagram. It stands in for the Android node and is a Linux container precisely so it can do what the real node cannot: log, check and capture ([§2.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#21-topology)).

**Scope:** Python 3, standard library only.

- Two lines per datagram plus a `[SUMMARY]` every `SUMMARY_EVERY_S` seconds, in §2.3's exact shapes. **`15.4.11.4` greps these strings literally**, so `[RX]`, `[CHECK]`, `[SUMMARY]`, `both_vehicles=yes`, `c_source_relayed=yes` and the field names beside them are the contract of this file.
- **Explicit field checks, not full schema validation** — §2.3 fixes that so the image stays standard-library only. `both_vehicles=yes` requires `geometry.vehicleB` and `geometry.vehicleC` both present with numeric `x` and `y`; `c_source_relayed=yes` requires `object.source` to be exactly `v2x_relayed`; `rejected` counts datagrams that were not valid JSON or whose `type` was neither `warning` nor `state`.
- The field list is taken from [`ADA_ECU/contracts/r4-ada-ivi.schema.json`](../ADA_ECU/contracts/r4-ada-ivi.schema.json) and kept byte-identical to it. **A null `geometry.vehicleC` is legitimate before C is first tracked** and must produce `both_vehicles=no` rather than a rejection — [§5.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#53-check-3--the-warning-reaches-the-ivi-stand-in-carrying-both-vehicles)'s failure note turns on being able to read `no` on the first `seq` and `yes` afterwards.
- **Out of scope:** rendering anything, the capture (`5.4.9.4` owns `capture.sh`), and any dependency on `ADA_ECU/tools/mock_ivi_receiver.py` — that is `18.4.3.1`'s CI-side tool and **is not shipped in this image**.

**Acceptance:** `python -m py_compile` passes; a loopback self-check feeds it the committed `contracts/samples/r4-warning.json` body and a null-`vehicleC` variant, and the emitted `[RX]`/`[CHECK]`/`[SUMMARY]` lines match §2.3's shapes with `both_vehicles` reading `yes` then `no` respectively and `rejected=0` — evidence in the Status line.

**Dependencies:** none. Parallel with `2.4.9.2`. **Commit:** `[4.4.9.3] feat: add the IVI bench warning sink and checker`

### [ ] `5.4.9.4` — Bench image `tools/ada-bench/{entrypoint.sh,capture.sh,Dockerfile}` *(agent)*

**Objective:** one image serving both roles — [§2.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#23-the-bench-image--one-image-two-roles)'s five-file table completed, so **a deploy alone produces evidence and no shell session is ever needed** ([§3.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#31-write-the-bench-scripts)).

**Scope:** the three remaining rows of §2.3's file table.

- `entrypoint.sh`: `[BOOT]` line, launch `capture.sh` in the background, then `exec` the script named by `ROLE`. A misspelled `ROLE` must fail loudly at start — the climbing-restart-count row of [§6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#6-troubleshooting) is what that diagnoses.
- `capture.sh`: `tcpdump -i any -n -l` on `CAPTURE_FILTER`, each line prefixed `[CAP]`, falling back to packet counters without `NET_RAW`. **Port `tools/netcheck/capture.sh` rather than writing a new one** — same category of artifact, and `15.4.11.4`'s `[CAP]` criterion depends on the line shape. It writes no rotating pcap: the `[PCAP-BEGIN]` machinery is `ADA_ECU/capture.sh`'s (`6.4.4.1`), not this one's.
- `Dockerfile`: `FROM alpine:3.20`, `apk add python3 tcpdump`, `WORKDIR /app`, copy the four files, `CMD ["./entrypoint.sh"]`. **`command` is relative to `/app`** — §4.3's closing note: `./entrypoint.sh` works and `/entrypoint.sh` does not exist, so the container dies at start.
- Self-contained context: no file outside `tools/ada-bench/` enters the build, and nothing here imports from a node folder ([node-code-layout.md § Build rules](../.claude/rules/node-code-layout.md#build-rules-all-container-nodes)).

**Acceptance:** `sh -n` and `bash -n` clean on both scripts, LF line endings, exec bit set; the `5.4.9.5` lane's build succeeds; running the built image with `ROLE=v2x_mock` prints `[BOOT]` and then `[TX]` lines, and with `ROLE=ivi_mock` prints `[BOOT]` and binds.

**Dependencies:** after `2.4.9.2` + `4.4.9.3` (the two files it `exec`s must exist). **Commit:** `[5.4.9.4] feat: add the ADA bench image entrypoint, capture and Dockerfile`

### [ ] `5.4.9.5` — CI lane `ada-bench-image` *(agent)*

**Objective:** the first of the two jobs [§3.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#32-build-and-push-the-images-on-ci) requires — **the lane that makes a push build and publish the bench image**, which nothing else in the repository does.

**What this job covers.** **One image, `m1-ada-bench:latest`, which two of the Room's three nodes run** — the V2X Bench mock under `ROLE=v2x_mock` and the IVI Sink mock under `ROLE=ivi_mock`. **One build, one push, one tag, deployed twice** — the lane does not build a second mock image and must not be extended to, because the roles are an env-var selection inside `entrypoint.sh` (`5.4.9.4`), not separate artifacts. The third node's image is `ada-ecu-image`'s and is confirmed by `5.4.9.6`.

**Scope:** one job `ada-bench-image` building context `tools/ada-bench/` and pushing `m1-ada-bench:latest`, in the shape of `v2x-ecu-image` in [phase1-ci.yml](../.github/workflows/phase1-ci.yml). Every flag and value comes from §3.2's command block — `--platform linux/arm64`, `--provenance=false --sbom=false`, registry host `registry.hackathon-2.carsky.io` used identically in the login, the tag and the node's `image` field, and the key read only from the `CARSKY_ZOT_API_KEY` repository secret ([zot-registry-api-key.md § CI secret](../requirements/car-sky-guide/zot-registry-api-key.md#ci-secret-carsky_zot_api_key)). Push only when the secret exists, with the same notice-and-exit-0 guard the Phase 1 lanes use; verify the pushed artifact through the existing `.github/actions/verify-arm64-image` composite.

**File placement:** `.github/workflows/phase4-ci.yml` — this phase's § CI ruling. That file is also `15.4.5.1`'s and `2.4.9.7`'s; **whichever lands first creates it** with the standard `on:`/`concurrency:`/header block, and the others add only their job. Sequence the three edits — they are the only shared file in this phase.

**Acceptance:** workflow YAML valid; run-blocks `bash -n` clean; the lane is green on the pushed branch and the build step completes in roughly a minute (§3.2: Alpine plus two packages under emulation).

**Dependencies:** after `5.4.9.4`. **Commit:** `[5.4.9.5] chore: add the ADA bench image build-push CI lane`

### [ ] `5.4.9.6` — Confirm the `ada-ecu-image` lane matches §3.2's table *(agent)*

**Objective:** the second job of [§3.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#32-build-and-push-the-images-on-ci) exists and is correct. §3.2 is unambiguous about the order: *"If a job's context, tag, registry host or platform flag does not match the table above, fix the `.yml` first — there is nothing to verify without it."*

**What this covers.** `m1-ada-ecu:latest` — the **third** of the Room's three node images. With `5.4.9.5`'s `ada-bench-image` it completes the set: **two CI jobs, two images, three nodes, zero hand builds.** If this lane is wrong or missing, the node under test cannot be deployed at all, and the two mocks alone prove nothing.

**Scope:** the job is **already planned as Phase 2 `5.2.8.1`** and is **not** re-planned here — IDs are never duplicated. This subtask's single objective is the confirmation and, if needed, the correction:

- Check the live job against §3.2's table row for row: job name, build context, image tag, registry host, `--platform linux/arm64`, `--provenance=false --sbom=false`, and the secret's name.
- Check its timeout against §3.2's closing paragraph — the ADA image compiles C++ and installs the detector's Python dependencies under emulation, so **360 minutes** is the value. A shorter cap is the "red after 360 minutes" row of [§6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#6-troubleshooting) waiting to happen.
- Confirm the image the lane produces carries all four ADA runtime deliverables — `ada_ecu`, `entrypoint.sh`, `capture.sh`, `detector/` + `models/` + `media/` — since a Room deployed from an image missing the detector cannot close `13.4.11.3`'s own-sensor half.
- Any mismatch is fixed in the `.yml` in this same commit. A clean check produces no code change and the commit is the record — **this subtask creates `plans/doc/phase4-ada-isolated-room-run.md`.**

**Acceptance:** every row of §3.2's table confirmed against the live workflow and recorded, with any correction applied and the lane green; the run doc exists and names the two job files (`phase2-ci.yml` for the ADA image, `phase4-ci.yml` for the bench image) so no later reader hunts for them.

**Dependencies:** after Phase 2 `5.2.8.1` + `6.4.4.1` + Phase 3 `5.3.6.1` (the image must carry everything before its contents are confirmed). Parallel with `5.4.9.5`. **Commit:** `[5.4.9.6] docs: confirm the ADA ECU image lane against the walkthrough build table`

### [ ] `2.4.9.7` — CI lane `ada-bench-selfcheck` — emitter → sink loopback *(agent)*

**Objective:** prove [§8.1 item 5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these) — *"the bench image, its two roles, its env names and its log-line shapes … not yet written"* — **before a Room slot is spent on them**. An unproven route is proved ahead of the work depending on it; this is the cheapest place that can happen, and the only check in this group that runs without the platform.

**Scope:** a job in `phase4-ci.yml`. Start `mock_ivi.py` on a loopback port; start `mock_v2x.py` with `TARGET_HOST=127.0.0.1`, that port, a short `START_DELAY_S` and `PROFILE=approaching`; feed the sink a handful of synthetic R4 warning bodies from `contracts/samples/`; then assert **all** of:

- the emitter's datagrams validate against `ADA_ECU/contracts/r2-v2x-object.schema.json` (the `parse_reject` failure mode, caught here rather than in a Room);
- the emitter's `[TX]` lines and the sink's `[RX]`/`[CHECK]`/`[SUMMARY]` lines parse under the exact greps `2.4.11.2` and `15.4.11.4` use — the lane fails if a prefix or field name drifts;
- `PROFILE=out_of_range` holds distance at `START_DISTANCE_M`, which is what makes `13.4.11.5`'s negative case meaningful;
- a malformed datagram increments the sink's `rejected` counter rather than killing it.

**Out of scope:** anything requiring the platform, a registry or a deployed node.

**Acceptance:** lane green on the pushed branch, with a non-zero examined-datagram count asserted so the lane cannot pass vacuously.

**Dependencies:** after `2.4.9.2` + `4.4.9.3` + `5.4.9.5` (same file). **Commit:** `[2.4.9.7] chore: add the ADA bench loopback self-check CI lane`

---

## Task Group 4.10 — Isolated ADA test: create and deploy the Room (serves R5, R6)

> [Walkthrough §3.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#33-confirm-the-run-passed-and-the-images-landed) through [§4.5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#45-deploy), in the order the document states — **that ordering is binding**: the images must exist before the nodes reference them, the nodes before the pins, the pins before validation passes, and the config read-back before a Room slot is spent.
>
> **The Room:** Ethernet Bridge · V2X bench mock `10.99.0.11` · **ADA ECU `10.99.0.12` — the node under test** · IVI sink mock `10.99.0.13`. Both neighbours are mocks from `tools/ada-bench/`; there is no V2X ECU, no Scenario Player and no IVI app in it.
>
> **Execution stops and waits for a person four times.** [§7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#7-work-division-between-ai-and-human) assigns the canvas, every node-config edit and the deploy to a human, and the reasons are structural: REST has no `ETHERNET` pin type, no update route and no delete operation, and picking the Device spends one of two Room slots.

| Step | Subtask | Owner |
|---|---|---|
| Confirm the two image jobs passed | `5.4.10.1` | **Human** |
| Confirm both images reached the registry | `5.4.10.2` | AI — *car-sky* |
| Clone `baseline_phase1` | `5.4.10.3` | AI — *car-sky* |
| Reduce the clone and wire the sink's ethernet pin | `6.4.10.4` | **Human** |
| Configure the correct image and env on each node | `5.4.10.5` | **Human** |
| Read the stored config back and diff it | `5.4.10.6` | AI — *car-sky* |
| Deploy the blueprint | `5.4.10.7` | **Human** |
| Poll to `Running`, resolve every `nodeKey` | `5.4.10.8` | AI — *car-sky* |

### [ ] `5.4.10.1` — Confirm the two image jobs passed *(Human)*

**Objective:** the first half of [§3.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#33-confirm-the-run-passed-and-the-images-landed) — green on **both** `ada-bench-image` and `ada-ecu-image` in the newest Actions run. **Both, not either**: the two jobs between them build all three of the Room's node images, so one green job is half a Room.

**Scope:** GitHub → Actions → the newest run → the two jobs. §7 assigns this to a human because *an agent session holds no GitHub token*; the same note records that it **flips to AI on a machine with an authenticated `gh` CLI**, in which case §3.3's two `gh` commands replace the browser and this subtask is handed to [[car-sky]] instead. A red push step printing `secret not set` means the credential of [§1.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#12-cloud-platform-access) is missing, not that the code is wrong.

**Acceptance:** both job names and their conclusions recorded in `plans/doc/phase4-ada-isolated-room-run.md`, with the run id. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `5.4.9.5` + `5.4.9.6`, and a commit pushed. **Commit:** `[5.4.10.1] docs: record the image CI run for the isolated ADA Room`

### [ ] `5.4.10.2` — Confirm both images reached the registry *(car-sky)*

**Objective:** the second half of [§3.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#33-confirm-the-run-passed-and-the-images-landed) — the registry's own answer, **independent of what the run reported**.

**Scope:** the three `curl` calls §3.3 gives, against the catalog and both tag lists. The Zot credential is supplied at run time and never stored ([zot-registry-api-key.md](../requirements/car-sky-guide/zot-registry-api-key.md)). Confirm each manifest is a **single-platform `linux/arm64` image, not a manifest index**: an index is what makes a node hang in `Provisioning`, and §3.3's warning is that the failure "appears late and reads like a network fault".

**Acceptance, verbatim from [§3.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#33-confirm-the-run-passed-and-the-images-landed):** both repository names present in the catalog, and `{"name":"m1-ada-ecu","tags":["latest"]}` / `{"name":"m1-ada-bench","tags":["latest"]}` returned by the tag lists. **Both tags, not one** — `m1-ada-bench:latest` is what *two* of the three nodes pull and `m1-ada-ecu:latest` is what the node under test pulls, so a single missing tag leaves either the whole bench or the subject of the test unable to start. Both digests recorded in the run doc. A name missing here stops the group.

**Dependencies:** after `5.4.10.1`. **Commit:** `[5.4.10.2] docs: record the registry confirmation for both isolated-Room images`

### [ ] `5.4.10.3` — Clone `baseline_phase1` *(car-sky)*

**Objective:** [§4.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#41-create-the-blueprint)'s one scripted call — `POST /api/v1/blueprints/{baselineId}/clone` against `baseline_phase1`, the sanctioned clone source for every Room after the smoke test ([carsky-4-node-blueprint.md § The blueprints on CarSky](../requirements/car-sky-guide/carsky-4-node-blueprint.md#8-the-blueprints-on-carsky)).

**Scope:** the clone call and nothing else. **Cloning is the only route that preserves `ethernet` pins** — REST cannot create them and an import silently drops them (§4.1), so this call is what makes `6.4.10.4` a one-pin job instead of a four-pin one. Do not create a blueprint from scratch, do not `POST` or import `requirements/car-sky-guide/blueprint-ada-isolated.json`: that file is the **config specification** this Room is edited to and diffed against ([§2.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#22-the-blueprint-definition-and-where-it-lives)), never an import payload.

- **Never edit `baseline_phase1` itself** — every other Room is cloned from it, so a Room-specific edit there propagates to every later clone (§4.1's second blockquote).
- **Never edit the `<name>-deploy` snapshot** a deploy creates: §4.1's first blockquote — edits to it appear to save and are ignored by the next deploy.

**Acceptance:** the clone returns an `id`, and a `GET` on it shows all five baseline nodes with their pins and edges intact. The `id` recorded in the run doc — every call in groups 4.10 and 4.11 needs it. A clone that came back without pins is reported as a finding against [§8.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these) item 9 before `6.4.10.4` starts, because it changes that subtask's size.

**Dependencies:** after `5.4.9.1` + `5.4.10.2`. **Commit:** `[5.4.10.3] docs: record the isolated blueprint cloned from baseline_phase1`

### [ ] `6.4.10.4` — Reduce the clone to the isolated topology and validate *(Human, Nydus canvas)*

**Objective:** [§4.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#41-create-the-blueprint)'s four canvas edits plus [§4.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#42-wire-the-ethernet-pins)'s one pin — turning the five-node clone into the four-node isolated Room of [§2.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#21-topology), with three edges all terminating at the bridge. A star, not a chain.

**Scope:** §4.1's four-row edit table and §4.2's pin table, in that order:

| # | Edit | Node |
|---|---|---|
| 1 | **Rename** the clone in the blueprint Inspector — the name is the only place the differentiator goes | — |
| 2 | **Delete** the bench Scenario Player node; its pin and edge go with it | `10.99.0.10` |
| 3 | **Delete** the IVI Skycraft node and **add a Container node** in its place — a node's type cannot be changed, so the sink is a new node | `10.99.0.13` |
| 4 | **Draw one `ETHERNET`/`OUTPUT` pin** on that new sink node at `10.99.0.13` and wire it to the bridge | `10.99.0.13` |

The V2X ECU node at `.11` and the ADA node at `.12` are **not touched here** — their pins and edges survive the clone, and `5.4.10.5` handles the V2X node's image and env. The pin shape is at [node-ada-ecu.md § Pins](../requirements/car-sky-guide/node-ada-ecu.md#pins); only the address differs per node, and same-type wiring only.

**This is canvas work with no scripted alternative and it is the reason the whole procedure has a human in it:** REST cannot create `ETHERNET` pins, cannot change a node's type, and has no delete operation — so every row above is done by hand or not at all.

**Acceptance, verbatim from [§4.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#42-wire-the-ethernet-pins):** `POST /api/v1/blueprints/{id}/validate` returns a **pass**. A 422 naming a node means that node still has no pin. The validate call itself is a read and is run by [[car-sky]]; the drawing is not. The clone's chosen name and the four edits recorded in the run doc.

**Dependencies:** after `5.4.10.3`. **Commit:** `[6.4.10.4] docs: record the isolated blueprint reduction and validation`

### [ ] `5.4.10.5` — Configure each node's image and env in the Inspector *(Human, Nydus UI)*

**Objective:** [§4.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#43-configure-each-nodes-image) — every node's `image`, `command`, `capabilities` and `env` set and confirmed against §2.2.

**Three nodes, three image strings to type — and two of them are identical.** This is §4.3's three-row image table, reproduced because it is the one thing this subtask exists to get right; every other value is cited, not restated.

| Node | Address | `image` | Distinguishing env | Built by |
|---|---|---|---|---|
| V2X Bench (mock) | `10.99.0.11` | `registry.hackathon-2.carsky.io/m1-ada-bench:latest` | **`ROLE=v2x_mock`** | CI job `ada-bench-image` |
| ADA ECU | `10.99.0.12` | `registry.hackathon-2.carsky.io/m1-ada-ecu:latest` | — | CI job `ada-ecu-image` |
| IVI Sink (mock) | `10.99.0.13` | `registry.hackathon-2.carsky.io/m1-ada-bench:latest` | **`ROLE=ivi_mock`** | CI job `ada-bench-image` |

**The risk this brief exists to call out: rows 1 and 3 carry the *same* image string and differ only by one environment variable.** Nothing on the canvas distinguishes them, so the misconfiguration is invisible on inspection and is the most likely single defect in this step. Three ways it goes wrong, all of them looking like something else:

- **Both mocks given the same `ROLE`.** Two emitters and no sink, or two sinks and no emitter. With `v2x_mock` on both, `15.4.11.4`'s `sink.log` never exists; with `ivi_mock` on both, `2.4.11.2` finds `r2_ingest` at 0 and reads like a routing fault.
- **`ROLE` misspelled on either.** §4.3's fourth row: the container exits at start and the **restart count climbs**. `v2x_mock` and `ivi_mock` are exact strings, underscore not hyphen.
- **`m1-ada-ecu:latest` typed onto a mock node, or `m1-ada-bench:latest` onto `.12`.** The Room comes up `Running` and proves nothing about the node under test.

**Type the `ROLE` and the `image` as a pair, node by node, and confirm which node is selected in the Inspector before each edit.** The read-back at `5.4.10.6` is what proves it, not the Inspector's truncated fields.

**Scope:** the table above plus §4.3's four-row *"values that decide whether anything works at all"* table — `TARGET_HOST`, `TARGET_PORT` = `V2X_LISTEN_PORT`, `IVI_ECU_HOST`/`IVI_ECU_PORT` = the sink's `LISTEN_PORT`, and `ROLE`. **Every value here is typed by hand, and the `.11` node arrives carrying the baseline's V2X ECU config** — that whole config is replaced with the emitter's, not amended. §4.3: the API has no update route, so every field is an Inspector edit. Click the node, edit, click empty canvas to commit.

**The ADA node's `command` and `capabilities`, and why neither is a judgement call:**

- `command: ["./entrypoint.sh"]` — relative to the image workdir `/app`; `/entrypoint.sh` does not exist and the container dies at start (§4.3's closing note).
- `capabilities: ["NET_RAW"]` — **unconditional on this node** ([node-ada-ecu.md § Blueprint node config](../requirements/car-sky-guide/node-ada-ecu.md#blueprint-node-config): *"always part of this node's config, never conditional"*). Type it whether or not this run wants a capture. Omitting it does not fail the deploy — it silently degrades `capture.sh` to a packet counter, which is the worst kind of miss, and it is what `15.4.6.5` needs.
- **The sink node needs `NET_RAW` too**, or its `[CAP]` lines never appear and `15.4.11.4`'s fourth criterion cannot be met by any route in the document.

**Acceptance:** every field typed, then proven by `5.4.10.6`'s read-back rather than by the Inspector's truncated fields. Record in the run doc which values were typed, **and the `image` + `ROLE` pair for each of the three container nodes explicitly** — that pairing is what a later reader needs to tell the two bench nodes apart.

**Dependencies:** after `6.4.10.4`. Sequential — `5.4.10.6` reads back what this types, and no deploy starts before that diff is clean. **Commit:** `[5.4.10.5] docs: record the isolated-Room node configuration`

### [ ] `5.4.10.6` — Read the stored config back and diff it against the definition *(car-sky)*

**Objective:** [§4.4](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#44-read-the-stored-config-back) — *"one call catches every row of the two tables above"*. **This is the gate in front of the Room slot**: hand transcription is where drift enters, and a mistyped octet found here costs a re-edit rather than a deploy cycle.

**Scope:** `GET /api/v1/blueprints/{id}`, then a field-for-field diff against `requirements/car-sky-guide/blueprint-ada-isolated.json`, ignoring only platform-assigned `id`s and node positions. Every one of these is a diff line, not a judgement call: each node's `nodeType` and whole `config`; the bench's `TARGET_HOST`/`TARGET_PORT`; the ADA node's `V2X_LISTEN_PORT`, `IVI_ECU_HOST`/`IVI_ECU_PORT` and every threshold; the sink's `LISTEN_PORT`; the bridge's `bridgeMode` and `subnet`, without which the `10.99.0.x` addresses have no network.

**Four assertions this diff must make explicitly**, because they are what `5.4.10.5` most likely got wrong and what a field-by-field diff would otherwise pass over: `.11` and `.13` both carry `image` = `…/m1-ada-bench:latest`; their `ROLE` values are `v2x_mock` and `ivi_mock` respectively and **are not equal to each other**; `.12` carries `image` = `…/m1-ada-ecu:latest` with `command: ["./entrypoint.sh"]` and `capabilities: ["NET_RAW"]` present; and `.13` also carries `NET_RAW`. Report any of the four as a mismatch by name.

**Acceptance, verbatim from [§4.4](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#44-read-the-stored-config-back):** four nodes; **three `ETHERNET`/`OUTPUT` pins at `10.99.0.11`, `.12`, `.13` plus the bridge's single `INPUT` pin and three edges**; and each container node's `config` carrying the image, command, capabilities and env exactly as typed. A clean diff recorded in the run doc — or every mismatching field named and handed back to `5.4.10.5` for a canvas fix, then re-run. **A deploy does not start on a dirty diff.**

**Dependencies:** after `5.4.10.5`. **Commit:** `[5.4.10.6] docs: record the isolated blueprint read-back diff`

### [ ] `5.4.10.7` — Deploy the blueprint *(Human, Nydus UI)*

**Objective:** [§4.5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#45-deploy)'s three clicks — blueprint Inspector → **New Deployment** → pick an **existing Device** → **Deploy**.

**Scope:** §4.5's numbered steps. §7 keeps this human because picking the Device is the user's call and **consumes one of two Room slots** (§ Open items item 7). `+ Create new device` is unnecessary and eats into that budget. Deploy the **original** blueprint, never the `<name>-deploy` snapshot.

**Readiness is this check and nothing else is coming** — [m1-run-timing-and-event-triggering.md §4.2](../requirements/m1-run-timing-and-event-triggering.md) rules out a node-to-node startup handshake: the platform offers no `dependsOn`, no readiness probe and no "deployment started" event into a container, and the R5/R6 topology has no reverse path to build acks on. The human's Deployment-Viewer check **is** the barrier, paired with the bench's `START_DELAY_S`. Do not plan or assume a barrier message; if a node misses early traffic, the remedy is the delay.

**Acceptance:** the deployment exists and its `roomId` is recorded in the run doc; the phase evidence itself is `5.4.10.8`'s.

**Dependencies:** after `5.4.10.6` (clean diff) and a free Room slot. **Commit:** `[5.4.10.7] docs: record the isolated Room deployment`

### [ ] `5.4.10.8` — Poll every node to `Running` and resolve every `nodeKey` *(car-sky)*

**Objective:** the AI row of [§4.5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#45-deploy) — turn the human's deploy into recorded evidence, and produce the three keys every log call in group 4.11 needs.

**Scope:** poll `GET /api/v1/deployments/{roomId}/nodes` until every node reads `Running` with restart count 0, recording each entry's `name` — the `nodeKey`. §4.5's two timing facts are binding on how this is judged: **the ADA node is the slowest to become useful** (largest image, and the detector loads its model before the first detection), so give it a minute past `Running` before anything reads its log; and *stuck in `Provisioning`* means the image could not be pulled — re-check `5.4.10.2` and the node's `image` field, diagnosing per [carsky-room-diagnostics](../.claude/skills/carsky-room-diagnostics/SKILL.md), never by redeploying blind.

**Acceptance:** 4/4 nodes `Running` with restart count 0, and all three container `nodeKey` values recorded in the run doc. This is the precondition every [§8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance) proof rests on, and it replaces reading node badges off the Deployment Viewer by eye — the Phase 1 finding that its summary header is unreliable stands.

**Dependencies:** after `5.4.10.7`. **Commit:** `[5.4.10.8] docs: record the isolated Room reaching Running`

---

## Task Group 4.11 — Isolated ADA test: the three checks, the negative case, teardown (serves R2, R13, R14, R15, R18)

> [Walkthrough §5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#5-run-the-checks) through [§5.7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#57-tear-down). **Every acceptance below is [§8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance)'s, quoted, not invented** — the planner may not write its own criteria for a walkthrough-governed check.
>
> **The three proofs do not substitute for one another:** the first two prove what happened *inside* the node and the third proves it *left* the node. A run with checks 1 and 2 green and check 3 silent is a routing failure, not a partial pass.
>
> **The event names are the node's design, not an observed fact yet** — [§8.1 item 4](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these). `r2_ingest`, `own_sensor_ingest`, `track_transition`, `parse_reject`, `assessment`, `risk_transition` and `r4_tx` are literal strings from [HLD D8](../ADA_ECU/doc/ada-ecu-design-decisions.md#d8--r18-the-ada-half-of-the-evidence-stream); a check that finds none of them is more likely reading a node that emits different names than a node that did nothing. Compare against the log before concluding a failure.

### [ ] `18.4.11.1` — Save the three node logs and the run's threshold values *(car-sky)*

**Objective:** the AI row *"Save the three node logs"* of [§5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#5-run-the-checks) — one fetch per node into a file, so all three checks read **one window** instead of three.

**Scope:** §5's three `curl` redirections into `ada.log`, `bench.log` and `sink.log`. Two of its rules are load-bearing and are why this is its own subtask rather than a step inside each check:

- **`container=user` is mandatory** — omitting it returns 500 listing the two container names, the [§6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#6-troubleshooting) row for it.
- **Let the Room run at least 90 seconds after the ADA node reaches `Running`** before reading anything — the emitter waits `START_DELAY_S`, the detector loads its model, and `CAPTURE_ROTATE_S` is 60 s, so a shorter window yields no `[PCAP-BEGIN]` block for `15.4.6.5`. Re-fetching per grep gives three checks three different windows, which §5 says plainly not to do.

Also record, per [§8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance)'s closing instruction: `PROFILE`, `START_DISTANCE_M`, `MIN_DISTANCE_M`, `CLOSING_RATE_MPS`, `GATE_ENTER_M`, `GATE_EXIT_M`, `RISK_NEAR_M`, `RISK_CRITICAL_M` as deployed. **A pass at unknown thresholds proves nothing** — that sentence is §8's, and it is why this subtask exists ahead of the checks rather than beside them.

**These three logs are the evidence base for more than this group:** `15.4.6.5` extracts its pcap from `ada.log`, and Phase 3 `5.3.6.2` measures the deployed detector rate from it. Archive them rather than reading and discarding.

**Acceptance:** three non-empty log files archived under `plans/doc/`, the eight threshold values recorded beside them in the run doc, and the window (Room start, `Running` time, fetch time) stated.

**Dependencies:** after `5.4.10.8` + 90 s. **Commit:** `[18.4.11.1] docs: record the isolated-Room node logs and their threshold values`

### [ ] `2.4.11.2` — Check 1: the relayed message is received and raises its event *(car-sky)*

**Objective:** [§5.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#51-check-1--the-relayed-message-is-received-and-raises-its-event)'s claim under test — the ADA node receives the bench's datagram and raises the corresponding event.

**Scope:** §5.1's five greps over `ada.log` and `bench.log`, and its two expected-line blocks. Nothing is added to them.

**Acceptance — [§8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance) output 1, verbatim:** `r2_ingest` count ≥ 1 and ≥ 90% of the bench's `[TX]` count · the first payload's `stationId` and `object.objectId` match the bench's configured values · `parse_reject` count is 0 · one `track_transition` to `tentative` and a later one to `tracked`, both `"source":"v2x_relayed"`.

**On failure, §5.1 already says which side is wrong** — `r2_ingest` at 0 while `bench.log` shows `[TX]` is a routing fault (`TARGET_HOST`/`TARGET_PORT`/`V2X_LISTEN_PORT`, back to `5.4.10.5`); a non-zero `parse_reject` means **the emitter is wrong, not the node**, and is fixed against `ADA_ECU/contracts/r2-v2x-object.schema.json` at `2.4.9.2`. Report the verdict and the side; do not improvise a fix on the platform.

**Dependencies:** after `18.4.11.1`. Parallel with `13.4.11.3` and `15.4.11.4` — three reads of saved files, no shared state. **Commit:** `[2.4.11.2] docs: record check 1 — relayed message received and event raised`

### [ ] `13.4.11.3` — Check 2: both vehicles are in the track store, and the run reconstructs offline *(car-sky)*

**Objective:** [§5.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#52-check-2--both-vehicles-are-in-the-track-store)'s claim under test — the store holds a track for vehicle **B**, produced by ego's own detector from the baked-in clip, and a track for vehicle **C**, present only through the relayed path. **This is the check that carries the milestone's whole point**, and it also closes the R18 offline-reconstruction box and the demo box.

**Scope — the greps, then the project's own instruments over the same saved log:**

- §5.2's five greps and its three expected-line blocks, over `ada.log`. The single strongest line is the emitted `r4_tx`, **because it proves both tracks existed at the same instant** rather than at two different times — §5.2's own words.
- `python ADA_ECU/tools/check_evt_log.py --admission --fusion --both-tracks --r4-schema ADA_ECU/contracts/r4-ada-ivi.schema.json ada.log` → **exit 0**. This is the scripted form of the same claim and it additionally asserts the edge-triggered chain (`18.4.3.3`).
- `python ADA_ECU/tools/event_report.py ada.log` — record the rendered collision-risk event list. **This is the §1 demo-table artifact and the "event list reconstructs a full run offline" acceptance**, demonstrated on a real node's log with no live process.
- Record verbatim the two `[EVT]` excerpts carrying B's and C's full TrackedObjects.

**Acceptance — [§8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance) output 2, verbatim:** ≥ 1 `track_transition` to `tracked` with `"source":"own_sensor"` · ≥ 1 with `"source":"v2x_relayed"` · ≥ 1 `r4_tx` payload with `object.source` = `v2x_relayed` and numeric `geometry.vehicleB` · **zero** own-sensor entries claiming a relayed source or a `v2x:` id, and **zero** relayed entries claiming an `own:` id. Plus: both tool runs recorded, `check_evt_log.py` exit 0, and the event report non-empty.

**Those three zeros are the zero-C guarantee in text** (§5.2) — nothing ego's detector produced can claim to be C, and nothing relayed can claim to have been seen directly. They are the same claim `ADA_ECU/tools/check_zero_c.py` (`12.3.5.1`) makes structurally, evidenced here on a deployed node.

**Two failures with different owners, per §5.2:** no `own_sensor_ingest` at all is a detector problem — read for `detector_spawn` first, then `VIDEO_CLIP_PATH` / `MODEL_PATH` / `DETECTOR_ENABLED`. `r4_tx` absent though both tracks reached `tracked` is **a tuning problem, not a defect** — the risk level never changed, and the route is `14.4.11.6`, not a code change. [§8.1 item 10](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these) warns this may be the expected outcome rather than the exception.

**Dependencies:** after `18.4.11.1` + `18.4.3.2` + `18.4.3.3`. Parallel with `2.4.11.2` and `15.4.11.4`. **Commit:** `[13.4.11.3] docs: record check 2 — both vehicles in the track store and the offline event list`

### [ ] `15.4.11.4` — Check 3: the warning reaches the IVI stand-in carrying both vehicles *(car-sky)*

**Objective:** [§5.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#53-check-3--the-warning-reaches-the-ivi-stand-in-carrying-both-vehicles)'s claim under test — the ADA node puts a warning datagram on the wire, addressed to the IVI node, carrying both vehicles.

**Scope:** §5.3's four greps over `sink.log`, and its expected-output block. `cSource=v2x_relayed` on every warning is what §5.3 calls *the point of the whole Room*.

**Acceptance — [§8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance) output 3, verbatim:** ≥ 1 `[RX]` with `type=warning` from `10.99.0.12` · ≥ 1 `[CHECK] both_vehicles=yes c_source_relayed=yes` · last `[SUMMARY]` with `rejected=0` · ≥ 1 `[CAP] IP 10.99.0.12.<port> > 10.99.0.13.47300: UDP` · the sink's `received` count equals the ADA log's `r4_tx` count.

**The sink's `[CAP]` text lines are this check's traffic evidence; the `.pcap` is `15.4.6.5`'s and does not gate this subtask** ([§5.4](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#54-traffic-evidence-and-wireshark-scope)). **`[CAP]` needs `NET_RAW` on the sink node** ([§8.1 item 12](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these)); without it `capture.sh` falls back to packet counters, the `[RX]` line still stands and the `[CAP]` criterion cannot be met by any route in the document — report that state rather than substituting other evidence for it.

**Two failures, per §5.3:** `r4_tx` present with the sink silent is a routing fault (`IVI_ECU_HOST`/`IVI_ECU_PORT` against `LISTEN_PORT`, back to `5.4.10.5`). `both_vehicles=no` needs the `seq` numbers read before it is called a defect — **`no` on the first datagram and `yes` afterwards is expected** because a null C is legitimate before C is first tracked; `no` throughout is the defect.

**Dependencies:** after `18.4.11.1`. Parallel with `2.4.11.2` and `13.4.11.3`. **Commit:** `[15.4.11.4] docs: record check 3 — the warning on the wire carrying both vehicles`

### [ ] `13.4.11.5` — Run the `PROFILE=out_of_range` negative case *(Human, Nydus UI)*

**Objective:** [§8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance)'s first *further observation* — prove the admission gate **rejects** on distance, so admission in the main run was earned rather than automatic.

**Scope:** §5.1's *"Run the negative case too"* paragraph — set `PROFILE=out_of_range` on the V2X bench node and redeploy. §7 marks it human: it is a node-config edit plus a fresh deployment, and both are Inspector/canvas work. The re-run of the checks over the new logs is [[car-sky]]'s, under `18.4.11.1` and `2.4.11.2`'s briefs.

**Acceptance, verbatim from [§8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance):** with `PROFILE=out_of_range`, `r2_ingest` still counts up and **no** relayed `track_transition` appears. §5.1 states the failure meaning exactly: a track admitted under this profile means the gate is not reading the message's distance.

**Never reach for the gate constants to make a warning appear** — [§5.5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#55-retune-when-no-warning-is-emitted)'s closing rule: moving `GATE_ENTER_M`/`GATE_EXIT_M` to change the alarm makes admission and alarm indistinguishable and **invalidates this very subtask**.

**Dependencies:** after `2.4.11.2` (the positive case must have passed first, or the negative proves nothing). **Commit:** `[13.4.11.5] docs: record the out-of-range negative case`

### [ ] `14.4.11.6` — Retune when no warning is emitted *(Human, Nydus UI — contingency, run only if `13.4.11.3` finds no `r4_tx`)*

**Objective:** [§5.5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#55-retune-when-no-warning-is-emitted) — get the assessed risk level to **change**, which is what emits a warning, using node config and a redeploy. **No rebuild.**

**Scope:** §5.5's four numbered steps and its three-row lever table (`MIN_DISTANCE_M` down on the bench, `RISK_NEAR_M` up or `RISK_CRITICAL_M` up on the ADA node). §5.5's step 2 carries the fact that makes this likely rather than exotic: **the composed range is ego-to-B plus B-to-C, so it is always larger than the distance the bench emits** — and the risk defaults are set against a scenario rather than against this clip's actual ego-to-B range, which is [§8.1 item 10](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these)'s warning that §5.5 may be on the critical path rather than a fallback.

- **Pick one lever and change only it** (§5.5 step 3), then re-run `13.4.11.3` and `15.4.11.4`.
- **Never retune the admission gate** — §5.5's closing rule, restated at `13.4.11.5`.
- **Record every value changed.** §5.5: *a run whose thresholds are unknown proves nothing*; the record goes beside `18.4.11.1`'s threshold list, superseding it for the re-run.

**Acceptance:** §8 outputs 2 and 3 met on the re-run, with the changed lever and its before/after values recorded. If no lever produces a transition, that is a finding about the risk defaults (§ Open items item 2) — report it rather than widening the search.

**Dependencies:** after `13.4.11.3`, only if it found no `r4_tx`. **Commit:** `[14.4.11.6] docs: record the risk-threshold retune and its re-run evidence`

### [ ] `5.4.11.7` — Tear the Room down *(Human, Nydus UI)*

**Objective:** [§5.7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#57-tear-down) — **Delete Deployment**, releasing one of the two Room slots so Phase 5's system test can take it.

**Scope:** §5.7, and its one irreversible condition: **save all three log files before deleting — the log route returns nothing once the Room is gone.** `18.4.11.1` is what satisfies that, so this subtask does not run until its files are archived and every consumer of them has run: `2.4.11.2`, `13.4.11.3`, `15.4.11.4`, `15.4.6.5`, and Phase 3's `5.3.6.2`.

**Acceptance:** the deployment deleted and the release recorded in the run doc; `plans/doc/` holds the three logs, the extracted pcap and every check's verdict.

**Dependencies:** after `2.4.11.2` + `13.4.11.3` + `15.4.11.4` + `13.4.11.5` + `15.4.6.5` + Phase 3 `5.3.6.2`, and after `14.4.11.6` if it ran. **Commit:** `[5.4.11.7] docs: record the isolated Room teardown`

---

## Task Group 4.12 — Alternative test: real bench and V2X ECU upstream (serves R13, R15, R18)

> **One subtask, and it is a person's.** This is the isolated Room with the **upstream** mock removed: the bench Scenario Player and the V2X ECU are the real nodes, so relayed C originates in a real scenario and arrives as a real decoded CPM. Only the downstream sink stays mocked, because the real one is the IVI app and it does not exist yet.
>
> **The ADA node is not reconfigured** — [§5.6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#56-the-full-blueprint-route) is explicit that its `image`, `command`, `capabilities`, address, port and env are the same in every composition, and that is what makes this rung cheap: the only edits are which neighbours sit beside it. A value that differs here from the isolated Room is a transcription error, not a design difference.
>
> **This group creates no new checks.** The three checks, the negative case and the log-saving are groups 4.10–4.11's subtasks, **re-run against this Room under their existing briefs** — no new IDs, no second copy of the criteria.

### [ ] `13.4.12.1` — Deploy the real-upstream blueprint and re-run the checks *(Human, Nydus UI)*

**Objective:** close the *"with bench scenarios live"* wording of this phase's acceptance boxes — C tracked as `v2x_relayed` through the **real** relay, at least one R4 warning carrying the composed geometry, and a scenario swap that visibly changes what the ADA node tracks.

**The composition** — the [4-node blueprint](../requirements/car-sky-guide/carsky-4-node-blueprint.md) with its Skycraft node replaced by a container sink:

| Node | Address | Node type | `image` | Note |
|---|---|---|---|---|
| Ethernet Bridge | `10.99.0.1` | Ethernet Bridge | — | `bridgeMode: "linux"`, `subnet: "10.99.0.0/24"` |
| Bench — Scenario Player | `10.99.0.10` | Container | `…/scenario-player:latest` | real, per [node-scenario-player.md](../requirements/car-sky-guide/node-scenario-player.md) |
| V2X ECU | `10.99.0.11` | Container | `…/m1-v2x-ecu:latest` | real, per [node-v2x-ecu.md](../requirements/car-sky-guide/node-v2x-ecu.md) |
| **ADA ECU** | `10.99.0.12` | Container | `…/m1-ada-ecu:latest` | **unchanged from the isolated Room, field for field** |
| IVI Sink (mock) | `10.99.0.13` | **Container**, not Skycraft | `…/m1-ada-bench:latest`, `ROLE=ivi_mock` | stands in for the IVI app so a `[RX]`/`[CHECK]`/`[CAP]` log exists |

**Why the sink stays a mock, and what it costs.** The Android node runs no container, so on the real 5-node blueprint there is **no sink log at all** and the only downstream evidence is the ADA node's own capture. Keeping `10.99.0.13` a container preserves `15.4.11.4`'s entire criterion set through this rung. The cost is that nothing renders — which is R16/R17's box, not one of this phase's.

**Scope — what the person does, in order:**

- **Compose the blueprint by cloning**, never by importing a JSON file. An imported or script-built blueprint arrives without its `ethernet` pins and is rejected at deploy ([§4.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#41-create-the-blueprint)). Cloning the isolated blueprint keeps three of the four pins and the ADA node's whole config; what remains is to add the bench node at `.10`, repoint `.11` from the mock to the real V2X ECU image, and wire the new pin. Work on the blueprint, never on a `<name>-deploy` snapshot.
- **Bench and V2X ECU config** comes from their own node guides, unchanged — this group re-derives neither. The bench's `SCENARIO_CONFIG` selects the scenario, and `Scenario_Player/scenarios/default.yaml` is the approach case.
- **Confirm the ADA node was not touched** by the clone: `image`, `command: ["./entrypoint.sh"]`, `capabilities: ["NET_RAW"]`, `V2X_LISTEN_PORT=47200`, `IVI_ECU_HOST=10.99.0.13`, `IVI_ECU_PORT=47300` and every threshold identical to what `5.4.10.6` diffed. The read-back call is [[car-sky]]'s under `5.4.10.6`'s brief.
- **Deploy**, then re-run — under their existing briefs, no new IDs — `18.4.11.1` (save `ada.log`, `bench.log`, `sink.log` and the deployed threshold values), `2.4.11.2` (check 1), `13.4.11.3` (check 2 plus the two evidence tools), `15.4.11.4` (check 3) and `15.4.6.5` (the pcap, if not already taken off the isolated Room).
- **Then swap the scenario and re-run check 1.** Set `SCENARIO_CONFIG=/app/scenarios/c-out-of-range.yaml` on the bench node and redeploy — config only, no rebuild. **This is the negative case's real-relay form and a different lever from `13.4.11.5`'s**: that one sets `PROFILE=out_of_range` on a mock emitter, this one changes a committed bench scenario file, and passing both is what shows the gate reads the *message's* distance rather than one emitter's quirk. It is also R11's own acceptance — different scenario configurations produce observably different message streams — observed at the ADA node.

**Scope note — the two things this rung newly exercises, and who owns a failure in each.** The R1 CPM encode/decode path and the real R2 producer shape are **Phase 1's**, not this node's. A `parse_reject` storm here after a clean isolated run means the V2X ECU's R2 output has drifted from `ADA_ECU/contracts/r2-v2x-object.schema.json`, and the fix is on that side. Report the side; do not retune the ADA node to accept a drifted message.

**Acceptance:** §8 outputs 1, 2 and 3 met on this Room exactly as worded for the isolated one, plus: the first `r2_ingest` payload's `stationId` and `object.objectId` match the **bench scenario's** configured values rather than a mock's; and with `c-out-of-range.yaml` deployed, `r2_ingest` still counts up while **no** relayed `track_transition` appears. Recorded in `plans/doc/phase4-ada-fusion-run.md`, created by this subtask and kept separate from the isolated Room's record so no reader has to work out which Room a log came from. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `5.4.11.7` (this Room takes the slot the isolated one releases) and after Phase 1's bench and V2X ECU images are pushed and their nodes known-good. **Commit:** `[13.4.12.1] docs: record the real-upstream ADA run and its scenario swap`

---

## Execution order & parallelism

```
Lane A (fusion code and tooling - agent only, no platform, no person)
  tooling    18.4.3.1                                    (anytime)
  capture    6.4.4.1 (after phase-2 5.2.7.1) ∥ 6.4.4.2   (anytime)
  rules      15.4.1.1 ──► 14.4.1.2 ──► 14.4.1.3
  output     15.4.2.1 (after 15.4.1.1 + 14.4.1.2) ──► 15.4.2.2 ──► 15.4.2.3 (after 14.4.1.3)
                                                        └──► 15.4.2.4 (OPTIONAL)
  evidence   18.4.3.2 (after 14.4.1.3 + 15.4.2.2) ∥ 18.4.3.3 (after 15.4.2.3)
  CI         15.4.5.1 (after 15.4.2.3 + 18.4.3.1 + 18.4.3.2 + 18.4.3.3)

Lane B (isolated ADA test - starts day one, never blocks lane A)
  4.9   5.4.9.1 (day one, no dependency)
        2.4.9.2 ∥ 4.4.9.3 (day one) ──► 5.4.9.4 ──► 5.4.9.5 ──► 2.4.9.7
        5.4.9.6 (after phase-2 5.2.8.1 + 6.4.4.1 + phase-3 5.3.6.1)
  4.10  5.4.10.1 (HUMAN) ──► 5.4.10.2 ──► 5.4.10.3 ──► 6.4.10.4 (HUMAN) ──► 5.4.10.5 (HUMAN)
        ──► 5.4.10.6 ──► 5.4.10.7 (HUMAN) ──► 5.4.10.8
  4.11  18.4.11.1 ──► { 2.4.11.2 ∥ 13.4.11.3 ∥ 15.4.11.4 ∥ 15.4.6.5 ∥ phase-3 5.3.6.2 }
        13.4.11.5 (HUMAN, after 2.4.11.2)   14.4.11.6 (HUMAN, only if 13.4.11.3 found no r4_tx)
        5.4.11.7 (HUMAN - releases the Room slot)
  4.12  13.4.12.1 (HUMAN, after 5.4.11.7 + phase-1's bench and V2X ECU nodes known-good)
        └─ re-runs 18.4.11.1 -> { 2.4.11.2 | 13.4.11.3 | 15.4.11.4 | 15.4.6.5 } against the new Room,
           under their existing briefs - no new IDs
```

**Recommended runtime order (single tree):** 18.4.3.1 → 6.4.4.1 → 6.4.4.2 → 15.4.1.1 → 14.4.1.2 → 14.4.1.3 → 15.4.2.1 → 15.4.2.2 → 15.4.2.3 → 18.4.3.2 → 18.4.3.3 → 15.4.5.1 → 15.4.2.4 *(if time)* → **group 4.9 → 4.10 → 4.11 → 4.12**.

**Group 4.9 runs beside lane A from day one.** `5.4.9.1` (a JSON file), `2.4.9.2` and `4.4.9.3` (two standalone Python scripts) touch no file lane A touches, and the bench is an unproven dependency of the acceptance run — proving it early is the point.

**Groups 4.10 and 4.11 are genuinely sequential.** The walkthrough's ordering is binding: images before the nodes that reference them, nodes before pins, pins before `validate` passes, the read-back before the Room slot, the Room before any log exists, and the logs saved before the teardown deletes them.

**Group 4.10's entry condition is not "the code compiles"** — it is [§1.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#13-deliverable-prerequisites)'s deliverable rows present *in the image*: `ada_ecu`, `entrypoint.sh`, `capture.sh`, the detector, the model and the clip. Those are Phase 2, 3 and 4 work planned above; this group consumes them and plans none of them. **In particular, Phase 3 must have landed** — without a real detector in the image there is no `own_sensor` B track and `13.4.11.3` cannot pass.

**Shared-file sequencing with Phase 3:** `ADA_ECU/CMakeLists.txt` (this phase adds targets, Phase 3 adds none) and `ADA_ECU/Dockerfile` (Phase 3 adds three `COPY` lines, this phase adds one for `capture.sh`). Sequence those two edits, not the phases. Inside this phase, `.github/workflows/phase4-ci.yml` is written by `15.4.5.1`, `5.4.9.5` and `2.4.9.7` — sequence those three.

## Acceptance traceability

| Milestone Phase 4 box | Closed by | Where |
|---|---|---|
| C tracked with `source = v2x_relayed` only, full R13 lifecycle | Phase 2 `2.2.3.1` + `13.2.4.3` · CI `15.4.5.1` · deployed `13.4.11.3` + `13.4.11.5` · **real relay `13.4.12.1`** | node level at the isolated Room; *"bench scenarios live"* in full at group 4.12 |
| NLOS plugin registers through the CRA interface; abstraction + DB schema are the artifacts (R14) | `14.4.1.2` (one file + one line) over Phase 2 `14.2.5.1`–`14.2.5.4` | off-platform, fully |
| ≥ 1 R4 warning per run with risk state and composed geometry (R15) | `15.4.1.1` · `15.4.2.1` · `15.4.2.2` · `15.4.2.3` · CI `15.4.5.1` · deployed `15.4.11.4` | isolated Room |
| The event list reconstructs a full run offline (R18) | `18.4.3.2` (tool) · `18.4.3.3` (checker) · `13.4.11.3` (run over a real node's log) | isolated Room |
| **Demo:** ADA logs — collision-risk event list | `18.4.3.2` · `13.4.11.3` | isolated Room |
| **Output check:** B's and C's TrackedObjects reach the IVI path, by log **and** pcap | **path A** `13.4.11.3` (both full R3 objects + `r4_tx`) · **path B** `15.4.6.5` (C's full R3 object + B's position on the wire) | isolated Room; caveat in § Phase 4 output acceptance |

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

## Open items & flags (no Phase 4 subtask may silently close them)

| # | Item | Owner |
|---|---|---|
| 1 | **The frozen R4 carries C's full TrackedObject and B's *position*, not B's TrackedObject.** This plan accepts that and proves B's full object from the `[EVT]` log — reasoning in § Phase 4 output acceptance. **Adding an optional `trackedObjects` array is out of scope and no subtask implements it**: it changes a frozen contract and forces a re-freeze across the ADA binding, the ADA emitter, the golden samples, both synced copy sets, the IVI Kotlin binding and both languages' round-trip tests, days from the deadline. Recorded so the decision is visible, not so the work is queued | **user** (if they want it, it becomes new work, planned then) |
| 2 | **`(proposal)` risk defaults proceed as proposed** — `RISK_NEAR_M=25`, `RISK_CRITICAL_M=15`, `RISK_TTC_WARN_S=6`, `RISK_TTC_CRITICAL_S=3`, `RISK_DWELL_MS=300`, `ASSESS_LOG_EVERY_MS=1000`. Chosen so an approach produces a visible `low → medium → high` progression and an out-of-range profile never leaves `low`. Externalized, so ratification is a node-config edit. **They have not been checked against the real clip's ego-to-B range**, which is why `14.4.11.6` exists | user |
| 3 | **Planner-designated test/tool paths beyond the HLD's list**: `tests/output/test_ivi_sender.cpp`; `tests/fusion/test_scene_composer.cpp`; the `--fusion` / `--both-tracks` / `--r4-schema` modes of `tools/check_evt_log.py`. HLD-consistent additions, not new design | [[project-architecture]] (ack) |
| 4 | **This phase closes its own boxes; only the rendering half waits on Phase 5.** *"With bench scenarios live"* means the real Scenario Player and V2X ECU upstream, and group 4.12 supplies exactly that with the sink still mocked — so no Phase 4 box depends on the IVI app. What does wait on Phase 5 is the **God view drawing ghost C**, which is R16/R17 and R19, recorded in [phase5_minh_tasks.md group 5.10](phase5_minh_tasks.md#task-group-510--system-verification-test-serves-r4-r5-r6-r16-r17-r18-r19) and re-recorded in Phase 6's continuous run. **A subtask may not claim a box at a rung that cannot close it** — the isolated Room closes them at node level, group 4.12 at their full wording | [[project-planner]] (scheduling) |
| 5 | **`15.4.2.4` (periodic awareness state) is optional and may be dropped** without affecting any acceptance box — R15 words it optional and the warning event alone renders the M1 demo. It must stay off by default (`STATE_RATE_HZ=0`), asserted in test | Phase 4, if time permits |
| 6 | **R20/R21 are not planned and no subtask implements them.** [m1-run-timing-and-event-triggering.md §8(1)](../requirements/m1-run-timing-and-event-triggering.md) schedules them behind every Phase 3 and Phase 4 acceptance box, they gate no box, and **the user has not accepted them**. That covers: bench deadline scheduling and `mono_ms` on the `[TX]` line; detector real-time pacing and its `DETECTOR_REALTIME_PACING` / `DETECTOR_CLIP_FPS` / `DETECTOR_START_DELAY_S` keys; bench `start_delay_s` / `reference_time_epoch`; the `[EVT] ready` readiness line; and the K1–K5 run-alignment checker. If the user accepts R20/R21 they become new work with new IDs, planned then | **user** (accept/reject per §8(1)) |
| 7 | **Room quota — two concurrent deployments across the whole account, contended four ways.** This phase's isolated Room (groups 4.10–4.11) and its real-upstream Room (group 4.12), Phase 5's isolated IVI Room (group 5.9) and Phase 5's system test (group 5.10). Phase 3's `5.3.6.2` books none — it reads this phase's log. **The two Phase 4 Rooms are sequential, not concurrent**: `13.4.12.1` takes the slot `5.4.11.7` releases, so this phase holds one slot at a time. Tear each down — `5.4.11.7` here, `17.5.9.16` and `19.5.10.8` in Phase 5 | [[project-planner]] (scheduling) |
| 8 | **[§8.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#81-confirm-before-relying-on-these) items that authoring cannot retire, and where each first bites.** Item 1 (the whole route is unexercised) — the lane as a whole. Item 2 (the §1.3 ADA deliverables) — group 4.10's entry condition; Phases 2–4 work planned above. Item 4 (the `[EVT]` names are design, not observation) — `2.4.11.2`, `13.4.11.3`. Item 6 (arm64 wheels for the detector) — Phase 3 `12.3.1.1`, and `5.4.9.6`'s 360-minute timeout check. Item 9 (the clone-and-reduce route: `/clone` returning intact pins, deleting a node leaving the survivors' pins alone, a hand-drawn pin on a new node validating) — `5.4.10.3` and `6.4.10.4`, whose `validate` pass is the first proof of it. Item 10 (the defaults may produce no risk transition) — `13.4.11.3`, contingency `14.4.11.6`. Item 11 (detector frame rate on the Room's CPU) — Phase 3 `5.3.6.2`. Item 12 (whether the platform **honours** a requested `NET_RAW`) — `15.4.11.4`'s `[CAP]` criterion and `15.4.6.5`'s pcap. **None is filled in by guessing; each is reported from the subtask that hits it** | per subtask |

---

*Phase 4 = 10 task groups, 37 subtasks — 20 *agent* (1 optional), 9 *car-sky*, 8 *Human*. Nothing started. Retired IDs, never reused: `5.4.6.1`, `5.4.6.2`, `13.4.6.3`, `18.4.6.4`, `4.4.7.1`–`4.4.7.4`, `20.4.8.1`, `21.4.8.2`. Decomposed from [ada-ecu-hld.md](../ADA_ECU/doc/ada-ecu-hld.md) D4/D5/D7/D8/D9, [deploy-ada-ecu-walkthrough.md](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md) per [walkthrough-driven-delivery.md](../.claude/rules/walkthrough-driven-delivery.md) stage 2, and [milestone1.md § Phase 4](milestone1.md#phase-4--obscured-object-fusion-relayed-c--risk--warning-r13r15--runs--with-phase-3).*
