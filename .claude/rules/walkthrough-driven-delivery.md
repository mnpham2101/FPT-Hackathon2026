# Walkthrough-Driven Delivery

The standing workflow for every objective whose goal is to **test, verify, or deploy** — on the CarSky platform or on any target a human must drive. Three agents act in a fixed order on one artifact: the `*-walkthrough.md` under [requirements/car-sky-guide/](../../requirements/car-sky-guide/).

Authoring mechanics live in [walkthrough-authoring](../skills/walkthrough-authoring/SKILL.md), planning mechanics in [task-planning-conventions.md](task-planning-conventions.md), platform mechanics in the `carsky-*` skills. This document fixes only **who acts, in what order, and what each hands the next**.

## The three stages

| # | Stage | Agent | Follows | Hands on |
|---|---|---|---|---|
| 1 | **Investigate & author** | [[project-researcher]] | [walkthrough-authoring](../skills/walkthrough-authoring/SKILL.md) | The walkthrough — enumerated procedure, AI/Human work division, acceptance |
| 2 | **Plan** | [[project-planner]] | [task-planning-conventions.md](task-planning-conventions.md) | Tasks/subtasks `X.Y.Z.W`, each citing the walkthrough section that governs it |
| 3 | **Execute** | [[car-sky]] | the `carsky-*` skills | The AI-marked steps performed, plus the evidence acceptance demands |

The order is not advisory. **No stage starts from the raw platform**: stage 2 plans from the document rather than from the REST API, and stage 3 executes the document rather than its own reconstruction of it. Work that skips stage 1 produces a plan nobody can re-run and evidence nobody can reproduce.

Existing walkthroughs, and the subjects they already cover:

| Document | Subject |
|---|---|
| [deploy-walkthrough-netcheck.md](../../requirements/car-sky-guide/deploy-walkthrough-netcheck.md) | Source file → container image → registry → running Room |
| [deploy-ivi-hmi-walkthrough.md](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md) | APK build (local and CI) → deploy → install → launch → verified HMI |

## Stage 1 — the procedure exists before anything is planned against it

[[project-researcher]] investigates the route and writes it down. The output is a document a human can follow start to finish, reconstructing nothing.

- **This is the only stage that writes the procedure.** No other agent authors or edits a `*-walkthrough.md`.
- The **work-division table is what makes the document plannable** — it is what stages 2 and 3 read to decide who performs each step. A walkthrough without it forces the later stages to guess.
- Steps nobody has yet exercised belong in **§ Confirm before relying on these**, marked unproven rather than asserted.

## Stage 2 — the planner decomposes the walkthrough, it does not re-derive it

When a phase or request has test, verification or deployment as its objective, [[project-planner]] reads the subject's walkthrough — plus the node reference beside it — and builds the task tree from it.

- **Cite, never restate.** Each subtask links the section (by anchor) that governs its step; the commands stay in the walkthrough. A subtask carrying its own copy of the procedure is drift waiting to happen.
- **Acceptance comes from § Expected outputs and acceptance** — subtask criteria are that section's observable results, not new ones the planner invents.
- **§ Confirm before relying on these becomes the phase's earliest subtasks.** An unproven route is proved before the work depending on it is scheduled, so a dead route costs hours instead of days.
- **The work-division table assigns the executor.** AI rows are briefed to [[car-sky]]; Human rows are tracked by the plan as steps no agent performs, labelled as such in the brief.
- **The walkthrough's ordering is binding** where it states one — a deploy precedes an install because the target must exist first.
- **No walkthrough, no plan.** If the subject has none, flag it back and get stage 1 run; do not guess the steps.

## Stage 3 — car-sky performs the AI rows, and stops at the rest

[[car-sky]] is the agent side of the document. It is spawned for anything touching the live platform: deploying, checking deployment status, building or testing there, gathering acceptance evidence.

- **Read the walkthrough first.** The brief should name it; when it does not, find the subject's `*-walkthrough.md` before improvising a route.
- **Perform only the rows marked AI.** At a Human row, stop, report exactly what the human must do, and wait. Do not improvise around a browser-only, visual-judgement, or download step.
- **Produce the evidence § Expected outputs and acceptance names** — that section, not the agent's own judgement, is what closes the subtask.
- **Never edit the walkthrough.** Findings go back in the report: a failed command, a route that proved unreachable, an unproven step now proven. [[project-researcher]] folds procedure changes in; a platform or node *fact* goes in the `node-*.md` reference, which is unowned and which any agent may correct.
- **A walkthrough with no work-division table** predates the rule that mandates it. Treat browser-only, visual-judgement and download steps as Human, and report the missing table as a finding.

## The dependency runs one way

Walkthrough ← plan ← execution. Each stage cites the one before it and never the reverse.

- A walkthrough **never cites a task ID, a plan file, a requirement number, or a commit** — [[project-planner]] reads it to *produce* those IDs, so citing them is circular. The full prohibition is [walkthrough-authoring § Prohibited content](../skills/walkthrough-authoring/SKILL.md).
- A plan **never becomes the procedure's home**. When a step changes, the walkthrough changes and every plan citing it follows; a plan that has grown its own copy of the steps has broken the workflow.
- Evidence **never becomes authority**. A run's outcome is recorded in the plan's run doc; only the procedure that produced it is folded back into the walkthrough.

## When an input is missing

| Situation | Action |
|---|---|
| Objective is test/verify/deploy, no walkthrough exists | [[project-planner]] flags back; stage 1 runs first |
| Walkthrough exists but a needed step is absent | Report to [[project-researcher]] to add it — do not fill the gap in a subtask brief |
| Walkthrough states a fact that turns out wrong | A node/platform fact is corrected in the `node-*.md` reference, by whichever agent established it; a step goes to [[project-researcher]] |
| A Human row blocks an agent mid-run | [[car-sky]] halts and reports; the plan tracks the step as human work |

## How to apply

- [[project-researcher]] runs stage 1 on every request for a build/deploy/verify procedure, per [walkthrough-authoring](../skills/walkthrough-authoring/SKILL.md).
- [[project-planner]] runs stage 2 whenever a phase or request carries test, verification or deployment work, alongside [task-planning-conventions.md](task-planning-conventions.md).
- [[car-sky]] runs stage 3 on every spawn, alongside its preflight and login skills.
- Any agent finding itself about to perform a stage it does not own hands off instead. The roles table in [CLAUDE.md](../../CLAUDE.md) is the boundary.
