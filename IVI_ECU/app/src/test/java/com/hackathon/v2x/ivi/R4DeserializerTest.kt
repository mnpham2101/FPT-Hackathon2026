package com.hackathon.v2x.ivi

import com.hackathon.v2x.ivi.data.MalformedR4PayloadException
import com.hackathon.v2x.ivi.data.R4Deserializer
import com.hackathon.v2x.ivi.data.UnknownMessageTypeException
import com.hackathon.v2x.ivi.model.R4Message
import com.hackathon.v2x.ivi.model.R4StateMessage
import com.hackathon.v2x.ivi.model.R4WarningEvent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

/**
 * Unit tests for [R4Deserializer] — covers all 5 acceptance criteria from subtask 4.5.1.2.
 */
class R4DeserializerTest {

    private lateinit var deserializer: R4Deserializer

    @Before
    fun setUp() {
        deserializer = R4Deserializer()
    }

    // ── Test 1: Valid warning event ───────────────────────────────────────────

    @Test
    fun `valid warning event deserializes to R4WarningEvent with all fields populated`() {
        val json = """
            {
              "schemaVersion": 1,
              "type": "warning",
              "warningType": "nlos_obstruction",
              "riskState": "high",
              "object": {
                "id": "C-001",
                "source": "v2x_relayed",
                "position": {"x": 25.0, "y": 1.5},
                "distance": 25.0,
                "speed": 13.5,
                "confidence": 0.87,
                "state": "tracked"
              },
              "geometry": {
                "ego":      {"x": 0.0, "y": 0.0},
                "vehicleB": {"x": 15.0, "y": 0.0},
                "vehicleC": {"x": 25.0, "y": 1.5}
              }
            }
        """.trimIndent()

        val result = deserializer.deserialize(json.toByteArray())

        assertTrue("Result should be success", result.isSuccess)
        val event = result.getOrThrow() as R4WarningEvent
        assertEquals(1, event.schemaVersion)
        assertEquals("nlos_obstruction", event.warningType)
        assertEquals("high", event.riskState)
        assertEquals("C-001", event.trackedObject.id)
        assertEquals("v2x_relayed", event.trackedObject.source)
        assertEquals(25.0f, event.geometry.vehicleC!!.x)
    }

    // ── Test 2: Valid state message ───────────────────────────────────────────

    @Test
    fun `valid state message deserializes to R4StateMessage with all fields populated`() {
        val json = """
            {
              "schemaVersion": 1,
              "type": "state",
              "seq": 42,
              "vehicles": {
                "ego": {"x": 0.0, "y": 0.0},
                "vehicleB": {"x": 15.0, "y": 0.0}
              }
            }
        """.trimIndent()

        val result = deserializer.deserialize(json.toByteArray())

        assertTrue(result.isSuccess)
        val state = result.getOrThrow() as R4StateMessage
        assertEquals(42L, state.seq)
        assertEquals(0.0f, state.vehicles.ego.x)
    }

    // ── Test 3: Unknown warningType → "unknown", no exception ────────────────

    @Test
    fun `unknown warningType is degraded to 'unknown' without exception`() {
        val json = """
            {
              "schemaVersion": 1,
              "type": "warning",
              "warningType": "future_unknown_type",
              "riskState": "medium",
              "object": {
                "id": "C-001", "source": "v2x_relayed",
                "position": {"x": 28.0, "y": 1.5},
                "distance": 28.0, "speed": 13.5, "confidence": 0.87, "state": "tracked"
              },
              "geometry": {
                "ego": {"x": 0.0, "y": 0.0},
                "vehicleB": {"x": 15.0, "y": 0.0},
                "vehicleC": {"x": 28.0, "y": 1.5}
              }
            }
        """.trimIndent()

        val result = deserializer.deserialize(json.toByteArray())

        assertTrue("Should succeed even with unknown warningType", result.isSuccess)
        val event = result.getOrThrow() as R4WarningEvent
        assertEquals(R4WarningEvent.UNKNOWN_WARNING_TYPE, event.warningType)
    }

    // ── Test 4: Malformed JSON → Result.failure, no crash ────────────────────

    @Test
    fun `malformed JSON returns Result failure without crash`() {
        val bad = "{ this is not valid json !!".toByteArray()

        val result = deserializer.deserialize(bad)

        assertTrue("Result should be failure for malformed JSON", result.isFailure)
        // No exception propagated — Result wraps it
    }

    // ── Test 5: Extra unknown JSON fields are ignored ─────────────────────────

    @Test
    fun `extra unknown JSON fields are ignored and remaining fields parsed correctly`() {
        val json = """
            {
              "schemaVersion": 1,
              "type": "warning",
              "warningType": "nlos_obstruction",
              "riskState": "low",
              "futureFieldNotInSchema": "some_value",
              "anotherNewField": 999,
              "object": {
                "id": "C-002", "source": "v2x_relayed",
                "position": {"x": 40.0, "y": 0.5},
                "distance": 40.0, "speed": 11.0, "confidence": 0.75, "state": "tracked",
                "extraObjectField": true
              },
              "geometry": {
                "ego": {"x": 0.0, "y": 0.0},
                "vehicleB": {"x": 12.0, "y": 0.0},
                "vehicleC": {"x": 40.0, "y": 0.5},
                "extraGeomField": "ignored"
              }
            }
        """.trimIndent()

        val result = deserializer.deserialize(json.toByteArray())

        assertTrue("Extra fields must not cause failure", result.isSuccess)
        val event = result.getOrThrow() as R4WarningEvent
        assertEquals("nlos_obstruction", event.warningType)
        assertEquals("low", event.riskState)
        assertEquals("C-002", event.trackedObject.id)
        assertEquals(40.0f, event.geometry.vehicleC!!.x)
    }
}

