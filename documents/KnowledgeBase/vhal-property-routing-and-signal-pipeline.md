# VHAL Property Routing: From Physical Bus Bytes to Kotlin (AAOS Signal Pipeline)

> **Topic:** Vehicle Hardware Abstraction Layer (VHAL) Property Routing & Byte-to-Kotlin Signal Traversal  
> **Target Platform:** Android Automotive OS (AAOS - API 29+ / AIDL VHAL)  
> **Category:** Knowledge Base Article  
> **Status:** Standalone Architectural Reference  

---

## 1. The Port Customs Broker Analogy

Think of VHAL as a **port customs broker** operating at the edge of the vehicle operating system:

```text
+---------------------------+
|   Ship at Harbor          |  Cargo Ships = CAN / LIN / Ethernet Buses
|   (Physical Bus Frames)   |  Raw Binary Crates & Signals
+-------------+-------------+
              |
              v
+---------------------------+
|   Port Customs Broker     |  VHAL (Vehicle Hardware Abstraction Layer)
|   (Native VHAL Service)   |  Offloads, validates manifests, standardizes
+-------------+-------------+  into Android VehiclePropValue structures
              |
              v
+---------------------------+
|   City Transport Fleet    |  CarService & App CarPropertyManager
|   (Kotlin App UI)         |  Delivers strongly-typed Kotlin properties
+---------------------------+  to driver-facing HMI applications
```

A container ship does not deliver raw wooden crates directly to local couriers in a city. A port gateway offloads the containers, and a customs broker standardizes the manifests before handing clean packages to city transport.

In Android Automotive OS (AAOS), the **Vehicle Hardware Abstraction Layer (VHAL)** is that customs broker - translating raw physical vehicle frames (CAN, LIN, SPI, Ethernet) into standardized, permission-enforced Android property structures.

---

## 2. The 5-Stage Traversal Across Two Binder Boundaries

Telemetry crosses **TWO Binder IPC boundaries** across 5 distinct hops to reach a Kotlin UI application:

```text
[1] Gateway MCU
  | Signal Conditioning, Validation, Protocol Termination (CAN/LIN/Ethernet)
  v
[2] Native VHAL Service (AIDL)
  | Maps vendor signals -> AIDL VehiclePropValue structs
  v
  =========================================================================
  BINDER BOUNDARY 1 (IPC: IVehicleCallback)
  =========================================================================
  v
[3] CarService (PropertyHalService)
  | Receives properties via Binder IPC and validates payload structure
  v
[4] CarService (CarPropertyService)
  | Resolves Area IDs (Driver vs Passenger zone) & Enforces Caller Permissions
  v
  =========================================================================
  BINDER BOUNDARY 2 (IPC: ICarPropertyEventListener)
  =========================================================================
  v
[5] CarPropertyManager (Kotlin App UI)
  | Delivers strongly-typed Kotlin CarPropertyValue<T> into Jetpack Compose / ViewModel
```

### Stage Breakdown:

| Hop | Component | Layer / Boundary | Responsibility |
|---|---|---|---|
| **1** | **Gateway MCU** | Firmware / Hardware | Terminates physical CAN/LIN bus framing, filters noise, validates CRC, forwards payload to SoC over SPI or Ethernet. |
| **2** | **Native VHAL Service** | C++ / AIDL (`hardware/interfaces/automotive/vehicle/aidl/`) | Translates proprietary OEM CAN signals into standardized AIDL `VehiclePropValue` structures (`prop`, `areaId`, `value`, `timestamp`). |
| **-** | **Binder Boundary 1** | **IPC Crossing 1** | Pushes serialized AIDL parcelable across process boundary via `IVehicleCallback` into Java system server. |
| **3** | **PropertyHalService** | System Server (`com.android.car`) | Ingests `VehiclePropValue` from native HAL into Android CarService environment. |
| **4** | **CarPropertyService** | System Server (`com.android.car`) | Checks app permissions (e.g., `Car.PERMISSION_CONTROL_CAR_CLIMATE`), matches caller UID, and resolves multi-zone Area IDs. |
| **-** | **Binder Boundary 2** | **IPC Crossing 2** | Dispatches event via `ICarPropertyEventListener` interface across process boundary into application sandbox. |
| **5** | **CarPropertyManager** | Kotlin Application (`android.car.hardware.property`) | Converts Binder parcel into strongly-typed `CarPropertyValue<T>` for consumption by Kotlin ViewModel / Compose UI. |

---

## 3. Byte-to-Kotlin Signal Transformation

Understanding how raw physical bytes become high-level Kotlin objects is critical for automotive software engineering:

```text
[Raw Physical Bytes on CAN Bus]
0x1A 0x04 0x82 0x41 0xC0 0x00 0x00
         |
         v  (Stage 1-2: Gateway MCU & Native VHAL AIDL)
[AIDL Struct: VehiclePropValue]
  prop      = 0x11600503 (HVAC_TEMPERATURE_SET)
  areaId    = 0x00000001 (SEAT_1_LEFT / Driver Zone)
  timestamp = 1715678901234
  value     = FloatValues[ 24.0f ]
         |
         v  (Stage 3-4: CarService Binder IPC & Permission Check)
[CarPropertyValue<Float>]
  propertyId = 358614275 (HVAC_TEMPERATURE_SET)
  areaId     = 1
  status     = STATUS_AVAILABLE (0)
  timestamp  = 1715678901234
  value      = 24.0f
         |
         v  (Stage 5: Kotlin App Callback)
Log.d("VHAL", "Zone 1 Set: 24.0 deg C")
```

---

## 4. Kotlin Code Implementation

The snippet below demonstrates clean, leak-free registration with `CarPropertyManager` for an `ON_CHANGE` property split by Area IDs (e.g., `HVAC_TEMPERATURE_SET`):

```kotlin
package com.hackathon.v2x.ivi.vhal

import android.car.Car
import android.car.hardware.CarPropertyValue
import android.car.hardware.property.CarPropertyManager
import android.car.VehiclePropertyIds
import android.content.Context
import android.util.Log

class VehiclePropertyObserver(private val context: Context) {

    private var car: Car? = null
    private var propMgr: CarPropertyManager? = null

    private val hvacCallback = object : CarPropertyManager.CarPropertyEventCallback {
        override fun onChangeEvent(value: CarPropertyValue<*>) {
            // 1. Guard against unavailable sensor status
            if (value.status != CarPropertyValue.STATUS_AVAILABLE) {
                Log.w(TAG, "Property ${value.propertyId} unavailable")
                return
            }

            // 2. Filter property ID and extract strongly-typed value
            when (value.propertyId) {
                VehiclePropertyIds.HVAC_TEMPERATURE_SET -> {
                    val tempC = value.value as? Float ?: return
                    val areaId = value.areaId
                    Log.d(TAG, "[VHAL RX] Zone $areaId Temp Set: $tempC deg C (ts=${value.timestamp})")
                }
                VehiclePropertyIds.PERF_VEHICLE_SPEED -> {
                    val speedMs = value.value as? Float ?: return
                    val speedKmh = speedMs * 3.6f
                    Log.d(TAG, "[VHAL RX] Vehicle Speed: $speedKmh km/h")
                }
            }
        }

        override fun onErrorEvent(propId: Int, areaId: Int) {
            Log.e(TAG, "[VHAL ERR] Property Error: propId=$propId areaId=$areaId")
        }
    }

    fun startListening() {
        car = Car.createCar(context.applicationContext)
        propMgr = car?.getCarManager(Car.CAR_PROPERTY_SERVICE) as? CarPropertyManager

        // Register callback for HVAC Temperature (requires Car.PERMISSION_CONTROL_CAR_CLIMATE)
        propMgr?.registerCallback(
            hvacCallback,
            VehiclePropertyIds.HVAC_TEMPERATURE_SET,
            CarPropertyManager.SENSOR_RATE_ONCHANGE
        )

        // Register callback for Vehicle Speed (requires Car.PERMISSION_SPEED)
        propMgr?.registerCallback(
            hvacCallback,
            VehiclePropertyIds.PERF_VEHICLE_SPEED,
            CarPropertyManager.SENSOR_RATE_UI
        )
    }

    fun stopListening() {
        // Always unregister callbacks to prevent Binder listener memory leaks
        propMgr?.unregisterCallback(hvacCallback)
        car?.disconnect()
        propMgr = null
        car = null
    }

    companion object {
        private const val TAG = "VHAL_Observer"
    }
}
```

> [!IMPORTANT]
> **Required Android Manifest Permissions:**
> ```xml
> <!-- Climate Control Permission -->
> <uses-permission android:name="android.car.permission.CONTROL_CAR_CLIMATE" />
> <!-- Vehicle Speed Permission -->
> <uses-permission android:name="android.car.permission.CAR_SPEED" />
> ```

---

## 5. VHAL IPC vs. Direct UDP Safety Warnings (Our Project Architecture)

In our AAOS `IVI_ECU` head unit, two communication paradigms co-exist, serving different automotive requirements:

```text
+-----------------------------------------------------------------------------+
|                       AAOS Head Unit (IVI ECU Guest VM)                     |
|                                                                             |
|  [Path A: VHAL Property Routing (Binder)]  | [Path B: Direct UDP Warning]   |
|  * HVAC, Speed, Fuel, Gear                 | * NLOS Collision Warning (R4)  |
|  * IPC: 2 Binder Boundaries                | * IPC: Direct Socket (No Binder)
|  * Latency: ~10-20 ms                      | * Latency: < 2 ms              |
|  * Rate: On-Change / Polling (1-10 Hz)     | * Rate: 10-100 Hz Real-Time    |
|  * Contract: CarPropertyValue<T>           | * Contract: R4WarningEvent JSON|
+-----------------------------------------------------------------------------+
```

| Criterion | VHAL Property Routing (Path A) | Direct UDP Socket Pipeline (Path B - Our R4 Implementation) |
|---|---|---|
| **Use Case** | Vehicle State (HVAC, Speed, Fuel, Doors) | Ultra-Low Latency Safety Warnings (NLOS Collision Alert) |
| **Transport** | Binder IPC via CarService | Direct UDP Socket (`BuildConfig.R4_UDP_PORT` = 47300) |
| **Hops & Boundaries** | 5 Hops across 2 Binder IPC Boundaries | Direct 1-hop socket read inside `R4ListenerService` |
| **Latency Budget** | ~10-20 ms (Acceptable for telemetry) | **< 2 ms** (Critical for stopping distance at 60 km/h) |
| **Data Format** | AIDL `VehiclePropValue` / `CarPropertyValue<T>` | `ByteArray` -> `R4Json` -> `R4WarningEvent` (Kotlin) |
| **Independence** | Bound to Android `CarService` AIDL schema | **Transport Agnostic** (Seams decouple UDP from Compose UI) |

---

### 5.1 IVI App Internal Ingest Protocol Pipeline (R4 Datagram to Compose UI)

In our actual `IVI_ECU` application, raw UDP datagram packets arriving from ADA ECU on port 47300 cross 5 internal architectural layers to update the driver HMI:

```text
[ADA ECU (10.99.0.12)]
       |
       | UDP Port 47300 (Raw R4 JSON Datagram Byte Array)
       v
[1. R4ListenerService.kt] (ForegroundService on Dispatchers.IO)
       | socket.receive(packet) -> ByteArray
       v
[2. R4Deserializer.kt] (kotlinx.serialization)
       | deserialize(bytes) -> R4WarningEvent Kotlin Object
       v
[3. R4Repository.kt] (Single Source of Truth)
       | warningEvents.emit(event) -> SharedFlow<R4WarningEvent>
       v
[4. WarningViewModel.kt] (State Machine & Timer)
       | uiWarningState = Active(event) + 10s Auto-Clear Countdown Job
       v
[5. MainScreen.kt & Canvas3DWarningView.kt] (Jetpack Compose UI)
       | Render 3D Cybertruck God-View + WarningBannerOverlay (Amber/Red)
```

#### Step-by-Step Code Flow in IVI_ECU:

1. **Network Ingress (`R4ListenerService.kt`):**
   ```kotlin
   // Opens UDP socket on port 47300 in a background coroutine
   val buffer = ByteArray(BUFFER_SIZE)
   val packet = DatagramPacket(buffer, buffer.size)
   DatagramSocket(BuildConfig.R4_UDP_PORT).use { socket ->
       while (isActive) {
           packet.setLength(buffer.size)
           socket.receive(packet) // Hứng byte thô từ L4
           val bytes = packet.data.copyOf(packet.length)
           deserializer.deserialize(bytes).onSuccess { msg ->
               _r4EventFlow.emit(msg) // Đẩy bản tin lên SharedFlow
           }
       }
   }
   ```

2. **JSON Deserialization (`R4Deserializer.kt`):**
   ```kotlin
   // Converts raw byte array -> UTF-8 String -> R4WarningEvent object
   fun deserialize(bytes: ByteArray): Result<R4Message> = runCatching {
       val raw = bytes.decodeToString()
       R4Json.decodeFromString<R4WarningEvent>(raw)
   }
   ```

3. **Central Repository Storage (`R4Repository.kt`):**
   ```kotlin
   // Routes validated events to subscribers
   private val _warningEvents = MutableSharedFlow<R4WarningEvent>(extraBufferCapacity = 32)
   val warningEvents: SharedFlow<R4WarningEvent> = _warningEvents.asSharedFlow()
   ```

4. **UI ViewModel & Auto-Clear Logic (`WarningViewModel.kt`):**
   ```kotlin
   // Receives warning, updates state, and schedules 10s auto-dismiss job
   private fun onWarningReceived(event: R4WarningEvent) {
       _uiWarningState.value = WarningUiState.Active(event)
       scheduleAutoClear() // Resets 10-second timer
   }
   ```

5. **3D God View Render (`MainScreen.kt` & `Canvas3DWarningView.kt`):**
   ```kotlin
   // Jetpack Compose renders 3D Cybertruck + Warning Overlay based on riskState
   WarningBannerOverlay(
       warningActive = uiState is WarningUiState.Active,
       riskState = event.riskState // "medium" (Amber) or "high" (Red)
   )
   ```

---

## 6. AOSP Source Code Grounding

For further exploration of VHAL AIDL definitions in AOSP:
- Native AIDL Interface: `hardware/interfaces/automotive/vehicle/aidl/android/hardware/automotive/vehicle/`
- Vehicle Property IDs: `packages/services/Car/car-lib/src/android/car/VehiclePropertyIds.java`
- CarPropertyService: `packages/services/Car/service/src/com/android/car/CarPropertyService.java`
