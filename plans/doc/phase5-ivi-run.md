# Phase 5 IVI run record

Evidence record for the Phase 5 in-Room IVI subtasks of [phase5_minh_tasks.md](../phase5_minh_tasks.md) group 5.9. Each section carries one subtask's recorded outputs; the procedure the outputs came from is [deploy-ivi-hmi-walkthrough.md](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md), cited by section.

**Deviation covering every section below:** the run was performed against the team's full **m1-system-test Room** (deployment `m1_system_test-deploy`, Rework device `KIS`), which was already `Running`, rather than the mini-blueprint of `5.5.9.1`–`5.5.9.5`. The ADB route, the guest properties and the install are node facts independent of which Room hosts the node; the mini-blueprint subtasks stay open.

## `16.5.9.19` — Provenance of the three ADB tunnel inputs

Answers [§6.1](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) items 2, 3 and 4. All three values were obtained 2026-08-05.

| Input | Provenance |
|---|---|
| `reach-backend` binary | Organizer-supplied zip, unpacked to `tools/apk uploader/reach_be/reach/` — `reach-backend.exe` (Windows, used here) beside a POSIX `reach-backend`. Git-ignored; only the folder's guide is committed |
| Gateway URL | `https://hackathon-2.carsky.io` — **the workbench base URL itself**, no separate gateway host. Read from the Rework **Local ADB** dialog: Devices → device `KIS` → the ADB widget's tab (part `ivi-adb`) → ADB SHELL panel → **Local ADB** button → *Connect from Terminal* |
| `a8k_…` token | Shown in the same *Connect from Terminal* dialog. It is **not** the CarSky API key: a distinct per-device derived value in single-segment `a8k_<value>` form, against the API key's `a8k_<prefix>_<secret>` form. Stored at `secrets/reach-adb-token-ivi.txt` (git-ignored); the value is not written into the repository. A redeploy may mint a new token — re-open the dialog after redeploying |

The click path to the dialog, step by step, is in [tools/apk uploader/README.md](../../tools/apk%20uploader/README.md) step 1.

## `16.5.9.6` — ADB tunnel start

Per [§4.4](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#44-get-an-adb-endpoint), started 2026-08-05T19:36:51Z and left serving in its own terminal:

```
[reach-backend] kind=adb  node_key=o6xi8rl0wkzc2ymnshpyv-n4  local_port=5555
[reach-backend] WebSocket endpoint: wss://hackathon-2.carsky.io/reach/adb/o6xi8rl0wkzc2ymnshpyv-n4
[reach-backend] Listening on 127.0.0.1:5555
```

The mentor-supplied route (§6.1 item 1) carried ADB on first use — no failure to record.

## `16.5.9.7` — Proven ADB route and guest properties

Per [§4.5](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#45-connect-and-check-the-guest); answers §6.1 items 1 and 5.

- `adb connect localhost:5555` → `connected to localhost:5555`; `adb devices` lists `localhost:5555   device`. A local emulator (`emulator-5554`) was also attached on this host, so every command was pinned with `-s localhost:5555` — the guide records the same caveat.
- `ro.build.version.sdk` = **34** (Android 14) — clears `minSdk 29`.
- `pm list features` carries `feature:android.hardware.type.automotive` — the guest accepts an automotive-required APK.
- The install route itself is proven by the `Success` recorded under `16.5.9.10` below.
- **Finding — the evidence filter streams but is empty on this build:** `adb logcat -s IVI_V2X` carries no lines. The installed debug APK logs its bind as `R4ListenerService: UDP socket open on port 47300` instead of the designed `[LINK] state=bound port=47300` on tag `IVI_V2X` ([§4.8](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) rung V1). Either the build predates the designed logging or the walkthrough describes a build that this APK is not — to reconcile before any V1-ladder evidence is cited.

