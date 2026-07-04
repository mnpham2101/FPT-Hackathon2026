# Researcher Alpha (Pragmatist) — Round-1 Position Paper
## Requirement Analysis & Solutioning — Milestone 1: Cooperative Vehicle Awareness (A ← B ← C)

Input: the entire `milestone1.md` feature set. Procedure followed:
`requirement-analysis-and-solutioning/SKILL.md` steps 1–4. Rules honored:
`requirement-quality-criteria.md`, `solution-selection-criteria.md`.
Debate stance: criteria #1 (highest chance of working end-to-end) and #2 (fastest path to M1
acceptance) weighted heavily; #3 satisfied by cheap abstractions, never by heavier stacks.

---

## 1. Whole-input feasibility verdict

**ACHIEVABLE** within 2 months for 2–3 developers (tight-but-possible for 1 dev on the
sequential fallback), **conditional on three things the plan already permits**:

1. **Transport runs on the UDP stub** unless a real C-V2X OBU physically arrives by ~week 2.
   Real modem bring-up (QMI/AT, PC5 config, vendor stack) is the single biggest schedule
   unknown and is explicitly stubbable per §2 of the plan.
2. **The demo video is provided** (recorded or simulator) per §2. If it is NOT provided,
   generating an A-B-C occlusion convoy clip (CARLA scenario scripting) is a ~1-week unbudgeted
   task — flagged below.
3. **GNSS timebase** degrades gracefully to NTP/chrony when no modem GNSS exists (bench/stub
   setup). Flagged as a precision-of-requirement change, not a scope change.

Reasoning: the plan is already contract-first with mock-then-real convergence; every phase is
individually small (days, not weeks) once the stack picks below are accepted. The critical path
is the perception track (Phases 2→3→4 ≈ 3–4 weeks), which parallelizes cleanly against comms
(Phase 1 ≈ 1 week on stub) and display (Phase 6 ≈ 1 week on mock). Phase 5 integration ≈ 1
week. That leaves ~2 weeks of slack for tuning (gate constants vs. real monocular jitter) and
demo rehearsal. Nothing in the input requires deferred-scope items (§6) — no re-scoping needed.

Hard-constraint check on the input itself: everything requested is implementable with
open-source, Linux-native tooling — with two license flags (AGPL detector, non-commercial KITTI
eval data) argued in §4 and listed in §5 for user decision.

---

## 2. Enumerated requirements (project-global numbering, 1..17)

Ordering used: **urgency** — contract-first order of the plan itself. Requirements 1–2 block
everything (frozen contracts); then per-track in phase order; 17 is the capstone
definition-of-done. Verdicts: ACH = achievable, RISK = at-risk.

Every "agreed X" in the plan is translated to a proposed number below and **marked (A) =
assumption to confirm** — the user can veto any of them.

### Contracts (Step 0)

**R1 — Frozen V2X message schema.** A versioned schema document + reference implementation of
the perceived-object message, CPM (ETSI TS 103 324) / SDSM (SAE J3224)-shaped: `stationId`,
`generationDeltaTime`, sender reference position/time, perceived-object entry (`objectId`,
relative position/distance, `classification`, `confidence`). Field names and nesting mirror the
standard containers so a later ASN.1 codec swap changes no field semantics.
- Verdict: **ACH** — pure specification work, days.
- KPI: schema file is version-tagged v1.0; an encode→decode round-trip unit test reproduces
  100% of fields bit-exact; a checklist maps every schema field to its CPM/SDSM counterpart
  (no unmapped required field).
- Vague→precise: "standard-conformant schema" → *every field traceable 1:1 to a CPM
  perceived-object-container or SDSM DetectedObjectData field, verified by a written mapping
  table*.

**R2 — Frozen TrackedObject struct.** The 9-field struct of plan §4 (`id, class, source,
first_seen, last_seen, bbox, distance, confidence, state`) implemented as a typed model shared
by all perception sub-phases.
- Verdict: **ACH**.
- KPI: unit test constructs the struct with all fields, exercises all three `state` values and
  both `source` values; struct is import-shared (single definition) by detection, distance,
  gate, and relay code — verified by one-definition check.

### Comms track (Phase 1)

**R3 — V2X broadcast service, auto-start.** A Linux service auto-starts on boot, initializes
the transport, and broadcasts the full R1 schema with mock object contents at 10 Hz (A).
- Verdict: **ACH** on stub. (Real-modem variant is R6.)
- KPI: after a cold reboot, `systemctl is-active` reports active within 30 s (A); observed
  message rate 10 Hz ± 10% over a 60 s window.
- Vague→precise: "runs the broadcast loop" → *10 Hz sustained, rate error ≤ 10% over 60 s* (A).
  10 Hz chosen to match BSM/CAM upper rate so the stub is rate-realistic.

**R4 — Peer reception + full-field parse.** The receiving vehicle logs every received message
parsed into 100% of schema fields, both directions (A↔B).
- Verdict: **ACH**.
- KPI: over a 60 s bench run, ≥ 95% of sent messages are received (A — stub on bench LAN/Wi-Fi;
  loss is tolerated by design, this bounds a broken link) and every received message logs all
  schema fields with correct units/ranges (range-check assertions, 0 violations).

**R5 — Common timebase.** Both hosts' timestamps are comparable within a bound.
- Verdict: **ACH**.
- KPI: measured clock offset between the two hosts ≤ **50 ms** (A) sustained over 10 min —
  via chrony/NTP on the stub; ≤ 10 ms if GNSS-disciplined (A).
- Vague→precise: "offset within an agreed bound" → *|Δt| ≤ 50 ms (stub/NTP), ≤ 10 ms (GNSS)* (A).

**R6 — Modem bring-up + GNSS logging (hardware-conditional).** If a C-V2X modem is available:
QMI/AT identity/firmware/V2X-mode/GNSS-fix query with startup log, and GNSS position+time
logged at 1 Hz (A). If not: a mock GNSS provider feeds the same log format.
- Verdict: **RISK** — entirely gated on hardware availability, unknown today. Mitigation:
  hardware go/no-go decision at end of week 2 (A); stub path keeps every other requirement
  green.
- KPI (hardware): startup log contains modem model, firmware version, V2X mode string, and a
  3D GNSS fix within 120 s (A) of service start; position logged at 1 Hz ± 0.1 Hz.
  KPI (stub): identical log format emitted from the mock provider, verified by the same parser.

### Perception track (Phases 2–4)

**R7 — Video harness.** The provided video opens; container/codec/fps/resolution are detected
and logged; frames stream with per-frame timestamps.
- Verdict: **ACH**.
- KPI: 100% of frames delivered exactly once; per-frame timestamps strictly monotonic;
  metadata log matches `ffprobe` output for the same file.

**R8 — TrackedObject store + gate state machine (mock-driven).** Store exposes all R2 fields;
`not_tracked → tentative → tracked → expired` manager runs off an injected synthetic C.
- Verdict: **ACH**.
- KPI: with mock on, log shows one full lifecycle cycle with timestamps; with mock off, zero
  tracks created over the whole clip (binary check); state transitions obey
  `confirm_hits`/`miss_limit` exactly in a scripted-input unit test (0 deviations).

**R9 — Detection of C + lead selection.** Pretrained detector finds C as `car`/`truck`; the
in-lane lead (largest central box) is selected when multiple vehicles are present; bboxes are
written to the store.
- Verdict: **ACH** — pretrained COCO, no training (per §2).
- KPI: C detected in ≥ 90% (A) of frames where C is visible and within 50 m (A); lead-selection
  correct in ≥ 95% (A) of multi-vehicle frames on a hand-labeled 200-frame sample; mock path
  disabled (binary).

**R10 — Detection throughput.** Perception keeps pace with the demo.
- Verdict: **ACH** (nano model on CPU meets it; GPU is margin, not a dependency).
- KPI: detection update rate ≥ 10 Hz (A) with mean per-frame latency ≤ 100 ms (A) on the demo
  machine; frame-skip policy allowed as long as update rate holds.
- Vague→precise: "keeps pace with the video frame rate (or agreed latency target)" →
  *≥ 10 Hz update rate, ≤ 100 ms mean latency* (A) — 10 Hz matches the R3 message rate, so
  perception is never the relay bottleneck.

**R11 — Monocular distance estimation.** Per-frame range to the lead computed from the bbox
(ground-plane projection primary), written back to the track.
- Verdict: **ACH** for the ±15% band on calibrated data; would be RISK if the provided demo
  video ships without intrinsics — flagged in §5.
- KPI: ≥ 90% (A) of frames within **±15%** of ground truth for true range 10–50 m (A) on a
  KITTI (or CARLA) clip with known intrinsics; distance present on the track every frame C is
  detected.
- Vague→precise: "agreed tolerance (e.g. ±15%)" → *adopted as ±15%, scoped to 10–50 m and
  ≥ 90% of frames* (A).

**R12 — Active proximity gate, externalized constants.** Admission only when ≤ `gate_enter`
(30 m) for `confirm_hits` (3) consecutive frames; drop only beyond `gate_exit` (35 m) or after
`miss_limit` (5) misses. All four constants in a config file.
- Verdict: **ACH**.
- KPI: on a synthetic approach/retreat distance sweep with injected Gaussian noise σ = 2 m (A),
  exactly one admit and one drop event (0 flicker oscillations); changing a constant in config
  alters behavior with no code change/rebuild (binary); static check finds no gate literals in
  code.
- Vague→precise: "no add/remove flicker at the boundary" → *0 state oscillations under a
  σ = 2 m noise sweep across the 30–35 m band* (A).

### Convergence (Phase 5)

**R13 — Real-data relay gated on track state.** B's broadcast carries real Phase-4 data for C
and includes C **only while C is `tracked`**.
- Verdict: **ACH**.
- KPI: no mock code on the transmit path (binary, by config/audit); relay starts ≤ 1 message
  period (100 ms @ 10 Hz) after C enters `tracked` and stops ≤ 1 period after C leaves;
  transmitted perceived-object fields match the store values for the same timestamp (log diff
  = 0).

**R14 — Relayed-track admission on A + E2E latency.** A decodes and creates a
`source = v2x_relayed` track carrying B's reference position/time; end-to-end latency within
bound.
- Verdict: **ACH** on stub.
- KPI: latency from B's frame timestamp (detection) to A's relayed-track creation ≤ **200 ms**
  (A) at p95 over a 60 s run; relayed track exposes B's reference position/time (field-presence
  check); A drops the relayed track after `miss_limit` missed periods (mirrors R12).
- Vague→precise: "agreed latency bound" → *≤ 200 ms p95, B-detection→A-track* (A). Budget:
  ≤ 100 ms detection + ≤ 5 ms encode/send/decode on stub + one 100 ms message period.

### Display track (Phase 6) + capstone

**R15 — Distance composition on A.** A measures `d_AB` locally, composes
`d_AC = d_AB + d_BC` (lateral offsets component-wise).
- Verdict: **ACH**.
- KPI: composed `d_AC` within **±20%** (A) of ground truth for the demo geometry — wider than
  R11's ±15% because two ±15% monocular errors compound; unit test with mock inputs verifies
  the composition arithmetic exactly.
- Vague→precise: "matches ground truth within the agreed tolerance" → *±20% composed* (A).

**R16 — Dual display: direct C on B, ghost C on A's BEV.** B renders C from direct detection
with relative position; A renders C as an occluded "ghost" marker on a bird's-eye view. Built
against a mock message first (per plan), integrated with real data at the end.
- Verdict: **ACH**.
- KPI: BEV marker for C updates ≤ 500 ms (A) after message receipt on A; C appears on A's BEV
  within 2 s (A) of B's first `tracked` admission; dev-mode mock rendering passes before
  integration (binary); display loop sustains ≥ 15 FPS (A).

**R17 — End-to-end demo (Definition of Done).** With no mocks anywhere in the path, C appears
on A's display purely via B's V2X relay while A's own perception never detects C.
- Verdict: **ACH**, conditional on R6/R11 flags resolving.
- KPI: one continuous recorded demo run in which (a) A's detector log contains zero direct
  detections of C, (b) A's BEV shows the ghost C sourced `v2x_relayed`, (c) all six phases'
  acceptance checklists are ticked. Binary pass/fail.

---

## 3. Technical solution analysis (decision points a–g)

All candidates below pass the hard constraints (open-source, Linux) unless explicitly
disqualified. "C1–C4" = the four ranked criteria (implementability, M1 speed, extensibility,
smaller-dependency tie-break).

### (a) Transport — serves R3, R4, R13, R14

| Candidate                                           | License                                          | C1 works?                                                            | C2 M1 speed                                      | C3 future                                                   | C4 size                                                                                                                                |
| --------------------------------------------------- | ------------------------------------------------ | -------------------------------------------------------------------- | ------------------------------------------------ | ----------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------- |
| **Plain UDP broadcast/multicast (OS sockets)**      | n/a (stdlib)                                     | Certain — datagrams on a bench LAN                                   | Hours; debuggable with tcpdump/Wireshark         | PC5 is also connectionless datagrams → semantics carry over | Zero deps                                                                                                                              |
| ZeroMQ pub/sub (libzmq/pyzmq)                       | MPL-2.0 / LGPL+exc / BSD                         | Certain                                                              | Fast, but adds broker-less TCP session semantics | Fine                                                        | One dep we don't need                                                                                                                  |
| Vanetza (ETSI ITS stack, GeoNetworking/BTP + ASN.1) | LGPLv3, actively maintained (nfiniity-sponsored) | Real but weeks of C++ integration; needs raw sockets/ITS-G5-ish link | Slow for M1                                      | Excellent (real CAM/CPM, §6 items)                          | Heavy                                                                                                                                  |
| Vendor PC5 SDK                                      | Proprietary                                      | —                                                                    | —                                                | —                                                           | **Disqualified**: not open-source. If an OBU arrives, its SDK use is a user-authorized hardware exception, not a shortlisted solution. |

**Pick: plain UDP multicast, behind a ~5-function transport interface (`send(bytes)`,
`recv()→bytes`, lifecycle).** Driven by **C1 + C2**: it cannot fail, it is transparent to
debug, and — decisively — connectionless best-effort broadcast is *semantically the same model
as PC5*, so gate/expiry logic tuned on the stub stays valid on real radio. ZeroMQ's TCP pub/sub
actively hides the loss behavior R12/R14 must tolerate. C3 is bought for free by the tiny
interface: Vanetza or a real OBU slots in as another implementation later. C4: zero
dependencies beats one.

### (b) Message encoding — serves R1, R4, R13

| Candidate                                                               | C1                                                                                    | C2                                                                    | C3                                                               | C4                              |
| ----------------------------------------------------------------------- | ------------------------------------------------------------------------------------- | --------------------------------------------------------------------- | ---------------------------------------------------------------- | ------------------------------- |
| **JSON (stdlib) with CPM/SDSM-mirrored schema + validation (pydantic)** | Certain                                                                               | Hours; human-readable logs satisfy R4's "parsed into fields" for free | Field names map 1:1 to CPM/SDSM → ASN.1 swap is codec-only       | stdlib + pydantic               |
| ASN.1 UPER now (asn1tools, MIT; ETSI CPM modules)                       | Works but ETSI module wrangling is days–weeks; opaque payloads slow every debug cycle | Slow                                                                  | Best                                                             | Small lib, big integration cost |
| Protobuf/CBOR                                                           | Certain                                                                               | Fast                                                                  | Neutral — *neither* standard ASN.1 *nor* readable; worst of both | Extra dep                       |

**Pick: JSON with a schema that mirrors CPM/SDSM field-for-field, enforced by a pydantic model
and the R1 mapping table.** Driven by **C1 + C2**; the plan (§2) explicitly blesses JSON for M1
and defers full ASN.1 to §6. The standards obligation is on the **schema**, and R1's KPI (1:1
mapping table + round-trip test) enforces exactly that. Protobuf is rejected as a middle option
that buys nothing either side wants.

### (c) Language(s) per track — serves all

| Candidate                                       | C1                                                                                                                                                     | C2                                                    | C3                                                               | C4             |
| ----------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------ | ----------------------------------------------------- | ---------------------------------------------------------------- | -------------- |
| **Python 3.10+ everywhere (all three tracks)**  | Highest — perception is Python-locked anyway (PyTorch/YOLO); one language = no cross-language schema drift                                             | Fastest; shared pydantic schema classes across tracks | Enough — future Vanetza/C++ hides behind the transport interface | One toolchain  |
| Python perception + C++ (standalone asio) comms | Works, but two build systems + a serialization boundary for zero M1 benefit; 200 ms budget makes C++ performance irrelevant (Python UDP+JSON ≈ 1–5 ms) | Slower                                                | Better only if Vanetza lands in M2                               | Two toolchains |
| C++ everywhere                                  | Perception in C++ (libtorch/ONNX) is the riskiest possible M1 path                                                                                     | Slowest                                               | —                                                                | —              |

**Pick: single-language Python 3.10+ mono-stack.** Driven by **C1 + C2**: perception has no
realistic non-Python option at this timeline, and splitting comms into C++ adds an integration
seam (the historical top killer of multi-track hackathon projects) to beat a latency budget we
already beat by ~100×. C3 note: if M2 adopts Vanetza, the comms track is rewritten behind the
same transport interface — R1's schema, not the code, is the durable asset.

### (d) Detector — serves R9, R10 (+ license position)

| Candidate                              | License      | C1                                                                           | C2                                                   | C3                         | C4                  |
| -------------------------------------- | ------------ | ---------------------------------------------------------------------------- | ---------------------------------------------------- | -------------------------- | ------------------- |
| **Ultralytics YOLOv8n/YOLO11n (COCO)** | **AGPL-3.0** | Highest — plan names it; car/truck classes pretrained; enormous install base | 3-line API, best docs; nano model does ≥10 Hz on CPU | Fine (tracking, seg later) | Medium dep          |
| YOLOX-s                                | Apache-2.0   | Proven but repo maintenance stale since ~2022; clunkier inference API        | Moderate                                             | OK                         | Medium              |
| RT-DETR / RF-DETR                      | Apache-2.0   | Good accuracy; transformer inference heavier on CPU — risks R10 without GPU  | Moderate                                             | Good                       | Heavier             |
| torchvision SSDlite/Faster R-CNN       | BSD-3        | Works; weaker realtime ergonomics, slower on CPU                             | Moderate                                             | OK                         | Already-present dep |

**License position (explicit, per debate brief): AGPL-3.0 is acceptable under this project's
hard constraint.** The constraint text bans *commercial or paid* tools and demands open source;
AGPL-3.0 is an OSI-approved open-source license, and Vanetza's LGPLv3 sits in the same copyleft
family without controversy. Compliance path: this hackathon project can itself be released
under AGPL-3.0. The *real* consequence is criterion-3-shaped and belongs to the user, not to
the license checkbox: **AGPL is viral — if FPT later productizes this proprietarily, the only
exits are Ultralytics' paid enterprise license (disqualified: paid) or swapping the detector.**
Therefore: **Pick Ultralytics YOLO11n (fallback v8n), driven by C1 + C2, with YOLOX-s
(Apache-2.0) as the pre-named swap** — the detector touches one module (bbox in, per R2's
seam), so the swap cost is ~2 days and the risk is contained. Flag F4 (§5) puts the
accept/reject in the user's hands.

### (e) Monocular distance method + validation dataset — serves R11, R15

Method:

| Candidate                                                                                 | C1                                                                          | C2                      | C3                                        | C4        |
| ----------------------------------------------------------------------------------------- | --------------------------------------------------------------------------- | ----------------------- | ----------------------------------------- | --------- |
| **bbox-bottom ground-plane projection (pinhole; needs intrinsics + camera height/pitch)** | Strong on flat-road convoy (our exact geometry); ±10–15% typical at 10–50 m | Closed-form, NumPy only | Extends to lateral offset (needed by R15) | Zero deps |
| Known-height/width similar triangles (H·f/h_px, car prior ≈ 1.5 m)                        | Works; ±15–25% due to vehicle-size variance                                 | Trivial                 | Weak lateral story                        | Zero deps |
| Monocular depth net (Depth Anything/MiDaS)                                                | Scale-ambiguous → needs calibration anyway; heavy                           | Slow                    | Overkill                                  | Big dep   |

**Pick: ground-plane projection primary, similar-triangles as a per-frame sanity cross-check
(cheap, both are ~30 lines).** Driven by **C1** (best accuracy for the flat near-collinear
convoy that §2 assumes) and **C2**. Depth networks rejected: they *add* a calibration problem.

Validation dataset:

| Candidate                                          | License                      | C1/C2                                                                                                         |
| -------------------------------------------------- | ---------------------------- | ------------------------------------------------------------------------------------------------------------- |
| **KITTI (raw clips + calib + LiDAR ground truth)** | CC BY-NC-SA (non-commercial) | Download-and-go; real intrinsics + GT distance ready; zero setup                                              |
| CARLA (simulator)                                  | MIT (code)                   | GT and intrinsics perfect and scriptable, but a multi-GB install needing a real GPU + scenario-scripting time |

**Pick: KITTI for R11 validation** (C2 — hours vs. days), with the explicit position that
CC BY-NC-SA evaluation data is acceptable because it is *measurement input*, never a shipped or
linked component — flagged (F5) for the user since "open-source only" could be read broader.
**CARLA is the named contingency for two cases:** (i) user rejects KITTI's NC license,
(ii) no A-B-C demo video is provided and we must synthesize one (F2) — CARLA then does double
duty (demo clip + ground truth for R15's ±20% check).

### (f) Display / BEV — serves R16

| Candidate                                                 | License                  | C1                                                               | C2                                                               | C3                              | C4               |
| --------------------------------------------------------- | ------------------------ | ---------------------------------------------------------------- | ---------------------------------------------------------------- | ------------------------------- | ---------------- |
| **OpenCV drawing for both camera overlay and BEV canvas** | Apache-2.0               | Certain; 30+ FPS trivially                                       | Already a project dependency (video I/O) — zero new code surface | "Production GUI later" per plan | **No new dep**   |
| matplotlib animation BEV                                  | PSF-ish (BSD-compatible) | Works but interactive redraw ~few FPS → risks R16's 15 FPS       | Fast to write                                                    | Poor realtime                   | New dep for less |
| Rerun viewer                                              | MIT/Apache-2.0           | Nice, but new tool to learn + heavyweight viewer for two markers | Moderate                                                         | Good                            | Unneeded surface |
| Foxglove                                                  | No longer fully open     | —                                                                | —                                                                | —                               | **Disqualified** |

**Pick: OpenCV for both surfaces.** Driven by **C2 + C4**: it is already in the dependency set
for R7, draws rectangles/circles/text at video rate, and a BEV is literally a blank canvas with
two markers and range rings. The plan itself defers a production GUI.

### (g) Service management — serves R3

| Candidate                       | C1                                                                                                 | C2                    | C3                                    | C4             |
| ------------------------------- | -------------------------------------------------------------------------------------------------- | --------------------- | ------------------------------------- | -------------- |
| **systemd unit**                | Present on every target Linux; acceptance criterion is phrased in systemd terms ("service status") | One 10-line unit file | Real OBU images are systemd-based too | Zero install   |
| supervisord                     | Works, but itself needs a boot-start mechanism (…systemd) → circular                               | Extra dep             | Neutral                               | Strictly worse |
| Docker + compose restart policy | Works; container adds GPU/device-passthrough friction for perception                               | Slower                | Packaging nice-to-have later          | Heavy          |
| Adaptive AUTOSAR Execution Mgmt | No viable open-source Adaptive EM stack exists; commercial offerings **disqualified** (paid)       | —                     | —                                     | —              |

**Pick: systemd.** Driven by **C1 + C2 + C4** simultaneously — it is the null-cost option that
the acceptance criteria were written against. The plan's "or Adaptive AUTOSAR EM" branch is
unreachable under the open-source hard constraint and is flagged (F6), not silently absorbed.

---

## 4. Stack summary (per track)

| Track      | Stack                                                                                                                                                                                                        |
| ---------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Contracts  | JSON schema mirrored on ETSI CPM / SAE J3224 SDSM; pydantic models; versioned schema doc + mapping table                                                                                                     |
| Comms      | Python 3.10+, stdlib UDP multicast behind a swappable transport interface, chrony for timebase, systemd service                                                                                              |
| Perception | Python, OpenCV `VideoCapture` (PyAV only if timestamp precision forces it), Ultralytics YOLO11n/v8n (AGPL — flagged; YOLOX-s named fallback), NumPy ground-plane distance, config-file gate constants (YAML) |
| Display    | OpenCV overlay + OpenCV BEV canvas                                                                                                                                                                           |
| Validation | KITTI clips + calib (flagged NC license); CARLA (MIT) contingency for demo-video synthesis                                                                                                                   |

---

## 5. Proposed scope/requirement changes & risk flags (for user accept/reject)

- **F1 — Hardware gate (decision by end of week 2, assumption):** commit to the UDP stub as
  the *M1 acceptance path*; real modem/PC5 work (R6 hardware KPI) proceeds only if an OBU is
  in hand by then, otherwise R6 runs its mock-provider KPI and PC5 stays deferred (§6 already
  allows this). Prevents the comms track from stalling all convergence.
- **F2 — Demo video contingency:** §2 says video is provided. If not confirmed by week 3,
  add a CARLA scenario-recording task (~1 week, needs a GPU machine). Silent risk otherwise.
- **F3 — GNSS→NTP fallback:** without modem GNSS, R5's timebase is chrony/NTP at ≤ 50 ms.
  This satisfies "common timebase" for the demo; GNSS-disciplined time returns with hardware.
- **F4 — AGPL-3.0 detector:** Ultralytics YOLO is open-source (AGPL) and my pick on C1/C2,
  but it virally binds the repo if distributed. Accept (open-source the project) or reject
  (I switch the pick to YOLOX-s, Apache-2.0, +~2 days integration and slightly worse docs).
- **F5 — KITTI is CC BY-NC-SA:** used only as evaluation data, never shipped. Accept that
  reading, or reject → CARLA becomes the validation source (+setup cost, folds into F2).
- **F6 — Adaptive AUTOSAR EM branch of Phase 1 is unreachable** under "open-source only"
  (no open Adaptive EM exists). Proposal: strike it; systemd is the M1 mechanism. This is a
  plan-text cleanup, not a capability loss.
- **F7 — All numeric values marked (A)** in §2 (10 Hz rate, 50 ms offset, 10 Hz/100 ms
  perception, ±15% / ±20% tolerances, 200 ms E2E p95, 500 ms/2 s display, σ=2 m flicker test,
  90/95% detection rates) are proposed defaults awaiting confirmation — none exist verbatim
  in the plan doc.

---

## 6. Anticipated attacks from Beta + pre-emptive defenses

1. **"JSON betrays the V2X standards story."** Defense: the plan (§2) explicitly authorizes
   JSON for M1 and defers full ASN.1 (§6); the *schema* is where conformance lives, and R1's
   KPI enforces a 1:1 CPM/SDSM field mapping table + round-trip test. Adopting ASN.1 UPER now
   costs days-to-weeks of ETSI module integration and makes every debug session hex-diving,
   for zero M1 acceptance gain. If Beta wants ASN.1-readiness, R1 already *is* that.
2. **"Use Vanetza, not a UDP toy."** Defense: Vanetza is my own shortlisted C3 path and the
   transport interface is designed so it drops in; but it is C++, needs link-layer plumbing,
   and §6 explicitly defers real PC5/ASN.1. Criterion 1 says prefer the path most likely to
   work end-to-end — a 2-month clock with uncertain hardware makes the stub that path.
   Concession I'd accept: name Vanetza the *committed* M2 transport in the report.
3. **"AGPL fails 'open-source only'... or forecloses the future (C3)."** Defense: AGPL is
   OSI-approved open source — the constraint bans *commercial/paid*, and copyleft (LGPLv3
   Vanetza) is already in everyone's shortlist. The productization concern is real but is a
   user decision, which is exactly why it's flag F4 with a pre-named Apache fallback and a
   contained ~2-day swap cost. I will concede to YOLOX-s if the user signals proprietary
   intent — the requirement KPIs (R9/R10) are detector-agnostic on purpose.
4. **"Python comms won't survive real PC5 / is unserious."** Defense: the M1 latency budget is
   200 ms p95 and Python UDP+JSON measures single-digit ms; C1/C2 favor one language and zero
   integration seams. The durable artifact is the frozen schema (R1), not the transport code;
   M2's Vanetza rewrite lives behind the same 5-function interface.
5. **"KITTI's non-commercial license violates the hard constraint."** Defense: the constraint
   governs *solutions* — libraries/tools that become part of the system. Evaluation data is an
   instrument, not a dependency; nothing KITTI-derived ships. And I've still flagged it (F5)
   with an MIT-licensed CARLA fallback rather than deciding unilaterally.
