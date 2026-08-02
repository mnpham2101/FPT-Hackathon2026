---
marp: true
theme: default
paginate: true
title: Phase 0 — Task Execution
description: Planning deck — the vocabulary of tracks, lanes and task groups, how Phase 0's contract freeze was decomposed, what code it delivered, and in what order the six lanes actually ran
deck: Phase 0 — Task Execution · FPT Hackathon 2026
---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-title-city.jpg)

# Phase 0 — Task Execution

## From six lanes to twenty-seven atomic commits

**Milestone 1 · Cooperative Vehicle Awareness — FPT Hackathon 2026**

contract freeze · 8 task groups · 27 subtasks · closed 2026-07-31

The architecture it builds is Deck A: [phase0-design-concepts-deck.html](phase0-design-concepts-deck.html).

Source: [phase0_tasks.md](../../plans/phase0_tasks.md) · [phase0-contract-freeze-hld.md](../../plans/doc/phase0-contract-freeze-hld.md) · [phase1_tasks.md](../../plans/phase1_tasks.md)

---

# Table of contents

1. **Work organization** — track, execution lane, CI lane, and why they are not interchangeable
2. **Work decomposition** — the ID scheme, and the shape of Phase 0
3. **Task groups** — which lane each group builds, and who consumes it
4. **Code delivered** — the files Phase 0 put in the repository
5. **Execution order** — lane by lane, one diagram at a time
6. **Parallel or sequential** — and what forced each
7. **Handoff** — what Phase 1 was allowed to assume

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 01 · Work organization

---

# Planning terminology

Phase 0 and Phase 1 both use the word *lane* — for two different things. Neither of them is a *track*.

| Term               | What it groups                                                                    | Defined in                                        | Example               |
| ------------------ | --------------------------------------------------------------------------------- | ------------------------------------------------- | --------------------- |
| **Track**          | a workstream of whole phases, run by different people at the same time            | [milestone1.md §3](../../plans/milestone1.md)     | comms track = Phase 1 |
| **Execution lane** | subtasks that must run in dependency order, grouped by the folder they write into | each phase plan's *Execution order & parallelism* | Lane B = `V2X_ECU/`   |
| **CI lane**        | one job in the GitHub Actions workflow                                            | `.github/workflows/phase0-ci.yml`                 | `v2x-core-build`      |

- **A track spans phases; a lane lives inside one phase.** "The V2X work" is Lane B in Phase 0, Lane V in Phase 1, and the comms track across the milestone — three different objects with three different lifetimes.
- **A CI lane is work a machine actually performs.** It builds, tests, and either passes or fails; three of the ten also push a node image. A track and an execution lane are planning structure, not programs.

---

# Tracks

A track is a workstream that belongs to people and runs for weeks. Three of them run at once — possible only because Phase 0 first froze the **contracts**.

> A **contract** is an agreed message or data format: the exact fields, types and units two nodes exchange, written down once and never changed without re-agreeing with every consumer. This project has six of them, covering every message the nodes exchange — and **defining them is what Phase 0 was for**.

| Track                | Phases        | Builds against                                                                                                               |
| -------------------- | ------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| **Comms**            | 1             | real V2X messages on the wire from the start; mock perception contents until Phase 6                                         |
| **Perception (ADA)** | 2, then 3 ∥ 4 | the tracked-object store — detection writes its own entries, fusion consumes the store and the live message from the V2X ECU |
| **Display**          | 5             | mock warning messages from the start                                                                                         |

- **Tracks share contracts (message schema)** Phase 0 defines massage scheme, or inputs to be shared by phases, allow them to mocked input and run in parallel.
- **Phases 3 and 4 are one track running two phases side by side.** They never call each other; they meet only at the tracked-object store.
- **The tracks converge at Phase 6**, where every mock is replaced by real data.

---

# Tracks and the phases inside them

One contract freeze, three tracks running side by side, one convergence.

![h:440 Three tracks — comms, perception, display — each with its phases, gated by the Phase 0 contract freeze and converging at Phase 6](../assets/phase0-tracks-phases.svg)

---

# Execution lanes — sequential subtasks

Every lane in both phases. A lane is named after the folder it writes into, so no two touch the same files.

| Phase | Lane | Writes into        | Waits on                         | What the lane lands                                                             |
| ----- | ---- | ------------------ | -------------------------------- | ------------------------------------------------------------------------------- |
| **0** | A    | `contracts/`       | nothing — it is the source       | the message profile, all four schemas, shared samples                           |
| **0** | B    | `V2X_ECU/`         | A, from its codec steps on       | C++ toolchain, the codec seam, the six reference messages                       |
| **0** | C    | `ADA_ECU/`         | A, from its bindings on          | ADA toolchain and its C++ and Python message bindings                           |
| **0** | D    | `Scenario_Player/` | A and B                          | the bench-side message dataclass and its round-trip test                        |
| **0** | E    | `IVI_ECU/`         | A                                | the Kotlin bindings the display decodes into                                    |
| **0** | F    | the live blueprint | nothing                          | the baseline connectivity smoke test, run on the platform                       |
| **1** | V    | `V2X_ECU/`         | Phase 0's contracts              | receive pipeline, composition root, node image                                  |
| **1** | P    | `Scenario_Player/` | Phase 0's contracts + codec seam | the bench application and its codec helper                                      |
| **1** | D    | the deployed Room  | V and P                          | push the images, deploy the blueprint, read the logs to confirm messages arrive |

- **Inside a lane, the order is fixed.** Each subtask consumes the output of the one before it.
- **Between lanes, there is no order at all.** A through F are names, not a sequence. Only *Waits on* sequences them, and it names an artifact, never a letter.
---

# CI lanes — the Linux verification path

The dev host has no Docker and no WSL. Every Linux build, every C++ compile, every Gradle run therefore happens on GitHub Actions: CI is not a safety net here, it is *the* verification path.

- **Ten jobs in one file:** `contracts-gate` · `python-tests` · `ada-core-build` · `v2x-core-build` · `v2x-comms-check` · `sp-codec-helper` · `ivi-unit-tests` · `netcheck-image` · `v2x-ecu-image` · `scenario-player-image`.
- **The file `phase0-ci.yml`** defines jobd on GithubAction. Phase 1 added four jobs to it instead of starting a second workflow 

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 02 · Work decomposition

---

# The unit of work is a subtask, and its ID says everything

Every task and subtask carries `X.Y.Z.W`.

| Segment | Reads as               | In `2.0.3.1`                     |
| ------- | ---------------------- | -------------------------------- |
| `X`     | the requirement served | the V2X-to-ADA object message    |
| `Y`     | the phase              | 0 — the contract freeze          |
| `Z`     | the task group         | group 0.3 — V2X-to-ADA bindings  |
| `W`     | the subtask            | the V2X-side binding, one commit |

**The group number is not the requirement number.** `2.0.3.1` and `3.0.4.2` sit in different groups and serve different requirements; a group can hold subtasks from several requirements, which is why group 0.3 contains both `2.0.3.1` and `2.0.3.2`.

---

# What Phase 0 looked like as a work item

- **8 task groups, 27 subtasks, 6 lanes.** Lane A `contracts/` 6 · lane B `V2X_ECU/` 6 · lane C `ADA_ECU/` 6 · lane D `Scenario_Player/` 1 · lane E `IVI_ECU/` 2 · lane F smoke test 4 · plus the integrity gate and the CI workflow, one subtask each.
- **Three execution kinds.** 24 subtasks implemented by spawned agents, 1 executed by the deployment agent, 2 performed by the user in the Nydus UI. The plan tracks all three the same way; only the executor differs.
- **One subtask, one objective, one atomic commit** — plus a build that passes and unit tests that pass. A subtask without a `**Status:**` line in the plan file is not started.
- **Verification done by GithubAction** The dev host has no Docker and no WSL, and its JDK exceeds the Gradle wrapper's supported range, so GitHub Actions is the verification authority. Only lane D could be run on the developer's own machine.

---

# Tasks assigned to lanes

Left: what must happen, in what order. Right: what proves it happened.

![h:450 Phase 0 execution lanes mapped onto the CI lanes that verify them](../assets/phase0-lanes.svg)

---

# The four acceptance boxes

| Acceptance box                                                                                            | Closed by                                                       | Proof                                                                                                                                                                                                          |
| --------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| V2X message profile committed; the reference messages encode and decode through the codec seam            | `1.0.1.1` · `1.0.1.2` · `1.0.2.1`–`1.0.2.5`                     | the [`v2x-core-build` lane green](https://github.com/mnpham2101/FPT-Hackathon2026/actions/runs/30608005574) — all six reference messages decode to their readable form and re-encode to exactly the same bytes |
| V2X-to-ADA, tracked-object and warning schemas committed; round-trip tests pass in C++, Python and Kotlin | group 0.1 schemas + six round-trip suites across groups 0.3–0.6 | the [`contracts-gate` lane green](https://github.com/mnpham2101/FPT-Hackathon2026/actions/runs/30608202261), over all 36 synced copies                                                                         |
| The warning-message additive-version test is defined                                                      | `4.0.1.6` · `4.0.4.4` · `4.0.6.2`                               | the [`ada-core-build` lane green](https://github.com/mnpham2101/FPT-Hackathon2026/actions/runs/30602717230) — one shared fixture, both consumers                                                               |
| Blueprint topology documented and validated                                                               | `6.0.8.1` · `5.0.8.2` · `5.0.8.3` · `6.0.8.4`                   | a live deployment, not a CI run: 5/5 nodes Running, criteria C1–C5 met                                                                                                                                         |

> Three boxes were closed by a green pipeline. The fourth could only be closed by deploying to the platform and reading the logs — which is why lane F needed a human to finish it.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 03 · Task groups

---

# Every task group builds a lane, or feeds a later phase's lane

| Task group                                                  | What it delivers, and which lane it serves                                                                                                                                                                                           |
| ----------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **0.1** — contract source of truth, `contracts/`            | Writes the V2X message profile document plus all four message schemas and shared samples. **Implements lane A.** Its files are the compile-time input of every other lane and of every binding written after Phase 0.                |
| **0.2** — V2X ECU toolchain, codec seam, reference messages | Brings up the C++17 build, freezes the single V2X codec source, generates the six reference messages. **Implements lane B.** Phase 1's receive pipeline decodes through this seam; the bench encoder helper reuses the same sources. |
| **0.3** — V2X-to-ADA bindings, both C++ ends                | One handwritten binding per node, producer and consumer. **Splits across lanes B and C** — `2.0.3.1` in `V2X_ECU/`, `2.0.3.2` in `ADA_ECU/`. Phase 1 forwards that message with exactly these bindings.                              |
| **0.4** — ADA ECU contract layer                            | ADA toolchain, tracked-object and warning C++ bindings, the additive-version test, the Python detector binding. **Implements lane C.** Phase 2 extends the tree; Phases 3 and 4 meet across the tracked-object struct.               |
| **0.5** — Scenario Player contract layer                    | The bench-side CpmContent dataclass and its golden round-trip test. **Implements lane D.** Phase 1's bench starts here — and it deliberately stops short of an encoder.                                                              |
| **0.6** — IVI Kotlin warning and snapshot bindings          | The sealed Kotlin warning binding and the finalized tracked-object snapshot models. **Implements lane E.** Phase 5's HMI decodes into these models instead of defining its own.                                                      |
| **0.7** — contract integrity gate and CI                    | The byte-identity gate over every synced copy, and the Linux verification workflow. **Implements the gate and the CI lanes.** Every later subtask in the project is verified by this file.                                           |
| **0.8** — baseline connectivity smoke test                  | Builds, pushes, deploys and reads the netcheck tool on the live blueprint. **Implements lane F.** It is why Phase 1 could assume a deployable Room and reachable nodes.                                                              |

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 04 · Code delivered

---

# Shared contracts, the gate, the pipeline, the bench tool

| File group                                                                     | Delivered by                                                         | Purpose                                                                                                                                                                                       |
| ------------------------------------------------------------------------------ | -------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `contracts/` — `r1-cpm-profile.md`, four JSON Schemas, five shared samples     | group 0.1, subtasks `1.0.1.1` through `4.0.1.6`                      | The single source of truth every node copies from. It freezes, and re-freezes, as one unit.                                                                                                   |
| `contracts/golden-vectors/` — six readable + encoded pairs                     | `1.0.2.4`                                                            | The reference messages: each one's content, plus the exact bytes it must encode to. Generated by a build-only tool, and published by the CI run that proved regenerating them is byte-stable. |
| `contracts/sync-manifest.json` + `check_sync.py`                               | `1.0.7.1`                                                            | Maps 21 sources onto 36 node-local copies and exits 1 on any byte difference; it also carries the banned-token check over the V2X sources.                                                    |
| `.github/workflows/phase0-ci.yml`                                              | created by `1.0.7.2`; lanes added by `1.0.2.1`, `3.0.4.1`, `5.0.8.2` | The build-configuration workflow — the project's Linux verification path, where C++ builds, Gradle tests and image builds actually run.                                                       |
| `tools/netcheck/` — `Dockerfile`, `entrypoint.sh`, `capture.sh`, `netcheck.py` | `6.0.8.1`                                                            | The netcheck tool: one throwaway image, three container roles, self-starting at node start with no manual exec.                                                                               |

---

# Per-node code — libraries, message definitions, skeletons

| File group                                                                                                     | Delivered by                    | Purpose                                                                                                                                                                               |
| -------------------------------------------------------------------------------------------------------------- | ------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `V2X_ECU/src/codec/` — `cpm_codec.hpp`, `vanetza_cpm_codec.{hpp,cpp}`                                          | `1.0.2.2`, `1.0.2.3`            | The V2X library files: the codec seam interface and its only implementation over the ITS-r2 CPM type. Nothing above the seam converts units.                                          |
| `V2X_ECU/src/contracts/r2_message.{hpp,cpp}` + `V2X_ECU/contracts/*.schema.json`                               | `2.0.3.1`, `1.0.2.2`            | The V2X message-definition files: the V2X-to-ADA producer binding and the synced schema copies it is written against.                                                                 |
| `V2X_ECU/CMakeLists.txt` · `tools/golden_vectors/main.cpp` · `tests/`                                          | `1.0.2.1`, `1.0.2.4`, `1.0.2.5` | Toolchain with exact dependency pins, the generator that never ships in the node image, and the tests that hold every reference message to its exact bytes.                           |
| `ADA_ECU/` — `CMakeLists.txt`, `src/contracts/` for all three messages, `detector/contracts/tracked_object.py` | group 0.4 plus `2.0.3.2`        | The ADA skeleton: the consuming end of the V2X-to-ADA message, the producing end of the warning message, and the tracked-object line shape the Python detector subprocess will speak. |
| `Scenario_Player/player/contracts/cpm_content.py` + golden `.json` fixtures                                    | `1.0.5.1`                       | The bench side of the codec seam as a stdlib dataclass — model only, no encoder, by explicit constraint.                                                                              |
| `IVI_ECU/app/.../model/R4Message.kt` + finalized `R3Snapshot.kt` and `SceneGeometry.kt`                        | `4.0.6.1`, `4.0.6.2`            | The IVI skeleton's data layer: the sealed Kotlin binding the HMI decodes into, including its lenient handling of unknown warning types.                                               |

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 05 · Execution order

---

# Six lanes defining parallel or consecutive works

Horizontal position is dependency depth, not calendar time — how far a piece of work sits from the start of the phase.

![h:430 Phase 0 lane overview: six lanes, their dependency depth, and the gate that runs last](../assets/phase0-exec-overview.svg)

---

# Lane A — start up work

`contracts/` writes documents and schemas, so it depends on no build and no toolchain. It splits into two chains that never touch each other: the V2X message profile feeding the bench and V2X-to-ADA schemas, and the tracked-object schema feeding the warning schema and its additive-version fixture.

![h:430 Lane A: two independent contract chains and the lanes each output feeds](../assets/phase0-exec-lane-a.svg)

---

# Lane B — a five-step chain, with one binding running beside it

Each codec step compiles against the previous one, so the chain admits no shortcut. The V2X-to-ADA binding needs only the toolchain and its schema, so it ran alongside the whole chain rather than after it.

![h:430 Lane B: the V2X codec chain, its lane-A inputs, and the parallel V2X-to-ADA binding](../assets/phase0-exec-lane-b.svg)

---

# Lane C — fan out on the toolchain, then a single file at a time

The two message bindings share only the CMake setup, so they ran side by side. Everything after them is strictly ordered: the warning message embeds the tracked-object record, the additive test needs the warning binding, and the Python validator needs all four.

![h:430 Lane C: the ADA fan-out into two bindings and the sequential chain into the Python validator](../assets/phase0-exec-lane-c.svg)

---

# Lanes D and E — the two that had to wait

Neither lane was slow; both were blocked on frozen artifacts. Lane D needed the reference messages that lane B generates, lane E needed the warning schema and its additive fixture. That is the cost of contract-first, and it is paid once.

![h:420 Lanes D and E: the bench dataclass and the IVI Kotlin binding, with the inputs each waited on](../assets/phase0-exec-lanes-d-e.svg)

---

# The gate and the CI lanes — opposite ends of the ordering

The integrity gate is the one subtask deliberately scheduled last: it fails on a missing target, so it can only run once all ten copy-landing subtasks exist. The CI workflow is the opposite — it landed on day one and guarded every job it could not yet run.

![h:430 The contract integrity gate fed by ten copy-landing subtasks, and the six CI lanes with the subtask that made each live](../assets/phase0-exec-gate-ci.svg)

---

# Lane F — the smoke test and human work

Fully parallel with everything else, because it shares no file with any node folder. Internally it admits no reordering at all, and its last two steps are Nydus UI work the plan tracks but no agent performs.

![h:400 Lane F: author the tool, push the image, deploy, read the logs — with the open items each step closed or left open](../assets/phase0-exec-lane-f.svg)

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 06 · Parallel or sequential

---

# Relationship between lanes

| Relationship                 | What it covers                                            | What forces it                                                                                                                                   |
| ---------------------------- | --------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Parallel — across lanes**  | A, B, C, F and CI all start with no predecessor           | They write disjoint folders: `contracts/`, `V2X_ECU/`, `ADA_ECU/`, `tools/netcheck/`, `.github/`. No shared file, no ordering                    |
| **Parallel — inside lane A** | `1.0.1.1` against `3.0.1.4`                               | The tracked-object schema references neither the V2X message nor the V2X-to-ADA message, so the two chains are independent from the first minute |
| **Parallel — inside lane B** | `2.0.3.1` against the whole `1.0.2.x` chain               | The V2X-to-ADA binding needs the toolchain and a schema, not the codec                                                                           |
| **Parallel — inside lane C** | `2.0.3.2` against `3.0.4.2`                               | Different schemas, same toolchain, different files                                                                                               |
| **Sequential — lane B**      | `1.0.2.1` → `1.0.2.2` → `1.0.2.3` → `1.0.2.4` → `1.0.2.5` | Each step is the next step's compile-time input; the reference messages cannot exist before the codec that writes them                           |
| **Sequential — lane F**      | `6.0.8.1` → `5.0.8.2` → `5.0.8.3` → `6.0.8.4`             | You cannot push an image that has not been written, deploy a tag that was not pushed, or read logs from a node that is not running               |
| **Sequential — last of all** | `1.0.7.1` after all ten copy-landing subtasks             | The gate walks a manifest and fails on a missing target, so partial runs are meaningless                                                         |

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 07 · Handoff

---

# Phase1's input

Phase 1's plan opens with an explicit input list. Every line of it is a Phase 0 deliverable — the handoff is a contract.

- **Frozen contracts on disk.** `contracts/` with the reference messages, `sync-manifest.json` and `check_sync.py`; Phase 1 extended the manifest rather than re-deriving it.
- **A codec seam to build on.** `V2X_ECU/src/codec/` and `src/contracts/` plus the `CMakeLists.txt` baseline — Phase 1's receive pipeline decodes through the same seam, and the bench's encoder helper is built from byte-synced copies of those sources.
- **A bench that already speaks the wire shape.** `Scenario_Player/player/contracts/cpm_content.py` and the golden `.json` fixtures.
- **A platform proven deployable.** `tools/netcheck/` ran on real nodes, so Phase 1 planned deployment tasks instead of re-proving the bridge.
- **Six live CI lanes.** `contracts-gate`, `python-tests`, `v2x-core-build`, `ada-core-build`, `ivi-unit-tests`, `netcheck-image` — Phase 1 added four more to the same workflow file without rewriting it.

---

# Deferred work from Phase 0

- **Two smoke-test items stay open.** The MTU headroom probe was optional and not run; the AAOS listener check was unavailable on this deployment. Neither blocks Phase 0 or Phase 1.
- **Incoming traffic to the IVI ECU has indirect evidence only.** The ADA node's own transmit and capture lines prove the message left the bridge addressed to the IVI ECU — not that anything received it. The gap closes when the real listener lands there.
- **The bench encode path was left undecided on purpose.** `1.0.5.1` was explicitly forbidden from improvising one; the decision belongs to the bench design, and that is where it was made.

> A green pipeline proves the code. The plan file is what proves the process — including the parts that did not go to plan.

---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-fpt-tower.jpg)

# Thank you!

**Phase 0 — Task Execution** · Milestone 1 · FPT Hackathon 2026
