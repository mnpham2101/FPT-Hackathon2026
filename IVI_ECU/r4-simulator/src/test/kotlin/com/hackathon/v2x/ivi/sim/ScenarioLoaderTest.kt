package com.hackathon.v2x.ivi.sim

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import java.io.File

class ScenarioLoaderTest {

    private fun tempScenario(content: String): String {
        val file = File.createTempFile("scenario", ".json")
        file.deleteOnExit()
        file.writeText(content)
        return file.path
    }

    private fun assertRejected(content: String, vararg messageParts: String) {
        val path = tempScenario(content)
        try {
            ScenarioLoader.load(path)
            fail("expected ScenarioException")
        } catch (e: ScenarioException) {
            messageParts.forEach { part ->
                assertTrue("message must contain '$part': ${e.message}", e.message!!.contains(part))
            }
        }
    }

    // ── The committed scenario files (4.5.6.4) ────────────────────────────────

    @Test
    fun committedApproachScenarioLoads() {
        val scenario = ScenarioLoader.load("scenarios/approach.json")
        assertEquals("approach", scenario.name)
        assertTrue("the in-Room evidence scenario must loop so a late-launching IVI still catches warnings", scenario.loop)
        assertTrue(scenario.steps.size >= 5)
    }

    @Test
    fun committedDegradeScenarioLoads() {
        val scenario = ScenarioLoader.load("scenarios/degrade.json")
        assertEquals("degrade", scenario.name)
        assertEquals(Scenario.KIND_RAW, scenario.steps.last().kind)
    }

    @Test
    fun committedStateStreamScenarioLoads() {
        val scenario = ScenarioLoader.load("scenarios/state-stream.json")
        assertEquals("state-stream", scenario.name)
        assertTrue(scenario.steps.all { it.sample == "r4-state" })
    }

    // ── Rejections, each naming the cause ─────────────────────────────────────

    @Test
    fun malformedJsonIsRejected() {
        assertRejected("{not json", "not a valid scenario file")
    }

    @Test
    fun unknownSampleNameIsRejected() {
        assertRejected(
            """{"name":"x","steps":[{"sample":"r9-imaginary"}]}""",
            "unknown sample", "r9-imaginary",
        )
    }

    @Test
    fun unknownKindIsRejected() {
        assertRejected(
            """{"name":"x","steps":[{"kind":"telepathy","sample":"r4-warning"}]}""",
            "unknown kind", "telepathy",
        )
    }

    @Test
    fun emptyStepsAreRejected() {
        assertRejected("""{"name":"x","steps":[]}""", "'steps' is empty")
    }

    @Test
    fun missingScenarioFileIsRejectedNamingThePath() {
        try {
            ScenarioLoader.load("scenarios/does-not-exist.json")
            fail("expected ScenarioException")
        } catch (e: ScenarioException) {
            assertTrue(e.message!!.contains("does-not-exist.json"))
        }
    }
}
