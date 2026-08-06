# IVI design report — summary for Lead

**Author:** Vũ Xuân Bách · code-freeze document contribution  
**Why:** Lead noted the IVI presentation report was thinner than ADA/V2X design decks. This is the short index; the full design slides mirror [phase1-design-v2x-ecu-deck.md](../presentation/phase1/phase1-design-v2x-ecu-deck.md).

## Deliverable

| Artifact | Role |
| --- | --- |
| [phase5-design-ivi-ecu-deck.md](../presentation/phase5/phase5-design-ivi-ecu-deck.md) | Marp source — module architecture, layout, toolchain, call flow, config, observation |
| [phase5-design-ivi-ecu-deck.html](../presentation/phase5/phase5-design-ivi-ecu-deck.html) | Built HTML (same template as other phase decks) |
| [phase5-ivi-deck.md](../presentation/phase5/phase5-ivi-deck.md) | **Unchanged** demo/evidence deck (problem, God View, learnings) |

## Design authority (already in repo)

- [ivi-ecu-hld.md](../IVI_ECU/doc/ivi-ecu-hld.md)
- [ivi-ecu-design-decisions.md](../IVI_ECU/doc/ivi-ecu-design-decisions.md) (D1–D13)

## Companion learning wikis (this folder)

- [ivi-android-screen-lifecycle.md](ivi-android-screen-lifecycle.md) — Manifest → Activity → Compose modes
- [ivi-r4-observation-pipeline.md](ivi-r4-observation-pipeline.md) — UDP → Flow → ViewModel → UI

## One-line design claim

IVI is a **pure R4 consumer** on AAOS: foreground UDP observer → kotlinx parse → Flow → wake-on-warning Display Area → Canvas God View with **`v2x_relayed`-only** ghost C.
