---
marp: true
theme: default
paginate: true
title: Phase 1 — Task Execution
description: Planning deck — how Phase 1's comms bring-up was decomposed into 44 subtasks across two node lanes, who or what performed each, the order they ran in, what code landed, and exactly how far the acceptance evidence reaches
deck: Phase 1 — Task Execution · FPT Hackathon 2026
---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-title-city.jpg)

# Phase 1 — Task Execution

## Two node lanes, 44 subtasks, and one live Room

**Milestone 1 · Cooperative Vehicle Awareness — FPT Hackathon 2026**

comms bring-up · 12 task groups · 44 subtasks · 39 closed by machine, 5 needing the platform

The work it organizes is Phase 0's contract freeze: [phase0-task-execution-deck.html](../phase0/phase0-task-execution-deck.html) · [phase0-design-concepts-deck.html](../phase0/phase0-design-concepts-deck.html)

Source: [phase1_tasks.md](../../plans/phase1_tasks.md) · [phase1-comms-run.md](../../plans/doc/phase1-comms-run.md) · the two design documents [V2X](../../V2X_ECU/doc/phase1-v2x-ecu-comms-hld.md) and [bench](../../Scenario_Player/doc/phase1-scenario-player-hld.md)

---

# Table of contents

1. **Work organization** — track, execution lane, CI lane, and why Phase 1 has three of them
2. **Work decomposition** — the ID scheme, the shape of the phase, and who performed what
3. **Task groups** — what each of the twelve delivered
4. **Code delivered** — the files Phase 1 put in the repository
5. **Execution order** — lane by lane, one diagram at a time
6. **Parallel or sequential** — and what forced each
7. **What it proved** — the nine acceptance boxes, and how far the evidence reaches
8. **Handoff** — what Phase 2 may assume, and what stays open

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 01 · Work organization

---

# What Phase 1 had to do

The bench sends made-up traffic messages; the V2X ECU receives them, decodes them, and hands the result to the ADA ECU as a plain object message. **Receive only** — the car broadcasts nothing in this phase.

- **A phase is a stage of work with a written input and a written acceptance check.** Phase 1 could start only because Phase 0 had frozen the message formats; it is finished only when its nine acceptance boxes are ticked.
- **Two node folders were built at once:** the V2X ECU (C++) and the bench, called the Scenario Player (Python). They share no source code — only the frozen message formats and a byte-for-byte copy of the encoder sources.
- **Everything the phase produces is either a running program on a node, or a check that proves the program does what was agreed.**

> The bench is test equipment, not a mock: it is a real node on the real network sending real messages. Nothing downstream can tell the difference — which is the point.

---

# Planning terminology

Three different things get called a *lane* or a *track* in this project. They are not interchangeable.

| Term               | What it groups                                                                     | Defined in                                        | Example in Phase 1        |
| ------------------ | ---------------------------------------------------------------------------------- | ------------------------------------------------- | ------------------------- |
| **Track**          | a workstream of whole phases, run by different people at the same time             | [milestone1.md](../../plans/milestone1.md)        | the comms track = Phase 1 |
| **Phase**          | one stage with an input list and acceptance criteria                               | [task-planning-conventions.md](../../.claude/rules/task-planning-conventions.md) | Phase 1 |
| **Execution lane** | subtasks that must run in dependency order, named after the folder they write into | the phase plan's *Execution order & parallelism*  | Lane V = `V2X_ECU/`       |
| **CI lane**        | one job in the GitHub Actions workflow — it builds, it tests, it passes or fails   | [phase0-ci.yml](../../.github/workflows/phase0-ci.yml) | `v2x-comms-check`    |

- **Lane letters are phase-local and they collide.** Phase 0's Lane D was the bench folder; Phase 1's Lane D is the deploy chain. Always say which phase.
- **Lane letters are not an order.** Only a stated dependency sequences one lane against another, and a dependency always names an artifact, never a letter.
- **Only a CI lane actually runs.** A track and an execution lane are planning structure, not programs.

---

# Execution lanes in Phase 1

Named after the folder each writes into, so no two lanes touch the same files.

| Lane                     | Writes into           | Waits on                                | What it lands                                                             |
| ------------------------ | --------------------- | --------------------------------------- | ------------------------------------------------------------------------- |
| **V**                    | `V2X_ECU/`            | Phase 0's frozen formats                | the receive pipeline, the application, the capture scripts, the image     |
| **P**                    | `Scenario_Player/`    | Phase 0's formats + the encoder sources | the bench application, its encoder helper, the image                      |
| **Shared**               | `contracts/`          | nothing — then everything               | one pinned codec version for two builds; the copy-integrity gate          |
| **CI**                   | `.github/workflows/`  | nothing                                 | four new verification jobs, landed before the code they verify            |
| **Comms check**          | `tools/comms_check/`  | the event log format                    | the scripted proof that a sent message is received, decoded and forwarded |
| **D**                    | the deployed Room     | V and P                                 | push the images, deploy, read the logs, swap the scenario, capture traffic |

- **Lanes V and P are logically parallel workstreams.** They were run one after another only because every subtask shares a single working folder on one machine.

---

# CI lanes — the verification path, not a safety net

The development machine is Windows with no Docker and no Linux subsystem. Every C++ compile, every container image, every Linux test therefore runs on GitHub Actions.

- **Ten jobs in one workflow file.** Phase 0 left six; Phase 1 added four: `sp-codec-helper`, `v2x-comms-check`, `v2x-ecu-image`, `scenario-player-image`.
- **For a C++ subtask, "tests pass" *means* "the lane is green".** There is nowhere else to run them, so the definition of done points at a run URL.
- **Three runs carry the whole phase:** [8 lanes green](https://github.com/mnpham2101/FPT-Hackathon2026/actions/runs/30697863324) over the phase's code · [10 lanes green](https://github.com/mnpham2101/FPT-Hackathon2026/actions/runs/30698630956) adding both node images, which also pushed them to the platform registry · [10 lanes green](https://github.com/mnpham2101/FPT-Hackathon2026/actions/runs/30700052056) confirming the deliberately-broken-message tests.
- **The image lanes are slow and were expected to fail.** Compiling the message library for the platform's ARM processor runs under emulation; both images built inside the 6-hour ceiling on the first attempt, roughly 19 and 20 minutes each.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 02 · Work decomposition

---

# The unit of work is a subtask, and its ID says everything

Every task and subtask carries `X.Y.Z.W`.

| Segment | Reads as               | In `9.1.4.4`                                       |
| ------- | ---------------------- | -------------------------------------------------- |
| `X`     | the requirement served | the requirement to decode incoming messages safely |
| `Y`     | the phase              | 1 — comms bring-up                                 |
| `Z`     | the task group         | group 1.4 — the receive pipeline                   |
| `W`     | the subtask            | the fourth: compose the four stages, one commit    |

**The group number is not the requirement number.** Group 1.5 holds `8.1.5.1`, `6.1.5.2`, `6.1.5.3` and `5.1.5.4` — four different requirements in one group, because they jointly deliver one thing: a V2X ECU that runs, captures its own traffic, and ships as an image.

---

# What Phase 1 looked like as a work item

- **12 task groups, 44 subtasks, six lanes.** Lane V 19 · lane P 11 · deploy 5 · comms check 3 · shared pin 2 · CI 2 · ADA sink 1 · plan upkeep 1.
- **One subtask, one objective, one atomic commit**, a build that passes and unit tests that pass. A subtask with no `**Status:**` line in the plan file has not started.
- **Every subtask brief is self-contained** — file paths, the message fields it touches, its acceptance check — so the agent implementing it does not need to read the rest of the codebase.
- **Nothing was hardcoded.** Ports, peers, retry counts, the duplicate-message window, the capture filter and the scenario itself are all configuration the platform injects, which is why the second traffic scenario is a second file rather than a second build.

> 39 of the 44 needed no platform at all. That was deliberate: the deployment was known to be the scarce resource, so everything provable off-platform was made provable off-platform first.

---

# Who — or what — performed the work

| Performer                     | Count | What it did                                                                                        |
| ----------------------------- | ----- | ---------------------------------------------------------------------------------------------------|
| **AI implementation agents**  | 39    | Wrote every line of application code, its tests and its CI jobs; each made its own single commit    |
| **The cloud platform (CarSky)** | 1   | Planned as an agent-run step: push all three images to the platform registry                       |
| **A person, in the platform UI** | 4  | Configure the nodes, deploy, read the node logs, swap the scenario, pull the capture into Wireshark |

- **The one platform step was executed by CI instead.** The registry key turned out to be present in the repository, so the image lanes pushed the images themselves — the platform agent was never needed, and never was available in any session.
- **Human effort is unavoidable for the last four**, and not for want of trying: node configuration — image, start command, capabilities, environment variables — is editable **only** in the platform's web UI. Its REST API cannot set it. Reading a node's log window and opening a capture in Wireshark are the same kind of work.
- **All 44 are tracked identically.** Only the performer differs; the evidence requirement does not.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 03 · Task groups

---

# Groups 1.1 – 1.6 — the shared pin, the V2X ECU, the bench

| Task group                                       | What it delivers, and which lane it serves                                                                                                                            |
| ------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **1.1** — shared codec pin + copy gate           | One pinned version of the message library, used by both C++ builds, and the integrity gate extended from 36 to 47 tracked copies. **Shared lane**, and it finishes last. |
| **1.2** — V2X ECU foundation                     | The only piece that reads the environment, the only piece that owns a socket, the event log, and the sender to the ADA node. **Lane V**, and nothing above it knows about transport. |
| **1.3** — radio interface + stand-in modem       | The four-call radio interface a real modem would implement, and a stand-in that acknowledges each call, injects faults on demand and recovers from them. **Lane V.**   |
| **1.4** — the receive pipeline                   | Decode, reject anything outside the agreed profile, drop repeats, convert to real-world units, forward. Four independently tested stages, then composed. **Lane V.**   |
| **1.5** — application, capture, image            | The program itself, the traffic capture running inside its container, the host-side extraction tool, and the deployable image. **Lane V.**                             |
| **1.6** — the bench application                  | Config loading, the two scenario files, the motion model, the encoder client, the sender, the rate loop and the entrypoint. **Lane P.**                                |

---

# Groups 1.7 – 1.12 — the encoder, the lanes, the checks, the deploy

| Task group                              | What it delivers, and which lane it serves                                                                                                                             |
| --------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **1.7** — the bench's encoder path      | A small C++ helper the Python bench talks to, built from byte-identical copies of the V2X ECU's encoder sources, proven against the six reference messages, plus the bench image. **Lane P.** |
| **1.8** — two new CI jobs               | The helper build lane and the two node-image lanes. **Landed before their consumers**, skipping themselves until the code they verify exists.                           |
| **1.9** — the ADA-side listener         | The stand-in listener on the ADA node prints whole message bodies instead of the first 96 characters — one line changed, so the deploy could actually show the payload. |
| **1.10** — deploy and live verification | Push, configure, deploy, then read the evidence off the running nodes. **Lane D** — one step on CI, four performed by a person.                                         |
| **1.11** — plan upkeep                  | Reconciled the Phase 0 acceptance record with what Phase 0 actually closed. Docs only.                                                                                  |
| **1.12** — the bench-to-V2X check       | A sender, a log-chain assertion script, and the CI job that runs the real application between them. It is the only check that exercises the whole program without a deployed Room. |

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 04 · Code delivered

---

# The V2X ECU — a receiving program in eleven modules

| File group                                                                              | Delivered by                          | Purpose                                                                                                             |
| --------------------------------------------------------------------------------------- | ------------------------------------- | ------------------------------------------------------------------------------------------------------------------- |
| `src/config/` · `src/net/` · `src/log/` · `src/forward/`                                | `8.1.2.1` `7.1.2.2` `18.1.2.3` `2.1.2.4` | The foundation: the one environment reader, the one socket owner, the JSON event stream, and the sender to the ADA node |
| `src/adapter/` · `src/stub/`                                                            | `7.1.3.1` – `7.1.3.4`                 | The frozen four-call radio interface and the stand-in modem behind it, with fault injection and defined recoveries  |
| `src/pipeline/` — validator, deduper, builder, composition                              | `9.1.4.1` – `9.1.4.4`                 | Decode → check against the profile → drop repeats → convert to metres and m/s → hand on. Each stage tested alone     |
| `src/main.cpp` · `CMakeLists.txt`                                                       | `8.1.5.1`                             | The application: wires every part together, drives start-up, then serves traffic. No logic of its own                |
| `tests/` — 11 suites · `tests/fixtures/malformed/` — 10 cases                            | across all groups, `9.1.4.5`          | Including ten deliberately broken messages: six that must be rejected, four that must be tolerated                         |
| `capture.sh` · `tools/extract_pcap.sh` · `Dockerfile` · `entrypoint.sh`                 | `6.1.5.2` `6.1.5.3` `5.1.5.4`         | Capture running inside the container, extraction on the host, and the deployable image                              |
| `tools/check_transport_imports.py` · `doc/telux-parity-and-port-plan.md`                | `7.1.3.5` `7.1.3.6`                   | The gate that keeps sockets below the radio interface, and what changes when a real modem replaces the stand-in     |

---

# The bench, the shared pin, and the test equipment

| File group                                                                    | Delivered by                  | Purpose                                                                                                       |
| ----------------------------------------------------------------------------- | ----------------------------- | --------------------------------------------------------------------------------------------------------------|
| `player/config.py` · `scenarios/default.yaml` · `scenarios/c-out-of-range.yaml` | `11.1.6.1` `11.1.6.2`        | Configuration loading, and the two traffic situations as data: C approaching, and C parked out of range       |
| `player/scenario.py` · `player/generator.py`                                  | `11.1.6.3` `11.1.6.7`         | Where C is at any instant, and the loop that turns that into one message every 100 ms                         |
| `player/encoder_client.py` · `player/sender.py` · `main.py`                   | `11.1.6.5` `11.1.6.6` `11.1.6.8` | Talking to the encoder helper, putting bytes on the wire, and the entrypoint the platform starts           |
| `codec_helper/` — `cpm_encode` · `Dockerfile`                                 | `11.1.7.1` `5.1.7.3`          | A C++ encoder built from byte-identical copies of the V2X ECU's sources, and the bench image that carries it   |
| `tests/` — 10 suites, 116 local tests + the encoder-golden test               | `11.1.6.4` `11.1.7.2`         | Including the proof that the two scenario files produce different message streams, and that the helper's bytes match the reference messages exactly |
| `contracts/vanetza-pin.cmake` · `contracts/sync-manifest.json`                | `11.1.1.1` `11.1.1.2`         | One pinned library version for both builds; the gate now covers 47 byte-identical copies                      |
| `tools/comms_check/` · `tools/netcheck/netcheck.py`                           | `6.1.12.1` `9.1.12.2` `2.1.9.1` | Test equipment: the message sender, the log-chain assertion, and the listener that prints whole bodies       |

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 05 · Execution order

---

# Eight independent starting points

![h:470 Phase 1 lane overview: eight starting points, two node lanes running side by side, and the deploy chain that can only begin once both images exist](../assets/phase1-exec-overview.svg)

- **Horizontal position is dependency depth** — how far a piece of work sits from the start of the phase — not calendar time.

---

# Lane V, part 1 — the foundation and the radio interface

![h:495 Lane V foundation and radio interface: the parallel starts on the left and the four-step radio chain below](../assets/phase1-exec-lane-v-foundation.svg)

---

# Lane V, part 2 — the pipeline, the program, the image

![h:495 Lane V pipeline and assembly: three parallel stages feeding the composition, then the application and the container image](../assets/phase1-exec-lane-v-pipeline.svg)

---

# Lane P — the bench

![h:500 Lane P: the bench application chain, the encoder helper path beneath it, and the image both feed](../assets/phase1-exec-lane-p.svg)

---

# What landed first, what landed last, and the check in between

![h:495 CI lanes landed first, the shared pin and its copy gate, and the bench-to-V2X comms check with its CI lane](../assets/phase1-exec-ci-and-checks.svg)

---

# Lane D — the five steps that need the live platform

![h:430 Lane D: push, deploy, then read the object messages, swap the scenario and pull the capture — colour-coded by how far the evidence reaches](../assets/phase1-exec-lane-d-deploy.svg)

- **The Room was deployed and messages did arrive.** What is still missing is narrower than "the deploy": per-node status badges, a scripted check over a saved log, the second scenario, and a capture file.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 06 · Parallel or sequential

---

# What ran beside what, and what forced the order

| Relationship                        | What it covers                                                        | What forces it                                                                                                              |
| ----------------------------------- | ---------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------|
| **Parallel — the two node lanes**   | all of `V2X_ECU/` against all of `Scenario_Player/`                   | Disjoint folders. They meet only at frozen message formats and at byte-identical copies of the encoder sources              |
| **Parallel — inside the foundation**| the config reader, the socket, the event log                          | Different files, no shared type; only the forwarder needs the socket                                                        |
| **Parallel — inside the pipeline**  | validator, deduper, builder                                           | None of the three calls another; the composition is what needs all of them                                                  |
| **Sequential — the radio chain**    | interface → stand-in modem → faults → live receive thread             | Each step compiles against the one before it                                                                                |
| **Sequential — CI before code**     | the three verification jobs before the subtasks they verify           | A subtask whose acceptance check is "the lane is green" needs the lane to exist on the day it lands                          |
| **Sequential — last of all**        | the copy-integrity gate, after every new copy exists                  | The gate fails on a missing target, so running it early would prove nothing                                                 |
| **Sequential — the deploy chain**   | push → configure and deploy → read the logs → swap the scenario       | You cannot deploy an image that was never pushed, or read logs from a node that is not running                              |

> At run time everything was executed one subtask at a time: all the agents shared a single working folder. The parallel marks above are the dependency structure — what *could* run at once given more workers.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 07 · What it proved

---

# The nine acceptance boxes

| Acceptance box                                                     | State      | What the evidence actually is                                              |
| ------------------------------------------------------------------ | ---------- | ---------------------------------------------------------------------------|
| No socket code above the radio interface                           | **closed** | The gate runs on every push; the porting notes are committed               |
| Start-up calls acknowledged; every fault recovers                  | **closed** | Unit tests and the end-to-end CI job — **not** a deployed node             |
| Reference messages decode; broken ones rejected                    | **closed** | Through the real decoder: 6 rejected, 4 correctly tolerated, no crashes    |
| A bench message is received, decoded and forwarded                 | **closed** | The `v2x-comms-check` job, which fails if any link is removed              |
| Both images build for the platform, and the Room deploys           | *partly*   | Built, pushed, pulled and run — per-node status badges unconfirmed         |
| Traffic captured on the bridge network                             | *partly*   | Capture runs and shows both directions; no file pulled off yet             |
| Different scenarios give different message streams                 | *partly*   | Proven in the model and in the bytes; the live swap is not done            |
| Object messages at the ADA ECU are not constants                   | *partly*   | Seen live and checked; the scripted check on a saved log is pending        |
| **Demo:** the capture opened in Wireshark                          | **open**   | Needs a log from a Room that ran long enough to rotate a file              |

---

# How far the evidence reaches

A green pipeline and a running node prove different things. The plan says which, per box, and so does this deck.

- **Closed by machine, on every push:** the import gate, the fault recoveries, the broken-message tests, and the whole received → decoded → forwarded chain. These re-prove themselves on every commit.
- **Seen once, on a live Room:** object messages arriving at the ADA node with `distance` falling exactly **0.25 m per message** at 10 messages a second — precisely the 2.5 m/s closing speed in the scenario file. The derived distance matches `hypot(50.25, 1.2)` to every printed digit, so it is genuinely computed from the decoded position rather than copied through.
- **Measured as a side effect:** the V2X ECU's whole decode-to-forward path took **142–151 µs**, consistently, across every message pair sampled at the capture point.
- **Not proven at all yet:** anything needing a saved log export — the scripted log check, the second scenario, and the capture file.

> The bench-to-V2X check is not vacuous: deleting the forward event fails it at the forward link, and stripping the decoded payload fails it at the decode link. It was tested for its ability to fail.

---

# Three places the sources disagree

Flagged, not resolved — reconciling them is a planning decision to take with the owner, not something a deck should quietly fix.

- **The push step is recorded three ways in one file.** `5.1.10.1` is an unticked checkbox, is listed as **done** in the remaining-work table, and its own status line says "partly closed … Closed". The substance is not in doubt — the images are in the registry and every node pulled them — only the bookkeeping is.
- **The remaining-work table still lists the broken-message tests as awaiting confirmation**, while the same file's open-items table records the confirming run and marks it closed. The later record is the run that actually exists.
- **The phase's own summary line says 39 of 44 closed**, which was written before the deploy evidence landed. The deploy happened; the run record is the more recent source.

> The plan file also carries a standing hazard worth keeping: all three image tags are mutable, so whichever branch pushes last wins the tag. Identify a deployed image at deploy time, never from an old run log.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 08 · Handoff

---

# What Phase 2 may assume

Phase 2 builds the ADA ECU's track store and admission logic on mock input. Everything below is a Phase 1 deliverable it does not have to re-prove.

- **Real object messages arrive at the ADA node.** Their shape, units and field values were checked against the running bench, so Phase 2 can build against a format that has already been observed rather than only agreed.
- **The platform is a known quantity.** Images build for its processor, push to its registry and run; the node configuration gotchas — the start command is space-separated, not a JSON array — are written down in the run record.
- **The event stream exists.** Every stage of the receive path emits one JSON line with running counters; the later phases' reconstruction of a whole run starts from this format.
- **Ten CI jobs verify the tree on every push**, four of them added by this phase, and the copy-integrity gate now covers 47 files.
- **Test equipment is reusable.** The message sender, the log-chain assertion and the listener that prints whole bodies are all node-independent.

---

# What stays open

- **Five subtasks need the live platform.** One deploy session with a long enough log download closes four of them: the status badges, the scripted log check, the scenario swap and the capture file.
- **One requirement was moved out of the milestone.** The car does not transmit; the radio interface still declares the send call, and nothing calls it.
- **Two decisions are still the user's.** Whether the phase's retry and duplicate-window defaults are ratified as chosen, and whether the "team app launches on the display node" clause belongs to this phase at all — no design document covers it, and the deployment keeps the supplied artifact.
- **Two documentation corrections are owned elsewhere.** The node guides still print the old registry hostname, and the network's maximum-packet-size probe from Phase 0 was never run — harmless here, since a message is 58 bytes.

> None of the open items blocks Phase 2. All of them are written down where the next person will look for them.

---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-fpt-tower.jpg)

# Thank you!

**Phase 1 — Task Execution** · Milestone 1 · FPT Hackathon 2026
