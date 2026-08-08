# Design documents

The authoritative design of every node: its HLD, its decision record, its module and test designs, and the diagram sources they are drawn from. [[project-architecture]] produces and owns this section.

One subfolder per node, named for the node:

| Folder | Node | Code |
|---|---|---|
| [ADA-ECU/](ADA-ECU/) | ADA ECU — perception and fusion | [ADA_ECU/](../../ADA_ECU/) |
| [IVI-ECU/](IVI-ECU/) | IVI ECU — the AAOS head unit | [IVI_ECU/](../../IVI_ECU/) |
| [SCENARIO-PLAYER/](SCENARIO-PLAYER/) | Scenario Player — the bench | [Scenario_Player/](../../Scenario_Player/) |
| [V2X-ECU/](V2X-ECU/) | V2X ECU — the CPM relay | [V2X_ECU/](../../V2X_ECU/) |

Each subfolder carries a `README.md` separating its design authority from its diagram sources and any contributor notes, so a reader can tell which of the three binds.

## What does not live here

| Kind | Where |
|---|---|
| General knowledge a node's design draws on, not unique to it | [documents/KnowledgeBase/](../KnowledgeBase/) |
| Superseded documents, and reviews of work that has moved on | `<Node>/doc/deprecated/` |
| How to build, deploy and collect evidence | [requirements/car-sky-guide/](../../requirements/car-sky-guide/) |
| The requirements the design serves | [m1-cooperative-awareness.md](../../requirements/m1-cooperative-awareness.md) |
