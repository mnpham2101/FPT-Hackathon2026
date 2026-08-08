# Technical Deep-Dive Wiki — IVI ECU & HMI System

**Module:** `IVI_ECU/`  
**Target Platform:** Android Automotive OS (AAOS - API 29+ / Target 33)  
**Author:** Vinh & Team IVI  
**Date:** 03/08/2026  
**Contracts:** `contracts/r4-ada-ivi.schema.json` (R4 Version 1.0)

---

## 1. AAOS (Android Automotive OS) Architecture Overview

### Why AAOS instead of Mobile Android?

Standard Mobile Android is designed for personal smartphones (touchscreens, battery management, cellular lifecycle, portrait orientation). In contrast, **Android Automotive OS (AAOS)** is a full operating system designed directly for vehicle head units (In-Vehicle Infotainment - IVI):

| Category | Mobile Android | Android Automotive OS (AAOS) |
|---|---|---|
| **OS Footprint** | Companion device (requires Bluetooth/USB to car) | Embedded directly into vehicle head unit hardware |
| **Hardware Access** | Standard Sensors (GPS, Gyro, Camera) | Vehicle HAL (VHAL) access to CAN bus, HVAC, Speed, Gear |
| **System Lifecycle** | App dies on background/battery saver | Safety-critical services run continuously as `ForegroundService` |
| **HMI Layout** | Portrait phone display | Fixed landscape automotive resolution (e.g., 1280×720, 1920×1080) |
| **Driving Safety** | Distraction controls handled by app | System-enforced Driver Distraction Rules (UX Restrictions) |

---

## 2. End-to-End R4 Ingest & Data Pipeline

The IVI ECU ingests cooperative awareness warnings from the ADA (Autonomous Driving Assistant) ECU over the local Ethernet Bridge using raw UDP datagrams.

```
┌─────────────────┐       UDP Port 47300      ┌───────────────────────┐
│     ADA ECU     │ ────────────────────────> │  R4ListenerService    │
│  (Data Source)  │  Raw JSON Bytes (R4)      │  (ForegroundService)  │
└─────────────────┘                           └───────────┬───────────┘
                                                          │  r4EventFlow (SharedFlow)
                                                          ▼
┌─────────────────┐   warningEvents (SharedFlow) ┌───────────────────────┐
│ WarningViewModel│ <─────────────────────────── │     R4Repository      │
│  (State Machine)│   currentState (StateFlow)   │   (Single Source)     │
└────────┬────────┘                              └───────────────────────┘
         │
         │ uiWarningState & latestScene (StateFlow)
         ▼
┌────────────────────────────────────────────────────────────────────────┐
│                          Jetpack Compose UI                            │
│  MainScreen (R16 Switcher)  ──>  CanvasWarningView (R17 2D God View)   │
└────────────────────────────────────────────────────────────────────────┘
```

### 2.1 Serialization & Deserialization Protocol

Raw Ethernet packets arrive as byte arrays. `R4Deserializer` parses them via `kotlinx.serialization`:

```kotlin
@Serializable
sealed class R4Message {
    abstract val schemaVersion: Int
}

@Serializable
@SerialName("warning")
data class R4WarningEvent(
    override val schemaVersion: Int = 1,
    val warningType: String,
    val riskState: String,
    @SerialName("object") val objectSnapshot: R3Snapshot,
    val geometry: SceneGeometry,
) : R4Message()
```

### 2.2 Additive-Version Safety & Lenient Parsing
- **Unknown JSON keys:** Ignored silently (`ignoreUnknownKeys = true`).
- **Unknown `warningType`:** Preserved verbatim (`future_unknown_type`) so logs retain diagnostic evidence; classification happens at the UI layer.
- **Buffer Truncation Defense:** `packet.setLength(buffer.size)` is executed before every `socket.receive()` call to prevent JDK datagram buffer shrinkage.

---

## 3. Automotive Communication Protocols Evaluation

| Protocol | Latency | Overhead | Schema Enforcement | Use Case in Automotive |
|---|---|---|---|---|
| **Raw UDP Socket (Selected)** | **< 2 ms** | **Minimal (8-byte header)** | Application-layer JSON validator | **Safety Warnings (R4 ADA→IVI)** |
| **SOME/IP** | 5–10 ms | Moderate | AUTOSAR FIBEX / ARXML | ECU-to-ECU service-oriented RPC |
| **gRPC / Protobuf** | 10–20 ms | Low payload, high CPU | `.proto` IDL contract | Cloud-to-Vehicle Telemetry |
| **MQTT** | 30–100 ms | High (Broker dependency) | None (Topic string) | Non-critical Infotainment (Weather/Media) |
| **Zenoh / DDS** | 3–5 ms | Low-to-Moderate | Shared Memory / PubSub | ADAS Sensor Fusion Internal Bus |

**Why Raw UDP for R4?**
R4 safety warnings require zero-handshake, ultra-low latency broadcast (< 5ms deadline). If an obstacle is detected at 60 km/h, every millisecond saved in transmission translates directly to vehicle stopping distance.

---

## 4. MVVM & Unidirectional Data Flow (State Machine)

### 4.1 State Transition Lifecycle

```
                 Incoming R4 Warning Event
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

1. **Idle State (`HomeView`):** Displays digital clock, telemetry, V2X link status.
2. **Active Warning State (`WarningView`):** `MainViewModel` intercepts incoming `R4WarningEvent` and forces transition to `DisplayMode.WarningView` with a 200ms `AnimatedContent` fade.
3. **Auto-Clear:** After `WARNING_TIMEOUT_MS` (10,000 ms), state clears back to `HomeView` unless a new warning arrives.
4. **User Override:** If driver manually taps "Apps" or "Settings" during a warning, system respects driver intent without crashing.

---

## 5. Jetpack Compose 2D Canvas & Geometry

### 5.1 Coordinate Translation Math (`SceneCoordinateMapper`)

The ADA ECU reports vehicle positions in meters relative to Ego: `Ego = (0, 0)`. `SceneCoordinateMapper` translates metric coordinates to screen pixels:

$$\text{Pixel}_X = \frac{\text{Width}}{2} + (X_{\text{meters}} \times \text{Scale}_X)$$

$$\text{Pixel}_Y = \text{Height} - \text{Margin}_{\text{bottom}} - (Y_{\text{meters}} \times \text{Scale}_Y)$$

- **Clamping:** Off-screen vehicles are clamped 16 px inside canvas boundaries so objects never disappear silently.

### 5.2 R19 Defensive Provenance Guard

```kotlin
val snapshot = scene.vehicleCSnapshot
val cSourceTrusted = snapshot == null || snapshot.source == R3Snapshot.SOURCE_V2X_RELAYED
```

- **Safety Rationale:** Ghost C (relayed vehicle) is drawn in dashed red with pulsing glow **only if** `source == "v2x_relayed"`.
- If an untrusted source arrives (e.g., `own_sensor` or spoofed data), `CanvasWarningView` renders a yellow `[? UNKNOWN SOURCE]` marker and logs an ERROR (`IVI_V2X`).

---

## 6. Automotive HMI Framework Comparison

| Framework | Platform | Renderer | Industry Adoption |
|---|---|---|---|
| **Android Automotive (AAOS)** | Linux Kernel / Android | Jetpack Compose / View | Google, Volvo, Polestar, Ford, GM |
| **LG webOS Auto (Tiger)** | Linux | Qt / HTML5 Enact | LG, Hyundai Motor Group |
| **Qt / QML** | Bare Metal / RTOS / Linux | OpenGL / Vulkan | Mercedes-Benz (MB.OS), BMW |
| **Automotive Grade Linux (AGL)** | Yocto Linux | HTML5 / Flutter | Toyota, Mazda, Subaru |

### Conclusion
AAOS provides the optimal balance of developer ecosystem (Kotlin, Compose, Hilt) and native Automotive hardware integration via VHAL.
