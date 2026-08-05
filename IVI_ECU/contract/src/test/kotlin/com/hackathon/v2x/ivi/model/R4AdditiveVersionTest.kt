package com.hackathon.v2x.ivi.model

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Additive-version test of the frozen R4 sealed binding (4.0.6.1) against the
 * shared D4 fixture `r4-unknown-warning.json`: a `schemaVersion: 2` warning
 * with an unknown `warningType` and an unknown extra field must parse
 * leniently through [R4Json] (`ignoreUnknownKeys`) into a fully-usable
 * [R4WarningEvent] — never a parse failure (HLD D4). Binding-level only:
 * Phase 5's `R4Deserializer` (4.5.1.2) builds on this contract.
 */
class R4AdditiveVersionTest {

    private fun readResource(path: String): String =
        checkNotNull(javaClass.getResourceAsStream(path)) { "missing test resource: $path" }
            .bufferedReader()
            .readText()

    @Test
    fun unknownWarningFixtureParsesLenientlyAsWarningEvent() {
        val text = readResource("/contracts/samples/r4-unknown-warning.json")

        val parsed: R4Message = R4Json.decodeFromString(R4Message.serializer(), text)
        assertTrue("expected R4WarningEvent, got ${parsed::class}", parsed is R4WarningEvent)
    }

    @Test
    fun unknownWarningTypeClassifiesAsGenericUnknownWarning() {
        val text = readResource("/contracts/samples/r4-unknown-warning.json")

        val warning = R4Json.decodeFromString(R4Message.serializer(), text) as R4WarningEvent
        assertEquals("slippery_road", warning.warningType)
        assertNotEquals(R4WarningEvent.WARNING_TYPE_NLOS_OBSTRUCTION, warning.warningType)
    }

    @Test
    fun futureVersionAndUnknownFieldTolerated() {
        val text = readResource("/contracts/samples/r4-unknown-warning.json")

        val first: R4Message = R4Json.decodeFromString(R4Message.serializer(), text)
        val warning = first as R4WarningEvent
        assertEquals(2, warning.schemaVersion)
        assertEquals(R3Snapshot.SOURCE_V2X_RELAYED, warning.objectSnapshot.source)
        assertNotNull(warning.geometry.vehicleC)

        // Unknown `hazardDetail` is dropped on re-emit: additive tolerance, not preservation.
        val encoded = R4Json.encodeToString(R4Message.serializer(), first)
        val second: R4Message = R4Json.decodeFromString(R4Message.serializer(), encoded)
        assertEquals(first, second)
    }
}
