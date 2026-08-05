---
name: design-presentation
description: Procedure project-architecture follows to present a phase's design to humans — a markdown + HTML deck under presentation/phase<N>/ explaining the vocabulary, the blueprint and the slice this phase implements, the contracts, the protocol stack and its libraries, the images each node runs with its architecture and call flows, how the phase was tested, and what the next phase inherits, illustrated with draw.io/SVG diagrams derived from the HLDs. project-architecture only.
---

# Design Presentation (project-architecture)

Trigger: [[project-architecture]] is asked to present, explain or document a phase's *design* to humans — "make a design deck for phase N", "explain the phase N architecture", "present the blueprint".

Governed elsewhere, do not restate: section order and required content by [design-presentation-structure.md](../../rules/design-presentation-structure.md), deck mechanics by [deck-authoring-conventions.md](../../rules/deck-authoring-conventions.md), styling by [template.md](../../../presentation/template/template.md), diagram file authoring by [drawio-svg-pairs.md](../task-planning-presentation/references/drawio-svg-pairs.md), prose by [markdown-writing-style](../markdown-writing-style/SKILL.md).

## Ownership

[[project-architecture]] alone. **[[project-planner]] does not run this skill or author these decks**, and this skill is never substituted for [task-planning-presentation](../task-planning-presentation/SKILL.md) — they are two artifacts with two procedures.

The HLDs, the contracts and the requirements report stay the sources of truth. A deck is a reading of them, never a second source, and never where a design decision gets made. A decision discovered to be missing goes back through [high-level-design-procedure](../high-level-design-procedure/SKILL.md) — it does not get invented on a slide.

## Purpose and boundary

The deck answers one question — **what was designed, and why it is shaped that way**. Execution is out of scope: state a planning fact in one line and link the phase's task-execution deck from the cover.

| This deck | The task-execution deck |
|---|---|
| Blueprint and node topology | Phases, task groups, subtasks, IDs |
| Contracts and message formats | Tracks, execution lanes, CI lanes |
| Protocol stack and libraries | Who performed the work |
| Node internals and call flows | Execution order and parallelism |
| Design rationale from the HLD | What each group landed, and where |

Never let two decks own the same diagram.

## Audience

**The reader has not seen the design, and has no glossary.** That is what makes § Terminology the first section rather than an appendix — see [design-presentation-structure.md](../../rules/design-presentation-structure.md).

- **Define before use, every time.** The ordering rule is the whole point of the structure; breaking it is the one failure that cannot be patched by better wording.
- **Real paths keep their real names** — `contracts/golden-vectors/`, `r4-ada-ivi.schema.json`. Renaming them makes the deck point at things that do not exist.
- **Rationale beats inventory.** A slide listing what exists is weak; a slide saying why it is that way and what it rules out is the deck's reason to exist.

Ship test: could someone who has never opened the requirements report say what the system is, what this phase built of it, and what the next phase may now assume?

## Procedure

1. **Scope the phase.** Resolve which node folders the phase touches via [ecu-implementation-scoping](../ecu-implementation-scoping/SKILL.md) if it is not already folder-scoped. Read each folder's `doc/` — the HLDs and design notes are the substance of the deck.
2. **Source, do not recall.** Read the phase plan, the contracts under `contracts/`, and the requirements report sections the phase serves. Every number, port, path and licence on a slide is read at authoring time, not remembered.
3. **Outline against the structure rule.** Fix the sections and their order first, and decide then — not later — which sections warrant a companion deck.
4. **Draft the terminology section before any other slide.** It is the constraint every later slide is written against: a term that turns out to be undefined means the terminology section was wrong, not the slide that used it.
5. **Collect the diagrams before drawing any.** The required set is in [design-presentation-structure.md](../../rules/design-presentation-structure.md) §§ 2, 4 and 5; each node's HLD supplies its component diagram and call flow, so the work is deriving deck copies (that rule's § Diagrams come from the HLD) and authoring only what no HLD holds. File authoring, the render box and the four verification checks are in [drawio-svg-pairs.md](../task-planning-presentation/references/drawio-svg-pairs.md).
6. **Write the remaining slides**, then the handoff last, as the next phase's input list.
7. **Build, verify, ship** — § Before shipping, then § Output.

## Diagrams

Required, not decorative — a protocol stack and a call flow cannot be read from prose.

- **Take the diagram from the HLD wherever the HLD has one.** A node's component diagram and call flow were decided there; a deck that redraws a simplified version has created a second, unauthored design. Derive a trimmed copy instead — the rule is [design-presentation-structure.md § Diagrams come from the HLD](../../rules/design-presentation-structure.md), the mechanics are [drawio-svg-pairs.md § The render box](../task-planning-presentation/references/drawio-svg-pairs.md).
- **Every asset lives in [presentation/assets/](../../../presentation/assets/)** as a `.drawio` + `.svg` pair, prefixed `phase<N>-` when phase-specific. A slide never reaches into a node folder for a picture, and a derived copy names its source in a header comment.
- **One idea per diagram, one diagram per slide.** A single picture of a whole phase is unreadable at projector scale; split it instead of shrinking it.
- **Check the rendered type size, not the file.** `scale = min(1152 / width, 520 / height)`; below ~8 px of rendered type the slide is decoration. Trim, then split, before accepting it.
- **Segmented connectors, never diagonals.** Route runs on separate trunks so no two cross, and verify by rasterising — crossings look deliberate in source and wrong on screen.
- **Draw this phase's fidelity.** Illustrating a phase with a later phase's design misrepresents what shipped; a Phase 1 call flow does not belong in a Phase 0 deck.
- **Generate both halves from one shape list** so they cannot drift.

## Before shipping

Check, do not assume:

- Builder ran clean; expected slide count.
- Zero `base64` in the HTML; every asset path resolves.
- Every diagram passes the four checks in [drawio-svg-pairs.md](../task-planning-presentation/references/drawio-svg-pairs.md).
- **Render the deck itself.** Fixed 1280×720 slides clip silently — content colliding with the footer or pushed off the bottom is invisible in the markdown and obvious in a screenshot.
- **Read the deck in order and stop at the first undefined term.** This is the check the structure exists to pass, and the only one that catches a reordering that looked harmless.
- Every path, port, schema name, licence and link cited exists.

## Honesty

- **A design is not an implementation.** Say which parts of the blueprint the phase actually built, and label a placeholder as one — with what retires it.
- **Distinguish frozen from proposed.** A contract in `contracts/` is frozen; a shape sketched in an HLD is not. Presenting the second as the first is the most damaging error this deck can make.
- **Carry open items**, not design history — an unresolved decision or a deferred seam earns a slide; a superseded draft does not.
- **Flag contradictions, do not resolve them.** Where the HLD and the contract disagree, present the discrepancy; resolving it is a design decision taken with the user.

## Output

A `<deck-slug>-deck.md` plus its generated `.html` in `presentation/phase<N>/`, the diagram pairs in `presentation/assets/`, and one commit carrying sources, HTML and assets together — the source and its export never land in separate commits.

Hand back the slide outline, the assets created, the check results, any companion deck spun out, and every ambiguity flagged rather than silently decided.

When authoring through parallel subagents, each owns a disjoint file set and **does not commit** — architecture makes the single combined commit, since concurrent agents in one working tree cannot commit safely.
