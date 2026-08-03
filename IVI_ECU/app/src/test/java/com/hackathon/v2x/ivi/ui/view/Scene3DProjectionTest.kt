package com.hackathon.v2x.ivi.ui.view

import com.hackathon.v2x.ivi.model.VehiclePosition
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class Scene3DProjectionTest {

    @Test
    fun project_egoOrigin_anchorsNearBottomCenter() {
        val egoPoint = Point3D(0f, 0f, 0f)
        val canvasWidth = 1000f
        val canvasHeight = 600f

        val proj = Scene3DProjection.project(
            point = egoPoint,
            canvasWidthPx = canvasWidth,
            canvasHeightPx = canvasHeight,
        )

        assertEquals(500f, proj.screenX, 0.01f)
        assertEquals(480f, proj.screenY, 0.01f)
        assertEquals(1.0f, proj.scale, 0.01f)
    }

    @Test
    fun project_forwardVehicle_movesUpwardsAndShrinksScale() {
        val egoPoint = Point3D(0f, 0f, 0f)
        val forwardPoint = Point3D(20f, 0f, 0f)

        val canvasWidth = 1000f
        val canvasHeight = 600f

        val egoProj = Scene3DProjection.project(egoPoint, canvasWidth, canvasHeight)
        val fwdProj = Scene3DProjection.project(forwardPoint, canvasWidth, canvasHeight)

        assertTrue(fwdProj.screenY < egoProj.screenY)
        assertTrue(fwdProj.scale < egoProj.scale)
    }

    @Test
    fun buildVehicleBox_returnsEightCorners() {
        val pos = VehiclePosition(10f, 2f)
        val box = Scene3DProjection.buildVehicleBox(
            position = pos,
            canvasWidthPx = 1000f,
            canvasHeightPx = 600f,
        )

        assertEquals(4, box.bottomCorners.size)
        assertEquals(4, box.topCorners.size)
        assertTrue(box.centerTop.screenY < box.centerBase.screenY)
    }
}
