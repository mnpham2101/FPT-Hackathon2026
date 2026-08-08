# IVI ECU — PR #2 review, v1

> **Version:** v1 — 2026-08-05. First review of this pull request; no previous version.
> **Pull request:** #2 — branch `feat/phase5-ivi-hmi-dev` → `main` (title not read; `gh` was unauthenticated, so the PR body was not consulted and this review judges the branch content alone)
> **Reviewed commit:** `f7f2f55` — *fix(ivi): prevent Log.w RuntimeException in unit tests and use R4Json for deserialization*
> **Base:** `main` at `f5e8a31`; merge base `e4991a9`; diff range `main...pr-2` — 21 files, +1261 / −22. Phase 3 and Phase 4 landed on `main` after this branch forked and touched no file under `IVI_ECU/`, so the range is unaffected by them.
> **Judged against:** [ivi-ecu-hld.md](../../../documents/Design/IVI-ECU/ivi-ecu-hld.md) and [ivi-ecu-design-decisions.md](../../../documents/Design/IVI-ECU/ivi-ecu-design-decisions.md) · [m1-cooperative-awareness.md §2](../../../requirements/m1-cooperative-awareness.md) · [phase5_minh_tasks.md](../../../plans/phase5_minh_tasks.md), the authoritative Phase 5 plan · the frozen schemas [r4-ada-ivi.schema.json](../../contracts/r4-ada-ivi.schema.json) and [r3-tracked-object.schema.json](../../contracts/r3-tracked-object.schema.json)
> **Verified:** unit tests 15/15 pass (`R4DeserializerTest` 5, `WarningViewModelTest` 4, `R4RoundTripTest` 3, `R4AdditiveVersionTest` 3) and `app-debug.apk` builds, both read from the build output in the review worktree. **Not verified:** any behaviour in a deployed Room — no `phase5-ci.yml` lane exists on the branch and no run evidence is recorded.

Two facts frame everything below, and neither is a criticism of the code itself.

**The branch does not contain the node's design.** `IVI_ECU/doc/` — this file's own folder, holding the HLD, the decision record and the diagrams — exists on `main` and not on `pr-2`. Per [hld-content-and-commit-format.md](../../../.claude/rules/hld-content-and-commit-format.md) that HLD is the IVI node's *sole* design authority, so it is what this review scores against. The gap between the two is the single largest cause of the findings here.

**`main` already reverted an earlier version of this work.** Commit `ff032ff` — *"[4] fix: revert the unsanctioned R4 binding that broke the IVI build"* — removed an off-contract R4 binding and named this branch as the correct implementation of the receive path. That judgement holds: `R4Deserializer`, `R4Repository` and `R4ListenerService` here are a real improvement on what was reverted. Several of the specific defects that revert called out have nonetheless reappeared in softer form, and they are marked below.

---

## 1 · Architecture review

Rows are the components the IVI HLD defines ([§3](../../../documents/Design/IVI-ECU/ivi-ecu-hld.md#3-the-component-architecture), [§6](../../../documents/Design/IVI-ECU/ivi-ecu-hld.md#6-internal-components)), each at the path [§4](../../../documents/Design/IVI-ECU/ivi-ecu-hld.md#4-folder-structure) designates. A component the branch never created is a row at 0% rather than an omission — that is what makes this a completion measure. Rows marked *inherited* came from the merge base and were not touched by this PR; they are scored for **where the branch leaves them**, since this PR's work was supposed to reach them.

The design is **five Gradle modules** (`:contract`, `:serializer`, `:observer`, `:app`, `:r4-simulator`). The branch has one — `settings.gradle.kts` includes `:app` only — so every component below lands in `:app` regardless of the layer it belongs to. That single fact drives most of the "does not abide" comments.

| Module in design | % complete | Abides by the architecture? Relations to other components |
|---|---|---|
| **`:contract`** — `model/R4Message`, `R3Snapshot`, `SceneGeometry`, `VehiclePosition`, `R4Json` *(Data)* | **55%** | **No — module absent and the frozen binding was loosened.** Types live in `:app/model/`, so nothing can share them with `:serializer` or `:r4-simulator` as the design intends. Worse, [R3Snapshot.kt:20-27](../../app/src/main/java/com/hackathon/v2x/ivi/model/R3Snapshot.kt#L20-L27) now gives defaults to `class`, `distance`, `speed`, `confidence`, `state` and `timestamps`, and [SceneGeometry.kt:30](../../app/src/main/java/com/hackathon/v2x/ivi/model/SceneGeometry.kt#L30) defaults `vehicleC` — all nine R3 fields and all three geometry fields are `required` in the frozen schemas. The binding now silently accepts messages the contract rejects, and invents `class="vehicle"`, `state="tracked"`, `confidence=1.0` for data ADA never sent. `id`, `source` and `position` were correctly left required. |
| **`model/R4ServiceError`** *(not in the design)* | — | **Unsanctioned.** [R4Message.kt:57-62](../../app/src/main/java/com/hackathon/v2x/ivi/model/R4Message.kt#L57-L62) adds a third member to the sealed R4 hierarchy with `@SerialName("error")`. R4 is a **frozen** contract whose `oneOf` admits `warning` and `state` only. A transport-failure signal is real and needed — but it is an internal event type, not an R4 message, and putting it in the wire binding is the same class of drift `ff032ff` reverted. |
| **`model/VehicleState`** *(not in the design)* | — | **Unsanctioned and unreachable.** [R4Message.kt:65-70](../../app/src/main/java/com/hackathon/v2x/ivi/model/R4Message.kt#L65-L70) is referenced by nothing. Its `{position, speed}` shape matches what `mock_r4_sender.py` emits, while `R4Vehicles` binds the contract's `{x, y}` — the orphan is the visible seam of that disagreement. |
| **`:serializer/R4Deserializer`** *(Business logic)* | **50%** | **Partly.** The core judgement is right and matches the design: returns `Result` instead of throwing, so one bad datagram cannot stop the next, and unknown `warningType` degrades rather than failing — the R4 acceptance clause, and well tested. Three departures: it sits in `:app/data/`, an Android-importing module, against a design that keeps it plain-JVM; it takes `ByteArray` rather than the designed `(buffer, offset, length)`, so it cannot express a slice; and [R4Deserializer.kt:59-64](../../app/src/main/java/com/hackathon/v2x/ivi/data/R4Deserializer.kt#L59-L64) **rewrites** the unknown `warningType` to `"unknown"`. The design splits this deliberately — the parser preserves the wire value, `WarningClassifier` decides its presentation — and rewriting destroys the original before any consumer or log sees it. `schemaVersionAhead` is not surfaced. |
| **`:observer/JdkDatagramSource`** — the only socket holder *(Business logic)* | **10%** | **No.** The component does not exist; the socket is opened inline at [R4ListenerService.kt:103](../../app/src/main/java/com/hackathon/v2x/ivi/service/R4ListenerService.kt#L103), inside the Android service. That collapses the layer the [HLD §8](../../../documents/Design/IVI-ECU/ivi-ecu-hld.md#8-interfaces-ports-and-the-layer-rule) rule protects, and removes the seam (`R4DatagramSource`) that makes the receive loop testable without a device — which is why no test covers this file. The design's one-line description of this component ("resets packet length before every receive") is the defect noted under § Findings below. |
| **`:observer/R4SocketObserver`** — receive loop, truncation check, back-off, link state *(Business logic)* | **30%** | **Partly, and inline.** Rebind back-off is implemented (5 attempts, 1 s) and is genuinely the right shape. Missing: the truncation check, the `DROP_OLDEST` bounded flow (this uses a suspending `extraBufferCapacity = 64`, so a slow collector *can* stall the socket), `R4LinkState`, and the `[LINK]` / `[RX]` / `[DROP]` evidence lines. No seam, so no test. |
| **`warning/WarningClassifier`** *(Business logic)* | **0%** | **Absent, and its job was absorbed upstream.** `isKnownWarningType` at [R4Deserializer.kt:74-75](../../app/src/main/java/com/hackathon/v2x/ivi/data/R4Deserializer.kt#L74-L75) hardcodes the M1 registry into a boolean in the parser. R4 calls the registry extensible and CLAUDE.md principle 5 forbids the literal; more importantly this is the component that should own it. |
| **`ui/view/SceneCoordinateMapper`** *(Business logic)* | **100%** *(inherited)* | Correct and untouched. Pure math, no Android types, correctly outside the view. |
| **`data/R4Repository`** *(Data)* | **55%** | **Right shape, no owner.** Routing `warning` → `SharedFlow` and `state` → last-value-wins `StateFlow` is exactly the designed responsibility, and it holds zero Android imports as required. But `attachToService` is called by nothing in `main/`, so the repository never collects anything at runtime. It also swallows the error case: [R4Repository.kt:51](../../app/src/main/java/com/hackathon/v2x/ivi/data/R4Repository.kt#L51) maps `R4ServiceError → Unit` with the comment *"handled by WarningViewModel"*, while the repository is the flow's only collector — so the view-model never learns. No link state, and no dev-injector entry point. Untested. |
| **`ui/WarningViewModel`** *(UI logic)* | **50%** | **Right layer, incomplete behaviour, unreachable states.** No drawing code and no socket — the layer rule holds, and Idle → Active → Idle on `WARNING_TIMEOUT_MS` works and is tested. But `onServiceError()` at [WarningViewModel.kt:85-87](../../app/src/main/java/com/hackathon/v2x/ivi/ui/WarningViewModel.kt#L85-L87) has no caller, so `WarningUiState.Error` cannot occur; the `currentState` collector at [lines 60-64](../../app/src/main/java/com/hackathon/v2x/ivi/ui/WarningViewModel.kt#L60-L64) has an empty body, so R15 state messages update nothing. Decision **D13** (a `low` risk updates the scene *without* leaving Active) is not implemented — any warning re-arms the same 10 s timeout. The R19 provenance snapshot is never forwarded to the renderer. |
| **`ui/MainViewModel` + `ui/DisplayMode`** *(UI logic)* | **40%** *(inherited)* | Untouched by this PR. Wake-on-warning is the connection this PR's work existed to enable, and no `WarningUiState` reaches it — the two view-models have no relation at all. |
| **`config/IviRuntimeConfig`** *(UI logic)* | **0%** | **Absent, and the port is wrong.** [build.gradle.kts:29](../../app/build.gradle.kts#L29) sets `R4_UDP_PORT = 5004`; the designated ADA→IVI port is **47300**. A `buildConfigField` is also compile-time only, where D10 requires a runtime merge point so the blueprint's node config can set it — which is what CLAUDE.md principle 5 asks for. See § Findings for why 47300 is now settled rather than merely designated. |
| **`ui/screen/MainScreen`** *(UI)* | **90%** *(inherited)* | The R16 layout composable is present and good. It is called by nothing outside `@Preview` — see the host row. |
| **`ui/view/IviWarningViewSeam`** *(UI)* | **100%** *(inherited)* | The R17 swap point is correctly defined and correctly realized by `CanvasWarningView`. Nothing this PR added uses it. |
| **`ui/view/CanvasWarningView`** + the provenance guard *(UI)* | **85%** *(inherited)* | The renderer and the R19 guard are present and are the strongest existing asset. The guard reads `SceneGeometry.vehicleCSnapshot`; this PR's `WarningViewModel` publishes `event.geometry`, which the ADA producer does not populate with a snapshot — so on this branch the guard would receive `null` and treat C as trusted. |
| **`ui/view/WarningBannerOverlay`** *(UI)* | **100%** *(inherited)* | Correctly built and correctly mounted nowhere, per D11. |
| **`MainActivity` + `IviApplication` + `di/IviGraph`** *(Host)* | **0%** | **Absent — and this is the finding that gates the rest.** No activity class, no `<activity>` in [AndroidManifest.xml](../../app/src/main/AndroidManifest.xml), no `Application`, no composition root. The APK builds and installs but has **no launchable component**, and nothing constructs `WarningViewModel`, starts `R4ListenerService`, or calls `attachService`. Every component above is correct in isolation and connected to nothing. |
| **`service/R4ListenerService`** *(Host)* | **60%** | **Exists, but holds three components' work.** As a foreground host it is sound: channel, notification, `dataSync` type, `SupervisorJob` scope. It also owns the socket and the receive loop, which the design assigns to `:observer` — so the layer collapse noted above lives here. It is declared in the manifest but started by nobody. `onDestroy` cancels the job ([lines 64-67](../../app/src/main/java/com/hackathon/v2x/ivi/service/R4ListenerService.kt#L64-L67)) without closing the socket, and `socket.receive()` is a blocking call — cancellation will not interrupt it, so the thread survives until a datagram arrives. |
| **`service/AndroidR4Logger`** — the `IVI_V2X` evidence bridge *(Host)* | **0%** | **Absent.** Logging is ad-hoc `Log.w` on per-class tags, so `adb logcat -s IVI_V2X` returns nothing and R18's single-tag structured surface does not exist. See § The event raised when a warning arrives for what this costs on the happy path. Relatedly, `safeLogW` at [R4Deserializer.kt:77-83](../../app/src/main/java/com/hackathon/v2x/ivi/data/R4Deserializer.kt#L77-L83) wraps `Log.w` in `runCatching` to survive unit tests — a test concern solved in production code, and redundant with `isReturnDefaultValues = true` already set in `build.gradle.kts`. The designed `R4Logger` seam removes the need for both. |
| **`r4-simulator/` → `m1-r4-sim:latest`** *(Test equipment)* | **20%** | **Delivered as something else, in the wrong place, off-contract.** `IVI_ECU/mock-sender/` is a genuinely useful harness, but [node-code-layout.md § `tools/`](../../../.claude/rules/node-code-layout.md) permits a mock inside a consumer node folder only when all four conditions hold — this one shares no build with `IVI_ECU`'s Gradle project, so its home is `tools/`. Its payloads also disagree with the contract: [mock_r4_sender.py:55-64](../../mock-sender/mock_r4_sender.py#L55-L64) emits `vehicles.B` as `{position, speed}` where the schema and `R4Vehicles` require `vehicles.vehicleB` as `{x, y}`, so **every state message it sends fails to decode**; and it omits `class` from the R3 object, which is precisely why the model gained defaults. |
| **`debug/DevInjectorReceiver`** *(Test equipment)* | **0%** | Absent. The no-network UI path has no way to be exercised. |
| **Hilt / KSP stack** *(not in the design)* | — | **Unsanctioned and inert.** `build.gradle.kts` applies `com.google.dagger.hilt.android` and `ksp`, and `@HiltViewModel` / `@Singleton` / `@Inject` annotate the new classes — but with no `@HiltAndroidApp` and no `@AndroidEntryPoint` anywhere, nothing is ever injected. Decision **D7** chose a manual composition root (`IviGraph`) instead, and the authoritative plan's `4.5.1.6` removes this stack outright. |

### The event raised when a warning arrives

The question a reader of this node most often needs answered, and the answer is not obvious from any one file.

**No dedicated event type is raised. The wire model is emitted directly as the event**, twice, and terminates in a state change:

| # | Where | What is raised | Type |
|---|---|---|---|
| 1 | [R4ListenerService.kt:109](../../app/src/main/java/com/hackathon/v2x/ivi/service/R4ListenerService.kt#L109) | `_r4EventFlow.emit(message)` — the decoded message itself | `MutableSharedFlow<R4Message>`, `extraBufferCapacity = 64` |
| 2 | [R4Repository.kt:49](../../app/src/main/java/com/hackathon/v2x/ivi/data/R4Repository.kt#L49) | `_warningEvents.emit(message)` after `is R4WarningEvent` narrows the sealed type | `MutableSharedFlow<R4WarningEvent>`, `extraBufferCapacity = 32` |
| 3 | [WarningViewModel.kt:67-71](../../app/src/main/java/com/hackathon/v2x/ivi/ui/WarningViewModel.kt#L67-L71) | `WarningUiState.Idle → WarningUiState.Active(event)`, and `_latestScene.value = event.geometry` | `MutableStateFlow<WarningUiState>` / `MutableStateFlow<SceneGeometry?>` |

So the mechanism is **Kotlin coroutine flow emission** throughout — no Android `Intent`, no `BroadcastReceiver`, no `LiveData`, no listener callback, no event bus. The sealed-class narrowing at step 2 is the only routing decision, and `R4StateMessage` diverges there to `_currentState` while `R4ServiceError` is dropped.

This differs from the design in one way worth naming: the HLD's `:observer` raises a **distinct domain event** — `R4Event.Message` / `R4Event.Dropped`, alongside `R4LinkState` — so that a drop, a truncation and a link change are all first-class and observable. Here the wire model *is* the event, which is why a dropped datagram has nowhere to be represented and why `R4ServiceError` had to be smuggled into the frozen contract to signal a transport failure. That is the root of two separate findings above.

**At runtime none of this fires.** Nothing calls `attachService`, so step 1 has no collector and steps 2 and 3 are never reached.

### Is the event logged, and is the schema deduction evidenced?

Checked directly, and the answer to both is **no**.

**Every log statement on the receive path is on a failure or a setup path.** A successful warning arrival produces no log line at all:

| Line | When it fires | Path |
|---|---|---|
| `Log.i` "UDP socket open on port …" — [R4ListenerService.kt:104](../../app/src/main/java/com/hackathon/v2x/ivi/service/R4ListenerService.kt#L104) | once, at socket open | setup |
| `Log.w` "Skipping bad packet: …" — [line 110](../../app/src/main/java/com/hackathon/v2x/ivi/service/R4ListenerService.kt#L110) | decode failed | failure |
| `Log.w` "UDP socket error (attempt n/5)" — [line 80](../../app/src/main/java/com/hackathon/v2x/ivi/service/R4ListenerService.kt#L80) | socket threw | failure |
| `Log.e` "Max retries reached" / "Unexpected error" — [lines 82, 88](../../app/src/main/java/com/hackathon/v2x/ivi/service/R4ListenerService.kt#L82-L88) | loop giving up | failure |
| `safeLogW` — [R4Deserializer.kt:41, 54, 60](../../app/src/main/java/com/hackathon/v2x/ivi/data/R4Deserializer.kt#L41) | parse failure, or unknown `warningType` degradation | failure / degradation |

`_r4EventFlow.emit(...)` at line 109, `_warningEvents.emit(...)` in the repository, and the `Idle → Active` transition in the view-model are all **silent**. There is no `[RX]`, no `[EVT]`, no per-message line of any kind — so on a live node a received warning and a node that received nothing at all look identical in logcat. That is what the missing `AndroidR4Logger` and the `IVI_V2X` tag cost, and it is why R18 scores 5% below.

**The schema deduction is correct but unevidenced, and the version half is not checked at all.**

- *Type* is deduced correctly. `R4Json` sets `classDiscriminator = "type"` ([R4Message.kt:77-80](../../app/src/main/java/com/hackathon/v2x/ivi/model/R4Message.kt#L77-L80)), and `R4Deserializer` additionally pre-reads `root["type"]` to pick the branch. A `warning` decodes to `R4WarningEvent`, a `state` to `R4StateMessage`, anything else to `UnknownMessageTypeException`. Correct — and never logged, so nothing records which variant was deduced for a given datagram.
- *Version* is not checked anywhere. `schemaVersion` appears in the models and is read by no code — no comparison against a supported version, no constant to compare to, and none of the `schemaVersionAhead` signal the design asks the deserializer to surface. A message stamped `schemaVersion: 9` decodes and renders exactly as a `1`, with no warning to anyone.

The unknown-`warningType` degradation *is* logged, which makes the asymmetry sharper: the one additive-evolution case that is handled is announced, and the two that are not — a newer `schemaVersion`, and a successful decode worth confirming — are silent.

### Findings that cross several rows

- **Buffer reuse silently truncates every datagram after the first.** [R4ListenerService.kt:101-111](../../app/src/main/java/com/hackathon/v2x/ivi/service/R4ListenerService.kt#L101-L111) builds one `DatagramPacket` outside the loop and never restores its length. `DatagramSocket.receive` sets `packet.length` to the bytes received, and the next `receive` will not write more than that — so a 900-byte warning following a 200-byte state message is delivered as 200 bytes and fails to parse. A real runtime defect that unit tests cannot see, because no test exercises the socket. The HLD names the fix in `JdkDatagramSource`'s own description, and [plans/doc/research_notes/TruncationRepro.java](../../../plans/doc/research_notes/TruncationRepro.java) exists for this exact case.
- **The port disagreement is now settled against this branch, not merely designated.** `main` ships `ADA_ECU/src/output/ivi_sender` (commit `8afd294`), whose egress the ADA HLD fixes at `udp → 10.99.0.13:47300` — matching the baseline blueprint, the node guide and the Phase 0 smoke-test evidence. This branch listens on **5004**. The real producer now exists and this consumer cannot hear it.
- **Oversized datagrams are truncated without detection.** `BUFFER_SIZE = 4096` with no check for `packet.length == buffer.size`; a larger message becomes a parse failure rather than a reported truncation.
- **The non-local `return` inside `runCatching` bypasses the failure logging.** At [R4Deserializer.kt:27-43](../../app/src/main/java/com/hackathon/v2x/ivi/data/R4Deserializer.kt#L27-L43) the `return Result.failure(...)` and `return when(...)` return from `deserialize` directly, so the `.onFailure` block is unreachable for every path that actually fails.
- **The state path is broken end to end**, from `mock_r4_sender.py` through `R4StateMessage` to the empty collector in `WarningViewModel` — three independent breaks in one path, none covered by a test.
- **No CI.** [ci-lane-placement.md](../../../.claude/rules/ci-lane-placement.md) designates `ivi-unit-tests`, `ivi-assemble` and `r4-sim-image` to `.github/workflows/phase5-ci.yml`. That file does not exist on this branch, so the 15 passing tests are a local result only.
- **Document placement.** `requirements/blueprint-2node-task51-test-guide.md` is a deploy procedure, and [walkthrough-driven-delivery.md](../../../.claude/rules/walkthrough-driven-delivery.md) puts those in `requirements/car-sky-guide/` as a `*-walkthrough.md` authored by `project-researcher`. `plans/doc/session-summary-2026-07-30.md` is a progress record, which the documentation rules exclude from `doc/`.

---

## 2 · Low-level review

**Variant used: requirements, not task IDs — the IDs do not resolve against the authoritative plan.** The branch carries its own `plans/phase5_tasks.md`, which `main` does not have, and `main` carries [phase5_minh_tasks.md](../../../plans/phase5_minh_tasks.md), which the branch does not. Both assign the **same IDs to different work**:

| ID | Means, in the branch's plan | Means, in the authoritative plan on `main` |
|---|---|---|
| `4.5.1.1` | Define R4 Kotlin data models | Version catalog `gradle/libs.versions.toml` + root plugin aliases |
| `4.5.1.2` | Implement R4 JSON deserializer | Move `:app` onto the catalog |
| `4.5.1.3` | Implement UDP listener foreground service | Create the `:contract` module skeleton + `R4Contract.kt` |
| `4.5.1.4` | Build R4 repository & ViewModel bridge | Relocate the models, tests and samples into `:contract` |

Task IDs are the project's traceability anchor ([task-planning-conventions.md](../../../.claude/rules/task-planning-conventions.md)), so a commit tagged `[4.5.1.3]` now points at two different pieces of work depending on which branch is read. Scoring commit-by-commit would compound that, so the delivered features are scored against the requirements instead.

### Completion against requirements

Percentages are against each requirement's **acceptance** clause in [m1-cooperative-awareness.md §2](../../../requirements/m1-cooperative-awareness.md), not its headline. Only requirements this branch touches are listed.

| Requirement | % complete | What is missing or deviated |
|---|---|---|
| **R3** — Ego object/track contract (IVI binding) | **60%** | Round-trip tests pass (inherited). Deviated: the Kotlin binding no longer matches the frozen schema — 6 of 9 `required` fields carry defaults in `R3Snapshot`, so a snapshot missing `class` or `state` decodes with fabricated values instead of being rejected. The defaults were added to accommodate `mock_r4_sender.py`, which omits `class` — the contract was moved to fit the test equipment. |
| **R4** — ADA → IVI warning message | **70%** | The strongest area. Schema committed and byte-synced (inherited); round-trip and additive-version tests pass; **unknown `warningType` degrades gracefully** — the explicit acceptance clause — is implemented and tested. Deviated: `R4ServiceError` adds a third `type` to a frozen two-member `oneOf`; `geometry.vehicleC` made optional though `required`; the parser overwrites the unknown value rather than preserving it; `schemaVersion` is never checked; and the `state` half of the contract does not decode from the only producer on the branch. |
| **R5** — ECU deployment onto CarSky nodes | **10%** | The APK builds, and that is the whole of it. The acceptance clause is *"the team APK launches on the AAOS node"* — with no `<activity>` in the manifest and no `MainActivity`, it cannot. `blueprint-2node-task51-test.json` is a useful bring-up aid but is not the R5 blueprint, and no deployment of either is recorded. |
| **R6** — Inter-ECU network: Ethernet Bridge + `ethernet` pins | **15%** | The listener binds **5004**; the R6 topology assigns ADA→IVI **47300**, and `main` now ships an ADA sender that targets it. The mock sender and the 2-node blueprint agree with 5004 and therefore with each other, but not with the deployed network — this branch's IVI node would hear nothing from the real ADA node. No UDP reachability demonstrated in a Room. |
| **R16** — HMI on the AAOS node | **25%** | `MainScreen` is a good R16 layout (inherited) but is mounted by nothing, so the HMI does not run. *"An R4 warning brings the warning view up in the Display area"* is the acceptance clause this PR's data layer was built to satisfy, and the connection from `WarningViewModel` to `MainViewModel` to `MainScreen` was not made. The button/app areas do not switch anything at runtime. |
| **R17** — Warning view: God view of the 3 vehicles | **30%** | `CanvasWarningView`, the seam, the mapper and the provenance guard are inherited and in good shape. This PR added no wiring to them: `latestScene` reaches no composable. The `v2x_relayed`-only check cannot be demonstrated, and since the producer never fills `vehicleCSnapshot`, the guard would read `null` and pass C as trusted. |
| **R18** — Evidence logging | **5%** | No structured JSONL and no `IVI_V2X` tag, and — per § Is the event logged — no line at all on a successful receive. A run cannot be reconstructed offline, which is the acceptance clause. |
| **R19** — End-to-end demo run | **0%** | Blocked by R5, R6, R16 and R17 above. No recorded run, no capture, no ghost-C source check. |

### Commit inventory

Supporting evidence for the variant choice, and for the discipline findings.

| # | Commit | Tag |
|---|---|---|
| 1 | `ca2e185` chore: ignore local .agents runtime files and skills lock | none |
| 2 | `78c7c43` feat: add 2-node test blueprint + mock R4 sender | none — this is the branch plan's `4.5.1.5` |
| 3 | `0649d8a` docs: add 2-node blueprint answer for team review | none |
| 4 | `50a6a5f` **[4.5.1.0]** chore: add R4_UDP_PORT BuildConfig, coroutines, deps | `W = 0`, which no plan defines |
| 5 | `423f0ef` **[4.5.1.1]** feat: define R4 Kotlin sealed models | collides |
| 6 | `7c9df5c` **[4.5.1.2]** feat: implement R4 JSON deserializer | collides |
| 7 | `a9d0267` **[4.5.1.3]** feat: implement R4 UDP foreground service | collides |
| 8 | `c6aa9ad` **[4.5.1.4]** feat: add R4Repository and WarningViewModel bridge | collides |
| 9 | `e5161d7` docs: add session summary 2026-07-30 | none |
| 10 | `aaf1997` merge: sync latest origin/main, resolve conflicts | none; `merge` is not a sanctioned `<type>` |
| 11 | `b7351a6` fix(ivi): resolve Kotlin compilation errors for R4Message imports | none; `fix(scope)` is not the project format |
| 12 | `92794da` fix(test): provide default parameters for R3Snapshot | none — this is the commit that loosened the frozen R3 binding, to make a test compile |
| 13 | `f7f2f55` fix(ivi): prevent Log.w RuntimeException in unit tests | none |

Five of thirteen commits carry a `[X.Y.Z.W]` tag; the format is `[<taskID>] <type>: <subject>` for every commit. Commits 11–13 are fix-ups to subtasks 4.5.1.1–4.5.1.4, which means those subtasks were committed with a failing build — against the non-negotiable rule that a subtask passes build and unit tests before it is done. Commit 12 is the clearest case of the cost: a contract binding was relaxed to resolve a compile error in a test.

---

## 3 · Conclusion

There is real, careful engineering in this branch, and it deserves saying before anything else. The `R4Deserializer` → `R4Repository` → `WarningViewModel` chain is a sound piece of design: the parser returns a `Result` so one bad datagram cannot kill the loop, the repository routes edge-triggered events and last-value state through the right flow types and holds no Android imports, and the view-model owns the warning lifecycle without touching a socket or a canvas. That is the MVC separation the HLD asks for, arrived at independently. The unknown-`warningType` degradation is implemented *and* tested, and it is the acceptance clause R4 is most likely to be judged on. The rebind back-off, the foreground-service framing, and the mock sender as a bring-up aid are all good instincts. Fifteen tests pass and the APK builds — the branch is in far better shape than the version `main` had to revert.

Four themes are worth your attention.

**The pieces are excellent and nothing connects them.** This is the big one. There is no `MainActivity`, no `<activity>` in the manifest, no Application class — so the APK has nothing to launch, nothing constructs the view-model, nothing starts the listener service, and `attachService` is never called. `MainScreen` and `CanvasWarningView` are reached only from `@Preview`. Every component scores well on its own and the runtime path from socket to screen does not exist, which is why R16 and R17 sit near a quarter despite good code on both ends. Green unit tests are exactly what makes this easy to miss — they exercise the components, not the path between them.

**Nothing on the branch can tell you it worked.** A warning that arrives, decodes and reaches the UI writes no log line at all; only failures speak. Combined with a `schemaVersion` that no code reads, that leaves no way to distinguish "received and rendered a valid message" from "received nothing" on a live node — which is the exact question a demo has to answer. The `IVI_V2X` tag and the `[RX]` line are not paperwork; they are how anyone, you included, will debug this in the Room.

**The frozen contract moved to fit the test equipment, rather than the other way round.** The mock sender omits `class` and sends `vehicles.B` where the schema says `vehicles.vehicleB`; the response was to give six required R3 fields defaults and add a third member to a two-member sealed hierarchy. The contracts are frozen precisely so a disagreement between two implementations surfaces as a decode failure instead of being absorbed — and the ADA side is written against the schema, so these defaults hide a mismatch rather than resolve one. It is worth reading `ff032ff`'s message in full; the direction of drift it describes is the same one, and the state path is already broken today because of it.

**The branch is working from a different plan and a different design than the repo's.** `plans/phase5_tasks.md` here and `phase5_minh_tasks.md` on `main` assign the same four IDs to different work, and `IVI_ECU/doc/` — the HLD, the decision record, the diagrams — is on `main` but not here. Almost everything in the architecture table traces back to that: the five-module split, `IviRuntimeConfig` and port 47300, `WarningClassifier` owning the registry, `AndroidR4Logger` and the `IVI_V2X` tag, `IviGraph` instead of Hilt, `r4-simulator` instead of `mock-sender`. None of that was knowable from where this branch stands, and syncing with `main` before the next push will resolve more of this list than any amount of rework here — the more so now that Phase 4 has landed a real ADA sender to talk to.

Merging as-is would put an unlaunchable APK and a loosened frozen contract on `main`, so this is not ready yet — but the distance is shorter than the length of this review suggests. The judgement calls in the data layer are good ones, and most of what is left is connecting work and reconciling with `main`'s design rather than rethinking anything. Nice work on the parser in particular; it is the part that is hardest to get right and you got it right.
