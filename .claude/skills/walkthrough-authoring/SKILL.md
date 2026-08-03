---
name: walkthrough-authoring
description: Procedure project-researcher follows when asked to produce or revise an operational walkthrough — the authoritative, human-followed build/export/deploy/install/verify guides under requirements/car-sky-guide/ (e.g. deploy-walkthrough-netcheck.md, deploy-ivi-hmi-walkthrough.md). Covers the mandatory sections, the AI/human split, the prohibited content, and the one-way dependency that keeps the documents plannable.
---

# Operational Walkthrough Authoring (project-researcher)

Trigger: [[project-researcher]] is asked to write or revise a document a human follows to get something built, deployed, running, or verified on the platform — "produce a guide on how to…", "add the deploy steps for…", "make the bring-up procedure authoritative".

Distinct from the other researcher modes: this defines **no requirement numbers** ([requirement-analysis-and-solutioning](../requirement-analysis-and-solutioning/SKILL.md)), proposes **no implementation steps for decomposition** ([implementation-step-proposal](../implementation-step-proposal/SKILL.md)), and answers **no open environment question** ([dev-environment-research](../dev-environment-research/SKILL.md)). It writes the procedure a human executes.

## Ownership and placement

[[project-researcher]] owns these files. They live in [requirements/car-sky-guide/](../../../requirements/car-sky-guide/), named `<subject>-walkthrough.md`.

Worked examples, correct in every respect — follow their shape rather than inventing one:

| File | Subject |
|---|---|
| [deploy-walkthrough-netcheck.md](../../../requirements/car-sky-guide/deploy-walkthrough-netcheck.md) | Source file → container image → registry → running Room |
| [deploy-ivi-hmi-walkthrough.md](../../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md) | APK build (local and CI) → deploy → install → launch → verified HMI |

A walkthrough owns the **doing**. The sibling reference files in the same folder — `node-*.md`, the blueprint and REST references — own the **facts** (artifact IDs, config blocks, pin shapes, platform limits) and belong to [[project-architecture]] via [carsky-deployment-guide](../carsky-deployment-guide/SKILL.md). Link them for every fact; never copy one in. A fact that turns out to be missing is reported to that owner, not restated here.

## The six rules

1. **Enumerated, step-by-step.** Numbered sections and numbered steps, one action per step. Every command copy-pasteable in a fenced block, with the expected output stated directly beneath it. A step whose result the reader cannot check is not finished being written.
2. **Simple, unambiguous sentences.** Short declaratives in the imperative. One instruction per sentence. A reader following the document literally must not have to choose between two readings.
3. **No unproven results, no history, no backtracking, no slang.** See § Prohibited content — this is the rule that most often decides whether a document is authoritative or merely descriptive.
4. **A work-division section is mandatory.** Always state which steps an agent performs and which a human must perform. See § Work division.
5. **Other sections follow the request.** Beyond the mandatory set below, structure the document the way the person asking for it asked. Their section list is the outline.
6. **Written to be planned against.** [[project-planner]] reads these documents to devise the tasks and subtasks that perform verification, testing and deployment. See § Authority.

## Mandatory sections

Every walkthrough carries these. Everything else is the requester's call (rule 5).

| Section | Contains |
|---|---|
| **Prerequisites** | The toolchain on the build machine · cloud platform access · the **deliverable prerequisite** — the software that must already exist for the procedure to run at all, stated as required deliverables |
| **The procedure** | The enumerated steps, grouped into numbered sections in execution order |
| **Work division between AI and human** | The three-column table below |
| **Expected outputs and acceptance** | What success looks like, as observable results |
| **Confirm before relying on these** | Only when a step is unproven — the forward-looking list of what to confirm first |

**Order the procedure by what must physically happen first**, even when the requester listed it differently. A deploy precedes an install because the target must exist before anything installs into it. State the ordering rule where the two would otherwise read as a contradiction.

### Work division

```
| Action | AI / Human | Description |
```

- **Action** — an anchor link to the step elsewhere in the document.
- **AI / Human** — one word. Nothing else.
- **Description** — one terse line. The detail lives at the step; do not duplicate it here.

Put any qualification ("an agent can do this once a credential is configured") in a note under the table, so the middle column stays one word.

## Prohibited content

These are what separate an authoritative document from a status report. Each has broken one of these files before.

| Never appears | Why | Write instead |
|---|---|---|
| Task or subtask IDs (`X.Y.Z.W`) | [[project-planner]] reads this document to *produce* those IDs. Citing them is circular, and the document rots the moment a plan is renumbered | Name the deliverable or the step |
| "not built on `main`", "today's APK", PR numbers | Branch status is a snapshot of a moment, not a procedure | State what must exist, in § Prerequisites |
| Commit hashes offered as justification | History | The instruction alone |
| Coined requirement shorthand (`R4`, `R16`, …) | Requirement numbers are assigned after the architecture is designed; a human at bring-up time has no map for them | Plain description — "the message from the ADA ECU" |
| Why a past decision was made; what was retracted, superseded or tried | History | The current instruction |
| An unverified thing asserted as fact | Misleads a reader who follows it literally | Mark it unverified, or move it to § Confirm before relying on these |
| Slang, hedging, filler | Ambiguity | Plain imperative |

**Unproven is not the same as unmentionable.** A route nobody has yet exercised still belongs in the document — as an instruction plus an explicit note that it is unconfirmed. What is forbidden is presenting it as established. Strip the history ("never proven on this deployment, see the Phase 0 finding") and keep the forward-looking warning ("confirm this before relying on it").

## Authority

These documents are authoritative for their subject, and named as such in [CLAUDE.md](../../../CLAUDE.md) § Repository layout. That has two consequences.

- **The dependency runs one way.** Plans and task documents cite the walkthrough; the walkthrough cites nothing under [plans/](../../../plans/). A walkthrough that references a task has inverted the relationship it exists to support.
- **Reference, never restate.** When a plan, node guide or HLD needs a procedure this document owns, it links the section. A second copy is drift waiting to happen — every duplicated procedure in this repo has diverged from its original.

Apply the same rule inside the document: state a fact once, in the section that owns it, and link it from everywhere else.

## Before shipping

1. **Every anchor resolves.** GitHub slugs `### 4.5 Connect and check the guest` as `#45-connect-and-check-the-guest`. Renumbering a section breaks every inbound link — walk them all after any structural change.
2. **Inbound references still resolve** — the node guide, both plan files, `CLAUDE.md`, the node `README.md`, the HLD, and any workflow file naming the document or an artifact it names.
3. **No prohibited content survives.** Grep the file for task-ID patterns and requirement shorthand before committing.
4. **Followable start to finish** by a reader who reconstructs nothing.
5. **Nothing the document deletes is left dangling** — a removed section that another file pointed at, or a named artifact that no longer exists.

## How to apply

[[project-researcher]] applies this to every walkthrough it writes or revises, alongside [markdown-writing-style](../markdown-writing-style/SKILL.md) at its **human how-to guide** audience — rules 1–6 of that document hold here too, and rule 6 (conclusions only, no history) is the one this skill sharpens most.

Commit with the requirement-only `[X] docs:` form when the document serves an enumerated requirement, or the untagged `docs:` form when it does not ([task-planning-conventions.md](../../rules/task-planning-conventions.md#commit-message-format)).
