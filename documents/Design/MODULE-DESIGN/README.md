# Module design

This page contains links to authoritative design of every node: its HLD, its decision record, its module and test designs, and the diagram sources they are drawn from.

One folder per node. Each folder's own README indexes it in full, including the diagram sources listed here only by name.

## IVI-ECU

The head unit: receives the warning stream and renders it. Folder: [IVI-ECU/](IVI-ECU/) · Code: [IVI_ECU/](../../../IVI_ECU/)

| Document | Topic |
|---|---|
| [IVI ECU — high-level design](IVI-ECU/ivi-ecu-hld.md) | The sole design authority: components, folder structure, seams, contract and test strategy |
| [IVI ECU — design decisions](IVI-ECU/ivi-ecu-design-decisions.md) | D1–D13 with their rationale and rejected alternatives, cited by number from the HLD |
| [Mini-blueprint: ADA ECU + IVI ECU + Ethernet Bridge](IVI-ECU/phase5-mini-blueprint-ada-ivi.md) | The reduced topology the node is exercised in |
| [Simulating the ADA ECU and driving IVI logic](IVI-ECU/phase5-r4-simulator.md) | The stand-in producer's design |
| [How the IVI app observes ADA→IVI messages](IVI-ECU/ivi-r4-observation-pipeline.md) | Contributor note — explanatory, not authoritative |

Diagram sources: `ivi-ecu-module-architecture.drawio` / `.svg`, and four `.puml` component and call-flow sources.

## ADA-ECU

Perception and fusion: detects what it can see, admits what is relayed to it, and emits the warning. Folder: [ADA-ECU/](ADA-ECU/) · Code: [ADA_ECU/](../../../ADA_ECU/)

| Document | Topic |
|---|---|
| [ADA ECU — high-level design](ADA-ECU/ada-ecu-hld.md) | The sole design authority: components, folder structure, seams, contract and test strategy |
| [ADA ECU — design decisions](ADA-ECU/ada-ecu-design-decisions.md) | The decision record, cited by number from the HLD |

Diagram sources: `ada-ecu-module-architecture.drawio` / `.svg`, and three `.puml` sources covering components, call flow and track admission.

## V2X-ECU

The relay: encodes and decodes the cooperative message, carrying one vehicle's perception to another. Folder: [V2X-ECU/](V2X-ECU/) · Code: [V2X_ECU/](../../../V2X_ECU/)

| Document | Topic |
|---|---|
| [V2X ECU — high-level design](V2X-ECU/v2x-ecu-hld.md) | The sole design authority: components, folder structure, seams, contract and test strategy |
| [V2X ECU — design decisions](V2X-ECU/v2x-ecu-design-decisions.md) | The decision record, cited by number from the HLD |
| [Telux parity notes and port plan](V2X-ECU/telux-parity-and-port-plan.md) | Why the radio seam mirrors the vendor surface, and what changes on real modem hardware |

Diagram sources: `v2x-ecu-module-architecture.drawio` / `.svg`, and two `.puml` component and call-flow sources.

## Scenario Player

The bench: plays a scenario into the V2X ECU as cooperative message traffic. Test equipment, and a node of the blueprint in its own right. Folder: [SCENARIO-PLAYER/](SCENARIO-PLAYER/) · Code: [Scenario_Player/](../../../Scenario_Player/)

| Document | Topic |
|---|---|
| [Scenario Player — high-level design](SCENARIO-PLAYER/scenario-player-hld.md) | The sole design authority: components, folder structure, seams, contract and test strategy |
| [Scenario Player — design decisions](SCENARIO-PLAYER/scenario-player-design-decisions.md) | The decision record, cited by number from the HLD |
| [Scenario Player ↔ V2X ECU — call flow and message structure](SCENARIO-PLAYER/scenario-player-v2x-callflow-messages.md) | The exchange between the two, message by message, against the standards it cites |

Diagram sources: `scenario-player-module-architecture.drawio` / `.svg`, `cpm-message-structure.drawio`, and three `.puml` / `.svg` call-flow and component sources.
