# PR #2 review — Phase 5 IVI HMI, and what it finishes

Review of pull request **#2 `Feat/phase5 ivi hmi dev`** (head `f7f2f55`) against [phase5_minh_tasks.md](../phase5_minh_tasks.md) and the [Phase 5 HLD](../../IVI_ECU/doc/phase5-ivi-hld.md). Written 2026-08-02 by the orchestrating session.

**Nothing in the plan is checked off by this document.** A subtask is done only when its own commit lands on the phase branch with build and tests green; PR #2 is unmerged, and none of it is on `main`. What follows is *evidence about how much work already exists*, so no one retypes it.

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

## 8. Fix status — `feat/phase5-ivi-hmi-dev` (updated 2026-08-02)

The four defects identified in § 4 above have been addressed on `feat/phase5-ivi-hmi-dev`
in four sequential commits. The document and all findings above are preserved as written;
this section records what was done and by whom.

| § | Severity | Description | Commit | Status |
|---|---|---|---|---|
| 4.2 | 🔴 Blocking | Silent datagram truncation — `packet.setLength(buffer.size)` missing before `socket.receive()` | `7c48b1b` | ✅ Fixed |
| 4.1 | 🔴 Blocking | R19 provenance guard inert — `vehicleCSnapshot` not wired from `objectSnapshot` | `8203714` | ✅ Fixed + regression test added |
| 4.4 | ⚠️ Contract | `warningType` rewritten to `"unknown"` at parse time — wire value now passed through intact | `3cdbc6b` | ✅ Fixed + test assertion updated |
| 4.3 | ⚠️ Contract | `R4ServiceError` was a `@Serializable` wire subtype — moved to standalone transport-only class | `6542211` | ✅ Fixed |

### What was changed per commit

**`7c48b1b`** `fix(ivi): reset DatagramPacket length each iteration to prevent silent truncation`
- `R4ListenerService.kt`: added `packet.setLength(buffer.size)` before `socket.receive(packet)` in `openSocketAndReceive()`.

**`8203714`** `fix(ivi): wire objectSnapshot into vehicleCSnapshot to activate R19 provenance guard`
- `WarningViewModel.kt`: changed `_latestScene.value = event.geometry` →
  `_latestScene.value = event.geometry.copy(vehicleCSnapshot = event.objectSnapshot)`.
- `WarningViewModelTest.kt`: added Test 5 `provenance guard snapshot is wired from objectSnapshot into scene geometry` — asserts `vehicleCSnapshot` is non-null and its `source`/`id` match `objectSnapshot`. This is the named regression test required by subtask `17.5.4.4`.

**`3cdbc6b`** `fix(ivi): preserve wire warningType instead of rewriting to 'unknown' at parse time`
- `R4Deserializer.kt`: removed `event.copy(warningType = UNKNOWN_WARNING_TYPE)` block; unknown types now only log a warning and the event passes through unchanged.
- `R4DeserializerTest.kt`: Test 3 updated — assertion changed from `assertEquals(UNKNOWN_WARNING_TYPE, event.warningType)` to `assertEquals("future_unknown_type", event.warningType)` (the actual wire value).

**`6542211`** `fix(ivi): move R4ServiceError out of wire message hierarchy`
- `R4Message.kt`: `R4ServiceError` loses `@Serializable`, `@SerialName("error")`, and `: R4Message()`. It is now a plain, non-serializable transport-only data class.
- `R4ListenerService.kt`: adds `serviceError: StateFlow<R4ServiceError?>` as a separate channel. The two `_r4EventFlow.emit(R4ServiceError())` calls are replaced with `_serviceError.value = R4ServiceError(...)`.

### What remains open from this review

The items listed below from § 4.5 and § 5 are **not** addressed by the four commits above —
they are tracked in [phase5_minh_tasks.md](../phase5_minh_tasks.md) and require the
five-module restructure the plan describes:

- Hardcoded `BUFFER_SIZE`, `MAX_RETRIES`, `RETRY_DELAY_MS`, notification constants (§ 4.5).
- Retry policy terminates after 5 attempts — should use bounded back-off (§ 4.5).
- Mock sender `state` message schema mismatch (§ 4.5).
- Mock sender not loading from `contracts/samples/` (§ 4.5).
- Module split, version catalog, CI lane, in-Room evidence — groups 5.1, 5.7, 5.9 (§ 5, 0 %).
