package com.hackathon.v2x.ivi.model

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable

/**
 * Sealed hierarchy for the two R4 message variants sent by ADA ECU to IVI ECU.
 *
 * Subtask 4.5.1.1 — R4 contract:
 *   - [R4WarningEvent]: edge-triggered alert (type = "warning")
 *   - [R4StateMessage]: optional periodic state heartbeat (type = "state")
 *
 * Additive-version rule: unknown fields are ignored (lenient JSON parser in
 * R4Deserializer). Unknown [R4WarningEvent.warningType] values degrade to
 * [UNKNOWN_WARNING_TYPE] — never throw.
 */
sealed class R4Message {

    /** type = "warning" — edge-triggered warning event from ADA ECU. */
    @Serializable
    data class R4WarningEvent(
        @SerialName("schemaVersion") val schemaVersion: Int,
        @SerialName("type") val type: String,
        /** e.g. "nlos_obstruction". Unknown values degraded to [UNKNOWN_WARNING_TYPE]. */
        @SerialName("warningType") val warningType: String,
        /** "low" | "medium" | "high" */
        @SerialName("riskState") val riskState: String,
        @SerialName("object") val trackedObject: R3Snapshot,
        @SerialName("geometry") val geometry: SceneGeometry,
    ) : R4Message() {
        companion object {
            const val TYPE_KEY = "warning"
        }
    }

    /** type = "state" — optional periodic snapshot of all vehicle positions. */
    @Serializable
    data class R4StateMessage(
        @SerialName("schemaVersion") val schemaVersion: Int,
        @SerialName("type") val type: String,
        @SerialName("seq") val seq: Int,
        @SerialName("vehicles") val vehicles: Map<String, VehicleState>,
    ) : R4Message() {
        companion object {
            const val TYPE_KEY = "state"
        }
    }

    companion object {
        /** Sentinel warningType used when incoming value is unknown (additive-version safety). */
        const val UNKNOWN_WARNING_TYPE = "unknown"
    }
}

/** Minimal vehicle state carried inside the R4 state heartbeat. */
@Serializable
data class VehicleState(
    @SerialName("position") val position: VehiclePosition,
    @SerialName("speed") val speed: Float,
)
