# Design documents

How each node is built and why, written for a teammate who has not worked on it. One subfolder per ECU, named for the node.

| Folder | Node | Code |
|---|---|---|
| [ADA-ECU/](ADA-ECU/) | ADA ECU — perception and fusion | [ADA_ECU/](../../ADA_ECU/) |
| [IVI-ECU/](IVI-ECU/) | IVI ECU — the AAOS head unit | [IVI_ECU/](../../IVI_ECU/) |
| [V2X-ECU/](V2X-ECU/) | V2X ECU — the CPM relay | [V2X_ECU/](../../V2X_ECU/) |

Each subfolder carries a `README.md` indexing its documents, with the form (wiki, short pointer) and the topic of each.

The node's own `doc/` folder — [IVI_ECU/doc/](../../IVI_ECU/doc/) and its siblings — holds the HLD, the decision record and the pull-request reviews. Those are authorities and stay there; a document here explains them rather than restating them.
