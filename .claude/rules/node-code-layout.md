# Node Code Layout & Build

Where implementation code lives, and how each node's artifact is built. Authoritative for the folder scoping done in [ecu-implementation-scoping](../skills/ecu-implementation-scoping/SKILL.md) and [high-level-design-procedure](../skills/high-level-design-procedure/SKILL.md), and for the file paths [[project-planner]] writes into subtask briefs.

## One top-level folder per R5 node

The repo has exactly four code folders — one per node in the R5 blueprint ([carsky-4-node-blueprint.md](../../requirements/car-sky-guide/carsky-4-node-blueprint.md)). No implementation code lives outside them.

| Folder | CarSky node | Requirements | Language / runtime (report §3(d)) | Artifact | Node guide |
|---|---|---|---|---|---|
| [Scenario_Player/](../../Scenario_Player/) | Container Node (bench) | R11 | Python | OCI image `scenario-player:latest` | [node-scenario-player.md](../../requirements/car-sky-guide/node-scenario-player.md) |
| [V2X_ECU/](../../V2X_ECU/) | Container Node | R1, R7–R9 (R10 deferred) | C++17 (Vanetza codec in-process) | OCI image `v2x-ecu:latest` | [node-v2x-ecu.md](../../requirements/car-sky-guide/node-v2x-ecu.md) |
| [ADA_ECU/](../../ADA_ECU/) | Container Node | R3, R12–R15 | C++17 core + Python detector subprocess | OCI image `ada-ecu:latest` | [node-ada-ecu.md](../../requirements/car-sky-guide/node-ada-ecu.md) |
| [IVI_ECU/](../../IVI_ECU/) | Skycraft Node (AAOS) | R4, R16–R17 | Kotlin / Jetpack Compose | APK via Gradle | [node-ivi-ecu.md](../../requirements/car-sky-guide/node-ivi-ecu.md) |

- **Bench ≠ V2X ECU.** The Scenario Player is a separate node with its own folder and its own image — it is sanctioned test equipment sharing the Room network (CLAUDE.md governing principle 2), not a module of `V2X_ECU/`. R11 code never lands in `V2X_ECU/`.
- Cross-cutting requirements (R2, R5, R6, R18, R19) touch more than one folder — list every folder touched, never just the first match.

## Per-folder `doc/`

Each work folder — the four node folders above plus [plans/](../../plans/) — carries a `doc/` subfolder holding **report-style documents about that folder's design and rationale**: HLDs, design notes, decision rationale, and `doc/research_notes/` for investigation findings and their diagrams (`.puml`/`.svg`/`.drawio`). Existing examples: [Scenario_Player/doc/research_notes/](../../Scenario_Player/doc/research_notes/), [plans/doc/research_notes/](../../plans/doc/research_notes/).

- **Read before writing.** Any agent working in a folder reads that folder's `doc/` first — it is the local context (why the design is what it is, what was already investigated and rejected) that the requirements report and plan do not carry.
- **Write design & rationale there, not in code comments or the plan.** Design/rationale output for a node lands in that node's `doc/`; the HLD content and commit rules still apply ([hld-content-and-commit-format.md](hld-content-and-commit-format.md)).
- `doc/` is documentation only — no implementation code, and it is never part of the built image.
- The folder is created when its first document lands (git does not track empty directories), so a missing `doc/` means "nothing written yet", not "convention does not apply".
- Documents follow [markdown-writing-style](../skills/markdown-writing-style/SKILL.md); reference the report's requirement numbers instead of restating requirements.

## Build rules (all container nodes)

- Each node folder is **self-contained and independently buildable**: its own `Dockerfile` at the folder root, its own dependency manifest, its own tests. Build from the repo root, e.g. `docker build -t scenario-player:latest Scenario_Player/`.
- Local image tag → registry tag → blueprint `image` field: the tag/push commands and the node's blueprint config are in that node's guide, not restated here.
- **No cross-node source imports.** Folders couple only through the frozen R1–R4 contracts; a shared artifact needed by two nodes is a contract deliverable, not a relative import across folders.
- **No hardcoded tunables** (CLAUDE.md governing principle 5): peer addresses, ports, thresholds, and cadences come from env vars or config files, with the values injected by the blueprint node config — see each node's guide for its env set.
- Unit tests live inside the node folder they test and must pass before a subtask is done ([task-planning-conventions.md](task-planning-conventions.md#subtask-discipline-non-negotiable)).

## Scenario_Player specifics (R11)

- **Entrypoint** `main.py` at the image workdir `/app`, which mirrors `Scenario_Player/` — fixed by the blueprint node config `command: ["python", "main.py"]` ([node-scenario-player.md](../../requirements/car-sky-guide/node-scenario-player.md#blueprint-node-config)).
- **Scenario configs are data, not code**: files under `Scenario_Player/scenarios/`, selected at runtime by the `SCENARIO_CONFIG` env var (default `/app/scenarios/default.yaml`). Different configs must produce observably different message streams — that is R11's acceptance, so scenario variants (e.g. C approaching vs C out of range) are added as config files, never as code branches.
- **Target peer** is the V2X ECU via `V2X_ECU_HOST`/`V2X_ECU_PORT`; the bench wires exactly one `ethernet` OUTPUT pin to the Ethernet Bridge (R6).
- **Open item — flag, don't invent:** the report §3(c) has the bench (Python) drive "the shared R1 codec (Vanetza-based encoder)", but Vanetza is a C++ library. How Python reaches the R1 encoder (binding, sidecar, or pre-encoded vectors) is unresolved; [[project-architecture]] decides it in the R11 HLD and records it here-adjacent, in that HLD — no agent may improvise a codec path in implementation code.

## How to apply

- [[project-architecture]] resolves the target folder(s) against the table above before any HLD, and creates the folder's structure inside it — never at the repo root; the HLD and its diagrams go in that folder's `doc/`.
- [[project-planner]] cites paths from these folders in every subtask brief, and pairs each node's feature tasks with its deployment tasks from that node's guide; a brief that depends on a design decision points at the `doc/` document that records it.
- Implementation subagents write only inside the node folder their subtask names, and read its `doc/` before making design-affecting choices.
