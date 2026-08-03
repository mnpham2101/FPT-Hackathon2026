package com.hackathon.v2x.ivi.ui.view

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test
import java.util.Locale

/**
 * Unit tests for Cybertruck God View design tokens and helper functions.
 * All drawing helpers are pure functions or accessible internal vals —
 * tested without Compose / Android instrumentation.
 */
class CybertruckGodViewTest {

    // ── Design token sanity ──────────────────────────────────────────────────

    @Test
    fun cyberBackground_isNearBlackSteel() {
        // #0A0C10 — cold near-black, NOT pure black
        assertNotEquals(Color.Black, CyberBackground)
        assertEquals(Color(0xFF0A0C10), CyberBackground)
    }

    @Test
    fun cyberEgo_isHighContrastWhiteSteel() {
        // EGO colour is bright white-steel (#E8ECF0), visible on dark bg
        assertEquals(Color(0xFFE8ECF0), CyberEgo)
    }

    @Test
    fun cyberB_isDistinctFromEgo() {
        assertNotEquals(CyberEgo, CyberB)
    }

    @Test
    fun cyberGhostC_isDistinctFromBothEgoAndB() {
        assertNotEquals(CyberGhostC, CyberEgo)
        assertNotEquals(CyberGhostC, CyberB)
    }

    // ── cyberRiskColor mapping ───────────────────────────────────────────────

    @Test
    fun cyberRiskColor_low_returnsMutedGreen() {
        val c = cyberRiskColor("low")
        assertEquals(CyberRiskLow, c)
    }

    @Test
    fun cyberRiskColor_medium_returnsDarkAmber() {
        val c = cyberRiskColor("medium")
        assertEquals(CyberRiskMedium, c)
    }

    @Test
    fun cyberRiskColor_high_returnsMilitaryRed() {
        val c = cyberRiskColor("high")
        assertEquals(CyberRiskHigh, c)
    }

    @Test
    fun cyberRiskColor_unknown_failsSafeToHigh() {
        val c = cyberRiskColor("???")
        assertEquals(CyberRiskHigh, c)
    }

    @Test
    fun cyberRiskColor_caseInsensitive() {
        assertEquals(cyberRiskColor("LOW"),    cyberRiskColor("low"))
        assertEquals(cyberRiskColor("MEDIUM"), cyberRiskColor("medium"))
        assertEquals(cyberRiskColor("HIGH"),   cyberRiskColor("high"))
    }

    // ── riskColor (shared from CanvasWarningView) still works ────────────────

    @Test
    fun sharedRiskColor_lowReturnsLowRiskGlowColor() {
        val c = riskColor("low")
        assertNotEquals(c, riskColor("high"))
    }

    @Test
    fun sharedRiskColor_unknownFailsSafeToHigh() {
        assertEquals(riskColor("high"), riskColor("unknown_state"))
    }

    // ── Source guard ─────────────────────────────────────────────────────────

    @Test
    fun ghostCSourceGuard_nullSnapshot_trusted() {
        assert(isGhostCSourceTrusted(null))
    }

    @Test
    fun ghostCSourceGuard_v2xRelayedSnapshot_trusted() {
        val snap = com.hackathon.v2x.ivi.model.R3Snapshot(
            id = "test-001",
            source = com.hackathon.v2x.ivi.model.R3Snapshot.SOURCE_V2X_RELAYED,
            position = com.hackathon.v2x.ivi.model.VehiclePosition(10f, 0f),
        )
        assert(isGhostCSourceTrusted(snap))
    }

    @Test
    fun ghostCSourceGuard_directDetectionSnapshot_notTrusted() {
        val snap = com.hackathon.v2x.ivi.model.R3Snapshot(
            id = "test-002",
            source = "direct_detection",
            position = com.hackathon.v2x.ivi.model.VehiclePosition(10f, 0f),
        )
        assert(!isGhostCSourceTrusted(snap))
    }

    @Test
    fun ghostCSourceGuardErrorMessage_containsSourceAndV2xRelayed() {
        val snap = com.hackathon.v2x.ivi.model.R3Snapshot(
            id = "test-003",
            source = "camera",
            position = com.hackathon.v2x.ivi.model.VehiclePosition(10f, 0f),
        )
        val msg = ghostCSourceGuardErrorMessage(snap)
        assert(msg.contains("camera")) { "Error message must include actual source" }
        assert(msg.contains(com.hackathon.v2x.ivi.model.R3Snapshot.SOURCE_V2X_RELAYED)) {
            "Error message must reference expected source"
        }
    }

    // ── Scene3DProjection consistency (Cybertruck pitch is still 52°) ────────

    @Test
    fun projection_forwardVehicle_isAboveEgoOnScreen() {
        val ego     = Scene3DProjection.project(Point3D(0f, 0f, 0f), 1000f, 600f)
        val forward = Scene3DProjection.project(Point3D(15f, 0f, 0f), 1000f, 600f)
        assert(forward.screenY < ego.screenY) {
            "Vehicle ahead should project higher (smaller Y) than ego"
        }
    }

    @Test
    fun projection_rightVehicle_isRightOfEgoOnScreen() {
        val ego   = Scene3DProjection.project(Point3D(0f, 0f, 0f), 1000f, 600f)
        val right = Scene3DProjection.project(Point3D(0f, 5f, 0f), 1000f, 600f)
        assert(right.screenX > ego.screenX) {
            "Vehicle to the right should project further right (larger X)"
        }
    }

    @Test
    fun projection_raisedVehicle_isAboveSameGroundPointOnScreen() {
        val ground  = Scene3DProjection.project(Point3D(10f, 0f, 0f), 1000f, 600f)
        val raised  = Scene3DProjection.project(Point3D(10f, 0f, 2f), 1000f, 600f)
        assert(raised.screenY < ground.screenY) {
            "Raised vehicle (z>0) should project higher than its ground point"
        }
    }

    @Test
    fun vehicleBox_centerTopIsAboveCenterBase() {
        val pos = com.hackathon.v2x.ivi.model.VehiclePosition(12f, 0f)
        val box = Scene3DProjection.buildVehicleBox(pos, canvasWidthPx = 1000f, canvasHeightPx = 600f)
        assert(box.centerTop.screenY < box.centerBase.screenY) {
            "Box roof must render above box floor on screen"
        }
    }

    @Test
    fun vehicleBox_hasFourCornersEachFaceTopAndBottom() {
        val pos = com.hackathon.v2x.ivi.model.VehiclePosition(8f, 2f)
        val box = Scene3DProjection.buildVehicleBox(pos, canvasWidthPx = 1000f, canvasHeightPx = 600f)
        assertEquals("Must have exactly 4 bottom corners", 4, box.bottomCorners.size)
        assertEquals("Must have exactly 4 top corners",    4, box.topCorners.size)
    }
}
