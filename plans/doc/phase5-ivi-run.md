# Phase 5 IVI run record

Evidence record for the Phase 5 in-Room IVI subtasks of [phase5_minh_tasks.md](../phase5_minh_tasks.md) group 5.9. Each section carries one subtask's recorded outputs; the procedure the outputs came from is [deploy-ivi-hmi-walkthrough.md](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md), cited by section.

**Deviation covering every section below:** the run was performed against the team's full **m1-system-test Room** (deployment `m1_system_test-deploy`, Rework device `KIS`), which was already `Running`, rather than the mini-blueprint of `5.5.9.1`–`5.5.9.5`. The ADB route, the guest properties and the install are node facts independent of which Room hosts the node; the mini-blueprint subtasks stay open.

## `16.5.9.19` — Provenance of the three ADB tunnel inputs

Answers [§6.1](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#61-confirm-before-relying-on-these) items 2, 3 and 4. All three values were obtained 2026-08-05.

| Input | Provenance |
|---|---|
| `reach-backend` binary | Organizer-supplied zip, unpacked to `tools/apk-uploader/reach_be/reach/` — `reach-backend.exe` (Windows, used here) beside a POSIX `reach-backend`. Git-ignored; only the folder's guide is committed |
| Gateway URL | `https://hackathon-2.carsky.io` — **the workbench base URL itself**, no separate gateway host. Read from the Rework **Local ADB** dialog: Devices → device `KIS` → the ADB widget's tab (part `ivi-adb`) → ADB SHELL panel → **Local ADB** button → *Connect from Terminal* |
| `a8k_…` token | Shown in the same *Connect from Terminal* dialog. It is **not** the CarSky API key: a distinct per-device derived value in single-segment `a8k_<value>` form, against the API key's `a8k_<prefix>_<secret>` form. Stored at `secrets/reach-adb-token-ivi.txt` (git-ignored); the value is not written into the repository. A redeploy may mint a new token — re-open the dialog after redeploying |

The click path to the dialog, step by step, is in [tools/apk-uploader/README.md](../../tools/apk-uploader/README.md) step 1.

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
- **Finding — only the bind line lacks the `IVI_V2X` tag on this build:** the bind is logged as `R4ListenerService: UDP socket open on port 47300` instead of the designed `[LINK] state=bound port=47300` on `IVI_V2X` ([§4.8](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#48-verify-the-hmi-and-the-logging) rung V1). The receive path does log `[RX]`/`[DROP]` on `IVI_V2X` ([R4ListenerService.kt](../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/service/R4ListenerService.kt) receive loop), so the filter streamed empty at this point only because no datagram had yet arrived. V1's designed line is the one deviation to reconcile; rungs V2 upward filter correctly on this build.

## `16.5.9.10` — APK install, launch, boot-to-listener time (partial)

Per [§4.6](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#46-install-the-apk) and the launch half of [§4.7](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#47-open-the-screen-and-launch-the-app), on 2026-08-05T19:37Z:

- `adb install -r "tools/apk-uploader/app-debug.apk"` → `Success` (streamed install).
- `pm path com.hackathon.v2x.ivi` → `/data/app/~~a8i7wEwPnSJ3wSecEXpztQ==/com.hackathon.v2x.ivi-yjJOmC0GhKaioG-9Q4Bq6w==/base.apk`.
- `am start -n com.hackathon.v2x.ivi/.MainActivity` → `Displayed … +1s173ms`; `R4ListenerService` started as a foreground service.
- Listener bound: `R4ListenerService: UDP socket open on port 47300` at 19:37:31.564, corroborated by `/proc/net/udp` showing `*:47300` UNCONN.
- **Launch → listener-bound ≈ 0.6 s** (`am start` 19:37:30.93 → socket open 19:37:31.564).

Not produced, so the subtask stays open:

- **Guest-boot → launcher delta:** the guest had booted ~19:06Z, ~31 minutes before this install — the delta requires a fresh boot with the APK already installed, which this run did not perform. The boot-to-listener floor for the bench's `start_delay_s` is therefore still unmeasured.
- **Rung V1 as specified:** the `[LINK] state=bound port=47300` line on `IVI_V2X` did not appear (tag finding under `16.5.9.7`); the bind is proven by the `R4ListenerService` line and the socket table instead.
- **No warning datagram observed:** a 3-minute logcat watch after launch saw no `[RX]`/`[DROP]` in the app log. The m1-system-test Room runs the correctly configured real chain (operator-confirmed), so a missing feed does not explain the silence — resolved by the hop investigation below.

## ADA → IVI hop investigation — the warning dies before the wire

Evidence gathered 2026-08-05T20:14–20:20Z against the m1-system-test Room (roomId `zvpi8tzdj8s08wmxaaye2`), 13 minutes of guest watch plus the ADA node's `user`-container log over REST. Each link of the chain, with its verdict:

| Link | Verdict | Evidence |
|---|---|---|
| App receive path (socket → deserializer → `IVI_V2X` log) | **Good** | A loopback probe injected in the guest (`echo probe-not-json \| nc -u 127.0.0.1 47300`) produced `[DROP] Skipping bad packet: Malformed R4 payload …` on `IVI_V2X` — the walkthrough's "a drop is the pass" observable, via loopback instead of the bridge |
| ADA warning production | **Good** | `r4_tx` events at ~1 per 3 s (counter 62 → 90 across the watch), each a well-formed R4 warning (`type=warning`, `warningType=nlos_obstruction`, ghost C `source=v2x_relayed`, 515 bytes) with `"dest":"10.99.0.13:47300","send_ok":true` |
| ADA node config | **Good** | Blueprint read-back: env `IVI_ECU_HOST=10.99.0.13`, `IVI_ECU_PORT=47300`; ADA pin `10.99.0.12`, IVI pin `10.99.0.13`; `CAPTURE_FILTER` unset → default `udp`, unfiltered by port |
| The warning on the wire | **Never appears** | ADA's tcpdump (`-i any`, filter `udp`) captures inbound V2X → ADA traffic and even other nodes' bridge traffic (promiscuous `P` lines, bench → V2X), but shows **zero** packets toward `10.99.0.13` and zero `Out` lines across a window holding 67 `r4_tx` events |
| The guest's addressing | **No bridge presence** | `ip addr` in the guest shows only `buried_eth0` `10.0.2.15/24` and `wlan0` `10.0.2.96/24` (cuttlefish NAT) — no `10.99.0.x` interface; the guest's interface RX counters are near zero |

**Reading:** `send_ok=true` with nothing captured is consistent with an unanswered ARP — UDP `sendto` is fire-and-forget, the kernel holds the datagram while ARP for `10.99.0.13` goes unanswered, and ARP is invisible to a `udp` capture filter. Nothing on the Room bridge answers for `10.99.0.13`: the Skycraft node's ethernet pin does not put an ARP-answering `10.99.0.13` on the wire, and even if it did, the guest sits behind cuttlefish NAT with no visible forward of UDP 47300 into the VM.

**Open question for the organizers (BTC):** how is a Skycraft node's ethernet pin supposed to deliver UDP into the AAOS guest — does the sidecar forward it, and does that require the pin's `port` field (currently `null`) or other config? Until answered, the ADA → IVI hop is the one link of the R19 chain with no proven route, and the loopback injection above is the only way to exercise the app's warning path in-Room.
