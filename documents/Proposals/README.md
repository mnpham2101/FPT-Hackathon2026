# Introduction

This page states the project objective, the planned delivery, and the argument for the product's strength.

**Abridged version.** A reader preferring a presentation can open [Product & Delivery](../../presentation/m1-business-delivery/m1-business-delivery-deck.md) ([HTML](../../presentation/m1-business-delivery/m1-business-delivery-deck.html)) — the business view of this page: what the product does, the guaranteed-delivery checklist, and the delivery options. The engineering proposal remains [Cooperative Vehicle Awareness](../../presentation/m1-proposal-deck.md) ([HTML](../../presentation/m1-proposal-deck.html)). Where any presentation and this document differ, this document governs.

## Objective

Make a vehicle aware of a hazard it cannot see by relaying another vehicle's perception over V2X. Milestone 1 demonstrates one scenario on the CarSky cloud platform: vehicle A follows B follows C in a collinear convoy, B blocks A's view of C, and B's perception of C reaches A over the relay — A renders ghost C from the relayed data alone, with zero direct detections of C. Requirements authority: [m1-cooperative-awareness.md](../Requirements/m1-cooperative-awareness.md).

## Planned Delivery

Every item below is delivered with Milestone 1 — checked means it is complete and included in the package; ❌ marks an item implemented and shipped but not yet tested.

| Delivered | Item | What you receive |
|:---:|---|---|
| ✅ | **Application code** | Three ECU applications — V2X-ECU, ADA-ECU and IVI-ECU — full source on [GitHub](https://github.com/mnpham2101/FPT-Hackathon2026) |
| ✅ | **Utility tools** | Automation for APK installation and log collection — `tools/apk-uploader/` and `tools/logs-collector/` |
| ❌ | **Data-provenance tools** | Shipped inside the ADA-ECU application to verify that the hazard reaches the driver through the V2X relay alone (implemented, not yet tested) |
| ✅ | **Isolated test images** | Each ECU has mock images of its neighbouring ECUs, so every application can be exercised and accepted on its own |
| ✅ | **CI tools** | Six CI pipelines, one per development phase, under `.github/workflows/` — every change is built and tested automatically |
| ✅ | **Demo video** | [A video demo](../../video-evidence/system-test.mp4) of the full system test is included |
| ✅ | **Wiki** | The full, complete explanation of the project design, and the research articles providing the foundation for the project |
| ✅ | **Deployment blueprints** | CarSky blueprints ready to be deployed and tested |
| ✅ | **Documentation** | Wiki pages and presentations provide insight into the product. The wiki is a work in progress, with articles being added |

### Screen output

- The IVI warning screen: the NLOS God View with ego A, occluder B, and ghost C drawn dashed with its risk state — rendered from warning messages only.
- Captured as screenshots and recordings on the platform's Screen widget.

![The IVI warning screen on the platform's Screen widget: the NLOS God View with ego A, occluder B and ghost C dashed at RISK: MEDIUM, and the IVI log below showing the risk state stepping medium → high → low](../Delivery/Acceptance/Evidence1_WarningScreen_IVILog_RiskLabel.png)

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
- **Target variety of customer needs** —
  - **Automotive OEMs** bringing cooperative awareness to connected vehicle lines: the three applications map onto the ECUs a production vehicle already carries — connectivity, driver assistance, and cabin display.
  - **Tier-1 suppliers** building one of those ECUs: the frozen contracts let a supplier adopt a single application and integrate it against their own stack.
  - **Fleet and logistics operators** running convoys, where a following vehicle routinely blocks the driver's view and an NLOS warning directly reduces rear-end risk.
  - **Validation and research teams** studying V2X: the fully virtual deployment, the scenario bench, and the isolated test images make a complete cooperative-awareness testbed with no hardware investment.
  - The system can be developed and delivered as a **whole package of the three ECU applications**, integrated and system-tested end to end.
  - Or each application can be delivered as a **single, standalone product per ECU** — each ships with mock images of its neighbours, so a single application is testable and acceptable on its own.

## Financial Analysis & Business Case

For the complete commercial valuation, financial modeling, and investment returns:
- 📊 **[V2X Business Case, Cost-Benefit Analysis & Financial ROI Model](v2x-business-case-roi-model.md)**: Includes the 5-year Cost-Benefit Analysis (CBA) proof diagram ($7.38x BCR), the Tri-Stream Monetization Revenue Model, and the 3-Year 4,000% ROI & 14-Month Break-Even Timeline.

## Product Limitations

- **Warning threshold** — based on the distance required to stop a vehicle traveling at over 75 km/h. Variable speed and stopping distance are not calculated.
- **Occluded cars in different lanes or around corners** are not considered. Those are future implementations.
- **No true 3D warning screen** is implemented. This is a future feature.
- **No machine learning on live video** — the ADA-ECU performs machine learning on stored video. The video is looped to create repeating cycles of events for ease of demo. A live camera feed is a future feature.