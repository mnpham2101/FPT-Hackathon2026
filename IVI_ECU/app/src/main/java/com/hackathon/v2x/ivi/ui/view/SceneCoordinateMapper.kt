package com.hackathon.v2x.ivi.ui.view

import com.hackathon.v2x.ivi.model.SceneGeometry
import com.hackathon.v2x.ivi.model.VehiclePosition

/**
 * Canvas position in pixels. Deliberately not Compose's `Offset` so this
 * layer stays free of Android/Compose UI types and fully unit-testable.
 */
data class PixelOffset(val x: Float, val y: Float)

/** Everything the renderer needs to draw one vehicle marker. */
data class VehicleRenderData(val offset: PixelOffset, val radiusPx: Float)

/** Mapped render data for the whole scene; `vehicleC` is null while C is untracked. */
data class SceneRenderData(
    val ego: VehicleRenderData,
    val vehicleB: VehicleRenderData,
    val vehicleC: VehicleRenderData?,
)

/**
 * Pure-Kotlin math layer converting ego-frame meters into canvas pixels.
 * Zero `android.*` / `androidx.*` imports — no drawing happens here.
 *
 * Frame conventions:
 * - ego-frame: `x` longitudinal, positive forward; `y` lateral, positive right (meters)
 * - canvas: origin top-left; forward maps to *up* (smaller canvas y), right maps to right
 * - ego is always anchored at `(canvasWidth / 2, canvasHeight * 0.75)` — center, bottom third
 */
object SceneCoordinateMapper {

    /** Default zoom: how many meters one canvas pixel represents. */
    const val DEFAULT_SCALE_METERS_PER_PIXEL = 0.5f

    /** Vehicle centers never map closer to a canvas edge than this. */
    const val EDGE_MARGIN_PX = 16f

    /** Ego anchor sits at this fraction of the canvas height (center-bottom third). */
    const val EGO_ANCHOR_Y_FRACTION = 0.75f

    /** The fixed canvas anchor of the ego vehicle. */
    fun egoAnchor(canvasWidthPx: Float, canvasHeightPx: Float): PixelOffset =
        PixelOffset(canvasWidthPx / 2f, canvasHeightPx * EGO_ANCHOR_Y_FRACTION)

    /**
     * Maps one ego-relative position (meters) to a clamped canvas offset.
     * Ego itself is `(0, 0)` and therefore maps exactly onto [egoAnchor].
     */
    fun mapVehicle(
        position: VehiclePosition,
        canvasWidthPx: Float,
        canvasHeightPx: Float,
        radiusPx: Float,
        scaleMetersPerPixel: Float = DEFAULT_SCALE_METERS_PER_PIXEL,
    ): VehicleRenderData {
        val anchor = egoAnchor(canvasWidthPx, canvasHeightPx)
        val rawX = anchor.x + position.y / scaleMetersPerPixel
        val rawY = anchor.y - position.x / scaleMetersPerPixel
        val clamped = PixelOffset(
            x = rawX.coerceIn(EDGE_MARGIN_PX, canvasWidthPx - EDGE_MARGIN_PX),
            y = rawY.coerceIn(EDGE_MARGIN_PX, canvasHeightPx - EDGE_MARGIN_PX),
        )
        return VehicleRenderData(offset = clamped, radiusPx = radiusPx)
    }

    /** Maps a whole [SceneGeometry] in one call; radii are supplied by the renderer's styling. */
    fun mapScene(
        scene: SceneGeometry,
        canvasWidthPx: Float,
        canvasHeightPx: Float,
        egoRadiusPx: Float,
        vehicleBRadiusPx: Float,
        vehicleCRadiusPx: Float,
        scaleMetersPerPixel: Float = DEFAULT_SCALE_METERS_PER_PIXEL,
    ): SceneRenderData = SceneRenderData(
        ego = mapVehicle(scene.ego, canvasWidthPx, canvasHeightPx, egoRadiusPx, scaleMetersPerPixel),
        vehicleB = mapVehicle(scene.vehicleB, canvasWidthPx, canvasHeightPx, vehicleBRadiusPx, scaleMetersPerPixel),
        vehicleC = scene.vehicleC?.let {
            mapVehicle(it, canvasWidthPx, canvasHeightPx, vehicleCRadiusPx, scaleMetersPerPixel)
        },
    )
}
