# Requirement Analysis & Technical Solution Report — M1: Cooperative Vehicle Awareness

## 1. Project description

### Project goals

The system makes a vehicle aware of a hazard it cannot see by relaying another vehicle's perception.

### Milestone 1 development goals

Milestone 1, developed for the FPT Hackathon, scopes implementation to a single scenario running entirely on a cloud virtual environment:

Three vehicles drive in a collinear convoy — **A** follows **B** follows **C**. Vehicle A's view of C is **blocked by B**, so A's own camera can never detect C. Vehicle B *can* see C, detects it, and **broadcasts that perception to A over V2X**. The result: both A and B display vehicle C and its relative position, even though A never sees C directly.

![Convoy geometry: A cannot see C; B sees C and relays it to A over V2X](m1_convoy_nlos_relay_geometry.png)

*Objective — B's perception of C reaches A over a V2X relay. A reconstructs C's position by composing its own measurement of B with B's reported measurement of C:* `d_AC ≈ d_AB + d_BC` *(valid for the near-collinear convoy; absolute/GPS composition is a later milestone).*

Distance to C is the admission criterion for tracking and for assessing collision risk. M1 uses a fixed distance threshold; a future milestone should scale it with traffic speed (§ Future developments).

![Vehicle Tracking State](vehicleC_track_admission_state_machine.png)

### System design

The 4-ECU design below realizes the goals above; per-ECU M1 scope is in the responsibility list right after it.

![System Design](coorperative-awareness.png)

### ECU and Scenario Player responsibility

Responsibility of each ECU and the Scenario Player in Milestone 1; the Cortex-M ECU is listed only to record its exclusion.

- **V2X ECU**
  - Configures the modem and interfaces with it — simulated only.
  - Connects to the V2X network — either simulated or handled entirely by a 3rd-party library.
  - Receives V2X message payloads, applies business logic, forwards results to the ADA ECU.
  - Receives information from the ADA ECU, constructs V2X message payloads, broadcasts them over the V2X network.
- **ADA ECU**
  - Incorporates information from the V2X ECU and detected objects from the video feed to construct a warning message for the IVI ECU, via a **Collision Risk Assessment abstraction** that analyzes and categorizes risk — built so future warning scenarios can be added as extensions without reworking this code (§ Future developments: modular warning scenarios).
  - M1 need not classify every risk type or filter by criticality; the warning message is designed so new hazard types and criticality levels can be added later without reworking this design (§ Future developments: criticality filtering, other hazard-warning types).
  - Configures the camera and receives live video feeds — neither is implemented (no live camera bring-up is a frozen scope boundary; § Future developments: camera live feed in the IVI central view).
  - Detects objects only from provided saved video files — not a live feed (§ Future developments: live object detection at speed).
  - Does not receive GNSS or other sensor data from the Cortex-M ECU.
- **IVI ECU**
  - Displays GUI and applications, and renders information received from the ADA ECU.
  - View goals (2D or 3D god view of all vehicles). 
  - Multi-process applications **should** be developed but could be deferred to a future milestone (§ Future developments: multi-process front end with wake-on-warning).
- **Scenario Player**
  - Emulates the Quectel modem's connection point toward the V2X ECU.
  - Simulates decoded V2X messages arriving at different rates, in place of the Quectel modem.
  - Optionally simulates Quectel modem configuration responses.
  - Live camera feed simulation is out of scope (§ ADA ECU above); the ADA ECU consumes provided video files directly.
- **Cortex-M ECU**
  - Not developed — sensor data is omitted.

### Cloud development constraints

Development runs entirely on the cloud virtual platform, under its blueprint/node model:

- **Blueprint:** 1 blueprint = 1 car — this project builds **one blueprint**, the ego vehicle; deploying it brings up the whole virtual car on the cloud.
- **Node:** 1 node = 1 ECU, packaged as a container. Nodes cover both the ego ECUs to be developed (§ ECU and Scenario Player responsibility) and the **bench node** — the Scenario Player to be developed, standing outside the ego blueprint ([working-env note, deliverable 1](m1-phase1-working-environment.md)).

The table below covers the ECUs and the Scenario Player — node roles are described in § ECU and Scenario Player responsibility and the working-env note, not restated here. Per node it states the virtualization level, the scope of development within it, and the goals to achieve.

| Node name                    | Virtualization level                                                        | Focus goals                                                                                                                                                                    |
| ---------------------------- | --------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| IVI ECU                      | Full vECU — developed fully; the container itself ships in the starter pack | 3D display: god view of the 3 vehicles with every instance displayed; 2D ⇄ 3D view switching; switching to another app                                                         |
| V2X ECU                      | App-level (Linux container) — developed in part: only certain layers        | Portability to different hardware — the developed layers move across hardware unchanged                                                                                        |
| ADA ECU                      | App-level (Linux container) — developed fully                               | Structured, high-performance architecture receiving and sending inputs: condensed messages, low bandwidth, low latency; object detection / AI deep learning is not an M1 focus |
| Bench node — Scenario Player | Container — team-built, developed fully                                     | V2X message playback across different scenarios (§ ECU and Scenario Player responsibility)                                                                                     |

### Input constraints

- V2X control messages and V2X data (broadcast messages) are inputs to the V2X ECU and carry inter-vehicle communication. Development focuses on both extracting **and** constructing application-layer data — e.g. "there is an obstruction ahead of me, broadcast it" and "a broadcast was received, check what it is."
- The V2X protocol stack ships in the modem and stays out of scope for the whole project, not just M1 — a project-wide goal: the V2X application layer must port to any hardware without code changes (§ Cloud development constraints).
- Modem↔Cortex-A interfacing (boot-up, configuration) *could* be implemented at the interface layer; control-plane messages *could* be constructed by the V2X ECU and sent to the modem to satisfy the required 3GPP call flow.
- Live video is the ADA ECU's eventual input; in M1, video files are provided instead.
- Video format, frame rate, data rate, and capture conditions are not yet known — these *should be studied and proposed* to FPT-Mentor so a supported input spec can be provided.
- A pre-trained model *should be used* to detect the obstruction (vehicle C) — no training in M1.
- The ADA ECU → IVI ECU data path *must be* developed.

### System demo requirements

Candidate demonstration methods, scored 1 (low) – 10 (high) by preference. The score reflects how convincing the method is to the jury — invest effort in each method roughly in proportion to its score.

| Method                                         | Demonstrates                                                                                                            | Prefer score |
| ---------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------- | ------------ |
| 3D drawing on A's IVI HMI                      | Bird's-eye (god) view of the 3-vehicle convoy, showing occluded vehicle C                                               | 10           |
| 2D drawing on A's IVI HMI                      | 2D top-down view of the convoy, showing occluded vehicle C                                                              | 8            |
| Video feed on A's IVI HMI                      | A's video feed, showing B — the vehicle occluding C                                                                     | 8            |
| Wireshark capture                              | V2X messages on the wire — confirms V2X data PDUs are correctly sent/received at the V2X ECU's interface                | 7            |
| Logging in ADA ECU — collision-risk event list | Shows the ADA ECU correctly incorporates both V2X information and its own object detection                              | 7            |
| Logging in ADA ECU — annotated video export    | TTC overlay + risk-level label per dangerous event; corroborates that ADA correctly detects in-range objects from video | 6            |

### Future developments

Milestone 1's design is deliberately extensible toward the following features, all out of M1 scope:

- **Modular warning scenarios.** ADA's warning-scenario logic is a self-contained module; future warning scenarios (intersection hazards, curve blind spots) plug in as new realizations of a shared Collision Risk Assessment abstraction.
- **Priority-vehicle preemption warning.** Drawn as a future realization in [ada-ecu.svg](ada-ecu.svg) (user decision 2026-07-12): warn when an emergency/priority vehicle approaches so the driver can yield — plugs in as a new realization of the Collision Risk Assessment abstraction (§ Modular warning scenarios).
- **Speed-scaled collision risk assessment.** Collision Risk Assessment extends from M1's fixed distance threshold (§ Milestone 1 development goals) to one that scales with traffic speed.
- **Camera live feed in the IVI central view.** The central view renders live camera frames. Live video also feeds ADA's obstruction algorithm.
- **Live object detection at speed.** B travels up to 120 km/h (33.3 m/s) and must detect obstructions within stopping + broadcast distance: detect a car at ≥ 130–150 m dry / ≥ 190 m wet (braking + reaction + broadcast margin; ≥ 100/160 m with automated braking). Detection range, not frame rate, is the binding constraint — reliable detection at that range needs ≥ 1280–1920 px inference at ≥ 10 Hz, beyond CPU capacity, so this requires GPU-class acceleration (flag F11).
- **Multi-process front end with wake-on-warning.** A home app defines the outer frame/buttons and orchestrates other apps; on an obstruction event, a separate sleeping app wakes and displays the 3-vehicle scene. Per-app engine RAM and cold-start latency (100s of ms) are known costs of this pattern on Flutter; AGL/Toyota production use suggests it's workable. **Mandatory future feature** (user decision 2026-07-12, F15) — M1 lays its foundation via R18's foundation obligation.
- **2D ⇄ 3D view switching.** User can opt to show 2D God View instead of 3D.
- **Custom appearance / themes.** Vehicle's avatar could be customized.
- **Fast-UI + C2X benchmark criteria.** Extremely fast UI behavior alongside C2X services — e.g. a foreground media app plus 2D/3D display or notification of vehicle C / V2X message arrival; benchmark thresholds are negotiable against technical feasibility.
- **Multiple hidden obstructions detection.** ADA detects and tracks multiple objects outside line of sight simultaneously (M1 is single-object only).
- **Single-message aggregation.** Multiple detected obstructions collapse into one V2X message — no broadcast storm.
- **User-opt message reduction.** The user can opt for fewer V2X messages.
- **Criticality filtering.** The user can opt to receive only warnings at or above a chosen criticality level; criticality is looked up from the ADA→IVI message's `warningType` field (§ ECU and Scenario Player responsibilities), not carried as its own value — see [m1-warning-message-extensibility.md](m1-warning-message-extensibility.md) R22.
- **Other hazard-warning types.** Slippery roads, falling rocks, road holes, road condition, presence of children, police, speed limits, no-horn/other road rules, traffic conditions.
- **Commands to other ECUs.** The [ada-ecu.svg](ada-ecu.svg) output stage sends Current TrackedObject/Risk/**Commands** to other ECUs/hardware (user decision 2026-07-12); M1 implements only the R8 store snapshot to the V2X ECU — command/actuation output to further ECUs is a future path.

### Numbering

**Numbering.** Requirements were re-enumerated from scratch as **R1–R25** at round-3 convergence (2026-07-10) under a **user-granted exception** to the frozen-numbering rule in [research-report-format.md](../.claude/rules/research-report-format.md) — the CarSky re-platforming (BTC letter 09/07/2026) invalidated the round-1/2 set's structure, so continuation numbering would have preserved staleness, not stability. Old numbers (round-1/2 R1–R20) are **void outside this report's supersession notes**; any pre-2026-07-10 task ID minted against an old number is re-mapped via those notes. From this report onward R1–R25 are project-global and **frozen again** as the `X` segment of task IDs `X.Y.Z.W`. **Notation:** every proposed numeric value is marked **(A) = assumption to confirm** — none exists verbatim in §1 or the BTC letter; the user may veto any of them (§4, F10). **Dependency** lines state what each requirement consumes — another requirement (Rx), external input, or a platform fact from the starter pack; *none* = self-contained.

---

## 2. Enumerated requirements & feasibility study

**Ordering rationale: urgency.** Contracts (R1–R4) block every track — contract-first is the retained planning shape from [milestone1.md](../.claude/plans/milestone1.md). Then the BTC-P1 demo chain in data-flow order: V2X ECU (R5–R9), bench/scenario layer (R10–R14), ADA (R15–R17), IVI (R18–R19). Then the P2 bonus layers (R20–R21), evidence infrastructure (R22–R24), and the end-to-end capstone (R25).

### Contracts

**R1 — V2X message contract: ETSI TS 103 324 CPM profile v1.0.** A versioned profile document of the Collective Perception Message: management info (`stationId`, reference time), originating-station container carrying B's WGS84 reference position + heading/speed, perceived-object container with ≥ 1 object (objectId, measurement-time offset, position **relative to B's reference position** with per-axis confidence, velocity, object classification with confidence, explicit reference-point semantics). The profile fixes optional-field usage, units, resolutions, and BTP destination ports (CPM port confirmed in the spike (A)). CPM carries B's pose *and* the perceived C in one message — no CAM required for geometry. DENM is the **named M2+ hazard-type family** entering through R7's extensible dispatch, not through this contract. *(Supersedes the round-1 JSON+pydantic contract shape; reaffirms CPM as family — the round-1 "Vanetza CPM outdated" objection is stale, see §3(a). Reasoned deviation from the BTC letter's DENM suggestion — F2.)*
- **Dependency:** none — [ETSI forge ASN.1](https://forge.etsi.org/rep/ITS/asn1/cpm_ts103324) is public.
- **Feasibility: achievable** (spec work, days).
- **KPI:** frozen by end of week 1 (A); mapping table traces 100% of profile fields to TS 103 324 field paths; golden-vector encode→decode round trip bit-exact **across both codec endpoints** (bench encoder vs V2X ECU decoder); 0 schema/range violations at runtime across all demo runs.
- Vague→precise: §1 "V2X data … carry inter-vehicle communication" → *the CPM profile above, the only V2X payload type in M1 besides R8's ego CPM (same profile)*.

**R2 — Wire encoding: staged JSON (P1) → ASN.1 UPER (P2) behind one codec seam.** Bench and V2X ECU share one codec interface. P1 ships versioned JSON whose keys are the R1 field paths (the JSON *is* the living mapping table). P2 upgrades to UPER using the **Vanetza ITS2 asn1 library as the single codec source** (§3(a)); a ≤ 3-day cross-decode spike in weeks 1–2 proves the path early (encode → decode bit-exact with the independent stack). Fallback, pre-ratified (F9): if the spike or the **week-5 (A) P2 upgrade gate** fails, M1 ships CPM-shaped JSON on the wire (BTC-sanctioned realism-for-time trade); the family never migrates mid-milestone.
- **Dependency:** R1.
- **Feasibility:** JSON **achievable**; UPER **achievable-at-risk** (new-to-team codec integration) — bounded by the spike + gate + fallback.
- **KPI:** JSON path operational end of week 2 (A); ≥ 3 golden messages cross-decoded bit-exact (A); codec swap = config change only, 0 code diffs outside the codec modules (binary).
- Vague→precise: BTC "realism of encoding" → *standard UPER decodable by an independent ETSI stack by the week-5 gate, else versioned JSON with the trade recorded (F9)*.

**R3 — Ego object/track contract (TrackedObject).** One language-neutral schema (JSON Schema + per-language bindings) shared by the ADA store, bench LOS sensor view, camera perception, and V2X-relayed entries: `id, class, source ∈ {own_sensor, v2x_relayed}, position (ego frame), distance, speed, confidence, state ∈ {not_tracked, tentative, tracked}, timestamps` (retains [milestone1.md §4](../.claude/plans/milestone1.md)'s shape). Standalone by design: the letter's mock↔perception swap guarantee hangs on this schema.
- **Dependency:** R1 (relayed entries map losslessly from the CPM profile).
- **Feasibility: achievable.**
- **KPI:** single schema file; CI round-trip in each consumer language; **swap test** — ADA runs unmodified against bench objects (R13) and camera objects (R21) (binary).

**R4 — ADA→IVI warning contract + data path (§1: *must be* developed).** Versioned JSON message set: (a) warning events `{schemaVersion, warningType (string registry), riskState, object snapshot (R3), composed geometry}`; (b) awareness state at 10 Hz (A) `{ego, B, C poses}`. Transport: UDP datagrams into the IVI app behind thin adapters both ends (D3 honored in the ego path — F4). Extensibility per §1 futures (criticality looked up from `warningType`). *(Retains the round-2 UDP+JSON shape — the one round-2 display pick that survives re-derivation; receiver re-derived for AAOS in §3(f).)*
- **Dependency:** R3; producer R17; consumer R18/R19.
- **Feasibility: achievable** (sockets + JSON both ends, days).
- **KPI:** ADA-emit → IVI-receive ≤ 50 ms p95 same-host / ≤ 100 ms p95 cross-node (A); delivery ≥ 99.9% (A) over a 10-min soak; state message ≤ 1 KB at 10 Hz ≈ ≤ 80 kbit/s (A); CI round trip; **extension tests:** a new warningType touches ≤ 1 registry file on the IVI (diff check) and an old app parses a newer message with unknown `warningType` gracefully (additive-version test).
- Vague→precise: §1 ADA "high-performance … condensed messages, low bandwidth, low latency" → *the payload/latency/rate figures above (all (A))*.

### V2X ECU (app-level Linux container)

**R5 — Adapter seam mirroring `IV2xRadio`/telux semantics.** The V2X application touches the radio only via `init · configure · subscribeRx · send` + `subscribeLocation` (R9); Rx subscription yields a readable socket delivering datagrams (telux parity — the letter: production Rx is already "read from socket"). Hackathon impl below the seam: UDP over the Ethernet pin. **Fidelity checklist (≤ 3 days total (A)):** documented lifecycle/readiness + error-model parity against telux `Cv2xRadioManager`/`Cv2xRadio` — capped to the **M1-used API subset** (cv2x + location surfaces actually called); CI import-isolation check; written telux port plan enumerating exactly which functions change on real hardware.
- **Dependency:** R1; starter pack (pin mechanics).
- **Feasibility: achievable** (≤ 1 week).
- **KPI:** app layer has 0 direct transport imports above the seam (CI check); impl swap (loopback-replay ⇄ bench) with 0 app-code diffs; parity doc + port plan committed; Rx callback delivers ≤ 5 ms (A) after socket arrival.
- Vague→precise: §1 "portability to different hardware — the developed layers move across hardware unchanged" → *the checklist above plus a language that can link telux (§3(d)) — anything less makes the node's focus goal claim-only*.

**R6 — Modem stub: config call flow + fail-inject.** In-node stub with FSM `idle → initialized → configured → rx-subscribed`; acks the app's init/configure/subscribe sequence (3GPP-flavored, simplified call flow to confirm with FPT-Mentor (A)); config-driven fail injection (init timeout, configure reject, subscription drop, mid-run radio loss) exercising the app's error handling.
- **Dependency:** R5.
- **Feasibility: achievable** (2–3 days).
- **KPI:** 100% of the scripted call flow acked, each step ≤ 100 ms (A); ≥ 3 (A) fail-inject cases each with a defined, logged recovery (retry/backoff/degraded state), demoable live via config flip / signal inspector.
- Vague→precise: §1 "configures the modem … simulated only" / control-plane messages "*could* be constructed" → *committed as the stub + call flow above (the letter names the modem stub as the config answer)*.

**R7 — Rx pipeline: decode → validate → dedupe → forward.** Adapter datagram → R2 decode → semantic validation (mandatory fields, ranges) → dedupe (stationId + objectId + measurement time) → map to R3 relayed object + B pose → JSON to ADA. Malformed/unknown input rejected + counted, never crashes. **Message-type dispatch is extensible** (messageId/BTP port): CAM/DENM arrive later as new codec modules — the entry door for §1's future hazard-type messages.
- **Dependency:** R1, R2, R3, R5.
- **Feasibility: achievable.**
- **KPI:** decode+forward ≤ 20 ms p95 (A) at 10 Hz; 100% golden vectors decoded; malformed corpus ≥ 20 cases (A) → 0 crashes, 100% rejected+logged over a 10-min impaired soak; adding a 2nd message type touches only a new codec module + 1 dispatch entry (diff check).

**R8 — Ego Tx: ego CPM of own-sensor B.** Ego constructs its own CPM (R1 profile) carrying its own-sensor perceived object (B, from ADA's R15 store) and broadcasts via `send` at 10 Hz (A) while B is tracked — implements §1's "receives information from the ADA ECU, constructs V2X message payloads, broadcasts them" literally, and completes the "extracting **and** constructing" input constraint. Ego CAM = stretch, never gating. *(Restored at round-3 rebuttal — contested point 15; a §1 responsibility neither position paper had covered as committed scope.)*
- **Dependency:** R1, R2, R5, R15.
- **Feasibility: achievable** (days — reuses the R2 codec).
- **KPI:** 10 Hz ± 10% (A) cadence measured at the bench while B is tracked; payload populated from the live store (no constants — spot check); frames appear in the R24 Wireshark evidence.

**R9 — Ego GNSS routing: bench → V2X ECU → ADA (user decision 2026-07-10).** Bench emits ego GNSS to the **V2X ECU**; the R5 adapter exposes it as a `subscribeLocation` subscription (mirrors production: the telux location subsystem lives with the modem — the letter itself notes production V2X apps take GNSS via telux); the V2X ECU forwards fixes to ADA. Wire + API shape: **telux-LocationInfo-shaped JSON fixes** (lat, lon, alt, speed, bearing, accuracy, timestamp) at 10 Hz (A) on a dedicated port (NMEA recorded as a conforming below-seam alternative — architecture's choice). **No IVI-side GNSS injection in M1** — the awareness view renders relative geometry (letter: no map needed); IVI injection becomes needed only if the real-coordinate map stretch (§3(i)) is taken. *(Diverges deliberately from the BTC reference sketch's "GNSS → IVI location stack" arrow — F13.)*
- **Dependency:** R5, R10; binding user decision.
- **Feasibility: achievable** — and more production-faithful than the letter's own blueprint sketch.
- **KPI:** 10 Hz ± 10% (A) fixes at ADA; bench-emit → ADA-receive ≤ 30 ms p95 (A); GNSS-stream loss ⇒ logged degraded-mode transition ≤ 1 s (A); binary — no location API used on the IVI node in the M1 build.

### Bench node — scenario player (single source of truth)

**R10 — Scenario player core: deterministic trajectory engine.** Team-built Python engine computes A/B/C ground truth per tick from declarative scenario config (speeds, gaps, C-approach profile, event times, admission distance); every bench output (R11 messages, R9 GNSS, R13 sensor view, R14 god view, R20 video timing) derives from the same trajectory table per tick. **C is V2X-silent** (a non-equipped vehicle — the use-case premise). Scenario coordinates **anchored at a real WGS84 road segment** (segment user-selectable in config (A)) — cost ≈ 0 now, keeps the future map option open, and ETSI absolute-position ranges validate cleanly.
- **Dependency:** starter pack (pin mechanics) for emission; engine itself self-contained.
- **Feasibility: achievable** (straight-line kinematics; letter: hand-computed trajectories suffice).
- **KPI:** same seed+config ⇒ byte-identical ground-truth log (hash compare); scenario switch = config only, 0 code changes; ≥ 3 (A) variants (baseline, C-braking, impaired); tick ≥ 20 Hz (A); 0 tunables hardcoded (config lint — constitution rule 5).
- Vague→precise: §1 "V2X message playback across different scenarios" → *config-driven scenario set + the determinism KPI above*.

**R11 — Bench V2X emission (B's CPM).** From ground truth the bench emits B's CPM (R1/R2) at 10 Hz (A) **only while C is inside B's simulated perception gate** (R15 constants); B CAM = stretch, never gating.
- **Dependency:** R1, R2, R10.
- **Feasibility: achievable.**
- **KPI:** cadence 10 Hz ± 10% (A); relay starts/stops with C entering/leaving B's gate per ground truth (event-log cross-check); 0 emissions outside the scripted window (binary).
- Vague→precise: §1 "simulates decoded V2X messages arriving at different rates" → *encoded wire-format messages at configurable rates (rate multipliers in scenario config); "decoded" superseded by the R2 seam — bench emits wire format, ego decodes, closer to production than pre-decoded injection*.

**R12 — Impairment engine (in the scenario player).** Drop/delay/jitter applied to emitted V2X messages per config (loss %, delay, jitter distribution, seed) **before** emission — BTC's recommended placement; reproducible, no `tc netem`/NET_ADMIN dependency. *(Supersedes round-1's tc-netem plan.)*
- **Dependency:** R11.
- **Feasibility: achievable** (days).
- **KPI:** configured 30% loss measures 30% ± 3 pts over 10 min (A); identical drop pattern per seed (binary); P3 degradation demo: R17's warning KPIs hold at ≤ 30% loss + 200 ms jitter (A).

**R13 — LOS-filtered ego sensor view (premise keeper).** Bench synthesizes ego's own-sensor object list from ground truth filtered by line-of-sight: contains B, **never C while occluded**. Delivered to ADA as R3 objects `source = own_sensor` (direct injection; the optional Cortex-M mock node is omitted — F12, architecture may restore the topology).
- **Dependency:** R3, R10.
- **Feasibility: achievable.**
- **KPI:** **binary — 0 frames across all demo runs with C in the ego sensor list while ground-truth-occluded** (automated check vs truth log); B present ≥ 99% of ticks (A); LOS filter unit-tested on ≥ 5 (A) geometry cases; schema-identical to R21 output (shared schema test).
- Vague→precise: §1 "A's own camera can never detect C" → *enforced at the sensor-view source by the binary check above, re-verified on the R25 demo run* (the letter calls a naive mock here use-case-destroying).

**R14 — God view + jury dashboard (bench-served).** Browser page served from the bench container: live 2D top-down god view of A/B/C from ground truth (occlusion shading) + risk-state panel fed by ADA pushes; placed split-screen beside the IVI for the jury. Observer tooling, rank-2 output by design (letter §7) — never the product.
- **Dependency:** R10; R17 (risk push); F4 (D3 scope).
- **Feasibility: achievable** (≤ 1 week).
- **KPI:** view updates ≥ 5 Hz (A) in-browser; risk overlay matches the ADA event log 100% in replay comparison; fully offline assets (no external tiles/keys); one-command start with the bench node; zero WebSocket usage (SSE/polling — §3(h)).

### ADA ECU (app-level Linux container)

**R15 — Track store + admission state machine.** R3-shaped store; per-source admission: `own_sensor` (B) via `not_tracked → tentative → tracked` with confirm hits; `v2x_relayed` (C) admitted on relayed receipt, expired on message-stop (miss window); distance gate with hysteresis — `gate_enter` 30 m, `gate_exit` 35 m, N = 3, M = 5 (all (A), externalized config).
- **Dependency:** R3; inputs R7, R13.
- **Feasibility: achievable.**
- **KPI:** full lifecycle observable in logs per §1's state diagram; 0 admit/drop oscillations under a σ = 2 m (A) noise sweep across 30–35 m; C's track carries `source = v2x_relayed` only (binary — feeds R25); gate constants absent from code (config lint).

**R16 — Collision Risk Assessment abstraction (extensible — §1 demand).** Risk logic behind a scenario-plugin interface consuming the store + ego state and emitting typed warnings (R4 registry). M1 ships one realization (R17); §1's future scenarios (intersection hazards, curve blind spots, speed-scaled thresholds) plug in as new realizations.
- **Dependency:** R3, R4.
- **Feasibility: achievable.**
- **KPI:** **0-diff plug-in test** — a stub second scenario realization registers and emits a distinct warningType with 0 diff lines outside its own module + registry entries (binary); M1 realization selected by config; interface doc committed.
- Vague→precise: §1 "future warning scenarios can be added as extensions without reworking this code" → *the 0-diff test above*.

**R17 — NLOS obstruction warning + TTC risk engine (the M1 realization).** Fuses `v2x_relayed` C, `own_sensor` B, ego GNSS. Geometry: `d_AC = d_AB ⊕ d_BC` in the ego frame — **d_BC from the CPM relative perceived object; d_AB from ego own-sensor distance to B while B is tracked, else GNSS-differenced against B's CPM reference position (precedence explicit — (A))**. Policy: warn when d_AC ≤ gate (R15) **or TTC ≤ 3.5 s (A)** — the OR-trigger is **held for user ratification (F18)**: §1 states a fixed distance threshold, the TTC branch is a policy addition; strike to distance-only if vetoed. Risk states from a config registry (proposed `none/caution/warning`, count (A)) with hysteresis; TTC is an internal engine (letter) — risk state reaches IVI/dashboard, raw TTC goes to R23 logs; every transition appends to the collision-risk event list.
- **Dependency:** R9, R15, R16.
- **Feasibility: achievable.**
- **KPI:** warning ≤ 200 ms (A) after the qualifying message reaches ADA; **0 false warnings** on a C-absent run; composed d_AC within ±(0.15·d_AC + 2.0 m) (A) of ground truth for ≥ 90% of samples (round-1 bound carried — now a pipeline-consistency check); TTC error ≤ 10% (A) at closing speed ≥ 1 m/s; 0 risk-state oscillations under the R12 demo impairment profile; thresholds in config, not code.
- Vague→precise: §1 "fixed distance threshold; a future milestone should scale it with traffic speed" → *threshold(+TTC (A)) as an externalized policy object; speed-scaled policy = a new R16 realization*.

### IVI ECU (provided AAOS full vECU)

**R18 — IVI HMI shell app on the AAOS node.** Kotlin/Compose app (§3(e)): ≥ 2 buttons (A — proposed 2D/3D toggle + demo reset, set to confirm) + central view; warning-driven `idle ⇄ warning-view` state machine on R4 events; **app switching** (§1 cloud-table goal): user switches to another head-unit app, an incoming warning surfaces as a heads-up notification, tap returns to the awareness view. Multi-process wake-on-warning deferred per §1's own wording (F15). **Foundation obligation (user decision 2026-07-12 — multi-process wake-on-warning is a mandatory future feature):** the warning/awareness view sits behind an **exported intent entry point** carrying the R4 event as serialized extras, the R4 ingest lives in a UI-free module, and no in-process singletons are shared between shell and view; the M1 internal `idle ⇄ warning-view` transition routes through the same entry point.
- **Dependency:** R4; starter pack (AAOS image API level, install path — (A): sideload permitted).
- **Feasibility: achievable** (2.5–3 weeks incl. Kotlin ramp — schedule-riskiest committed item; starts week 1).
- **KPI:** warning-receipt → view switch ≤ 300 ms (A); button response ≤ 100 ms (A); auto-start with the node ≤ 30 s (A); app-switch demoed (background → heads-up → return ≤ 2 s (A)); binary — runs on the AAOS node viewed in-browser, not a dev desktop; **foundation checks:** view module has 0 compile-time imports from the shell module (CI dependency check), and the warning view launches standalone via `adb shell am start` with an R4-event extra (binary).
- Vague→precise: §1 "multi-process applications *should* … *could* be deferred" → *M1 = single app + heads-up pattern; multi-app wake = registered deferral (F15)*.

**R19 — Awareness view: 3D god view committed-with-gate, 2D floor, 2D⇄3D switching.** The central view renders the 3-vehicle relative-geometry scene from R4 state only (no map, no bench feed): ego + B + **ghost C** (`v2x_relayed` styling, occlusion cue, numeric range). Per §1's IVI focus goals + demo table (3D = 10, 2D = 8): 3D committed behind a **two-stage gate** — stage 1 feasibility spike (1–2 day SceneView probe APK) by **end of week 3 or AAOS access + 2 weeks, whichever is later (A)**; stage 2 week-6 (A) polish/ship checkpoint; gate failure ships the committed 2D floor (the 2D side of the required toggle). Renderer behind a **view-interface seam** (binding design obligation — carries the toggle, any fallback swap, and §1's future camera-feed central view). *(Supersedes round-2's "2D committed / 3D stretch" posture — §1's cloud table promotes the 3D god view to a focus goal — F5.)*
- **Dependency:** R4, R18; the gate.
- **Feasibility:** 2D **achievable**; 3D **achievable-at-risk** (virtual-AAOS GPU path unknown — SwiftShader possible; the gate bounds it).
- **KPI:** ghost C appears ≤ 2 s (A) after the scenario's first qualifying relay (judge-visible); marker update ≤ 500 ms (A); **C rendered only from `v2x_relayed` source** (binary); 2D⇄3D toggle ≤ 300 ms (A); FPS ≥ 15 target / ≥ 10 floor (A) at node resolution; scene state sourced solely from R4 (binary).
- Vague→precise: §1 "3D display: god view … every instance displayed" → *3 vehicle entities + ghost styling at composed positions, KPIs above*; "2D ⇄ 3D view switching" → *in-seam toggle ≤ 300 ms (A)*.

### P2 bonuses (fusion story — timeboxed, never gating P1/P3)

**R20 — Ego video display (provided clip, display-only).** The provided ego-POV clip (B occluding ahead) plays on the demo surface (IVI secondary view vs dashboard panel — architecture's choice (A)), timeline-aligned to the scenario — §1 demo-table "video feed on A's IVI HMI" (score 8; the provided file, not live camera — D7's deferral untouched). Includes §1's input-constraint action: a written video-spec proposal (container/codec/fps/resolution/capture conditions) to FPT-Mentor by end of week 2 (A).
- **Dependency:** video source (external — F11); R10 (timeline).
- **Feasibility: achievable**, timeboxed ≤ 1 week (A).
- **KPI:** playback ≥ 24 FPS (A); scenario alignment ± 200 ms (A); spec proposal delivered (binary).
- Vague→precise: §1 "video format … *should be studied and proposed*" → *the week-2 proposal deliverable*.

**R21 — Camera perception SKU (pretrained, CPU-only).** YOLO11n ONNX-CPU (§3(g)) runs on the ego clip, detects **B** (the visible occluder; C is by definition not in ego's frame), and emits an object list **interface-identical to R13** — powering the fusion story ("camera sees B, V2X knows C behind B") as overlay/cross-check, never the gating ADA source. Timeboxed ≤ 2 weeks (A); **no GPU requested** (F16).
- **Dependency:** R3 (interface), R20 (clip), R15 (store).
- **Feasibility: achievable as a bonus; explicitly non-critical-path** (letter: timebox it).
- **KPI:** B detected in ≥ 90% (A) of in-range frames; mean inference ≤ 100 ms/frame CPU (A) (cached YOLO11n benchmark: 56 ms); **0 detections labeled C** (premise binary); swap test — ADA consumes R21 output through the R3 interface with 0 ADA code change.
- Vague→precise: §1 "a pre-trained model *should be used* to detect the obstruction (vehicle C)" → *detector detects the occluder B in ego video; B-side "detection of C" is scenario-synthesized (R13) — §1's wording predates the single-ego blueprint; interpretation recorded for user veto*.

### Evidence & capstone

**R22 — Cross-node timebase & latency measurement.** Bench stamps generation time in-message (single time source of truth); node clock offsets measured per run via UDP echo (median of N). Assumption (A): CarSky containers share the host clock (offset ≈ 0) — verify with BTC. Contingency if |offset| > 50 ms (A) or nodes span hosts: **escalate to BTC for platform/host-level sync, or apply the measured per-run offset as an analysis-time correction** (containers likely lack `CAP_SYS_TIME` — no per-node chrony daemon). *(Supersedes round-1's chrony/gpsd KPI set.)*
- **Dependency:** platform facts (A).
- **Feasibility: achievable.**
- **KPI:** measured |offset| logged every demo run, expected ≤ 20 ms (A); every latency KPI in this report computable from R23 logs alone (binary), quoted with the run's offset uncertainty.

**R23 — Evidence logging.** Structured JSONL event logs on V2X ECU + ADA (message rx/tx, decode results, track transitions, TTC values, risk events) → the collision-risk event list (§1 demo score 7); annotated video export (TTC overlay + risk label — score 6) as the designated drop-first item at P2+.
- **Dependency:** R7, R17, R22; R20/R21 (annotated export only).
- **Feasibility: achievable.**
- **KPI:** event list reconstructs the full demo timeline offline with 100% event correspondence vs bench truth (replay-compare tool); E2E bench-emit → IVI-warning-render ≤ 1 s p95 unimpaired (A); annotated export produced for ≥ 1 run (A).

**R24 — Demo packaging.** The jury experience per §1's demo table + the letter's "điểm nhấn": **split screen — god view (R14) beside the IVI** — capturing the moment the IVI warns of C before anything is visible. Wireshark evidence (score 7): **offline scapy re-encapsulation** of the actual transmitted UPER bytes into Ethernet/GN/BTP frames → stock Wireshark dissects natively — with integrity amendments: original pcap + re-encapsulated pcap + per-frame UPER-byte hash manifest, original timestamps preserved, one disclosure line in the demo script; dissection verified in the spike week (CPM dissector bug [#19886](https://gitlab.com/wireshark/wireshark/-/work_items/19886)); JSON-fallback mode shows the UDP/JSON pcap (honestly lower realism). Live on-wire GN/BTP framing = stretch note only. Scripted, rehearsed run + recorded backup.
- **Dependency:** R14, R19, R23; R2 (encoding mode).
- **Feasibility: achievable.**
- **KPI:** rehearsed script executes e2e ≤ 10 min (A) with 0 operator improvisation; pcap CPM fields match the R1 contract in Wireshark's dissection tree (binary); recorded backup exists (binary).

**R25 — End-to-end definition of done.** One continuous recorded run on the demo impairment profile (A), full chain live (bench → V2X ECU → ADA → IVI), no scripted shortcuts inside ego software: (a) ego sensor view (R13) and detector log (R21, if present) contain **zero** C entries for the entire run; (b) the IVI shows the obstruction warning + ghost C sourced `v2x_relayed` ≤ 2 s (A) after the warning-condition onset; (c) the god-view split screen visibly shows C occluded while the IVI warns; (d) the P1 chain demoable in isolation and each later layer independently demoable (the letter's non-collapsing phase structure, adopted as the DoD shape).
- **Dependency:** all committed requirements above.
- **Feasibility: achievable** — mechanical integration once the R2/R19 gates resolve.
- **KPI:** binary pass/fail on the recorded run + all committed per-requirement KPIs ticked.

### Feasibility study result

**Whole-input verdict: ACHIEVABLE** for §1's full M1 scope on CarSky within the ~2-month window (letter 09/07 → assumed deadline ~2026-09-10 (A), ≈ 9 weeks), **conditioned on**: (1) platform access + starter pack landing in weeks 1–2 (external — everything CarSky-touching integrates only after it; contracts and ego logic build against loopback stubs meanwhile); (2) the two gates passing or their pre-ratified fallbacks firing cleanly — R2 codec spike/week-5 UPER gate and R19's two-stage 3D gate; (3) the week-0 language skill gates (F6, F19) resolving. The BTC P1 chain is a complete demo by itself; P2/P3 are additive and independently demoable — later-phase slippage degrades the proposal, never collapses it. **Critical path:** platform access → pin integration → P1 chain e2e (weeks 3–4) → IVI HMI/3D → packaging + rehearsal. **Slack ≈ 1.5–2.5 weeks, of which the F19 hybrid election prices in ≈ 1–2** (its week-5 revert-to-Python fallback restores the slack if fired); slack items (timeboxed, never gating): R20, R21, ego CAM, live GN/BTP framing, annotated export, dashboard polish.

| R# | Depends on | Verdict | Note |
|---|---|---|---|
| 1 | none | achievable | spec work; forge ASN.1 public |
| 2 | R1 | JSON achievable / UPER at-risk | spike + week-5 gate + pre-ratified JSON fallback (F9) |
| 3 | R1 | achievable | schema + CI round-trips |
| 4 | R3, R17, R18/19 | achievable | stdlib sockets + JSON both ends |
| 5 | R1, starter pack | achievable | checklist capped ≤ 3 days (A) |
| 6 | R5 | achievable | 2–3 days |
| 7 | R1–R3, R5 | achievable | dispatch = the DENM-at-M2 entry door |
| 8 | R1, R2, R5, R15 | achievable | days; reuses the codec |
| 9 | R5, R10 | achievable | user decision; production-faithful routing |
| 10 | starter pack (emission only) | achievable | hand-computable kinematics |
| 11 | R1, R2, R10 | achievable | gate-windowed relay |
| 12 | R11 | achievable | player-side, reproducible |
| 13 | R3, R10 | achievable | premise keeper — binary zero-C KPI |
| 14 | R10, R17 | achievable | rank-2 output by design |
| 15 | R3, R7, R13 | achievable | hysteresis unit-tested |
| 16 | R3, R4 | achievable | 0-diff plug-in test |
| 17 | R9, R15, R16 | achievable | TTC OR-trigger held (F18) |
| 18 | R4, starter pack | achievable | schedule-riskiest committed item — starts week 1 |
| 19 | R4, R18 | 2D achievable / 3D at-risk | two-stage gate; 2D floor in-seam |
| 20 | video source, R10 | achievable (bonus) | timeboxed ≤ 1 week (A); F11 |
| 21 | R3, R15, R20 | achievable (bonus) | timeboxed ≤ 2 weeks (A); CPU-only |
| 22 | platform facts (A) | achievable | contingency = escalation/correction, not chrony |
| 23 | R7, R17, R22 | achievable | annotated export drop-first |
| 24 | R2, R14, R19, R23 | achievable | scapy re-encap + integrity manifest |
| 25 | all committed | achievable | binary DoD |

## 3. Technical solution analysis

Hard-constraint screening precedes every comparison (open-source only, Linux-targeted per [solution-selection-criteria.md](../.claude/rules/solution-selection-criteria.md)). Environment notes: the provided AAOS node is Linux-kernel and platform-provided; CarSky is sanctioned competition infrastructure, not a team dependency choice.

### (a) V2X message family + encoding — serves R1, R2, R7, R8, R11

- **Family candidates:** SAE J2735 BSM — **disqualified** (paywalled spec = unauditable; Part I carries no perceived objects; the SAE analogue SDSM is equally paywalled — round-1 precedent). Survivors: ETSI CAM+DENM vs ETSI CPM TS 103 324.
- **Comparison (C1–C4):** C1 — parity: Vanetza master ships **release-2 codecs incl. CPM TS 103 324 v2.1.1** ([cache](../.claude/references/vanetza-its2-release2-cpm-ts103324.md); issue #194 closed) — the round-1 "Vanetza CPM outdated" objection is stale. C2 — CPM wins message-count economics: one message carries B's reference position + relative perceived C (one codec/profile/golden-vector set); CAM+DENM needs two. Semantics — §1's "broadcasts *that perception*", relative composition (DENM's absolute `eventPosition` would smuggle in the deferred absolute method), classification/confidence fields native. C3 — split honestly: multi-object + single-message aggregation futures are CPM-native; §1's hazard-type list is DENM's causeCode territory and enters at M2+ through R7's dispatch **regardless of the M1 pick**.
- **Pick: CPM TS 103 324 v2.1.1** (drivers: C1 parity + C2 single-message + §1 semantics); **DENM = named M2+ hazard family**; ego Tx uses the same profile (R8). Reasoned deviation from the BTC letter's DENM suggestion — F2. *(Reaffirms round-1's CPM target; supersedes Alpha's round-3 CAM+DENM opening position — conceded on verified evidence, record point 12.)*
- **Encoding candidates:** asn1tools — **disqualified for CPM** (no X.681 CLASS / X.683 parameterization, verified — [cache](../.claude/references/asn1tools-ioc-parameterization-limits.md)); raw asn1c — redundant (Vanetza embeds maintained asn1c output); pycrate — IOC-capable but no precompiled CPM (compile spike; [cache](../.claude/references/pycrate-etsi-its-asn1-modules.md)); Vanetza ITS2 asn1 targets (LGPLv3, standalone without the GN/BTP stack); JSON (letter-sanctioned).
- **Pick: staged JSON (P1) → UPER (P2) behind one codec seam; Vanetza ITS2 as the single codec source** (bench encode shape — resident emitter daemon vs pre-encoded UPER replay CLI — both conforming, **delegated to [[project-architecture]]**); pycrate only on the F6 veto chain; fallback CPM-shaped JSON for M1. Drivers: C1 (one proven codec, golden vectors can't drift), C2 (P1 off the codec critical path), C4 (asn1-only targets).

### (b) Transport, adapter seam, modem stub — serves R5, R6

- **Candidates:** thin team adapter over UDP (BTC default) vs telux porting into the container (real SKU; letter warns infra eats demo budget) vs generic middleware (ZeroMQ/MQTT) under the seam.
- Middleware screened out on fidelity (inserts semantics production doesn't have; the letter's point is production Rx is already "read from socket"); telux port declined for M1 (F17).
- **Pick: thin adapter + fidelity checklist** (socket-fd Rx, M1-subset telux parity doc, CI import isolation, written port plan — capped ≤ 3 days (A)) + in-node modem stub with FSM + fail-inject. Drivers: C2 dominant (letter: least work, enough for demo), C1, C3 preserved (the checklist keeps the telux swap mechanical).

### (c) Scenario player build — serves R10–R14

- **Candidates:** self-written Python trajectory engine; MetaDrive as engine (Apache-2.0, [cache](../.claude/references/metadrive-license-topdown.md)); CARLA (GPU-heavy vs the shared GPU budget — rejected C1/C2); SUMO (no LOS/POV concept — rejected C2).
- **Pick: self-written engine; the MetaDrive starter sample mined for pin-integration code only** (the letter's own guidance). Ground-truth consistency is architectural: one trajectory table per tick feeds every output. Drivers: C1 (deterministic, no engine unknowns), C2 (days), C4 (zero engine dependency); C3 — the letter calls the scenario layer a reusable SKU for every future V2X use case, which a config-driven engine is.

### (d) Languages per track — serves all

- **V2X ECU: C++17, skill-gated (F6).** Drivers: §1 makes portability the node's focus goal and telux is a C++ API — only C++ keeps "developed layers move across hardware unchanged" code-true (the letter: "same code, same logic — the evaluated part"; SKU (a) "carries straight to the real product"); Vanetza codec locality (a Python ECU would need an FFI/sidecar, erasing Python's C2 edge). Conditions: week-0 gate (named owner + 1-day socket+decode hello-world on the actual toolchain), scope cap (adapter/stub/decode/validate/ego-Tx (R8)/forward/GNSS only), veto chain explicit — **Python ⇒ CPM-UPER rests on an unverified pycrate compile ⇒ JSON wire near-certain**. *(Supersedes rounds 1–2 mono-Python — the portability goal was re-premised by §1's cloud table.)*
- **ADA: C++17 core + Python detector-as-callable-binary (user-elected hybrid, 2026-07-12 — [study](m1-ada-dual-language-study.md)).** C++17 implements the [ada-ecu.svg](ada-ecu.svg) DataObserver (all listeners), Data Parser (thin R3 validator — CPM decode stays in R7), R15 store, R16 CRA «interface» + R17 Chained Collision realization, R23 logging, R4/R8 emission; Python implements **R21 only**, as a long-running subprocess (venv or PyInstaller-onedir) streaming R3 TrackedObject JSONL over stdout. **Boundary clarification: no FFI (no pybind11/ctypes/embedded interpreter) and no cross-language API call (no RPC) is needed** — the only cross-language surface is the process-level contract (argv + exit codes + R3 JSONL), language-neutral by R3's design. Skill-gated per F19. *(Supersedes the round-3 mono-Python pick — §1's KPIs are met by either language; the election trades ≈ 1–2 weeks of slack for the C++ core, priced honestly in F19 with a pre-ratified revert.)*
- **Bench: Python** (+ the (a) C++ encode component per architecture's shape choice). **IVI: Kotlin** (§3(e)). Cross-language drift contained by versioned contracts (R1/R3/R4) with CI round-trips.

### (e) IVI HMI framework on AAOS — serves R18, R19

- **Candidates:** Kotlin + Jetpack Compose (+ SceneView/Filament 3D — Apache-2.0, Compose-native, maintained; [cache](../.claude/references/sceneview-filament-android-3d.md)); Flutter-on-Android; web-in-browser (fails D3 + weak app-switching — screened); Qt-for-Android (JNI/cross-compile friction, no AAOS idiom gain — rejected C2).
- **Pick: Kotlin + Compose; 3D via SceneView; 2D Compose-canvas floor behind the view seam; Flutter-on-Android pre-named framework fallback** (fires only if the Kotlin ramp fails early — then 2D-only). Drivers: C1 on the now-committed 3D axis (Flutter 3D remains paused — round-2 caches unchanged), C2 (native toolchain of the provided node; no engine layer; app switching/notifications first-class), C3 (the seam carries §1's future toggle/themes/camera-feed view). Both round-3 papers converged on this independently. *(Supersedes the round-2 Flutter pick and its Slint fallback + week-4 gate — the IVI target was re-premised by the BTC letter (provided AAOS node, not a team-built Linux vECU); reverses user decisions D4-set candidates — F3.)*

### (f) ADA→IVI data path — serves R4, R18

- **Candidates:** UDP + versioned JSON into an Android foreground service (`DatagramSocket`); TCP stream (trivial swap if soak KPIs demand); gRPC (codegen + streaming surface unneeded — rejected C2/C4); MQTT (broker infra — rejected C4); WebSocket (banned by D3 — kept); AAOS VHAL property injection (starter-pack-unknown — exploration note only, C1 risk).
- **Pick: UDP + versioned JSON**, state (10 Hz, last-value-wins, sequence numbers) + edge-triggered warning events. Drivers: C1 + C2 + C4; the round-2 rationale survives the receive-end swap intact.

### (g) Perception stack — serves R20, R21

- **Candidates:** YOLO11n ONNX-CPU (AGPL-3.0; 56 ms/frame CPU — [cache](../.claude/references/yolo11-cpu-inference-benchmarks.md)); YOLOX-s (Apache-2.0) pre-named license fallback (F7); SSD-MobileNet (weaker accuracy — rejected C1).
- **Pick: YOLO11n CPU-only, timeboxed;** detections-from-scenario (R13) remains the ADA acceptance-path source in every phase; camera detections complement via the identical R3 interface (the letter's GPU-free fusion pattern). No GPU requested (F16). *(Supersedes rounds 1–2's vision-critical-path role — B's perception is bench-synthesized in the single-ego blueprint.)*

### (h) Dashboard / god-view delivery — serves R14

- **Candidates:** bench-served page + SSE/polling (BTC's suggested shape); WebSocket push (keeps a banned tech alive for no gain — rejected); MapLibre map page (meaningless without the real-coordinate demo — deferred, kept open by the WGS84 anchor); native window streamed (clunky in a browser-operated cloud — rejected C2).
- **Pick: bench-served page, SSE/polling, own 2D canvas, offline assets, no map.** Drivers: C1 + C2 + C4. D3 scope: the page's browser JS is bench-side jury tooling — F4.

### (i) GNSS feed mechanics — serves R9

- **Candidates:** telux-LocationInfo-shaped JSON fixes (pick); NMEA sentences (conforming below-seam alternative — architecture may choose; production parses no sentences, so parity lives at the `subscribeLocation` surface); gpsd chain (extra daemon for zero consumer benefit — rejected C4, stale round-1 machinery).
- **Pick: telux-shaped JSON fixes end-to-end, 10 Hz (A)**, routing bench → V2X ECU → ADA per the 2026-07-10 user decision (jointly endorsed — the letter itself notes production V2X apps take GNSS via telux); **no IVI location in M1**; scenario WGS84-anchored (F13). Drivers: C1 + C2, production-faithful.

### (j) Timebase & latency measurement — serves R22

- **Candidates:** bench-authoritative in-message timestamps + per-run measured offset (pick); chrony mesh (containers likely lack `CAP_SYS_TIME`; solves a problem the design doesn't have — rejected C2/C4, supersedes round-1); PTP (sub-µs for a ≤ 20 ms need — rejected C4).
- **Pick: bench timestamps + UDP-echo offset per run; contingency = BTC platform-level escalation or analysis-time offset correction.** Drivers: C1 + C2; every KPI computable from logs alone.

### Stack summary (per track)

| Track / node | Language | Key components | Notes |
|---|---|---|---|
| V2X ECU (app-level container) | C++17 (skill-gated, F6; Python veto path defined) | adapter seam (`IV2xRadio`-mirror) + modem stub + R2 codec seam (CPM decode/encode — JSON P1 → Vanetza ITS2 UPER P2, F9 fallback) + JSON forward; ego CPM Tx (R8); `subscribeLocation` GNSS forward | portability = §1 focus goal; scope-capped |
| ADA ECU (app-level container) | C++17 core (skill-gated, F19) + Python 3.11 detector subprocess | track store + CRA plugin abstraction + NLOS/TTC realization + evidence logging + R4/R8 emission (C++); YOLO11n ONNX-CPU JSONL detector (Python, R21 timeboxed bonus, off P1 path; YOLOX-s fallback F7) | detector boundary = R3 JSONL over stdout — no FFI, no cross-language API |
| IVI (provided AAOS full vECU) | Kotlin | Jetpack Compose shell + SceneView/Filament 3D + Canvas 2D floor + UDP foreground service | Flutter-on-Android pre-named fallback; candidate R20 clip surface (vs dashboard — architecture's choice) |
| Bench (team-built container) | Python (+ C++ encode component per architecture) | scenario engine + CPM emission + impairment + LOS filter + GNSS feed + god-view/dashboard (SSE) + R22 timebase (in-message timestamps + UDP-echo offset) | single source of truth; MetaDrive sample = pin reference |
| Contracts / CI | JSON Schema + ETSI ASN.1 | R1 CPM profile, R3 TrackedObject, R4 warning message; golden vectors; round-trip + diff-check tests | golden vectors are the conformance oracle |
| Demo evidence | — | scapy GN/BTP re-encapsulation + hash manifest; Wireshark; recorded runs | disclosure line mandatory (R24) |

## 4. Flags for user ratification

**Ratification pass (user, 2026-07-12): all flags F1–F19 below are approved as recorded.** Staleness review: no flag is irrelevant to the current design — each remains referenced by live requirements — so none is removed; F15 alone no longer fit as worded (plain deferral) and is replaced per [the verification note](m1-node-toolset-support-verification.md); F18's OR-trigger is approved, not struck. The flags stay listed as the decision record the report cites throughout.

1. **F1 — Numbering re-enumeration executed** (R1–R25); ratify the replacement Numbering text in §1 (§ Numbering).
2. **F2 — Message family = CPM TS 103 324**, a reasoned deviation from the BTC letter's DENM suggestion; DENM recorded as the M2+ hazard family via R7's dispatch. Joint position after Alpha's evidence-based concession (record point 12).
3. **F3 — IVI framework reversal: Flutter → Kotlin/Compose + SceneView** (reverses the round-2 ratified pick and its D4 candidate set; Flutter-on-Android = pre-named fallback). No Dart code exists, so the write-off is zero.
4. **F4 — D3 ("no JS, no WebSocket") re-scoped:** binary ban kept for the **ego software path** (V2X ECU, ADA, IVI app); the bench jury dashboard is a browser page (JS) using SSE/polling — no WebSocket anywhere. Extending the ban to the bench forces a clunkier streamed-window dashboard (C2 cost).
5. **F5 — 3D raised from stretch to committed-with-gate** (amends round-2 F12 posture): two-stage gate — spike by end of week 3 or AAOS access + 2 weeks (whichever later, (A)); week-6 (A) ship checkpoint; FPS ≥ 15 target / ≥ 10 floor (A). Ratify dates + bars.
6. **F6 — V2X ECU language = C++17, skill-gated:** week-0 gate (named owner + 1-day toolchain hello-world), scope cap (adapter/stub/decode/validate/ego-Tx (R8)/forward/GNSS only). **Veto chain stated honestly: choosing Python ⇒ Python-side CPM UPER rests on an unverified pycrate compile ⇒ the JSON wire fallback becomes near-certain** — vetoing C++ likely costs UPER realism too.
7. **F7 — YOLO11n is AGPL-3.0** (fine under open-source-only; copyleft caveat for productization); **YOLOX-s (Apache-2.0) pre-named fallback**. Decide before R21 starts.
8. **F8 — Vanetza is LGPLv3** — now on the primary codec path (not just a fallback); dynamic linking keeps the repo clean. Pre-approve so neither the codec nor the fallback stalls.
9. **F9 — Encoding staged JSON→UPER:** ≤ 3-day cross-decode spike (weeks 1–2), P2 upgrade gate week 5 (A); failure ⇒ M1 ships CPM-shaped JSON (BTC-sanctioned) and the Wireshark evidence weakens to UDP/JSON. Ratify the trade in advance.
10. **F10 — Every numeric KPI is (A)** — an assumption to confirm; none exists verbatim in §1 or the letter; veto any individually.
11. **F11 — Video source dependency:** R20/R21 need a clip; the team owes FPT-Mentor a format spec (end of week 2 (A)); if none is provided, the team produces its own matching clip (small scope add). Ratify the self-production fallback.
12. **F12 — Cortex-M mock node omitted** (BTC offers mock-or-omit; §1 already excludes Cortex-M development): LOS object list injects directly to ADA; architecture may restore the mock-node topology without contract change.
13. **F13 — GNSS routing per the 2026-07-10 user decision adopted** (bench → V2X ECU → ADA, telux-mirroring — jointly endorsed as more production-faithful than the letter's IVI-injection sketch); **no IVI location injection in M1**; scenario anchored at real WGS84 coordinates (segment user-selectable (A)) so a future map never forces scenario re-authoring. Any future real-coordinate map reopens IVI GNSS injection as a scope add.
14. **F14 — Single-ego blueprint confirmed;** the multi-real-car variant declined per BTC's own cost/value warning (forecloses a two-IVI demo).
15. **F15 — Multi-process IVI front end: implementation deferred; end-state MANDATORY** (upgraded from a nice-to-have deferral by user decision 2026-07-12). M1 ships the R18 foundation obligation so the split is a refactor, not a rewrite; the M1-visible slice remains app switching + heads-up warning. Wake mechanism recorded: foreground-orchestrator `startActivity` (unprivileged, primary); home-as-launcher exemption or privileged `START_ACTIVITIES_FROM_BACKGROUND` as fallbacks; HUN tap as the user-in-loop path.
16. **F16 — No GPU requested from BTC** — locks CPU-only perception and display-only video; reopening later competes for a shared budget and needs early BTC notice.
17. **F17 — telux-porting SKU declined for M1** (BTC option (b)); the R5 seam + port plan keep it open as an optional future SKU.
18. **F18 — TTC OR-trigger HELD:** §1 fixes *distance* as the M1 criterion; R17's "or TTC ≤ 3.5 s (A)" is a policy addition that makes the TTC engine visibly useful. **Approved 2026-07-12 — the OR-trigger stands.**
19. **F19 — ADA core language = C++17 (user-elected hybrid, 2026-07-12):** week-0 skill gate mirroring F6 — named ADA C++ owner **distinct from the F6 V2X owner** (or explicit user acceptance of shared-owner risk), 1-day toolchain hello-world (UDP rx + JSON parse + one gtest); scope cap = R15/R16/R17/R23/R4-emission; the R21 detector stays Python behind the subprocess JSONL boundary in long-running streaming mode — **no FFI, no cross-language API call needed** (process-level contract only). Fallback pre-ratified: gate failure or R15–R17 not KPI-green by end of week 5 (A) ⇒ ADA reverts to mono-Python — R3/R4 make the reversal contract-loss-free.

**Prior user decisions — disposition record:** D1 (B-side display dropped) — upheld, now moot (B is bench-simulated). D2 (GUI = real app on the IVI vECU) — upheld, retargeted to the provided AAOS node. D3 (no JS/WebSocket) — kept, re-scoped (F4). D4 (GUI candidate set {Slint+Rust, React, Flutter}) — superseded; candidates re-derived for AAOS (F3). D5 (3D→2D→aesthetics focus) — upheld, now aligned with F5. D6 (buttons + central view + warning-triggered 3-car view) — carried into R18/R19. D7 (live camera feed = future) — upheld; R20 renders the provided file only.

## 5. Appendix — adversarial requirement analysis & solution proposal record

- Round 1 — [scratchpad-requirement-analysis-round1/adversarial-review-record.md](../.claude/prompts/scratchpad-requirement-analysis-round1/adversarial-review-record.md) (contested points 1–5).
- Round 2 (2026-07-08, display re-scope + CUDA) — [scratchpad-requirement-analysis-round2/adversarial-review-record.md](../.claude/prompts/scratchpad-requirement-analysis-round2/adversarial-review-record.md) (points 6–11).
- Round 3 (2026-07-10, BTC-letter full rework) — [scratchpad-requirement-analysis-round3/adversarial-review-record.md](../.claude/prompts/scratchpad-requirement-analysis-round3/adversarial-review-record.md) (points 12–24).
