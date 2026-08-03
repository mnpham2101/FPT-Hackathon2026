# IVI ECU — component architecture

> Component-level design of the IVI node (R4, R16, R17): every component this node runs, its role, input and output, where it lives on disk, and how the components connect. Frozen contract: [contracts/r4-ada-ivi.schema.json](../../contracts/r4-ada-ivi.schema.json). Build, install and verification procedure: [deploy-ivi-hmi-walkthrough.md](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md). Node facts — VM artifact, pin, address: [node-ivi-ecu.md](../../requirements/car-sky-guide/node-ivi-ecu.md).
>
> Diagrams: [ivi-ecu-module-architecture.svg](research_notes/ivi-ecu-module-architecture.svg) (component map) · [phase5-ivi-callflow.puml](phase5-ivi-callflow.puml) (sequence) · [phase5-ivi-components.puml](phase5-ivi-components.puml) (module map).

## 1. Scope

`IVI_ECU/` only — the consumer side of R4 and everything downstream of it, up to the rendered God View and the log lines that evidence it.

- **In scope:** the five Gradle modules of this folder, the components inside them, the seams between them, the node's one network endpoint, and the test equipment that exercises the node on its own.
- **Out of scope:** how the R4 message is produced — this node depends on the `ADA-ECU` interface, never on a particular producer; the deploy/install/verify procedure, which is the walkthrough's; and the task decomposition, which is the plan's.

## 2. Sourced research notes

| Note | Adopted here |
|---|---|
| [phase5-r4-parsing.md](research_notes/phase5-r4-parsing.md) | §1 wire truth — no application header, de-framing is buffer slicing (D3). §2 decode-failure table as the `R4DecodeResult` shape. §3 unknown-`warningType` preservation (D4). §5 pure-JVM submodule placement (D1, D2). |
| [phase5-r4-simulator.md](research_notes/phase5-r4-simulator.md) | The simulator's two run modes and scenario-case table, and "payloads come from the frozen samples, never a literal" (D9). The simulator is IVI test equipment. |
| [deploy-ivi-hmi-walkthrough.md](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md) — a walkthrough, not a note, and authoritative rather than scratch | The mini-blueprint target (§4.11) and the ADA-node env contract (`IVI_ECU_HOST` / `IVI_ECU_PORT` / `R4_SCENARIO` / `R4_RATE_HZ` / `START_DELAY_S`, §4.8) that the simulator's in-Room mode reads verbatim. |

The two notes are non-authoritative scratch; the walkthrough in the last row is not, and on conflict the CLAUDE.md authority order wins.

## 3. The component architecture

![IVI ECU component architecture](research_notes/ivi-ecu-module-architecture.svg)

Source: [research_notes/ivi-ecu-module-architecture.svg](research_notes/ivi-ecu-module-architecture.svg).

The diagram is a UML component diagram: fill colour is the component's role, `«use»` dependencies are dashed with an open arrowhead, realization is dashed with a hollow triangle, and each seam is drawn as a provided interface meeting a required one at an assembly connector. Two `«node»` rectangles enclose the components — **IVI-ECU** holds everything this node runs, **ADA-ECU** holds the mocked producer it depends on — and the CarSky observation surfaces sit outside both. Component names in the tables below are the short `package/File` form; §4 resolves each to its module and full path.

### MVC separation

| Layer | Where | Rule that keeps it separate |
|---|---|---|
| **Data** | `:contract` models + frozen samples; `:app data/R4Repository` (last warning, last `state` by `seq`, link state) | The repository stores and routes; it never decides what a warning *means* and never formats anything |
| **Business logic** | `:serializer` (bytes → typed), `:observer` (receive, retry, back-pressure), `:app warning/WarningClassifier`, `ui/view/SceneCoordinateMapper` | All four are free of Android UI types; three of them are free of Android entirely, and every one is unit-testable without a device |
| **UI logic** | `:app ui/WarningViewModel` (Idle↔Active, timeout), `ui/MainViewModel` (mode, wake-on-warning, restore), `config/IviRuntimeConfig` | View-models hold no drawing code and no socket; they translate domain state into what the Display Area should show |
| **UI** | `ui/screen/MainScreen`, `ui/view/CanvasWarningView` behind `IviWarningViewSeam` | The seam is the swap point for the optional 3D renderer; `MainScreen` never touches a message type, only `WarningUiState` |

## 4. Folder structure

Five Gradle modules under `IVI_ECU/`, all sharing the package root `com.hackathon.v2x.ivi` — the Android module rooted at `src/main/java/`, the four pure-JVM ones at `src/main/kotlin/`, so a component's package is the same wherever it lives. Only the folders and files this document names appear below; the full per-file designation map, the tests and the build files are in [phase5-ivi-hld.md §3](phase5-ivi-hld.md#3-folder-structure-map--file-location-designations).

```
IVI_ECU/
├── settings.gradle.kts             includes :contract, :serializer, :observer, :app, :r4-simulator
├── gradle/libs.versions.toml       the version catalog governing all five modules
├── contracts/                      byte-synced R4 and R3 schema copies — the field list the model binds against
│
├── contract/                       :contract — the Data Model, pure JVM, zero Android
│   └── src/main/kotlin/…/model/
│       ├── R4Message.kt            the R4 message set and R4Json
│       ├── R3Snapshot.kt           the carried R3 snapshot, with its `source` provenance field
│       ├── SceneGeometry.kt        SceneGeometry and VehiclePosition
│       └── R4Contract.kt           known schema version, sample paths, warning-registry keys
│
├── serializer/                     :serializer — the parser, pure JVM
│   └── src/main/kotlin/…/serializer/
│       ├── R4Decoder.kt            the decode seam and R4DecodeResult
│       ├── R4Deserializer.kt       slice → BOM/UTF-8 → R4Json; returns a result, never throws
│       └── PayloadPreview.kt       the bounded preview of bad bytes that the [DROP] line carries
│
├── observer/                       :observer — the receive path, pure JVM
│   └── src/main/kotlin/…/observer/
│       ├── R4DatagramSource.kt     the source seam
│       ├── JdkDatagramSource.kt    the only socket holder — binds 0.0.0.0:47300
│       ├── R4SocketObserver.kt     the receive loop, truncation check, rebind back-off, bounded flow
│       ├── R4Event.kt              R4Event and R4LinkState
│       ├── R4ObserverConfig.kt     port, buffer, capacity, back-off bounds — no literals
│       └── R4Logger.kt             the logging seam AndroidR4Logger fills
│
├── app/                            :app — the front end, the only Android module
│   └── src/
│       ├── main/AndroidManifest.xml    launcher activity, listener service, automotive feature, permissions
│       ├── main/java/…/
│       │   ├── IviApplication.kt       the object graph and application scope
│       │   ├── MainActivity.kt         the Compose host and process entry
│       │   ├── di/IviGraph.kt          the hand-written composition root
│       │   ├── config/IviRuntimeConfig.kt   BuildConfig defaults merged with launch-time extras
│       │   ├── service/                R4ListenerService.kt, AndroidR4Logger.kt
│       │   ├── data/R4Repository.kt    the event raiser and single injection target
│       │   ├── warning/WarningClassifier.kt   warningType → presentation
│       │   ├── ui/                     WarningUiState.kt, WarningViewModel.kt, MainViewModel.kt, DisplayMode.kt
│       │   ├── ui/screen/MainScreen.kt the R16 layout
│       │   └── ui/view/                IviWarningViewSeam.kt, CanvasWarningView.kt (with the provenance guard),
│       │                               SceneCoordinateMapper.kt, WarningBannerOverlay.kt
│       └── debug/java/…/debug/DevInjectorReceiver.kt   debug source set only — absent from any release build
│
├── r4-simulator/                   :r4-simulator — test equipment; builds m1-r4-sim:latest
│   ├── Dockerfile · entrypoint.sh  the image the ADA Container node of the reduced IVI Room pulls
│   ├── scenarios/*.json            scenario data — a new case is a new file, never a new code branch
│   └── src/main/kotlin/…/sim/      the sender: scenario load, message build, validate through R4Json, UDP send
│
└── doc/                            this document, the HLD, the .puml diagrams, and research_notes/
```

Each module carries its own `src/test/` mirroring its main package, so the pure-JVM receive path is exercised with no device attached.

Where each component group of this document lands in that tree:

| Component group | Modules and folders |
|---|---|
| Business logic | `:serializer` and `:observer` entire, plus `app/…/warning/` |
| Data Model | `:contract` entire, plus `app/…/data/` |
| UI logic | `app/…/ui/` — the ViewModels and `DisplayMode` |
| UI / front-end | `app/…/ui/screen/` and `app/…/ui/view/` |
| Host and lifecycle | the `app/…/` root, `di/`, `service/` |
| Configuration and descriptors | `app/src/main/AndroidManifest.xml`, `IVI_ECU/contracts/`, `app/…/config/`, `r4-simulator/scenarios/` |
| Test equipment | `r4-simulator/` and `app/src/debug/` |

## 5. Platform and boundary

| Component | Role | Input | Output |
|---|---|---|---|
| **AAOS (Android Automotive OS)** | the platform the IVI-ECU app runs on: the Skycraft node's guest, which the APK is installed into | the APK, installed over ADB; touch and key events from the Screen widget | `java.net` datagram sockets, the Compose/SurfaceFlinger display surface, the logcat buffer, and the ADB surface that installs and launches the app |
| `«interface»` **ADA-ECU** | the producer's side of R4 — what this node depends on for input, never a particular producer | the scene composed by whatever realizes it | one R4 warning JSON datagram per event, to `10.99.0.13:47300` |
| **Screen widget** — CarSky | the visual observation surface: a platform widget, part of neither ECU | the guest's display output | a recording or screenshot of the God View |
| **Guest logcat** — CarSky | the text observation surface, likewise on the platform | the `IVI_V2X` tag | `adb logcat -s IVI_V2X`, or the Log widget in the Devices panel |

The ADA ECU is a Container node at `10.99.0.12`. It is mocked while the IVI ECU is exercised on its own: the `r4-simulator` realizes the `ADA-ECU` interface and runs there in place of the real ADA image, which realizes the same interface when it takes the node back — nothing on this side changes with the swap.

## 6. Internal components

### Business logic

Plain-JVM components, free of Android types, so the receive path is exercisable without a device.

| Component | Role | Input | Output |
|---|---|---|---|
| `observer/JdkDatagramSource` | the only holder of a socket; binds `0.0.0.0:47300` — never the node address — and resets packet length before every receive | UDP datagrams arriving on the port | `Received(buffer, offset, length)`, through the provided `R4DatagramSource` interface |
| `observer/R4SocketObserver` | the receive loop — truncation check, rebind back-off, and a bounded flow with `DROP_OLDEST` so a slow collector cannot stall the socket | `Received`, plus the decoder's result | `R4Event.Message` / `R4Event.Dropped`, `R4LinkState`, and the `[LINK]`, `[RX]` and `[DROP]` log lines |
| `serializer/R4Deserializer` | the parser: slice → BOM/UTF-8 → `R4Json`. Returns a result rather than throwing, so one bad datagram cannot stop the next good one | `buffer, offset, length` | `R4DecodeResult.Decoded` (carrying `schemaVersionAhead`) or `.Failed(reason, detail, preview)` |
| `warning/WarningClassifier` | maps the wire `warningType` to a presentation; an unrecognised value maps to the generic warning presentation and is never rewritten on the way through | `R4WarningEvent` | the presentation the Warning View renders |

Graceful degradation is split deliberately across the last two: the parser preserves the wire value and flags a schema version ahead of this one, and the classifier decides what an unrecognised value looks like. Neither treats it as an error.

### Data Model

| Component | Role | Input | Output |
|---|---|---|---|
| `model/` — `R4Message`, `R3Snapshot`, `SceneGeometry`, `VehiclePosition`, `R4Json` | the typed binding of the R4 message set and the R3 snapshot it carries, including the `source` provenance field and the nullable `vehicleC` | decoded JSON | `R4WarningEvent` / `R4StateMessage`, and the `SceneGeometry` the renderer draws |
| `data/R4Repository` | the event raiser — routes received events into app state, and is the single injection target the dev injector writes to | `R4Event`, injected samples | last warning, last `state` (last-value-wins by `seq`), link state |

### UI logic

| Component | Role | Input | Output |
|---|---|---|---|
| `ui/WarningViewModel` | the warning lifecycle: Idle ↔ Active, with `WARNING_TIMEOUT_MS` of silence returning the Display Area to Idle | the warning presentation and its scene | `WarningUiState` |
| `ui/MainViewModel` + `ui/DisplayMode` | which view the Display Area shows — Warning / Home / Apps / Settings — and whether the change came from a message (`cause=warning`) or a tap (`cause=user`) | mode requests from the button areas, warning state | `currentMode`, and the `[UI]` log line |

### UI / front-end

| Component | Role | Input | Output |
|---|---|---|---|
| `ui/screen/MainScreen` | the R16 layout: central Display Area, Home / Apps / Settings button areas, mode labels and bottom status bar; hosts the Warning View slot | `DisplayMode`, `WarningUiState`, `R4LinkState` | the composed screen |
| `ui/view/IviWarningViewSeam` | the R17 render seam — the contract `Render(scene, riskState)` that decouples the app from the rendering engine, so an optional 3D renderer swaps in with no consumer change | — | the interface both renderers realize |
| `ui/view/CanvasWarningView` | the God View as R17 fixes it: a dark canvas, a lane-marked road converging toward the top, and the three vehicles as car-shaped silhouettes in one lane with ego nearest the viewer — ego and B solid, ghost C dashed and translucent on a pulsing ground glow coloured by the risk state, and a `null` `vehicleC` rendered without C. **The scene alone is the warning:** no legend, no distance labels, no text overlay, no banner. The `[V2X]` badge and the A→B / A→C distance callouts belong to R17's annotated explanatory figure, not to this renderer | `SceneGeometry`, `riskState` | Compose Canvas draw calls |
| `ui/view/SceneCoordinateMapper` | scene metres → canvas coordinates for that view: the camera is slightly inclined rather than overhead, so the mapping is an oblique projection — depth compresses toward the top and each vehicle shows a shallow rear face — not a uniform top-down scale. Pure math, free of Android types | `SceneGeometry`, the base scale from config | screen-space geometry for the draw calls |
| the **provenance guard**, nested inside `CanvasWarningView` | ghost C is drawn only when its snapshot `source` is `v2x_relayed`; any other value draws the yellow `[? UNKNOWN SOURCE]` marker and logs at ERROR. This is the mechanical form of the R19 claim | `SceneGeometry.vehicleCSnapshot` | ghost C, or the marker and an ERROR line |
| `ui/view/WarningBannerOverlay` | the risk banner, kept out of the Display Area by standing decision and by R17's "no banner" so the God-View canvas renders unobstructed | `riskState` | a banner, mounted nowhere |

### Host and lifecycle

| Component | Role | Input | Output |
|---|---|---|---|
| `MainActivity` + `IviApplication` + `di/IviGraph` | the process entry and composition root: one launcher activity, one hand-written object graph, one application scope | the launch `Intent`, `BuildConfig` defaults | the running screen, and every component above wired together |
| `service/R4ListenerService` | the foreground host that keeps the receive loop alive while the Display Area shows something else | start / stop | the observer's lifetime, and the process's foreground priority |
| `service/AndroidR4Logger` | the only bridge from the plain-JVM components to `android.util.Log`, on the single tag `IVI_V2X`; realizes the `R4Logger` interface those components require | log calls from every layer | one greppable `key=value` line per event: `[LINK]`, `[RX]`, `[DROP]`, `[UI]` |

### Configuration and descriptors

Files, not components — neither data, business logic, UI logic nor front-end.

| Artifact | Role |
|---|---|
| `app/src/main/AndroidManifest.xml` | declares the launcher activity, the listener service, the `automotive` feature and the permissions |
| `IVI_ECU/contracts/*.schema.json` | this node's byte-synced copy of the R4 and R3 schemas — the field list the model and its round-trip tests bind against |
| `BuildConfig` / `config/IviRuntimeConfig` | port, timeout and scene-scale defaults, merged once with launch-time intent extras (`--ei r4_port`) so no other component carries a literal |
| `r4-simulator/scenarios/*.json` | scenario data; a new case is a new file, never a new code branch |

## 7. External related components

Everything the design names that sits outside the IVI-ECU node boundary: the `ADA-ECU` interface and the platform's two observation surfaces, tabulated in §5, and the test equipment below.

### Test equipment

Scaffolding for exercising the IVI ECU on its own, and nothing else depends on it. Two pieces, two delivery routes — only one of them is an image:

- **The simulator is a container image.** `IVI_ECU/r4-simulator/` builds `m1-r4-sim:latest`; GitHub Actions builds and pushes it to the CarSky Zot registry, and the ADA **Container node** of the reduced IVI Room pulls that tag — the same push-and-pull route every other node image takes ([phase5-ivi-hld.md §6.1](phase5-ivi-hld.md), [deploy-ivi-hmi-walkthrough.md §4.11](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#411-the-mini-blueprint-route), [zot-registry-api-key.md](../../requirements/car-sky-guide/zot-registry-api-key.md)). It is the one place Zot enters IVI work: the APK itself never touches the registry ([deploy-ivi-hmi-walkthrough.md §1](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md)).
- **The injector is not.** `DevInjectorReceiver` is a `BroadcastReceiver` in the app's debug source set — it ships inside the debug APK, and has no image, no registry and no node of its own. That is why the diagram places it inside the IVI-ECU boundary and the simulator inside ADA-ECU.

Neither reaches a release build: the injector is excluded by source set, and the simulator is a node away from the APK entirely.

| Component | Role | Input | Output |
|---|---|---|---|
| `IVI_ECU/r4-simulator/` → `m1-r4-sim:latest` | realizes the `ADA-ECU` interface; runs on the ADA Container node in place of the real ADA image | a scenario file and the frozen contract samples | R4 datagrams at `R4_RATE_HZ` to `10.99.0.13:47300`, and `[TX]` lines in the node log |
| `debug/DevInjectorReceiver` | exercises the whole UI path with no network; confined to the debug source set so no release build can fabricate a warning | `am broadcast -a com.hackathon.v2x.ivi.DEV_INJECT --es sample …` | one sample message onto the repository's flow |

## 8. Interfaces, ports and the layer rule

- **`ADA-ECU`** is the only thing outside this node the app depends on: an interface, not a box. `r4-simulator` realizes it while the IVI ECU is exercised alone, the real ADA ECU realizes it afterwards, and the app is written against neither.
- **`udp :47300`** is a port on `JdkDatagramSource` — the app's one external network endpoint, and where it meets the `ADA-ECU` interface. Nothing else in the app opens a socket.
- **`R4DatagramSource`** and **`R4Decoder`** are the seams that make the receive loop testable: the loop requires them; a fake or the real implementation provides them.
- **`R4Logger`** is the seam that keeps the plain-JVM components free of Android — they require it, `AndroidR4Logger` provides it.
- **`IviWarningViewSeam`** is realized by `CanvasWarningView` and used by `MainScreen`, which never touches a message type — only `WarningUiState`.

No layer is collapsed: the receive loop cannot reach a Composable and a Composable cannot reach a socket. The only path between them is `R4Event → R4Repository → WarningUiState`.

## 9. Call flow

[phase5-ivi-callflow.puml](phase5-ivi-callflow.puml) — PlantUML sequence: startup and bind, then the nominal warning path datagram → `JdkDatagramSource` → `R4SocketObserver` → `R4Deserializer` → `R4Repository` → `WarningViewModel` → `MainViewModel` → `MainScreen` → `CanvasWarningView` with the provenance guard, plus the malformed-drop, socket-rebind, warning-timeout and dev-injector branches.

## 10. Tech stack, build and CI

| Area | Stack | Trace |
|---|---|---|
| App language / UI | Kotlin 2.2.20, Jetpack Compose (BOM 2024.09.03), AndroidX, Material3 | report §3(e); R16/R17 tech stack |
| 2D renderer | Compose Canvas behind `IviWarningViewSeam`; SceneView/Filament optional | report §3(e), R17 |
| Contract binding | kotlinx.serialization-json | report R4 tech stack ("kotlinx.serialization (IVI side)"), D1 |
| Transport | `java.net.DatagramSocket`, UDP + versioned JSON | report §3(f), R6 |
| Concurrency | kotlinx-coroutines-core (`SharedFlow`/`StateFlow`) | D5 back-pressure policy |
| Build | Gradle multi-project, AGP 8.13, JDK 17, version catalog | D2, D8 |
| Tests | JUnit4 (+ kotlinx-coroutines-test in `:observer`); **no Robolectric** | D2 — the loop is plain JVM |
| Simulator image | multi-stage Docker; single-platform `linux/arm64`, `--provenance=false --sbom=false` | the cluster rejects manifest indexes |

Build commands, from `IVI_ECU/`: `./gradlew assembleDebug` · `./gradlew :contract:test :serializer:test :observer:test :r4-simulator:test :app:testDebugUnitTest` · `./gradlew lint`.

| CI lane | File | What it does |
|---|---|---|
| `ivi-unit-tests` | [phase0-ci.yml](../../.github/workflows/phase0-ci.yml) | the module tests on a plain JVM, no device and no emulator |
| `ivi-assemble` | [phase5-ci.yml](../../.github/workflows/phase5-ci.yml) | `assembleDebug` + `lint`, uploading `app-debug.apk` under the stable artifact name the walkthrough tells a human to download |
| the simulator image | [phase5-ci.yml](../../.github/workflows/phase5-ci.yml) | `linux/arm64` build of `m1-r4-sim:latest` from context `IVI_ECU/`, pushed to Zot and verified by pull-back |

`ivi-unit-tests` invokes `:app:testDebugUnitTest` until the other four modules exist; [phase5-ivi-hld.md §6.1](phase5-ivi-hld.md#61-the-ci-invocation-that-must-change) fixes the extended invocation and the simulator lane. The simulator's `Dockerfile` sits at `r4-simulator/Dockerfile` with build context `IVI_ECU/` — a flagged deviation from "own `Dockerfile` at the folder root", because this folder's primary artifact is the APK and the image is secondary test equipment; self-containment, the property that rule protects, holds since the build reads nothing outside `IVI_ECU/`.

## 11. Test strategy

Two configurations exercise the same node, and they differ in exactly one component — who realizes `ADA-ECU`:

- **Isolated IVI test — the mini-blueprint.** Ethernet Bridge + ADA Container node running `m1-r4-sim:latest` + the IVI Skycraft node. The scenario file chooses what the producer sends, including the degraded cases a real ADA run would not reproduce on demand.
- **System test — the whole blueprint.** The real ADA ECU takes its node back and sends R4 from its own fusion output, over the same bridge to the same port.

**The expected output and the observed behaviour are identical in both.** The log lines below are what each run is read against, and the God View is judged the same way; nothing in this node distinguishes the two producers, which is the point of depending on the interface rather than on a box. A difference between the two runs is therefore a producer finding, never an IVI one.

Expected observables, per [deploy-ivi-hmi-walkthrough.md §6](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance), and the component that produces each:

| Observable | Produced by |
|---|---|
| `[LINK] state=bound port=47300`, and the status bar's link indicator | `R4SocketObserver`'s link state, through `AndroidR4Logger` and `MainScreen` |
| `[RX] type=warning bytes=… from=…` | `JdkDatagramSource` → `R4SocketObserver` → `AndroidR4Logger` |
| `warningType=`, `risk=`, `cSource=`, `cPos=` on that line | `R4Deserializer` decoding into the contract model; the fields are read off the parsed message |
| `[DROP] reason=malformed …`, with the next valid message still rendering | `R4Deserializer` returning a result instead of throwing |
| `[UI] mode=WarningView cause=warning` | `R4Repository` → `WarningViewModel` → `MainViewModel` |
| `cause=user` on a button tap, and `cause=timeout` back to Idle | `MainViewModel`; `WarningViewModel`'s `WARNING_TIMEOUT_MS` |
| the God View drawn in the Display Area | `MainScreen` → `IviWarningViewSeam` → `CanvasWarningView` |
| `[? UNKNOWN SOURCE]` on an `own_sensor` message | the provenance guard |

Below both sits the plain-JVM unit layer: `:contract`, `:serializer` and `:observer` are tested with a fake source or a loopback socket, so the receive path is proven before any Room is booked, and the dev injector reaches the real UI with no network at all.

## 12. Design decisions

### D1 — The contract library is kotlinx.serialization in a pure-JVM submodule

The shared R4/R3 models and the configured `Json` live in a Gradle submodule `:contract` with zero Android dependencies, consumed by both the APK and the simulator. nlohmann/json stays on the ADA (producer) side where the report puts it — using it inside a Kotlin APK would mean an NDK/JNI layer for a job the Kotlin binding already does. A submodule rather than a package in `:app` because it must be usable by a command-line tool with no Android SDK and testable in the plain-JVM CI job: one artifact consumed by producer and consumer is what stops the two from drifting.

### D2 — Five modules, one-way dependency graph

`:contract` ← `:serializer` ← `:observer` ← `:app`, and `:contract` ← `:r4-simulator`. No cycles, and **Android types exist only in `:app`**.

| Module | Its one job | Depends on |
|---|---|---|
| `:contract` | the R4/R3 models, `R4Json`, and the frozen samples | — (kotlinx-serialization-json only) |
| `:serializer` | datagram bytes → typed R4 payload, or a typed failure | `:contract` |
| `:observer` | owns the socket, the receive loop, the retry policy, and the event flow | `:serializer` |
| `:app` | R16 layout, Display Area switcher, warning view-model, R17 God View | `:observer` (+ transitive) |
| `:r4-simulator` | test equipment: scenario-driven R4 traffic | `:contract` |

Consequences that are the point of the split: `:serializer` and `:observer` are plain JVM, so they run in CI with no device and no Robolectric; `:observer` never imports `android.util.Log`, logging through the `R4Logger` seam instead; `:serializer` never logs and never throws across the loop, which is what keeps one bad producer message from stopping the next good one; and the simulator reaches the models the app parses with by depending on `:contract`, never by importing across node folders.

### D3 — De-framing is buffer slicing, not header parsing

By the time Android hands the app a packet, the NIC and kernel have removed the Ethernet, IP and UDP headers: one R4 message is one UDP datagram is one UTF-8 JSON object, with no length prefix, envelope or framing header. "Strip the header, keep the payload" therefore means exactly these five things:

| Real job | Module | Failure it prevents |
|---|---|---|
| Decode `data[offset until offset+length]`, never the whole backing array | `:serializer` | Trailing bytes of a previous, longer datagram silently appended |
| `packet.setLength(buffer.size)` before **every** `receive()` | `:observer` | Every datagram after the first truncated to the shortest one seen |
| Treat `length == bufferBytes` as truncation-suspect: log, still attempt decode | `:observer` | Silent UDP truncation passing as malformed JSON with no clue why |
| Strip a UTF-8 BOM and surrounding whitespace before decoding | `:serializer` | A BOM the JSON parser will not tolerate |
| **No accumulate-and-split logic at all** | both | Inventing TCP framing for a datagram protocol that already preserves boundaries |

`R4_SOCKET_BUFFER_BYTES` defaults to 2048 against a ~450 B frozen warning. `network_security_config.xml` governs HTTP stacks only and has no bearing on a raw `DatagramSocket`.

### D4 — Unknown `warningType` is preserved verbatim; classification happens at the UI edge

The parser puts the wire value into `warningType` and stops; `WarningClassifier` in `:app` maps known types to their presentation and everything else to a generic one. **The parser must never rewrite an unknown `warningType` to `"unknown"`** — the committed `R4AdditiveVersionTest` asserts the opposite, and rewriting the field would destroy what the log needs and push a UI concern into the data layer. A `schemaVersion` above `R4Contract.KNOWN_SCHEMA_VERSION` is not a gate either: decode succeeds and `schemaVersionAhead` is set so the observer logs it once.

### D5 — A foreground service hosts the observer

`R4ListenerService` starts foreground immediately, survives the Display Area switching away from the Warning View, and holds foreground priority for the whole recorded run (R19 is *one continuous* run). The rejected alternative — a receive loop scoped to the Activity lifecycle — ties reception to whether the UI happens to be resumed.

- **The service is a lifecycle host, not the loop.** The loop, the back-off and the flow are plain-JVM code in `:observer`; the service only calls `start`/`stop`.
- **Socket:** bind `0.0.0.0:<port>`, never the node address, which the bridge assigns. Bind failure is logged at ERROR and retried with back-off, never swallowed.
- **Back-pressure:** `MutableSharedFlow` with bounded `extraBufferCapacity` and `DROP_OLDEST`, emitted with `tryEmit` — for warnings the newest message is the one that matters, and a slow collector must never stall the socket.
- `POST_NOTIFICATIONS` is a runtime permission from API 33: a denial suppresses the notification only and is never a failure to start.

### D6 — The frozen samples are **main** resources of `:contract`

One byte-synced location at `contract/src/main/resources/contracts/samples/*.json`, registered in [contracts/sync-manifest.json](../../contracts/sync-manifest.json), reachable from the three places that must agree: the contract tests, the simulator's payload builder, and the dev injector. Accepted cost: ~2 KB of fixtures ship inside the release APK. Rejected alternative: three separate copies, which is exactly the drift the sync manifest exists to prevent.

### D7 — Manual composition root; no Hilt

The object graph is seven objects with one Activity and one service. A hand-written `IviGraph` created in `IviApplication.onCreate` wires it in ~40 lines with no annotation processor; Hilt would additionally need `hilt-navigation-compose` and a KSP round on every build. Against [solution-selection-criteria](../../.claude/rules/solution-selection-criteria.md): C2 (fastest path to the milestone) and C4 (smaller surface). Replacing `IviGraph` later changes no consumer, because view-models are obtained through a single `ViewModelProvider.Factory` either way.

### D8 — A Gradle version catalog governs all five modules

`IVI_ECU/gradle/libs.versions.toml` is the single place Kotlin, kotlinx-serialization, coroutines, the Compose BOM, AGP and JUnit versions are declared; every module's `build.gradle.kts` uses aliases. Five modules resolving coroutines and serialization independently is precisely how a version skew appears at runtime instead of at build time. `settings.gradle.kts` keeps `RepositoriesMode.FAIL_ON_PROJECT_REPOS`; no module declares its own repositories.

### D9 — The simulator mutates JSON, then validates through `R4Json` before sending

A simulator carrying its own copy of the schema is a second, unversioned contract that keeps passing after the real one changes. So it loads the frozen sample from the `:contract` classpath, applies the scenario step's overrides at `JsonElement` level (`riskState`, `warningType`, `schemaVersion`, `object.source`, `object.distance`, `geometry.vehicleC` including explicit `null`, plus additive junk fields), decodes the result through `R4Json` before sending — a payload the simulator cannot parse is one the app cannot parse, and the run fails loudly at the producer — then sends, logs `[TX]`, and waits for the scenario's rate. The one exception is a step of kind `raw`, which sends literal bytes on purpose.

**Scenarios are data, not code** — the same rule R11 imposes on the bench. Format is JSON, parsed by kotlinx.serialization, so the tool adds zero dependencies beyond `:contract`.

### D10 — Configuration: `BuildConfig` defaults, launch-time override, no literals anywhere

Unlike a container node, whose env comes from the blueprint at deploy time, an installed APK cannot be reconfigured without a rebuild — so `BuildConfig` supplies the default and `MainActivity` accepts an intent-extra override at launch, merged in the one place, `IviRuntimeConfig.resolve(intent)`.

| `BuildConfig` field | Default | Consumer | Overridable at launch |
|---|---|---|---|
| `R4_UDP_PORT` | `47300` (blueprint-frozen) | `R4ObserverConfig.port` | `--ei r4_port` |
| `R4_SOCKET_BUFFER_BYTES` | `2048` | `R4ObserverConfig.bufferBytes` | — |
| `R4_FLOW_BUFFER_EVENTS` | `8` | `SharedFlow` extra buffer (DROP_OLDEST) | — |
| `R4_RETRY_INITIAL_MS` / `R4_RETRY_MAX_MS` | `500` / `5000` | rebind back-off | — |
| `WARNING_TIMEOUT_MS` | `10000` | `WarningViewModel` auto-dismiss | `--el warning_timeout_ms` |
| `SCENE_SCALE_M_PER_PX` | `0.5` | `CanvasWarningView` → `SceneCoordinateMapper` | `--ef scene_scale` |

`SCENE_SCALE_M_PER_PX` is the projection's **base** scale, not a uniform metres-per-pixel: R17's inclined camera compresses depth toward the top of the canvas, so the effective scale varies with distance and the mapper derives it from this one value.

### D11 — Standing decisions binding on this design

- **`WarningBannerOverlay` is built but not mounted in the Display Area** (standing user decision, 2026-07-26) — the God-View canvas is the deliverable and must render unobstructed. R17's visual target now requires the same thing independently: no banner, no legend, no text overlay, the scene alone.
- **Ghost C renders only from `v2x_relayed`** — the renderer's source guard is the mechanical form of the R19 claim and stays exercised by a test.
- **3D (`SceneViewWarning3D`) and multi-process wake-on-warning are optional**, not committed M1 deliverables; nothing else depends on either.
- **The periodic `state` message is optional on the producer side**; the consumer parses it (last-value-wins by `seq`) and no acceptance box depends on it.

### D12 — The provenance guard fails open, so every scene composer must fill the snapshot

`SceneGeometry.vehicleCSnapshot` is nullable and the guard treats a `null` snapshot as trusted. The guard therefore protects the render only when whatever composes the scene copies the R4 message's `object` snapshot into that field. A `SceneGeometry` built from the message's `geometry` alone carries no snapshot, draws ghost C unchallenged, and silently voids the R19 provenance claim — so every composer of a scene for the renderer populates `vehicleCSnapshot`.
