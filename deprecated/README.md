# deprecated/

Documents kept for the citations that still point at them, and for nothing else. Nothing here is authority: a document in this folder never settles a design question, and no new work cites it.

| Document | Why it is here | Live authority |
|---|---|---|
| [phase0-contract-freeze-hld.md](phase0-contract-freeze-hld.md) + [phase0-contract-freeze-call-flow.puml](phase0-contract-freeze-call-flow.puml) | A phase HLD, against the one-HLD-per-node rule of [hld-content-and-commit-format.md](../.claude/rules/hld-content-and-commit-format.md), and placed in `plans/doc/` rather than a node folder | The four node HLDs for design; [contracts/](../contracts/) for the frozen R1–R4 contracts themselves |

Its decisions D1–D4 are cited by the §2 reading list of [scenario-player-hld.md](../Scenario_Player/doc/scenario-player-hld.md), [v2x-ecu-hld.md](../V2X_ECU/doc/v2x-ecu-hld.md) and [ada-ecu-hld.md](../ADA_ECU/doc/ada-ecu-hld.md), which is why the file is retained rather than deleted. Folding those decisions into the node HLDs that depend on them is [[project-architecture]]'s call.
