# Phase 5 — IVI HMI AAOS build & deployment

How to build the team APK, install it on the CarSky Skycraft (AAOS) node, and verify R4 ingest on a deployed Room. Node-level blueprint/VM config lives in [node-ivi-ecu.md](../../requirements/car-sky-guide/node-ivi-ecu.md); this note is the APK path only.

## Prerequisites

- Android SDK Platform-Tools, with `adb` on `PATH` — it installs to `%LOCALAPPDATA%\Android\Sdk\platform-tools`; add that folder to `PATH` if `adb version` does not answer
- The `reach-backend` tunnel CLI, unpacked at `tools/apk-uploader/reach_be/reach/` (organizer-supplied; git-ignored, so it will not arrive with a fresh clone)
- A CarSky workbench login
- **For Route B of Step 1 only** — JDK 17+ (the Android Studio JBR is fine) and the Gradle wrapper already in this tree (`IVI_ECU/gradlew` / `gradlew.bat`); no global Gradle install. Route A needs neither.

Working directory for the Gradle commands in Step 1 Route B: **`IVI_ECU/`**. Every PowerShell block in Steps 2–5 runs from the **repo root**.

## Step 1 — Get `app-debug.apk` into `tools/apk-uploader/`

Two routes produce the same artifact. Pick either; the chain ends at the same place — the APK sitting beside the uploader tool, which is where Step 3 reads it from.

### Route A — download the CI build

The `ivi-assemble` lane builds the APK on every push and publishes it as a run artifact. No local Android toolchain needed.

1. GitHub → the repo → **Actions**.
2. Pick the workflow **phase5-ci.yml** in the left-hand list, then the run you want (the latest green one on your branch).
3. Open the **`ivi-assemble`** job.
4. Confirm its last step, **Upload the debug APK**, succeeded — its log ends with `Artifact app-debug-apk has been successfully uploaded!` and an artifact download URL.
5. Download the artifact **`app-debug-apk`** from the run summary page (it downloads as `app-debug-apk.zip`) and unzip it. Inside is a single file, `app-debug.apk`.

The artifact name `app-debug-apk` is stable and must not be renamed — this document and the walkthrough both tell a human to fetch exactly that name.

### Route B — build locally in Android Studio / Gradle

Working directory `IVI_ECU/`:

```bash
# Linux / macOS
./gradlew assembleDebug

# Windows
gradlew.bat assembleDebug
```

In Android Studio the equivalent is **Build → Build Bundle(s) / APK(s) → Build APK(s)**. Output:

```text
IVI_ECU/app/build/outputs/apk/debug/app-debug.apk
```

Acceptance: build succeeds; APK size under 50 MB (`dir` / `ls -lh` on the path above).

### Then — copy it to the uploader folder

Whichever route produced it, put the APK at:

```text
tools/apk-uploader/app-debug.apk
```

Overwrite the copy already there. Application id: `com.hackathon.v2x.ivi`.

## Step 2 — Deploy the blueprint and copy the tunnel command (browser, human)

Everything here happens in the CarSky workbench. Nothing is run on your machine yet; this step's only output is the two commands you copy at the end, which Step 3 runs.

1. **Log in** to the CarSky workbench at `https://hackathon-2.carsky.io`.
2. **Deploy the blueprint you want to test on**, and wait for it to go **green — every node in `Running` state**. The Skycraft (IVI) node is the slowest to come up; wait for it rather than assuming it, because a tunnel started against a node that is not yet running has nothing to dial.

   > **Which blueprint?** The isolated IVI-ECU bring-up and the full system test are **different blueprints**. Deploy whichever one this run is for. This document names neither on purpose, so it does not go stale when a blueprint is renamed or replaced.

3. **Devices** → find **the device the deployed blueprint is running on** → **Connect**. The button reads **Switch** instead if another device session is already open. After connecting, the dot beside the device name must be green.
4. **Choose the IVI ADB widget** in the panel below the Stage. The ADB SHELL panel opens with a green `connected` badge.
5. Top-right of that panel → **Local ADB**. A dialog opens showing two copyable commands. **Copy both** — they are the input to Step 3:

   ```text
   reach-backend adb --gateway https://hackathon-2.carsky.io --key a8k_…
   adb connect localhost:5555
   ```

   The first carries the gateway URL and the **per-device `a8k_` token**; the second is the port the tunnel will serve on.

![The IVI ADB widget selected in the Devices panel, its ADB SHELL tab open below the Stage, and the Local ADB button top-right of that panel](images/ivi-adb-local-adb.png)

The three boxes mark the three things this step depends on, in the order the sub-steps above reach them: the **IVI ADB** widget in the device's Widgets list (step 4), the **IVI ADB** tab that opens in the panel below the Stage, and **Local ADB** at that panel's top right (step 5). **Reconnect** beside it re-dials the shell after a redeploy; it does not re-mint the token — the Local ADB dialog is the only place the current one is shown.

The `a8k_` token is **derived per device, not a CarSky API key**, and a redeploy mints a new one — so re-open this dialog after every redeploy instead of reusing an old value. Keep it out of the repository: paste it into `secrets/reach-adb-token-ivi.txt` (one line, no quotes; the folder is git-ignored) rather than into a command line, a log, or a document.

## Step 3 — Establish the tunnel and install the APK (local machine, two terminals)

Step 2 produced the two commands; this step runs them. The first terminal holds the tunnel open for as long as you need the guest, and the second does the install through it.

### Terminal 1 — establish the tunnel, then leave it alone

The command copied from the dialog names the CLI as bare `reach-backend`, which is not on `PATH` — type the repo's own binary by its path instead, and paste your `a8k_` token in place of the placeholder. From the repo root:

```powershell
.\tools\apk-uploader\reach_be\reach\reach-backend.exe adb --gateway https://hackathon-2.carsky.io --key a8k_PASTE_YOUR_TOKEN_HERE --port 5555
```

`reach-backend.exe` is the Windows build; `reach-backend` beside it is the macOS/Linux one.

This opens a local TCP server on `localhost:5555`. **Leave this terminal open** — closing it drops the tunnel and every later command fails. If it exits immediately, it is one of three things: the token is stale (redeploy → re-copy it from the Local ADB dialog), the gateway URL is wrong, or port 5555 is already taken (pass a different `--port` here and use that same port below).

### Terminal 2 — connect and install

Every `adb` command in this document assumes `adb` is on `PATH`. If it is not, add `%LOCALAPPDATA%\Android\Sdk\platform-tools` to your `PATH` once and open a fresh terminal; the commands below then work exactly as typed.

```powershell
adb connect localhost:5555
adb devices
adb install -r .\tools\apk-uploader\app-debug.apk
adb shell pm list packages
```

Expected, in order: `connected to localhost:5555`; a `localhost:5555   device` row; `Success`; and `package:com.hackathon.v2x.ivi` somewhere in the package list. `-r` reinstalls over an existing copy and keeps its data — use it on every reinstall.

If `adb devices` shows `offline` or nothing at all, the tunnel in Terminal 1 is not serving. Do not retry the install blind — check that terminal is still alive and its blueprint still `Running`, restart it, then reconnect.

## Step 4 — Test

**The app launches itself.** Once the blueprint is correctly deployed and the APK is installed, it comes up on the guest with no `am start` and no tap — [AndroidManifest.xml](../app/src/main/AndroidManifest.xml) declares `MainActivity` as the only `MAIN`/`LAUNCHER` activity on the node. An app that has to be started by hand is a finding: either the install did not take, or the guest is not the node you think it is.

### The two test paths

They differ only in **what produces the R4 warning stream**. The app, the install and the evidence are identical across both, so they are listed in the order you would run them — the second adds real components upstream of the app.

| Path | What produces the R4 stream | What it proves that the other cannot |
|---|---|---|
| **Isolated test** | The `m1-r4-sim` simulator on the ADA node, driven by a scenario file | The app parses R4 and wakes the Warning View, with degraded cases a live run cannot reproduce on demand |
| **System test** | The full chain — bench → V2X ECU → ADA ECU | That ghost C on the screen came from a relayed detection, which is the milestone's definition of done |

> **Addresses are the same in both**, because every path is derived from the same subnet: bridge `10.99.0.1/24`, bench `10.99.0.10`, V2X `10.99.0.11`, ADA (or whatever stands in for it) `10.99.0.12`, IVI `10.99.0.13`. The IVI node's own config never changes between paths — same address, same pin, same `image` block.

### Path 1 — Isolated IVI test

Three nodes. The bench and V2X nodes contribute nothing to display work, and every node removed is one fewer image that can fail to pull while the Skycraft guest — always the slowest to reach `Running` — is still booting.

| Node name | Image to deploy | Its config |
|---|---|---|
| **Ethernet Bridge** | — (node type `eth-bridge`, no image) | `bridgeMode: linux`, `subnet: 10.99.0.0/24` |
| **ADA ECU** (simulator standing in) | `registry.hackathon-2.carsky.io/m1-r4-sim:latest` | `command: ["./entrypoint.sh"]` · `IVI_ECU_HOST=10.99.0.13`, `IVI_ECU_PORT=47300`, `R4_SCENARIO=/app/scenarios/approach.json`, `R4_RATE_HZ=1`, `START_DELAY_S=20` · pin `10.99.0.12` |
| **IVI ECU** | The AAOS VM artifact — **Artifacts → AAOS**, version `0.0.1`, `arch aarch64`; no registry pull | `prefix: ivi`, `displayWidth: 1920`, `displayHeight: 1080`, `gpuBackend: virglrenderer` · pin `10.99.0.13` |

`START_DELAY_S=20` exists so the simulator is not already mid-stream when the guest finishes booting.

**Steps to deploy:**

1. Deploy the blueprint **`<blueprint name>`** — it carries the three nodes above with their `ethernet` pins already wired to the bridge.
2. Confirm the ADA node carries the simulator image and the env of the table above. Nothing about the IVI node changes between this path and Path 2.
3. Wait for all three nodes green, then run Steps 2–3 to install the APK.
4. Swap `R4_SCENARIO` to `/app/scenarios/degrade.json` and redeploy that node to exercise the degraded cases: an unknown `warningType` with `schemaVersion: 2` must render a generic warning while **preserving the wire value** in the log; an `object.source: "own_sensor"` message must **trip** the provenance guard into a yellow `[? UNKNOWN SOURCE]` marker — on that scenario the trip is the pass; a raw non-JSON step must be dropped with the next valid warning still rendering.

### Path 2 — System test

The full five-node blueprint — the chain the milestone is judged on.

| Node name | Image to deploy | Its config |
|---|---|---|
| **Ethernet Bridge** | — (node type `eth-bridge`, no image) | `bridgeMode: linux`, `subnet: 10.99.0.0/24` |
| **Bench — Scenario Player** | `registry.hackathon-2.carsky.io/m1-scenario-player:latest` | `command: ["python", "main.py"]` · `SCENARIO_CONFIG=/app/scenarios/default.yaml`, `V2X_ECU_HOST=10.99.0.11`, `V2X_ECU_PORT=47100` · pin `10.99.0.10` |
| **V2X ECU** | `registry.hackathon-2.carsky.io/m1-v2x-ecu:latest` | `command: ["./v2x_ecu"]` · `LISTEN_PORT=47100`, `ADA_ECU_HOST=10.99.0.12`, `ADA_ECU_PORT=47200` · `capabilities: ["NET_RAW"]` for capture · pin `10.99.0.11` |
| **ADA ECU** | `registry.hackathon-2.carsky.io/m1-ada-ecu:latest` | `command: ["./ada_ecu"]` · `V2X_LISTEN_PORT=47200`, `IVI_ECU_HOST=10.99.0.13`, `IVI_ECU_PORT=47300`, `GATE_ENTER_M=30`, `GATE_EXIT_M=35` · pin `10.99.0.12` |
| **IVI ECU** | The AAOS VM artifact — **Artifacts → AAOS**, version `0.0.1`, `arch aarch64`; no registry pull | `prefix: ivi`, `displayWidth: 1920`, `displayHeight: 1080`, `gpuBackend: virglrenderer` · pin `10.99.0.13` |

The IVI row is byte-identical to Path 1's. That is deliberate: converging from the isolated Room to this one is an image swap on the ADA node, never an IVI config edit.

**Steps to deploy:**

1. Confirm all four container images are actually in the registry — not merely that their CI lanes were green.
2. Create the blueprint by cloning one that already has pins. If you import the five nodes from JSON instead, expect to add **all five ethernet pins and their edges to the bridge by hand**, and to fill in the Skycraft `image` block, which import usually drops.
3. Deploy and wait for every node green. The Skycraft node is last.
4. Run Steps 2–3 to install the APK.
5. Restart the Bench node to replay the scenario from its first step.

Pass is the whole chain: the bench emits CPMs, the V2X ECU decodes and relays, the ADA ECU gates and emits a warning, and the app draws ghost C — with **zero direct detections of C on the ego vehicle**, every rendered frame sourced from `v2x_relayed`.

## Step 5 — Evidence collection

Both paths converge here — the evidence is the same regardless of what produced the stream, and no single surface is sufficient on its own. The screen does not prove where the data came from, the log does not prove anything rendered, and neither proves what actually crossed the wire.

### 1 · Warning screen

- **Screen recording** — set **Recorder Part** on the Screen widget *before* the run starts; the clip lands under **Videos** and downloads as `.mp4`. Recording is at native resolution, so files are large.
- **Screenshots** — from the same Screen widget.
- What must be visible: `EGO` and `B` drawn solid, **C dashed** with a pulsing risk glow and the badge `[V2X] C · <d> m · RISK: HIGH`, and the connector labels `d_AB` and `d_AC`. A yellow `[? UNKNOWN SOURCE]` marker where ghost C belongs means the provenance guard tripped — on the approach scenario that is a blocking defect, not a display quirk.

### 2 · Logs

Two surfaces, both required — one for the producer, one for the app.

| Surface | How to read it | What it carries |
|---|---|---|
| **Node log** (the sending container) | Deployment Viewer → the node → **View Log** | `[TX]` lines from the producer, and `[CAP]` tcpdump lines on nodes that have `NET_RAW` |
| **Guest logcat** (the app) | The **Log** widget, or `adb logcat` over the tunnel | `[LINK]`, `[RX]`, `[DROP]`, `[UI]` — everything the app does |

```powershell
adb logcat -s IVI_V2X R4ListenerService R4Deserializer MainViewModel WarningViewModel
```

The app is already running by the time you attach, so **dump the buffer rather than waiting for the lines live** — add `-d` to the command above. The ring buffer still holds the startup `[UI] mode=HomeView` line and the first `[UI] mode=WarningView cause=warning`, and the interval between them is itself an acceptance observable. Keep logcat's default `threadtime` format, which is what carries the timestamps that interval is read from.

The line that matters most is `[RX] type=warning … cSource=v2x_relayed cPos=(…)`. Its fields are read off the **parsed** message, so it is the proof that the JSON decoded into the typed model — and `cSource=v2x_relayed` on every rendered warning is the definition of done in text.

Also fail the run on any fatal exception in this process:

```powershell
adb logcat *:E | Select-String -Pattern "FATAL","com.hackathon.v2x.ivi"
```

Two things that look like defects and are not: an empty `-s IVI_V2X` stream before the first datagram arrives is normal, and on the current debug build the bind is logged as `R4ListenerService: UDP socket open on port <port>` rather than the designed `[LINK] state=bound` line — read the bind evidence from the `R4ListenerService` tag and corroborate it with `/proc/net/udp` showing `*:47300`.

**There is no in-app injector.** No `DEV_INJECT` broadcast receiver exists in the app source or its manifest, so the UI cannot be driven without a real datagram — every path above has to put one on the wire. The only R4 messages the repository builds without a network are JVM fixtures under `IVI_ECU/app/src/test/resources/contracts/samples/`, which exercise the parser in unit tests and never reach a running guest.

### 3 · Wireshark

There is **no platform pcap facility** — capture runs inside the container. A node needs `"capabilities": ["NET_RAW"]` flat in its `config`; without it the capture degrades to `/proc/net/dev` packet counters. The node image runs two tcpdump processes: one printing `[CAP]`-prefixed lines to stdout for the live "traffic is flowing" check, and one writing a rotated pcap that is emitted to stdout as base64 between `[PCAP-BEGIN <name>]` and `[PCAP-END]` markers — the log is the node's only egress, and base64 keeps the export byte-perfect.

1. Deployment Viewer → the capturing node → **View Log** → save it locally as e.g. `v2x.log`.
2. Extract the captures: `V2X_ECU/tools/extract_pcap.sh v2x.log` writes one `.pcap` per block beside the log. One-block fallback:

   ```bash
   sed -n '/\[PCAP-BEGIN/,/\[PCAP-END\]/p' v2x.log | grep -v '\[PCAP' | base64 -d > capture.pcap
   ```

3. Open it in Wireshark and filter `udp.port == 47100 || udp.port == 47200 || udp.port == 47300`.

Reading it: **R1 CPMs on 47100 will not dissect as ITS.** The wire format is raw UPER with no GeoNetworking/BTP envelope, and Wireshark's ITS dissector keys on that framing — so the payload shows as opaque UDP data. That is expected. Correlate those datagrams with the node's `[EVT]` log by timestamp and byte length, and match the bytes against the golden vectors in `contracts/golden-vectors/*.uper`. R2 on 47200 and R4 on 47300 are plain JSON and read directly in the packet-bytes pane.

## Verification checklist

| Check | Pass criteria | Path |
|---|---|---|
| APK obtained | CI artifact `app-debug-apk` downloaded, or `assembleDebug` exits 0 | all |
| APK size | `app-debug.apk` &lt; 50 MB | all |
| Blueprint green | Every node in `Running` before the tunnel is started | all |
| ADB connected | `adb devices` shows `localhost:5555   device` | all |
| Install | `adb install -r …` prints `Success`, package `com.hackathon.v2x.ivi` listed | all |
| Launch | App is up on the AAOS guest without being started by hand; no `FATAL EXCEPTION` in logcat | all |
| Layout | Display Area + side buttons + status bar visible (R16) | all |
| Socket bound | Status bar reads `BOUND :47300`; `R4ListenerService: UDP socket open on port 47300` | all |
| Warning parsed | `[RX] type=warning … cSource=v2x_relayed` in logcat | all |
| Wake-on-warning | `[UI] mode=WarningView cause=warning`, and the Display Area switches by itself | all |
| Additive version | An unknown `warningType` renders generically, wire value preserved, no crash | isolated |
| Provenance guard | `object.source: "own_sensor"` trips the yellow `[? UNKNOWN SOURCE]` marker | isolated |
| Malformed survival | `[DROP] reason=malformed …`, and the next valid warning still renders | isolated |
| Zero direct C | Ghost C rendered with no direct detection of C on the ego vehicle | system |
| Evidence captured | Screen recording + logcat excerpt + extracted `.pcap` all retained | all |

