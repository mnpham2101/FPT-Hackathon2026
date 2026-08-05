package com.hackathon.v2x.ivi.model

import kotlinx.serialization.Serializable

/**
 * Ego-relative position of one vehicle, in meters.
 *
 * @property x longitudinal offset — positive is forward of ego
 * @property y lateral offset — positive is right of ego
 */
@Serializable
data class VehiclePosition(
    val x: Float,
    val y: Float,
)

/**
 * Relative geometry of the cooperative-awareness scene, as delivered by the
 * R4 warning message.
 *
 * @property ego vehicle A — always the frame origin `(0, 0)` by definition
 * @property vehicleB the occluder, relative to ego
 * @property vehicleC the occluded vehicle relayed over V2X; null until C
 *   is first tracked — renderers must accept this without crashing
 */
@Serializable
data class SceneGeometry(
    val ego: VehiclePosition,
    val vehicleB: VehiclePosition,
    val vehicleC: VehiclePosition? = null,
    /** R3 snapshot backing vehicleC, used by the renderer's defensive source guard (17.5.3.4). */
    val vehicleCSnapshot: R3Snapshot? = null,
)

