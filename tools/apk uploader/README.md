# Upload the IVI APK over the reach-backend ADB tunnel

How to install (sideload) the team APK into the **IVI-ECU Skycraft node** of a deployed blueprint — e.g. `m1-system-test` — from this Windows machine, using the organizers' `reach-backend` tunnel CLI kept in this folder.

**Authority:** [deploy-ivi-hmi-walkthrough.md §4.4–§4.8](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#44-get-an-adb-endpoint) is the authoritative procedure. This guide pins the machine-specific values that document deliberately leaves as placeholders: the local tool paths, the secrets location, and the exact PowerShell forms. On any conflict, the walkthrough wins.

## What "upload" means here

The IVI node takes **no image push**. Its VM image is the platform's stock AAOS artifact; the team deliverable is an APK installed over ADB into the **running** guest, after the Skycraft node reaches `Running` ([§4.1](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#41-how-the-apk-reaches-the-ivi-ecu-node)).

## Prerequisites

| Item | Where it is | Notes |
|---|---|---|
| Tunnel CLI | `tools\apk uploader\reach_be\reach\reach-backend.exe` | Windows build; `reach-backend` (no `.exe`) beside it is the macOS/Linux build |
| APK | `tools\apk uploader\app-debug.apk` | Package `com.hackathon.v2x.ivi` |
| `adb` | `%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe` | From the Android SDK already on this machine |
| Gateway URL | `https://hackathon-2.carsky.io` | The workbench base URL doubles as the A8 gateway — confirmed by the **Local ADB** dialog (step 1); re-read it there if the organizers rotate hosts |
| `a8k_` tunnel token | `secrets\reach-adb-token-ivi.txt` | A **per-device derived token**, shown in the Local ADB dialog (step 1) — it is *not* the CarSky API key. The folder is git-ignored; never print, log, or commit its contents |
| A running Room | Deployment of the target blueprint with the Skycraft node `Running` | The tunnel has nothing to dial otherwise; the Skycraft node is the slowest to come up |

## Step 1 — Get the gateway URL and token (browser, human)

The values live in the ADB panel's **Connect from Terminal** dialog. Exact path, click by click:

1. Log in to the CarSky workbench at `https://hackathon-2.carsky.io` (Keycloak SSO).
2. Confirm the target deployment's **Skycraft node is `Running`** (**Nydus** on the DockBar → the deployment).
3. **Devices** on the DockBar → the deployment's device in the list (its row carries the deployment name, e.g. `KIS` / `m1_system_test-deploy`) → **Connect**. The button reads **Switch** if another device session is open; after connecting, the dot beside the device name must be green.
4. If the device's **Widgets** list has no ADB widget yet: `+` beside **Widgets** → **ADB**; in the **Inspector** (right column), under `PROPERTIES → ADB`, pick the part the Skycraft node exposes (e.g. `ivi-adb`).
5. In the panel below the Stage, click the ADB widget's tab (e.g. **IVI ADB**, beside `Logs: …`). The **ADB SHELL** panel opens with a green `connected` badge.
6. Top-right of the ADB SHELL panel → **Local ADB**. The **Connect from Terminal** dialog opens, showing two copyable commands:
   - `reach-backend adb --gateway https://hackathon-2.carsky.io --key a8k_…` — the gateway URL and the **per-device `a8k_` token**
   - `adb connect localhost:5555`
7. Copy the token (only the `a8k_…` value) into `secrets\reach-adb-token-ivi.txt`, one line, no quotes.

Two facts the dialog settles: the **gateway is the workbench base URL itself** (no separate `sslip.io` host on this deployment), and the token is **derived per device** — it is not the CarSky API key, and a redeploy may mint a new one, so re-open the dialog after redeploying. If the gateway ever presents a self-signed certificate, add `--insecure` to the tunnel command (the dialog says so too).

## Step 2 — Start the tunnel (PowerShell, leave it running)

From the repo root. The key is loaded from the file into a variable so it never appears on screen or in shell history:

```powershell
$gateway = "https://hackathon-2.carsky.io"
$key = (Get-Content "secrets\reach-adb-token-ivi.txt" -Raw).Trim()
& ".\tools\apk uploader\reach_be\reach\reach-backend.exe" adb --gateway $gateway --key $key --port 5555
```

This opens a local TCP server on `localhost:5555`. **Leave this terminal open** — closing it drops the tunnel. If the tunnel exits immediately: the token is stale (redeploy mints a new one — re-open the Local ADB dialog, step 1.6), the gateway URL is wrong, or port 5555 is taken (pass a different `--port` and connect to that port in step 3).

## Step 3 — Connect and check the guest (new terminal)

```powershell
$adb = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe"
& $adb connect localhost:5555
& $adb devices        # expect: localhost:5555   device
```

`offline` or an empty list means the tunnel is not serving — see [walkthrough §4.10](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#410-troubleshooting) rather than retrying blind.

Two guest properties decide whether the install can succeed at all:

```powershell
& $adb shell getprop ro.build.version.sdk          # must be ≥ 29 (the APK's minSdk)
& $adb shell pm list features | Select-String automotive   # android.hardware.type.automotive must be present
```

## Step 4 — Install

```powershell
& $adb install -r ".\tools\apk uploader\app-debug.apk"
& $adb shell pm list packages | Select-String hackathon
```

Expected: `Success`, then `package:com.hackathon.v2x.ivi`. `-r` reinstalls over an existing copy and keeps its data — use it on every reinstall.

## Step 5 — Launch and verify

```powershell
& $adb shell am start -n com.hackathon.v2x.ivi/.MainActivity
& $adb logcat -s IVI_V2X
```

The pass evidence is `[LINK] state=bound port=47300` on the `IVI_V2X` tag, and the HMI visible in the Devices panel's **Screen** widget (Connect → `+` → Screen). The full verification ladder — V1 socket bound through V5 end-to-end warning — and the screen-widget procedure are [walkthrough §4.7–§4.8](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#47-open-the-screen-and-launch-the-app); this guide stops at the install being provably alive.

## Troubleshooting

The walkthrough's [§4.10 table](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#410-troubleshooting) covers the failure modes. The two seen most on this route: the tunnel terminal was closed (restart step 2), and the Skycraft node was not yet `Running` when the tunnel dialed (wait, then restart step 2).

## Secrets hygiene

`secrets/` is git-ignored ([.gitignore](../../.gitignore)). Keys are loaded into variables from the files, passed as arguments, and never echoed, logged, committed, or pasted into documents — including this one.
