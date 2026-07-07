# Researcher Beta — Position Paper (Round 1)
## Cooperative Vehicle Awareness, Milestone 1 — Standards & Extensibility Advocate

Input: the entire `milestone1.md` feature set. Procedure: `requirement-analysis-and-solutioning` followed in order. This is a debate position paper, NOT the canonical `requirements/` report.

---

## 1. Whole-input feasibility verdict

**Achievable in 2 months with 2–3 developers — conditional on three early decisions.**

Reasoning:
- The plan is already de-risked by design: contract-first, mock-then-real, three parallel tracks, and a stub transport escape hatch. Every phase is individually a known-technology build (UDP/JSON messaging, pretrained COCO detection, pinhole monocular ranging, OpenCV display). Nothing requires research-grade novelty.
- The three conditions that keep it achievable:
  1. **Stub transport (UDP multicast) is the committed baseline**; real PC5 is a stretch goal gated on hardware arrival (decision date proposed: end of week 2). Vendor PC5 SDKs are typically proprietary — a hard-constraint conflict (open-source only) that must be flagged to the user, not absorbed.
  2. **The demo dataset problem is solved deliberately, not incidentally.** The end-to-end demo needs *two synchronized camera clips from one scripted convoy* (A seeing B with C occluded; B seeing C) *plus ground-truth d_AB, d_BC, d_AC* to pass Phase 4's ±15% and Phase 6's composed-tolerance criteria. KITTI cannot stage this; CARLA can. This is a real work item (~3–5 dev-days) that must be scheduled, or the Phase 6 tolerance criterion is unverifiable.
  3. **Schema conformance is enforced mechanically at contract freeze** (JSON Schema + field→ ASN.1 mapping table), because section 2 makes "schema must be standard-conformant" a hard requirement even when JSON-encoded. Asserted-but-unverified conformance is fake conformance.
- At-risk items (detailed per requirement below): modem/GNSS bring-up (hardware uncertainty), monocular distance accuracy, composed-distance accuracy (reference-point bias + temporal skew).
- Nothing in the input is infeasible as stated. No deferred-scope item (section 6) needs to be pulled forward.

---

## 2. Enumerated requirements (project-global 1..14)

Ordering: **urgency** — contracts first (they block all three tracks), then per-track bring-up in dependency order, convergence last. Rationale: the plan is contract-first; R1/R2 gate everything.

Proposed numbers below marked **[A]** are assumptions to confirm with the user.

---

**R1 — Frozen V2X message schema, standard-conformant to CPM (ETSI TS 103 324).** The schema is committed as (a) a JSON Schema file, (b) a mapping table tracing every field to its TS 103 324 ASN.1 path with the standard's units, ranges, and resolution (e.g. `generationDeltaTime` = TimestampIts mod 65536 ms; positions/distances in standard units), and (c) explicit **measurement reference-point semantics** for the perceived object and the sender reference position (CPM defines these; naive omission causes the composition bias in R13).
- Vague→precise: "standard-conformant schema" → *100% of schema fields appear in the mapping table with a valid TS 103 324 field path, standard units and value ranges; every emitted message validates against the JSON Schema in CI.*
- Base standard choice: **CPM (ETSI) over SDSM (SAE J3224)** — ETSI spec PDF and ASN.1 are free on forge.etsi.org, so conformance is auditable at zero cost; SAE J3224 is paywalled, making conformance unverifiable in an open-source-only workflow. **[A]** (user to confirm CPM base).
- KPI: mapping table covers 100% of fields; JSON Schema validation pass rate = 100% of emitted messages; zero fields whose unit/range deviates from the standard.
- Feasibility: **achievable** — 2–4 days of spec reading + schema writing; ETSI ASN.1 is public.

**R2 — Frozen TrackedObject struct.** The 9-field struct of section 4 (`id, class, source, first_seen, last_seen, bbox, distance, confidence, state`) committed as a typed definition with unit tests; both perception sub-phases and the display track import it, never redefine it.
- KPI: struct exposes all 9 fields with declared types/units (timestamps in ms since epoch, distance in m); unit test instantiates every field and every `state`/`source` enum value; downstream code imports from one module (verified by grep: no duplicate definition).
- Feasibility: **achievable** — trivially, ≤1 day.

**R3 — Modem bring-up + GNSS acquisition (hardware path).** Host queries modem identity/firmware/V2X mode/GNSS fix over QMI or AT and logs a startup record; GNSS position+time logged periodically.
- Vague→precise: "periodically" → *1 Hz* **[A]**.
- KPI: startup log contains modem ID, firmware version, V2X mode, first GNSS fix time-to-fix; GNSS log line rate = 1 Hz ± 10% over a 10-minute run.
- Feasibility: **at-risk** — hardware availability unknown; QMI/AT bring-up on an unfamiliar modem can burn 1–2 weeks. Mitigation: gate on hardware-arrival decision date (end of week 2 **[A]**); the stub path (R5, R6-fallback) keeps the milestone demo-able without it. Flagged, not absorbed.

**R4 — V2X service auto-starts on boot.**
- KPI: `systemctl is-enabled` = enabled and `is-active` = active after an unattended reboot; service reaches "broadcast loop running" log line within 30 s of boot **[A]**.
- Feasibility: **achievable** — systemd unit is a solved problem.

**R5 — Transport broadcasts and receives the full schema with mock contents (stub baseline).**
- Vague→precise: "broadcast and receive the full agreed message" → *message rate 10 Hz **[A]** (matches CPM generation-rule range 0.1–1 s); receiver parses all fields.*
- KPI: over a 10-minute bench run on the two-host LAN: delivery ratio ≥ 99% **[A]**; 100% of received messages parse into all schema fields; each vehicle logs the peer's full message.
- Feasibility: **achievable** — UDP multicast + JSON is days, not weeks.

**R6 — Common timebase across A and B.**
- Vague→precise: "offset within an agreed bound" → *|clock offset between A and B| ≤ 50 ms* **[A]**. Rationale: at 5 m/s relative speed, 50 ms ≈ 0.25 m of composition error — negligible against R13's budget; achievable by chrony+gpsd (GNSS path) or chrony NTP peering between the two hosts (stub path, no GNSS needed).
- KPI: measured offset (chrony tracking report, cross-checked by round-trip-halving UDP echo probe) ≤ 50 ms for the duration of the demo run; offset logged at 0.1 Hz.
- Feasibility: **achievable** on both hardware and stub paths; the *measurement method* must exist before R12's latency KPI is testable (ordering dependency — see risks).

**R7 — Video harness with per-frame timestamps.**
- KPI: container/codec/fps/resolution detected and logged for the provided clip; per-frame timestamps monotonic with jitter ≤ 1 frame period; frame count matches container metadata.
- Feasibility: **achievable**.

**R8 — TrackedObject store + admission state machine, driven by mock.**
- KPI: with mock on, log shows a complete `not_tracked → tentative → tracked → expired` cycle with timestamps; with mock off, zero tracks created over the full clip (binary check).
- Feasibility: **achievable**.

**R9 — Detection of C and in-lane-lead selection with a pretrained detector.**
- Vague→precise: "keeps pace with the video frame rate (or agreed latency target)" → *effective detection rate ≥ 10 Hz **[A]** with a documented frame-skip policy, and per-frame inference latency ≤ 100 ms on the dev machine (CPU acceptable for recorded video); lead-selection rule = largest bbox whose center falls in the middle third of the image* **[A]**.
- KPI: recall on the lead vehicle ≥ 95% of frames where it is visible in the validation clip; lead selected correctly in ≥ 95% of multi-vehicle frames; effective rate ≥ 10 Hz sustained.
- Feasibility: **achievable** — pretrained COCO detection of cars is commodity.

**R10 — Monocular distance to the lead, per leg, within tolerance.**
- Vague→precise: "agreed tolerance (e.g. ±15%)" → *90th-percentile absolute relative error ≤ 15% over the 10–50 m range **[A]**, against dataset ground truth, using ground-plane projection of the bbox bottom with dataset-provided intrinsics.*
- KPI: the 90th-percentile figure above, reported per validation clip; distance written to the store every processed frame.
- Feasibility: **at-risk** — ±15% is realistic for ground-plane projection on flat road with correct intrinsics and camera height, but degrades with pitch bounce and bbox-bottom noise. Mitigation: validate on KITTI first (real noise), median-filter over 5 frames **[A]**, height-prior method as cross-check. Flagged.

**R11 — Proximity gate with confirmation + hysteresis, constants externalized.**
- KPI: on a scripted boundary-crossing clip, zero admit/drop flicker (no track lifecycle shorter than 2 s **[A]** near the gate boundary); admission only after `confirm_hits`=3 consecutive in-range frames; drop only past `gate_exit`=35 m or `miss_limit`=5; grep of source shows zero gate literals — all four constants load from a config file.
- Feasibility: **achievable**.

**R12 — Real-data relay window (Phase 5 convergence).** B populates the perceived-object entry from the real track and relays only while C is `tracked`; A admits a `v2x_relayed` track on receipt.
- Vague→precise: "latency within the agreed bound" → *end-to-end latency, from B's frame capture timestamp to A's track-admission timestamp (comparable via R6), ≤ 300 ms* **[A]** (budget: ≤100 ms inference + ≤100 ms message period slack at 10 Hz + ≤5 ms LAN + margin).
- KPI: latency P95 ≤ 300 ms over a 5-minute run; relay starts within 1 message period of C entering `tracked` and stops within `miss_limit` expiry of C leaving; zero mock code on the path (binary: mock modules not imported in the run's process — assertable in the startup log).
- Feasibility: **achievable** on the stub path *given R6 lands first*; at-risk only in the hardware-PC5 variant (inherits R3's risk).

**R13 — Composition d_AC = d_AB + d_BC with explicit error budget, within composed tolerance.**
- **Error-budget math (the part "±15% per leg" hides):**
  - Proportional part: worst case |e_AC| ≤ 0.15·d_AB + 0.15·d_BC = 0.15·d_AC — composition of proportional errors over a sum of positive lengths is tolerance-neutral (≤15% worst case, better under independence). The naive "15%+15%=30%" fear is wrong.
  - **Additive terms not covered by ±15%:** (a) *Reference-point bias:* A measures camera→B-rear; B measures camera→C-rear. True A→C(rear) = d_AB + (B-rear→B-camera offset, ≈3–4.5 m) + d_BC. Uncorrected, the composition **systematically underestimates by ~one car length — ~7–8% at 50 m**, silently consuming half the budget. Fix: a configured `ref_point_offset_B` constant (externalized per rule 5) and reference-point semantics carried in the R1 schema. (b) *Temporal skew:* d_BC is stale by (pipeline latency + clock offset) ≤ 0.35 s → at 5 m/s relative speed ≈ 1.75 m. (c) Heading/collinearity cosine error: < 0.5% below 5° misalignment — negligible; assumption holds for the scripted convoy.
- Vague→precise: "composed d_AC matches ground truth within the agreed tolerance" → *after reference-point correction, |d̂_AC − d_AC| ≤ 0.15·d_AC + 2.0 m **[A]** (≈ ±20% at the 40–60 m demo range).*
- KPI: the bound above holds for ≥ 90% of composed samples in the demo run **[A]**; the reference-point constant is present in config (not code).
- Feasibility: **at-risk** — depends on R10 twice, on the correction constant being applied, and on ground-truth availability (only CARLA can supply d_AC truth for the staged convoy — see decision point (e)). Flagged with the mitigation above.

**R14 — End-to-end NLOS demo (definition of done).**
- KPI: with real Phase 5 data, C renders as a ghost marker on A's BEV within 1 s **[A]** of A's first admitted relayed message; B's display shows directly-detected C with distance; A's own-sensor track log contains **zero** detections of C over the entire run (binary proof that awareness came only via relay); all six phases' acceptance boxes checked.
- Feasibility: **achievable** conditional on R12/R13.

---

## 3. Technical solution analysis (decision points)

All candidates below pass the hard constraints (open-source, Linux) unless explicitly disqualified. Each pick states which ranked criteria drove it.

### (a) Transport
| Candidate | Notes |
|---|---|
| **Plain UDP multicast (Python stdlib socket)** | Zero dependencies; broadcast semantics match V2X; trivially replaceable. |
| ZeroMQ pub/sub | Nicer API, but adds a dependency for one small periodic datagram; pub/sub topology is a poorer analogue of broadcast V2X than multicast. |
| Vanetza (ETSI GeoNet/BTP/facilities stack) | Real ITS stack, C++; **but its CPM is the older TR 103 562 shape, not TS 103 324 (verified via vanetza issue #194)** — it does not buy current-spec conformance for free; heavy integration cost for M1. |
| Vendor PC5 SDK | Typically proprietary → **disqualified by the open-source hard constraint**; cannot even be shortlisted. If hardware arrives with a proprietary SDK, that is a user-level conflict to flag (real PC5 is deferred scope anyway, section 6). |

**Pick: plain UDP multicast**, wrapped behind a thin transport interface (send/recv of an encoded payload) so a real stack can slot in at M2. Drivers: criteria 1 & 2 (most certain, fastest); criterion 4 (smallest dependency — beats ZeroMQ); the interface wrapper is the criterion-3 protection and costs ~an hour, not an architecture. Honest concession to the pragmatist: raw UDP wins even on my stance — Vanetza is *not* the extensible choice here because it encodes the wrong CPM revision.

### (b) Message encoding
| Candidate | Notes |
|---|---|
| **JSON mirroring TS 103 324 structure + committed JSON Schema + field→ASN.1 mapping table** | Explicitly allowed by section 2; conformance made *checkable*, not asserted. |
| Full ASN.1 UPER (asn1c/pycrate against ETSI forge ASN.1) | True wire conformance, but section 6 explicitly defers full ASN.1; UPER debugging burns days; violates criterion 2 for zero M1 credit. |
| Vanetza-provided encoding | Wrong CPM revision (TR 103 562) — fails the conformance goal it exists to serve. |
| CBOR/Protobuf | Neither is the standard encoding nor human-debuggable; worst of both worlds. |

**Pick: JSON with enforced conformance** — field names/nesting mirror the TS 103 324 CPM containers (management / station data / perceived-object), units and ranges exactly per the ASN.1, plus the R1 mapping table and CI validation. Drivers: criteria 1 & 2 pick JSON; the mapping discipline is not gold-plating — it is the *hard requirement* of section 2 ("the schema must be standard-conformant") made falsifiable, and it makes the M2 ASN.1 swap mechanical (criterion 3 at near-zero cost). Base CPM (ETSI, free spec) over SDSM (SAE, paywalled) — see R1.

### (c) Language(s) per track
| Candidate | Notes |
|---|---|
| **Python everywhere (perception, display, comms stub, modem control via qmicli subprocess)** | One language, one runtime, fastest integration; perception stack is Python-native anyway. |
| C++ (asio) for comms, Python for perception/display | Matches where real V2X stacks live (C/C++), but adds a cross-language contract boundary and build system for an M1 stub that doesn't need it. |
| Rust for comms | Same objection, plus lower team familiarity risk. |

**Pick: Python for all M1 tracks**; note explicitly that the M2 real-PC5/Vanetza path will likely force C++ in the comms track — the R1 schema and transport interface are the insulation. Drivers: criteria 1 & 2 (single-language integration risk is the lowest); criterion 3 satisfied by the contract, not by pre-paying for C++ now.

### (d) Detector + license position
| Candidate | License | Notes |
|---|---|---|
| **Ultralytics YOLOv8n/11n (pretrained COCO)** | AGPL-3.0 | Best docs, one-line inference, built-in tracking; named in the plan. |
| YOLOX (Megvii) | Apache-2.0 | Mature, pretrained COCO, permissive; slightly more integration glue. |
| RT-DETR (original Baidu repo) | Apache-2.0 | Strong accuracy; heavier than needed for "detect a car". |
| OpenCV DNN + open weights | varies | No PyTorch dependency, but weakest tooling; more work for less. |

**License position (taking a stand as instructed):** AGPL-3.0 is OSI-approved open source. The hard constraint bans *commercial/paid* tools; it does not ban copyleft. **Ultralytics passes the hard constraints.** The real AGPL consequence — if this hackathon project is ever distributed or run as a service without releasing full source, the enterprise license (commercial!) becomes necessary — is a *project-IP decision that belongs to the user*, not a researcher veto, and note that criterion 3 concerns future *features*, not licensing posture. **Pick: Ultralytics YOLOv8n** (criteria 1 & 2 — it is the most certain and fastest path, and the plan names it), **with two conditions:** (i) the AGPL implication is put to the user as an explicit accept/reject item; (ii) the detector sits behind a one-function interface (`frame → [bbox, class, conf]`) so a YOLOX swap is a one-module change if the user rejects AGPL. If rejected, YOLOX is a near-equal second at the cost of ~1–2 extra days.

### (e) Monocular distance method + validation data
Method candidates:
| Candidate | Notes |
|---|---|
| **Ground-plane projection of bbox bottom (pinhole + camera height + intrinsics)** | Standard, no learned scale ambiguity, intrinsics available from dataset. |
| Known-height similar triangles (car height prior ~1.5 m) | Robust to pitch, weaker to class height variance; excellent as a cross-check. |
| Learned monocular depth (MiDaS / Depth Anything) | Relative depth needs metric calibration; big model, big integration cost; criterion 2 loses badly. |

**Pick: ground-plane projection primary, height-prior cross-check** (criteria 1 & 2).

Validation-data candidates — **this is a two-part need Alpha may collapse into one:** | Need | KITTI | CARLA | |---|---|---| | Unit-validate detector + per-leg distance (R9, R10) | ✅ real imagery, LiDAR-truth ranges, intrinsics, zero setup | possible but synthetic | | **End-to-end demo clips**: two synchronized cameras from one scripted A–B–C convoy, C occluded from A, with ground-truth d_AB, d_BC, **d_AC** (R13, R14) | ❌ cannot stage it; no d_AC truth | ✅ scriptable exactly; exports truth |

**Pick: KITTI for Phase 3/4 unit validation + CARLA for the demo scenario clips.** Driver: criterion 1 — without CARLA (or an equivalent staged recording with measured truth), Phase 6's acceptance criterion "composed d_AC matches ground truth" is *untestable*, and no existing dataset provides an occluded-convoy pair of synchronized clips. Cost: ~3–5 dev-days of CARLA scripting, scheduled early in the display/perception tracks. This is a feasibility requirement, not gold-plating.

### (f) Display / BEV
| Candidate | Notes |
|---|---|
| **OpenCV: overlay on camera frame + drawn BEV canvas** | Already a dependency (R7); one imshow window each; fast enough at 10–30 Hz. |
| matplotlib BEV | Fine for stills; interactive-update performance is poor for a live demo loop. |
| Web dashboard (Flask + JS canvas) | Nice for judges but a second codebase; criterion 2 loses. |
| rerun.io / Foxglove | Attractive visualizers, heavier dependency and learning curve than needed. |

**Pick: OpenCV for both overlay and BEV** (criteria 2 & 4 — zero new dependencies, fastest; "production GUI later" is explicitly out of M1 scope).

### (g) Service management
| Candidate | Notes |
|---|---|
| **systemd unit** | Present on every Linux target; `Restart=on-failure`, journald logging; zero new deps. |
| Adaptive AUTOSAR Execution Management | No practically usable open-source implementation; enormous integration cost; effectively disqualified. |
| supervisord | A new dependency to do what systemd already does. |
| Docker restart policies | Adds containerization work with no M1 payoff. |

**Pick: systemd** (criteria 1, 2, 4 — unanimous).

### (h) Time synchronization tooling (implied by R6/R12)
| Candidate | Notes |
|---|---|
| **chrony (+ gpsd SHM on the hardware path; NTP peer mode host-to-host on the stub path)** | Handles both paths with one tool; good step/jitter behavior; simple offset reporting for the KPI. |
| ntpd | Older, slower convergence, no advantage. |
| systemd-timesyncd | SNTP client only — cannot peer two hosts or ingest gpsd. |
| PTP (linuxptp) | Sub-µs is overkill for a 50 ms bound; NIC/hw-timestamp dependency. |

**Pick: chrony (+gpsd when GNSS exists)** (criteria 1 & 2; criterion 4 over PTP).

---

## 4. Proposed scope/requirement changes & risks (user accept/reject list)

1. **Confirm CPM (ETSI TS 103 324) — not SDSM (SAE J3224) — as the schema base.** ETSI spec + ASN.1 are freely accessible, making conformance auditable in an open-source-only workflow; SAE is paywalled. (Affects R1.)
2. **Add reference-point semantics + a configured `ref_point_offset_B` correction** to the frozen contract/config. Without it, composition carries a systematic ~one-car-length (~4 m) underestimate — half the error budget at demo range. (Affects R1, R13.)
3. **Accept AGPL-3.0 (Ultralytics) or direct the switch to YOLOX (Apache-2.0).** AGPL passes the written hard constraints; the copyleft consequence is a project-IP decision only the user can make. (Affects R9.)
4. **Approve CARLA as the demo-clip source (~3–5 dev-days).** Otherwise Phase 6's "composed d_AC matches ground truth" criterion has no ground truth to match, and no dataset supplies synchronized occluded-convoy clips for A and B. (Affects R13, R14.)
5. **Confirm the proposed numbers** (all marked [A]): 50 ms timebase offset; 10 Hz message rate; ≥99% bench delivery; 300 ms E2E latency P95; ≥10 Hz effective detection with ≤100 ms inference; ±15% 90th-pct per-leg distance over 10–50 m; composed bound 0.15·d_AC + 2.0 m; 1 s display-appearance bound; gate constants 30/35/N=3/M=5.
6. **Hardware decision date (end of week 2):** if the C-V2X modem is not in hand, R3 becomes a stretch goal and the stub is the committed demo path — flagged now so it isn't a silent re-scope later. Also flag: vendor PC5 SDKs are usually proprietary, conflicting with the open-source hard constraint; resolving that conflict (waiver vs Vanetza-on-hardware at M2) is the user's call.
7. **Ordering dependency to enforce in planning:** R6 (timebase + offset measurement) must land before R12's latency KPI is measurable — the latency number is meaningless across two unsynchronized hosts.

---

## 5. Where I expect the pragmatist (Alpha) to cut corners — challenges

1. **"JSON with sensible field names" ≠ standard-conformant.** If Alpha's schema lacks a committed field→TS 103 324 mapping with standard units/ranges and CI validation, section 2's hard requirement is unmet and the M2 ASN.1 swap becomes a rewrite. Challenge: show the mapping table, or the conformance claim is unfalsifiable.
2. **KITTI-only validation.** How does Alpha pass Phase 6's "composed d_AC matches ground truth" and produce *two synchronized clips of one convoy with C occluded from A*? KITTI has neither. If the answer is "we'll eyeball it", the acceptance criterion is being quietly deleted.
3. **Reference-point bias ignored.** If the error budget stops at "15% per leg composes to 15%", it misses the systematic ~4 m camera-vs-rear-bumper offset — ~8% at 50 m, silently consumed. Challenge: show the additive error terms, not just the proportional one.
4. **Latency KPI without a timebase plan on the stub path.** No GNSS on the stub — what synchronizes the two hosts, to what bound, and how is the offset *measured* so the ≤300 ms claim means anything? If unanswered, Phase 5's latency criterion is untestable.
5. **AGPL taken silently / vendor SDK assumed.** Ultralytics-AGPL must be an explicit user decision; a vendor PC5 SDK is proprietary and fails the hard constraint outright. Silence on either is a rules violation, not pragmatism.

## 6. Honest concessions (where the simple option genuinely wins)

- Raw UDP beats Vanetza and ZeroMQ for M1 even on extensibility grounds — Vanetza's CPM is the outdated TR 103 562 shape, so it doesn't even deliver the conformance it promises.
- Full ASN.1/UPER now would be over-engineering: section 6 defers it, and the mapping table buys the same M2 optionality for ~5% of the cost.
- Python everywhere, systemd, OpenCV display: the boring picks win criteria 1–2 outright; I make no standards-flavored objection.
- The ±15%→composed math is *not* the disaster it superficially looks like — proportional errors through a sum are tolerance-neutral; the fight is over the additive terms only.
