# ADA ECU — design

The authoritative design of the ADA ECU: the detector, the fusion of own perception with relayed objects, and the R4 warning it emits. [[project-architecture]] produces and owns everything here.

**Code:** [ADA_ECU/](../../../../ADA_ECU/)

## Design authority

| Document | What it fixes |
|---|---|
| [ada-ecu-hld.md](ada-ecu-hld.md) | The node's components, folder structure, seams, contract and test strategy — the sole design authority for work in `ADA_ECU/` |
| [ada-ecu-design-decisions.md](ada-ecu-design-decisions.md) | `D1…Dn`, each with its rationale and rejected alternative; cited by number from the HLD |

## Diagram sources

| Source | Renders |
|---|---|
| [ada-ecu-module-architecture.drawio](ada-ecu-module-architecture.drawio) | [ada-ecu-module-architecture.svg](ada-ecu-module-architecture.svg) — the component map |
| [phase2-4-ada-ecu-components.puml](phase2-4-ada-ecu-components.puml) | The component diagram |
| [phase2-4-ada-ecu-callflow.puml](phase2-4-ada-ecu-callflow.puml) | The call-flow sequence |
| [phase2-4-ada-ecu-admission.puml](phase2-4-ada-ecu-admission.puml) | The track-admission state machine |

## Elsewhere

- **General knowledge** applied to this node: [documents/KnowledgeBase/](../../../KnowledgeBase/)
- **Superseded documents:** [ADA_ECU/doc/deprecated/](../../../../ADA_ECU/doc/deprecated/)
- **How to deploy and collect evidence:** [deploy-ada-ecu-walkthrough.md](../../../../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md)
