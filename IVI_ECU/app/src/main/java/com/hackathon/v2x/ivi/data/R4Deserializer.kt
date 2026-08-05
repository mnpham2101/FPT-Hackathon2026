package com.hackathon.v2x.ivi.data

import android.util.Log
import com.hackathon.v2x.ivi.model.R4Json
import com.hackathon.v2x.ivi.model.R4Message
import com.hackathon.v2x.ivi.model.R4StateMessage
import com.hackathon.v2x.ivi.model.R4WarningEvent
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.jsonPrimitive

/**
 * Stateless JSON deserializer for R4 UDP packets.
 *
 * Aligned to main's authority contract ([R4Message] / [R4Json]):
 * - Unknown extra JSON fields → ignored (`ignoreUnknownKeys`)
 * - Unknown `warningType` → **preserved verbatim** (HLD D4, §4.4 fix); presentation degrades at UI edge
 * - Unknown message `type` → [Result.failure] with [UnknownMessageTypeException]
 * - Malformed JSON → [Result.failure] with [MalformedR4PayloadException]
 */
class R4Deserializer {

    fun deserialize(bytes: ByteArray): Result<R4Message> = runCatching {
        val raw = bytes.decodeToString()
        val root = parseJsonObject(raw, bytes)

        val type = root["type"]?.jsonPrimitive?.content
            ?: return Result.failure(MalformedR4PayloadException("Missing 'type' field", bytes))

        return when (type) {
            TYPE_WARNING -> parseWarningEvent(raw, bytes)
            TYPE_STATE -> parseStateMessage(raw)
            else -> Result.failure(UnknownMessageTypeException(type))
        }
    }.onFailure { e ->
        if (e !is UnknownMessageTypeException && e !is MalformedR4PayloadException) {
            safeLogW(TAG, "R4 parse failure: ${bytes.take(256).toByteArray().decodeToString()}", e)
        }
    }

    private fun parseJsonObject(raw: String, bytes: ByteArray): JsonObject =
        runCatching { R4Json.parseToJsonElement(raw) as? JsonObject }
            .getOrNull()
            ?: throw MalformedR4PayloadException("Not a JSON object", bytes)

    private fun parseWarningEvent(raw: String, bytes: ByteArray): Result<R4Message> {
        val event = runCatching {
            R4Json.decodeFromString<R4WarningEvent>(raw)
        }.getOrElse { e ->
            safeLogW(TAG, "Warning event parse error: ${bytes.take(256).toByteArray().decodeToString()}")
            return Result.failure(MalformedR4PayloadException(e.message ?: "parse error", bytes))
        }

        // §4.4 fix: do NOT rewrite warningType here. The frozen contract requires the wire
        // value to survive intact to the consumer. Classification belongs at the UI edge.
        if (event.warningType.isBlank() || !isKnownWarningType(event.warningType)) {
            safeLogW(
                TAG,
                "Non-registry warningType='${event.warningType}' — preserving verbatim for UI-layer classification",
            )
        }
        return Result.success(event)
    }

    private fun parseStateMessage(raw: String): Result<R4Message> =
        runCatching {
            val msg: R4Message = R4Json.decodeFromString<R4StateMessage>(raw)
            Result.success(msg)
        }.getOrElse { e -> Result.failure(e) }

    private fun isKnownWarningType(type: String): Boolean =
        type == R4WarningEvent.WARNING_TYPE_NLOS_OBSTRUCTION || type == R4WarningEvent.UNKNOWN_WARNING_TYPE

    private fun safeLogW(tag: String, message: String, throwable: Throwable? = null) {
        runCatching {
            if (throwable != null) Log.w(tag, message, throwable) else Log.w(tag, message)
        }.onFailure {
            println("[$tag] WARN: $message")
        }
    }

    companion object {
        private const val TAG = "R4Deserializer"
        private const val TYPE_WARNING = "warning"
        private const val TYPE_STATE = "state"
    }
}

/** Thrown when the R4 packet's `type` field is not "warning" or "state". */
class UnknownMessageTypeException(type: String) :
    Exception("Unknown R4 message type: '$type'")

/** Thrown when the raw bytes cannot be parsed as valid R4 JSON. */
class MalformedR4PayloadException(reason: String, bytes: ByteArray) :
    Exception("Malformed R4 payload ($reason): first 256 bytes = '${bytes.take(256).toByteArray().decodeToString()}'")
