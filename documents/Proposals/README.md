# Introduction

This page states the project objective, the planned delivery, and the argument for the product's strength.

**Abridged version.** A reader preferring a presentation can open [Cooperative Vehicle Awareness](../../presentation/m1-proposal-deck.md) ([HTML](../../presentation/m1-proposal-deck.html)). It presents this plan; where the two differ, this document governs.

## Objective

Make a vehicle aware of a hazard it cannot see by relaying another vehicle's perception over V2X. Milestone 1 demonstrates one scenario on the CarSky cloud platform: vehicle A follows B follows C in a collinear convoy, B blocks A's view of C, and B's perception of C reaches A over the relay — A renders ghost C from the relayed data alone, with zero direct detections of C. Requirements authority: [m1-cooperative-awareness.md](../Requirements/m1-cooperative-awareness.md).

## Planned Delivery

A complete five-node blueprint deployed and running on CarSky — three ECUs, the Scenario Player bench, and the Ethernet bridge — with the evidence reported in [system-delivery.md](../Delivery/Acceptance/system-delivery.md) and reproducible via the [Test-Guides](../Delivery/Test-Guides/README.md).

### Screen output

- The IVI warning screen: the NLOS God View with ego A, occluder B, and ghost C drawn dashed with its risk state — rendered from warning messages only.
- Captured as screenshots and recordings on the platform's Screen widget.

### Internal logs and Wireshark capture

- Per-node internal logs: `[TX]` at the bench, decode/forward counters at the V2X-ECU, fusion and risk counters at the ADA-ECU, `[RX]` warning lines at the IVI app.
- Wireshark captures exported from the V2X-ECU and ADA-ECU logs, byte-exact against the expected call flow.

## Product Strength

- **Fully virtual** — the whole system builds, deploys and demonstrates on the cloud platform, with no dependency on vehicle hardware.
- **Modular design** — SOLID principles and frozen contracts between nodes; future features extend behind existing seams without rework.
- **Scenario-grounded risk model** — the admission and risk criteria derive from a study of the specific scenario: vehicle speed, expected stopping distance, and expected driver response; speed-scaled criteria are a registered future development.
- **Documentation discipline** —
  - ASPICE-oriented traceability: features tracked to requirements to task IDs.
  - A continuously updated Knowledge Base and wiki that give future members a working understanding of the project and connect human knowledge to AI-assisted development.
