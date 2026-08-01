package com.hackathon.v2x.ivi.ui.view

import com.hackathon.v2x.ivi.model.SceneGeometry
import com.hackathon.v2x.ivi.model.VehiclePosition
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Pure-Kotlin coordinate mapping tests (17.5.3.2 / 17.5.4.2).
 */
class SceneCoordinateMapperTest {

    private val width = 800f
    private val height = 600f
    private val radius = 20f

    @Test
    fun egoAtOrigin_mapsToCenterBottomAnchor() {
        val mapped = SceneCoordinateMapper.mapVehicle(
            position = VehiclePosition(0f, 0f),
            canvasWidthPx = width,
            canvasHeightPx = height,
            radiusPx = radius,
        )
        val anchor = SceneCoordinateMapper.egoAnchor(width, height)
        assertEquals(anchor.x, mapped.offset.x, 0.01f)
        assertEquals(anchor.y, mapped.offset.y, 0.01f)
        assertEquals(width / 2f, anchor.x, 0.01f)
        assertEquals(height * SceneCoordinateMapper.EGO_ANCHOR_Y_FRACTION, anchor.y, 0.01f)
    }

    @Test
    fun vehicleForward20m_maps40pxAboveEgo() {
        val ego = SceneCoordinateMapper.egoAnchor(width, height)
        val mapped = SceneCoordinateMapper.mapVehicle(
            position = VehiclePosition(x = 20f, y = 0f),
            canvasWidthPx = width,
            canvasHeightPx = height,
            radiusPx = radius,
            scaleMetersPerPixel = 0.5f,
        )
        // Forward = smaller canvas Y: 20 / 0.5 = 40 px above ego.
        assertEquals(ego.x, mapped.offset.x, 0.01f)
        assertEquals(ego.y - 40f, mapped.offset.y, 0.01f)
    }

    @Test
    fun vehicleRight5m_maps10pxRightOfEgo() {
        val ego = SceneCoordinateMapper.egoAnchor(width, height)
        val mapped = SceneCoordinateMapper.mapVehicle(
            position = VehiclePosition(x = 0f, y = 5f),
            canvasWidthPx = width,
            canvasHeightPx = height,
            radiusPx = radius,
            scaleMetersPerPixel = 0.5f,
        )
        assertEquals(ego.x + 10f, mapped.offset.x, 0.01f)
        assertEquals(ego.y, mapped.offset.y, 0.01f)
    }

    @Test
    fun extremeForwardPosition_clampsToTopEdgeMargin() {
        val mapped = SceneCoordinateMapper.mapVehicle(
            position = VehiclePosition(x = 500f, y = 0f),
            canvasWidthPx = width,
            canvasHeightPx = height,
            radiusPx = radius,
            scaleMetersPerPixel = 0.5f,
        )
        assertEquals(SceneCoordinateMapper.EDGE_MARGIN_PX, mapped.offset.y, 0.01f)
        assertTrue(mapped.offset.x >= SceneCoordinateMapper.EDGE_MARGIN_PX)
        assertTrue(mapped.offset.x <= width - SceneCoordinateMapper.EDGE_MARGIN_PX)
    }

    @Test
    fun mapScene_nullVehicleC_leavesRenderCNull() {
        val scene = SceneGeometry(
            ego = VehiclePosition(0f, 0f),
            vehicleB = VehiclePosition(15f, 0f),
            vehicleC = null,
        )
        val render = SceneCoordinateMapper.mapScene(
            scene = scene,
            canvasWidthPx = width,
            canvasHeightPx = height,
            egoRadiusPx = 24f,
            vehicleBRadiusPx = 20f,
            vehicleCRadiusPx = 20f,
        )
        assertNull(render.vehicleC)
        assertEquals(24f, render.ego.radiusPx, 0.01f)
    }
}
