# How the IVI app observes ADA→IVI (R4) messages

**Audience:** learning note for the delivery report (code freeze).  
**Author:** Vũ Xuân Bách · Phase 5 IVI  
**Contract:** [r4-ada-ivi.schema.json](../../../../contracts/r4-ada-ivi.schema.json)  
**Authorities:** [R4ListenerService.kt](../../../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/service/R4ListenerService.kt) · [R4Repository.kt](../../../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/data/R4Repository.kt) · [R4Deserializer.kt](../../../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/data/R4Deserializer.kt) · [WarningViewModel.kt](../../../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/ui/WarningViewModel.kt) · [android-automotive-os.md](../../../KnowledgeBase/android-automotive-os.md) §2 · [ivi-ecu-hld.md](ivi-ecu-hld.md)

This note answers Lead’s topic: *app IVI dùng gì để observe bản tin từ ADA-ECU?* It cites the technical wiki; it does not replace it.

---

## 1. One-sentence answer

The IVI **does not poll** ADA. ADA sends **UDP datagrams** (R4 JSON) to **`10.99.0.13:47300`**. The app **binds a `DatagramSocket`** inside a **foreground `R4ListenerService`**, **deserializes** with **kotlinx.serialization**, and **observes** results through **Kotlin `SharedFlow` / `StateFlow`** collected in ViewModels and Compose.

There is **no** MQTT, gRPC, REST long-poll, ContentProvider, or LiveData bus on this path for M1.

---

## 2. End-to-end observe chain

```
ADA ECU (or m1-r4-sim)
    │  UDP sendto(10.99.0.13, 47300)  — one JSON object per datagram
    ▼
R4ListenerService   (ForegroundService, Dispatchers.IO)
    │  DatagramSocket.receive → ByteArray
    │  R4Deserializer.deserialize → Result<R4Message>
    │  MutableSharedFlow.emit  → r4EventFlow
    ▼
MainActivity bind  →  WarningViewModel.attachService(r4EventFlow)
    │
    ▼
R4Repository.attachToService
    │  warning → warningEvents (SharedFlow)
    │  state   → currentState (StateFlow)
    ▼
WarningViewModel.collect
    │  Idle → Active(event); latestScene = geometry + objectSnapshot
    │  auto-clear after WARNING_TIMEOUT_MS
    ▼
MainViewModel.collect(uiWarningState)   → wake-on-warning DisplayMode
MainScreen.collectAsStateWithLifecycle  → CanvasWarningView.Render
```

Producer and pin facts: [node-ada-ecu.md](../../../../requirements/car-sky-guide/node-ada-ecu.md) · [node-ivi-ecu.md](../../../../requirements/car-sky-guide/node-ivi-ecu.md).

---

## 3. Transport layer — what “listen” means

| Item | Value / behaviour |
| --- | --- |
| Port | `BuildConfig.R4_UDP_PORT` — default **47300** (blueprint ADA→IVI) |
| Bind address | All interfaces (`0.0.0.0`), not a hardcoded `10.99.0.13` in Kotlin |
| Host class | `R4ListenerService` — Manifest service, `foregroundServiceType=dataSync` |
| Threading | Receive loop on `Dispatchers.IO`; UI never calls `receive()` |
| Failure | Socket errors → log + retry / backoff; max retries can surface a service error path |

Room Ethernet must assign the guest **`10.99.0.13`** or ADA’s packets never arrive; loopback inject (`127.0.0.1:47300`) only proves the listener, not the eth hop.

---

## 4. Parse layer — typed messages, additive safety

`R4Deserializer` turns UTF-8 JSON bytes into sealed `R4Message`:

| Wire `type` | Kotlin type | Downstream |
| --- | --- | --- |
| `warning` | `R4WarningEvent` | Warning UI + God View geometry |
| `state` | `R4StateMessage` | Last-value `currentState` (heartbeat) |
| other / corrupt | `Result.failure` | Skip / log; loop continues |

Design rules (HLD / D4):

- Unknown JSON fields ignored (`ignoreUnknownKeys`).
- Unknown `warningType` **preserved on the wire value** — not rewritten to `"unknown"` in the parser.
- One bad datagram must not stop the next good one.

---

## 5. Observation API — Flows, not callbacks in the UI

| Channel | Type | Role |
| --- | --- | --- |
| `R4ListenerService.r4EventFlow` | `SharedFlow<R4Message>` | Hot stream of parsed messages from the socket |
| `R4Repository.warningEvents` | `SharedFlow<R4WarningEvent>` | Warning-only fan-out |
| `R4Repository.currentState` | `StateFlow<R4StateMessage?>` | Last state message |
| `WarningViewModel.uiWarningState` | `StateFlow<WarningUiState>` | Idle / Active / Error for UI logic |
| `WarningViewModel.latestScene` | `StateFlow<SceneGeometry?>` | Canvas input (`vehicleCSnapshot` from `object`) |

**Why Flow:**

- Multiple collectors (repository, tests) without a custom listener list.
- Structured concurrency via `viewModelScope` — cancelled when the ViewModel clears.
- Compose uses `collectAsStateWithLifecycle` so collection follows the UI lifecycle safely.

**Bind step:** `MainActivity`’s `BindR4ListenerService` obtains `LocalBinder.getService()` and calls `warningViewModel.attachService(…)`, which calls `repository.attachToService(serviceFlow, viewModelScope)`. Until bind succeeds, the socket may already be open, but the UI is not yet attached to the flow.

---

## 6. From observation to HMI

| Step | Mechanism |
| --- | --- |
| Warning arrives | `WarningViewModel` → `WarningUiState.Active`; `latestScene` updated |
| Wake-on-warning | `MainViewModel` forces `DisplayMode.WarningView` |
| Draw | `CanvasWarningView` behind `IviWarningViewSeam`; provenance guard requires `source == v2x_relayed` for ghost C |
| Silence | After `BuildConfig.WARNING_TIMEOUT_MS`, Active → Idle; Display Area restores previous mode (unless user override) |

UI shell details: [android-screen-lifecycle.md](../../../KnowledgeBase/android-screen-lifecycle.md).

---

## 7. What this is *not*

| Pattern | Used for R4 M1? |
| --- | --- |
| `LiveData` | No — StateFlow / SharedFlow |
| Room / SQLite cache of warnings | No — in-memory flows |
| HTTP / WebSocket to ADA | No — raw UDP |
| BroadcastReceiver as primary path | No (debug inject only, if present) |
| Binding ADA’s process | Impossible — ADA is another CarSky node |

---

## 8. How to prove observation in a demo

| Surface | What to show |
| --- | --- |
| ADA **View Log** | `[TX] … → 10.99.0.13:47300` |
| Guest logcat | Tags `R4ListenerService` / `R4Deserializer` / designed `IVI_V2X` lines |
| Screen | `MODE: WARNING`, God View, status `BOUND :47300` |

Procedure ladder: [deploy-ivi-hmi-walkthrough.md](../../../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md) §4.8.
