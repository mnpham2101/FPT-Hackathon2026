package com.hackathon.v2x.ivi.sim

import com.hackathon.v2x.ivi.model.R4WarningEvent
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonPrimitive
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test

class MessageBuilderTest {

    @Test
    fun everyNonRawStepOfEveryCommittedScenarioDecodes() {
        listOf("scenarios/approach.json", "scenarios/degrade.json", "scenarios/state-stream.json")
            .forEach { path ->
                ScenarioLoader.load(path).steps.forEachIndexed { i, step ->
                    val built = MessageBuilder.build(step, i)
                    if (step.kind != Scenario.KIND_RAW) {
                        assertNotNull("$path step $i must decode through R4Json", built.message)
                    }
                }
            }
    }

    @Test
    fun vehicleCNullOverrideProducesJsonNullNotAbsentKey() {
        val step = Step(sample = "r4-warning", overrides = mapOf("geometry.vehicleC" to JsonNull))
        val built = MessageBuilder.build(step, 0)
        val text = built.bytes.decodeToString()
        assertTrue("explicit null must survive onto the wire: $text", text.contains("\"vehicleC\":null"))
        assertNull((built.message as R4WarningEvent).geometry.vehicleC)
    }

    @Test
    fun additiveJunkFieldSurvivesOntoTheWireAndStillDecodes() {
        val step = Step(sample = "r4-warning", overrides = mapOf("simJunk" to JsonPrimitive("kept")))
        val built = MessageBuilder.build(step, 0)
        assertTrue(built.bytes.decodeToString().contains("\"simJunk\":\"kept\""))
        assertNotNull(built.message)
    }

    @Test
    fun overrideChangesTheDecodedFieldValue() {
        val step = Step(sample = "r4-warning", overrides = mapOf("riskState" to JsonPrimitive("medium")))
        val built = MessageBuilder.build(step, 0)
        assertEquals("medium", (built.message as R4WarningEvent).riskState)
    }

    @Test
    fun rawStepBytesAreReturnedUntouchedAndUnvalidated() {
        val step = Step(kind = Scenario.KIND_RAW, text = "not-json {truncated")
        val built = MessageBuilder.build(step, 0)
        assertEquals("not-json {truncated", built.bytes.decodeToString())
        assertNull(built.message)
    }

    @Test
    fun invalidOverrideFailsTheBuildNamingTheStep() {
        val step = Step(sample = "r4-warning", overrides = mapOf("object.distance" to JsonPrimitive("far")))
        try {
            MessageBuilder.build(step, 4)
            fail("expected ScenarioException")
        } catch (e: ScenarioException) {
            assertTrue("message must name the step: ${e.message}", e.message!!.contains("step 4"))
        }
    }
}
