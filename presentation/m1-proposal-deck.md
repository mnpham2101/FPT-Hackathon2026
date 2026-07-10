---
marp: true
theme: default
paginate: true
title: Cooperative Vehicle Awareness — Milestone 1 Proposal
description: Proposal deck — cooperative NLOS hazard awareness over V2X, virtual-ECU system design, AI-driven ASPICE-style development process
---

<!-- _class: lead -->
<!-- _paginate: false -->

# Cooperative Vehicle Awareness

## See the hazard before it can be seen

**Milestone 1 proposal — FPT Hackathon 2026**

Source: [m1-cooperative-awareness.md §1](../requirements/m1-cooperative-awareness.md) · [milestone1.md](../.claude/plans/milestone1.md)

---

<!-- Slide 1 — Project goals -->

# 1 · Project goals — safer travel, beyond line of sight

**The accident nobody sees coming:** a vehicle braking two cars ahead is invisible to the following driver *and* to every onboard sensor — until it is too late.

- **Prevent accidents.** Vehicles share what they perceive over V2X: a hazard is detected, risk-assessed, and warned **seconds before it becomes visible** — time to slow down instead of collide.
- **Safer travel.** Awareness is no longer limited by one vehicle's line of sight — every connected vehicle extends every other's perception.
- **Effortless user experience.** The hazard appears on the in-vehicle display as an intuitive bird's-eye scene of the road ahead — glance, understand, react. No cryptic alarms, no interpretation.

![h:280 Convoy geometry — A cannot see C; B sees C and relays it over V2X](../requirements/m1_convoy_nlos_relay_geometry.png)

**Milestone 1 proves it end-to-end:** convoy A → B → C. Vehicle A can never see C (blocked by B). B detects C and relays its perception over V2X — **C appears on A's display anyway.**

---

<!-- Slide 2 — Technical approach: system design on virtual ECUs -->

# 2 · System design — a full vehicle, fully virtual

The complete E/E architecture runs as **virtual ECUs on a cloud platform** — no hardware, yet a full-system demonstration.

![h:330 4-ECU system design](../requirements/coorperative-awareness.png)

- **Blueprint/node model:** 1 blueprint = 1 car; 1 node = 1 ECU, packaged as a container — deploying the blueprint brings up the whole virtual vehicle.
- **Four cooperating nodes:** V2X ECU (communication), ADA ECU (perception + risk), IVI ECU (display), plus a team-built **Scenario Player** bench node emulating the modem and the world.
- Virtualization fidelity is chosen per node — full vECU for the IVI, app-level Linux containers for V2X/ADA — matching effort to what each ECU must prove.

---

<!-- Slide 3 — Technical approach: ECU responsibilities -->

# 3 · ECU responsibilities — one clear job each

| Node | Milestone-1 responsibility |
|---|---|
| **V2X ECU** | Modem interfacing (simulated); receives V2X payloads, applies business logic, forwards to ADA; builds and broadcasts payloads from ADA data |
| **ADA ECU** | Detects objects from video, estimates distance, admits tracks; fuses V2X + own perception in a **Collision Risk Assessment** abstraction; emits warnings to IVI |
| **IVI ECU** | GUI shell with buttons + central view; renders the 3-vehicle god-view scene, including the occluded "ghost" vehicle, on warning |
| **Scenario Player** | Bench node: emulates the modem connection point, plays back V2X messages at configurable rates, drives every test scenario |

Track admission is an explicit, testable state machine — no hidden heuristics:

![h:230 Vehicle C track admission state machine](../requirements/vehicleC_track_admission_state_machine.png)

---

<!-- Slide 4 — Technical approach: extensibility by design -->

# 4 · Designed for what comes next

Every ECU boundary is a **deliberate extension seam** — Milestone 1 ships the scenario, the architecture ships the roadmap:

- **V2X ECU — hardware portability.** Payload logic lives at the application layer, independent of modem and interface setup: a real C-V2X modem later swaps in **without touching the business logic**.
- **ADA ECU — pluggable warning scenarios.** M1's "occluded vehicle" logic is one realization of the shared Collision Risk Assessment abstraction; intersection hazards, curve blind spots, speed-scaled risk plug in as new modules. The warning message is versioned and typed so **new hazard types and criticality filtering extend it, not rework it**.
- **IVI ECU — swappable views.** The central view sits behind a binding view-interface seam: 2D today, 3D / live camera feed / multi-app wake-on-warning tomorrow — shell untouched.

Future features are not wishful thinking — they are **registered, analysed, and traced** to the seams above ([§ Future developments](../requirements/m1-cooperative-awareness.md)): live detection at 120 km/h, multiple hidden obstructions, single-message aggregation, user-opt filtering, themes.

---

<!-- Slide 5 — Development process: ASPICE-inspired discipline -->

# 5 · Development process — ASPICE-style discipline, hackathon speed

We simulate the ASPICE traceability chain end-to-end: **requirement ↔ architecture ↔ task ↔ commit ↔ test** — every artifact answers "why does this exist?"

- **Requirements are engineered, not collected:** numbered and frozen project-globally (R1–R20), each with a feasibility verdict and a **measurable KPI**; every vague wording is translated `original → precise` and recorded for veto.
- **Phases carry contracts:** each phase declares its input, its output, and binary acceptance criteria — a phase is done when the check passes, not when it feels done.
- **Total traceability by construction:** every task ID `X.Y.Z.W` encodes requirement · phase · task · subtask; every commit carries its ID — from any line of code back to the requirement it serves in one hop.
- **Atomic units of work:** one subtask = one single objective = **one atomic commit**, gated by build + unit tests before it may be called done.

---

<!-- Slide 6 — Development process: agentic AI, human in the loop -->

# 6 · Agentic AI with a human in the loop

AI does the volume; the process keeps it **manageable, auditable, and steerable**.

- **Three specialized agents, non-overlapping mandates:** *researcher* (requirements, feasibility, tech selection) → *architect* (high-level design, module boundaries) → *planner* (task decomposition, spawning implementation subagents). Each owns its stage — and explicitly may not do the others'.
- **Procedure as code:** every working procedure — analysis, design, planning, writing style, commit format — is a versioned rule or skill in the repository, reviewed and evolved like source code.
- **Human decision points are built in:** every proposed number is marked *(A) assumption to confirm*; user decisions are logged (D1–D7); contracts are ratified by a human at freeze; scope changes are **flagged, never silently absorbed**.
- **Result:** AI velocity with audit-grade accountability — every decision has an author, a reason, and a record.

---

<!-- Slide 7 — Development process: parallel by construction -->

# 7 · Parallel by construction — contracts first, convergence by design

Two contracts are frozen before any implementation: the **V2X message schema** and the **TrackedObject struct**. Everything downstream builds against them — with mocks — so three tracks run **in parallel**:

![h:300 Three parallel tracks converge at Phase 5 over frozen contracts](../.claude/plans/dev_phases_parallelism.png)

- **Comms track** broadcasts the full schema with mock payloads · **perception track** produces real detections · **display track** renders against a mock message.
- **Phase 5 converges them:** swap mock contents for real perceived data — integration is a **data swap inside a shape that already works**, not a rewrite.
- **Payoff:** the 2-month timeline holds with slack; teams (and AI subagents) never block on each other's internals — only on frozen, versioned contracts.

---

<!-- _class: lead -->
<!-- _paginate: false -->

# Why this proposal

**A safety feature you can watch work** — the invisible vehicle appears on the display.
**An architecture that is already the roadmap** — every future feature has its seam.
**A development process you can audit** — every commit traces to a requirement, every decision to a human.
