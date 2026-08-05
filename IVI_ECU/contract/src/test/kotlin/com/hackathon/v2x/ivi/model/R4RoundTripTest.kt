package com.hackathon.v2x.ivi.model

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Round-trip test of the frozen R4 sealed binding (4.0.1.5) and the finalized
 * R3 snapshot model (3.0.1.4) against the shared contract samples: each sample
 * must decode -> encode -> decode to an equal object. Encoding goes through the
 * sealed base serializer [R4Message.serializer] so the `type` discriminator is
 * always emitted on the wire.
 */
class R4RoundTripTest {

    private fun readResource(path: String): String =
        checkNotNull(javaClass.getResourceAsStream(path)) { "missing test resource: $path" }
            .bufferedReader()
            .readText()

    @Test
    fun warningSampleRoundTripsThroughSealedBinding() {
        val text = readResource("/contracts/samples/r4-warning.json")

        val first: R4Message = R4Json.decodeFromString(R4Message.serializer(), text)
        assertTrue("expected R4WarningEvent, got ${first::class}", first is R4WarningEvent)
        val warning = first as R4WarningEvent
        assertEquals(R4WarningEvent.WARNING_TYPE_NLOS_OBSTRUCTION, warning.warningType)
        assertEquals(R3Snapshot.SOURCE_V2X_RELAYED, warning.objectSnapshot.source)
        assertEquals("vehicle", warning.objectSnapshot.objectClass)
        assertNotNull(warning.geometry.vehicleC)

        val encoded = R4Json.encodeToString(R4Message.serializer(), first)
        val second: R4Message = R4Json.decodeFromString(R4Message.serializer(), encoded)
        assertEquals(first, second)
    }

    @Test
    fun stateSampleRoundTripsThroughSealedBinding() {
        val text = readResource("/contracts/samples/r4-state.json")

        val first: R4Message = R4Json.decodeFromString(R4Message.serializer(), text)
        assertTrue("expected R4StateMessage, got ${first::class}", first is R4StateMessage)
        val state = first as R4StateMessage
        assertEquals(42L, state.seq)
        assertNotNull(state.vehicles.vehicleC)

        val encoded = R4Json.encodeToString(R4Message.serializer(), first)
        val second: R4Message = R4Json.decodeFromString(R4Message.serializer(), encoded)
        assertEquals(first, second)
    }

    @Test
    fun r3SampleRoundTripsThroughFinalizedSnapshotModel() {
        val text = readResource("/contracts/samples/r3-tracked-object.json")

        val first = R4Json.decodeFromString(R3Snapshot.serializer(), text)
        assertEquals("vehicle", first.objectClass)
        assertEquals(1789000000073L, first.timestamps.measured)

        val encoded = R4Json.encodeToString(R3Snapshot.serializer(), first)
        val second = R4Json.decodeFromString(R3Snapshot.serializer(), encoded)
        assertEquals(first, second)
    }
}
