# IVI ECU — design

The authoritative design of the IVI ECU: the high-level design, the decisions behind it, the module and test designs, and the diagrams they are drawn from. [[project-architecture]] produces and owns everything in this section.

**Code:** [IVI_ECU/](../../../IVI_ECU/)

## Design authority

| Document | What it fixes |
|---|---|
| [ivi-ecu-hld.md](ivi-ecu-hld.md) | The node's components, folder structure, seams, contract and test strategy — the sole design authority for work in `IVI_ECU/` |
| [ivi-ecu-design-decisions.md](ivi-ecu-design-decisions.md) | `D1…Dn`, each with its rationale and rejected alternative; cited by number from the HLD |
| [phase5-mini-blueprint-ada-ivi.md](phase5-mini-blueprint-ada-ivi.md) | The reduced ADA→IVI blueprint the node is exercised in |
| [phase5-r4-simulator.md](phase5-r4-simulator.md) | The R4 simulator's design — the stand-in that feeds the node its input |

## Diagram sources

| Source | Renders |
|---|---|
| [ivi-ecu-module-architecture.drawio](ivi-ecu-module-architecture.drawio) | [ivi-ecu-module-architecture.svg](ivi-ecu-module-architecture.svg) — the component map the HLD and the deck both embed |
| [phase5-ivi-ecu-components.puml](phase5-ivi-ecu-components.puml) · [phase5-ivi-components.puml](phase5-ivi-components.puml) | The component diagrams |
| [phase5-ivi-ecu-callflow.puml](phase5-ivi-ecu-callflow.puml) · [phase5-ivi-callflow.puml](phase5-ivi-callflow.puml) | The call-flow sequences |

## Contributor notes

Learning notes by Vũ Xuân Bách — explanatory, not authoritative.

| Document | Form | Topic |
|---|---|---|
| [ivi-android-screen-lifecycle.md](ivi-android-screen-lifecycle.md) | Wiki (long) | How one screen reaches the AAOS display: Manifest → Activity → Compose modes |
| [ivi-r4-observation-pipeline.md](ivi-r4-observation-pipeline.md) | Wiki (long) | How the IVI app observes R4 from the ADA ECU |

## Elsewhere

- **General knowledge** applied to this node, rather than unique to it: [documents/KnowledgeBase/](../../KnowledgeBase/) — including [android-automotive-os.md](../../KnowledgeBase/android-automotive-os.md), the platform this node runs on
- **Superseded reviews and audits:** [IVI_ECU/doc/deprecated/](../../../IVI_ECU/doc/deprecated/)
- **How to deploy and collect evidence:** [deploy-ivi-hmi-walkthrough.md](../../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md)
- **Decks:** design [phase5-design-ivi-ecu-deck.md](../../../presentation/phase5/phase5-design-ivi-ecu-deck.md) · demo [phase5-ivi-deck.md](../../../presentation/phase5/phase5-ivi-deck.md)
