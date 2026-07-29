package com.hackathon.v2x.ivi.model

import kotlinx.serialization.Serializable

/**
 * Interim model of the R3 TrackedObject snapshot carried inside an R4
 * warning event. Subtask 4.5.1.1 finalizes it against the frozen R3 schema
 * without changing these field names.
 *
 * @property source provenance of the track — the Warning View renders Ghost C
 *   only for [SOURCE_V2X_RELAYED]; anything else trips the defensive guard
 *   (17.5.3.4)
 */
@Serializable
data class R3Snapshot(
    val id: String,
    val source: String,
    val position: VehiclePosition,
    val distance: Float,
    val speed: Float,
    val confidence: Float,
    val state: String,
) {
    companion object {
        const val SOURCE_V2X_RELAYED = "v2x_relayed"
        const val SOURCE_OWN_SENSOR = "own_sensor"
    }
}
