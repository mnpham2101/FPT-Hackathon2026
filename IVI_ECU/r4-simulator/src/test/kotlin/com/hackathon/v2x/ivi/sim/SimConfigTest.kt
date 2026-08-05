package com.hackathon.v2x.ivi.sim

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test

class SimConfigTest {

    private val fullEnv = mapOf(
        SimConfig.ENV_HOST to "10.99.0.13",
        SimConfig.ENV_PORT to "47300",
        SimConfig.ENV_SCENARIO to "scenarios/degrade.json",
        SimConfig.ENV_RATE_HZ to "2.5",
        SimConfig.ENV_START_DELAY_S to "12",
    )

    @Test
    fun fullEnvMapParses() {
        val config = SimConfig.fromEnv(fullEnv)
        assertEquals("10.99.0.13", config.host)
        assertEquals(47300, config.port)
        assertEquals("scenarios/degrade.json", config.scenarioPath)
        assertEquals(2.5, config.rateHz!!, 0.0)
        assertEquals(12.0, config.startDelayS, 0.0)
    }

    @Test
    fun missingHostFailsNamingTheVariable() {
        try {
            SimConfig.fromEnv(fullEnv - SimConfig.ENV_HOST)
            fail("expected SimConfigException")
        } catch (e: SimConfigException) {
            assertTrue("message must name ${SimConfig.ENV_HOST}: ${e.message}",
                e.message!!.contains(SimConfig.ENV_HOST))
        }
    }

    @Test
    fun absentRateLeavesScenarioDefaultInCharge() {
        val config = SimConfig.fromEnv(fullEnv - SimConfig.ENV_RATE_HZ)
        assertNull(config.rateHz)
    }

    @Test
    fun envRateOverridesScenarioFileDefault() {
        // rateHz non-null is the contract: Main resolves rateHz ?: scenario.defaultRateHz.
        val config = SimConfig.fromEnv(fullEnv)
        assertEquals(2.5, config.rateHz!!, 0.0)
    }

    @Test
    fun argsModeParsesAllFlags() {
        val config = SimConfig.fromArgs(
            arrayOf("--host", "127.0.0.1", "--port", "5005", "--scenario", "s.json", "--rate", "10"),
        )
        assertEquals("127.0.0.1", config.host)
        assertEquals(5005, config.port)
        assertEquals("s.json", config.scenarioPath)
        assertEquals(10.0, config.rateHz!!, 0.0)
    }

    @Test
    fun argsModeMissingHostFailsNamingTheFlag() {
        try {
            SimConfig.fromArgs(arrayOf("--port", "5005"))
            fail("expected SimConfigException")
        } catch (e: SimConfigException) {
            assertTrue(e.message!!.contains("--host"))
        }
    }

    @Test
    fun invalidPortFailsNamingTheVariable() {
        try {
            SimConfig.fromEnv(fullEnv + (SimConfig.ENV_PORT to "0"))
            fail("expected SimConfigException")
        } catch (e: SimConfigException) {
            assertTrue(e.message!!.contains(SimConfig.ENV_PORT))
        }
    }
}
