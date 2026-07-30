package com.hackathon.v2x.ivi.data

import android.util.Log
import com.hackathon.v2x.ivi.model.R4Message
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.jsonPrimitive

/**
 * Stateless JSON deserializer for R4 UDP packets.
 *
 * Subtask 4.5.1.2 — Additive-version safety rules:
 * - Unknown [R4Message.R4WarningEvent.warningType] → degraded to [R4Message.UNKNOWN_WARNING_TYPE]
 * - Unknown extra JSON fields → ignored silently (lenient mode)
 * - Unknown message `type` → [Result.failure] with [UnknownMessageTypeException]
 * - Malformed JSON → [Result.failure] with [MalformedR4PayloadException] (byte offset included)
 */
class R4Deserializer {

    private val json = Json {
        ignoreUnknownKeys = true
        isLenient = true
        coerceInputValues = true
    }

    /**
     * Parse raw [bytes] into an [R4Message].
     *
     * @return [Result.success] with the parsed message, or [Result.failure] on any error.
     */
    fun deserialize(bytes: ByteArray): Result<R4Message> = runCatching {
        val raw = bytes.decodeToString()
        val root = parseJsonObject(raw, bytes)

        val type = root["type"]?.jsonPrimitive?.content
            ?: return Result.failure(MalformedR4PayloadException("Missing 'type' field", bytes))

        return when (type) {
            R4Message.R4WarningEvent.TYPE_KEY -> parseWarningEvent(raw, bytes)
            R4Message.R4StateMessage.TYPE_KEY -> parseStateMessage(raw)
            else -> Result.failure(UnknownMessageTypeException(type))
        }
    }.onFailure { e ->
        if (e !is UnknownMessageTypeException && e !is MalformedR4PayloadException) {
            Log.w(TAG, "R4 parse failure: ${bytes.take(256).toByteArray().decodeToString()}", e)
        }
    }

    private fun parseJsonObject(raw: String, bytes: ByteArray): JsonObject =
        runCatching { json.parseToJsonElement(raw) as? JsonObject }
            .getOrNull()
            ?: throw MalformedR4PayloadException("Not a JSON object", bytes)

    private fun parseWarningEvent(raw: String, bytes: ByteArray): Result<R4Message> {
        val event = runCatching {
            json.decodeFromString<R4Message.R4WarningEvent>(raw)
        }.getOrElse { e ->
            Log.w(TAG, "Warning event parse error: ${bytes.take(256).toByteArray().decodeToString()}")
            return Result.failure(MalformedR4PayloadException(e.message ?: "parse error", bytes))
        }

        // Additive-version safety: unknown warningType degrades gracefully, never throws.
        val safeEvent = if (event.warningType.isBlank() || !isKnownWarningType(event.warningType)) {
            Log.w(TAG, "Unknown warningType='${event.warningType}' — degrading to '${R4Message.UNKNOWN_WARNING_TYPE}'")
            event.copy(warningType = R4Message.UNKNOWN_WARNING_TYPE)
        } else {
            event
        }
        return Result.success(safeEvent)
    }

    private fun parseStateMessage(raw: String): Result<R4Message> =
        runCatching { Result.success(json.decodeFromString<R4Message.R4StateMessage>(raw)) }
            .getOrElse { e -> Result.failure(e) }

    private fun isKnownWarningType(type: String): Boolean =
        type == "nlos_obstruction" || type == R4Message.UNKNOWN_WARNING_TYPE

    companion object {
        private const val TAG = "R4Deserializer"
    }
}

/** Thrown when the R4 packet's `type` field is not "warning" or "state". */
class UnknownMessageTypeException(type: String) :
    Exception("Unknown R4 message type: '$type'")

/** Thrown when the raw bytes cannot be parsed as valid R4 JSON. */
class MalformedR4PayloadException(reason: String, bytes: ByteArray) :
    Exception("Malformed R4 payload ($reason): first 256 bytes = '${bytes.take(256).toByteArray().decodeToString()}'")
