# Scenario Player — design

The authoritative design of the bench: the node that plays a scenario into the V2X ECU as CPM traffic. [[project-architecture]] produces and owns everything here.

Being test equipment changes what the design says, not whether it has one — the Scenario Player is a node of the R5 blueprint with its own address and pin.

**Code:** [Scenario_Player/](../../../Scenario_Player/)

## Design authority

| Document | What it fixes |
|---|---|
| [scenario-player-hld.md](scenario-player-hld.md) | The node's components, folder structure, seams, contract and test strategy — the sole design authority for work in `Scenario_Player/` |
| [scenario-player-design-decisions.md](scenario-player-design-decisions.md) | `D1…Dn`, each with its rationale and rejected alternative; cited by number from the HLD |
| [scenario-player-v2x-callflow-messages.md](scenario-player-v2x-callflow-messages.md) | The exchange with the V2X ECU, message by message, verified against the ETSI sources it names |

## Diagram sources

| Source | Renders |
|---|---|
| [scenario-player-module-architecture.drawio](scenario-player-module-architecture.drawio) | [scenario-player-module-architecture.svg](scenario-player-module-architecture.svg) — the component map |
| [scenario-player-v2x-callflow.puml](scenario-player-v2x-callflow.puml) | [scenario-player-v2x-callflow.svg](scenario-player-v2x-callflow.svg) — the bench-to-V2X sequence |
| [cpm-message-structure.drawio](cpm-message-structure.drawio) | The CPM structure the call-flow note walks through |
| [scenario-player-components.puml](scenario-player-components.puml) | The component diagram |
| [phase1-scenario-player-callflow.puml](phase1-scenario-player-callflow.puml) | The call-flow sequence |

## Elsewhere

- **General knowledge** applied to this node: [documents/KnowledgeBase/](../../KnowledgeBase/)
- **The contract it produces:** [r1-cpm-profile.md](../../../contracts/r1-cpm-profile.md)
- **The node reference:** [node-scenario-player.md](../../../requirements/car-sky-guide/node-scenario-player.md)
