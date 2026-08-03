# Deploying an IVI Application to a Skycraft Node on CarSky

Answers one question: **how does an IVI application get onto the IVI ECU node, which is a Skycraft-type node, on the CarSky platform?**

Two sources only, and every claim is labelled with which one carries it:

| Label | Source |
|---|---|
| (documented) | [Car-Sky-Platform.html](../../requirements/development-platform-doc/Car-Sky-Platform.html), the authoritative platform reference |
| (observed) | The live Rework UI at `https://hackathon-2.carsky.io`, 2026-08-03 |
| (inferred) / (unproven) | Neither source settles it — see [§ Gaps and unproven claims](#gaps-and-unproven-claims) |

## The route in one pass

1. An **ANDROID IMAGE artifact version** carries the AAOS operating system into the Artifacts store — two files, an AOSP composite image zip and a Cuttlefish host package (documented).
2. A **Skycraft node** in the blueprint pins that artifact and version, plus OS, architecture, display geometry, GPU backend and a part prefix (documented, observed).
3. **Deploying the blueprint** makes the Nydus Operator reconcile a Kubernetes Room; the Skycraft pod downloads the artifact and boots the guest VM (documented).
4. A **Device** is the deploy target and the handle the workbench connects to; its ID is the `roomId` used by REST and MCP (documented).
5. The **application APK is not in the artifact**. It is installed onto the already-running guest over ADB — the device's `adb` widget, the one-shot ADB REST/MCP calls, or a local ADB tunnel (documented).
6. The **screen widget** renders the guest and accepts touch and keyboard, and the **log** surfaces show why when it does not (documented, observed).

Steps 5 and 6 are where an application deploy differs from every other node in the blueprint: nothing about the application passes through the blueprint at all.

## What each node type is given at deploy time

The distinction that governs everything below — the two node types in play take entirely different inputs.

| | Skycraft node | Container node |
|---|---|---|
| What it runs | A guest virtual machine — Android Automotive, AGL, or generic Linux (documented) | A user-provided OCI image (documented) |
| What it is given | An **artifact version** from the Artifacts store, resolved by `artifactId` + `versionId` (documented) | An **OCI image reference**, e.g. `registry.local/carsky/my-gateway:latest` (documented) |
| Where that lives | Artifacts panel, under one of the fixed categories (documented) | The Zot container registry at `https://registry.carsky.io/`, pushed with `docker login` / `docker tag` / `docker push` (documented) |
| Who builds it | Not Skycraft — it downloads pre-built artifacts at deploy time and never builds an OS image (documented) | The project's own build, pushed from a dev machine or CI runner (documented) |

- The registry and the Artifacts store are separate systems: the registry holds only Docker/OCI images, Artifacts holds OS images, DBC/LDF, VSS, scripts and USB images (documented).
- An APK is neither an artifact category nor an OCI image. It has no place in either store on this route.

## The Android artifact model

### Category and version shape

- Category **ANDROID IMAGE** is the one used by a Skycraft node running Android Automotive; **AGL IMAGE** is its AGL counterpart (documented).
- An Android artifact version must carry exactly **two file roles** (documented):

| Role | File | What it is |
|---|---|---|
| `image` | `aosp_*-img-*.zip` | AOSP Android emulator composite image zip — contains `system.img`, `vendor.img`, `super.img`, `boot.img` |
| `host_package` | `cvd-host_package.tar.gz` | Host tools package — provides `cvd`, the bootloader, and the scripts that assemble disks on the host |

- Observed `AAOS` artifact: versions `0.0.2` tagged `latest` (8/2/2026) and `0.0.1` (7/21/2026); each version holds an `Image` slot `aosp_trout_arm64-img-eng….` (733.0 MB / 731.9 MB) and a `Host Package` slot `aaos_trout_cvd-host_pa…` (463.6 MB). This matches the documented two-role shape exactly.
- AGL and generic Linux guests take a different pair of roles — `kernel` and `rootfs` — and boot straight from them (documented). Not the route the IVI node uses.

### Versioning and how a node pins one

- An artifact may hold many versions; the one tagged `latest` is the default a blueprint node uses (documented).
- Creating one is two steps: **New Artifact** sets a name and a category and produces an empty artifact; **Add Version** uploads the actual files (documented, and the Add Version control is observed on both inspected artifacts).
- The node's Version dropdown lists only versions that uploaded successfully — an empty dropdown means the upload never completed, not a node misconfiguration (documented).
- A node may pin an older version than `latest`: the observed `IVI - Android` node pins `AAOS` / `0.0.1` while `0.0.2` carries the `latest` tag.
- In blueprint JSON the node's `image` block is an artifact reference — `artifactId`, `versionId`, `os`, `arch` (documented).
- **Changing the version re-provisions the VM disks and restarts the pod** (documented), and an artifact change is one of the listed triggers that require a redeploy (documented).
- Artifacts default to Private with a Public toggle for the workspace, and deleting one is irreversible — detach every referencing blueprint node first (documented; the Sharing & Ownership and Danger Zone sections are observed).

### The USB category is a different thing

- **USB** artifacts are FAT32 `.img` disk images mounted into a guest as virtual USB mass storage by a **Device Proxy Node** — not by Skycraft itself (documented).
- Build and upload path: `truncate` / `mkfs.vfat` / `mcopy`, then New Artifact with category USB, Add Version, tag `latest` (documented).
- Wiring: the Device Proxy node's `usb` OUTPUT pin connects to a Skycraft node's `usb` INPUT pin, one attached device per pair at a time (documented).
- At runtime the USB Device Proxy widget lists the images, and **Plug** attaches one, reporting the guest mount path (the documented example is `/sdcard/Music/usb_1`).
- Observed `usb_images` artifact: category USB, version `0.0.1` tagged `latest`, one file slot `usb_image: USB_storage.img (64.0 MB)`.

**Is USB an APK-delivery route? The evidence does not establish it (unproven).** The documented payload examples are an OTA `update.zip`, video and mp3 files; neither source states that an APK is installed from a mounted USB image, and no source describes AAOS installing a package from that mount path. The observed project blueprint contains no Device Proxy node at all. Any file can mechanically be placed on the image, but the install step is undocumented and unobserved — treat this as a route to prove, not a route to plan against. The documented APK path is ADB ([§ Where the APK enters](#where-the-apk-enters)).

## The Skycraft node configuration surface

| Field | What it sets (documented) | Observed on `IVI - Android` |
|---|---|---|
| **OS** | `android`, `agl` or `linux`; `android` boots in the Android emulator's BIOS mode | `Android` |
| **Artifact** / **Version** | The boot image — both must already exist in the Artifacts panel | `AAOS` / `0.0.1` |
| **Architecture** | The `arch` field of the node's `image` block | `aarch64 (ARM)` |
| **Display Width / Height / DPI** | The virtual screen resolution the Screen widget renders | `1280` / `720` / `320` |
| **GPU Backend** | The virtual graphics driver given to the VM | `VirGL Renderer` |
| **Part Prefix** | Auto-naming prefix for every part of this node, unique within the room | `face`, helper text `hu` → `hu-logcat`, `hu-screen` |

Every documented `arch` example in the platform reference is `x86_64`, while the observed node is `aarch64 (ARM)` and the observed artifact file is `aosp_trout_arm64`. The documented registry description likewise shows image cards as `linux/arm64`. Target arm64 for anything built for this cluster; the documented examples are not the deployed architecture.

### Part prefix and the names widgets bind to

- The prefix generates every part name the node exposes. Documented suffixes: `{prefix}-screen`, `{prefix}-audio`, `{prefix}-adb`, `{prefix}-logcat`. Every node in a deployment — Skycraft or not — additionally has a `<node>-main` log part (documented).
- Observed on the phase5 deployment's screen widget: Video Part `ivi-screen`, Audio Part `ivi-audio`, Touch Part `ivi-touch-panel`, Keyboard Part `ivi-keyboard-panel`.
- **The prefix and the part names were observed on different nodes** — `face` on the `KIS` blueprint's `IVI - Android` node, the `ivi-*` names on the `phase5_xuanbach_test` deployment. That a Part Prefix of `ivi` is what produced them is *inferred* from the documented rule: consistent with it, but not observed on one node.
- `-touch-panel` and `-keyboard-panel` are observed but appear in no documented suffix list, so the complete set a prefix generates is unproven.
- Practical consequence: the prefix is the join between the node and every widget. Change it and every widget binding must be repointed — a black Screen widget most often means the Video Part no longer matches the node's prefix (documented).

### Pins

Skycraft supports five pin kinds: `vhal`, `kuksa`, `ethernet`, `video`, `usb` (documented). The observed node carries two.

- **`eth`** — joins an Ethernet Bridge node, a virtual L2 broadcast domain behaving as an unmanaged software switch. Default subnet `10.99.0.0/24` with the bridge on `.1`, listener port `29400`, backend `linux` or `ovs` (documented). The guest's address may be left empty for auto-assignment or declared statically on the pin; for Skycraft guests the bridge runtime runs a small DHCP server that binds the deterministic guest MAC to the declared IP, so the guest boots with the expected address without running a DHCP client (documented).
- **`vhal`** — the guest AAOS connects as a VHAL client to a Script Node acting as a VehicleServer over gRPC (documented). The pin carries child properties identified by `propId` (e.g. `0x21400400`), each flagged Custom when overridden rather than taking the AOSP default (documented). Observed badge on the node: `15 props`.
- Observed add-pin row: a pin-name field, a type dropdown showing `VHAL`, and a direction dropdown showing `Client`.

**Direction semantics.** The documented wiring model is INPUT/OUTPUT plus a target for client-only pins: server-only nodes (KUKSA Broker, CAN Bus, LIN Bus, GPIO Panel) accept only INPUT, client nodes use OUTPUT with a target, and only same-type pins may be wired. The observed inspector instead labels both pins `Client, Auto`. `Client` lines up with the documented client-role pin. **`Auto` appears nowhere in the platform reference** — for `eth` it is consistent with the documented empty-IP auto-assignment, and for `vhal` its meaning is unproven.

## Deploying the blueprint

The documented operational loop is design → validate → deploy → monitor → teardown. A blueprint alone runs nothing.

1. Design the topology on the Nydus canvas: drag nodes, add and name pins, pick pin types, wire them.
2. Configure the blueprint itself — click empty canvas, set Name / Description / Locked in the Inspector.
3. **New Deployment** — choose or create a Device and set a deployment name, defaulting to `<blueprint>-deploy`.
4. **Deploy** — the Nydus Operator reconciles the blueprint into a Kubernetes Room: a namespace with Pods, Services and ConfigMaps. Status reaches `Running (n/n)` when every node is ready.
5. Monitor in the Deployment Viewer, which offers Edit, Redeploy, Restart All and Delete Deployment.

All five steps are documented. Observed at the same time: deployment `phase5_xuanbac…` at **Running (5/5)**, "from phase5_xuanbach_t…", matching the documented default `-deploy` name; the canvas reports `5 / 30 nodes`.

- **Redeploy is required** when topology, a node's artifact/image, or node configuration changes. It is **not** required to control the screen, send signals, read logs, or change device widgets (documented).
- Every abstract placeholder node must be instantiated to a real type before deploy; the compiler rejects a blueprint that still holds one (documented). `Abstract` is the first entry in the observed node palette.
- Equivalents outside the browser: `POST /api/v1/deployments` or MCP `deploy(blueprintId, roomId, name)`, then `wait_ready` or polling `GET /api/v1/deployments/{roomId}/status` (documented).
- A blueprint cannot be deleted while any of its deployments still runs (documented).

## Where the APK enters

The artifact is the operating system. **Nothing in either source puts an application APK inside an ANDROID IMAGE artifact version** — the version's two roles are the AOSP composite image and the host package, and Skycraft's job ends at booting them. The application is installed onto the guest after it is running.

Every documented delivery mechanism is ADB:

| Mechanism | How it is reached (documented) |
|---|---|
| **`adb` widget** | Attached to a device; its Inspector binds one field, the ADB part a Skycraft node exposes (e.g. `face-adb`). The Stage shows an xterm running real shell commands on the guest, prompt `trout_arm64:/ $`. The platform reference describes this widget as the place to "run commands, **install APKs**, view logs" — its only mention of APKs anywhere. |
| **One-shot ADB command** | `POST /api/v1/vms/{roomId}/{nodeKey}/shell`, `POST /api/v1/deployments/{roomId}/adb-exec/{nodeKey}`, or MCP `adb_shell(roomId, nodeKey, command)` |
| **Persistent local tunnel** | `GET /api/v1/deployments/{roomId}/adb-tunnel` or MCP `vm_tunnel_open(roomId, nodeKey)` returns `localhost:<port>`; `adb connect localhost:<port>` then makes local `adb`, Appium and UIAutomator work as if the VM were plugged in over USB. The Skycraft pod exposes ADB on a fixed port. |

Supporting calls on the same path: `wait_boot(roomId, nodeKey)` blocks until `sys.boot_completed=1`, and `vm_tunnel_close` / `vm_tunnel_list` manage open tunnels (documented).

What separates the two sides of this boundary:

- **The platform provides** the running AAOS guest, the ADB transport in three forms, and the screen, screenshot, UI-tree and log surfaces used to check the result.
- **The project provides** the APK and the command that installs it. Getting the file from a build machine to wherever the install runs is a project procedure — see [deploy-ivi-hmi-walkthrough.md](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md), authoritative for the IVI APK's build, export, install, launch and verification.

Two consequences worth stating plainly:

- **An APK change never needs a redeploy.** The documented redeploy triggers are topology, artifact/image and node config; an install into the running guest is none of them. A system-image change does need one, and re-provisions the VM disks.
- The platform reference documents **no file-upload route into a Skycraft guest other than ADB and the Device Proxy USB image**. `POST /api/v1/deployments/{roomId}/container-file/{nodeKey}` exists but is documented for container nodes, not Skycraft.

## The device and widget layer

- A **Device** is the deployment target: it determines the namespace, resource pool and hub connectivity, and its ID is the `roomId` every node-scoped REST/MCP call takes (documented). It can be selected or created inline in the deploy dialog.
- A device created by a deployment must not be deleted by hand — stop the corresponding deployment instead (documented).
- **Connect / Switch / Disconnect** opens or moves the browser's control session. Disconnect ends that session only; it does not stop the pods, which are released by acting on the deployment (documented).
- Observed devices: `Minh_Test_IVI`, `trial2_minh_netc…`, `Clever Franklin`, `KIS`, each offering **Switch**; the connected one shows **Disconnect** and, beneath it, the deployment it is bound to — `phase5_xuanbach_test-deploy`.
- **Widgets** are workbench panels that read and write the parts and pins of running nodes; the documented catalog has 12 types. Observed on the connected device: `PWT CAN Signal` (signal), `IVI ADB` (adb), `Driver Sensor` (gpio), `IVI Screen` (screen), `BCM CAN Signal` (signal), `New Log` (log).

| Widget type | What it does (documented) | Role for an HMI |
|---|---|---|
| `adb` | Shell into the Android guest — run commands, install APKs, view logs | Install surface, and the first place to look when the app is not there |
| `screen` | Live video + audio of the guest plus touch and keyboard control | **Primary verification surface** — what the driver would see and touch |
| `log` | Live runtime log of a selected node/part | Diagnosis when the screen is wrong or blank |
| `signal` | Read-only watch of VSS/CAN signals declared in the blueprint | Not on the IVI path — belongs to the CAN/VSS side |
| `gpio` | Writes a value into a source signal to simulate a physical sensor | Stimulus, not verification |

For an HMI the verification pair is `screen` (what happened) plus `adb` and `log` (why). `signal` and `gpio` are on the vehicle-signal side and prove nothing about the application's display.

## Verification and diagnostics

### The node Logs panel

- Observed docked below the canvas: title `Logs: IVI ECU`, a stream selector currently set to **`skycraft`**, a text Filter box, download and clear controls, and a live-indicator dot.
- The documented Log widget offers the same controls — substring filter, auto-scroll to bottom, clear, and a Connected badge for a live stream.
- Documented log sources are `<node>-main` for every node, plus `<node>-logcat` on Skycraft nodes carrying the guest's logcat (e.g. `face-logcat`). **`skycraft` is not among them** — what that stream contains relative to the two documented ones is unproven.
- Outside the browser: `GET /api/v1/deployments/{roomId}/logs/{nodeKey}` for live logs, `.../search` or MCP `search_logs` for history via Loki (documented).

### The screen widget

- Observed toolbar above the rendered surface: viewer count `1`, an eye toggle, and a **Record** button, with expand and pop-out controls on the panel.
- Recording is bound to the Inspector's **Recorder Part**; the recording appears under **Videos** once stopped (documented).
- Observed Inspector values: Video Part `ivi-screen`, Audio Part `ivi-audio`, Touch Part `ivi-touch-panel`, Keyboard Part `ivi-keyboard-panel`, and Recorder Part `Client Microphone` with a `Mic` selector and a disabled "Enable microphone" toggle. The documented microphone control is a client-side microphone for two-way audio; a Recorder Part holding `Client Microphone` rather than a node part is observed, and its effect on what Record captures is unproven.
- Scriptable equivalents that need no browser: `GET /api/v1/vms/{roomId}/{nodeKey}/screenshot` (PNG), `/accessibility` (UI tree), and MCP `screenshot`, `ui_tree`, `find_text` (documented) — a usable acceptance check for "the warning is on screen".

### Reading the observed log stream

| Signal | Reading |
|---|---|
| `INFO [skycraft_native::audio_publisher] Audio: 501 frames published (48000Hz, 2ch)` | Guest→browser media pipeline alive and publishing steadily. Healthy. |
| Guest renders the AAOS home screen — status bar, clock, Weather card, HVAC steppers, nav bar (observed) | Guest booted and the display path works end to end. Healthy. |
| `No maps application installed. Please contact your car manufacturer.` in the map pane (observed) | The AOSP image ships no maps app. Not a fault. |
| `WARN [skycraft_native::mic_receiver] opus decode dropped` with `stats: ok=1519239 decode_err=4305 pub_err=0` | Browser→guest microphone uplink degraded: roughly 0.3% of decodes failing, zero publish errors. Affects client audio only — not video, touch, ADB, or the Ethernet path. |
| `[capture-source] stats: received=1519251 decoded=1519251 ring_underrun=0 ring_overrun=2916847680` | Received equals decoded and there are no underruns. Neither source documents these counters or a threshold for them; the very large overrun value is not a fault that can be asserted from the available evidence. |
| Deployment `Running` but a short ready count, e.g. 20/22 (documented) | Some nodes still starting, or in an image-pull error or crash loop. Check the Dashboard for a node with a high restart count. |
| Screen widget black (documented) | Video Part does not match the node's Part Prefix; if it does match, Disconnect then Connect the device to re-establish the WebRTC session. |

## Relation to the project's IVI ECU node

Requirement numbers are referenced, not restated — see the report's R4, R5, R6, R16 and R17.

- The IVI ECU is the R5 blueprint's Skycraft node. Observed on the `phase5_xuanbach_test` stage: a node labelled **IVI ECU** with a single `eth` pin wired to **Ethernet Bridge 1** — the R6 network, shared with the other nodes.
- R4's warning message arrives over that `eth` pin as ordinary traffic on the bridge subnet. The platform gives the guest its address either by auto-assignment or from a static address declared on the pin, bound to the guest MAC by the bridge's DHCP server (documented).
- R16 and R17 are application-layer work living inside the guest. They ship as the APK installed over ADB, so **nothing about them appears in the blueprint** — a blueprint diff will never show an HMI change.
- What an HMI deploy depends on, in order: an ANDROID IMAGE artifact version selected on the node → a Running deployment → the guest booted (`sys.boot_completed=1`) → ADB reachable on the node's `<prefix>-adb` part → the APK installed → the screen widget bound to `<prefix>-screen` and `<prefix>-audio`.
- No `vhal` or `kuksa` pin is required for that chain. The observed project node carries only `eth`; the observed `IVI - Android` reference node in the `KIS` blueprint carries `eth` plus `vhal` because it is driven by vehicle properties, which the R4 path is not.
- The build, export, install, launch and verification procedure is [deploy-ivi-hmi-walkthrough.md](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md) — follow it rather than improvising a route.

## Gaps and unproven claims

### Documented, but not visible in the observed UI

- The Skycraft page calls the Android artifact type **VM Image**; the Artifacts page calls the category **ANDROID IMAGE**; the MCP tool reference uses `VM_IMAGE` as a category value. The observed UI shows **ANDROID IMAGE**.
- Skycraft's `usb`, `video` and `kuksa` pin kinds — none present on either observed node.
- The `ethernet` pin's `address` / `prefixLen` / `vlanId` fields and the bridge's `bridgeMode` / `subnet` / `port` fields — the observed pin inspector exposes no such controls.
- The `Shell`, `USB Device Proxy`, `CAPL TestScript`, `Road Simulator`, `Simulator Box`, `Text-to-speech` and `CAN Panel` widget types from the documented 12-type catalog — none present on the observed device.
- The Android boot mode ("Bios / Android emulator") and the `image` block's JSON shape — the inspector exposes fields, not JSON.
- Every documented `arch` example is `x86_64`, against an observed `aarch64 (ARM)` node and an arm64 artifact.

### Observed, but absent from the platform reference

- The Part Prefix helper text's `hu` example, and the `-touch-panel` / `-keyboard-panel` part suffixes — the reference names only `-screen`, `-audio`, `-adb`, `-logcat` and `<node>-main`.
- The Logs panel stream selector and its `skycraft` value.
- Pin direction labels `Client` and `Auto`, in place of the documented INPUT/OUTPUT.
- The `15 props` count badge on a `vhal` pin.
- The `5 / 30 nodes` canvas capacity indicator.
- The device inspector's Mode `Editing`, Reachability `UNKNOWN` and Operational `In use` fields.
- A Recorder Part holding `Client Microphone`, with a `Mic` selector and a disabled "Enable microphone" toggle.
- The `skycraft_native::audio_publisher`, `skycraft_native::mic_receiver` and `[capture-source]` log emitters and their counters.

### Unproven until confirmed on the live platform

- That a Part Prefix of `ivi` is what produced `ivi-screen` / `ivi-audio` / `ivi-touch-panel` / `ivi-keyboard-panel` — prefix and part names were seen on different nodes.
- The complete set of part-name suffixes a prefix generates.
- Whether a USB artifact can deliver an APK, and whether AAOS can install a package from the mounted guest path.
- Whether an ADB-installed APK survives a pod restart, a Restart All, or a Redeploy.
- What the `skycraft` log stream carries relative to `<node>-main` and `<node>-logcat`.
- What `Auto` means on a `vhal` pin.
- Whether `ring_overrun=2916847680` indicates a fault or an unset/wrapped counter, and whether a ~0.3% opus decode-error rate is normal for an idle client.
- Whether the IVI node needs a static `eth` address or can rely on bridge auto-assignment for the R4 path.
- The organizers' advisory PDF beside the platform reference was not consulted — it could not be rendered in this environment, so anything it states about the IVI/Skycraft route is not reflected here.
