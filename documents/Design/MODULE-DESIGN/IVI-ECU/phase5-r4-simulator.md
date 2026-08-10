# Simulating the ADA ECU and driving IVI logic

Research note: how Phase 5 produces R4 traffic without an ADA ECU, and how each layer of the IVI app is exercised by it. The display track is mock-driven by definition ([milestone1_high_level_plan.md](../../../Plan/milestone1_high_level_plan.md) § Phase 5) — the simulator is sanctioned test equipment, not a mock to be deleted later (CLAUDE.md governing principle 2).

## 1. Four injection points, weakest coupling first

The IVI app has four places a test can enter. They are not alternatives — each proves something the others cannot, and each costs an order of magnitude more to run than the one above it.

| # | Injection point | What runs | What it proves | Where it runs |
|---|---|---|---|---|
| **I1** | Parser API — feed bytes/text directly | Parse + observer + view-model, no socket, no UI | Contract conformance: every sample and every malformed case | `:app:testDebugUnitTest`, already a CI job |
| **I2** | Loopback socket — send to `127.0.0.1:47300` in-process | The real listener, real socket, real flow | The receive loop, buffer handling, back-pressure | Robolectric / JVM test |
| **I3** | Dev injector — a debug-only entry that pushes one R4 message into the same flow the socket feeds | Everything above **plus the full UI** | The warning view actually comes up, on a real screen | Device or emulator, no network needed |
| **I4** | Real UDP from a peer | The whole path end-to-end | R6 hop 3, R16 acceptance | AAOS emulator (dev) or the mini-blueprint Room (evidence) |

**I1 and I2 are the ones that must be automated.** I3 exists because the AAOS guest may be hard to reach over the network before the ADB route is proven; it keeps UI work unblocked. I4 is what produces the recorded evidence.

### The dev injector (I3) is worth its keep

An `adb`-triggered entry point in the **debug build only** that constructs an `R4Message` from a bundled sample and emits it on the same flow the listener emits on:

```
adb shell am broadcast -a com.hackathon.v2x.ivi.DEV_INJECT --es sample r4-warning
```

Because it joins downstream of the socket and upstream of everything else, it exercises parse → observer → view-model → Compose exactly as a real datagram does. It must be excluded from the release build — a release path that can fabricate a warning would undermine the R19 claim that C came only from `v2x_relayed` data.

## 2. The simulator tool

One program, two run modes, one scenario format.

- **In-Room mode** — a container image on the ADA node of the mini-blueprint ([deploy-ivi-hmi-walkthrough.md § The mini-blueprint route](../../../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md#411-the-mini-blueprint-route)), reading `IVI_ECU_HOST` / `IVI_ECU_PORT` from the environment, starting itself from the entrypoint so a deploy alone produces evidence (the netcheck rule: no shell session is ever needed).
- **Host mode** — the same program run from a laptop against an emulator or a loopback listener, with the target given as arguments.

**Scenarios are data, not code.** A scenario is a file listing timed messages; the tool walks it and sends. Different scenario files must produce observably different streams — the same rule R11 imposes on the bench Scenario Player (the agreed layout), and for the same reason: a new case is a new file, never a new code branch.

### Scenario cases the tool must be able to emit

Each maps to an acceptance criterion; a scenario file selects a subset.

| Case | Message | Exercises |
|---|---|---|
| C approaching | `warning`, `riskState` low → medium → high, `geometry.vehicleC` closing | R17 render, reactive distance labels |
| C not yet tracked | `warning` with `geometry.vehicleC: null` | Renderer's null-C path — no crash, no placeholder |
| C leaves range | stream stops | View-model timeout → Idle → view restored |
| Unknown warning type | `warningType: "slippery_road"`, `schemaVersion: 2`, extra unknown field | **R4 additive-version test** — the committed Phase 5 acceptance box |
| Wrong provenance | `object.source: "own_sensor"` | Renderer's defensive source guard |
| Malformed | truncated / non-JSON bytes | Receive loop survives, logs, keeps listening |
| Periodic state | `type: "state"`, ascending `seq` | Optional R15 path, last-value-wins |

### Payloads come from the frozen contract, not from the tool

The first four cases already exist as committed fixtures — [contracts/samples/](../../../../contracts/samples/) holds `r4-warning.json`, `r4-state.json`, `r4-unknown-warning.json`, and they are byte-synced into `IVI_ECU/app/src/test/resources/contracts/samples/` by [contracts/check_sync.py](../../../../contracts/check_sync.py) against [sync-manifest.json](../../../../contracts/sync-manifest.json).

**The simulator must build its messages from those files, not from a hand-written literal.** A simulator with its own copy of the schema is a second, unversioned contract — it would keep passing after the real one changed. Motion (an approaching C) is produced by overwriting the geometry and distance numbers of a loaded sample, leaving every other field as the contract froze it.

## 3. Reaching an AAOS emulator with UDP (I4, dev)

Getting a datagram *into* an emulator is the one genuinely awkward step, and the obvious tool does not work:

- **`adb forward` cannot do this.** It forwards TCP (and abstract/local sockets) only — there is no UDP transport, and there is no reverse-UDP option either.
- **Emulator console redirection works** for a standard user-mode-networking emulator: connect to `telnet localhost 5554`, authenticate with the token in `~/.emulator_console_auth_token`, then `redir add udp:47300:47300`.
- **Simplest reliable route: send from inside the guest.** Push a tiny sender (or use the dev injector, I3) and run it over `adb shell`, so the datagram never crosses the emulator's NAT.

In the Room (I4, evidence) none of this applies: the ADA node and the guest share the bridge subnet, and the simulator sends straight to `10.99.0.13:47300`.

## 4. What "invoking IVI logic" means per layer

| Layer | Driven by | Assertion |
|---|---|---|
| Parser | I1 | Sample → typed message; malformed → failure result, no throw escaping |
| Observer / listener | I2 | N datagrams in → N events out on the flow; socket error → retry, not death |
| View-model | I1 or I2 | Idle → Active on a warning; Active → Idle after the configured timeout; state message updates last-value-wins |
| Display switcher | I1 | Warning forces the Warning View; the previous mode is restored, unless the user navigated away deliberately |
| Renderer | I3 (visual) + I1 (pure math) | Coordinate mapping is unit-tested with no Android types; the drawing is verified by eye against previews |
| End-to-end | I4 | `adb logcat -s IVI_V2X` shows the received event; the warning view is on screen |

The coordinate mapper and the parse layer stay free of Android imports precisely so I1 covers them in the existing `ivi-unit-tests` CI job (`phase5-ci.yml`) with no device in the loop.

## 5. Where the simulator lives

- **Language:** Kotlin/JVM, depending on the shared contract submodule — the same models the APK parses with, so a message the simulator can build is by construction a message the app can read. (User decision 2026-08-02; nlohmann/json stays on the C++ producer side, where the real ADA ECU uses it.)
- **Folder:** inside `IVI_ECU/`, as a Gradle submodule. It is IVI test equipment, and the no-cross-node-source-imports rule (the agreed layout) forbids it reaching into `ADA_ECU/`.
- **Container image:** built from that submodule's distribution for the in-Room mode, `linux/arm64`, pushed by CI like every other node image.
