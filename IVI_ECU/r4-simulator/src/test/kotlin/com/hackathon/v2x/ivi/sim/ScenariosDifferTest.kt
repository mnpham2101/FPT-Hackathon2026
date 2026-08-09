package com.hackathon.v2x.ivi.sim

import com.hackathon.v2x.ivi.model.R4WarningEvent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Different scenario files must produce observably different streams — the
 * same rule R11 imposes on the bench. Asserted on decoded field values, not
 * byte inequality.
 */
class ScenariosDifferTest {

    private fun warningsOf(path: String): List<R4WarningEvent> =
        ScenarioLoader.load(path).steps.mapIndexedNotNull { i, step ->
            MessageBuilder.build(step, i).message as? R4WarningEvent
        }

    @Test
    fun approachRisesFallsAndKeepsTrustedProvenance() {
        val warnings = warningsOf("scenarios/approach.json")
        val risks = warnings.map { it.riskState }
        assertTrue("approach must reach high: $risks", "high" in risks)
        assertEquals("approach must fall back to low at the end", "low", risks.last())
        assertTrue(warnings.all { it.objectSnapshot.source == "v2x_relayed" })
        assertTrue(warnings.all { it.warningType == "nlos_obstruction" })
    }

    @Test
    fun degradeCarriesTheCasesApproachNeverSends() {
        val warnings = warningsOf("scenarios/degrade.json")
        val types = warnings.map { it.warningType }
        val sources = warnings.map { it.objectSnapshot.source }
        assertTrue("degrade must send an unknown warningType: $types", "slippery_road" in types)
        assertTrue("degrade must send an untrusted source: $sources", "own_sensor" in sources)
    }

    @Test
    fun approachAndDegradeStreamsDifferObservably() {
        val approach = warningsOf("scenarios/approach.json").map { it.riskState to it.warningType }
        val degrade = warningsOf("scenarios/degrade.json").map { it.riskState to it.warningType }
        assertNotEquals(approach, degrade)
    }

    @Test
    fun stateStreamSequencesAscend() {
        val seqs = ScenarioLoader.load("scenarios/state-stream.json").steps.mapIndexed { i, step ->
            (MessageBuilder.build(step, i).message as com.hackathon.v2x.ivi.model.R4StateMessage).seq
        }
        assertEquals(seqs.sorted(), seqs)
        assertEquals(seqs.distinct(), seqs)
    }
}
