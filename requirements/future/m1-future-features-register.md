# Future-Features Register — Cooperative Vehicle Awareness (recorded, not M1 scope)

> Moved out of [m1-cooperative-awareness.md §5](../m1-cooperative-awareness.md#5-future-features-register-recorded-not-m1-scope) on 2026-07-08. **This register is the single authoritative source for future (post-M1) features** — it fully absorbs the future-features list of the original goals prompt (`.claude/prompts/project_goals.md`), which is retired as a source and must not be read or cited for future-feature analysis. [[project-researcher]] looks features up here per the [future-features-analysis skill](../../.claude/skills/future-features-analysis/SKILL.md).

Nothing here is M1 scope. Requirement (Rx) and flag (Fx) references resolve to [m1-cooperative-awareness.md](../m1-cooperative-awareness.md) and [deprecated/m1-scope-change-flags.md](../deprecated/m1-scope-change-flags.md).

## Analysed round 2 (2026-07-08)

- **Camera live feed in the central view window (user decision D7).** The R19 central view later also renders live camera frames. Framework note for that milestone: Flutter = `Texture` widget / platform frame streaming (video-plugin support on flutter-elinux is limited — re-verify then); Slint = direct frame-blit into `slint::Image`. Pairs with the live-feed detection feature below.
- **Live object detection over live video at speed (spec amended round 2).** B travels up to 120 km/h (33.3 m/s) and must detect obstructions within stopping + broadcast distance. Derived spec, all (A): **detect a car at ≥ 130–150 m dry / ≥ ~190 m wet** — braking distance 79 m dry (a = 7 m/s²) / 139 m wet (a = 4 m/s²) + driver reaction 1.0–1.5 s ≈ 33–50 m + ~10 m for the R14 300 ms broadcast budget; an automated-braking variant (no human reaction term) relaxes to ≥ 100 m dry / ≥ 160 m wet. **Camera HFOV is an explicit (A) assumption** (60° vs 90° swings pixel subtension ~1.5×) — fixed when the camera spec exists. **The binding constraint is detection range (input resolution), not FPS:** at 640-px inference a 1.8 m car is ~10–12 px at 100 m — below the ~20–30 px nano-detector reliability floor (COCO "small" < 32² px); ⇒ ≥ 1280–1920 px inference at ≥ 10 Hz ⇒ CPU nano fails outright (≈ 224 ms @ 1280 / ≈ 505 ms @ 1920) ⇒ **GPU-class acceleration required** (reference: T4 TensorRT ≈ 1.5 ms @ 640, scaling to ≈ 6–14 ms) — acceleration-stack openness decided at that milestone (flag F11).
- **Multi-process front end / home-app orchestration + wake-on-warning.** A home application defines the outer frame and buttons, watches for user commands, and orchestrates other apps; on an obstruction event (e.g. vehicle C in the M1 scenario), a separate sleeping app wakes and displays the 3-vehicle scene. **Flutter liabilities recorded per the round-2 concession conditions:** per-app Flutter engine instances cost significant RAM, and engine cold start is 100s of ms — material to wake-on-warning and the fast-UI benchmark feature. AGL/Toyota precedent says the multi-app pattern is workable — liability to re-measure at that milestone, not a blocker.
- **2D↔3D view switching.** Switching between 2D and 3D scene views is allowable. Carried by R19's binding view-interface seam (F12). While flutter_gpu remains paused, the true-3D route may be the Texture-widget + native-GL escape hatch — re-evaluate at that milestone.

## Recorded, not yet analysed

Restated in full from the original goals list (no new analysis yet):

- **Themes / custom appearance.** Custom appearance is allowable; themes are provided.
- **Fast-UI + C2X benchmark criteria.** Extremely fast UI behaviors and C2X services; benchmark tests could run a media app in the foreground while providing 2D/3D display or notification messages of vehicle C / obstructions / V2X message arrival. Reduced performance-benchmark criteria may be debated over technical feasibility and trade-offs.
- **Multiple hidden obstructions detection.**
- **Single-message aggregation (no broadcast storm).** If multiple obstructions are detected, V2X messaging provides a single obstruction message.
- **User-opt message reduction.** The user can opt for fewer V2X messages.
- **Criticality filtering.** The user can opt to receive only V2X messages at or above a chosen criticality level.
- **Other hazard-warning types.** V2X messages covering slippery roads, falling rocks, road holes, road condition, presence of children, police, speed limits, no-horn or other road rules, and traffic conditions.
