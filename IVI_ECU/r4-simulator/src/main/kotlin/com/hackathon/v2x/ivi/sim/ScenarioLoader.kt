package com.hackathon.v2x.ivi.sim

import kotlinx.serialization.json.Json
import java.io.File

/**
 * File path → validated [Scenario]. Every rejection names the file and the
 * offending field — an unknown sample name, an unknown kind, and an empty
 * step list are rejections, never silent defaults.
 */
object ScenarioLoader {

    private val json = Json { ignoreUnknownKeys = false }

    fun load(path: String): Scenario {
        val file = File(path)
        if (!file.isFile) throw ScenarioException("$path: scenario file does not exist")

        val scenario = try {
            json.decodeFromString(Scenario.serializer(), file.readText())
        } catch (e: Exception) {
            throw ScenarioException("$path: not a valid scenario file — ${e.message}")
        }

        if (scenario.steps.isEmpty()) {
            throw ScenarioException("$path: 'steps' is empty — a scenario must send something")
        }
        if (scenario.defaultRateHz <= 0.0) {
            throw ScenarioException("$path: 'defaultRateHz' must be positive, got ${scenario.defaultRateHz}")
        }
        scenario.steps.forEachIndexed { index, step ->
            when (step.kind) {
                Scenario.KIND_SAMPLE -> {
                    val sample = step.sample
                        ?: throw ScenarioException("$path: step $index has kind 'sample' but no 'sample' name")
                    if (sample !in Scenario.KNOWN_SAMPLES) {
                        throw ScenarioException(
                            "$path: step $index names unknown sample '$sample' — known: ${Scenario.KNOWN_SAMPLES.sorted()}",
                        )
                    }
                }
                Scenario.KIND_RAW -> {
                    if (step.text == null) {
                        throw ScenarioException("$path: step $index has kind 'raw' but no 'text'")
                    }
                }
                else -> throw ScenarioException(
                    "$path: step $index has unknown kind '${step.kind}' — known: sample, raw",
                )
            }
            if (step.delayS != null && step.delayS < 0.0) {
                throw ScenarioException("$path: step $index has negative delayS ${step.delayS}")
            }
        }
        return scenario
    }
}

/** A scenario file failed to load; the message names the file and the cause. */
class ScenarioException(message: String) : IllegalArgumentException(message)
