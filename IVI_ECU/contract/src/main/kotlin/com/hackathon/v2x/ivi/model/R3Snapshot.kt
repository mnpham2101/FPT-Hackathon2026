package com.hackathon.v2x.ivi.model

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable

/**
 * Model of the R3 TrackedObject snapshot carried inside an R4 warning event,
 * finalized against the frozen R3 schema (3.0.1.4,
 * `contracts/r3-tracked-object.schema.json`) by subtask 4.0.6.1 — which
 * supersedes 4.5.1.1; field names are unchanged from the interim model.
 *
 * @property objectClass the schema's `class` field (Kotlin-safe name); M1: `"vehicle"`
 * @property source provenance of the track — the Warning View renders Ghost C
 *   only for [SOURCE_V2X_RELAYED]; anything else trips the defensive guard
 *   (17.5.3.4)
 */
@Serializable
data class R3Snapshot(
    val id: String,
    @SerialName("class") val objectClass: String = "vehicle",
    val source: String,
    val position: VehiclePosition,
    val distance: Float = 0f,
    val speed: Float = 0f,
    val confidence: Float = 1.0f,
    val state: String = "tracked",
    val timestamps: R3Timestamps = R3Timestamps(),
) {
    companion object {
        const val SOURCE_V2X_RELAYED = "v2x_relayed"
        const val SOURCE_OWN_SENSOR = "own_sensor"
    }
}

/** Per-track timestamps of the frozen R3 schema (3.0.1.4), ms since epoch. */
@Serializable
data class R3Timestamps(
    val measured: Long = 0L,
    val received: Long = 0L,
    val lastUpdated: Long = 0L,
)

