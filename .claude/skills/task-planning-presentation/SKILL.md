---
name: task-planning-presentation
description: Procedure project-planner follows to present a phase's task planning to humans — a markdown + HTML deck under presentation/phase<N>/ showing how work was organized, who or what performed it, in what order, and what came out, illustrated with hand-authored draw.io/SVG diagrams and written for a reader who has not seen the design. project-planner only.
---

# Task-Planning Presentation (project-planner)

Trigger: [[project-planner]] is asked to present, explain or report a phase's task planning — "make a deck for phase N", "explain the phase N tasks".

Governed elsewhere, do not restate: planning itself by [task-planning-conventions.md](../../rules/task-planning-conventions.md), deck mechanics and slide titles by [deck-authoring-conventions.md](../../rules/deck-authoring-conventions.md), styling by [template.md](../../../presentation/template/template.md), diagram authoring by [references/drawio-svg-pairs.md](references/drawio-svg-pairs.md).

## Ownership

[[project-planner]] alone. **[[project-architecture]] does not run this skill or author these decks.**

`plans/phase<N>_tasks.md` stays the source of truth; a deck is a reading of it, never a second source and never where new planning decisions get made.

## Purpose and boundary

The deck answers one question — **how the work was organized, and how it ran in the planned order** — in four parts: what the units are · who or what performed each · in what order and what overlapped · what came out. A slide serving none of the four belongs elsewhere.

Architecture is out of scope. State an architectural fact in one line and link the phase's design deck from the cover; never re-explain it, and never let two decks own the same diagram.

| This deck | The design deck |
|---|---|
| Phases, task groups, subtasks, IDs | Blueprint and node topology |
| Tracks, execution lanes, CI lanes | Node internals, call flows |
| Who performed the work | Message formats and contracts |
| Execution order and parallelism | Protocol stack and libraries |
| What each group landed, and where | Design rationale from the HLD |

## Plain language

**The audience has not read the design.** Every term coined for it is noise — translate or drop it. A deck needing the requirements report open beside it has failed. Worked translations from Phase 0:

| Design vocabulary | The deck says |
|---|---|
| R1 · R2 · R3 · R4 | the V2X message on the air · the V2X-to-ADA message · the tracked-object record · the warning message |
| R1–R6 | the six contracts, covering every message the nodes exchange |
| hop 3 | incoming traffic to the IVI ECU |
| golden vectors, the corpus | the six reference messages |
| octets | bytes |
| a CI run ID | the lane that produced it, linked to the run URL |

- **A requirement number is never a name** — say what the thing is.
- **Define a term before first use, or translate it.** "Contract", "track", "lane" are load-bearing and get defined; design vocabulary gets translated instead.
- **Real paths keep their real names** — `contracts/golden-vectors/`, `R4Message.kt`, task IDs. Renaming them makes the deck point at things that do not exist; explain them in surrounding prose.
- **No opaque identifiers as evidence** — name the thing, put the number in the link.

Ship test: could someone who has never opened the requirements report follow the workflow, see what it cost, and say what it produced?

## Required content

Treat a requester's content suggestion as the outline, not the substance — honour its shape, then say plainly what it omits.

- **Decomposition** — phases → task groups → subtasks, with `X.Y.Z.W` read out on one worked example. Flag that the group number is not the requirement number.
- **Objective per task group**, and which phase acceptance criterion it closed.
- **Track and lane per group**, and what that implies about what could proceed at once.
- **Resources** — every phase plan marks each subtask *agent* (AI subagent), *car-sky* (cloud platform: build, push, deploy, verify) or *USER-MANUAL* (a person: console/UI work, credentials, physical checks). Give the counts, say where human effort was unavoidable and why, and name the cloud platform as the dependency it is.
- **Order and overlap** — the diagrams, plus which relationships are genuinely parallel and which are forced sequential, and by what.
- **What was delivered** — the files that landed, grouped, each group's purpose.
- **Handoff** — what the next phase may assume, from that phase's own input list.
- **A cover link to the design deck.**

## Planning glossary

Project-coined terms that collide with ordinary usage. Define each on its own slide, with a diagram, before use — reading the definition from the source at authoring time, since the plans evolve.

| Term | What it groups | Defined in |
|---|---|---|
| **Track** | a workstream of whole phases, run by different people at once | [milestone1_high_level_plan.md](../../../documents/Plan%20and%20Proposal/milestone1_high_level_plan.md) |
| **Phase** | the `Y` segment — a stage with input and acceptance criteria | [task-planning-conventions.md](../../rules/task-planning-conventions.md) |
| **Task group** | the `Z` segment — subtasks delivering one feature or solution | as above |
| **Subtask** | the `W` segment — one objective, one commit, tests green | as above |
| **Execution lane** | a dependency chain, named after the folder it writes into | each phase plan's *Execution order & parallelism* |
| **CI lane** | one GitHub Actions job — often the literal acceptance ("lane green") | [.github/workflows/](../../../.github/workflows/) |

Four traps to defuse rather than inherit:

- **Track, execution lane and CI lane are three axes**, not three words for one.
- **Lane letters are phase-local and collide** — Phase 0's Lane D is a code folder, Phase 1's is the deploy chain. Name the phase when both are on screen.
- **Lane letters are not an order.** Only a stated dependency sequences one lane against another, and it names an artifact, never a letter.
- **Only a CI lane actually runs.** Tracks and execution lanes are planning structure, not programs.

## Diagrams

Required, not optional — ordering and parallelism cannot be read from prose.

- **Several diagrams, one per slide**: lanes overview and independent start points → per-lane or per-cluster detail → parallel-versus-sequential summary. One dense picture of a whole phase is unreadable at projector scale.
- **Every arrow traces to a real dependency** — a subtask's `Dependencies:` line or the plan's execution-order block, never a plausible-looking ordering.
- **A `.drawio` + `.svg` pair per diagram**, shared in [presentation/assets/](../../../presentation/assets/) prefixed `phase<N>-`. Authoring and verification: [references/drawio-svg-pairs.md](references/drawio-svg-pairs.md).

## Before shipping

Check, do not assume:

- Builder ran clean; expected slide count.
- Zero `base64` in the HTML; every asset path resolves.
- Every diagram passes the four checks in [references/drawio-svg-pairs.md](references/drawio-svg-pairs.md).
- **Render the deck itself** — content colliding with the footer or pushed off the bottom, especially after a table gains a column or a bullet.
- Every subtask ID, arrow, link and path cited exists.
- Grep for surviving requirement numbers and § Plain language vocabulary.

## Honesty

- **Mirror the `Status:` lines.** A subtask closed by CI is not one proven on a deployed node — say which, and show partly-closed acceptance as partly closed.
- **Carry open items, not work history.** An item earns a slide when someone must still know or act on it: an open risk, a deferred decision, evidence weaker than it looks. Settled history — a subtask recorded blocked before completing, a mis-tagged commit, an erratum already routed to its owner — is the plan file's job.
- **Flag contradictions, do not resolve them.** Where sources disagree, present the discrepancy; resolving it is a planning decision taken with the user.
- **Mark reconstruction as reconstruction**, and say from what.

## Exemplar and output

[phase0-task-execution-deck.md](../../../presentation/phase0/phase0-task-execution-deck.md) is the worked example — follow its shape rather than inventing one: work organization and glossary → decomposition → task groups → code delivered → execution order across several diagrams → parallel versus sequential → handoff and deferred work.

Deliver a `<deck-slug>-deck.md` + generated `.html` in `presentation/phase<N>/`, the diagram pairs in `presentation/assets/`, and one commit carrying sources, HTML and assets together. Hand back the slide outline, assets created, check results, and every ambiguity flagged.

When authoring decks through parallel subagents, each owns a disjoint file set and **does not commit** — the planner makes the single combined commit, since concurrent agents in one working tree cannot commit safely.
