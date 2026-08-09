package com.hackathon.v2x.ivi.sim

import kotlinx.serialization.Serializable
import kotlinx.serialization.json.JsonElement

/**
 * A scenario is data, not code (D9) — a named, ordered list of timed messages.
 * A new case is a new file, never a new Kotlin branch.
 *
 * The frozen file shape:
 * ```json
 * {
 *   "name": "approach",
 *   "defaultRateHz": 1.0,
 *   "loop": false,
 *   "steps": [
 *     { "sample": "r4-warning", "overrides": { "riskState": "low", "geometry.vehicleC": null } },
 *     { "sample": "r4-warning", "delayS": 15.0 },
 *     { "kind": "raw", "text": "not-json" }
 *   ]
 * }
 * ```
 *
 * - `sample` names a frozen `:contract` fixture ([KNOWN_SAMPLES]).
 * - `overrides` maps dotted JSON paths to values; explicit `null` is meaningful.
 * - `kind: "raw"` sends `text` as literal bytes, unvalidated on purpose.
 * - `delayS` is the pause after sending this step, replacing the default
 *   `1 / rateHz` tick — how a file expresses silence longer than the rate
 *   (e.g. outlasting the consumer's warning timeout) without stopping the run.
 */
@Serializable
data class Scenario(
    val name: String,
    val defaultRateHz: Double = 1.0,
    val loop: Boolean = false,
    val steps: List<Step>,
) {
    companion object {
        const val KIND_SAMPLE = "sample"
        const val KIND_RAW = "raw"

        /** The frozen `:contract` fixtures a step may name. */
        val KNOWN_SAMPLES = setOf("r4-warning", "r4-state", "r4-unknown-warning")
    }
}

@Serializable
data class Step(
    val kind: String = Scenario.KIND_SAMPLE,
    val sample: String? = null,
    val overrides: Map<String, JsonElement> = emptyMap(),
    val text: String? = null,
    val delayS: Double? = null,
)
