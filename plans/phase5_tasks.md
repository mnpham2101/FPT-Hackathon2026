# Phase 5 — IVI HMI & Warning View: Full Task Breakdown (R4, R16, R17)

> **Authority & Context:**
> - **Milestone 1 Plan:** [plans/milestone1.md](milestone1.md) → Phase 5 section
> - **Requirements Report:** [requirements/m1-cooperative-awareness.md](../requirements/m1-cooperative-awareness.md) (R4, R16, R17)
> - **Task Planning Rules:** [.claude/rules/task-planning-conventions.md](../.claude/rules/task-planning-conventions.md) (`X.Y.Z.W`)
> - **HMI Layout Reference:** [requirements/ivi-ecu.svg](../requirements/ivi-ecu.svg)
>
> **Task ID Legend:** `X.Y.Z.W` — X = Requirement | Y = Phase | Z = Task Group | W = Atomic Subtask

---

## 📌 Phase 5 Overview

Phase 5 builds the **IVI HMI app on Android Automotive OS (AAOS)** running on the provided Skycraft node.
It is the **Display Track** — developed entirely against mock R4 data, then wired to real data in Phase 6.

| Requirement | Description |
|---|---|
| **R4** | ADA → IVI versioned JSON warning message (contract); edge-triggered warning event + optional periodic state |
| **R16** | AAOS HMI layout: central Display area + surrounding buttons; R4 warning activates Warning View |
| **R17** | Warning View: 2D God-View Canvas of Ego (A), Occluder (B), Ghost C (`v2x_relayed`); 3D optional |

**Tech Stack:** Kotlin · Jetpack Compose · AndroidX · Compose Canvas 2D · kotlinx.serialization · UDP foreground service

**Parallelism:** Phase 5 runs in parallel with Phases 2–4 (ADA track). All it needs from other tracks is:
- **R4 JSON schema** (from Phase 0 — already frozen)
- **Mock R4 generator** (built inside this phase, subtask 5.1.3)

**Phase 5 is done** when all committed acceptance criteria below are checked:
- [ ] HMI runs on AAOS node; layout matches R16 (Display Area + buttons)
- [ ] Incoming mock R4 warning event activates Warning View
- [ ] 2D Canvas shows Ego, B, Ghost C with correct relative geometry
- [ ] Ghost C is labeled `v2x_relayed` — no direct detection data used
- [ ] Unknown `warningType` degrades gracefully (additive-version test)

---

## 📦 Task Group 5.1 — Data Layer: R4 Ingest Service & Contract (Serves R4)

> **Goal:** Build a reliable UDP receiver that deserializes R4 JSON packets into Kotlin models, flows them to the UI layer, and can tolerate schema evolution (unknown fields, new warning types).

---

### `4.5.1.1` — Define R4 Kotlin Data Models

**Objective:** Create typed Kotlin data classes matching the R4 contract schema for both the `warning` event and the optional `state` message. This is the canonical model layer for the whole IVI app.

**Scope:**
- `R4Message.kt` — sealed class: `R4WarningEvent` | `R4StateMessage`
- `R4WarningEvent` fields: `schemaVersion`, `type`, `warningType`, `riskState`, `object: R3Snapshot`, `geometry: SceneGeometry`
- `R4StateMessage` fields: `schemaVersion`, `type`, `seq`, `vehicles: VehiclePoses`
- `R3Snapshot` fields matching R3 schema: `id`, `source` (`own_sensor` | `v2x_relayed`), `position.x`, `position.y`, `distance`, `speed`, `confidence`, `state`
- `SceneGeometry` fields: `ego`, `vehicleB`, `vehicleC` relative positions (x, y, distance in meters)
- All fields use `@Optional` / `@SerialName` from kotlinx.serialization

**Acceptance Criteria:**
- [ ] All model classes compile with zero warnings
- [ ] `@Serializable` annotations applied — round-trip encode/decode works in unit test
- [ ] No direct dependency on any UI or Android framework (pure Kotlin model layer)

**Files:** `app/src/main/java/com/hackathon/v2x/ivi/model/R4Message.kt`

**Commit:** `[4.5.1.1] feat: define R4 Kotlin sealed models with kotlinx.serialization`

---

### `4.5.1.2` — Implement R4 JSON Deserializer with Additive-Version Safety

**Objective:** Build a deserializer that safely parses incoming R4 JSON bytes into `R4Message` sealed types. Unknown `warningType` values must degrade gracefully to a generic warning — never crash.

**Scope:**
- `R4Deserializer.kt` — accepts raw `ByteArray`, returns `Result<R4Message>`
- Unknown `type` field → `Result.failure(UnknownMessageTypeException)` (caller logs and skips)
- Unknown `warningType` → parsed as `warningType = "unknown"` without exception (additive-version contract)
- Unknown extra JSON fields → ignored silently (lenient mode)
- Malformed JSON → `Result.failure(MalformedJsonException)` with byte offset in message
- Log every parse failure at WARN level with first 256 bytes of the bad payload

**Acceptance Criteria:**
- [ ] Unit test: valid `warning` event → `R4WarningEvent` with all fields populated
- [ ] Unit test: valid `state` message → `R4StateMessage` with all fields populated
- [ ] Unit test: `warningType = "future_type"` → parses as `warningType = "unknown"`, no exception
- [ ] Unit test: malformed JSON → `Result.failure(...)`, no crash
- [ ] Unit test: extra unknown JSON fields → ignored, remaining fields correct

**Files:** `app/src/main/java/com/hackathon/v2x/ivi/data/R4Deserializer.kt`

**Commit:** `[4.5.1.2] feat: implement R4 JSON deserializer with additive-version safety`

---

### `4.5.1.3` — Implement UDP Listener Foreground Service

**Objective:** Build an Android Foreground Service that opens a non-blocking UDP socket, receives raw R4 packets on the R6 network port, and emits deserialized events to an internal `SharedFlow`.

**Scope:**
- `R4ListenerService.kt` — `ForegroundService` with binding lifecycle
- Opens `DatagramSocket` on configurable port (read from `BuildConfig.R4_UDP_PORT`, default `5004`)
- Non-blocking receive loop on a dedicated `Dispatchers.IO` coroutine
- Passes raw bytes to `R4Deserializer` → emits `R4Message` on `_r4EventFlow: MutableSharedFlow<R4Message>`
- Reconnects automatically on socket error (with 1-second back-off, max 5 retries)
- Exposes `r4EventFlow: SharedFlow<R4Message>` as public API
- Service started at app launch; shows persistent notification (required for Android foreground service)
- Port is externalized in `BuildConfig` — no hardcoded literals

**Acceptance Criteria:**
- [ ] Service starts on app launch and binds within 2 seconds
- [ ] Unit test (Robolectric): 5 UDP packets sent to loopback → 5 events emitted on flow
- [ ] Socket error → service retries; 5 consecutive errors → emits `ServiceErrorEvent`
- [ ] `BuildConfig.R4_UDP_PORT` drives the port — no literal `5004` in source

**Files:**
- `app/src/main/java/com/hackathon/v2x/ivi/service/R4ListenerService.kt`
- `app/src/main/res/xml/network_security_config.xml`

**Commit:** `[4.5.1.3] feat: implement R4 UDP foreground service with SharedFlow emission`

---

### `4.5.1.4` — Build R4 Repository & WarningViewModel Bridge

**Objective:** Create the `R4Repository` that collects raw service events and the `WarningViewModel` that exposes `StateFlow`s for the UI layer. This is the single source of truth for warning state across the whole app.

**Scope:**
- `R4Repository.kt`: collects `R4ListenerService.r4EventFlow`, buffers last `state` message (last-value-wins per R4 spec), emits:
  - `warningEvents: SharedFlow<R4WarningEvent>`
  - `currentState: StateFlow<R4StateMessage?>`
- `WarningViewModel.kt`:
  - `uiWarningState: StateFlow<WarningUiState>` — `Idle` | `Active(event: R4WarningEvent)` | `Error`
  - `latestScene: StateFlow<SceneGeometry?>` — current composed geometry for the canvas
  - Clears `Active` state after configurable timeout (default 10 s, from `BuildConfig.WARNING_TIMEOUT_MS`)
- All flows consumed via `viewModelScope`; no coroutine leaks

**Acceptance Criteria:**
- [ ] Unit test: repository emits `warningEvent` on `warning` type, updates `currentState` on `state` type
- [ ] Unit test: ViewModel transitions `Idle → Active` on incoming warning event
- [ ] Unit test: ViewModel transitions `Active → Idle` after timeout with no new event
- [ ] No Android UI imports in `R4Repository` (pure logic layer)
- [ ] `BuildConfig.WARNING_TIMEOUT_MS` drives the timeout — no literal `10000`

**Files:**
- `app/src/main/java/com/hackathon/v2x/ivi/data/R4Repository.kt`
- `app/src/main/java/com/hackathon/v2x/ivi/ui/WarningViewModel.kt`
- `app/src/main/java/com/hackathon/v2x/ivi/ui/WarningUiState.kt`

**Commit:** `[4.5.1.4] feat: add R4Repository and WarningViewModel StateFlow bridge`

---

### `4.5.1.5` — Build Mock R4 Event Generator (Dev/Test Harness)

**Objective:** Standalone Kotlin binary that sends scripted R4 JSON packets to the IVI app over UDP loopback. Allows full UI development and testing without a live ADA ECU.

**Scope:**
- `MockR4Sender.kt` in a dedicated `:mock-sender` Gradle module
- Sends a sequence of events: C approaching (distance 28 m → `nlos_obstruction` warning), C leaving range, C re-entering
- Also sends one message with `warningType = "future_unknown"` to verify the additive-version path in UI
- Configurable via CLI args: `--ip`, `--port`, `--interval-ms`, `--cycles`
- Prints each sent packet to stdout with timestamp

**Acceptance Criteria:**
- [ ] Running the sender → IVI app receives and renders warning in Warning View
- [ ] Unknown `warningType` message → app does not crash; logs WARN; renders generic warning
- [ ] Sender exits cleanly with code 0 after completing all cycles

**Files:** `mock-sender/src/main/kotlin/com/hackathon/v2x/mock/MockR4Sender.kt`

**Commit:** `[4.5.1.5] test: add standalone mock R4 UDP event generator for dev harness`

---

## 📦 Task Group 5.2 — AAOS HMI Scaffold & View Switcher (Serves R16)

> **Goal:** Build the main AAOS screen matching the R16 layout: central Display Area + functional button zones. Any incoming R4 warning automatically switches Display Area to Warning View.

---

### `16.5.2.1` — Configure Android Automotive OS Gradle Module

**Objective:** Set up the Gradle build for a valid AAOS app — correct SDK levels, AAOS feature declarations, manifest permissions needed for the Skycraft node.

**Scope:**
- `build.gradle.kts`: `minSdk 29`, `targetSdk 33`, AAOS manifest declarations, kotlinx.serialization plugin
- `AndroidManifest.xml`: `<uses-feature android:name="android.hardware.type.automotive"/>`, `FOREGROUND_SERVICE` and `INTERNET` permissions
- `network_security_config.xml`: cleartext allowed for ADA ECU subnet (CarSky bridge network)
- `proguard-rules.pro`: keep kotlinx.serialization classes
- Hilt dependency added

**Acceptance Criteria:**
- [ ] `./gradlew assembleDebug` succeeds with zero errors
- [ ] `./gradlew lint` — zero `Error` severity issues
- [ ] App installable on Android Automotive emulator (API 29+)

**Files:** `app/build.gradle.kts`, `app/src/main/AndroidManifest.xml`

**Commit:** `[16.5.2.1] chore: configure AAOS Gradle module, manifest, and network security`

---

### `16.5.2.2` — Build R16 Main Layout Scaffold (Compose)

**Objective:** Implement the HMI screen matching the R16 layout reference (`ivi-ecu.svg`): a central Display Area slot surrounded by areas for functional buttons and app tiles.

**Scope:**
- `MainScreen.kt` — top-level `@Composable`
- `DisplayArea(content: @Composable () -> Unit)` — center slot, fills ~70% screen width
- `SideButtonBar` left/right: icon + label buttons (Home, Apps, Settings)
- `BottomNavBar`: scene mode / status indicators
- Layout uses `BoxWithConstraints` for AAOS landscape resolution (1280×720 default)
- Dark automotive color scheme: background `#1A1A2E`, accent `#00D4FF`, text `#E8E8F0`
- Font: `Roboto Mono` for distance labels (technical feel), `Roboto` for UI labels

**Acceptance Criteria:**
- [ ] Compose Preview renders correctly at 1280×720 AAOS resolution
- [ ] All button tap areas ≥ 48dp (Android accessibility minimum)
- [ ] Layout uses no hardcoded pixel dimensions — all values in `Dp` tokens

**Files:** `app/src/main/java/com/hackathon/v2x/ivi/ui/screen/MainScreen.kt`

**Commit:** `[16.5.2.2] feat: build R16 AAOS main screen layout scaffold in Jetpack Compose`

---

### `16.5.2.3` — Implement Display Area View Switcher State

**Objective:** State machine controlling which composable is shown in the Display Area. Each nav button press switches the content; state persists across recompositions.

**Scope:**
- `DisplayMode` sealed class: `WarningView` | `HomeView` | `AppsView` | `SettingsView`
- `MainViewModel.kt`: `currentMode: StateFlow<DisplayMode>` with `setMode(mode: DisplayMode)` function
- Button presses call `MainViewModel.setMode(...)` 
- Compose `AnimatedContent` transition (fade 200ms) between modes in `DisplayArea`
- Home button always visible; pressing Home during active `WarningView` does NOT dismiss warning (safety: warning stays until `WarningViewModel` timeout)

**Acceptance Criteria:**
- [ ] Unit test: calling `setMode(AppsView)` → `currentMode` emits `AppsView`
- [ ] Unit test: active warning state — `setMode(HomeView)` during `WarningView` is blocked
- [ ] Compose preview: `AnimatedContent` shows correct content on mode change

**Files:**
- `app/src/main/java/com/hackathon/v2x/ivi/ui/MainViewModel.kt`
- `app/src/main/java/com/hackathon/v2x/ivi/ui/DisplayMode.kt`

**Commit:** `[16.5.2.3] feat: implement Display Area view switcher state machine`

---

### `16.5.2.4` — Implement Wake-on-Warning Auto-Switch

**Objective:** When an R4 `warning` event is received while Display Area is NOT showing Warning View, automatically switch to Warning View. When warning expires, restore the previous mode.

**Scope:**
- `MainViewModel` collects `WarningViewModel.uiWarningState`
- On `Active` → force `DisplayMode = WarningView`, save `previousMode` (only if not already `WarningView`)
- On `Idle` (after warning active) → restore `previousMode`
- If user manually navigated away during an active warning → no forced restore on Idle (user intent respected)
- Flag `userOverrodeDuringWarning: Boolean` tracks this case

**Acceptance Criteria:**
- [ ] Unit test: `Idle → Active` → `DisplayMode` becomes `WarningView`
- [ ] Unit test: `Active → Idle` after timeout → `DisplayMode` restored to saved `previousMode`
- [ ] Unit test: user manually switches mode while Active → on Idle, DisplayMode stays as user set it (no forced restore)

**Files:** `app/src/main/java/com/hackathon/v2x/ivi/ui/MainViewModel.kt` (update)

**Commit:** `[16.5.2.4] feat: auto-switch Display Area to Warning View on R4 warning event`

---

### `16.5.2.5` — AAOS APK Build & Deployment Smoke Test Documentation

**Objective:** Verify the HMI APK can be installed and launched on the AAOS Skycraft node via ADB. Document the exact deployment procedure for the team.

**Scope:**
- Run `./gradlew assembleDebug` → verify clean build
- Install via `adb install -r app-debug.apk` on AAOS node
- Verify app launches without crash (logcat: no `FATAL EXCEPTION`)
- Commit deployment instructions in `deployment/phase5-ivi-deploy.md`:
  - CarSky Room ADB port-forward steps
  - APK install command
  - Logcat filter for IVI app
  - Smoke test checklist (launch, mock sender test)

**Acceptance Criteria:**
- [ ] `./gradlew assembleDebug` produces a valid APK under 50 MB
- [ ] `adb install` succeeds and app launches on AAOS node
- [ ] `deployment/phase5-ivi-deploy.md` committed with working instructions

**Files:** `deployment/phase5-ivi-deploy.md`

**Commit:** `[16.5.2.5] docs: add AAOS APK build and deployment smoke test instructions`

---

## 📦 Task Group 5.3 — God View 2D Canvas & Visual Rendering (Serves R17)

> **Goal:** Build the Warning View composable rendering a 2D top-down "God View" of all 3 vehicles. Ghost C strictly sourced from `v2x_relayed` data. The view seam separates 2D and optional 3D.

---

### `17.5.3.1` — Define `IviWarningViewSeam` Interface

**Objective:** Abstract interface decoupling the data layer from the rendering engine, so 2D and 3D implementations are swappable without touching the main screen.

**Scope:**
- `IviWarningViewSeam.kt`: `interface IviWarningViewSeam { @Composable fun Render(scene: SceneGeometry, riskState: String) }`
- `SceneGeometry`: `ego`, `vehicleB`, `vehicleC?` — all ego-relative positions in meters (x longitudinal, y lateral)
- Seam implementation injected via Hilt or `CompositionLocal`
- Default: `CanvasWarningView` (2D). Optional: `SceneViewWarning3D` stub.

**Acceptance Criteria:**
- [ ] Zero Android UI framework imports in `IviWarningViewSeam.kt` interface file
- [ ] Unit test: interface accepts `vehicleC = null` (C not yet tracked) without crash
- [ ] Swap of implementation does not require changing `MainScreen.kt`

**Files:** `app/src/main/java/com/hackathon/v2x/ivi/ui/view/IviWarningViewSeam.kt`

**Commit:** `[17.5.3.1] design: define IviWarningViewSeam rendering interface`

---

### `17.5.3.2` — Implement 2D Canvas Scene Coordinate Mapper

**Objective:** Pure Kotlin math layer converting `SceneGeometry` (ego-frame meters) into Compose Canvas pixel coordinates. No drawing, no Android imports — fully unit-testable.

**Scope:**
- `SceneCoordinateMapper.kt` — pure Kotlin `object`, zero Android imports
- Takes `canvasWidthPx: Float`, `canvasHeightPx: Float`, `scaleMetersPerPixel: Float` (default `0.5 m/px`, from config)
- Maps `x` (longitudinal forward = up on canvas) and `y` (lateral right = right on canvas) to canvas `Offset`
- Ego is always anchored at `(canvasWidth/2, canvasHeight * 0.75)` — center-bottom third
- Clamps all vehicle positions to canvas bounds with 16 px margin
- Returns `VehicleRenderData(offset, radiusPx)` for each vehicle

**Acceptance Criteria:**
- [ ] Unit test: ego at `(0, 0)` → maps to center-bottom anchor
- [ ] Unit test: B at `(20 m, 0)` forward → maps 20 / 0.5 = 40 px above ego
- [ ] Unit test: B at `(0, 5 m)` right → maps 5 / 0.5 = 10 px to the right of ego
- [ ] Unit test: position 500 m forward → clamped to canvas top edge
- [ ] Zero `android.*` or `androidx.*` imports

**Files:** `app/src/main/java/com/hackathon/v2x/ivi/ui/view/SceneCoordinateMapper.kt`

**Commit:** `[17.5.3.2] feat: implement ego-frame to canvas coordinate mapper (pure Kotlin)`

---

### `17.5.3.3` — Implement 2D Canvas God View Core Renderer

**Objective:** Draw the top-down God View using Compose `Canvas` with base vehicle styling. This is the committed M1 deliverable.

**Scope (CanvasWarningView.kt implements IviWarningViewSeam):**
- Background fill: `#0D0D1A` dark slate
- Road centerline: dashed vertical line, color `#333355`, from canvas top to bottom
- **Ego A:** solid circle `#00D4FF` cyan, radius 24 px; small forward-pointing triangle; label "EGO" below
- **Vehicle B:** solid circle `#FFB300` amber, radius 20 px; small triangle; label "B" below
- **Ghost C:** dashed circle outline `#FF4040` red (`PathEffect.dashPathEffect([12f, 8f], 0f)`), radius 20 px; rendered only when `vehicleC != null`
- Connector lines between ego–B and B–C: thin dashed `#555577`
- Distance labels: `d_AB = X.X m` and `d_AC ≈ X.X m` drawn centered on each connector line (Roboto Mono 12sp)
- `vehicleC = null` → no C drawn, no crash, no placeholder

**Acceptance Criteria:**
- [ ] Compose Preview renders all 3 vehicles with mock `SceneGeometry(ego, B at 20m, C at 35m)`
- [ ] `vehicleC = null` → only Ego and B rendered, no error
- [ ] Distance labels update reactively when `SceneGeometry` StateFlow recomposes
- [ ] No `drawBitmap` or external asset dependencies — pure Canvas draw calls

**Files:** `app/src/main/java/com/hackathon/v2x/ivi/ui/view/CanvasWarningView.kt`

**Commit:** `[17.5.3.3] feat: implement 2D Compose Canvas God View renderer`

---

### `17.5.3.4` — Add Ghost C `v2x_relayed` Badge, Risk Glow & Defensive Guard

**Objective:** Augment Ghost C rendering with a source badge, animated risk glow, and a defensive guard ensuring `v2x_relayed` data is never confused with direct detections.

**Scope (CanvasWarningView.kt update):**
- Ghost C outer glow ring: `InfiniteTransition` pulsing alpha `0.3 → 0.8`, period 1.2 s
- Glow color driven by `riskState`: `"low"` → `#FFB300` amber, `"medium"` → `#FF6600` orange, `"high"` → `#FF1A1A` red
- Badge above Ghost C: `[V2X] C · 28.3 m · RISK: HIGH` in Roboto Mono 11sp on semi-transparent dark background card (`#1A1A2E` with alpha 0.85), rounded 6 dp corners
- **Defensive guard:** if `R3Snapshot.source != "v2x_relayed"` → do NOT render C normally; render as yellow `#FFD700` question-mark circle with label `[? UNKNOWN SOURCE]` and log `ERROR` with the full snapshot JSON
- Guard is exercised in unit test by injecting `source = "own_sensor"` on vehicleC

**Acceptance Criteria:**
- [ ] Compose Preview: Ghost C shows pulsing glow ring and badge
- [ ] `riskState = "high"` → glow ring is red `#FF1A1A`
- [ ] Defensive guard unit test: `source = "own_sensor"` on vehicleC → renders yellow `[?]` circle; test captures log ERROR
- [ ] Pulsing animation uses `drawBehind` lambda only — does not trigger layout recomposition

**Files:** `app/src/main/java/com/hackathon/v2x/ivi/ui/view/CanvasWarningView.kt` (update)

**Commit:** `[17.5.3.4] feat: add Ghost C V2X badge, risk glow, and defensive source guard`

---

### `17.5.3.5` — Warning Alert Banner Overlay with Countdown Timer

**Objective:** Overlay an alert banner on top of the God View canvas, showing the warning type and a countdown until auto-dismiss. Banner clears when `WarningViewModel` transitions to `Idle`.

**Scope:**
- `WarningBannerOverlay.kt` — composable overlaid via `Box` above `CanvasWarningView`
- Banner content: `"⚠  NLOS OBSTRUCTION — Vehicle C ahead (relayed via V2X)"`
- Countdown: animated `LinearProgressIndicator` depleting over `WARNING_TIMEOUT_MS` seconds
- Banner background: semi-transparent bar spanning full canvas width; color matches `riskState` (same mapping as glow)
- Dismiss (X) button: hides banner immediately but does NOT clear Warning View (canvas + ViewModel remain active)
- When `uiWarningState` transitions to `Idle`: both banner and canvas are cleared by ViewModel

**Acceptance Criteria:**
- [ ] Compose Preview: banner renders with countdown bar and X button
- [ ] `riskState = "medium"` → banner background is orange `#FF6600`
- [ ] User taps X → banner hidden; Warning View canvas stays active
- [ ] `WarningViewModel.Idle` event → banner disappears (collected via `LaunchedEffect` in overlay)

**Files:** `app/src/main/java/com/hackathon/v2x/ivi/ui/view/WarningBannerOverlay.kt`

**Commit:** `[17.5.3.5] feat: add warning alert banner with risk color and countdown timer`

---

### `17.5.3.6` — (Optional) 3D SceneView Seam Stub

**Objective:** Non-crashing SceneView stub satisfying `IviWarningViewSeam` for the optional 3D extension path in Phase 6. Off by default via `BuildConfig`.

**Scope:**
- `SceneViewWarning3D.kt` — implements `IviWarningViewSeam`
- Renders `Text("3D scene view — not yet implemented")` placeholder in the correct canvas dimensions
- Annotated `@Experimental3DApi` — excluded from committed M1 deliverable
- Switchable via `BuildConfig.ENABLE_3D_VIEW` (default `false`); Hilt provides correct implementation based on flag
- SceneView/Filament dependency behind a Gradle `compileOnly` block to avoid APK bloat

**Acceptance Criteria:**
- [ ] App compiles and defaults to 2D with `ENABLE_3D_VIEW = false`
- [ ] `ENABLE_3D_VIEW = true` shows placeholder text without crash
- [ ] 3D flag has zero effect on 2D rendering pipeline or APK size when false

**Files:** `app/src/main/java/com/hackathon/v2x/ivi/ui/view/SceneViewWarning3D.kt`

**Commit:** `[17.5.3.6] refactor: add optional 3D SceneView seam stub behind BuildConfig flag`

---

## 📦 Task Group 5.4 — Full Stack Integration & Acceptance Tests

---

### `16.5.4.1` — Wire Full UI Stack with Hilt DI

**Objective:** Connect all layers end-to-end with dependency injection: `R4ListenerService` → `R4Repository` → `WarningViewModel` → `MainScreen` → `CanvasWarningView`. Validate with mock sender.

**Scope:**
- `AppModule.kt` (@Module @InstallIn(SingletonComponent)): provides `R4Repository` singleton, `R4Deserializer`, `IviWarningViewSeam` binding
- `MainActivity.kt`: starts `R4ListenerService` on `onCreate`, collects `WarningViewModel.uiWarningState`
- `MainScreen` consumes `WarningViewModel.uiWarningState` and `latestScene` via `collectAsState()`
- Manual integration test: run `MockR4Sender` → observe IVI screen updates live on AAOS emulator

**Acceptance Criteria:**
- [ ] Mock sender `approaching` scenario → Warning View activates; Ghost C shown at correct distance
- [ ] Mock sender `leaving` scenario → Ghost C disappears; Warning View clears after timeout
- [ ] LeakCanary: no memory leaks after full approach / leave cycle
- [ ] No direct `R4Deserializer` or `DatagramSocket` instantiation in UI layer

**Files:**
- `app/src/main/java/com/hackathon/v2x/ivi/di/AppModule.kt`
- `app/src/main/java/com/hackathon/v2x/ivi/MainActivity.kt`

**Commit:** `[16.5.4.1] feat: wire full R4 service → UI stack with Hilt DI`

---

### `17.5.4.2` — Phase 5 Full Acceptance Test Suite

**Objective:** Automated tests covering all Phase 5 acceptance criteria, runnable in CI without a physical AAOS device.

**Test Files & Coverage:**

| Test File | Subtasks Covered |
|---|---|
| `R4DeserializerTest.kt` | 4.5.1.2 — all 5 JSON parsing cases |
| `R4RepositoryTest.kt` | 4.5.1.4 — event routing, last-value-wins state |
| `WarningViewModelTest.kt` | 4.5.1.4 — Idle/Active/timeout transitions |
| `MainViewModelTest.kt` | 16.5.2.3, 16.5.2.4 — mode switching, wake-on-warning, user override |
| `SceneCoordinateMapperTest.kt` | 17.5.3.2 — coordinate mapping math, clamping |
| `CanvasWarningViewTest.kt` | 17.5.3.4 — defensive source guard (own_sensor → [?]) |

**Acceptance Criteria:**
- [ ] `./gradlew test` passes with zero failures
- [ ] Code coverage ≥ 80% for `data/`, `ui/`, `model/` packages (excluding `SceneViewWarning3D.kt` stub)
- [ ] CI pipeline (GitHub Actions or equivalent) runs tests on every push

**Files:** `app/src/test/java/com/hackathon/v2x/ivi/` (all test files above)

**Commit:** `[17.5.4.2] test: add Phase 5 full acceptance test suite`

---

## 🗂️ Expected File Tree at Phase 5 Completion

```
app/
├── build.gradle.kts
├── src/
│   ├── main/
│   │   ├── AndroidManifest.xml
│   │   ├── java/com/hackathon/v2x/ivi/
│   │   │   ├── model/
│   │   │   │   ├── R4Message.kt             ← 4.5.1.1
│   │   │   │   ├── R3Snapshot.kt            ← 4.5.1.1
│   │   │   │   └── SceneGeometry.kt         ← 4.5.1.1
│   │   │   ├── data/
│   │   │   │   ├── R4Deserializer.kt        ← 4.5.1.2
│   │   │   │   └── R4Repository.kt          ← 4.5.1.4
│   │   │   ├── service/
│   │   │   │   └── R4ListenerService.kt     ← 4.5.1.3
│   │   │   ├── di/
│   │   │   │   └── AppModule.kt             ← 16.5.4.1
│   │   │   └── ui/
│   │   │       ├── DisplayMode.kt           ← 16.5.2.3
│   │   │       ├── MainViewModel.kt         ← 16.5.2.3 / 16.5.2.4 / 16.5.4.1
│   │   │       ├── WarningUiState.kt        ← 4.5.1.4
│   │   │       ├── WarningViewModel.kt      ← 4.5.1.4
│   │   │       ├── screen/
│   │   │       │   └── MainScreen.kt        ← 16.5.2.2
│   │   │       └── view/
│   │   │           ├── IviWarningViewSeam.kt      ← 17.5.3.1
│   │   │           ├── SceneCoordinateMapper.kt   ← 17.5.3.2
│   │   │           ├── CanvasWarningView.kt        ← 17.5.3.3 / 17.5.3.4
│   │   │           ├── WarningBannerOverlay.kt     ← 17.5.3.5
│   │   │           └── SceneViewWarning3D.kt       ← 17.5.3.6 (optional)
│   │   └── res/
│   │       └── xml/network_security_config.xml
│   └── test/java/com/hackathon/v2x/ivi/
│       ├── R4DeserializerTest.kt
│       ├── R4RepositoryTest.kt
│       ├── WarningViewModelTest.kt
│       ├── MainViewModelTest.kt
│       ├── SceneCoordinateMapperTest.kt
│       └── CanvasWarningViewTest.kt
├── mock-sender/
│   └── src/main/kotlin/com/hackathon/v2x/mock/MockR4Sender.kt  ← 4.5.1.5
└── deployment/
    └── phase5-ivi-deploy.md                                     ← 16.5.2.5
```

---

## 🔀 Execution Order & Parallelism

```
Can start immediately (only dependency: R4 schema from Phase 0):
  4.5.1.1 → 4.5.1.2 → 4.5.1.3 → 4.5.1.4    (data layer — sequential)
  4.5.1.5                                      (mock sender — parallel with data layer)
  16.5.2.1                                     (gradle config — unblocks all UI tasks)
  17.5.3.1                                     (seam interface — unblocks canvas tasks)

After 16.5.2.1:
  16.5.2.2 → 16.5.2.3 → 16.5.2.4            (HMI scaffold — sequential)
  16.5.2.5                                    (deploy doc — any time after 16.5.2.1)

After 17.5.3.1:
  17.5.3.2 → 17.5.3.3 → 17.5.3.4 → 17.5.3.5 (canvas renderer — sequential)
  17.5.3.6                                     (optional 3D stub — parallel)

Final (after all above):
  16.5.4.1 → 17.5.4.2
```

---

## 🛠️ Build & Verify Commands

```bash
# Build debug APK
./gradlew assembleDebug

# Run all unit tests
./gradlew test

# Run lint checks
./gradlew lint

# Install on AAOS node (after ADB port-forward to CarSky Room)
adb install -r app/build/outputs/apk/debug/app-debug.apk

# Run mock sender for manual integration testing
./gradlew :mock-sender:run --args="--ip 127.0.0.1 --port 5004 --interval-ms 2000 --cycles 5"

# Launch app after install and check no crash
adb logcat -s "IVI_V2X" | head -50
```

---

*Last updated: 2026-07-24 — based on R4, R16, R17 in [m1-cooperative-awareness.md](../requirements/m1-cooperative-awareness.md) and Phase 5 acceptance in [milestone1.md](milestone1.md).*
