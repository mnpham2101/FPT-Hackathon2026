# Phase 5 HLD — IVI ECU display track (R4 consumer, R16, R17)

> High-level design for [milestone1.md § Phase 5](../../plans/milestone1.md) (IVI HMI, mock-driven), per [hld-content-and-commit-format.md](../../.claude/rules/hld-content-and-commit-format.md). Requirement definition and stack: [m1-cooperative-awareness.md](../../requirements/m1-cooperative-awareness.md) R4 (consumer), R16, R17; §3(e). Researcher 1st-choice: [phase5-ivi-mini-research.md](../../requirements/phase5-ivi-mini-research.md). Call-flow: [phase5-ivi-ecu-callflow.puml](phase5-ivi-ecu-callflow.puml). Components: [phase5-ivi-ecu-components.puml](phase5-ivi-ecu-components.puml).
>
> **Greenfield module boundaries** — this HLD designates the intended architecture and target paths under `IVI_ECU/`; existing sources that already match these paths are treated as aligned implementations of the same design, not as authority over it.
>
> **Commit deferred** — design files only in this run; no `[X] design:` commit until explicitly requested.

## 1. Sourced research notes

| Note | Adopted |
|---|---|
| [phase5-ivi-mini-research.md](../../requirements/phase5-ivi-mini-research.md) | Four-module 1st-choice (Kotlin contract, UDP adapter, SharedFlow observer, Compose frontend); nlohmann rejected on IVI; raw UDP JSON framing. |
| [phase5-r4-parse-approach.md](research_notes/phase5-r4-parse-approach.md) | Parse pipeline; sealed `R4Message`; additive-version degrade; optional `:r4-contract` (Kotlin only); no M1 Ethernet app header. |
| [phase5-r4-simulation-harness.md](research_notes/phase5-r4-simulation-harness.md) | Python `mock-sender/` as Phase 5 producer; invocation modes; expected HMI reactions per packet type. |
| [phase5-ivi-implementation-notes.md](research_notes/phase5-ivi-implementation-notes.md) | Ports 5004 vs 47300; BuildConfig tunables; R16/R17 HMI rules; Skycraft + ADB install constraints; module→MVC map. |
| [phase5-mini-blueprint-ada-ivi.md](research_notes/phase5-mini-blueprint-ada-ivi.md) | Optional ADA+IVI+bridge Room on 47300; ADA-without-V2X gap (a/b/c). |
| [task51-2node-blueprint-answer.md](../../plans/doc/task51-2node-blueprint-answer.md) | Existing mock 2-node topology and cycle script as the sanctioned display-track Room path. |
| [node-ivi-ecu.md](../../requirements/car-sky-guide/node-ivi-ecu.md) | Skycraft artifact IDs, ethernet pin `10.99.0.13`, post-deploy ADB install (APK not in image). |

## 2. Design decisions

### D1 — IVI JSON parse: **kotlinx.serialization**; **nlohmann/json stays on ADA only**

- **Pick:** Kotlin models + `kotlinx.serialization` deserializer (report R4 tech stack: “nlohmann/json (ADA side); kotlinx.serialization (IVI side)”).
- **Reject for IVI:** nlohmann/json as a Gradle/native submodule, JNI bridge, or shared C++ parser inside `IVI_ECU/` — wrong AAOS runtime, duplicates ADA’s binding, violates R4’s per-side stack split (researcher criteria + [phase5-r4-parse-approach.md](research_notes/phase5-r4-parse-approach.md)).
- **Shared truth** is [contracts/r4-ada-ivi.schema.json](../../contracts/r4-ada-ivi.schema.json) (+ fixtures under `contracts/samples/`), not a shared C++ library.
- **Optional:** extract models + deserializer into Gradle submodule `:r4-contract` **inside** `IVI_ECU/` only (`settings.gradle.kts` `include(":r4-contract")`). Default for M1: keep packages under `:app` (`model/`, `data/R4Deserializer.kt`) until a second JVM consumer appears; submodule is a non-blocking refactor, not a Phase 5 gate.

### D2 — UDP framing: **raw JSON bytes; no custom Ethernet application header**

- Datagram payload = UTF-8 JSON object bytes (`DatagramPacket.data[0..length)`).
- M1 frozen R4 contract has no app-level Ethernet header to strip; do not invent one on the IVI path.
- Bridge remains L2 fabric only; listen port is app config (D4), not a bridge property.

### D3 — Four modules → MVC (independent seams)

| Module (researcher name) | MVC layer | Responsibility | Primary paths |
|---|---|---|---|
| **Contract / parse library** | Data | R4 (+ embedded R3) models; deserialize to sealed `R4Message`; additive-version degrade | `app/.../model/`, `app/.../data/R4Deserializer.kt` (or `:r4-contract`) |
| **UDP / payload adapter** | Controller edge (I/O) | Foreground service; bind port; IO-thread receive; bytes → string → deserialize; emit domain events | `app/.../service/R4ListenerService.kt` |
| **Observer** | Business / domain event bus | Route warning vs state; `SharedFlow` warnings; last-value-wins `StateFlow` for state | `app/.../data/R4Repository.kt` |
| **Frontend** | UI logic + UI | R16 shell; wake-on-warning; `IviWarningViewSeam` + Compose Canvas R17 2D; Hilt | `app/.../ui/**`, `di/AppModule.kt` |

No module may collapse parse into the Compose layer or draw Canvas from the service thread.

### D4 — Ports and tunables: **BuildConfig / env; never literals in logic**

| Context | Port | Who sets it |
|---|---|---|
| Phase 5 mock / 2-node Room | **5004** | `BuildConfig.R4_UDP_PORT` default; mock-sender `IVI_ECU_PORT` |
| Production / mini ADA+IVI / full M1 | **47300** | Same BuildConfig field (flavor or field override); ADA `IVI_ECU_PORT` |

- Warning auto-clear: `BuildConfig.WARNING_TIMEOUT_MS`.
- Align producer and consumer ports before claiming Room integration ([phase5-ivi-implementation-notes.md](research_notes/phase5-ivi-implementation-notes.md)).

### D5 — R17 view seam: **2D Canvas is M1; 3D optional behind same interface**

- `IviWarningViewSeam.Render(scene, riskState)` — UI-framework-light seam; M1 impl = `CanvasWarningView`.
- Ghost C only when `object.source == v2x_relayed`; otherwise visible guard, not silent accept.
- Ego-relative meters; `ego` at `(0,0)`; `vehicleC` nullable.

### D6 — Phase 5 producer: **Python `IVI_ECU/mock-sender/`**

- Sanctioned test equipment for display-track parallel work (not a mock to eliminate later — Phase 6 points IVI at live ADA).
- Cycles: approach warnings → state → leave → unknown `warningType` (additive-version).
- Real ADA emission is out of IVI’s code scope; optional mini-blueprint flagged in §10.

## 3. Folder structure map — file-location designations

Intended layout (paths are the designations implementers use; stubs not required when files already exist):

```
IVI_ECU/
├── settings.gradle.kts                 # include(":app"); optional include(":r4-contract") (D1)
├── build.gradle.kts                    # root plugins / versions
├── gradlew · gradlew.bat
├── app/
│   ├── build.gradle.kts                # BuildConfig.R4_UDP_PORT, WARNING_TIMEOUT_MS; kotlinx + Compose + Hilt
│   └── src/
│       ├── main/
│       │   ├── AndroidManifest.xml     # MainActivity; foreground R4ListenerService; INTERNET
│       │   ├── res/                    # themes, network_security_config
│       │   └── java/com/hackathon/v2x/ivi/
│       │       ├── IviApplication.kt   # @HiltAndroidApp
│       │       ├── MainActivity.kt
│       │       ├── model/              # DATA — contract models (or live in :r4-contract)
│       │       │   ├── R4Message.kt    # sealed: warning | state | service error
│       │       │   ├── R3Snapshot.kt   # embedded R3 on warning.object
│       │       │   └── SceneGeometry.kt
│       │       ├── data/
│       │       │   ├── R4Deserializer.kt   # DATA — kotlinx parse (D1)
│       │       │   └── R4Repository.kt     # OBSERVER — SharedFlow / StateFlow (D3)
│       │       ├── service/
│       │       │   └── R4ListenerService.kt  # UDP ADAPTER — Dispatchers.IO (D2)
│       │       ├── di/
│       │       │   └── AppModule.kt    # Hilt bindings
│       │       └── ui/                 # FRONTEND
│       │           ├── DisplayMode.kt
│       │           ├── WarningUiState.kt
│       │           ├── MainViewModel.kt      # UI logic — R16 mode / Display Area
│       │           ├── WarningViewModel.kt   # UI logic — wake, timeout, risk map
│       │           ├── screen/MainScreen.kt  # R16 shell
│       │           └── view/
│       │               ├── IviWarningViewSeam.kt   # R17 seam (D5)
│       │               ├── CanvasWarningView.kt    # R17 2D M1
│       │               ├── SceneCoordinateMapper.kt
│       │               └── WarningBannerOverlay.kt
│       └── test/java/com/hackathon/v2x/ivi/
│           ├── R4DeserializerTest.kt          # fixtures + additive-version
│           ├── model/R4RoundTripTest.kt
│           ├── model/R4AdditiveVersionTest.kt
│           ├── R4RepositoryTest.kt
│           ├── WarningViewModelTest.kt
│           ├── ui/MainViewModelTest.kt
│           ├── ui/view/CanvasWarningViewTest.kt
│           ├── ui/view/SceneCoordinateMapperTest.kt
│           └── integration/FullStackIntegrationTest.kt
├── r4-contract/                        # OPTIONAL (D1) — Kotlin JVM/Android library module
│   └── src/main/kotlin/.../            # move model/ + R4Deserializer here if extracted
├── mock-sender/                        # D6 harness (Python; own Dockerfile)
│   ├── mock_r4_sender.py
│   ├── Dockerfile
│   └── README.md
├── deployment/
│   └── phase5-ivi-deploy.md            # APK build + ADB install smoke
└── doc/
    ├── research_notes/                 # sourced above
    ├── phase5-ivi-ecu-hld.md           # this document
    ├── phase5-ivi-ecu-callflow.puml
    └── phase5-ivi-ecu-components.puml
```

**Outside this folder (referenced, not owned):** schema/fixtures in `contracts/`; blueprints/guides under `requirements/` and `requirements/car-sky-guide/`.

## 4. Tech stack

| Area | Stack | Trace |
|---|---|---|
| Language / UI | Kotlin, Jetpack Compose, AndroidX, Material3 | report §3(e), R16 |
| JSON (IVI) | kotlinx.serialization | report R4 IVI side; D1 |
| JSON (ADA producer) | nlohmann/json | report R4 ADA side — **not** linked into IVI |
| Concurrency | kotlinx-coroutines (`Dispatchers.IO` receive; Main for UI collect) | implementation notes |
| DI | Hilt | researcher frontend pick |
| R17 2D | Compose Canvas behind `IviWarningViewSeam` | report R17 |
| R17 3D | SceneView/Filament optional / stub | report §4 optional |
| Harness | Python 3 + stdlib UDP (`mock-sender/`) | D6; research harness note |
| Artifact | Debug/release APK via `IVI_ECU/gradlew`; minSdk 29 / targetSdk 33 | node guide; Skycraft AAOS |
| Deploy | CarSky Skycraft node + post-Running ADB install | R5; node-ivi-ecu.md |

## 5. Configuration

| Knob | Home | Default / notes |
|---|---|---|
| `R4_UDP_PORT` | `app/build.gradle.kts` → `BuildConfig` | `5004` Phase 5 mock; override to `47300` for production / mini ADA Room |
| `WARNING_TIMEOUT_MS` | `BuildConfig` | e.g. `10000L` — auto-clear Warning View |
| `IVI_ECU_HOST` / `IVI_ECU_PORT` | mock-sender env (and ADA env on live path) | must match IVI listen port |
| `INTERVAL_MS` / `CYCLES` / `SCHEMA_VERSION` | mock-sender env | harness pacing only |

No proximity, port, or timeout literals in ViewModel / service / repository logic.

## 6. Call flow

[phase5-ivi-ecu-callflow.puml](phase5-ivi-ecu-callflow.puml) — PlantUML sequence: ADA/mock → R6 bridge → UDP service → kotlinx deserialize → repository flows → ViewModels → R16 shell + R17 Canvas; branches for unknown `warningType`, malformed payload, and auto-clear.

## 7. MVC mapping

- **Data** — `model/*`, `R4Deserializer`, contract schema/fixtures (repo `contracts/`); optional `:r4-contract`.
- **Business logic** — `R4Repository` routing (warning edge vs state last-value-wins); ghost-C source guard policy consumed by UI logic; no Compose imports.
- **UI logic** — `MainViewModel`, `WarningViewModel` (DisplayMode, wake-on-warning, timeout restore, UiState mapping).
- **UI** — `MainScreen` (R16 layout), `IviWarningViewSeam` + `CanvasWarningView` / banner (R17). Transport service is a controller I/O edge, not UI.

## 8. Deployment shape (R5 / Phase 5)

| Path | Topology | Port | Guide |
|---|---|---|---|
| Local / emulator | mock-sender → `127.0.0.1` | 5004 | [mock-sender/README.md](../mock-sender/README.md) |
| Mock 2-node Room | `m1-mock-r4-sender` + Skycraft IVI + bridge | 5004 (align both) | [task51-2node-blueprint-answer.md](../../plans/doc/task51-2node-blueprint-answer.md), [blueprint-2node-task51-test-guide.md](../../requirements/blueprint-2node-task51-test-guide.md) |
| Mini ADA+IVI (optional) | ADA + IVI + bridge | **47300** | [phase5-mini-blueprint-ada-ivi.md](research_notes/phase5-mini-blueprint-ada-ivi.md) — see §10 gap |
| Full M1 | 4 nodes + bridge | 47300 | [node-ivi-ecu.md](../../requirements/car-sky-guide/node-ivi-ecu.md) |

Post-deploy always: build APK → ADB tunnel to Skycraft → `adb install` ([phase5-ivi-deploy.md](../deployment/phase5-ivi-deploy.md)). Ethernet pins often missing after JSON import — add/wire in Nydus UI.

## 9. Acceptance traceability

| [Phase 5 acceptance](../../plans/milestone1.md) | Closed by |
|---|---|
| HMI on AAOS with R16 layout; button/app areas switch Display area | D3 Frontend + `MainScreen` / `DisplayMode` |
| (Dev) Mock R4 warning → Warning View with ego, B, ghost C | D6 harness + call-flow warning path + R17 Canvas |
| Ghost C from `v2x_relayed` only; 2D delivered | D5 seam + source guard |
| Unknown `warningType` degrades (R4 additive-version) | D1 deserializer + unit tests on fixtures |
| UDP ingest of R4 | D2 service + D4 port config |

## 10. Open items & flags

| # | Item | Owner / closes at |
|---|---|---|
| 1 | **nlohmann on IVI rejected (D1)** — document for planner: do not spawn JNI/nlohmann subtasks under `IVI_ECU/`; ADA keeps nlohmann. | closed by this HLD; planner briefs must cite D1 |
| 2 | **Port 5004 vs 47300** — Phase 5 default BuildConfig stays 5004 for mock; production/mini-ADA Rooms require an explicit BuildConfig/flavor switch to 47300 and matching producer env. | planner: separate config/deploy subtasks; do not hardcode either in Kotlin logic |
| 3 | **Mini-blueprint ADA-without-V2X gap** — stock ADA expects R2 on `V2X_LISTEN_PORT`; ADA+IVI-only Room needs (a) R2 injector, (b) ADA fixture/test emit mode, or (c) keep mock-sender until Phase 4 emission is fixture-triggerable. Do not claim ADA↔IVI E2E without one of (a)–(c). | planner + ADA track; not an IVI code task |
| 4 | Optional `:r4-contract` extract — non-gating; only if planner wants a dedicated library module commit. | optional later subtask |
| 5 | Optional 3D / multi-process wake — report §4 optional; out of M1 Phase 5 committed scope. | future / timebox only |

## 11. Subagent shape (for planner spawn specs)

| Subagent focus | Writes | Does not touch |
|---|---|---|
| IVI contract/parse | `model/`, `R4Deserializer`, round-trip + additive tests | Compose screens, ADA C++ |
| IVI UDP service | `R4ListenerService`, manifest service entry, IO loop | Canvas drawing |
| IVI observer + VM | `R4Repository`, ViewModels, Hilt module | native/JNI |
| IVI frontend R16/R17 | `MainScreen`, seam, Canvas, mapper, banner | UDP socket code |
| IVI harness/deploy | `mock-sender/` env docs, deploy smoke checklist | ADA image internals |
