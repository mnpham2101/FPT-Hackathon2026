# Phase 5 — IVI HMI (R4, R16, R17): Implementation & Test Plan (v2)

> **Authority & context:**
>
> - **Phase content:** [milestone1.md § Phase 5](milestone1.md#phase-5--ivi-hmi-mock-driven-r16-r17--display-track-parallel-from-the-start) — its acceptance checkboxes are the phase output, and it names this phase as the home of the system verification test.
> - **Design:** [ivi-ecu-hld.md](../IVI_ECU/doc/ivi-ecu-hld.md) — the node's sole design authority. Every path below is cited from its **[§4](../IVI_ECU/doc/ivi-ecu-hld.md#4-folder-structure)**; component responsibilities **[§6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components)**; seams and ports **[§8](../IVI_ECU/doc/ivi-ecu-hld.md#8-interfaces-ports-and-the-layer-rule)**; the R4 contract **[§10](../IVI_ECU/doc/ivi-ecu-hld.md#10-the-contract--r4-the-message-set-from-ada-ecu)**; stack, build and CI **[§11](../IVI_ECU/doc/ivi-ecu-hld.md#11-tech-stack-build-and-ci)**; test configurations and observables **[§12](../IVI_ECU/doc/ivi-ecu-hld.md#12-test-strategy)**; decisions **D1–D12** ([ivi-ecu-design-decisions.md](../IVI_ECU/doc/ivi-ecu-design-decisions.md)).
> - **Requirements:** [m1-cooperative-awareness.md §2](../requirements/m1-cooperative-awareness.md) — R4, R16, R17 in full, plus R5, R6, R18, R19 where this phase touches them. Referenced by number, never restated.
> - **Bring-up procedure:** [deploy-ivi-hmi-walkthrough.md](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md) — authoritative for build, retrieval, deploy, install, launch and verification. Part II decomposes from it per stage 2 of [walkthrough-driven-delivery.md](../.claude/rules/walkthrough-driven-delivery.md); its [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human) work-division table decides every executor label.
> - **Research notes:** [phase5-r4-parsing.md](../IVI_ECU/doc/research_notes/phase5-r4-parsing.md) · [phase5-r4-simulator.md](../IVI_ECU/doc/research_notes/phase5-r4-simulator.md) — non-authoritative; the HLD wins on conflict.
> - **Rules:** [task-planning-conventions.md](../.claude/rules/task-planning-conventions.md) · [node-code-layout.md](../.claude/rules/node-code-layout.md).
>
> **Task ID legend:** `X.5.Z.W` — X = requirement served · 5 = this phase · Z = task group · W = subtask. IDs are stable; never renumber.

## Phase 5 overview

**Objective.** The IVI node renders the R17 God View — ego, B and ghost C — from R4 messages alone, inside the R16 layout, on a launchable APK; the R4 simulator and a 3-node mini-blueprint produce the traffic and the in-Room evidence that closes the phase; the 5-node blueprint then shows the same behaviour on live relayed data.

**Input (must exist before start):**

- R4 and R3 frozen: [contracts/r4-ada-ivi.schema.json](../contracts/r4-ada-ivi.schema.json), [contracts/r3-tracked-object.schema.json](../contracts/r3-tracked-object.schema.json), the samples under [contracts/samples/](../contracts/samples/), and [contracts/sync-manifest.json](../contracts/sync-manifest.json) with its `check_sync.py` gate.
- The IVI HLD, its decision record D1–D12, and the two research notes.
- CarSky access, the baseline blueprint `baseline_phase1` ([carsky-4-node-blueprint.md](../requirements/car-sky-guide/carsky-4-node-blueprint.md#8-the-blueprints-on-carsky)), the AAOS artifact, and `registry.hackathon-2.carsky.io/m1-netcheck:latest` pushed.
- GitHub secret `CARSKY_ZOT_API_KEY` and the [verify-arm64-image](../.github/actions/verify-arm64-image) action.
- The organizers' `reach-backend` CLI, its gateway URL and its derived token — supplied, not derivable (walkthrough [§6.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) items 2–4).

**Output — phase acceptance:**

- [ ] The HMI runs on the AAOS node with the R16 layout; the button and app areas switch what the Display Area shows.
- [ ] An R4 warning brings the Warning View up showing ego, B and ghost C at the composed positions.
- [ ] Ghost C renders from `v2x_relayed` data only; the 2D drawing is delivered (3D stays optional).
- [ ] A newer message with an unknown `warningType` degrades gracefully.
- [ ] Optional paths, only if built: an ADA message wakes a separate warning app; 3D renders through the view seam.

Per-subtask traceability: § Acceptance traceability.

**Suggested branch:** `feat/phase5-ivi-hmi`. Creating, checking out and pushing it is the user's call.

**Structure.** Part I is the implementation plan — groups 5.1–5.7, all agent work. Part II is the test plan — the test levels, the observable matrix, and groups 5.8–5.9, the two Room configurations of [HLD §12](../IVI_ECU/doc/ivi-ecu-hld.md#12-test-strategy).

## Conventions

### Execution labels

| Label | Who does it |
|---|---|
| *agent* | A spawned implementation subagent. The default for code, tests, CI and docs. |
| *car-sky* | The [[car-sky]] agent: authenticated REST calls, `adb` commands, log reads. The planner keeps the ID and the done-tracking. |
| *Human* | A person, at the Nydus canvas or the Devices panel. No agent performs these. The evidence commit is made by the orchestrating session once the person confirms. |

### Subtask discipline

Per [task-planning-conventions.md § Subtask discipline](../.claude/rules/task-planning-conventions.md#subtask-discipline-non-negotiable): **single objective · no out-of-scope code · exactly one atomic commit with the stated message · build passes · unit tests pass · the brief is self-contained.** Implementation subagents inherit this as their definition of done.

### The existing-file rule

This plan is decomposed from the design, not from the tree. A subtask that names a file it must produce **checks whether that file already exists before writing it**, and takes one of three routes:

- **Absent** — create it to the brief.
- **Present and already satisfying the brief** — the subtask degrades to verification: run the stated acceptance, commit nothing but a status line, and say so.
- **Present and diverging from the brief** — bring it to the brief. Where the divergence is in a *committed contract artifact* or a *committed test*, stop and report instead: a frozen artifact is re-frozen across every consumer, never edited inside a subtask.

Relocations are `git mv` and must show as pure renames under `git diff -M --stat`; a moved test passes unchanged or the move is wrong.

### Standing constraints every `IVI_ECU/` subtask inherits

- **No hardcoded tunables** (CLAUDE.md principle 5): ports, buffer sizes, timeouts, cadences and scales come from `BuildConfig` plus the launch override (D10), or from the simulator's env and scenario file. Never a literal in a class.
- **No module declares its own repositories.** `settings.gradle.kts` sets `RepositoriesMode.FAIL_ON_PROJECT_REPOS` (D8).
- **Android types exist only in `:app`** (D2). `:contract`, `:serializer`, `:observer` and `:r4-simulator` compile with no Android SDK present.
- **No `android.util.Log` on a unit-tested path.** The stubbed Android jar throws from every `Log` method. `:serializer` and `:observer` log through the `R4Logger` seam; in `:app`, log through `AndroidR4Logger` and inject a recording logger in tests.
- **Nothing mounts `WarningBannerOverlay`** (D11).

### Build & verification commands

From `IVI_ECU/` (`gradlew.bat` on the Windows dev host, `./gradlew` on CI and Linux). The command set is [HLD §11](../IVI_ECU/doc/ivi-ecu-hld.md#11-tech-stack-build-and-ci).

| Target | Command |
|---|---|
| One module's tests | `./gradlew :contract:test` · `:serializer:test` · `:observer:test` · `:r4-simulator:test` |
| App unit tests | `./gradlew :app:testDebugUnitTest` |
| Full suite | `./gradlew :contract:test :serializer:test :observer:test :r4-simulator:test :app:testDebugUnitTest` |
| APK | `./gradlew assembleDebug` → `app/build/outputs/apk/debug/app-debug.apk` |
| Lint | `./gradlew lint` |
| Contract integrity gate | from the repo root: `python contracts/check_sync.py` → exit 0 |
| Simulator image | `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -f IVI_ECU/r4-simulator/Dockerfile -t m1-r4-sim:latest IVI_ECU/` |

A module's test target is only valid once that module exists; before group 5.1 completes, `:app:testDebugUnitTest` is the whole suite.

### Two constants no subtask may get wrong

The IVI node's address is **`10.99.0.13`** and its UDP port is **`47300`**, frozen by R6 and the blueprint topology ([HLD §10](../IVI_ECU/doc/ivi-ecu-hld.md#10-the-contract--r4-the-message-set-from-ada-ecu)). Any other value reaching a class, a scenario file or a node config is a defect, whatever its source.

### Status tracking

Each subtask gains a `**Status:**` line appended in that subtask's own atomic commit, recording done or blocked plus its verification evidence. A subtask with no status line is not started. Nothing in this file is started.

---

# Part I — Implementation plan

## Task Group 5.1 — Module graph and the `:contract` layer (serves R4)

> [HLD §4](../IVI_ECU/doc/ivi-ecu-hld.md#4-folder-structure) five-module tree under one version catalog (**D8**), and the pure-JVM contract module (**D1**, **D2**) whose frozen samples are main resources (**D6**). This group gates every other code group.

### [ ] `4.5.1.1` — Version catalog, root build and settings *(agent)*

**Objective:** make one file the source of every dependency and plugin version for all five modules (D8).

**Scope:**

- `IVI_ECU/gradle/libs.versions.toml` — `[versions]`, `[libraries]` and `[plugins]` covering everything [HLD §11](../IVI_ECU/doc/ivi-ecu-hld.md#11-tech-stack-build-and-ci) names: Kotlin 2.2.20, AGP 8.13, Compose BOM 2024.09.03, Material3 and AndroidX through the BOM, `androidx-activity-compose`, `lifecycle-viewmodel-compose`, kotlinx-serialization-json, kotlinx-coroutines-core and `-test`, JUnit 4. Plugin aliases: `android-application`, `kotlin-android`, `kotlin-jvm`, `kotlin-serialization`, `kotlin-compose`.
- `IVI_ECU/build.gradle.kts` — every plugin declared `apply false` through `alias(libs.plugins.…)`. No version literal survives outside the catalog.
- `IVI_ECU/settings.gradle.kts` — `RepositoriesMode.FAIL_ON_PROJECT_REPOS` in `dependencyResolutionManagement`, and `include(":app")`. The four other modules are included by the subtasks that create them, so the build never references a project that does not exist.
- **Remove any annotation-processor or dependency-injection framework the design does not use** (D7 — the graph is wired by hand in `IviGraph`). Grep `IVI_ECU/` for `dagger`, `hilt` and `ksp` first: a hit at an `@Inject` or `@HiltAndroidApp` site is a finding to report, not to force through.

**Acceptance:** `./gradlew projects` succeeds; `./gradlew :app:testDebugUnitTest` green; no module declares a version the catalog also declares, and no `repositories { }` block exists in any module.

**Dependencies:** none — the first subtask of the phase. **Commit:** `[4.5.1.1] chore: add the IVI version catalog, root plugin aliases and settings`

### [ ] `4.5.1.2` — `:contract` module and `R4Contract` *(agent)*

**Objective:** stand up the pure-JVM contract module (D1) with its constants file.

**Scope:**

- `settings.gradle.kts`: `include(":contract")`.
- `IVI_ECU/contract/build.gradle.kts` — `alias(libs.plugins.kotlin.jvm)` + `alias(libs.plugins.kotlin.serialization)`; **`api(libs.kotlinx.serialization.json)`**, because `:app` and `:r4-simulator` use `R4Json` and the `@Serializable` types directly; `testImplementation(libs.junit)`; JVM toolchain 17. **Zero Android dependencies** — the module must compile with no Android SDK present (D2).
- `contract/src/main/kotlin/com/hackathon/v2x/ivi/model/R4Contract.kt` — `object R4Contract` holding `KNOWN_SCHEMA_VERSION` (the value the frozen `r4-warning.json` carries), the frozen sample resource paths as constants (`/contracts/samples/r4-warning.json`, `r4-state.json`, `r4-unknown-warning.json`, `r3-tracked-object.json`), and the M1 warning-registry key `nlos_obstruction` ([HLD §10](../IVI_ECU/doc/ivi-ecu-hld.md#10-the-contract--r4-the-message-set-from-ada-ecu)).

**Acceptance:** `./gradlew :contract:build` green; `:app:testDebugUnitTest` still green; `contract/build.gradle.kts` names no `com.android.*` plugin.

**Dependencies:** after `4.5.1.1`. **Commit:** `[4.5.1.2] feat: add the pure-JVM :contract module with R4Contract constants`

### [ ] `4.5.1.3` — The R4/R3 models and `R4Json` *(agent)*

**Objective:** bind the frozen R4 message set and its R3 snapshot, field for field, in `:contract`.

**Scope — four files under `contract/src/main/kotlin/com/hackathon/v2x/ivi/model/`, bound against [contracts/r4-ada-ivi.schema.json](../contracts/r4-ada-ivi.schema.json) and [contracts/r3-tracked-object.schema.json](../contracts/r3-tracked-object.schema.json), not against this brief's prose:**

- `R4Message.kt` — a sealed `R4Message` with `@SerialName("warning")` and `@SerialName("state")` subclasses carrying the fields [HLD §10](../IVI_ECU/doc/ivi-ecu-hld.md#10-the-contract--r4-the-message-set-from-ada-ecu)'s two tables list; plus `R4Json`, a `Json` configured `classDiscriminator = "type"`, `ignoreUnknownKeys = true`, **`isLenient = false`**. Leniency would hide producer bugs; unknown-field tolerance is a different switch and is the one the contract asks for.
- `R3Snapshot.kt` — the R3 TrackedObject binding, **including `source`**, which is what the R19 claim rests on (D12).
- `SceneGeometry.kt` — `SceneGeometry` and `VehiclePosition`. `vehicleC` is **nullable** — `null` is a normal state (C not yet tracked), never an error. `SceneGeometry` also carries the nullable `vehicleCSnapshot` the provenance guard reads (D12).
- **The three consumer obligations of [HLD §10](../IVI_ECU/doc/ivi-ecu-hld.md#10-the-contract--r4-the-message-set-from-ada-ecu) are structural, not error paths:** unknown fields are ignored, a newer `schemaVersion` is accepted, and an unknown `warningType` stays a plain `String` whose wire value is preserved verbatim (D4).

**Acceptance:** `./gradlew :contract:build` green; every field of both schema files has a binding, and no binding invents a field neither schema declares.

**Dependencies:** after `4.5.1.2`. **Commit:** `[4.5.1.3] feat: bind the R4 message set and R3 snapshot in :contract`

### [ ] `4.5.1.4` — Frozen samples as `:contract` main resources, and the sync manifest *(agent)*

**Objective:** put the frozen fixtures in the one place the contract tests, the simulator and the dev injector all reach (D6), and keep the integrity gate green in the same commit.

**Scope:**

- `contract/src/main/resources/contracts/samples/` — byte-identical copies of `r3-tracked-object.json`, `r4-warning.json`, `r4-state.json`, `r4-unknown-warning.json`. **`main`, not `test`**: the simulator and the injector read them off the same classpath (D6).
- `IVI_ECU/contracts/r4-ada-ivi.schema.json` and `r3-tracked-object.schema.json` — byte-synced copies of the normative schemas ([HLD §10](../IVI_ECU/doc/ivi-ecu-hld.md#10-the-contract--r4-the-message-set-from-ada-ecu), node copy row).
- **Same commit, out-of-folder edit:** register every one of those six targets in [contracts/sync-manifest.json](../contracts/sync-manifest.json) under its source entry. **This cannot be deferred** — an unregistered or stale target makes `check_sync.py` report a mismatch and the contracts gate goes red the moment the files land.
- Accepted cost, recorded by D6: ~2 KB of fixtures ship inside the release APK.

**Acceptance:** `python contracts/check_sync.py` exits 0 from the repo root; every sample and both schema copies are byte-identical to their source; `./gradlew :contract:build` green.

**Dependencies:** after `4.5.1.2`. **Commit:** `[4.5.1.4] chore: sync the frozen R4/R3 samples and schemas into :contract`

### [ ] `4.5.1.5` — R4's own acceptance tests: round-trip and additive version *(agent)*

**Objective:** deliver the two tests [HLD §12](../IVI_ECU/doc/ivi-ecu-hld.md#12-test-strategy) names as R4's acceptance, and R4's acceptance clause requires.

**Scope — `contract/src/test/kotlin/com/hackathon/v2x/ivi/model/`:**

- `R4RoundTripTest.kt` — **every** frozen sample decodes through `R4Json` into its typed subclass and re-encodes to an equal document. This is what makes this side of the contract match the producer's.
- `R4AdditiveVersionTest.kt` — `r4-unknown-warning.json`, which carries a newer `schemaVersion`, an unknown `warningType` and an unknown extra field, **decodes successfully**; the unknown `warningType` is readable **verbatim** off the parsed message and is not equal to `R4Contract`'s M1 key; the extra field is ignored rather than failing the decode. Rewriting the wire value to `"unknown"` fails this test by design (D4).
- Both read fixtures with `getResourceAsStream("/contracts/samples/…")` so they resolve off `:contract`'s own main resources.

**Acceptance:** `./gradlew :contract:test` green with both classes running; the additive test asserts preservation, not replacement.

**Dependencies:** after `4.5.1.3` and `4.5.1.4`. **Commit:** `[4.5.1.5] test: add the R4 round-trip and additive-version contract tests`

### [ ] `4.5.1.6` — `:app` on the catalog, wired to `:observer`, with ProGuard keeps *(agent)*

**Objective:** make the Android module resolve everything through the catalog, depend on the module graph, and keep serialization working in a minified build.

**Scope:**

- `app/build.gradle.kts` — plugins and dependencies through `alias(libs.plugins.…)` / `libs.…` only; `implementation(project(":observer"))` (which transitively `api`s `:serializer` and `:contract`, per D2's one-way graph); `compileSdk`, `minSdk 29`, `targetSdk 33` and JDK 17 per [HLD §11](../IVI_ECU/doc/ivi-ecu-hld.md#11-tech-stack-build-and-ci); `testImplementation(libs.junit)` and `testImplementation(libs.kotlinx.coroutines.test)`; `buildFeatures { compose = true; buildConfig = true }`.
- `app/proguard-rules.pro` — the kotlinx-serialization keep set scoped to `com.hackathon.v2x.ivi.model.**`: generated `$$serializer` classes and fields, `Companion.serializer()`, and the `INSTANCE`/`Companion` of `@Serializable` classes. Nothing wider, and `isMinifyEnabled` is not changed.

**Acceptance:** `./gradlew assembleDebug` and `./gradlew :app:assembleRelease` both succeed (unsigned release output is fine); `:app:testDebugUnitTest` green; the keep rules name only the `model` package.

**Dependencies:** after `4.5.3.1` (the `:observer` project must exist to be depended on). Everything else in this subtask is independent of it. **Commit:** `[4.5.1.6] chore: put :app on the catalog with the module graph and serialization keeps`

---

## Task Group 5.2 — `:serializer` — datagram bytes to a typed R4 result (serves R4)

> **D3** (de-framing is buffer slicing, not header parsing) and [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components). Pure Kotlin/JVM: it **never logs and never throws across the receive loop** — it returns a result and the observer decides what to log.

### [ ] `4.5.2.1` — Module `:serializer` and the decode seam *(agent)*

**Objective:** declare the module and the decode types of [HLD §8](../IVI_ECU/doc/ivi-ecu-hld.md#8-interfaces-ports-and-the-layer-rule), with no implementation behind them.

**Scope:**

- `settings.gradle.kts`: `include(":serializer")`. `IVI_ECU/serializer/build.gradle.kts` — `alias(libs.plugins.kotlin.jvm)`; `api(project(":contract"))`; `testImplementation(libs.junit)`; toolchain 17; no Android, no repositories block.
- `serializer/src/main/kotlin/com/hackathon/v2x/ivi/serializer/R4Decoder.kt` — the seam and its result type:

  ```kotlin
  interface R4Decoder { fun decode(buffer: ByteArray, offset: Int, length: Int): R4DecodeResult }
  sealed interface R4DecodeResult {
      data class Decoded(val message: R4Message, val schemaVersionAhead: Boolean) : R4DecodeResult
      data class Failed(val reason: DecodeFailure, val detail: String, val preview: String) : R4DecodeResult
  }
  enum class DecodeFailure { EMPTY, UNKNOWN_MESSAGE_TYPE, MALFORMED }
  ```

**Acceptance:** `./gradlew :serializer:build` green; the file declares exactly the three types above.

**Dependencies:** after `4.5.1.3`. **Commit:** `[4.5.2.1] feat: add the :serializer module and the R4 decode seam`

### [ ] `4.5.2.2` — `R4Deserializer` and `PayloadPreview`, with the decode-failure table under test *(agent)*

**Objective:** implement `R4Decoder` so every row of the [parsing note §2](../IVI_ECU/doc/research_notes/phase5-r4-parsing.md) table maps to the right result, and nothing escapes as an exception.

**Scope — three files:**

- `serializer/src/main/kotlin/…/PayloadPreview.kt` — `fun preview(buffer, offset, length, maxChars): String`, a **bounded single-line** rendering for the log: non-printable bytes escaped, newlines and tabs replaced, truncated with an ellipsis at `maxChars`, whose default is a named constant in this file. It never returns the whole datagram.
- `serializer/src/main/kotlin/…/R4Deserializer.kt` — `class R4Deserializer : R4Decoder`:
  1. Slice `buffer[offset until offset + length]` — **never** the whole backing array (D3 row 1).
  2. Decode UTF-8; strip a leading BOM (`EF BB BF`) and surrounding whitespace (D3 row 4).
  3. Empty or blank after trimming → `Failed(EMPTY, …)`.
  4. `R4Json.decodeFromString(...)` inside `try`/`catch (SerializationException)`.
  5. Success → `Decoded(message, schemaVersionAhead = message.schemaVersion > R4Contract.KNOWN_SCHEMA_VERSION)`.
  6. An unresolved polymorphic discriminator → `Failed(UNKNOWN_MESSAGE_TYPE, …)`; every other `SerializationException` — malformed, truncated, wrong field type, missing required field → `Failed(MALFORMED, …)`. Catch `Throwable` at the outer edge and map to `MALFORMED`; nothing propagates.
  7. Convenience overloads `decode(bytes)` and `decode(text)` for tests.
  - `R4Json` is consumed as-is from `:contract`; **do not construct a second `Json`**, and add no runtime JSON-Schema validation — the typed decode already enforces required fields and types, and the schema is enforced in `4.5.1.5`.
- `serializer/src/test/kotlin/…/R4DeserializerTest.kt` — one case per table row, reading fixtures off the `:contract` classpath: `r4-warning.json` → `Decoded` warning with `schemaVersionAhead == false`; `r4-state.json` → `Decoded` state; `r4-unknown-warning.json` → `Decoded` with the wire `warningType` **preserved verbatim** and `schemaVersionAhead == true`; an unknown `type` discriminator → `UNKNOWN_MESSAGE_TYPE`; non-JSON bytes → `MALFORMED`; a truncated prefix of a valid sample → `MALFORMED`; a wrong-typed field → `MALFORMED`; a warning with a required object removed → `MALFORMED`; empty and all-whitespace input → `EMPTY`. Assert **no case throws**, and that every `Failed.preview` is non-empty and single-line.

**Acceptance:** `./gradlew :serializer:test` green with every row covered; a grep of `serializer/src/main` finds no `println`, no logging import and no `throw`.

**Dependencies:** after `4.5.2.1`. **Commit:** `[4.5.2.2] feat: implement R4Deserializer with the full decode-failure table`

### [ ] `4.5.2.3` — Buffer-slicing, BOM and bounds tests *(agent)*

**Objective:** prove the D3 buffer discipline the decode-table test cannot reach.

**Scope — `serializer/src/test/kotlin/…/BufferSlicingTest.kt`:**

- A large buffer pre-filled with garbage, a valid payload written at a **non-zero offset**, decoded with that `offset`/`length` → `Decoded`. Decoding the buffer whole would fail; that is the point.
- The same buffer reused for a second, **shorter** payload → still `Decoded`, proving the previous message's trailing bytes are not appended.
- A payload prefixed with a UTF-8 BOM → `Decoded`.
- A payload wrapped in leading and trailing whitespace and a trailing newline → `Decoded`.
- `length = 0` → `EMPTY`; `offset + length` beyond the array → `MALFORMED`, never an exception.

**Acceptance:** `./gradlew :serializer:test` green with all six cases present.

**Dependencies:** after `4.5.2.2`. **Commit:** `[4.5.2.3] test: cover offset/length slicing, BOM handling and out-of-bounds input`

---

## Task Group 5.3 — `:observer` — socket, receive loop, back-off, event flow (serves R4, R6)

> **D5** (the loop is plain-JVM code; the service is only its lifecycle host) and [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components). Plain Kotlin/JVM so the whole receive path runs in CI with no device and **no Robolectric** (D2). This module never imports `android.util.Log`; it logs through the `R4Logger` seam.

### [ ] `4.5.3.1` — Module `:observer` with its seams, events and config *(agent)*

**Objective:** declare the module and its four value and seam files, with no loop yet.

**Scope:**

- `settings.gradle.kts`: `include(":observer")`. `IVI_ECU/observer/build.gradle.kts` — `alias(libs.plugins.kotlin.jvm)`; `api(project(":serializer"))`; `api(libs.kotlinx.coroutines.core)`; `testImplementation(libs.junit)` and `testImplementation(libs.kotlinx.coroutines.test)`; toolchain 17; no Android.
- `observer/src/main/kotlin/…/R4DatagramSource.kt` — the source seam: `bind()`, `receive(): Received`, `close()`, and `data class Received(val buffer: ByteArray, val offset: Int, val length: Int)`. Interface only.
- `…/R4Event.kt` — `sealed interface R4Event` with `Message(message, receivedAtMs, bytes)` and `Dropped(reason, detail, bytes)`; plus `sealed interface R4LinkState` with `Bound(port)`, `Rebinding` and `Error(detail)`, which the status bar of `17.5.5.6` binds to.
- `…/R4ObserverConfig.kt` — `data class R4ObserverConfig(port, bufferBytes, flowBufferEvents, retryInitialMs, retryMaxMs)`. **No default values and no literals** — every field is supplied by `IviRuntimeConfig` (D10).
- `…/R4Logger.kt` — `fun interface R4Logger { fun log(level: R4LogLevel, line: String) }`, `enum class R4LogLevel { INFO, WARN, ERROR }`, and a no-op implementation for tests and for the plain-JVM default. `:app` supplies the real one at `18.5.5.1`.

**Acceptance:** `./gradlew :observer:build` green; a grep of `observer/src/main` finds zero `android.` or `androidx.` imports.

**Dependencies:** after `4.5.2.1`. **Commit:** `[4.5.3.1] feat: add the :observer module with its datagram seam, events and config`

### [ ] `4.5.3.2` — `JdkDatagramSource` — the only socket holder *(agent)*

**Objective:** implement `R4DatagramSource` over `java.net.DatagramSocket`, owning the one rule that silently truncates every datagram if forgotten.

**Scope — `observer/src/main/kotlin/…/JdkDatagramSource.kt`:**

- Constructed with `port` and `bufferBytes` from `R4ObserverConfig` — no literals.
- `bind()` binds **`0.0.0.0:<port>`**, never the node address, which the bridge assigns (D5, [HLD §8](../IVI_ECU/doc/ivi-ecu-hld.md#8-interfaces-ports-and-the-layer-rule)). It allocates one reusable buffer and one reusable `DatagramPacket`.
- `receive()` calls **`packet.setLength(buffer.size)` before every `socket.receive(packet)`** (D3 row 2 — without it every datagram after the first is truncated to the shortest seen), then returns `Received(packet.data, packet.offset, packet.length)`.
- `close()` is idempotent and unblocks a pending `receive()`.
- Bind failure **propagates** — `R4SocketObserver` turns it into `Rebinding`; it is not swallowed here.
- Test `observer/src/test/kotlin/…/JdkDatagramSourceTest.kt`: bind on port `0`, send a long datagram then a **shorter** one from a local socket, and assert the second `Received.length` equals the second payload's length. Without the `setLength` call this test fails; it is what holds the rule.

**Acceptance:** `./gradlew :observer:test` green; the `setLength` call is textually inside `receive()`, before `socket.receive(...)`.

**Dependencies:** after `4.5.3.1`. **Commit:** `[4.5.3.2] feat: add JdkDatagramSource with per-receive packet length reset`

### [ ] `4.5.3.3` — `R4SocketObserver` — the receive loop and its event flow *(agent)*

**Objective:** implement the loop so N datagrams in produce N events out, and one bad message never stops the next good one.

**Scope — `observer/src/main/kotlin/…/R4SocketObserver.kt`:**

```kotlin
class R4SocketObserver(
    private val config: R4ObserverConfig,
    private val decoder: R4Decoder,
    private val sourceFactory: () -> R4DatagramSource,
    private val logger: R4Logger,
) { val events: SharedFlow<R4Event>; val linkState: StateFlow<R4LinkState>; fun start(scope: CoroutineScope): Job; fun stop() }
```

- `events` is a `MutableSharedFlow` with `extraBufferCapacity = config.flowBufferEvents` and `BufferOverflow.DROP_OLDEST`, emitted with **`tryEmit`, never a suspending emit** (D5 back-pressure): the newest warning is the one that matters and a slow collector must never stall the socket.
- `start(scope)` launches on `Dispatchers.IO`. On bind success `linkState = Bound(port)` and the logger emits `[LINK] state=bound port=<p>`.
- Per datagram: if `length == config.bufferBytes`, log a truncation-suspect WARN and **still attempt the decode** (D3 row 3). Then `decoder.decode(buffer, offset, length)` — `Decoded` → the `[RX]` line and `tryEmit(R4Event.Message(...))`; `Failed` → `[DROP] reason=… bytes=… preview="…"` and `tryEmit(R4Event.Dropped(...))`. `schemaVersionAhead` is logged **once per observer lifetime**, not per message (D4).
- The `[RX]` line for a warning carries `type=`, `bytes=`, `from=`, `warningType=`, `risk=`, `cSource=` and `cPos=`, read off the **parsed** message ([HLD §12](../IVI_ECU/doc/ivi-ecu-hld.md#12-test-strategy)). `cSource` on every rendered warning is what backs the R19 claim in text.
- **No accumulate-and-split logic anywhere** (D3 row 5) — UDP preserves message boundaries.
- `stop()` closes the source and cancels the job; `linkState` keeps its last value.
- Test `…/R4SocketObserverTest.kt` against a **fake** source, no socket: N valid datagrams in → N `Message` events out in order; one malformed among them → one `Dropped` and the following good message still arrives; a source whose `receive()` throws once → the loop does not die; a recording `R4Logger` captures the `[LINK]`, `[RX]` and `[DROP]` lines and their shapes are asserted.

**Acceptance:** `./gradlew :observer:test` green; `tryEmit` is the only emit call in the loop; the three log shapes match [HLD §12](../IVI_ECU/doc/ivi-ecu-hld.md#12-test-strategy).

**Dependencies:** after `4.5.3.2`. **Commit:** `[4.5.3.3] feat: implement the R4 receive loop with typed events and truncation checks`

### [ ] `4.5.3.4` — Bounded rebind back-off, and a reachable `Error` state *(agent)*

**Objective:** make a socket error a recoverable, bounded-back-off rebind instead of a dead listener.

**Scope — extend `R4SocketObserver`; no new production file:**

- On a bind or receive error: log at ERROR, `linkState = Rebinding`, close the source, `delay(d)`, recreate through `sourceFactory()`, retry. `d` starts at `config.retryInitialMs`, doubles to a ceiling of `config.retryMaxMs`, and **resets to `retryInitialMs` on the next successful bind**, which returns `linkState` to `Bound`.
- **`R4LinkState.Error` must be reachable.** A declared state nothing emits is a defect. Once the back-off has saturated at `retryMaxMs` — long enough to rule out a transient blip — set `linkState = Error(detail)` and **keep retrying**: the observer never gives up, because a listener that stops after N attempts is worse for a recorded run than one that keeps trying. A later success returns it to `Bound`.
- Test `…/RetryBackoffTest.kt` on `kotlinx-coroutines-test` **virtual time**: a factory failing the first three binds then succeeding → the observed delays are `initial, 2×initial, 4×initial` clamped at `retryMaxMs`, `linkState` passes `Rebinding → Bound`, and a later failure restarts at `retryInitialMs`. A second case: a factory that keeps failing past the ceiling → `linkState` reaches `Error`, retries continue, and a later success returns it to `Bound`.

**Acceptance:** `./gradlew :observer:test` green; no `Thread.sleep` anywhere — the test runs on virtual time.

**Dependencies:** after `4.5.3.3`. **Commit:** `[4.5.3.4] feat: add bounded exponential rebind back-off to the R4 observer`

### [ ] `4.5.3.5` — Loopback socket test — the whole receive path, no device *(agent)*

**Objective:** prove the observer end to end over a **real** `DatagramSocket`, with no device and no Robolectric.

**Scope — `observer/src/test/kotlin/…/LoopbackSocketTest.kt`:**

- A real `JdkDatagramSource` on an ephemeral port on `127.0.0.1`, wired into a real `R4SocketObserver` with a real `R4Deserializer`, collecting `events`.
- The frozen `r4-warning.json` bytes sent from a plain `DatagramSocket` → one `R4Event.Message` whose warning carries the R3 snapshot with `source == "v2x_relayed"`.
- `r4-unknown-warning.json` → a `Message` whose unknown `warningType` is preserved verbatim — D4 proven end to end.
- Non-JSON bytes → one `Dropped(MALFORMED, …)`, and a following valid datagram still produces a `Message`: the loop survived.
- Bounded waits with a clear timeout failure message. The test must not hang CI.

**Acceptance:** `./gradlew :observer:test` green on a machine with no Android SDK; no Robolectric dependency is added anywhere.

**Dependencies:** after `4.5.3.4`. **Commit:** `[4.5.3.5] test: add the loopback socket test for the R4 receive path`

---

## Task Group 5.4 — `:app` data and logic layer (serves R4, R16, R17)

> [HLD §3 MVC separation](../IVI_ECU/doc/ivi-ecu-hld.md#3-the-component-architecture) and [§6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components). Everything here is plain Kotlin, testable by `:app:testDebugUnitTest` with no device. Group 5.5 then hosts it in an Activity, a service and a screen.

### [ ] `4.5.4.1` — `IviRuntimeConfig` and the `BuildConfig` defaults (D10) *(agent)*

**Objective:** make every Phase 5 tunable a compile-time default that a launch-time intent extra can override, in one place.

**Scope:**

- `app/build.gradle.kts` `defaultConfig` — one `buildConfigField` per row of **D10**: `R4_UDP_PORT` (`47300`, blueprint-frozen), `R4_SOCKET_BUFFER_BYTES`, `R4_FLOW_BUFFER_EVENTS`, `R4_RETRY_INITIAL_MS`, `R4_RETRY_MAX_MS`, `WARNING_TIMEOUT_MS`, `SCENE_SCALE_M_PER_PX`. The defaults are D10's, not this brief's.
- `app/src/main/java/…/config/IviRuntimeConfig.kt` — a data class carrying every resolved value, plus `fun resolve(intent: Intent?): IviRuntimeConfig` applying D10's launch overrides (`--ei r4_port`, `--el warning_timeout_ms`, `--ef scene_scale`). An invalid or out-of-range extra — a port outside 1–65535, a non-positive timeout or scale — is ignored in favour of the default. Add `fun toObserverConfig(): R4ObserverConfig`.
- **This is the only class in the app that reads `BuildConfig`** (D10). Every other component receives resolved values.
- Test `app/src/test/java/…/config/IviRuntimeConfigTest.kt` — a null intent yields the defaults; each override key is applied; each out-of-range value falls back; `toObserverConfig()` carries the resolved port and buffer sizes. Extract the extras read into an internal overload taking a plain map so the test needs no Android `Intent`.

**Acceptance:** `./gradlew :app:testDebugUnitTest` green; a grep of `app/src/main` shows `BuildConfig.` referenced only inside `IviRuntimeConfig.kt`; the port default is `47300`.

**Dependencies:** after `4.5.3.1` and `4.5.1.6`. **Commit:** `[4.5.4.1] feat: add IviRuntimeConfig with BuildConfig defaults and launch overrides`

### [ ] `4.5.4.2` — `R4Repository` — the event raiser and the single routing point *(agent)*

**Objective:** collect the observer's events once, on the application scope, and expose them as the app's data layer.

**Scope — `app/src/main/java/…/data/R4Repository.kt`:**

- Constructed from the observer's `events` and `linkState` flows plus a `CoroutineScope` — flows rather than the observer itself, so the repository is testable with no socket.
- Exposes `warnings` (a `SharedFlow` with replay 1, so a late collector sees the current warning), `lastState` (a `StateFlow`, **last-value-wins by `seq`**: a message whose `seq` is not greater than the stored one is discarded), `linkState` (passthrough) and `droppedCount`.
- Exposes `fun inject(event: R4Event)` — the **single injection target** the dev injector uses (`4.5.6.7`), so the injected path is exactly the path a real datagram takes.
- **It stores and routes. It never decides what a warning means and never formats anything** ([HLD §3](../IVI_ECU/doc/ivi-ecu-hld.md#3-the-component-architecture) Data-layer rule).
- Test `app/src/test/java/…/data/R4RepositoryTest.kt` — a warning `Message` appears on `warnings` and does not touch `lastState`; a state `Message` updates `lastState`; a state with a lower or equal `seq` is discarded; a `Dropped` increments `droppedCount` and emits no warning; `inject()` produces an observable result identical to the same event arriving from the flow.

**Acceptance:** `./gradlew :app:testDebugUnitTest` green; `R4Repository.kt` contains no string formatting, no `warningType` comparison and no UI type.

**Dependencies:** after `4.5.3.1` and `4.5.1.6`. Parallel with `4.5.4.1`. **Commit:** `[4.5.4.2] feat: add R4Repository routing warnings, state and link status`

### [ ] `4.5.4.3` — `WarningClassifier` — presentation at the UI edge (D4) *(agent)*

**Objective:** map known `warningType` values to a presentation and everything else to a generic one, without ever rewriting the wire value.

**Scope — `app/src/main/java/…/warning/WarningClassifier.kt`:**

- Declare the presentation type in this same file — [HLD §4](../IVI_ECU/doc/ivi-ecu-hld.md#4-folder-structure) designates no separate file for it.
- `fun classify(warningType: String)`: `R4Contract`'s `nlos_obstruction` → the M1 NLOS presentation, marked known; **any other value → a generic presentation marked unknown, with the wire value carried through unchanged** (D4 — the parser preserved it, this is where classification happens, and nothing here writes back into the message).
- `fun normaliseRisk(riskState: String)`: `low` / `medium` / `high`, case-insensitively; **an unrecognised `riskState` maps to the highest urgency** — fail-safe, and it must agree with the renderer's risk colouring (`17.5.5.4`) rather than drifting from it.
- Test `app/src/test/java/…/warning/WarningClassifierTest.kt` — the M1 key is known; the additive fixture's value is unknown and still readable; `"HIGH"`, `"high"`, an empty string and an unrecognised word all resolve, the last two at highest urgency.

**Acceptance:** `./gradlew :app:testDebugUnitTest` green; no branch in this file mutates a decoded message.

**Dependencies:** after `4.5.1.6`. Parallel with `4.5.4.1` and `4.5.4.2`. **Commit:** `[4.5.4.3] feat: add WarningClassifier mapping unknown warning types to a generic presentation`

### [ ] `17.5.4.4` — `WarningViewModel` and `WarningUiState`, including the D12 snapshot wiring *(agent)*

**Objective:** turn warnings into `Idle ↔ Active` state with an auto-dismiss timeout, and **compose the scene so the renderer's provenance guard is armed**.

**Scope — two files:**

- `app/src/main/java/…/ui/WarningUiState.kt` — `sealed interface WarningUiState { data object Idle; data class Active(scene, riskState, presentation) }`.
- `app/src/main/java/…/ui/WarningViewModel.kt`:
  - Collects `repository.warnings`; each warning → `Active(...)` and re-arms a `warningTimeoutMs` timer; expiry → `Idle`. A new warning **resets** the timer rather than stacking timers.
  - **The composition step D12 exists for.** `SceneGeometry` as it arrives inside `geometry` carries no `vehicleCSnapshot`, and the guard treats a `null` snapshot as trusted — so passing `geometry` straight through **silently disables the R19 guard**. The view-model copies the message's R3 `object` snapshot into `vehicleCSnapshot` before the scene reaches the renderer. `riskState` and the presentation come from `WarningClassifier`.
  - Holds no drawing code and no socket ([HLD §3](../IVI_ECU/doc/ivi-ecu-hld.md#3-the-component-architecture) UI-logic rule).
- Test `app/src/test/java/…/ui/WarningViewModelTest.kt` on virtual time:
  - Idle initially; a warning → `Active`; silence for the timeout → `Idle`; a second warning inside the window extends rather than double-fires.
  - **The guard-armed test — name it for what it is.** Decode the frozen `r4-warning.json`, feed it in, and assert the composed scene's `vehicleCSnapshot` is **not null** and its `source` is `v2x_relayed`. Then feed a warning whose snapshot `source` is `own_sensor` and assert the composed scene carries that snapshot verbatim, so the guard can trip. A `null` snapshot in either case fails the test.
  - The timeout value comes from the injected config, never a literal.

**Acceptance:** `./gradlew :app:testDebugUnitTest` green including the named guard-armed test.

**Dependencies:** after `4.5.4.2` and `4.5.4.3`. **Commit:** `[17.5.4.4] feat: add WarningViewModel with timeout and the D12 snapshot composition`

### [ ] `16.5.4.5` — `DisplayMode` and `MainViewModel` — wake on warning, restore, user override *(agent)*

**Objective:** make a warning force the Display Area to the Warning View and restore the previous view when it clears, without trapping the user.

**Scope — two files under `app/src/main/java/…/ui/`:**

- `DisplayMode.kt` — the modes [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components) names: Warning, Home, Apps, Settings.
- `MainViewModel.kt`:
  - `currentMode`, and a `previousMode` captured when a warning forces the Warning View.
  - `fun onWarningState(state: WarningUiState)` — `Active` forces the Warning View; `Idle` restores `previousMode`, **unless** the user deliberately navigated away during the warning, in which case the user's choice stands.
  - A user-override flag that `setMode` honours: a deliberate navigation while the warning is active is recorded and obeyed, so `Idle` does not pull the user back. The safety intent holds — the warning still comes up unconditionally, and a later warning clears the override and forces the view again.
  - Emits the `[UI] mode=… cause=…` line of [HLD §12](../IVI_ECU/doc/ivi-ecu-hld.md#12-test-strategy) with `cause=warning`, `cause=timeout` or `cause=user` — the distinction is what proves a message and not a tap raised the view.
- Test `app/src/test/java/…/ui/MainViewModelTest.kt` — from Home, a warning forces Warning; `Idle` returns to Home; a user selection during the warning survives `Idle`; a second warning forces Warning again and clears the override; each transition emits its `cause`.

**Acceptance:** `./gradlew :app:testDebugUnitTest` green with all five cases; the `[UI]` lines go through the logger seam, not `android.util.Log`.

**Dependencies:** after `17.5.4.4`. **Commit:** `[16.5.4.5] feat: add DisplayMode and MainViewModel with wake-on-warning and user override`

---

## Task Group 5.5 — `:app` UI and shell — the launchable APK (serves R16, R17, R18)

> The group that gives the APK its renderer, its screen, its service and its process entry. [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components) fixes each component's one responsibility and [§4](../IVI_ECU/doc/ivi-ecu-hld.md#4-folder-structure) its path. **Nothing here mounts `WarningBannerOverlay`** (D11).

### [ ] `18.5.5.1` — `AndroidR4Logger` — the `IVI_V2X` evidence bridge *(agent)*

**Objective:** implement the `R4Logger` seam over `android.util.Log` on one tag, so the whole run's text evidence is a single `adb logcat -s IVI_V2X`.

**Scope — `app/src/main/java/…/service/AndroidR4Logger.kt`:** maps `R4LogLevel.{INFO,WARN,ERROR}` to `Log.i/w/e` on the tag `IVI_V2X`, emitting the caller's line verbatim. The `[LINK]`, `[RX]` and `[DROP]` shapes are composed in `:observer`; the `[UI]` line in `MainViewModel`. Add whatever narrow entry point those callers need and nothing more. **This is the only file in the node that bridges to `android.util.Log`** ([HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components)).

**Acceptance:** `./gradlew assembleDebug` and `:app:testDebugUnitTest` green; a grep of `serializer/` and `observer/` for `android.util` returns nothing.

**Dependencies:** after `4.5.3.1` and `4.5.1.6`. Parallel with group 5.4. **Commit:** `[18.5.5.1] feat: add AndroidR4Logger bridging the logger seam to the IVI_V2X tag`

### [ ] `4.5.5.2` — `R4ListenerService` — the foreground lifecycle host (D5) *(agent)*

**Objective:** host the observer in a foreground service so reception survives the Display Area showing something else, for the whole continuous run R19 requires.

**Scope:**

- `app/src/main/java/…/service/R4ListenerService.kt` — creates its notification channel and calls `startForeground()` **immediately** on start; on start calls `observer.start(applicationScope)`, on destroy `observer.stop()`. **The service is a lifecycle host, not the loop** (D5): no socket code and no decode code lives here.
- `POST_NOTIFICATIONS` is a runtime permission from API 33. **A denial suppresses the notification only and is never a failure to start** (D5) — guard the notification post, not the service start.
- `app/src/main/AndroidManifest.xml` — the `<service>` declaration, `android:exported="false"`, with its foreground-service type; and the permissions [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components) lists for this node. Record in a manifest comment which permission a `targetSdk` bump would additionally require; **do not bump the target**.
- No unit test for the service itself — that is what D5's plain-JVM split buys; the loop is covered by `4.5.3.3`–`4.5.3.5`.

**Acceptance:** `./gradlew assembleDebug` green and `:app:testDebugUnitTest` still green; the manifest declares the service; the class body contains no `DatagramSocket` and no decode call.

**Dependencies:** after `18.5.5.1`. **Commit:** `[4.5.5.2] feat: add R4ListenerService as the foreground host for the R4 observer`

### [ ] `17.5.5.3` — `SceneCoordinateMapper` — the oblique projection, pure math *(agent)*

**Objective:** turn scene metres into canvas coordinates for R17's **inclined** camera, in a file with no Android types so it is unit-tested without a device.

**Scope:**

- `app/src/main/java/…/ui/view/SceneCoordinateMapper.kt` — `SceneGeometry` plus a base scale → screen-space geometry. R17's camera is inclined rather than overhead, so **depth compresses toward the top** and each vehicle shows a shallow rear face; the mapper derives a distance-varying scale from the single `SCENE_SCALE_M_PER_PX` base value (D10). Ego anchors near the bottom of the canvas; forward (`x` positive) maps upward, right (`y` positive) maps right. A vehicle beyond the canvas is clamped to a margin rather than drawn off-screen. A `null` `vehicleC` yields no C geometry and does not throw.
- Test `app/src/test/java/…/ui/view/SceneCoordinateMapperTest.kt` — the ego anchor is exact; forward maps up and right maps right; an out-of-range vehicle is clamped on both axes; halving the base scale doubles the pixel displacement for the same metres; a `null` `vehicleC` yields a `null` result without throwing; radii and other pass-through values are unmodified.

**Acceptance:** `./gradlew :app:testDebugUnitTest` green with all six cases; the file and its test import nothing from `android.*` or `androidx.*`.

**Dependencies:** after `4.5.1.6`. **Fully parallel** with the rest of groups 5.2–5.6. **Commit:** `[17.5.5.3] feat: add SceneCoordinateMapper with the R17 oblique projection`

### [ ] `17.5.5.4` — `IviWarningViewSeam` and `CanvasWarningView` with the provenance guard *(agent)*

**Objective:** draw the R17 God View behind the seam that makes an optional 3D renderer swappable, and make the R19 claim mechanical.

**Scope — two files under `app/src/main/java/…/ui/view/`, plus one test:**

- `IviWarningViewSeam.kt` — the render seam `Render(scene, riskState)` ([HLD §8](../IVI_ECU/doc/ivi-ecu-hld.md#8-interfaces-ports-and-the-layer-rule)). An interface only; `MainScreen` sees nothing else.
- `CanvasWarningView.kt` — realizes the seam over Compose Canvas, drawing exactly what [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components) fixes: a dark canvas, a lane-marked road converging toward the top, three car silhouettes in one lane with ego nearest; ego and B solid; **ghost C dashed and translucent on a pulsing ground glow coloured by `riskState`**; a `null` `vehicleC` drawn without C, with no placeholder and no crash. Geometry comes from `SceneCoordinateMapper`; the scale comes from the runtime config, not from a literal (D10).
  - **The scene alone is the warning.** No legend, no distance labels, no text overlay, no banner, and no `[V2X]` badge — those belong to R17's annotated explanatory figure, which the IVI never renders. See § Open items item 1 for the one document that says otherwise.
  - **The provenance guard.** Ghost C is drawn **only** when its snapshot `source` is `v2x_relayed`. Any other value draws the yellow `[? UNKNOWN SOURCE]` marker instead and logs at ERROR through `AndroidR4Logger`. A `null` snapshot is treated as trusted — the guard fails open, which is exactly why `17.5.4.4` must fill it (D12).
  - Put the guard decision and its ERROR message in **`internal` top-level functions** in this file, called by the composable. A `Canvas`-drawn marker is not in the Compose semantics tree, so a composition test cannot reach a decision that stays inline; extracting it is what makes the R19 guard assertable at all.
  - Risk colouring maps an unrecognised `riskState` to the highest-urgency colour — the same fail-safe as `WarningClassifier.normaliseRisk`, so the two cannot drift.
- Test `app/src/test/java/…/ui/view/CanvasWarningViewTest.kt` — `null` snapshot → trusted; `v2x_relayed` → trusted; `own_sensor` → **not** trusted; the error message names both the offending source and `v2x_relayed`; an unrecognised `riskState` maps to the high-urgency colour.

**Acceptance:** `./gradlew assembleDebug` and `:app:testDebugUnitTest` green with all five guard cases; a grep of the file finds no legend, distance-label or badge drawing.

**Dependencies:** after `17.5.5.3` and `4.5.4.3`. **Commit:** `[17.5.5.4] feat: add the Canvas God View behind the render seam with the provenance guard`

### [ ] `17.5.5.5` — `WarningBannerOverlay` — built, mounted nowhere (D11) *(agent)*

**Objective:** deliver the banner component the design names, and leave it unmounted so the canvas renders unobstructed.

**Scope:** `app/src/main/java/…/ui/view/WarningBannerOverlay.kt` — a composable taking `riskState` and rendering the risk banner. **It is referenced by no screen.** D11 is a standing user decision: the God-View canvas must render unobstructed, which is also R17's own requirement. A subtask that mounts it has broken a user decision, not made an improvement.

**Acceptance:** `./gradlew assembleDebug` green; a repo-wide grep finds no reference to `WarningBannerOverlay` outside its own file. A `@Preview` is the only sanctioned caller.

**Dependencies:** after `4.5.1.6`. Parallel with everything in this group. **Commit:** `[17.5.5.5] feat: add the unmounted WarningBannerOverlay component`

### [ ] `16.5.5.6` — `MainScreen` — the R16 layout, the view slot and the status bar *(agent)*

**Objective:** build the R16 layout as [ivi-ecu.svg](../requirements/ivi-ecu.svg) fixes it, hosting the Warning View through the seam.

**Scope — `app/src/main/java/…/ui/screen/MainScreen.kt`:**

- The layout of [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components): a central **Display Area**, the Home / Apps / Settings areas around it, mode labels, and a bottom status bar. Tapping a side area changes `MainViewModel.currentMode` and the Display Area follows — R16's acceptance clause.
- The Display Area's Warning branch renders through `IviWarningViewSeam` when the warning state is `Active`, and shows neutral idle content when it is `Idle`. **`MainScreen` sees only `WarningUiState` and the seam** — never `CanvasWarningView` concretely ([HLD §3](../IVI_ECU/doc/ivi-ecu-hld.md#3-the-component-architecture) UI-layer rule).
- It collects the warning state and feeds it to `MainViewModel.onWarningState`, so a message raises the view.
- The status bar renders the live `R4LinkState`: bound with the port, rebinding, or error — the visual half of the `[LINK]` evidence. No hardcoded standby string.
- `@Preview` functions for the idle and active states, drawn at the guest's display size once `4.5.8.2` has measured it.

**Acceptance:** `./gradlew assembleDebug` and `:app:testDebugUnitTest` green; a grep of the file finds no `WarningBannerOverlay` and no literal status string.

**Dependencies:** after `16.5.4.5` and `17.5.5.4`. **Commit:** `[16.5.5.6] feat: add MainScreen with the R16 layout, view seam slot and link status bar`

### [ ] `4.5.5.7` — `IviGraph` — the manual composition root (D7) *(agent)*

**Objective:** wire the object graph by hand, in one place, with no annotation processor.

**Scope — `app/src/main/java/…/di/IviGraph.kt`:**

- Constructs, in dependency order: `IviRuntimeConfig` → `R4ObserverConfig` → `R4Deserializer` → `R4SocketObserver` (with `sourceFactory = { JdkDatagramSource(port, bufferBytes) }` and `logger = AndroidR4Logger`) → `R4Repository`, collecting on the **application** scope so a service restart cannot lose the last warning → one `ViewModelProvider.Factory` producing `MainViewModel` and `WarningViewModel` → the `CanvasWarningView` instance with its scale from config.
- One object, created once, owned by `IviApplication`. It exposes the factory and the renderer and nothing else.
- `fun updateConfig(resolved: IviRuntimeConfig)` so the launch-time override reaches the graph before the service starts.
- No annotation, no reflection, no dependency-injection framework (D7).

**Acceptance:** `./gradlew assembleDebug` green; the file declares no annotations and no singleton state beyond what `IviApplication` holds.

**Dependencies:** after `4.5.5.2`, `4.5.4.1`, `4.5.4.2`, `17.5.4.4` and `17.5.5.4`. **Commit:** `[4.5.5.7] feat: add IviGraph, the manual composition root and view-model factory`

### [ ] `16.5.5.8` — `IviApplication`, `MainActivity` and the manifest — the APK becomes startable *(agent)*

**Objective:** give the APK its application class, its launcher Activity and the manifest entries that make it install and start on the guest.

**Scope:**

- `app/src/main/java/…/IviApplication.kt` — `Application` creating `IviGraph` in `onCreate` and holding the application `CoroutineScope`; exposes the graph to the Activity.
- `app/src/main/java/…/MainActivity.kt` — `ComponentActivity`; in `onCreate` resolve `IviRuntimeConfig.resolve(intent)`, hand it to the graph, start `R4ListenerService` as a foreground service, request `POST_NOTIFICATIONS` on API 33+ **without blocking startup on the answer**, then `setContent { MainScreen(...) }` with view-models from the graph's factory.
- `app/src/main/AndroidManifest.xml` — `android:name` on `<application>`; the `<activity>` with `MAIN`/`LAUNCHER`; the `automotive` feature declaration [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components) lists. Use a **platform theme**; [HLD §4](../IVI_ECU/doc/ivi-ecu-hld.md#4-folder-structure) designates no new resource file, and if a platform theme proves unusable on the guest that is a finding to report, not a licence to add an undesignated file.

**Acceptance:** `./gradlew assembleDebug` green; `:app:testDebugUnitTest` still green; the built APK declares **exactly one** LAUNCHER activity, checked by the route [walkthrough §2.6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#26-check-the-apk-is-launchable) fixes.

**Dependencies:** after `4.5.5.7` and `16.5.5.6`. **Commit:** `[16.5.5.8] feat: add IviApplication and MainActivity as the launcher entry`

---

## Task Group 5.6 — Test equipment: `:r4-simulator` and the dev injector (serves R4, R5)

> [HLD §7 Test equipment](../IVI_ECU/doc/ivi-ecu-hld.md#7-external-related-components) and **D9**. Sanctioned IVI test equipment inside `IVI_ECU/`, not a mock to eliminate. It cannot reach into `ADA_ECU/` — no cross-node source imports — so it reaches the same models the app parses with by depending on `:contract`. Its placement inside the consuming node's folder is the sanctioned exception in [node-code-layout.md § `tools/`](../.claude/rules/node-code-layout.md#tools--test-equipment-and-ecu-mocks).

### [ ] `4.5.6.1` — Module `:r4-simulator` and `SimConfig` *(agent)*

**Objective:** stand up the CLI module and its two configuration sources.

**Scope:**

- `settings.gradle.kts`: `include(":r4-simulator")`. `IVI_ECU/r4-simulator/build.gradle.kts` — `alias(libs.plugins.kotlin.jvm)`, `alias(libs.plugins.kotlin.serialization)`, the built-in `application` plugin with its main class; `implementation(project(":contract"))`; `testImplementation(libs.junit)`. **Nothing beyond `:contract` and JUnit** (D9, and criterion C4): no YAML library, no CLI framework, no logging framework.
- `r4-simulator/src/main/kotlin/…/sim/SimConfig.kt` — a data class with two factories. `fromEnv()` reads **exactly** the variable names the walkthrough's ADA-node contract fixes — `IVI_ECU_HOST`, `IVI_ECU_PORT`, `R4_SCENARIO`, `R4_RATE_HZ`, `START_DELAY_S` ([walkthrough §4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) rung V4) — because matching the real ADA node's names is what makes Phase 6 an image swap with no node-config edit. `fromArgs(args)` serves host mode. Env wins over the scenario file's own default rate; a missing required value fails loudly with the variable name in the message; defaults are named constants in this file, never inline literals.
- Test `r4-simulator/src/test/kotlin/…/SimConfigTest.kt` — a full env map parses; a missing host fails with a message naming it; the env rate overrides the file default; args mode parses each flag.

**Acceptance:** `./gradlew :r4-simulator:test` green; the module's dependency block names only `:contract` and JUnit.

**Dependencies:** after `4.5.1.4`. **Fully parallel** with groups 5.2–5.5. **Commit:** `[4.5.6.1] feat: add the :r4-simulator module and its env/args configuration`

### [ ] `4.5.6.2` — Scenario model and loader — scenarios are data, not code *(agent)*

**Objective:** make a new test case a new file, never a new code branch (D9, and the rule R11 imposes on the bench).

**Scope:**

- `r4-simulator/src/main/kotlin/…/Scenario.kt` — `@Serializable` scenario and step models. Freeze the shape here, because three data files and three tests depend on it:

  ```json
  {
    "name": "approach",
    "defaultRateHz": 1.0,
    "loop": false,
    "steps": [
      { "sample": "r4-warning", "overrides": { "riskState": "low", "geometry.vehicleC": null } },
      { "kind": "raw", "text": "not-json" }
    ]
  }
  ```

  `sample` names one of the frozen `:contract` fixtures; `overrides` maps dotted JSON paths to values, with an explicit `null` allowed and meaningful; `kind: "raw"` sends `text` as literal bytes and skips validation.
- `r4-simulator/src/main/kotlin/…/ScenarioLoader.kt` — file path → `Scenario`, with rejection messages naming the file and the offending field. An unknown `sample` name, an unknown `kind` and an empty step list are **rejections, not silent defaults**.
- Test `…/ScenarioLoaderTest.kt` — a valid scenario loads; malformed JSON, an unknown sample name, an unknown kind and an empty step list are each rejected with a message naming the cause.

**Acceptance:** `./gradlew :r4-simulator:test` green; no scenario behaviour is expressed as a Kotlin branch on a scenario name.

**Dependencies:** after `4.5.6.1`. **Commit:** `[4.5.6.2] feat: add the scenario model and loader for the R4 simulator`

### [ ] `4.5.6.3` — `SampleLibrary` and `MessageBuilder` — overrides validated through `R4Json` *(agent)*

**Objective:** build every payload from the frozen sample and prove the app can parse it **before** it goes on the wire (D9).

**Scope:**

- `r4-simulator/src/main/kotlin/…/SampleLibrary.kt` — loads the frozen samples off the **`:contract` classpath** (D6). A sample carried as a literal in this file is a defect: a simulator with its own copy of the schema is a second, unversioned contract that keeps passing after the real one changes.
- `r4-simulator/src/main/kotlin/…/MessageBuilder.kt`, following D9's four steps:
  1. Parse the named sample to a JSON object.
  2. Apply the step's dotted-path overrides **at element level** — `riskState`, `warningType`, `schemaVersion`, `object.source`, `object.distance`, `geometry.vehicleC` including explicit `null`, plus arbitrary additive junk fields. Element-level editing is what lets an unknown extra field survive onto the wire; a typed round trip would drop it.
  3. **Decode the result through `:contract`'s `R4Json` before returning it.** A payload the simulator cannot parse is one the app cannot parse, and the run must fail loudly at the producer. The one exception is a `raw` step, which returns its literal bytes unvalidated on purpose.
  4. Return UTF-8 bytes.
- Test `…/MessageBuilderTest.kt` — every non-raw step's payload decodes through `R4Json`; a `geometry.vehicleC: null` override produces JSON `null` and not an absent key; an added junk field is present in the emitted bytes **and** the payload still decodes; a `raw` step's bytes are returned untouched and unvalidated; an override that makes the payload invalid fails the build with a message naming the step.

**Acceptance:** `./gradlew :r4-simulator:test` green; a grep of `r4-simulator/src/main` finds no embedded R4 JSON literal.

**Dependencies:** after `4.5.6.2`. **Commit:** `[4.5.6.3] feat: build simulator payloads from the frozen samples with validated overrides`

### [ ] `4.5.6.4` — The three scenario data files and the stream-difference test *(agent)*

**Objective:** commit, as data, the scenarios the acceptance evidence needs, and prove different files produce observably different streams.

**Scope — three files under `IVI_ECU/r4-simulator/scenarios/` ([HLD §4](../IVI_ECU/doc/ivi-ecu-hld.md#4-folder-structure)), covering the case table of [the simulator note §2](../IVI_ECU/doc/research_notes/phase5-r4-simulator.md):**

- `approach.json` — the full lifecycle in one file, because the timeout-and-restore path is otherwise reachable only by waiting for a stream to stop: a **first step carrying `geometry.vehicleC: null`** (C not yet tracked — the renderer's null-C path), then C approaching with `riskState` climbing low → medium → high and `vehicleC` closing, then C leaving with the distance opening and risk falling, then **silence longer than `WARNING_TIMEOUT_MS`** so the view times out and the previous mode is restored while the scenario is still running. This is the file the evidence run uses.
- `degrade.json` — the three degradation cases of [walkthrough §4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) rung **V5**, one step each: an unknown `warningType` with a newer `schemaVersion` and a junk field; `object.source: "own_sensor"` to trip the provenance guard; and a `raw` non-JSON step the receive loop must survive.
- `state-stream.json` — the optional R15 path: periodic `state` messages with ascending `seq`.
- Test `…/ScenariosDifferTest.kt` — all three files load through `ScenarioLoader`; the streams `approach.json` and `degrade.json` produce differ **observably**, asserted on decoded field values (the risk progression, the `warningType`, the snapshot `source`), not on byte inequality alone.

**Acceptance:** `./gradlew :r4-simulator:test` green; the three files exist at the designated paths; no new Kotlin branch keys off a scenario name.

**Dependencies:** after `4.5.6.3`. **Commit:** `[4.5.6.4] feat: add the approach, degrade and state-stream scenario files`

### [ ] `4.5.6.5` — `UdpSender` and `Main` — the two run modes and the `[TX]` line *(agent)*

**Objective:** make the simulator runnable from a laptop in host mode and from a container entrypoint in-Room, so a deploy alone produces traffic.

**Scope:**

- `r4-simulator/src/main/kotlin/…/UdpSender.kt` — a `DatagramSocket` sending to `host:port`, one send per step; send errors are logged and counted, never fatal to the loop.
- `r4-simulator/src/main/kotlin/…/Main.kt` — resolves `SimConfig` (args present → host mode, otherwise env → in-Room mode), waits `START_DELAY_S` because the AAOS guest boots slower than a container, loads the scenario, then walks the steps at the resolved rate, logging one `[TX]` line per send carrying the step index, message type, byte count and destination. `loop: true` repeats the step list. It **exits non-zero** if the scenario fails to load or a non-raw payload fails validation — the run must fail loudly at the producer (D9).
- No new test: `Main` is glue, and the behaviour is covered by `4.5.6.2`–`4.5.6.4`.

**Acceptance:** `./gradlew :r4-simulator:installDist` produces a runnable distribution; run in host mode against the loopback listener of `4.5.3.5`, it emits `[TX]` lines and the datagrams arrive; `./gradlew :r4-simulator:test` still green.

**Dependencies:** after `4.5.6.4`. **Commit:** `[4.5.6.5] feat: add the R4 simulator UDP sender and its host/in-Room entrypoint`

### [ ] `5.5.6.6` — Simulator `Dockerfile` and `entrypoint.sh` *(agent)*

**Objective:** package the simulator as the single-platform arm64 image the mini-blueprint's ADA node pulls.

**Scope:**

- `IVI_ECU/r4-simulator/Dockerfile` — multi-stage: the build stage on `$BUILDPLATFORM`, because JVM bytecode is architecture-neutral and an emulated Gradle build is pure cost; the runtime stage `linux/arm64` on a JRE 17 base. **Build context is `IVI_ECU/`** — it needs the wrapper, `settings.gradle.kts`, the catalog, `:contract` and `:r4-simulator`. [HLD §11](../IVI_ECU/doc/ivi-ecu-hld.md#11-tech-stack-build-and-ci) records this as a sanctioned deviation from "own `Dockerfile` at the folder root", because this folder's primary artifact is the APK; self-containment holds, since the build reads nothing outside `IVI_ECU/`. Copy `scenarios/` into the image; workdir `/app`.
- `IVI_ECU/r4-simulator/entrypoint.sh` — sleeps `START_DELAY_S`, then runs the distribution. The blueprint node config's `command` is `["./entrypoint.sh"]` — **relative, not absolute**: it resolves against the image workdir, and the absolute form is a mistake already made once on this platform ([deploy-walkthrough-netcheck.md §7](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#7-mistakes-already-made--check-these-first)). LF line endings, executable bit set.
- `docker build` is unavailable on the Windows dev host, so image verification transfers to the CI lane `5.5.7.3`.

**Acceptance:** `sh -n r4-simulator/entrypoint.sh` passes; the Dockerfile declares `--platform=$BUILDPLATFORM` on the build stage only and copies `scenarios/` into the image; the image build itself is verified by `5.5.7.3`.

**Dependencies:** after `4.5.6.5`. **Commit:** `[5.5.6.6] feat: add the R4 simulator Dockerfile and entrypoint`

### [ ] `4.5.6.7` — `DevInjectorReceiver` — the debug-only injection point *(agent)*

**Objective:** let an `adb` broadcast push one frozen sample onto the same flow the socket feeds, so UI work is unblocked while the ADB and network route is still unproven.

**Scope:**

- `app/src/debug/java/…/debug/DevInjectorReceiver.kt` — a `BroadcastReceiver` for `com.hackathon.v2x.ivi.DEV_INJECT` reading a sample name, loading that frozen sample off the `:contract` classpath, decoding it through `R4Deserializer`, and calling `R4Repository.inject(...)`. Because it joins **downstream of the socket and upstream of everything else**, it exercises decode → repository → view-model → Compose exactly as a real datagram does.
- Register it in `app/src/debug/AndroidManifest.xml` — the **debug source set only**. **It must be absent from the release build**: a release path able to fabricate a warning would undermine the R19 claim that C came only from relayed data ([HLD §7](../IVI_ECU/doc/ivi-ecu-hld.md#7-external-related-components)).
- The broadcast command and its expected result are [walkthrough §4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) rung **V3**; running it in the Room is `17.5.8.12`, not this subtask.

**Acceptance:** `./gradlew assembleDebug` and `assembleRelease` both green; the **release** merged manifest contains no `DEV_INJECT` receiver; `:app:testDebugUnitTest` still green.

**Dependencies:** after `16.5.5.8` and `4.5.4.2`. **Commit:** `[4.5.6.7] feat: add the debug-only dev injector for UI testing without a network`

---

## Task Group 5.7 — CI lanes (serves R4, R5, R16)

> [HLD §11](../IVI_ECU/doc/ivi-ecu-hld.md#11-tech-stack-build-and-ci) fixes three lanes and where each is maintained: `ivi-unit-tests` originated in Phase 0 and stays in [phase0-ci.yml](../.github/workflows/phase0-ci.yml) even when a Phase 5 subtask edits it; the two lanes originating here live in `phase5-ci.yml`.

### [ ] `16.5.7.1` — `phase5-ci.yml` with the `ivi-assemble` lane *(agent)*

**Objective:** build the APK on every push, gate it on the IVI unit tests, and publish it as a run artifact so the install steps have a build to fetch.

**Scope — `.github/workflows/phase5-ci.yml`, with the same triggers and concurrency block as `phase0-ci.yml` so every lane still runs on every push:**

- Job `ivi-assemble` — checkout, `setup-java` (temurin 17, Gradle cache), `working-directory: IVI_ECU`, then `./gradlew :app:testDebugUnitTest`, `./gradlew assembleDebug` and **`./gradlew lint`** — the three commands [HLD §11](../IVI_ECU/doc/ivi-ecu-hld.md#11-tech-stack-build-and-ci) names for this lane — then upload `app/build/outputs/apk/debug/app-debug.apk` under the artifact name [walkthrough §3.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#33-get-the-apk-off-ci) references, with `if-no-files-found: error`. A `timeout-minutes` bounds a hung dependency resolve without capping a slow cold build.
- **The unit tests run in this job deliberately, overlapping `ivi-unit-tests`.** This job's output is hand-installed onto a guest, so an APK must never leave the workflow unless its own tests passed in the job that produced it. It is a gate on the artifact, not a second test lane: when test targets change, extend `ivi-unit-tests`, never this step.
- **Lint is part of the lane, not an optional extra.** If the first run surfaces `Error`-severity findings, the resolution is to record them and set `abortOnError = false` in `app/build.gradle.kts` with a comment naming them — **not** to drop the step, because `NewApi` is what would flag a call above `minSdk` on a guest whose API level is still unread ([walkthrough §6.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) item 5).
- Two reporting steps between assemble and upload: the APK size as a notice, failing the lane only if the APK is missing after a successful assemble; and whether the APK declares a launcher activity, which never fails the lane and exists because a missing launcher entry is the most expensive surprise in the bring-up route.
- Add an Android SDK setup action only if the runner image's SDK or licence state proves insufficient — try without it first and record which was needed ([walkthrough §6.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) item 10).

**The shipped lane is documented by [walkthrough §3.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#31-the-workflow-its-job-and-its-triggers)** — job name, step order, triggers, concurrency, timeout and the notices it emits. That section and this subtask stay in step; neither restates the other's detail.

**Acceptance:** the lane runs green on the branch, confirmed by either route of [§3.2](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#32-check-that-the-run-finished-and-passed), and the artifact is retrievable by either route of [§3.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#33-get-the-apk-off-ci); record the run ID and the APK size in the status line.

**Dependencies:** none beyond a pushable branch — **land this early, in parallel with group 5.1**, so an artifact exists before group 5.8 needs one. **Commit:** `[16.5.7.1] ci: add phase5-ci with the ivi-assemble lane`

### [ ] `4.5.7.2` — Extend `ivi-unit-tests` to all five modules *(agent)*

**Objective:** run every module's tests in CI, not just `:app`'s. A module whose tests only ever run locally is untested as far as the phase's acceptance is concerned.

**Scope:** in [.github/workflows/phase0-ci.yml](../.github/workflows/phase0-ci.yml), the `ivi-unit-tests` job's run step becomes the full-suite command of § Build & verification commands. Nothing else in that file changes. Add a one-line comment recording that this Phase 0-origin lane was extended by Phase 5 subtask `4.5.7.2`, matching the file's existing convention.

**Acceptance:** the lane runs green with all five module test tasks visible in the run log; record the run ID.

**Dependencies:** **sequential after `4.5.6.4`** — every one of the five Gradle projects must exist and carry tests, or the lane fails on a missing project. **Commit:** `[4.5.7.2] ci: run all five IVI Gradle modules' tests in ivi-unit-tests`

### [ ] `5.5.7.3` — `r4-sim-image` lane — arm64 build, push and verify *(agent)*

**Objective:** publish `m1-r4-sim:latest` to the CarSky Zot registry as a single-platform arm64 image the ADA node can pull.

**Scope — a second job in `.github/workflows/phase5-ci.yml`, modelled on `phase0-ci.yml`'s netcheck image job; copy its structure rather than re-inventing it:**

- Registry host `registry.hackathon-2.carsky.io` — the `hackathon-2` host, because the bare registry host 502s ([deploy-walkthrough-netcheck.md §7](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#7-mistakes-already-made--check-these-first)); image `m1-r4-sim:latest`; platform `linux/arm64` only.
- QEMU and Buildx setup, then the build command of § Build & verification commands with `--push` and a metadata file. **`--provenance=false --sbom=false` and a single platform are mandatory, not stylistic** — the cluster rejects manifest indexes ([HLD §11](../IVI_ECU/doc/ivi-ecu-hld.md#11-tech-stack-build-and-ci)).
- Log in with `secrets.CARSKY_ZOT_API_KEY`; **skip the push with a notice when the secret is absent**, exactly as the netcheck lane does — a green lane is not by itself evidence that the tag reached the registry ([node-code-layout.md § Build rules](../.claude/rules/node-code-layout.md#build-rules-all-container-nodes)).
- Verify with the existing [verify-arm64-image](../.github/actions/verify-arm64-image) action, passing the digest from the metadata file.

**Acceptance:** the lane pushes and the verification step confirms the tag is pullable and single-platform `linux/arm64`; record the run ID and the pushed digest.

**Dependencies:** after `5.5.6.6`. **Commit:** `[5.5.7.3] ci: add the r4-sim-image build/push/verify lane`

---

# Part II — Test plan

[HLD §12](../IVI_ECU/doc/ivi-ecu-hld.md#12-test-strategy) fixes the strategy: a plain-JVM unit layer below, and above it **two Room configurations differing in exactly one component** — which realizes the `ADA-ECU` interface. Group 5.8 runs the isolated configuration with the simulator; group 5.9 runs the full one with the real producer. **Expected output is identical in both**, so a difference between the runs is a finding about the other node, never about this one.

## Test levels

Four levels, weakest coupling first. They are not alternatives — each proves something the others cannot, and each costs an order of magnitude more to run than the one above it ([the simulator note §1](../IVI_ECU/doc/research_notes/phase5-r4-simulator.md)).

| Level | What runs | What it proves | Where | Delivered by |
|---|---|---|---|---|
| **L1 — decode API** | `:contract` + `:serializer`, no socket, no UI | Contract conformance: every frozen sample, every malformed case | `:contract:test`, `:serializer:test` | `4.5.1.5`, `4.5.2.2`, `4.5.2.3` |
| **L2 — real socket, no device** | `:observer` over a loopback `DatagramSocket` | The receive loop, buffer discipline, back-pressure, rebind | `:observer:test` | `4.5.3.2`–`4.5.3.5` |
| **L3 — app logic and the dev injector** | `:app` logic on a JVM; the injector on a device with no network | View-model state, mode switching, the armed guard, the whole UI path | `:app:testDebugUnitTest`; `4.5.6.7` on a guest | `4.5.4.*`, `17.5.5.3`, `17.5.5.4`, `17.5.8.12` |
| **L4 — real UDP from a peer** | The whole node in a Room | R6's ADA→IVI hop, R16 and R17 acceptance, the recorded evidence | Groups 5.8 and 5.9 | — |

**L1 and L2 are the levels that must be automated**, and `4.5.7.2` is what puts them in CI. L3's injector exists because the ADB route to the guest is unproven, so it keeps UI work unblocked. L4 is what produces the evidence.

## What the unit layer must cover

Every row is a test this plan commits to, traced to what makes it necessary. A row with no test is an untested claim.

| Claim under test | Test | Subtask | Source |
|---|---|---|---|
| Every frozen sample round-trips through the binding | `R4RoundTripTest` | `4.5.1.5` | R4 acceptance; HLD §12 |
| A newer `schemaVersion` + unknown `warningType` + unknown field decodes and preserves the wire value | `R4AdditiveVersionTest` | `4.5.1.5` | R4 acceptance; D4 |
| Every decode-failure row maps to its typed result, and nothing throws | `R4DeserializerTest` | `4.5.2.2` | parsing note §2 |
| A dirty backing array, a reused buffer, a BOM and out-of-bounds input all behave | `BufferSlicingTest` | `4.5.2.3` | D3 rows 1 and 4 |
| A reused packet's length is reset before every receive | `JdkDatagramSourceTest` | `4.5.3.2` | D3 row 2 |
| N datagrams in → N events out; one bad message does not stop the next | `R4SocketObserverTest` | `4.5.3.3` | HLD §6 |
| Rebind back-off is bounded, resets on success, and reaches `Error` without giving up | `RetryBackoffTest` | `4.5.3.4` | D5 |
| The whole receive path works over a real socket with no device | `LoopbackSocketTest` | `4.5.3.5` | HLD §12 |
| Defaults, launch overrides and out-of-range fallbacks resolve in one place | `IviRuntimeConfigTest` | `4.5.4.1` | D10 |
| Warnings, last-value-wins `state`, drops and injection route identically | `R4RepositoryTest` | `4.5.4.2` | HLD §6 |
| An unknown `warningType` degrades to generic; unknown risk fails safe to highest | `WarningClassifierTest` | `4.5.4.3` | D4 |
| **The composed scene carries the R3 snapshot, so the guard is armed** | `WarningViewModelTest` guard-armed case | `17.5.4.4` | **D12** |
| A warning forces the view; the previous mode is restored unless the user overrode it | `MainViewModelTest` | `16.5.4.5` | R16 acceptance |
| The oblique projection anchors, clamps, scales and survives a `null` C | `SceneCoordinateMapperTest` | `17.5.5.3` | R17 |
| **Ghost C is drawn only for `v2x_relayed`; anything else marks and logs ERROR** | `CanvasWarningViewTest` | `17.5.5.4` | **R19, D12** |
| A scenario file is rejected loudly rather than defaulted silently | `ScenarioLoaderTest` | `4.5.6.2` | D9 |
| Every non-raw payload validates through `R4Json` before sending; junk fields survive | `MessageBuilderTest` | `4.5.6.3` | D9 |
| Different scenario files produce observably different streams | `ScenariosDifferTest` | `4.5.6.4` | D9 |

**Two of these are R4's own acceptance** — the round-trip and the additive-version tests — and **two are the R19 claim in code**: the guard-armed composition and the guard itself. Losing either R19 test leaves a guard that is present, passing and disabled.

## The in-Room observables

[HLD §12](../IVI_ECU/doc/ivi-ecu-hld.md#12-test-strategy) names each observable and the component that produces it; [walkthrough §6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance) names the four proofs they must add up to. This table is the checklist both Room groups work against.

| Observable | Produced by | Rung | Proof |
|---|---|---|---|
| `[LINK] state=bound port=47300`, and the status bar's link indicator | `R4SocketObserver` → `AndroidR4Logger`; `MainScreen` | V1 | precondition |
| `[DROP] reason=malformed …` on a non-JSON probe datagram, app still running | `R4Deserializer` returning a result instead of throwing | V2 | the network hop |
| The Display Area switches to the Warning View from an injected sample, no network | `DevInjectorReceiver` → `R4Repository` → the view path | V3 | the UI path |
| `[RX] type=warning bytes=… from=…` per datagram | `JdkDatagramSource` → `R4SocketObserver` → `AndroidR4Logger` | V4 | §6 proof 1 |
| `warningType=`, `risk=`, `cSource=`, `cPos=` on that line, read off the parsed message | `R4Deserializer` | V4 | §6 proof 2 |
| `[UI] mode=WarningView cause=warning` — and not `cause=user` | `R4Repository` → `WarningViewModel` → `MainViewModel` | V4 | §6 proof 3 |
| The God View in the Display Area: ego and B solid, ghost C dashed on a risk-coloured glow | `MainScreen` → `IviWarningViewSeam` → `CanvasWarningView` | V4 | §6 proof 4 |
| A `null` `vehicleC` first step renders without C, without crash or placeholder | `CanvasWarningView` | V4 | R17 |
| `cause=user` on a tap; `cause=timeout` back to Idle with the previous mode restored | `MainViewModel`; `WarningViewModel`'s timeout | V4 | R16 acceptance |
| A generic warning on an unknown `warningType`, with the wire value preserved in the log | `WarningClassifier`; `R4Deserializer` | V5 | box 4 |
| `[? UNKNOWN SOURCE]` and an ERROR line on an `own_sensor` message — **the trip is the pass** | the provenance guard | V5 | R19 |
| `[DROP] reason=malformed …` and the next valid warning still rendering | `R4Deserializer`; `R4SocketObserver` | V5 | loop survival |

## Test data

One scenario file per purpose, all data (`4.5.6.4`): `approach.json` drives V4 and carries the null-C, risk-progression and timeout cases; `degrade.json` drives V5's three rows; `state-stream.json` exercises the optional R15 path, which no acceptance criterion depends on (D11). Payloads come from the frozen samples, never from a literal (D9).

---

## Task Group 5.8 — Isolated IVI test, the mini-blueprint (serves R4, R5, R6, R16, R17, R18)

> The first configuration of [HLD §12](../IVI_ECU/doc/ivi-ecu-hld.md#12-test-strategy): Ethernet Bridge + an ADA node running `m1-r4-sim:latest` + the IVI Skycraft node. Composition and creation route: [walkthrough §4.11](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#411-the-mini-blueprint-route), which fixes clone-then-delete, states that the mechanics are §4.2–§4.10 with only the composition differing, and makes the ADA node the only node ever reconfigured.
>
> **This group closes four of the five acceptance boxes.** The ladder is [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging)'s rungs **V1–V5**; the proofs are [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance), from which every subtask below takes its acceptance. All evidence lands in `plans/doc/phase5-ivi-run.md`, on the [phase0-smoke-test-run.md](doc/phase0-smoke-test-run.md) pattern.
>
> **Two different things are called "install", on opposite sides of the AI/human split.** Setting a node's **image field** is a Nydus Inspector edit — the REST API has no update route for an existing node's config — so it is *Human*. Installing the **APK** with `adb install -r` and launching it are commands against the guest, which [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human) assigns to AI, so they are *car-sky*.
>
> **`4.5.8.6`–`4.5.8.7` are the phase's earliest risk and must not wait behind the code groups.** The ADB tunnel route is the organizers' own, this team has not run it, and the guest's API level and `automotive` feature are unread ([§6.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) items 1 and 5). A negative answer on either moves every criterion below to AAOS-emulator evidence — cheap to learn early, expensive to learn late — and neither subtask needs a line of Phase 5 code.

### [ ] `5.5.8.1` — Compose the mini-blueprint by cloning the baseline *(Human)*

**Objective:** produce a 3-node blueprint — Ethernet Bridge, ADA container node, IVI Skycraft node — that keeps the baseline's `ethernet` pins.

**Scope:** in Nydus, clone **`baseline_phase1`** — the sanctioned clone source for every Room after the smoke test ([carsky-4-node-blueprint.md](../requirements/car-sky-guide/carsky-4-node-blueprint.md#8-the-blueprints-on-carsky)) — rename the clone, then delete the Bench and V2X nodes on the canvas. That is the route [§4.11](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#411-the-mini-blueprint-route) fixes, and it also states what deleting a node does to its pin and edge.

**Clone; do not build or import one.** §4.11 states that a script-built or imported blueprint arrives without its `ethernet` pins, and usually without the Skycraft `image` block, and is rejected at deploy.

Three nodes remain, at the baseline's own addresses and pin shapes. Change nothing else — leaving them untouched is what lets `4.5.8.9` later change one node's image and env and nothing more.

**The clone's name is the user's to pick**, and it is the only place the differentiator goes. Record it: every subtask after this one edits and deploys *that* blueprint — never `baseline_phase1` itself, and never the `-deploy` snapshot a deployment creates ([§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint)). Editing the snapshot by mistake costs the most time here.

**Acceptance:** the blueprint exists with exactly three nodes and their pins intact, confirmed by `6.5.8.3`'s read-back and recorded in `plans/doc/phase5-ivi-run.md`, which this subtask creates. Evidence commit by the orchestrating session once the user confirms.

**Dependencies:** none — needs no Phase 5 code and no image. **Commit:** `[5.5.8.1] docs: record the mini-blueprint composition`

### [ ] `4.5.8.2` — Set the ADA node's probe config; confirm the IVI node and measure its display *(Human)*

**Objective:** give the ADA node a config that proves the network hop before the simulator image exists, and confirm the IVI node can deploy at all.

**Scope — three things in the Nydus Inspector. Nothing is installed and nothing is deployed here.**

- **The ADA node's probe config** — the one [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) rung **V2** names: the netcheck image, with its next-hop host and port pointing at `10.99.0.13:47300`. Prefix the image with the registry host, and set `NET_RAW` so a capture line can corroborate the datagram on the wire (R6). This config stays until `4.5.8.9` replaces it.
- **The IVI node's `image` block** — the four fields of [node-ivi-ecu.md § Blueprint node config](../requirements/car-sky-guide/node-ivi-ecu.md#blueprint-node-config). On a clone they are already right; leave them alone. [§4.2](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#42-configure-the-blueprint-and-its-ivi-node) quotes the message a deploy is rejected with when they are missing.
- **Read the IVI node's Part Prefix, display width, height, DPI and GPU backend off the live node** and write them down. §4.2 says not to assume them; `16.5.8.11` needs the Part Prefix to point the widgets at the right parts, and the display size is the resolution `16.5.5.6`'s previews are drawn for ([§6.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) item 6).

Setting an image field is a canvas edit, not an installation: the platform pulls the image when the Room deploys.

**Acceptance:** `6.5.8.3`'s read-back shows the probe image, the next-hop env and `NET_RAW` on the ADA node and the four `image` fields on the IVI node; the measured display fields are recorded. Evidence commit by the orchestrating session once the user confirms.

**Dependencies:** after `5.5.8.1`. **Commit:** `[4.5.8.2] docs: record the mini-blueprint node configuration and measured display fields`

### [ ] `6.5.8.3` — Read the mini-blueprint back and confirm its topology *(car-sky)*

**Objective:** prove from stored state, not from the Inspector's truncated fields, that the blueprint is deployable before a Room slot is spent on it.

**Scope:** `GET /api/v1/blueprints/{id}` — the AI read-back row of [§4.2](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#42-configure-the-blueprint-and-its-ivi-node). Confirm every one of:

- one `ETHERNET` / `OUTPUT` pin on the ADA node and one on the IVI node, each edged to the bridge's single `INPUT` pin, in the shape [node-ivi-ecu.md § Pins](../requirements/car-sky-guide/node-ivi-ecu.md#pins) fixes;
- the bridge node's `bridgeMode` and subnet — without them the addresses have no network;
- the IVI node's four Skycraft `image` fields;
- the ADA node's image reference, `NET_RAW`, and every env value — **the next-hop port above all**, since a wrong port produces a silent no-traffic run that looks like a code defect.

`POST /api/v1/blueprints/{id}/validate` is a cheap second confirmation: it fails until every node has a pin.

**Acceptance:** the read-back excerpt recorded with every point confirmed, or the exact mismatch named and handed back to `5.5.8.1`/`4.5.8.2` for a canvas fix. **A deploy does not start on an unconfirmed blueprint.**

**Dependencies:** after `4.5.8.2`. **Commit:** `[6.5.8.3] docs: record the mini-blueprint topology read-back`

### [ ] `5.5.8.4` — Deploy the mini-blueprint *(Human)*

**Objective:** bring up the Room the rest of this group observes.

**Scope:** [§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint) — open the blueprint, click empty canvas for its Inspector, **New Deployment**, pick an **existing** Device from the dropdown, Deploy. Do not create a new Device. §5 keeps this Human because choosing the Device is a judgement call and deploying spends one of the two Room slots the comms track also draws on.

Deploy the blueprint itself, not the `-deploy` snapshot deploying creates. Watching the node badges belongs to `5.5.8.5`; expect the Skycraft node to lag the containers.

**Acceptance:** the deployment exists and its Room id is recorded; `5.5.8.5` confirms the phases. Evidence commit by the orchestrating session once the user confirms.

**Dependencies:** after `6.5.8.3`, and a free Room slot. **Commit:** `[5.5.8.4] docs: record the mini-blueprint deployment`

### [ ] `5.5.8.5` — Poll the nodes to `Running` and resolve every `nodeKey` *(car-sky)*

**Objective:** record that every node came up, and produce the keys every log route needs.

**Scope:** the AI row of [§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint) — poll `GET /api/v1/deployments/{roomId}/nodes` until every node reads `Running` with restart count 0, recording each entry's name. A node stuck in `Provisioning` is almost always an image that cannot be pulled ([§4.10](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#410-troubleshooting-the-deploy-and-install)); diagnose per [carsky-room-diagnostics](../.claude/skills/carsky-room-diagnostics/SKILL.md) rather than redeploying blind.

**Acceptance:** 3/3 nodes `Running` with restart count 0, and all three `nodeKey` values recorded — the precondition every §6 proof rests on.

**Dependencies:** after `5.5.8.4`. **Commit:** `[5.5.8.5] docs: record the mini-blueprint Room reaching Running`

### [ ] `4.5.8.6` — Start the ADB tunnel to the guest *(car-sky)*

**Objective:** leave the organizers' ADB tunnel serving a local port, so every step below can reach the Skycraft guest.

**Scope:** [§4.4](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#44-get-an-adb-endpoint) — one route, not a search. §5 assigns it to AI because it is a CLI invocation. Run the tunnel command exactly as §4.4 fixes it and **leave it running in its own terminal**: closing that terminal drops the tunnel, and `4.5.8.7` runs in a second one. The flags stay in §4.4 and are not copied here.

**Three inputs are human work and must be in hand first** — the CLI binary, the gateway URL and the derived token. All three come from the organizers, none is derivable from this repository, and §5's closing qualification keeps obtaining them a person's job. They are [§6.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) items 2–4, answered when they are handed over. **Record where each came from; never record the token value.**

**The route is supplied and unexercised by this team** (§6.1 item 1). Treat a failure as a finding, not a retry loop — §4.10's rows say what each failure means.

**Acceptance:** the CLI serving on the local port, recorded with the port used and the provenance of the three inputs, or the exact failure. `4.5.8.7` is what confirms the tunnel actually carries ADB.

**Dependencies:** after `5.5.8.5`, and after the three inputs are supplied. **Commit:** `[4.5.8.6] docs: record the ADB tunnel start against the Skycraft guest`

### [ ] `4.5.8.7` — Prove the ADB route and read the guest's properties *(car-sky)*

**Objective:** answer whether the guest is reachable and whether it will accept the APK at all — the two findings that invalidate every in-Room criterion below if negative.

**Scope:** [§4.5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#45-connect-and-check-the-guest) then [§4.6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#46-install-the-apk), both AI rows, over the tunnel `4.5.8.6` left running. Those sections carry the connect, property-read and install commands and what each failure means; none of that is restated. What is specific to running them before the app is finished:

- **Install whatever build exists now** — a local `assembleDebug`, or `16.5.7.1`'s artifact. Before `16.5.5.8` lands, that build has no launcher activity, so it installs and cannot be started. That is expected: this subtask proves the **route**, and `16.5.8.10` is where the finished build is installed and launched.
- **Confirm the evidence filter streams** — `adb logcat -s IVI_V2X`, the guest-side surface the whole demo's text evidence depends on.
- **Try the screenshot route once** — the scriptable alternative [§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence) offers. One call, recorded either way: a live route gives the later evidence subtasks a path that needs no browser.

The findings are [§6.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) items **1, 5 and 9**. Answer each on this deployment and write the answer down.

**If connect or install fails**, that is the finding: record it, and every criterion below degrades to **AAOS-emulator** evidence on an *automotive* system image — a phone image rejects the APK on the `automotive` feature. Escalate rather than retrying blind; §4.10 is a troubleshooting table, not a licence to repeat a failed route.

**Acceptance:** the outputs of §4.5 and §4.6 — or the exact failure — recorded, with the guest's API level against `minSdk 29`, its `automotive` answer, and the fallback decision if either is negative.

**Dependencies:** after `4.5.8.6`. **This is the phase's earliest risk: it needs no Phase 5 code and must not wait behind groups 5.1–5.7.** **Commit:** `[4.5.8.7] docs: record the proven ADB route and AAOS guest properties`

### [ ] `16.5.8.8` — Record the proven route in the IVI node guide *(agent — docs)*

**Objective:** write the facts `4.5.8.6` and `4.5.8.7` established into the per-node deploy guide, which is where node facts live.

**Scope:** extend [node-ivi-ecu.md](../requirements/car-sky-guide/node-ivi-ecu.md) § Post-deploy with, verbatim from the recorded outputs, the **facts** that file owns: that §4.4's tunnel carried ADB to this node, the local port it served, the connect target that answered, the guest's API level, and its automotive answer. **The commands are not copied in** — install is §4.6, launch and the port override are §4.7, and the logcat filter is §4.8; link them. That is the division the walkthrough states about itself: the node guide owns the node's *facts*, the walkthrough owns the *doing* ([walkthrough-driven-delivery.md](../.claude/rules/walkthrough-driven-delivery.md)). If the route failed, record that instead, plus the emulator fallback.

**Acceptance:** § Post-deploy carries the tunnel form that connected — or the failure — plus the API-level and automotive answers, and links §4.6–§4.8 for the commands instead of duplicating them. Every line traces to a recorded output; no invented values. Doc-only.

**Dependencies:** after `4.5.8.7` and `16.5.5.8` — the launch override must exist before it is documented as working. **Commit:** `[16.5.8.8] docs: record the proven ADB route and launch override in the IVI node guide`

### [ ] `4.5.8.9` — Switch the ADA node to the simulator's evidence config *(Human)*

**Objective:** put the R4 simulator on the wire toward `10.99.0.13:47300`.

**Scope:** open the mini-blueprint in Nydus — the blueprint, not the `-deploy` snapshot — click the ADA node, and replace `4.5.8.2`'s probe config with the **evidence config** of [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) rung **V4**. That rung fixes the image, the **relative** `command`, and the full env set — including `R4_SCENARIO` pointing at `approach.json`. Add the registry host prefix and keep `NET_RAW`.

Change nothing on the other two nodes. Addresses, the port and the pin shapes were fixed at the baseline, so this is the only node config that ever changes — §4.11 is why. Then deploy again per §4.3, wait for the ADA node to read `Running`, and open its log by either route in §4.8's log-surface table.

**Acceptance:** the ADA node `Running` with restart count 0, and its log showing V4's **link 1** — `[TX] … → 10.99.0.13:47300` at the configured rate — plus the capture-line corroboration. Evidence commit by the orchestrating session once the user confirms.

**Dependencies:** after `5.5.8.5`, and after `5.5.7.3` has pushed **and verified** the image — a green lane alone is not proof the tag reached the registry. **Commit:** `[4.5.8.9] docs: record the R4 simulator running on the mini-blueprint ADA node`

### [ ] `16.5.8.10` — V1: install and launch the Phase 5 APK, and record the boot-to-listener time *(car-sky)*

**Objective:** get the Phase 5 build running on the guest, close rung **V1**, and capture the one timing number no other phase can produce.

**Scope:** [§4.6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#46-install-the-apk) then the launch half of [§4.7](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#47-open-the-screen-and-launch-the-app) — install, confirm the package path, then start the activity, with the port override available. §5 assigns both rows to AI. The build comes from `16.5.7.1`, fetched per §3.3; the tunnel is `4.5.8.6`'s, still running. §4.1 fixes the ordering: the guest must exist before anything installs into it.

**This installs the APK. It does not set a node image field, does not touch the canvas, and does not open the Screen widget** — that is `16.5.8.11`, which needs the app already launched.

**Record the boot-to-listener time.** [m1-run-timing-and-event-triggering.md](../requirements/m1-run-timing-and-event-triggering.md) names the elapsed time from the AAOS guest starting to boot until `[LINK] state=bound port=47300` appears as the one number **only this phase can produce**, and as the **floor for the bench's start delay**: the IVI is the only node whose readiness cannot be observed from a container, so the bench must not start streaming before the guest is bound. Record guest boot → launcher, launch → `[LINK] state=bound`, and their sum. If `4.5.8.7` came back negative and this runs on an emulator, say so — an emulator figure is a lower bound, not the number.

**Acceptance:** install success and the package path; the app started; **rung V1** — `[LINK] state=bound port=47300` on `IVI_V2X` — and the three timing values, all recorded. Install failures map to §4.10; an SDK-level or `automotive` feature error is an escalation, not a retry.

**Dependencies:** after `4.5.8.9`, `4.5.8.7` and `16.5.5.8`. **Commit:** `[16.5.8.10] docs: record the APK install, launch and boot-to-listener time`

### [ ] `16.5.8.11` — Open the device widgets and confirm the R16 layout *(Human)*

**Objective:** close acceptance box 1 — the HMI runs on the AAOS node with the R16 layout, and the button and app areas switch what the Display Area shows.

**Scope:** the app is already installed and launched; this subtask opens the screen and looks at it, following the widget half of [§4.7](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#47-open-the-screen-and-launch-the-app): connect to the device the deployment created, add the **Screen** widget and set its Video, Touch and Keyboard parts from the Part Prefix `4.5.8.2` wrote down — **without the Touch part, clicks in the browser never reach the guest** — then add **Log** and **ADB** widgets. If the run is to be recorded, set the **Recorder Part now**: [§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence) cannot capture a run that has already happened.

A black screen is expected to be fixable, not fatal; §4.7 gives the recovery.

Then confirm two things on the screen:

- The **link indicator reads bound on port 47300** — rung V1 seen on the display rather than in the log, which proves the status bar is wired to the listener and not hardcoded.
- **Tapping Home, Apps and Settings changes what the Display Area shows**, with `[UI] mode=… cause=user` appearing in the Log widget. That is [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance)'s second table, first row.

**Acceptance:** the screen streaming live with clicks reaching the guest; a screenshot of the R16 layout with the link indicator bound, and a second showing a different Display Area mode after a tap. Evidence commit by the orchestrating session once the user confirms.

**Dependencies:** after `16.5.8.10`. **Commit:** `[16.5.8.11] docs: record the R16 layout running on the AAOS node`

### [ ] `4.5.8.12` — V2: prove the network hop with the probe datagram *(car-sky)*

**Objective:** close rung **V2** — a datagram from the ADA node reaches the guest and the receive loop survives it.

**Scope:** with the ADA node briefly back on `4.5.8.2`'s **probe config**, read both surfaces per [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) rung **V2**. The netcheck payload is not JSON, so **a `[DROP] reason=malformed …` per datagram is the pass**: it proves the socket, the bridge hop and the loop's survival in one observation, and it is the only rung that works before any warning traffic exists. The producer's `[TX]` line corroborates it. `[TX]` with nothing on `IVI_V2X` means the datagram is not arriving — re-check the pin address and the port before suspecting code.

Reconfiguring the node is a Human edit; **this subtask reads, it does not configure.** Where the schedule allows, run it while the probe config is still in place before `4.5.8.9` swaps it, and note the ordering used.

**Acceptance:** paired `[TX]` and `[DROP]` excerpts recorded, with the app still running afterwards. This closes the ADA→IVI network hop, which the connectivity smoke test could only check indirectly.

**Dependencies:** after `16.5.8.10`, and the probe config in place. **Commit:** `[4.5.8.12] docs: record the V2 probe-datagram evidence for the ADA to IVI hop`

### [ ] `17.5.8.13` — V3: the UI comes up from an injected sample, with no network *(car-sky broadcast, Human judgement)*

**Objective:** close rung **V3** — prove the whole UI path independently of the producer, so a later failure can be localised to the network rather than the app.

**Scope:** [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) rung **V3** — broadcast the dev-injector intent with a frozen sample name over `adb` (an AI row: it is a shell command), and watch the Screen widget (a Human judgement no log line replaces). The Display Area must switch to the Warning View **by itself** and draw the God View. A completed broadcast with no UI change means the installed build is a release build — the injector exists in the debug build only, by design.

**Acceptance:** the broadcast command's output and a screenshot of the resulting Warning View, recorded. If the guest is unreachable and this runs on an emulator, say so.

**Dependencies:** after `16.5.8.11` and `4.5.6.7`. **Commit:** `[17.5.8.13] docs: record the V3 dev-injector evidence for the UI path`

### [ ] `18.5.8.14` — V4 in text: read both log surfaces on `approach.json` *(car-sky)*

**Objective:** produce in text three of the four proofs of [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance) — the incoming warning, its parsed fields, and the event raised — making no visual judgement.

**Scope:** the "read the two log surfaces" AI row of [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human), against the two surfaces §4.8's table names: the ADA node's producer log over the logs route — **`container` is a mandatory query parameter, and omitting it returns 500** — and the guest's `adb logcat -s IVI_V2X`.

The rung is **V4** with `approach.json` on the ADA node. Its four links are the checklist and are not restated here; this subtask covers links 1–3, because link 4 is a visual judgement §5 assigns to Human. Note in the record that the scenario's first step carries a `null` `vehicleC`, so `17.5.8.15` can check it rendered without C.

**Acceptance:** §6 proofs 1, 2 and 3 as log excerpts — one `[RX] type=warning … cSource=v2x_relayed` per datagram corroborated by the producer's `[TX]`, the parsed `warningType` / `risk` / `cSource` / `cPos` fields on that line, and `[UI] mode=WarningView cause=warning` carrying `cause=warning` and not `cause=user`.

**Dependencies:** after `16.5.8.11` and `4.5.8.9`. **Commit:** `[18.5.8.14] docs: record the approach-scenario log evidence for the R4 warning chain`

### [ ] `17.5.8.15` — V4 on screen: confirm the God View and capture it *(Human)*

**Objective:** close acceptance boxes 2 and 3 — a warning brings the Warning View up showing ego, B and ghost C at the composed positions, and ghost C renders from `v2x_relayed` data only with the 2D drawing delivered.

**Scope:** start the recording, watch the Screen widget while `approach.json` plays, and check five things. The first is [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) V4's **link 4**; the rest belong to this scenario file.

1. The Display Area **switches to the Warning View by itself**, with nobody tapping anything: ego and B solid, ghost C dashed and translucent on a pulsing risk-coloured glow. **The scene alone is the warning** — [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components) and R17 forbid a legend, distance labels, a text overlay, a banner and the `[V2X]` badge. §4.8's link-4 row describes the annotated explanatory figure instead; see § Open items item 1, and judge against the HLD.
2. The scenario's **first step draws no C at all** — it carries a `null` `vehicleC` — and the app neither crashes nor shows a placeholder.
3. When the stream stops, the view **times out back to the previous mode**, not merely to Idle. Restoring the *previous* mode is `16.5.4.5`'s behaviour, which V4's own row does not name.
4. **Risk climbs low → medium → high** across the approach and the glow colour follows it.
5. **No `[? UNKNOWN SOURCE]` marker** appears where ghost C belongs. If one does, stop: V4 calls that a **blocking defect**, not a display quirk, and it must be reported rather than worked around.

Recording and screenshots per [§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence), using the Recorder Part `16.5.8.11` set.

**Acceptance:** §6 proof 4 — a recording showing the God View with ghost C dashed and glowing — with the null-C first step, the risk progression and the timeout-restore all observed, and `18.5.8.14`'s excerpt supplying `cSource=v2x_relayed` on every warning in text. Evidence commit by the orchestrating session once the user confirms.

**Dependencies:** after `18.5.8.14`. **Commit:** `[17.5.8.15] docs: record the God View evidence with v2x_relayed provenance`

### [ ] `4.5.8.16` — Switch the ADA node to `degrade.json` *(Human)*

**Objective:** put the degradation scenario on the wire.

**Scope:** click the ADA node in Nydus, change **one** environment value — the scenario path — and deploy again or restart the node. Work on the blueprint, not the `-deploy` snapshot. Leave the image, the `command`, the addresses and the port exactly as `4.5.8.9` set them. §5 assigns node-config edits to Human.

**Acceptance:** the ADA node `Running` with restart count 0 and its log showing `[TX]` lines for the new scenario, recorded. Evidence commit by the orchestrating session once the user confirms.

**Dependencies:** after `17.5.8.15`. **Commit:** `[4.5.8.16] docs: record the degradation scenario running on the ADA node`

### [ ] `4.5.8.17` — V5 in text: degradation, the guard trip and loop survival *(car-sky)*

**Objective:** produce the text half of acceptance box 4 — a newer message with an unknown `warningType` degrades gracefully — and of the two defensive paths beside it.

**Scope:** both log surfaces again, against rung **V5** of [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging). Its three rows are exactly this scenario's three steps, each with its correct and incorrect result stated there and not restated here. Two things the excerpt must show explicitly: the unknown `warningType` appearing **preserved** in logcat and never rewritten, and the raw step producing `[DROP] reason=malformed …` with the next valid warning still arriving.

**Acceptance:** logcat and producer-log excerpts covering all three V5 rows, including the ERROR line the guard trip emits.

**Dependencies:** after `4.5.8.16`. **Commit:** `[4.5.8.17] docs: record the degradation and loop-survival log evidence`

### [ ] `17.5.8.18` — V5 on screen: confirm the three outcomes, capture, tear down *(Human)*

**Objective:** see V5's three outcomes on the display, and release the Room slot.

**Scope:** watch the Screen widget while `degrade.json` plays, screenshot each of rung **V5**'s three outcomes, and judge each:

1. **Unknown warning type** — a generic warning is drawn. A fatal exception is a failure.
2. **`own_sensor` provenance** — the yellow `[? UNKNOWN SOURCE]` marker appears where ghost C would be. **Here the marker is the pass.** If ghost C is drawn normally instead, the R19 wiring `17.5.4.4` armed is broken — a blocking finding for the phase, not a display quirk. Stop and report it.
3. **A raw non-JSON message** — the app keeps running and the next valid warning still draws.

**Then tear the Room down, and only after `4.5.8.17` has saved its excerpts** — the log route returns nothing once the Room is gone. Delete the deployment per [§4.12](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#412-tear-down); the blueprint stays and is redeployable. The slot matters: only two Rooms run at once and the comms track needs one.

**Acceptance:** screenshots for all three V5 rows and the teardown confirmed. This record closes the isolated IVI test's evidence trail. Evidence commit by the orchestrating session once the user confirms.

**Dependencies:** after `4.5.8.17`. **Commit:** `[17.5.8.18] docs: record the degradation outcomes, guard trip and teardown`

---

## Task Group 5.9 — System verification test, the full blueprint (serves R4, R5, R6, R16, R17, R18, R19)

> The second configuration of [HLD §12](../IVI_ECU/doc/ivi-ecu-hld.md#12-test-strategy): five nodes — Bench, V2X ECU, ADA ECU, IVI ECU and the Ethernet Bridge — each on its own real image. The warning the IVI renders starts in a bench scenario and travels the whole relay instead of coming from a stand-in beside it. [milestone1.md § Phase 5](milestone1.md#phase-5--ivi-hmi-mock-driven-r16-r17--display-track-parallel-from-the-start) names this plan as the only home of that run.
>
> **Nothing about the IVI node changes between the two Rooms** — same address, same pin, same `image` block, same APK, same expected observables. That is what makes a difference between the runs a producer finding rather than an IVI one.
>
> **Composition and creation route:** [deploy-ada-ecu-walkthrough.md §5.6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#56-the-full-blueprint-route), which states its mechanics are that document's §4.1–§5.5 with only the composition differing, and hands consumer-side evidence to [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) of the IVI walkthrough.
>
> **This group builds no image.** Each node's image is published by the phase that owns that node.

### [ ] `5.5.9.1` — Clone the baseline into the full blueprint and set every node's real image *(Human)*

**Objective:** produce the 5-node blueprint, each container node carrying its own real image.

**Scope:** in Nydus, clone **`baseline_phase1`** and edit the clone. Never edit the baseline itself and never import a blueprint file — an imported or script-built blueprint arrives without its `ethernet` pins and usually without the Skycraft `image` block, and is rejected at deploy. Each node's image, config and pin come from [carsky-4-node-blueprint.md](../requirements/car-sky-guide/carsky-4-node-blueprint.md) and the per-node files it points at.

- **Set each container node's image field** to that node's own real image, whatever the clone arrived carrying.
- **Leave the IVI node's Skycraft `image` block alone.** Without its four fields the deploy is rejected outright.
- **Leave the ADA node's `command`, capabilities, env, address and port alone.** §5.6 states they are identical in both compositions; only the neighbours change.
- **Confirm the ADA node still has `NET_RAW`.** Here it is not optional: the Android node runs no container, so there is no sink log, and the ADA node is the only place a capture line can record the outgoing warning.
- Work on the blueprint, never on a `-deploy` snapshot.

**Acceptance:** a 5-node blueprint whose every node names its real image, confirmed by `5.5.9.2`'s read-back. Evidence commit by the orchestrating session once the user confirms.

**Dependencies:** every node's image published by its own phase. **Commit:** `[5.5.9.1] docs: record the full blueprint composition and its node images`

### [ ] `5.5.9.2` — Read the full blueprint back and confirm all five nodes *(car-sky)*

**Objective:** confirm from stored state that five nodes carry the right images, pins and addresses, and that every image is pullable, before a Room slot is spent.

**Scope:** `GET /api/v1/blueprints/{id}` — the read-back AI row of both walkthroughs. Confirm each node's image reference, `command`, env and capabilities against its node file, and one `ETHERNET` / `OUTPUT` pin per non-bridge node edged to the bridge's single `INPUT` pin. Confirm the IVI node's four Skycraft `image` fields.

Then confirm each image resolves in the registry. An image that cannot be pulled is the `Provisioning` hang of [§4.10](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#410-troubleshooting-the-deploy-and-install); the catalog check is an AI row of the netcheck walkthrough.

**Acceptance:** the read-back excerpt with all five nodes confirmed and every image resolvable, or the exact mismatch named and handed back to `5.5.9.1`.

**Dependencies:** after `5.5.9.1`. **Commit:** `[5.5.9.2] docs: record the full blueprint read-back and image confirmation`

### [ ] `5.5.9.3` — Deploy the full blueprint *(Human)*

**Objective:** bring up the Room the whole-system evidence is gathered in.

**Scope:** [§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint) — **New Deployment**, an **existing** Device, Deploy. The blueprint, not its snapshot. The full blueprint is one deployment like any other, so the two-Room budget still applies and the isolated test's Room must be released first.

**Acceptance:** the deployment exists and its Room id is recorded; `5.5.9.4` confirms the phases. Evidence commit by the orchestrating session once the user confirms.

**Dependencies:** after `5.5.9.2`, and after the teardown at `17.5.8.18`. **Commit:** `[5.5.9.3] docs: record the full blueprint deployment`

### [ ] `5.5.9.4` — Poll the five nodes to `Running` and resolve every `nodeKey` *(car-sky)*

**Objective:** record that all five nodes came up, and produce every key the log routes need.

**Scope:** poll `GET /api/v1/deployments/{roomId}/nodes` until every node reads `Running` with restart count 0, recording each name. Four container nodes and one Skycraft node, the latter slowest. A node stuck in `Provisioning` is the image-pull signature; diagnose per [carsky-room-diagnostics](../.claude/skills/carsky-room-diagnostics/SKILL.md).

**Acceptance:** 5/5 nodes `Running` with restart count 0 and every `nodeKey` recorded.

**Dependencies:** after `5.5.9.3`. **Commit:** `[5.5.9.4] docs: record the full blueprint Room reaching Running`

### [ ] `16.5.9.5` — Install and launch the APK on the system-test guest *(car-sky)*

**Objective:** get the Phase 5 build running on the IVI node of the full topology.

**Scope:** [§4.6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#46-install-the-apk) then the launch half of [§4.7](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#47-open-the-screen-and-launch-the-app), both AI rows, over a tunnel started as §4.4 fixes it in the form `16.5.8.8` recorded as working. The Room is new, so the guest is new and the install runs again. §4.1's ordering holds: the node must be `Running` first.

This is the **APK**, not a node image field — the container images were set at `5.5.9.1` and pulled at deploy.

**Acceptance:** install success, the package path, the app started, and `[LINK] state=bound port=47300` on `IVI_V2X`.

**Dependencies:** after `5.5.9.4`. **Commit:** `[16.5.9.5] docs: record the APK install and launch on the system-test guest`

### [ ] `16.5.9.6` — Open the device widgets and arm the recorder *(Human)*

**Objective:** make the guest's display visible for the system test, with recording armed before anything is sent.

**Scope:** the widget half of [§4.7](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#47-open-the-screen-and-launch-the-app), which §5 keeps Human: connect to the device this deployment created, add the **Screen** widget with its Video, Touch and Keyboard parts from this node's Part Prefix, and add the **Log** and **ADB** widgets beside it.

**Set the Recorder Part before anything is sent.** The recorded evidence comes from this run and [§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence) cannot capture a run that has already happened.

**Acceptance:** the screen streaming live with the recorder armed. Evidence commit by the orchestrating session once the user confirms.

**Dependencies:** after `16.5.9.5`. **Commit:** `[16.5.9.6] docs: record the device widgets opened on the system-test guest`

### [ ] `19.5.9.7` — Read the whole relay's logs end to end *(car-sky)*

**Objective:** produce the text evidence that the warning the IVI renders originated in a bench scenario and travelled the full relay.

**Scope:** authenticate per [carsky-deploy-preflight](../.claude/skills/carsky-deploy-preflight/SKILL.md), then read every surface the topology exposes, with `container=user` on the logs route.

- **Producer side:** the relayed-message and track-store checks of [deploy-ada-ecu-walkthrough.md §5.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#51-check-1--the-relayed-message-is-received-and-raises-its-event) and [§5.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#52-check-2--both-vehicles-are-in-the-track-store), unchanged in this composition — but with the relayed traffic now originating in the real V2X ECU driven by the bench scenario, so the station and object identifiers and the distance profile come from that scenario rather than from node env, and that document's distance lever is not available.
- **Consumer side:** §5.6 states that the sink-log check has **no counterpart here** — the Android node runs no container — so consumer-side evidence is the guest's own log: `adb logcat -s IVI_V2X`.
- **Wire evidence:** the ADA node's capture line is the only record of the outgoing warning in this composition, which is why its `NET_RAW` was required at `5.5.9.1`.

**Save every node log before teardown** — the log route returns nothing once the Room is gone.

**Acceptance:** [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance) proofs 1–3 on the guest side plus the producer-side checks of [deploy-ada-ecu-walkthrough.md §8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance), with `cSource=v2x_relayed` on every rendered warning, correlated across nodes by timestamp.

**Dependencies:** after `16.5.9.5`. **Commit:** `[19.5.9.7] docs: record the system-test log evidence across the relay`

### [ ] `19.5.9.8` — Confirm the God View on live relayed data, capture it, tear down *(Human)*

**Objective:** see the warning view come up from data that travelled the whole relay, capture it, and release the Room slot.

**Scope:** watch the Screen widget with the recording running, and confirm the Display Area switches itself to the Warning View and draws the God View — V4's **link 4**, captured per [§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence). Only the source of the data differs from the isolated test, so the drawing to expect is the same one — judged against [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components), per § Open items item 1 — and a `[? UNKNOWN SOURCE]` marker where ghost C belongs is still a blocking defect.

**Then tear the Room down**, after `19.5.9.7` has saved every node log. The blueprint stays and is redeployable.

**Acceptance:** a recording showing the God View drawn from live relayed data, with `19.5.9.7`'s excerpts backing it in text; the deployment deleted. Evidence commit by the orchestrating session once the user confirms.

**Dependencies:** after `19.5.9.7` and `16.5.9.6`. **Commit:** `[19.5.9.8] docs: record the system-test God View evidence and teardown`

---

# Execution order

Dependencies are real — the Gradle project graph, contract artifacts, deployed Room state — not default assumptions. **Three subtasks depend on nothing and open at once:** `4.5.1.1` (code), `16.5.7.1` (CI), `5.5.8.1` (the mini-blueprint, Human).

```
Lane A  foundation:   4.5.1.1 ─► 4.5.1.2 ─► { 4.5.1.3 ∥ 4.5.1.4 } ─► 4.5.1.5
                                 4.5.1.6 (after 4.5.3.1)   │ gate for lanes B–F
Lane B  :serializer:  4.5.2.1 ─► 4.5.2.2 ─► 4.5.2.3
Lane C  :observer:    4.5.3.1 ─► 4.5.3.2 ─► 4.5.3.3 ─► 4.5.3.4 ─► 4.5.3.5
                      (4.5.3.1 needs only 4.5.2.1 — starts before lane B ends)
Lane D  app logic:    { 4.5.4.1 ∥ 4.5.4.2 ∥ 4.5.4.3 } ─► 17.5.4.4 ─► 16.5.4.5
Lane E  app UI/shell: 18.5.5.1 ─► 4.5.5.2 ─► 4.5.5.7 ─► 16.5.5.8
                      17.5.5.3 ─► 17.5.5.4 ─► 16.5.5.6   (16.5.5.6 also needs 16.5.4.5)
                      17.5.5.5 ∥ everything               (4.5.5.7 also needs lane D through 17.5.4.4)
Lane F  test equip:   4.5.6.1 ─► 4.5.6.2 ─► 4.5.6.3 ─► 4.5.6.4 ─► 4.5.6.5 ─► 5.5.6.6
                      4.5.6.7 (after 16.5.5.8 + 4.5.4.2)
Lane G  CI:           16.5.7.1 (∥ everything)   4.5.7.2 (after 4.5.6.4)   5.5.7.3 (after 5.5.6.6)
Lane H  isolated:     5.5.8.1 ─► 4.5.8.2 ─► 6.5.8.3 ─► 5.5.8.4 ─► 5.5.8.5 ─► 4.5.8.6 ─► 4.5.8.7
                        ─► 4.5.8.9 ─► 16.5.8.10 ─► 16.5.8.11 ─► { 4.5.8.12 ∥ 17.5.8.13 }
                        ─► 18.5.8.14 ─► 17.5.8.15 ─► 4.5.8.16 ─► 4.5.8.17 ─► 17.5.8.18
                      (5.5.8.1 → 4.5.8.7 need no Phase 5 code and run parallel to lanes A–G;
                       4.5.8.6 also needs the organizers' CLI, gateway URL and token in hand;
                       16.5.8.8 branches off 4.5.8.7, also needing 16.5.5.8;
                       4.5.8.9 needs 5.5.7.3's verified image; 16.5.8.10 needs 16.5.5.8)
Lane J  system:       5.5.9.1 ─► 5.5.9.2 ─► 5.5.9.3 ─► 5.5.9.4 ─► 16.5.9.5 ─► 16.5.9.6 ─► 19.5.9.7 ─► 19.5.9.8
                      (5.5.9.1 needs every node's real image, published by its own phase;
                       5.5.9.3 needs the Room slot lane H releases at 17.5.8.18)
```

- **Parallel:** lanes B, D, E-partial, F and G against each other once lane A's `:contract` exists; the three `4.5.4.*` subtasks; `17.5.5.3`/`17.5.5.4`/`17.5.5.5` against lane E's shell chain. **Lane H's first seven subtasks are parallel with all code work by design** — they are canvas, deploy and tunnel work needing no Phase 5 code, and must not wait for it.
- **Sequential:** every arrow. Lane A is strictly sequential and gates the rest, because a Gradle module graph cannot be built out of order. Lanes H and J are strictly sequential — each step's evidence depends on the previous step's Room state.
- **Lane J follows lane H on the Room budget, not on logic.** Only two Rooms run at once and the comms track holds one.
- **Spawn order:** `4.5.1.1`, `16.5.7.1` and `5.5.8.1` open together. The rest of lane H opens once `5.5.7.3` has pushed a verified image; lane J once every node's real image exists. The *car-sky* subtasks in both lanes are spawned at the Room events they attach to.

## Critical path

The shortest ordered set that closes all four committed boxes:

`4.5.1.1 → 4.5.1.2 → 4.5.1.3 → 4.5.1.4 → 4.5.1.5 → 4.5.2.1 → 4.5.2.2 → 4.5.3.1 → 4.5.1.6 → 4.5.3.2 → 4.5.3.3 → 4.5.4.1 → 4.5.4.2 → 4.5.4.3 → 17.5.4.4 → 16.5.4.5 → 18.5.5.1 → 4.5.5.2 → 17.5.5.3 → 17.5.5.4 → 16.5.5.6 → 4.5.5.7 → 16.5.5.8 → (lane F through 5.5.6.6) → 5.5.7.3 → 4.5.8.9 → 16.5.8.10 → 16.5.8.11 → 18.5.8.14 → 17.5.8.15 → 4.5.8.16 → 4.5.8.17 → 17.5.8.18`

with **`5.5.8.1` → `4.5.8.7` running alongside it**, unblocked from the start. Those seven do not sit on the path, but they decide whether its last steps produce in-Room or emulator evidence.

**The isolated IVI test closes the path.** Every committed box is met against the simulator feed, which is what "mock-driven" means for this phase.

**The system test is not on the path.** It produces the whole-system evidence Phase 6's convergence run builds on — valuable, and not a gate on Phase 5.

**Droppable without failing a box, in this order if time runs short:** `4.5.6.7` and `17.5.8.13` (the dev injector and its rung — needed only if the ADB or UI route proves awkward); `4.5.8.12` (V2 — its hop is re-proven by V4); `4.5.2.3` and `4.5.3.5` (extra test depth, not extra behaviour); `state-stream.json` inside `4.5.6.4` (the periodic `state` message is optional on the producer side and no box depends on it); `17.5.5.5` (the banner is built for D11's completeness and mounted nowhere). **Not droppable at any price:** `17.5.4.4`'s guard-armed test and `17.5.5.4`'s guard test — dropping either leaves the R19 guard present, passing and disabled.

# Acceptance traceability

| Acceptance box | Closed by |
|---|---|
| The HMI runs on the AAOS node with the R16 layout; the button and app areas switch the Display Area | `16.5.5.6` · `16.5.5.8` (the launcher entry) · `16.5.4.5` · deployed by `5.5.8.1`–`5.5.8.4`, confirmed `Running` by `5.5.8.5`, the tunnel started at `4.5.8.6` and proven at `4.5.8.7`, installed and launched by `16.5.8.10`, observed by `16.5.8.11` |
| An R4 warning brings the Warning View up with ego, B and ghost C at the composed positions | `4.5.2.2` · `4.5.3.3` · `4.5.4.2` · `17.5.4.4` · `16.5.4.5` · `17.5.5.4` · `16.5.5.6` · fed by `4.5.6.3`/`4.5.6.4` (`approach.json`) and `4.5.8.9` · proven without a network by `17.5.8.13` · read by `18.5.8.14` and seen by `17.5.8.15` |
| Ghost C renders from `v2x_relayed` data only; the 2D drawing is delivered | **`17.5.4.4`** (the D12 snapshot wiring that arms the guard — without it the guard silently disables) · **`17.5.5.4`** (the guard itself, under test) · `17.5.5.3` · `16.5.5.6` · `4.5.3.3` (`cSource=` on every `[RX]`) · the guard-trip step in `4.5.6.4` · evidenced in text by `18.5.8.14`, on screen by `17.5.8.15`, tripped at `17.5.8.18`, and shown again from live relayed data by `19.5.9.7`/`19.5.9.8` |
| A newer message with an unknown `warningType` degrades gracefully | `4.5.1.5` (`R4AdditiveVersionTest`) · `4.5.2.2` (decode preserves the wire value, D4) · `4.5.4.3` (the generic presentation) · `4.5.6.4` (`degrade.json`) · read by `4.5.8.17` and observed by `17.5.8.18` |
| Optional paths, only if built | **Not built.** Declared, not attempted — D11 makes 3D and multi-process wake-on-warning optional and nothing depends on either. `4.5.5.2`'s foreground service keeps multi-process reachable and `17.5.5.4`'s seam keeps 3D reachable. Recorded as not built by `17.5.8.18`, per [walkthrough §6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance)'s instruction to record rather than leave ambiguous |

**Beyond the boxes.** The system test (`5.5.9.1`–`19.5.9.8`) closes no box on its own. It proves the same IVI behaviour inside the full topology with every node on its real image, and its record is what Phase 6's convergence run starts from.

# Open items

Carried, not decided. No subtask may close one by assuming an answer.

| # | Item | Owner / closes at |
|---|---|---|
| 1 | **The walkthrough and the HLD disagree about what the God View draws.** [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) V4 link 4 and [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance) proof 4 expect a `[V2X]` badge and `d_AB`/`d_AC` distance labels; [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components), D11 and **R17 itself** forbid them, and R17 names that annotated figure as explanatory and *not* what the IVI renders. **This plan follows the HLD and R17**, which the authority order puts above a walkthrough's description of a screen; the walkthrough stays authoritative for procedure. | Report to [[project-researcher]] to correct §4.8 and §6, per [walkthrough-driven-delivery.md](../.claude/rules/walkthrough-driven-delivery.md) — a walkthrough is never edited by the agent that finds the error. Flag before `17.5.8.15` judges the screen |
| 2 | **ADB reach to the Skycraft guest.** One route exists, the organizers' tunnel, and this team has not run it ([§6.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) item 1); its CLI, gateway URL and token are supplied, not derived (items 2–4) | `4.5.8.6` starts it, `4.5.8.7` proves it. Negative ⇒ every later subtask in group 5.8 degrades to AAOS-emulator evidence |
| 3 | **AAOS guest API level** against `minSdk 29`, and the `automotive` feature (§6.1 item 5) | `4.5.8.7`, same connection |
| 4 | **The guest's Part Prefix, display size, DPI and GPU backend** (§6.1 items 6 and 7) — the resolution `16.5.5.6`'s previews are drawn for | `4.5.8.2` measures them; `16.5.5.6` consumes them |
| 5 | **Coroutines version skew** between `:observer` and what AndroidX resolves in `:app` — mitigated by the catalog (`4.5.1.1`), but a skew shows as a runtime failure, not a build failure | Watch for it at `16.5.8.10` |
| 6 | **Room budget: two concurrent deployments.** `17.5.8.18` and `19.5.9.8` release theirs; coordinate with the comms track before `5.5.8.4` deploys | `17.5.8.18`, `19.5.9.8` |
| 7 | **The AAOS boot-to-listener time sets the bench start-delay floor** — a number no other phase can produce ([m1-run-timing-and-event-triggering.md](../requirements/m1-run-timing-and-event-triggering.md)). **No startup handshake is coming**: readiness is the Deployment-Viewer check plus that delay, so nothing here should be designed around a barrier the topology has no reverse path for | Measured at `16.5.8.10`; consumed by the bench key, then **user** / Phase 6 |
| 8 | **MTU headroom** on the bridge hop — a non-issue for a ~450 B warning against a 2048 B buffer, but formally open | Carried from Phase 0 |

# Deliberately not in this phase

Each is a decision with its reason, not an oversight.

- **3D and multi-process wake-on-warning** — optional, not committed M1 deliverables (D11). No subtask attempts either, and not even a stub: a component whose only acceptance is that it does not crash earns nothing. `17.5.5.4`'s seam keeps 3D swappable and `4.5.5.2`'s foreground service keeps multi-process reachable.
- **`WarningBannerOverlay` mounted anywhere** — forbidden by a standing user decision (D11); the canvas must render unobstructed. It is built (`17.5.5.5`) and mounted nowhere.
- **A `[V2X]` badge, distance labels, a legend or any text overlay on the God View** — R17 and [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components) exclude them; they belong to the annotated explanatory figure. See § Open items item 1.
- **Robolectric, a coverage threshold, and leak tooling** — none has a basis in R4, R16 or R17 acceptance; the boxes are behavioural, and D2's plain-JVM split is what removes the need for Robolectric.
- **Runtime JSON-Schema validation on the device** — the typed decode already enforces required fields and types, and the schema is enforced in `4.5.1.5` on both sides of the contract.
- **A listener that gives up after N socket errors** — `4.5.3.4`'s back-off never stops retrying: a listener that stops mid-run is worse for a recorded demo than one that keeps rebinding.
- **A repetition or cadence CLI flag on the simulator** — repetition and rate are scenario data, not flags (D9).
- **Real ADA data in group 5.8.** The phase is mock-driven by definition; the simulator honours the real ADA node's env var *names* so Phase 6 is an image swap with no node-config edit.
- **A latency criterion.** Neither R4, R16 nor R17 states one, and no acceptance box turns on it.
- **Map or GNSS data on the IVI** — relative geometry only (D11).

---

*Decomposed by [[project-planner]] from [ivi-ecu-hld.md](../IVI_ECU/doc/ivi-ecu-hld.md), its decision record and two research notes, [m1-cooperative-awareness.md](../requirements/m1-cooperative-awareness.md) R4/R16/R17, and [milestone1.md § Phase 5](milestone1.md#phase-5--ivi-hmi-mock-driven-r16-r17--display-track-parallel-from-the-start); the two Room groups from [deploy-ivi-hmi-walkthrough.md](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md) per stage 2 of [walkthrough-driven-delivery.md](../.claude/rules/walkthrough-driven-delivery.md). 9 task groups, 63 subtasks: 38 agent, 12 car-sky, 12 human, and one (`17.5.8.13`) split between a car-sky command and a human judgement. Nothing started.*
