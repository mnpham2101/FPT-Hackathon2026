# IVI_ECU — IVI ECU node (R4, R16–R17)

The AAOS head unit: renders the R16 HMI layout and the R17 warning view — the God view of ego (A), occluder (B), and ghost C — from R4 messages alone. Ghost C is drawn from `v2x_relayed` data only, which is what the R19 definition of done turns on.

- **Requirements:** R4, R16–R17 — [m1-cooperative-awareness.md](../requirements/m1-cooperative-awareness.md) §2; HMI layout reference [ivi-ecu.svg](../requirements/ivi-ecu.svg).
- **Node/deploy guide:** [node-ivi-ecu.md](../requirements/car-sky-guide/node-ivi-ecu.md) — VM artifact prep, blueprint config, pins, post-deploy `adb install`, verification.
- **Layout & build rules:** [node-code-layout.md](../.claude/rules/node-code-layout.md) — Kotlin / Jetpack Compose, APK via `./gradlew assembleDebug` from this folder.
- **Plan:** Phase 5 of [milestone1.md](../plans/milestone1.md), decomposed in [phase5_tasks.md](../plans/phase5_tasks.md); wired to real ADA output in Phase 6.

Unlike the container nodes, this is a **Skycraft Node (AAOS guest)** — the VM artifact ships in the starter pack, and only the team APK is built here and installed post-deploy over ADB; there is no image to push to the registry.

## Current structure

Gradle project, application id `com.hackathon.v2x.ivi` (minSdk 29 / targetSdk 33, the Skycraft AAOS baseline):

| Path | Holds |
|---|---|
| [app/src/main/java/com/hackathon/v2x/ivi/model/](app/src/main/java/com/hackathon/v2x/ivi/model/) | R4/R3-derived data types (`R3Snapshot`, `SceneGeometry`) |
| [app/src/main/java/com/hackathon/v2x/ivi/ui/](app/src/main/java/com/hackathon/v2x/ivi/ui/) | UI logic — `MainViewModel`, `DisplayMode` |
| [app/src/main/java/com/hackathon/v2x/ivi/ui/screen/](app/src/main/java/com/hackathon/v2x/ivi/ui/screen/) | R16 layout composables |
| [app/src/main/java/com/hackathon/v2x/ivi/ui/view/](app/src/main/java/com/hackathon/v2x/ivi/ui/view/) | R17 warning view behind the view seam (`IviWarningViewSeam`, `CanvasWarningView`, `SceneCoordinateMapper`) |

The view seam is what keeps optional 3D (SceneView/Filament) additive — new renderers implement the seam rather than editing the 2D Canvas view. Tunables are `buildConfigField`s (e.g. `WARNING_TIMEOUT_MS`), never literals in source ([CLAUDE.md](../CLAUDE.md) governing principle 5).
