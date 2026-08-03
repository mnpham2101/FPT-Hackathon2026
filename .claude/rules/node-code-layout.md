# Node Code Layout & Build

Where implementation code lives, and how each node's artifact is built. Authoritative for the folder scoping done in [ecu-implementation-scoping](../skills/ecu-implementation-scoping/SKILL.md) and [high-level-design-procedure](../skills/high-level-design-procedure/SKILL.md), and for the file paths [[project-planner]] writes into subtask briefs.

## One top-level folder per R5 node

The repo has exactly four **node** code folders — one per node in the R5 blueprint ([carsky-4-node-blueprint.md](../../requirements/car-sky-guide/carsky-4-node-blueprint.md)). No product code lives outside them; the one sanctioned category outside is test equipment, in `tools/` (§ `tools/` below).

| Folder | CarSky node | Requirements | Language / runtime (report §3(d)) | Artifact | Node guide |
|---|---|---|---|---|---|
| [Scenario_Player/](../../Scenario_Player/) | Container Node (bench) | R11 | Python | OCI image `scenario-player:latest` | [node-scenario-player.md](../../requirements/car-sky-guide/node-scenario-player.md) |
| [V2X_ECU/](../../V2X_ECU/) | Container Node | R1, R7–R9 (R10 deferred) | C++17 (Vanetza codec in-process) | OCI image `v2x-ecu:latest` | [node-v2x-ecu.md](../../requirements/car-sky-guide/node-v2x-ecu.md) |
| [ADA_ECU/](../../ADA_ECU/) | Container Node | R3, R12–R15 | C++17 core + Python detector subprocess | OCI image `ada-ecu:latest` | [node-ada-ecu.md](../../requirements/car-sky-guide/node-ada-ecu.md) |
| [IVI_ECU/](../../IVI_ECU/) | Skycraft Node (AAOS) | R4, R16–R17 | Kotlin / Jetpack Compose | APK via Gradle | [node-ivi-ecu.md](../../requirements/car-sky-guide/node-ivi-ecu.md) |

- **Bench ≠ V2X ECU.** The Scenario Player is a separate node with its own folder and its own image — it is sanctioned test equipment sharing the Room network (CLAUDE.md governing principle 2), not a module of `V2X_ECU/`. R11 code never lands in `V2X_ECU/`.
- Cross-cutting requirements (R2, R5, R6, R18, R19) touch more than one folder — list every folder touched, never just the first match.

## `tools/` — test equipment and ECU mocks

Not every container in this repo is a node. **Diagnostic tools, simulators, and containers that mock another ECU** — so a node can be exercised alone in a reduced mini-blueprint — live at `tools/<name>/`, one folder per tool, outside the four node folders.

| Folder | What it is | Artifact |
|---|---|---|
| [tools/netcheck/](../../tools/netcheck/) | Baseline connectivity check; deploys as three Container nodes, role selected by `ROLE` | OCI image `m1-netcheck:latest` |
| [tools/comms_check/](../../tools/comms_check/) | Golden-vector UDP sender and `[EVT]`-stream assertion for the V2X comms chain; runs locally and in CI | Python scripts, never deployed |
| `tools/ada-bench/` | The V2X emitter and IVI sink standing in for those nodes in the isolated ADA Room, two roles selected by `ROLE` ([deploy-ada-ecu-walkthrough.md §2.3](../../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#23-the-bench-image--one-image-two-roles)) | OCI image `m1-ada-bench:latest` |

The boundary — this carves out test equipment, it does not weaken the product-code rule above:

- **Test equipment only.** A container that *replaces* a node in a reduced blueprint belongs here; a node's own real image never does. `tools/` is not a second home for product code that was awkward to place in a node folder.
- **A mock of node X does not live in X's folder.** It must be able to change without rebuilding the thing it tests, and must never ship inside the real image — so the four node folders stay one folder → one node → one image.
- **These are not R5 nodes.** They may deploy as Container nodes, but they stand in for nodes rather than being them: no row in the node table above, no requirement number of their own, and no place in the full blueprint.
- **The Scenario Player stays a node folder.** Being test equipment is not what puts a folder here — *replacing a node* is. The bench is a node of the R5 blueprint with its own address and pin (R11), so it keeps [Scenario_Player/](../../Scenario_Player/).
- **Same build rules.** A `tools/<name>/` that builds an image is self-contained under § Build rules exactly as a node folder is: own `Dockerfile` at the folder root, own dependency manifest, own tests, no cross-folder source imports, no hardcoded tunables (role, peer addresses, ports and cadences come from env). A host-side tool that builds no image still obeys everything but the `Dockerfile` line.
- **Mirrored contracts are copied, never forked.** A tool that speaks R1–R4 takes its field list from the owning node's `contracts/` copy; a drifted copy makes the tool pass messages the real consumer rejects.

## Per-folder `doc/`

Each work folder — the four node folders above plus [plans/](../../plans/) — carries a `doc/` subfolder holding **report-style documents about that folder's design and rationale**: HLDs, design notes, decision rationale, and `doc/research_notes/` for investigation findings and their diagrams (`.puml`/`.svg`/`.drawio`). Existing examples: [Scenario_Player/doc/research_notes/](../../Scenario_Player/doc/research_notes/), [plans/doc/research_notes/](../../plans/doc/research_notes/).

- **Read before writing.** Any agent working in a folder reads that folder's `doc/` first — it is the local context (why the design is what it is, what was already investigated and rejected) that the requirements report and plan do not carry.
- **Write design & rationale there, not in code comments or the plan.** Design/rationale output for a node lands in that node's `doc/`; the HLD content and commit rules still apply ([hld-content-and-commit-format.md](hld-content-and-commit-format.md)).
- `doc/` is documentation only — no implementation code, and it is never part of the built image.
- The folder is created when its first document lands (git does not track empty directories), so a missing `doc/` means "nothing written yet", not "convention does not apply".
- Documents follow [markdown-writing-style](../skills/markdown-writing-style/SKILL.md); reference the report's requirement numbers instead of restating requirements.

## Build rules (all container nodes)

These apply unchanged to any image-building `tools/<name>/` folder — read "node folder" as "build folder" there.

- Each node folder is **self-contained and independently buildable**: its own `Dockerfile` at the folder root, its own dependency manifest, its own tests. Build from the repo root, e.g. `docker build -t scenario-player:latest Scenario_Player/`.
- Local image tag → registry tag → blueprint `image` field: the tag/push commands and the node's blueprint config are in that node's guide, not restated here.
- **No cross-node source imports.** Folders couple only through the frozen R1–R4 contracts; a shared artifact needed by two nodes is a contract deliverable, not a relative import across folders.
- **No hardcoded tunables** (CLAUDE.md governing principle 5): peer addresses, ports, thresholds, and cadences come from env vars or config files, with the values injected by the blueprint node config — see each node's guide for its env set.
- Unit tests live inside the node folder they test and must pass before a subtask is done ([task-planning-conventions.md](task-planning-conventions.md#subtask-discipline-non-negotiable)).

## Scenario_Player specifics (R11)

- **Entrypoint** `main.py` at the image workdir `/app`, which mirrors `Scenario_Player/` — fixed by the blueprint node config `command: ["python", "main.py"]` ([node-scenario-player.md](../../requirements/car-sky-guide/node-scenario-player.md#blueprint-node-config)).
- **Scenario configs are data, not code**: files under `Scenario_Player/scenarios/`, selected at runtime by the `SCENARIO_CONFIG` env var (default `/app/scenarios/default.yaml`). Different configs must produce observably different message streams — that is R11's acceptance, so scenario variants (e.g. C approaching vs C out of range) are added as config files, never as code branches.
- **Target peer** is the V2X ECU via `V2X_ECU_HOST`/`V2X_ECU_PORT`; the bench wires exactly one `ethernet` OUTPUT pin to the Ethernet Bridge (R6).
- **Codec path — resolved (2026-07-30):** how Python reaches the R1 encoder is decided in the [R11 HLD, decision D1](../../Scenario_Player/doc/phase1-scenario-player-hld.md): a `cpm_encode` C++ helper subprocess built inside `Scenario_Player/codec_helper/` from byte-synced copies of the V2X ECU codec-seam sources (sync-manifest extension, HLD D2). Implementation follows that HLD — no agent may improvise a different codec path.

## How to apply

- [[project-architecture]] resolves the target folder(s) against the table above before any HLD, and creates the folder's structure inside it — never at the repo root; the HLD and its diagrams go in that folder's `doc/`. A deliverable that is test equipment rather than node code goes to `tools/<name>/`, with the design that places it there recording why.
- [[project-planner]] cites paths from these folders in every subtask brief, and pairs each node's feature tasks with its deployment tasks from that node's guide; a brief that depends on a design decision points at the `doc/` document that records it.
- Implementation subagents write only inside the folder their subtask names — a node folder, or the `tools/<name>/` a test-equipment subtask names — and read its `doc/` before making design-affecting choices.
