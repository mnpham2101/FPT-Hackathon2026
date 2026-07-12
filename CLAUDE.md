# Project Constitution — Cooperative Vehicle Awareness (Milestone 1)

> Requested as `.CLAUDE`; kept as `CLAUDE.md` — the filename Claude Code auto-loads, and `.CLAUDE` would collide with `.claude/` on case-insensitive Windows.

## Mission

Demonstrate cooperative (non-line-of-sight) awareness over V2X on FPT's CarSky cloud platform: vehicle A warns its driver about vehicle C — which A's own sensors can never see, occluded by B — because B's perception of C reaches A over a V2X relay. **Definition of done: R25** of the authoritative report below (one continuous recorded run, zero direct C detections on A, ghost C rendered from `v2x_relayed` only). Timeline: 2 months from 2026-07-09.

## Document authority (read in this order)

1. [requirements/m1-cooperative-awareness.md](requirements/m1-cooperative-awareness.md) — **the authoritative document, in whole**: §1 project description (ultimate authority on goals/scope), §2 enumerated requirements R1–R25 + feasibility, §3 technical solutions, §4 flags awaiting user ratification, §5 debate-record appendix.
2. [tmp/BTC_phan_hoi_V2X_team.pdf](tmp/BTC_phan_hoi_V2X_team.pdf) — the organizers' (BTC) technical advisory of 09/07/2026: **first technical proposal**; authoritative on the CarSky environment (blueprint/node/pin model, provided AAOS IVI node, bench node), strong default on technique — §1 wins conflicts.
3. [.claude/plans/milestone1.md](.claude/plans/milestone1.md) — **planning-shape notes only** (contract-first, mock-then-real, parallel tracks converging, perception store seam, phases with input/output acceptance criteria); its concrete stack/scope claims are superseded by the report.
4. All other markdown under [requirements/](requirements/) predating 2026-07-10 is **stale** — reference-only, never a basis for decisions. Old requirement numbers (pre-round-3 R1–R20) are void per the report's § Numbering.

## Governing principles

1. **Contract-first.** The contracts are the report's R1–R4 (CPM profile, codec seam, TrackedObject schema, ADA→IVI warning message). Freeze before dependent work; never change a frozen contract without re-freezing across every consumer.
2. **Mock-then-real behind seams.** Tracks couple to contracts and seams (R5 adapter, R16 risk abstraction, R19 view interface), never to another track's internals; the bench scenario player is sanctioned test equipment, not a mock to eliminate.
3. **Scope discipline.** §1 of the report + its §4 flags are the hard boundary. BTC's P1 chain is the committed demo; P2/P3 layers are additive and timeboxed, never gating. Flag, don't silently absorb, anything that needs a deferred item or an unratified flag.
4. **Atomic, traceable work.** Every task/subtask maps to a requirement via task ID `X.Y.Z.W`, has a single objective, one atomic commit, and passes build + unit tests before "done". Full rule: [.claude/rules/task-planning-conventions.md](.claude/rules/task-planning-conventions.md).
5. **No hardcoded tunables.** Gate constants, thresholds, cadences, scenario parameters — externalized configuration, never literals (report KPIs enforce this by lint).

## Roles (non-overlapping)

Working order: **project-researcher → project-architecture → project-planner** (researcher may invoke architecture per feature once a 1st-choice solution exists; roles may be revisited when a downstream conflict surfaces). Full specs in [.claude/agents/](.claude/agents/):

| Agent | Owns | Does not do |
|---|---|---|
| [project-researcher](.claude/agents/project-researcher.md) | Requirements, feasibility, tech-stack selection | Architecture, task breakdown, code |
| [project-architecture](.claude/agents/project-architecture.md) | HLD, folder structure, dependency/toolchain config, subagent definitions | Requirements research, task decomposition, implementation |
| [project-planner](.claude/agents/project-planner.md) | Phase/task/subtask plans with `X.Y.Z.W` IDs, subagent spawning, completion tracking | Requirements research, architecture, direct implementation |

## Repository layout

- [requirements/](requirements/) — researcher reports ([format](.claude/rules/research-report-format.md)); `m1-cooperative-awareness.md` is the live one.
- [.claude/rules/](.claude/rules/) — standing process rules (task planning, report format, requirement quality, solution selection, HLD format).
- [.claude/skills/](.claude/skills/) — reusable procedures (requirement analysis, HLD, environment research, markdown style).
- [.claude/agents/](.claude/agents/) — the three agent specs. [.claude/plans/](.claude/plans/) — planning notes. [.claude/prompts/](.claude/prompts/) — saved prompts + debate scratchpads. [.claude/references/](.claude/references/) — cached external evidence.

## Commit & task discipline

One subtask → one atomic commit, tagged `[X.Y.Z.W] <type>: <subject>` (`[X]` for pre-decomposition design commits; no tag for research reports) — full rules in [task-planning-conventions.md](.claude/rules/task-planning-conventions.md). Subtask briefs must be self-contained (paths, contract fields, acceptance criteria).
