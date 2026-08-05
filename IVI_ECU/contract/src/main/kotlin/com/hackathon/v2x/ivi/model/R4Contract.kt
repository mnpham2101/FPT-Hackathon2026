package com.hackathon.v2x.ivi.model

/**
 * Frozen R4 contract anchors (`contracts/r4-ada-ivi.schema.json`).
 *
 * The version half of additive evolution (HLD D4): a message whose
 * `schemaVersion` is above [KNOWN_SCHEMA_VERSION] still decodes leniently;
 * consumers may log the gap but must not reject the message.
 */
object R4Contract {
    /** The schema version this binding was written against. */
    const val KNOWN_SCHEMA_VERSION: Int = 1
}
