package com.hackathon.v2x.ivi.model

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json

private val R4Json = Json {
    ignoreUnknownKeys = true
}

@Serializable
data class R4WarningMessage(
    val schemaVersion: Int,
    val type: String,
    val warningType: String,
    val riskState: String,
    @SerialName("object")
    val triggeringObject: R3Snapshot,
    val trackedObjects: List<R3Snapshot> = emptyList(),
    val geometry: R4Geometry,
) {
    fun toSceneGeometry(): SceneGeometry {
        val ghostSnapshot = trackedObjects.lastOrNull { it.source == R3Snapshot.SOURCE_V2X_RELAYED }
            ?: triggeringObject.takeIf { it.source == R3Snapshot.SOURCE_V2X_RELAYED }

        return SceneGeometry(
            ego = geometry.ego,
            vehicleB = geometry.b,
            vehicleC = geometry.c,
            vehicleCSnapshot = ghostSnapshot,
        )
    }

    companion object {
        fun decode(payload: String): R4WarningMessage = R4Json.decodeFromString(payload)
    }
}

@Serializable
data class R4Geometry(
    val ego: VehiclePosition,
    val b: VehiclePosition,
    val c: VehiclePosition? = null,
)
