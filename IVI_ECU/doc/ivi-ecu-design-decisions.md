# IVI ECU — design decisions

> The decision record for [ivi-ecu-hld.md](ivi-ecu-hld.md), which cites these by number. Binding on implementation: a decision is revisited by changing its entry here, never by an implementation that departs from it.

## D1 — The contract library is kotlinx.serialization in a pure-JVM submodule

The R4/R3 models and the configured `Json` live in `:contract`, with zero Android dependencies, consumed by the APK and the simulator alike.

- nlohmann/json stays on the ADA side, where the report puts it. Inside a Kotlin APK it would mean an NDK/JNI layer for a job the Kotlin binding already does.
- A submodule, not a package in `:app`: it must be usable by a command-line tool with no Android SDK, and testable on a plain JVM. One artifact shared by producer and consumer is what stops the two drifting.

## D2 — Five modules, one-way dependency graph

`:contract` ← `:serializer` ← `:observer` ← `:app`, and `:contract` ← `:r4-simulator`. No cycles, and **Android types exist only in `:app`**.

| Module | Its one job | Depends on |
|---|---|---|
| `:contract` | the R4/R3 models, `R4Json`, the frozen samples | — (kotlinx-serialization-json only) |
| `:serializer` | datagram bytes → typed payload, or a typed failure | `:contract` |
| `:observer` | the socket, the receive loop, the retry policy, the event flow | `:serializer` |
| `:app` | R16 layout, Display Area switcher, warning view-model, R17 God View | `:observer` |
| `:r4-simulator` | test equipment: scenario-driven R4 traffic | `:contract` |

What the split buys:

- `:serializer` and `:observer` are plain JVM, so they run in CI with no device and no Robolectric.
- `:observer` never imports `android.util.Log`; it logs through the `R4Logger` seam.
- `:serializer` never logs and never throws across the loop, so one bad message cannot stop the next good one.
- The simulator reaches the app's models through `:contract`, never by importing across node folders.

## D3 — De-framing is buffer slicing, not header parsing

Android hands the app a packet with the Ethernet, IP and UDP headers already removed: one R4 message is one datagram is one UTF-8 JSON object — no length prefix, no envelope, no framing header. "Strip the header, keep the payload" therefore means five things and nothing more:

| Required behaviour | Module | Failure it prevents |
|---|---|---|
| Decode `data[offset until offset+length]`, never the whole backing array | `:serializer` | Trailing bytes of a previous, longer datagram appended |
| `packet.setLength(buffer.size)` before **every** `receive()` | `:observer` | Every datagram after the first truncated to the shortest seen |
| Treat `length == bufferBytes` as truncation-suspect: log, still decode | `:observer` | Silent UDP truncation presenting as malformed JSON |
| Strip a UTF-8 BOM and surrounding whitespace before decoding | `:serializer` | A BOM the JSON parser will not tolerate |
| **No accumulate-and-split logic at all** | both | TCP-style framing invented for a protocol that preserves boundaries |

`R4_SOCKET_BUFFER_BYTES` defaults to 2048 against a ~450 B warning. `network_security_config.xml` governs HTTP stacks only, not a raw `DatagramSocket`.

## D4 — Unknown `warningType` is preserved verbatim; classification happens at the UI edge

The parser puts the wire value into `warningType` and stops. `WarningClassifier` maps known types to a presentation and everything else to a generic one.

- **The parser must never rewrite an unknown `warningType` to `"unknown"`.** `R4AdditiveVersionTest` asserts the opposite, and rewriting would destroy what the log needs and push a UI concern into the data layer.
- A `schemaVersion` above `R4Contract.KNOWN_SCHEMA_VERSION` is not a gate: decode succeeds, `schemaVersionAhead` is set, the observer logs it once.

## D5 — A foreground service hosts the observer

`R4ListenerService` starts foreground immediately and holds foreground priority for the whole recorded run (R19 is *one continuous* run). Rejected alternative: a loop scoped to the Activity lifecycle, which ties reception to whether the UI is resumed.

- **The service is a lifecycle host, not the loop.** The loop, the back-off and the flow are plain-JVM code in `:observer`; the service only calls `start`/`stop`.
- **Socket:** bind `0.0.0.0:<port>`, never the node address, which the bridge assigns. Bind failure logs at ERROR and retries with back-off.
- **Back-pressure:** `MutableSharedFlow`, bounded `extraBufferCapacity`, `DROP_OLDEST`, emitted with `tryEmit`. The newest warning is the one that matters, and a slow collector must not stall the socket.
- `POST_NOTIFICATIONS` is a runtime permission from API 33: a denial suppresses the notification only, and is never a failure to start.

## D6 — The frozen samples are **main** resources of `:contract`

One byte-synced location, `contract/src/main/resources/contracts/samples/*.json`, registered in [contracts/sync-manifest.json](../../contracts/sync-manifest.json), reachable from the three places that must agree: the contract tests, the simulator's payload builder, the dev injector.

Accepted cost: ~2 KB of fixtures ship inside the release APK. Rejected alternative: three separate copies — the drift the sync manifest exists to prevent.

## D7 — Manual composition root; no Hilt

The object graph is seven objects, one Activity and one service. A hand-written `IviGraph` in `IviApplication.onCreate` wires it with no annotation processor; Hilt would add `hilt-navigation-compose` and a KSP round per build. Against [solution-selection-criteria](../../.claude/rules/solution-selection-criteria.md): C2 and C4. Replacing `IviGraph` changes no consumer, since view-models come from one `ViewModelProvider.Factory` either way.

## D8 — A Gradle version catalog governs all five modules

`IVI_ECU/gradle/libs.versions.toml` is the single place Kotlin, kotlinx-serialization, coroutines, the Compose BOM, AGP and JUnit versions are declared; every module uses aliases. Five modules resolving versions independently is how a skew reaches runtime instead of failing the build. `settings.gradle.kts` keeps `RepositoriesMode.FAIL_ON_PROJECT_REPOS`; no module declares its own repositories.

## D9 — The simulator mutates JSON, then validates through `R4Json` before sending

A simulator with its own copy of the schema is a second, unversioned contract that keeps passing after the real one changes. So each step:

1. Loads the frozen sample from the `:contract` classpath.
2. Applies the scenario's overrides at `JsonElement` level — `riskState`, `warningType`, `schemaVersion`, `object.source`, `object.distance`, `geometry.vehicleC` (including explicit `null`), plus additive junk fields.
3. Decodes the result through `R4Json` before sending. A payload the simulator cannot parse is one the app cannot parse, so the run fails at the producer.
4. Sends, logs `[TX]`, waits for the scenario's rate.

A step of kind `raw` is the one exception: it sends literal bytes on purpose. **Scenarios are data, not code** — the rule R11 imposes on the bench. JSON, parsed by kotlinx.serialization, so the tool adds no dependency beyond `:contract`.

## D10 — Configuration: `BuildConfig` defaults, launch-time override, no literals

An installed APK cannot be reconfigured without a rebuild, unlike a container node fed by the blueprint. So `BuildConfig` supplies the default, `MainActivity` accepts an intent-extra override at launch, and `IviRuntimeConfig.resolve(intent)` is the one place they merge. Every other component receives the resolved value.

| `BuildConfig` field | Default | Consumer | Override at launch |
|---|---|---|---|
| `R4_UDP_PORT` | `47300` (blueprint-frozen) | `R4ObserverConfig.port` | `--ei r4_port` |
| `R4_SOCKET_BUFFER_BYTES` | `2048` | `R4ObserverConfig.bufferBytes` | — |
| `R4_FLOW_BUFFER_EVENTS` | `8` | `SharedFlow` extra buffer | — |
| `R4_RETRY_INITIAL_MS` / `R4_RETRY_MAX_MS` | `500` / `5000` | rebind back-off | — |
| `WARNING_TIMEOUT_MS` | `10000` | `WarningViewModel` auto-dismiss | `--el warning_timeout_ms` |
| `SCENE_SCALE_M_PER_PX` | `0.5` | `CanvasWarningView` → `SceneCoordinateMapper` | `--ef scene_scale` |

`SCENE_SCALE_M_PER_PX` is the projection's **base** scale, not a uniform metres-per-pixel: R17's inclined camera compresses depth toward the top, so the mapper derives a distance-varying scale from this one value.

## D11 — Standing decisions binding on this design

- **`WarningBannerOverlay` is built but never mounted.** The God-View canvas must render unobstructed, which is also R17's requirement: no banner, no legend, no text overlay.
- **Ghost C renders only from `v2x_relayed`.** The source guard is the mechanical form of the R19 claim, and stays exercised by a test.
- **3D (`SceneViewWarning3D`) and multi-process wake-on-warning are optional**, not M1 deliverables. Nothing depends on either.
- **The periodic `state` message is optional on the producer side.** The consumer parses it, last-value-wins by `seq`; no acceptance criterion depends on it.
- **Relative geometry only** — no map, no GNSS injection. Positions arrive in the ego frame and are drawn in it.
- **No JavaScript and no WebSocket in this node**, as in the rest of the ego software path.
- **The ego video clip in the Display Area is deferred from M1** — a later milestone's addition, not a gap to fill.

## D12 — The provenance guard fails open, so every scene composer fills the snapshot

`SceneGeometry.vehicleCSnapshot` is nullable and the guard treats `null` as trusted. It therefore protects the render only when the scene composer copies the R4 message's `object` snapshot into that field. A `SceneGeometry` built from `geometry` alone carries no snapshot, draws ghost C unchallenged, and voids the R19 provenance claim.
