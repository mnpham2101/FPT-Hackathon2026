# Requirement Analysis & Technical Solution Report — M1: Cooperative Vehicle Awareness

- **Input analysed:** the entire [milestone1.md](../.claude/plans/milestone1.md) feature set
  (Milestone 1, "Cooperative Vehicle Awareness A ← B ← C").
- **Date:** 2026-07-04.
- **Authorship:** converged output of a two-researcher adversarial review — Researcher Alpha
  (implementability / M1-speed emphasis) vs. Researcher Beta (standards-conformance /
  extensibility emphasis). Contested points and their resolutions are recorded in the Appendix.
- **Numbering:** this is the project's first report. **Requirement numbers 1–18 are
  project-global and frozen permanently** as the `X` segment of task IDs `X.Y.Z.W` per
  [research-report-format.md](../.claude/rules/research-report-format.md) — never reused or
  renumbered.
- **Notation:** every proposed numeric value is marked **(A) = assumption to confirm** — none
  exists verbatim in the plan doc; the user may veto any of them (see §5, flag F7).
  Verdicts: **ACH** = achievable, **RISK** = at-risk.

---

## 1. Enumerated requirements (1–18)

**Ordering rationale: urgency.** Contracts (1–2) block all three tracks — the plan is
contract-first by design. Then per-track requirements in the plan's own dependency/phase order
(comms 3–6, perception 7–12, convergence 13–14, display/composition 15–16), capstone 17, and the
cross-cutting dataset precondition 18.

### Contracts (Step 0)

**R1 — Frozen V2X message schema, normative target = ETSI TS 103 324 CPM.**
A versioned (v1.0) schema document + reference implementation of the perceived-object message,
carrying `stationId`, `generationDeltaTime`, sender reference position/time, and a
perceived-object entry (`objectId`, relative position/distance, `classification`, `confidence`)
— per plan §4. The **single normative target is ETSI TS 103 324 CPM** (v2.1.1 ASN.1, freely
auditable at the [ETSI forge](https://forge.etsi.org/rep/ITS/asn1/cpm_ts103324)); SAE J3224 SDSM
is referenced informatively only, because it is paywalled and therefore unauditable under the
open-source-only constraint. The schema **must carry object reference-point semantics** (CPM
defines them; R15's composition math requires them). Field names/nesting mirror the CPM
containers (management / station data / perceived-object) with the standard's units, ranges, and
resolutions, so a later ASN.1 codec swap changes no field semantics. Enforcement: a pydantic
model is the single source of truth (JSON Schema generated *from* it), validating every message
at runtime; an encode→decode round-trip test and a mapping-coverage check run in CI.
- **Verdict: ACH** — pure specification work, days; ETSI spec and ASN.1 are public.
- **KPI:** mapping table traces 100% of schema fields to a TS 103 324 field path with standard
  units/resolution; round-trip unit test reproduces 100% of fields bit-exact; 0 range violations
  at runtime across all demo runs.
- Vague→precise: "standard-conformant schema" → *every field traceable 1:1 to a TS 103 324 CPM
  field, verified by a committed mapping table + CI round-trip test; reference-point semantics
  explicit.*

**R2 — Frozen TrackedObject struct.**
The 9-field struct of plan §4 (`id, class, source, first_seen, last_seen, bbox, distance,
confidence, state`) implemented as one typed model shared by detection, distance, gate, relay,
and display code — never redefined.
- **Verdict: ACH.**
- **KPI:** unit test constructs the struct with all 9 fields (timestamps in ms since epoch,
  distance in m), exercises all `state` and `source` enum values; one-definition check passes
  (single import source, zero duplicate definitions).

### Comms track (Phase 1)

**R3 — V2X broadcast service, auto-start on boot.**
A Linux service auto-starts on boot, initializes the transport, and broadcasts the full R1
schema with mock object contents at 10 Hz (A).
- **Verdict: ACH** on the stub path (hardware variant is R6).
- **KPI:** after a cold reboot, service is active within 30 s (A); observed message rate
  10 Hz ± 10% over a 60 s window (A).
- Vague→precise: "runs the broadcast loop" → *10 Hz sustained, rate error ≤ 10% over 60 s (A)* —
  10 Hz matches the CAM/BSM upper rate so the stub is rate-realistic.

**R4 — Peer reception + full-field parse.**
The receiving vehicle logs every received message parsed into 100% of schema fields, both
directions (A↔B).
- **Verdict: ACH.**
- **KPI:** delivery ratio ≥ 99% over a 10-minute bench-LAN soak (A) — on a wired bench, lower
  indicates a bug, and a 10-min run exposes buffer/leak issues a 60 s check misses; every
  received message logs all schema fields with 0 unit/range assertion violations.

**R5 — Common timebase across A and B.**
Both hosts' timestamps are comparable within a bound; the offset is *measured*, not assumed.
- **Verdict: ACH.**
- **KPI:** measured clock offset |Δt| ≤ 50 ms via chrony/NTP peering on the stub path, ≤ 10 ms
  when GNSS-disciplined (A), sustained over 10 min; offset logged at 0.1 Hz via chrony tracking
  report cross-checked by a round-trip-halved UDP echo probe.
- Vague→precise: "offset within an agreed bound" → *≤ 50 ms (stub/NTP), ≤ 10 ms (GNSS) (A)*.
- **Ordering note:** R5 must land before R14's latency KPI is measurable — a cross-host latency
  number is meaningless between unsynchronized clocks.

**R6 — Modem bring-up + GNSS logging (hardware-conditional).**
If a C-V2X modem is available: QMI/AT query of identity, firmware, V2X mode, GNSS fix, with a
startup log; GNSS position+time logged at 1 Hz (A). If not: a mock GNSS provider emits the
identical log format.
- **Verdict: RISK** — entirely gated on hardware availability (unknown today); QMI/AT bring-up
  on an unfamiliar modem can burn 1–2 weeks. Mitigation: week-2 go/no-go gate (flag F1); the
  stub path keeps every other requirement green. Flagged, not absorbed.
- **KPI (hardware):** startup log contains modem model, firmware version, V2X mode string, and
  a 3D GNSS fix within 120 s (A) of service start; position logged at 1 Hz ± 0.1 Hz.
  **KPI (stub):** identical log format from the mock provider, verified by the same parser.

### Perception track (Phases 2–4)

**R7 — Video harness with per-frame timestamps.**
The provided video opens; container/codec/fps/resolution detected and logged; frames stream with
per-frame timestamps.
- **Verdict: ACH.**
- **KPI:** 100% of frames delivered exactly once; timestamps strictly monotonic (jitter ≤ 1
  frame period); metadata log matches `ffprobe` output for the same file.

**R8 — TrackedObject store + admission state machine (mock-driven).**
Store exposes all R2 fields; the `not_tracked → tentative → tracked → expired` manager runs off
an injected synthetic C.
- **Verdict: ACH.**
- **KPI:** with mock on, log shows a full lifecycle cycle with timestamps; with mock off, zero
  tracks over the whole clip (binary); transitions obey `confirm_hits`/`miss_limit` exactly in a
  scripted-input unit test (0 deviations).

**R9 — Detection of C + in-lane lead selection.**
Pretrained detector finds C as `car`/`truck`; the in-lane lead (largest central bbox) is selected
when multiple vehicles are present; bboxes are written to the store.
- **Verdict: ACH** — pretrained COCO, no training (plan §2).
- **KPI:** recall ≥ 90% (A) of frames where C is visible and within 50 m (A); lead selection
  correct in ≥ 95% (A) of multi-vehicle frames on a hand-labeled 200-frame sample; mock path
  disabled (binary).

**R10 — Perception throughput.**
- **Verdict: ACH** (nano detector on CPU meets it; GPU is margin, not a dependency).
- **KPI:** detection update rate ≥ 10 Hz (A) with mean per-frame latency ≤ 100 ms (A) on the
  demo machine; a documented frame-skip policy is allowed as long as the update rate holds.
- Vague→precise: "keeps pace with the video frame rate (or agreed latency target)" → *≥ 10 Hz
  effective update rate, ≤ 100 ms mean latency, frame-skip permitted and documented (A)* —
  10 Hz matches R3's message rate so perception never bottlenecks the relay.

**R11 — Monocular distance per leg.**
Per-frame range to the lead from bbox-bottom ground-plane projection (camera intrinsics +
height), written back to the track.
- **Verdict: RISK until R18 confirms intrinsics** — ±15% is realistic for ground-plane
  projection on a flat road with correct intrinsics/camera height, but degrades with pitch
  bounce and bbox-bottom noise, and is unachievable if the demo video ships without calibration.
  Mitigations: validate on KITTI first (real noise); 5-frame median filter (A); height-prior
  cross-check.
- **KPI:** ≥ 90% (A) of frames within ±15% of ground truth for true range 10–50 m (A) on a
  calibrated clip; distance present on the track every frame C is detected.
- Vague→precise: "agreed tolerance (e.g. ±15%)" → *±15%, scoped to 10–50 m, ≥ 90% of frames (A)*.

**R12 — Active proximity gate, constants externalized.**
Admission only when ≤ `gate_enter` (30 m) for `confirm_hits` (3) consecutive frames; drop only
beyond `gate_exit` (35 m) or after `miss_limit` (5) misses. All four constants in a config file
(constitution rule 5 — no hardcoded tunables).
- **Verdict: ACH.**
- **KPI:** on a synthetic approach/retreat sweep with injected Gaussian noise σ = 2 m (A),
  exactly one admit and one drop event (0 flicker oscillations); changing a constant in config
  alters behavior without rebuild (binary); static check finds no gate literals in code.
- Vague→precise: "no add/remove flicker at the boundary" → *0 state oscillations under a σ = 2 m
  noise sweep across the 30–35 m band (A)*.

### Convergence (Phase 5)

**R13 — Real-data relay gated on track state.**
B's broadcast carries real Phase-4 data for C and includes C **only while C is `tracked`**.
- **Verdict: ACH.**
- **KPI:** zero mock code on the transmit path (binary — mock modules not imported, assertable
  in the startup log); relay starts ≤ 1 message period (100 ms @ 10 Hz) after C enters `tracked`
  and stops ≤ 1 period after C leaves; transmitted perceived-object fields match store values
  for the same timestamp (log diff = 0).

**R14 — Relayed-track admission on A + end-to-end latency.**
A decodes and creates a `source = v2x_relayed` track carrying B's reference position/time;
latency within bound; relayed track expires after `miss_limit` missed periods (mirror of R12).
- **Verdict: ACH** on the stub path, given R5 (timebase) lands first.
- **KPI:** latency from B's frame-capture timestamp to A's relayed-track creation ≤ **300 ms
  p95** (A), with 200 ms as the engineering target; B's reference position/time present
  (field-presence check); mirror-expiry honored.
- Vague→precise: "agreed latency bound" → *≤ 300 ms p95 (200 ms target) (A)*. Budget: ≤ 100 ms
  detection (mean; p95 higher) + up to one 100 ms message period + ~5 ms stub network/codec +
  ±50 ms clock-offset measurement uncertainty — a 200 ms p95 *acceptance* bound fails its own
  worst-case arithmetic, hence 300 ms as the smallest honest bound.

### Display track (Phase 6) + capstone

**R15 — Distance composition on A, with reference-point correction.**
A measures `d_AB` locally, composes `d_AC = d_AB + d_BC` (lateral offsets component-wise),
**after applying a configured `ref_point_offset_B` correction** (externalized per constitution
rule 5; semantics carried in R1's schema; ground-truth reference convention declared per R18).
- **Error-budget derivation (why the KPI has this shape):**
  d̂_AC = d_AB(1+ε₁) + d_BC(1+ε₂) ⇒ relative error = (ε₁·d_AB + ε₂·d_BC)/(d_AB+d_BC), a convex
  combination of ε₁, ε₂ — bounded by max(|ε₁|,|ε₂|) ≤ 15% for *any* sign combination (same-sign
  worst case exactly 15%; opposite signs partially cancel). **The naive "two ±15% legs compound
  to ±30%" claim is false** — proportional error through a sum of positive lengths cannot exceed
  the worse leg. The tolerance widening is needed for the **additive** terms instead:
  (a) *reference-point bias* — A measures camera→B-rear, B measures its camera→C-rear, so true
  A→C = d_AB + (B-rear→B-camera ≈ 3–4 m) + d_BC; uncorrected this is a systematic ~7–8%
  underestimate at 50 m that alone would break a plain ±20% bound (0.15·d_AC + 3.5 m ≈ 22% at
  50 m) — the correction constant is what makes the KPI passable;
  (b) *temporal skew* — v_rel × (latency + |Δt|) ≈ 5 m/s × 0.35 s ≈ 1.75 m;
  (c) heading/collinearity cosine error < 0.5% below 5° misalignment — negligible for the
  scripted convoy (plan §2 assumption holds).
- **Verdict: RISK** — depends on R11 twice, on the correction being applied, and on R18's
  ground truth existing.
- **KPI:** |d̂_AC − d_AC| ≤ **0.15·d_AC + 2.0 m** for ≥ 90% of composed samples over the demo
  range (A) (≈ ±19–20% at 50 m); `ref_point_offset_B` present in config, not code; composition
  arithmetic verified exactly by a mock-input unit test.
- Vague→precise: "composed d_AC matches ground truth within the agreed tolerance" →
  *≤ 0.15·d_AC + 2.0 m, ≥ 90% of samples, after reference-point correction (A)*.

**R16 — Dual display: direct C on B, ghost C on A's BEV.**
B renders C from direct detection with relative position; A renders C as an occluded "ghost"
marker on a bird's-eye view. Built against a mock message first (per plan), integrated with real
data at the end.
- **Verdict: ACH.**
- **KPI (requirement-level, judge-visible):** ghost C appears on A's BEV within **2 s (A) of B's
  first `not_tracked→…→tracked` transition** (measured across hosts via the R5-synced timebase —
  a prerequisite); BEV marker updates ≤ 500 ms (A) after message receipt; display loop sustains
  ≥ 15 FPS (A); dev-mode mock rendering passes before integration (binary).
- **Diagnostic measurement (not the requirement KPI):** time from A's first admitted relayed
  message to ghost render ≤ 1 s (A) — isolates the display slice when the 2 s budget is missed.

**R17 — End-to-end demo (Definition of Done).**
With no mocks anywhere in the path, C appears on A's display purely via B's V2X relay while A's
own perception never detects C.
- **Verdict: ACH**, conditional on R6, R11, R18 flags resolving.
- **KPI:** one continuous recorded demo run in which (a) A's detector log contains **zero**
  direct detections of C, (b) A's BEV shows ghost C sourced `v2x_relayed`, (c) all six phases'
  acceptance checklists are ticked. Binary pass/fail.

### Cross-cutting precondition

**R18 — Demo dataset package.**
A validated demo dataset exists, containing: **(i)** two synchronized clips from one convoy
(A-view and B-view), **(ii)** C occluded from A's view for the demo duration, **(iii)** per-frame
ground-truth d_AB, d_BC, d_AC with a declared reference-point convention, **(iv)** camera
intrinsics for both cameras. Provided by end of week 3 (A); if any item is missing, the CARLA
scenario-recording task (~1 week, GPU machine) is scheduled (flag F2).
- **Verdict: RISK** — depends on the video provider or on the CARLA contingency. Without this
  package, R11's tolerance and R15's composed tolerance are **untestable** (no existing dataset,
  KITTI included, offers a synchronized occluded-convoy pair with d_AC truth), and A's clip may
  not even have C occluded.
- **KPI:** 4-item checklist above complete; ground truth sanity-checked against the scenario
  script (or measurement notes); intrinsics load successfully in the distance module.

### Vague → precise translation table (consolidated)

| Plan wording (original) | Precise, testable translation | Where |
|---|---|---|
| "agreed latency bound" (Phase 5) | E2E B-frame-capture → A-track-creation ≤ 300 ms p95, 200 ms target (A) | R14 |
| "agreed tolerance (e.g. ±15%)" (Phase 4) | per-leg: ≥ 90% of frames within ±15%, true range 10–50 m (A) | R11 |
| "composed d_AC … within the agreed tolerance" (Phase 6) | ≤ 0.15·d_AC + 2.0 m for ≥ 90% of samples, after `ref_point_offset_B` correction (A) — derivation in R15; naive "15%+15%→30%" compounding is mathematically false | R15 |
| "keeps pace with the video frame rate (or … latency target)" (Phase 3) | ≥ 10 Hz effective update rate, ≤ 100 ms mean latency, documented frame-skip allowed (A) | R10 |
| "offset within an agreed bound" (Phase 1) | \|Δt\| ≤ 50 ms (stub/NTP), ≤ 10 ms (GNSS-disciplined) (A), measured and logged | R5 |
| "starts automatically on boot" | active ≤ 30 s after cold reboot (A) | R3 |
| "broadcast … receive on the peer vehicle" | 10 Hz ± 10% rate (A); ≥ 99% delivery over 10-min bench soak (A); 100% field parse | R3, R4 |
| "reads GNSS … periodically" | 1 Hz ± 0.1 Hz (A); first 3D fix ≤ 120 s of service start (A) | R6 |
| "standard-conformant schema" | 100% field mapping to ETSI TS 103 324 CPM + CI round-trip + runtime validation | R1 |
| "no add/remove flicker at the boundary" | 0 oscillations under σ = 2 m noise sweep across 30–35 m (A) | R12 |
| "C is detected" | recall ≥ 90% (visible, ≤ 50 m); lead selection ≥ 95% on 200-frame labeled sample (A) | R9 |
| "both … display C" | ghost ≤ 2 s of B's first `tracked` (A); marker update ≤ 500 ms (A); ≥ 15 FPS (A) | R16 |

---

## 2. Feasibility study result

### Whole-input verdict

**ACHIEVABLE within 2 months for 2–3 developers** (tight-but-possible for one developer on the
plan's sequential fallback), **conditional on three early decisions**:

1. **Week-2 hardware go/no-go (F1):** the UDP-multicast stub is the *committed M1 acceptance
   path*; real modem/PC5 work proceeds only if an OBU is in hand by end of week 2 — otherwise
   R6 runs its mock-provider KPI and PC5 stays deferred (plan §6 already allows this).
2. **Week-3 demo-dataset checklist (R18/F2):** the 4-item package (two synced clips, occlusion,
   ground truth + reference convention, intrinsics) must be confirmed by week 3, else the CARLA
   task is scheduled. Without it, R11/R15 acceptance criteria are untestable.
3. **Mechanically enforced schema conformance at contract freeze (R1):** conformance to
   TS 103 324 is checked (mapping table + round-trip test + runtime validation), not asserted —
   asserted-but-unverified conformance is fake conformance and would make the M2 ASN.1 swap a
   rewrite.

Reasoning: the plan is already de-risked — contract-first, mock-then-real, three parallel tracks,
stub escape hatch. Every phase is individually a known-technology build. Critical path is the
perception track (Phases 2→3→4 ≈ 3–4 weeks), parallelizing against comms (~1 week on stub) and
display (~1 week on mock); Phase 5 integration ≈ 1 week; ~2 weeks of slack remain for gate
tuning against real monocular jitter and demo rehearsal. Nothing in the input requires
deferred-scope items (§6) — no re-scoping needed. Everything requested is implementable with
open-source, Linux-native tooling, with the license flags argued in §3(d)/(e) and listed in §4.

### Per-requirement verdicts

| R# | Verdict | Reasoning (abridged; full reasoning in §1) |
|---|---|---|
| 1, 2 | ACH | Specification work, days; ETSI references public. |
| 3, 4, 5 | ACH | UDP/JSON/chrony on a bench LAN — days, low risk. |
| **6** | **RISK** | Hardware availability unknown; QMI/AT bring-up can burn 1–2 weeks. Mitigated by F1 gate + mock-provider fallback. |
| 7, 8 | ACH | OpenCV harness + state machine, commodity work. |
| 9, 10 | ACH | Pretrained COCO car detection at ≥ 10 Hz on CPU (nano model) is commodity. |
| **11** | **RISK** | ±15% needs correct intrinsics/camera height; unachievable if calibration is missing → gated on R18. Mitigations: KITTI validation, median filter, height-prior cross-check. |
| 12, 13, 14 | ACH | Gate/relay logic + stub latency budget met ~10× over; R14 conditional on R5 ordering. |
| **15** | **RISK** | Depends on R11 twice + reference-point correction + R18 ground truth. Error budget explicit in R15. |
| 16 | ACH | Two OpenCV surfaces; trivially meets 15 FPS. |
| 17 | ACH (cond.) | Conditional on R6/R11/R18 resolving; otherwise mechanical integration. |
| **18** | **RISK** | Provider-dependent; CARLA contingency (~1 week, GPU) if the checklist fails. |

---

## 3. Technical solution analysis

All candidates pass the hard constraints (open-source, Linux) unless explicitly disqualified.
C1–C4 = the ranked criteria in
[solution-selection-criteria.md](../.claude/rules/solution-selection-criteria.md):
C1 implementability, C2 M1 speed/ease, C3 future features, C4 smaller-dependency tie-break.

### (a) Transport — serves R3, R4, R13, R14

| Candidate | License | Assessment |
|---|---|---|
| **Plain UDP multicast (OS sockets, stdlib)** | stdlib | C1: cannot fail on a bench LAN; debuggable with tcpdump/Wireshark. C2: hours. C3: connectionless best-effort broadcast is semantically the same model as PC5 — gate/expiry logic tuned on the stub stays valid on real radio. C4: zero deps. |
| ZeroMQ pub/sub (pyzmq) | MPL-2.0/BSD | Works, but adds a dependency for one small periodic datagram, and its TCP session semantics *hide* the loss behavior R12/R14 must tolerate — a worse PC5 analogue. |
| Vanetza (ETSI GeoNetworking/BTP stack) | LGPLv3 | Real ITS stack, but C++ with weeks of integration and link-layer plumbing, and **its CPM is the outdated TR 103 562 shape, not TS 103 324** ([riebl/vanetza#194](https://github.com/riebl/vanetza/issues/194)) — it does not deliver current-spec conformance. Rejected for M1 on C1/C2; see M2 note below. |
| Vendor PC5 SDK | proprietary | **Disqualified** — not open-source (hard constraint). If an OBU arrives, SDK use is a user-authorized hardware exception, never a shortlisted solution. |

**Pick: plain UDP multicast behind a ~5-function transport interface** (`send(bytes)`,
`recv()→bytes`, lifecycle). **Drivers: C1 + C2**, with C4 over ZeroMQ; C3 is bought for ~an hour
of interface code, not a heavier stack.

**M2 transport wording (binding for downstream docs):** *M2 transport TBD; Vanetza is the
leading open-source candidate, adoption conditional on TS 103 324 CPM support (tracked:
[riebl/vanetza#194](https://github.com/riebl/vanetza/issues/194)), re-evaluated at M2 kickoff.*
A name-commitment now would bind M2 to a stack that does not yet encode the normative target.

### (b) Message encoding + normative target — serves R1, R4, R13

| Candidate | Assessment |
|---|---|
| **JSON (stdlib) mirroring TS 103 324 CPM structure, enforced by pydantic + mapping table** | C1: certain. C2: hours; human-readable logs satisfy R4's "parsed into fields" for free. C3: 1:1 field mapping makes the M2 ASN.1 swap codec-only. C4: stdlib + pydantic. |
| Full ASN.1 UPER now (asn1tools MIT + ETSI CPM modules) | True wire conformance, but plan §6 explicitly defers full ASN.1; ETSI module wrangling costs days–weeks and opaque payloads slow every debug cycle — C2 loses for zero M1 acceptance gain. |
| Vanetza-provided encoding | Encodes the outdated TR 103 562 CPM — fails the conformance goal it exists to serve. |
| Protobuf / CBOR | Neither the standard encoding nor human-readable — a middle option that buys nothing. Rejected. |

**Pick: JSON mirroring TS 103 324 CPM field-for-field**, with the pydantic model as the single
source of truth (JSON Schema generated *from* the model), the R1 mapping table, runtime
validation of every message, and a CI round-trip test. **Normative target: ETSI TS 103 324 CPM
only** — its [spec](https://www.etsi.org/deliver/etsi_ts/103300_103399/103324/02.01.01_60/ts_103324v020101p.pdf)
and [ASN.1](https://forge.etsi.org/rep/ITS/asn1/cpm_ts103324) are free, so conformance is
auditable at zero cost; SAE J3224 is paywalled → unauditable under open-source-only (informative
reference only). **Drivers: C1 + C2** pick JSON (plan §2 blesses it); the mapping discipline is
the plan's own hard requirement ("the schema must be standard-conformant") made falsifiable, and
the cheap C3 insurance.

### (c) Language(s) per track — serves all

| Candidate | Assessment |
|---|---|
| **Python 3.10+ everywhere (all three tracks; modem control via qmicli subprocess)** | C1: highest — perception is Python-locked anyway (PyTorch/YOLO); one language = no cross-language schema drift. C2: fastest; shared pydantic schema classes across tracks. C3: sufficient — future Vanetza/C++ hides behind the transport interface. C4: one toolchain. |
| Python perception + C++ (standalone asio, per the small-lib principle — not Boost) comms | Two build systems + a serialization seam for zero M1 benefit: the 300 ms budget is beaten ~100× by Python UDP+JSON (single-digit ms). Better only if Vanetza lands — which is an M2 decision. |
| C++ everywhere | Perception in C++ (libtorch/ONNX) is the riskiest possible M1 path. Rejected on C1. |

**Pick: single-language Python 3.10+ mono-stack.** **Drivers: C1 + C2.** The durable asset is
the frozen R1 schema, not the transport code; an M2 C++ rewrite lives behind the same interface.

### (d) Detector — serves R9, R10 (license position included)

| Candidate | License | Assessment |
|---|---|---|
| **Ultralytics YOLO11n / YOLOv8n (pretrained COCO)** | **AGPL-3.0** ([license](https://www.ultralytics.com/license), [repo](https://github.com/ultralytics/ultralytics)) | C1: highest — plan names it; car/truck classes pretrained; huge install base. C2: 3-line API, best docs; nano model ≥ 10 Hz on CPU. |
| YOLOX-s | Apache-2.0 | Proven and permissive; repo maintenance stale since ~2022, clunkier inference API — modestly behind on C1/C2. **Pre-named fallback.** |
| RT-DETR / RF-DETR | Apache-2.0 | Good accuracy; transformer inference heavier on CPU — risks R10 without a GPU. |
| torchvision SSDlite / Faster R-CNN | BSD-3 | Works; weaker realtime ergonomics on CPU. |

**License position (explicit):** AGPL-3.0 is OSI-approved open source; the project's hard
constraint bans *commercial/paid* tools, not copyleft (Vanetza's LGPLv3 sits in the same family
uncontroversially). **Ultralytics therefore passes the hard constraints.** The real consequence
is IP-shaped and belongs to the user: AGPL is viral — if this project is later productized
proprietarily, the only exits are Ultralytics' *paid* enterprise license (disqualified: paid) or
a detector swap. **Pick: Ultralytics YOLO11n (fallback v8n), drivers C1 + C2**, with two
conditions: (i) flag F4 puts the AGPL accept/reject in the user's hands; (ii) the detector sits
behind a one-function interface (`frame → [bbox, class, conf]`) so the **YOLOX-s (Apache-2.0)
swap is a contained ~2-day change** if AGPL is rejected. R9/R10 KPIs are detector-agnostic by
design.

### (e) Monocular distance method + validation data — serves R11, R15, R18

Method:

| Candidate | Assessment |
|---|---|
| **bbox-bottom ground-plane projection (pinhole; intrinsics + camera height/pitch)** | C1: strong on the flat near-collinear convoy §2 assumes — ±10–15% typical at 10–50 m. C2: closed-form, NumPy only. C3: extends to the lateral offset R15 needs. |
| Known-height similar triangles (car prior ≈ 1.5 m) | ±15–25% due to vehicle-size variance; kept as a cheap per-frame **sanity cross-check** (~30 lines). |
| Monocular depth nets (Depth Anything / MiDaS) | Scale-ambiguous → *adds* a calibration problem; heavy. Rejected on C1/C2/C4. |

**Pick: ground-plane projection primary + similar-triangles cross-check. Drivers: C1 + C2.**

Validation data — **two distinct needs, deliberately split**:

| Need | Source | Rationale |
|---|---|---|
| Per-leg validation (R9, R11) | **KITTI** (raw clips + calib + LiDAR ground truth) | Download-and-go: real imagery/noise, intrinsics and ground-truth range included — hours, not days (C2). **License position:** KITTI is CC BY-NC-SA; acceptable because it is an *evaluation instrument*, never a shipped or linked component — nothing KITTI-derived ships. Flagged (F5) for the user; CARLA becomes the validation source if rejected. |
| Composed-d_AC + demo clips (R15, R17, R18) | **CARLA** (MIT) — via the R18 checklist trigger | No existing dataset (KITTI included) provides two synchronized clips of one occluded convoy with d_AC ground truth; CARLA scripts exactly that and exports truth + intrinsics. Required *unless* the provided video package passes R18's 4-item checklist (C1: without it, Phase 6's tolerance criterion is untestable). |

### (f) Display / BEV — serves R16

| Candidate | License | Assessment |
|---|---|---|
| **OpenCV drawing for both camera overlay and BEV canvas** | Apache-2.0 | Already a project dependency (R7); rectangles/circles/text at 30+ FPS; a BEV is a blank canvas with two markers and range rings. |
| matplotlib animation | BSD-compat | Interactive redraw is a few FPS — **risks R16's 15 FPS floor**. Rejected on C1. |
| Rerun viewer | MIT/Apache-2.0 | Capable but a new heavyweight tool for two markers — C4 loses. |
| Foxglove | not fully open | **Disqualified** — no longer fully open-source. |

**Pick: OpenCV for both surfaces. Drivers: C2 + C4** — zero new dependencies; the plan itself
defers a production GUI.

### (g) Service management — serves R3

| Candidate | Assessment |
|---|---|
| **systemd unit** | Present on every target Linux (real OBU images included); the plan's acceptance criterion is phrased in systemd terms; one 10-line unit file; zero install. |
| supervisord | Itself needs a boot-start mechanism (…systemd) — circular; strictly worse. |
| Docker + restart policy | Container adds GPU/device passthrough friction for perception; packaging is a later nice-to-have. |
| Adaptive AUTOSAR Execution Management | **Effectively disqualified** — no viable open-source Adaptive EM exists; commercial offerings are paid. The plan's "or Adaptive AUTOSAR EM" branch is unreachable under the hard constraints → **propose striking it** (flag F6); plan-text cleanup, not a capability loss. |

**Pick: systemd. Drivers: C1 + C2 + C4 unanimously.**

### (h) Timebase tooling — serves R5, R14, R16

| Candidate | Assessment |
|---|---|
| **chrony (+ gpsd SHM on the hardware path; NTP peer mode host-to-host on the stub path)** | One tool covers both paths; good step/jitter behavior; tracking report gives the R5 offset KPI directly. |
| ntpd | Older, slower convergence, no advantage. |
| systemd-timesyncd | SNTP client only — cannot peer two hosts or ingest gpsd. Rejected on C1. |
| linuxptp (PTP) | Sub-µs precision is overkill for a 50 ms bound; NIC hardware-timestamp dependency. Rejected on C4. |

**Pick: chrony (+ gpsd when GNSS exists). Drivers: C1 + C2** (C4 over PTP).

### Stack summary (per track)

| Track | Stack |
|---|---|
| Contracts | JSON schema normatively mapped to ETSI TS 103 324 CPM; pydantic models (single source of truth, JSON Schema generated from them); versioned schema doc + mapping table with reference-point semantics |
| Comms | Python 3.10+, stdlib UDP multicast behind a swappable transport interface, chrony (+gpsd) timebase, systemd service; modem via qmicli subprocess (hardware path) |
| Perception | Python, OpenCV `VideoCapture` (PyAV only if timestamp precision forces it), Ultralytics YOLO11n/v8n (AGPL — flag F4; YOLOX-s pre-named fallback), NumPy ground-plane distance + height-prior cross-check, YAML-config gate constants |
| Display | OpenCV overlay + OpenCV BEV canvas |
| Validation/demo data | KITTI (per-leg, flag F5) + CARLA (composed + demo clips, via R18 checklist trigger) |

---

## 4. Proposed scope/requirement changes & flags (user accept/reject)

- **F1 — Week-2 hardware gate.** Commit to the UDP stub as the *M1 acceptance path*; real
  modem/PC5 work (R6 hardware KPI) proceeds only if an OBU is in hand by end of week 2 (A);
  otherwise R6 runs its mock-provider KPI and PC5 stays deferred (plan §6 allows this). Prevents
  the comms track from stalling convergence. Also note: vendor PC5 SDKs are proprietary — any
  use is a user-authorized hardware exception to the open-source constraint, decided by the
  user, never silently absorbed.
- **F2 — Demo-dataset checklist trigger (R18).** The plan says video is "provided". The CARLA
  contingency task (~1 week, GPU machine) fires at week 3 unless the provided package contains
  **all four**: (i) two synchronized clips (A-view, B-view) from one convoy; (ii) C occluded
  from A for the demo duration; (iii) per-frame ground-truth d_AB/d_BC/d_AC with declared
  reference-point convention; (iv) intrinsics for both cameras. A video that exists but fails
  the checklist still fires the contingency — otherwise R11/R15 are untestable.
- **F3 — GNSS→chrony/NTP timebase fallback.** Without modem GNSS, R5's timebase is chrony/NTP
  at ≤ 50 ms (A). This satisfies "common timebase" for the demo; GNSS-disciplined (≤ 10 ms)
  returns with hardware. Precision-of-requirement change, not scope change.
- **F4 — AGPL-3.0 detector: accept or swap.** Ultralytics YOLO is open source (AGPL-3.0) and
  the C1/C2 pick, but virally binds the repo if distributed. Accept (project releasable under
  AGPL-3.0) or reject → pick switches to **YOLOX-s (Apache-2.0)**, ~2-day contained swap behind
  the detector interface.
- **F5 — KITTI is CC BY-NC-SA.** Used only as evaluation data — an instrument, never a shipped
  or linked component. Accept that reading, or reject → CARLA becomes the validation source too
  (folds into F2's task).
- **F6 — Strike the Adaptive AUTOSAR EM branch** of Phase 1: no open-source Adaptive EM exists,
  so the branch is unreachable under the hard constraints; systemd is the M1 mechanism.
  Plan-text cleanup, not a capability loss.
- **F7 — All (A)-marked numbers are proposals**, none verbatim in the plan: 10 Hz ± 10% rate;
  ≥ 99% delivery / 10-min soak; ≤ 50 ms NTP / ≤ 10 ms GNSS offset; 30 s boot; 120 s GNSS fix;
  1 Hz GNSS log; ≥ 10 Hz / ≤ 100 ms perception; recall ≥ 90% (≤ 50 m) and lead ≥ 95% / 200
  frames; ±15% per-leg (10–50 m, ≥ 90% frames); σ = 2 m flicker sweep; 300 ms p95 (200 ms
  target) E2E; 0.15·d_AC + 2.0 m ≥ 90% composed; 2 s ghost / 500 ms marker / 15 FPS display;
  week-2 and week-3 gates. Confirm or veto any individually.
- **F8 — Confirm CPM (ETSI TS 103 324), not SDSM (SAE J3224), as the schema's normative base.**
  ETSI's spec and ASN.1 are freely accessible → conformance auditable at zero cost under
  open-source-only; SAE is paywalled → unauditable. SDSM stays an informative reference.

---

## 5. Appendix — adversarial review record

Five contested points between Researcher Alpha (pragmatist) and Researcher Beta
(standards/extensibility), and how each resolved — recorded so downstream readers know why
these numbers/picks look the way they do.

1. **Schema conformance mechanism & target.** Alpha: JSON + pydantic + "CPM/SDSM" mapping
   table. Beta: CPM TS 103 324 as *sole* normative target (SDSM paywalled → unauditable) +
   CI validation. **Resolution:** pydantic-at-runtime accepted as *stronger* than CI-only
   (round-trip test still runs in CI); normative target narrowed to TS 103 324 only;
   reference-point semantics added to R1; JSON Schema generated from the pydantic model
   (single source of truth).
2. **Validation data.** Alpha: KITTI primary, CARLA only if "no video provided". Beta: CARLA
   effectively required — a video can be *provided* yet unusable. **Resolution:** Beta's 4-item
   checklist (sync'd pair, occlusion, ground truth + reference convention, intrinsics) became
   R18 and F2's trigger; Alpha's KITTI-for-per-leg stands; "CARLA unconditional" was withdrawn.
3. **Composed error budget.** Alpha: flat ±20% "because two ±15% legs compound". Beta: the
   compounding claim is mathematically false (convex combination — composed relative error is
   bounded by the worse leg for any error signs); the width is needed for *additive* terms:
   systematic reference-point bias (~3–4 m — alone breaks a plain ±20% at 50 m worst-case) and
   temporal skew (~1.75 m). **Resolution:** Alpha adopted Beta's KPI verbatim —
   0.15·d_AC + 2.0 m for ≥ 90% of samples after a configured `ref_point_offset_B` correction —
   with the derivation recorded in R15.
4. **Vanetza M2 commitment.** Alpha offered to name Vanetza the committed M2 transport as a
   concession. Beta declined it: Vanetza's CPM is the outdated TR 103 562 shape
   ([vanetza#194](https://github.com/riebl/vanetza/issues/194)); a name-commitment would be
   asserted-not-verified conformance. **Resolution:** both adopted "M2 transport TBD; Vanetza
   leading candidate, conditional on TS 103 324 support, re-evaluated at M2 kickoff."
5. **Numbers.** Diverged on latency (Alpha 200 ms p95 / Beta 300 ms), delivery (95%/60 s vs
   99%/10 min), and the ghost KPI (each initially adopted the other's). **Resolution:** 300 ms
   p95 with 200 ms target (Alpha's 200 ms p95 acceptance bound failed its own budget arithmetic
   once detection-p95 and ±50 ms clock-offset measurement uncertainty are included);
   ≥ 99% / 10-min soak (bench LAN; longer soak exposes service defects); ghost KPI = Alpha's
   judge-visible ≤ 2 s from B's first `tracked` transition, with Beta's ≤ 1 s-from-first-relay
   retained as a diagnostic only. Alpha's detection/lead-selection KPIs and σ = 2 m flicker
   sweep were endorsed as superior and adopted.

### Key external sources

- [riebl/vanetza issue #194](https://github.com/riebl/vanetza/issues/194) — Vanetza CPM is TR 103 562, TS 103 324 not merged.
- [ETSI TS 103 324 CPM ASN.1 (ETSI forge)](https://forge.etsi.org/rep/ITS/asn1/cpm_ts103324) and [spec PDF](https://www.etsi.org/deliver/etsi_ts/103300_103399/103324/02.01.01_60/ts_103324v020101p.pdf) — freely accessible normative references.
- [Ultralytics license page](https://www.ultralytics.com/license) and [repo](https://github.com/ultralytics/ultralytics) — AGPL-3.0.
- [YOLOX (Megvii)](https://github.com/Megvii-BaseDetection/YOLOX) — Apache-2.0 fallback detector.
