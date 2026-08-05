package com.hackathon.v2x.ivi.sim

import com.hackathon.v2x.ivi.model.R4Json
import com.hackathon.v2x.ivi.model.R4Message
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject

/**
 * One wire payload built from a step: the exact bytes to send, plus the typed
 * message the bytes decoded to — `null` only for a raw step, whose bytes are
 * deliberately unvalidated (the malformed case).
 */
data class BuiltMessage(
    val bytes: ByteArray,
    val message: R4Message?,
)

/**
 * Builds every payload from the frozen sample, applies the step's overrides at
 * `JsonElement` level, and proves the app can parse it before it goes on the
 * wire (D9). Element-level editing is what lets an additive junk field survive
 * onto the wire — a typed round trip would drop it.
 */
object MessageBuilder {

    fun build(step: Step, stepIndex: Int): BuiltMessage {
        if (step.kind == Scenario.KIND_RAW) {
            return BuiltMessage(checkNotNull(step.text).toByteArray(Charsets.UTF_8), message = null)
        }

        var root = R4Json.parseToJsonElement(SampleLibrary.read(checkNotNull(step.sample))) as JsonObject
        for ((path, value) in step.overrides) {
            root = withOverride(root, path.split('.'), value)
        }
        val text = root.toString()

        val message = try {
            R4Json.decodeFromString(R4Message.serializer(), text)
        } catch (e: Exception) {
            throw ScenarioException(
                "step $stepIndex (sample '${step.sample}'): overrides produce a payload the app cannot parse — ${e.message}",
            )
        }
        return BuiltMessage(text.toByteArray(Charsets.UTF_8), message)
    }

    /** Immutable dotted-path set: replaces (or adds) the element at [path] inside [obj]. */
    private fun withOverride(obj: JsonObject, path: List<String>, value: JsonElement): JsonObject {
        val key = path.first()
        val replacement = if (path.size == 1) {
            value
        } else {
            val child = obj[key] as? JsonObject
                ?: throw ScenarioException("override path segment '$key' is not an object in the sample")
            withOverride(child, path.drop(1), value)
        }
        return buildJsonObject {
            obj.forEach { (k, v) -> if (k != key) put(k, v) }
            put(key, replacement)
        }
    }
}
