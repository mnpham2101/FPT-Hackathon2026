# Project Constitution — Cooperative Vehicle Awareness (Milestone 1)

> Requested as `.CLAUDE`; kept as `CLAUDE.md` — the filename Claude Code auto-loads, and `.CLAUDE` would collide with `.claude/` on case-insensitive Windows.

## Mission

Demonstrate cooperative (non-line-of-sight) awareness over V2X on FPT's CarSky cloud platform: vehicle A warns its driver about vehicle C — which A's own sensors can never see, occluded by B — because B's perception of C reaches A over a V2X relay. **Definition of done: R19** of the authoritative report below (one continuous recorded run, zero direct C detections on A, ghost C rendered from `v2x_relayed` only). Timeline: 1 month from 2026-07-09, hard deadline 2026-08-08.

## Document authority (read in this order)

1. [requirements/m1-cooperative-awareness.md](requirements/m1-cooperative-awareness.md) — **the authoritative document, in whole**: §1 project description (ultimate authority on goals/scope), §2 enumerated requirements R1–R19 (each with definition, acceptance, tech stack), §3 technical solutions, §4 standing user decisions.
2. [presentation/m1-proposal-deck.md](presentation/m1-proposal-deck.md) — **second authority**: an abridged presentation of the 1st authority; on any conflict the report (1) wins.
3. [requirements/development-platform-doc/Car-Sky-Platform.html](requirements/development-platform-doc/Car-Sky-Platform.html) — **reference**: the CarSky development environment and deployment platform (blueprint/node/pin model, node types, deploy flow).
4. [requirements/development-platform-doc/BTC_phan_hoi_V2X_team.pdf](requirements/development-platform-doc/BTC_phan_hoi_V2X_team.pdf) — **reference**: the organizers' (BTC) advisory of 09/07/2026 on the V2X ECU, demo suggestions, and the deployment platform.

All other markdown under [requirements/](requirements/) predating 2026-07-23 is **stale** — reference-only, never a basis for decisions. The report's R1–R19 are the only valid requirement numbers; any requirement number from an earlier document is void.

## Governing principles

1. **Contract-first.** The contracts are the report's R1–R6 (CPM profile, V2X→ADA object message, TrackedObject schema, ADA→IVI warning message, CarSky node deployment, Ethernet-bridge network). Freeze before dependent work; never change a frozen contract without re-freezing across every consumer.
2. **Mock-then-real behind seams.** Tracks couple to contracts and seams (R7 adapter, R14 risk abstraction, R17 view interface), never to another track's internals; the bench scenario player is sanctioned test equipment, not a mock to eliminate.
3. **Scope discipline.** §1 of the report + its §4 decision record are the hard boundary. BTC's P1 chain is the committed demo; P2/P3 layers are additive and timeboxed, never gating. Flag, don't silently absorb, anything that needs a deferred item or an unratified decision.
4. **Atomic, traceable work.** Every task/subtask maps to a requirement via task ID `X.Y.Z.W`, has a single objective, one atomic commit, and passes build + unit tests before "done". Full rule: [.claude/rules/task-planning-conventions.md](.claude/rules/task-planning-conventions.md).
5. **No hardcoded tunables.** Gate constants, thresholds, cadences, scenario parameters — externalized configuration, never literals (report KPIs enforce this by lint).
6. **Read on demand.** A referenced document is read when the task actually needs it, not because a spec names it. Plan a phase with no deployment work and the deploy guides stay unread; write a report and the walkthrough format is irrelevant. Specs list what is *available*; the prompt and the work decide what is *opened*.

## Roles (non-overlapping)

Working order: **project-researcher → project-architecture → project-planner** (researcher may invoke architecture per feature once a 1st-choice solution exists; roles may be revisited when a downstream conflict surfaces). Full specs in [.claude/agents/](.claude/agents/):

| Agent | Owns | Does not do |
|---|---|---|
| [project-researcher](.claude/agents/project-researcher.md) | Requirements, feasibility, tech-stack selection, the `*-walkthrough.md` human procedures, [documents/KnowledgeBase/](documents/KnowledgeBase/) | Architecture, task breakdown, code |
| [project-architecture](.claude/agents/project-architecture.md) | HLD and everything in [documents/Design/](documents/Design/), folder structure, dependency/toolchain config, subagent definitions | Requirements research, task decomposition, implementation |
| [project-planner](.claude/agents/project-planner.md) | Phase/task/subtask plans with `X.Y.Z.W` IDs, subagent spawning, completion tracking | Requirements research, architecture, direct implementation |
| [car-sky](.claude/agents/car-sky.md) | Executing deploys on the platform, Room diagnostics, acceptance evidence | Authoring any document, product code |
| [project-reviewer](.claude/agents/project-reviewer.md) | Reviewing a delivered pull request or branch against the HLD, plan and requirements; the versioned `doc/` review documents | Planning the fix, fixing the code, authoring design or requirements |

[[project-reviewer]] sits outside the working order above — it acts on work already delivered, and only when asked. It judges against the four authorities and stops there: what falls short is the review's to state, how and when it gets fixed is [[project-planner]]'s to decide.

[requirements/car-sky-guide/](requirements/car-sky-guide/) holds two kinds of file, split by artifact rather than by folder. The **reference** files (`node-*.md`, blueprint and REST references — what the platform and each node *are*) are **unowned**: any agent that establishes a platform or node fact records it there, and no edit waits on another agent. Researcher owns `*-walkthrough.md` (the procedure a human *follows*) and is the only agent that writes one.

**Test, verification and deployment run one fixed workflow** — researcher writes the `*-walkthrough.md`, planner decomposes tasks from it, car-sky executes its AI-marked steps: [.claude/rules/walkthrough-driven-delivery.md](.claude/rules/walkthrough-driven-delivery.md). No stage starts from the raw platform, and no stage may be skipped.

## Repository layout

- **Code: one top-level folder per R5 node** — [Scenario_Player/](Scenario_Player/) (bench, R11) · [V2X_ECU/](V2X_ECU/) (R1, R7–R9; R10 deferred to a future milestone) · [ADA_ECU/](ADA_ECU/) (R3, R12–R15) · [IVI_ECU/](IVI_ECU/) (R4, R16–R17). Languages, build artifacts, and the self-contained-folder rules: [.claude/rules/node-code-layout.md](.claude/rules/node-code-layout.md); per-node deploy steps: [requirements/car-sky-guide/](requirements/car-sky-guide/) — within it, [deploy-ivi-hmi-walkthrough.md](requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md) is **authoritative for the IVI APK's build, export, install, launch and verification**; follow it rather than improvising a route.
- **CI: one workflow file per phase** — `.github/workflows/phase<N>-ci.yml`, all carrying the same triggers so every lane runs on every push. A lane lives with the node it exercises, never with the phase that wrote it: [.claude/rules/ci-lane-placement.md](.claude/rules/ci-lane-placement.md) designates the file for each one.
- [requirements/](requirements/) — researcher reports ([format](.claude/rules/research-report-format.md)); `m1-cooperative-awareness.md` is the live one; [future/m1-future-features-register.md](requirements/future/m1-future-features-register.md) mirrors its § Future developments ([Vietnamese translation](requirements/future/m1-future-features-register.vi.md) — non-authoritative; the English register wins on conflict, and changes land there first).
- **[documents/Plan/](documents/Plan/)** — [milestone1_high_level_plan.md](documents/Plan/milestone1_high_level_plan.md) is the **authority for milestone phase planning**: project-planner decomposes all tasks/subtasks from its phases (the `Y` segment of task IDs) and phase acceptance criteria. Its abridged presentation is [m1-proposal-deck.md](presentation/m1-proposal-deck.md); on any conflict the plan wins.
- [plans/](plans/) — the per-phase task breakdowns decomposed from that plan, plus [plans/doc/](plans/doc/) for run records and planning research.
- **Human-facing publications, one top-level folder each, self-contained with its own content, design system and generator** — [presentation/](presentation/) holds the decks, their `template/` and `slide-build-tool/` ([deck-authoring-conventions.md](.claude/rules/deck-authoring-conventions.md)); [website/](website/) holds the static hub site, its `css/`, `js/` and `build-pages.py` ([website/README.md](website/README.md)). Both render [documents/](documents/) and neither owns it. They are not [tools/](tools/), which is test equipment ([node-code-layout.md § tools/](.claude/rules/node-code-layout.md)).
- [.claude/rules/](.claude/rules/) — standing process rules (task planning, report format, requirement quality, solution selection, HLD format, walkthrough-driven delivery, CI lane placement, PR review format, reasoning visibility).
- [.claude/skills/](.claude/skills/) — reusable procedures (requirement analysis, HLD, environment research, walkthrough authoring, markdown style).
- [.claude/agents/](.claude/agents/) — the four agent specs. [.claude/prompts/](.claude/prompts/) — saved prompts + debate scratchpads. `.claude/references/` — cached external evidence, created when the first cache file lands.
- [documents/](documents/) — **every document about a node, filed by what it is rather than by where its code sits**; the three destinations and their owners are fixed by [.claude/rules/node-code-layout.md](.claude/rules/node-code-layout.md#where-a-nodes-documents-live).
  - [documents/Design/`<NODE>`/](documents/Design/) — the node's design authority, owned by [[project-architecture]]: **exactly one HLD per node**, `<node-slug>-hld.md`, plus its decision record, its module and test designs, and the diagram sources they are drawn from. The mandatory section structure and the worked example, [documents/Design/IVI-ECU/ivi-ecu-hld.md](documents/Design/IVI-ECU/ivi-ecu-hld.md), are in [.claude/rules/hld-content-and-commit-format.md](.claude/rules/hld-content-and-commit-format.md).
  - [documents/KnowledgeBase/](documents/KnowledgeBase/) — general knowledge the design draws on, owned by [[project-researcher]]: technique, platform findings, protocol study. A note belongs here when it is about a subject rather than about one of our nodes.
  - [documents/Delivery/](documents/Delivery/) — reports on what was delivered and the evidence behind them.
- **`doc/` inside a work folder** — a node folder's `doc/` holds its **pull-request reviews**, `<node-slug>-pr<N>-review-v<K>.md`, versioned and never overwritten, and its `doc/deprecated/` holds superseded documents and the reviews of work that has since landed ([pull-request-review-format.md](.claude/rules/pull-request-review-format.md)). It holds no design. [plans/doc/](plans/doc/) is the exception that is not a node: it keeps run records and planning research. Agents read a node's design folder as context before working on it.

## Commit & task discipline

One subtask → one atomic commit, tagged `[X.Y.Z.W] <type>: <subject>` (`[X]` for pre-decomposition design commits; no tag for research reports) — full rules in [task-planning-conventions.md](.claude/rules/task-planning-conventions.md). Subtask briefs must be self-contained (paths, contract fields, acceptance criteria).
