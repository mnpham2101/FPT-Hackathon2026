# Phase 5 — Technical approach: parse R4 (ADA→IVI)

R4 is UDP + JSON on the R6 bridge. Schema authority: [r4-ada-ivi.schema.json](../../../contracts/r4-ada-ivi.schema.json); requirement R4 in [m1-cooperative-awareness.md](../../../requirements/m1-cooperative-awareness.md). Report tech stack: **nlohmann/json on ADA (C++)**, **kotlinx.serialization on IVI (Kotlin)**.

## Message surface (consumer view)

Discriminate on `type`:

| `type` | Required fields | Notes |
|---|---|---|
| `warning` | `schemaVersion`, `warningType`, `riskState`, `object` (R3), `geometry` | Edge-triggered HMI wake |
| `state` | `schemaVersion`, `seq`, `vehicles` | Optional R15; last-value-wins |

`geometry` / `vehicles`: ego-relative meters; `ego` origin `(0,0)`; `vehicleC` may be null.

Additive-version: unknown `warningType` string → degrade, do not throw.

Fixtures: [contracts/samples/](../../../contracts/samples/).

## Ethernet / framing

- **No custom Ethernet application header** in the frozen R4 contract: payload is the JSON object bytes in the UDP datagram.
- “Strip header, keep payload” applies only if a future wrapper is introduced; M1 IVI must treat `DatagramPacket.data[0..length)` as JSON text.
- Do not confuse bridge L2 with UDP ports: listen port is app config (`47300` production / `5004` Phase 5 mock).

## Solution comparison (IVI JSON parse)

| Candidate | Criteria fit | Verdict |
|---|---|---|
| **A. kotlinx.serialization** | Report-mandated for IVI; Compose/Android native; round-trip + additive tests already patterned in Phase 0 | **Pick** (possibility + milestone speed + report) |
| **B. nlohmann/json via JNI / native submodule** | Matches ADA library name; user-requested submodule | Reject for IVI AAOS in M1 — wrong runtime, JNI cost, duplicates ADA binding, violates “IVI = kotlinx” in R4 tech stack |
| **C. org.json / Gson** | Familiar | Extra deps / weaker sealed hierarchy vs kotlinx; no report mandate |

**nlohmann belongs in `ADA_ECU/` (and V2X where used), not as an IVI Gradle submodule.** Shared truth is the JSON Schema under `contracts/`, not a shared C++ parser.

If a “reusable parse library” module is desired inside `IVI_ECU/`: make it a **Kotlin** Gradle module (e.g. `:r4-contract`) wrapping kotlinx models + deserializer — not nlohmann.

## Recommended parse pipeline (IVI)

1. **UDP receive** (IO dispatcher / foreground service) → `ByteArray` + length.
2. **Decode** UTF-8 string; reject empty.
3. **Deserialize** to sealed `R4Message` (`warning` | `state`); map unknown `warningType` to explicit unknown/degrade path.
4. **Validate defensive fields** for UI: `object.source` for ghost-C guard; null-safe `vehicleC`.
5. **Publish** to repository flows; ViewModels map to UI state.
6. **Unit tests**: fixtures round-trip; additive-version sample; malformed JSON → failure without process crash.

ADA producer remains nlohmann ↔ same schema (Phase 0 bindings).

## Related

- Simulation: [phase5-r4-simulation-harness.md](phase5-r4-simulation-harness.md)
- Implementation notes: [phase5-ivi-implementation-notes.md](phase5-ivi-implementation-notes.md)
