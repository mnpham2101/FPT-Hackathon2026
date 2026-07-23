# Milestone 1 — Cooperative Vehicle Awareness (A ← B ← C)

> Requirements, scope, and tech stacks live in the authoritative report [m1-cooperative-awareness.md](../../requirements/m1-cooperative-awareness.md) (R1–R19); this plan references R-numbers instead of restating them, and on any conflict the report wins. Phase structure follows the [proposal-deck timeline](../../presentation/assets/m1-phase-timeline.svg) (second authority).

## 1. Introduction

Milestone 1 demonstrates **cooperative (non-line-of-sight) awareness** over V2X: making a vehicle aware of a hazard it cannot see, by relaying another vehicle's perception (report §1).

Three vehicles drive in a collinear convoy — **A** follows **B** follows **C**. Vehicle A's view of C is **blocked by B**, so A's own camera can never detect C. Vehicle B sees C and **broadcasts that perception to A over V2X**, and A displays C and its relative position without ever seeing it. **M1 builds only A's vehicle (ego)** — B and C exist as bench-generated V2X messages (R11) and as content of the provided video; the bench Scenario Player is sanctioned test equipment, not a mock to eliminate.

![Convoy geometry: A cannot see C; B sees C and relays it to A over V2X](../../requirements/m1_convoy_nlos_relay_geometry.png)

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

With the contracts frozen, three tracks run in parallel and converge at Phase 6 ([timeline](../../presentation/assets/m1-phase-timeline.svg)):

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
| `miss_limit` (M) | proposed 5 | consecutive missed updates before expiry |

![Track admission state machine with proximity gate and hysteresis](../../requirements/vehicleC_track_admission_state_machine.png)

*Own-sensor objects traverse `not_tracked → tentative → tracked`; relayed C is admitted on R2 distance within the gate, dropped on leaving it or when messages stop, and carries `source = v2x_relayed` only. Ego's own Tx (R10) snapshots `own_sensor` tracks — the B-side relay trigger is simulated by the bench scenarios (R11).*

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
- [ ] R1 profile document committed; golden-vector CPMs encode/decode through the Vanetza codec seam.
- [ ] R2, R3, R4 schemas committed; round-trip tests pass in each consumer language (C++ / Python / Kotlin).
- [ ] The R4 additive-version test is defined (a consumer parsing an unknown `warningType` degrades gracefully).
- [ ] Blueprint topology documented (nodes, `ethernet` pins, edges to the bridge).

### Phase 1 — Comms bring-up: V2X ECU + Scenario Player (R5–R11)

**Objective.** Bench and ego exchange the full R1 message over the deployed Room: bench CPMs decode into R2 messages at the ADA ECU, and ego broadcasts CPMs from (mock) store snapshots.

**Tasks.**
- Build & push the OCI images, author the blueprint, deploy, verify all nodes Running (R5); prove pair-wise UDP reachability (R6).
- Radio adapter seam `init · configure · subscribeRx · send` (R7); modem stub FSM with fault injection (R8).
- Rx pipeline decode → validate → dedupe → forward (R9); ego Tx via the adapter `send` (R10 — mock store contents until Phase 6).
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

**Tasks.**
- ADA C++17 module skeleton per the [ADA HLD](../../requirements/ada-ecu.svg); CRA database schema defined (consumed by R14 in Phase 4).
- R3-shaped store; R13 state machine driven by mock R2 messages and mock own-sensor entries.
- Video-input study: identify and propose format / frame rate / data rate to FPT-Mentor (report § Input constraints); stand up the video harness for Phase 3.

**Acceptance Criteria.**
- [ ] The store exposes all R3 fields; detector-shaped and relayed-shaped entries enter through the identical interface (R3).
- [ ] Mock-driven state transitions are observable in logs and match the R13 diagram; toggling the mock off yields no tracks.
- [ ] Mock C is admitted only within `gate_enter` and dropped only beyond `gate_exit` or after `miss_limit` — no add/remove flicker.
- [ ] Gate constants are read from configuration — no literals.
- [ ] CRA database schema committed; video-input proposal sent to FPT-Mentor.
- [ ] **Demo:** build + CI round-trip tests green on the frozen contracts (golden vectors).

### Phase 3 — Object detection from video (R12) — runs ∥ with Phase 4

**Objective.** Replace the mock own-sensor input with real detection: a pretrained detector finds **B, the visible occluder**, in the provided video and estimates its distance.

**Tasks.**
- YOLO11n exported to ONNX on ONNX Runtime CPU; OpenCV video decode.
- Per-frame detection + distance estimation; stream R3 JSONL over stdout into the store (subprocess contract — no FFI, no RPC).

**Acceptance Criteria.**
- [ ] Detection log over the provided clip with per-frame objects and distance estimates (R12).
- [ ] Entries enter the store via the same R3 interface as relayed entries, `source = own_sensor` — mock no longer required.
- [ ] **Zero detections labeled C** — checked on the detection log (feeds the R19 zero-C check).
- [ ] Runs CPU-only on the provided clip; offline pace acceptable (live detection at speed is future scope).

### Phase 4 — Obscured-object fusion: relayed C + risk + warning (R13–R15) — runs ∥ with Phase 3

**Objective.** ADA turns live R2 traffic into a tracked ghost C, assesses NLOS collision risk through the CRA abstraction, and emits R4 warnings carrying the composed scene.

**Tasks.**
- Relayed-C admission per R13 from R2 `object.distance` (`source = v2x_relayed`).
- CRA abstraction + the M1 NLOS plugin registered through it, reading/writing the Phase 2 database schema (R14).
- Scene composition (ego, B, ghost C — `d_AC = d_AB + d_BC`, lateral offsets component-wise) and edge-triggered R4 warning emission on risk transitions (R15); the periodic awareness state only if time permits (optional).
- ADA-side JSONL event logs (track transitions, risk events) completing the R18 collision-risk event list.

**Acceptance Criteria.**
- [ ] With bench scenarios live, C's track appears with `source = v2x_relayed` only and follows the full R13 lifecycle.
- [ ] The NLOS plugin registers through the CRA interface; the abstraction + database schema are the committed artifacts (R14).
- [ ] At least one R4 warning event per scenario run, carrying the risk state and the composed geometry (R15).
- [ ] The event list reconstructs a full run offline (R18).
- [ ] **Demo:** ADA logs — collision-risk event list; optional annotated video export with per-event risk labels (§1 demo table).

### Phase 5 — IVI HMI, mock-driven (R16, R17) — display track, parallel from the start

**Objective.** The IVI renders the warning view — the God view of ego, B, and ghost C — from R4 messages alone, developed against mock warnings and integrated against real data at Phase 6.

**Tasks.**
- Compose HMI with the R16 layout (central Display area + button/app areas) on the provided AAOS node; UDP ingest service for R4.
- 2D Canvas warning view behind the view seam (R17); optional, only if time permits: SceneView 3D through the same seam, multi-process wake-on-warning.

**Acceptance Criteria.**
- [ ] The HMI runs on the AAOS node with the R16 layout; button/app areas switch what the Display area shows.
- [ ] **(Dev)** A mock R4 warning brings the warning view up showing ego, B, and ghost C at the composed positions.
- [ ] Ghost C renders from `v2x_relayed` data only; the 2D drawing is delivered (R17 — 3D stays optional).
- [ ] A newer message with an unknown `warningType` degrades gracefully (R4 additive-version test).
- [ ] Optional paths, only if built: an ADA message wakes the separate warning app; 3D renders through the view seam.

### Phase 6 — Convergence: real data end-to-end (R10, R18, R19)

**Objective.** Replace every mock with **real** data and record the definition-of-done run — a swap-and-verify step, not a new build.

**Tasks.**
- Point the IVI at live ADA output; feed ego Tx (R10) from the real R12 store snapshot instead of mock contents.
- Full-chain rehearsal bench → V2X ECU → ADA → IVI across bench scenarios (e.g. C approaching vs C out of range).
- Record the R19 run with its captures.

**Acceptance Criteria.**
- [ ] No mocks anywhere in the ego path (the bench is sanctioned test equipment, not a mock).
- [ ] Ego Tx frames captured on the bridge carry live store data, not constants (R10).
- [ ] One continuous recorded run: the IVI warns and renders ghost C from `v2x_relayed` only, while the R12 detection log shows zero C for the whole run (R19).
- [ ] Wireshark/pcap of the V2X exchange and of the ADA→IVI traffic corroborates the chain (R15, R19).
- [ ] Every §1 demo-evidence method that cites logs is producible from the R18 logs.

---

## 6. Deferred to Later Milestones

The single source is the report's § Future developments, mirrored in the [future-features register](../../requirements/future/m1-future-features-register.md). Standing M1 exclusions live in the report's §4 decision record (Cortex-M omitted, telux port declined, 3D and multi-process optional, ego video clip deferred, no GPU, no map/GNSS on the IVI).

## 7. Definition of Done

R19: all phases pass their acceptance criteria, and one continuous recorded run shows **vehicle C appearing on ego's IVI purely via the V2X relay** — ghost C rendered from `v2x_relayed` data only, zero direct C detections in ego's own perception — corroborated by the R19 captures.
