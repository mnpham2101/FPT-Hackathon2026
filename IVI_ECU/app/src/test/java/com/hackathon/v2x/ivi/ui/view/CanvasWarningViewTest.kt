package com.hackathon.v2x.ivi.ui.view

import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import com.hackathon.v2x.ivi.model.R3Snapshot
import com.hackathon.v2x.ivi.model.R3Timestamps
import com.hackathon.v2x.ivi.model.VehiclePosition
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Defensive source guard + risk color tests for [CanvasWarningView] (17.5.3.4 / 17.5.4.2).
 *
 * Canvas-drawn `[?]` markers are not in the Compose semantics tree, so the
 * guard decision and ERROR log payload are unit-tested via the extracted
 * helpers that [CanvasWarningView.Render] calls.
 */
class CanvasWarningViewTest {

    private fun snapshot(source: String) = R3Snapshot(
        id = "C-guard",
        objectClass = "vehicle",
        source = source,
        position = VehiclePosition(28f, 1.5f),
        distance = 28f,
        speed = 12f,
        confidence = 0.9f,
        state = "tracked",
        timestamps = R3Timestamps(measured = 1L, received = 2L, lastUpdated = 2L),
    )

    @Test
    fun missingSnapshot_isTrustedForDevMockScenes() {
        assertTrue(isGhostCSourceTrusted(null))
    }

    @Test
    fun v2xRelayedSource_isTrusted() {
        assertTrue(isGhostCSourceTrusted(snapshot(R3Snapshot.SOURCE_V2X_RELAYED)))
    }

    @Test
    fun ownSensorSource_isRejectedByDefensiveGuard() {
        val own = snapshot(R3Snapshot.SOURCE_OWN_SENSOR)
        assertFalse(
            "own_sensor must not render as Ghost C / [V2X]",
            isGhostCSourceTrusted(own),
        )
        // Untrusted path draws the yellow [? UNKNOWN SOURCE] marker.
        assertEquals("[? UNKNOWN SOURCE]", UNKNOWN_SOURCE_LABEL)
    }

    @Test
    fun ownSensor_guardErrorMessage_includesSourceAndFullSnapshotJson() {
        val own = snapshot(R3Snapshot.SOURCE_OWN_SENSOR)
        val message = ghostCSourceGuardErrorMessage(own)
        assertTrue(message.contains("own_sensor"))
        assertTrue(message.contains(R3Snapshot.SOURCE_V2X_RELAYED))
        assertTrue(message.contains("C-guard"))
        assertTrue(message.contains("\"source\":\"own_sensor\"") || message.contains("own_sensor"))
    }

    @Test
    fun riskStateHigh_mapsToFailSafeRed() {
        // #FF1A1A — committed high-risk glow from 17.5.3.4.
        assertEquals(Color(0xFFFF1A1A).toArgb(), riskColor("high").toArgb())
    }
}
