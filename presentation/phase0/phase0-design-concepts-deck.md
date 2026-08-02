---
marp: true
theme: default
paginate: true
title: Phase 0 — Design Concepts
description: Companion deck — the vocabulary Phase 0 is written in — execution lanes vs CI lanes vs tracks, the five-node blueprint and its contract-labelled message flows, the smoke test, and the protocol stack
deck: Phase 0 — Design Concepts · FPT Hackathon 2026
---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-title-city.jpg)

# Phase 0 — Design Concepts

## The words the plan is written in

**Milestone 1 · Cooperative Vehicle Awareness — FPT Hackathon 2026**

Deck A of two — Deck B covers the task groups, the code delivered, and the execution order.

Source: [phase0_tasks.md](../../plans/phase0_tasks.md) · [phase1_tasks.md](../../plans/phase1_tasks.md) · [milestone1.md](../../plans/milestone1.md) · [phase0-contract-freeze-hld.md](../../plans/doc/phase0-contract-freeze-hld.md)

---

# Table of contents

1. **One word, three meanings** — track, execution lane, CI lane, and why they are not interchangeable
2. **Lanes on the wall** — Phase 0's six chains and the ten CI jobs that verify them
3. **The blueprint** — five nodes, four contracts, one bridge
4. **The message path** — R1 → R2 → R3 → R4, hop by hop
5. **The smoke test** — what it established, and where the full story lives
6. **Protocol stack & libraries** — placeholder, source document pending

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 01 · Two things called a lane

---

# One word, three meanings

Phase 0 and Phase 1 both use the word *lane* — for two different things. Neither of them is a *track*.

| Term | What it groups | Defined in | Example |
| ---- | -------------- | ---------- | ------- |
| **Track** | a workstream of whole phases, run by different people at the same time | [milestone1.md §3](../../plans/milestone1.md) | comms track = Phase 1 |
| **Execution lane** | subtasks that must run in dependency order, grouped by the folder they write into | each phase plan's *Execution order & parallelism* | Lane B = `V2X_ECU/` |
| **CI lane** | one job in the GitHub Actions workflow | `.github/workflows/phase0-ci.yml` | `v2x-core-build` |

- **A track spans phases; a lane lives inside one phase.** "The V2X work" is Lane B in Phase 0, Lane V in Phase 1, and the comms track across the milestone — three different objects with three different lifetimes.
- **Only the CI lane is a runnable thing.** The other two are structure: who works on what, and what has to finish before what.

---

# Execution lanes — dependency chains in the plan

An execution lane is a chain of subtasks that must run in order because they share files, fixtures, or a toolchain. Lanes are grouped by target folder, so two lanes never write into the same tree.

| Phase | Lanes | Grouping |
| ----- | ----- | -------- |
| **0** | A `contracts/` · B `V2X_ECU/` · C `ADA_ECU/` · D `Scenario_Player/` · E `IVI_ECU/` · F smoke test | one lane per folder, plus one for the on-platform smoke test |
| **1** | V `V2X_ECU/` · P `Scenario_Player/` · D deploy | the two node folders Phase 1 touches, plus the deferred deploy chain |

- **Every arrow is a real dependency** — a file, a contract artifact, a fixture — never a default assumption. `1.0.1.1 → 1.0.1.2` exists because the schema mirrors the profile document it is written from.
- **Independent start points are named explicitly.** Phase 0 lists `1.0.1.1`, `1.0.2.1`, `3.0.4.1`, `6.0.8.1`, `1.0.7.2`; everything else waits on an input.
- **The integrity gate is deliberately last.** `1.0.7.1` runs after every copy-landing subtask, because what it proves is that the 36 synced contract copies never drifted.

> Lanes are the logical dependency structure, not literal concurrency — at run time everything executes sequentially in one working tree.

---

# CI lanes — the Linux verification path

The dev host has no Docker and no WSL. Every Linux build, every C++ compile, every Gradle run therefore happens on GitHub Actions: CI is not a safety net here, it is *the* verification path.

- **Ten jobs in one file:** `contracts-gate` · `python-tests` · `ada-core-build` · `v2x-core-build` · `v2x-comms-check` · `sp-codec-helper` · `ivi-unit-tests` · `netcheck-image` · `v2x-ecu-image` · `scenario-player-image`.
- **The file is still named `phase0-ci.yml`.** Phase 1 added four jobs to it instead of starting a second workflow — the name is historical, not a scope statement.
- **A job lands before its consumer**, guarded on a file existing (`[ -f contracts/check_sync.py ]`), so the lane stays green while the code it will verify does not yet exist. No empty placeholder jobs.
- **"Lane green" is the acceptance criterion**, written literally into subtask briefs — Phase 1's `9.1.12.3` accepts on *lane green on the pushed branch*, and Phase 0's per-node build commands are only ever executed for real inside these jobs.

---

# One plan lane, one or two CI lanes

Left: what must happen, in what order. Right: what proves it happened.

![h:450 Phase 0 execution lanes mapped onto the CI lanes that verify them](../assets/phase0-lanes.svg)

---

# Tracks — the third axis

A track is a workstream that belongs to people and runs for weeks. Phase 0 froze the contracts precisely so that three of them could start at once.

| Track | Phases | Builds against |
| ----- | ------ | -------------- |
| **Comms** | 1 | real R1 CPMs from the start; mock perception contents until Phase 6 |
| **Perception (ADA)** | 2, then 3 ∥ 4 | the R3 store — detection writes `own_sensor` entries, fusion consumes the store and the live R2 feed |
| **Display** | 5 | mock R4 warnings from the start |

- **Tracks share contracts, never internals.** That is the whole reason they can run in parallel — and why freezing R1–R6 had to come first.
- **Phases 3 and 4 are one track running two phases side by side.** They never call each other; they meet only at the R3 store.
- **The tracks converge at Phase 6**, where every mock is replaced by real data.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 02 · The blueprint

---

# Five nodes, not four

One blueprint is one car — vehicle A, the ego. Four of its nodes are ECU or bench roles; the fifth is the Ethernet Bridge that turns them into a network.

| Node | CarSky node type | Address | Serves |
| ---- | ---------------- | ------- | ------ |
| **Scenario Player (bench)** | Container Node | `10.99.0.10` | R11 |
| **V2X ECU** | Container Node | `10.99.0.11` | R7–R9 |
| **ADA ECU** | Container Node | `10.99.0.12` | R12–R15 |
| **IVI ECU** | Skycraft Node (AAOS guest) | `10.99.0.13` | R16–R17 |
| **Ethernet Bridge** | Ethernet Bridge Node | `10.99.0.1` | R6 |

- **The bridge owns no pin of its own.** Each role node declares exactly one `ethernet` pin wired to it — a star, not a chain, on the single subnet `10.99.0.0/24`.
- **The bench is a node, not a module.** It shares the Room network as sanctioned test equipment; R11 code never lands in `V2X_ECU/`.
- **Static addresses on purpose**, so every UDP target stays deterministic across redeploys — and each one arrives by node config, never as a literal in source.

---

# Every flow labelled by its contract

![h:450 Milestone 1 blueprint: five nodes, three contract-labelled UDP flows, one Ethernet Bridge](../assets/m1-blueprint-5-nodes.svg)

---

# Walking the message path

<div class="chain">
<div class="link"><span>Scenario Player</span><small>bench · R11</small></div>
<div class="arr">→</div>
<div class="link hot"><span>V2X ECU</span><small>R1 in · R2 out</small></div>
<div class="arr">→</div>
<div class="link"><span>ADA ECU</span><small>R3 store · R4 out</small></div>
<div class="arr">→</div>
<div class="link"><span>IVI ECU</span><small>renders ghost C</small></div>
</div>

- **R1 — on the wire.** ETSI CPM (TS 103 324), ASN.1 UPER, one PDU per UDP datagram on port `47100`. The only V2X message type in M1, and the only place a non-JSON encoding appears.
- **R2 — V2X ECU → ADA ECU.** One JSON `v2x_object` message per perceived-object update, port `47200`. `object.distance` is *derived* at this hop from the relative position, never transmitted.
- **R3 — TrackedObject.** Not a hop: the single schema every ego-side object obeys, whether it came from the detector or from the relay. It reaches the wire only as the `object` snapshot embedded in an R4 message.
- **R4 — ADA ECU → IVI ECU.** Versioned warning events carrying the composed scene geometry, port `47300` — the IVI renders the view from this message alone.
- **Every hop is same-subnet UDP.** No routing, no gateway, no broker, no middleware.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 03 · The smoke test

---

# Proving the wire, before the ECUs existed

Phase 0's fourth acceptance box was closed by a connectivity smoke test on blueprint `trial2_minh`. Its method, tooling, AI/human split, and evidence are a deck of their own — this slide states only what it settled.

- **All five pass criteria met, 2026-07-31.** C1 every node Running · C2 zero errors · C3 live per-node logs · C4 traffic captured on the wire · C5 the chain proven by the payload's own accumulated stamps.
- **It closed the last Phase 0 acceptance box** — blueprint topology documented *and* validated — which is what unblocked Phase 1.
- **It settled two facts every later image inherits:** which registry host actually answers, and that node images must be single-platform `linux/arm64`.
- **It left O3 and O4 open** — bridge MTU headroom, and whether the AAOS guest can host a listener — neither of them blocking.
- **Full story, slide by slide:** [phase0-smoke-test-deck.html](../phase0/phase0-smoke-test-deck.html) — method, tested object, testing agent, human in the loop, results. Nothing from it is repeated here.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 04 · Protocol stack & libraries

---

<!-- PLACEHOLDER: link to protocol-stack doc when the user creates it -->

# Protocol stack and supporting libraries

**Placeholder.** The source document for this section does not exist yet — it will be authored separately, and this slide is its anchor in the deck.

- **Topic:** the protocol stack from the wire upward, and the third-party libraries each layer rests on.
- **One-line teaser, from what the repository already contains:** Vanetza supplies the ETSI ITS release-2 ASN.1/UPER codec behind a single codec seam; nlohmann/json binds the C++ side of R2–R4; kotlinx.serialization binds the Kotlin side.
- **Nothing further is stated here on purpose.** The substance belongs to the forthcoming document — this deck holds the place, it does not invent the content.

---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-fpt-tower.jpg)

# Thank you!

**Phase 0 — Design Concepts** · FPT Hackathon 2026
