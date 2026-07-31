package com.hackathon.v2x.ivi.model

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json

/**
 * Sealed Kotlin binding of the frozen R4 ADA-to-IVI message set (4.0.1.5,
 * `contracts/r4-ada-ivi.schema.json`): an [R4WarningEvent] or an
 * [R4StateMessage], discriminated on the wire by `"type": "warning" | "state"`
 * (see [R4Json]).
 */
@Serializable
sealed class R4Message {
    abstract val schemaVersion: Int
}

/**
 * Edge-triggered warning event: carries the R3 snapshot of the triggering
 * track (frozen R3 schema 3.0.1.4, `$ref` from the R4 schema) and the composed
 * [SceneGeometry].
 */
@Serializable
@SerialName("warning")
data class R4WarningEvent(
    override val schemaVersion: Int,
    val warningType: String,
    val riskState: String,
    @SerialName("object") val objectSnapshot: R3Snapshot,
    val geometry: SceneGeometry,
) : R4Message() {

    val trackedObject: R3Snapshot get() = objectSnapshot

    companion object {
        const val TYPE_KEY = "warning"
        const val WARNING_TYPE_NLOS_OBSTRUCTION = "nlos_obstruction"
        const val UNKNOWN_WARNING_TYPE = "unknown"
    }
}

/** Periodic awareness state (R15), last-value-wins by [seq]. */
@Serializable
@SerialName("state")
data class R4StateMessage(
    override val schemaVersion: Int,
    val seq: Long,
    val vehicles: R4Vehicles,
) : R4Message() {
    companion object {
        const val TYPE_KEY = "state"
    }
}

/** Minimal vehicle state carried inside the R4 state heartbeat. */
@Serializable
data class VehicleState(
    @SerialName("position") val position: VehiclePosition,
    @SerialName("speed") val speed: Float,
)

/** Vehicle positions of the state message; [vehicleC] absent-or-null until C is relayed. */
@Serializable
data class R4Vehicles(
    val ego: VehiclePosition,
    val vehicleB: VehiclePosition,
    val vehicleC: VehiclePosition? = null,
)

/** Binding-level Json for the R4 wire: "type" discriminates warning|state; unknown fields are ignored (additive evolution, HLD D4). */
val R4Json: Json = Json {
    classDiscriminator = "type"
    ignoreUnknownKeys = true
}

