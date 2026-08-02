# Parsing the R4 message on the IVI side

Research note: the technical approach for turning a datagram arriving on `10.99.0.13:47300` into a typed R4 message the UI can render. The contract itself is [contracts/r4-ada-ivi.schema.json](../../../contracts/r4-ada-ivi.schema.json) (byte-synced to [IVI_ECU/contracts/](../../contracts/)) — this note covers how it is consumed, not what it says.

## 1. What is actually on the wire — there is no application header

**One R4 message = one UDP datagram = one UTF-8 JSON object. Nothing else.** No length prefix, no envelope, no framing header, no sequence header — the schema's `oneOf` is the entire payload.

This matters because "strip the Ethernet header, keep the payload" describes work the operating system has already done by the time Android hands the app a packet:

```
Ethernet frame │ IP header │ UDP header │ ───────── payload ─────────
   stripped by the NIC/kernel ───────────▶  { "schemaVersion": 1, "type": "warning", … }
                                            this is what DatagramPacket exposes
```

An app-level "de-framing" step is therefore not header removal but **correct slicing of the receive buffer**, which is where the real bugs live:

- `DatagramPacket.getData()` returns *the whole backing array*, not the received bytes. The message is `data[offset until offset + length]`. Decoding the full array appends whatever the previous, longer datagram left behind — a silent corruption that only appears when message sizes vary.
- A reused `DatagramPacket` keeps the *shortened* length from the last receive. `packet.setLength(buffer.size)` before every `receive()`, or every datagram after the first is truncated to the length of the shortest one seen.
- **UDP truncates silently.** If `length == buffer.size`, the datagram may have been larger than the buffer and the tail is gone. Treat that case as suspect and log it. A 2 KB buffer is ample: the frozen `r4-warning.json` sample is ~450 bytes, and the bridge's MTU headroom (open item O3) is far above that.
- **Datagram boundaries are preserved by UDP** — unlike TCP, no accumulate-and-split logic is needed, and none should be written.

Two further wire notes: strip a UTF-8 BOM and surrounding whitespace before decoding (the JSON parser tolerates whitespace, not a BOM), and note that `network_security_config.xml` governs the HTTP stacks only — it has no bearing on a raw `DatagramSocket`. It stays for correctness of any future HTTP use, not because UDP needs it.

## 2. Decoding — kotlinx.serialization, sealed on `type`

The binding already exists and is contract-tested: [R4Message.kt](../../app/src/main/java/com/hackathon/v2x/ivi/model/R4Message.kt) declares a sealed `R4Message` with `@SerialName("warning")` / `@SerialName("state")` subclasses, and the `R4Json` instance configured `classDiscriminator = "type"`, `ignoreUnknownKeys = true`. Round-trip and additive-version tests pass against the frozen samples.

The decoder built on top of it needs only to add failure handling:

| Input | kotlinx behaviour | Decoder maps it to |
|---|---|---|
| Valid `warning` / `state` | Typed subclass | Success |
| Unknown extra fields | Dropped (`ignoreUnknownKeys`) | Success — additive tolerance |
| `schemaVersion` newer than known (e.g. `2`) | Just an `Int` field; no gate | Success; log once if above the known version |
| Unknown `warningType` | Just a `String` field; **the value is preserved** | Success (see §3) |
| Unknown `type` discriminator | `SerializationException` — sealed polymorphism has no subclass to select | Failure, distinguishable: "unknown message type" |
| Malformed JSON / truncated | `SerializationException` | Failure: "malformed" |
| Wrong field type (`"distance": "far"`) | `SerializationException` | Failure: "malformed" |
| Missing required field | `MissingFieldException` (a `SerializationException`) | Failure: "malformed" |

Design points:

- **Return a result, never throw across the receive loop.** A parse failure must log (level WARN, with a bounded prefix of the payload — never the whole datagram) and continue receiving. One bad producer message must not stop the app from rendering the next good one.
- **Keep `isLenient = false`.** Leniency here means unquoted keys and other JSON-syntax slack, which would hide producer bugs; the tolerance the contract asks for is `ignoreUnknownKeys`, which is a different switch and is already on.
- **Do not add runtime JSON-Schema validation.** The typed decode already enforces required fields and types; a schema validator on the device would duplicate that at cost. The schema is enforced where it belongs — in the round-trip tests, on both sides of the contract.
- **Parse off the main thread** (the IO dispatcher the receive loop already runs on). Rate is ~1–10 messages/second; the cost is irrelevant, but the main thread must never block on a socket.

## 3. Unknown `warningType` — preserve the value, classify at the edge

This is the one place where an existing task description and the committed test disagree, and the test is right.

- The frozen contract says: *"Unknown values must degrade gracefully at the consumer"* — it does not say the value is replaced.
- The committed additive-version test asserts the opposite of replacement: `r4-unknown-warning.json` carries `warningType: "slippery_road"` and [R4AdditiveVersionTest](../../app/src/test/java/com/hackathon/v2x/ivi/model/R4AdditiveVersionTest.kt) asserts the parsed value **is** `"slippery_road"`, merely not equal to the M1 `nlos_obstruction` constant.
- [plans/phase5_tasks.md](../../../plans/phase5_tasks.md) subtask `4.5.1.2` instead specifies *"Unknown `warningType` → parsed as `warningType = "unknown"`"*.

**Recommended:** the parser preserves the wire value verbatim; a separate classification step maps *known* types to their presentation and everything else to a generic warning presentation. Rewriting the field at parse time destroys information the log needs (which unknown type arrived), breaks the committed round-trip equality, and pushes a UI concern into the data layer. The user-visible behaviour the acceptance box asks for — a newer message degrades gracefully instead of crashing — is delivered either way.

## 4. Validation the decoder does *not* cover

Type-correct JSON can still be semantically wrong. Two checks belong above the parser:

- **Provenance guard.** `object.source` must be `v2x_relayed` before anything renders as ghost C. This is the R19 claim made mechanical, and it is already implemented in the renderer ([CanvasWarningView](../../app/src/main/java/com/hackathon/v2x/ivi/ui/view/CanvasWarningView.kt): non-relayed source renders a yellow `[?]` marker and logs at ERROR).
- **`geometry.vehicleC` may legitimately be `null`** — C not yet tracked. It is `null`, not absent, in the frozen schema; the Kotlin model declares it nullable and every consumer must accept it. This is a normal state, not an error, and must not be logged as one.

## 5. Where the parsing code should live

A **pure Kotlin/JVM Gradle submodule** holding the models, the configured `Json`, and the byte-slice-to-message entry point — depended on by the APK and by the R4 simulator ([phase5-r4-simulator.md](phase5-r4-simulator.md)), so producer and consumer cannot drift.

- The submodule must have **zero Android dependencies**, which keeps it testable in the plain-JVM CI job and reusable by a command-line tool.
- The models currently sit in the app module; moving them is a relocation, not a rewrite — the committed round-trip and additive-version tests move with them and must keep passing unchanged.
- The contract copies under [IVI_ECU/contracts/](../../contracts/) and the test-resource samples are byte-synced by [check_sync.py](../../../contracts/check_sync.py); any new location for the samples must be registered in [sync-manifest.json](../../../contracts/sync-manifest.json) or the integrity gate stops matching.
