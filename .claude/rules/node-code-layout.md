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

Not every container in this repo is a node. **Diagnostic tools, simulators, and containers that mock another ECU** — so a node can be exercised alone in a reduced mini-blueprint — live at `tools/<name>/`, one folder per tool, outside the four node folders. The table below is the inventory of this repo's test equipment; one entry sits inside a node folder under the narrow exception in the boundary list, and carries its real path.

| Folder | What it is | Artifact |
|---|---|---|
| [tools/netcheck/](../../tools/netcheck/) | Baseline connectivity check; deploys as three Container nodes, role selected by `ROLE` | OCI image `m1-netcheck:latest` |
| [tools/comms_check/](../../tools/comms_check/) | Golden-vector UDP sender and `[EVT]`-stream assertion for the V2X comms chain; runs locally and in CI | Python scripts, never deployed |
| `tools/ada-bench/` | The V2X emitter and IVI sink standing in for those nodes in the isolated ADA Room, two roles selected by `ROLE` ([deploy-ada-ecu-walkthrough.md §2.3](../../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#23-the-bench-image--one-image-two-roles)). Stays in `tools/`: stdlib-only Python sharing no build with the C++ ADA node, so the consumer-folder exception below does not apply | OCI image `m1-ada-bench:latest` |
| `IVI_ECU/r4-simulator/` | The ADA node's stand-in, emitting the scenario-driven R4 stream the IVI node consumes; in `IVI_ECU/` under the consumer-folder exception below | OCI image `m1-r4-sim:latest` |

The boundary — this carves out test equipment, it does not weaken the product-code rule above:

- **Test equipment only.** A container that *replaces* a node in a reduced blueprint belongs here; a node's own real image never does. `tools/` is not a second home for product code that was awkward to place in a node folder.
- **A mock of node X does not live in X's folder.** It must be able to change without rebuilding the thing it tests, and must never ship inside the real image — so the four node folders stay one folder → one node → one image.
- **A mock may live in the *consumer* node's folder when it cannot leave that node's build.** A narrow exception to the placement above, never to what that bullet protects. All four conditions must hold, or it goes to `tools/`:
  - It mocks a **different node than the one hosting it** — the producer whose messages the host node consumes. The mocked node's own folder still holds no mock of itself.
  - **Moving it to `tools/` would break a contract rule**: it shares the host folder's build and its frozen-contract module, so a move either duplicates the contract models — a second, unversioned contract — or forces the cross-folder source import § Build rules forbids.
  - Its **artifact is separate from the host folder's own**, and the dependency runs one way: the mock depends on the shared contract module, never the host's shipped artifact on the mock. Changing the mock therefore rebuilds neither the node it mocks nor the artifact it feeds, and nothing of it ships inside a real image.
  - It is **listed in the table above** with its real path, and the design that placed it there records why.

  The one sanctioned case today is `IVI_ECU/r4-simulator/`, which shares `IVI_ECU/`'s Gradle build, wrapper, version catalog and `:contract` module, and builds `m1-r4-sim:latest` — separate from the node's APK, which does not depend on it. Rationale: [ivi-ecu-hld.md](../../IVI_ECU/doc/ivi-ecu-hld.md) decisions D1, D2 and D9.
- **These are not R5 nodes.** They may deploy as Container nodes, but they stand in for nodes rather than being them: no row in the node table above, no requirement number of their own, and no place in the full blueprint.
- **The Scenario Player stays a node folder.** Being test equipment is not what puts a folder here — *replacing a node* is. The bench is a node of the R5 blueprint with its own address and pin (R11), so it keeps [Scenario_Player/](../../Scenario_Player/).
- **Same build rules.** A test-equipment folder that builds an image — a `tools/<name>/`, or the in-folder mock above — is self-contained under § Build rules exactly as a node folder is: own `Dockerfile` (that section fixes where it may sit), own dependency manifest, own tests, no cross-folder source imports, no hardcoded tunables (role, peer addresses, ports and cadences come from env). A host-side tool that builds no image still obeys everything but the `Dockerfile` line.
- **Take contract fields from the node's `contracts/` file, never a private copy.** A tool whose field list has drifted accepts messages the real node would reject — the test passes and the system is still broken.

## Per-folder `doc/`

Each work folder — the four node folders above plus [plans/](../../plans/) — carries a `doc/` subfolder holding **report-style documents about that folder's design and rationale**: HLDs, design notes, decision rationale, and `doc/research_notes/` for investigation findings and their diagrams (`.puml`/`.svg`/`.drawio`). Existing examples: [Scenario_Player/doc/research_notes/](../../Scenario_Player/doc/research_notes/), [plans/doc/research_notes/](../../plans/doc/research_notes/).

- **Read before writing.** Any agent working in a folder reads that folder's `doc/` first — it is the local context (why the design is what it is, what was already investigated and rejected) that the requirements report and plan do not carry.
- **Write design & rationale there, not in code comments or the plan.** Design/rationale output for a node lands in that node's `doc/`; the HLD content and commit rules still apply ([hld-content-and-commit-format.md](hld-content-and-commit-format.md)).
- `doc/` is documentation only — no implementation code, and it is never part of the built image.
- The folder is created when its first document lands (git does not track empty directories), so a missing `doc/` means "nothing written yet", not "convention does not apply".
- Documents follow [markdown-writing-style](../skills/markdown-writing-style/SKILL.md); reference the report's requirement numbers instead of restating requirements.

## Build rules (all container nodes)

These apply unchanged to any image-building folder that is not a container node — a `tools/<name>/`, or the sanctioned in-folder mock of § `tools/`; read "node folder" as "build folder" there.

- Each node folder is **self-contained and independently buildable**: its own `Dockerfile` at the folder root, its own dependency manifest, its own tests. Build from the repo root, e.g. `docker build -t scenario-player:latest Scenario_Player/`.
- **Self-containment, not the `Dockerfile`'s path, is what that rule protects.** A folder whose primary artifact is not an image may carry a secondary image's `Dockerfile` in a subfolder, provided the **build context stays inside the folder** — the build still reads nothing outside it. Today: `IVI_ECU/r4-simulator/Dockerfile` built with context `IVI_ECU/`, because that folder's primary artifact is the APK and the image is secondary test equipment ([ivi-ecu-hld.md §11](../../IVI_ECU/doc/ivi-ecu-hld.md#11-tech-stack-build-and-ci)). A folder whose primary artifact *is* an image keeps its `Dockerfile` at the root.
- Local image tag → registry tag → blueprint `image` field: the tag/push commands and the node's blueprint config are in that node's guide, not restated here.
- **CI builds every image, but pushes only when `CARSKY_ZOT_API_KEY` is set.** Each image lane's push step is gated on that secret: without it the image is built, the step emits a `::notice::` saying it was not pushed, the pull-back verification is skipped, and the job still ends green. **A green lane is therefore not evidence that an image reached the registry** — read the run's push notice, or query the registry, before treating a tag as deployable.
- **No cross-node source imports.** Folders couple only through the frozen R1–R4 contracts; a shared artifact needed by two nodes is a contract deliverable, not a relative import across folders.
- **No hardcoded tunables** (CLAUDE.md governing principle 5): peer addresses, ports, thresholds, and cadences come from env vars or config files, with the values injected by the blueprint node config — see each node's guide for its env set.
- Unit tests live inside the node folder they test and must pass before a subtask is done ([task-planning-conventions.md](task-planning-conventions.md#subtask-discipline-non-negotiable)).

## Scenario_Player specifics (R11)

- **Entrypoint** `main.py` at the image workdir `/app`, which mirrors `Scenario_Player/` — fixed by the blueprint node config `command: ["python", "main.py"]` ([node-scenario-player.md](../../requirements/car-sky-guide/node-scenario-player.md#blueprint-node-config)).
- **Scenario configs are data, not code**: files under `Scenario_Player/scenarios/`, selected at runtime by the `SCENARIO_CONFIG` env var (default `/app/scenarios/default.yaml`). Different configs must produce observably different message streams — that is R11's acceptance, so scenario variants (e.g. C approaching vs C out of range) are added as config files, never as code branches.
- **Target peer** is the V2X ECU via `V2X_ECU_HOST`/`V2X_ECU_PORT`; the bench wires exactly one `ethernet` OUTPUT pin to the Ethernet Bridge (R6).
- **Codec path — resolved (2026-07-30):** how Python reaches the R1 encoder is decided in the [R11 HLD, decision D1](../../Scenario_Player/doc/phase1-scenario-player-hld.md): a `cpm_encode` C++ helper subprocess built inside `Scenario_Player/codec_helper/` from byte-synced copies of the V2X ECU codec-seam sources (sync-manifest extension, HLD D2). Implementation follows that HLD — no agent may improvise a different codec path.

## How to apply

- [[project-architecture]] resolves the target folder(s) against the table above before any HLD, and creates the folder's structure inside it — never at the repo root; the HLD and its diagrams go in that folder's `doc/`. A deliverable that is test equipment rather than node code goes to `tools/<name>/` — or, where all four conditions of the consumer-folder exception in § `tools/` hold, the consuming node's folder — with the design that places it there recording why.
- [[project-planner]] cites paths from these folders in every subtask brief, and pairs each node's feature tasks with its deployment tasks from that node's guide; a brief that depends on a design decision points at the `doc/` document that records it.
- Implementation subagents write only inside the folder their subtask names — a node folder, or the `tools/<name>/` a test-equipment subtask names — and read its `doc/` before making design-affecting choices.
