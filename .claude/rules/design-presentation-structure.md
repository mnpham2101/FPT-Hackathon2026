# Design Presentation Structure

Governs the deck [[project-architecture]] produces via [design-presentation](../skills/design-presentation/SKILL.md). Deck mechanics — file placement, the asset policy, the build workflow — are in [deck-authoring-conventions.md](deck-authoring-conventions.md); styling is in [template.md](../../presentation/template/template.md). This document fixes only **what a design deck must contain, and in what order**.

Worked example, correct in every respect: [phase0-design-concepts-deck.md](../../presentation/phase0/phase0-design-concepts-deck.md). Follow its shape rather than inventing one.

## Section order is mandatory

Nothing may be used before it is defined. The order below is what enforces that, and a deck that reorders it is wrong even if every slide is individually good.

| # | Section | Required | Must contain |
|---|---|---|---|
| 1 | **Terminology** | always | Every coined or narrowed term the deck goes on to use |
| 2 | **The blueprint** | always | The whole topology, then the part *this phase* implements |
| 3 | **The contracts** | when the phase freezes, changes, or first consumes one | One slide per contract, plus where each lives on disk |
| 4 | **Protocol stack & libraries** | always | The stack diagram, the third-party libraries, the call flow |
| 5 | **The nodes this phase develops** | when the phase builds node code | What each node became this phase, and the test tooling used |
| 6 | **Handoff** | always | What the next phase takes from this one |

A companion deck (§ Companion decks) may be spun out of 3, 4 or 5 — never out of 1, 2 or 6.

## 1 · Terminology — before anything uses it

The first section, always. A reader who stops after it should be able to read every later slide without a glossary.

- **Separate the platform's words from ours.** CarSky's vocabulary (blueprint, node, pin, Room, deployment) and the project's coined words (contract, seam, frozen, ego, bench) are different kinds of term and belong on different slides.
- **A requirement number is not a name.** `R1`–`R4` get a slide naming what each *is* before any diagram labels an arrow with them.
- **Define only what the deck actually uses.** A glossary of unused terms is padding; a term used before its definition is a defect.
- **Coined terms are defined; design vocabulary inherited from elsewhere is translated.** If no source defines a term for what is observed, describe the behaviour instead of coining one.

## 2 · The blueprint — whole, then this phase's slice

- **Show the full topology first** — every node, its type, its address, and the network that joins them — so the phase's work has somewhere to sit.
- **Then state plainly which part this phase implements**, and by implication which parts it does not. This is the slide that stops a reader assuming the whole picture shipped.
- **Diagrams use segmented connectors**, never diagonals across the canvas; route runs on separate trunks so no two cross.

## 3 · The contracts

Required whenever the phase freezes, changes, or first consumes a contract.

- **One slide per contract** — what it defines, its direction and transport, its encoding, its artifacts, and every folder holding a copy.
- **One further slide for where they live** — the authority, the per-node copies, and what keeps them in sync.
- **Do not also walk the contracts in a flow slide.** Pick one owner; a routing overview names ports and direction only.

## 4 · Protocol stack & libraries

Two diagrams are mandatory here, and neither substitutes for the other.

- **A protocol-stack diagram** — the layers from the link upward, and for each message: its encoding, and the library that serves it. Show which layers are shared by every flow and which are not.
- **A call-flow diagram** between the nodes the phase connects, drawn at *this phase's* fidelity. Phase 0 shows the connectivity payloads it actually sent; a later phase shows its real messages. Never illustrate a phase with a later phase's design.
- **A library slide** — every third-party dependency the phase uses, its licence, what it serves, and its one job. Name what is a library and what is a framework; the project prefers libraries it calls over frameworks that call it.

## 5 · The nodes this phase develops

Include when the phase produces node code. Depth follows what the phase actually built — no more.

- **State what each node became this phase**, and say when a node is standing in rather than implemented. A placeholder must be labelled a placeholder, with what retires it.
- **The test tooling is part of the design** when the phase's evidence depends on it. Say what it is, and that it is scaffolding rather than production code.

## 6 · Handoff — mandatory, always last of the content

Every design deck ends with what the next phase takes from this one: the contracts it may now assume frozen, the facts established, the tooling it inherits, and what deliberately did not ship.

- **Write it as the next phase's input list**, not as this phase's summary.
- **Name what does not carry forward**, so nothing is assumed reusable that is not.

## Companion decks

A section that outgrows its slides becomes its own deck rather than bloating this one — the pattern Phase 0 used for its smoke test.

- **The parent keeps one slide**: what the topic settled, and a link. Nothing from the companion is repeated.
- **Companions may only be spun out of sections 3, 4 and 5.** Terminology, the blueprint, and the handoff stay in the parent — they are what makes it readable.
- **Both decks link each other** from the cover.

## How to apply

[[project-architecture]] applies this at the outline step of [design-presentation](../skills/design-presentation/SKILL.md), and again before shipping. A deck missing a mandatory section, or ordering them differently, is not done.
