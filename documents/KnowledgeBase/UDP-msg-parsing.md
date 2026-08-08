# Parsing UDP messages on the IVI side

## 1. Introduction

The IVI application receives messages as UDP datagrams on a configured port. Each datagram has to be converted into a typed Kotlin object that the presentation layer can consume: the receive path yields raw bytes, and every stage above it — repository, view model, renderer — needs fields with names and types rather than a byte buffer.

The payload is a UTF-8 JSON document. Deserialising it into Kotlin data classes gives compile-time field access, type checking at the boundary, and a single place where a malformed message is rejected. In this project the message exchanged from the ADA-ECU to the IVI-ECU is defined in [r4-ada-ivi.schema.json](../../contracts/r4-ada-ivi.schema.json).

This document covers the consumer side: the structure on the wire, the deserialisation library selected for it, and the handling that library does not provide.

## 2. Message structure

![Message structure](message-structure.svg)

*Source: [message-structure.drawio](message-structure.drawio).*

**One message is one UDP datagram containing one UTF-8 JSON object.** There is no application-layer framing: no length prefix, no envelope, no sequence header. The JSON object is the entire payload.

The link, network and transport headers are removed by the network interface and the kernel before the datagram reaches the application, so an application-level "de-framing" step is not header removal. It is correct slicing of the receive buffer, which is where the defects occur:

- `DatagramPacket.getData()` returns the whole backing array, not the received bytes. The message occupies `data[offset until offset + length]`. Decoding the full array appends residue left by a previous, longer datagram — a silent corruption that appears only when message sizes vary.
- A reused `DatagramPacket` retains the shortened length of the previous receive. Call `packet.setLength(buffer.size)` before every `receive()`, or every datagram after the first is truncated to the shortest length observed.
- **UDP truncates without notification.** If `length == buffer.size`, the datagram may have exceeded the buffer and the remainder is lost. Treat that condition as suspect and log it. A 2 KB buffer is sufficient: a representative warning message is approximately 450 bytes.
- **UDP preserves datagram boundaries.** Unlike a stream transport, no accumulate-and-split logic is required, and none should be implemented.
- Strip a UTF-8 byte-order mark and surrounding whitespace before deserialising. A JSON parser tolerates whitespace but not a leading BOM.

The listen port is application configuration — `47300` against the ADA-ECU, `5004` against a local simulator — and is unrelated to the link-layer configuration of the bridge.

### Message surface

The `type` field discriminates two variants:

| `type` | Required fields | Delivery semantics |
|---|---|---|
| `warning` | `schemaVersion`, `warningType`, `riskState`, `object`, `geometry` | Edge-triggered; raises the warning view |
| `state` | `schemaVersion`, `seq`, `vehicles` | Periodic; last value wins |

Coordinates in `geometry` and `vehicles` are ego-relative and expressed in metres, with `ego` at the origin `(0,0)`. The `geometry.vehicleC` member is nullable and is `null`, not absent, while C is untracked.

An unrecognised `warningType` value must degrade rather than raise — see §4.1.

## 3. Proposed solutions

| Candidate | Assessment | Verdict |
|---|---|---|
| **A. kotlinx.serialization** | First-party Kotlin library; compiler-plugin code generation, so no reflection on Android; sealed-class polymorphism maps directly onto the `type` discriminator; already exercised by round-trip and additive-version tests | **Selected** |
| **B. nlohmann/json through JNI or a native submodule** | A C++ library on an Android runtime. Requires a JNI boundary and an NDK build for a payload the platform already decodes natively, and duplicates a binding that exists on the producer side | Rejected |
| **C. org.json or Gson** | Reflective, untyped or weakly typed; no sealed-hierarchy support, so the discriminator must be branched by hand and every field re-checked at the call site | Rejected |

**nlohmann/json belongs on the C++ producer side.** The element shared between producer and consumer is the JSON schema, not a parser implementation: each side binds the schema in the library idiomatic to its runtime, and the round-trip tests on both sides are what keep the two bindings aligned.

Where a reusable parsing component is wanted inside `IVI_ECU/`, it is a Kotlin Gradle module wrapping the models and the configured deserialiser — see §4.3.

## 4. kotlinx.serialization

The binding declares a sealed `R4Message` type with `@SerialName("warning")` and `@SerialName("state")` subclasses ([R4Message.kt](../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/model/R4Message.kt)), decoded by a `Json` instance configured with `classDiscriminator = "type"` and `ignoreUnknownKeys = true`. Sealed polymorphism performs the variant selection, so no manual branch on `type` is written.

The decoder wrapping it adds failure handling:

| Input | Library behaviour | Decoder result |
|---|---|---|
| Valid `warning` or `state` | Typed subclass | Success |
| Unrecognised additional fields | Discarded (`ignoreUnknownKeys`) | Success — forward compatibility |
| `schemaVersion` above the known value | Plain `Int` field; no gate applied | Success; logged once |
| Unrecognised `warningType` | Plain `String` field; value preserved | Success — see §4.1 |
| Unrecognised `type` discriminator | `SerializationException`; the sealed hierarchy has no matching subclass | Failure, reported as unknown message type |
| Malformed or truncated JSON | `SerializationException` | Failure, reported as malformed |
| Type mismatch, e.g. `"distance": "far"` | `SerializationException` | Failure, reported as malformed |
| Missing required field | `MissingFieldException` | Failure, reported as malformed |

Receive-path design points:

- **Return a result; do not propagate an exception across the receive loop.** A failed deserialisation is logged at WARN with a bounded prefix of the payload, never the entire datagram, and reception continues. One malformed message must not prevent the next valid one from rendering.
- **Keep `isLenient = false`.** Leniency admits unquoted keys and comparable syntax deviations, which conceal producer defects. Forward compatibility is provided by `ignoreUnknownKeys`, which is a separate setting.
- **Do not add runtime schema validation.** Typed deserialisation already enforces required fields and their types; a schema validator executing on the device duplicates that work at runtime cost. Schema conformance is verified by the round-trip tests on both sides.
- **Deserialise off the main thread**, on the IO dispatcher the receive loop already occupies. At 1–10 messages per second the cost is negligible, but the main thread must never block on a socket.

The resulting pipeline:

1. Receive the datagram on the IO dispatcher; capture the byte array and the received length.
2. Decode `data[offset until offset + length]` as UTF-8; reject an empty result.
3. Deserialise into the sealed `R4Message` hierarchy.
4. Apply the semantic checks of §4.2.
5. Publish to the repository flow; view models map the result to UI state.

### 4.1 Unrecognised `warningType` — preserve the value, classify at the presentation edge

Substituting a placeholder such as `"unknown"` at deserialisation time discards information. The value must be preserved verbatim, and a separate classification step maps recognised values to their presentation and every other value to a generic warning presentation.

The committed additive-version test asserts this behaviour: `r4-unknown-warning.json` carries `warningType: "slippery_road"`, and [R4AdditiveVersionTest](../../IVI_ECU/app/src/test/java/com/hackathon/v2x/ivi/model/R4AdditiveVersionTest.kt) asserts the parsed value **is** `"slippery_road"` — merely not equal to the known constant.

Rewriting the field during deserialisation would remove the diagnostic record of which unrecognised value arrived, break round-trip equality, and place a presentation concern in the data layer. The externally observable behaviour — an unrecognised value degrades instead of raising — is achieved either way.

### 4.2 Validation the decoder does not cover

Type-correct JSON can still be semantically invalid. Two checks belong above the deserialiser:

- **Provenance guard.** The `object.source` member must equal `v2x_relayed` before the object is rendered as a relayed vehicle. This is implemented in the renderer: [CanvasWarningView](../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/ui/view/CanvasWarningView.kt) draws a `[?]` marker and logs at ERROR for any other value.
- **`geometry.vehicleC` may legitimately be `null`**, indicating that C is not yet tracked. The Kotlin model declares it nullable and every consumer accepts it. This is a normal operating state and must not be logged as an error.

### 4.3 Where the parsing code should live

A **pure Kotlin/JVM Gradle module** holding the models, the configured `Json` instance, and the byte-slice-to-message entry point, depended upon by the application and by the simulator that produces the same messages ([producer-simulation-harness.md](producer-simulation-harness.md)), so that producer and consumer cannot diverge.

- The module carries **no Android dependencies**, which keeps it executable in a plain-JVM test job and reusable from a command-line tool.
- The models currently reside in the application module. Relocating them is a move, not a rewrite: the committed round-trip and additive-version tests move with them and must continue to pass unchanged.
