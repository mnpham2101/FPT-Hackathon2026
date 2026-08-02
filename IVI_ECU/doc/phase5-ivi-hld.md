# Phase 5 HLD — IVI ECU: R4 ingest and the God View (R4, R16, R17)

> High-level design for [milestone1.md § Phase 5](../../plans/milestone1.md#phase-5--ivi-hmi-mock-driven-r16-r17--display-track-parallel-from-the-start), per [hld-content-and-commit-format.md](../../.claude/rules/hld-content-and-commit-format.md). Requirement definitions and stack: [m1-cooperative-awareness.md](../../requirements/m1-cooperative-awareness.md) R4 / R16 / R17 and §3(e)/(f). Frozen contract: [contracts/r4-ada-ivi.schema.json](../../contracts/r4-ada-ivi.schema.json).
>
> Diagrams: [phase5-ivi-components.puml](phase5-ivi-components.puml) (module map) · [phase5-ivi-callflow.puml](phase5-ivi-callflow.puml) (sequence).
>
> **Scope:** `IVI_ECU/` only. Everything this design adds lands in this folder, except the five out-of-folder edits listed in §3.2 — each in a location this repo already sanctions for that artifact kind.

## 1. What this phase is actually building

The contract layer and the drawing layer are already committed and are the two hardest pieces; **the gap is the middle of the app** plus the Activity that hosts it ([implementation notes §1](research_notes/phase5-ivi-implementation-notes.md)). Phase 5 closes that gap as four independent modules — a reusable contract library, a serializer, a socket observer, and the front end — plus an R4 simulator that produces the traffic they consume.

| Already committed (do not rewrite) | Missing (this design) |
|---|---|
| `R4Message` / `R3Snapshot` / `SceneGeometry` / `R4Json` and their round-trip + additive-version tests | The Gradle submodule they move into, and the decode entry point above them |
| `IviWarningViewSeam`, `CanvasWarningView`, `SceneCoordinateMapper`, `WarningBannerOverlay` | The socket, the receive loop, the repository, the warning view-model |
| `MainScreen` (R16 layout, Warning View is a placeholder), `MainViewModel`, `DisplayMode` | `MainActivity` + `IviApplication` — **the APK has no launcher entry today**, so nothing renders on the node |
| Byte-synced R3/R4 schemas and samples | The R4 simulator, the dev injector, and the CI lanes that run them |

### 1.1 Sourced research notes

| Note | Adopted here |
|---|---|
| [phase5-r4-parsing.md](research_notes/phase5-r4-parsing.md) | §1 wire truth — no application header; de-framing is buffer slicing (D3). §2 decode-failure table as the `R4DecodeResult` shape. §3 unknown-`warningType` preservation (D4). §5 pure-JVM submodule placement (D1, D2). |
| [phase5-r4-simulator.md](research_notes/phase5-r4-simulator.md) | Injection points I1–I4 as the test strategy (§7). Two run modes, scenario-cases table, and "payloads come from the frozen samples, never a literal" (D9). Simulator is IVI test equipment, inside this folder. |
| [phase5-ivi-implementation-notes.md](research_notes/phase5-ivi-implementation-notes.md) | §1 inventory (above). §2 Android constraints → D5 and the back-pressure policy. §3 `BuildConfig`-default + launch-override config model (D10). §4 decisions in force (D11). §6 CI facts (§6). §7 R18/R19 log obligations (§5.4). |
| [phase5-mini-blueprint.md](research_notes/phase5-mini-blueprint.md) | The 3-node deploy target and the ADA-node env contract (`IVI_ECU_HOST`/`IVI_ECU_PORT`/`R4_SCENARIO`/`R4_RATE_HZ`/`START_DELAY_S`) that the simulator's in-Room mode must read verbatim (§8). |

Notes are non-authoritative scratch; on conflict the CLAUDE.md authority order wins. One such conflict is resolved in D4 and one in §9.1.

## 2. Design decisions

### D1 — The contract library is kotlinx.serialization in a pure-JVM submodule (user decision, 2026-08-02)

The shared R4/R3 models and the configured `Json` live in a **Gradle submodule `:contract` with zero Android dependencies**, consumed by both the APK and the simulator.

- **kotlinx.serialization, not nlohmann/json.** nlohmann is a C++ header library; parsing JSON with it inside a Kotlin APK would require an NDK/JNI layer — a whole extra toolchain for a job the Kotlin standard binding already does, already tested against the frozen samples. nlohmann stays where the report puts it: the ADA (producer) side, in C++ (report R4 tech stack: *"nlohmann/json (ADA side); kotlinx.serialization (IVI side)"*). **Not re-openable.**
- **Why a submodule and not a package in `:app`:** it must be reusable by a command-line tool with no Android SDK, and it must be testable in the plain-JVM CI job. One artifact consumed by producer and consumer is what stops the two from drifting.
- **Moving the models is a relocation, not a rewrite.** `R4Message.kt`, `R3Snapshot.kt`, `SceneGeometry.kt` and both committed tests move verbatim — same package `com.hackathon.v2x.ivi.model`, same test-resource paths (D6 keeps `/contracts/samples/…` valid), so `R4RoundTripTest` and `R4AdditiveVersionTest` keep passing **unchanged**.

### D2 — Four modules, one-way dependency graph

`:contract` ← `:serializer` ← `:observer` ← `:app`, and `:contract` ← `:r4-simulator`. No cycles, and **Android types exist only in `:app`**.

| # | Module | Gradle path | Its one job | Depends on |
|---|---|---|---|---|
| 1 | Contract library | `:contract` | The R4/R3 models, `R4Json`, and the frozen samples | — (kotlinx-serialization-json only) |
| 2 | Serializer | `:serializer` | Datagram bytes → typed R4 payload, or a typed failure | `:contract` |
| 3 | Observer | `:observer` | Owns the socket, the receive loop, the retry policy, and the event flow | `:serializer` |
| 4 | Front end | `:app` | R16 layout, Display Area switcher, warning view-model, R17 God View | `:observer` (+ transitive) |
| 5 | Simulator | `:r4-simulator` | Test equipment: scenario-driven R4 traffic | `:contract` |

Consequences that are the point of the split:

- **`:serializer` and `:observer` are plain JVM**, so injection points I1 and I2 run in CI with no device and no Robolectric — the cheaper design the implementation notes call for. Robolectric is not added.
- **`:observer` never imports `android.util.Log`.** It logs through an `R4Logger` seam that `:app` implements over the `IVI_V2X` tag.
- **`:serializer` never logs and never throws across the loop.** It returns a result; the observer decides what to log. This is what keeps one bad producer message from stopping the next good one.
- The simulator cannot reach into `ADA_ECU/` (no cross-node source imports, [node-code-layout.md](../../.claude/rules/node-code-layout.md)); it reaches the same models the app parses with, by depending on `:contract`.

### D3 — De-framing is buffer slicing, not header parsing

**An implementer who thinks they must parse an Ethernet header will write wrong code.** By the time Android hands the app a packet, the NIC/kernel have already removed the Ethernet, IP and UDP headers; one R4 message is one UDP datagram is one UTF-8 JSON object, with no length prefix, no envelope and no framing header ([parsing note §1](research_notes/phase5-r4-parsing.md)). "Strip the header, keep the payload" therefore means exactly these five things, split across modules 2 and 3:

| Real job | Module | Failure it prevents |
|---|---|---|
| Decode `data[offset until offset+length]`, never the whole backing array | `:serializer` | Trailing bytes of a previous, longer datagram silently appended |
| `packet.setLength(buffer.size)` before **every** `receive()` | `:observer` (`JdkDatagramSource`) | Every datagram after the first truncated to the shortest one seen |
| Treat `length == bufferBytes` as truncation-suspect: log, still attempt decode | `:observer` | Silent UDP truncation passing as malformed JSON with no clue why |
| Strip a UTF-8 BOM and surrounding whitespace before decoding | `:serializer` | A BOM the JSON parser will not tolerate |
| **No accumulate-and-split logic at all** | both | Inventing TCP framing for a datagram protocol that already preserves boundaries |

`R4_SOCKET_BUFFER_BYTES` defaults to 2048 — the frozen `r4-warning.json` is ~450 B and bridge MTU headroom (open item O3) is far above that. `network_security_config.xml` governs HTTP stacks only and has no bearing on a raw `DatagramSocket`; it stays for correctness of any future HTTP use.

### D4 — Unknown `warningType` is preserved verbatim; classification happens at the UI edge

The parser puts the wire value into `warningType` and stops. A separate `WarningClassifier` in `:app` maps *known* types to their presentation and everything else to a generic warning presentation.

**This overrides [plans/phase5_tasks.md](../../plans/phase5_tasks.md) subtask 4.5.1.2** ("Unknown `warningType` → parsed as `warningType = "unknown"`"). The committed [R4AdditiveVersionTest](../app/src/test/java/com/hackathon/v2x/ivi/model/R4AdditiveVersionTest.kt) asserts the opposite — `assertEquals("slippery_road", warning.warningType)` — and the frozen schema says unknown values *degrade gracefully*, not that they are replaced. Rewriting the field at parse time would destroy the information the log needs, break the committed round-trip equality, and push a UI concern into the data layer. The user-visible acceptance ("a newer message degrades gracefully instead of crashing") is met either way.

A `schemaVersion` above `R4Contract.KNOWN_SCHEMA_VERSION` is not a gate either: decode succeeds and `R4DecodeResult.Decoded.schemaVersionAhead` is set so the observer logs it once.

### D5 — A foreground service hosts the observer; a lifecycle-scoped receiver is the rejected alternative

Both work for M1 — the head unit runs this app in the foreground for the whole demo — so this is recorded as a conscious choice, not a default.

- **Picked:** `R4ListenerService`, a foreground service with a notification channel and `startForeground()` immediately on start. It survives the Display Area switching away from the Warning View, keeps the process at foreground priority for the whole recorded run (R19 is *one continuous* run), and is the only shape from which the optional multi-process wake-on-warning path (R16) is reachable without a rewrite.
- **Rejected:** a receive loop scoped to the Activity/Application lifecycle. Cheaper, but it ties message reception to whether the UI happens to be resumed, which is exactly the coupling the R16 optional path forbids.
- `POST_NOTIFICATIONS` is a runtime permission from API 33: **a denied permission suppresses the notification only and must never be treated as a failure to start.** `foregroundServiceType="connectedDevice"` is declared although `targetSdk 33` does not yet require it; at `targetSdk 34` it additionally needs `FOREGROUND_SERVICE_CONNECTED_DEVICE`.
- **The service is a lifecycle host, not the loop.** The loop, the back-off and the flow live in `:observer` as plain-JVM code; the service only calls `start`/`stop` and holds the process priority. That is what makes I2 a plain-JVM test.
- **Socket:** bind `0.0.0.0:<port>` — never the node address, which the bridge assigns. Bind failure is logged at ERROR and retried with back-off, never swallowed.
- **Back-pressure:** `MutableSharedFlow` with bounded `extraBufferCapacity` and `BufferOverflow.DROP_OLDEST`; the loop uses `tryEmit`, never a suspending emit. For warnings the newest message is the one that matters, and a slow collector must never stall the socket.

### D6 — The frozen samples become **main** resources of `:contract`

One byte-synced location, reachable from three places that must agree: the contract tests, the simulator's payload builder, and the dev injector.

- Path: `IVI_ECU/contract/src/main/resources/contracts/samples/*.json`, re-registered in [contracts/sync-manifest.json](../../contracts/sync-manifest.json) (four targets repointed from `IVI_ECU/app/src/test/resources/…`). Without that edit the integrity gate stops matching.
- Because the resource root still contains `contracts/samples/`, the committed tests' `getResourceAsStream("/contracts/samples/r4-warning.json")` calls resolve unchanged — the relocation touches no test source.
- **Accepted cost:** ~2 KB of contract fixtures ship inside the release APK. Rejected alternative: three separate copies (test resources, simulator resources, debug resources), which is exactly the drift the sync manifest exists to prevent.

### D7 — Manual composition root; Hilt is removed from the app build

The object graph is seven objects with one Activity and one service. A hand-written `IviGraph` created in `IviApplication.onCreate` wires it in ~40 lines and needs no annotation processor; Hilt would additionally need `androidx.hilt:hilt-navigation-compose` for `hiltViewModel()` and a KSP round on every build.

Against [solution-selection-criteria](../../.claude/rules/solution-selection-criteria.md): **C2** (fastest path to the milestone — six days to the deadline) and **C4** (smaller surface). The Hilt plugin and its two dependencies come out of `app/build.gradle.kts`; nothing uses them today (there is no `@HiltAndroidApp` class). If the team later prefers Hilt, only `IviGraph` is replaced — no consumer of it changes, because view-models are obtained through a single `ViewModelProvider.Factory` either way.

### D8 — A Gradle version catalog governs five modules' dependencies

`IVI_ECU/gradle/libs.versions.toml` becomes the single place where Kotlin, kotlinx-serialization, coroutines, Compose BOM, AGP and JUnit versions are declared; every module's `build.gradle.kts` uses aliases. Five modules resolving coroutines and serialization independently is precisely how a version skew appears at runtime instead of at build time. `app/build.gradle.kts`'s existing dependency block is rewritten to aliases in the same subtask — a mechanical change with no behavioural effect. `settings.gradle.kts` keeps `RepositoriesMode.FAIL_ON_PROJECT_REPOS`; **no module declares its own repositories** or the build fails.

### D9 — The simulator mutates JSON, then validates through `R4Json` before sending

A simulator carrying its own copy of the schema is a second, unversioned contract that keeps passing after the real one changes. So:

1. Load the frozen sample from the `:contract` classpath (D6).
2. Apply the scenario step's overrides at `JsonElement` level — `riskState`, `warningType`, `schemaVersion`, `object.source`, `object.distance`, `geometry.vehicleC` (including explicit `null`), plus arbitrary additive junk fields. Element-level editing is what lets an *unknown extra field* survive onto the wire; a typed round trip would drop it (the committed additive test proves the drop).
3. **Decode the result through `:contract`'s `R4Json` before sending** — a payload the simulator cannot parse is a payload the app cannot parse, and the run fails loudly at the producer. The one exception is a step of kind `raw`, which sends literal bytes on purpose (the malformed case).
4. Send, log `[TX]`, wait for the scenario's rate.

**Scenarios are data, not code** — the same rule R11 imposes on the bench: a new case is a new file under `r4-simulator/scenarios/`, never a new code branch. Format is JSON, parsed by kotlinx.serialization, so the tool adds **zero** dependencies beyond `:contract` (C4); the bench's YAML choice does not carry over because that would mean adding a YAML library for three files.

### D10 — Configuration: `BuildConfig` defaults, launch-time override, no literals anywhere

`buildConfigField` values are baked at compile time. Unlike a container node — whose env comes from the blueprint at deploy time — an installed APK cannot be reconfigured without a rebuild, so `BuildConfig` supplies the **default** and `MainActivity` accepts an intent-extra override at launch:

```
adb shell am start -n com.hackathon.v2x.ivi/.MainActivity --ei r4_port 47301
```

`IviRuntimeConfig.resolve(intent)` is the one place that merges the two; every other class receives the resolved value. That keeps a wrong-port day from costing a rebuild-reinstall cycle mid-demo.

| `BuildConfig` field | Default | Consumer | Overridable at launch |
|---|---|---|---|
| `R4_UDP_PORT` | `47300` (blueprint-frozen) | `R4ObserverConfig.port` | `--ei r4_port` |
| `R4_SOCKET_BUFFER_BYTES` | `2048` | `R4ObserverConfig.bufferBytes` | — |
| `R4_FLOW_BUFFER_EVENTS` | `8` | `SharedFlow` extra buffer (DROP_OLDEST) | — |
| `R4_RETRY_INITIAL_MS` / `R4_RETRY_MAX_MS` | `500` / `5000` | rebind back-off | — |
| `WARNING_TIMEOUT_MS` | `10000` *(already committed)* | `WarningViewModel` auto-dismiss | `--el warning_timeout_ms` |
| `SCENE_SCALE_M_PER_PX` | `0.5` | `CanvasWarningView` → `SceneCoordinateMapper` | `--ef scene_scale` |

`SceneCoordinateMapper.DEFAULT_SCALE_METERS_PER_PIXEL` stays as the library default; `CanvasWarningView` gains a defaulted constructor parameter that the graph fills from config, so the committed previews still compile untouched.

### D11 — Decisions in force, restated as binding on this design

Not re-litigable during implementation ([implementation notes §4](research_notes/phase5-ivi-implementation-notes.md)):

- **`WarningBannerOverlay` is built but must NOT be mounted in the Display Area** (standing user decision, 2026-07-26). The God-View canvas is the deliverable and must render unobstructed. The integration work does not add the banner unless that decision is explicitly revisited.
- **Ghost C renders only from `v2x_relayed`** — the renderer's source guard is the mechanical form of the R19 claim and stays exercised by a test.
- **3D (`SceneViewWarning3D`) and multi-process wake-on-warning are optional**, not committed M1 deliverables. The 3D path's file location is designated in §3 so an optional attempt has a home, and nothing else depends on it.
- **The periodic `state` message is optional on the producer side**; the consumer parses it (last-value-wins by `seq`) and no acceptance box depends on it.

## 3. Folder structure map — file-location designations

Every deliverable's target path. `[C]` = already committed, listed for context; `[R]` = relocated verbatim; everything else is new in Phase 5.

### 3.1 Inside `IVI_ECU/`

```
IVI_ECU/
├── settings.gradle.kts                    [C] + include(":contract", ":serializer", ":observer", ":r4-simulator")
├── build.gradle.kts                       [C] + kotlin("jvm") and application plugin aliases (apply false)
├── gradle/libs.versions.toml              version catalog — single source of dependency versions (D8)
├── contracts/                             [C] r3/r4 schema copies, byte-synced — unchanged
│
├── contract/                              MODULE 1 — pure Kotlin/JVM, zero Android (D1)
│   ├── build.gradle.kts                   kotlin-jvm + serialization plugin; api(kotlinx-serialization-json)
│   └── src/
│       ├── main/kotlin/com/hackathon/v2x/ivi/model/
│       │   ├── R4Message.kt               [R] from app/src/main/java/... — verbatim
│       │   ├── R3Snapshot.kt              [R] verbatim
│       │   ├── SceneGeometry.kt           [R] verbatim
│       │   └── R4Contract.kt              KNOWN_SCHEMA_VERSION, sample resource paths, warning-registry keys
│       ├── main/resources/contracts/samples/
│       │   ├── r3-tracked-object.json     [R] byte-synced; manifest target repointed (D6)
│       │   ├── r4-warning.json            [R] byte-synced
│       │   ├── r4-state.json              [R] byte-synced
│       │   └── r4-unknown-warning.json    [R] byte-synced
│       └── test/kotlin/com/hackathon/v2x/ivi/model/
│           ├── R4RoundTripTest.kt         [R] must pass unchanged
│           └── R4AdditiveVersionTest.kt   [R] must pass unchanged
│
├── serializer/                            MODULE 2 — pure Kotlin/JVM (D3)
│   ├── build.gradle.kts                   deps: :contract
│   └── src/
│       ├── main/kotlin/com/hackathon/v2x/ivi/serializer/
│       │   ├── R4Decoder.kt               interface R4Decoder + sealed R4DecodeResult + enum DecodeFailure
│       │   ├── R4Deserializer.kt          impl: slice -> BOM/UTF-8 -> R4Json -> typed result; never throws
│       │   └── PayloadPreview.kt          bounded, single-line preview of bad bytes for the log
│       └── test/kotlin/com/hackathon/v2x/ivi/serializer/
│           ├── R4DeserializerTest.kt      every sample + every failure row of the decode table (I1)
│           └── BufferSlicingTest.kt       offset/length slicing, dirty backing array, BOM, whitespace
│
├── observer/                              MODULE 3 — pure Kotlin/JVM (D5)
│   ├── build.gradle.kts                   deps: :serializer, kotlinx-coroutines-core
│   └── src/
│       ├── main/kotlin/com/hackathon/v2x/ivi/observer/
│       │   ├── R4DatagramSource.kt        seam: bind/receive/close + Received(buffer, offset, length)
│       │   ├── JdkDatagramSource.kt       java.net impl; owns setLength(buffer.size) before every receive
│       │   ├── R4SocketObserver.kt        receive loop, truncation check, back-off, events + linkState
│       │   ├── R4Event.kt                 sealed: Message | Dropped;  R4LinkState: Bound|Rebinding|Error
│       │   ├── R4ObserverConfig.kt        port, bufferBytes, flow capacity, back-off bounds — no literals
│       │   └── R4Logger.kt                fun interface + NoopR4Logger (impl supplied by :app)
│       └── test/kotlin/com/hackathon/v2x/ivi/observer/
│           ├── R4SocketObserverTest.kt    fake source: N datagrams in -> N events out; failure -> Dropped
│           ├── LoopbackSocketTest.kt      real DatagramSocket on 127.0.0.1 (I2)
│           └── RetryBackoffTest.kt        error -> rebind, bounded exponential back-off, reset on success
│
├── app/                                   MODULE 4 — the front end; the only Android module
│   ├── build.gradle.kts                   [C] + module deps, new BuildConfig fields (D10), Hilt removed (D7)
│   ├── proguard-rules.pro                 [C] + keep rules for the relocated @Serializable models
│   └── src/
│       ├── main/AndroidManifest.xml       [C] + application android:name=".IviApplication";
│       │                                      MainActivity (LAUNCHER); R4ListenerService
│       │                                      (foregroundServiceType="connectedDevice"); POST_NOTIFICATIONS
│       ├── main/java/com/hackathon/v2x/ivi/
│       │   ├── IviApplication.kt           owns IviGraph + the application coroutine scope
│       │   ├── MainActivity.kt             Compose host; resolves launch-time config; starts the service
│       │   ├── di/IviGraph.kt              manual composition root + ViewModelProvider.Factory (D7)
│       │   ├── config/IviRuntimeConfig.kt  BuildConfig defaults merged with intent extras (D10)
│       │   ├── service/R4ListenerService.kt  foreground host: notification channel, start/stop observer
│       │   ├── service/AndroidR4Logger.kt    R4Logger -> Log(IVI_V2X) with the §5.4 line shapes
│       │   ├── data/R4Repository.kt          DATA: last warning, last state (LVW by seq), link state
│       │   ├── warning/WarningClassifier.kt  BUSINESS: warningType -> presentation (D4); risk normalisation
│       │   ├── ui/WarningUiState.kt          sealed: Idle | Active(scene, riskState, presentation)
│       │   ├── ui/WarningViewModel.kt        UI LOGIC: Idle<->Active + WARNING_TIMEOUT_MS auto-dismiss
│       │   ├── ui/MainViewModel.kt         [C] + wake-on-warning, previousMode restore, user-override flag
│       │   ├── ui/DisplayMode.kt           [C] unchanged
│       │   ├── ui/screen/MainScreen.kt     [C] + mount IviWarningViewSeam in the Display Area;
│       │   │                                   bottom status bar bound to link state; banner NOT mounted (D11)
│       │   └── ui/view/
│       │       ├── IviWarningViewSeam.kt   [C] unchanged
│       │       ├── SceneCoordinateMapper.kt[C] unchanged
│       │       ├── CanvasWarningView.kt    [C] + defaulted scaleMetersPerPixel constructor param (D10)
│       │       ├── WarningBannerOverlay.kt [C] stays unmounted (D11)
│       │       └── SceneViewWarning3D.kt   OPTIONAL, not an M1 deliverable — designated location only
│       ├── debug/java/com/hackathon/v2x/ivi/debug/
│       │   └── DevInjectorReceiver.kt      I3: adb-broadcast entry emitting a bundled sample onto the
│       │                                   same flow; debug source set only -> absent from release
│       └── test/java/com/hackathon/v2x/ivi/
│           ├── data/R4RepositoryTest.kt        routing, last-value-wins state
│           ├── warning/WarningClassifierTest.kt known vs unknown warningType (D4)
│           ├── ui/WarningViewModelTest.kt      Idle->Active->timeout->Idle
│           ├── ui/MainViewModelTest.kt         forced switch, restore, user-override
│           └── ui/view/SceneCoordinateMapperTest.kt  the committed pure-math layer, finally covered
│
├── r4-simulator/                          MODULE 5 — test equipment (D9)
│   ├── build.gradle.kts                   kotlin-jvm + application + serialization; deps: :contract
│   ├── Dockerfile                         multi-stage; build stage on $BUILDPLATFORM, runtime linux/arm64
│   ├── entrypoint.sh                      START_DELAY_S then run; blueprint command ["./entrypoint.sh"]
│   ├── scenarios/
│   │   ├── approach.json                  C approaching, risk low -> medium -> high; first step vehicleC null
│   │   ├── degrade.json                   unknown warningType + schemaVersion 2 + junk field; own_sensor; raw bytes
│   │   └── state-stream.json              optional R15 path: periodic state, ascending seq
│   └── src/
│       ├── main/kotlin/com/hackathon/v2x/ivi/sim/
│       │   ├── Main.kt                    host mode (args) and in-Room mode (env); rate loop; [TX] logging
│       │   ├── SimConfig.kt               IVI_ECU_HOST/PORT, R4_SCENARIO, R4_RATE_HZ, START_DELAY_S
│       │   ├── Scenario.kt                @Serializable scenario + step model
│       │   ├── ScenarioLoader.kt          file -> Scenario, with rejection messages
│       │   ├── SampleLibrary.kt           frozen samples off the :contract classpath (D6)
│       │   ├── MessageBuilder.kt          JsonElement overrides + validate through R4Json (D9)
│       │   └── UdpSender.kt               DatagramSocket send to the target
│       └── test/kotlin/com/hackathon/v2x/ivi/sim/
│           ├── ScenarioLoaderTest.kt      the three committed scenarios load; bad files rejected
│           ├── MessageBuilderTest.kt      every built payload decodes through R4Json (except raw steps)
│           └── ScenariosDifferTest.kt     different scenario files -> observably different streams
│
└── doc/
    ├── phase5-ivi-hld.md                  this document
    ├── phase5-ivi-components.puml         module map
    ├── phase5-ivi-callflow.puml           sequence diagram
    └── research_notes/                    [C] the four Phase 5 notes
```

### 3.2 Outside `IVI_ECU/` — five edits, each in its sanctioned home

| Path | Change | Why it cannot live in `IVI_ECU/` |
|---|---|---|
| [contracts/sync-manifest.json](../../contracts/sync-manifest.json) | Repoint 4 IVI sample targets to `IVI_ECU/contract/src/main/resources/contracts/samples/` | The manifest is the contract gate's normative home; an unregistered copy breaks `check_sync.py` |
| [.github/workflows/phase0-ci.yml](../../.github/workflows/phase0-ci.yml) | `ivi-unit-tests` job: extend the Gradle invocation (§6) | The lane originated in Phase 0, and that file's own rule is that a lane is maintained where it was created |
| `.github/workflows/phase5-ci.yml` | **New:** `ivi-assemble` (APK + lint) and `r4-sim-image` (arm64 build/push) | Same rule, other direction: lanes originating in Phase 5 get their own phase file |
| [requirements/car-sky-guide/node-ivi-ecu.md](../../requirements/car-sky-guide/node-ivi-ecu.md) | Extend § Post-deploy with the proven ADB route, the launch-override command, and the logcat filter | Per-node deploy steps live in the car-sky guide, per [node-code-layout.md](../../.claude/rules/node-code-layout.md) |
| `requirements/car-sky-guide/phase5-mini-blueprint-deploy.md` | **New:** clone-then-delete procedure, ADA-node simulator config, verification ladder — promoted from the research note | Same: deployment procedure, not node design |

**The stale plan's `deployment/phase5-ivi-deploy.md` is void** — a repo-root `deployment/` folder is not a sanctioned location. Nothing is written there.

## 4. MVC separation

| Layer | Where | Rule that keeps it separate |
|---|---|---|
| **Data** | `:contract` models + frozen samples; `:app data/R4Repository` (last warning, last `state` by `seq`, link state) | The repository stores and routes; it never decides what a warning *means* and never formats anything |
| **Business logic** | `:serializer` (bytes → typed), `:observer` (receive, retry, back-pressure), `:app warning/WarningClassifier`, `ui/view/SceneCoordinateMapper` | All four are free of Android UI types; three of them are free of Android entirely, and every one is unit-testable without a device |
| **UI logic** | `:app ui/WarningViewModel` (Idle↔Active, timeout), `ui/MainViewModel` (mode, wake-on-warning, restore), `config/IviRuntimeConfig` | View-models hold no drawing code and no socket; they translate domain state into what the Display Area should show |
| **UI** | `ui/screen/MainScreen`, `ui/view/CanvasWarningView` behind `IviWarningViewSeam` | The seam is the swap point for the optional 3D renderer; `MainScreen` never touches a message type, only `WarningUiState` |

No layer is collapsed: the socket loop cannot reach a Composable, and a Composable cannot reach a socket — the only path between them is `R4Event → R4Repository → WarningUiState`.

## 5. Module interfaces

### 5.1 `:serializer` — the decode entry point

```kotlin
interface R4Decoder {
    fun decode(buffer: ByteArray, offset: Int, length: Int): R4DecodeResult
}

sealed interface R4DecodeResult {
    data class Decoded(val message: R4Message, val schemaVersionAhead: Boolean) : R4DecodeResult
    data class Failed(val reason: DecodeFailure, val detail: String, val preview: String) : R4DecodeResult
}

enum class DecodeFailure { EMPTY, UNKNOWN_MESSAGE_TYPE, MALFORMED }
```

`R4Deserializer` is the implementation and also offers `decode(bytes)` and `decode(text)` convenience overloads for I1 tests. Mapping from kotlinx behaviour to results is the [parsing note §2](research_notes/phase5-r4-parsing.md) table verbatim: unknown fields and a newer `schemaVersion` succeed; an unknown `type` discriminator is `UNKNOWN_MESSAGE_TYPE`; malformed, truncated, wrong-typed and missing-required-field inputs are `MALFORMED`. **`isLenient` stays `false`** — leniency would hide producer bugs; the tolerance the contract asks for is `ignoreUnknownKeys`, a different switch that is already on. **No runtime JSON-Schema validation** is added: the typed decode already enforces required fields and types, and the schema is enforced where it belongs — in the round-trip tests on both sides.

### 5.2 `:observer` — the event source

```kotlin
class R4SocketObserver(
    private val config: R4ObserverConfig,
    private val decoder: R4Decoder,
    private val sourceFactory: () -> R4DatagramSource,
    private val logger: R4Logger,
) {
    val events: SharedFlow<R4Event>        // extraBufferCapacity = config.flowBufferEvents, DROP_OLDEST
    val linkState: StateFlow<R4LinkState>  // Bound | Rebinding | Error — drives the bottom status bar
    fun start(scope: CoroutineScope): Job
    fun stop()
}

sealed interface R4Event {
    data class Message(val message: R4Message, val receivedAtMs: Long, val bytes: Int) : R4Event
    data class Dropped(val reason: DecodeFailure, val detail: String, val bytes: Int) : R4Event
}
```

`sourceFactory` is what makes the loop testable: tests inject a fake source (I1-speed) or the real `JdkDatagramSource` on loopback (I2). `Dropped` is on the flow, not only in the log, so the UI can show a producer-fault count without the UI ever seeing bytes.

### 5.3 `:app` — the seams it fills

- `AndroidR4Logger : R4Logger` — the only bridge from module 3 to `android.util.Log`.
- `IviGraph` — constructs `IviRuntimeConfig → R4ObserverConfig → R4Deserializer → R4SocketObserver → R4Repository → view-model factory → CanvasWarningView(scale)`. One object, created once, owned by `IviApplication`.
- `R4Repository` collects `observer.events` on the **application** scope, not the service's, so a service restart cannot lose the last warning; it is also the single injection target for the dev injector (I3), which is why I3 exercises exactly the same downstream path as a real datagram.

### 5.4 Evidence log lines (R18 / R19)

One tag, `IVI_V2X`, so the demo's evidence is a single `adb logcat -s IVI_V2X`. One line per event, greppable, key=value:

```
[LINK] state=bound port=47300
[RX]   type=warning bytes=452 from=10.99.0.12:41234 warningType=nlos_obstruction risk=high cSource=v2x_relayed cPos=(35.0,0.0)
[RX]   type=state   bytes=210 seq=42
[DROP] reason=malformed bytes=17 preview="not-json…"
[UI]   mode=WarningView cause=warning
[UI]   mode=HomeView cause=timeout
```

`cSource=` on **every** rendered warning is what backs the R19 claim in text: the recording shows ghost C, the log proves every frame of it came from `v2x_relayed`.

## 6. Tech stack, build and CI

| Area | Stack | Trace |
|---|---|---|
| App language / UI | Kotlin 2.2.20, Jetpack Compose (BOM 2024.09.03), AndroidX, Material3 | report §3(e); R16/R17 tech stack |
| 2D renderer | Compose Canvas behind `IviWarningViewSeam`; SceneView/Filament optional | report §3(e), R17 |
| Contract binding | kotlinx.serialization-json | report R4 tech stack ("kotlinx.serialization (IVI side)"), D1 |
| Transport | `java.net.DatagramSocket`, UDP + versioned JSON | report §3(f), R6 |
| Concurrency | kotlinx-coroutines-core (`SharedFlow`/`StateFlow`) | D5 back-pressure policy |
| Build | Gradle multi-project, AGP 8.13, JDK 17, version catalog | D2, D8 |
| Tests | JUnit4 (+ kotlinx-coroutines-test in `:observer`); **no Robolectric** | D2 — the loop is plain JVM |
| Simulator image | multi-stage Docker; single-platform `linux/arm64`, `--provenance=false --sbom=false` | mini-blueprint note §4; the cluster rejects manifest indexes |

Build commands (from `IVI_ECU/`): `./gradlew assembleDebug` · `./gradlew :contract:test :serializer:test :observer:test :r4-simulator:test :app:testDebugUnitTest` · `./gradlew lint`.

### 6.1 The CI invocation that must change

**`ivi-unit-tests` in [phase0-ci.yml](../../.github/workflows/phase0-ci.yml) currently runs `./gradlew :app:testDebugUnitTest --no-daemon` and nothing else.** New modules' tests would pass locally and never run in CI. That job's command becomes:

```yaml
./gradlew :contract:test :serializer:test :observer:test :r4-simulator:test :app:testDebugUnitTest --no-daemon
```

Two new lanes go in a new `.github/workflows/phase5-ci.yml` (per that file's own placement rule — a lane lives with the phase that created it):

- **`ivi-assemble`** — `./gradlew assembleDebug lint`, uploading `app-debug.apk` as a run artifact so the ADB install step has a build to fetch. May need `android-actions/setup-android` if the runner image's SDK/licence state is insufficient.
- **`r4-sim-image`** — `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -f IVI_ECU/r4-simulator/Dockerfile -t $REGISTRY/m1-r4-sim:latest IVI_ECU/`, pushed to Zot and verified through the existing [verify-arm64-image](../../.github/actions/verify-arm64-image) action, exactly as the other node images are.

The simulator's Dockerfile **build context is `IVI_ECU/`** (it needs the Gradle wrapper, `settings.gradle.kts`, the catalog, `:contract` and `:r4-simulator`), with the Dockerfile itself at `r4-simulator/Dockerfile` rather than the folder root. This is a deliberate, flagged deviation from "own `Dockerfile` at the folder root" in [node-code-layout.md](../../.claude/rules/node-code-layout.md): this folder's primary artifact is an APK, and the image is secondary test equipment. Self-containment — the property the rule protects — is preserved: the build reads nothing outside `IVI_ECU/`. Because JVM bytecode is architecture-neutral, the build stage runs on `$BUILDPLATFORM` (no QEMU-emulated Gradle) and only the JRE runtime stage is `linux/arm64`.

## 7. Test strategy — the four injection points

Adopted from the [simulator note §1](research_notes/phase5-r4-simulator.md); each proves something the others cannot.

| # | Entry | Automated? | Module under test | Where it runs |
|---|---|---|---|---|
| **I1** | `R4Deserializer.decode(text/bytes)` and view-model inputs | **Yes** — must be | `:contract`, `:serializer`, classifier, view-models, mapper | `:*:test` / `:app:testDebugUnitTest` in CI |
| **I2** | Loopback socket to `127.0.0.1:<port>` | **Yes** — must be | `:observer` end to end, real `DatagramSocket` | `:observer:test`, plain JVM, no Robolectric |
| **I3** | `adb shell am broadcast -a com.hackathon.v2x.ivi.DEV_INJECT --es sample r4-warning` | Manual | Everything above **plus the real UI** | Device/emulator, no network |
| **I4** | Real UDP from the simulator | Manual | The whole path — R6 hop 3, R16/R17 acceptance | AAOS emulator (dev) / mini-blueprint Room (evidence) |

I3 exists because the AAOS guest may be hard to reach over the network before the ADB route is proven; it keeps UI work unblocked. **It must be excluded from the release build by source set** — a release path that can fabricate a warning would undermine the R19 claim that C came only from relayed data. Getting a datagram into an emulator for I4 is the one awkward step: `adb forward` cannot do it (TCP only); use emulator console `redir add udp:47300:47300`, or simply send from inside the guest. In the Room none of this applies — the ADA node and the guest share the bridge subnet.

## 8. Deployment shape (R5/R6)

- **Blueprint:** the 3-node mini-blueprint — Ethernet Bridge + ADA node running the simulator image + IVI Skycraft node — created by **cloning the baseline and deleting two nodes**, because neither REST nor JSON import can create `ethernet` pins. The IVI node's `image` block (artifact `AAOS`, `x9oqgIwzTp1m26SWIQqJt` / `xSU_Q7YJZUxxUgDr4Ugcp`, `aarch64`) is left untouched or the deploy is rejected.
- **The simulator honours the real ADA node's env var *names*** — `IVI_ECU_HOST`, `IVI_ECU_PORT` — so Phase 6 is an image swap with no node-config edit. `R4_SCENARIO`, `R4_RATE_HZ` and `START_DELAY_S` are simulator-only additions; `START_DELAY_S` exists because the AAOS guest boots slower than a container.
- **The APK is not baked into the VM image**: `adb connect <skycraft-adb-endpoint>` then `adb install -r app-debug.apk` after the node reaches Running. **Prove this route early** — it is unverified on this deployment and the platform's REST VM-shell route answers 502. If it is unreachable, every in-Room criterion degrades to emulator evidence, and discovering that late costs days.
- **The port is 47300**, frozen in the blueprint, and the app binds `0.0.0.0` — the node address is bridge-assigned and must never be hardcoded.

## 9. Contradictions found, and how they are resolved

### 9.1 The stale Phase 5 task file

[plans/phase5_tasks.md](../../plans/phase5_tasks.md) (last updated 2026-07-24) predates the frozen contract, the mini-blueprint and this design. Four of its statements are wrong against committed artifacts and are superseded here:

| Stale statement | Correct value | Authority |
|---|---|---|
| `4.5.1.2`: unknown `warningType` → `"unknown"` | Value preserved verbatim; classified at the UI edge | Committed `R4AdditiveVersionTest`; D4 |
| `4.5.1.3`: `BuildConfig.R4_UDP_PORT` default `5004` | `47300` | Blueprint/R6 topology, node guide |
| `4.5.1.5`: `:mock-sender` module writing its own messages | `:r4-simulator`, payloads built from the frozen samples | D9 |
| `16.5.2.5`: `deployment/phase5-ivi-deploy.md` | `requirements/car-sky-guide/` (§3.2) | node-code-layout.md |

Also noted, not adopted: `17.5.4.2`'s "code coverage ≥ 80 %" and `16.5.4.1`'s LeakCanary requirement have no basis in R4/R16/R17 acceptance and add tooling for it; the acceptance boxes are behavioural.

### 9.2 Against the committed code

- `R4WarningEvent.WARNING_TYPE_NLOS_OBSTRUCTION` is the only registry entry, and `CanvasWarningView.riskColor` already treats an **unknown risk state as highest urgency** (fail-safe). `WarningClassifier` must not contradict that: unknown `riskState` keeps the high-urgency presentation.
- `SceneGeometry.vehicleCSnapshot` is populated by the R4 message's `object` field — the renderer's source guard is only armed if the view-model **fills it** when composing the scene. A view-model that passes `SceneGeometry` straight from `geometry` (where the snapshot is `null`) would silently disable the R19 guard, because a `null` snapshot is treated as trusted. **The composition step must copy `objectSnapshot` into `vehicleCSnapshot`.** This is the single most easily-missed wiring detail in the phase.
- `MainScreen`'s bottom bar shows a hardcoded `"V2X LINK: STANDBY"`; it is bound to `R4LinkState` when the front end is wired.
- `AndroidManifest.xml` already declares `FOREGROUND_SERVICE` and comments `R4ListenerService (4.5.1.3)`, but declares no `<activity>`, no `<service>` and no application class — the APK installs and cannot be started.

## 10. Acceptance traceability

| [Phase 5 acceptance](../../plans/milestone1.md#phase-5--ivi-hmi-mock-driven-r16-r17--display-track-parallel-from-the-start) | Closed by |
|---|---|
| HMI runs on the AAOS node with the R16 layout; button/app areas switch the Display Area | `MainActivity` + `MainScreen` (committed layout) + `MainViewModel`; installed via §8 |
| **(Dev)** A mock R4 warning brings the Warning View up with ego, B and ghost C | Simulator `approach.json` → I3/I4 → wake-on-warning (D5, `MainViewModel`) → `CanvasWarningView` |
| Ghost C renders from `v2x_relayed` only; 2D delivered | Committed source guard + §9.2's snapshot-wiring rule + `[RX] cSource=` log line |
| Unknown `warningType` degrades gracefully | D4 + committed `R4AdditiveVersionTest` (relocated, unchanged) + simulator `degrade.json` |
| Optional paths, only if built | `SceneViewWarning3D` location designated; multi-process left reachable by D5 |

## 11. Open items and risks

| # | Item | Impact / owner |
|---|---|---|
| 1 | **ADB reach to the Skycraft guest is unverified** (REST VM-shell route is known-502). | No `adb install` ⇒ every in-Room criterion falls back to emulator evidence. Verify **before** the APK is finished — project-planner should schedule it as an early, parallel task. |
| 2 | **AAOS guest Android version unknown**; the APK targets `minSdk 29`. | A guest below API 29 rejects the APK outright. Same early check as #1. |
| 3 | Simulator `Dockerfile` sits at `r4-simulator/`, not the node-folder root (§6.1). | Flagged deviation with rationale; self-containment preserved. Revisit only if `IVI_ECU/` ever gains a root image. |
| 4 | Hilt removal (D7) contradicts the stale plan's `16.5.4.1`. | Decided here; if reversed, only `IviGraph` changes. |
| 5 | Deployment budget: 2 concurrent Rooms. | Tear the Phase 5 Room down before the comms track needs a second. |
| 6 | Coroutines version must be aligned across `:observer` and what AndroidX resolves in `:app`. | The version catalog (D8) is the mitigation; a skew shows up as a runtime `NoSuchMethodError`, not a build failure. |
