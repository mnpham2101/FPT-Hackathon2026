package com.hackathon.v2x.ivi.ui.view

import com.hackathon.v2x.ivi.model.VehiclePosition
import kotlin.math.cos
import kotlin.math.sin

/**
 * 3D Coordinate in Ego-relative meters.
 * - [x]: longitudinal distance (positive = forward)
 * - [y]: lateral distance (positive = right)
 * - [z]: vertical height (positive = up)
 */
data class Point3D(val x: Float, val y: Float, val z: Float = 0f)

/**
 * Screen projected point in pixels.
 * - [screenX], [screenY]: pixel coordinates on Canvas
 * - [scale]: perspective scale factor (1.0 at origin, smaller farther away)
 * - [zDepth]: z-depth for sorting (larger = further from camera)
 */
data class ProjectedPoint3D(
    val screenX: Float,
    val screenY: Float,
    val scale: Float,
    val zDepth: Float,
)

/**
 * Extruded 3D Vehicle Bounding Box containing 8 projected corners.
 * Top 4 corners and Bottom 4 corners.
 */
data class Vehicle3DBox(
    val bottomCorners: List<ProjectedPoint3D>,
    val topCorners: List<ProjectedPoint3D>,
    val centerBase: ProjectedPoint3D,
    val centerTop: ProjectedPoint3D,
)

/**
 * Pure Kotlin 3D Projection Engine for IVI God View HUD.
 * Zero Android/Compose dependencies for fast unit testing.
 *
 * Implements isometric perspective projection mapping 3D space (x=forward, y=right, z=up)
 * onto 2D screen coordinates.
 */
object Scene3DProjection {

    const val DEFAULT_PITCH_DEG = 52f
    const val DEFAULT_METER_TO_PIXEL = 18f
    const val PERSPECTIVE_STRENGTH = 0.012f

    // Standard vehicle dimensions in meters
    const val VEHICLE_LENGTH_M = 4.5f
    const val VEHICLE_WIDTH_M = 2.0f
    const val VEHICLE_HEIGHT_M = 1.6f

    /**
     * Projects a 3D point in meters relative to Ego into 2D screen canvas pixels.
     */
    fun project(
        point: Point3D,
        canvasWidthPx: Float,
        canvasHeightPx: Float,
        pitchDeg: Float = DEFAULT_PITCH_DEG,
        meterToPixel: Float = DEFAULT_METER_TO_PIXEL,
        anchorYFraction: Float = 0.80f,
    ): ProjectedPoint3D {
        val pitchRad = Math.toRadians(pitchDeg.toDouble()).toFloat()
        val cosP = cos(pitchRad)
        val sinP = sin(pitchRad)

        // Anchor Ego at bottom third center
        val anchorX = canvasWidthPx / 2f
        val anchorY = canvasHeightPx * anchorYFraction

        // Perspective depth compression based on distance forward (x)
        val depthScale = 1.0f / (1.0f + point.x * PERSPECTIVE_STRENGTH)

        // Lateral displacement (y -> right)
        val projX = anchorX + (point.y * meterToPixel * depthScale)

        // Longitudinal displacement (x -> forward, z -> height)
        val distanceY = (point.x * cosP + point.z * sinP) * meterToPixel * depthScale
        val projY = anchorY - distanceY

        return ProjectedPoint3D(
            screenX = projX,
            screenY = projY,
            scale = depthScale,
            zDepth = point.x + point.z * 0.5f,
        )
    }

    /**
     * Builds an 8-vertex 3D Bounding Box for a vehicle at a given position.
     */
    fun buildVehicleBox(
        position: VehiclePosition,
        lengthM: Float = VEHICLE_LENGTH_M,
        widthM: Float = VEHICLE_WIDTH_M,
        heightM: Float = VEHICLE_HEIGHT_M,
        canvasWidthPx: Float,
        canvasHeightPx: Float,
        pitchDeg: Float = DEFAULT_PITCH_DEG,
        meterToPixel: Float = DEFAULT_METER_TO_PIXEL,
    ): Vehicle3DBox {
        val halfL = lengthM / 2f
        val halfW = widthM / 2f

        // Local offsets for 4 corners (Front-Left, Front-Right, Rear-Right, Rear-Left)
        val dxs = floatArrayOf(halfL, halfL, -halfL, -halfL)
        val dys = floatArrayOf(-halfW, halfW, halfW, -halfW)

        val bottomCorners = mutableListOf<ProjectedPoint3D>()
        val topCorners = mutableListOf<ProjectedPoint3D>()

        for (i in 0 until 4) {
            val px = position.x + dxs[i]
            val py = position.y + dys[i]

            bottomCorners.add(
                project(
                    point = Point3D(px, py, 0f),
                    canvasWidthPx = canvasWidthPx,
                    canvasHeightPx = canvasHeightPx,
                    pitchDeg = pitchDeg,
                    meterToPixel = meterToPixel,
                ),
            )

            topCorners.add(
                project(
                    point = Point3D(px, py, heightM),
                    canvasWidthPx = canvasWidthPx,
                    canvasHeightPx = canvasHeightPx,
                    pitchDeg = pitchDeg,
                    meterToPixel = meterToPixel,
                ),
            )
        }

        val centerBase = project(
            point = Point3D(position.x, position.y, 0f),
            canvasWidthPx = canvasWidthPx,
            canvasHeightPx = canvasHeightPx,
            pitchDeg = pitchDeg,
            meterToPixel = meterToPixel,
        )

        val centerTop = project(
            point = Point3D(position.x, position.y, heightM),
            canvasWidthPx = canvasWidthPx,
            canvasHeightPx = canvasHeightPx,
            pitchDeg = pitchDeg,
            meterToPixel = meterToPixel,
        )

        return Vehicle3DBox(
            bottomCorners = bottomCorners,
            topCorners = topCorners,
            centerBase = centerBase,
            centerTop = centerTop,
        )
    }
}
