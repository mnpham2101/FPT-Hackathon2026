# Phase 5 — IVI HMI (R4, R16, R17): Full Task Breakdown

> **Authority & context:**
>
> - **Phase content:** [milestone1.md § Phase 5](milestone1.md#phase-5--ivi-hmi-mock-driven-r16-r17--display-track-parallel-from-the-start) — its five acceptance criteria are the phase output.
> - **Design:** [ivi-ecu-hld.md](../IVI_ECU/doc/ivi-ecu-hld.md) — the node's sole design authority — with [phase5-ivi-components.puml](../IVI_ECU/doc/phase5-ivi-components.puml) and [phase5-ivi-callflow.puml](../IVI_ECU/doc/phase5-ivi-callflow.puml). Every target path below is cited from its **[§4](../IVI_ECU/doc/ivi-ecu-hld.md#4-folder-structure)** folder map; component responsibilities **[§6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components)**, seams **[§8](../IVI_ECU/doc/ivi-ecu-hld.md#8-interfaces-ports-and-the-layer-rule)**, the R4 input contract **§10**, CI **[§11](../IVI_ECU/doc/ivi-ecu-hld.md#11-tech-stack-build-and-ci)**, test configurations and log shapes **[§12](../IVI_ECU/doc/ivi-ecu-hld.md#12-test-strategy)**, decisions **D1–D13** ([§13](../IVI_ECU/doc/ivi-ecu-hld.md#13-design-decisions)). Deployment steps come from the bring-up procedure below, not from the design.
> - **Research notes:** [phase5-r4-simulator.md](../IVI_ECU/doc/research_notes/phase5-r4-simulator.md) · [phase5-r4-parsing.md](../IVI_ECU/doc/research_notes/phase5-r4-parsing.md) — non-authoritative; the HLD wins on conflict.
> - **Requirements:** [m1-cooperative-awareness.md §2](../requirements/m1-cooperative-awareness.md) R4, R16, R17 (plus R5, R6, R18, R19 where this phase touches them) and [m1-run-timing-and-event-triggering.md §7](../requirements/m1-run-timing-and-event-triggering.md) R22, whose K7 this phase's system test reads — referenced by number, never restated.
> - **Deploy facts:** [node-ivi-ecu.md](../requirements/car-sky-guide/node-ivi-ecu.md) · [carsky-4-node-blueprint.md](../requirements/car-sky-guide/carsky-4-node-blueprint.md) · [deploy-walkthrough-netcheck.md](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md).
> - **Bring-up procedure:** [deploy-ivi-hmi-walkthrough.md](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md) — **authoritative** for the APK's build, retrieval from CI, blueprint deploy, `adb install`, launch and verification. Every subtask that installs, launches, observes or reads logs from the IVI app cites the section governing that step instead of restating it. Its [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human) decides which of those steps an agent can perform and which need a person, and is what the *Human* label below follows.
> - **Rules:** [task-planning-conventions.md](../.claude/rules/task-planning-conventions.md) (`X.Y.Z.W`; subtask discipline restated once in § Subtask discipline) · [node-code-layout.md](../.claude/rules/node-code-layout.md).
>
> **Task ID legend:** `X.5.Z.W` — X = requirement served · 5 = this phase · Z = task group · W = subtask. IDs are stable; never renumber.
>
> **What counts as pre-existing:** the files listed in § Components already in the tree. Everything else is written by the subtask that names it, under § The existing-file rule.

## Phase 5 overview

**Objective.** The IVI renders the R17 God View — ego, B, and ghost C — from R4 messages alone, inside the R16 layout, on a launchable APK; and an R4 simulator plus a 3-node mini-blueprint produce the traffic and the in-Room evidence that closes the phase's acceptance criteria.

**Input (must exist before start):**

- R4 frozen in Phase 0: [contracts/r4-ada-ivi.schema.json](../contracts/r4-ada-ivi.schema.json), the four R3/R4 samples under [contracts/samples/](../contracts/samples/), and the committed Kotlin binding + `R4RoundTripTest` / `R4AdditiveVersionTest`.
- The IVI node's HLD, [ivi-ecu-hld.md](../IVI_ECU/doc/ivi-ecu-hld.md), and its two research notes.
- `IVI_ECU/` as a single-module Gradle project (`:app`) carrying the contract layer (`model/`) and the drawing layer (`ui/view/`), AGP 8.13 / Kotlin 2.2.20 / Compose BOM 2024.09.03 / `minSdk 29`, `targetSdk 33`, `compileSdk 34`.
- CarSky access with the baseline blueprint `baseline_phase1` ([carsky-4-node-blueprint.md § The blueprints on CarSky](../requirements/car-sky-guide/carsky-4-node-blueprint.md#8-the-blueprints-on-carsky)), the `AAOS` artifact (`x9oqgIwzTp1m26SWIQqJt` / `xSU_Q7YJZUxxUgDr4Ugcp`, `0.0.1`, `aarch64`), and `registry.hackathon-2.carsky.io/m1-netcheck:latest` already pushed.
- GitHub secret `CARSKY_ZOT_API_KEY` and the reusable [verify-arm64-image](../.github/actions/verify-arm64-image) action.

**The component set and every target path are [HLD §4](../IVI_ECU/doc/ivi-ecu-hld.md#4-folder-structure) and [§6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components).** **Briefs below cite them; they do not restate them.** A brief names the file it changes and what it must end up doing, and the reader gets the component's responsibility, its inputs and its outputs from the design's tables rather than from a sentence in each subtask.

**Output (phase acceptance — the five criteria of [milestone1.md § Phase 5](milestone1.md#phase-5--ivi-hmi-mock-driven-r16-r17--display-track-parallel-from-the-start)):**

- [ ] The HMI runs on the AAOS node with the R16 layout; button/app areas switch what the Display area shows.
- [ ] **(Dev)** A mock R4 warning brings the warning view up showing ego, B, and ghost C at the composed positions.
- [ ] Ghost C renders from `v2x_relayed` data only; the 2D drawing is delivered (R17 — 3D stays optional).
- [ ] A newer message with an unknown `warningType` degrades gracefully (R4 additive-version test).
- [ ] Optional paths, only if built: an ADA message wakes the separate warning app; 3D renders through the view seam.

Per-subtask traceability to these five criteria: § Acceptance traceability.

**Suggested branch:** `feat/phase5-ivi-hmi`. Creating, checking out and pushing it is the user's call.

**If time runs short**, § Critical path is the shortest ordered set of subtasks that closes the five criteria. Everything outside it is quality work that can be dropped or deferred without failing a criterion; § Critical path lists the drop order.

### Execution labels

Every subtask carries one. The walkthroughs' own AI/Human work-division tables decide which.

| Label | Who does it |
|---|---|
| *agent* | A spawned implementation subagent. The default for code, tests and CI. |
| *car-sky* | The [[car-sky]] agent: authenticated REST calls, `adb` commands, log reads. The planner keeps the ID and the done-tracking. |
| *Human* | A person, at the Nydus canvas or the Devices panel. No agent performs these. The evidence-record commit is made by the orchestrating session once the person confirms. |
| *split* | A car-sky command whose result a person judges. Both halves are named in the brief. |

### Components already in the tree

These paths exist under `IVI_ECU/app/`. A file's presence is not evidence that it implements [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components); § The three file tiers governs what a subtask may assume about each. Everything else the HLD names is written by the subtask that names it.

| Path under `IVI_ECU/` | Touched by |
|---|---|
| `app/src/main/java/…/model/R4Message.kt`, `R3Snapshot.kt`, `SceneGeometry.kt` | relocated by `4.5.1.4` |
| `app/src/test/java/…/model/R4RoundTripTest.kt`, `R4AdditiveVersionTest.kt` | relocated by `4.5.1.4`, must pass unchanged |
| `app/src/test/resources/contracts/samples/*.json` (four) | relocated by `4.5.1.4` |
| `contracts/r4-ada-ivi.schema.json`, `r3-tracked-object.schema.json` | untouched — their manifest targets do not move |
| `app/src/main/java/…/ui/DisplayMode.kt` | unchanged |
| `app/src/main/res/xml/network_security_config.xml` | unchanged — governs HTTP stacks only, not a raw `DatagramSocket` (D3) |
| `app/src/main/java/…/ui/MainViewModel.kt` | extended by `16.5.4.5` — see § The three file tiers |
| `app/src/main/java/…/ui/screen/MainScreen.kt` | **rebuilt to the design** by `16.5.5.6` |
| `app/src/main/java/…/ui/view/IviWarningViewSeam.kt` | **rebuilt to the design** by `17.5.5.4` |
| `app/src/main/java/…/ui/view/SceneCoordinateMapper.kt` | **rebuilt to the design** by `17.5.5.3` |
| `app/src/main/java/…/ui/view/CanvasWarningView.kt` | **rebuilt to the design** by `17.5.5.4`, which absorbs the configurable scale |
| `app/src/main/java/…/ui/view/WarningBannerOverlay.kt` | **rebuilt to the design** by `17.5.5.5`; stays unmounted (D11) |
| `app/src/main/AndroidManifest.xml`, `app/build.gradle.kts`, `app/proguard-rules.pro` | extended across groups 5.1, 5.4, 5.5 |
| `.github/workflows/phase5-ci.yml` | shipped by `16.5.7.1`; extended by `16.5.7.4` |

`gradle/libs.versions.toml`, the `:contract` / `:serializer` / `:observer` / `:r4-simulator` modules, `MainActivity.kt` and `IviApplication.kt` are **absent** — groups 5.1–5.6 create them.

Comments inside several of these files cite task IDs that carry a different meaning in this plan; § Open items item 9 records the collision and its owner.

### The three file tiers

The `ui/` files above do not implement [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components). Group 5.5 builds the renderer, the mapper, the seam, the banner and the screen to the design rather than editing and testing what is there. A subtask whose objective is "add a test to the committed X" ratifies whatever X currently does.

Three tiers, and a subtask must know which one it is in:

| Tier | Files | What a subtask may assume |
|---|---|---|
| **Authority** | `contracts/*.schema.json`, the four frozen samples, `R4RoundTripTest`, `R4AdditiveVersionTest` | Correct and frozen. Relocate byte-identically; **never edit**. A divergence here is reported, not fixed in place |
| **Load-bearing** | `model/R4Message.kt`, `R3Snapshot.kt`, `SceneGeometry.kt` | Contract-tested by the two tests above, so trusted **to the extent those tests cover**. `SceneGeometry.vehicleCSnapshot` must exist and be nullable (D12); if it does not, that is a design-blocking finding for `17.5.4.4`, not a quiet addition |
| **Scaffolding** | everything under `ui/`, plus `AndroidManifest.xml` | Nothing. Read it for what it reveals about the guest and the toolchain, then build to the HLD. Keep an existing behaviour only where the HLD independently calls for it |

### The existing-file rule

A subtask that names a file it must produce checks whether that file exists before writing it, and takes one of three routes:

- **Absent** — create it to the brief.
- **Present, and in the Scaffolding tier** — build it to the brief. Existing code is input, not a constraint; anything the brief does not call for is removed rather than carried.
- **Present, and in the Authority or Load-bearing tier** — verify against the brief. A match makes the subtask a verification that commits its status line alone; a divergence is **reported, not silently corrected**, because a frozen artifact is re-frozen across every consumer.

Relocations are `git mv` and must show as pure renames under `git diff -M --stat`; a moved test passes unchanged or the move is wrong.

### Subtask discipline

Per [task-planning-conventions.md § Subtask discipline](../.claude/rules/task-planning-conventions.md#subtask-discipline-non-negotiable): **single objective · no out-of-scope code · exactly one atomic commit with the stated message · build passes · unit tests pass · the brief is self-contained.** Implementation subagents inherit this as their definition of done.

Two standing constraints every `IVI_ECU/` subtask inherits:

- **No hardcoded tunables** (CLAUDE.md principle 5): ports, buffer sizes, timeouts, cadences and scales come from `BuildConfig` + launch override (D10) or from the simulator's env/scenario file — never a literal in a class.
- **No module declares its own repositories.** `settings.gradle.kts` sets `RepositoriesMode.FAIL_ON_PROJECT_REPOS`; a module `repositories { }` block fails the build (D8).
- **No `android.util.Log` call on a unit-tested path in `:app`.** The stubbed Android jar throws `RuntimeException("Stub!")` from every `Log` method, so a logging line inside tested logic fails the test for a reason unrelated to the logic. D2 already keeps `Log` out of `:serializer`/`:observer`; in `:app`, log through `AndroidR4Logger` (`18.5.5.1`) and inject a recording logger in tests. `testOptions { unitTests { isReturnDefaultValues = true } }` is the fallback, not the first move — it silences the symptom for the whole module.

**Status tracking:** as execution proceeds each subtask gains a `**Status:**` line appended in that subtask's own atomic commit, recording done/blocked plus verification evidence. A subtask without a status line is not started.

### Build & verification commands

All Gradle commands run from `IVI_ECU/` (`gradlew.bat` on the Windows dev host, `./gradlew` on CI/Linux).

| Target | Command |
|---|---|
| One module's tests | `./gradlew :contract:test` · `:serializer:test` · `:observer:test` · `:r4-simulator:test` |
| App unit tests | `./gradlew :app:testDebugUnitTest` |
| **Full suite (post-5.1)** | `./gradlew :contract:test :serializer:test :observer:test :r4-simulator:test :app:testDebugUnitTest` |
| APK | `./gradlew assembleDebug` → `app/build/outputs/apk/debug/app-debug.apk` |
| Lint | `./gradlew lint` |
| Contract integrity gate | from the repo root: `python contracts/check_sync.py` → exit 0 |
| Simulator image, local tag `m1-r4-sim:latest` | `docker buildx build --platform linux/arm64 --provenance=false --sbom=false -f IVI_ECU/r4-simulator/Dockerfile -t m1-r4-sim:latest IVI_ECU/` |
| Simulator image, registry tag `registry.hackathon-2.carsky.io/m1-r4-sim:latest` | pushed by `5.5.7.3`; the blueprint `image` field names this tag |

Until subtask `4.5.1.4` lands, the only valid test command is `./gradlew :app:testDebugUnitTest` — the other modules do not exist yet.

---

## The IVI address and port

The IVI's UDP port is **`47300`** and its address is **`10.99.0.13`**, frozen by R6 and the blueprint topology. Any other value reaching a class, a scenario file or a node config is a defect, whatever its source.

---

## Test plan — levels and coverage

[HLD §12](../IVI_ECU/doc/ivi-ecu-hld.md#12-test-strategy) fixes the strategy: a plain-JVM unit layer below, and above it **two Room configurations differing in one component: the one that realizes the `ADA-ECU` interface**. Group 5.9 runs the isolated configuration against the simulator; group 5.10 runs the full one against the real producer. **Expected output is identical in both**, so a difference between the runs is a finding about the other node, never about this one.

### The four levels

`I1` to `I4` below are this plan's labels for the four test levels, used nowhere else and naming no component. The walkthrough's own `V1`–`V5` rungs are separate and are cited by their own numbers.

| Level | What runs | What it proves | Where | Delivered by |
|---|---|---|---|---|
| **I1 — decode API** | `:contract` + `:serializer`, no socket, no UI | Contract conformance: every frozen sample, every malformed case | `:contract:test`, `:serializer:test` | `4.5.1.4`, `4.5.2.2`, `4.5.2.3` |
| **I2 — real socket, no device** | `:observer` over a loopback `DatagramSocket` | The receive loop, buffer discipline, back-pressure, rebind | `:observer:test` | `4.5.3.2`–`4.5.3.5` |
| **I3 — app logic and the dev injector** | `:app` logic on a JVM; the injector on a guest with no network | View-model state, mode switching, the armed guard, the whole UI path | `:app:testDebugUnitTest`; the injector on a device | `4.5.4.*`, `17.5.5.3`, `17.5.5.4`, `4.5.6.7`, `17.5.9.18` |
| **I4 — real UDP from a peer** | The whole node in a Room | R6's ADA→IVI hop, R16 and R17 acceptance, the recorded evidence | Groups 5.9 and 5.10 | — |

**I1 and I2 are the levels that must be automated**, and `4.5.7.2` is what puts them in CI. I3's injector exists because the ADB route to the guest is unproven, so it keeps UI work unblocked. I4 produces the evidence.

### What the unit layer must cover

Every row is a test this plan commits to, traced to what makes it necessary. A row with no test is an untested claim.

| Claim under test | Test | Subtask | Source |
|---|---|---|---|
| Every frozen sample round-trips through the binding | `R4RoundTripTest` | `4.5.1.4` (relocated, must pass unchanged) | R4 acceptance |
| A newer `schemaVersion` + unknown `warningType` + unknown field decodes and preserves the wire value | `R4AdditiveVersionTest` | `4.5.1.4` (relocated) | R4 acceptance; D4 |
| Every decode-failure row maps to its typed result, and nothing throws | `R4DeserializerTest` | `4.5.2.2` | parsing note §2 |
| A dirty backing array, a reused buffer, a BOM and out-of-bounds input all behave | `BufferSlicingTest` | `4.5.2.3` | D3 rows 1, 4 |
| A reused packet's length is reset before every receive | `JdkDatagramSourceTest` | `4.5.3.2` | D3 row 2 |
| N datagrams in → N events out; one bad message does not stop the next | `R4SocketObserverTest` | `4.5.3.3` | HLD §6 |
| Rebind back-off is bounded, resets on success, and reaches `Error` without giving up | `RetryBackoffTest` | `4.5.3.4` | D5 |
| The whole receive path works over a real socket with no device | `LoopbackSocketTest` | `4.5.3.5` | HLD §12 |
| Defaults, launch overrides and out-of-range fallbacks resolve in one place | `IviRuntimeConfigTest` | `4.5.4.1` | D10 |
| Warnings, last-value-wins `state`, drops and injection route identically | `R4RepositoryTest` | `4.5.4.2` | HLD §6 |
| An unknown `warningType` degrades to generic; unknown risk fails safe to highest | `WarningClassifierTest` | `4.5.4.3` | D4 |
| **The composed scene carries the R3 snapshot, so the guard is armed** | `WarningViewModelTest` guard-armed case | `17.5.4.4` | **D12** |
| A `low` neither raises the warning nor dismisses it, and restarts the countdown | `WarningViewModelTest` D13 cases | `17.5.4.4` | **D13** |
| A warning forces the view; the previous mode restores unless the user overrode it | `MainViewModelTest` | `16.5.4.5` | R16 acceptance |
| The projection anchors, clamps, scales and survives a `null` C | `SceneCoordinateMapperTest` | `17.5.5.3` | R17 |
| **Ghost C is drawn only for `v2x_relayed`; anything else marks and logs ERROR** | `CanvasWarningViewTest` | `17.5.5.4` | **R19, D12** |
| The simulator's env and args config parses, and a missing value fails loudly | `SimConfigTest` | `4.5.6.1` | D9 |
| A scenario file is rejected loudly rather than defaulted silently | `ScenarioLoaderTest` | `4.5.6.2` | D9 |
| Every non-raw payload validates through `R4Json` before sending; junk fields survive | `MessageBuilderTest` | `4.5.6.3` | D9 |
| Different scenario files produce observably different streams | `ScenariosDifferTest` | `4.5.6.4` | D9 |
| `downgrade.json` emits `medium` then three `low`s, differing from `approach.json` | `ScenariosDifferTest` downgrade case | `4.5.6.8` | D13 |

**Two of these are R4's own acceptance** — the round-trip and additive-version tests — and **two are the R19 claim in code**: the guard-armed composition (`17.5.4.4`) and the guard itself (`17.5.5.4`). Losing either R19 test leaves a guard that is present, passing and disabled, which is the exact failure D12 exists to prevent. **Neither is droppable.**

### The in-Room observables

[HLD §12](../IVI_ECU/doc/ivi-ecu-hld.md#12-test-strategy) names each observable and the component that produces it; [walkthrough §6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance) names the four proofs they add up to. This is the checklist both Room groups work against.

| Observable | Produced by | Rung | Owned by | Proof |
|---|---|---|---|---|
| `[LINK] state=bound port=47300`, and the status bar's link indicator | `R4SocketObserver` → `AndroidR4Logger`; `MainScreen` | V1 | `16.5.9.10`, `16.5.9.11` | precondition |
| `[DROP] reason=malformed …` on a non-JSON probe datagram, app still running | `R4Deserializer` returning a result instead of throwing | V2 | `4.5.9.17` | the network hop |
| The Display Area switches to the Warning View from an injected sample, no network | `DevInjectorReceiver` → `R4Repository` → the view path | V3 | `17.5.9.18` | the UI path |
| `[RX] type=warning bytes=… from=…` per datagram | `JdkDatagramSource` → `R4SocketObserver` → `AndroidR4Logger` | V4 | `18.5.9.12` | §6 proof 1 |
| `warningType=`, `risk=`, `cSource=`, `cPos=` on that line, off the parsed message | `R4Deserializer` | V4 | `18.5.9.12` | §6 proof 2 |
| `[UI] mode=WarningView cause=warning` — and not `cause=user` | `R4Repository` → `WarningViewModel` → `MainViewModel` | V4 | `18.5.9.12` | §6 proof 3 |
| The God View: ego and B solid, ghost C dashed on a risk-coloured glow, nothing else | `MainScreen` → `IviWarningViewSeam` → `CanvasWarningView` | V4 | `17.5.9.13` | §6 proof 4 |
| A `null` `vehicleC` first step renders without C, without crash or placeholder | `CanvasWarningView` | V4 | `17.5.9.13` | R17 |
| `cause=user` on a tap; `cause=timeout` back to Idle with the previous mode restored | `MainViewModel`; `WarningViewModel`'s timeout | V4 | `16.5.9.11`, `17.5.9.13` | R16 acceptance |
| A generic warning on an unknown `warningType`, wire value preserved in the log | `WarningClassifier`; `R4Deserializer` | V5 | `4.5.9.15`, `17.5.9.16` | acceptance criterion 4 |
| `[? UNKNOWN SOURCE]` and an ERROR line on an `own_sensor` message — **the trip is the pass** | the provenance guard | V5 | `4.5.9.15`, `17.5.9.16` | R19 |
| `[DROP] reason=malformed …` and the next valid warning still rendering | `R4Deserializer`; `R4SocketObserver` | V5 | `4.5.9.15`, `17.5.9.16` | loop survival |
| A `medium` followed by a `low` leaves the Display Area on Warning, risk colour updated | `WarningViewModel` (D13) | — | `17.5.4.4` (the rules) · `4.5.6.8` (the file that steps it on demand) | D13 |
| The run's first `[UI] mode=WarningView cause=warning` follows the startup `[UI] mode=HomeView` by ≥ 8.0 s | `MainViewModel` under D13, against the **real** producer | — | `22.5.10.10` | R22 K7 |

### Test data

One scenario file per purpose, all data: `approach.json` drives V4 and carries the null-C, risk-progression and timeout cases; `degrade.json` drives V5's three rows; `state-stream.json` exercises R4's optional `state` message, which no acceptance criterion depends on (D11) — all three from `4.5.6.4`. `downgrade.json` (`4.5.6.8`) steps `medium` then `low` on demand, which is D13's in-Room half. Payloads come from the frozen samples, never from a literal (D9).

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

**Acceptance:** `./gradlew :app:testDebugUnitTest` still green; `./gradlew projects` succeeds; `libs.versions.toml` contains every version listed above and no module declares a version literal that the catalog also declares. Record the build host's `java -version` output and its `ANDROID_HOME` value in the commit body — [walkthrough §6.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) item 8 makes a JDK and an Android SDK on the build host an unconfirmed point, and every `./gradlew` acceptance in this phase rests on it. A missing JDK or SDK is a finding for § Open items item 11, not a reason to improvise a toolchain install.

**Dependencies:** none — the first subtask of the phase, and the gate for the whole `IVI_ECU/` tree. **Commit:** `[4.5.1.1] chore: add IVI Gradle version catalog and root plugin aliases`

### [ ] `4.5.1.2` — Move `:app` onto the catalog *(agent)*

**Objective:** make `app/build.gradle.kts` resolve every plugin and dependency through the catalog (HLD D8).

**Scope:**

- Rewrite `app/build.gradle.kts`'s `plugins { }` and `dependencies { }` blocks to `alias(libs.plugins.…)` / `libs.…` references. Behaviour must not change.
- Leave the Hilt and KSP declarations alone — `4.5.1.6` removes them.
- Do not add the new `buildConfigField`s here — `4.5.4.1` owns those.

**Acceptance:** `./gradlew :app:testDebugUnitTest` and `./gradlew assembleDebug` both green; `app/build.gradle.kts` declares no version literal the catalog also declares.

**Dependencies:** after `4.5.1.1`. **Commit:** `[4.5.1.2] refactor: put :app on the version catalog`

### [ ] `4.5.1.6` — Remove the unused Hilt and KSP stack *(agent)*

**Objective:** delete the dependency-injection framework the design does not use (HLD D7).

**Scope:**

- **Delete these four lines.** They declare **Hilt** — Google's dependency-injection framework for Android, built on Dagger, which generates object wiring from annotations at compile time. D7 wires the app by hand in `IviGraph` (`4.5.5.7`) instead, so nothing needs it:

  | Line | File |
  |---|---|
  | `id("com.google.dagger.hilt.android") version "2.58" apply false` | `IVI_ECU/build.gradle.kts` |
  | `id("com.google.dagger.hilt.android")` | `app/build.gradle.kts` |
  | `implementation("com.google.dagger:hilt-android:2.58")` | `app/build.gradle.kts` |
  | `ksp("com.google.dagger:hilt-android-compiler:2.58")` | `app/build.gradle.kts` |

  **Grep for `dagger` and `Hilt` under `IVI_ECU/` before committing.** The removal is safe only while no `@HiltAndroidApp` class and no `@Inject` site exists; a hit means something depends on it and the removal has to be reported, not forced.
- KSP is the annotation processor Hilt drove. If the grep shows nothing else uses it, remove `id("com.google.devtools.ksp")` from `app/build.gradle.kts` too and say so in the commit body.

**Acceptance:** `./gradlew :app:testDebugUnitTest` and `./gradlew assembleDebug` both green; a repo-wide grep for `hilt`/`dagger` under `IVI_ECU/` returns nothing.

**Dependencies:** after `4.5.1.2`. **Commit:** `[4.5.1.6] refactor: remove the unused Hilt and KSP stack`

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

**Objective:** move the committed contract layer verbatim into `:contract` and keep the contract-integrity gate green in the same commit (HLD D1, D6).

**Scope — a relocation, not a rewrite. No source line may change.**

- `git mv` these three files from `IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/model/` to `IVI_ECU/contract/src/main/kotlin/com/hackathon/v2x/ivi/model/`, **byte-identical**, package `com.hackathon.v2x.ivi.model` unchanged: `R4Message.kt`, `R3Snapshot.kt`, `SceneGeometry.kt`.
- `git mv` both committed tests from `IVI_ECU/app/src/test/java/com/hackathon/v2x/ivi/model/` to `IVI_ECU/contract/src/test/kotlin/com/hackathon/v2x/ivi/model/`, **byte-identical**: `R4RoundTripTest.kt`, `R4AdditiveVersionTest.kt`. They must pass **unchanged** — do not touch their `getResourceAsStream("/contracts/samples/…")` calls.
- `git mv` the four sample JSONs from `IVI_ECU/app/src/test/resources/contracts/samples/` to `IVI_ECU/contract/src/main/resources/contracts/samples/` (**main**, not test — D6: the simulator and the dev injector read them off the same classpath): `r3-tracked-object.json`, `r4-warning.json`, `r4-state.json`, `r4-unknown-warning.json`. Because the resource root still contains `contracts/samples/`, the moved tests resolve unchanged.
- `app/build.gradle.kts`: add `implementation(project(":contract"))`.
- **Same commit, out-of-folder edit:** in [contracts/sync-manifest.json](../contracts/sync-manifest.json), repoint the four `IVI_ECU/app/src/test/resources/contracts/samples/<f>.json` targets to `IVI_ECU/contract/src/main/resources/contracts/samples/<f>.json`. They sit under the `contracts/samples/r3-tracked-object.json`, `r4-warning.json`, `r4-state.json` and `r4-unknown-warning.json` source entries. **This edit cannot be deferred** — the moment the files move, `check_sync.py` reports four missing targets and `contracts-gate` goes red.
- `IVI_ECU/contracts/r3-tracked-object.schema.json` and `r4-ada-ivi.schema.json` are **not** touched — their manifest targets are unchanged.

**Acceptance:** `./gradlew :contract:test` green with `R4RoundTripTest` and `R4AdditiveVersionTest` passing (5 test methods total, no source change — verify with `git diff -M --stat` showing pure renames); `./gradlew :app:testDebugUnitTest` and `./gradlew assembleDebug` green; `python contracts/check_sync.py` exits 0.

**Dependencies:** after `4.5.1.3`. **Commit:** `[4.5.1.4] refactor: relocate R4/R3 models, tests and samples into :contract`

### [ ] `4.5.1.5` — ProGuard keep rules for the relocated serializable models *(agent)*

**Objective:** keep the release build's kotlinx-serialization reflection working after the relocation (HLD §4).

**Scope:** add to `IVI_ECU/app/proguard-rules.pro` the standard kotlinx-serialization keep set scoped to `com.hackathon.v2x.ivi.model.**` — keep the generated `$$serializer` fields/classes, the `Companion.serializer()` methods, and `@kotlinx.serialization.Serializable` annotated classes' `INSTANCE`/`Companion`. Nothing else; do not change `isMinifyEnabled`.

**Acceptance:** `./gradlew :app:assembleRelease` succeeds (unsigned output is fine); the rules name only the `com.hackathon.v2x.ivi.model` package.

**Dependencies:** after `4.5.1.4`. Parallel with groups 5.2–5.6. **Commit:** `[4.5.1.5] chore: add ProGuard keep rules for the relocated serializable models`

---

## Task Group 5.2 — `:serializer` — datagram bytes to a typed R4 result (serves R4; test level I1)

> HLD **D3** (de-framing is buffer slicing, not header parsing) and **§6** (the decode component), with its seam in **§8**. Pure Kotlin/JVM: **never logs, never throws across the receive loop** — it returns a result and the observer decides what to log.

### [ ] `4.5.2.1` — Module `:serializer` + the decode contract types *(agent)*

**Objective:** declare the module and the types of HLD §6, with no implementation behind them.

**Scope:**

- `settings.gradle.kts`: `include(":serializer")`. New `IVI_ECU/serializer/build.gradle.kts`: `alias(libs.plugins.kotlin.jvm)`; `api(project(":contract"))`; `testImplementation(libs.junit)`; toolchain 17; no Android, no repositories block.
- New `IVI_ECU/serializer/src/main/kotlin/com/hackathon/v2x/ivi/serializer/R4Decoder.kt` containing the declarations below, which realize the decode-component row of [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components) and its seam entry in [HLD §8](../IVI_ECU/doc/ivi-ecu-hld.md#8-interfaces-ports-and-the-layer-rule):

  ```kotlin
  interface R4Decoder { fun decode(buffer: ByteArray, offset: Int, length: Int): R4DecodeResult }
  sealed interface R4DecodeResult {
      data class Decoded(val message: R4Message, val schemaVersionAhead: Boolean) : R4DecodeResult
      data class Failed(val reason: DecodeFailure, val detail: String, val preview: String) : R4DecodeResult
  }
  enum class DecodeFailure { EMPTY, UNKNOWN_MESSAGE_TYPE, MALFORMED }
  ```

**Acceptance:** `./gradlew :serializer:build` green; the file's three declarations match the block above field-for-field, that block being the specification.

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
  7. Convenience overloads `decode(bytes: ByteArray)` and `decode(text: String)` for I1 tests (HLD §6).
  - **`isLenient` stays `false`** and no runtime JSON-Schema validation is added (HLD §6). `R4Json` is consumed as-is from `:contract`; do not construct a second `Json`.
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

## Task Group 5.3 — `:observer` — socket, receive loop, back-off, event flow (serves R4, R6; test level I2)

> HLD **D5** (the loop is plain-JVM code; the service is only its lifecycle host) and **§6**. Plain Kotlin/JVM so I2 runs in CI with no device and **no Robolectric**. `:observer` never imports `android.util.Log` — it logs through the `R4Logger` seam.

### [ ] `4.5.3.1` — Module `:observer` + seams, config and event types *(agent)*

**Objective:** declare the module and its four value/seam files, with no loop yet.

**Scope:**

- `settings.gradle.kts`: `include(":observer")`. New `IVI_ECU/observer/build.gradle.kts`: `alias(libs.plugins.kotlin.jvm)`; `api(project(":serializer"))`; `api(libs.kotlinx.coroutines.core)`; `testImplementation(libs.junit)` + `testImplementation(libs.kotlinx.coroutines.test)`; toolchain 17; no Android.
- `observer/src/main/kotlin/com/hackathon/v2x/ivi/observer/R4DatagramSource.kt` — the seam: `fun bind()`, `fun receive(): Received`, `fun close()`, and `data class Received(val buffer: ByteArray, val offset: Int, val length: Int)`. Interface only; `4.5.3.2` implements it.
- `.../R4Event.kt` — the declarations below, which realize the event types of [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components): `sealed interface R4Event { data class Message(val message: R4Message, val receivedAtMs: Long, val bytes: Int); data class Dropped(val reason: DecodeFailure, val detail: String, val bytes: Int) }`, plus `sealed interface R4LinkState { Bound(port) | Rebinding | Error(detail) }` (the bottom status bar of `16.5.5.6` binds to this).
- `.../R4ObserverConfig.kt` — `data class R4ObserverConfig(val port: Int, val bufferBytes: Int, val flowBufferEvents: Int, val retryInitialMs: Long, val retryMaxMs: Long)`. **No default values and no literals** — every field is supplied by `IviRuntimeConfig` (`4.5.4.1`).
- `.../R4Logger.kt` — `fun interface R4Logger { fun log(level: R4LogLevel, line: String) }` plus `object NoopR4Logger : R4Logger` and an `enum class R4LogLevel { INFO, WARN, ERROR }`. `:app` supplies the real implementation in `18.5.5.1`.

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
- Unit test `observer/src/test/kotlin/.../JdkDatagramSourceTest.kt`: bind on port `0` (ephemeral), send two datagrams from a local `DatagramSocket` — a long one then a **shorter** one — and assert the second `Received.length` equals the second payload's length — without the `setLength` call it reports the first payload's length, and this test is what holds the rule.

**Acceptance:** `./gradlew :observer:test` green; the `setLength` call is textually inside the `receive()` body, before the `socket.receive(...)` call.

**Dependencies:** after `4.5.3.1`. **Commit:** `[4.5.3.2] feat: add JdkDatagramSource with per-receive packet length reset`

### [ ] `4.5.3.3` — `R4SocketObserver` — receive loop, truncation check, event flow *(agent)*

**Objective:** implement the loop of HLD §6 so N datagrams in produce N events out and one bad message never stops the next good one.

**Scope:** `observer/src/main/kotlin/com/hackathon/v2x/ivi/observer/R4SocketObserver.kt` with the signature below, which realizes the receive-loop row of [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components) and its seam entry in [HLD §8](../IVI_ECU/doc/ivi-ecu-hld.md#8-interfaces-ports-and-the-layer-rule):

```kotlin
class R4SocketObserver(
    private val config: R4ObserverConfig,
    private val decoder: R4Decoder,
    private val sourceFactory: () -> R4DatagramSource,
    private val logger: R4Logger,
) { val events: SharedFlow<R4Event>; val linkState: StateFlow<R4LinkState>; fun start(scope: CoroutineScope): Job; fun stop() }
```

- `events` is a `MutableSharedFlow` with `extraBufferCapacity = config.flowBufferEvents` and `BufferOverflow.DROP_OLDEST`; the loop uses **`tryEmit`, never a suspending emit** (D5 back-pressure) — a slow collector must never stall the socket.
- `start(scope)` launches on `Dispatchers.IO`; on bind success `linkState = Bound(port)` and log `[LINK] state=bound port=<p>` (§12).
- Per datagram: if `length == config.bufferBytes`, log a truncation-suspect WARN and **still attempt the decode** (D3 row 3). Call `decoder.decode(buffer, offset, length)`; `Decoded` → log the `[RX]` line of §12 and `tryEmit(R4Event.Message(msg, System.currentTimeMillis(), length))`; `Failed` → log `[DROP] reason=… bytes=… preview="…"` and `tryEmit(R4Event.Dropped(...))`. Log `schemaVersionAhead` **once** per observer lifetime, not per message.
- The `[RX]` line for a warning carries `warningType=`, `risk=`, `cSource=` (the R3 snapshot's `source`) and `cPos=` — `cSource` on every rendered warning is what backs the R19 claim in text (§12).
- **No accumulate-and-split logic** anywhere (D3 row 5) — UDP preserves message boundaries.
- `stop()` closes the source and cancels the job; `linkState` is left at its last value.
- Test `observer/src/test/kotlin/.../R4SocketObserverTest.kt` with a **fake** `R4DatagramSource` (no socket): 5 valid datagrams in → 5 `R4Event.Message` out in order; one malformed among them → 1 `Dropped` and the following good message still arrives; a fake whose `receive()` throws once → the loop does not die (leaves the back-off to `4.5.3.4`); assert nothing was logged through a real Android type by injecting a recording `R4Logger`.

**Acceptance:** `./gradlew :observer:test` green; `tryEmit` is the only emit call in the loop; the `[RX]`/`[DROP]`/`[LINK]` shapes match HLD §12 exactly.

**Dependencies:** after `4.5.3.2`. **Commit:** `[4.5.3.3] feat: implement the R4 receive loop with typed events and truncation checks`

### [ ] `4.5.3.4` — Rebind back-off + its test *(agent)*

**Objective:** make a socket error a recoverable, bounded-back-off rebind instead of a dead listener.

**Scope:**

- Extend `R4SocketObserver` (no new production file): on a bind or receive error → log at ERROR, `linkState = Rebinding`, close the source, `delay(d)`, recreate through `sourceFactory()`, retry; `d` starts at `config.retryInitialMs`, doubles to a ceiling of `config.retryMaxMs`, and **resets to `retryInitialMs` on the next successful bind**. `linkState` returns to `Bound`.
- **Make `R4LinkState.Error` reachable.** `4.5.3.1` declares it, and a declared state that nothing ever emits is a defect, not a spare: once the back-off has saturated at `retryMaxMs` (the link has been down long enough that a transient blip is ruled out), set `linkState = Error(detail)` and **keep retrying** — R19 requires one continuous run, so the observer retries indefinitely. A successful bind returns it to `Bound`. The status bar of `16.5.5.6` renders this state.
- `observer/src/test/kotlin/.../RetryBackoffTest.kt` using `kotlinx-coroutines-test` virtual time: a source factory that fails the first three binds then succeeds → assert the observed delays are `initial, 2×initial, 4×initial` clamped at `retryMaxMs`, that `linkState` passes `Rebinding → Bound`, and that after a later failure the delay restarts at `retryInitialMs` (reset-on-success). One further case: a factory that keeps failing past the ceiling → `linkState` reaches `Error`, retries continue, and a later success returns it to `Bound`.

**Acceptance:** `./gradlew :observer:test` green; no `Thread.sleep` anywhere — the test runs on virtual time.

**Dependencies:** after `4.5.3.3`. **Commit:** `[4.5.3.4] feat: add bounded exponential rebind back-off to the R4 observer`

### [ ] `4.5.3.5` — Loopback socket test (I2) *(agent)*

**Objective:** prove the observer end to end over a **real** `DatagramSocket`, with no device and no Robolectric (HLD §12, level I2).

**Scope:** `observer/src/test/kotlin/com/hackathon/v2x/ivi/observer/LoopbackSocketTest.kt`:

- Bind a real `JdkDatagramSource` on an ephemeral port on `127.0.0.1`, wire it into a real `R4SocketObserver` with a real `R4Deserializer`, collect `events`.
- Send the frozen `r4-warning.json` bytes from a plain `DatagramSocket` → one `R4Event.Message` whose `message` is an `R4WarningEvent` with `objectSnapshot.source == "v2x_relayed"`.
- Send `r4-unknown-warning.json` → `Message` with `warningType == "slippery_road"` (D4 end to end).
- Send `not-json` bytes → one `Dropped(MALFORMED, …)`, and a following valid datagram still produces a `Message` — the loop survived.
- Bounded waits (a few seconds) with a clear timeout failure message; the test must not hang CI.

**Acceptance:** `./gradlew :observer:test` green on a machine with no Android SDK; no Robolectric dependency is added anywhere.

**Dependencies:** after `4.5.3.4`. **Commit:** `[4.5.3.5] test: add the I2 loopback socket test for the R4 observer`

---

## Task Group 5.4 — `:app` data & logic layer (serves R4, R16, R17)

> HLD **§4** `app/src/main/java/com/hackathon/v2x/ivi/{config,data,warning,ui}` and **§3** MVC. Everything in this group is plain Kotlin testable by `:app:testDebugUnitTest` with no device. Group 5.5 then hosts it in an Activity and a service.

### [ ] `4.5.4.1` — `IviRuntimeConfig` + the new `BuildConfig` fields (D10) *(agent)*

**Objective:** make every Phase 5 tunable a compile-time default that a launch-time intent extra can override, in one place.

**Scope:**

- `app/build.gradle.kts` `defaultConfig`: add the `buildConfigField`s of HLD D10 beside the committed `WARNING_TIMEOUT_MS`: `R4_UDP_PORT = 47300`, the value [D10](../IVI_ECU/doc/ivi-ecu-design-decisions.md) fixes and the blueprint's `ethernet` pin uses; `R4_SOCKET_BUFFER_BYTES = 2048`, `R4_FLOW_BUFFER_EVENTS = 8`, `R4_RETRY_INITIAL_MS = 500L`, `R4_RETRY_MAX_MS = 5000L`, `SCENE_SCALE_M_PER_PX = 0.5f`.
- New `app/src/main/java/com/hackathon/v2x/ivi/config/IviRuntimeConfig.kt`: `data class IviRuntimeConfig(port, socketBufferBytes, flowBufferEvents, retryInitialMs, retryMaxMs, warningTimeoutMs, sceneScaleMetersPerPixel)` plus `fun resolve(intent: Intent?): IviRuntimeConfig` — reads the `BuildConfig` defaults and applies the D10 overrides when present: `--ei r4_port`, `--el warning_timeout_ms`, `--ef scene_scale`. Invalid or out-of-range extras (port outside 1–65535, non-positive timeout/scale) are ignored in favour of the default. Add `fun toObserverConfig(): R4ObserverConfig`. **This is the only class that reads `BuildConfig`** — every other class receives resolved values.
- Test `app/src/test/java/com/hackathon/v2x/ivi/config/IviRuntimeConfigTest.kt`: a null intent yields the defaults; each override key is applied; an out-of-range value falls back; `toObserverConfig()` carries the resolved port and buffer sizes. (Use a plain fake for the extras lookup if `android.content.Intent` is unavailable in a unit test — extract the extras read into an internal `resolve(overrides: Map<String, Any>)` that the intent overload delegates to, and test that.)

**Acceptance:** `./gradlew :app:testDebugUnitTest` green; a grep of `app/src/main` shows `BuildConfig.` referenced only inside `IviRuntimeConfig.kt`; port default is `47300`.

**Dependencies:** after `4.5.3.1` (needs `R4ObserverConfig`). **Commit:** `[4.5.4.1] feat: add IviRuntimeConfig with BuildConfig defaults and launch overrides`

### [ ] `4.5.4.2` — `R4Repository` — the single routing point *(agent)*

**Objective:** collect the observer's events once, on the application scope, and expose them as the app's data layer (HLD §3 Data layer, §6).

**Scope:** `app/src/main/java/com/hackathon/v2x/ivi/data/R4Repository.kt`:

- Constructor takes the `R4SocketObserver` (or, better for testing, its `events` + `linkState` flows) and a `CoroutineScope`.
- Exposes `warnings: SharedFlow<R4WarningEvent>` (replay 1, so a late collector sees the current warning), `lastState: StateFlow<R4StateMessage?>` with **last-value-wins by `seq`** (a message with a `seq` lower than or equal to the stored one is discarded), `linkState: StateFlow<R4LinkState>` (passthrough), and `droppedCount: StateFlow<Int>`.
- Exposes `fun inject(event: R4Event)` — the single injection target the dev injector (I3, `4.5.6.7`) uses, so I3 exercises exactly the same downstream path as a real datagram (HLD §6).
- **The repository stores and routes; it never decides what a warning means and never formats anything** (HLD §3).
- Test `app/src/test/java/com/hackathon/v2x/ivi/data/R4RepositoryTest.kt`: a `Message` carrying a warning appears on `warnings` and does not touch `lastState`; a `Message` carrying a state updates `lastState`; a state with `seq = 41` after `seq = 42` is discarded (LVW); a `Dropped` increments `droppedCount` and emits no warning; `inject()` produces the identical observable result as an event arriving from the flow.

**Acceptance:** `./gradlew :app:testDebugUnitTest` green; `R4Repository.kt` contains no `String.format`, no `warningType` comparison and no UI type.

**Dependencies:** after `4.5.3.1`. Parallel with `4.5.4.1`. **Commit:** `[4.5.4.2] feat: add R4Repository routing warnings, state and link status`

### [ ] `4.5.4.3` — `WarningClassifier` — presentation at the UI edge (D4) *(agent)*

**Objective:** map *known* `warningType` values to their presentation and everything else to a generic warning presentation, without ever rewriting the wire value.

**Scope:** `app/src/main/java/com/hackathon/v2x/ivi/warning/WarningClassifier.kt`:

- Declare `data class WarningPresentation(val title: String, val known: Boolean, val urgency: Urgency)` (or equivalent) in this same file — HLD §4 designates no separate file for it.
- `fun classify(warningType: String): WarningPresentation`: `R4Contract`'s `nlos_obstruction` → the M1 NLOS presentation with `known = true`; **any other value → a generic presentation with `known = false`, and the wire value is carried through unchanged for the log** (D4 — the parser preserved it; this is where classification happens, and nothing here writes `"unknown"` back into the message).
- `fun normaliseRisk(riskState: String): Urgency`: `low`/`medium`/`high` case-insensitively; **an unknown `riskState` maps to the highest urgency**. `17.5.5.4` applies the same fail-safe in the renderer's risk colouring ([HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components)), so the two cannot drift.
- Test `app/src/test/java/com/hackathon/v2x/ivi/warning/WarningClassifierTest.kt`: `nlos_obstruction` → `known = true`; `slippery_road` (the frozen additive fixture's value) → `known = false` and the value is still readable; `"HIGH"`, `"high"`, `"" `, `"catastrophic"` all resolve, with the last two at highest urgency.

**Acceptance:** `./gradlew :app:testDebugUnitTest` green; no branch in this file mutates an `R4WarningEvent`.

**Dependencies:** after `4.5.1.4`. Parallel with `4.5.4.1`/`4.5.4.2`. **Commit:** `[4.5.4.3] feat: add WarningClassifier mapping unknown warning types to a generic presentation`

### [ ] `17.5.4.4` — `WarningViewModel` + `WarningUiState`, **including the R19 snapshot wiring** *(agent)*

**Objective:** turn warnings into `Idle ↔ Active` UI state with an auto-dismiss timeout, and **compose the scene so the renderer's provenance guard is armed** ([HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components)).

**Scope — two files:**

- `app/src/main/java/com/hackathon/v2x/ivi/ui/WarningUiState.kt`: `sealed interface WarningUiState { data object Idle; data class Active(val scene: SceneGeometry, val riskState: String, val presentation: WarningPresentation) }`.
- `app/src/main/java/com/hackathon/v2x/ivi/ui/WarningViewModel.kt`:
  - Collects `repository.warnings`. **The lifecycle is [D13](../IVI_ECU/doc/ivi-ecu-design-decisions.md#d13--active-risk-raises-the-warning-only-silence-lowers-it)'s three rules and nothing else moves the mode automatically:**

    | Event | Effect |
    |---|---|
    | a warning with `riskState` `medium` or `high` | Idle → Active; already Active, the scene refreshes. The countdown restarts |
    | a warning with `riskState` `low` | the scene and risk colour update, the countdown restarts, **the mode does not change** |
    | `warningTimeoutMs` with no warning of any level | Active → Idle |

  - **A `low` neither raises nor dismisses.** Arriving while the Display Area shows Home it leaves Home showing, which is what holds the normal screen up across R22's pre-warning window; arriving while Active it restarts the countdown like any other warning, so the warning survives the gap to the next cycle. Dismissal is the countdown alone.
  - A new warning of any level resets the timer rather than stacking timers.
  - **The composition step — the wiring the R19 guard depends on.** `SceneGeometry` arriving in `warning.geometry` has `vehicleCSnapshot = null`, and `CanvasWarningView` treats a `null` snapshot as **trusted** — so passing `warning.geometry` straight through silently disables the R19 source guard. The view-model must build the scene as `warning.geometry.copy(vehicleCSnapshot = warning.objectSnapshot)` (an internal function of this file; HLD §4 designates no separate composer file). `riskState` and `presentation` come from `WarningClassifier`.
  - Holds no drawing code and no socket (HLD §3 UI logic).
- Test `app/src/test/java/com/hackathon/v2x/ivi/ui/WarningViewModelTest.kt`:
  - Idle initially; a `medium` warning → `Active`; no further warning for `warningTimeoutMs` → `Idle`; a second warning inside the window extends rather than double-fires.
  - **The three D13 cases, one test each:** a `low` arriving from `Idle` leaves the state `Idle`; a `low` arriving while `Active` leaves the state `Active` with the scene and risk level updated; a `low` restarts the countdown, so `Active` survives to `warningTimeoutMs` **after** the `low` rather than after the preceding `medium`. A `high` from `Idle` raises like a `medium`.
  - **Guard-armed test — the R19 test, and name it so:** decode the frozen `/contracts/samples/r4-warning.json`, feed it in, and assert `(state as Active).scene.vehicleCSnapshot?.source == "v2x_relayed"` — i.e. the snapshot is **not null**. Then feed a warning whose `object.source` is `own_sensor` and assert the composed scene carries that snapshot verbatim, so the renderer's guard can trip. A `null` `vehicleCSnapshot` in either case fails the test.
  - Use virtual time (`kotlinx-coroutines-test`) for the timeout; add `testImplementation(libs.kotlinx.coroutines.test)` to `app/build.gradle.kts` if absent.

**Acceptance:** `./gradlew :app:testDebugUnitTest` green including the named guard-armed test and the three D13 cases; the timeout value is read from the injected config, never a literal.

**`WARNING_TIMEOUT_MS` stays 10000 and is bound to the producer's cycle** (D13, [D10](../IVI_ECU/doc/ivi-ecu-design-decisions.md)): it must exceed the interval from a cycle's clearing `low` to the next cycle's first active-risk message, ≈ 8.5 s under R22's 10.0 s cycle. Lowering it drops the warning for part of every cycle. It is externalized, so a retune is a config edit — but it is not free.

**Dependencies:** after `4.5.4.2` and `4.5.4.3`. **Commit:** `[17.5.4.4] feat: add WarningViewModel with timeout and R19 snapshot composition`

### [ ] `16.5.4.5` — Extend `MainViewModel` — wake-on-warning, restore, user override *(agent)*

**Objective:** make a warning force the Display Area to the Warning View and restore the previous view when it clears, without trapping the user (HLD §6).

**Scope:** edit the committed `app/src/main/java/com/hackathon/v2x/ivi/ui/MainViewModel.kt`:

- Add `previousMode` capture: on entering `WarningView` because of a warning, remember the mode that was showing.
- Add `fun onWarningState(state: WarningUiState)` (or an injected flow collect): `Active` → force `WarningView`; `Idle` → restore `previousMode`, **unless** the user deliberately navigated away during the warning, in which case the user's chosen mode stands.
- Add the user-override flag, and relax `setMode` to honour it: a deliberate user navigation while `WarningView` is active is recorded as an override and obeyed, so `Idle` does not pull the user back. Keep the safety intent — the warning still *comes up* unconditionally.
- Test `app/src/test/java/com/hackathon/v2x/ivi/ui/MainViewModelTest.kt`: from `HomeView`, a warning forces `WarningView`; on `Idle` the mode returns to `HomeView`; if the user selects `SettingsView` during the warning, `Idle` leaves `SettingsView` in place; a second warning still forces `WarningView` again and clears the override.

**Acceptance:** `./gradlew :app:testDebugUnitTest` green with all four cases; `DisplayMode.kt` is unchanged.

**Dependencies:** after `17.5.4.4`. **Commit:** `[16.5.4.5] feat: add wake-on-warning, previous-mode restore and user override to MainViewModel`

---

## Task Group 5.5 — `:app` UI and shell — the launchable APK (serves R16, R17, R18 — § Open items item 8 carries R18's scope)

> **This group builds its components to the design; it does not extend the scaffolding of the same name** (§ The three file tiers). The group that gives the APK its renderer, its screen, its service and its process entry. [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components) fixes each component's one responsibility and [§4](../IVI_ECU/doc/ivi-ecu-hld.md#4-folder-structure) its path. **Nothing here mounts `WarningBannerOverlay`** (D11).

### [ ] `18.5.5.1` — `AndroidR4Logger` — the `IVI_V2X` evidence bridge *(agent)*

**Objective:** implement the `R4Logger` seam over `android.util.Log` on one tag, so the whole run's text evidence is a single `adb logcat -s IVI_V2X`.

**Scope — `app/src/main/java/…/service/AndroidR4Logger.kt`:** maps `R4LogLevel.{INFO,WARN,ERROR}` to `Log.i/w/e` on the tag `IVI_V2X`, emitting the caller's line verbatim. The `[LINK]`, `[RX]` and `[DROP]` shapes are composed in `:observer`; the `[UI]` line in `MainViewModel`. Add whatever narrow entry point those callers need and nothing more. **This is the only file in the node that bridges to `android.util.Log`** ([HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components)).

**Acceptance:** `./gradlew assembleDebug` and `:app:testDebugUnitTest` green; a grep of `serializer/` and `observer/` for `android.util` returns nothing.

**Dependencies:** after `4.5.3.1` and `4.5.1.4`. Parallel with group 5.4. **Commit:** `[18.5.5.1] feat: add AndroidR4Logger bridging the logger seam to the IVI_V2X tag`

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

**Dependencies:** after `4.5.1.4`. **Fully parallel** with the rest of groups 5.2–5.6. **Commit:** `[17.5.5.3] feat: add SceneCoordinateMapper with the R17 oblique projection`

### [ ] `17.5.5.4` — `IviWarningViewSeam` and `CanvasWarningView` with the provenance guard *(agent)*

**Objective:** draw the R17 God View behind the seam that makes an optional 3D renderer swappable, and make the R19 claim mechanical.

**Scope — two files under `app/src/main/java/…/ui/view/`, plus one test:**

- `IviWarningViewSeam.kt` — the render seam `Render(scene, riskState)` ([HLD §8](../IVI_ECU/doc/ivi-ecu-hld.md#8-interfaces-ports-and-the-layer-rule)). An interface only; `MainScreen` sees nothing else.
- `CanvasWarningView.kt` — realizes the seam over Compose Canvas, drawing exactly what [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components) fixes: a dark canvas, a lane-marked road converging toward the top, three car silhouettes in one lane with ego nearest; ego and B solid; **ghost C dashed and translucent on a pulsing ground glow coloured by `riskState`**; a `null` `vehicleC` drawn without C, with no placeholder and no crash. Geometry comes from `SceneCoordinateMapper`; the scale comes from the runtime config, not from a literal (D10).
  - **The scene alone is the warning.** No legend, no distance labels, no text overlay, no banner, and no `[V2X]` badge — those belong to R17's annotated explanatory figure, which the IVI never renders. See § Open items item 1 for the one document that says otherwise.
  - **The provenance guard.** Ghost C is drawn **only** when its snapshot `source` is `v2x_relayed`. Any other value draws the yellow `[? UNKNOWN SOURCE]` marker instead and logs at ERROR through `AndroidR4Logger`. A `null` snapshot is treated as trusted — the guard fails open, which is exactly why `17.5.4.4` must fill it (D12).
  - Put the guard decision and its ERROR message in **`internal` top-level functions** in this file, called by the composable. A `Canvas`-drawn marker is not in the Compose semantics tree, so a composition test cannot reach a decision that stays inline; extracting it is what makes the R19 guard assertable at all.
  - Risk colouring maps an unrecognised `riskState` to the highest-urgency colour — the same fail-safe as `WarningClassifier.normaliseRisk`, so the two cannot drift.
- Test `app/src/test/java/…/ui/view/CanvasWarningViewTest.kt` — `null` snapshot → ghost C is drawn; `v2x_relayed` → ghost C is drawn; `own_sensor` → the `[? UNKNOWN SOURCE]` marker is drawn instead of ghost C; the error message names both the offending source and `v2x_relayed`; an unrecognised `riskState` maps to the high-urgency colour.

**Acceptance:** `./gradlew assembleDebug` and `:app:testDebugUnitTest` green with all five guard cases; a grep of the file finds no legend, distance-label or badge drawing.

**Dependencies:** after `17.5.5.3` and `4.5.4.3`. **Commit:** `[17.5.5.4] feat: add the Canvas God View behind the render seam with the provenance guard`

### [ ] `17.5.5.5` — `WarningBannerOverlay` — built, mounted nowhere (D11) *(agent)*

**Objective:** deliver the banner component the design names, and leave it unmounted so the canvas renders unobstructed.

**Scope:** `app/src/main/java/…/ui/view/WarningBannerOverlay.kt` — a composable taking `riskState` and rendering the risk banner. **It is referenced by no screen.** D11 is a standing user decision: the God-View canvas must render unobstructed, which is also R17's own requirement. A subtask that mounts it has broken a user decision, not made an improvement.

**Acceptance:** `./gradlew assembleDebug` green; a repo-wide grep finds no reference to `WarningBannerOverlay` outside its own file. A `@Preview` is the only sanctioned caller.

**Dependencies:** after `4.5.1.4`. Parallel with everything in this group. **Commit:** `[17.5.5.5] feat: add the unmounted WarningBannerOverlay component`

### [ ] `16.5.5.6` — `MainScreen` — the R16 layout, the view slot and the status bar *(agent)*

**Objective:** build the R16 layout as [ivi-ecu.svg](../requirements/ivi-ecu.svg) fixes it, hosting the Warning View through the seam.

**Scope — `app/src/main/java/…/ui/screen/MainScreen.kt`:**

- The layout of [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components): a central **Display Area**, the Home / Apps / Settings areas around it, mode labels, and a bottom status bar. Tapping a side area changes `MainViewModel.currentMode` and the Display Area follows — R16's acceptance clause.
- The Display Area's Warning branch renders through `IviWarningViewSeam` when the warning state is `Active`, and shows neutral idle content when it is `Idle`. **`MainScreen` sees only `WarningUiState` and the seam** — never `CanvasWarningView` concretely ([HLD §3](../IVI_ECU/doc/ivi-ecu-hld.md#3-the-component-architecture) UI-layer rule).
- It collects the warning state and feeds it to `MainViewModel.onWarningState`, so a message raises the view.
- The status bar renders the live `R4LinkState`: bound with the port, rebinding, or error — the visual half of the `[LINK]` evidence. No hardcoded standby string.
- `@Preview` functions for the idle and active states, at the committed preview size of `MainScreen.kt` (`widthDp = 1280, heightDp = 720`). `6.5.9.3` reads the guest's real display size off the live node and records it in the run doc; a preview size is an authoring convenience and closes no acceptance criterion.

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

## Task Group 5.6 — Test equipment: `:r4-simulator` and the dev injector (serves R4; test levels I3 and I4)

> HLD **D9** (build from the frozen samples, validate through `R4Json` before sending) and **§7**. This is sanctioned IVI test equipment inside `IVI_ECU/`, not a mock to eliminate — it cannot reach into `ADA_ECU/` (no cross-node source imports), so it reaches the same models the app parses with by depending on `:contract`.

### [ ] `4.5.6.1` — Module `:r4-simulator` + `SimConfig` *(agent)*

**Objective:** stand up the CLI module and its two configuration sources.

**Scope:**

- `settings.gradle.kts`: `include(":r4-simulator")`. New `IVI_ECU/r4-simulator/build.gradle.kts`: `alias(libs.plugins.kotlin.jvm)`, `alias(libs.plugins.kotlin.serialization)`, `id("application")` with `mainClass = "com.hackathon.v2x.ivi.sim.MainKt"`; `implementation(project(":contract"))`; `testImplementation(libs.junit)`. **Zero dependencies beyond `:contract`** (D9, criterion C4) — no YAML library, no CLI framework, no logging framework.
- `r4-simulator/src/main/kotlin/com/hackathon/v2x/ivi/sim/SimConfig.kt` — `data class SimConfig(host, port, scenarioPath, rateHz, startDelayS)` with two factories: `fromEnv()` reading **exactly** the mini-blueprint's ADA-node variable names — `IVI_ECU_HOST`, `IVI_ECU_PORT`, `R4_SCENARIO`, `R4_RATE_HZ`, `START_DELAY_S` (HLD §7: the names match the real ADA node so Phase 6 is an image swap with no node-config edit) — and `fromArgs(args)` for host mode. Env wins over the scenario file's own default rate. Missing required values fail loudly with the variable name in the message. No literals: defaults live as named constants in this file.
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

**Objective:** commit the scenarios the acceptance criteria need, as data files, and prove different files produce observably different streams.

**Scope — three files under `IVI_ECU/r4-simulator/scenarios/` (HLD §4):**

- `approach.json` — the full lifecycle in one file, because the timeout/restore path is otherwise only reachable by waiting for a stream to stop: a **first step with `geometry.vehicleC: null`** (C not yet tracked — the renderer's null-C path), then C approaching with `riskState` low → medium → high and `geometry.vehicleC` closing, then **C leaving** — distance opening back out with risk falling to `low` — and finally silence for longer than `WARNING_TIMEOUT_MS` so the view times out and the previous mode is restored while the scenario is still running. This is the scenario the in-Room evidence run uses.
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
- `docker build` on this Windows dev host is unavailable — verification transfers to the CI lane `5.5.7.3`.

**Acceptance:** `sh -n r4-simulator/entrypoint.sh` passes; the Dockerfile declares `FROM --platform=$BUILDPLATFORM` on the build stage only and copies `scenarios/` into `/app/scenarios/`; image build verified by `5.5.7.3`.

**Dependencies:** after `4.5.6.5`. **Commit:** `[5.5.6.6] feat: add the R4 simulator Dockerfile and entrypoint`

### [ ] `4.5.6.7` — `DevInjectorReceiver` — test level I3, debug build only *(agent)*

**Objective:** let an `adb broadcast` push one frozen sample onto the same flow the socket feeds, so UI work is unblocked while the ADB/network route is still unproven.

**Scope:**

- `app/src/debug/java/com/hackathon/v2x/ivi/debug/DevInjectorReceiver.kt` — a `BroadcastReceiver` for `com.hackathon.v2x.ivi.DEV_INJECT` reading `--es sample <name>`, loading that frozen sample from the `:contract` classpath, decoding it through `R4Deserializer`, and calling `R4Repository.inject(...)`. Because it joins **downstream of the socket and upstream of everything else**, it exercises parse → repository → view-model → Compose exactly as a real datagram does (HLD §6, §7).
- Register it in a **`app/src/debug/AndroidManifest.xml`** — the debug source set only. **It must be absent from the release build**: a release path that can fabricate a warning would undermine the R19 claim that C came only from relayed data (HLD §7).
- Manual invocation is rung **V3** of [deploy-ivi-hmi-walkthrough.md §4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) — the broadcast command, the expected Display-Area switch, and the `result=0`-with-no-UI-change failure that means a release build are all defined there. `17.5.9.18` runs that rung and records its evidence.

**Acceptance:** `./gradlew assembleDebug` and `assembleRelease` both green, and the **release** merged manifest contains no `DEV_INJECT` receiver (check `app/build/intermediates/merged_manifests/release/AndroidManifest.xml`); `:app:testDebugUnitTest` still green.

**Dependencies:** after `16.5.5.8` and `4.5.4.2`. **Commit:** `[4.5.6.7] feat: add the debug-only dev injector for I3 UI testing`

### [ ] `4.5.6.8` — `downgrade.json` — the D13 medium→low case on demand *(agent)*

**Objective:** a scenario file that steps `medium` then `low` and then holds, so [D13](../IVI_ECU/doc/ivi-ecu-design-decisions.md#d13--active-risk-raises-the-warning-only-silence-lowers-it)'s "a `low` does not dismiss" rule is provable in the Room without waiting for a producer cycle to wrap. [IVI HLD §12](../IVI_ECU/doc/ivi-ecu-hld.md#12-test-strategy) names this the mini-blueprint half of D13's proof; the view-model half is `17.5.4.4`'s.

**Scope — one data file, no Kotlin:**

- `IVI_ECU/r4-simulator/scenarios/downgrade.json`, in the `4.5.6.2` scenario shape, `loop: false`:
  1. a step at `riskState: "medium"` with `geometry.vehicleC` present — raises the warning;
  2. a step at `riskState: "low"` with `geometry.vehicleC` present and a **different** `object.distance`, so the scene visibly updates — must **not** change the mode;
  3. two further `low` steps at the same rate, proving the countdown keeps restarting;
  4. then the file ends, so silence times the view out `WARNING_TIMEOUT_MS` later — the only automatic dismissal path.
- Every payload is built from the frozen `:contract` samples through `4.5.6.3`'s `MessageBuilder`. **A literal message body in this file is a defect** (D9) — only `sample` + `overrides`.
- **No new Kotlin branch keys off the scenario name**, and no existing file changes: `approach.json`, `degrade.json` and `state-stream.json` are `4.5.6.4`'s and stay as written.
- Extend `r4-simulator/src/test/kotlin/.../ScenarioLoaderTest.kt` to load this file too, and `ScenariosDifferTest.kt` with one case: the decoded `riskState` sequence of `downgrade.json` is `medium` then `low`, and differs from `approach.json`'s.

**Acceptance:** `./gradlew :r4-simulator:test` green; the file exists at the designated path; the emitted `riskState` sequence read back off the built payloads is `medium, low, low, low`.

**Dependencies:** after `4.5.6.4` (the shape, the loader test and the differ test it extends). **Parallel** with `4.5.6.5`, and **before `5.5.6.6`**, whose `COPY scenarios/` is what puts the file in the image. **Commit:** `[4.5.6.8] feat: add the downgrade scenario proving a low does not dismiss the warning`

---

## Task Group 5.7 — CI lanes (serves R4, R5, R16)

> **Where a lane goes is decided by its origin phase**, per [phase0-ci.yml](../.github/workflows/phase0-ci.yml)'s own header rule: a CI lane is maintained in its origin phase's workflow file, whichever phase edits it. `ivi-unit-tests` is maintained in `phase0-ci.yml`; the two new lanes originate here and are maintained in `phase5-ci.yml`.

### [ ] `16.5.7.1` — New `phase5-ci.yml` with the `ivi-assemble` lane *(agent)*

**Objective:** build the APK on every push, gate it on the IVI unit tests, and publish `app-debug.apk` as a run artifact so the ADB install steps have a build to fetch.

**Scope:** new `.github/workflows/phase5-ci.yml`, named `phase5-ci`, with the same `on:` triggers and `concurrency` block as `phase0-ci.yml` (so all lanes still run on every push; the split changes where a lane is maintained, never whether it executes). One job:

- `ivi-assemble` — `actions/checkout@v4`, `actions/setup-java@v4` (temurin 17, `cache: gradle`), `working-directory: IVI_ECU`, `chmod +x gradlew`, `./gradlew :app:testDebugUnitTest --no-daemon` then `./gradlew assembleDebug --no-daemon`, then `actions/upload-artifact@v4` publishing `IVI_ECU/app/build/outputs/apk/debug/app-debug.apk` as `app-debug-apk` with `if-no-files-found: error`. `timeout-minutes: 30` bounds a hung dependency resolve without capping a slow cold build.
- **The unit tests run in the same job as the build, deliberately overlapping `phase0-ci.yml`'s `ivi-unit-tests`** — this job's output is hand-installed onto a guest, so an APK must never leave the workflow unless its own tests passed in the job that produced it. It is a gate on the artifact, not a second test lane: extend `ivi-unit-tests` when test targets change, never this step.
- Two reporting steps between assemble and upload: record the APK size as a `::notice::` (`::error::` and fail if the APK is missing after a successful assemble), and report whether the APK declares a launcher activity — the latter never fails the lane, and reports the missing launcher entry that [walkthrough §2.6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#26-check-the-apk-is-launchable) exists to detect.
- Add `android-actions/setup-android` only if the runner image's SDK/licence state turns out insufficient (HLD §11) — try without it first and record which was needed.
- `ivi-assemble` carries no `lint` step. It is the only lane producing the APK, so one `Error`-severity finding would stop APK production for a reason unrelated to the push. `16.5.7.4` adds lint.

**The lane is documented by [deploy-ivi-hmi-walkthrough.md §3.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#31-the-workflow-its-job-and-its-triggers)** — its job name, step order, triggers, concurrency, timeout and the three notices it emits. That section and this subtask must stay in step; neither restates the other's detail.

**Executor split.** Authoring the workflow file and pushing it are agent work — [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human)'s *Trigger a CI build* row is AI. Confirming the run passed ([§3.2](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#32-check-that-the-run-finished-and-passed)) and downloading `app-debug-apk` ([§3.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#33-get-the-apk-off-ci)) are §5's rows 2 and 3, both **Human**, and are `16.5.7.5`. §5's first qualification flips both rows to AI on a host with an authenticated `gh` CLI; without `gh auth login` they stay Human, and § Open items item 10 carries which state applies.

**Acceptance:** `.github/workflows/phase5-ci.yml` is committed with the `ivi-assemble` job as scoped above, and pushed on the phase branch so the lane is triggered. The report states no APK size budget, so `16.5.7.5` records the size rather than gating on it.

**Dependencies:** none beyond a pushable branch — **land this early, in parallel with group 5.1**, so an APK artifact exists for group 5.9. **Commit:** `[16.5.7.1] ci: add phase5-ci with the ivi-assemble lane`

**Status:** the workflow file is committed and pushed. The checkbox above stays unticked: the run confirmation and the artifact retrieval belong to `16.5.7.5`, which is not started.

### [ ] `16.5.7.5` — Confirm the `ivi-assemble` run and retrieve the APK artifact *(Human, or agent where `gh auth login` has been run)*

**Objective:** produce the built APK and its run record, so the install subtasks have a build to install.

**Scope — two steps, in order:**

1. The person confirms the `ivi-assemble` run finished and passed, by either route of [§3.2](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#32-check-that-the-run-finished-and-passed).
2. The person downloads and unzips the `app-debug-apk` artifact, by either route of [§3.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#33-get-the-apk-off-ci).

Both steps are §5's rows 2 and 3. **Both flip to AI**, and this subtask is then spawned to an implementation subagent instead of handed to a person, **on a host with an authenticated `gh` CLI** — §5's first qualification, Route B of §3.2 and §3.3, which needs no browser. Without `gh auth login` they stay Human. §5's second qualification names the third route: an agent with a JDK and an Android SDK runs the local build of [§2](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#2-building-on-the-local-machine) and produces the same APK, which § Open items item 11 gates.

**Acceptance:** the run ID, its conclusion, the local path of the unzipped `app-debug.apk` and the APK's size, all recorded in `plans/doc/phase5-ivi-run.md`. Evidence commit by the orchestrating session once the person confirms, or by the subagent on the `gh` route.

**Dependencies:** after `16.5.7.1`, and after the push that triggered the run. **Commit:** `[16.5.7.5] docs: record the ivi-assemble run and the retrieved APK`

### [ ] `4.5.7.2` — Extend `ivi-unit-tests` to all five modules *(agent)*

**Objective:** run every one of the five modules' tests in CI, not just `:app`'s (HLD §11 — the invocation that must change). A module whose tests only ever run locally is untested as far as the phase's acceptance is concerned.

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

### [ ] `16.5.7.4` — Add the `lint` step to `ivi-assemble` *(agent)*

**Objective:** bring the lane to [HLD §11](../IVI_ECU/doc/ivi-ecu-hld.md#11-tech-stack-build-and-ci), which defines `ivi-assemble` as `assembleDebug` + `lint`, without letting one finding block the only lane that produces the APK.

**Scope — in this order; the order is the whole subtask:**

1. **Run `./gradlew lint` once from `IVI_ECU/` and read the report.** Record the `Error`-severity findings in the commit body — count, check id, and the file each names. Nothing below is decided without its output.
2. **If the run is clean, leave lint fatal.** A lane that fails on a real `Error` is the stronger gate. Add the step and stop.
3. **If it reports `Error`-severity findings**, set `lint { abortOnError = false }` in `app/build.gradle.kts` **with a comment naming the specific findings it covers**, so it reads as a recorded exception and not a permanent opt-out. Lint still runs, still prints every finding to the run log, and still writes its report — only the build failure is suppressed. **Nothing is silenced.**
4. Add the `./gradlew lint --no-daemon` step to the `ivi-assemble` job in `.github/workflows/phase5-ci.yml`, after `assembleDebug` and before the reporting steps, so a lint failure can never prevent the APK from being built.

**What lint catches:** `NewApi` is what flags a call above `minSdk 29`, and the unit tests cannot catch that — they run on a desktop JVM against a stubbed Android jar, so an API-31 call compiles, passes, and then fails on the guest. The guest's actual API level is unread until `16.5.9.7` ([§6.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) item 5), so this is the only automated guard standing until then. It also catches the manifest defects that present as "app starts, then the screen returns to the launcher" in [§4.10](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#410-troubleshooting-the-deploy-and-install).

**Acceptance:** the lane runs green with the lint step executing and its findings visible in the run log; the APK artifact is still published; if `abortOnError = false` was set, `app/build.gradle.kts` carries the comment naming the findings. Record the run ID and the finding count.

**Dependencies:** after `16.5.5.8` — lint on a module with no launcher activity reports noise that says nothing about the finished app. Parallel with group 5.6. **Commit:** `[16.5.7.4] ci: add the lint step to ivi-assemble`

---

## Task Group 5.9 — Isolated IVI test (serves R4, R5, R6, R16, R17, R18 — § Open items item 8 carries R18's scope)

> The Room holds three nodes: the Ethernet Bridge, an ADA container node standing in for the ADA ECU, and the IVI Skycraft node. The stand-in runs the R4 simulator image of groups 5.6–5.7, so this test depends on neither the ADA nor the comms track. Composition and creation route: [deploy-ivi-hmi-walkthrough.md §4.11](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#411-the-mini-blueprint-route), which fixes clone-then-delete as the route, states that the mechanics are §4.2–§4.10 with only the composition differing, and makes the ADA node the only node reconfigured.
>
> **This group closes four of the five Phase 5 acceptance criteria.** The verification ladder is [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging)'s rungs **V1–V5**; the four proofs they must produce are [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance), which every subtask below takes its acceptance from.
>
> **Two different things are called "install", and they fall on opposite sides of the split.** Setting a node's **image field** is a Nydus Inspector edit, because the REST API has no update route for an existing node's config, so it is *Human*. Installing the **APK** with `adb install -r` and launching it with `adb shell am start` are commands against the guest, which §5 assigns to AI, so they are *car-sky*.
>
> **The APK is installed twice, by two subtasks with different purposes.** `16.5.9.7` installs whatever build exists at the time, only to prove the ADB route works. `16.5.9.10` installs the finished Phase 5 build, which is the one every observation after it is made against.
>
> **`16.5.9.6`–`16.5.9.7` are the phase's earliest risk.** The ADB tunnel route is the organizers' own and there is only one of it, and it is unproven by this team ([§6.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) item 1); the guest's API level and `automotive` feature are unread (item 5). A negative answer on either moves every criterion below to AAOS-emulator evidence. Neither subtask needs Phase 5 code, so their dependency lines place them ahead of the code groups.

### [ ] `5.5.9.1` — Compose the mini-blueprint by cloning the baseline *(Human)*

**Objective:** produce a 3-node blueprint — Ethernet Bridge, ADA container node, IVI Skycraft node — that keeps the baseline's `ethernet` pins.

**Scope — the creation route of [§4.11](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#411-the-mini-blueprint-route), which fixes the clone-then-delete steps and what deleting a node does to its pin and edge. The person performs each step in Nydus:**

1. The person clones **`baseline_phase1`** per §4.11, and clones no other blueprint — [carsky-4-node-blueprint.md § The blueprints on CarSky](../requirements/car-sky-guide/carsky-4-node-blueprint.md#8-the-blueprints-on-carsky) is the authority for that source.
2. The person names the clone. **The name is the user's to pick**, and it is the only place the differentiator goes.
3. The person deletes the Bench node on the canvas.
4. The person deletes the V2X node on the canvas.
5. The person records the clone's name in `plans/doc/phase5-ivi-run.md`.

Three nodes are left: Ethernet Bridge `10.99.0.1` (`10.99.0.0/24`, `bridgeMode: "linux"`), ADA Container Node `10.99.0.12`, IVI Skycraft Node `10.99.0.13`. Change nothing else — addresses, the `47300` port and the pin shapes stay at the baseline values, which is what lets `4.5.9.9` later change the ADA node's image and env and nothing more.

Every subtask after this one deploys and edits *that* blueprint. Do not edit `baseline_phase1` itself, and do not edit a `<name>-deploy` snapshot that a deployment creates ([§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint)).

**Acceptance:** the blueprint exists with exactly three nodes and their pins intact, confirmed by `6.5.9.3`'s read-back, and recorded in `plans/doc/phase5-ivi-run.md` (created by this subtask, on the [phase0-smoke-test-run.md](doc/phase0-smoke-test-run.md) pattern). Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** none — needs no Phase 5 code and no image. **Commit:** `[5.5.9.1] docs: record the mini-blueprint composition`

### [ ] `4.5.9.2` — Set the ADA node's probe config and confirm the IVI node's VM image *(Human)*

**Objective:** give the ADA node a config that proves the network hop before the simulator image exists, and confirm the IVI node can deploy at all.

**Scope — four steps in the Nydus Inspector. Nothing is installed and nothing is deployed here. The person performs each step:**

1. The person clicks the ADA node.
2. The person sets the ADA node's probe config to the one [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) rung **V2** fixes — that rung names the image, the `NEXT_HOP_*` env pair and the port, and they are not restated here. Prefix the image reference with `registry.hackathon-2.carsky.io/` and set `capabilities: ["NET_RAW"]`, so a `[CAP]` line can corroborate the datagram on the wire (R6).
3. The person clicks the IVI node.
4. The person checks the IVI node's `image` block against the four fields of [node-ivi-ecu.md § Blueprint node config](../requirements/car-sky-guide/node-ivi-ecu.md#blueprint-node-config), and changes nothing. [§4.2](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#42-configure-the-blueprint-and-its-ivi-node) quotes the message the deploy is rejected with when they are missing.

The probe config stays until `4.5.9.9` replaces it with the simulator's. Setting an image field is a canvas edit, not an installation: the platform pulls the image when the Room deploys.

The IVI node's Part Prefix and display fields come off the read-back at `6.5.9.3`, which §5 assigns to AI.

**Acceptance:** `6.5.9.3`'s read-back shows the ADA node's probe-config image, `NEXT_HOP_*` env and `NET_RAW`, and the IVI node's four `image` fields. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `5.5.9.1`. **Commit:** `[4.5.9.2] docs: record the mini-blueprint node configuration and measured display fields`

### [ ] `6.5.9.3` — Read the mini-blueprint back and confirm its topology *(car-sky)*

**Objective:** prove from stored state, not from the Inspector's truncated fields, that the blueprint is deployable before a Room slot is spent on it.

**Scope:** `GET /api/v1/blueprints/{id}` — the AI read-back row of [§4.2](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#42-configure-the-blueprint-and-its-ivi-node). Confirm every one of:

- one `ETHERNET` / `OUTPUT` pin on the ADA node at `10.99.0.12` and one on the IVI node at `10.99.0.13`, each edged to the bridge's single `INPUT` pin, in the shape [node-ivi-ecu.md § Pins](../requirements/car-sky-guide/node-ivi-ecu.md#pins) fixes;
- the bridge node's `bridgeMode` and `subnet` — without them the `10.99.0.x` addresses have no network;
- the IVI node's four Skycraft `image` fields;
- the ADA node's image reference, `NET_RAW`, and every env value — `NEXT_HOP_PORT=47300` above all, since a wrong port produces a silent no-traffic run;
- the IVI node's **Part Prefix, Display Width, Height, DPI and GPU Backend**, which §5's read-back row states this same call captures. §4.2 says not to assume them. `16.5.9.11` needs the Part Prefix to point the Screen, Log and ADB widgets at the right parts, and the display size is the guest's real resolution against which `16.5.5.6`'s preview size is confirmed.

`POST /api/v1/blueprints/{id}/validate` is a cheap second confirmation: it fails until every node has a pin.

**Acceptance:** the read-back excerpt in `plans/doc/phase5-ivi-run.md` with every point above confirmed, including the Part Prefix and the five display fields, or the exact mismatch named and handed back to `5.5.9.1`/`4.5.9.2` for a canvas fix. A deploy does not start on an unconfirmed blueprint.

**Dependencies:** after `4.5.9.2`. **Commit:** `[6.5.9.3] docs: record the mini-blueprint topology read-back`

### [ ] `5.5.9.4` — Deploy the mini-blueprint *(Human)*

**Objective:** bring up the Room the rest of this group observes.

**Scope:** the person runs [§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint)'s deploy steps against the mini-blueprint `5.5.9.1` cloned. §5 keeps the row Human because choosing the Device is a judgement call and deploying spends one of the two Room slots the comms track also draws on. What is specific to this subtask:

1. The subject is the mini-blueprint itself, not the `-deploy` snapshot deploying creates.
2. The Device picked is an **existing** one; no new Device is created.

Watching the node badges is not part of this subtask — `5.5.9.5` records the phases. Expect the Skycraft node to lag the containers.

**Acceptance:** the deployment exists and its Room id is recorded in `plans/doc/phase5-ivi-run.md`; `5.5.9.5` confirms the node phases. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `6.5.9.3`; a free Room slot. **Commit:** `[5.5.9.4] docs: record the mini-blueprint deployment`

### [ ] `5.5.9.5` — Poll the nodes to `Running` and resolve every `nodeKey` *(car-sky)*

**Objective:** record that every node came up, and produce the keys every log route needs.

**Scope:** the AI row of [§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint) — poll `GET /api/v1/deployments/{roomId}/nodes` until every node reads `Running` with restart count 0, recording each entry's `name`. A node stuck in `Provisioning` is almost always an image that cannot be pulled ([§4.10](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#410-troubleshooting-the-deploy-and-install)); diagnose per [carsky-room-diagnostics](../.claude/skills/carsky-room-diagnostics/SKILL.md) rather than redeploying blind.

**Acceptance:** 3/3 nodes `Running` with restart count 0 and the three `nodeKey` values in `plans/doc/phase5-ivi-run.md` — the precondition every [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance) proof rests on.

**Dependencies:** after `5.5.9.4`. **Commit:** `[5.5.9.5] docs: record the mini-blueprint Room reaching Running`

### [x] `16.5.9.6` — Start the ADB tunnel to the guest *(car-sky)*

**Objective:** leave the organizers' ADB tunnel serving a local port, so every step below can reach the Skycraft guest.

**Scope — three steps against [§4.4](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#44-get-an-adb-endpoint), which is one route rather than a search. §5's *Start the ADB tunnel* row assigns it to AI, because a CLI invocation is agent-runnable. The car-sky agent performs each step:**

1. The agent confirms the three inputs `16.5.9.19` supplies are in hand.
2. The agent runs the `reach-backend` tunnel command as §4.4 fixes it. The command and its flags stay in §4.4 and are not copied here.
3. The agent leaves the command running in its own terminal. Closing that terminal drops the tunnel, and `16.5.9.7` runs in a second one.

**The route is mentor-supplied and unexercised by this team** (§6.1 item 1). Treat a failure as a finding, not a retry loop: §4.10's `command not found` and tunnel-exits rows say what each failure means.

**Acceptance:** the CLI serving on the local port, recorded in `plans/doc/phase5-ivi-run.md` with the port used — or the exact failure. `16.5.9.7` is what confirms the tunnel actually carries ADB, via §4.5's `localhost:<port>   device` line.

**Dependencies:** after `5.5.9.5` and `16.5.9.19`. **Commit:** `[16.5.9.6] docs: record the ADB tunnel start against the Skycraft guest`

**Status:** done — `reach-backend` served `127.0.0.1:5555` on first use and was left running; output recorded in [doc/phase5-ivi-run.md](doc/phase5-ivi-run.md). Deviation: run against the already-Running m1-system-test Room rather than the mini-blueprint, so the `5.5.9.5` dependency was met by that Room instead.

### [x] `16.5.9.19` — Obtain the tunnel CLI, its gateway URL and its token *(Human)*

**Objective:** put the three organizer-supplied inputs the ADB tunnel needs into the team's hands.

**Scope — three steps. No agent performs any of them: §5's third qualification states that obtaining all three is human work, and `16.5.9.6`'s AI row assumes they are already in hand:**

1. The person obtains the `reach-backend` binary and installs it on the host that runs `16.5.9.6` ([§6.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) item 2).
2. The person obtains the gateway URL this team passes to `--gateway` (§6.1 item 3).
3. The person obtains the `a8k_…` derived token, and establishes whether it is the CarSky API key of [§1.2](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#12-cloud-platform-access) or a separate credential (§6.1 item 4).

**Acceptance:** the provenance of each of the three inputs recorded in `plans/doc/phase5-ivi-run.md`, answering §6.1 items 2, 3 and 4. **The token value is never written into the repository.** Evidence commit by the orchestrating session after the person confirms.

**Dependencies:** none — needs no Phase 5 code, no image and no Room. **Commit:** `[16.5.9.19] docs: record the provenance of the ADB tunnel inputs`

**Status:** done — all three inputs in hand, provenance recorded in [doc/phase5-ivi-run.md](doc/phase5-ivi-run.md): the binary under `tools/apk uploader/` (git-ignored), the gateway is the workbench base URL itself, and the token is a per-device derived value from the Rework Local ADB dialog — not the CarSky API key — held under `secrets/`. §6.1 items 2–4 answered; the token value is not in the repository.

### [ ] `16.5.9.7` — Prove the ADB route and read the guest's properties *(car-sky)*

**Objective:** answer whether the guest is reachable and whether it will accept the APK at all — the two findings that invalidate every in-Room criterion below if negative.

**Scope — four steps against [§4.5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#45-connect-and-check-the-guest) then [§4.6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#46-install-the-apk), both AI rows in §5, over the tunnel `16.5.9.6` left running. Those sections carry the `adb connect`, `getprop` and `pm list features` commands, the install command, and what each failure means; none of that is restated here. The car-sky agent performs each step:**

1. The agent connects to the guest and reads its properties, per §4.5.
2. The agent obtains a build to install. **The agent route is a local `./gradlew assembleDebug`**, which § Open items item 11 gates on a JDK and an Android SDK being present. **The CI route is a Human hand-off** — `16.5.7.5` downloads `app-debug-apk`, because §5 rows 2 and 3 are Human.
3. The agent installs that build, per §4.6. Before `16.5.5.8` lands the build has no launcher activity, so it installs and cannot be started; that is expected, because this subtask proves the **route** and `16.5.9.10` is where the finished build is installed and launched.
4. The agent confirms the evidence filter streams — `adb logcat -s IVI_V2X`, the guest-side surface [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) reads and the whole demo's text evidence depends on.

The findings this subtask produces are items **1 and 5** of [§6.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) — the tunnel route carrying ADB at all, and the guest's API level and `automotive` feature. Answer each on this deployment and write the answer down. Item 9, the screenshot route, is `16.5.9.21`.

**If §4.5's connect or §4.6's install fails**, that is the finding: record it, and every criterion below degrades to **AAOS emulator** evidence on an *automotive* system image — a phone image rejects the APK on the `automotive` feature. Escalate rather than retrying blind; [§4.10](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#410-troubleshooting-the-deploy-and-install) is a troubleshooting table, not a licence to repeat a failed route.

**Acceptance:** the outputs of §4.5 and §4.6 — or the exact failure — recorded in `plans/doc/phase5-ivi-run.md`, with the guest's API level against `minSdk 29`, its `automotive` answer, and the fallback decision if either is negative.

**Dependencies:** after `16.5.9.6`. **This is the phase's earliest risk: it needs no Phase 5 code, so it must not wait behind groups 5.1–5.7.** **Commit:** `[16.5.9.7] docs: record the proven ADB route and AAOS guest properties`

### [ ] `16.5.9.21` — Try the screenshot route once *(car-sky)*

**Objective:** answer whether the scriptable screenshot route answers on this deployment, so the later evidence subtasks know whether a browser-free capture path exists.

**Scope — two steps. The car-sky agent performs both:**

1. The agent issues one `GET /api/v1/vms/{roomId}/{nodeKey}/screenshot` call, the scriptable alternative [§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence) offers for evidence capture.
2. The agent records the outcome either way, answering [§6.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) item 9.

One call, not a retry loop. §4.9's Recorder Part and the Devices panel stay the primary capture route whatever this answers.

**Acceptance:** the call's response — an image or the exact failure — recorded in `plans/doc/phase5-ivi-run.md`, with §6.1 item 9 answered there.

**Dependencies:** after `5.5.9.5`, which resolves the `nodeKey`. Parallel with `16.5.9.7`. **Commit:** `[16.5.9.21] docs: record whether the VM screenshot route answers`

### [ ] `16.5.9.8` — Record the proven route in the IVI node guide *(agent — docs)*

**Objective:** get the ADB facts `16.5.9.6` and `16.5.9.7` established into the per-node deploy guide, which is where node facts live.

**Scope — the subagent makes the edit.** The `node-*.md` reference files are unowned ([CLAUDE.md § Roles](../CLAUDE.md)): the agent that establishes a node fact records it there. This subtask edits [node-ivi-ecu.md § Post-deploy](../requirements/car-sky-guide/node-ivi-ecu.md#post-deploy-install-the-team-apk) itself and commits it.

The edit carries, verbatim from `16.5.9.6`'s and `16.5.9.7`'s recorded outputs, the **facts** that file owns: that [§4.4](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#44-get-an-adb-endpoint)'s tunnel carried ADB to this node, the local port it served and the `adb connect` target that answered, the guest's API level, and its automotive-feature answer. **The commands are not copied in** — install is [§4.6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#46-install-the-apk), launch and the `--ei r4_port` override are [§4.7](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#47-open-the-screen-and-launch-the-app), and the `adb logcat -s IVI_V2X` filter is [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging); link them. That is the division the walkthrough states about itself: the node guide owns the node's *facts*, the walkthrough owns the *doing*. If the route failed, record that instead, plus the emulator fallback. Do not restate the blueprint procedure — that is [§4.2](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#42-configure-the-blueprint-and-its-ivi-node) and [§4.11](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#411-the-mini-blueprint-route).

**Acceptance:** [node-ivi-ecu.md](../requirements/car-sky-guide/node-ivi-ecu.md) § Post-deploy carries the tunnel form that connected — or the failure — and the guest's API-level and automotive answers, and links §4.6–§4.8 for the commands instead of duplicating them; every line traces to a recorded output, with no invented values. Doc-only.

**Dependencies:** after `16.5.9.7` and `16.5.5.8` (the launch-override command must exist before it is documented as working). **Commit:** `[16.5.9.8] docs: record the proven ADB route in the IVI node guide`

### [ ] `4.5.9.9` — Switch the ADA node to the R4 simulator's evidence config *(Human)*

**Objective:** put the R4 simulator on the wire toward `10.99.0.13:47300`.

**Scope — five steps. §5 assigns *Configure the ADA node's feed* to Human, because the Inspector is the only way to change an existing node's config. The person performs each step:**

1. The person opens the mini-blueprint in Nydus — the blueprint itself, not the `-deploy` snapshot ([§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint)).
2. The person clicks the ADA node.
3. The person replaces the probe config `4.5.9.2` set with the **evidence config** of [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) rung **V4**. That rung fixes the image, the relative `command` and the env set. Add the `registry.hackathon-2.carsky.io/` prefix to the image reference and keep `capabilities: ["NET_RAW"]`.
4. The person deploys again, per §4.3.
5. The person opens the ADA node's log by either route in §4.8's log-surface table, once the node reads `Running`.

Change nothing on the other two nodes. Addresses, the port and the pin shapes were fixed at the baseline, so this is the only node config that ever changes — [§4.11](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#411-the-mini-blueprint-route) is why.

**Acceptance:** the ADA node `Running` with restart count 0, and its log showing §4.8 V4's **link 1** — `[TX] … → 10.99.0.13:47300` at ~1 Hz — plus the `[CAP]` corroboration. Recorded in `plans/doc/phase5-ivi-run.md`. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `5.5.9.5`, and after `5.5.7.3` has pushed and verified the image. **Commit:** `[4.5.9.9] docs: record the R4 simulator running on the mini-blueprint ADA node`

### [ ] `16.5.9.10` — Install and launch the Phase 5 APK, and record the boot-to-listener time *(car-sky)*

**Objective:** get the Phase 5 build running on the AAOS guest, and capture the one timing number no other phase can produce.

**Scope — six steps against [§4.6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#46-install-the-apk) then the launch half of [§4.7](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#47-open-the-screen-and-launch-the-app), both AI rows in §5. Those sections carry the commands and are not restated here. The car-sky agent performs each step:**

1. The agent takes the finished Phase 5 build. `16.5.7.5` hands it over — the download off CI is a Human row, so this subtask starts at the install.
2. The agent runs `adb install -r` over the tunnel `16.5.9.6` left running, per §4.6.
3. The agent confirms the package with `pm path`.
4. The agent launches the app with `adb shell am start`, per §4.7, with the `--ei r4_port` override available.
5. The agent records the two wall-clock deltas §4.7 instructs at first launch: guest boot → launcher, and launch → `[LINK] state=bound`.
6. The agent records their sum.

[§4.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#41-how-the-apk-reaches-the-ivi-ecu-node) fixes the ordering — the guest must exist before anything installs into it.

**This installs the APK; it does not set a node image field and does not touch the canvas.** It also does not open the Screen widget — that is `16.5.9.11`, which runs after this one and needs the app already launched.

**Why the timing values matter.** [m1-run-timing-and-event-triggering.md §9 open item 5](../requirements/m1-run-timing-and-event-triggering.md) names the elapsed time from the AAOS guest starting to boot until `[LINK] state=bound port=47300` appears on `IVI_V2X` as the one number **only Phase 5 can produce**, and as the **floor for the bench's `start_delay_s`**: the IVI is the only node whose readiness cannot be observed from a container, so the bench must not start streaming before the guest is bound. If `16.5.9.7` came back negative and this runs on an emulator, say so — an emulator figure is a lower bound, not the number.

**Acceptance:** `Success` from the install and the package path from `pm path`; the app started; `[LINK] state=bound port=47300` on `IVI_V2X` — rung **V1** — and the three timing values, all in `plans/doc/phase5-ivi-run.md`. Install failures map to §4.10's table; `INSTALL_FAILED_OLDER_SDK` and the `automotive` feature error are escalations, not retries.

**Dependencies:** after `4.5.9.9`, `16.5.9.7` and `16.5.5.8`, which delivers `MainActivity` and the launcher manifest entry `adb shell am start` resolves. **Commit:** `[16.5.9.10] docs: record the APK install, launch and boot-to-listener time`

### [ ] `16.5.9.11` — Open the device widgets and confirm the R16 layout *(Human)*

**Objective:** close Phase 5 acceptance criterion 1 — *the HMI runs on the AAOS node with the R16 layout; button/app areas switch what the Display area shows.*

**Scope — five steps. The widget procedure is the widget half of [§4.7](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#47-open-the-screen-and-launch-the-app), which §5 keeps Human and which is not restated here. The app is already installed and launched by `16.5.9.10`. The person performs each step:**

1. The person opens the Devices panel and connects to the device this deployment created, per §4.7.
2. The person adds the Screen widget and sets its parts from the Part Prefix `6.5.9.3` read back.
3. The person adds the Log widget and the ADB widget, on the parts §4.7 names.
4. The person sets the **Recorder Part** before the run, which is what [§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence) requires for a recorded run.
5. The person confirms the two observations below on the streaming screen.

§4.7 also carries the black-screen recovery, so a black screen is a step in that section rather than a failure of this subtask.

The two observations:

- The **V2X LINK indicator reads `BOUND :47300`**. This is rung **V1** seen on the display instead of in the log, and it proves the status bar is wired to the listener.
- **Tapping Home, Apps and Settings changes what the Display Area shows**, with `[UI] mode=… cause=user` appearing in the Log widget. That is [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance)'s second table, first row.

**Acceptance:** the AAOS screen streaming live with clicks reaching the guest; a screenshot of the R16 layout with the link indicator bound, and a second showing a different Display Area mode after a side-button tap ([§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence)). Recorded in `plans/doc/phase5-ivi-run.md`. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `16.5.9.10`. **Commit:** `[16.5.9.11] docs: record the R16 layout running on the AAOS node`

### [ ] `18.5.9.12` — Read both log surfaces on `approach.json` *(car-sky)*

**Objective:** produce in text three of the four proofs of [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance) — the incoming warning, its parsed fields, and the event raised — making no visual judgement.

**Scope — four steps on the "Read the two log surfaces" AI row of [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human), against the two surfaces [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging)'s table names. The rung is §4.8 V4 with `approach.json` on the ADA node; its four links are the checklist and are not restated here. The car-sky agent performs each step:**

1. The agent reads the ADA node's producer log, `GET /api/v1/deployments/{roomId}/logs/{nodeKey}?container=user`. **`container` is mandatory**, and omitting it returns 500.
2. The agent reads the guest's `adb logcat -s IVI_V2X`.
3. The agent captures the excerpts covering V4 links 1, 2 and 3. Link 4 is a visual judgement §5 assigns to Human, and is `17.5.9.13`.
4. The agent notes in the record that the scenario's first step carries `geometry.vehicleC: null`, so `17.5.9.13` can check it rendered without C and without a crash.

**Acceptance:** §6 proofs 1, 2 and 3 as log excerpts in `plans/doc/phase5-ivi-run.md` — one `[RX] type=warning … cSource=v2x_relayed` per datagram corroborated by the ADA's `[TX] … → 10.99.0.13:47300`, the parsed `warningType` / `risk` / `cSource` / `cPos` fields on that line, and `[UI] mode=WarningView cause=warning` carrying `cause=warning` and not `cause=user`.

**Dependencies:** after `16.5.9.11`. **Commit:** `[18.5.9.12] docs: record the approach-scenario log evidence for the R4 warning chain`

### [ ] `17.5.9.13` — Confirm the God View on `approach.json` and capture it *(Human)*

**Objective:** close Phase 5 acceptance criteria 2 and 3 — *a mock R4 warning brings the warning view up showing ego, B and ghost C at the composed positions*, and *ghost C renders from `v2x_relayed` data only; the 2D drawing is delivered.*

**Scope:** start the recording, watch the Screen widget while `approach.json` plays, and check five things. The first three are [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) V4's **link 4** and the rows beside it; the last two belong to this scenario file.

1. The Display Area **switches to the Warning View by itself**, with nobody tapping anything: ego and B drawn solid, ghost C dashed and translucent on a pulsing risk-coloured ground glow. **The scene alone is the warning** — no legend, no distance labels, no text overlay, no banner and **no `[V2X]` badge**. Judge against [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components) and R17, **not** against §4.8's link-4 row, which describes R17's annotated explanatory figure instead; see § Open items item 1.
2. The scenario's **first step draws no C at all** — it carries `geometry.vehicleC: null` — and the app neither crashes nor shows a placeholder.
3. When the stream stops, the view **times out back to the previous mode**, not merely to Idle. Restoring the *previous* mode is `16.5.4.5`'s behaviour, which V4's own row does not name.
4. **Risk climbs low → medium → high** across the approach, and the glow colour follows it.
5. **No yellow `[? UNKNOWN SOURCE]` marker** appears where ghost C belongs. If one does, stop: V4 calls that a **blocking defect**, not a display quirk, and it must be reported rather than worked around.

Recording and screenshots are taken per [§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence), using the Recorder Part `16.5.9.11` set.

**Acceptance:** §6 proof 4 — a recording showing the God View with ghost C dashed and glowing on its risk-coloured ground glow, with no badge, legend or distance label — with the null-C first step, the risk progression and the timeout-restore all observed, and `18.5.9.12`'s excerpt supplying `cSource=v2x_relayed` on every warning in text. Recorded in `plans/doc/phase5-ivi-run.md`. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `18.5.9.12`. **Commit:** `[17.5.9.13] docs: record the God View evidence with v2x_relayed provenance`

### [ ] `4.5.9.14` — Switch the ADA node to `degrade.json` *(Human)*

**Objective:** put the degradation scenario on the wire.

**Scope — three steps. §5 assigns node-config edits to Human. The person performs each step:**

1. The person clicks the ADA node in Nydus, on the blueprint rather than the `-deploy` snapshot ([§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint)).
2. The person changes one environment value, the `R4_SCENARIO` path, to the `degrade.json` file `4.5.6.4` commits. §4.8's V5 lead-in fixes the value. Leave the image, the `command`, the addresses and the port exactly as `4.5.9.9` set them.
3. The person deploys again, per §4.3.

**Acceptance:** the ADA node `Running` with restart count 0 and its log showing `[TX]` lines for the new scenario, recorded in `plans/doc/phase5-ivi-run.md`. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `17.5.9.13`. **Commit:** `[4.5.9.14] docs: record the degradation scenario running on the ADA node`

### [ ] `4.5.9.15` — Read the degradation, guard-trip and loop-survival logs *(car-sky)*

**Objective:** produce the text half of Phase 5 acceptance criterion 4 — *a newer message with an unknown `warningType` degrades gracefully* — and of the two defensive paths beside it.

**Scope — three steps, one per row of rung V5 of [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging), read on both log surfaces. Each row's correct and incorrect result is stated there and not restated here. The car-sky agent performs each step:**

1. The agent captures the unknown-`warningType` row's excerpts. The wire value must appear **preserved** in logcat, never rewritten to `unknown`.
2. The agent captures the `object.source: "own_sensor"` guard-trip row's excerpts, including the ERROR line the trip emits.
3. The agent captures the raw non-JSON row's excerpts: `[DROP] reason=malformed …` with the next valid warning still arriving.

**Acceptance:** logcat and producer-log excerpts covering all three V5 rows, in `plans/doc/phase5-ivi-run.md`, including the ERROR line the guard trip emits.

**Dependencies:** after `4.5.9.14`. **Commit:** `[4.5.9.15] docs: record the degradation and loop-survival log evidence`

### [ ] `17.5.9.16` — Confirm the degradation outcomes on screen and capture them *(Human)*

**Objective:** see V5's three outcomes on the display and capture each.

**Scope — three steps, one per outcome of rung V5 ([§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging)), watched on the Screen widget while `degrade.json` plays. Screenshots are taken per [§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence). The person performs each step:**

1. The person confirms and captures the **unknown warning type** outcome: a generic warning is drawn. A `FATAL EXCEPTION` is a failure.
2. The person confirms and captures the **`object.source: "own_sensor"`** outcome: a yellow `[? UNKNOWN SOURCE]` marker appears where ghost C would be. **Here the marker is the pass.** If ghost C is drawn normally instead, the R19 wiring `17.5.4.4` armed is broken — a blocking finding for the phase, not a display quirk. Stop and report it.
3. The person confirms and captures the **raw non-JSON message** outcome: the app keeps running, and the next valid warning still draws.

Teardown is `5.5.9.22`, which runs after this subtask and after `4.5.9.15`.

**Acceptance:** screenshots for all three V5 rows, recorded in `plans/doc/phase5-ivi-run.md`. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `4.5.9.15`. **Commit:** `[17.5.9.16] docs: record the degradation outcomes and the guard trip`

### [ ] `5.5.9.22` — Tear the mini-blueprint Room down *(Human)*

**Objective:** release the Room slot the isolated IVI test holds.

**Scope — two steps. §5 assigns *Tear the Room down* to Human. The person performs both:**

1. The person deletes the deployment, per [§4.12](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#412-tear-down).
2. The person records the teardown in `plans/doc/phase5-ivi-run.md`.

The mini-blueprint itself stays and can be deployed again. Only two Rooms run at once and the comms track needs one, which is why this subtask exists rather than leaving the Room up.

**Acceptance:** the deployment deleted and recorded in `plans/doc/phase5-ivi-run.md`. This record closes the isolated IVI test's evidence trail. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `17.5.9.16`, `4.5.9.15` and `17.5.9.18` — every log excerpt and every in-Room observation must be saved first, because the log route returns nothing once the Room is gone. **Commit:** `[5.5.9.22] docs: record the mini-blueprint Room teardown`

### [ ] `4.5.9.17` — Rung V2: the probe datagram proving the ADA→IVI hop *(car-sky)*

**Objective:** close rung **V2** of [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) — a datagram from the ADA node reaches the guest and the receive loop survives it — so a silent V4 can be localised to the network or to the app instead of neither.

**Run this before `4.5.9.9` swaps the config.** If the APK is installed and launched while `4.5.9.2`'s probe config is still on the ADA node, that is the rung's designed position: §4.8 states V2 "works before the simulator exists". Where the probe config is no longer on the node, `4.5.9.20` puts it back first, and this subtask becomes the first diagnostic when `18.5.9.12` finds no `[RX]` — the answer separates a bridge or port fault from a decode or UI fault in one observation.

**Scope — two steps on §4.8's log-surface table. The car-sky agent performs both:**

1. The agent reads the ADA node's log over the logs route, with the mandatory `container` parameter.
2. The agent reads `adb logcat -s IVI_V2X` on the guest.

**The netcheck payload is not JSON, so a `[DROP] reason=malformed …` per datagram is the pass**: it proves the socket, the bridge hop and the loop's survival together. The producer's `[TX] … relayed to 10.99.0.13:47300` corroborates it. `[TX]` with nothing on `IVI_V2X` means the datagram is not arriving — re-check the pin address and the port before suspecting code.

**This subtask reads; it does not configure.** Any config change is a Human Inspector edit, per §5, and is `4.5.9.20`.

**Acceptance:** paired `[TX]` and `[DROP]` excerpts in `plans/doc/phase5-ivi-run.md`, with the app still running afterwards. This closes the ADA ECU → IVI ECU network hop, which the connectivity smoke test could only check indirectly.

**Dependencies:** after `16.5.9.10`, with the probe config in place — either still there from `4.5.9.2`, or restored by `4.5.9.20`. **Commit:** `[4.5.9.17] docs: record the V2 probe-datagram evidence for the ADA to IVI hop`

### [ ] `4.5.9.20` — Restore the probe config on the ADA node *(Human)*

**Objective:** put `4.5.9.2`'s probe config back on the ADA node, so `4.5.9.17` has a probe datagram to read.

**Scope — three steps. §5 assigns *Configure the ADA node's feed* to Human, because the Inspector is the only way to change an existing node's config. The person performs each step:**

1. The person clicks the ADA node in Nydus, on the blueprint rather than the `-deploy` snapshot ([§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint)).
2. The person sets the probe config of [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) rung **V2**, which is the same config `4.5.9.2` set.
3. The person deploys again, per §4.3.

This subtask runs only where the simulator config has already replaced the probe config. `4.5.9.9` puts the evidence config back afterwards.

**Acceptance:** the ADA node `Running` with restart count 0 on the probe config, recorded in `plans/doc/phase5-ivi-run.md`. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `4.5.9.9`, and only on the diagnostic branch `4.5.9.17` names. **Commit:** `[4.5.9.20] docs: record the probe config restored on the ADA node`

### [ ] `17.5.9.18` — Rung V3: the UI comes up from an injected sample, with no network *(split)*

**Objective:** close rung **V3** — prove the whole UI path independently of the producer, so a failure anywhere above can be attributed to the network rather than the app.

**Scope — two steps on rung V3 of [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging), one per executor:**

1. The car-sky agent broadcasts the dev-injector intent `4.5.6.7` registered, naming a frozen sample. §4.8 V3 carries the command. This is an AI row, because it is a shell command against the guest.
2. The person watches the Screen widget and confirms the Display Area switches to the Warning View **by itself** and draws the God View, judged against [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components) exactly as `17.5.9.13` item 1 sets out. §5 keeps this Human because it is a visual judgement no log line replaces.

`Broadcast completed: result=0` with no UI change means the installed build is a **release** build — the injector exists in the debug build only, by design, and that is a build-selection finding rather than a UI defect.

**Run this before `5.5.9.22` tears the Room down.**

**Acceptance:** the broadcast command's output and a screenshot of the resulting Warning View, in `plans/doc/phase5-ivi-run.md`. If `16.5.9.7` came back negative and this runs on an emulator, record that it is emulator evidence.

**Dependencies:** after `16.5.9.11` and `4.5.6.7`. **Commit:** `[17.5.9.18] docs: record the V3 dev-injector evidence for the UI path`

---

## Task Group 5.10 — System verification test (serves R4, R5, R6, R16, R17, R18, R19)

> The Room holds five nodes — Bench Scenario Player, V2X ECU, ADA ECU, IVI ECU and the Ethernet Bridge — each on its own real image. The warning the IVI renders starts in a bench scenario and travels the whole relay instead of coming from a stand-in beside it.
>
> **Composition and creation route:** [deploy-ada-ecu-walkthrough.md §5.6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#56-the-full-blueprint-route). It states that its mechanics are that document's §4.1–§5.5 with only the composition differing, points at [carsky-4-node-blueprint.md](../requirements/car-sky-guide/carsky-4-node-blueprint.md) and its per-node files for each node's image, config and pin, and hands consumer-side evidence to the IVI walkthrough's [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging). The IVI node's own mechanics are [§4.2–§4.10](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#42-configure-the-blueprint-and-its-ivi-node) of that walkthrough, which the larger composition does not change.
>
> **Clone the baseline; do not build or import a blueprint.** Both §5.6 and [§4.11](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#411-the-mini-blueprint-route) state that a script-built or imported blueprint arrives without its `ethernet` pins, and usually without the Skycraft `image` block, and is rejected at deploy.
>
> **The same two senses of "install" apply here.** Setting a node's **image field** is a canvas edit and is *Human*, because [deploy-ada-ecu-walkthrough.md §7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#7-work-division-between-ai-and-human) states the API has no update route for an existing node. Installing the **APK** with `adb install -r` is *car-sky*, an AI row of [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human).
>
> **This group builds no image.** Each node's image is published by the phase that owns that node.
>
> **This group closes no Phase 5 acceptance criterion.** It produces the whole-topology evidence Phase 6's R19 convergence run starts from ([milestone1.md § Phase 6](milestone1.md#phase-6--convergence-real-data-end-to-end-r18-r19--r10-moved-to-the-future-plan)).

### [ ] `5.5.10.1` — Clone the baseline into the full blueprint and set every node's real image *(Human)*

**Objective:** produce the 5-node blueprint with Bench, V2X ECU, ADA ECU, IVI ECU and the Ethernet Bridge, each container node carrying its own real image.

**Scope:** in Nydus, clone **`baseline_phase1`** ([carsky-4-node-blueprint.md § The blueprints on CarSky](../requirements/car-sky-guide/carsky-4-node-blueprint.md#8-the-blueprints-on-carsky)) and edit the clone on the canvas. Never edit `baseline_phase1` itself and never import a blueprint file. That is the route of [deploy-ada-ecu-walkthrough.md §5.6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#56-the-full-blueprint-route) and [§4.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#41-create-the-blueprint), and §7 of that document assigns it to Human. Each node's image, config and pin are in [carsky-4-node-blueprint.md](../requirements/car-sky-guide/carsky-4-node-blueprint.md) and the per-node files it points at: [node-scenario-player.md](../requirements/car-sky-guide/node-scenario-player.md), [node-v2x-ecu.md](../requirements/car-sky-guide/node-v2x-ecu.md), [node-ada-ecu.md](../requirements/car-sky-guide/node-ada-ecu.md), [node-ivi-ecu.md](../requirements/car-sky-guide/node-ivi-ecu.md).

The person performs each step:

1. The person clones `baseline_phase1` and opens the clone on the canvas.
2. The person clicks each container node in turn.
3. The person sets that node's image field to its own real image, whatever the clone arrived carrying.
4. The person confirms the ADA node still has `NET_RAW`. Here it is not optional: the Android node runs no container, so there is no sink log, and the ADA node is the only place a `[CAP]` line can capture the outgoing warning.
5. The person leaves the IVI node's Skycraft `image` block alone — the four fields of [node-ivi-ecu.md § Blueprint node config](../requirements/car-sky-guide/node-ivi-ecu.md#blueprint-node-config). Without them the deploy is rejected outright.
6. The person leaves the ADA node's `command`, `capabilities`, env, address and port alone. §5.6 states they are the same in the isolated and full compositions; only the neighbours change.

Work on the blueprint, never on a `<name>-deploy` snapshot ([§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint)).

**Acceptance:** a 5-node blueprint whose every node names its real image, confirmed by `5.5.10.2`'s read-back and recorded in `plans/doc/phase5-ivi-run.md`. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** every node's image published by its own phase. **Commit:** `[5.5.10.1] docs: record the full blueprint composition and its node images`

### [ ] `5.5.10.2` — Read the full blueprint back and confirm all five nodes *(car-sky)*

**Objective:** confirm from stored state that five nodes carry the right images, pins and addresses, and that every image is pullable, before a Room slot is spent.

**Scope — six steps on one `GET /api/v1/blueprints/{id}`, the "Read the stored config back" AI row of [deploy-ada-ecu-walkthrough.md §7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#7-work-division-between-ai-and-human) and the matching row of [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human). The car-sky agent performs each step:**

1. The agent confirms the Bench node's image reference, `command`, env and capabilities against [node-scenario-player.md](../requirements/car-sky-guide/node-scenario-player.md), and its `ETHERNET` / `OUTPUT` pin at `10.99.0.10` edged to the bridge's single `INPUT` pin.
2. The agent confirms the V2X node's fields against [node-v2x-ecu.md](../requirements/car-sky-guide/node-v2x-ecu.md), and its pin at `10.99.0.11`.
3. The agent confirms the ADA node's fields against [node-ada-ecu.md](../requirements/car-sky-guide/node-ada-ecu.md), and its pin at `10.99.0.12`.
4. The agent confirms the IVI node's four Skycraft `image` fields against [node-ivi-ecu.md](../requirements/car-sky-guide/node-ivi-ecu.md), and its pin at `10.99.0.13`.
5. The agent confirms the bridge node carries the single `INPUT` pin the four edges land on.
6. The agent confirms each image resolves in the registry. An image that cannot be pulled is the `Provisioning` hang of [§4.10](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#410-troubleshooting-the-deploy-and-install)'s image row; the catalog check is the AI row of [netcheck §5](../requirements/car-sky-guide/deploy-walkthrough-netcheck.md#5-work-division-between-ai-and-human), with the calls in the form [deploy-ada-ecu-walkthrough.md §3.3](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#33-confirm-the-run-passed-and-the-images-landed) gives.

**Acceptance:** the read-back excerpt in `plans/doc/phase5-ivi-run.md` with all five nodes confirmed and every image resolvable, or the exact mismatch named and handed back to `5.5.10.1`.

**Dependencies:** after `5.5.10.1`. **Commit:** `[5.5.10.2] docs: record the full blueprint read-back and image confirmation`

### [ ] `5.5.10.3` — Deploy the full blueprint *(Human)*

**Objective:** bring up the Room the whole-system evidence is gathered in.

**Scope:** the person runs [§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint)'s deploy steps, which [deploy-ada-ecu-walkthrough.md §4.5](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#45-deploy) repeats for this composition. What is specific to this subtask:

1. The subject is the full blueprint `5.5.10.1` composed, not its `-deploy` snapshot.
2. The Device picked is an **existing** one.

§5.6 notes the full blueprint is one deployment like any other, so the two-Room budget still applies and the isolated IVI test's Room must be released first.

**Acceptance:** the deployment exists and its Room id is recorded in `plans/doc/phase5-ivi-run.md`; `5.5.10.4` confirms the node phases. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `5.5.10.2` and the teardown at `5.5.9.22`. **Commit:** `[5.5.10.3] docs: record the full blueprint deployment`

### [ ] `5.5.10.4` — Poll the five nodes to `Running` and resolve every `nodeKey` *(car-sky)*

**Objective:** record that all five nodes came up, and produce every key the log routes need.

**Scope:** the AI row of [§4.3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#43-deploy-the-blueprint) and of [deploy-ada-ecu-walkthrough.md §7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#7-work-division-between-ai-and-human) — poll `GET /api/v1/deployments/{roomId}/nodes` until every node reads `Running` with restart count 0, recording each `name`. Four container nodes and one Skycraft node, the latter slowest. A node stuck in `Provisioning` is the image-pull signature of [§4.10](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#410-troubleshooting-the-deploy-and-install); diagnose per [carsky-room-diagnostics](../.claude/skills/carsky-room-diagnostics/SKILL.md).

**Acceptance:** 5/5 nodes `Running` with restart count 0 and every `nodeKey` recorded in `plans/doc/phase5-ivi-run.md`.

**Dependencies:** after `5.5.10.3`. **Commit:** `[5.5.10.4] docs: record the full blueprint Room reaching Running`

### [ ] `16.5.10.5` — Install and launch the APK on the system test guest *(car-sky)*

**Objective:** get the Phase 5 build running on the IVI node of the full topology.

**Scope — four steps against [§4.6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#46-install-the-apk) then the launch half of [§4.7](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#47-open-the-screen-and-launch-the-app), both AI rows in [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human). The car-sky agent performs each step:**

1. The agent starts a tunnel as [§4.4](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#44-get-an-adb-endpoint) fixes it, in the form `16.5.9.8` recorded as working.
2. The agent takes the same APK `16.5.7.5` handed over — the download off CI is a Human row, so this subtask starts at the install.
3. The agent runs `adb install -r` and confirms the package with `pm path`. The Room is new, so the guest is new and the install runs again.
4. The agent launches the app per §4.7.

[§4.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#41-how-the-apk-reaches-the-ivi-ecu-node)'s ordering holds — the node must be `Running` first.

This is the **APK**, not a node image field: the container nodes' images were set at `5.5.10.1` and pulled at deploy.

**Acceptance:** `Success` from the install, the package path from `pm path`, the app started, and `[LINK] state=bound port=47300` on `IVI_V2X`, recorded in `plans/doc/phase5-ivi-run.md`.

**Dependencies:** after `5.5.10.4`. **Commit:** `[16.5.10.5] docs: record the APK install and launch on the system test guest`

### [ ] `16.5.10.6` — Open the device and its Screen widget on the system test *(Human)*

**Objective:** make the guest's display visible for the system test, with recording armed before anything is sent.

**Scope — four steps on the widget half of [§4.7](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#47-open-the-screen-and-launch-the-app), which §5 keeps Human and which is not restated here. The person performs each step:**

1. The person opens the Devices panel and connects to the device this deployment created.
2. The person adds the Screen widget and sets its parts from this node's Part Prefix.
3. The person adds the Log widget and the ADB widget.
4. The person sets the **Recorder Part** before anything is sent. The recorded evidence comes from this run, and [§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence) cannot capture a run that has already happened.

**Acceptance:** the AAOS screen streaming live with the recorder armed, recorded in `plans/doc/phase5-ivi-run.md`. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `16.5.10.5`. **Commit:** `[16.5.10.6] docs: record the device widgets opened on the system test guest`

### [ ] `19.5.10.7` — Read the whole relay's logs end to end *(car-sky)*

**Objective:** produce the text evidence that the warning the IVI renders originated in a bench scenario and travelled the full relay.

**Scope — five steps, with `container=user` on every logs-route call. The car-sky agent performs each step:**

1. The agent authenticates to CarSky ([carsky-deploy-preflight](../.claude/skills/carsky-deploy-preflight/SKILL.md)).
2. The agent reads the producer-side checks of [deploy-ada-ecu-walkthrough.md §5.1](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#51-check-1--the-relayed-message-is-received-and-raises-its-event) and [§5.2](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#52-check-2--both-vehicles-are-in-the-track-store), unchanged in this composition per [§5.6](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#56-the-full-blueprint-route). The relayed traffic originates in the real V2X ECU driven by the bench scenario, so `STATION_ID`, `OBJECT_ID` and the distance profile come from that scenario rather than from node env, and §5.5's `MIN_DISTANCE_M` lever is not available.
3. The agent reads `adb logcat -s IVI_V2X` for the consumer side. §5.6 states that **check 3 has no sink log** here, because the Android node runs no container, so consumer-side evidence is the guest's own log and belongs to [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging).
4. The agent reads the ADA node's `[CAP]` line, the only capture of the outgoing warning in this composition, which is why `5.5.10.1` requires its `NET_RAW`.
5. The agent saves every node log before teardown. [deploy-ada-ecu-walkthrough.md §5.7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#57-tear-down) states the log route returns nothing once the Room is gone.

**Acceptance:** [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance) proofs 1–3 on the guest side and [deploy-ada-ecu-walkthrough.md §8](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#8-expected-outputs-and-acceptance)'s producer-side checks, with `cSource=v2x_relayed` on every rendered warning — all as excerpts in `plans/doc/phase5-ivi-run.md`, correlated across nodes by timestamp.

**Dependencies:** after `16.5.10.5`. **Commit:** `[19.5.10.7] docs: record the system test log evidence across the relay`

### [ ] `19.5.10.8` — Confirm the God View on live relayed data and capture it *(Human)*

**Objective:** see the warning view come up from data that travelled the whole relay, and capture it.

**Scope — three steps on [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) V4's link 4, captured per [§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence). The person performs each step:**

1. The person watches the Screen widget with the recording running.
2. The person confirms the Display Area switches itself to the Warning View and draws the God View. Only the source of the data differs from the isolated IVI test, so the drawing to expect is the same one, judged against [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components) per `17.5.9.13` item 1 and § Open items item 1.
3. The person confirms no yellow `[? UNKNOWN SOURCE]` marker appears where ghost C belongs. One that does is a blocking defect.

Teardown is `5.5.10.9`, which runs after this subtask and after `19.5.10.7`.

**Acceptance:** a recording showing the God View drawn from live relayed data, with `19.5.10.7`'s excerpts backing it in text; recorded in `plans/doc/phase5-ivi-run.md`. Evidence commit by the orchestrating session after the user confirms.

**Dependencies:** after `19.5.10.7` and `16.5.10.6`. **Commit:** `[19.5.10.8] docs: record the system test God View evidence`

### [ ] `5.5.10.9` — Tear the full blueprint Room down *(Human)*

**Objective:** release the Room slot the system test holds.

**Scope — two steps. §5 assigns *Tear the Room down* to Human. The person performs both:**

1. The person deletes the deployment, per [§4.12](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#412-tear-down) and [deploy-ada-ecu-walkthrough.md §5.7](../requirements/car-sky-guide/deploy-ada-ecu-walkthrough.md#57-tear-down).
2. The person records the teardown in `plans/doc/phase5-ivi-run.md`.

The blueprint itself stays and can be deployed again.

**Acceptance:** the deployment deleted and recorded in `plans/doc/phase5-ivi-run.md`.

**Dependencies:** after `19.5.10.8`, `19.5.10.7`, which saves every node log first, and `22.5.10.10`, which reads them. **Commit:** `[5.5.10.9] docs: record the full blueprint Room teardown`

### [ ] `22.5.10.10` — R22 run-choreography evidence: K6 and K7 on the system-test run *(car-sky)*

**Objective:** R22's two measurable outputs, read off the run `19.5.10.7` and `19.5.10.8` already produce. This is the **only** configuration that can produce them — the real bench drives the real detector-paced ADA node, which drives the real IVI app.

**Scope — reading two surfaces of one run. No deploy, no new Room, no configuration change.**

- **K6 — the ADA side** ([ADA HLD §12](../ADA_ECU/doc/ada-ecu-hld.md#12-test-strategy)). Save the ADA node's `[EVT]` stream, the detector's R3 JSONL and the bench node's `[TX]` JSONL from the same run, then run Phase 4's checker:

  ```
  python ADA_ECU/tools/check_run_alignment.py --evt ada.log --detector r3.jsonl --bench tx.jsonl
  ```

  **Accepted when K6 passes: the interval from `T0` — the run's first `own_sensor` R3 line — to the first `r4_tx` is 8.0 s ≤ Δ < 10.0 s.** Report K1–K5's verdicts from the same invocation; a K1–K5 failure is a finding against R20/R21, not against R22.
- **K7 — the IVI side** ([IVI HLD §12](../IVI_ECU/doc/ivi-ecu-hld.md#12-test-strategy)). From `adb logcat -s IVI_V2X` over the tunnel `16.5.10.5` established — the AI row of [deploy-ivi-hmi-walkthrough.md §5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human), procedure [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging), evidence capture [§4.9](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#49-capture-the-evidence). **Accepted when both hold:** the run's first `[UI] mode=WarningView cause=warning` line is the **first** Warning-mode line of the run, and it follows the app's own startup `[UI] mode=HomeView` line by **≥ 8.0 s**. Record both timestamps and the delta.
- **The screen recording is the visual half and it is armed by `16.5.10.6` and confirmed by `19.5.10.8`.** This subtask cites it, does not re-record it, and stops at any Human row rather than improvising around it.
- **The two clocks never meet.** K6 reads the ADA node's own stamps, K7 the guest's own logcat stamps; no check subtracts one from the other ([ADA D10](../ADA_ECU/doc/ada-ecu-design-decisions.md#d10--clock-domains-and-stimulus-paced-against-clock_monotonic)).
- **Record the configuration the run used** — the bench's `start_delay_s`, `duration_s`, `initial_distance_m`, `closing_speed_mps`, `cpm_rate_hz`; the ADA node's `DETECTOR_REALTIME_PACING`, `DETECTOR_CLIP_FPS`, `DETECTOR_FRAME_STRIDE`, `GATE_ENTER_M`, `RISK_NEAR_M`, `RISK_CRITICAL_M`, `RISK_DWELL_MS`. A pass at unknown values proves nothing.
- **A K6 miss is a `start_delay_s` finding first.** The remedy is the bench key, re-derived from this run's measured `T0`; it is never the R13 gate and never the risk bands ([SP D7](../Scenario_Player/doc/scenario-player-design-decisions.md#d7--the-demo-cycle-is-one-clip-length-and-its-geometry-is-solved-backwards-from-the-first-warning)). Report the number and hand back to Phase 1 `22.1.13.4` rather than retuning here.

**Acceptance:** the checker's six verdicts with K6's measured Δ, K7's two log lines with their delta, and the run's configuration values — all recorded in `plans/doc/phase5-ivi-run.md`, cited by the run this group produced.

**Dependencies:** after `19.5.10.7` (the logs it reads are saved there) and Phase 4 `21.4.3.4` (the checker). Must run **before `5.5.10.9`** tears the Room down — the logs route returns nothing once the Room is gone. **Commit:** `[22.5.10.10] docs: record the R22 K6 and K7 evidence from the system test run`

---

## Execution order

Dependencies are real (files, Gradle project graph, contract artifacts, deployed Rooms) — not default assumptions. Execution tracks A–J below group subtasks by dependency chain; "lane" is reserved for a CI job. **Four subtasks depend on nothing and can all be opened at once:** `4.5.1.1` (code), `5.5.9.1` (the mini-blueprint, Human), `16.5.9.19` (the tunnel inputs, Human) and `16.5.7.1` (the CI lane).

```
Track A  foundation:   4.5.1.1 ─► 4.5.1.2 ─► 4.5.1.6 ─► 4.5.1.3 ─► 4.5.1.4 ─► 4.5.1.5
                                                           │  (gate for tracks B–F)
Track B  :serializer:  4.5.2.1 ─► 4.5.2.2 ─► 4.5.2.3
Track C  :observer:    4.5.3.1 ─► 4.5.3.2 ─► 4.5.3.3 ─► 4.5.3.4 ─► 4.5.3.5
                       (4.5.3.1 needs 4.5.2.1 only — starts before track B ends)
Track D  app logic:    { 4.5.4.1 ∥ 4.5.4.2 ∥ 4.5.4.3 } ─► 17.5.4.4 ─► 16.5.4.5
                       (4.5.4.1/4.5.4.2 need 4.5.3.1; 4.5.4.3 needs only 4.5.1.4)
Track E  app UI/shell: 18.5.5.1 ─► 4.5.5.2 ─► 4.5.5.7 ─► 16.5.5.8
                       17.5.5.3 ─► 17.5.5.4 ─► 16.5.5.6   (16.5.5.6 also needs 16.5.4.5)
                       17.5.5.5 ∥ everything, after 4.5.1.4   (4.5.5.7 also needs track D through 17.5.4.4, and 17.5.5.4)
Track F  test equip:   4.5.6.1 ─► 4.5.6.2 ─► 4.5.6.3 ─► 4.5.6.4 ─► { 4.5.6.5 ∥ 4.5.6.8 } ─► 5.5.6.6
                       (needs only 4.5.1.4 — fully parallel with tracks B–E)
                       4.5.6.7 dev injector (after 16.5.5.8 + 4.5.4.2)
Track G  CI:           16.5.7.1 (no dependencies, ∥ everything) ─► 16.5.7.5 (Human, or agent on an authenticated gh CLI)
                       16.5.7.4 (after 16.5.5.8)   4.5.7.2 (after 4.5.6.4)   5.5.7.3 (after 5.5.6.6)
Track H  isolated:     5.5.9.1 (Human) ─► 4.5.9.2 (Human) ─► 6.5.9.3 ─► 5.5.9.4 (Human) ─► 5.5.9.5
                         ─► 16.5.9.6 ─► 16.5.9.7 ─► 4.5.9.9 (Human) ─► 16.5.9.10 ─► 16.5.9.11 (Human)
                         ─► 18.5.9.12 ─► 17.5.9.13 (Human) ─► 4.5.9.14 (Human) ─► 4.5.9.15 ─► 17.5.9.16 (Human)
                         ─► 5.5.9.22 (Human, the teardown)
                       16.5.9.19 (Human) ─► 16.5.9.6      16.5.9.21 ∥ 16.5.9.7, both after 5.5.9.5
                       4.5.9.17 (V2) and 17.5.9.18 (V3) run after 16.5.9.10/16.5.9.11 and before 5.5.9.22
                       4.5.9.20 (Human) only on 4.5.9.17's diagnostic branch, after 4.5.9.9
                       (5.5.9.1 through 16.5.9.7 need no Phase 5 code and run parallel to tracks A–G;
                        16.5.9.7 and 16.5.9.10 need an APK — 16.5.7.5's artifact, or a local assembleDebug;
                        16.5.9.8 branches off 16.5.9.7, also needing 16.5.5.8;
                        4.5.9.9 needs 5.5.7.3's pushed image; 16.5.9.10 needs 16.5.5.8;
                        4.5.9.17 needs the probe config in place, so run it before 4.5.9.9 where the APK is ready in time)
Track J  system:       5.5.10.1 (Human) ─► 5.5.10.2 ─► 5.5.10.3 (Human) ─► 5.5.10.4
                         ─► 16.5.10.5 ─► 16.5.10.6 (Human) ─► 19.5.10.7 ─► 22.5.10.10
                         ─► 19.5.10.8 (Human) ─► 5.5.10.9 (Human)
                       (5.5.10.1 needs every node's real image published by its own phase;
                        5.5.10.3 needs the Room slot track H releases at 5.5.9.22;
                        22.5.10.10 needs phase-4 21.4.3.4's checker and must read before 5.5.10.9 tears down)
```

- **Parallel:** tracks B, D-partial, F and G against each other once `4.5.1.4` has landed; `4.5.4.1 ∥ 4.5.4.2 ∥ 4.5.4.3` inside track D; `17.5.5.3 → 17.5.5.4 → 16.5.5.6` as its own chain; `17.5.5.5` against everything. The first seven subtasks of track H are parallel with **all** code work by design — `5.5.9.1` through `16.5.9.7` are canvas, deploy and ADB-tunnel work that needs no Phase 5 code, and must not wait for it.
- **Sequential:** every arrow above. Track A is strictly sequential and gates everything (a Gradle module graph cannot be built out of order). Tracks H and J are strictly sequential — each step's evidence depends on the previous step's Room state.
- **Track J follows track H on the Room budget.** Only two Rooms run at once and the comms track holds one, so the full blueprint deploys after the mini-blueprint's Room is released. Track J also waits on every node's real image, which its own phase publishes — nothing in Phase 5 builds them.
- **Spawn order:** `4.5.1.1` and `16.5.7.1` go to subagents together, since neither waits on anything. Track H opens at the same time — `5.5.9.1` and `16.5.9.19` wait on nothing. The rest of track H opens once `5.5.7.3` has pushed a verified image, and track J once every node's real image exists. The *car-sky* subtasks in both tracks are spawned to [[car-sky]] at the Room events they attach to; the *Human* subtasks are handed to a person.

### Critical path

The shortest ordered set that closes all five acceptance criteria:

`4.5.1.1 → 4.5.1.2 → 4.5.1.3 → 4.5.1.4 → 4.5.2.1 → 4.5.2.2 → 4.5.3.1 → 4.5.3.2 → 4.5.3.3 → 4.5.4.1 → 4.5.4.2 → 4.5.4.3 → 17.5.4.4 → 16.5.4.5 → 18.5.5.1 → 4.5.5.2 → 17.5.5.3 → 17.5.5.4 → 16.5.5.6 → 4.5.5.7 → 16.5.5.8 → (track F through 5.5.6.6) → 5.5.7.3 → 16.5.7.5 → 5.5.9.1 → 4.5.9.2 → 6.5.9.3 → 5.5.9.4 → 5.5.9.5 → 16.5.9.19 → 16.5.9.6 → 16.5.9.7 → 4.5.9.9 → 16.5.9.10 → 16.5.9.11 → 18.5.9.12 → 17.5.9.13 → 4.5.9.14 → 4.5.9.15 → 17.5.9.16`

with **`5.5.9.1` → `16.5.9.7` running alongside it**, unblocked from the start — it does not sit on the critical path but it decides whether the path's last steps are in-Room or on an emulator.

**The isolated IVI test closes this path** — `4.5.9.9` through `17.5.9.16` turn a built app into the four proofs of [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance). Its first seven subtasks — the blueprint, the deploy and the ADB tunnel — need no APK and can be done while tracks A–F are still running; only `4.5.9.9` onward waits on `5.5.7.3`'s pushed image.

**The system test is not on the path to the five acceptance criteria.** Every criterion is closed by the isolated IVI test against the R4 simulator feed, which is what "mock-driven" means for this phase. The system test waits on the other tracks' real images and produces the whole-system evidence Phase 6's convergence run builds on, and it is not a Phase 5 gate.

**Droppable without failing a criterion, in this order if time runs short:** `4.5.1.5` (ProGuard rules — release build is not an acceptance target), `17.5.5.5` (the banner is built for D11's completeness and mounted nowhere), `4.5.6.7` (dev injector — only needed if the ADB/UI route is awkward), `4.5.2.3` and `4.5.3.5` (extra test depth, not extra behaviour), `state-stream.json` inside `4.5.6.4` (the periodic `state` message is optional on the producer side and no criterion depends on it).

## Acceptance traceability

Every Phase 5 acceptance criterion in [milestone1.md](milestone1.md#phase-5--ivi-hmi-mock-driven-r16-r17--display-track-parallel-from-the-start) maps to at least one subtask.

| Phase 5 acceptance criterion | Closed by |
|---|---|
| The HMI runs on the AAOS node with the R16 layout; button/app areas switch the Display area | `16.5.5.6` · `16.5.5.8` (the launcher entry) · `4.5.5.7` · `16.5.4.5` · deployed by `5.5.9.1`–`5.5.9.4`, confirmed `Running` by `5.5.9.5`, the ADB tunnel started by `16.5.9.6` and proven by `16.5.9.7`, installed and launched by `16.5.9.10`, observed by `16.5.9.11` |
| **(Dev)** A mock R4 warning brings the warning view up with ego, B and ghost C at the composed positions | `4.5.2.2` · `4.5.3.3` · `4.5.4.2` · `17.5.4.4` · `16.5.4.5` · `16.5.5.6` · simulator `4.5.6.3`/`4.5.6.4` (`approach.json`) · fed to the node by `4.5.9.9` · dev path `4.5.6.7`, exercised in the Room by `17.5.9.18` (I3) · read by `18.5.9.12` and seen by `17.5.9.13` (I4), with the network hop under it closed by `4.5.9.17` |
| Ghost C renders from `v2x_relayed` data only; the 2D drawing is delivered | **`17.5.4.4`** (the D12 snapshot wiring that arms the guard — without it the guard silently disables) · **`17.5.5.4`** (the renderer and the guard itself, under test) · `16.5.5.6` · `17.5.5.3` · `4.5.3.3` (`cSource=` on every `[RX]`) · `degrade.json` guard-trip step in `4.5.6.4` · `cSource=v2x_relayed` on every warning evidenced in text by `18.5.9.12` and on screen by `17.5.9.13`, the guard trip gated by `17.5.9.16`, and the whole shown again from live relayed data by `19.5.10.7`/`19.5.10.8` |
| A newer message with an unknown `warningType` degrades gracefully | `4.5.1.4` (the committed `R4AdditiveVersionTest` relocated and still green) · `4.5.2.2` (decode preserves the value, D4) · `4.5.4.3` (`WarningClassifier` generic presentation) · `4.5.6.4` (`degrade.json`) · read by `4.5.9.15` and observed by `17.5.9.16` |
| Optional paths, only if built | **Not built in M1** — declared, not attempted. 3D stays reachable through the **`IviWarningViewSeam`** render seam ([HLD §8](../IVI_ECU/doc/ivi-ecu-hld.md#8-interfaces-ports-and-the-layer-rule)), which is what a second renderer would realize; multi-process wake-on-warning stays reachable because `4.5.5.2` chose the foreground service (D5). Recorded as "not built" in `plans/doc/phase5-ivi-run.md` by `17.5.9.16`, per [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance)'s instruction to record rather than leave the criteria ambiguous. |
| *(no Phase 5 criterion)* **R22** — the first R4 lands in (`T0` + 8.0 s, `T0` + 10.0 s) and the HMI holds Home until then | `22.5.10.10` (K6 and K7 on the system-test run) · `17.5.4.4` (D13's lifecycle, which is what keeps Home showing through the `low`s) · `4.5.6.8` (the D13 case on demand) · producer side: Phase 1 group 1.13, Phase 3 `12.3.2.8`/`22.3.6.3`, Phase 4 `21.4.3.4` |

**What group 5.10 adds.** The system test (`5.5.10.1`–`22.5.10.10`) closes no Phase 5 acceptance criterion on its own — all five are met against the R4 simulator feed. It proves the same IVI behaviour inside the full topology with every node on its real image, it is the only configuration that can produce R22's K6 and K7, and its record is what Phase 6's convergence run starts from.

## Open items

Carried, not decided. No Phase 5 subtask may close one of these by assuming an answer.

| # | Item | Owner / closes at |
|---|---|---|
| 1 | **The walkthrough and the HLD disagree about what the God View draws.** [§4.8](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) rung V4 link 4 and [§6](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#6-expected-outputs-and-acceptance) proof 4 expect a `[V2X]` badge and `d_AB`/`d_AC` distance labels; [HLD §6](../IVI_ECU/doc/ivi-ecu-hld.md#6-internal-components), D11 and **R17 itself** forbid them — R17 names that annotated figure explanatory and *not* what the IVI renders. **User decision: follow R17 and the HLD — no badge, no distance labels.** `17.5.9.13` and `19.5.10.8` judge against the HLD | Report to [[project-researcher]] to correct §4.8 and §6 — [walkthrough-driven-delivery.md](../.claude/rules/walkthrough-driven-delivery.md) forbids any other agent editing a walkthrough. **Raise before `17.5.9.13` judges a screen** |
| 2 | **ADB reach to the Skycraft guest** — one route exists, the organizers' `reach-backend` tunnel, unexercised by this team ([§6.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) item 1). Its CLI, gateway URL and token are supplied, not derived (items 2–4) | `16.5.9.19` obtains the three inputs, `16.5.9.6` starts the tunnel, `16.5.9.7` proves it; none needs Phase 5 code and all three are scheduled first. Negative ⇒ every later subtask in group 5.9 degrades to AAOS-emulator evidence |
| 3 | **AAOS guest Android version** vs `minSdk 29`, and the `automotive` feature | `16.5.9.7`, same connection |
| 4 | Coroutines version skew between `:observer` and what AndroidX resolves in `:app` | Mitigated by the catalog (`4.5.1.1`); a skew shows as a runtime `NoSuchMethodError`, not a build failure — watch for it at `16.5.9.10` |
| 5 | Deployment budget: 2 concurrent Rooms | `5.5.9.22` and `5.5.10.9` tear their Rooms down; coordinate with the comms track before `5.5.9.4` deploys |
| 6 | MTU headroom (Phase 0 open item O3) | Non-issue for this hop — an R4 warning is ~450 B against a 2048 B buffer — but still formally open |
| 7 | **AAOS boot-to-listener time is a constraint on this node, not an input to the bench's `start_delay_s`.** That key is set by the ADA detector's warm-up ([§6.6(g)](../requirements/m1-run-timing-and-event-triggering.md), Phase 3 `22.3.6.3` → Phase 1 `22.1.13.4`) and is not free to absorb the guest's boot time. R22 states the requirement directly here instead: **the app must be listening on its R4 port before `T0` + 8.0 s**, the earliest instant a warning can arrive ([§4.2](../requirements/m1-run-timing-and-event-triggering.md)). `16.5.9.10` produces the boot-to-listener number; if it exceeds that window the finding is this node's, and the operator's R5 Deployment-Viewer check before recording is what covers it. **No startup handshake is coming** — the topology has no reverse path for a barrier | `16.5.9.10`, then **user** |
| 8 | **`18.5.5.1` and `18.5.9.12` carry `X = 18`, and R18's definition does not cover this node.** R18 defines structured JSONL event logs **on the V2X ECU and ADA**; the IVI's `key=value` logcat lines are neither JSONL nor produced on those nodes. The two IDs are published and are not renumbered ([task-planning-conventions.md § Task ID scheme](../.claude/rules/task-planning-conventions.md#task-id-scheme)), and the group headers of 5.5, 5.9 and 5.10 name R18 because those subtasks sit in them | [[project-researcher]] — either widen R18's definition to the IVI's evidence surface, or state that the IVI's log evidence traces to R4 alone. **Raise before Phase 6 reads the R18 event list** |
| 9 | **Committed comments name task IDs this plan assigns different work to.** `IVI_ECU/app/proguard-rules.pro` and `IVI_ECU/app/src/main/java/…/model/SceneGeometry.kt` cite `4.5.1.1`; `IVI_ECU/app/build.gradle.kts` cites `4.5.1.4`; `SceneGeometry.kt` and `R3Snapshot.kt` cite `17.5.3.1` and `17.5.3.4`, which this plan does not assign at all. The IDs in this file are the live assignment. Neither set is renumbered, and `4.5.1.4` moves those files byte-identically, so its `git mv` cannot refresh the comments | [[project-planner]] — the comment in each file is corrected by the next subtask that rewrites that file for its own objective (`17.5.5.4` for `CanvasWarningView.kt`), never by a subtask opened to edit comments |
| 10 | **`gh` CLI authentication on the execution host.** [§5](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#5-work-division-between-ai-and-human)'s first qualification flips the run-confirmation and artifact-download rows from Human to AI on a host with `gh auth login` run. Which state applies is unrecorded | **user** — the answer is recorded at `16.5.7.5`, which is where it bites, and decides whether that subtask is handed to a person or spawned to a subagent |
| 11 | **A JDK and an Android SDK on the build host** ([§6.1](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) item 8). Every `./gradlew` acceptance in groups 5.1–5.7 rests on it, and `5.5.6.6` records that `docker build` on this host is unavailable, so the host's tooling is partial | `4.5.1.1` records `java -version` and `ANDROID_HOME`. Negative ⇒ the CI route of [§3](../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#3-building-on-ci) is the fallback, and `16.5.9.7`'s local-build option is unavailable |

## Out of scope for this phase

Each is a decision with its reason.

- **3D and multi-process wake-on-warning** — optional, not committed M1 deliverables (D11). No subtask attempts either, and no 3D stub is created. The optional path stays open through the `IviWarningViewSeam` render seam rather than through a reserved file path, and `4.5.5.2`'s foreground service keeps multi-process reachable.
- **The ego video clip in the Display Area** — deferred by the report's §4 decision record and itemized in [milestone1.md §6](milestone1.md#6-deferred-to-later-milestones); no subtask implements any part of it, including a `DisplayMode`, a media dependency or a `video` pin.
- **`WarningBannerOverlay` mounted in the Display Area** — forbidden by a standing user decision (D11). The God-View canvas must render unobstructed.
- **Robolectric, a coverage threshold, and LeakCanary** — none has a basis in R4/R16/R17 acceptance; the acceptance criteria are behavioural, and D2's plain-JVM split is what removes the need for Robolectric (HLD §11, §12).
- **Runtime JSON-Schema validation on the device** — the typed decode already enforces required fields and types; the schema is enforced in the round-trip tests on both sides (HLD §6).
- **Real ADA data.** Phase 5 is mock-driven by definition; the simulator honours the real ADA node's env var *names* so Phase 6 is an image swap with no node-config edit (HLD §7).
- **A listener that gives up after N socket errors.** R19 requires one continuous run, so `4.5.3.4`'s bounded back-off never stops retrying.
- **A `--cycles`/`--interval-ms` sender CLI.** Repetition and cadence are scenario data (`loop`, `defaultRateHz`), not flags — D9's "scenarios are data, not code".
- **A service-bind latency criterion.** Neither R4, R16 nor R17 states a latency requirement, and no acceptance criterion turns on it.

---

*9 task groups, 72 subtasks: 40 agent, 14 car-sky, 17 Human, and one (`17.5.9.18`) split between a car-sky command and a human judgement. **There is no task group 5.8.** Nothing started except `16.5.7.1`.*
