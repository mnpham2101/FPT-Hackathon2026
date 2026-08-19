# Android Automotive OS

**Target platform:** Android Automotive OS (AAOS — API 29+ / target 33)
**Author:** Vinh & Team IVI
**Date:** 03/08/2026

Class and flow names below are generic role names, not the identifiers of any one codebase. They describe the position a component occupies — listener, repository, view model — so the pattern reads independently of what a given project calls its types.

---

## 1. AAOS (Android Automotive OS) Architecture Overview

### Why AAOS instead of Mobile Android?

Standard Mobile Android is designed for personal smartphones (touchscreens, battery management, cellular lifecycle, portrait orientation). In contrast, **Android Automotive OS (AAOS)** is a full operating system designed directly for vehicle head units (In-Vehicle Infotainment — IVI):

| Category | Mobile Android | Android Automotive OS (AAOS) |
|---|---|---|
| **OS Footprint** | Companion device (requires Bluetooth/USB to car) | Embedded directly into vehicle head unit hardware |
| **Hardware Access** | Standard Sensors (GPS, Gyro, Camera) | Vehicle HAL (VHAL) access to CAN bus, HVAC, Speed, Gear |
| **System Lifecycle** | App dies on background/battery saver | Safety-critical services run continuously as `ForegroundService` |
| **HMI Layout** | Portrait phone display | Fixed landscape automotive resolution (e.g., 1280×720, 1920×1080) |
| **Driving Safety** | Distraction controls handled by app | System-enforced Driver Distraction Rules (UX Restrictions) |

> [!NOTE]
> For the deep-dive architectural reference on VHAL 5-stage signal routing across Binder IPC boundaries, see [VHAL Property Routing: From Physical Bus Bytes to Kotlin](vhal-property-routing-and-signal-pipeline.md).

---

## 2. End-to-End Message Ingest & Data Pipeline

A head-unit application ingests warnings from a perception ECU over a local Ethernet link using raw UDP datagrams. The datagram arrives on a service, not on the UI, so reception survives the screen showing something else.

```
┌─────────────────┐        UDP datagram       ┌────────────────────────────┐
│  Perception ECU │ ────────────────────────> │  IncomingMessageListener   │
│  (Data Source)  │      Raw JSON bytes       │     (ForegroundService)    │
└─────────────────┘                           └─────────────┬──────────────┘
                                                            │  messageFlow (SharedFlow)
                                                            ▼
┌─────────────────┐   warningEvents (SharedFlow) ┌────────────────────────────┐
│ AlertViewModel  │ <─────────────────────────── │     MessageRepository      │
│ (State Machine) │   currentState (StateFlow)   │     (Single Source)        │
└────────┬────────┘                              └────────────────────────────┘
         │
         │ alertUiState & latestScene (StateFlow)
         ▼
┌────────────────────────────────────────────────────────────────────────┐
│                          Jetpack Compose UI                            │
│    RootScreen (mode switcher)  ──>  CanvasAlertView (2D god view)      │
└────────────────────────────────────────────────────────────────────────┘
```

### 2.1 Serialization & Deserialization Protocol

Raw datagrams arrive as byte arrays. A deserialiser parses them via `kotlinx.serialization`, discriminating variants on a `type` field:

```kotlin
@Serializable
sealed class Message {
    abstract val schemaVersion: Int
}

@Serializable
@SerialName("warning")
data class WarningEvent(
    override val schemaVersion: Int = 1,
    val warningType: String,
    val riskState: String,
    @SerialName("object") val objectSnapshot: ObjectSnapshot,
    val geometry: SceneGeometry,
) : Message()
```

### 2.2 Additive-Version Safety & Lenient Parsing

- **Unknown JSON keys:** ignored silently (`ignoreUnknownKeys = true`).
- **Unknown enumeration value:** preserved verbatim so logs retain diagnostic evidence; classification happens at the UI layer.
- **Buffer truncation defence:** `packet.setLength(buffer.size)` is executed before every `socket.receive()` call to prevent JDK datagram buffer shrinkage.

---

## 3. Automotive Communication Protocols Evaluation

| Protocol | Latency | Overhead | Schema Enforcement | Use Case in Automotive |
|---|---|---|---|---|
| **Raw UDP Socket (Selected)** | **< 2 ms** | **Minimal (8-byte header)** | Application-layer JSON validator | **Safety warnings, ECU to head unit** |
| **SOME/IP** | 5–10 ms | Moderate | AUTOSAR FIBEX / ARXML | ECU-to-ECU service-oriented RPC |
| **gRPC / Protobuf** | 10–20 ms | Low payload, high CPU | `.proto` IDL contract | Cloud-to-Vehicle Telemetry |
| **MQTT** | 30–100 ms | High (Broker dependency) | None (Topic string) | Non-critical Infotainment (Weather/Media) |
| **Zenoh / DDS** | 3–5 ms | Low-to-Moderate | Shared Memory / PubSub | ADAS Sensor Fusion Internal Bus |

**Why Raw UDP for safety warnings?**
Safety warnings require zero-handshake, ultra-low latency broadcast (< 5 ms deadline). If an obstacle is detected at 60 km/h, every millisecond saved in transmission translates directly to vehicle stopping distance.

---

## 4. MVVM & Unidirectional Data Flow (State Machine)

### 4.1 State Transition Lifecycle

```
                 Incoming Warning Event
             ┌───────────────────────────────┐
             │                               ▼
    ┌────────────────┐   10s Timeout   ┌──────────────────┐
    │  HomeView      │ <────────────── │   WarningView    │
    │  (Idle Screen) │                 │ (Active Warning) │
    └────────────────┘                 └──────────────────┘
             ▲                               │
             │   Driver Manual Override Tab  │
             └───────────────────────────────┘
```

1. **Idle state (`HomeView`):** displays digital clock, telemetry, link status.
2. **Active warning state (`WarningView`):** the mode view model intercepts an incoming warning event and forces a transition to the alert mode with a 200 ms `AnimatedContent` fade.
3. **Auto-clear:** after a timeout constant (10,000 ms), state clears back to the idle screen unless a new warning arrives.
4. **User override:** if the driver manually taps another mode during a warning, the system respects driver intent rather than fighting it.

---

## 5. Jetpack Compose 2D Canvas & Geometry

### 5.1 Coordinate translation math

The perception ECU reports positions in metres relative to the ego vehicle: `Ego = (0, 0)`. A coordinate mapper translates metric coordinates to screen pixels:

$$\text{Pixel}_X = \frac{\text{Width}}{2} + (X_{\text{meters}} \times \text{Scale}_X)$$

$$\text{Pixel}_Y = \text{Height} - \text{Margin}_{\text{bottom}} - (Y_{\text{meters}} \times \text{Scale}_Y)$$

- **Clamping:** off-screen vehicles are clamped 16 px inside canvas boundaries so objects never disappear silently.

### 5.2 Defensive provenance guard

```kotlin
val snapshot = scene.relayedVehicleSnapshot
val sourceTrusted = snapshot == null || snapshot.source == ObjectSnapshot.SOURCE_RELAYED
```

- **Safety rationale:** a relayed vehicle — one reported by another vehicle rather than seen by the host's own sensors — is drawn as a hazard **only if** its provenance field says so.
- If an untrusted source arrives (own-sensor data mislabelled, or spoofed), the renderer draws an explicit unknown-source marker and logs at ERROR instead of presenting it as a confirmed hazard.

A provenance check belongs in the renderer rather than the parser: the parser's job is to accept a well-formed message, and a well-formed message can still carry a value the display must not treat as trustworthy.

---

## 6. Automotive HMI Framework Comparison

| Framework | Platform | Renderer | Industry Adoption |
|---|---|---|---|
| **Android Automotive (AAOS)** | Linux Kernel / Android | Jetpack Compose / View | Google, Volvo, Polestar, Ford, GM |
| **LG webOS Auto (Tiger)** | Linux | Qt / HTML5 Enact | LG, Hyundai Motor Group |
| **Qt / QML** | Bare Metal / RTOS / Linux | OpenGL / Vulkan | Mercedes-Benz (MB.OS), BMW |
| **Automotive Grade Linux (AGL)** | Yocto Linux | HTML5 / Flutter | Toyota, Mazda, Subaru |

### Conclusion

AAOS provides the optimal balance of developer ecosystem (Kotlin, Compose, Hilt) and native automotive hardware integration via VHAL.
