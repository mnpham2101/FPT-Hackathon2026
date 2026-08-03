# Phase 5 IVI HMI branch review — PR #2 and the `-complete` head

Review of the two unmerged Phase 5 IVI branches against [phase5_minh_tasks.md](../phase5_minh_tasks.md) and the [Phase 5 HLD](../../IVI_ECU/doc/phase5-ivi-hld.md).

| Covered | Head | Sections |
|---|---|---|
| PR **#2 `Feat/phase5 ivi hmi dev`** | `f7f2f55` | § 1–§ 7 |
| `feat/phase5-ivi-hmi-complete`, **rebased onto `main` (`74b0aa7`)** | `06cb936` | § 8–§ 11 |

§ 1–§ 7 keep their original scope — the `-dev` branch and the pre-rebase `-complete` tip (`1e36cdc`). § 8 re-checks their defect list against `06cb936`; § 9 reconciles [phase5_bach_tasks.md](../phase5_bach_tasks.md) (which exists only on the branch) against the Lead plan; § 10 supersedes § 5's completion table; § 11 supersedes § 6's manual guide.

**Nothing in the plan is checked off by this document.** A subtask is done only when its own commit lands on the phase branch with build and tests green; neither branch is on `main`. What follows is *evidence about how much work already exists*, so no one retypes it.

## 1. What the PR actually contains

PR #2's head is the branch `feat/phase5-ivi-hmi-dev`, which is a **strict ancestor of `feat/phase5-ivi-hmi-complete`** — verified: `git merge-base --is-ancestor` holds one way and not the other. So there is a second, larger branch with everything in this PR plus more.

| | PR #2 (`-dev`, `f7f2f55`) | `-complete` (`1e36cdc`) |
|---|---|---|
| R4 models, deserializer, listener service, repository, `WarningViewModel` | ✅ | ✅ |
| Python mock sender + Dockerfile | ✅ | ✅ |
| 2-node blueprint JSON + guide + answer doc | ✅ | ✅ |
| `MainActivity`, `IviApplication`, `AppModule` (Hilt) | ❌ | ✅ |
| `MainScreen` wired to the view seam | ❌ | ✅ |
| Wake-on-warning in `MainViewModel` | ❌ | ✅ |
| `SceneCoordinateMapperTest`, `CanvasWarningViewTest`, `MainViewModelTest`, `R4RepositoryTest`, integration test | ❌ | ✅ |
| APK deployment doc + completion report | ❌ | ✅ |

**Review and merge the superset, not the PR.** Everything below applies to both unless stated; where the two differ, `-complete` is the later state of that file.

## 2. Verified, not assumed — both branches were built and tested

Both branches were checked out into worktrees and actually built on 2026-08-02 — Android Studio JBR (JDK 21), local SDK, `--no-daemon`. Nothing in this section is inferred from reading code.

| Check | PR #2 (`-dev`) | `-complete` |
|---|---|---|
| `:app:testDebugUnitTest` | **BUILD SUCCESSFUL** — 15 tests, 0 failures | **BUILD SUCCESSFUL** — 34 tests, 0 failures |
| Test classes | `R4DeserializerTest` (5), `WarningViewModelTest` (4), plus the frozen-contract `R4RoundTripTest` (3) and `R4AdditiveVersionTest` (3) inherited from `main` | adds `R4RepositoryTest` (4), `MainViewModelTest` (3), `CanvasWarningViewTest` (5), `SceneCoordinateMapperTest` (5), `FullStackIntegrationTest` (2) |
| `assembleDebug` | **BUILD SUCCESSFUL** — 24.49 MB | **BUILD SUCCESSFUL** — 24.88 MB |
| `aapt dump badging` → `launchable-activity` | **absent** | `com.hackathon.v2x.ivi.MainActivity` |

**The single most important fact about PR #2: its APK has no launchable activity.** `aapt` reports the package and no `launchable-activity` line, because the manifest declares a `<service>` and no `<activity>`. The APK installs, and nothing can start it. Every piece of UI on this branch is unreachable on a device. That is not a criticism of the data layer — it is why PR #2 alone cannot produce a demo, and why the deployment guide in § 6 uses `-complete`.

Note also that the passing `R4AdditiveVersionTest` does **not** vindicate the deserializer's unknown-`warningType` handling: that test decodes through the model binding directly and never touches `R4Deserializer`, so the rewrite described in § 4.1 is invisible to it.

## 3. His plan — the 2-node test blueprint

The PR carries a genuine planning artifact: [`plans/doc/task51-2node-blueprint-answer.md`](https://github.com/) plus `requirements/blueprint-2node-task51-test.{json,md}`. Its proposal:

```
[Mock R4 Sender]  ──eth──►  [Ethernet Bridge]  ◄──eth──  [IVI ECU]
 Container, Python           config: null                 Skycraft AAOS
 10.88.0.11                                               10.88.0.12:5004
```

### 3.1 What it gets right

- **The core insight is correct and matches this plan's own conclusion**: the display track does not need the bench or the V2X ECU, so a reduced topology is the right test target. Both plans arrived at that independently.
- **It knows the platform's hardest limitation** — that ETHERNET pins cannot be created by REST or by JSON import, and says so in bold as a mandatory manual step. That is the trap most people fall into.
- **The scenario shape is better than mine was**: approach → state heartbeat → **leaving** → unknown type, cycled. The explicit *leaving* phase exercises the warning timeout and the mode restore without waiting for the stream to stop. **Adopted** into `4.5.6.4`.
- **It records Skycraft node config fields no document in this repo has**: `prefix`, `gpuBackend: "virglrenderer"`, `displayWidth: 1280`, `displayHeight: 720`. The display size is exactly what the committed R16 previews are drawn for. **Adopted** into `5.5.8.1` as values to verify against a live export.
- **It proposes REST evidence routes** (`.../screenshot`, `.../shell`) that would not depend on ADB at all. Both were 502 in Phase 0, but a working `screenshot` endpoint would be a second evidence path. **Adopted** into `16.5.8.3` as an explicit probe.

### 3.2 What is wrong with it

Five defects, each of which stops a deploy or produces a wrong result:

| # | Defect | Consequence |
|---|---|---|
| 1 | **The Skycraft node has no `image` artifact block** — its config is `prefix`/`gpuBackend`/display only | Deploy is rejected outright: `invalid blueprint: node 'IVI ECU': skycraft requires 'image' config with VM image artifact details`. This is the known #1 Skycraft failure |
| 2 | **The Ethernet Bridge's `config` is `null`** — no `bridgeMode`, no `subnet` | The `10.88.0.x` addresses describe a network that was never defined |
| 3 | **Import-then-add-pins is the fragile route** | Import silently drops pins, so both nodes need pins hand-drawn. Cloning the working `trial2_minh_netcheck` and deleting the unwanted nodes **preserves** the pins — zero manual pin work, and it starts from a topology that has already passed C1–C5 |
| 4 | **`registry.carsky.io`**, and a plain `docker build` | That host 502s from outside; the push host is `registry.hackathon-2.carsky.io`. A plain build also produces a multi-platform manifest index, which this cluster cannot pull — it needs `--platform linux/arm64 --provenance=false --sbom=false` |
| 5 | **Subnet `10.88.0.x` and port `5004`** | The frozen R6 topology is `10.99.0.0/24` with the IVI at `10.99.0.13` and ADA→IVI on **`47300`**. Two nodes agreeing with each other on the wrong numbers still fails the moment the real ADA ECU appears at Phase 6 |

**Reconciliation:** the reduced-topology idea is kept and the three good findings above are folded into [phase5_minh_tasks.md](../phase5_minh_tasks.md); the blueprint JSON itself is not imported, and the addresses and port stay at the frozen values.

## 4. Defects in the code

These are what a merge would bring with it. Two are blocking.

### 4.1 Blocking — the R19 provenance guard is inert

`WarningViewModel.onWarningReceived` does:

```kotlin
_latestScene.value = event.geometry
```

`SceneGeometry` decoded from the wire's `geometry` object has `vehicleCSnapshot = null`, and `CanvasWarningView` treats a `null` snapshot as **trusted**. The R3 snapshot that carries `source` lives in the message's *sibling* `object` field and is never copied across. So on every real message the guard passes without checking anything — and the guard is the mechanism behind R19's "ghost C came only from `v2x_relayed`". `CanvasWarningViewTest` on `-complete` passes because it calls the guard helper directly with a hand-built snapshot; nothing tests the wiring.

Fix: compose the scene as `warning.geometry.copy(vehicleCSnapshot = warning.objectSnapshot)`. This is subtask `17.5.4.4`, with a named regression test.

### 4.2 Blocking — silent datagram truncation

`R4ListenerService.openSocketAndReceive` allocates one `DatagramPacket` and reuses it across the loop without ever calling `packet.setLength(buffer.size)` before `socket.receive(packet)`. The JDK sets the packet's length to each received message's size, and the *next* receive reads at most that many bytes. One short datagram permanently shrinks the receive window, and every later message is truncated — which surfaces as intermittent `MALFORMED` parse failures that look like a producer bug.

Fix: reset the length every iteration. Subtask `4.5.3.2` exists for this one line, with a regression test that sends a long datagram then a short one.

### 4.3 Contract weakening

- **`R3Snapshot` required fields were given Kotlin defaults** (`distance = 0f`, `state = "tracked"`, `confidence = 1.0f`, `timestamps = R3Timestamps()`), and `SceneGeometry.vehicleC` defaults to `null`. The frozen R3/R4 schemas mark these required. A producer that omits `distance` now decodes to a silent, plausible-looking `0.0 m` instead of failing — the worst kind of failure in a distance-warning system. These defaults exist to let the mock sender omit fields; the mock should be fixed, not the contract.
- **`R4ServiceError` was added as a `@Serializable @SerialName("error")` subclass of the frozen `R4Message` sealed type.** A transport-level condition is now a wire message type that does not exist in `r4-ada-ivi.schema.json`, and it is serialisable, so it can be emitted. Transport state belongs in a separate link-state type — `R4LinkState` in the plan's `:observer` module.

### 4.4 Unknown `warningType` is rewritten

`R4Deserializer` replaces any unrecognised `warningType` with the literal `"unknown"`. The frozen contract says an unknown value must *degrade gracefully at the consumer*, not that it be overwritten, and the committed `R4AdditiveVersionTest` asserts the wire value survives. Rewriting at parse time destroys the one piece of information the log needs — *which* unknown type arrived. Classification belongs at the UI edge (`4.5.4.3`).

### 4.5 Smaller items

- Hardcoded tunables in `R4ListenerService`: `BUFFER_SIZE = 4096`, `MAX_RETRIES = 5`, `RETRY_DELAY_MS = 1000`, notification id and channel — against CLAUDE.md principle 5, which the same file honours for the port.
- The retry policy **gives up after 5 attempts** and emits a terminal error. For a recorded demo, a listener that stops trying is worse than one that keeps rebinding with bounded back-off.
- The mock sender's `state` message does not match the R4 schema at all: it emits `vehicles.ego = {position:{x,y}, speed}` and a key `B`, where the schema has `vehicles.ego = {x,y}` and `vehicleB`. That message cannot decode in the app — the state path has never actually worked end to end.
- The mock sender writes its own payloads from Python literals rather than loading the frozen `contracts/samples/`, so it is a second, unversioned copy of the contract — precisely how producer and consumer drift apart.
- `-complete` adds `IVI_ECU/app/src/main/res/values/themes.xml` and `IVI_ECU/deployment/`; neither location is designated by the HLD, and `deployment/` is not a sanctioned folder under [node-code-layout.md](../../.claude/rules/node-code-layout.md).

## 5. What the plan's subtasks already have working code for

Mapping the branches onto [phase5_minh_tasks.md](../phase5_minh_tasks.md). **None of these is marked done in the plan** — the plan targets a five-module structure that neither branch has, so in every row the existing code is a *starting point inside one module*, not a finished subtask.

| Plan subtask | Existing code | How complete |
|---|---|---|
| `4.5.2.2` R4 decoder | `data/R4Deserializer.kt` + 5 tests | **~60 %** — structure and failure taxonomy are sound. Must drop the `warningType` rewrite (§ 4.4) and take `offset`/`length` |
| `4.5.3.2`/`4.5.3.3` socket + loop | inside `service/R4ListenerService.kt` | **~50 %** — loop, back-off and flow are there but fused into the service and carrying the truncation bug (§ 4.2). Needs splitting into the plain-JVM `:observer` |
| `4.5.3.4` back-off | same file | **~70 %** — policy exists; change "give up after 5" to bounded back-off that keeps retrying |
| `4.5.4.2` repository | `data/R4Repository.kt` (+ test on `-complete`) | **~70 %** — routing is right; no last-value-wins by `seq`, no `droppedCount`, no `inject()` |
| `17.5.4.4` warning view-model | `ui/WarningViewModel.kt` + `WarningUiState.kt` + 4 tests | **~70 %** — `Idle ↔ Active` and the timeout work. **Missing the R19 snapshot composition** (§ 4.1), which is the point of the subtask |
| `16.5.4.5` wake-on-warning | `ui/MainViewModel.kt` on **`-complete`** + test | **~90 %** — `previousMode`, `userOverrodeDuringWarning` and the four cases all match the brief. The closest thing to finished on either branch |
| `4.5.5.2` foreground service host | `service/R4ListenerService.kt` + manifest `<service>` | **~60 %** — notification, channel and `foregroundServiceType` are right; it must stop owning the loop |
| `16.5.5.4`/`16.5.5.5` app shell | `IviApplication`, `MainActivity`, manifest on **`-complete`** | **~60 %** — manifest shape and LAUNCHER entry are correct; wiring is Hilt, which D7 replaces with `IviGraph` |
| `17.5.5.6` mount the view seam | `ui/screen/MainScreen.kt` on **`-complete`** | **~80 %** — seam mounted, lifecycle-aware collection, banner correctly not mounted. Status bar still hardcoded |
| `17.5.5.8` mapper test | `SceneCoordinateMapperTest` on **`-complete`**, 5 tests green | **~80 %** — reusable nearly as-is |
| `17.5.5.9` guard test | `CanvasWarningViewTest` on **`-complete`**, 5 tests green | **~90 %** — the branch already has essentially this test, including the `isGhostCSourceTrusted` / `ghostCSourceGuardErrorMessage` helper extraction the subtask asks for |
| group 5.6 simulator | `mock-sender/` (Python) | **~20 %** — the *scenario* is reusable as a specification; the code is not (§ 4.5) |
| `5.5.8.1` deploy doc | `blueprint-2node-*`, `deployment/phase5-ivi-deploy.md` | **~40 %** — much of the prose is reusable; the values and the location are not (§ 3.2) |
| Everything in groups 5.1, 5.7, and 5.9 | — | **0 %** — no module split, no version catalog, no CI lane, no in-Room evidence |

Rough overall: **the branches cover perhaps half of the plan's code volume, and none of its structure.** The honest way to use them is file-by-file, per the plan's § Prior work — not a merge.

## 6. Deploying and testing his work by hand

This is the fastest route to seeing something on screen today. It uses **`feat/phase5-ivi-hmi-complete`**, not PR #2, because PR #2's APK cannot be launched (§ 2).

It also uses the **frozen** addresses and port, not the branch's. That means one build-time edit — worth it, because it is the only way the result tells you anything about the real topology.

### Step 0 — get the branch and fix the two constants

```bash
git fetch origin feat/phase5-ivi-hmi-complete
git worktree add ../ivi-demo origin/feat/phase5-ivi-hmi-complete
```

In `../ivi-demo/IVI_ECU/app/build.gradle.kts`, change the port default:

```kotlin
buildConfigField("int", "R4_UDP_PORT", "47300")   // was 5004
```

Leave everything else alone — the sender's target is an environment variable, so nothing else is baked in.

### Step 1 — build the APK

```bash
cd ../ivi-demo/IVI_ECU
# Windows: set JAVA_HOME to the Android Studio JBR first
export JAVA_HOME="/c/Program Files/Android/Android Studio/jbr"
./gradlew assembleDebug
```

Output: `app/build/outputs/apk/debug/app-debug.apk`, ≈24.5 MB. Confirm it is launchable before going near the platform — this one check separates the two branches:

```bash
"$ANDROID_HOME/build-tools/35.0.0/aapt" dump badging app/build/outputs/apk/debug/app-debug.apk | grep launchable-activity
# expect: launchable-activity: name='com.hackathon.v2x.ivi.MainActivity' ...
```

### Step 2 — build and push the sender image

The branch's `Dockerfile` is fine; its documented build command is not. Build single-platform arm64 and push to the host that answers:

```bash
cd ../ivi-demo/IVI_ECU/mock-sender
docker login registry.hackathon-2.carsky.io -u <registry-account>   # password = the zak_… Zot API key
docker buildx build --platform linux/arm64 --provenance=false --sbom=false \
  -t registry.hackathon-2.carsky.io/m1-mock-r4-sender:latest --push .
```

Multi-platform is not a style choice here — a manifest index fails to pull on this cluster.

If you have no Docker host, skip to § 6.1 and test on an emulator instead.

### Step 3 — build the Room by cloning, not importing

Do **not** import `blueprint-2node-task51-test.json` (§ 3.2). Instead:

1. Nydus → Blueprint list → clone `trial2_minh_netcheck` (Export Selected → Import from File, or `POST /api/v1/blueprints/{id}/clone`).
2. Rename the clone, e.g. `trial3_minh_ivi`.
3. Delete the **Bench** and **V2X ECU** nodes on the canvas. The ADA and IVI pins and their bridge edges survive — this is the whole reason for cloning.
4. Read the config back and check it before deploying — `GET /api/v1/blueprints/{id}` shows each node's stored config verbatim, which the Inspector's truncated fields do not. Expect one `ETHERNET`/`OUTPUT` pin per role node at `10.99.0.12` and `10.99.0.13`, both wired to the bridge's single `INPUT` pin.
5. Configure the **ADA node** as the sender:

   | Field | Value |
   |---|---|
   | Image | `registry.hackathon-2.carsky.io/m1-mock-r4-sender:latest` |
   | Command | *(leave empty — the image's own `CMD` runs)* |
   | Env | `IVI_ECU_HOST=10.99.0.13`, `IVI_ECU_PORT=47300`, `INTERVAL_MS=2000`, `CYCLES=20` |

6. **Do not touch the IVI node.** Its `image` artifact block (AAOS `x9oqgIwzTp1m26SWIQqJt` / `xSU_Q7YJZUxxUgDr4Ugcp`, `0.0.1`, `aarch64`) is what makes the deploy legal.
7. Inspector → **New Deployment** → pick an existing Device → Deploy. Wait for every node `Running` with restart count 0. The Android node is the slow one.

Edit the original blueprint, never the `<name>-deploy` snapshot that deploying creates.

### Step 4 — install the APK

```bash
adb connect <skycraft-adb-endpoint>     # from the Rework device panel or the CarSky Gateway ADB tunnel
adb devices                             # must show 'device', not 'offline'
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.hackathon.v2x.ivi/.MainActivity
```

**This is the step most likely to fail, and it has never been proven on this deployment.** The REST VM-shell route answers 502, so the ADB tunnel is the only route, and it is untested. If `adb connect` does not give you a `device`, stop and go to § 6.1 — do not keep retrying blind.

### Step 5 — watch it work

```bash
adb logcat -s IVI_V2X R4ListenerService R4Deserializer WarningViewModel
```

| What you should see | Meaning |
|---|---|
| ADA node **View Log**: `[TX] …` lines every 2 s | the sender is alive |
| logcat: the service logs its socket open on 47300 | the listener bound |
| Display Area switches to the Warning View by itself | wake-on-warning |
| Ego, B and a dashed ghost C with a pulsing glow and a `[V2X]` badge | R17 delivered |
| The `future_unknown_type` packet arrives, no `FATAL EXCEPTION` | R4 additive-version behaviour |

Two things this run will **not** prove, by construction:

- **The provenance guard.** With § 4.1 unfixed, ghost C renders whatever the source says. The demo looks right and proves nothing about R19.
- **The `state` message path.** The sender's state payload does not match the schema (§ 4.5); expect it to be logged as a parse failure. That is the sender being wrong, not the app.

Tear the Room down when finished — only two deployments may run at once.

### 6.1 Fallback — no Room, no ADB

Everything except the R6 hop can be seen on an **Android Automotive** emulator (a phone image will refuse the APK: the manifest requires `android.hardware.type.automotive`).

```bash
adb install -r app-debug.apk
adb shell am start -n com.hackathon.v2x.ivi/.MainActivity
```

`adb forward` cannot carry UDP, so send from **inside** the emulator, or open a console redirect:

```bash
# option A — console redirect (token in ~/.emulator_console_auth_token)
telnet localhost 5554
auth <token>
redir add udp:47300:47300
# then, on the host:
IVI_ECU_HOST=127.0.0.1 IVI_ECU_PORT=47300 CYCLES=2 python mock_r4_sender.py

# option B — push the sender into the guest and run it there (no NAT involved)
```

Before either, sanity-check the sender alone — it needs no Android at all:

```bash
nc -ul 47300                                                    # terminal 1
IVI_ECU_HOST=127.0.0.1 IVI_ECU_PORT=47300 CYCLES=1 python mock_r4_sender.py   # terminal 2
```

## 7. Recommendation

**Do not merge PR #2 as-is.** It builds and its tests pass, but it would put two live defects (§ 4.1, § 4.2) and a weakened contract (§ 4.3) onto `main`, and it lands a data layer whose UI cannot be started.

Either:

- **Preferred** — keep both branches as reference and execute [phase5_minh_tasks.md](../phase5_minh_tasks.md), lifting file-by-file per its § Prior work. The five-module split is what makes the serializer and the observer testable in CI with no device, and that is where the two blocking defects get their regression tests.
- **If time forces it** — merge `-complete` (not PR #2) onto a phase branch as a starting commit, then fix § 4.1 and § 4.2 *first*, before anything else is built on top. The R19 guard defect in particular is invisible in a demo: everything looks correct while the check that the whole milestone rests on is switched off.

---

## 8. Defect status at the rebased `-complete` head (`06cb936`)

`06cb936` is 23 commits ahead of `main` and 0 behind — the rebase took `main`'s `R4Message.kt` and `SceneGeometry.kt` verbatim, so the § 4 findings need re-checking rather than re-quoting. **§ 7's recommendation stands unchanged**: one of the two blocking defects is still live, and the branch still targets a structure the plan does not have.

| § | Finding | Status at `06cb936` |
|---|---|---|
| 4.1 | R19 provenance guard inert | **Fixed** |
| 4.2 | Silent datagram truncation | **Live — blocking** |
| 4.3a | `R3Snapshot` required fields defaulted | **Persists, widened** |
| 4.3b | `R4ServiceError` in the sealed `R4Message` | **Fixed**, with a residue |
| 4.4 | Unknown `warningType` rewritten to `"unknown"` | **Fixed** |
| 4.5 | Hardcoded tunables, give-up-after-5, mock schema mismatch, `deployment/` | **Mixed** — see below |
| — | Status bar reports `BOUND` unconditionally | **New — blocking a demo claim** |
| — | Suspending `emit` inside the receive loop | **New** |
| — | Documentation authority conflicts | **New** |

### 8.1 Fixed

- **R19 wiring (§ 4.1).** [`WarningViewModel.kt:70`](../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/ui/WarningViewModel.kt) composes `event.geometry.copy(vehicleCSnapshot = event.objectSnapshot)`. A regression assertion exists — `WarningViewModelTest.kt:133-135` asserts the composed scene's `vehicleCSnapshot` is non-null and its `source` is `v2x_relayed`. Two gaps remain against `17.5.4.4`: no `own_sensor` carry-through case (so nothing proves an untrusted snapshot reaches the guard), and the event is hand-built rather than decoded from `/contracts/samples/r4-warning.json`. A **second, redundant composition** at `MainScreen.kt:248` re-applies the same `copy` in the composable — harmless today, two places to keep in step tomorrow.
- **Unknown `warningType` (§ 4.4).** `R4Deserializer.parseWarningEvent` now logs a non-registry value and returns the event unmodified; nothing writes `"unknown"` back. D4 is satisfied at the parser.
- **`R4ServiceError` (§ 4.3b).** Gone — `model/R4Message.kt` is byte-identical to `main`. Residue: `WarningUiState.Error` ([`WarningUiState.kt:16`](../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/ui/WarningUiState.kt)) still models transport failure as a UI state, which is the `R4LinkState` the plan puts in `:observer` (`4.5.3.1`).
- **Mock sender `state` shape (§ 4.5).** `make_state_message` now emits `vehicles.{ego,vehicleB,vehicleC}` with `{x,y}` positions and decodes against the frozen schema. The state path works for the first time.

### 8.2 Still live

- **Datagram truncation (§ 4.2) — blocking.** [`R4ListenerService.kt:118`](../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/service/R4ListenerService.kt) allocates `DatagramPacket(buffer, buffer.size)` outside the loop, and `:124` calls `socket.receive(packet)` with no `packet.setLength(buffer.size)` first. Every datagram after the shortest one seen is silently truncated. § 11.6 shows the mock sender's own scenario triggering this within one cycle.
- **`R3Snapshot` defaults (§ 4.3a) — widened.** `R3Snapshot.kt` now defaults `objectClass`, `distance`, `speed`, `confidence`, `state` and `timestamps`, and `R3Timestamps` defaults all three fields. [`contracts/r3-tracked-object.schema.json`](../../contracts/r3-tracked-object.schema.json) marks all nine fields required. The cause is visible in the same PR: `mock_r4_sender.py:38-46` omits `class` and `timestamps`, so the model was relaxed to let the mock parse. **Fix the producer, not the contract** — a missing `distance` currently decodes to a plausible `0.0 m` in a distance-warning system.
- **Hardcoded tunables (§ 4.5).** `R4ListenerService` companion: `BUFFER_SIZE = 4096`, `MAX_RETRIES = 5`, `RETRY_DELAY_MS = 1_000L`, `NOTIFICATION_ID = 1001` (`:155-158`).
- **Give-up-after-5 (§ 4.5).** `:97-100` and `:106-109` return from the loop after five consecutive failures, leaving a dead listener. `4.5.3.4` replaces this with bounded back-off that never stops retrying.
- **Mock payloads written from Python literals (§ 4.5).** `mock_r4_sender.py` builds JSON in code rather than loading `contracts/samples/` — a second, unversioned copy of the contract, and D9 forbids it.
- **`IVI_ECU/deployment/` (§ 4.5).** Both files still there; not a sanctioned location under [node-code-layout.md](../../.claude/rules/node-code-layout.md). `deployment/phase5_completion_report.md:25,61,86` still cites port **5004**, contradicting the branch's own 47300.
- **`res/values/themes.xml` (§ 4.5).** Still added, and now referenced from the manifest's `<application>` and `<activity>`. `16.5.5.5` requires a platform theme and no new resource file.

### 8.3 New at this head

- **The status bar reports `BOUND` unconditionally.** [`MainScreen.kt:136`](../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/ui/screen/MainScreen.kt) defaults `linkStatusLabel = "BOUND :${BuildConfig.R4_UDP_PORT}"` — a compile-time string, not a live link state. Nothing in the app can make it read anything else, so the indicator says `BOUND :47300` even when the socket never bound or has died after five retries. A demo screenshot of that indicator is evidence of nothing. The dot colour beside it is `if (currentMode == DisplayMode.WarningView) AccentColor else AccentColor` (`:364`) — a dead branch.
- **Suspending `emit` in the receive loop.** `R4ListenerService.kt:127` uses `_r4EventFlow.emit(...)`, so a slow collector back-pressures the socket. D5 requires `tryEmit` with `DROP_OLDEST`.
- **`BuildConfig` read from three layers** — the service, `WarningViewModel.kt:77`, and the UI at `MainScreen.kt:136`. `4.5.4.1` makes `IviRuntimeConfig` the only reader.
- **Test libraries on the main compile classpath.** `app/build.gradle.kts` adds `compileOnly` entries for JUnit, MockK, Turbine, Robolectric, `hilt-android-testing` and `androidx.test:core` to work around a language-server classpath issue. They are not packaged, but an editor workaround does not belong in the build contract.
- **Robolectric, MockK and Turbine added.** [phase5_minh_tasks.md § Deliberately not in this phase](../phase5_minh_tasks.md) rules Robolectric out; `FullStackIntegrationTest` is built on it plus `HiltTestApplication`, and binds the fixed port 47300 in-process — collision-prone on a shared CI runner.
- **`testOptions { unitTests { isReturnDefaultValues = true } }`.** The plan names this the fallback, not the first move: it silences `RuntimeException("Stub!")` for the whole module, so a stray `Log` call on a tested path can no longer fail loudly.
- **A competing HLD.** The branch adds `IVI_ECU/doc/phase5-ivi-ecu-hld.md` with its own `phase5-ivi-ecu-{components,callflow}.puml`, alongside the authoritative [`phase5-ivi-hld.md`](../../IVI_ECU/doc/phase5-ivi-hld.md) (`85387b5`) that the Lead plan and CLAUDE.md cite. Two HLDs for one phase, disagreeing on the module split, is an authority conflict — one must be withdrawn.
- **An overwritten research note.** `IVI_ECU/doc/research_notes/phase5-ivi-implementation-notes.md` exists on `main` and is cited by both the HLD and the Lead plan; the branch replaces its content with a shorter checklist, dropping the Android-constraint and configuration analysis the plan's briefs lean on.
- **A report in the researcher's namespace.** `requirements/phase5-ivi-mini-research.md` sits where [research-report-format.md](../../.claude/rules/research-report-format.md) puts [[project-researcher]]'s reports, without being one.
- **Task-ID collisions.** Branch commits carry `[4.5.1.3]`, `[4.5.1.4]`, `[4.5.1.5]`, `[16.5.4.1]`, `[16.5.2.4]`, `[16.5.2.5]`, `[17.5.4.2]`. Three of those IDs mean something else in the Lead plan (`4.5.1.5` is the ProGuard keep rules; `4.5.1.3`/`4.5.1.4` are the `:contract` module and the model relocation) and four do not exist in it at all. `IVI_ECU/doc/research_notes/phase5-minh-implementation-audit.md` repeats the collision in prose. Per [task-planning-conventions.md](../../.claude/rules/task-planning-conventions.md), IDs are assigned once and never re-mean — these commit tags are unusable for traceability and must not be treated as closing the Lead subtasks they name.
- **The 2-node blueprint JSON is unchanged.** `requirements/blueprint-2node-task51-test.json` still carries all five § 3.2 defects: no `image` block on the Skycraft node, `config: null` on the bridge, `10.88.0.12`, port `5004`, and `registry.carsky.io`. It contradicts the branch's own Kotlin and Python defaults.

---

## 9. Reconciling `phase5_bach_tasks.md` with the Lead plan

[phase5_bach_tasks.md](../phase5_bach_tasks.md) exists only on the branch. Its `B.n`/`V.n` labels are **not project task IDs** — [task-planning-conventions.md](../../.claude/rules/task-planning-conventions.md) defines the ID space as `X.Y.Z.W`, and [phase5_minh_tasks.md](../phase5_minh_tasks.md) is the file that owns Phase 5's. The mapping below is what the two documents have in common; it does not make `B.n` an alias.

### 9.1 Bách's items onto Lead subtasks

| Bách | Lead subtasks it touches | Coverage |
|---|---|---|
| **B.1** Gradle AAOS + `BuildConfig.R4_UDP_PORT=47300` | `4.5.4.1` (the port field only) | 1 of 7 `BuildConfig` fields; no `IviRuntimeConfig`, no `--ei` override. Opposes `4.5.1.1`/`4.5.1.2` (no catalog; Hilt expanded) |
| **B.2** UDP FGS listener + deserializer | `4.5.2.1` `4.5.2.2` `4.5.3.1` `4.5.3.2` `4.5.3.3` `4.5.3.4` `4.5.5.2` | Seven subtasks fused into two classes. Decode preserves `warningType`; the loop carries § 8.2's truncation defect and § 8.3's suspending emit |
| **B.3** Mapper + Canvas God View + guard | `17.5.5.8` `17.5.5.9` | The mapper and the renderer are `[C]` on `main` — Phase 5's work here is the two test files plus the helper extraction, and both landed |
| **B.4** Default `HomeView`, status `BOUND :{port}` | `17.5.5.6` | Default mode yes; the status string is a constant, not the live `R4LinkState` the subtask requires (§ 8.3) |
| **B.5** Wake-on-warning + restore | `16.5.4.5`, mount half of `17.5.5.6` | The one item that meets its subtask's acceptance |
| **B.6** `vehicleCSnapshot` wiring | `17.5.4.4` | The composition landed; the `own_sensor` case and the frozen-sample drive did not |
| **B.7** Mock + deploy doc + tests | group 5.6, `5.5.8.1`, `16.5.8.4` | Python, not the `:r4-simulator` of D9; doc content in an unsanctioned folder |
| **B.8** Rebase onto `main`, prefer main's contracts | **no Lead subtask** | Branch hygiene, not planned work. It is also incomplete — `R3Snapshot.kt` did not go back to `main`'s shape (§ 8.2) |

| Vinh / shared | Lead subtasks | Note |
|---|---|---|
| **V.1** Module split | `4.5.1.1` `4.5.1.3` `4.5.1.4` `4.5.2.1` `4.5.3.1` `4.5.6.1` | Correctly identified as pending; it is the gate for lanes B–F |
| **V.2** Kotlin `:r4-simulator` | `4.5.6.1`–`5.5.6.6` | |
| **V.3** Live `R4LinkState` | `4.5.3.1` `4.5.3.4` `17.5.5.6` | Listed pending while B.4 is claimed done — the two contradict each other |
| **V.4** Hilt → `IviGraph` | `4.5.1.2` `4.5.5.3` | |
| **V.5** `IviRuntimeConfig` + D10 | `4.5.4.1` | Overlaps B.1, which claims the same subtask done |
| **V.6** CI + in-Room evidence | groups 5.7, 5.8, 5.9 | 11 subtasks, including the four USER-MANUAL evidence runs that close four of the five milestone boxes |

**Lead task groups with no Bách coverage at all:** group **5.1** entirely (`4.5.1.1`–`4.5.1.5` — and `4.5.1.2` is actively opposed), `4.5.2.3`, `4.5.3.5` as specified, `4.5.4.3` (`WarningClassifier` does not exist in any form), `18.5.5.1` (`AndroidR4Logger`), `4.5.5.3` (`IviGraph`), `17.5.5.7`, `4.5.6.7`, and groups **5.7**, **5.8** and **5.9** in full.

**Branch deliverables that map to no Lead subtask:** `IVI_ECU/doc/phase5-ivi-ecu-hld.md` + its two `.puml`, `requirements/phase5-ivi-mini-research.md`, `IVI_ECU/doc/research_notes/phase5-minh-implementation-audit.md`, `plans/doc/session-summary-2026-07-30.md`, `.vscode/settings.json`, and the `.gitignore` agent-tooling entries.

### 9.2 His "Gap vs Lead" table, adjudicated against the code

His marks are claims. Each row below is checked against the tree at `06cb936`.

| His row | His mark | Verdict | Evidence |
|---|---|---|---|
| Authority contract R4 = main | `[x] done` | **No** | `R4Message.kt` and `SceneGeometry.kt` match `main`, but `R3Snapshot.kt` defaults six required fields and `R3Timestamps` all three (§ 8.2). The contract layer is not `main`'s |
| IVI `10.99.0.13` + UDP `47300` | `[x] done` | **Partial** | True in `app/build.gradle.kts`, `mock_r4_sender.py:25-26`, `deployment/phase5-ivi-deploy.md`. False in `requirements/blueprint-2node-task51-test.json` (`10.88.0.12`, `5004`, `registry.carsky.io`), `deployment/phase5_completion_report.md` (5004), `mock-sender/README.md:10-11` (wrong registry host) |
| Drop Standby/Video, default `HomeView`, status `BOUND :<port>` | `[x] done` | **Partial** | Default `HomeView` holds (`MainViewModel.kt:22`, `MainViewModelTest.defaultMode_isHomeView`). The status is a compile-time string that cannot report anything but `BOUND` (§ 8.3) |
| Wake-on-warning + restore `previousMode` | `[x] done` | **Yes** | `MainViewModel.kt:52-79`; six cases in `ui/MainViewModelTest.kt` covering force, restore, override and re-arm |
| `vehicleCSnapshot` → Canvas (R19) | `[x] done` | **Partial** | Wiring and a regression assertion exist (§ 8.1); the `own_sensor` case `17.5.4.4` names does not |
| Live `R4LinkState` / module split / `IviGraph` | `[ ] pending` | **Accurate** | And it is the largest remaining block — 30 of the plan's 45 subtasks (§ 10) |

### 9.3 Where his plan contradicts the Lead plan or a frozen artifact

| Subject | His branch | Authority | Consequence |
|---|---|---|---|
| DI | Hilt, expanded — `hilt-navigation-compose`, `kspTest` compiler, `HiltTestApplication` | HLD **D7** / `4.5.1.2`, `4.5.5.3`: hand-written `IviGraph`, no annotation processor | Every wiring file must be rewritten, not adapted; `MainActivity`'s hand-rolled `MainViewModelFactory` already exists because Hilt cannot inject one `@HiltViewModel` into another |
| Module structure | single `:app` | **D2**: `:contract` `:serializer` `:observer` `:r4-simulator` `:app` | The observer and serializer cannot be tested without an Android SDK, which is what forced Robolectric in |
| Simulator | Python `mock-sender/`, payloads from literals | **D9** / group 5.6: Kotlin `:r4-simulator`, payloads built from the frozen samples and validated through `R4Json` | Producer and consumer drift; the missing `class`/`timestamps` in the mock is what weakened `R3Snapshot` |
| Ports / IPs | `47300` / `10.99.0.13` in code; `5004` / `10.88.0.12` in `blueprint-2node-task51-test.json` and the completion report | R6 frozen topology; § 3.2 | A deploy from that JSON is rejected outright and points at the wrong subnet |
| Foreground service type | `dataSync` + `FOREGROUND_SERVICE_DATA_SYNC` | `4.5.5.2`: `connectedDevice` | Wrong category for a network listener on a vehicle bus; harmless at `targetSdk 33`, wrong at 34 |
| Theme | new `res/values/themes.xml` | `16.5.5.5`: platform theme, no new resource file | An undesignated file, referenced from two manifest elements |
| Test stack | Robolectric + MockK + Turbine | § Deliberately not in this phase | Robolectric is excluded by name; `FullStackIntegrationTest` binds the real 47300 in-process |
| Logging | three raw `android.util.Log` call sites on tags `R4ListenerService`, `R4Deserializer`, `IVI_V2X` | `18.5.5.1`: one `AndroidR4Logger` on the single `IVI_V2X` tag, with the `[LINK]`/`[RX]`/`[DROP]`/`[UI]` shapes of HLD §5.4 | `adb logcat -s IVI_V2X` — the filter the whole demo's evidence plan rests on — shows only the guard-trip ERROR. § 11 works around this; the fix is `18.5.5.1` |
| HLD | `phase5-ivi-ecu-hld.md` | `phase5-ivi-hld.md` (`85387b5`) | Two HLDs, disagreeing on the module split |
| Task IDs | `B.n`/`V.n`, plus commit tags reusing Lead IDs for other work | `X.Y.Z.W`, assigned once | Traceability broken; see § 8.3 |

---

## 10. Completion against the Lead plan's subtasks

**This table supersedes § 5**, which measured the pre-rebase branches against the same plan. § 5 is kept for the `-dev`/`1e36cdc` record; where the two disagree, this one is current.

`Done?` is a binary check against each subtask's **own acceptance clause** in [phase5_minh_tasks.md](../phase5_minh_tasks.md), not against a self-report. `% complete` is read off the files named in the notes.

**Totals: 2 done · 13 partial · 30 untouched, of 45 subtasks.**

| Task ID | Done? | % complete | Notes — what to improve |
|---|---|---|---|
| `4.5.1.1` catalog | No | 0 % | No `IVI_ECU/gradle/libs.versions.toml`; every version is a literal in `app/build.gradle.kts` |
| `4.5.1.2` catalog + drop Hilt | No | 0 % | Negative progress — `app/build.gradle.kts` adds `hilt-navigation-compose:1.2.0`, `hilt-android-testing` and `kspTest` compiler on top of the existing Hilt stack |
| `4.5.1.3` `:contract` module | No | 0 % | `settings.gradle.kts` includes `:app` only |
| `4.5.1.4` relocate models/tests/samples | No | 0 % | Models still under `app/src/main/java/.../model/`, samples under `app/src/test/resources/contracts/samples/`. `contracts/sync-manifest.json` untouched — correct, because nothing moved |
| `4.5.1.5` ProGuard keeps | No | 0 % | `app/proguard-rules.pro` unchanged |
| `4.5.2.1` decode contract types | No | 0 % | No `R4Decoder` / `R4DecodeResult` / `DecodeFailure`; the branch returns `Result<R4Message>` and throws `UnknownMessageTypeException` / `MalformedR4PayloadException` |
| `4.5.2.2` `R4Deserializer` + decode table | No | 55 % | `data/R4Deserializer.kt` preserves unknown `warningType` and never escapes an exception — keep both. Add `decode(buffer, offset, length)` (`:22` takes the whole array), the three-value failure taxonomy as a return, `schemaVersionAhead`, BOM/whitespace stripping and the empty-input case; bound the preview — `MalformedR4PayloadException` (`:91`) dumps up to 256 raw bytes including newlines. `R4DeserializerTest` covers 4 of the 10 required rows and builds its JSON from string literals rather than the frozen fixtures: missing are unknown message `type`, empty, all-whitespace, truncated prefix, `"distance": "far"`, and `object` removed |
| `4.5.2.3` buffer-slicing test | No | 0 % | No offset/length API to test |
| `4.5.3.1` `:observer` module + seams | No | 5 % | Only a raw `SharedFlow<R4Message>` on the service. No `R4Event`, `R4LinkState`, `R4ObserverConfig` or `R4Logger` |
| `4.5.3.2` `JdkDatagramSource` + `setLength` | No | 0 % | **Blocking.** `R4ListenerService.kt:118` allocates the packet outside the loop; `:124` receives without `packet.setLength(buffer.size)`. Fix in place first if the branch is used at all, then extract to the seam |
| `4.5.3.3` receive loop + typed events | No | 30 % | Loop and IO dispatch are sound. Replace the suspending `emit` (`:127`) with `tryEmit`/`DROP_OLDEST`; add `R4Event.Dropped`, the `length == bufferBytes` truncation-suspect check, and the `[RX]`/`[DROP]`/`[LINK]` shapes with `cSource=` |
| `4.5.3.4` bounded rebind back-off | No | 25 % | Fixed 1 s delay, no doubling, no ceiling, and `:97-100`/`:106-109` abandon the loop after 5 failures. Replace with `retryInitialMs`→`retryMaxMs` doubling that resets on bind and never gives up; emit `R4LinkState` |
| `4.5.3.5` loopback socket test (I2) | No | 35 % | `FullStackIntegrationTest.udpR4Warning_propagatesThroughServiceToMainViewModelWarningView` does send a real datagram, but through Robolectric + Hilt on the fixed port 47300. Rewrite as a plain-JVM test on an ephemeral port, and add the malformed-then-valid survival case |
| `4.5.4.1` `IviRuntimeConfig` + D10 | No | 20 % | `R4_UDP_PORT=47300` with a `local.properties` override is right. Add the other six `buildConfigField`s, the `IviRuntimeConfig` class with `resolve(intent)` and `toObserverConfig()`, and move the three `BuildConfig` reads (`R4ListenerService.kt:119`, `WarningViewModel.kt:77`, `MainScreen.kt:136`) behind it |
| `4.5.4.2` `R4Repository` | No | 55 % | `data/R4Repository.kt` routes warning vs state correctly. Add last-value-wins by `seq` (`:38` assigns unconditionally; `R4RepositoryTest.stateMessages_lastValueWinsOnCurrentState` only sends ascending `seq`, so it does not test the rule), `droppedCount`, `linkState` passthrough, and `inject()`. Raise `warningEvents` to `replay = 1` (`:27` is replay 0 — why the integration test must attach before sending) |
| `4.5.4.3` `WarningClassifier` | No | 0 % | No such file. Nothing maps a warning type to a presentation, so an unknown type renders identically to `nlos_obstruction` |
| `17.5.4.4` `WarningViewModel` + R19 | No | 75 % | Composition and its regression assertion are in (§ 8.1). Add the `own_sensor` carry-through case; drive the test from `/contracts/samples/r4-warning.json`; change `WarningUiState.Active` to carry `(scene, riskState, presentation)` instead of the raw event; read the timeout from injected config, not `BuildConfig` at `:77`; drop the duplicate composition at `MainScreen.kt:248`; move `WarningUiState.Error` out to `R4LinkState` |
| `16.5.4.5` `MainViewModel` wake/restore/override | **Yes** | 95 % | `MainViewModel.kt:52-79` with six passing cases. `DisplayMode.kt` gained comments only — declarations are identical. Only gap: it is constructed by a hand-rolled factory in `MainActivity`, which `4.5.5.3` replaces |
| `18.5.5.1` `AndroidR4Logger` | No | 0 % | Three direct `android.util.Log` call sites on three tags; only `CanvasWarningView` uses `IVI_V2X` |
| `4.5.5.2` `R4ListenerService` host | No | 50 % | Notification, channel and `startForeground` (`:140-151`) plus the manifest `<service>` are reusable. The class must stop owning the socket and the decode (`:116-136`); change `foregroundServiceType` from `dataSync` to `connectedDevice`; guard the notification post so a denied `POST_NOTIFICATIONS` cannot stop the service |
| `4.5.5.3` `IviGraph` | No | 0 % | `di/AppModule.kt` is a Hilt `@Module`; D7 removes Hilt entirely |
| `16.5.5.4` `IviApplication` | No | 70 % | Class and manifest `android:name` are in. Add the application `CoroutineScope` and the graph property — collection currently runs on `viewModelScope` (`WarningViewModel.kt:51`), so clearing the ViewModel drops the repository's only collector |
| `16.5.5.5` `MainActivity` + LAUNCHER | **Yes** | 85 % | Manifest activity with LAUNCHER, `activity-compose:1.9.2` added — the acceptance clause is met. Outstanding scope violations: replace `@style/Theme.V2xIvi` with a platform theme and delete `res/values/themes.xml`; call `IviRuntimeConfig.resolve(intent)`; request `POST_NOTIFICATIONS` on API 33+ |
| `17.5.5.6` mount seam + status bar | No | 65 % | Seam mounted (`MainScreen.kt:251-253`), no banner, lifecycle-aware collection — all correct. Bind `linkStatusLabel` to the live `R4LinkState` instead of the constant at `:136`; delete `WarningViewPlaceholder` (`:271`); fix the dead colour branch at `:364`; emit the `[UI] mode=… cause=…` lines |
| `17.5.5.7` configurable God-View scale | No | 0 % | `CanvasWarningView` still takes no `scaleMetersPerPixel` |
| `17.5.5.8` `SceneCoordinateMapperTest` | No | 80 % | Five cases green, including the ego anchor, clamping and the null-C path. Add the scale-sensitivity case (halving `scaleMetersPerPixel` doubles displacement) and the radii-pass-through case |
| `17.5.5.9` Ghost C guard test | No | 90 % | `isGhostCSourceTrusted` / `ghostCSourceGuardErrorMessage` extracted and the previews untouched — exactly as briefed. The fifth case is wrong: `riskStateHigh_mapsToFailSafeRed` asserts `riskColor("high")`, where the subtask requires an **unknown** `riskState` to map to the high-urgency colour. Change that assertion |
| group **5.6** `4.5.6.1`–`5.5.6.6` | No | 10 % | `mock-sender/mock_r4_sender.py` is reusable as a *scenario specification* only — its approach → state → leave → unknown-type shape is what `approach.json` and `degrade.json` encode. The code is Python, builds payloads from literals, has no scenario data files, and omits the R3-required `class`/`timestamps` (the cause of § 8.2's contract weakening). Its state message now matches the schema |
| `4.5.6.7` dev injector | No | 0 % | No `app/src/debug/` source set |
| group **5.7** `16.5.7.1` `4.5.7.2` `5.5.7.3` | No | 0 % | `.github/workflows/` holds `phase0-ci.yml` and `phase1-ci.yml` only. No APK artifact, no image lane |
| `5.5.8.1` mini-blueprint deploy doc | No | 25 % | Prose in `IVI_ECU/deployment/phase5-ivi-deploy.md` is partly reusable and already uses 47300/`10.99.0.13`. It is in an unsanctioned folder; the target is `requirements/car-sky-guide/phase5-mini-blueprint-deploy.md`. `requirements/blueprint-2node-task51-test.json` must not be imported (§ 3.2) |
| group **5.8** `5.5.8.2` `16.5.8.3` `16.5.8.4` | No | 0 % | The ADB route to the Skycraft guest is still unproven; `node-ivi-ecu.md` § Post-deploy is unchanged. **Start this first** — it decides whether group 5.9 runs in-Room or on an emulator |
| group **5.9** `5.5.9.1` `16.5.9.2` `17.5.9.3` `4.5.9.4` | No | 0 % | No `plans/doc/phase5-ivi-run.md`; four of the five milestone boxes have no in-Room evidence |

---

## 11. Verifying this PR by hand

An enumerated pass over `feat/phase5-ivi-hmi-complete` at `06cb936`. Windows + Git Bash, same `JAVA_HOME` convention as § 6. **No source edit is needed any more** — the port default is already 47300 and the mock sender already targets `10.99.0.13:47300`, so § 6's step 0 edit is obsolete.

Each step states its own pass condition. A step that fails is a finding; do not work around it silently.

### 11.1 Get the branch and build

1. **Fetch and check out a worktree.**

   ```bash
   git fetch origin feat/phase5-ivi-hmi-complete
   git worktree add ../ivi-demo origin/feat/phase5-ivi-hmi-complete
   ```

   **Pass:** `git -C ../ivi-demo rev-parse HEAD` prints `06cb936…`.

2. **Point Gradle at a JDK 17 or 21.** JDK 25 fails Kotlin/Gradle parsing.

   ```bash
   export JAVA_HOME="/c/Program Files/Android/Android Studio/jbr"
   "$JAVA_HOME/bin/java" -version
   ```

   **Pass:** the version prints `17.x` or `21.x`. If the path does not exist, find the JBR under your Android Studio install before continuing — nothing below works without it.

3. **Run the unit suite.**

   ```bash
   cd ../ivi-demo/IVI_ECU
   ./gradlew :app:testDebugUnitTest --no-daemon
   ```

   **Pass:** `BUILD SUCCESSFUL`, and `app/build/reports/tests/testDebugUnitTest/index.html` lists the nine test classes of § 2 plus `FullStackIntegrationTest`. **Fail-and-note:** `FullStackIntegrationTest` binding 47300 fails if anything else on the host holds that port — free it and re-run rather than dismissing the failure.

4. **Build the APK.**

   ```bash
   ./gradlew assembleDebug --no-daemon
   ls -l app/build/outputs/apk/debug/app-debug.apk
   ```

   **Pass:** `BUILD SUCCESSFUL` and an APK of roughly 25 MB.

5. **Confirm it is launchable** — the single check that separates this branch from PR #2.

   ```bash
   "$ANDROID_HOME/build-tools/35.0.0/aapt" dump badging \
     app/build/outputs/apk/debug/app-debug.apk | grep -E "launchable-activity|uses-feature"
   ```

   **Pass:** a line `launchable-activity: name='com.hackathon.v2x.ivi.MainActivity'`, and `uses-feature-not-required:'android.hardware.type.automotive'` or the required form. **Fail:** no `launchable-activity` line means the APK installs and cannot be started.

### 11.2 Sanity-check the sender with no Android at all

6. **Listen on the port.** In one terminal:

   ```bash
   nc -ul 47300
   ```

7. **Send two cycles.** In a second terminal:

   ```bash
   cd ../ivi-demo/IVI_ECU/mock-sender
   IVI_ECU_HOST=127.0.0.1 IVI_ECU_PORT=47300 CYCLES=2 INTERVAL_MS=500 python mock_r4_sender.py
   ```

   **Pass:** the sender prints `SENT (APPROACH …)`, `SENT (STATE heartbeat)`, `SENT (LEAVING …)`, `SENT (ADDITIVE-VERSION-TEST)`, and `nc` shows each JSON body. **Note:** the sender prints `SENT`, not the `[TX]` shape the plan's simulator uses — do not grep for `[TX]` here.

### 11.3 Run it on a device or an emulator

8. **Preferred — the Skycraft guest.** Follow § 6 steps 3–4 for the Room and the ADB endpoint. This route has never been proven on this deployment; if `adb connect` does not yield a `device`, stop and use step 9 instead of retrying blind.

9. **No-device fallback — an Android Automotive emulator.** A phone image **refuses this APK**: the manifest declares `android.hardware.type.automotive`. Create an AVD from an *Automotive* system image at API 33 or above, then:

   ```bash
   adb install -r app/build/outputs/apk/debug/app-debug.apk
   adb shell am start -n com.hackathon.v2x.ivi/.MainActivity
   ```

   **Pass:** `Success`, then `Starting: Intent…` with no `Error type 3`.

10. **Open a UDP path into the guest.** `adb forward` cannot carry UDP. Use the emulator console redirect:

    ```bash
    telnet localhost 5554        # token in ~/.emulator_console_auth_token
    auth <token>
    redir add udp:47300:47300
    ```

    **Pass:** `redir list` shows the mapping. **Alternative:** push the sender into the guest and run it there, which involves no NAT.

11. **Start the log stream.** The branch logs on three tags, so `-s IVI_V2X` alone shows almost nothing:

    ```bash
    adb logcat -s IVI_V2X R4ListenerService R4Deserializer
    ```

    **Pass:** within a second or two of launch, `R4ListenerService: UDP socket open on port 47300`. **Fail:** no such line means the socket never bound — and note that the on-screen indicator will still read `BOUND` regardless (§ 8.3).

### 11.4 Observe the UI

12. **Default view.** **Pass:** the Display Area shows `Home View Placeholder` between the two side button bars, with the bottom bar reading `MODE: HOME` and `V2X LINK: BOUND :47300`. **Caveat:** that `BOUND` is a compile-time string — step 11's log line is the only real proof the socket bound.

13. **Manual navigation.** Tap **Apps**, then **Settings**, then **Home**. **Pass:** the Display Area cross-fades between the three placeholders and `MODE:` follows.

14. **Wake-on-warning.** Run the sender at the guest:

    ```bash
    IVI_ECU_HOST=127.0.0.1 IVI_ECU_PORT=47300 CYCLES=1 INTERVAL_MS=2000 python mock_r4_sender.py
    ```

    **Pass:** with no user input the Display Area switches to the Warning View and `MODE: WARNING`.

15. **Ghost C and the `[V2X]` badge.** **Pass:** the God View draws ego at the lower-centre anchor, B ahead of it, and a dashed ghost C carrying a `[V2X]` badge and a risk glow. As the approach steps advance, the glow moves through the low → medium → high colours and C closes on ego.

16. **Unknown `warningType`.** The cycle's last packet is `future_unknown_type`. **Pass:** no `FATAL EXCEPTION` in logcat and the app keeps rendering; `R4Deserializer` logs `Non-registry warningType='future_unknown_type' — preserving verbatim`. **Note:** it renders as an ordinary warning — there is no `WarningClassifier` (`4.5.4.3`), so "degrades gracefully" here means "does not crash", not "is presented as a generic warning".

17. **Timeout and restore.** Stop the sender and wait past `WARNING_TIMEOUT_MS` (10 s). **Pass:** the Display Area returns by itself to whatever was showing before the warning, and `MODE:` follows.

18. **User override.** Trigger a warning, then tap **Settings** while it is up, then let it time out. **Pass:** the Settings view stays; the app does not yank you back.

### 11.5 Tear down

19. Stop the sender, `adb uninstall com.hackathon.v2x.ivi`, remove the redirect, and — if you used a Room — delete the deployment (only two may run at once). Finally `git worktree remove ../ivi-demo`.

### 11.6 The truncation defect is visible in this run

The mock sender's own scenario triggers § 8.2 within one cycle, because the packet's length is never reset between receives:

| Order | Packet | Effect |
|---|---|---|
| 1 | approach, `risk="low"` | Sets the receive window to that packet's size |
| 3 | approach, `risk="medium"` — three bytes longer | Truncated by three bytes → `R4Deserializer` fails → `R4ListenerService: Skipping bad packet` |
| 6 | `STATE heartbeat` — far shorter | Shrinks the window permanently to roughly 180 bytes |
| 7+ | every later warning | Truncated → dropped; the Warning View stops updating and times out to Idle |

**Watch for `Skipping bad packet` in logcat.** Seeing it is the defect, not a producer bug. A run that looks clean for one or two packets and then goes quiet has reproduced it.

### 11.7 What this run cannot prove

- **R19 provenance end to end.** The mock only ever sends `source: "v2x_relayed"`, so the guard is never asked to reject anything. The wiring is proven by `WarningViewModelTest`, not by the demo. Nothing here exercises the `own_sensor` trip path or the yellow `[? UNKNOWN SOURCE]` marker — `degrade.json` (`4.5.6.4`) is what does.
- **The link indicator.** It reads `BOUND :47300` unconditionally. A screenshot of it is not evidence that the socket bound; step 11's log line is.
- **Rebind and back-off.** Nothing in the run kills the socket, so neither the 1 s retry nor the give-up-after-5 behaviour is exercised. In a recorded demo the give-up is the one that would bite.
- **The R3 contract.** The mock omits the required `class` and `timestamps`, and the app's Kotlin defaults absorb it. A green run actively conceals § 8.2 — it demonstrates that the weakened model parses, not that the contract holds.
- **The null-C path.** The mock never sends `geometry.vehicleC: null`, so the "C not yet tracked" render is untested outside `SceneCoordinateMapperTest`.
- **Anything past a truncated packet.** Once § 11.6 has bitten, every downstream observation in that run is about a broken receive window rather than about the feature under test.
- **The real topology.** An emulator run proves the app; it proves nothing about the R6 bridge, the Skycraft guest, or the ADB route — those are `16.5.8.3` and group 5.9.
