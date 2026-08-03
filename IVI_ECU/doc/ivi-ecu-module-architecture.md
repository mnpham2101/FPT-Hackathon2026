# IVI ECU — module architecture

Module map of the IVI node (R4, R16, R17) and, per module, its role, input and output. It describes **what is in `IVI_ECU/` today** and what is designed but unwritten; the design decisions behind it are [phase5-ivi-hld.md](phase5-ivi-hld.md), and the build/install/verify procedure is [deploy-ivi-hmi-walkthrough.md](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md). Node facts — VM artifact, pin, address — are [node-ivi-ecu.md](../../requirements/car-sky-guide/node-ivi-ecu.md).

The module set below is sized to the walkthrough's deliverable list (§1.3), its verification ladder (§4.8, V1–V5) and its acceptance table (§6): every observable those name has an owner here.

![IVI ECU module architecture](research_notes/ivi-ecu-module-architecture.svg)

Diagram source: [research_notes/ivi-ecu-module-architecture.drawio](research_notes/ivi-ecu-module-architecture.drawio) — edit the `.drawio` and re-export the `.svg` beside it; the two are one artifact.

**State** in the tables below: `built` = in the tree · `partial` = present but incomplete, with the gap named · `specified` = designed, no source yet. Paths are relative to `IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/` unless shown otherwise.

## The boundary

The app is one process inside the AAOS guest. Everything in this table is outside it.

| Module | Role | Input | Output | State |
|---|---|---|---|---|
| **AAOS guest on the Skycraft node** | the platform the APK is installed into — Android runtime, `java.net` sockets, the Compose/SurfaceFlinger display, logcat, ADB. Not our code | the APK, over ADB | the runtime services every module below uses | platform |
| **ADA ECU node** (`10.99.0.12`) | the R4 producer | its composed scene | one R4 warning JSON datagram per event → `10.99.0.13:47300` | external |
| **`IVI_ECU/r4-simulator/`** → `m1-r4-sim:latest` | test equipment standing in for the ADA ECU; runs on the ADA node, never in the APK | a scenario file plus the frozen samples | R4 datagrams at `R4_RATE_HZ`, and `[TX]` lines in the node log | specified |
| **Screen widget** | the visual observable — acceptance proof 4 | the guest framebuffer | recording or screenshot | external |
| **Guest logcat** | the text observable — acceptance proofs 1–3 | the `IVI_V2X` tag | `adb logcat -s IVI_V2X`, or the Log widget | external |

What AAOS is, and every route by which an Android app reaches a Skycraft node, is [research_notes/skycraft-android-deployment-methods.md](research_notes/skycraft-android-deployment-methods.md) — not repeated here.

## Business logic — ingress

Plain-JVM modules, free of Android types, so the receive path is testable without a device.

| Module | Role | Input | Output | State |
|---|---|---|---|---|
| `observer/JdkDatagramSource` | the only socket holder: binds `0.0.0.0:47300` (never the node address), resets packet length before every receive | UDP datagrams from the bridge | `Received(buffer, offset, length)` | specified |
| `observer/R4SocketObserver` | the receive loop — truncation check, rebind back-off, bounded flow with `DROP_OLDEST` | `Received` plus the decoder's result | `R4Event.Message` / `R4Event.Dropped`, `R4LinkState`; the `[LINK]`, `[RX]` and `[DROP]` lines | specified |
| `serializer/R4Deserializer` | the parser: slice → BOM/UTF-8 → `R4Json`; never throws, so one bad datagram cannot stop the next good one | `buffer, offset, length` | `R4DecodeResult.Decoded` (with `schemaVersionAhead`) or `.Failed(reason, detail, preview)` → a `[DROP]` | specified |
| `warning/WarningClassifier` | maps the wire `warningType` to a presentation; an unknown value degrades to a generic warning and is **never rewritten** on the way through | `R4WarningEvent` | the presentation the Warning View uses | specified |

Graceful degradation is split deliberately: the parser preserves the wire value and flags a schema version ahead of ours, the classifier decides what that renders as. Neither crashes on either input — the V5 rows of the walkthrough's ladder.

## Data

| Module | Role | Input | Output | State |
|---|---|---|---|---|
| `model/R4Message.kt`, `R3Snapshot.kt`, `SceneGeometry.kt` (+ `R4Json`) | the typed binding of the R4 message set and the R3 snapshot it carries, including the `source` provenance field and the nullable `vehicleC` | decoded JSON | `R4WarningEvent` / `R4StateMessage` and their `SceneGeometry` | built |
| `IVI_ECU/contracts/*.schema.json` | this node's byte-synced copy of the R4 and R3 schemas | — | the field list the model and its round-trip tests bind against | built |
| `data/R4Repository` | the event raiser — routes events into app state and is the single injection target for the dev injector | `R4Event`, injected samples | last warning, last `state` (last-value-wins by `seq`), link state | specified |

The models live under `app/` today; the HLD relocates them verbatim into a pure-JVM `:contract` submodule so the simulator can share them. That is a move, not a rewrite.

`SceneGeometry.vehicleCSnapshot` is what arms the provenance guard. Whatever composes the scene must copy the message's `object` snapshot into it — a `null` snapshot is treated as trusted, so omitting the copy silently disables the guard.

## UI logic

| Module | Role | Input | Output | State |
|---|---|---|---|---|
| `ui/WarningViewModel` | Idle ↔ Active; `WARNING_TIMEOUT_MS` with no further message returns the Display Area to Idle | the warning presentation and its scene | `WarningUiState` | specified |
| `ui/MainViewModel` + `ui/DisplayMode` | which view the Display Area shows — Warning / Home / Apps / Settings — and whether the change was `cause=warning` or `cause=user` | mode requests from the button areas, warning state | `currentMode`, and the `[UI]` line | partial — the switcher and the warning lock are built; wake-on-warning, previous-mode restore and the `cause=` distinction are not |
| `config/IviRuntimeConfig` + `BuildConfig` | resolves port, timeout and scene scale once, from compiled defaults merged with launch-time intent extras (`--ei r4_port`) — no literals anywhere else | `BuildConfig`, the launch `Intent` | the resolved values every other module is constructed with | partial — `WARNING_TIMEOUT_MS` exists; the port field and the intent merge do not |

## UI — the rendering layer

| Module | Role | Input | Output | State |
|---|---|---|---|---|
| `ui/screen/MainScreen` | the R16 layout: central Display Area, Home/Apps/Settings button areas, mode labels, bottom status bar | `DisplayMode`, `WarningUiState` | the composed screen | partial — layout and the mode switcher are built; the Warning View slot is a placeholder and `V2X LINK` is a hardcoded string, not the observer's link state |
| `ui/view/IviWarningViewSeam` | the R17 render seam — the 2D renderer is committed, an optional 3D one swaps in behind the same interface with no consumer change | `SceneGeometry`, `riskState` | a drawn scene | built |
| `ui/view/CanvasWarningView` + `ui/view/SceneCoordinateMapper` | the God View: ego and B solid with heading markers, ghost C dashed with a pulsing risk glow and the `[V2X]` badge, connector labels, and a `null` `vehicleC` rendered without C and without a crash. The mapper is pure math, no Android types | `SceneGeometry`, `riskState` | Compose Canvas draw calls | built |
| the **provenance guard**, inside `CanvasWarningView` | ghost C is drawn only when its snapshot `source` is `v2x_relayed`; anything else draws the yellow `[? UNKNOWN SOURCE]` marker and logs at ERROR. This is the mechanical form of the R19 claim | `SceneGeometry.vehicleCSnapshot` | ghost C, or the marker plus an ERROR line | built |
| `ui/view/WarningBannerOverlay` | built and **deliberately left unmounted** — the God-View canvas must render unobstructed (standing decision) | — | — | built, unmounted |

## Host, lifecycle and Android adapters

| Module | Role | Input | Output | State |
|---|---|---|---|---|
| `MainActivity` | the launcher entry `am start -n com.hackathon.v2x.ivi/.MainActivity` starts; hosts Compose and resolves launch-time config | the launch `Intent` | the running screen | specified |
| `IviApplication` + `di/IviGraph` | the composition root — one hand-written object graph, one application scope, no annotation processor | — | every module above, wired | specified |
| `service/R4ListenerService` | foreground host that keeps the receive loop alive while the Display Area shows something else | start/stop | the observer's lifetime and process priority | specified |
| `service/AndroidR4Logger` | the **only** bridge from the plain-JVM modules to `android.util.Log`, on the single tag `IVI_V2X`: `[LINK]`, `[RX]`, `[DROP]`, `[UI]` | log calls from every layer | one greppable `key=value` line per event | partial — only the guard's ERROR line exists today |
| `debug/DevInjectorReceiver` | debug-build-only broadcast entry that puts a frozen sample onto the repository's flow, exercising the whole UI path with no network. Excluded from release by source set, so no release build can fabricate a warning | `am broadcast -a com.hackathon.v2x.ivi.DEV_INJECT --es sample …` | one injected message | specified |
| `app/src/main/AndroidManifest.xml` | declares the launcher activity, the service, the `automotive` feature and the permissions | — | an installable, **launchable** APK | partial — declares the feature and permissions but neither `<activity>` nor `<service>`, so the APK installs and cannot be started |

## MVC separation

| Layer | Modules |
|---|---|
| **Data** | the contract models and the byte-synced schemas; `data/R4Repository` — stores and routes, never decides what a warning means and never formats |
| **Business logic** | `observer/`, `serializer/`, `warning/WarningClassifier`, `ui/view/SceneCoordinateMapper` — all free of Android UI types, all unit-testable without a device |
| **UI logic** | `ui/WarningViewModel`, `ui/MainViewModel`, `config/IviRuntimeConfig` — no drawing code, no socket |
| **UI** | `ui/screen/MainScreen`, and the renderer behind `IviWarningViewSeam` — never touches a message type, only `WarningUiState` |

No layer is collapsed: the receive loop cannot reach a Composable and a Composable cannot reach a socket. The only path between them is `R4Event → R4Repository → WarningUiState`.

## Which module closes which acceptance observable

Per [deploy-ivi-hmi-walkthrough.md §6](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance):

| Observable | Produced by |
|---|---|
| `[RX] type=warning bytes=… from=…` | `JdkDatagramSource` → `R4SocketObserver` → `AndroidR4Logger` |
| `warningType=`, `risk=`, `cSource=`, `cPos=` on that line | `R4Deserializer` decoding into the contract model; the fields are read off the parsed message |
| `[UI] mode=WarningView cause=warning` | `R4Repository` → `WarningViewModel` → `MainViewModel` |
| the God View drawn in the Display Area | `MainScreen` → `IviWarningViewSeam` → `CanvasWarningView` |
| `cause=user` on a button tap, and the timeout back to Idle | `MainViewModel`; `WarningViewModel`'s `WARNING_TIMEOUT_MS` |
| `[? UNKNOWN SOURCE]` on an `own_sensor` message | the provenance guard |
| `[LINK] state=bound port=47300` and the status bar | `R4SocketObserver`'s link state, bound into `MainScreen`'s bottom bar |
| `[DROP] reason=malformed …`, with the next valid message still rendering | `R4Deserializer` returning a result instead of throwing |

## What this means for the node today

- **6 modules are built** — the contract layer and the whole drawing layer, including the provenance guard. They are the two hardest pieces and they are done.
- **5 are partial**, each with a named gap above.
- **11 are specified only**: the entire ingress path, the repository, the warning view-model, the host and lifecycle classes, the dev injector, and the simulator.
- The gap is the middle of the app plus the Activity that hosts it. Until the manifest declares a launcher activity, nothing renders on the node at all — the APK installs and cannot be started, which is why the walkthrough's `aapt` launcher check exists.
