# Phase 5 — IVI HMI (R4, R16, R17): Full Task Breakdown

> **Authority & context:**
>
> - **Phase content:** [milestone1.md § Phase 5](milestone1.md#phase-5--ivi-hmi-mock-driven-r16-r17--display-track-parallel-from-the-start) — its five acceptance checkboxes are the phase output.
> - **Design:** [phase5-ivi-hld.md](../IVI_ECU/doc/phase5-ivi-hld.md) (commit `85387b5`) with [phase5-ivi-components.puml](../IVI_ECU/doc/phase5-ivi-components.puml) and [phase5-ivi-callflow.puml](../IVI_ECU/doc/phase5-ivi-callflow.puml). Every target path below is cited verbatim from its **§3.1 / §3.2** folder map; decisions **D1–D11**, module interfaces **§5**, log shapes **§5.4**, CI **§6.1**, test ladder **§7**, deployment **§8**, the latent defect **§9.2**, open items **§11**.
> - **Research notes:** [phase5-mini-blueprint.md](../IVI_ECU/doc/research_notes/phase5-mini-blueprint.md) · [phase5-r4-simulator.md](../IVI_ECU/doc/research_notes/phase5-r4-simulator.md) · [phase5-r4-parsing.md](../IVI_ECU/doc/research_notes/phase5-r4-parsing.md) · [phase5-ivi-implementation-notes.md](../IVI_ECU/doc/research_notes/phase5-ivi-implementation-notes.md) — non-authoritative; the HLD wins on conflict.
> - **Requirements:** [m1-cooperative-awareness.md §2](../requirements/m1-cooperative-awareness.md) R4, R16, R17 (plus R5, R6, R18, R19 where this phase touches them) — referenced by number, never restated.
> - **Deploy facts:** [node-ivi-ecu.md](../requirements/car-sky-guide/node-ivi-ecu.md) · [carsky-4-node-blueprint.md](../requirements/car-sky-guide/carsky-4-node-blueprint.md) · [deploy-walkthrough-netcheck.md](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md).
> - **Bring-up procedure:** [deploy-ivi-hmi-walkthrough.md](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md) — **authoritative** for the APK's build (local and CI), retrieval from CI, blueprint deploy, `adb install`, launch and verification. **Every subtask below that installs, launches, observes or reads logs from the IVI app cites the section that governs that step instead of restating it** — groups 5.7, 5.8 and 5.9 throughout, plus `16.5.5.5` (§2.6, §4.7) and `4.5.6.7` (§4.8 V3). Its [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human) fixes which of those steps an agent can perform and which need a human, and is what the *USER-MANUAL* labels below follow.
> - **Rules:** [task-planning-conventions.md](../.claude/rules/task-planning-conventions.md) (`X.Y.Z.W`; subtask discipline restated once in § Subtask discipline) · [node-code-layout.md](../.claude/rules/node-code-layout.md).
>
> **Task ID legend:** `X.5.Z.W` — X = requirement served · 5 = this phase · Z = task group · W = subtask. IDs are stable; never renumber.
>
> **Independence note.** This breakdown was written from the requirements and the HLD alone, from zero, on 2026-08-02. It does not read, reconcile with, or inherit IDs from any earlier Phase 5 task file, and it marks nothing as pre-existing except where the HLD itself designates a file `[C]` (already committed) or `[R]` (relocated verbatim).
>
> **Gap check (2026-08-02, orchestrating session).** After the independent pass, this plan was diffed against [phase5_tasks.md](phase5_tasks.md). Four gaps were closed here: the renderer-guard unit test (`17.5.5.9`), the unreachable `R4LinkState.Error` (`4.5.3.4`), APK-size recording (`16.5.7.1`), and § Prior work below. Everything the older file carries that this one deliberately does not is listed in § Deliberately not in this phase. `phase5_tasks.md` itself was not modified.

## Phase 5 overview

**Objective.** The IVI renders the R17 God View — ego, B, and ghost C — from R4 messages alone, inside the R16 layout, on a launchable APK; and an R4 simulator plus a 3-node mini-blueprint produce the traffic and the in-Room evidence that closes the phase's acceptance boxes.

**Input (must exist before start — all present as of 2026-08-02):**

- R4 frozen in Phase 0: [contracts/r4-ada-ivi.schema.json](../contracts/r4-ada-ivi.schema.json), the four samples under [contracts/samples/](../contracts/samples/), and the committed Kotlin binding + `R4RoundTripTest` / `R4AdditiveVersionTest`.
- The Phase 5 HLD (`85387b5`) and its four research notes.
- `IVI_ECU/` as a single-module Gradle project (`:app`) carrying the contract layer (`model/`) and the drawing layer (`ui/view/`), AGP 8.13 / Kotlin 2.2.20 / Compose BOM 2024.09.03 / `minSdk 29`, `targetSdk 33`, `compileSdk 34`.
- CarSky access with the baseline blueprint `trial2_minh_netcheck`, the `AAOS` artifact (`x9oqgIwzTp1m26SWIQqJt` / `xSU_Q7YJZUxxUgDr4Ugcp`, `0.0.1`, `aarch64`), and `registry.hackathon-2.carsky.io/m1-netcheck:latest` already pushed.
- GitHub secret `CARSKY_ZOT_API_KEY` and the reusable [verify-arm64-image](../.github/actions/verify-arm64-image) action.

**Output (phase acceptance = the five milestone boxes):**

- [ ] The HMI runs on the AAOS node with the R16 layout; button/app areas switch what the Display area shows.
- [ ] **(Dev)** A mock R4 warning brings the warning view up showing ego, B, and ghost C at the composed positions.
- [ ] Ghost C renders from `v2x_relayed` data only; the 2D drawing is delivered (R17 — 3D stays optional).
- [ ] A newer message with an unknown `warningType` degrades gracefully (R4 additive-version test).
- [ ] Optional paths, only if built: an ADA message wakes the separate warning app; 3D renders through the view seam.

Per-subtask traceability to these five boxes: § Acceptance traceability.

**Suggested branch (suggestion only — creation, checkout and push are the user's call):** `feat/phase5-ivi-hmi`

**Deadline context.** Hard milestone deadline 2026-08-08; this plan is written 2026-08-02. The critical path to the five boxes is § Critical path — the shortest ordered set of subtasks that closes them; everything else is quality work that can be dropped or deferred without failing a box.

### Execution split legend

| Label | Meaning |
|---|---|
| *agent* | implemented by a spawned implementation subagent (default) |
| *car-sky* | executed by the [[car-sky]] agent (deploy preflight → build/push/deploy/verify); planner keeps the ID and done-tracking |
| *USER-MANUAL* | Nydus UI / ADB / device steps performed by the user; the plan tracks them, no agent performs them; the evidence-record commit is made by the orchestrating session after the user confirms |

### Subtask discipline (applies to every subtask below)

Per [task-planning-conventions.md § Subtask discipline](../.claude/rules/task-planning-conventions.md#subtask-discipline-non-negotiable): **single objective · no out-of-scope code · exactly one atomic commit with the stated message · build passes · unit tests pass · the brief is self-contained.** Implementation subagents inherit this as their definition of done.

Two standing constraints every `IVI_ECU/` subtask inherits:

- **No hardcoded tunables** (CLAUDE.md principle 5): ports, buffer sizes, timeouts, cadences and scales come from `BuildConfig` + launch override (D10) or from the simulator's env/scenario file — never a literal in a class.
- **No module declares its own repositories.** `settings.gradle.kts` sets `RepositoriesMode.FAIL_ON_PROJECT_REPOS`; a module `repositories { }` block fails the build (D8).
- **No `android.util.Log` call on a unit-tested path in `:app`.** The stubbed Android jar throws `RuntimeException("Stub!")` from every `Log` method, so a logging line inside tested logic fails the test for a reason unrelated to the logic — PR #2 hit exactly this and spent a fix commit on it. D2 already keeps `Log` out of `:serializer`/`:observer`; in `:app`, log through `AndroidR4Logger` (`18.5.5.1`) and inject a recording logger in tests. `testOptions { unitTests { isReturnDefaultValues = true } }` is the fallback, not the first move — it silences the symptom for the whole module.

**Status tracking:** as execution proceeds each subtask gains a `**Status:**` line appended in that subtask's own atomic commit, recording done/blocked plus verification evidence. A subtask without a status line is not started. Nothing in this file is started.

### Build & verification commands (cited in acceptance below)

All Gradle commands run from `IVI_ECU/` (`gradlew.bat` on the Windows dev host, `./gradlew` on CI/Linux).

| Target | Command |
|---|---|
| One module's tests | `./gradlew :contract:test` · `:serializer:test` · `:observer:test` · `:r4-simulator:test` |
| App unit tests | `./gradlew :app:testDebugUnitTest` |
| **Full suite (post-5.1)** | `./gradlew :contract:test :serializer:test :observer:test :r4-simulator:test :app:testDebugUnitTest` |
| APK | `./gradlew assembleDebug` → `app/build/outputs/apk/debug/app-debug.apk` |
| Lint | `./gradlew lint` |
| Contract integrity gate | from the repo root: `python contracts/check_sync.py` → exit 0 |
| Simulator image | `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -f IVI_ECU/r4-simulator/Dockerfile -t m1-r4-sim:latest IVI_ECU/` |

Until subtask `4.5.1.4` lands, the only valid test command is `./gradlew :app:testDebugUnitTest` — the other modules do not exist yet.

---

## Prior work — the unmerged Phase 5 branch is a salvage source, not an authority

Two unmerged branches carry an earlier Phase 5 implementation. Nothing on `main` carries either, so this plan's "nothing is started" is accurate for the trunk — but no subagent should retype from scratch what is already written and reviewable.

| Branch | What it is | Contains |
|---|---|---|
| **`feat/phase5-ivi-hmi-dev`** = **PR #2** (tip `f7f2f55`) | The open pull request | The R4 data layer only — deserializer, listener service, repository, `WarningViewModel` — plus the Python mock sender and a **2-node test blueprint** (`requirements/blueprint-2node-task51-test.{json,md}`, `plans/doc/task51-2node-blueprint-answer.md`) |
| **`feat/phase5-ivi-hmi-complete`** (tip `1e36cdc`, 2026-08-01) | A strict superset of PR #2 | Everything above **plus** `MainActivity`, `IviApplication`, `AppModule`, the wired `MainScreen`, wake-on-warning in `MainViewModel`, five more test files, and a deployment doc |

**Review the superset, not the PR**, whenever the two differ — `-complete` is where each file reached its latest state. Analysis of both: [phase5-pr2-review.md](doc/phase5-pr2-review.md).

**The rule for every code subtask below:** read the corresponding file on that branch first (`git show origin/feat/phase5-ivi-hmi-complete:IVI_ECU/<path>`), lift what meets the brief, and write the rest. The brief is the specification; the branch is prior art with no authority. **Do not merge or cherry-pick the branch wholesale** — it predates the mini-blueprint, the module split, and the port freeze, and it carries the defects below.

| Branch file | Useful to | Carry over? |
|---|---|---|
| `data/R4Deserializer.kt` | `4.5.2.2` | Structure only. It rewrites unknown `warningType` to `"unknown"` — **forbidden by D4**; and it decodes `ByteArray` whole, with no offset/length (D3) |
| `service/R4ListenerService.kt` | `4.5.3.2`, `4.5.3.3`, `4.5.5.2` | Notification/foreground shape yes. **The receive loop never calls `packet.setLength(...)` before `receive()`** — every datagram after the first short one is silently truncated (the exact bug `4.5.3.2` exists to prevent). Loop, socket and decode are also fused into the service, which D2/D5 separate |
| `data/R4Repository.kt`, `ui/WarningViewModel.kt` | `4.5.4.2`, `17.5.4.4` | Flow shapes yes. **`_latestScene = event.geometry` passes the scene through without `vehicleCSnapshot`, so the R19 provenance guard is inert** — the defect `17.5.4.4` fixes. No last-value-wins by `seq` |
| `ui/MainViewModel.kt` | `16.5.4.5` | Yes — wake-on-warning, `previousMode`, `userOverrodeDuringWarning` are all there and match the brief |
| `MainActivity.kt`, `IviApplication.kt`, manifest | `16.5.5.4`, `16.5.5.5` | Manifest shape yes (activity + service + LAUNCHER). Wiring is Hilt-based, which **D7 removes** — rewrite against `IviGraph`. It also adds `res/values/themes.xml`, an undesignated file |
| `ui/screen/MainScreen.kt` | `17.5.5.6` | Yes — seam mounting and `collectAsStateWithLifecycle` are sound, and it correctly does **not** mount the banner. The status bar is still hardcoded |
| `mock-sender/mock_r4_sender.py` | group 5.6 | **No.** Python, writes its own payloads (D9 forbids), targets `10.88.0.12:5004` — both wrong — and its `state` message shape (`vehicles.ego.position/speed`, key `B`) does not match the frozen R4 schema at all |
| test files | groups 5.2–5.5 | Case lists are a useful checklist. They test the branch's behaviour, including the two defects above, so no assertion transfers unread |
| `deployment/phase5-ivi-deploy.md`, `phase5_completion_report.md` | `16.5.8.4` | Content is useful (a real Skycraft device id, the logcat filters). The **location is not sanctioned** — a repo-root/node-root `deployment/` folder is not in [node-code-layout.md](../.claude/rules/node-code-layout.md); the material lands in `requirements/car-sky-guide/` |
| PR #2's `blueprint-2node-task51-test.json` + guide | `5.5.8.2` | **Do not import it.** Its Skycraft node has no `image` artifact block (deploy rejected outright), its bridge `config` is `null` (no `bridgeMode`/`subnet`, so its `10.88.0.x` addresses have no network), and it targets `registry.carsky.io` (502s). Its *ideas* are already adopted: the reduced topology, the display-config fields (`5.5.8.2`) and the approach/leave scenario shape (`4.5.6.4`) |

Two branch-wide constants are wrong against the frozen topology and must not survive into any subtask: the UDP port is **`47300`**, not `5004`, and the IVI address is **`10.99.0.13`**, not `10.88.0.12`.

---

## Task Group 5.1 — Gradle multi-project foundation & contract relocation (serves R4)

> Turns the single-module project into the five-module graph of HLD **D2**, under one version catalog (**D8**), and moves the committed contract layer into `:contract` (**D1**, **D6**) without touching a line of its source or its tests. This group gates every other code group.

### [ ] `4.5.1.1` — Version catalog `gradle/libs.versions.toml` + root plugin aliases *(agent)*

**Objective:** create the single source of dependency and plugin versions for all five modules (HLD D8).

**Scope:**

- New file `IVI_ECU/gradle/libs.versions.toml` declaring, at the versions already in use (read them from `IVI_ECU/build.gradle.kts` and `IVI_ECU/app/build.gradle.kts`): AGP `8.13.0`, Kotlin `2.2.20`, KSP `2.2.20-2.0.4`, kotlinx-serialization-json `1.9.0`, Compose BOM `2024.09.03`, androidx-core-ktx `1.13.1`, lifecycle-viewmodel-compose `2.8.6`, JUnit `4.13.2`; plus two versions this phase adds: `kotlinx-coroutines` (`1.9.0`, core + test) and `androidx-activity-compose` (`1.9.3`).
- `[plugins]` aliases for: `android-application`, `kotlin-android`, `kotlin-jvm` (`org.jetbrains.kotlin.jvm`), `kotlin-serialization`, `kotlin-compose`, `ksp`, and `application` is a built-in Gradle plugin (no alias needed, applied by id in `:r4-simulator`).
- `IVI_ECU/build.gradle.kts`: restate the existing `plugins { … apply false }` block through `alias(libs.plugins.…)`, and **add** `kotlin-jvm` (needed by `:contract`, `:serializer`, `:observer`, `:r4-simulator`). Leave the Hilt line untouched here — `4.5.1.2` removes it.
- Do not add or remove any module, and do not edit `app/build.gradle.kts` in this subtask.

**Acceptance:** `./gradlew :app:testDebugUnitTest` still green; `./gradlew projects` succeeds; `libs.versions.toml` contains every version listed above and no module declares a version literal that the catalog also declares.

**Dependencies:** none — the first subtask of the phase, and the gate for the whole `IVI_ECU/` tree. **Commit:** `[4.5.1.1] chore: add IVI Gradle version catalog and root plugin aliases`

### [ ] `4.5.1.2` — Move `:app` onto the catalog and drop Hilt *(agent)*

**Objective:** make `app/build.gradle.kts` resolve every plugin and dependency through the catalog, removing the unused Hilt stack (HLD D7).

**Scope:**

- Rewrite `app/build.gradle.kts`'s `plugins { }` and `dependencies { }` blocks to `alias(libs.plugins.…)` / `libs.…` references. Behaviour must not change apart from the removal below.
- **Remove** `id("com.google.dagger.hilt.android")` from `app/build.gradle.kts` and from `IVI_ECU/build.gradle.kts`, and remove `implementation("com.google.dagger:hilt-android:2.58")` and `ksp("com.google.dagger:hilt-android-compiler:2.58")`. Nothing references Hilt today — there is no `@HiltAndroidApp` class and no `@Inject` site (verify with a repo-wide grep for `dagger` and `Hilt` before committing). D7 replaces it with the hand-written `IviGraph` of `4.5.5.3`.
- Keep the KSP plugin only if something still uses it; if the grep shows Hilt was its only consumer, remove `id("com.google.devtools.ksp")` from `app/build.gradle.kts` too and say so in the commit body.
- Do not add the new `buildConfigField`s here — `4.5.4.1` owns those.

**Acceptance:** `./gradlew :app:testDebugUnitTest` and `./gradlew assembleDebug` both green; a repo-wide grep for `hilt`/`dagger` under `IVI_ECU/` returns nothing.

**Dependencies:** after `4.5.1.1`. **Commit:** `[4.5.1.2] refactor: put :app on the version catalog and remove the unused Hilt stack`

### [ ] `4.5.1.3` — Create the `:contract` module skeleton + `R4Contract.kt` *(agent)*

**Objective:** stand up the pure-JVM contract module (HLD D1) with nothing in it yet but its own constants file.

**Scope:**

- `IVI_ECU/settings.gradle.kts`: add `include(":contract")`.
- New `IVI_ECU/contract/build.gradle.kts`: `alias(libs.plugins.kotlin.jvm)` + `alias(libs.plugins.kotlin.serialization)`; `api(libs.kotlinx.serialization.json)` (**`api`, not `implementation`** — `:app` and `:r4-simulator` use `R4Json` and the `@Serializable` types directly); `testImplementation(libs.junit)`; JVM toolchain 17. **Zero Android dependencies** — this module must compile with no Android SDK present.
- New `IVI_ECU/contract/src/main/kotlin/com/hackathon/v2x/ivi/model/R4Contract.kt` holding, as an `object R4Contract`: `KNOWN_SCHEMA_VERSION = 1` (the frozen `contracts/samples/r4-warning.json` value), the four sample resource paths as constants (`/contracts/samples/r4-warning.json`, `r4-state.json`, `r4-unknown-warning.json`, `r3-tracked-object.json`), and the M1 warning-registry key list (`R4WarningEvent.WARNING_TYPE_NLOS_OBSTRUCTION` is the only entry — reference it once the models land in `4.5.1.4`; until then declare the string constant `"nlos_obstruction"` here and have `4.5.4.3` consume `R4Contract`).
- Do not move any file in this subtask.

**Acceptance:** `./gradlew :contract:build` green; `./gradlew :app:testDebugUnitTest` still green; `contract/build.gradle.kts` contains no `com.android.*` plugin and no `repositories { }` block.

**Dependencies:** after `4.5.1.1`. **Commit:** `[4.5.1.3] feat: add the pure-JVM :contract module with R4Contract constants`

### [ ] `4.5.1.4` — Relocate the models, tests and samples into `:contract`; repoint the sync manifest *(agent)*

**Objective:** move the committed contract layer verbatim into `:contract` and keep the contract-integrity gate green in the same commit (HLD D1, D6, §3.2 row 1).

**Scope — a relocation, not a rewrite. No source line may change.**

- `git mv` these three files from `IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/model/` to `IVI_ECU/contract/src/main/kotlin/com/hackathon/v2x/ivi/model/`, **byte-identical**, package `com.hackathon.v2x.ivi.model` unchanged: `R4Message.kt`, `R3Snapshot.kt`, `SceneGeometry.kt`.
- `git mv` both committed tests from `IVI_ECU/app/src/test/java/com/hackathon/v2x/ivi/model/` to `IVI_ECU/contract/src/test/kotlin/com/hackathon/v2x/ivi/model/`, **byte-identical**: `R4RoundTripTest.kt`, `R4AdditiveVersionTest.kt`. They must pass **unchanged** — do not touch their `getResourceAsStream("/contracts/samples/…")` calls.
- `git mv` the four sample JSONs from `IVI_ECU/app/src/test/resources/contracts/samples/` to `IVI_ECU/contract/src/main/resources/contracts/samples/` (**main**, not test — D6: the simulator and the dev injector read them off the same classpath): `r3-tracked-object.json`, `r4-warning.json`, `r4-state.json`, `r4-unknown-warning.json`. Because the resource root still contains `contracts/samples/`, the moved tests resolve unchanged.
- `app/build.gradle.kts`: add `implementation(project(":contract"))`.
- **Same commit, out-of-folder edit (§3.2 row 1):** in [contracts/sync-manifest.json](../contracts/sync-manifest.json), repoint the four `IVI_ECU/app/src/test/resources/contracts/samples/<f>.json` targets to `IVI_ECU/contract/src/main/resources/contracts/samples/<f>.json`. They sit under the `contracts/samples/r3-tracked-object.json`, `r4-warning.json`, `r4-state.json` and `r4-unknown-warning.json` source entries. **This edit cannot be deferred** — the moment the files move, `check_sync.py` reports four missing targets and `contracts-gate` goes red.
- `IVI_ECU/contracts/r3-tracked-object.schema.json` and `r4-ada-ivi.schema.json` are **not** touched — their manifest targets are unchanged.

**Acceptance:** `./gradlew :contract:test` green with `R4RoundTripTest` and `R4AdditiveVersionTest` passing (5 test methods total, no source change — verify with `git diff -M --stat` showing pure renames); `./gradlew :app:testDebugUnitTest` and `./gradlew assembleDebug` green; `python contracts/check_sync.py` exits 0.

**Dependencies:** after `4.5.1.3`. **Commit:** `[4.5.1.4] refactor: relocate R4/R3 models, tests and samples into :contract`

### [ ] `4.5.1.5` — ProGuard keep rules for the relocated serializable models *(agent)*

**Objective:** keep the release build's kotlinx-serialization reflection working after the relocation (HLD §3.1, `app/proguard-rules.pro` `[C] +`).

**Scope:** add to `IVI_ECU/app/proguard-rules.pro` the standard kotlinx-serialization keep set scoped to `com.hackathon.v2x.ivi.model.**` — keep the generated `$$serializer` fields/classes, the `Companion.serializer()` methods, and `@kotlinx.serialization.Serializable` annotated classes' `INSTANCE`/`Companion`. Nothing else; do not change `isMinifyEnabled`.

**Acceptance:** `./gradlew :app:assembleRelease` succeeds (unsigned output is fine); the rules name only the `com.hackathon.v2x.ivi.model` package.

**Dependencies:** after `4.5.1.4`. Parallel with groups 5.2–5.6. **Commit:** `[4.5.1.5] chore: add ProGuard keep rules for the relocated serializable models`

---

## Task Group 5.2 — `:serializer` — datagram bytes to a typed R4 result (serves R4; injection point I1)

> HLD **D3** (de-framing is buffer slicing, not header parsing) and **§5.1** (the decode entry point). Pure Kotlin/JVM: **never logs, never throws across the receive loop** — it returns a result and the observer decides what to log.

### [ ] `4.5.2.1` — Module `:serializer` + the decode contract types *(agent)*

**Objective:** declare the module and the types of HLD §5.1, with no implementation behind them.

**Scope:**

- `settings.gradle.kts`: `include(":serializer")`. New `IVI_ECU/serializer/build.gradle.kts`: `alias(libs.plugins.kotlin.jvm)`; `api(project(":contract"))`; `testImplementation(libs.junit)`; toolchain 17; no Android, no repositories block.
- New `IVI_ECU/serializer/src/main/kotlin/com/hackathon/v2x/ivi/serializer/R4Decoder.kt` containing **exactly** HLD §5.1's declarations:

  ```kotlin
  interface R4Decoder { fun decode(buffer: ByteArray, offset: Int, length: Int): R4DecodeResult }
  sealed interface R4DecodeResult {
      data class Decoded(val message: R4Message, val schemaVersionAhead: Boolean) : R4DecodeResult
      data class Failed(val reason: DecodeFailure, val detail: String, val preview: String) : R4DecodeResult
  }
  enum class DecodeFailure { EMPTY, UNKNOWN_MESSAGE_TYPE, MALFORMED }
  ```

**Acceptance:** `./gradlew :serializer:build` green; the file's three declarations match the block above field-for-field.

**Dependencies:** after `4.5.1.4`. **Commit:** `[4.5.2.1] feat: add the :serializer module and the R4 decode contract`

### [ ] `4.5.2.2` — `R4Deserializer` implementation + the decode-table test *(agent)*

**Objective:** implement `R4Decoder` so every row of the [parsing note §2](../IVI_ECU/doc/research_notes/phase5-r4-parsing.md) table maps to the right `R4DecodeResult`, and nothing escapes as an exception.

**Scope — two files:**

- `IVI_ECU/serializer/src/main/kotlin/com/hackathon/v2x/ivi/serializer/PayloadPreview.kt` — `fun preview(buffer: ByteArray, offset: Int, length: Int, maxChars: Int): String`: a **bounded, single-line** rendering of the bytes for the log; non-printable bytes escaped, newlines/tabs replaced, truncated with an ellipsis at `maxChars` (default a constant in this file, e.g. 48). Never returns the whole datagram.
- `IVI_ECU/serializer/src/main/kotlin/com/hackathon/v2x/ivi/serializer/R4Deserializer.kt` — `class R4Deserializer : R4Decoder`:
  1. Slice `buffer[offset until offset + length]` — **never** the whole backing array (D3 row 1).
  2. Decode as UTF-8, strip a leading UTF-8 BOM (`EF BB BF`) and surrounding whitespace (D3 row 4).
  3. Empty/blank after trimming → `Failed(EMPTY, …)`.
  4. `R4Json.decodeFromString(R4Message.serializer(), text)` inside a `try`/`catch (SerializationException)`.
  5. Success → `Decoded(message, schemaVersionAhead = message.schemaVersion > R4Contract.KNOWN_SCHEMA_VERSION)`.
  6. Failure whose message indicates an unresolved polymorphic discriminator → `Failed(UNKNOWN_MESSAGE_TYPE, …)`; every other `SerializationException` (malformed, truncated, wrong field type, missing required field) → `Failed(MALFORMED, …)`. Catch `Throwable` at the outer edge and map to `MALFORMED` rather than letting anything propagate.
  7. Convenience overloads `decode(bytes: ByteArray)` and `decode(text: String)` for I1 tests (HLD §5.1).
  - **`isLenient` stays `false`** and no runtime JSON-Schema validation is added (HLD §5.1). `R4Json` is consumed as-is from `:contract`; do not construct a second `Json`.
- `IVI_ECU/serializer/src/test/kotlin/com/hackathon/v2x/ivi/serializer/R4DeserializerTest.kt` — one test per decode-table row, reading fixtures off the `:contract` classpath (`/contracts/samples/…`): `r4-warning.json` → `Decoded`, warning, `schemaVersionAhead == false`; `r4-state.json` → `Decoded` state; `r4-unknown-warning.json` → `Decoded` **with `warningType == "slippery_road"` preserved verbatim** (D4) and `schemaVersionAhead == true`; `{"type":"telemetry",…}` → `UNKNOWN_MESSAGE_TYPE`; `not-json` → `MALFORMED`; a truncated prefix of `r4-warning.json` → `MALFORMED`; `"distance": "far"` → `MALFORMED`; a warning with `object` removed → `MALFORMED`; empty and all-whitespace input → `EMPTY`. Assert **no test throws** and every `Failed.preview` is non-empty and single-line.

**Acceptance:** `./gradlew :serializer:test` green with every row above covered; a grep of `serializer/src/main` shows no `println`, no logging import, and no `throw`.

**Dependencies:** after `4.5.2.1`. **Commit:** `[4.5.2.2] feat: implement R4Deserializer with the full decode-failure table`

### [ ] `4.5.2.3` — Buffer-slicing test *(agent)*

**Objective:** prove the D3 buffer discipline that the decode-table test cannot reach.

**Scope:** `IVI_ECU/serializer/src/test/kotlin/com/hackathon/v2x/ivi/serializer/BufferSlicingTest.kt`:

- A 2048-byte buffer pre-filled with garbage, the valid `r4-warning.json` bytes written at a **non-zero offset**, decoded with that `offset`/`length` → `Decoded`. This is the "dirty backing array" case: decoding `buffer` whole would fail.
- The same buffer reused for a second, **shorter** payload → still `Decoded`, proving trailing bytes of the previous message are not appended.
- A payload prefixed with a UTF-8 BOM → `Decoded`.
- A payload wrapped in leading/trailing whitespace and a trailing newline → `Decoded`.
- `length = 0` → `EMPTY`; `offset + length` beyond the array → `MALFORMED`, not an exception.

**Acceptance:** `./gradlew :serializer:test` green; all six cases present.

**Dependencies:** after `4.5.2.2`. **Commit:** `[4.5.2.3] test: cover offset/length slicing, dirty buffers and BOM handling`

---

## Task Group 5.3 — `:observer` — socket, receive loop, back-off, event flow (serves R4, R6; injection point I2)

> HLD **D5** (the loop is plain-JVM code; the service is only its lifecycle host) and **§5.2**. Plain Kotlin/JVM so I2 runs in CI with no device and **no Robolectric**. `:observer` never imports `android.util.Log` — it logs through the `R4Logger` seam.

### [ ] `4.5.3.1` — Module `:observer` + seams, config and event types *(agent)*

**Objective:** declare the module and its four value/seam files, with no loop yet.

**Scope:**

- `settings.gradle.kts`: `include(":observer")`. New `IVI_ECU/observer/build.gradle.kts`: `alias(libs.plugins.kotlin.jvm)`; `api(project(":serializer"))`; `api(libs.kotlinx.coroutines.core)`; `testImplementation(libs.junit)` + `testImplementation(libs.kotlinx.coroutines.test)`; toolchain 17; no Android.
- `observer/src/main/kotlin/com/hackathon/v2x/ivi/observer/R4DatagramSource.kt` — the seam: `fun bind()`, `fun receive(): Received`, `fun close()`, and `data class Received(val buffer: ByteArray, val offset: Int, val length: Int)`. Interface only; `4.5.3.2` implements it.
- `.../R4Event.kt` — **exactly** HLD §5.2: `sealed interface R4Event { data class Message(val message: R4Message, val receivedAtMs: Long, val bytes: Int); data class Dropped(val reason: DecodeFailure, val detail: String, val bytes: Int) }`, plus `sealed interface R4LinkState { Bound(port) | Rebinding | Error(detail) }` (the bottom status bar of `4.5.5.6` binds to this).
- `.../R4ObserverConfig.kt` — `data class R4ObserverConfig(val port: Int, val bufferBytes: Int, val flowBufferEvents: Int, val retryInitialMs: Long, val retryMaxMs: Long)`. **No default values and no literals** — every field is supplied by `IviRuntimeConfig` (`4.5.4.1`).
- `.../R4Logger.kt` — `fun interface R4Logger { fun log(level: R4LogLevel, line: String) }` plus `object NoopR4Logger : R4Logger` and an `enum class R4LogLevel { INFO, WARN, ERROR }`. `:app` supplies the real implementation in `4.5.5.1`.

**Acceptance:** `./gradlew :observer:build` green; a grep of `observer/src/main` shows zero `android.` / `androidx.` imports.

**Dependencies:** after `4.5.2.1`. **Commit:** `[4.5.3.1] feat: add the :observer module with its datagram seam, events and config`

### [ ] `4.5.3.2` — `JdkDatagramSource` — the real socket, with the setLength rule *(agent)*

**Objective:** implement `R4DatagramSource` over `java.net.DatagramSocket`, owning the one rule that silently truncates every datagram if forgotten.

**Scope:** `observer/src/main/kotlin/com/hackathon/v2x/ivi/observer/JdkDatagramSource.kt`:

- Constructor takes `port` and `bufferBytes` (from `R4ObserverConfig` — no literals).
- `bind()` binds **`0.0.0.0:<port>`**, never the node address — the bridge assigns it (HLD D5, §8). Allocates one reusable `ByteArray(bufferBytes)` and one reusable `DatagramPacket`.
- `receive()` calls **`packet.setLength(buffer.size)` before every `socket.receive(packet)`** (D3 row 2 — without it every datagram after the first is truncated to the shortest one seen), then returns `Received(packet.data, packet.offset, packet.length)`.
- `close()` is idempotent and unblocks a pending `receive()`.
- Bind failure propagates as an exception for `R4SocketObserver` to turn into `Rebinding` — it is **not** swallowed here.
- Unit test `observer/src/test/kotlin/.../JdkDatagramSourceTest.kt`: bind on port `0` (ephemeral), send two datagrams from a local `DatagramSocket` — a long one then a **shorter** one — and assert the second `Received.length` equals the second payload's length (the regression this class exists to prevent).

**Acceptance:** `./gradlew :observer:test` green; the `setLength` call is textually inside the `receive()` body, before the `socket.receive(...)` call.

**Dependencies:** after `4.5.3.1`. **Commit:** `[4.5.3.2] feat: add JdkDatagramSource with per-receive packet length reset`

### [ ] `4.5.3.3` — `R4SocketObserver` — receive loop, truncation check, event flow *(agent)*

**Objective:** implement the loop of HLD §5.2 so N datagrams in produce N events out and one bad message never stops the next good one.

**Scope:** `observer/src/main/kotlin/com/hackathon/v2x/ivi/observer/R4SocketObserver.kt` with the §5.2 signature verbatim:

```kotlin
class R4SocketObserver(
    private val config: R4ObserverConfig,
    private val decoder: R4Decoder,
    private val sourceFactory: () -> R4DatagramSource,
    private val logger: R4Logger,
) { val events: SharedFlow<R4Event>; val linkState: StateFlow<R4LinkState>; fun start(scope: CoroutineScope): Job; fun stop() }
```

- `events` is a `MutableSharedFlow` with `extraBufferCapacity = config.flowBufferEvents` and `BufferOverflow.DROP_OLDEST`; the loop uses **`tryEmit`, never a suspending emit** (D5 back-pressure) — a slow collector must never stall the socket.
- `start(scope)` launches on `Dispatchers.IO`; on bind success `linkState = Bound(port)` and log `[LINK] state=bound port=<p>` (§5.4).
- Per datagram: if `length == config.bufferBytes`, log a truncation-suspect WARN and **still attempt the decode** (D3 row 3). Call `decoder.decode(buffer, offset, length)`; `Decoded` → log the `[RX]` line of §5.4 and `tryEmit(R4Event.Message(msg, System.currentTimeMillis(), length))`; `Failed` → log `[DROP] reason=… bytes=… preview="…"` and `tryEmit(R4Event.Dropped(...))`. Log `schemaVersionAhead` **once** per observer lifetime, not per message.
- The `[RX]` line for a warning carries `warningType=`, `risk=`, `cSource=` (the R3 snapshot's `source`) and `cPos=` — `cSource` on every rendered warning is what backs the R19 claim in text (§5.4).
- **No accumulate-and-split logic** anywhere (D3 row 5) — UDP preserves message boundaries.
- `stop()` closes the source and cancels the job; `linkState` is left at its last value.
- Test `observer/src/test/kotlin/.../R4SocketObserverTest.kt` with a **fake** `R4DatagramSource` (no socket): 5 valid datagrams in → 5 `R4Event.Message` out in order; one malformed among them → 1 `Dropped` and the following good message still arrives; a fake whose `receive()` throws once → the loop does not die (leaves the back-off to `4.5.3.4`); assert nothing was logged through a real Android type by injecting a recording `R4Logger`.

**Acceptance:** `./gradlew :observer:test` green; `tryEmit` is the only emit call in the loop; the `[RX]`/`[DROP]`/`[LINK]` shapes match HLD §5.4 exactly.

**Dependencies:** after `4.5.3.2`. **Commit:** `[4.5.3.3] feat: implement the R4 receive loop with typed events and truncation checks`

### [ ] `4.5.3.4` — Rebind back-off + its test *(agent)*

**Objective:** make a socket error a recoverable, bounded-back-off rebind instead of a dead listener.

**Scope:**

- Extend `R4SocketObserver` (no new production file): on a bind or receive error → log at ERROR, `linkState = Rebinding`, close the source, `delay(d)`, recreate through `sourceFactory()`, retry; `d` starts at `config.retryInitialMs`, doubles to a ceiling of `config.retryMaxMs`, and **resets to `retryInitialMs` on the next successful bind**. `linkState` returns to `Bound`.
- **Make `R4LinkState.Error` reachable.** `4.5.3.1` declares it, and a declared state that nothing ever emits is a defect, not a spare: once the back-off has saturated at `retryMaxMs` (the link has been down long enough that a transient blip is ruled out), set `linkState = Error(detail)` and **keep retrying** — the observer never gives up, because a listener that stops after N attempts is worse for a recorded demo than one that keeps trying. A successful bind returns it to `Bound`. The status bar of `17.5.5.6` renders this state.
- `observer/src/test/kotlin/.../RetryBackoffTest.kt` using `kotlinx-coroutines-test` virtual time: a source factory that fails the first three binds then succeeds → assert the observed delays are `initial, 2×initial, 4×initial` clamped at `retryMaxMs`, that `linkState` passes `Rebinding → Bound`, and that after a later failure the delay restarts at `retryInitialMs` (reset-on-success). One further case: a factory that keeps failing past the ceiling → `linkState` reaches `Error`, retries continue, and a later success returns it to `Bound`.

**Acceptance:** `./gradlew :observer:test` green; no `Thread.sleep` anywhere — the test runs on virtual time.

**Dependencies:** after `4.5.3.3`. **Commit:** `[4.5.3.4] feat: add bounded exponential rebind back-off to the R4 observer`

### [ ] `4.5.3.5` — Loopback socket test (I2) *(agent)*

**Objective:** prove the observer end to end over a **real** `DatagramSocket`, with no device and no Robolectric (HLD §7, I2).

**Scope:** `observer/src/test/kotlin/com/hackathon/v2x/ivi/observer/LoopbackSocketTest.kt`:

- Bind a real `JdkDatagramSource` on an ephemeral port on `127.0.0.1`, wire it into a real `R4SocketObserver` with a real `R4Deserializer`, collect `events`.
- Send the frozen `r4-warning.json` bytes from a plain `DatagramSocket` → one `R4Event.Message` whose `message` is an `R4WarningEvent` with `objectSnapshot.source == "v2x_relayed"`.
- Send `r4-unknown-warning.json` → `Message` with `warningType == "slippery_road"` (D4 end to end).
- Send `not-json` bytes → one `Dropped(MALFORMED, …)`, and a following valid datagram still produces a `Message` — the loop survived.
- Bounded waits (a few seconds) with a clear timeout failure message; the test must not hang CI.

**Acceptance:** `./gradlew :observer:test` green on a machine with no Android SDK; no Robolectric dependency is added anywhere.

**Dependencies:** after `4.5.3.4`. **Commit:** `[4.5.3.5] test: add the I2 loopback socket test for the R4 observer`

---

## Task Group 5.4 — `:app` data & logic layer — the missing middle (serves R4, R16, R17)

> HLD **§3.1** `app/src/main/java/com/hackathon/v2x/ivi/{config,data,warning,ui}` and **§4** MVC. Everything in this group is plain Kotlin testable by `:app:testDebugUnitTest` with no device. Group 5.5 then hosts it in an Activity and a service.

### [ ] `4.5.4.1` — `IviRuntimeConfig` + the new `BuildConfig` fields (D10) *(agent)*

**Objective:** make every Phase 5 tunable a compile-time default that a launch-time intent extra can override, in one place.

**Scope:**

- `app/build.gradle.kts` `defaultConfig`: add the `buildConfigField`s of HLD D10 beside the committed `WARNING_TIMEOUT_MS`: `R4_UDP_PORT = 47300` (blueprint-frozen — **not** 5004), `R4_SOCKET_BUFFER_BYTES = 2048`, `R4_FLOW_BUFFER_EVENTS = 8`, `R4_RETRY_INITIAL_MS = 500L`, `R4_RETRY_MAX_MS = 5000L`, `SCENE_SCALE_M_PER_PX = 0.5f`.
- New `app/src/main/java/com/hackathon/v2x/ivi/config/IviRuntimeConfig.kt`: `data class IviRuntimeConfig(port, socketBufferBytes, flowBufferEvents, retryInitialMs, retryMaxMs, warningTimeoutMs, sceneScaleMetersPerPixel)` plus `fun resolve(intent: Intent?): IviRuntimeConfig` — reads the `BuildConfig` defaults and applies the D10 overrides when present: `--ei r4_port`, `--el warning_timeout_ms`, `--ef scene_scale`. Invalid or out-of-range extras (port outside 1–65535, non-positive timeout/scale) are ignored in favour of the default. Add `fun toObserverConfig(): R4ObserverConfig`. **This is the only class that reads `BuildConfig`** — every other class receives resolved values.
- Test `app/src/test/java/com/hackathon/v2x/ivi/config/IviRuntimeConfigTest.kt`: a null intent yields the defaults; each override key is applied; an out-of-range value falls back; `toObserverConfig()` carries the resolved port and buffer sizes. (Use a plain fake for the extras lookup if `android.content.Intent` is unavailable in a unit test — extract the extras read into an internal `resolve(overrides: Map<String, Any>)` that the intent overload delegates to, and test that.)

**Acceptance:** `./gradlew :app:testDebugUnitTest` green; a grep of `app/src/main` shows `BuildConfig.` referenced only inside `IviRuntimeConfig.kt`; port default is `47300`.

**Dependencies:** after `4.5.3.1` (needs `R4ObserverConfig`). **Commit:** `[4.5.4.1] feat: add IviRuntimeConfig with BuildConfig defaults and launch overrides`

### [ ] `4.5.4.2` — `R4Repository` — the single routing point *(agent)*

**Objective:** collect the observer's events once, on the application scope, and expose them as the app's data layer (HLD §4 Data, §5.3).

**Scope:** `app/src/main/java/com/hackathon/v2x/ivi/data/R4Repository.kt`:

- Constructor takes the `R4SocketObserver` (or, better for testing, its `events` + `linkState` flows) and a `CoroutineScope`.
- Exposes `warnings: SharedFlow<R4WarningEvent>` (replay 1, so a late collector sees the current warning), `lastState: StateFlow<R4StateMessage?>` with **last-value-wins by `seq`** (a message with a `seq` lower than or equal to the stored one is discarded), `linkState: StateFlow<R4LinkState>` (passthrough), and `droppedCount: StateFlow<Int>`.
- Exposes `fun inject(event: R4Event)` — the single injection target the dev injector (I3, `4.5.6.7`) uses, so I3 exercises exactly the same downstream path as a real datagram (HLD §5.3).
- **The repository stores and routes; it never decides what a warning means and never formats anything** (HLD §4).
- Test `app/src/test/java/com/hackathon/v2x/ivi/data/R4RepositoryTest.kt`: a `Message` carrying a warning appears on `warnings` and does not touch `lastState`; a `Message` carrying a state updates `lastState`; a state with `seq = 41` after `seq = 42` is discarded (LVW); a `Dropped` increments `droppedCount` and emits no warning; `inject()` produces the identical observable result as an event arriving from the flow.

**Acceptance:** `./gradlew :app:testDebugUnitTest` green; `R4Repository.kt` contains no `String.format`, no `warningType` comparison and no UI type.

**Dependencies:** after `4.5.3.1`. Parallel with `4.5.4.1`. **Commit:** `[4.5.4.2] feat: add R4Repository routing warnings, state and link status`

### [ ] `4.5.4.3` — `WarningClassifier` — presentation at the UI edge (D4) *(agent)*

**Objective:** map *known* `warningType` values to their presentation and everything else to a generic warning presentation, without ever rewriting the wire value.

**Scope:** `app/src/main/java/com/hackathon/v2x/ivi/warning/WarningClassifier.kt`:

- Declare `data class WarningPresentation(val title: String, val known: Boolean, val urgency: Urgency)` (or equivalent) in this same file — HLD §3.1 designates no separate file for it.
- `fun classify(warningType: String): WarningPresentation`: `R4Contract`'s `nlos_obstruction` → the M1 NLOS presentation with `known = true`; **any other value → a generic presentation with `known = false`, and the wire value is carried through unchanged for the log** (D4 — the parser preserved it; this is where classification happens, and nothing here writes `"unknown"` back into the message).
- `fun normaliseRisk(riskState: String): Urgency`: `low`/`medium`/`high` case-insensitively; **an unknown `riskState` maps to the highest urgency** — this must not contradict the committed `CanvasWarningView.riskColor`, which already treats unknown risk as highest (fail-safe, HLD §9.2).
- Test `app/src/test/java/com/hackathon/v2x/ivi/warning/WarningClassifierTest.kt`: `nlos_obstruction` → `known = true`; `slippery_road` (the frozen additive fixture's value) → `known = false` and the value is still readable; `"HIGH"`, `"high"`, `"" `, `"catastrophic"` all resolve, with the last two at highest urgency.

**Acceptance:** `./gradlew :app:testDebugUnitTest` green; no branch in this file mutates an `R4WarningEvent`.

**Dependencies:** after `4.5.1.4`. Parallel with `4.5.4.1`/`4.5.4.2`. **Commit:** `[4.5.4.3] feat: add WarningClassifier mapping unknown warning types to a generic presentation`

### [ ] `17.5.4.4` — `WarningViewModel` + `WarningUiState`, **including the R19 snapshot wiring** *(agent)*

**Objective:** turn warnings into `Idle ↔ Active` UI state with an auto-dismiss timeout, and **compose the scene so the renderer's provenance guard is actually armed** (HLD §9.2 — the single most easily-missed wiring detail in the phase).

**Scope — two files:**

- `app/src/main/java/com/hackathon/v2x/ivi/ui/WarningUiState.kt`: `sealed interface WarningUiState { data object Idle; data class Active(val scene: SceneGeometry, val riskState: String, val presentation: WarningPresentation) }`.
- `app/src/main/java/com/hackathon/v2x/ivi/ui/WarningViewModel.kt`:
  - Collects `repository.warnings`; on each warning → `Active(...)`, and (re-)arms a `warningTimeoutMs` timer; on expiry → `Idle`. A new warning resets the timer rather than stacking timers.
  - **The composition step (the defect fix).** `SceneGeometry` arriving in `warning.geometry` has `vehicleCSnapshot = null`, and `CanvasWarningView` treats a `null` snapshot as **trusted** — so passing `warning.geometry` straight through silently disables the R19 source guard. The view-model must build the scene as `warning.geometry.copy(vehicleCSnapshot = warning.objectSnapshot)` (an internal function of this file; HLD §3.1 designates no separate composer file). `riskState` and `presentation` come from `WarningClassifier`.
  - Holds no drawing code and no socket (HLD §4 UI logic).
- Test `app/src/test/java/com/hackathon/v2x/ivi/ui/WarningViewModelTest.kt`:
  - Idle initially; a warning → `Active`; no further warning for `warningTimeoutMs` → `Idle`; a second warning inside the window extends rather than double-fires.
  - **Guard-armed test (the R19 regression test, name it explicitly):** decode the frozen `/contracts/samples/r4-warning.json`, feed it in, and assert `(state as Active).scene.vehicleCSnapshot?.source == "v2x_relayed"` — i.e. the snapshot is **not null**. Then feed a warning whose `object.source` is `own_sensor` and assert the composed scene carries that snapshot verbatim, so the renderer's guard can trip. A `null` `vehicleCSnapshot` in either case fails the test.
  - Use virtual time (`kotlinx-coroutines-test`) for the timeout; add `testImplementation(libs.kotlinx.coroutines.test)` to `app/build.gradle.kts` if absent.

**Acceptance:** `./gradlew :app:testDebugUnitTest` green including the named guard-armed test; the timeout value is read from the injected config, never a literal.

**Dependencies:** after `4.5.4.2` and `4.5.4.3`. **Commit:** `[17.5.4.4] feat: add WarningViewModel with timeout and R19 snapshot composition`

### [ ] `16.5.4.5` — Extend `MainViewModel` — wake-on-warning, restore, user override *(agent)*

**Objective:** make a warning force the Display Area to the Warning View and restore the previous view when it clears, without trapping the user (HLD §3.1 `ui/MainViewModel.kt` `[C] +`).

**Scope:** edit the committed `app/src/main/java/com/hackathon/v2x/ivi/ui/MainViewModel.kt`:

- Add `previousMode` capture: on entering `WarningView` because of a warning, remember the mode that was showing.
- Add `fun onWarningState(state: WarningUiState)` (or an injected flow collect): `Active` → force `WarningView`; `Idle` → restore `previousMode`, **unless** the user deliberately navigated away during the warning, in which case the user's chosen mode stands.
- Add the user-override flag: the committed `setMode` currently **ignores** every navigation request while `WarningView` is active. Relax it to record a deliberate user navigation as an override and honour it, so `Idle` does not yank the user back. Keep the safety intent — the warning still *comes up* unconditionally.
- Test `app/src/test/java/com/hackathon/v2x/ivi/ui/MainViewModelTest.kt`: from `HomeView`, a warning forces `WarningView`; on `Idle` the mode returns to `HomeView`; if the user selects `SettingsView` during the warning, `Idle` leaves `SettingsView` in place; a second warning still forces `WarningView` again and clears the override.

**Acceptance:** `./gradlew :app:testDebugUnitTest` green with all four cases; `DisplayMode.kt` is unchanged (HLD §3.1 marks it `[C] unchanged`).

**Dependencies:** after `17.5.4.4`. **Commit:** `[16.5.4.5] feat: add wake-on-warning, previous-mode restore and user override to MainViewModel`

---

## Task Group 5.5 — `:app` shell & UI wiring — the launchable APK (serves R16, R17, R18)

> **The APK has no launcher entry today** — no `<activity>`, no `<service>`, no application class (HLD §9.2, implementation notes §1). This group is what makes something render on the node. Nothing here may mount `WarningBannerOverlay` (**D11**, standing user decision 2026-07-26).

### [ ] `18.5.5.1` — `AndroidR4Logger` — the `IVI_V2X` evidence bridge *(agent)*

**Objective:** implement the `R4Logger` seam over `android.util.Log` on the single `IVI_V2X` tag, so the demo's evidence is one `adb logcat -s IVI_V2X`.

**Scope:** `app/src/main/java/com/hackathon/v2x/ivi/service/AndroidR4Logger.kt` — maps `R4LogLevel.{INFO,WARN,ERROR}` to `Log.i/w/e` on tag `IVI_V2X` (the tag the committed `CanvasWarningView` already uses). It emits the caller's line verbatim; the `[LINK]`/`[RX]`/`[DROP]` shapes of HLD §5.4 are composed in `:observer` (`4.5.3.3`), not here. Add a `fun ui(line: String)` convenience for the `[UI] mode=… cause=…` lines of §5.4, used by `4.5.5.6`.

**Acceptance:** `./gradlew :app:testDebugUnitTest` and `assembleDebug` green; this is the **only** file bridging `:observer` to `android.util.Log` — a grep of `observer/` and `serializer/` for `android.util` returns nothing.

**Dependencies:** after `4.5.3.1`. Parallel with group 5.4. **Commit:** `[18.5.5.1] feat: add AndroidR4Logger bridging the observer seam to the IVI_V2X tag`

### [ ] `4.5.5.2` — `R4ListenerService` — the foreground lifecycle host (D5) *(agent)*

**Objective:** host the observer in a foreground service so reception survives the Display Area switching away and the process stays at foreground priority for the whole recorded run.

**Scope:**

- `app/src/main/java/com/hackathon/v2x/ivi/service/R4ListenerService.kt`: creates a notification channel and calls `startForeground()` **immediately** on start (API 26+/29+ requirement); on start calls `observer.start(applicationScope)`, on destroy calls `observer.stop()`. **The service is a lifecycle host, not the loop** (D5) — no socket code, no decode code lives here.
- `POST_NOTIFICATIONS` is a runtime permission from API 33: **a denied permission suppresses the notification only and must never be treated as a failure to start** (D5). Guard the notification post, not the service start.
- `app/src/main/AndroidManifest.xml`: add `<uses-permission android:name="android.permission.POST_NOTIFICATIONS" />` and the `<service android:name=".service.R4ListenerService" android:exported="false" android:foregroundServiceType="connectedDevice" />` declaration. `FOREGROUND_SERVICE` and `INTERNET` are already declared. (At `targetSdk 34` this would additionally need `FOREGROUND_SERVICE_CONNECTED_DEVICE`; the app targets 33 — note it in a manifest comment, do not bump the target.)
- No unit test is required for the service itself (that is what D5's plain-JVM split avoids); the loop is already covered by `4.5.3.3`–`4.5.3.5`.

**Acceptance:** `./gradlew assembleDebug` green and `./gradlew :app:testDebugUnitTest` still green; the manifest declares the service; the class body contains no `DatagramSocket` and no `decode` call.

**Dependencies:** after `18.5.5.1`. **Commit:** `[4.5.5.2] feat: add R4ListenerService as the foreground host for the R4 observer`

### [ ] `4.5.5.3` — `IviGraph` — the manual composition root (D7) *(agent)*

**Objective:** wire the seven objects of the app by hand, in one place, with no annotation processor.

**Scope:** `app/src/main/java/com/hackathon/v2x/ivi/di/IviGraph.kt`:

- Constructs, in HLD §5.3's order: `IviRuntimeConfig` → `R4ObserverConfig` → `R4Deserializer` → `R4SocketObserver` (with `sourceFactory = { JdkDatagramSource(port, bufferBytes) }` and `logger = AndroidR4Logger`) → `R4Repository` (collecting on the **application** scope, so a service restart cannot lose the last warning) → a single `ViewModelProvider.Factory` producing `MainViewModel` and `WarningViewModel` → the `CanvasWarningView` instance with its `scaleMetersPerPixel` from config.
- One object, created once, owned by `IviApplication`. Exposes the factory and the renderer; exposes nothing else to the UI.
- `fun updateConfig(resolved: IviRuntimeConfig)` so `MainActivity`'s launch-time override reaches the graph before the service starts.

**Acceptance:** `./gradlew assembleDebug` green; the file is under ~80 lines and contains no annotation, no reflection and no `object` singleton state beyond what `IviApplication` holds.

**Dependencies:** after `4.5.5.2`, `4.5.4.1`, `4.5.4.2`, `17.5.4.4`. **Commit:** `[4.5.5.3] feat: add IviGraph manual composition root and view-model factory`

### [ ] `16.5.5.4` — `IviApplication` + manifest application entry *(agent)*

**Objective:** give the APK an application class that owns the graph and the application coroutine scope.

**Scope:**

- `app/src/main/java/com/hackathon/v2x/ivi/IviApplication.kt`: `class IviApplication : Application()` creating `IviGraph` in `onCreate` and holding an application `CoroutineScope` (`SupervisorJob() + Dispatchers.Default`). Expose the graph through a property the Activity reads.
- `app/src/main/AndroidManifest.xml`: add `android:name=".IviApplication"` to the existing `<application>` element. Leave `label`, `networkSecurityConfig` and `supportsRtl` as they are — `network_security_config.xml` governs HTTP stacks only and has no bearing on the raw `DatagramSocket` (D3); it stays for correctness of any future HTTP use.

**Acceptance:** `./gradlew assembleDebug` green; the manifest's `<application>` carries `android:name`.

**Dependencies:** after `4.5.5.3`. **Commit:** `[16.5.5.4] feat: add IviApplication owning the graph and the application scope`

### [ ] `16.5.5.5` — `MainActivity` + the LAUNCHER entry — **the APK becomes startable** *(agent)*

**Objective:** add the launcher Activity that hosts Compose, resolves the launch-time config override, and starts the listener service. This is the subtask that closes "nothing renders on the node today".

**Scope:**

- `app/build.gradle.kts`: add `implementation(libs.androidx.activity.compose)` (catalog alias from `4.5.1.1`). **This dependency is absent today** and `ComponentActivity.setContent` will not resolve without it.
- `app/src/main/java/com/hackathon/v2x/ivi/MainActivity.kt`: `class MainActivity : ComponentActivity()`; in `onCreate` call `IviRuntimeConfig.resolve(intent)`, hand it to the graph (`updateConfig`), `startForegroundService(Intent(this, R4ListenerService::class.java))`, request `POST_NOTIFICATIONS` on API 33+ **without blocking startup on the answer**, then `setContent { MainScreen(viewModel = viewModel(factory = graph.viewModelFactory)) }`.
- `app/src/main/AndroidManifest.xml`: add

  ```xml
  <activity android:name=".MainActivity" android:exported="true"
            android:theme="@android:style/Theme.DeviceDefault.NoActionBar.Fullscreen">
      <intent-filter>
          <action android:name="android.intent.action.MAIN" />
          <category android:name="android.intent.category.LAUNCHER" />
      </intent-filter>
  </activity>
  ```

  **Use a platform theme, not a new `res/values/themes.xml`** — HLD §3.1 designates no new resource files, and the project has no `res/values/` today. If the platform theme proves unusable on the guest, that is a finding to report, not a licence to invent an unlisted file.
- Verify the D10 override reaches `IviRuntimeConfig.resolve`. The launch command and its `--ei r4_port` form are [deploy-ivi-hmi-walkthrough.md §4.7](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#47-open-the-screen-and-launch-the-app) — run it from there; this subtask only proves the extra arrives.

**Acceptance:** `./gradlew assembleDebug` green and the built `app-debug.apk` passes the launchable check of [deploy-ivi-hmi-walkthrough.md §2.6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#26-check-the-apk-is-launchable) with exactly one LAUNCHER activity — the check CI also reports as a notice ([§3.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#31-the-workflow-its-job-and-its-triggers)); `./gradlew :app:testDebugUnitTest` still green.

**Dependencies:** after `16.5.5.4`. **Commit:** `[16.5.5.5] feat: add MainActivity as the launcher entry hosting the R16 screen`

### [ ] `17.5.5.6` — Wire `MainScreen`: mount the view seam and bind the status bar *(agent)*

**Objective:** replace the Warning View placeholder with the real renderer and make the bottom bar tell the truth about the link.

**Scope:** edit the committed `app/src/main/java/com/hackathon/v2x/ivi/ui/screen/MainScreen.kt` (HLD §3.1 `[C] +`):

- `MainScreen` collects `WarningViewModel.uiState` alongside `MainViewModel.currentMode`, and feeds the warning state into `MainViewModel.onWarningState`.
- `DisplayModeSwitcher`'s `DisplayMode.WarningView` branch renders `warningView.Render(scene, riskState)` through `IviWarningViewSeam` when the state is `Active`, and keeps a neutral idle content when it is `Idle`. Delete `WarningViewPlaceholder`.
- Bottom status bar: replace the hardcoded `"V2X LINK: STANDBY"` with the live `R4LinkState` — `BOUND :47300` / `REBINDING` / `ERROR` — and the dot colour with it. The mode indicator stays as it is.
- Emit the `[UI] mode=… cause=…` lines of HLD §5.4 through `AndroidR4Logger.ui(...)` on each mode change (`cause=warning` / `cause=timeout` / `cause=user`).
- **`WarningBannerOverlay` is NOT mounted** (D11). Do not add it. Do not touch `IviWarningViewSeam.kt` or `SceneCoordinateMapper.kt` (`[C] unchanged`).
- Keep the two committed `@Preview` functions compiling; add a preview that renders `Active` with the frozen sample geometry if it costs nothing.

**Acceptance:** `./gradlew assembleDebug` and `:app:testDebugUnitTest` green; a grep of `MainScreen.kt` shows no `WarningBannerOverlay` and no literal `"V2X LINK: STANDBY"`.

**Dependencies:** after `16.5.5.5` and `16.5.4.5`. **Commit:** `[17.5.5.6] feat: mount the warning view seam and bind the link status bar`

### [ ] `17.5.5.7` — `CanvasWarningView` configurable scale (D10) *(agent)*

**Objective:** let the God View's zoom come from configuration instead of the library default, without breaking the committed previews.

**Scope:** edit `app/src/main/java/com/hackathon/v2x/ivi/ui/view/CanvasWarningView.kt`: add a **defaulted** constructor parameter `scaleMetersPerPixel: Float = SceneCoordinateMapper.DEFAULT_SCALE_METERS_PER_PIXEL` and pass it into the `SceneCoordinateMapper.mapScene(...)` call. `IviGraph` fills it from `IviRuntimeConfig.sceneScaleMetersPerPixel`. `SceneCoordinateMapper.DEFAULT_SCALE_METERS_PER_PIXEL` **stays** as the library default (D10) — do not delete it. The three committed `@Preview` functions must compile untouched.

**Acceptance:** `./gradlew assembleDebug` green; `git diff` on the previews is empty.

**Dependencies:** after `4.5.5.3`. Parallel with `17.5.5.6`. **Commit:** `[17.5.5.7] feat: make the God View scale configurable through the runtime config`

### [ ] `17.5.5.8` — `SceneCoordinateMapperTest` — cover the committed pure-math layer *(agent)*

**Objective:** test the layer that was built to be testable and never was (implementation notes §1, HLD §3.1).

**Scope:** `app/src/test/java/com/hackathon/v2x/ivi/ui/view/SceneCoordinateMapperTest.kt` — no Android types, no Compose:

- `egoAnchor` is `(w/2, h·0.75)`; ego at `(0,0)` maps exactly onto it.
- Forward (`x` positive) maps **up** — smaller canvas `y`; right (`y` positive) maps right.
- A vehicle far outside the canvas is clamped to `EDGE_MARGIN_PX` on both axes, never off-canvas.
- Scale sensitivity: halving `scaleMetersPerPixel` doubles the pixel displacement for the same metres.
- `mapScene` with `scene.vehicleC == null` yields `SceneRenderData.vehicleC == null` and does not throw — the "C not yet tracked" path.
- Radii pass through unmodified.

**Acceptance:** `./gradlew :app:testDebugUnitTest` green with all six cases; the test file imports nothing from `android.*`/`androidx.*`.

**Dependencies:** after `4.5.1.4`. **Fully parallel** with everything else in groups 5.2–5.6. **Commit:** `[17.5.5.8] test: cover SceneCoordinateMapper's ego anchor, clamping and null-C path`

### [ ] `17.5.5.9` — Unit-test the Ghost C provenance guard itself *(agent)*

**Objective:** put the R19 guard under test at the renderer, not only at the view-model that arms it (`17.5.4.4`) and the in-Room observation that exercises it (`4.5.9.4`).

**Scope:** `app/src/test/java/com/hackathon/v2x/ivi/ui/view/CanvasWarningViewTest.kt`.

The guard decision currently lives inline in `CanvasWarningView.Render`, and a `Canvas`-drawn marker is not in the Compose semantics tree, so it cannot be asserted from a composition test. Extract the decision and its ERROR line into two `internal` top-level functions in the committed `CanvasWarningView.kt` — `isGhostCSourceTrusted(snapshot: R3Snapshot?): Boolean` and `ghostCSourceGuardErrorMessage(snapshot: R3Snapshot): String` — and have `Render` call them. **Extraction only: no behaviour may change**, and the three committed `@Preview` functions must compile untouched.

Then assert: `null` snapshot → trusted (the dev/mock-scene path); `source = "v2x_relayed"` → trusted; `source = "own_sensor"` → **not** trusted; the error message names both the offending source and `v2x_relayed` and carries the snapshot JSON; and `riskColor` maps an unknown `riskState` to the high-urgency colour (fail-safe, HLD §9.2) so this test and `WarningClassifier.normaliseRisk` (`4.5.4.3`) cannot drift apart.

**Acceptance:** `./gradlew :app:testDebugUnitTest` green with all five cases; `git diff` on the three `@Preview` functions is empty.

**Dependencies:** after `4.5.1.4`. Parallel with `17.5.5.8`. **Commit:** `[17.5.5.9] test: cover the Ghost C provenance guard and fail-safe risk colour`

---

## Task Group 5.6 — Test equipment: `:r4-simulator` and the dev injector (serves R4; injection points I3, I4)

> HLD **D9** (build from the frozen samples, validate through `R4Json` before sending) and **§7**. This is sanctioned IVI test equipment inside `IVI_ECU/`, not a mock to eliminate — it cannot reach into `ADA_ECU/` (no cross-node source imports), so it reaches the same models the app parses with by depending on `:contract`.

### [ ] `4.5.6.1` — Module `:r4-simulator` + `SimConfig` *(agent)*

**Objective:** stand up the CLI module and its two configuration sources.

**Scope:**

- `settings.gradle.kts`: `include(":r4-simulator")`. New `IVI_ECU/r4-simulator/build.gradle.kts`: `alias(libs.plugins.kotlin.jvm)`, `alias(libs.plugins.kotlin.serialization)`, `id("application")` with `mainClass = "com.hackathon.v2x.ivi.sim.MainKt"`; `implementation(project(":contract"))`; `testImplementation(libs.junit)`. **Zero dependencies beyond `:contract`** (D9, criterion C4) — no YAML library, no CLI framework, no logging framework.
- `r4-simulator/src/main/kotlin/com/hackathon/v2x/ivi/sim/SimConfig.kt` — `data class SimConfig(host, port, scenarioPath, rateHz, startDelayS)` with two factories: `fromEnv()` reading **exactly** the mini-blueprint's ADA-node variable names — `IVI_ECU_HOST`, `IVI_ECU_PORT`, `R4_SCENARIO`, `R4_RATE_HZ`, `START_DELAY_S` (HLD §8: the names match the real ADA node so Phase 6 is an image swap with no node-config edit) — and `fromArgs(args)` for host mode. Env wins over the scenario file's own default rate. Missing required values fail loudly with the variable name in the message. No literals: defaults live as named constants in this file.
- Test `r4-simulator/src/test/kotlin/.../SimConfigTest.kt`: a full env map parses; a missing `IVI_ECU_HOST` fails with a message naming it; `R4_RATE_HZ` overrides the file default; args mode parses `--host/--port/--scenario/--rate`.

**Acceptance:** `./gradlew :r4-simulator:test` green; the module's dependency block names only `:contract` and JUnit.

**Dependencies:** after `4.5.1.4`. **Fully parallel** with groups 5.2–5.5. **Commit:** `[4.5.6.1] feat: add the :r4-simulator module and its env/args configuration`

### [ ] `4.5.6.2` — Scenario model + loader *(agent)*

**Objective:** make scenarios **data, not code** — the same rule R11 imposes on the bench (D9).

**Scope:**

- `r4-simulator/src/main/kotlin/.../Scenario.kt` — `@Serializable` scenario and step models. The shape all three data files and three tests below depend on (freeze it here):

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

  `sample` names one of the frozen `:contract` fixtures (`r4-warning`, `r4-state`, `r4-unknown-warning`); `overrides` is a map of dotted JSON paths to `JsonElement` values (explicit `null` allowed and meaningful); `kind: "raw"` sends `text` as literal bytes and skips validation.
- `r4-simulator/src/main/kotlin/.../ScenarioLoader.kt` — file path → `Scenario`, with rejection messages that name the file and the offending field. An unknown `sample` name, an unknown `kind`, and an empty `steps` list are all rejections, not silent defaults.
- Test `r4-simulator/src/test/kotlin/.../ScenarioLoaderTest.kt`: the three committed scenario files of `4.5.6.4` load (this test lands green once `4.5.6.4` does — until then it loads inline fixtures and `4.5.6.4` extends it to the real files); malformed JSON, unknown sample name, unknown kind and empty steps are each rejected with a message naming the cause.

**Acceptance:** `./gradlew :r4-simulator:test` green; no scenario behaviour is expressed as a Kotlin branch on a scenario name.

**Dependencies:** after `4.5.6.1`. **Commit:** `[4.5.6.2] feat: add the scenario model and loader for the R4 simulator`

### [ ] `4.5.6.3` — `SampleLibrary` + `MessageBuilder` — overrides validated through `R4Json` *(agent)*

**Objective:** build every payload from the frozen sample, apply the step's overrides at `JsonElement` level, and prove the app can parse it **before** it goes on the wire (D9).

**Scope:**

- `r4-simulator/src/main/kotlin/.../SampleLibrary.kt` — loads the frozen samples off the **`:contract` classpath** (`/contracts/samples/…`, D6). A sample carried as a literal in this file is a defect: a simulator with its own copy of the schema is a second, unversioned contract.
- `r4-simulator/src/main/kotlin/.../MessageBuilder.kt`:
  1. Parse the named sample to a `JsonObject`.
  2. Apply the step's dotted-path overrides at `JsonElement` level — `riskState`, `warningType`, `schemaVersion`, `object.source`, `object.distance`, `geometry.vehicleC` (including explicit `null`), plus **arbitrary additive junk fields**. Element-level editing is what lets an unknown extra field survive onto the wire; a typed round trip would drop it (the committed `R4AdditiveVersionTest` proves the drop).
  3. **Decode the result through `:contract`'s `R4Json` before returning it** — a payload the simulator cannot parse is a payload the app cannot parse, and the run must fail loudly at the producer. The **one exception** is a `kind: "raw"` step, which returns its literal bytes unvalidated on purpose (the malformed case).
  4. Return UTF-8 bytes.
- Test `r4-simulator/src/test/kotlin/.../MessageBuilderTest.kt`: every built payload from every non-raw step decodes through `R4Json`; a `geometry.vehicleC: null` override produces JSON `null` (not an absent key); an added junk field is present in the emitted bytes **and** the payload still decodes; a `raw` step's bytes are returned untouched and are *not* validated; an override that makes the payload invalid (e.g. `object.distance: "far"`) fails the build with a message naming the step.

**Acceptance:** `./gradlew :r4-simulator:test` green; a grep of `r4-simulator/src/main` finds no `{"schemaVersion"` literal.

**Dependencies:** after `4.5.6.2`. **Commit:** `[4.5.6.3] feat: build simulator payloads from the frozen samples with validated overrides`

### [ ] `4.5.6.4` — The three scenario data files + the stream-difference test *(agent)*

**Objective:** commit the scenarios the acceptance boxes need, as data files, and prove different files produce observably different streams.

**Scope — three files under `IVI_ECU/r4-simulator/scenarios/` (HLD §3.1):**

- `approach.json` — the full lifecycle in one file, because the timeout/restore path is otherwise only reachable by waiting for a stream to stop: a **first step with `geometry.vehicleC: null`** (C not yet tracked — the renderer's null-C path), then C approaching with `riskState` low → medium → high and `geometry.vehicleC` closing, then **C leaving** — distance opening back out with risk falling to `low` — and finally silence for longer than `WARNING_TIMEOUT_MS` so the view times out and the previous mode is restored while the scenario is still running. (The approach/leave shape is taken from PR #2's mock sender, which had it right.) This is the scenario the in-Room evidence run uses.
- `degrade.json` — the degradation cases in one file: a step with an unknown `warningType` + `schemaVersion: 2` + a junk field (the R4 additive-version case); a step with `object.source: "own_sensor"` (trips the renderer's provenance guard); and a `kind: "raw"` step of non-JSON bytes (the receive loop must survive and keep listening).
- `state-stream.json` — the optional R15 path: periodic `state` messages with ascending `seq`.
- Test `r4-simulator/src/test/kotlin/.../ScenariosDifferTest.kt`: all three files load through `ScenarioLoader`; the byte streams produced by `approach.json` and `degrade.json` differ **observably** (assert on decoded field values — `riskState` progression, `warningType`, `object.source` — not just on byte inequality); and extend `ScenarioLoaderTest` to load the three real files.

**Acceptance:** `./gradlew :r4-simulator:test` green; the three files exist at the designated paths; no new Kotlin branch keys off a scenario name.

**Dependencies:** after `4.5.6.3`. **Commit:** `[4.5.6.4] feat: add the approach, degrade and state-stream scenario files`

### [ ] `4.5.6.5` — `UdpSender` + `Main` — the two run modes and the `[TX]` log *(agent)*

**Objective:** make the simulator runnable, in host mode from a laptop and in-Room from a container entrypoint.

**Scope:**

- `r4-simulator/src/main/kotlin/.../UdpSender.kt` — a `DatagramSocket` sending to `host:port`; one send per step; errors logged and counted, never fatal to the loop.
- `r4-simulator/src/main/kotlin/.../Main.kt` — resolves `SimConfig` (args present → host mode; otherwise env → in-Room mode), waits `START_DELAY_S` (the AAOS guest boots slower than a container), loads the scenario, then walks the steps at the resolved rate, logging one line per send: `[TX] step=<i> type=<t> bytes=<n> → <host>:<port> risk=<r> warningType=<w>`. On `loop: true` it repeats the step list. Exits non-zero if the scenario fails to load or a non-raw payload fails validation — the run must fail loudly at the producer (D9).
- No test beyond what exists — `Main` is glue; the behaviour is covered by `4.5.6.2`–`4.5.6.4`.

**Acceptance:** `./gradlew :r4-simulator:installDist` produces a runnable distribution; running it in host mode against a local `nc -ul <port>` (or the `4.5.3.5` loopback listener) emits `[TX]` lines and the datagrams arrive; `./gradlew :r4-simulator:test` still green.

**Dependencies:** after `4.5.6.4`. **Commit:** `[4.5.6.5] feat: add the R4 simulator UDP sender and its host/in-Room entrypoint`

### [ ] `5.5.6.6` — Simulator `Dockerfile` + `entrypoint.sh` *(agent)*

**Objective:** package the simulator as the single-platform arm64 image the mini-blueprint's ADA node pulls.

**Scope:**

- `IVI_ECU/r4-simulator/Dockerfile` — multi-stage: **build stage on `$BUILDPLATFORM`** (JVM bytecode is architecture-neutral, so no QEMU-emulated Gradle), runtime stage `linux/arm64` on a JRE 17 base. **Build context is `IVI_ECU/`** — it needs the Gradle wrapper, `settings.gradle.kts`, the catalog, `:contract` and `:r4-simulator`. This is a deliberate, HLD-flagged deviation from "own `Dockerfile` at the folder root" ([node-code-layout.md](../.claude/rules/node-code-layout.md)); self-containment is preserved because the build reads nothing outside `IVI_ECU/`. Copy the `scenarios/` folder into the image; workdir `/app`.
- `IVI_ECU/r4-simulator/entrypoint.sh` — sleeps `START_DELAY_S` then runs the distribution; the blueprint node config's `command` is `["./entrypoint.sh"]` — **relative, not `/entrypoint.sh`** ([deploy-walkthrough-netcheck.md § 7](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#7-mistakes-already-made--check-these-first) mistake #5: it resolves against the image workdir `/app`). The same relative form is what [deploy-ivi-hmi-walkthrough.md §4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) rung **V4** carries in the evidence config. LF line endings; `chmod +x`.
- `docker build` on this Windows dev host is unavailable — verification transfers to the CI lane `5.5.7.3`, exactly as Phase 0's `6.0.8.1` transferred to `5.0.8.2`.

**Acceptance:** `sh -n r4-simulator/entrypoint.sh` passes; the Dockerfile declares `FROM --platform=$BUILDPLATFORM` on the build stage only and copies `scenarios/` into `/app/scenarios/`; image build verified by `5.5.7.3`.

**Dependencies:** after `4.5.6.5`. **Commit:** `[5.5.6.6] feat: add the R4 simulator Dockerfile and entrypoint`

### [ ] `4.5.6.7` — `DevInjectorReceiver` — injection point I3, debug build only *(agent)*

**Objective:** let an `adb broadcast` push one frozen sample onto the same flow the socket feeds, so UI work is unblocked while the ADB/network route is still unproven.

**Scope:**

- `app/src/debug/java/com/hackathon/v2x/ivi/debug/DevInjectorReceiver.kt` — a `BroadcastReceiver` for `com.hackathon.v2x.ivi.DEV_INJECT` reading `--es sample <name>`, loading that frozen sample from the `:contract` classpath, decoding it through `R4Deserializer`, and calling `R4Repository.inject(...)`. Because it joins **downstream of the socket and upstream of everything else**, it exercises parse → repository → view-model → Compose exactly as a real datagram does (HLD §5.3, §7).
- Register it in a **`app/src/debug/AndroidManifest.xml`** — the debug source set only. **It must be absent from the release build**: a release path that can fabricate a warning would undermine the R19 claim that C came only from relayed data (HLD §7).
- Manual invocation is rung **V3** of [deploy-ivi-hmi-walkthrough.md §4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) — the broadcast command, the expected Display-Area switch, and the `result=0`-with-no-UI-change failure that means a release build are all defined there. Record the run in the commit body.

**Acceptance:** `./gradlew assembleDebug` and `assembleRelease` both green, and the **release** merged manifest contains no `DEV_INJECT` receiver (check `app/build/intermediates/merged_manifests/release/AndroidManifest.xml`); `:app:testDebugUnitTest` still green.

**Dependencies:** after `16.5.5.5` and `4.5.4.2`. **Commit:** `[4.5.6.7] feat: add the debug-only dev injector for I3 UI testing`

---

## Task Group 5.7 — CI lanes (serves R4, R5, R16) — HLD §3.2 rows 2–3, §6.1

> **Where a lane goes is decided by its origin phase**, per [phase0-ci.yml](../.github/workflows/phase0-ci.yml)'s own header rule: `ivi-unit-tests` originated in Phase 0 and is maintained there even when a Phase 5 subtask edits it; the two new lanes originate here and get their own `phase5-ci.yml`.

### [ ] `16.5.7.1` — New `phase5-ci.yml` with the `ivi-assemble` lane *(agent)*

**Objective:** build the APK on every push, gate it on the IVI unit tests, and publish `app-debug.apk` as a run artifact so the ADB install steps have a build to fetch.

**Scope:** new `.github/workflows/phase5-ci.yml`, named `phase5-ci`, with the same `on:` triggers and `concurrency` block as `phase0-ci.yml` (so all lanes still run on every push; the split changes where a lane is maintained, never whether it executes). One job:

- `ivi-assemble` — `actions/checkout@v4`, `actions/setup-java@v4` (temurin 17, `cache: gradle`), `working-directory: IVI_ECU`, `chmod +x gradlew`, `./gradlew :app:testDebugUnitTest --no-daemon` then `./gradlew assembleDebug --no-daemon`, then `actions/upload-artifact@v4` publishing `IVI_ECU/app/build/outputs/apk/debug/app-debug.apk` as `app-debug-apk` with `if-no-files-found: error`. `timeout-minutes: 30` bounds a hung dependency resolve without capping a slow cold build.
- **The unit tests run in the same job as the build, deliberately overlapping `phase0-ci.yml`'s `ivi-unit-tests`** — this job's output is hand-installed onto a guest, so an APK must never leave the workflow unless its own tests passed in the job that produced it. It is a gate on the artifact, not a second test lane: extend `ivi-unit-tests` when test targets change, never this step.
- Two reporting steps between assemble and upload: record the APK size as a `::notice::` (`::error::` and fail if the APK is missing after a successful assemble), and report whether the APK declares a launcher activity — the latter never fails the lane, and exists because the missing launcher entry (until `16.5.5.5`) is the most expensive surprise in the bring-up route.
- Add `android-actions/setup-android` only if the runner image's SDK/licence state turns out insufficient (HLD §6.1) — try without it first and record which was needed.
- **No `lint` step — deferred, not dropped.** This subtask originally specified `assembleDebug lint`; the lane as shipped (`4120d9b`) omits lint entirely, because this project's lint findings have never been read and `ivi-assemble` is the *only* lane producing the APK, so one pre-existing `Error`-severity finding would block APK production for reasons unrelated to the push. **Re-entry condition:** run `./gradlew lint` once, record the findings, set `lint { abortOnError = false }` in `app/build.gradle.kts` with a comment naming them, then add the step — as its own subtask, not by reopening this one. Until then `phase5_tasks.md`'s "zero `Error` severity issues" bar is unmeasured, and the `NewApi` check that would flag a call above `minSdk` is not running (relevant to the unverified guest API level in [deploy-ivi-hmi-walkthrough.md §4.5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#45-connect-and-check-the-guest)).

**The shipped lane is documented by [deploy-ivi-hmi-walkthrough.md §3.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#31-the-workflow-its-job-and-its-triggers)** — its job name, step order, triggers, concurrency, timeout and the three notices it emits. That section and this subtask must stay in step; neither restates the other's detail.

**Acceptance:** the lane runs green on the branch, confirmed by either route of [§3.2](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#32-check-that-the-run-finished-and-passed), and the `app-debug-apk` artifact is retrievable by either route of [§3.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#33-get-the-apk-off-ci); record the run ID **and the APK's size** in the status line. The size is recorded, not gated: the stale plan cites a "< 50 MB" budget which no requirement in the report carries, so a number over it is a finding to raise, not a build failure.

**Dependencies:** none beyond a pushable branch — **land this early, in parallel with group 5.1**, so an APK artifact exists for group 5.8. **Commit:** `[16.5.7.1] ci: add phase5-ci with the ivi-assemble lane`

### [ ] `4.5.7.2` — Extend `ivi-unit-tests` to all five modules *(agent)*

**Objective:** stop new modules' tests from passing locally and never running in CI (HLD §6.1 — the invocation that must change).

**Scope:** in [.github/workflows/phase0-ci.yml](../.github/workflows/phase0-ci.yml), the `ivi-unit-tests` job's run step becomes exactly:

```yaml
./gradlew :contract:test :serializer:test :observer:test :r4-simulator:test :app:testDebugUnitTest --no-daemon
```

Nothing else in that file changes. Add a one-line comment recording that this Phase 0-origin lane was extended by Phase 5 subtask `4.5.7.2`, matching the file's existing convention for `7.1.3.5` and `11.1.1.1`.

**Acceptance:** the `ivi-unit-tests` lane runs green with all five module test tasks executing (visible in the run log); record the run ID.

**Dependencies:** **sequential after `4.5.6.4`** — every one of the five Gradle projects must exist and have tests, or the lane fails on a missing project. **Commit:** `[4.5.7.2] ci: run all five IVI Gradle modules' tests in ivi-unit-tests`

### [ ] `5.5.7.3` — `r4-sim-image` lane — arm64 build, push and verify *(agent)*

**Objective:** publish `m1-r4-sim:latest` to the CarSky Zot registry as a single-platform arm64 image the ADA node can pull.

**Scope:** add a second job to `.github/workflows/phase5-ci.yml`, modelled on `phase0-ci.yml`'s `netcheck-image` job (copy its structure, do not re-invent it):

- `REGISTRY_HOST: registry.hackathon-2.carsky.io` (the `hackathon-2` host — `registry.carsky.io` 502s, [deploy-walkthrough-netcheck.md § 7](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#7-mistakes-already-made--check-these-first) mistake #2), `IMAGE: m1-r4-sim:latest`, `PLATFORMS: linux/arm64`.
- `docker/setup-qemu-action@v3` + `docker/setup-buildx-action@v3`, then `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -f IVI_ECU/r4-simulator/Dockerfile -t $REGISTRY_HOST/$IMAGE --push --metadata-file … IVI_ECU/` — **the cluster rejects manifest indexes**, so `--provenance=false --sbom=false` and a single platform are mandatory, not stylistic.
- Login with `secrets.CARSKY_ZOT_API_KEY` / `vars.CARSKY_REGISTRY_USER || 'kis@hackathon.fpt.com'`; skip the push with a notice when the secret is absent, exactly as `netcheck-image` does.
- Verify with the existing [`./.github/actions/verify-arm64-image`](../.github/actions/verify-arm64-image) action, passing the digest from the `--metadata-file`.

**Acceptance:** the lane pushes and the verification step confirms the tag is pullable and single-platform `linux/arm64`; record the run ID and the pushed digest.

**Dependencies:** after `5.5.6.6`. **Commit:** `[5.5.7.3] ci: add the r4-sim-image build/push/verify lane`

---

## Task Group 5.8 — Retire the deployment unknowns early (serves R5, R16) — HLD §11 items 1–2

> **Scope against group 5.10.** This group exists to answer the ADB and guest questions before anything depends on the answer; group 5.10 owns the two full in-Room runs and their choreography. `5.5.8.2`'s Room is the one chain A reuses, so the two are sequenced, not duplicated: `5.5.8.2` stands a Room up against **today's** APK to probe the route, and `5.5.10.1`–`17.5.10.9` run the evidence chain on it once the app exists.
>
> **This group is the phase's biggest schedule risk and depends on nothing, so it must not wait behind any code work.** The ADB route to the Skycraft guest is unverified on this deployment and the platform's REST VM-shell route answers 502. If the guest is unreachable, **every in-Room criterion falls back to emulator evidence** — an answer that is cheap to get first and expensive to get last. The probe deliberately uses **today's** APK, so it does not wait for a single line of Phase 5 code.

### [ ] `5.5.8.2` — USER-MANUAL: create and deploy the mini-blueprint (probe config)

**Objective:** the user creates `trial3_minh_ivi` by clone-then-delete and deploys it with the probe ADA config, so a Room with a Running IVI node exists to test ADB against.

**Scope:** the procedure is [deploy-ivi-hmi-walkthrough.md §4.11](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#411-the-mini-blueprint-route) for the clone-then-delete creation route, [§4.2](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#42-configure-the-blueprint-and-its-ivi-node) for the IVI node's `image` block, its `ethernet` pin and the config read-back, and [§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint) for the deploy and the wait for `Running`. This subtask supplies only the phase's own values:

- Clone `trial2_minh_netcheck` → `trial3_minh_ivi`, delete the Bench and V2X nodes. The surviving topology is Ethernet Bridge `10.99.0.1` (`10.99.0.0/24`, `bridgeMode: "linux"`), ADA Container Node `10.99.0.12`, IVI Skycraft Node `10.99.0.13`.
- Set the ADA node to the **probe** feed of [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) rung **V2** — `m1-netcheck:latest`, `NEXT_HOP_HOST=10.99.0.13`, `NEXT_HOP_PORT=47300` — prefixing the image with `registry.hackathon-2.carsky.io/` and setting `capabilities: ["NET_RAW"]` for the `[CAP]` corroboration of R6.
- During §4.2's read-back, record the IVI node's measured Part Prefix, Display Width/Height/DPI and GPU Backend into the run record. Read them off the live node; PR #2's `virglrenderer` / `1280×720` are unverified. The display size is the resolution the committed R16 previews are drawn for.
- The 2-concurrent-Room budget is shared with the comms track.

No agent performs these steps; the plan tracks them. [§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint) carries the rule that costs the most time when broken — **always edit the original blueprint, never the `<name>-deploy` snapshot**.

**Acceptance:** 3/3 nodes `Running` with restart count 0, and the measured Skycraft display fields recorded, in a new `plans/doc/phase5-ivi-run.md` created by this subtask (the Phase 0 `phase0-smoke-test-run.md` pattern). Evidence commit made by the orchestrating session after the user confirms.

**Dependencies:** none — **starts immediately**, in parallel with `4.5.1.1`. **Commit:** `[5.5.8.2] docs: record the Phase 5 mini-blueprint deployment and node-Running evidence`

### [ ] `16.5.8.3` — USER-MANUAL: prove the ADB route and read the guest API level

**Objective:** answer HLD open items **#1** (ADB reach) and **#2** (AAOS guest Android version) before the APK is finished, because both invalidate the in-Room evidence plan if negative.

**Scope:** with the Room from `5.5.8.2` Running, run [deploy-ivi-hmi-walkthrough.md §4.4](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#44-get-an-adb-endpoint) → [§4.5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#45-connect-and-check-the-guest) → [§4.6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#46-install-the-apk) end to end, recording each command's exact output. Those three sections carry the endpoint routes, the connect and `getprop` / `pm list features` commands, the install command, and what each failure means; nothing of that is restated here. What is specific to running them **early**, before the app exists:

- **Use today's APK** at §4.6 — `./gradlew assembleDebug` locally, or the `app-debug-apk` artifact from lane `16.5.7.1`. Today's APK has no launcher activity; that is expected and irrelevant, because this subtask proves the **route**, not the app.
- **Confirm the evidence filter streams** — `adb logcat -s IVI_V2X`, the guest-side log surface [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) reads and the whole demo's text evidence depends on.
- **Probe the REST pair while you are in §4.4** — that section already instructs it. Record which of `…/screenshot` and `…/shell` answer: a live `screenshot` route is a **second, independent evidence route** for group 5.9 that does not depend on ADB at all.

**The findings this subtask exists to produce** are items **1, 2, 3, 7 and 9** of [§6.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) — ADB reach by any route, the `adb-tunnel` / `adb-exec` routes answering on this deployment, the guest's API level and `automotive` feature, whether the 502 pair is still 502, and whether the ADB widget can install in practice. Answer each on this deployment and write the answer down.

**If §4.5's connect or §4.6's install fails**, that is the finding: record it, and every in-Room criterion in group 5.9 degrades to **AAOS emulator** evidence (I3/I4 on an *automotive* system image — a phone image rejects the APK on the `automotive` feature), unless the screenshot route came back alive. Escalate to the orchestrating session rather than retrying blind; §4.10 is the troubleshooting table, not a licence to retry the same route.

**Acceptance:** the outputs of §4.4 through §4.6 (or the exact failure) recorded in `plans/doc/phase5-ivi-run.md`; open items #1 and #2 marked closed-positive or closed-negative with the fallback decision written down. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `5.5.8.2`. **This is the phase's earliest schedule risk — run it before group 5.5 finishes, not after.** **Commit:** `[16.5.8.3] docs: record the proven ADB route and AAOS guest API level`

### [ ] `16.5.8.4` — Extend `node-ivi-ecu.md` with the proven route *(agent — docs)*

**Objective:** write the now-known ADB facts into the per-node deploy guide, their sanctioned home (HLD §3.2 row 4).

**Scope:** extend [requirements/car-sky-guide/node-ivi-ecu.md](../requirements/car-sky-guide/node-ivi-ecu.md) § Post-deploy with, verbatim from `16.5.8.3`'s recorded outputs, the **facts** that file owns: which endpoint source of [§4.4](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#44-get-an-adb-endpoint) actually worked and the `adb connect` form it produced, the guest's API level, and its automotive-feature answer. **The commands are not copied in** — install is [§4.6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#46-install-the-apk), launch and the D10 `--ei r4_port` override are [§4.7](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#47-open-the-screen-and-launch-the-app), and the `adb logcat -s IVI_V2X` filter is [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging); link them. That is the division the walkthrough states about itself: the node guide owns the node's *facts*, the walkthrough owns the *doing*. If the route failed, record that instead, plus the emulator fallback. Do not restate the blueprint procedure — that is [§4.2](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#42-configure-the-blueprint-and-its-ivi-node) / [§4.11](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#411-the-mini-blueprint-route).

**Acceptance:** the guide's § Post-deploy carries the proven (or failed) endpoint and the guest's API-level and automotive answers, and links §4.6–§4.8 for the commands instead of duplicating them; no invented values — every line traces to `16.5.8.3`'s record. Doc-only.

**Dependencies:** after `16.5.8.3` and `16.5.5.5` (the launch-override command must exist before it is documented as working). **Commit:** `[16.5.8.4] docs: record the proven ADB route and launch override in the IVI node guide`

---

## Task Group 5.9 — In-Room R4 evidence run (serves R5, R6, R16, R17, R4)

> The verification ladder of [deploy-ivi-hmi-walkthrough.md §4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) — rungs **V1–V5**, one per subtask below — which is what closes four of the five milestone boxes with in-Room evidence; its [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance) states the four proofs those rungs must produce. ([phase5-mini-blueprint.md §5](../IVI_ECU/doc/research_notes/phase5-mini-blueprint.md) is the research note the ladder came from — non-authoritative since the walkthrough absorbed it.) **Reuse the `trial3_minh_ivi` Room from group 5.8** — reconfigure the ADA node in place rather than creating a second Room (2-deployment budget, HLD §11 item 5). If `16.5.8.3` came back negative, every subtask below runs on an AAOS emulator instead, and each records that it did.
>
> **This group owns the scenario content of the runs; group 5.10 owns the Room choreography that carries it.** The platform steps §5 assigns to AI — installing and launching the APK, reading the two log surfaces, polling node phases — are group 5.10's subtasks with their own labels; what stays here is which scenario file is on the wire and what its rung must show. Read the two together: `17.5.9.3` is rung V4's scenario, `18.5.10.8` and `17.5.10.9` are the reading and the looking that close it.

### [ ] `5.5.9.1` — USER-MANUAL: swap the ADA node to the simulator image and redeploy

**Objective:** put the R4 simulator on the wire toward `10.99.0.13:47300`.

**Scope:** on `trial3_minh_ivi` (**the original blueprint, not the `-deploy` snapshot** — [deploy-ivi-hmi-walkthrough.md §4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint)), replace the ADA node's config with the **evidence config** of [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) rung **V4** — the image, the relative `command`, and the `IVI_ECU_HOST` / `IVI_ECU_PORT` / `R4_SCENARIO` / `R4_RATE_HZ` / `START_DELAY_S` env set are fixed there. This phase adds only the `registry.hackathon-2.carsky.io/` prefix on the image reference and `capabilities: ["NET_RAW"]`, so a `[CAP]` line can corroborate the datagram on the wire (R6). [§4.11](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#411-the-mini-blueprint-route) is why this is an image swap with no other node-config edit. Redeploy per §4.3; wait for `Running`; read the ADA node's log by either route of §4.8's log-surface table.

**Acceptance:** ADA node `Running` with restart count 0, and its log shows §4.8 V4's **link 1** — `[TX] … → 10.99.0.13:47300` at ~1 Hz — plus the `[CAP]` corroboration. Recorded in `plans/doc/phase5-ivi-run.md`. Evidence commit by the orchestrating session.

**Dependencies:** after `5.5.7.3` (the image must be pushed and verified) and `5.5.8.2`. **Commit:** `[5.5.9.1] docs: record the R4 simulator running on the mini-blueprint ADA node`

### [ ] `16.5.9.2` — USER-MANUAL: install and launch the Phase 5 APK → the R16 layout on the node

**Objective:** close milestone box 1 — *the HMI runs on the AAOS node with the R16 layout; button/app areas switch what the Display area shows.*

**Scope:** [deploy-ivi-hmi-walkthrough.md §4.6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#46-install-the-apk) then [§4.7](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#47-open-the-screen-and-launch-the-app), over the route proven in `16.5.8.3` and against the Phase 5 build fetched from lane `16.5.7.1` per [§3.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#33-get-the-apk-off-ci). §4.7 owns the Screen / Log / ADB widget setup, the launch command and its port override, the expected on-screen layout, and the black-screen recovery. The V2X LINK indicator reading **BOUND :47300** is rung **V1** of [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) — it proves the socket bound and that the status bar is live rather than the old hardcoded `STANDBY`. The side-button observation is [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance)'s second table, first row.

**Also record — the boot-to-listener number Phase 6 needs.** §4.7 already instructs the two wall-clock deltas at first launch; what this subtask adds is why they must not be skipped. [m1-run-timing-and-event-triggering.md §9 open item 5](../requirements/m1-run-timing-and-event-triggering.md) names this the one number **only Phase 5 can produce**: the elapsed time from the AAOS guest starting to boot until `[LINK] state=bound port=47300` appears on `IVI_V2X`. It sets the **floor for the bench's `start_delay_s`** (§4.2's B-1 readiness pick — the IVI is the only node whose readiness cannot be observed from a container, which is why the guest must not be streamed into before it is bound). Record the wall-clock deltas for guest boot → launcher, launch → `[LINK] state=bound`, and their sum, in `plans/doc/phase5-ivi-run.md`. Phase 0 could not obtain this (ADB returns 502, residual O4); if `16.5.8.3` came back negative and this runs on an emulator, say so — an emulator figure is a lower bound, not the number.

**Acceptance:** screenshot showing the R16 layout with the link indicator bound; a second screenshot showing a different Display Area mode after a side-button tap; `adb logcat -s IVI_V2X` shows `[LINK] state=bound port=47300` and `[UI] mode=… cause=user`; the boot-to-listener elapsed time recorded. Screenshots taken per [§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence). Recorded in `plans/doc/phase5-ivi-run.md`.

**Dependencies:** after `5.5.9.1`, `17.5.5.6` and `16.5.8.3`. **Commit:** `[16.5.9.2] docs: record the R16 layout running on the AAOS node`

### [ ] `17.5.9.3` — USER-MANUAL: `approach.json` → God View with ego, B and ghost C

**Objective:** close milestone boxes 2 and 3 — *a mock R4 warning brings the warning view up showing ego, B and ghost C at the composed positions*, and *ghost C renders from `v2x_relayed` data only; the 2D drawing is delivered.*

**Scope:** with `approach.json` running on the ADA node, work rung **V4** of [deploy-ivi-hmi-walkthrough.md §4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging). Its four links — ADA `[TX]`, `[RX] … cSource=v2x_relayed`, `[UI] mode=WarningView cause=warning`, and the Display Area drawing the God View by itself — are the check list, along with its `[? UNKNOWN SOURCE]` blocking-defect case, its `geometry.vehicleC: null` first-step rule, and its "Also check" timeout-to-Idle row. Do not re-derive any of them.

Two observations belong to this scenario file rather than to that section:

- **Risk progression low → medium → high** across the approach, with the glow colour following it.
- **The restore of the *previous* mode** on timeout, not merely the return to Idle — the `16.5.4.5` behaviour V4's row does not name.

Capture the evidence per [§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence): a screen recording of the warning coming up, plus the logcat excerpt.

**Acceptance:** recording + logcat excerpt covering proofs 1–4 of [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance) — ghost C rendered with `cSource=v2x_relayed` on every warning; the null-C first step and the timeout-restore both observed. Recorded in `plans/doc/phase5-ivi-run.md`.

**Dependencies:** after `16.5.9.2`. **Commit:** `[17.5.9.3] docs: record the God View in-Room evidence with v2x_relayed provenance`

### [ ] `4.5.9.4` — USER-MANUAL: `degrade.json` → graceful degradation, guard trip, loop survival, teardown

**Objective:** close milestone box 4 — *a newer message with an unknown `warningType` degrades gracefully* — and exercise the two defensive paths, then release the Room.

**Scope:** set the ADA node's `R4_SCENARIO=/app/scenarios/degrade.json`, redeploy (or restart the node), and work rung **V5** of [deploy-ivi-hmi-walkthrough.md §4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging). Its three rows are exactly this scenario's three steps — the unknown `warningType` with `schemaVersion: 2` and a junk field, the `object.source: "own_sensor"` guard trip, and the raw non-JSON step — each with its correct and incorrect result stated there. Two things that section does not carry, and this subtask does:

- **The guard trip is a phase gate.** If the `own_sensor` step does **not** trip the guard, the R19 wiring `17.5.4.4` armed is broken; that is a blocking finding for Phase 5, not a display quirk.
- **Teardown:** [§4.12](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#412-tear-down) — **Delete Deployment**, because the 2-concurrent-Room budget is shared and the comms track needs the slot. The blueprint `trial3_minh_ivi` is kept and redeployable.

**Acceptance:** logcat excerpt and screenshots ([§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence)) for all three V5 rows, and the teardown confirmed, recorded in `plans/doc/phase5-ivi-run.md`. This subtask's record closes the phase's evidence trail.

**Dependencies:** after `17.5.9.3`. **Commit:** `[4.5.9.4] docs: record graceful degradation, guard trip and loop survival evidence`

---

## Task Group 5.10 — In-Room verification: the mini-blueprint run and the whole-system run (serves R4, R5, R6, R16, R17, R18, R19)

> **This group verifies on the platform what the rest of the phase builds.** It writes no product code and no images. The IVI app is groups **5.1–5.5**; the mock-ADA container — the image that stands in for the ADA ECU and emits R4 warning messages — is groups **5.6–5.7**, which write its source and the CI lane that builds, pushes and verifies it. The real Bench, V2X ECU and ADA ECU images come from their own phases. Nothing below rebuilds any of them.
>
> **Two runs, two chains.** They differ only in composition, and each closes different evidence:
>
> - **Chain A — the mini-blueprint run** (`5.5.10.1`–`17.5.10.9`): Ethernet Bridge + a mocked ADA node + the IVI Skycraft node. The ADA node runs the group 5.6–5.7 image, so the IVI is exercised against R4 traffic with no dependency on the ADA or comms tracks.
> - **Chain B — the whole-system run** (`5.5.10.10`–`19.5.10.17`): every node in the blueprint — Bench, V2X ECU, ADA ECU, IVI ECU, Ethernet Bridge — each carrying its own real image, with the warning the IVI renders originating in a bench scenario and travelling the full relay.
>
> **The governing walkthroughs, by subject.** Chain A's composition is [deploy-ivi-hmi-walkthrough.md §4.11](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#411-the-mini-blueprint-route), which states that its mechanics are exactly §4.2–§4.10 with only the composition differing. Chain B's composition is [deploy-ada-ecu-walkthrough.md §5.6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#56-the-full-blueprint-route), which states the same about its own §4.1–§5.5 and hands consumer-side evidence to the IVI walkthrough's [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging). Container image build → push → pull is [deploy-walkthrough-netcheck.md](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md)'s subject, which the IVI walkthrough's header and [§1.2](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#12-cloud-platform-access) delegate to it outright. Each subtask cites the section governing *it*; [walkthrough-driven-delivery.md](../.claude/rules/walkthrough-driven-delivery.md) asks for that, not that one group cite one document.
>
> **Cloning is the only route that preserves `ethernet` pins.** Both §4.11 and [deploy-ada-ecu-walkthrough.md §5.6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#56-the-full-blueprint-route) say plainly that a script-built or imported blueprint arrives without its pins, and typically without the Skycraft `image` block, and is rejected at deploy. Every blueprint below is built by cloning and editing on the canvas.
>
> **Two different things are called "install", and they land on opposite sides of the split.** Setting a node's **image field** is a Nydus Inspector edit and is Human — the REST API has no update route for an existing node's config, which [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human) and [deploy-ada-ecu-walkthrough.md §7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#7-work-division-between-ai-and-human) both state. Installing the **APK** with `adb install -r` and launching it with `adb shell am start` are CLI calls against the guest, and §5 assigns both to AI. A subtask that sets an image field is never the one that installs the APK.
>
> **Where the two runs meet groups 5.8 and 5.9.** `16.5.8.3` answers the ADB-reach and guest-API questions both chains depend on. Group 5.9 owns the *scenario content* of the runs — `approach.json` at rung V4 and `degrade.json` at rung V5 — while this group owns the Room choreography that carries them. Neither restates the other.

**The work split, applied to both chains.**

| Action | Label | Why |
|---|---|---|
| Clone the baseline, delete nodes, compose a blueprint | *USER-MANUAL* | Nydus canvas; REST creates no `ETHERNET` pin and deletes no node |
| Set any node's image field or env | *USER-MANUAL* | Node Inspector; the API has no update route for an existing node |
| Deploy a blueprint | *USER-MANUAL* | **New Deployment** dialog; picking the Device is the user's call and spends a Room slot |
| Read a blueprint or a deployment back over REST | *car-sky* | Authenticated `GET`s; the read-back, not the Inspector, is what counts |
| `adb install -r` the APK, `adb shell am start` it | *car-sky* | CLI against the guest, over the endpoint `16.5.8.3` proved |
| Read any log surface | *car-sky* | Logs route with `container=user`, and `adb logcat -s IVI_V2X` |
| Open the device and its Screen, Log and ADB widgets | *USER-MANUAL* | Devices-panel widgets and their part fields |
| Judge what the display shows, capture it, tear down | *USER-MANUAL* | A visual judgement no log line replaces; teardown releases a Room slot |
| Write image sources and their CI lanes | *agent* | Groups 5.6–5.7 and the phases owning the real images — not this group |

---

### Chain A — the mini-blueprint run

### [ ] `5.5.10.1` — Compose the mini-blueprint by cloning the baseline *(USER-MANUAL)*

**Objective:** produce a 3-node blueprint — Ethernet Bridge, ADA container node, IVI Skycraft node — that keeps the baseline's `ethernet` pins.

**Scope:** the creation route of [deploy-ivi-hmi-walkthrough.md §4.11](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#411-the-mini-blueprint-route) — clone the known-good baseline `trial2_minh_netcheck`, rename the clone `trial3_minh_ivi`, and delete the Bench and V2X nodes on the canvas. §4.11 states why no other route is acceptable and what deleting a node does to its pin and edge; do not substitute a script-built or imported blueprint.

The surviving topology is Ethernet Bridge `10.99.0.1` (`10.99.0.0/24`, `bridgeMode: "linux"`), ADA Container Node `10.99.0.12`, IVI Skycraft Node `10.99.0.13`. Addresses, the `47300` port and the pin shapes stay at the baseline values — §4.11 fixes that, and it is what makes the later image swap a config-free change.

Edit the **original** blueprint, never the `<name>-deploy` snapshot a deployment creates ([§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint)).

**Acceptance:** the blueprint exists with exactly three nodes and their pins intact, confirmed by `6.5.10.3`'s read-back. Recorded in `plans/doc/phase5-ivi-run.md`. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** none within this group. **Commit:** `[5.5.10.1] docs: record the mini-blueprint composition`

### [ ] `4.5.10.2` — Set the mini-blueprint's node image fields and the mock-ADA feed *(USER-MANUAL)*

**Objective:** point the ADA node at the mock-ADA image built by groups 5.6–5.7, and confirm the IVI node carries its Skycraft VM image block.

**Scope:** Node Inspector edits only — no APK, no deploy.

- **The ADA node** takes the evidence config of [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) rung **V4**, which fixes the image, the relative `command` and the `IVI_ECU_HOST` / `IVI_ECU_PORT` / `R4_SCENARIO` / `R4_RATE_HZ` / `START_DELAY_S` env set. Prefix the image reference with `registry.hackathon-2.carsky.io/` and add `capabilities: ["NET_RAW"]` so a `[CAP]` line can corroborate the datagram on the wire (R6). Per [§4.11](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#411-the-mini-blueprint-route) the ADA node is the only node reconfigured.
- **The IVI node** needs the Skycraft `image` block of [node-ivi-ecu.md § Blueprint node config](../requirements/car-sky-guide/node-ivi-ecu.md#blueprint-node-config) — all four fields. On a clone it is already correct and is left alone; [§4.2](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#42-configure-the-blueprint-and-its-ivi-node) names its absence as the rejection that kills the deploy outright with the message quoted there.
- While in the Inspector, **record the IVI node's Part Prefix and Display Width/Height/DPI/GPU Backend** — §4.2 requires reading them off the live node, and §4.7's Screen, Log and ADB widgets select parts by that prefix. Do not assume a resolution.

Setting an image field is a canvas edit, not an installation: the platform pulls the image when the Room deploys.

**Acceptance:** `6.5.10.3`'s read-back shows the ADA node's image, `command`, env set and `NET_RAW`, and the IVI node's four `image` fields; the measured display fields are in `plans/doc/phase5-ivi-run.md`. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `5.5.10.1`; the image exists once `5.5.7.3` has pushed and verified it. **Commit:** `[4.5.10.2] docs: record the mini-blueprint node image fields and the mock-ADA feed`

### [ ] `6.5.10.3` — Read the mini-blueprint back and confirm its topology *(car-sky)*

**Objective:** prove from stored state, not from the Inspector's truncated fields, that the blueprint is deployable before a Room slot is spent on it.

**Scope:** `GET /api/v1/blueprints/{id}` — the AI read-back row of [§4.2](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#42-configure-the-blueprint-and-its-ivi-node), matching the "Read the stored config back" row of [deploy-ada-ecu-walkthrough.md §7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#7-work-division-between-ai-and-human). Confirm every one of:

- one `ETHERNET` / `OUTPUT` pin on the ADA node at `10.99.0.12` and one on the IVI node at `10.99.0.13`, each edged to the bridge's single `INPUT` pin, in the shape [node-ivi-ecu.md § Pins](../requirements/car-sky-guide/node-ivi-ecu.md#pins) fixes;
- the bridge node's `bridgeMode` and `subnet` — without them the `10.99.0.x` addresses have no network;
- the IVI node's four Skycraft `image` fields;
- the ADA node's image reference, relative `command`, `NET_RAW`, and every env value — `IVI_ECU_PORT=47300` above all, since a wrong port produces a silent no-traffic run.

`POST /api/v1/blueprints/{id}/validate` is a cheap second confirmation: it fails until every node has a pin.

**Acceptance:** the read-back excerpt in `plans/doc/phase5-ivi-run.md` with every point above confirmed, or the exact mismatch named and handed back to `5.5.10.1`/`4.5.10.2` for a canvas fix. A deploy does not start on an unconfirmed blueprint.

**Dependencies:** after `4.5.10.2`. **Commit:** `[6.5.10.3] docs: record the mini-blueprint topology read-back`

### [ ] `5.5.10.4` — Deploy the mini-blueprint *(USER-MANUAL)*

**Objective:** bring up the Room the IVI evidence is gathered in.

**Scope:** [§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint) — the blueprint Inspector's **New Deployment**, picking an existing Device from the dropdown rather than creating one. §5 keeps this Human because the Device choice is the user's and consumes one of the two Room slots. Deploy the original blueprint, not its `-deploy` snapshot.

The Skycraft node is the slowest to reach `Running` and is expected to lag the containers; `5.5.10.5` is what watches for it.

**Acceptance:** the deployment exists and its Room id is recorded in `plans/doc/phase5-ivi-run.md`; `5.5.10.5` confirms the node phases. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `6.5.10.3`; a free Room slot (only two run at once, shared with the comms track). **Commit:** `[5.5.10.4] docs: record the mini-blueprint deployment`

### [ ] `5.5.10.5` — Poll the mini-blueprint's nodes to `Running` *(car-sky)*

**Objective:** record that every node came up, and resolve the `nodeKey` every log route needs.

**Scope:** the AI row of [§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint) — poll `GET /api/v1/deployments/{roomId}/nodes` until every node reads `Running` with restart count 0, recording each entry's `name`. A node stuck in `Provisioning` is almost always an image that cannot be pulled ([§4.10](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#410-troubleshooting-the-deploy-and-install)); diagnose per [carsky-room-diagnostics](../.claude/skills/carsky-room-diagnostics/SKILL.md) rather than redeploying blind.

**Acceptance:** 3/3 nodes `Running` with restart count 0 and the three `nodeKey` values in `plans/doc/phase5-ivi-run.md` — the precondition every [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance) proof rests on.

**Dependencies:** after `5.5.10.4`. **Commit:** `[5.5.10.5] docs: record the mini-blueprint Room reaching Running`

### [ ] `16.5.10.6` — Install and launch the APK on the guest *(car-sky)*

**Objective:** get the Phase 5 build running on the AAOS node.

**Scope:** [§4.6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#46-install-the-apk) then the launch half of [§4.7](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#47-open-the-screen-and-launch-the-app) — `adb install -r`, `pm path` to confirm, then `adb shell am start`, with the `--ei r4_port` override available. §5 assigns both rows to AI. The build comes from lane `16.5.7.1`, fetched per [§3.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#33-get-the-apk-off-ci); the ADB endpoint is the one `16.5.8.3` proved. §4.1 fixes the ordering — the guest must be `Running` before anything installs into it.

**This is the APK, not a node image field.** Nothing here touches the canvas.

**Record the boot-to-listener time** §4.7 instructs: guest boot → launcher, launch → the first `[LINK] state=bound` line, and their sum. It is the floor for the bench's start delay and no other phase can produce it.

**Acceptance:** `Success` from the install and the package path from `pm path`; the app started; the two wall-clock deltas recorded in `plans/doc/phase5-ivi-run.md`. Install failures map to §4.10's table — `INSTALL_FAILED_OLDER_SDK` and the `automotive` feature error are escalations, not retries.

**Dependencies:** after `5.5.10.5`, `16.5.8.3` and `17.5.5.6`. **Commit:** `[16.5.10.6] docs: record the APK install and launch on the mini-blueprint guest`

### [ ] `16.5.10.7` — Open the device and its Screen, Log and ADB widgets *(USER-MANUAL)*

**Objective:** make the guest's display and logs visible in the browser, so the run can be watched and captured.

**Scope:** the widget half of [§4.7](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#47-open-the-screen-and-launch-the-app) — Devices on the DockBar, **Connect** to the device the deployment created, then add the **Screen** widget with its Video, Touch and Keyboard parts set from the Part Prefix `4.5.10.2` recorded, plus the **Log** widget on `<prefix>-logcat` and the **ADB** widget on `<prefix>-adb`. §4.7 carries the black-screen recovery and the wake keyevent; §5 keeps this Human because the widgets and their part fields are browser surfaces.

Set the **Recorder Part** now if the run is to be recorded — [§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence) needs it set before the run, not after.

**Acceptance:** the AAOS screen streaming live in the Stage with clicks reaching the guest, and both log surfaces readable. Recorded in `plans/doc/phase5-ivi-run.md`. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `16.5.10.6`. **Commit:** `[16.5.10.7] docs: record the device widgets opened on the mini-blueprint guest`

### [ ] `18.5.10.8` — Read both log surfaces on the mini-blueprint run *(car-sky)*

**Objective:** produce in text three of the four proofs of [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance) — the incoming warning, its parsed fields, and the event raised — making no visual judgement.

**Scope:** authenticate to CarSky ([carsky-deploy-preflight](../.claude/skills/carsky-deploy-preflight/SKILL.md)), then read the two surfaces [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging)'s table names:

- the ADA node's producer log, `GET /api/v1/deployments/{roomId}/logs/{nodeKey}?container=user` — **`container` is mandatory**, omitting it returns 500;
- the guest's `adb logcat -s IVI_V2X`.

The rung is §4.8 **V4**; its four links are the checklist and are not restated here. Stop after link 3 — link 4 is a visual judgement §5 assigns to Human. The scenario driving it is group 5.9's `approach.json`; rung V5's degradation cases are `4.5.9.4`.

**Acceptance:** §6 proofs 1, 2 and 3 as log excerpts in `plans/doc/phase5-ivi-run.md` — one `[RX] type=warning … cSource=v2x_relayed` per datagram corroborated by the ADA's `[TX] … → 10.99.0.13:47300`, the parsed `warningType` / `risk` / `cSource` / `cPos` fields on that line, and `[UI] mode=WarningView cause=warning` carrying `cause=warning` and not `cause=user`.

**Dependencies:** after `16.5.10.6`. **Commit:** `[18.5.10.8] docs: record the mini-blueprint log evidence for the R4 warning chain`

### [ ] `17.5.10.9` — Confirm the God View on the mini-blueprint run, capture it, tear down *(USER-MANUAL)*

**Objective:** produce [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance)'s fourth proof, and release the Room slot.

**Scope:** the "Confirm the display switched" and "Record the screen" Human rows of [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human), on the widgets `16.5.10.7` opened, captured per [§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence). What counts as correct is [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) V4's **link 4** and its two failure rows — including that a yellow `[? UNKNOWN SOURCE]` marker where ghost C belongs is a **blocking defect**, not a display quirk: stop and report it rather than accepting the run. Also confirm §6's second table row — tapping Home / Apps / Settings changes what the Display Area shows.

Tear down per [§4.12](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#412-tear-down) once the evidence is saved: the log route returns nothing after the Room is gone, and chain B needs the slot.

**Acceptance:** §6 proof 4 — a recording or screenshot showing the God View with ghost C dashed, badged and glowing — plus the side-button observation, recorded in `plans/doc/phase5-ivi-run.md`; the deployment deleted and the blueprint kept. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `18.5.10.8`; group 5.9's scenario rungs run against this Room before teardown. **Commit:** `[17.5.10.9] docs: record the mini-blueprint God View evidence and teardown`

---

### Chain B — the whole-system run

### [ ] `5.5.10.10` — Compose the full blueprint and set every node's real image *(USER-MANUAL)*

**Objective:** produce the 5-node blueprint with Bench, V2X ECU, ADA ECU, IVI ECU and the Ethernet Bridge, each container node carrying its own real image.

**Scope:** the composition route of [deploy-ada-ecu-walkthrough.md §5.6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#56-the-full-blueprint-route) — clone the baseline and edit on the canvas, never import; §5.6 states that its mechanics are exactly that document's §4.1–§5.5 with only the composition differing, and assigns "Switch to the full blueprint" to Human in [§7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#7-work-division-between-ai-and-human). Composition and each node's image, config and pin are [carsky-4-node-blueprint.md](../requirements/car-sky-guide/carsky-4-node-blueprint.md) and the per-node files it points at: [node-scenario-player.md](../requirements/car-sky-guide/node-scenario-player.md), [node-v2x-ecu.md](../requirements/car-sky-guide/node-v2x-ecu.md), [node-ada-ecu.md](../requirements/car-sky-guide/node-ada-ecu.md), [node-ivi-ecu.md](../requirements/car-sky-guide/node-ivi-ecu.md).

Set each container node's image field in the Inspector to that node's own image, replacing any probe or mock reference; the IVI node keeps its Skycraft `image` block. §5.6 fixes that the ADA node's own image, `command`, `capabilities`, env, address and port are unchanged between the isolated and full compositions — switching touches the neighbours only. The ADA node's `NET_RAW` and its capture script stop being optional here, because with the Android node running no container there is no sink log to capture on.

**Acceptance:** a 5-node blueprint whose every node names its real image, confirmed by `5.5.10.11`'s read-back; recorded in `plans/doc/phase5-ivi-run.md`. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** every node's image pushed and verified by its own phase's CI lane. **Commit:** `[5.5.10.10] docs: record the full blueprint composition and its node images`

### [ ] `5.5.10.11` — Read the full blueprint back and confirm all five nodes *(car-sky)*

**Objective:** confirm, from stored state, that five nodes carry the right images, pins and addresses before a Room slot is spent.

**Scope:** `GET /api/v1/blueprints/{id}` — the "Read the stored config back" AI row of [deploy-ada-ecu-walkthrough.md §7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#7-work-division-between-ai-and-human) and the same row in [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human). Confirm each node's image reference, `command`, env and capabilities against its node file, and one `ETHERNET`/`OUTPUT` pin per non-bridge node at `10.99.0.10` (bench), `.11` (V2X), `.12` (ADA), `.13` (IVI), each edged to the bridge's `INPUT` pin. Confirm the IVI node's four Skycraft `image` fields — its absence is still the rejection that kills the deploy.

Confirm each image is pullable from the registry before deploying: an unpullable image is the `Provisioning` hang of [§4.10](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#410-troubleshooting-the-deploy-and-install), and the catalog check is the AI row of [netcheck §5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human), with the calls in the form [deploy-ada-ecu-walkthrough.md §3.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#33-confirm-both-images-reached-the-registry) gives.

**Acceptance:** the read-back excerpt in `plans/doc/phase5-ivi-run.md` with all five nodes confirmed and every image resolvable in the registry, or the exact mismatch named and handed back to `5.5.10.10`.

**Dependencies:** after `5.5.10.10`. **Commit:** `[5.5.10.11] docs: record the full blueprint read-back and image confirmation`

### [ ] `5.5.10.12` — Deploy the full blueprint *(USER-MANUAL)*

**Objective:** bring up the Room the whole-system evidence is gathered in.

**Scope:** [§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint) and [deploy-ada-ecu-walkthrough.md §4.4](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#44-deploy) — **New Deployment**, an existing Device from the dropdown, the original blueprint rather than its snapshot. §5.6 notes the full blueprint is one deployment like any other and the two-Room budget still applies, so chain A's Room is released first.

**Acceptance:** the deployment exists and its Room id is recorded in `plans/doc/phase5-ivi-run.md`; `5.5.10.13` confirms the node phases. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `5.5.10.11` and chain A's teardown at `17.5.10.9`. **Commit:** `[5.5.10.12] docs: record the full blueprint deployment`

### [ ] `5.5.10.13` — Poll the full blueprint's nodes to `Running` *(car-sky)*

**Objective:** record that all five nodes came up, and resolve every `nodeKey`.

**Scope:** the AI row of [§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint) and of [deploy-ada-ecu-walkthrough.md §7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#7-work-division-between-ai-and-human) — poll `GET /api/v1/deployments/{roomId}/nodes` until every node reads `Running` with restart count 0, recording each `name`. Four container nodes and one Skycraft node, the latter slowest.

**Acceptance:** 5/5 nodes `Running` with restart count 0 and every `nodeKey` recorded in `plans/doc/phase5-ivi-run.md`.

**Dependencies:** after `5.5.10.12`. **Commit:** `[5.5.10.13] docs: record the full blueprint Room reaching Running`

### [ ] `16.5.10.14` — Install and launch the APK on the full-blueprint guest *(car-sky)*

**Objective:** get the Phase 5 build running on the IVI node of the full topology.

**Scope:** [§4.6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#46-install-the-apk) then the launch half of [§4.7](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#47-open-the-screen-and-launch-the-app), both AI rows in §5, over the endpoint `16.5.8.3` proved. The Room is new, so the guest is new: `adb install -r` runs again. §4.1's ordering holds — the node must be `Running` first.

Again this is the **APK**, not a node image field; the container nodes' images were set at `5.5.10.10` and pulled at deploy.

**Acceptance:** `Success` from the install, the package path from `pm path`, and the app started, recorded in `plans/doc/phase5-ivi-run.md`.

**Dependencies:** after `5.5.10.13`. **Commit:** `[16.5.10.14] docs: record the APK install and launch on the full-blueprint guest`

### [ ] `16.5.10.15` — Open the device and its Screen widget on the full-blueprint run *(USER-MANUAL)*

**Objective:** make the guest's display visible for the whole-system run, with recording armed.

**Scope:** the widget half of [§4.7](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#47-open-the-screen-and-launch-the-app) — Devices, **Connect**, the Screen widget with its parts set from the Part Prefix, plus Log and ADB widgets. Set the **Recorder Part** before the run: this is the run the recorded evidence comes from, and [§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence) needs it armed in advance.

**Acceptance:** the AAOS screen streaming live with the recorder armed, recorded in `plans/doc/phase5-ivi-run.md`. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `16.5.10.14`. **Commit:** `[16.5.10.15] docs: record the device widgets opened on the full-blueprint guest`

### [ ] `19.5.10.16` — Read the whole chain's logs end to end *(car-sky)*

**Objective:** produce the text evidence that the warning the IVI renders originated in a bench scenario and travelled the full relay, not a mock beside it.

**Scope:** authenticate to CarSky, then read every surface the chain exposes. The producer-side checks are [deploy-ada-ecu-walkthrough.md §5.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#51-check-1--the-relayed-message-is-received-and-raises-its-event) and [§5.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#52-check-2--both-vehicles-are-in-the-track-store), unchanged in the full composition per [§5.6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#56-the-full-blueprint-route) — but with the relayed traffic now originating in the real V2X ECU driven by the bench scenario, so `STATION_ID`, `OBJECT_ID` and the distance profile come from that scenario rather than node env. §5.6 also states that **check 3 has no sink log** in this composition: the Android node runs no container, so consumer-side evidence moves to the guest's own log and belongs to [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging). Read `adb logcat -s IVI_V2X` for that half. The ADA node's `[CAP]` line is the only wire evidence for the outgoing warning here, which is why its `NET_RAW` was required at `5.5.10.10`.

Save every node log before the Room is torn down — the log route returns nothing afterwards ([deploy-ada-ecu-walkthrough.md §5.7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#57-tear-down)).

**Acceptance:** [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance) proofs 1–3 on the guest side, and [deploy-ada-ecu-walkthrough.md §8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance)'s producer-side checks, with `cSource=v2x_relayed` on every rendered warning — all as excerpts in `plans/doc/phase5-ivi-run.md`, correlated across nodes by timestamp.

**Dependencies:** after `16.5.10.14`. **Commit:** `[19.5.10.16] docs: record the whole-system log evidence across the relay chain`

### [ ] `19.5.10.17` — Confirm the God View on live relayed data, capture it, tear down *(USER-MANUAL)*

**Objective:** see the warning view come up from data that travelled the whole chain, and capture it.

**Scope:** the visual-judgement and recording rows of [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human), against [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) V4's link 4 and captured per [§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence). The `[? UNKNOWN SOURCE]` marker remains a blocking defect. What differs from chain A is only the source of the data, so nothing about the expected drawing changes.

Tear down per [§4.12](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#412-tear-down) after `19.5.10.16` has saved every log, releasing the Room slot.

**Acceptance:** a recording showing the God View drawn from live relayed data, with `19.5.10.16`'s excerpts backing it in text; recorded in `plans/doc/phase5-ivi-run.md`; the deployment deleted and the blueprint kept. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `19.5.10.16` and `16.5.10.15`. **Commit:** `[19.5.10.17] docs: record the whole-system God View evidence and teardown`

---

## Execution order & parallelism

Dependencies are real (files, Gradle project graph, contract artifacts, deployed Rooms) — not default assumptions. **Four subtasks depend on nothing and can all be opened at once:** `4.5.1.1` (code), `5.5.8.2` (the probe Room, USER), `16.5.7.1` (CI lane), and `17.5.5.8` once `4.5.1.4` has landed.

```
Lane A  foundation:   4.5.1.1 ─► 4.5.1.2 ─► 4.5.1.3 ─► 4.5.1.4 ─► 4.5.1.5
                                                          │  (gate for lanes B–F)
Lane B  :serializer:  4.5.2.1 ─► 4.5.2.2 ─► 4.5.2.3
Lane C  :observer:    4.5.3.1 ─► 4.5.3.2 ─► 4.5.3.3 ─► 4.5.3.4 ─► 4.5.3.5
                      (4.5.3.1 needs 4.5.2.1 only — starts before lane B ends)
Lane D  app logic:    { 4.5.4.1 ∥ 4.5.4.2 ∥ 4.5.4.3 } ─► 17.5.4.4 ─► 16.5.4.5
                      (4.5.4.1/4.5.4.2 need 4.5.3.1; 4.5.4.3 needs only 4.5.1.4)
Lane E  app shell:    18.5.5.1 ─► 4.5.5.2 ─► 4.5.5.3 ─► 16.5.5.4 ─► 16.5.5.5 ─► 17.5.5.6
                      (4.5.5.3 also needs lane D through 17.5.4.4; 17.5.5.6 also needs 16.5.4.5)
                      17.5.5.7 (after 4.5.5.3, ∥ 17.5.5.6)     17.5.5.8 ∥ 17.5.5.9 (∥ everything, after 4.5.1.4)
Lane F  test equip:   4.5.6.1 ─► 4.5.6.2 ─► 4.5.6.3 ─► 4.5.6.4 ─► 4.5.6.5 ─► 5.5.6.6
                      (needs only 4.5.1.4 — fully parallel with lanes B–E)
                      4.5.6.7 dev injector (after 16.5.5.5 + 4.5.4.2)
Lane G  CI:           16.5.7.1 (no dependencies, ∥ everything)   4.5.7.2 (after 4.5.6.4)   5.5.7.3 (after 5.5.6.6)
Lane H  unknowns:     5.5.8.2 (USER) ─► 16.5.8.3 (USER) ─► 16.5.8.4
                      (fully parallel with lanes A–G; 16.5.8.4 also needs 16.5.5.5)
Lane I  evidence:     5.5.9.1 (USER) ─► 16.5.9.2 (USER) ─► 17.5.9.3 (USER) ─► 4.5.9.4 (USER)
                      (5.5.9.1 needs 5.5.7.3 + 5.5.8.2; 16.5.9.2 needs 17.5.5.6 + 16.5.8.3)
Lane J  mini run:     5.5.10.1 (USER) ─► 4.5.10.2 (USER) ─► 6.5.10.3 ─► 5.5.10.4 (USER)
                        ─► 5.5.10.5 ─► 16.5.10.6 ─► 16.5.10.7 (USER) ─► 18.5.10.8 ─► 17.5.10.9 (USER)
                      (4.5.10.2 needs 5.5.7.3's pushed image; 16.5.10.6 needs 17.5.5.6 + 16.5.8.3.
                       Lane I's scenario rungs run against this Room before 17.5.10.9 tears it down)
Lane K  full run:     5.5.10.10 (USER) ─► 5.5.10.11 ─► 5.5.10.12 (USER) ─► 5.5.10.13
                        ─► 16.5.10.14 ─► 16.5.10.15 (USER) ─► 19.5.10.16 ─► 19.5.10.17 (USER)
                      (5.5.10.10 needs every node's real image pushed by its own phase;
                       5.5.10.12 needs the Room slot lane J released at 17.5.10.9)
```

- **Parallel:** lanes B, D-partial, F, G and H against each other once `4.5.1.4` has landed; `4.5.4.1 ∥ 4.5.4.2 ∥ 4.5.4.3` inside lane D; `17.5.5.7 ∥ 17.5.5.6`; `17.5.5.8` against everything. Lane H is parallel with **all** code work by design — it must not wait for it. Lane J's first three subtasks are canvas and read-back work that needs no APK, so they run parallel to lanes A–F.
- **Sequential:** every arrow above. Lane A is strictly sequential and gates everything (a Gradle module graph cannot be built out of order). Lanes I, J and K are strictly sequential — each step's evidence depends on the previous step's Room state.
- **Lane K follows lane J on the Room budget, not on logic.** Only two Rooms run at once and the comms track holds one, so the full blueprint deploys after the mini-blueprint's Room is released. Lane K also waits on every node's real image, which its own phase publishes — nothing in Phase 5 builds them.
- **Lane I interleaves with lane J** rather than following it: lane I decides which scenario file is on the wire, lane J stands the Room up, installs the app and reads the result.
- **Spawn order:** `4.5.1.1` and `16.5.7.1` go to subagents together, since neither waits on anything. Lane H's USER steps go to the user at the same time — `5.5.8.2` waits on nothing. Lane I's USER steps and lane J's chain open once `5.5.7.3` has pushed a verified image. The *car-sky* subtasks in lanes J and K are spawned to [[car-sky]] at the Room events they attach to.

### Critical path

The shortest ordered set that closes all five acceptance boxes:

`4.5.1.1 → 4.5.1.2 → 4.5.1.3 → 4.5.1.4 → 4.5.2.1 → 4.5.2.2 → 4.5.3.1 → 4.5.3.2 → 4.5.3.3 → 4.5.4.1 → 4.5.4.2 → 4.5.4.3 → 17.5.4.4 → 16.5.4.5 → 18.5.5.1 → 4.5.5.2 → 4.5.5.3 → 16.5.5.4 → 16.5.5.5 → 17.5.5.6 → (lane F through 5.5.6.6) → 5.5.7.3 → 5.5.9.1 → 16.5.9.2 → 17.5.9.3 → 4.5.9.4`

with **lane H (`5.5.8.2` → `16.5.8.3`) running alongside**, unblocked from the start — it does not sit on the critical path but it decides whether the path's last steps are in-Room or on an emulator.

**Chain A of group 5.10 is the tail of this path** — `5.5.10.1` through `17.5.10.9` are what turn a built app into the four proofs of [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance). Its first three subtasks are canvas and read-back work that needs no APK, so they can be done while lanes A–F are still running; only `16.5.10.6` onward waits on `17.5.5.6`.

**Chain B is not on the path to the five boxes.** Every box is closed by chain A against the mock-ADA feed, which is what "mock-driven" means for this phase. Chain B waits on the other tracks' real images and produces the whole-system evidence Phase 6's convergence run builds on — valuable, and not a Phase 5 gate.

**Droppable without failing a box, in this order if time runs short:** `4.5.1.5` (ProGuard rules — release build is not an acceptance target), `17.5.5.7` (config-driven scale — the library default is correct), `4.5.6.7` (dev injector — only needed if the ADB/UI route is awkward), `4.5.2.3` and `4.5.3.5` (extra test depth, not extra behaviour), `state-stream.json` inside `4.5.6.4` (the periodic `state` message is optional on the producer side and no box depends on it).

## Acceptance traceability

Every Phase 5 acceptance criterion in [milestone1.md](milestone1.md#phase-5--ivi-hmi-mock-driven-r16-r17--display-track-parallel-from-the-start) maps to at least one subtask.

| Milestone Phase 5 box | Closed by |
|---|---|
| The HMI runs on the AAOS node with the R16 layout; button/app areas switch the Display area | `16.5.5.4` · `16.5.5.5` (the launcher entry the APK lacks today) · `17.5.5.6` · `16.5.4.5` · deployed by `5.5.10.1`–`5.5.10.4`, confirmed `Running` by `5.5.10.5`, installed and launched by `16.5.10.6`, observed by `16.5.10.7` and `17.5.10.9` · `16.5.8.3` · `16.5.9.2` |
| **(Dev)** A mock R4 warning brings the warning view up with ego, B and ghost C at the composed positions | `4.5.2.2` · `4.5.3.3` · `4.5.4.2` · `17.5.4.4` · `16.5.4.5` · `17.5.5.6` · simulator `4.5.6.3`/`4.5.6.4` (`approach.json`) · fed to the node by `4.5.10.2` · dev path `4.5.6.7` (I3) · scenario `17.5.9.3` (I4), read by `18.5.10.8` and seen by `17.5.10.9` |
| Ghost C renders from `v2x_relayed` data only; the 2D drawing is delivered | **`17.5.4.4`** (the §9.2 snapshot wiring that arms the committed guard — without it the guard silently disables) · **`17.5.5.9`** (the guard itself under test) · `17.5.5.6` · `17.5.5.7` · `17.5.5.8` · `4.5.3.3` (`cSource=` on every `[RX]`) · `degrade.json` guard-trip step in `4.5.6.4` · observed by `17.5.9.3` and `4.5.9.4`; `cSource=v2x_relayed` on every warning evidenced in text by `18.5.10.8` and on screen by `17.5.10.9`, and again from live relayed data by `19.5.10.16`/`19.5.10.17` |
| A newer message with an unknown `warningType` degrades gracefully | `4.5.1.4` (the committed `R4AdditiveVersionTest` relocated and still green) · `4.5.2.2` (decode preserves the value, D4) · `4.5.4.3` (`WarningClassifier` generic presentation) · `4.5.6.4` (`degrade.json`) · observed by `4.5.9.4` |
| Optional paths, only if built | **Not built in M1** — declared, not attempted. `SceneViewWarning3D.kt`'s location is designated by HLD §3.1 and nothing depends on it; multi-process wake-on-warning stays reachable because `4.5.5.2` chose the foreground service (D5). Recorded as "not built" in `plans/doc/phase5-ivi-run.md` by `4.5.9.4`. |


**Beyond the five boxes.** Chain B (`5.5.10.10`–`19.5.10.17`) closes no Phase 5 box on its own — all five are met against the mock-ADA feed. It proves the same IVI behaviour inside the full topology with every node on its real image, and its record is what Phase 6's convergence run starts from.
## Open items carried, not decided (no Phase 5 subtask may close them by assumption)

| # | Item | Owner / closes at |
|---|---|---|
| 1 | **ADB reach to the Skycraft guest** (REST VM-shell route known-502) | `16.5.8.3`, which waits on nothing and is scheduled first. Negative ⇒ group 5.9 degrades to AAOS-emulator evidence |
| 2 | **AAOS guest Android version** vs `minSdk 29`, and the `automotive` feature | `16.5.8.3`, same probe |
| 3 | Simulator `Dockerfile` at `r4-simulator/`, not the node-folder root | HLD-flagged deviation with rationale; self-containment preserved. Revisit only if `IVI_ECU/` gains a root image |
| 4 | Coroutines version skew between `:observer` and what AndroidX resolves in `:app` | Mitigated by the catalog (`4.5.1.1`); a skew shows as a runtime `NoSuchMethodError`, not a build failure — watch for it at `16.5.9.2` |
| 5 | Deployment budget: 2 concurrent Rooms | `4.5.9.4` tears the Phase 5 Room down; coordinate with the comms track before `5.5.8.2` deploys |
| 6 | MTU headroom (Phase 0 open item O3) | Non-issue for this hop — an R4 warning is ~450 B against a 2048 B buffer — but still formally open |
| 7 | **AAOS boot-to-listener time sets the bench `start_delay_s` floor** — a number no other phase can produce ([m1-run-timing-and-event-triggering.md §9 item 5](../requirements/m1-run-timing-and-event-triggering.md)). Measured at `16.5.9.2`; consumed by the bench key `start_delay_s`, which §6.1 defines and no phase currently schedules ([phase1_tasks.md § Open items item 10](phase1_tasks.md#open-items--flags-no-phase-1-subtask-may-silently-close-them)). **No startup handshake is coming** — §4.2 rules readiness as R5's Deployment-Viewer check plus that delay, so nothing in this phase should be designed around a barrier the topology has no reverse path for | `16.5.9.2`, then **user** / Phase 6 |

## Deliberately not in this phase

- **3D (`SceneViewWarning3D`) and multi-process wake-on-warning** — optional, not committed M1 deliverables (D11). A location is designated; no subtask attempts them.
- **`WarningBannerOverlay` mounted in the Display Area** — forbidden by the standing user decision of 2026-07-26 (D11). The God-View canvas must render unobstructed.
- **Robolectric, a coverage threshold, and LeakCanary** — none has a basis in R4/R16/R17 acceptance; the boxes are behavioural, and D2's plain-JVM split is what removes the need for Robolectric (HLD §6, §9.1).
- **Runtime JSON-Schema validation on the device** — the typed decode already enforces required fields and types; the schema is enforced in the round-trip tests on both sides (HLD §5.1).
- **Real ADA data.** Phase 5 is mock-driven by definition; the simulator honours the real ADA node's env var *names* so Phase 6 is an image swap with no node-config edit (HLD §8).

Carried by [phase5_tasks.md](phase5_tasks.md) and deliberately not carried here, beyond the four items above:

- **A 3D `SceneViewWarning3D` stub subtask.** R17 makes 3D optional and six days do not justify a stub whose only acceptance is that it does not crash. Its file location stays designated (HLD §3.1) so the optional path remains open.
- **"Five consecutive socket errors → terminate and emit a service error".** Replaced by bounded back-off that never gives up (`4.5.3.4`); a listener that stops trying mid-run is worse for a recorded demo than one that keeps rebinding.
- **A `--cycles`/`--interval-ms` sender CLI.** Repetition and cadence are scenario data (`loop`, `defaultRateHz`), not flags — D9's "scenarios are data, not code".
- **A 2-second service-bind latency criterion.** Neither R4, R16 nor R17 states a latency requirement, and nothing in the acceptance boxes turns on it.

---

*Created 2026-08-02 by project-planner from the Phase 5 HLD (`85387b5`), its four research notes, and [milestone1.md § Phase 5](milestone1.md#phase-5--ivi-hmi-mock-driven-r16-r17--display-track-parallel-from-the-start); gap-checked the same day against [phase5_tasks.md](phase5_tasks.md). In-Room verification is decomposed from the walkthroughs per stage 2 of [walkthrough-driven-delivery.md](../.claude/rules/walkthrough-driven-delivery.md). 10 task groups, 61 subtasks: 38 agent-implemented, 8 car-sky, 15 user-manual. Nothing blocked. Nothing started except `16.5.7.1`.*
