# Phase 5 — IVI HMI AAOS build & deployment smoke test

How to build the team APK, install it on the CarSky Skycraft (AAOS) node, and smoke-test R4 ingest with the mock sender. Node-level blueprint/VM config lives in [node-ivi-ecu.md](../../requirements/car-sky-guide/node-ivi-ecu.md); this note is the APK path only.

## Prerequisites

- JDK 17+ (Android Studio JBR is fine)
- Android SDK Platform-Tools (`adb` on `PATH`)
- Gradle wrapper in this tree (`IVI_ECU/gradlew` / `gradlew.bat`) — no global Gradle install required
- A CarSky Room with the IVI Skycraft node in **Running** state (blueprint + AAOS artifact per [node-ivi-ecu.md](../../requirements/car-sky-guide/node-ivi-ecu.md))

Working directory for all Gradle commands below: **`IVI_ECU/`**.

## Step 1 — Build the debug APK

```bash
# Linux / macOS
./gradlew assembleDebug

# Windows
gradlew.bat assembleDebug
```

Output APK:

```text
app/build/outputs/apk/debug/app-debug.apk
```

Acceptance: build succeeds; APK size under 50 MB (`dir` / `ls -lh` on the path above).

## Step 2 — Connect ADB to the CarSky Skycraft guest

1. Open the Room in the CarSky / Rework UI and select the IVI Skycraft node (Device ID example used by the team: `zvpi8tzdj8s08wmxaaye2` — replace with the live Room's device id if different).
2. Enable the **ADB port-forward** (or Gateway ADB tunnel) for that node and note the local port the UI assigns (Starter Pack / device panel — often `localhost:<port>`).
3. Connect and confirm the guest is online:

```bash
adb connect 127.0.0.1:<port>
# or: adb connect localhost:<port>

adb devices
```

Expected: a row for `127.0.0.1:<port>` (or `localhost:<port>`) in state `device`. If the state is `offline` / `unauthorized`, re-open the tunnel and reconnect.

## Step 3 — Install the APK

From `IVI_ECU/`:

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

`-r` replaces an existing install. Success prints `Success`.

Application id: `com.hackathon.v2x.ivi`.

## Step 4 — Launch & smoke-test

### Open the HMI

- In the CarSky interactive view (God View / guest display) for the IVI node, launch **V2X IVI** (or the package above).
- Confirm the R16 layout: central Display Area + side buttons (Home / Apps / Settings) + bottom status bar.

### Logcat filters

Watch for crash-free startup and R4 UDP activity:

```bash
adb logcat -s R4ListenerService R4Deserializer IVI_V2X MainViewModel WarningViewModel
```

Also fail the smoke test on any `FATAL EXCEPTION` for this process:

```bash
adb logcat *:E | findstr /I "FATAL com.hackathon.v2x.ivi"
# Linux/macOS: adb logcat *:E | grep -E 'FATAL|com.hackathon.v2x.ivi'
```

### Mock R4 sender (UDP → port 5004)

`BuildConfig.R4_UDP_PORT` defaults to **5004**. On the Room Ethernet bridge, the IVI pin address is documented in [node-ivi-ecu.md](../../requirements/car-sky-guide/node-ivi-ecu.md) (example static address `10.99.0.13` — use the live pin IP).

**Local loopback** (emulator / laptop only):

```bash
cd mock-sender
IVI_ECU_HOST=127.0.0.1 IVI_ECU_PORT=5004 CYCLES=2 python mock_r4_sender.py
```

**Against the CarSky Room** (sender on a container/host that can reach the IVI eth address):

```bash
IVI_ECU_HOST=<ivi-ethernet-ip> IVI_ECU_PORT=5004 CYCLES=2 python mock_r4_sender.py
```

See [mock-sender/README.md](../mock-sender/README.md) for cycles (`nlos_obstruction`, state heartbeat, `future_unknown_type`).

Expected UI (once wake-on-warning + Display Area wiring are active): Display Area switches to Warning View; Ghost C uses `v2x_relayed` data; unknown `warningType` must not crash the process.

## Smoke test checklist

| Check | Pass criteria |
|---|---|
| Debug APK builds | `assembleDebug` exits 0 |
| APK size | `app-debug.apk` &lt; 50 MB |
| ADB connected | `adb devices` shows `device` |
| Install | `adb install -r …` prints `Success` |
| Launch | App opens on AAOS guest; no `FATAL EXCEPTION` in logcat |
| Layout | Display Area + side buttons visible (R16) |
| Mock R4 | Packets reach `R4ListenerService` (logcat); UI does not crash on unknown `warningType` |
| Wake-on-warning | Display Area switches to Warning View on Active warning (16.5.2.4) when end-to-end wire is present |

## Related

- [node-ivi-ecu.md](../../requirements/car-sky-guide/node-ivi-ecu.md) — Skycraft artifact IDs, ethernet pin, post-deploy ADB overview
- [phase5_tasks.md](../../plans/phase5_tasks.md) — subtask `16.5.2.5` and Phase 5 acceptance
- [mock-sender/README.md](../mock-sender/README.md) — scripted R4 UDP harness
