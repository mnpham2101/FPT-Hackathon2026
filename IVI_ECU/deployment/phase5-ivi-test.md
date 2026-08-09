# Phase 5 — IVI HMI test & evidence

How to drive R4 warnings at the installed app and collect the evidence that closes the requirement. Building the APK, opening the ADB tunnel, installing and confirming the app is up is [phase5-ivi-deploy.md](phase5-ivi-deploy.md) — this note starts where that one ends and continues its numbering: **Steps 1–4 are there, Steps 5–6 are here.**

## Prerequisites

- A Room deployed green, with `app-debug.apk` installed and the app confirmed on screen — [phase5-ivi-deploy.md](phase5-ivi-deploy.md) Steps 1–4
- For anything that runs `adb`: the tunnel still serving on `localhost:5555` — [Step 3 Terminal 1](phase5-ivi-deploy.md#terminal-1--establish-the-tunnel-then-leave-it-alone), or `INSTALL-IVI-APK.cmd -KeepTunnel`
- A CarSky workbench login for the browser-only surfaces — the Screen widget, the Log widget, node logs

Every PowerShell block below runs from the **repo root**.

Two tools cover most of Step 6. [INSTALL-IVI-APK.cmd](../../tools/apk-uploader/INSTALL-IVI-APK.cmd) samples the evidence logcat as the tail of its install run, and `-SkipInstall` re-samples it without touching the Room. [Collect-Logs.ps1](../../tools/logs-collector/Collect-Logs.ps1) collects the whole set in one pass — every container node's log over REST, the guest-side logs over ADB, and a plain-text `summary.txt` of the checks below — resolving the node keys itself instead of asking you to paste them. Which rows stay manual, and why: [phase5-ivi-deploy.md § The tool](phase5-ivi-deploy.md#the-tool-does-most-of-steps-3-4-and-6). The steps below stay authoritative — read them when a tool fails, and to run any row by hand.

## Step 5 — Choose a test path

The two paths differ only in **what produces the R4 warning stream**. The app, the install and the evidence are identical across both, so they are listed in the order you would run them — the second adds real components upstream of the app.

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
3. Wait for all three nodes green, then run [Steps 2–4](phase5-ivi-deploy.md#step-2--deploy-the-blueprint-and-copy-the-tunnel-command-browser-human) to install the APK and confirm it is up.
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
4. Run [Steps 2–4](phase5-ivi-deploy.md#step-2--deploy-the-blueprint-and-copy-the-tunnel-command-browser-human) to install the APK and confirm it is up.
5. Restart the Bench node to replay the scenario from its first step.

Pass is the whole chain: the bench emits CPMs, the V2X ECU decodes and relays, the ADA ECU gates and emits a warning, and the app draws ghost C — with **zero direct detections of C on the ego vehicle**, every rendered frame sourced from `v2x_relayed`.

## Step 6 — Evidence collection

Both paths converge here — the evidence is the same regardless of what produced the stream, and no single surface is sufficient on its own. The screen does not prove where the data came from, the log does not prove anything rendered, and neither proves what actually crossed the wire.

### 1 · Warning screen

[Step 4](phase5-ivi-deploy.md#step-4--confirm-the-app-on-screen-browser-human) already put the Warning View in front of you on the **IVI Screen** widget. This section is about capturing it rather than finding it.

- **Screen recording** — set **Recorder Part** on the Screen widget *before* the run starts; the clip lands under **Videos** and downloads as `.mp4`. Recording is at native resolution, so files are large.
- **Screenshots** — from the same Screen widget.
- What must be visible: `EGO` and `B` drawn solid, **C dashed** with a pulsing risk glow and the badge `[V2X] C · <d> m · RISK: HIGH`, and the connector labels `d_AB` and `d_AC`. A yellow `[? UNKNOWN SOURCE]` marker where ghost C belongs means the provenance guard tripped — on the approach scenario that is a blocking defect, not a display quirk.

### 2 · Logs

Two surfaces, both required — one for the producer, one for the app.

| Surface | How to read it | What it carries |
|---|---|---|
| **Node log** (the sending container) | Deployment Viewer → the node → **View Log** | `[TX]` lines from the producer, and `[CAP]` tcpdump lines on nodes that have `NET_RAW` |
| **Guest logcat** (the app) | The **Log** widget, or `adb logcat` over the tunnel | `[RX]` and `[DROP]` on `IVI_V2X`; the bind line on `R4ListenerService` |

Two ways to get them. Path 1 needs only a browser; Path 2 needs the tunnel and collects more.

#### Path 1 — copy and paste nodes logs directly from the CarSky platform

| Surface | Where |
|---|---|
| Producer node | **Nydus** → the deployment → the node → **View Log** → the `user` stream |
| Guest logcat | **Devices** → the device → the **Log** widget, bound to the IVI node's part |

Select all → copy → paste into a text file under `tools\apk-uploader\test-report\<isolated|system>\`, one file per node. Name them however you like, then read them against § What the logs must show.

- Pick the **`user`** stream. `sidecar` carries the platform's own connection chatter, not the application's output.
- A Log widget bound to a part that no longer exists in the Room shows nothing — re-point it at this deployment's part.
- On the isolated path the producer is the ADA node; on the system path repeat for the bench, V2X and ADA nodes.

#### Path 2 — run commands from PowerShell to collect nodes logs

**First, put `adb` on `PATH`.** `adb version` answering `'adb' is not recognized` means it is missing. Add it once, in the User scope, then **open a new PowerShell window** — an already-open one keeps the old `PATH`:

```powershell
$p = [Environment]::GetEnvironmentVariable("PATH","User")
[Environment]::SetEnvironmentVariable("PATH", "$p;$env:LOCALAPPDATA\Android\Sdk\platform-tools", "User")
```

**Keep `-s localhost:5555` on every command.** A local emulator is a second device, and with two attached `adb` acts on the wrong one or refuses outright.

**Make the run's folder.** From the repo root, with the tunnel serving. `isolated` or `system` are the two paths of Step 5 — change that one word in every path below for a system-test run:

```powershell
mkdir tools\apk-uploader\test-report\isolated
```

`test-report/` is git-ignored in full: a run's logs, dumps and pcaps are evidence kept with the delivery report, not committed to the repository.

In each command below, the **last part sets the output file** — `| Out-File <path> -Encoding utf8`. Change that path to name the file whatever you like; the rest of the command decides what goes into it. Each is one line, safe to paste on its own.

**To get the IVI-ECU app's log** — the main evidence, and the only place `[RX]` appears:

```powershell
adb -s localhost:5555 logcat -d -v threadtime -s IVI_V2X R4ListenerService R4Deserializer | Out-File tools\apk-uploader\test-report\isolated\app-logcat.txt -Encoding utf8
```

`logcat -d` prints the buffer once and exits instead of streaming — the app started before you attached, so a live stream would show nothing that already happened. `-s IVI_V2X R4ListenerService R4Deserializer` keeps those three tags and silences everything else.

**To check the app never crashed:**

```powershell
adb -s localhost:5555 logcat -d -b crash | Out-File tools\apk-uploader\test-report\isolated\app-crash.txt -Encoding utf8
```

`-b crash` reads Android's separate crash buffer rather than the main one. The pass is a file with nothing in it naming the app.

**To find out whether datagrams reached the guest at all** — this is what separates "nothing arrived" from "arrived and was mishandled":

```powershell
adb -s localhost:5555 shell cat /proc/net/udp6 | Out-File tools\apk-uploader\test-report\isolated\guest-udp6.txt -Encoding utf8
adb -s localhost:5555 shell ip -4 addr show | Out-File tools\apk-uploader\test-report\isolated\guest-ifaces.txt -Encoding utf8
```

**To get a container node's log — ADA-ECU, V2X-ECU, or the Scenario Player bench.** These run as containers rather than on the guest, so `adb` cannot see them; their logs come over REST. `Invoke-RestMethod` is PowerShell's built-in HTTP client, so no separate API tool is needed.

**Not for IVI-ECU.** That node is a Skycraft VM, and this endpoint returns the VM host's own output — WebRTC encoder and GPU lines — with nothing the app wrote. The IVI app's log comes only from `adb logcat` above, or the Log widget in Path 1.

*Step 1 — list the nodes.* Paste this block; PowerShell runs the four lines in sequence and prints one row per node:

```powershell
$key = "PASTE-YOUR-CARSKY-API-KEY"
$base = "https://hackathon-2.carsky.io/api/v1"
$room = (Invoke-RestMethod -Headers @{"X-API-Key"=$key} -Uri "$base/deployments/find?blueprint=phase5").roomId
Invoke-RestMethod -Headers @{"X-API-Key"=$key} -Uri "$base/deployments/$room/nodes" | Select-Object name, displayName, nodeType, phase
```

*Step 2 — fetch one node's log.* In the table above, the **`name`** column is the node key and `displayName` tells you which node it is. Take the key of the node you want, replace `PASTE-NODE-NAME` with it, rename the output file to match, then paste:

```powershell
(Invoke-RestMethod -Headers @{"X-API-Key"=$key} -Uri "$base/deployments/$room/logs/PASTE-NODE-NAME`?container=user&tail=5000").lines | Out-File tools\apk-uploader\test-report\isolated\node-ada.txt -Encoding utf8
```

Repeat Step 2 per node. `$key`, `$base` and `$room` stay defined for the rest of the window, so Step 1 is run once.

**Check what you collected**, before tearing the Room down:

```powershell
cd tools\apk-uploader\test-report\isolated
Select-String app-logcat.txt -Pattern "UDP socket open on port 47300" | Measure-Object | % Count
Select-String app-logcat.txt -Pattern "\[RX\]"                        | Measure-Object | % Count
Select-String app-logcat.txt -Pattern "source=v2x_relayed"            | Measure-Object | % Count
Select-String app-logcat.txt -Pattern "riskState=high"                | Measure-Object | % Count
Select-String node-ada.txt   -Pattern "\[TX\]"                        | Measure-Object | % Count
Select-String app-crash.txt  -Pattern "com.hackathon.v2x.ivi"         | Measure-Object | % Count
cd ..\..\..\..
```

Scope the crash count **to the package**: a bare `FATAL` search matches the stock AAOS Bluetooth stack aborting at boot (`droid.bluetooth`, `Fatal signal 6`), which is guest noise.

Two smaller points. `Out-File … -Encoding utf8` rather than `>`, because bare redirection in Windows PowerShell 5.1 writes UTF-16. And when a filtered line needs surrounding context, the unfiltered buffer is the same command without `-s …` — tens of megabytes, so collect it only when something needs explaining:

```powershell
adb -s localhost:5555 logcat -d -v threadtime | Out-File tools\apk-uploader\test-report\isolated\app-logcat-full.txt -Encoding utf8
```

#### What the logs must show

| File | Pass |
|---|---|
| `app-logcat.txt` | `R4ListenerService: UDP socket open on port 47300`, then `[RX] R4 message received: R4WarningEvent(…)` with `source=v2x_relayed` on every warning, and `riskState` reaching `high` |
| `app-crash.txt` | No `FATAL EXCEPTION` naming `com.hackathon.v2x.ivi` |
| `guest-udp6.txt` | A listener on `B8C4` (47300 in hex) owned by the app's uid |
| `guest-ifaces.txt` | `eth0` carrying `10.99.0.13/24` — if it is missing, see [phase5-ivi-deploy.md § Troubleshooting](phase5-ivi-deploy.md#troubleshooting) |
| `node-ada.txt` | `[TX] … -> 10.99.0.13:47300` at the configured cadence |

`[RX]` is the line that matters: its fields are read off the **parsed** message, so it proves the JSON decoded into the typed model, and `source=v2x_relayed` on every warning is the definition of done in text.

Three things that look like defects and are not: an empty `IVI_V2X` filter before the first datagram is normal; the bind is logged on `R4ListenerService` rather than as the designed `[LINK] state=bound` line on `IVI_V2X`; and the socket appears in `/proc/net/udp6`, not `/proc/net/udp`, because it is bound dual-stack.

**No log line proves the UI switched.** This build has no logging in its UI layer, so the Warning View is evidenced only by § 1 above.

**There is no in-app injector.** No `DEV_INJECT` receiver exists in the app or its manifest, so the UI cannot be driven without a real datagram. The only R4 messages the repository builds without a network are the JVM fixtures under `IVI_ECU/app/src/test/resources/contracts/samples/`, which never reach a running guest.

### 3 · Wireshark

There is **no platform pcap facility** — capture runs inside the container, and the log is the node's only egress. A node needs `"capabilities": ["NET_RAW"]` flat in its `config`; without it the capture degrades to `/proc/net/dev` packet counters. The node image runs two tcpdump processes: one printing `[CAP]` lines for the live "traffic is flowing" check, and one writing a rotated pcap emitted to stdout as base64 between `[PCAP-BEGIN <name>]` and `[PCAP-END]` markers, which keeps the export byte-perfect.

**The pcap arrives inside a node log you have already collected.** No separate download exists: the ADA-ECU and V2X-ECU logs gathered in § 2 Logs — either path — are the input here. Collect them there first, then extract.

This is **system-test evidence**. The isolated path's `m1-r4-sim` stand-in does not run a capture, so its log carries no `[CAP]` lines and no blocks to extract.

**Step 1 — extract the captures.** One `.pcap` per block, written beside the input log:

```powershell
.\tools\pcap-extract\Extract-Pcap.ps1 tools\apk-uploader\test-report\system\node-v2x.txt
```

`-OutDir <dir>` writes elsewhere. Exit status: `0` every block extracted, `1` no block in the log at all — which usually means the node is missing `NET_RAW` — `2` a usage error, `3` a block failed and the reason was printed. Truncated blocks are reported rather than written, so a half file never masquerades as a complete capture. [extract_pcap.sh](../../V2X_ECU/tools/extract_pcap.sh) is the same tool for Git Bash.

**Step 2 — open it in Wireshark** and filter `udp.port == 47100 || udp.port == 47200 || udp.port == 47300`.

Reading it: **R1 CPMs on 47100 will not dissect as ITS.** Wireshark's ITS dissector covers ETSI's **Intelligent Transport Systems** message family — CAM, DENM and the CPM this project sends — and identifies a message by the GeoNetworking/BTP framing that normally wraps it. Our wire format is raw UPER with no such envelope, so the dissector has nothing to key on and the payload shows as opaque UDP data. That is expected. Correlate those datagrams with the node's `[EVT]` log by timestamp and byte length, and match the bytes against the golden vectors in `contracts/golden-vectors/*.uper`. R2 on 47200 and R4 on 47300 are plain JSON and read directly in the packet-bytes pane.

## Verification checklist

The install-side rows are in [phase5-ivi-deploy.md § Verification checklist](phase5-ivi-deploy.md#verification-checklist); a failure there invalidates everything below, so run that one first.

| Check | Pass criteria | Path |
|---|---|---|
| Socket bound | Status bar reads `BOUND :47300`; `R4ListenerService: UDP socket open on port 47300` | all |
| Warning parsed | `[RX] type=warning … cSource=v2x_relayed` in logcat | all |
| Wake-on-warning | The Display Area switches to the Warning View by itself | all |
| Additive version | An unknown `warningType` renders generically, wire value preserved, no crash | isolated |
| Provenance guard | `object.source: "own_sensor"` trips the yellow `[? UNKNOWN SOURCE]` marker | isolated |
| Malformed survival | `[DROP] reason=malformed …`, and the next valid warning still renders | isolated |
| Zero direct C | Ghost C rendered with no direct detection of C on the ego vehicle | system |
| Evidence captured | Screen recording + logcat excerpt + extracted `.pcap` all retained | all |
