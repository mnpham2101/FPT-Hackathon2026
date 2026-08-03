# Milestone 1 — Cooperative Vehicle Awareness (A ← B ← C)

> Requirements, scope, and tech stacks live in the authoritative report [m1-cooperative-awareness.md](../requirements/m1-cooperative-awareness.md) (R1–R19); this plan references R-numbers instead of restating them, and on any conflict the report wins. Phase structure follows the [proposal-deck timeline](../presentation/assets/m1-phase-timeline.svg) (second authority).

## 1. Introduction

Milestone 1 demonstrates **cooperative (non-line-of-sight) awareness** over V2X: making a vehicle aware of a hazard it cannot see, by relaying another vehicle's perception (report §1).

Three vehicles drive in a collinear convoy — **A** follows **B** follows **C**. Vehicle A's view of C is **blocked by B**, so A's own camera can never detect C. Vehicle B sees C and **broadcasts that perception to A over V2X**, and A displays C and its relative position without ever seeing it. **M1 builds only A's vehicle (ego)** — B and C exist as bench-generated V2X messages (R11) and as content of the provided video; the bench Scenario Player is sanctioned test equipment, not a mock to eliminate.

![Convoy geometry: A cannot see C; B sees C and relays it to A over V2X](../requirements/m1_convoy_nlos_relay_geometry.png)

*Objective — B's perception of C reaches A over a V2X relay. A reconstructs C's position by composing its own measurement of B with B's reported measurement of C:* `d_AC ≈ d_AB + d_BC` *(valid for the near-collinear convoy; absolute/GPS composition is a later milestone).*

---

## 2. Scope & Assumptions

Report §1 plus its §4 decision record are the hard scope boundary; each assumption below traces there.

- **Ego-only build on CarSky.** One blueprint with four nodes — V2X ECU, ADA ECU, IVI ECU (provided AAOS), bench Scenario Player — over one Ethernet Bridge (R5, R6); the Cortex-M ECU is omitted.
- **Video is provided** (saved files) — no live camera bring-up. The detector sees **B, the visible occluder** — C is by definition never in ego's frame (R12).
- **Pretrained detector, no training**; CPU-only — no GPU requested from BTC (§4 decisions).
- **Video input spec is unconfirmed** — format / frame rate / data rate are to be studied and proposed to FPT-Mentor (report § Input constraints); Phase 2 carries that study.
- **Wire encoding is standard ASN.1 UPER** via the Vanetza ITS2 codec (R1); JSON is used only on the intra-ego links (R2, R4).
- **Messages are unsigned** — no signing/PKI stack in M1 (the R1 profile carries no security envelope).
- **No GNSS path** — sender pose values originate in bench-generated CPM contents (R11); the IVI renders relative geometry only, no map and no GNSS injection (§4 decisions).
- **Composition assumes a near-collinear, same-heading convoy** (§1 objective note).
- **Risk** is the R14 Collision Risk Assessment (CRA) abstraction with the M1 NLOS plugin on the fixed distance criterion (R13); speed-scaled risk and further hazard types are future plugins (report § Future developments).

---

## 3. Development Plan & Order of Implementation

The plan is **contract-first**: the contracts are the report's R1–R6, frozen in Phase 0 before dependent work, so every later phase is "swap mock data for real data" inside a shape that already works.

> Deviation from the deck, flagged: the deck timeline places the ADA↔IVI message definition inside Phase 2, but the display track (Phase 5) starts in parallel with Phase 2 and must build against a frozen R4 mock — so this plan freezes R4 in Phase 0 with the other contracts (contract-first principle; report authority over deck).

With the contracts frozen, three tracks run in parallel and converge at Phase 6 ([timeline](../presentation/assets/m1-phase-timeline.svg)):

- **Comms track — Phase 1:** V2X ECU + bench Scenario Player exchanging real R1 CPMs (mock perception contents until Phase 6).
- **ADA track — Phase 2 first** (skeleton + store + state machine), **then Phases 3 ∥ 4** side by side. They never call each other — they share only the R3 store: detection (R12) writes `own_sensor` entries through the JSONL subprocess boundary; fusion (R13/R14/R15) consumes the store and the live R2 feed.
- **Display track — Phase 5:** IVI HMI built against mock R4 warnings from the start.

### Order of implementation

> **Step 0 — Phase 0:** freeze R1–R6.
>
> **Step 1 — run three tracks in parallel:** Phase 1 (comms) · Phase 2 → 3 ∥ 4 (ADA) · Phase 5 (display).
>
> **Step 2 — converge:** Phase 6 replaces every mock with real data and records the R19 run.
>
> **Single-developer fallback:** run the phases sequentially 0 → 1 → 2 → 3 → 4 → 5 → 6. The parallel plan is the optimization for multiple people.

### Platform & portability

- Node/blueprint/Room mechanics, deployment steps, and the per-node table: report § Cloud development constraints and R5/R6 — not restated here.
- All team code is plain Linux processes; the V2X ECU touches the radio only through the R7 adapter seam, so the layers above it move to real modem hardware unchanged — that node's focus goal.

---

## 4. Global Definitions

The contracts are defined once, in the report: **R1** CPM profile · **R2** V2X→ADA object message · **R3** TrackedObject schema · **R4** ADA→IVI warning/state messages · **R5** node deployment · **R6** Ethernet-bridge network.

### Track admission gate (R13)

Gate constants are **externalized configuration, never literals**. R13 fixes the 30 m admission threshold; the exit hysteresis and the N/M counters come from its attached state-machine diagram — N and M are proposals to confirm.

| Constant | Value | Meaning |
|---|---|---|
| `gate_enter` | 30 m | admit C when its reported distance (R2 `object.distance`) is within this |
| `gate_exit` | 35 m | drop C only beyond this (hysteresis — no flicker at the boundary) |
| `confirm_hits` (N) | proposed 3 | consecutive in-range updates before admission |
| `miss_limit` (M) | proposed 5 | consecutive missed updates before expiry — **realized as wall-clock, see below** |

**`miss_limit` change of form, awaiting the user's re-ratification.** The [Phase 2–4 ADA HLD, decision D3](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md#d3--r13-admission-one-state-machine-both-sources) implements M as a wall-clock `TRACK_TIMEOUT_MS` (default 1000 ms = 5 periods at the slower of the two sources), not as a count. Reason: "its messages stop" is a time condition that a counter cannot express — nothing arrives to increment it — and the two sources run at independently configured cadences, so one count M would mean two different real timeouts. Intent is unchanged; the form is. Flagged here rather than silently overridden ([phase2_tasks.md § Open items item 1](phase2_tasks.md#open-items--flags-no-phase-2-subtask-may-silently-close-them)).

![Track admission state machine with proximity gate and hysteresis](../requirements/vehicleC_track_admission_state_machine.png)

*Own-sensor objects traverse `not_tracked → tentative → tracked`; relayed C is admitted on R2 distance within the gate, dropped on leaving it or when messages stop, and carries `source = v2x_relayed` only. ~~Ego's own Tx (R10) snapshots `own_sensor` tracks~~ — **R10 moved to the future plan**; the B-side relay trigger is simulated by the bench scenarios (R11), so nothing in M1 consumes an ego Tx snapshot.*

---

## 5. Phases

Per-phase demo methods follow the deck's "defined output for each phase" table. Contradiction fixed, not absorbed: that table labels the HMI row "6" and the integration row "5", contradicting the deck's own timeline and narrative (phases 1, 2, **5** start in parallel; all converge at **6**) — this plan follows the timeline: **Phase 5 = HMI, Phase 6 = convergence**, matching the previous plan's numbering.

### Phase 0 — Freeze the contracts (R1–R6)

**Objective.** Every cross-track contract versioned and committed before dependent work starts.

**Tasks.**
- R1 profile document: fields, units, encoding, and the V2X exchange call flow.
- R2 / R3 / R4 schema files with per-language bindings; golden vectors for the R1 codec seam.
- Blueprint topology decided: node set (R5) and pin/edge wiring matching the communication topology (R6).

**Acceptance Criteria.**
- [x] R1 profile document committed; golden-vector CPMs encode/decode through the Vanetza codec seam.
- [x] R2, R3, R4 schemas committed; round-trip tests pass in each consumer language (C++ / Python / Kotlin).
- [x] The R4 additive-version test is defined (a consumer parsing an unknown `warningType` degrades gracefully).
- [x] Blueprint topology documented (nodes, `ethernet` pins, edges to the bridge) and validated by the baseline connectivity smoke test.

The baseline blueprint deployed as `trial2_minh_netcheck` with all nodes `Running` and smoke-test criteria C1–C5 met ([run record](doc/phase0-smoke-test-run.md)); the non-blocking residuals O3 (MTU headroom) and O4 (AAOS listener) stay open in [phase0_tasks.md](phase0_tasks.md).

### Phase 1 — Comms bring-up: V2X ECU + Scenario Player (R5–R9, R11 — **R10 moved to the future plan**)

**Objective.** Bench CPMs reach the deployed Room and decode into R2 messages at the ADA ECU. **Receive-only** — ego broadcasts nothing.

**Tasks.**
- Build & push the OCI images, author the blueprint, deploy, verify all nodes Running (R5); prove pair-wise UDP reachability (R6).
- Radio adapter seam `init · configure · subscribeRx · send` (R7); modem stub FSM with fault injection (R8).
- Rx pipeline decode → validate → dedupe → forward (R9). ~~Ego Tx via the adapter `send` (R10)~~ — **moved to the future plan; not implemented in M1.** The R7 seam still declares `send`, but nothing calls it.
- Bench scenario-configurable CPM generation (R11).
- V2X-side JSONL event logs (message rx/tx, decode results) — the R18 evidence stream starts here.

**Acceptance Criteria.**
- [ ] The blueprint deploys to a Room; the Deployment Viewer shows every node Running; the team APK launches on the AAOS node (R5).
- [ ] UDP reachability between every communicating pair; traffic captured on the bridge network (R6).
- [ ] CI import check passes — no direct transport imports above the seam; telux parity notes + port plan committed (R7).
- [ ] The full scripted call flow is acked and logged; each injected fault produces a defined, logged recovery (R8).
- [ ] Golden-vector CPMs decode correctly; the malformed-input corpus is fully rejected with zero crashes (R9).
- [ ] Different bench scenario configurations produce observably different message streams (R11).
- [ ] R2 messages observed at the ADA ECU carrying decoded bench-scenario values, not constants (R2).
- [ ] **Demo:** Wireshark capture of V2X PDUs correctly sent/received at the V2X ECU interface.

### Phase 2 — ADA scaffolding: store + state machine, no detector (R3, R13)

**Objective.** Stand up the ADA skeleton, the R3 track store, and the R13 admission state machine on **mock input**, so the pipeline works before any ML.

**Tasks.** Decomposed in [phase2_tasks.md](phase2_tasks.md); design of record is [phase2-4-ada-ecu-hld.md](../ADA_ECU/doc/phase2-4-ada-ecu-hld.md), which realizes [ada-ecu.svg](../requirements/ada-ecu.svg).

- ADA C++17 module skeleton inside [ADA_ECU/](../ADA_ECU/); CRA interface + registry + database schema defined (consumed by R14 in Phase 4).
- R3-shaped store; R13 state machine driven by mock R2 messages and mock own-sensor entries — the mocks are external stimulus (a JSONL fixture through the real detector-reader, real datagrams from `tools/mock_v2x_sender.py`), never a branch inside `src/`.
- Video-input study — **produced**: [video-source-for-r12.md](../ADA_ECU/doc/research_notes/video-source-for-r12.md) §3 is the format / frame rate / data rate proposal for FPT-Mentor (report § Input constraints); what remains is sending it and **the user supplying the clip**, which blocks Phase 3.

**Acceptance Criteria.**
- [ ] The store exposes all R3 fields; detector-shaped and relayed-shaped entries enter through the identical interface (R3).
- [ ] Mock-driven state transitions are observable in logs and match the R13 diagram; toggling the mock off yields no tracks.
- [ ] Mock C is admitted only within `gate_enter` and dropped only beyond `gate_exit` or after `miss_limit` — no add/remove flicker.
- [ ] Gate constants are read from configuration — no literals.
- [ ] CRA database schema committed; video-input proposal sent to FPT-Mentor.
- [ ] **Demo:** build + CI round-trip tests green on the frozen contracts (golden vectors).

### Phase 3 — Object detection from video (R12) — runs ∥ with Phase 4

**Objective.** Replace the mock own-sensor input with real detection: a pretrained detector finds **B, the visible occluder**, in the provided video and estimates its distance.

**Tasks.** Decomposed in [phase3_tasks.md](phase3_tasks.md); the ADA+IVI cross-node execution view is [ada-ivi-plan.md](ada-ivi-plan.md).

- YOLO11n exported to ONNX on ONNX Runtime CPU; OpenCV video decode behind the frame-source seam.
- Per-frame detection + distance estimation; stream R3 JSONL over stdout into the store (subprocess contract — no FFI, no RPC).
- **The clip at `ADA_ECU/media/ego-b-occluding-c.mp4` is sourced and post-produced inside this phase** (task group 3.7, added 2026-08-02): openly-licensed footage found online, cut and re-encoded with ffmpeg to the [research note §3 spec](../ADA_ECU/doc/research_notes/video-source-for-r12.md#3-video-input-spec-to-build-phase-3-against), then committed and baked into the image as one early `COPY media/ /app/media/` layer — the only way a file reaches a Container Node ([m1-video-source-and-ivi-dashcam.md §5](../requirements/m1-video-source-and-ivi-dashcam.md)). The user-supplied clip (`12.2.9.3`) remains the preferred source but no longer gates the phase. The synthetic generator stays a CI fixture and cannot produce R12 evidence. The two binding content rows — B occluding at 10–40 m in ≥ 90% of frames, **C never visible in any frame** — are established on the source and re-established on the encoded file.

**Acceptance Criteria.**
- [ ] Detection log over the provided clip with per-frame objects and distance estimates (R12).
- [ ] Entries enter the store via the same R3 interface as relayed entries, `source = own_sensor` — mock no longer required.
- [ ] **Zero detections labeled C** — checked on the detection log by `ADA_ECU/tools/check_zero_c.py` (feeds the R19 zero-C check).
- [ ] Runs CPU-only on the provided clip; offline pace acceptable — effective inference rate ≥ 5 Hz (≤ 200 ms per sampled frame) measured on the deployed node. Live detection at speed is future scope.

### Phase 4 — Obscured-object fusion: relayed C + risk + warning (R13–R15) — runs ∥ with Phase 3

**Objective.** ADA turns live R2 traffic into a tracked ghost C, assesses NLOS collision risk through the CRA abstraction, and emits R4 warnings carrying the composed scene.

**Tasks.** Decomposed in [phase4_tasks.md](phase4_tasks.md).

- Relayed-C admission per R13 from R2 `object.distance` (`source = v2x_relayed`).
- CRA abstraction + the M1 NLOS plugin registered through it, reading/writing the Phase 2 database schema (R14).
- Scene composition (ego, B, ghost C — `d_AC = d_AB + d_BC`, lateral offsets component-wise) and edge-triggered R4 warning emission on risk transitions (R15); the periodic awareness state only if time permits (optional).
- ADA-side JSONL event logs (track transitions, risk events) completing the R18 collision-risk event list.

**Acceptance Criteria.**
- [ ] With bench scenarios live, C's track appears with `source = v2x_relayed` only and follows the full R13 lifecycle.
- [ ] The NLOS plugin registers through the CRA interface; the abstraction + database schema are the committed artifacts (R14).
- [ ] At least one R4 warning event per scenario run, carrying the risk state and the composed geometry (R15).
- [ ] The event list reconstructs a full run offline (R18).
- [ ] **Output check:** with a scenario run live, **(a)** the ADA `[EVT]` log shows a `tracked` `own_sensor` TrackedObject for **B** and a `tracked` `v2x_relayed` TrackedObject for **C**, both with full R3 fields, plus at least one `r4_tx` carrying the emitted R4 body; **and (b)** a pcap of ADA→IVI UDP traffic decodes to the same R4 body — C's full R3 TrackedObject in `object`, B's position in `geometry.vehicleB` — correlated to the log by timestamp and length. The frozen R4 carries no full TrackedObject for B; that half is proven from the log, not the wire ([phase4_tasks.md § Phase 4 output acceptance](phase4_tasks.md#phase-4-output-acceptance--what-b-and-c-reach-the-ivi-means-precisely)).
- [ ] **Demo:** ADA logs — collision-risk event list; optional annotated video export with per-event risk labels (§1 demo table).

### Phase 5 — IVI HMI, mock-driven (R16, R17) — display track, parallel from the start

**Objective.** The IVI renders the warning view — the God view of ego, B, and ghost C — from R4 messages alone, developed against mock warnings and integrated against real data at Phase 6.

**Tasks.** Decomposed in [phase5_minh_tasks.md](phase5_minh_tasks.md) — the authoritative Phase 5 breakdown; [phase5_tasks.md](phase5_tasks.md) is superseded. Cross-node view: [ada-ivi-plan.md](ada-ivi-plan.md).

- Compose HMI with the R16 layout (central Display area + button/app areas) on the provided AAOS node; UDP ingest service for R4.
- 2D Canvas warning view behind the view seam (R17); optional, only if time permits: SceneView 3D through the same seam, multi-process wake-on-warning.
- **Not in this phase:** the ego video clip display ("dashcam view") in the Display area — deferred, confirmed by the user 2026-08-02. No Media3 player, no clip serving from ADA, no `exposedPorts` entry for it, no `video` pin. Itemized in [ada-ivi-plan.md §5](ada-ivi-plan.md).

**Acceptance Criteria.**
- [ ] The HMI runs on the AAOS node with the R16 layout; button/app areas switch what the Display area shows.
- [ ] **(Dev)** A mock R4 warning brings the warning view up showing ego, B, and ghost C at the composed positions.
- [ ] Ghost C renders from `v2x_relayed` data only; the 2D drawing is delivered (R17 — 3D stays optional).
- [ ] A newer message with an unknown `warningType` degrades gracefully (R4 additive-version test).
- [ ] Optional paths, only if built: an ADA message wakes the separate warning app; 3D renders through the view seam.

#### Task group 5.10 — the mini-blueprint import route and its AI/Human split

Added 2026-08-03. The user specified a seven-step chain for the IVI verification run whose blueprint is **authored and imported by an agent** rather than cloned by hand. That is a different route from the one [phase5_minh_tasks.md](phase5_minh_tasks.md) group 5.8 plans, and its AI rows are ones no Phase 5 subtask currently owns. This group carries the difference; everything already covered stays where it is and is referenced, never duplicated.

Stage 2 of [walkthrough-driven-delivery.md](../.claude/rules/walkthrough-driven-delivery.md) — every subtask cites the section of [deploy-ivi-hmi-walkthrough.md](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md) (its container-node companion [deploy-walkthrough-netcheck.md](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md)) that governs its step, and takes its acceptance from [§6 Expected outputs and acceptance](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance). The executor of each row is fixed by [§5 Work division](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human), not by this plan.

**Where the chain's seven steps live.**

| # | Chain step | Executor | Subtask |
|---|---|---|---|
| 1 | Author the mini-blueprint and import it over REST | AI (agent, then [[car-sky]]) | `5.5.10.1` · `5.5.10.2` — **new, blocked on gap G1** |
| 2 | Wire and configure the `ethernet` pin | Human, confirmed by [[car-sky]] | `6.5.10.3` · `6.5.10.4` — **new** |
| 3 | Write the mock-ADA container, push it, CI builds and exports to Zot | AI (agent) | Covered: `4.5.6.1`–`4.5.6.5`, `5.5.6.6`, `5.5.7.3`; registry-side confirmation is **new** `5.5.10.5` |
| 4 | Configure the ADA node with the simulator image | Human | Covered: `5.5.9.1` |
| 5 | Deploy the blueprint | Human, polled by [[car-sky]] | Deploy covered by `5.5.9.1`; the poll-to-`Running` AI row is **new** `5.5.10.6` |
| 6 | Verify via logs | AI ([[car-sky]]) | `18.5.10.7` — **new**; no Phase 5 subtask carried an AI log-read |
| 7 | Verify the warning screen on the device | Human | `16.5.10.8` — **new** as its own step; previously absorbed into `17.5.9.3` |

**Relationship to groups 5.8 and 5.9.** `5.5.10.1`–`6.5.10.4` are the import-route alternative to `5.5.8.2`'s clone-then-delete creation — run one or the other, never both. `5.5.10.5`–`16.5.10.8` are additive: they split the AI rows out of group 5.9's bundled *USER-MANUAL* subtasks, which stay as the record-keeping and human-judgement half. `16.5.8.3` (ADB reach) and `16.5.9.2` (install and launch) are unchanged prerequisites.

**Gap G1 — stage 1 is missing for the import route, and `5.5.10.1`/`5.5.10.2` are blocked until it is run.** [§4.11](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#411-the-mini-blueprint-route) states "**Do not import** a hand-authored blueprint JSON", for two reasons: an import arrives without its `ethernet` pins, and typically without the Skycraft `image` block. The user's route answers the first (step 2 draws the pins by hand) and intends to answer the second (the authored JSON carries the block). Neither answer is in the walkthrough, the route has no entry in its [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human) work-division table, and **whether an import preserves a `skycraft` node's `image` config on this deployment is asserted nowhere** — it belongs in [§6.1 Confirm before relying on these](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these).

The rejection is not a single document's opinion: [deploy-ada-ecu-walkthrough.md §2.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#22-the-blueprint-definition-and-where-it-lives) independently calls its own blueprint JSON "a record of intent, not an import route", and [§4.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#41-create-the-blueprint) prescribes clone-then-edit for the same reason. Two walkthroughs steering away from import is a finding to resolve at stage 1, not to override at stage 2. [[project-researcher]] owns that procedure; no subtask below may fill the gap by writing the steps into its own brief.

**Gap G2 (minor).** The IVI walkthrough's §5 table has no row for the simulator image's build, push or registry confirmation — it points at Zot only to say Zot is not on the APK path. `5.5.10.5` therefore cites the container-node companion's [M3 + M4](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m3--m4--build-and-push-automatic) and its [§5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human) rows instead. Worth a row in the IVI table so the ADA-node feed's provenance is not split across two documents.

**Execution split labels** are [phase5_minh_tasks.md § Execution split legend](phase5_minh_tasks.md)'s: *agent*, *car-sky*, *USER-MANUAL*. **Subtask discipline** — single objective, one atomic commit, build and unit tests green, self-contained brief — is [task-planning-conventions.md](../.claude/rules/task-planning-conventions.md#subtask-discipline-non-negotiable) and applies to every subtask below; the evidence subtasks' "build" is that their recorded outputs come from a live Room, not from prose.

##### `5.5.10.1` — Author the 3-node mini-blueprint import JSON *(agent)* — blocked on G1

**Objective:** produce one importable blueprint JSON describing the IVI mini topology, with full flat node config and no pins or edges.

**Scope:** new file `requirements/car-sky-guide/blueprint-m1-ivi-mini.json`, shaped like the existing `blueprint-m1-cooperative-awareness.json` beside it.

- Three nodes, composition per [§4.11](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#411-the-mini-blueprint-route): `eth-bridge` (`bridgeMode: "linux"`, `subnet: 10.99.0.0/24`, address `10.99.0.1`), an ADA `container` node at `10.99.0.12`, an IVI `skycraft` node at `10.99.0.13`.
- **Node config is flat** — `image`, `command`, `env`, `capabilities` sit directly in `config`, not wrapped in a `container` object ([carsky-rest-api-blueprint.md § Node config is flat](../requirements/car-sky-guide/carsky-rest-api-blueprint.md)).
- The IVI node's `config` carries the Skycraft `image` block **verbatim** from [node-ivi-ecu.md § Blueprint node config](../requirements/car-sky-guide/node-ivi-ecu.md#blueprint-node-config) — all four fields. Its absence gets the whole deploy rejected outright with the message quoted there.
- The ADA node carries the **probe** feed of [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) rung V2 — `registry.hackathon-2.carsky.io/m1-netcheck:latest`, `NEXT_HOP_HOST=10.99.0.13`, `NEXT_HOP_PORT=47300`, `capabilities: ["NET_RAW"]`. The simulator image arrives later by the human node edit of `5.5.9.1`; §4.11 fixes the ADA node as the only node ever reconfigured.
- `pins` and `edges` are **empty arrays**. REST import silently drops `ETHERNET` pins ([§1.4](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#14-blueprints); [carsky-rest-api-blueprint.md § Key finding](../requirements/car-sky-guide/carsky-rest-api-blueprint.md#key-finding-what-rest-can-and-cannot-do)), so declaring them would only look like they exist. `6.5.10.3` draws them.
- **No procedure text.** This subtask ships a machine-readable artifact, not a walkthrough section — the steps for using it are researcher-owned (G1).

**Acceptance:** the file parses as JSON; every one of the four `image` fields matches node-ivi-ecu.md § Blueprint node config character for character; the three addresses match §4.11's topology; `pins` and `edges` are empty.

**Dependencies:** G1 closed. **Commit:** `[5.5.10.1] docs: add the 3-node IVI mini-blueprint import JSON`

##### `5.5.10.2` — Import the mini-blueprint over REST and read the stored topology back *(car-sky)* — blocked on G1

**Objective:** create the mini-blueprint on `hackathon-2.carsky.io` from `5.5.10.1`'s file, and record exactly what the import kept and dropped.

**Scope:** `POST /api/v1/blueprints/import` then `GET /api/v1/blueprints/{id}` ([carsky-rest-api-blueprint.md § API reference](../requirements/car-sky-guide/carsky-rest-api-blueprint.md)). Reading the stored config back — rather than trusting the Inspector's truncated fields — is the AI row of [§4.2](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#42-configure-the-blueprint-and-its-ivi-node). Record per node: `nodeType`, the full stored `config`, and the surviving `pins`/`edges`.

**The one fact this subtask exists to settle, which no document currently asserts:** whether the import preserves a `skycraft` node's `image` config block on this deployment. Record the answer either way. Negative means the human sets it in the Inspector before deploying (§4.2's `image` row) and the finding goes back to [[project-researcher]] for the walkthrough and to [[project-architecture]] for the node reference.

**Acceptance:** the new blueprint's id, the read-back showing three nodes with their stored config, and an explicit yes/no on the Skycraft `image` block and on any surviving pin — all in `plans/doc/phase5-ivi-run.md`.

**Dependencies:** after `5.5.10.1`; G1 closed. **Commit:** `[5.5.10.2] docs: record the REST import of the IVI mini-blueprint and its read-back`

##### `6.5.10.3` — Wire the three `ethernet` pins on the Nydus canvas *(USER-MANUAL)*

**Objective:** give the imported blueprint the R6 network that REST cannot create.

**Scope:** the pin-drawing half of [§4.2](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#42-configure-the-blueprint-and-its-ivi-node) — draw each missing pin on the canvas and wire it to the Ethernet Bridge, same-type only. The pin's shape, `direction` and address are [node-ivi-ecu.md § Pins](../requirements/car-sky-guide/node-ivi-ecu.md#pins); the wiring rule and the address set are [netcheck M6](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m6--check-the-wiring). Every non-bridge node's pin is `OUTPUT` into the bridge's single `INPUT` pin, regardless of which way the data flows. Addresses: ADA `10.99.0.12`, IVI `10.99.0.13`.

While in the same canvas session, per §4.2: confirm the IVI node's Skycraft `image` block is present and set it from [node-ivi-ecu.md § Blueprint node config](../requirements/car-sky-guide/node-ivi-ecu.md#blueprint-node-config) if `5.5.10.2` found it dropped, and **write down the node's Part Prefix and Display Width/Height/DPI/GPU Backend** — §4.7's Screen, Log and ADB widgets select parts by that prefix, and the display size must not be assumed.

Both [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human) ("Configure the blueprint and its pins") and [netcheck §5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human) ("M6 — Draw a missing pin or edge") assign this to Human: the REST `pinType` enum has no `ETHERNET` member. No agent performs it; the plan tracks it.

**Acceptance:** `6.5.10.4`'s read-back confirms the pins and edges; the measured Skycraft display fields are recorded in `plans/doc/phase5-ivi-run.md`. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `5.5.10.2`. **Commit:** `[6.5.10.3] docs: record the hand-wired ethernet pins on the imported mini-blueprint`

##### `6.5.10.4` — Confirm the wired topology and node config by read-back *(car-sky)*

**Objective:** prove from stored state, not from the Inspector, that the blueprint is deployable before a Room slot is spent on it.

**Scope:** `GET /api/v1/blueprints/{id}` — the AI read-back row of [§4.2](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#42-configure-the-blueprint-and-its-ivi-node), and the "M6 — Check the wiring" and "M8 — Confirm the IVI node's VM artifact" AI rows of [netcheck §5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human). Assert every one of:

- one `ETHERNET` / `OUTPUT` pin on the ADA node at `10.99.0.12` and one on the IVI node at `10.99.0.13`, each edged to the bridge's `INPUT` pin;
- the bridge node's config carrying `bridgeMode` and `subnet` — without them the `10.99.0.x` addresses have no network;
- the IVI node's config carrying all four Skycraft `image` fields;
- the ADA node's config carrying its probe image, relative `command`, env set and `NET_RAW`.

`POST /api/v1/blueprints/{id}/validate` is a cheap second confirmation — it returns 422 `Node "…" has no pins` until `6.5.10.3` has landed, so a pass independently proves the wiring.

**Acceptance:** the read-back excerpt in `plans/doc/phase5-ivi-run.md` with every assertion above met, or the exact mismatch named and handed back to `6.5.10.3` rather than worked around.

**Dependencies:** after `6.5.10.3`. **Commit:** `[6.5.10.4] docs: record the mini-blueprint topology read-back and validation`

##### `5.5.10.5` — Confirm `m1-r4-sim:latest` reached the Zot registry *(car-sky)*

**Objective:** prove registry-side that the image the ADA node will pull exists and is single-platform `linux/arm64`.

**Scope:** the independent check of [netcheck M3 + M4](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#m3--m4--build-and-push-automatic) — the image appears in the Zot catalog — assigned AI by that document's "M4 — Confirm the image reached the registry" row. The catalog and tag-list calls in their exact form are [deploy-ada-ecu-walkthrough.md §3.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#33-confirm-both-images-reached-the-registry), run against `m1-r4-sim` instead of its two images. The credential is the Zot API key of [zot-registry-api-key.md](../requirements/car-sky-guide/zot-registry-api-key.md), supplied at run time and never stored. This is the registry-side half of `5.5.7.3`, whose verification runs inside the CI job; a node that cannot pull is the `Provisioning`-hang signature of [§4.10](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#410-troubleshooting-the-deploy-and-install)'s image row, and the cheapest place to catch it is before the deploy.

**Acceptance:** `m1-r4-sim` present in the catalog with tag `latest`; the manifest is a single-platform `linux/arm64` image, **not** a manifest index (the cluster rejects indexes); the digest recorded in `plans/doc/phase5-ivi-run.md` and matching the digest `5.5.7.3` recorded.

**Dependencies:** after `5.5.7.3`. Parallel with `5.5.10.1`–`6.5.10.4`. **Commit:** `[5.5.10.5] docs: record the registry-side confirmation of m1-r4-sim:latest`

##### `5.5.10.6` — Poll the deployed nodes to `Running` and resolve every `nodeKey` *(car-sky)*

**Objective:** turn the human's deploy into recorded evidence that the Room came up, and produce the keys every log route below needs.

**Scope:** the deploy itself is Human — [§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint), and [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human) keeps it there because picking the Device spends one of the two Room slots. This subtask is that section's AI row: poll `GET /api/v1/deployments/{roomId}/nodes` until every node reads `Running` with restart count 0, recording each entry's `name` — the `nodeKey`. The Skycraft node is the slowest and is expected to lag the containers (§4.3). A node stuck in `Provisioning` is diagnosed per [carsky-room-diagnostics](../.claude/skills/carsky-room-diagnostics/SKILL.md) and `5.5.10.5`, never by redeploying blind.

Deploy the **original** blueprint, never the `<name>-deploy` snapshot the first deployment creates — §4.3's most expensive rule when broken.

**Acceptance:** 3/3 nodes `Running` with restart count 0, and the three `nodeKey` values recorded in `plans/doc/phase5-ivi-run.md`. This is the precondition every [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance) proof rests on.

**Dependencies:** after `6.5.10.4` and the user's deploy. **Commit:** `[5.5.10.6] docs: record the mini-blueprint Room reaching Running`

##### `18.5.10.7` — Read the two log surfaces and produce proofs 1–3 *(car-sky)*

**Objective:** produce in text three of the four proofs of [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance) — the incoming warning, its parsed fields, and the event raised — making no visual judgement.

**Scope:** the "Read the two log surfaces" AI row of [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human), against the two surfaces [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging)'s table names:

- the ADA node's producer log, `GET /api/v1/deployments/{roomId}/logs/{nodeKey}?container=user` — **`container` is mandatory**, omitting it returns 500;
- the guest's `adb logcat -s IVI_V2X`, over the endpoint `16.5.8.3` proved.

The rung is §4.8 **V4**, with `approach.json` running on the ADA node. Its four links are the checklist and are not restated here; this subtask stops after link 3, because link 4 is a visual judgement §5 assigns to Human — `16.5.10.8`.

**Contingency:** the guest-side half depends on ADB reach, item 1 of [§6.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these), answered by `16.5.8.3`. If ADB is unreachable, produce the ADA-side half and record that the guest half moved to emulator evidence — do not improvise a third route.

**Acceptance:** §6 proofs 1, 2 and 3, as log excerpts in `plans/doc/phase5-ivi-run.md` — one `[RX] type=warning … cSource=v2x_relayed` per datagram corroborated by the ADA's `[TX] … → 10.99.0.13:47300`, the parsed `warningType` / `risk` / `cSource` / `cPos` fields on that same line, and `[UI] mode=WarningView cause=warning` carrying `cause=warning` and not `cause=user`.

**Dependencies:** after `5.5.10.6`, `5.5.9.1` (the evidence feed), `16.5.9.2` (app installed and launched) and `16.5.8.3`. **Commit:** `[18.5.10.7] docs: record the ADA and guest log evidence for the R4 warning chain`

##### `16.5.10.8` — Confirm the warning screen on the device, capture it, tear down *(USER-MANUAL)*

**Objective:** produce [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance)'s fourth proof — the only one no log line replaces — and release the Room slot.

**Scope:** the "Confirm the display switched" and "Record the screen" Human rows of [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human), on the Screen widget configured per [§4.7](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#47-open-the-screen-and-launch-the-app) and captured per [§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence). What counts as correct is [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) V4's **link 4** and its two failure rows, which are not restated here — including that a yellow `[? UNKNOWN SOURCE]` marker where ghost C belongs is a **blocking defect**, not a display quirk: stop and report it rather than accepting the run.

Then tear down per [§4.12](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#412-tear-down) — only two Rooms may run at once and the comms track needs the slot; the blueprint is kept and redeployable.

**Acceptance:** §6 proof 4 — a recording or screenshot showing the God View with ghost C dashed, badged and glowing — plus §6's second table row, `cSource=v2x_relayed` on every rendered warning, which `18.5.10.7`'s excerpt supplies. Recorded in `plans/doc/phase5-ivi-run.md`; the deployment deleted. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `18.5.10.7`. **Commit:** `[16.5.10.8] docs: record the God View screen evidence and the Room teardown`

##### Execution order

```
5.5.10.1 ─► 5.5.10.2 ─► 6.5.10.3 ─► 6.5.10.4 ─► [user deploys] ─► 5.5.10.6 ─► 18.5.10.7 ─► 16.5.10.8
   (G1)        (G1)       USER                                                                 USER
5.5.10.5  ∥ everything above, after 5.5.7.3
```

- **Sequential:** every arrow — each step's evidence depends on the previous step's platform state, and [§4.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#41-how-the-apk-reaches-the-ivi-ecu-node)'s ordering is binding (the guest must exist before anything installs into it).
- **Parallel:** `5.5.10.5` against the whole chain; and the chain against all of groups 5.1–5.7, which touch no Room.
- **Branch:** no new phase, so the existing Phase 5 suggestion **`feat/phase5-ivi-hmi`** stands. Every subtask here is docs or a platform artifact, which commits straight to `main`; the branch matters only for the code groups.

### Phase 6 — Convergence: real data end-to-end (R18, R19 — **R10 moved to the future plan**)

**Objective.** Replace every mock with **real** data and record the definition-of-done run — a swap-and-verify step, not a new build.

**Tasks.**
- Point the IVI at live ADA output. ~~Feed ego Tx (R10) from the real R12 store snapshot instead of mock contents.~~ — **moved to the future plan.**
- Full-chain rehearsal bench → V2X ECU → ADA → IVI across bench scenarios (e.g. C approaching vs C out of range).
- Record the R19 run with its captures.

**Acceptance Criteria.**
- [ ] No mocks anywhere in the ego path (the bench is sanctioned test equipment, not a mock).
- ~~[ ] Ego Tx frames captured on the bridge carry live store data, not constants (R10).~~ — **moved to the future plan; not an M1 acceptance criterion.**
- [ ] One continuous recorded run: the IVI warns and renders ghost C from `v2x_relayed` only, while the R12 detection log shows zero C for the whole run (R19).
- [ ] Wireshark/pcap of the V2X exchange and of the ADA→IVI traffic corroborates the chain (R15, R19).
- [ ] Every §1 demo-evidence method that cites logs is producible from the R18 logs.

---

## 6. Deferred to Later Milestones

The single source is the report's § Future developments, mirrored in the [future-features register](../requirements/future/m1-future-features-register.md). Standing M1 exclusions live in the report's §4 decision record (**R10 ego Tx deferred — the V2X ECU is receive-only**, Cortex-M omitted, telux port declined, 3D and multi-process optional, ego video clip deferred, no GPU, no map/GNSS on the IVI).

**Ego video clip display on the IVI — re-confirmed deferred, 2026-08-02.** [m1-video-source-and-ivi-dashcam.md §4/§6](../requirements/m1-video-source-and-ivi-dashcam.md) contains a worked design for it (option B4: ADA serves its own clip over HTTP, the IVI plays it with Media3), and §8 flags that adopting it needs the user's word. **The user's word was no.** The design stays on the shelf; the itemized exclusion — HTTP clip serving, `exposedPorts`, Media3, a dashcam `DisplayMode`, any `video` pin, and real-time detector pacing as a *requirement* — is [ada-ivi-plan.md §5](ada-ivi-plan.md). No Phase 3, 4 or 5 subtask may implement any of it.

## 7. Definition of Done

R19: all phases pass their acceptance criteria, and one continuous recorded run shows **vehicle C appearing on ego's IVI purely via the V2X relay** — ghost C rendered from `v2x_relayed` data only, zero direct C detections in ego's own perception — corroborated by the R19 captures.
