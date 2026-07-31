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
 * Model for the view seam (17.5.3.1), finalized against the frozen R4 schema
 * (4.0.1.5, `contracts/r4-ada-ivi.schema.json`) by subtask 4.0.6.1 — which
 * supersedes 4.5.1.1; field names are unchanged, and the frozen geometry
 * carries the three positions only (no distance fields).
 *
 * @property ego vehicle A — always the frame origin `(0, 0)` by definition
 * @property vehicleB the occluder, relative to ego
 * @property vehicleC the occluded vehicle relayed over V2X; `null` until C
 *   is first tracked — renderers must accept this without crashing
 */
@Serializable
data class SceneGeometry(
    val ego: VehiclePosition,
    val vehicleB: VehiclePosition,
    val vehicleC: VehiclePosition?,
    /**
     * R3 snapshot backing [vehicleC], used by the renderer's defensive
     * source guard (17.5.3.4). `null` in dev scenes that carry no snapshot —
     * treated as trusted.
     */
    val vehicleCSnapshot: R3Snapshot? = null,
)
