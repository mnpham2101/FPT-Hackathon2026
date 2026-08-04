---
name: hld-content-and-commit-format
description: The mandatory section structure, content and diagram rules for every node HLD project-architecture writes — one HLD per R5 node, modelled on IVI_ECU/doc/ivi-ecu-hld.md — and the commit format for design commits.
---

# HLD Structure, Content & Commit Format

Governs every high-level design [[project-architecture]] produces — the artifact of the "make the HLD" and "propose HLD, commit design" steps of [high-level-design-procedure](../skills/high-level-design-procedure/SKILL.md).

**Worked example, correct in every respect: [ivi-ecu-hld.md](../../IVI_ECU/doc/ivi-ecu-hld.md).** Follow its shape rather than inventing one. Where that file and this document disagree, the file is what an author copies and this document is what gets corrected.

## One HLD per node, not per phase

Every node of the R5 blueprint carries exactly one HLD, in its own folder's `doc/`, named for the node rather than for the phase that produced it:

| Node folder | HLD |
|---|---|
| [Scenario_Player/](../../Scenario_Player/) — the bench | `doc/scenario-player-hld.md` |
| [V2X_ECU/](../../V2X_ECU/) | `doc/v2x-ecu-hld.md` |
| [ADA_ECU/](../../ADA_ECU/) | `doc/ada-ecu-hld.md` |
| [IVI_ECU/](../../IVI_ECU/) | `doc/ivi-ecu-hld.md` |

- **The bench is a node and gets the full document.** Being test equipment changes what its sections say, not whether it has them.
- **A later phase extends the node's HLD; it does not add a second one.** Two design documents for one node means two answers to "where does this file go", and the implementer picks the wrong one.
- **The HLD is that node's sole design authority.** Where it exists, no other document defines the node's components, paths, seams, configuration keys or evidence lines.

## Section order is mandatory

Nothing may be used before it is defined, and a planner must be able to reach any fact by section number. A document that reorders or omits a required section is not done.

| # | Section | Required | Must contain |
|---|---|---|---|
| 1 | **Scope and authority** | always | What the node covers and does not; that the HLD is binding for work in the folder; that task planning decomposes from it plus the requirements report; what overrides it |
| 2 | **Required reading and sourced notes** | always | The requirement documents that had to be read in full to write the design, each with what it fixes for this node; then one row per research note with what the design adopts from it |
| 3 | **The component architecture** | always | The component diagram, its legend, and an **MVC separation** subsection placing every component in exactly one layer |
| 4 | **Folder structure** | always | The tree designating the target path of every component the document names |
| 5 | **Platform and boundary** | always | The runtime platform, the interfaces at the node's edge, and the observation surfaces the evidence comes from |
| 6 | **Internal components** | always | One row per component — role, input, output — grouped by MVC layer, plus the configuration and descriptor files |
| 7 | **External related components** | when anything outside the node is named | What sits outside the boundary, and a **test equipment** subsection for the mocks and simulators that exercise the node alone |
| 8 | **Interfaces, ports and the layer rule** | always | Each seam, which component requires it and which provides it, the node's network endpoints, and the rule that no layer is collapsed |
| 9 | **Call flow** | always | A link to the `.puml` sequence source and one sentence naming the path and its branches |
| 10 | **The contract** | always | The node's message schema — direction, transport, encoding, normative schema file, node copy, freeze status, and a field table per message kind |
| 11 | **Tech stack, build and CI** | always | Languages, libraries and versions traced to the report; the build commands; the CI lanes |
| 12 | **Test strategy** | always | The configurations that exercise the node, and the expected observables with the component that produces each |
| 13 | **Design decisions** | always | A link to the node's decision record, `doc/<node-slug>-design-decisions.md`, and one line naming what D1…Dn cover |

A header blockquote precedes §1: what the document is, the frozen contract, the procedure documents beside it, and the diagram sources.

## What each section must get right

- **§2 is the reading list, and it is not optional reading.** Before writing or changing an HLD, [[project-architecture]] reads the requirement documents that bear on the node **in full** — the whole requirement entries with their definition, dependency, acceptance and tech stack, the figures they embed, the frozen contract files, the §4 decision record, and any later report that adds requirement numbers or defers work touching the node. Skimming for requirement numbers produces a design that satisfies the headline and contradicts an acceptance clause. §2 then lists exactly what was read and what each document fixed, so a reviewer can tell a sourced design from a plausible one, and so a requirement that lands later has a visible place to be checked against.
- **§1 is what makes the document usable by a planner.** State plainly that task planning decomposes from this HLD plus the requirements report, that briefs cite sections rather than restating them, and that a component, path or configuration key not designated here is not created ad hoc.
- **§4 designates paths; it does not sketch them.** Every component named anywhere in the document has exactly one path in the tree. An implementer who has to choose a location is reading a defective HLD.
- **§6 gives each component one responsibility.** Role, input, output, in a table. Work that fits no row belongs to a component the design has not defined yet — a design change, not an implementer's judgement call.
- **§10 names which message set the contract is, and its direction.** Say it explicitly — "the message set from ADA-ECU" — and point at the normative schema file plus the node's byte-synced copy. A contract section that describes a format without naming producer and consumer leaves the reader guessing who sends it. A node that both consumes and produces gets one subsection per direction.
- **§12's observables are the acceptance evidence.** Log lines and rendered output, each traced to the component that produces it. Where more than one configuration exists (mock producer versus real), state that the expected output is identical in both, so a difference is a finding about the other node.
- **§13 is a pointer; the decisions live in a companion file.** `<Node_Folder>/doc/<node-slug>-design-decisions.md` holds `D1…Dn`, each a titled decision with its rationale and its rejected alternative — worked example: [ivi-ecu-design-decisions.md](../../IVI_ECU/doc/ivi-ecu-design-decisions.md). The HLD cites decisions by number wherever they bind a component, and keeps §13 to the link plus one line of coverage. Decisions are binding: one is revisited by changing its entry, never by an implementation that departs from it.

## Diagrams

- **Written as source, not as pre-rendered images** — `.puml` for sequence, component and activity diagrams, so they stay diffable and reviewable in the same commit as the design text. A hand-authored component map may be `.drawio` with an exported `.svg` beside it, which is what the deck and the HLD both embed.
- **Mandatory:** a call-flow sequence diagram (§9) and a component diagram (§3). Any further component or activity diagram is included only when it clarifies something the tables cannot.
- Diagram sources live in the node's `doc/`, or its `doc/research_notes/` for drawn assets, and are listed in the header blockquote.

## Writing rules

An HLD is read under context pressure, by a planner writing briefs and by an implementer about to write code. Every extra clause costs both. [markdown-writing-style](../skills/markdown-writing-style/SKILL.md) applies in full; these are what it means here, and [ivi-ecu-hld.md](../../IVI_ECU/doc/ivi-ecu-hld.md) is the style reference as well as the structural one.

- **One claim per sentence.** Split a sentence carrying two facts joined by "and", "which" or an em-dash aside. Long compound sentences are the main defect this rule exists to catch.
- **Tables carry the facts; prose carries only what a table cannot.** A component's role, input and output belong in its row. A lead-in of one or two sentences per section is the budget.
- **Say a fact once, in the section that owns it, and cite `§n` elsewhere.** Two statements of one fact become two answers the moment either is edited. A mapping table restating what a tree and a component table already show is deleted, not maintained.
- **No filler openers.** Cut "It is worth noting", "Note that", "In other words", and any sentence that restates the one before it.
- **Current state only — no history.** No committed-versus-missing tables, no decision dates, no "now", "already", "until X exists", "must change". Describe the design, not its progress. Design rationale is not history: "rejected alternative X because Y" belongs in §13; "we used to do X" belongs nowhere.
- **Reference, never repeat.** Requirement definitions stay in the report, procedures stay in the walkthrough, platform facts stay in the node reference — cited by section.
- **No slang, no hedging.** "an interface rather than a concrete producer", not "an interface, not a box".
- Everything else follows [markdown-writing-style](../skills/markdown-writing-style/SKILL.md).

## Commit format

HLD commits follow the project-wide format defined in [task-planning-conventions.md](task-planning-conventions.md#commit-message-format):

```
[<taskID>] <type>: <subject>
```

using `type = design` and the **requirement-only `X`** form of the taskID — HLD happens before phase/task/subtask decomposition exists for that requirement, so `Y.Z.W` aren't assignable yet.

## How to apply

- [[project-architecture]] applies the section list at the outline step of [high-level-design-procedure](../skills/high-level-design-procedure/SKILL.md), and again before committing. Do not commit an HLD without its `[X] design: ...` tagged message. A deliverable placed outside the sanctioned work-folder locations of [node-code-layout.md §Per-folder doc/](node-code-layout.md#per-folder-doc) is called out with its rationale and goes through that procedure's approval pause.
- [[project-planner]] reads the node's HLD as the structural input to task planning and cites its sections in every brief; a missing section is flagged back to architecture rather than filled in by the plan.
