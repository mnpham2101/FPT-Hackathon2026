# Node: IVI ECU

- **CarSky node type:** Skycraft Node (AAOS guest)
- **Virtualization level:** Full vECU — developed fully; the VM artifact itself ships in the starter pack (report §1 node table)
- **Focus goal:** God view of the 3 vehicles, every instance displayed
- **Part of:** [carsky-4-node-blueprint.md](carsky-4-node-blueprint.md)
- **Bring-up procedure:** [ivi-hmi-walkthrough.md](ivi-hmi-walkthrough.md) — local or CI build → export → deploy → ADB install → screen → verified God View. This file owns the node's *facts* (artifact IDs, config, pin shape); the walkthrough is **authoritative** for the *doing*.

## Responsibility

- Displays GUI and applications; renders information received from the ADA ECU.
- Warning view: 2D God view of the 3 vehicles (committed), 3D optional (R17).
- Multi-process front end (separate app woken on an ADA message) is optional in M1; a single-app warning view satisfies M1 (R16).

## Tech stack

Kotlin, Jetpack Compose, AndroidX; Compose Canvas for 2D (SceneView/Filament for optional 3D) behind the R17 view seam — report §3(e). Current implementation lives in [IVI_ECU/](../../IVI_ECU/).

## Prepare the VM artifact (once per team, not per deploy)

**Already done on this deployment — reuse it, do not upload a new one.** The platform carries one `ANDROID_IMAGE` artifact named **AAOS** (confirmed live 2026-07-31) with both required file roles (`image`, `host_package`):

| Field | Value |
|---|---|
| `artifactId` | `x9oqgIwzTp1m26SWIQqJt` |
| `versionId` | `xSU_Q7YJZUxxUgDr4Ugcp` |
| `version` | `0.0.1` |
| `arch` | `aarch64` (matches the actual image; the artifact's own metadata leaves arch unset) |

Only if that artifact is ever missing: **Artifacts → New Artifact**, category **ANDROID IMAGE** → **Add Version**, uploading the AOSP composite image zip (role `image`) and `cvd-host_package.tar.gz` (role `host_package`) from the starter pack, then note the new IDs.

**A Skycraft node without this block fails deployment** with `invalid blueprint: node 'IVI ECU': skycraft requires 'image' config with VM image artifact details` — the config is not optional, and it is not visible in a REST topology dump until set.

## Blueprint node config

```json
{
  "image": {
    "artifactId": "x9oqgIwzTp1m26SWIQqJt",
    "versionId": "xSU_Q7YJZUxxUgDr4Ugcp",
    "version": "0.0.1",
    "arch": "aarch64"
  }
}
```

The Inspector's other CONFIGURATION fields — **Display Width/Height/DPI**, **GPU Backend**, **Part Prefix** — are per-node-instance values this guide does not fix. The Part Prefix names the node's parts (`<prefix>-screen`, `<prefix>-logcat`, `<prefix>-adb`) that the Devices-panel widgets select by, so read the live values off the node before using them ([ivi-hmi-walkthrough.md §4.1](ivi-hmi-walkthrough.md)).

## Pins

Skycraft Node available pin kinds: `vhal`, `kuksa`, `ethernet`, `video`, `usb` ([full table](carsky-4-node-blueprint.md#pin-kinds-by-node-type)).

**M1 wires exactly one — `ethernet`, targeting the Ethernet Bridge node with a static address** (Skycraft guests get DHCP-bound to their declared IP, so this must not be left auto-assigned). The pin serves the R4 UDP ingest service (this node is the *receiving* end of the data flow), but the CarSky pin `direction` field itself is `OUTPUT` regardless of that role — every non-bridge node's `ethernet` pin in the real platform export is `OUTPUT`, wiring into the bridge's single `INPUT` pin (verified against [blueprint-KIS.json](../development-platform-doc/blueprint-KIS.json), where even a pure-consumer node like the TCU-NAD carries an `OUTPUT` ethernet pin). Added manually in the Nydus UI canvas after import — [blueprint-m1-cooperative-awareness.json](blueprint-m1-cooperative-awareness.json) creates this node via Import from File but carries no pins (JSON import silently drops `ethernet` pins, same limitation as the REST `addPin` endpoint — see [carsky-rest-api-blueprint.md](carsky-rest-api-blueprint.md)). Verified pin shape, per the real platform export above — `id` is platform-assigned when the pin is created, and there is no `prefixLen` field; only `address` (the eth-bridge node's own `subnet` config supplies the mask):

```json
{
  "id": "<platform-assigned>",
  "name": "eth",
  "pinType": "ETHERNET",
  "direction": "OUTPUT",
  "side": null,
  "port": null,
  "properties": {
    "address": "10.99.0.13"
  }
}
```

**Why nothing else:**
- No `vhal` — nothing in this topology runs a Script Node as a VehicleServer for the guest to bind to; R16/R17 don't touch Android VHAL properties.
- No `kuksa` — "the IVI renders relative geometry only — no map and no GNSS injection on the IVI node" (report §4 decision record); KUKSA/VSS is exactly that vehicle-signal path.
- No `video` — ego video clip display in the Display area is deferred from M1 for time (report § Future developments), so no live/replay video pin is wired.
- No `usb` — no USB dependency in R16/R17.

## Post-deploy: install the team APK

The APK is not baked into the VM image — install it after the node reaches Running, via ADB:

```
adb connect <skycraft-adb-endpoint>
adb install app-debug.apk
```

Use Rework's device panel or the CarSky Gateway ADB tunnel to get `<skycraft-adb-endpoint>` (ADB is exposed on a fixed port per Skycraft pod).

**The route is unproven on this deployment.** The candidate endpoint sources, the guest properties that gate the install (`minSdk 29`, the `automotive` feature), the launch command with its port override, and the emulator fallback are enumerated in [ivi-hmi-walkthrough.md §4.3–§4.6](ivi-hmi-walkthrough.md).

## Verification (feeds R16, R17 acceptance)

- HMI runs on the AAOS node with the report's layout (central Display area + button/app areas); button/app areas switch what the Display area shows.
- An R4 warning message brings the warning view up in the Display area.
- Warning view shows ego, B, and ghost C at composed positions; **ghost C sourced only from `v2x_relayed` data**; 2D drawing delivered.
- A newer message with an unknown `warningType` degrades gracefully (R4 additive-version test).

How to produce each observation — what to feed the app, and what a correct versus incorrect result looks like — is the verification ladder in [ivi-hmi-walkthrough.md §4.7](ivi-hmi-walkthrough.md).
