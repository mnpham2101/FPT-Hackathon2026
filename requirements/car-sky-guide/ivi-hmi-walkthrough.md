# IVI HMI Bring-Up — Build to God View on the AAOS Node

**The authoritative procedure for getting the IVI app from source onto the Skycraft AAOS node and proving it renders the R17 God View.** Anything about *how* the IVI APK is built, exported, installed, launched or verified belongs here; other documents link to this one rather than restating it.

- **Companion to** [deploy-walkthrough-netcheck.md](deploy-walkthrough-netcheck.md) — the template for **container** nodes (build → push to Zot → node pulls). Read it once for the platform model (blueprint, node, pin, Device, Room) and the credential table; nothing from it is repeated here.
- **Node facts** — VM artifact IDs, the `image` config block, the `ethernet` pin shape, the acceptance list — live in [node-ivi-ecu.md](node-ivi-ecu.md) and are linked, never copied. That file owns the node's *facts*; this one owns the *doing*.
- **Serves** R16 and R17 acceptance ([m1-cooperative-awareness.md §2](../m1-cooperative-awareness.md)), the R4 consumer half, and the R6 hop-3 check the Phase 0 smoke test left open.

**The one structural difference from every other node:** there is no image to push. The node's image is the starter-pack AAOS artifact; the team's deliverable is an APK installed **after** the Room is Running. That is why deploy comes before install below — the guest must exist before anything can be installed into it.

```
IVI_ECU/  ──gradlew──▶  app-debug.apk  ──copy/download──▶  your machine  ──adb install──▶  AAOS guest
 (source)                (§2 or §3)                          (§3.3)          (§4.5)      in a Running Room (§4.2)
```

> ## The APK on `main` cannot be started
>
> `IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/` holds the models, `MainViewModel`, `MainScreen`, `CanvasWarningView`, `IviWarningViewSeam`, `SceneCoordinateMapper` and `WarningBannerOverlay` — and **no `MainActivity`**. [AndroidManifest.xml](../../IVI_ECU/app/src/main/AndroidManifest.xml) declares no `<activity>` and no launcher entry.
>
> Today's APK therefore **installs cleanly and has no way to start**. `am start` answers `Error type 3 … Activity class does not exist`. The launcher entry lands with subtask **`16.5.5.5`** ([phase5_minh_tasks.md](../../plans/phase5_minh_tasks.md)).
>
> Until it does, §1–§4.5 are still worth running end to end: they prove the **route** to the guest, which is the phase's biggest schedule risk (`16.5.8.3`). §4.6 onward need the app.

---

## 1. Prerequisite

### 1.1 Toolchain on the build machine

| Need | Value | Why this value |
|---|---|---|
| JDK | **17**, or **21** | `compileOptions` / `jvmTarget` are Java 17 in [app/build.gradle.kts](../../IVI_ECU/app/build.gradle.kts) and CI uses Temurin 17. A JDK 21 toolchain emits the same Java 17 bytecode and builds this project. **JDK 25 fails** Kotlin/Gradle script parsing |
| Gradle | nothing to install | The wrapper pins Gradle 8.13 ([gradle-wrapper.properties](../../IVI_ECU/gradle/wrapper/gradle-wrapper.properties)) and downloads it on first run |
| Android SDK | platform **android-34** | `compileSdk = 34` |
| Build-tools | **34.0.0** or newer | Provides `aapt`, needed by §2.5 |
| Platform-tools | latest | Provides `adb`, needed by §4.4–§4.7 |

The full Android Studio IDE is **not** required — `sdkmanager` from the Android command-line tools installs the three SDK packages above on its own.

Point Gradle at the SDK one of two ways:

- `ANDROID_HOME` in the environment, or
- a `local.properties` file beside [IVI_ECU/settings.gradle.kts](../../IVI_ECU/settings.gradle.kts) containing one line:

```
sdk.dir=C\:\\Users\\<you>\\AppData\\Local\\Android\\Sdk
```

`local.properties` is git-ignored on purpose ([.gitignore](../../.gitignore)) — it is machine-specific and must never be committed.

*Assumption — check before following §2:* that a JDK and an Android SDK are installed at all. If neither is, §3 (the CI route) needs no local toolchain whatsoever and is the faster path to an APK.

### 1.2 Platform access

| Credential | Format | Needed for |
|---|---|---|
| Keycloak login | email + password | Signing in to the CarSky web UIs (Nydus, Devices) — §4.1, §4.2, §4.6 |
| CarSky API key | `a8k_…` | The REST calls in §4.1, §4.2, §4.3 |
| GitHub account | — | Reading the Actions run and downloading the artifact — §3.2, §3.3 |

Where the two CarSky credentials come from, and how to verify one: [carsky-deploy-preflight](../../.claude/skills/carsky-deploy-preflight/SKILL.md).

**Zot is not in the path for an APK.** Not "not usually" — not at all:

- The registry holds **container images**. The IVI node is a Skycraft node; it pulls its VM image from the CarSky **artifact store** ([node-ivi-ecu.md § Prepare the VM artifact](node-ivi-ecu.md#prepare-the-vm-artifact-once-per-team-not-per-deploy)), not from a registry.
- Nothing in this guide runs `docker login`, `docker push`, or tags an image. No `zak_…` key is used anywhere in §1–§6.
- The one place Zot enters Phase 5 is a **different** node: the R4 simulator image `m1-r4-sim:latest` that the ADA node pulls to generate traffic (§4.7). That is the message *source*, not the APK; it has its own CI lane (`5.5.7.3`) and its own credential path ([zot-registry-api-key.md](zot-registry-api-key.md)).

### 1.3 What must already exist

This walkthrough is executable **only as far as the artifacts it consumes exist**. Check this table before following it end to end.

| Input | State on `main` | Lands with |
|---|---|---|
| An APK that can be *installed* | Present — `assembleDebug` works today | — |
| CI lane publishing `app-debug-apk` | **Present** — [phase5-ci.yml](../../.github/workflows/phase5-ci.yml) | — |
| An APK that can be *launched* | **Absent** — no `<activity>`, no `MainActivity` (see the banner above) | `16.5.5.5` |
| The R4 listener (`IVI_V2X` log lines, socket on 47300) | **Absent** | `4.5.5.2`, `4.5.3.3` |
| The dev injector (broadcast injection point I3) | **Absent** | `4.5.6.7` |
| The R4 simulator image `m1-r4-sim:latest` | **Absent** | `5.5.6.6`, `5.5.7.3` |
| A Room with a Running Skycraft node | Deployable today from the baseline blueprint | — |

---

## 2. Building on the local machine

Everything in this section runs from the `IVI_ECU/` folder. If you have no local JDK or Android SDK, skip to §3.

### 2.1 Pick the command for your host

| Host | Wrapper command | What is different |
|---|---|---|
| **Windows x86_64** | `.\gradlew.bat assembleDebug` | Nothing — the reference case |
| **Windows arm64** | `.\gradlew.bat assembleDebug` | Same command, one JDK constraint and one hard limit — §2.1.1 |
| **Linux** | `chmod +x gradlew && ./gradlew assembleDebug` | The wrapper script loses its executable bit on a fresh clone on some checkouts; `chmod +x` once fixes it |

#### 2.1.1 Windows arm64 — what actually changes

**The APK build itself is unaffected.** The output is a universal APK: every native slice in it (`arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64`) is a **prebuilt** shipped inside an AndroidX AAR from Maven, and everything else is DEX bytecode compiled on the device by ART. Nothing is compiled for the host CPU, so an identical APK comes out of an x86_64 Linux runner, an Apple-silicon Mac, or an ARM64 Windows laptop — and the `arm64-v8a` slice the AAOS guest needs is present in all of them. No `abiFilters`, no per-architecture build.

Two things do change:

- **JDK availability.** Eclipse Temurin publishes **no Windows/AArch64 build of JDK 17**; its Windows/AArch64 line starts at **21.0.5**. On an ARM64 Windows host, install **Temurin 21** (native) rather than hunting for a native 17 — the project's Java 17 *bytecode* target is a compiler setting, not a JDK requirement. A Windows x64 JDK 17 also runs under Windows' x64 emulation layer if you need to match CI exactly.
- **No Android emulator, at all.** `sdkmanager` ships `emulator.exe` as an **x86_64 PE binary**, Google publishes no ARM64 Windows emulator (the native ARM host build is macOS-only), and `emulator -accel-check` exits `3` with *"requires an Intel/AMD processor with virtualization extension support"* on a Snapdragon X-series host. This is a platform limit, not a configuration problem.

  **Consequence:** the "fall back to an AAOS emulator" escape hatch in §4.9 **does not exist on an ARM64 Windows machine**. The Skycraft guest is the only in-Room target, which is why §4.3–§4.5 must be proven early. For UI-only checks a **physical Android 10+ phone over USB** works — the app declares no car-API dependency, and `adb install` does not enforce the manifest's `automotive` feature (that element filters the Play Store, not the installer).

One cosmetic effect of a Studio-less SDK install on any host: with no NDK present, `stripDebugDebugSymbols` reports *"Unable to strip … libandroidx.graphics.path.so, packaging as is"*. The library ships unstripped — a few KB larger, functionally identical.

### 2.2 Build the APK

```powershell
cd C:\Users\<you>\Documents\Work\FPT-Hackathon2026\IVI_ECU
.\gradlew.bat assembleDebug
```

Expected tail:

```
BUILD SUCCESSFUL in 7m 02s
```

A first run is the slow one — the wrapper downloads Gradle 8.13 and the build resolves AGP 8.13, Kotlin 2.2.20 and the Compose BOM. Later runs are a fraction of that.

### 2.3 Run the unit tests

Do this before shipping an APK anywhere; it is the same gate CI applies in §3.1.

```powershell
.\gradlew.bat :app:testDebugUnitTest
```

Expected: `BUILD SUCCESSFUL`, with the two committed contract tests — `R4RoundTripTest` and `R4AdditiveVersionTest` — reported green in `app/build/reports/tests/testDebugUnitTest/index.html`.

The five-module command replaces this one when the module split lands; the command table in [phase5_minh_tasks.md § Build & verification commands](../../plans/phase5_minh_tasks.md) is authoritative for which invocation is valid at which point.

### 2.4 Where the artifact lands

```
IVI_ECU/app/build/outputs/apk/debug/app-debug.apk
```

That exact path is what §2.5, §3.1 and §4.5 all refer to. Expect roughly 25 MB.

### 2.5 Check the APK is launchable

An APK with no launcher activity installs cleanly and cannot be started — the failure this check exists to catch:

```bash
"$ANDROID_HOME/build-tools/34.0.0/aapt" dump badging app/build/outputs/apk/debug/app-debug.apk | grep launchable-activity
```

| Result | Meaning |
|---|---|
| `launchable-activity: name='com.hackathon.v2x.ivi.MainActivity' …` | Launchable — §4.6 will work |
| *(no output)* | No launcher entry. **Expected on `main` today**; use this build for route-proving only |

The same check runs in CI and reports its answer as a run notice (§3.1), so a downloaded artifact never has to be inspected blind.

### 2.6 Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `SDK location not found. Define a valid SDK location with an ANDROID_HOME environment variable or by setting the sdk.dir path in your project's local properties file` | No SDK path | §1.1 |
| `Unsupported class file major version`, or a toolchain error naming a JDK | Wrong JDK on `JAVA_HOME` | Use JDK 17 or 21; JDK 25 is not usable |
| Kotlin DSL script parse errors in `build.gradle.kts` before any compilation | JDK 25 | Same |
| `./gradlew: Permission denied` | Wrapper not executable after a fresh clone on Linux | `chmod +x gradlew` |
| `Build was configured to prefer settings repositories … but repository 'X' was added by build file` | A module declared its own `repositories` block | Forbidden by `FAIL_ON_PROJECT_REPOS` in [settings.gradle.kts](../../IVI_ECU/settings.gradle.kts) — remove it |
| `Unable to strip … packaging as is` | No NDK installed | Not an error; see §2.1.1 |

---

## 3. Building on CI

The CI route needs **no local Android SDK and no local JDK** — only a GitHub account. It is the recommended path on a machine that does not already build the app.

### 3.1 What the workflow does, and what triggers it

[.github/workflows/phase5-ci.yml](../../.github/workflows/phase5-ci.yml), workflow name **`phase5-ci`**, one job **`ivi-assemble`**.

| Trigger | Scope |
|---|---|
| `push` | **Every** push, on every branch |
| `pull_request` | Pull requests targeting `main` |

There are **no path filters** — a push that touches nothing under `IVI_ECU/` still runs the lane. That matches `phase0-ci.yml` and `phase1-ci.yml`, which are equally unfiltered; superseded runs on the same ref are cancelled by the `concurrency` block rather than left to finish.

The job, in order:

| Step | What it proves |
|---|---|
| `actions/setup-java@v4`, Temurin 17, `cache: gradle` | The same JDK the app's `jvmTarget` names |
| `./gradlew :app:testDebugUnitTest --no-daemon` | **The gate.** No APK leaves the workflow unless its own unit tests passed in the same job |
| `./gradlew assembleDebug --no-daemon` | The debug APK builds on a clean Linux machine |
| Record the APK size | Emits a run notice with the byte count — the number `16.5.7.1` asks to be recorded |
| Report the launcher entry | Emits a run notice saying whether the APK declares a launchable activity (§2.5's check, run for you) |
| `actions/upload-artifact@v4` → **`app-debug-apk`** | The downloadable artifact §3.3 fetches |

Debug build only. There is no signing config and no release path.

The Android SDK comes from the `ubuntu-latest` runner image, as `phase0-ci.yml`'s `ivi-unit-tests` lane already relies on. *Assumption:* that the runner image's SDK and licence state stay sufficient for `compileSdk 34`. If a run fails on a missing SDK component, `android-actions/setup-android` is the documented remedy (Phase 5 HLD §6.1) — add it and record that it was needed.

**Why this lane duplicates `ivi-unit-tests`.** `phase0-ci.yml`'s `ivi-unit-tests` remains the maintenance home of the IVI test invocation (`4.5.7.2` extends it to five modules). The copy here is a gate on a hand-installed artifact, not a second test lane — extend `ivi-unit-tests` when test targets change, never this step.

### 3.2 Tell when the run finished and whether it passed

1. GitHub → the repository → **Actions**.
2. Left sidebar → **phase5-ci**. The newest run for your branch is at the top.
3. Read the run's status icon:

   | Icon | Meaning |
   |---|---|
   | Yellow dot / spinner | Still running — no artifact yet |
   | Green check | Finished, every step passed — the artifact exists |
   | Red cross | Finished and failed — open the job and read the first red step |
   | Grey slash | Cancelled, because a newer push to the same branch superseded it. Not a failure; use the newer run |

4. Open the run → job **`ivi-assemble`**. The job's **Annotations** carry the two notices worth reading before downloading anything: the APK's size, and whether it declares a launcher activity.
5. Scroll to the bottom of the run summary page. The **Artifacts** section lists **`app-debug-apk`**. An artifact appears only after the run finishes.

### 3.3 Download the APK

1. On the run summary page, click **`app-debug-apk`** under **Artifacts**. The browser downloads `app-debug-apk.zip`.
2. **Unzip it.** GitHub always wraps an artifact in a zip; `adb install` on the zip fails. The file inside is `app-debug.apk`.
3. Record its SHA-256 and size — `adb install` reports nothing about provenance, so this is the only way to prove later that the build on the guest is the build you fetched:

```powershell
Expand-Archive .\app-debug-apk.zip -DestinationPath .\apk
Get-FileHash -Algorithm SHA256 .\apk\app-debug.apk
```

The artifact name **`app-debug-apk`** is fixed by the workflow and is what every step below assumes. It is not a cosmetic label — renaming it in the workflow breaks this guide.

### 3.4 Is there a direct route from CI to CarSky?

**No.** There is no route that puts an APK onto the node without passing through a machine that holds the file:

- **Not Zot.** The registry serves container images; the IVI node pulls no image (§1.2).
- **Not the artifact store.** It holds the AAOS VM image, and the APK is deliberately not baked into that image.
- **Not the REST API.** [carsky-rest-api-blueprint.md](carsky-rest-api-blueprint.md) enumerates the live routes; none of them accepts a file upload to a guest.

The only ingress is `adb install` from a machine holding the `.apk` (§4.5). Download first, then install.

### 3.5 Troubleshooting

| Symptom | Meaning | Action |
|---|---|---|
| No **phase5-ci** entry in the Actions sidebar | The workflow file is not on the branch you are looking at | Check out / push the branch carrying `.github/workflows/phase5-ci.yml` |
| Run red at *Unit tests* | A committed test broke — the gate did its job | Fix the test; no artifact is published, by design |
| Run red at *Assemble the debug APK* with an SDK or licence error | Runner-image SDK insufficient | Add `android-actions/setup-android` (§3.1) and record it |
| Run green, no **Artifacts** section | Looking at a still-running or cancelled run | Wait for the green check, or open the newest run |
| `INSTALL_PARSE_FAILED_NOT_APK` at §4.5 | The zip was installed instead of the APK inside it | §3.3 step 2 |

---

## 4. Deploying on CarSky

### 4.1 Prepare the blueprint

| Blueprint | Use when |
|---|---|
| The 5-node baseline ([carsky-4-node-blueprint.md](carsky-4-node-blueprint.md)) | Full-chain work — bench, V2X, ADA, IVI, bridge |
| The 3-node mini-blueprint — bridge + ADA + IVI | IVI work alone: fewer nodes, faster deploy, leaves the second Room slot free — §4.10 |

Three things about the IVI node decide whether the deploy is even accepted, and all three are **facts owned by [node-ivi-ecu.md](node-ivi-ecu.md)** — read them there, do not retype them here:

| What | Where it is defined | What can go wrong |
|---|---|---|
| The VM image artifact IDs | [§ Prepare the VM artifact](node-ivi-ecu.md#prepare-the-vm-artifact-once-per-team-not-per-deploy) | Uploading a second copy of an artifact that already exists |
| The Skycraft `image` config block | [§ Blueprint node config](node-ivi-ecu.md#blueprint-node-config) | Its absence gets the whole deploy **rejected outright**, with the exact message quoted there. On a clone the block is already correct — leave it alone. On a hand-authored or imported node it is usually missing |
| The `ethernet` pin shape and address | [§ Pins](node-ivi-ecu.md#pins) | Its absence leaves the node with no network at all |

**The `ethernet` pin is a manual UI step.** Neither the REST API nor Nydus **Import from File** can create an `ETHERNET` pin ([carsky-rest-api-blueprint.md](carsky-rest-api-blueprint.md)) — an import silently drops it. A **clone keeps its pins**; an import does not. Prefer cloning. If the pin is missing, draw it by hand on the Nydus canvas and wire it to the Ethernet Bridge node, same-type only (`ethernet ↔ ethernet`).

**Verify by reading the stored config back**, not by trusting the Inspector's truncated fields:

```bash
export CS=https://hackathon-2.carsky.io
curl -H "Authorization: Bearer $KEY" $CS/api/v1/blueprints/{id}
```

Expect one `ETHERNET` / `OUTPUT` pin on the IVI node at the address [node-ivi-ecu.md § Pins](node-ivi-ecu.md#pins) fixes, wired to the bridge's single `INPUT` pin.

**Record the Skycraft display config while you are in that read-back.** The Inspector's CONFIGURATION group on a Skycraft node carries fields no other node type has, and §4.6 needs two of them:

| Field | Why you need it |
|---|---|
| **Part Prefix** | Auto-names every "part" of the node — `<prefix>-screen`, `<prefix>-audio`, `<prefix>-logcat`, `<prefix>-adb`. §4.3 and §4.6 select parts by these names |
| **Display Width / Height / DPI** | The virtual screen resolution the Screen widget shows. The committed R16 previews are drawn for 1280×720 ([MainScreen.kt](../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/ui/screen/MainScreen.kt) preview names) |
| **GPU Backend** | The virtual graphics driver given to the VM |

*Unverified:* the actual values on our IVI node. Read them off the read-back and write them down before you need them — **do not assume 1280×720**.

### 4.2 Deploy the blueprint

1. In Nydus, open the blueprint (the original, **never** the `<name>-deploy` snapshot that deploying creates).
2. Click empty canvas → the blueprint Inspector → **New Deployment**.
3. Pick an **existing Device** from the dropdown — the Kubernetes resource pool, not an ECU. `+ Create new device` is unnecessary and eats into the 2-concurrent-deployment budget.
4. **Deploy**, then wait until every node badge reads `Running` with restart count 0.

**The Skycraft node is the slowest to reach Running** — expect it to lag the containers.

Poll it without the browser if you prefer:

```bash
curl -H "Authorization: Bearer $KEY" $CS/api/v1/deployments/{roomId}/nodes
```

Each entry carries `{displayName, name, nodeType, phase, message}`. `name` is the **`nodeKey`** every route below needs; `{roomId}` is the Device's id. Deploy-dialog details and the two-Room budget: [deploy-walkthrough-netcheck.md § M9](deploy-walkthrough-netcheck.md). Diagnosis when a node hangs: [carsky-room-diagnostics](../../.claude/skills/carsky-room-diagnostics/SKILL.md).

### 4.3 Get an ADB endpoint

> **This step has never been proven on this deployment.** It is the phase's biggest schedule risk (`16.5.8.3`), and its outcome decides whether the R16/R17 evidence is in-Room or device-only. Run it early, against today's APK, before the app is finished.

The platform states the shape of the answer plainly: *"The Skycraft pod exposes ADB on a fixed port; use Rework's device panel or CarSky Gateway ADB tunnel to connect."* Three candidate routes follow from that, strongest first. **Record which one actually worked.**

| # | Route | State |
|---|---|---|
| 1 | `GET /api/v1/deployments/{roomId}/adb-tunnel` — *"Get ADB tunnel command info for Skycraft node."* Then `adb connect localhost:<port>` and use local `adb` normally | Documented by the platform ([Car-Sky-Platform.html](../development-platform-doc/Car-Sky-Platform.html) § API & MCP Tools). **Unverified on `hackathon-2`** |
| 2 | Devices panel → the device the deployment created → **Connect** → `+` → **ADB** widget, with the ADB part set to `<prefix>-adb` | Documented; gives an in-browser shell (prompt `trout_arm64:/ $`). The platform describes it as usable to *"run commands, install APKs, view logs"* — but it exposes **no file transfer**, so it cannot take a local `.apk` unless the file is already reachable from the guest |
| 3 | `POST /api/v1/deployments/{roomId}/adb-exec/{nodeKey}` — *"Run one-shot ADB command."* | Documented. **Unverified**, and the same file-transfer limitation as #2 |

Two routes that are **not** available, so nobody spends time on them:

- The MCP tunnel tool `vm_tunnel_open` needs CarSky's own MCP server package, which this repo does not carry ([carsky-rest-api-blueprint.md](carsky-rest-api-blueprint.md)).
- `POST /api/v1/vms/{roomId}/{nodeKey}/shell` is **known-502** on this deployment and must not be the primary plan.

Probe the 502 pair anyway while you are here — `…/shell` and `…/screenshot`. A live `screenshot` route is a **second evidence path** for §4.8 that does not depend on ADB at all, and it is a cheap check:

```bash
curl -s -o /dev/null -w '%{http_code}\n' -H "Authorization: Bearer $KEY" \
  $CS/api/v1/vms/{roomId}/{nodeKey}/screenshot
```

### 4.4 Connect and check the guest

```bash
adb connect <skycraft-adb-endpoint>
adb devices
```

Expected:

```
List of devices attached
<endpoint>   device
```

`offline` or an empty list means the endpoint is wrong or the tunnel is not up — go to §4.9 rather than retrying blind.

Two guest properties decide whether the APK can be installed at all, and both are unknown until you read them:

```bash
adb shell getprop ro.build.version.sdk
adb shell getprop ro.build.version.release
adb shell pm list features | grep automotive
```

| Reading | Requirement | If it fails |
|---|---|---|
| `ro.build.version.sdk` | ≥ **29** (`minSdk 29`) | Install is rejected outright; the APK's `minSdk` would have to change |
| `android.hardware.type.automotive` present | The manifest declares it `required="true"` | A non-automotive **guest** refuses the install. (A phone over USB does not — `adb install` does not enforce the feature; see §2.1.1) |

### 4.5 Install the APK

```bash
adb install -r app-debug.apk
adb shell pm list packages | grep hackathon
adb shell pm path com.hackathon.v2x.ivi
```

Expected:

```
Success
package:com.hackathon.v2x.ivi
package:/data/app/~~…/com.hackathon.v2x.ivi-…/base.apk
```

`-r` reinstalls over an existing copy and keeps its data — use it on every reinstall, not only the first.

### 4.6 Open the screen and launch the app

The display is a **widget in the Devices panel**, not something in the Deployment Viewer.

1. **Devices** on the DockBar. The deployment auto-creates its device; the platform marks such an entry `🚀 Started pack-deploy`. Do not delete it by hand — stop the deployment instead.
2. **Connect** (the button reads **Switch** if another device session is already open). The dot beside the name must be green.
3. `+` → **Screen** from the widget catalog.
4. In the widget's Inspector, set the parts from the node's Part Prefix (§4.1):
   - **Video Part** → `<prefix>-screen`
   - **Touch Part** / **Keyboard Part** → the corresponding parts, or clicks in the browser never reach the guest
   - **Recorder Part** → set it when you want the run recorded; the clip appears under **Videos** after recording stops, downloadable as `.mp4`
5. Add two more widgets now — they are what §4.7 reads:
   - **Log**, source part `<prefix>-logcat` — the guest's logcat in the browser
   - **ADB**, part `<prefix>-adb` — a shell beside the screen

Expected: the AAOS screen streaming live in the Stage, and clicking on it acting like a touch.

**Black screen?** Per the platform's own FAQ: check that **Video Part** names the part the node actually publishes (same prefix as its Part Prefix); if the part is right, **Disconnect** then **Connect** to re-establish the WebRTC session.

*Unverified:* whether the guest's display ever sleeps in a Room. If the stream is up but the guest looks asleep, `adb shell input keyevent KEYCODE_WAKEUP` is the standard AOSP wake command — untested against this guest.

Then launch:

```bash
adb shell am start -n com.hackathon.v2x.ivi/.MainActivity
```

To override the R4 port for one launch without rebuilding (HLD decision D10):

```bash
adb shell am start -n com.hackathon.v2x.ivi/.MainActivity --ei r4_port 47301
```

Expected on the screen — the R16 layout from [MainScreen.kt](../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/ui/screen/MainScreen.kt):

- A central **Display Area**, flanked by the button areas: **Home**, **Apps**, **Settings** on one side, mode labels `WARNING` / `HOME` / `APPS` / `SETTINGS` on the other.
- A bottom status bar reading `MODE: <mode>` and a `V2X LINK: …` indicator.
- Tapping a side button changes what the Display Area shows.

**Record the boot-to-listener time while the app starts for the first time.** Two wall-clock deltas: guest boot → launcher, and launch → the first `[LINK] state=bound` line. Their sum is the floor for the bench's start delay, and Phase 5 is the only phase that can measure it (`16.5.9.2`).

### 4.7 Verify the chain

The claim under test is one chain, not a set of independent checks:

```
ADA node sends an R4 datagram  ──▶  IVI guest receives it  ──▶  an event is raised in the app  ──▶  the HMI switches to the Warning View
       CarSky node log                    IVI_V2X log                     IVI_V2X log                        Screen widget
        [TX] … 47300                    [RX] type=warning              [UI] mode=WarningView             the God View, drawn
```

Every link has its own observable, and they come from **two different log surfaces**:

| Surface | How to read it | Carries |
|---|---|---|
| **CarSky node log** (the ADA container) | Deployment Viewer → the ADA node → **View Log**; or `GET /api/v1/deployments/{roomId}/logs/{nodeKey}?container=user` — **`container` is mandatory**, omitting it returns 500 | The producer's `[TX]` lines, and `[CAP]` tcpdump lines when the node has `NET_RAW` |
| **Guest logcat** (the IVI app) | The **Log** widget on part `<prefix>-logcat`, or `adb logcat -s IVI_V2X` | Everything the app does: `[LINK]`, `[RX]`, `[DROP]`, `[UI]` |

*Unverified:* every `IVI_V2X` line quoted below is the shape **designed** in the Phase 5 HLD §5.4, not yet built. Until the listener lands (`4.5.3.3`, `4.5.5.2`), the guest logcat carries nothing on this tag.

Work up the ladder — each rung needs less to exist than the one below it, so start at the highest rung your build supports.

#### V1 — the socket is bound

| | |
|---|---|
| **Feed** | Nothing; launching the app is enough |
| **Correct** | `[LINK] state=bound port=47300` on `IVI_V2X`, and the bottom status bar reading `BOUND :47300` |
| **Incorrect** | No `[LINK]` line at all (listener absent or not started), or a bind error followed by rebind attempts (port already taken — relaunch with `--ei r4_port`) |

#### V2 — a datagram reaches the guest (works before the simulator exists)

| | |
|---|---|
| **Feed** | Point the ADA node at the IVI: image `m1-netcheck:latest`, `NEXT_HOP_HOST=10.99.0.13`, `NEXT_HOP_PORT=47300` — the **probe config** of subtask `5.5.8.2`. The ADA node's **View Log** shows `[TX] … relayed to 10.99.0.13:47300` |
| **Correct** | `[DROP] reason=malformed bytes=… preview="seq=…"` on `IVI_V2X`, one per datagram, and the app keeps running. netcheck's payload is not JSON, so a drop **is** the pass — it proves the socket, the bridge hop and the loop's survival |
| **Incorrect** | ADA logs `[TX]` but the IVI logs nothing → the datagram is not arriving; re-check the pin address and the port |
| **Closes** | R6 hop 3 — the check Phase 0 could only make indirectly ([deploy-walkthrough-netcheck.md § Checking IVI RX traffic](deploy-walkthrough-netcheck.md)) |

#### V3 — the UI comes up, with no network at all

| | |
|---|---|
| **Feed** | `adb shell am broadcast -a com.hackathon.v2x.ivi.DEV_INJECT --es sample r4-warning` |
| **Correct** | The Display Area switches to the Warning View **by itself**, drawing the God View |
| **Incorrect** | `Broadcast completed: result=0` with no UI change → wrong build type: the injector exists in the **debug** build only, by design |

#### V4 — a real R4 message: the whole chain (R16 + R17 acceptance)

| | |
|---|---|
| **Feed** | ADA node → the **evidence config**: image `m1-r4-sim:latest`, `command: ["./entrypoint.sh"]` (relative), `IVI_ECU_HOST=10.99.0.13`, `IVI_ECU_PORT=47300`, `R4_SCENARIO=/app/scenarios/approach.json`, `R4_RATE_HZ=1`, `START_DELAY_S=20` |
| **Link 1 — sent** | ADA **View Log**: `[TX] step=… type=warning bytes=… → 10.99.0.13:47300`, at ~1 Hz |
| **Link 2 — received & parsed** | `IVI_V2X`: one `[RX] type=warning bytes=… from=10.99.0.12:… warningType=nlos_obstruction risk=… cSource=v2x_relayed cPos=(…)` per datagram. The fields after `bytes=` are read off the **parsed** message, so this line is the proof that the R4 JSON decoded into the typed model and its R3 `object` snapshot |
| **Link 3 — event raised** | `IVI_V2X`: `[UI] mode=WarningView cause=warning` — the app changed state because of the message, not because anyone tapped |
| **Link 4 — displayed** | Display Area switches itself to the Warning View. `EGO` and `B` drawn solid; **C dashed** with a pulsing risk glow and the badge `[V2X] C · <d> m · RISK: HIGH`; connector labels `d_AB = <n> m` and `d_AC ≈ <n> m`. The scenario's first step carries `geometry.vehicleC: null` and must render **without C and without a crash or placeholder** |
| **Incorrect** | A yellow **`[? UNKNOWN SOURCE]`** marker where ghost C should be. The provenance guard tripped: the scene reached the renderer without a `v2x_relayed` snapshot. On `approach.json` this is a **blocking defect**, not a display quirk |
| **Also check** | Let the stream stop: the view times out to Idle and the previous mode is restored — `[UI] mode=HomeView cause=timeout` |

`cSource=v2x_relayed` on every rendered warning **is the R19 claim in text**: the recording shows ghost C, and the log proves every frame of it came from relayed data.

#### V5 — degradation, the guard, and loop survival (R4 acceptance)

Switch the ADA node to `R4_SCENARIO=/app/scenarios/degrade.json`.

| Case in the scenario | Correct result | Incorrect result |
|---|---|---|
| Unknown `warningType` + `schemaVersion: 2` + a junk field | A generic warning renders; logcat shows the wire value **preserved** (`warningType=slippery_road`) and one schema-version-ahead notice | `FATAL EXCEPTION`, or the type rewritten to `unknown` — forbidden by HLD decision D4 |
| `object.source: "own_sensor"` | The guard **trips**: yellow `[? UNKNOWN SOURCE]` marker and an ERROR line on `IVI_V2X`. Here the trip is the pass | Ghost C drawn normally → the R19 provenance wiring is broken |
| A raw non-JSON step | `[DROP] reason=malformed …`, and **the next valid warning still renders** | The app stops rendering after the bad message |

### 4.8 Capture the evidence

- **Screen recording:** set **Recorder Part** on the Screen widget *before* the run; the clip lands under **Videos** and downloads as `.mp4`. Videos record at the screen's native resolution, so files are large.
- **Screenshots:** from the Screen widget. If the REST route `GET /api/v1/vms/{roomId}/{nodeKey}/screenshot` answered in §4.3, it is a second, scriptable evidence path.
- **Guest log excerpt:** `adb logcat -s IVI_V2X` — the `[RX] … cSource=v2x_relayed` lines are what back the recording in text.
- **Producer log excerpt:** the ADA node's `[TX]` lines, from **View Log** or the logs route.
- Record every result in the phase's run record, as [phase0-smoke-test-run.md](../../plans/doc/phase0-smoke-test-run.md) does for the smoke test.

### 4.9 Troubleshooting

| Symptom | Meaning | Action |
|---|---|---|
| `failed to connect` / device `offline` | The endpoint or tunnel is wrong | Try the next route in §4.3; do not retry the same one |
| `INSTALL_FAILED_OLDER_SDK` | Guest below API 29 (§4.4) | Blocking finding — escalate; the in-Room plan changes |
| `INSTALL_FAILED_MISSING_SHARED_LIBRARY`, or a feature error naming `automotive` | Not an automotive system image | Same — escalate |
| `INSTALL_FAILED_UPDATE_INCOMPATIBLE` | An older build with a different signature is installed | `adb uninstall com.hackathon.v2x.ivi`, then install again |
| `Error type 3 … Activity class {…/.MainActivity} does not exist` | The installed APK has no launcher entry | §2.5, and the banner at the top — it lands with `16.5.5.5` |
| App starts, then the screen returns to the launcher | Crash on start | Read `adb logcat` **unfiltered** before filtering by tag |
| `V2X LINK: STANDBY` never changes | The status bar is still the committed placeholder | It is bound to the real link state by subtask `17.5.5.6` |
| Node stuck in `Provisioning` | Almost always an image that cannot be pulled | Only affects container nodes; check the ADA node's image field. [carsky-room-diagnostics](../../.claude/skills/carsky-room-diagnostics/SKILL.md) |
| No route to the guest works at all | ADB to the guest is unreachable on this deployment | Fall back to a **physical Android 10+ device** over USB for §4.6–§4.7 and **record that the evidence is device evidence, not in-Room evidence**. On an x86_64 host an Android Automotive emulator is an alternative; on ARM64 Windows it is not (§2.1.1) |

### 4.10 The mini-blueprint alternative

For IVI-only work, a 3-node topology — Ethernet Bridge + ADA node + IVI Skycraft node — deploys faster, has fewer variables, and leaves the second Room slot free for the comms track. The mechanics are exactly §4.1–§4.9; only the composition differs.

- **Design and rationale:** [phase5-mini-blueprint.md](../../IVI_ECU/doc/research_notes/phase5-mini-blueprint.md). The sanctioned deployment procedure lands at `requirements/car-sky-guide/phase5-mini-blueprint-deploy.md` with subtask `5.5.8.1`.
- **Creation route — clone, then delete.** Clone the known-good baseline, rename it, then delete the Bench and V2X nodes on the canvas. Cloning is the only route that preserves `ethernet` pins (§4.1); a blueprint built from scratch by script has no network at all. Deleting a node removes its pin and edge; the ADA and IVI pins are untouched.
- **Do not import** a hand-authored blueprint JSON in place of this. Every such attempt so far omitted the Skycraft `image` block, which gets the deploy rejected outright.
- **The ADA node is the only node that is reconfigured** — probe config (`m1-netcheck:latest`) before the simulator image exists, evidence config (`m1-r4-sim:latest`) after. Both are in §4.7. Addresses, the `47300` port and the pin shapes stay at the baseline values, so the Phase 6 switch to the real ADA image is an image swap with no node-config edit.

### 4.11 Tear down

**Delete Deployment** when finished. Only two Rooms may run at once and the comms track needs the slot; the blueprint itself is kept and redeployable.

---

## 5. Work division between AI and human

The split is not a preference — it follows from what an agent can reach. An agent can run CLI tools and authenticated REST calls; it cannot use the Nydus canvas, a browser download, or its own eyes.

| Action | AI / Human | Description |
|---|---|---|
| [Trigger a CI build](#31-what-the-workflow-does-and-what-triggers-it) | AI | Push a commit; the lane runs unfiltered on every push |
| [Confirm the run passed](#32-tell-when-the-run-finished-and-whether-it-passed) | Human | Actions web UI; the agent session holds no GitHub token |
| [Download `app-debug-apk`](#33-download-the-apk) | Human | Artifact download needs an authenticated token; unzip before use |
| [Prepare the blueprint and its pins](#41-prepare-the-blueprint) | Human | Nydus canvas — REST cannot create `ETHERNET` pins |
| [Read the stored config back](#41-prepare-the-blueprint) | AI | `GET /api/v1/blueprints/{id}`; also captures the Part Prefix and display fields |
| [Deploy the blueprint](#42-deploy-the-blueprint) | Human | **New Deployment** dialog; picking the Device is a human call |
| [Poll node phases until Running](#42-deploy-the-blueprint) | AI | `GET /api/v1/deployments/{roomId}/nodes`; also yields each `nodeKey` |
| [Obtain the ADB endpoint](#43-get-an-adb-endpoint) | Human | Rework device panel or the Gateway tunnel — both are browser surfaces |
| [Probe the 502 REST routes](#43-get-an-adb-endpoint) | AI | One `curl` each for `…/shell` and `…/screenshot` |
| [Connect and read guest properties](#44-connect-and-check-the-guest) | AI | `adb connect`, `adb devices`, `getprop`, `pm list features` |
| [Install the APK](#45-install-the-apk) | AI | `adb install -r`, then `pm path` to confirm |
| [Open the Screen, Log and ADB widgets](#46-open-the-screen-and-launch-the-app) | Human | Devices panel widgets and their part fields |
| [Launch the app](#46-open-the-screen-and-launch-the-app) | AI | `adb shell am start`, with the optional port override |
| [Configure the ADA node's feed](#47-verify-the-chain) | Human | Node Inspector image and env fields, then redeploy |
| [Read the two log surfaces](#47-verify-the-chain) | AI | `adb logcat -s IVI_V2X` and the node logs route |
| [Confirm the display switched](#47-verify-the-chain) | Human | A visual judgement no log line replaces |
| [Record the screen](#48-capture-the-evidence) | Human | Recorder Part, then download the clip from **Videos** |
| [Tear the Room down](#411-tear-down) | Human | **Delete Deployment**; releases one of the two Room slots |

The local build of §2 is the AI-side alternative to the first three rows: an agent with a JDK and an Android SDK produces the same APK without touching a browser.

---

## 6. Expected outputs and acceptance

Four observables prove the phase, and each one closes a specific clause of R16 or R17. Three are text and one is visual; none substitutes for another.

| # | Proof | Where it appears | Closes |
|---|---|---|---|
| 1 | **Incoming R4 message from the ADA ECU** — one `[RX] type=warning bytes=… from=10.99.0.12:…` per datagram | `IVI_V2X` on the guest, corroborated by the ADA node's `[TX]` line | R4 (the consumer half) and **R6 hop 3** — the check Phase 0 could only make indirectly |
| 2 | **The parsed data model** — the same `[RX]` line's `warningType=`, `risk=`, `cSource=` and `cPos=` fields, read off the decoded message and its R3 `object` snapshot | `IVI_V2X` | R4 acceptance: the consumer parses the contract, and an unknown `warningType` degrades gracefully rather than crashing (V5, first row) |
| 3 | **The event raised in the IVI ECU** — `[UI] mode=WarningView cause=warning`, with `cause=warning` and not `cause=user` | `IVI_V2X` | R16 acceptance: *"an R4 warning brings the warning view up in the Display area"* — the message, not a tap, caused the switch |
| 4 | **The HMI switched to the warning display** — the Display Area drawing the God View: ego and B solid, ghost C dashed with its risk glow and `[V2X]` badge | Screen widget, captured as a recording or screenshot | R16 acceptance (the warning view is shown in the Display area) and **R17 acceptance** (the three vehicles, ghost C sourced only from `v2x_relayed`, 2D delivered) |

Two further observations complete R16 and R17 rather than repeating the four above:

| Observation | Closes |
|---|---|
| Tapping **Home / Apps / Settings** changes what the Display Area shows, and `[UI] mode=… cause=user` follows | R16: *"the button/app areas switch what the Display area shows"* |
| `cSource=v2x_relayed` on **every** rendered warning, and the guard tripping to `[? UNKNOWN SOURCE]` on an `own_sensor` message | R17: ghost C sourced **only** from `v2x_relayed`. This pair is also the IVI-side half of the R19 definition of done |

Optional paths — the separate app woken by an ADA message, and 3D through the view seam — are not M1 deliverables. Record them as not built rather than leaving the boxes ambiguous.

### 6.1 Unverified points — confirm before relying on them

Everything below is stated in the platform documentation or follows from the committed code, but has **not** been observed working on this deployment. A step that depends on one of them can fail without this guide being wrong.

| # | Point | Where it bites |
|---|---|---|
| 1 | ADB reach to the Skycraft guest, by any route | §4.3 — the whole in-Room evidence plan |
| 2 | `GET /api/v1/deployments/{roomId}/adb-tunnel` and `POST /…/adb-exec/{nodeKey}` answering on `hackathon-2` | §4.3 |
| 3 | The AAOS guest's API level and its `automotive` feature | §4.4 — either one blocks the install |
| 4 | The IVI node's real Part Prefix, display size and GPU backend | §4.1, §4.6 — the Screen, Log and ADB widgets select parts by these names |
| 5 | Whether the guest display sleeps, and that `KEYCODE_WAKEUP` wakes it | §4.6 |
| 6 | A JDK and an Android SDK being present on the build host | §1.1, §2 — §3 is the route that needs neither |
| 7 | `…/screenshot` and `…/shell` still answering 502 | §4.3, §4.8 |
| 8 | Every expected `IVI_V2X` log line and the bound-link indicator — **designed in the HLD, not yet built** | §4.6, §4.7, §6 |
| 9 | The `ubuntu-latest` runner image's Android SDK and licence state staying sufficient for `compileSdk 34` | §3.1 |
| 10 | Whether the ADB widget can install an APK in practice — the platform says it can, but it exposes no file transfer | §4.3 route #2 |
