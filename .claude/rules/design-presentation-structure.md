# Design Presentation Structure

Governs the deck [[project-architecture]] produces via [design-presentation](../skills/design-presentation/SKILL.md). Deck mechanics — file placement, the asset policy, the build workflow — are in [deck-authoring-conventions.md](deck-authoring-conventions.md); styling is in [template.md](../../presentation/template/template.md). This document fixes only **what a design deck must contain, and in what order**.

Worked examples, correct in every respect and consistent with each other: [phase0-design-concepts-deck.md](../../presentation/phase0/phase0-design-concepts-deck.md) and [phase1-design-deck.md](../../presentation/phase1/phase1-design-deck.md). Follow their shape rather than inventing one.

## Section order is mandatory

Nothing may be used before it is defined. The order below is what enforces that, and a deck that reorders it is wrong even if every slide is individually good.

| # | Section | Required | Must contain |
|---|---|---|---|
| 1 | **Terminology** | always | Every coined or narrowed term the deck goes on to use |
| 2 | **The blueprint** | always | The whole topology, then the part *this phase* implements |
| 3 | **The contracts** | when the phase freezes, changes, or first consumes one | One slide per contract, plus where each lives on disk |
| 4 | **Protocol stack & libraries** | always | The stack diagram and the third-party libraries |
| 5 | **The blueprint nodes** | always | The images delivered, then each node's architecture and call flow |
| 6 | **Testing** | always | What was verified, and the equipment that verified it |
| 7 | **Handoff** | always | What the next phase takes from this one |

A companion deck (§ Companion decks) may be spun out of 3, 4, 5 or 6 — never out of 1, 2 or 7.

**Every phase deck carries all seven, under these names.** A phase that built no node code still has section 5 — it says so, and names the phase where each node's architecture arrives. Consecutive phases read as one document only if their section numbers mean the same thing, so a deck does not rename a section to suit its content.

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

- **A protocol-stack diagram** — the layers from the link upward, and for each message: its encoding, and the library that serves it. Show which layers are shared by every flow and which are not.
- **A library slide** — every third-party dependency the phase uses, its licence, what it serves, and its one job. Name what is a library and what is a framework; the project prefers libraries it calls over frameworks that call it.

Call flows belong to section 5, beside the nodes they run between.

## 5 · The blueprint nodes

The section carrying what the phase built, in a fixed slide order.

1. **The images delivered** — one row per node: its image tag, its role, the contracts or messages it carries, and what it became this phase. A node running another phase's image or a stand-in says so in its own row, and a node with no image says that instead of being omitted.
2. **One architecture slide per node the phase developed** — the node's component diagram, sourced from its HLD (§ Diagrams come from the HLD).
3. **One internal call-flow slide per node** — what happens inside it, drawn at this phase's fidelity.
4. **The call flow between the nodes** — the one diagram showing the whole exchange, drawn at *this phase's* fidelity. Phase 0 shows the connectivity payloads it actually sent; a later phase shows its real messages. Never illustrate a phase with a later phase's design.

- **A placeholder is labelled a placeholder**, with what retires it — a node standing in is not a node built.
- **A phase that built no node code keeps the section** and says where each node's architecture arrives instead.

## 6 · Testing

What was verified, at design fidelity — not the execution record, which belongs to the task-execution deck.

- **The configurations that exercise each node**, and what each proves that the others cannot. Where a node is exercised by fakes and again by the real dependency, say that the expected output is identical, and what a difference between them would mean.
- **The test equipment the design depends on** — named as scaffolding rather than production code, with where it lives.
- **A test that outgrows the section becomes a companion deck**, with one slide left behind carrying its conclusions and the link.

## 7 · Handoff — mandatory, always last of the content

Every design deck ends with what the next phase takes from this one: the contracts it may now assume frozen, the facts established, the tooling it inherits, and what deliberately did not ship.

- **Write it as the next phase's input list**, not as this phase's summary.
- **Name what does not carry forward**, so nothing is assumed reusable that is not.

## Diagrams come from the HLD

A node's component diagram and its call flow already exist in that node's design folder under [documents/Design/MODULE-DESIGN/](../../documents/Design/MODULE-DESIGN/) — the HLD is where the design was decided, so it is where the deck's picture of it comes from.

- **Never redraw a simplified version.** A deck-only architecture diagram is a second design document with no authority, and it starts disagreeing with the HLD the moment either is edited.
- **The deck carries a derived copy**, in [presentation/assets/](../../presentation/assets/) like every other asset, never a path reaching into a node folder. The copy names its source and states that the HLD copy is authoritative.
- **The copy is trimmed to fit, not shrunk to fit.** An HLD diagram is drawn for a page, not a slide: lift out its title and its legend, re-crop the canvas to what remains, and replay the legend on the slide after it. Where the diagram still will not carry its own type size, split it along a subsystem boundary across two slides.
- **A PlantUML sequence source is content, not an asset.** Its render is far larger than a slide and in the wrong visual language; read it, then draw the flow at slide fidelity.

Mechanics — the render box, the scale arithmetic, and the crop procedure — are in [drawio-svg-pairs.md](../skills/task-planning-presentation/references/drawio-svg-pairs.md).

## Companion decks

A section that outgrows its slides becomes its own deck rather than bloating this one — the pattern Phase 0 used for its smoke test.

- **The parent keeps one slide**: what the topic settled, and a link. Nothing from the companion is repeated.
- **Companions may only be spun out of sections 3, 4, 5 and 6.** Terminology, the blueprint, and the handoff stay in the parent — they are what makes it readable.
- **Both decks link each other** from the cover.

## How to apply

[[project-architecture]] applies this at the outline step of [design-presentation](../skills/design-presentation/SKILL.md), and again before shipping. A deck missing a mandatory section, or ordering them differently, is not done.
