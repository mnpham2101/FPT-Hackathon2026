# Project Constitution — Cooperative Vehicle Awareness (Milestone 1)

> Note: requested as `.CLAUDE`; kept as `CLAUDE.md` since Windows is case-insensitive (`.CLAUDE`
> and `.claude/` would collide) and `CLAUDE.md` is the filename Claude Code auto-loads anyway.

## Mission

Demonstrate cooperative (non-line-of-sight) awareness over V2X: vehicle A becomes aware of vehicle
C — which A's own camera can never see, blocked by B — because B perceives C and relays that
perception to A. Full goals, scope, and phase plan: [.claude/plans/milestone1.md](.claude/plans/milestone1.md).

**Definition of done:** all six phases pass their acceptance criteria and the end-to-end demo shows
C appearing on A's display purely via B's V2X relay, while A's own perception cannot see C.
Timeline: 2 months.

## Governing principles

1. **Contract-first.** The V2X message schema and the TrackedObject struct (defined in
   [.claude/plans/milestone1.md](.claude/plans/milestone1.md) section 4) are frozen before any
   other work starts. Every phase after that is "swap mock data for real data" inside a shape that
   already works. Never change a frozen contract without re-freezing it across every downstream
   track.
2. **Mock-then-real.** Comms, perception, and display tracks build against mocks and converge on
   real data only at Phase 5. Don't couple a track's early work to another track's unfinished
   internals — couple to the contract instead.
3. **Scope discipline.** Honor the scope, assumptions, and deferred-scope sections of
   [.claude/plans/milestone1.md](.claude/plans/milestone1.md) (sections 2 and 6) as hard
   boundaries. Notably in M1: no live camera bring-up, no model training, no message signing/PKI,
   no Cortex-M GPS path, no TTC/trajectory risk analysis, single-object only. Flag, don't silently
   absorb, any requirement that needs one of these.
4. **Atomic, traceable work.** Every task/subtask maps to a requirement and phase via task ID
   `X.Y.Z.W` (requirement X, phase Y, task Z, subtask W), has a single objective, produces one
   atomic commit with no out-of-scope code, and must pass build + unit tests before being marked
   done. Full rule:
   [.claude/rules/task-planning-conventions.md](.claude/rules/task-planning-conventions.md).
5. **No hardcoded tunables.** Proximity-gate constants and similar tuned values are externalized
   configuration, never literals in code.

## Roles (non-overlapping)

Three specialized agents own distinct stages of the work. Each has a full spec under
[.claude/agents/](.claude/agents/); summary:

| Agent | Owns | Does not do |
|---|---|---|
| [project-researcher](.claude/agents/project-researcher.md) | Requirements enumeration, feasibility, tech-stack selection, trade-off & extensibility analysis | Architecture design, task breakdown, code |
| [project-planner](.claude/agents/project-planner.md) | Phase/task/subtask breakdown with `X.Y.Z.W` IDs, acceptance criteria, subagent spawning & completion tracking | Requirements research, architecture design, direct implementation |
| [project-architecture](.claude/agents/project-architecture.md) | High-level/module design, folder structure, dependency & toolchain config, subagent definitions | Requirements research, task/subtask decomposition |

Working order: **project-researcher → project-architecture → project-planner**. In practice
researcher invokes architecture directly, as a subagent, per feature once it has picked a
1st-choice solution (see
[high-level-design-procedure](.claude/skills/high-level-design-procedure/SKILL.md)); planner still
spawns implementation subagents only once architecture has finalized subagent definitions.
Researcher and architecture may be revisited if the planner or an implementing subagent surfaces a
conflict.

## Repository layout

- [CLAUDE.md](CLAUDE.md) — this file. Project constitution; read first.
- [.claude/plans/](.claude/plans/) — source planning documents (currently `milestone1.md` + its
  diagrams). Treat as the origin of truth for scope, contracts, and phase acceptance criteria.
- [.claude/rules/](.claude/rules/) — standing process rules that apply regardless of which plan is
  active (e.g. [task-planning-conventions.md](.claude/rules/task-planning-conventions.md)). Only
  add a rule file here for content that isn't just a restatement of the plan doc.
- [.claude/agents/](.claude/agents/) — subagent specs (Claude Code convention: one `.md` file per
  agent) for project-researcher, project-planner, project-architecture.
- [.claude/skills/](.claude/skills/) — reusable project-specific procedures (Claude Code
  convention: one `<skill-name>/SKILL.md` per skill) — currently `high-level-design-procedure` and
  `requirement-analysis-and-solutioning`.
- [.claude/prompt/](.claude/prompt/) — important prompts saved verbatim as markdown, on request.

## Commit & task discipline

- One subtask → one atomic commit. No unrelated changes riding along.
- A subtask is only "done" when: single objective met, build passes, unit tests pass, commit made.
- Subtask briefs must carry enough context (file paths, contract fields, acceptance criteria) that
  an implementing subagent should not need to re-read the wider codebase — only re-read on hitting
  an actual implementation issue.
