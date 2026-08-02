---
marp: true
theme: default
paginate: true
title: Phase 0 — Design Concepts
description: Design deck — the Phase 0 architecture — the five-node blueprint and its contract-labelled message flows, the message path step by step, the smoke test, and the protocol stack
deck: Phase 0 — Design Concepts · FPT Hackathon 2026
---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-title-city.jpg)

# Phase 0 — Design Concepts

## The architecture the contracts describe

**Milestone 1 · Cooperative Vehicle Awareness — FPT Hackathon 2026**

Deck A of two — the design. How that design was planned and built is Deck B: [phase0-task-execution-deck.html](phase0-task-execution-deck.html).

Source: [phase0_tasks.md](../../plans/phase0_tasks.md) · [phase1_tasks.md](../../plans/phase1_tasks.md) · [milestone1.md](../../plans/milestone1.md) · [phase0-contract-freeze-hld.md](../../plans/doc/phase0-contract-freeze-hld.md)

---

# Table of contents

1. **The blueprint** — five nodes, four contracts, one bridge
2. **The message path** — R1 → R2 → R3 → R4, step by step
3. **Contract terminology** — golden vectors, corpus, UPER, octet
4. **The smoke test** — what it established, and where the full story lives
5. **Protocol stack & libraries** — placeholder, source document pending

*Tracks, lanes and task groups — the planning vocabulary — moved to Deck B.*

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 01 · The blueprint

---

# Five nodes, not four

One blueprint is one car — vehicle A, the ego. Four of its nodes are ECU or bench roles; the fifth is the Ethernet Bridge that turns them into a network.

| Node                        | CarSky node type           | Address      | Serves  |
| --------------------------- | -------------------------- | ------------ | ------- |
| **Scenario Player (bench)** | Container Node             | `10.99.0.10` | R11     |
| **V2X ECU**                 | Container Node             | `10.99.0.11` | R7–R9   |
| **ADA ECU**                 | Container Node             | `10.99.0.12` | R12–R15 |
| **IVI ECU**                 | Skycraft Node (AAOS guest) | `10.99.0.13` | R16–R17 |
| **Ethernet Bridge**         | Ethernet Bridge Node       | `10.99.0.1`  | R6      |

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
- **R2 — V2X ECU → ADA ECU.** One JSON `v2x_object` message per perceived-object update, port `47200`. `object.distance` is *derived* here from the relative position, never transmitted.
- **R3 — TrackedObject.** Not a message between nodes: the single schema every ego-side object obeys, whether it came from the detector or from the relay. It reaches the wire only as the `object` snapshot embedded in an R4 message.
- **R4 — ADA ECU → IVI ECU.** Versioned warning events carrying the composed scene geometry, port `47300` — the IVI renders the view from this message alone.
- **Every link between nodes is same-subnet UDP.** No routing, no gateway, no broker, no middleware.

---

# Contract terminology

The words the contract artifacts carry. They appear throughout the plan, the CI lanes and the task deck.

| Term            | What it means here                                                                                                        |
| --------------- | ------------------------------------------------------------------------------------------------------------------------- |
| **UPER**        | *Unaligned Packed Encoding Rules* — the ASN.1 rules that pack one R1 CPM into 58 bytes. The only non-JSON encoding in M1. |
| **Octet**       | One byte. ASN.1 wording, kept because the profile document uses it.                                                       |
| **Test vector** | One case: a message content paired with the exact bytes it must encode to — `nominal.json` + `nominal.uper`.             |
| **Golden**      | Those bytes are committed and frozen. A test encodes the `.json` and compares byte for byte against the stored `.uper`, instead of recomputing what it should be. |
| **Corpus**      | The whole set of vectors — six here, chosen to cover the edges rather than one happy path.                               |

- **The six cases:** `nominal` · `mdt-max` / `mdt-min` (±2047 ms) · `conf-unavailable` (the 101 sentinel) · `gate-boundary` (an object at exactly 30 m) · `coord-large` (near the large-coordinate bound).
- **Why they exist:** the same message is encoded and decoded by different code — C++ in the V2X ECU, a separate helper on the bench. The corpus is the one reference both are measured against.
- **CI generated them, not a person** — and the same run proved regeneration is deterministic: generate twice, `diff` empty.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 02 · The smoke test

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

# 03 · Protocol stack & libraries

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
