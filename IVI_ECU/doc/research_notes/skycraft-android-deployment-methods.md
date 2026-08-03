# Getting an Android app onto a CarSky Skycraft node

Research note for the IVI track: what AAOS is, every route by which our APK can reach the Skycraft guest, and whether the APK can instead be shipped as a `.img` the way the platform's own example ships one.

- **Scope.** Delivery mechanics only. What the app does once it runs is [phase5-ivi-hld.md](../phase5-ivi-hld.md); the procedure a human follows is [deploy-ivi-hmi-walkthrough.md](../../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md); the node's facts are [node-ivi-ecu.md](../../../requirements/car-sky-guide/node-ivi-ecu.md).
- **Defines no requirement numbers.** Findings map to the existing R4, R5, R16 and R17 and to the walkthrough sections that already carry the procedure.
- **Upstream evidence** is cached at `.claude/references/aaos-aosp-image-facts.md` with its source links. That folder is git-ignored by project decision, so the cache is a local working copy, not a repo artifact — a fresh clone will not have it, and every claim below that depends on it is stated here in full rather than by reference.
- **Diagram.** [skycraft-apk-delivery-routes.puml](skycraft-apk-delivery-routes.puml) — the same routes as § 2, drawn.

## 1. What AAOS is

- **Android Automotive OS is a full operating system**, the same AOSP codebase as phone Android, running directly on the head-unit hardware and booting with the car. It is not a fork and not an app.
- **Android Auto is the opposite model** — an app on the driver's phone that projects a Google-branded UI onto a head unit. The car runs no Android at all. The two are unrelated as targets: nothing about our app touches Android Auto.
- **What AAOS adds over stock Android:** the Vehicle HAL and `CarService`/`android.car` for reading, writing and subscribing to vehicle properties; the system feature `android.hardware.type.automotive`; a driver-distraction UX-restriction system; and a car launcher and system bars in place of the phone's.
- **What AAOS takes away:** no guaranteed back button, no TalkBack, fixed orientation, no app widgets or picture-in-picture on many devices, and Video/Games/Browsers restricted to the parked state. A plain AOSP automotive build also has **no Google services and no Play Store** — the BTC advisory states this for the CarSky image specifically, which is why the map discussion there lands on MapLibre/osmdroid rather than Google Maps.
- **Skycraft runs AAOS as a guest VM.** The ADB widget's documented prompt `trout_arm64:/ $` names the guest: AOSP **trout**, the automotive reference platform that runs AAOS as a VirtIO guest, built on Cuttlefish. Trout 1.1 is based on Android 13 QPR1 — API 33.
- **What that implies for packaging:** nothing special. Our deliverable stays an ordinary APK built by Gradle, installed by `PackageManager` like any Android app. The guest being a VM changes only how a machine reaches it (over a network-forwarded ADB port rather than USB), not what is installed.
- **Two guest properties still gate the install**, and trout 1.1 predicts both pass: API 33 clears our `minSdk 29`, and an automotive target declares `android.hardware.type.automotive`. Read them off the deployment anyway — [walkthrough §4.5](../../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#45-connect-and-check-the-guest) is where they are checked.

## 2. Every route an app can reach the node by

**Status** is evidence, not preference: *documented* = the platform guide or its REST reference states it; *inferred* = every component is documented but the composition is not; *unavailable* = tried and failed, or the prerequisite does not exist here. Rows 1–5 are the same routes [walkthrough §4.4](../../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#44-get-an-adb-endpoint) already ranks; rows 6–10 are new to this note.

| # | Route | Requires | Status | Who drives it | Practical cost |
|---|---|---|---|---|---|
| 1 | **ADB tunnel** — `GET /api/v1/deployments/{roomId}/adb-tunnel`, then `adb connect` and `adb install -r` | Local platform-tools; a Running Room | **Documented**, unproven on `hackathon-2` | AI, once the endpoint is in hand | Seconds per reinstall. The only route that transfers a file by itself |
| 2 | **ADB widget** — Devices panel → **ADB**, part `<prefix>-adb` | Browser session; the node's Part Prefix | **Documented** — the widget catalog says it runs commands, installs APKs and views logs | Human (browser xterm) | Seconds per command, but it exposes **no file transfer**, so the APK must already be reachable from the guest |
| 3 | **One-shot `adb-exec`** — `POST /api/v1/deployments/{roomId}/adb-exec/{nodeKey}` | API key; `nodeKey` | **Documented**, unproven here | AI | Same file-transfer limitation as row 2 |
| 4 | **VM shell** — `POST /api/v1/vms/{roomId}/{nodeKey}/shell` | API key | **Unavailable** — 502 on this deployment, though the platform guide lists it under "ADB routes (always available)" | — | — |
| 5 | **MCP tunnel** — `vm_tunnel_open` | CarSky's own `mcp/` server package | **Unavailable** — the package is not in this repo | — | — |
| 6 | **USB disk image** — a FAT32 `.img` holding the APK, registered as a **USB** artifact, mounted into the guest by a Device Proxy node, then `pm install` from the mount | A Device Proxy node, a `usb` OUTPUT pin wired to the Skycraft `usb` INPUT pin (canvas only — the REST pin enum has no `USB`, same as `ETHERNET`), `mkfs.vfat`/`mcopy` on Linux or WSL2 | **Inferred** — every piece is documented (image build, USB artifact category, Device Proxy node, the widget's **Plug** action and its guest mount path), the install from the mount is not | Human for the canvas work and the **Plug** click; AI for `pm install` | ~1–2 h to set up once, then a new artifact version upload per APK rebuild |
| 7 | **HTTP pull over the bridge** — a container node on `10.99.0.0/24` serves the APK; the guest fetches it to `/data/local/tmp` and `pm install`s it | A download tool on the guest (`wget`/`curl`), plus row 2 or 3 for the shell | **Inferred**, unproven — Android's shell tools come from toybox and `wget`'s presence in this build is not established | AI end to end, once proven | ~30 min to stand up; the cheapest iteration afterwards, and the only fully AI-drivable route |
| 8 | **Base64 through a shell route** — chunk the APK into shell `echo`/`base64` calls | Row 2 or 3 | **Inferred**, impractical — ~25 MB of APK is ~34 MB of base64 across hundreds of calls | AI | Hours, fragile, no reason to prefer it over 6 or 7 |
| 9 | **Bake into the VM image** — a custom AOSP/trout build carrying the APK, registered as the node's `ANDROID_IMAGE` artifact | A full AOSP source tree and build host | Platform side **documented**; see § 3 for the verdict | Human + AI, days | Not viable on this milestone — § 3 |
| 10 | **Remount `/system` on the running guest** — `adb root; adb disable-verity; adb remount; adb push` | A `userdebug` guest and an ADB route | **Inferred**, and strictly worse than row 1 — it still needs ADB file transfer, then adds a reboot and a one-way dm-verity change | — | Never the right answer here |
| 11 | **Push the APK to Zot** | — | **Impossible by design** — § 3 | — | — |

Two clarifications the table would otherwise blur:

- **Rows 2, 3 and 7 do not install anything by themselves.** They run commands inside the guest. What makes row 7 interesting is that it is the only inferred route that solves the file-transfer gap *without* a browser step, which is what [walkthrough §6.1 point 9](../../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) is currently open on.
- **`screenshot`, `accessibility`, `tap`, `key` and `text` are evidence and control routes, not delivery routes.** The first two answer 502 here; none of them ever carries an APK.

## 3. Can the APK be shipped as a `.img`?

### What the Skycraft `image` block actually accepts

- An **artifact reference**, not a file: `{artifactId, versionId, version, arch}`, pointing at a version of an artifact in the CarSky artifact store. The block is mandatory — the deploy is rejected outright without it ([node-ivi-ecu.md § Blueprint node config](../../../requirements/car-sky-guide/node-ivi-ecu.md#blueprint-node-config)).
- For an Android guest that artifact version must carry **exactly two file roles**: `image` = an `aosp_*-img-*.zip` composite (`system.img`, `vendor.img`, `super.img`, `boot.img`, …) and `host_package` = `cvd-host_package.tar.gz`, the Cuttlefish host tooling. The pair must come from the same build.
- Uploading a custom one **is supported**: Artifacts → New Artifact, category **ANDROID IMAGE** → Add Version, with the two files tagged by role. The platform imposes no restriction on whose build it is.

So the platform is not the obstacle. The obstacle is producing the zip.

### Baking the APK into a custom AAOS image

- The sanctioned upstream route exists: a Soong `android_app_import` module for the prebuilt APK plus its name in `PRODUCT_PACKAGES`, landing it in `/system/app` (or `/system/priv-app` with the extra allowlist and platform-signing obligations). A `targetSdkVersion >= 30` prebuilt must also set `preprocessed: true`.
- It requires a **full AOSP source tree and an image rebuild**, every time the APK changes.

### What it would cost us

| Item | Figure |
|---|---|
| Disk for the source tree and build | 400 GB (250 GB checkout + 150 GB build) |
| RAM | 64 GB, on 64-bit x86 Linux — macOS unsupported, and our build host is an ARM64 Windows laptop |
| `repo sync` | Hours, bandwidth-bound |
| First full build | ~40 min on a 72-core machine; **~6 hours on a 6-core machine** |
| Per-APK iteration | Incremental build + repackage + multi-GB artifact upload + node reconfigure + **full redeploy** (changing the artifact version re-provisions the VM disks and restarts the pod) |
| Compare: the current route | `adb install -r app-debug.apk`, seconds, Room stays up |

### Verdict — not viable for Milestone 1

Against the ranked criteria in [solution-selection-criteria.md](../../../.claude/rules/solution-selection-criteria.md):

- **Criterion 1, probability of working end to end — fails.** Five days remain to the 2026-08-08 deadline and no AOSP build host exists in this project. A first-time trout build on unfamiliar hardware, followed by a multi-gigabyte upload and a redeploy, is not a route that lands.
- **Criterion 2, fastest path to this milestone's acceptance — fails by the widest possible margin.** R16/R17 acceptance is iterative: build, install, look at the screen, fix, repeat. Baking replaces a seconds-long reinstall with an hours-long rebuild-and-redeploy, and the APK is still changing.
- **Criterion 3, future features — does not rescue it, and is not foreclosed.** Nothing in M1 prevents a later milestone from baking the app in once the app is stable; that is exactly when it becomes reasonable.
- **Criterion 4** does not apply — this is not a library choice.

**An APK is not a container image, and cannot be pushed to Zot for a Skycraft node.** Three artifact kinds with three different consumers, and none of them substitutes for another:

| Artifact | Store | Consumed by |
|---|---|---|
| OCI/Docker image | Zot registry | **Container** nodes, pulled at deploy |
| VM image zip + host package | CarSky artifact store, category ANDROID IMAGE | **Skycraft** nodes, at boot |
| APK | Neither | Android's `PackageManager`, inside an already-running guest |

An APK is a ZIP-derived application package with no kernel, no filesystem and no partition table; a `.img` is a filesystem or partition image a bootloader or VMM consumes as block storage. Renaming one into the other produces a file nothing can boot. A Skycraft node has no registry field to point at an image, and `docker push`ing an APK is not a route at any level.

### The `.img` answer that *is* real

The platform's own `.img` example is not a bootable image — it is **§ Build a USB Disk Image**, a FAT32 data disk mounted as virtual USB mass storage through a Device Proxy node, with the widget reporting a guest mount path such as `/sdcard/Music/usb_1`. That disk can carry `app-debug.apk`, which makes row 6 of § 2 a genuine answer to "ship the APK as a `.img`" — as *cargo* on a data disk, never as the VM's own image. It is worth building only if ADB file transfer proves unreachable, because it costs a canvas change, a WSL2 image build and an artifact upload per APK.

## 4. What this changes for the current route

**Nothing about the walkthrough's ordering or its primary route.** Build → deploy → ADB → `adb install -r` remains correct, and row 1 stays the first thing to prove. What the note adds is a second and third fallback behind it, and one correction.

Findings for the walkthrough owner to fold in as a separate decision — **the walkthrough was not edited in this run**:

| # | Finding | Where it lands |
|---|---|---|
| 1 | **The guest is AOSP trout 1.1 — Android 13, API 33, an automotive target.** This predicts both §4.5 gates pass. It downgrades §6.1 point 3 from an unknown to a check | §4.5, §6.1 point 3 |
| 2 | **§4.4's "two routes that are not available" is right, and the platform guide is wrong.** The guide lists `/vms/.../shell` under "ADB routes (always available)"; it answers 502 here. Worth saying so explicitly, so nobody re-tries it on the strength of the platform doc | §4.4 |
| 3 | **A USB-image fallback exists for the file-transfer gap** — row 6 of § 2. It is the documented answer to §6.1 point 9, which currently ends at "the platform says it can, but it exposes no file transfer" | §4.4, §6.1 point 9 |
| 4 | **An HTTP-pull fallback exists and is fully AI-drivable** — row 7. Its one unknown, whether the guest has `wget` or `curl`, is a single cheap probe (`adb shell which wget curl toybox`) that belongs beside the other §4.4 probes | §4.4 |
| 5 | **The REST pin enum has no `USB`, exactly as it has no `ETHERNET`.** Any route using a Device Proxy node needs the pin drawn on the Nydus canvas by hand — the same limitation already recorded in [carsky-rest-api-blueprint.md](../../../requirements/car-sky-guide/carsky-rest-api-blueprint.md#key-finding-what-rest-can-and-cannot-do) | §1.4, §4.2 |
| 6 | **Baking the APK into the VM image is closed, not open.** Recording the verdict stops it being re-proposed when the ADB route looks shaky | §4.1 |

Nothing here touches a frozen contract, the plan's scope, or a published requirement. The one decision owner is the user: whether to spend the setup cost on row 6 or row 7 before row 1 is proven, or to keep them as contingencies.
