# IVI HMI Bring-Up — Build to God View on the AAOS Node

Worked example for the one node that is **not** a container: taking the Kotlin/Compose app in [IVI_ECU/](../../IVI_ECU/) from source to a running HMI on the Skycraft AAOS guest, and proving it renders the R17 God View.

- **Companion to** [deploy-walkthrough-netcheck.md](deploy-walkthrough-netcheck.md) — that guide is the template for **container** nodes (build → push to Zot → node pulls). Read it first for the platform model (blueprint, node, pin, Device, Room) and the credential table; nothing from it is repeated here.
- **Node facts** — VM artifact IDs, the `image` config block, the pin shape, the acceptance list — live in [node-ivi-ecu.md](node-ivi-ecu.md) and are linked, not copied.
- **Serves** R16 and R17 acceptance ([m1-cooperative-awareness.md §2](../m1-cooperative-awareness.md)), plus the R6 hop-3 check the Phase 0 smoke test left open.

**The one structural difference from every other node:** there is no image to push. The node's image is the starter-pack AAOS artifact; the team's deliverable is an APK installed **after** the Room is Running ([IVI_ECU/README.md](../../IVI_ECU/README.md)). That is why deploy comes before install below — the guest must exist before anything can be installed into it.

```
IVI_ECU/  ──gradlew──▶  app-debug.apk  ──copy──▶  your machine  ──adb install──▶  AAOS guest
 (source)                 (step 1)                 (step 2)         (step 4)      in a Running Room (step 3)
```

---

## 0 · Before you start

### 0.1 Toolchain on the build machine

| Need | Value | Why this value |
|---|---|---|
| JDK | **17** (Temurin) | `compileOptions`/`jvmTarget` are Java 17 in [app/build.gradle.kts](../../IVI_ECU/app/build.gradle.kts); CI uses `temurin` 17 |
| Gradle | none to install | The wrapper pins Gradle 8.13 ([gradle-wrapper.properties](../../IVI_ECU/gradle/wrapper/gradle-wrapper.properties)) and downloads it on first run |
| Android SDK | platform **android-34** + build-tools | `compileSdk = 34` |
| `adb` | Android SDK platform-tools | Steps 4–6 |

*Assumption — not verified on the dev host:* that a JDK 17 and an Android SDK are installed at all. Nothing in this repo records them; [deploy-walkthrough-netcheck.md §2.2](deploy-walkthrough-netcheck.md) only records that the dev machine has no Docker and no Linux. If the SDK is absent, step 1 fails with `SDK location not found` and the CI artifact route (§1.5) is the faster path.

Point Gradle at the SDK either way — `ANDROID_HOME` in the environment, or a `local.properties` beside `IVI_ECU/settings.gradle.kts` containing one line:

```
sdk.dir=C\:\\Users\\<you>\\AppData\\Local\\Android\\Sdk
```

`local.properties` is git-ignored on purpose ([.gitignore](../../.gitignore)) — it is machine-specific and must never be committed.

### 0.2 Platform access

Per [carsky-deploy-preflight](../../.claude/skills/carsky-deploy-preflight/SKILL.md): the Keycloak login for the UIs, and a CarSky API key (`a8k_…`) for the REST calls in steps 3–4. No Zot key is needed anywhere in this guide — this node pushes no image.

### 0.3 What must already be built

This walkthrough is executable **only as far as the artifacts it consumes exist**. Check this table before following it end to end.

| Input | State on `main` | Lands with |
|---|---|---|
| An APK that can be *installed* | Present — `./gradlew assembleDebug` works today | — |
| An APK that can be *launched* | **Absent.** No `<activity>` in [AndroidManifest.xml](../../IVI_ECU/app/src/main/AndroidManifest.xml) and no `MainActivity`, so the APK installs and has no launcher entry | `16.5.5.5` ([phase5_minh_tasks.md](../../plans/phase5_minh_tasks.md)) |
| The R4 listener (`IVI_V2X` log lines, socket on 47300) | **Absent** | `4.5.5.2`, `4.5.3.3` |
| The dev injector (I3 broadcast) | **Absent** | `4.5.6.7` |
| The R4 simulator image `m1-r4-sim:latest` | **Absent** | `5.5.6.6`, `5.5.7.3` |
| CI lane publishing `app-debug-apk` | **Absent** — no `phase5-ci.yml` | `16.5.7.1` |
| A Room with a Running Skycraft node | Deployable today from the baseline blueprint | — |

Until the launcher entry lands, steps 1–4 still run and are worth running: they prove the **route** to the guest, which is the phase's biggest schedule risk (`16.5.8.3`). Steps 5–6 need the app.

---

## 1 · Build the APK

Everything in this step runs from the `IVI_ECU/` folder.

### 1.1 Run the build

| Host | Command |
|---|---|
| Windows (PowerShell) | `.\gradlew.bat assembleDebug` |
| Linux / macOS / CI | `chmod +x gradlew && ./gradlew assembleDebug` |

```powershell
cd C:\Users\<you>\Documents\Work\FPT-Hackathon2026\IVI_ECU
.\gradlew.bat assembleDebug
```

Expected tail:

```
BUILD SUCCESSFUL in 1m 12s
```

First run is slow — the wrapper downloads Gradle 8.13 and the build resolves AGP, Kotlin and the Compose BOM.

### 1.2 Run the unit tests

Do this before shipping an APK anywhere; it is the same gate CI applies.

```powershell
.\gradlew.bat :app:testDebugUnitTest
```

The full five-module command replaces it once the module split lands — the command table in [phase5_minh_tasks.md § Build & verification commands](../../plans/phase5_minh_tasks.md) is authoritative for which invocation is valid at which point.

### 1.3 Where the artifact lands

```
IVI_ECU/app/build/outputs/apk/debug/app-debug.apk
```

That exact path is what every later step and the CI upload step refer to.

### 1.4 Verify the APK is launchable

An APK with no launcher activity installs cleanly and cannot be started — the failure this check exists to catch:

```bash
"$ANDROID_HOME/build-tools/<version>/aapt" dump badging app/build/outputs/apk/debug/app-debug.apk | grep launchable-activity
```

| Result | Meaning |
|---|---|
| `launchable-activity: name='com.hackathon.v2x.ivi.MainActivity' …` | Launchable — step 5 will work |
| *(no output)* | No launcher entry. Expected on `main` today (§0.3); use this build for route-proving only |

### 1.5 Alternative — take the APK from CI

Once the `ivi-assemble` lane exists, GitHub → **Actions** → the `phase5-ci` run → artifact **`app-debug-apk`**. Downloading it skips steps 1.1–1.3 entirely and needs no local Android SDK. The lane is not in the repo yet (§0.3).

### 1.6 Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `SDK location not found. Define a valid SDK location with an ANDROID_HOME environment variable or by setting the sdk.dir path in your project's local properties file` | No SDK path | §0.1 |
| `Unsupported class file major version` / toolchain error naming a JDK | Wrong JDK on `JAVA_HOME` | Use JDK 17 |
| `./gradlew: Permission denied` | Wrapper not executable after a fresh clone on Linux | `chmod +x gradlew` |
| `Build was configured to prefer settings repositories … but repository 'X' was added by build file` | A module declared its own `repositories` block | Forbidden by `FAIL_ON_PROJECT_REPOS` in [settings.gradle.kts](../../IVI_ECU/settings.gradle.kts) — remove it |

---

## 2 · Export the artifact off the build machine

The deliverable is one file. There is nothing to tag, push, or register.

```powershell
$apk = "IVI_ECU\app\build\outputs\apk\debug\app-debug.apk"
Copy-Item $apk "$HOME\Desktop\app-debug.apk"
Get-FileHash -Algorithm SHA256 "$HOME\Desktop\app-debug.apk"
```

- **Record the hash and the file size.** They are the only way to prove later that the build on the guest is the build you made — `adb install` reports nothing about provenance.
- **From CI:** the downloaded artifact is a `.zip`; unzip it and use the `app-debug.apk` inside. Installing the zip fails.
- **Do not push it to Zot.** The registry holds container images; this node pulls its VM image from the artifact store instead ([node-ivi-ecu.md § Prepare the VM artifact](node-ivi-ecu.md#prepare-the-vm-artifact-once-per-team-not-per-deploy)).

---

## 3 · Deploy the blueprint

### 3.1 Pick the blueprint

| Blueprint | Use when |
|---|---|
| The 5-node baseline ([carsky-4-node-blueprint.md](carsky-4-node-blueprint.md)) | Full-chain work — bench, V2X, ADA, IVI, bridge |
| The 3-node mini-blueprint — bridge + ADA + IVI ([phase5-mini-blueprint.md](../../IVI_ECU/doc/research_notes/phase5-mini-blueprint.md)) | IVI work alone: fewer nodes, faster deploy, leaves the second Room slot free |

**Create the mini-blueprint by cloning the baseline and deleting the Bench and V2X nodes.** Cloning is the only route that preserves `ethernet` pins — the reasoning and the ordered steps are in that note's § Creation route; do not rebuild it from a JSON file.

### 3.2 The Skycraft node's `image` block — the deploy blocker

The block, its artifact IDs, and the exact rejection message are in [node-ivi-ecu.md § Blueprint node config](node-ivi-ecu.md#blueprint-node-config). Two rules:

- On a clone it is already correct — **leave it alone**.
- On a hand-authored or imported node it is usually missing, and the deploy is rejected outright rather than starting and failing later.

### 3.3 The `ethernet` pin — the manual UI step

The pin shape and its address are in [node-ivi-ecu.md § Pins](node-ivi-ecu.md#pins). What matters procedurally:

- **Neither the REST API nor Nydus "Import from File" can create an `ETHERNET` pin** ([carsky-rest-api-blueprint.md](carsky-rest-api-blueprint.md)) — an import silently drops it, so an imported IVI node has no network at all.
- A **clone keeps its pins**; an import does not. Prefer cloning.
- If the pin is missing, draw it by hand on the Nydus canvas and wire it to the Ethernet Bridge node. Same-type wiring only (`ethernet ↔ ethernet`).
- **Verify by reading the stored config back**, not by trusting the Inspector's truncated fields:

```bash
curl -H "Authorization: Bearer $KEY" $CS/api/v1/blueprints/{id}
```

Expect one `ETHERNET` / `OUTPUT` pin on the IVI node with `properties.address` = `10.99.0.13`, wired to the bridge's single `INPUT` pin.

### 3.4 Record the Skycraft display config

The Nydus Inspector's CONFIGURATION group on a Skycraft node carries fields no other node has, and step 5 needs two of them:

| Field | Why you need it |
|---|---|
| **Display Width / Height / DPI** | The virtual screen resolution the Screen widget shows. The committed R16 previews are drawn for 1280×720 ([MainScreen.kt](../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/ui/screen/MainScreen.kt) preview names) |
| **Part Prefix** | Auto-names every "part" of the node — `<prefix>-screen`, `<prefix>-audio`, `<prefix>-logcat`, `<prefix>-adb`. Step 5 selects parts by these names |
| **GPU Backend** | The virtual graphics driver given to the VM |

*Unverified:* the actual values on our IVI node. Read them off the node's Inspector (or the config read-back in §3.3) and write them down before you need them — do not assume 1280×720.

### 3.5 Deploy and wait

**New Deployment** → pick an existing **Device** (the K8s resource pool) → **Deploy**. Then wait for every node badge to read `Running` with restart count 0.

**The Skycraft node is the slowest to reach Running** — expect it to lag the containers. Deploy-dialog details and the two-concurrent-Room budget: [deploy-walkthrough-netcheck.md § M9](deploy-walkthrough-netcheck.md), diagnosis when a node hangs: [carsky-room-diagnostics](../../.claude/skills/carsky-room-diagnostics/SKILL.md).

---

## 4 · Install the APK into the AAOS guest

> This step has **never been proven on this deployment**. It is the phase's biggest schedule risk (`16.5.8.3`), and its outcome decides whether the R16/R17 evidence is in-Room or emulator-only. Run it early, against today's APK, before the app is finished.

### 4.1 Get an ADB endpoint

Three candidate routes, strongest first. Record which one actually worked.

| # | Route | State |
|---|---|---|
| 1 | `GET /api/v1/deployments/{roomId}/adb-tunnel` — returns ADB tunnel command info for a Skycraft node | Documented by the platform ([Car-Sky-Platform.html](../development-platform-doc/Car-Sky-Platform.html) § API & MCP Tools). **Unverified on `hackathon-2`** |
| 2 | Devices panel → the device the deployment auto-created → **Connect** → add the **ADB** widget → an ADB shell terminal in the browser | Documented; gives a shell, **not** a file transfer — it cannot take a local `.apk` |
| 3 | `POST /api/v1/deployments/{roomId}/adb-exec/{nodeKey}` — one-shot ADB command | Documented. **Unverified**, and same limitation as #2 |

The MCP tunnel tools (`vm_tunnel_open`) are **not usable here** — they require CarSky's own MCP server package, which this repo does not carry ([carsky-rest-api-blueprint.md](carsky-rest-api-blueprint.md)).

The `/api/v1/vms/{roomId}/{nodeKey}/shell` route is **known-502** on this deployment and must not be the primary plan. Probe it anyway while you are here, together with `…/screenshot` — a live `screenshot` route is a second evidence path in step 6 that does not depend on ADB at all.

`{nodeKey}` comes from `GET /api/v1/deployments/{roomId}/nodes` (the `name` field), and `{roomId}` is the device id.

### 4.2 Connect

```bash
adb connect <skycraft-adb-endpoint>
adb devices
```

Expected:

```
List of devices attached
<endpoint>   device
```

`offline` or an empty list means the endpoint is wrong or the tunnel is not up — go to §4.5 rather than retrying blind.

### 4.3 Check the guest will accept this APK

Two properties decide it, and both are unknown until you read them:

```bash
adb shell getprop ro.build.version.sdk
adb shell getprop ro.build.version.release
adb shell pm list features | grep automotive
```

| Reading | Requirement | If it fails |
|---|---|---|
| `ro.build.version.sdk` | ≥ **29** (`minSdk 29`) | Install is rejected outright; the APK's `minSdk` would have to change |
| `android.hardware.type.automotive` present | The manifest declares it `required="true"` | A non-automotive guest refuses the install |

### 4.4 Install and verify

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

### 4.5 Troubleshooting

| Symptom | Meaning | Action |
|---|---|---|
| `failed to connect` / device `offline` | The endpoint or tunnel is wrong | Try the next route in §4.1; do not retry the same one |
| `INSTALL_FAILED_OLDER_SDK` | Guest below API 29 (§4.3) | Blocking finding — escalate; the in-Room plan changes |
| `INSTALL_FAILED_MISSING_SHARED_LIBRARY` / a feature error naming `automotive` | Not an automotive system image | Same — escalate |
| `INSTALL_FAILED_UPDATE_INCOMPATIBLE` | An older build with a different signature is installed | `adb uninstall com.hackathon.v2x.ivi`, then install again |
| No route works at all | ADB to the guest is unreachable on this deployment | Fall back to an **Android Automotive** emulator for steps 5–6 (a phone image refuses the APK), and **record that the evidence is emulator evidence** |

---

## 5 · Open the screen and launch the HMI

### 5.1 Open the device's screen

The display is a **widget in the Devices panel**, not something in the Deployment Viewer.

1. **Devices** on the dock. The deployment auto-creates its device; the platform marks such an entry `🚀 Started pack-deploy`. Do not delete it by hand — stop the deployment instead.
2. **Connect** (the button reads **Switch** if another device session is already open). The dot beside the name must be green.
3. `+` → **Screen** from the widget catalog.
4. In the Inspector, set the parts from the node's Part Prefix (§3.4):
   - **Video Part** → `<prefix>-screen`
   - **Touch Part** / **Keyboard Part** → the corresponding parts, or clicks in the browser never reach the guest
   - **Recorder Part** → set it when you want the run recorded; the clip appears under **Videos** after recording stops, downloadable as `.mp4`

Expected: the AAOS screen streaming live in the Stage, and clicking on it acting like a touch.

**Black screen?** Per the platform FAQ: check that **Video Part** names the part the node actually publishes (same prefix as its Part Prefix); if the part is right, **Disconnect** then **Connect** to re-establish the session.

*Unverified:* whether the guest's display ever sleeps in a Room. If the stream is up but the guest looks asleep, `adb shell input keyevent KEYCODE_WAKEUP` is the standard AOSP wake command — untested against this guest.

Two more widgets are worth adding now: **Log**, with source part `<prefix>-logcat` (the guest's logcat in the browser), and **ADB**, for a shell beside the screen.

### 5.2 Launch the app

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

| Symptom | Meaning |
|---|---|
| `Error type 3 … Activity class {…/.MainActivity} does not exist` | The installed APK has no launcher entry — §1.4, and §0.3 for when that lands |
| App starts, then the screen returns to the launcher | Crash on start — read `adb logcat` unfiltered before filtering by tag |
| `V2X LINK: STANDBY` never changes | The status bar is still the committed placeholder; it is bound to the real link state by subtask `17.5.5.6` |

### 5.3 Record the boot-to-listener time

While the app is starting for the first time, record two wall-clock deltas: guest boot → launcher, and launch → the first `[LINK] state=bound` line. Their sum is the floor for the bench's start delay, and Phase 5 is the only phase that can measure it (`16.5.9.2`).

---

## 6 · Verify the front end works

One log filter carries all the app's evidence:

```bash
adb logcat -s IVI_V2X
```

Work up the ladder — each rung needs less to exist than the one below it, so start at the highest rung your build supports.

### V1 — the socket is bound

| | |
|---|---|
| **Feed** | Nothing; launching the app is enough |
| **Correct** | `[LINK] state=bound port=47300` |
| **Incorrect** | No `[LINK]` line at all (listener absent or not started), or a bind error followed by rebind attempts (port already taken — relaunch with `--ei r4_port`) |

### V2 — a datagram reaches the guest (works before the simulator exists)

| | |
|---|---|
| **Feed** | Point the ADA node at the IVI: image `m1-netcheck:latest`, `NEXT_HOP_HOST=10.99.0.13`, `NEXT_HOP_PORT=47300` — the probe config of subtask `5.5.8.2`. The ADA node's **View Log** shows `[TX] … relayed to 10.99.0.13:47300` |
| **Correct** | `[DROP] reason=malformed bytes=… preview="seq=…"` on `IVI_V2X`, one per datagram, and the app keeps running. netcheck's payload is not JSON, so a drop **is** the pass — it proves the socket, the bridge hop and the loop's survival |
| **Incorrect** | ADA logs `[TX]` but the IVI logs nothing → the datagram is not arriving; re-check the pin address and the port |
| **Closes** | R6 hop 3 — the check Phase 0 could only make indirectly ([deploy-walkthrough-netcheck.md § Checking IVI RX traffic](deploy-walkthrough-netcheck.md)) |

### V3 — the UI comes up, with no network at all

| | |
|---|---|
| **Feed** | `adb shell am broadcast -a com.hackathon.v2x.ivi.DEV_INJECT --es sample r4-warning` |
| **Correct** | The Display Area switches to the Warning View **by itself**, drawing the God View |
| **Incorrect** | `Broadcast completed: result=0` with no UI change → wrong build type: the injector exists in the **debug** build only, by design |

### V4 — a real R4 message: the God View (R16 + R17 acceptance)

| | |
|---|---|
| **Feed** | ADA node → simulator image `m1-r4-sim:latest`, `R4_SCENARIO=/app/scenarios/approach.json`, `R4_RATE_HZ=1`, `IVI_ECU_HOST=10.99.0.13`, `IVI_ECU_PORT=47300` — the evidence config in [phase5-mini-blueprint.md §4](../../IVI_ECU/doc/research_notes/phase5-mini-blueprint.md) |
| **Correct — screen** | Display Area switches itself to the Warning View. `EGO` and `B` drawn solid; **C dashed** with a pulsing risk glow and the badge `[V2X] C · <d> m · RISK: HIGH`; connector labels `d_AB = <n> m` and `d_AC ≈ <n> m`. The scenario's first step carries `geometry.vehicleC: null` and must render **without C and without a crash or placeholder** |
| **Correct — log** | One `[RX] type=warning … cSource=v2x_relayed` per rendered warning. **This is the R19 claim in text** — the recording shows ghost C, the log proves it came from relayed data |
| **Incorrect** | A yellow **`[? UNKNOWN SOURCE]`** marker where ghost C should be. The provenance guard tripped: the scene reached the renderer without a `v2x_relayed` snapshot. On `approach.json` this is a **blocking defect**, not a display quirk |
| **Also check** | Let the stream stop: the view times out to Idle and the previous mode is restored |

### V5 — degradation, the guard, and loop survival (R4 acceptance)

Switch the ADA node to `R4_SCENARIO=/app/scenarios/degrade.json`.

| Case in the scenario | Correct result | Incorrect result |
|---|---|---|
| Unknown `warningType` + `schemaVersion: 2` + a junk field | A generic warning renders; logcat shows the wire value **preserved** (`warningType=slippery_road`) and one schema-version-ahead notice | `FATAL EXCEPTION`, or the type rewritten to `unknown` — forbidden by HLD decision D4 |
| `object.source: "own_sensor"` | The guard **trips**: yellow `[? UNKNOWN SOURCE]` marker and an ERROR line on `IVI_V2X`. Here the trip is the pass | Ghost C drawn normally → the R19 provenance wiring is broken |
| A raw non-JSON step | `[DROP] reason=malformed …`, and **the next valid warning still renders** | The app stops rendering after the bad message |

### 6.1 Capture the evidence

- **Screen recording:** set **Recorder Part** on the Screen widget before the run; the clip lands in **Videos** and downloads as `.mp4`.
- **Screenshots:** from the Screen widget. The REST route `GET /api/v1/vms/{roomId}/{nodeKey}/screenshot` was 502 in Phase 0 — probe it (§4.1); if it answers, it is a second evidence path.
- **Log excerpt:** `adb logcat -s IVI_V2X` — the `[RX] … cSource=v2x_relayed` lines are what back the recording in text.
- Record every result in the phase's run record, as [phase0-smoke-test-run.md](../../plans/doc/phase0-smoke-test-run.md) does for the smoke test.

### 6.2 Tear down

**Delete Deployment** when finished. Only two Rooms may run at once, and the comms track needs the slot; the blueprint itself is kept and redeployable.

---

## 7 · Unverified points — confirm before relying on them

Everything below is stated somewhere in the platform doc or follows from the code, but has **not** been observed working on this deployment. A step that depends on one of them can fail without the guide being wrong.

| # | Point | Where it bites |
|---|---|---|
| 1 | ADB reach to the Skycraft guest, by any route | §4 — the whole in-Room evidence plan |
| 2 | `GET /api/v1/deployments/{roomId}/adb-tunnel` and `POST /…/adb-exec/{nodeKey}` answering on `hackathon-2` | §4.1 |
| 3 | The AAOS guest's API level and its `automotive` feature | §4.3 — either one blocks the install |
| 4 | The IVI node's real Part Prefix, display size and GPU backend | §3.4, §5.1 — the Screen widget needs the part names |
| 5 | Whether the guest display sleeps, and that `KEYCODE_WAKEUP` wakes it | §5.1 |
| 6 | A JDK 17 and an Android SDK being present on the dev host | §0.1, §1 |
| 7 | `…/screenshot` and `…/shell` being 502 *still* (both were in Phase 0) | §4.1, §6.1 |
| 8 | Every expected `IVI_V2X` log line and the bound-link indicator — designed, not yet built | §5.2, §6 |
