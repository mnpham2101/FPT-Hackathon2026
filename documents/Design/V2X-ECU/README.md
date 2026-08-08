# V2X ECU — design

The authoritative design of the V2X ECU: the CPM profile, the codec, and the relay that carries B's perception of C to A. [[project-architecture]] produces and owns everything here.

**Code:** [V2X_ECU/](../../../V2X_ECU/)

## Design authority

| Document | What it fixes |
|---|---|
| [v2x-ecu-hld.md](v2x-ecu-hld.md) | The node's components, folder structure, seams, contract and test strategy — the sole design authority for work in `V2X_ECU/` |
| [v2x-ecu-design-decisions.md](v2x-ecu-design-decisions.md) | `D1…Dn`, each with its rationale and rejected alternative; cited by number from the HLD |
| [telux-parity-and-port-plan.md](telux-parity-and-port-plan.md) | Why the radio seam mirrors the telux surface, and what changes when the node runs against real modem hardware |

## Diagram sources

| Source | Renders |
|---|---|
| [v2x-ecu-module-architecture.drawio](v2x-ecu-module-architecture.drawio) | [v2x-ecu-module-architecture.svg](v2x-ecu-module-architecture.svg) — the component map |
| [v2x-ecu-components.puml](v2x-ecu-components.puml) | The component diagram |
| [phase1-v2x-ecu-callflow.puml](phase1-v2x-ecu-callflow.puml) | The call-flow sequence |

## Elsewhere

- **General knowledge** applied to this node: [documents/KnowledgeBase/](../../KnowledgeBase/)
- **The wire to the bench:** [scenario-player-v2x-callflow-messages.md](../SCENARIO-PLAYER/scenario-player-v2x-callflow-messages.md)
- **The node reference:** [node-v2x-ecu.md](../../../requirements/car-sky-guide/node-v2x-ecu.md)
