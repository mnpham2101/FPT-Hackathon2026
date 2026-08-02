# IVI ECU — everything else needed to implement Phase 5

Research note collecting the facts an implementer needs that are not in the requirements, the blueprint guide, or the two companion notes ([mini-blueprint](phase5-mini-blueprint.md), [simulator](phase5-r4-simulator.md), [R4 parsing](phase5-r4-parsing.md)).

## 1. What already exists in this folder

Inventory as of 2026-08-02, read from the tree — not from the plan's checkboxes.

| Present | State |
|---|---|
| Gradle project, `:app` only | AGP 8.13, Kotlin 2.2.20, Compose BOM 2024.09.03, `minSdk 29 / targetSdk 33 / compileSdk 34` |
| Plugins applied | kotlin-android, serialization, compose, KSP, **Hilt** |
| `model/` | `R4Message`, `R3Snapshot`, `SceneGeometry`, `VehiclePosition`, `R4Json` — frozen bindings, tested |
| `ui/` | `DisplayMode`, `MainViewModel` (mode switcher + warning-lock rule) |
| `ui/screen/MainScreen.kt` | R16 layout: side bars, Display Area, bottom status, `AnimatedContent` switcher, previews. Warning View is a **placeholder** |
| `ui/view/` | `IviWarningViewSeam`, `CanvasWarningView` (full God View: ego/B/ghost C, glow, badge, source guard), `SceneCoordinateMapper` (pure Kotlin), `WarningBannerOverlay` |
| `contracts/` + test resources | R3/R4 schemas and samples, byte-synced |
| Tests | `R4RoundTripTest`, `R4AdditiveVersionTest` — contract level only |
| CI | `ivi-unit-tests` job runs `./gradlew :app:testDebugUnitTest` |

| Absent | Consequence |
|---|---|
| **Any `<activity>` in the manifest, and any `MainActivity`** | The APK has no launcher entry — it installs but cannot be started. Nothing renders on the node today |
| UDP listener, deserializer wrapper, repository, warning view-model | No message can reach the UI |
| Hilt `@HiltAndroidApp` application class and modules | Hilt is on the classpath and configured but not used anywhere |
| Simulator / mock sender | No way to produce R4 traffic |
| `SceneCoordinateMapper` / view-model tests | The pure-math layer built to be testable is untested |
| Deployment doc for the APK | The ADB route is unproven (§5) |

**The gap is the middle of the app.** The contract layer and the drawing layer are both built and are the two hardest pieces; what is missing is the plumbing between them, plus the Activity that hosts it.

## 2. Android constraints that shape the design

- **A foreground service needs a notification channel and `startForeground()` within seconds of starting** (API 26+/29+). On `targetSdk 33` the `foregroundServiceType` attribute is not yet mandatory (it becomes so at 34), but declaring one is harmless and future-proof.
- **`POST_NOTIFICATIONS` is a runtime permission from API 33.** Without it granted the service still runs; only its notification is suppressed. Do not let a denied permission be treated as a failure to start.
- **A simpler alternative exists and should be a conscious choice, not a default:** the head unit runs this app in the foreground for the whole demo, so a receive loop scoped to the Activity/Application lifecycle would satisfy M1 without any foreground-service machinery. The service is still the right pick — it is what makes the optional "separate app woken by an ADA message" path (R16) reachable, and it survives the Display Area switching away — but the trade is worth recording.
- **Socket lifecycle:** bind `0.0.0.0:<port>` (not the node address — that is assigned by the bridge and must not be hardcoded), close on service destroy, and rebind with back-off on error. A bind failure on an already-taken port must be visible in the log, not silent.
- **Back-pressure:** emit on a `MutableSharedFlow` with a bounded `extraBufferCapacity` and `DROP_OLDEST`. A suspending emit inside the receive loop would let a slow collector stall the socket; for warnings, the newest message is the one that matters.
- **`DatagramSocket` needs no permission beyond `INTERNET`**, which is already declared.

## 3. Configuration — the thing `BuildConfig` cannot do

`buildConfigField` values are **baked at compile time**. `WARNING_TIMEOUT_MS` is already one, and the port will be another. That satisfies the no-literals rule ([CLAUDE.md](../../../CLAUDE.md) principle 5) but not the *deployment* need: unlike a container node, whose env comes from the blueprint at deploy time, an installed APK cannot be reconfigured without a rebuild.

For M1 this is acceptable — the port is `47300`, frozen in the blueprint. Keep it honest by making `BuildConfig` the **default** and allowing an override at launch, e.g. an intent extra read by the Activity:

```
adb shell am start -n com.hackathon.v2x.ivi/.MainActivity --ei r4_port 47301
```

That keeps a wrong-port day from costing a rebuild-reinstall cycle mid-demo.

## 4. Decisions already in force

Not re-litigable during implementation:

- **The warning banner is built but must not be mounted.** Standing decision (user feedback 2026-07-26, recorded in [WarningBannerOverlay.kt](../../app/src/main/java/com/hackathon/v2x/ivi/ui/view/WarningBannerOverlay.kt)): the God-View canvas is the deliverable and must render unobstructed. The integration work does not add the banner to the Display Area unless that decision is explicitly revisited.
- **3D is optional**, behind the same view seam (R17). It is not part of the committed M1 deliverable.
- **Multi-process wake-on-warning is optional** (R16); a single-app warning view satisfies M1.
- **Ghost C renders only from `v2x_relayed`.** The renderer's source guard is the mechanical form of the R19 claim and must stay exercised by a test.
- **The periodic `state` message is optional** on the producer side (R15). The consumer parses it — the model exists — but no acceptance box depends on it.

## 5. Getting the APK onto the node

- The APK is **not** baked into the VM image; the node's image is the starter-pack AAOS artifact and only the APK is installed after the node reaches Running ([node-ivi-ecu.md](../../../requirements/car-sky-guide/node-ivi-ecu.md)). Unlike the container nodes, there is nothing to push to Zot.
- Route: `adb connect <skycraft-adb-endpoint>` then `adb install -r app-debug.apk`, with the endpoint from the Rework device panel or the CarSky Gateway ADB tunnel.
- **Unverified on this deployment.** The platform's REST VM-shell route answers 502, which is also why the Phase 0 smoke test could only check the IVI hop indirectly ([deploy-walkthrough-netcheck.md § M10](../../../requirements/car-sky-guide/deploy-walkthrough-netcheck.md), open item O4). **Prove the ADB route early** — an unreachable guest turns every in-Room criterion into emulator evidence, and that discovery is worth days if it lands late.
- Runtime log: `adb logcat -s IVI_V2X` — the tag the renderer already uses. Keeping every layer on one tag makes the demo's evidence a single command.

## 6. Build, test and CI facts

- Build and test from `IVI_ECU/`: `./gradlew assembleDebug`, `./gradlew :app:testDebugUnitTest`, `./gradlew lint`.
- **CI runs `:app:testDebugUnitTest` only** ([phase0-ci.yml](../../../.github/workflows/phase0-ci.yml)). Any new Gradle submodule needs its tests added to that job, or they will pass locally and never run in CI.
- New modules must be registered in [settings.gradle.kts](../../settings.gradle.kts); `RepositoriesMode.FAIL_ON_PROJECT_REPOS` is set, so a module declaring its own repositories fails the build.
- Robolectric is **not** currently a dependency; a service-level test needs it added (or the receive loop must be extracted into a plain-JVM class that a socket test can drive, which is the cheaper design).
- No instrumentation-test or screenshot-test infrastructure exists. Compose previews are the current visual check, and that is proportionate for M1 — the acceptance evidence is a recording, not a golden image.

## 7. Evidence obligations that touch this node

- **R18** wants an event stream that reconstructs a run offline. The IVI side's contribution is a log line per received message (received, parsed, rendered, dropped-with-reason) — structured and greppable, on the `IVI_V2X` tag.
- **R19** — the definition of done — needs the recorded run to show ghost C on the IVI display with zero direct C detections. The IVI-side proof is that C's `source` was `v2x_relayed` on every rendered frame; log it, so the recording is backed by text.
