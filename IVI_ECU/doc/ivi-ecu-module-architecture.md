# IVI ECU — component architecture

Component map of the IVI node (R4, R16, R17) and, per component, its role, input and output. Design decisions behind it: [phase5-ivi-hld.md](phase5-ivi-hld.md). Build, install and verification procedure: [deploy-ivi-hmi-walkthrough.md](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md). Node facts — VM artifact, pin, address: [node-ivi-ecu.md](../../requirements/car-sky-guide/node-ivi-ecu.md).

![IVI ECU component architecture](research_notes/ivi-ecu-module-architecture.svg)

Source: [research_notes/ivi-ecu-module-architecture.svg](research_notes/ivi-ecu-module-architecture.svg).

The diagram is a UML component diagram: fill colour is the component's role, `«use»` dependencies are dashed with an open arrowhead, realization is dashed with a hollow triangle, and each seam is drawn as a provided interface meeting a required one at an assembly connector. Two `«node»` rectangles enclose the components — **IVI-ECU** holds everything this node runs, **ADA-ECU** holds the mocked producer it depends on — and the CarSky observation surfaces sit outside both. Component names in the tables below are the short `package/File` form; § Folder structure resolves each to its module and full path.

## Folder structure

Five Gradle modules under `IVI_ECU/`, all sharing the package root `com.hackathon.v2x.ivi` — the Android module rooted at `src/main/java/`, the four pure-JVM ones at `src/main/kotlin/`, so a component's package is the same wherever it lives. Only the folders and files this document names appear below; the full per-file designation map, the tests, the build files and which files are already committed are in [phase5-ivi-hld.md §3](phase5-ivi-hld.md#3-folder-structure-map--file-location-designations).

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

Where each section of this document lands in that tree:

| Section | Modules and folders |
|---|---|
| Business logic | `:serializer` and `:observer` entire, plus `app/…/warning/` |
| Data Model | `:contract` entire, plus `app/…/data/` |
| UI logic | `app/…/ui/` — the ViewModels and `DisplayMode` |
| UI / front-end | `app/…/ui/screen/` and `app/…/ui/view/` |
| Host and lifecycle | the `app/…/` root, `di/`, `service/` |
| Configuration and descriptors | `app/src/main/AndroidManifest.xml`, `IVI_ECU/contracts/`, `app/…/config/`, `r4-simulator/scenarios/` |
| Test equipment | `r4-simulator/` and `app/src/debug/` |

## Platform and boundary

| Component | Role | Input | Output |
|---|---|---|---|
| **AAOS (Android Automotive OS)** | the platform the IVI-ECU app runs on: the Skycraft node's guest, which the APK is installed into | the APK, installed over ADB; touch and key events from the Screen widget | `java.net` datagram sockets, the Compose/SurfaceFlinger display surface, the logcat buffer, and the ADB surface that installs and launches the app |
| `«interface»` **ADA-ECU** | the producer's side of R4 — what this node depends on for input, never a particular producer | the scene composed by whatever realizes it | one R4 warning JSON datagram per event, to `10.99.0.13:47300` |
| **Screen widget** — CarSky | the visual observation surface: a platform widget, part of neither ECU | the guest's display output | a recording or screenshot of the God View |
| **Guest logcat** — CarSky | the text observation surface, likewise on the platform | the `IVI_V2X` tag | `adb logcat -s IVI_V2X`, or the Log widget in the Devices panel |

The ADA ECU is a Container node at `10.99.0.12`. It is mocked while the IVI ECU is exercised on its own: the `r4-simulator` realizes the `ADA-ECU` interface and runs there in place of the real ADA image, which realizes the same interface when it takes the node back — nothing on this side changes with the swap.

## IVI-ECU components

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
| `ui/view/CanvasWarningView` + `ui/view/SceneCoordinateMapper` | the God View: ego and B solid with heading markers, ghost C dashed with a pulsing risk glow and the `[V2X]` badge, connector labels, and a `null` `vehicleC` rendered without C. The mapper is pure math, free of Android types | `SceneGeometry`, `riskState` | Compose Canvas draw calls |
| the **provenance guard**, nested inside `CanvasWarningView` | ghost C is drawn only when its snapshot `source` is `v2x_relayed`; any other value draws the yellow `[? UNKNOWN SOURCE]` marker and logs at ERROR. This is the mechanical form of the R19 claim | `SceneGeometry.vehicleCSnapshot` | ghost C, or the marker and an ERROR line |
| `ui/view/WarningBannerOverlay` | the risk banner, kept out of the Display Area by standing decision so the God-View canvas renders unobstructed | `riskState` | a banner, mounted nowhere |

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

## Test equipment

Scaffolding for exercising the IVI ECU on its own, and nothing else depends on it. Two pieces, two delivery routes — only one of them is an image:

- **The simulator is a container image.** `IVI_ECU/r4-simulator/` builds `m1-r4-sim:latest`; GitHub Actions builds and pushes it to the CarSky Zot registry, and the ADA **Container node** of the reduced IVI Room pulls that tag — the same push-and-pull route every other node image takes ([phase5-ivi-hld.md §6.1](phase5-ivi-hld.md), [deploy-ivi-hmi-walkthrough.md §4.11](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#411-the-mini-blueprint-route), [zot-registry-api-key.md](../../requirements/car-sky-guide/zot-registry-api-key.md)). It is the one place Zot enters IVI work: the APK itself never touches the registry ([deploy-ivi-hmi-walkthrough.md §1](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md)).
- **The injector is not.** `DevInjectorReceiver` is a `BroadcastReceiver` in the app's debug source set — it ships inside the debug APK, and has no image, no registry and no node of its own. That is why the diagram places it inside the IVI-ECU boundary and the simulator inside ADA-ECU.

Neither reaches a release build: the injector is excluded by source set, and the simulator is a node away from the APK entirely.

| Component | Role | Input | Output |
|---|---|---|---|
| `IVI_ECU/r4-simulator/` → `m1-r4-sim:latest` | realizes the `ADA-ECU` interface; runs on the ADA Container node in place of the real ADA image | a scenario file and the frozen contract samples | R4 datagrams at `R4_RATE_HZ` to `10.99.0.13:47300`, and `[TX]` lines in the node log |
| `debug/DevInjectorReceiver` | exercises the whole UI path with no network; confined to the debug source set so no release build can fabricate a warning | `am broadcast -a com.hackathon.v2x.ivi.DEV_INJECT --es sample …` | one sample message onto the repository's flow |

## Interfaces, ports and the layer rule

- **`ADA-ECU`** is the only thing outside this node the app depends on: an interface, not a box. `r4-simulator` realizes it while the IVI ECU is exercised alone, the real ADA ECU realizes it afterwards, and the app is written against neither.
- **`udp :47300`** is a port on `JdkDatagramSource` — the app's one external network endpoint, and where it meets the `ADA-ECU` interface. Nothing else in the app opens a socket.
- **`R4DatagramSource`** and **`R4Decoder`** are the seams that make the receive loop testable: the loop requires them; a fake or the real implementation provides them.
- **`R4Logger`** is the seam that keeps the plain-JVM components free of Android — they require it, `AndroidR4Logger` provides it.
- **`IviWarningViewSeam`** is realized by `CanvasWarningView` and used by `MainScreen`, which never touches a message type — only `WarningUiState`.

No layer is collapsed: the receive loop cannot reach a Composable and a Composable cannot reach a socket. The only path between them is `R4Event → R4Repository → WarningUiState`.

## A design property worth flagging: the guard fails open

`SceneGeometry.vehicleCSnapshot` is nullable, and the guard treats a `null` snapshot as trusted. The guard therefore protects the render only when whatever composes the scene copies the R4 message's `object` snapshot into that field. A `SceneGeometry` built from the message's `geometry` alone carries no snapshot, draws ghost C unchallenged, and silently voids the R19 provenance claim — so every composer of a scene for the renderer must populate `vehicleCSnapshot`.

## Which component produces which acceptance observable

Per [deploy-ivi-hmi-walkthrough.md §6](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance):

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
