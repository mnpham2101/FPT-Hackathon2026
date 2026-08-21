# IVI fail-closed warning display

**Author:** Vũ Xuân Bách · lane IVI Phase 5  
**Scope:** how the IVI HMI behaves when input is bad, late, unknown, or untrusted — and why that behaviour is intentional for safety-style demos.  
**Authorities:** [UDP-msg-parsing.md](UDP-msg-parsing.md) · [ivi-ecu-hld.md](../Design/MODULE-DESIGN/IVI-ECU/ivi-ecu-hld.md) · [ivi-r4-observation-pipeline.md](../Design/MODULE-DESIGN/IVI-ECU/ivi-r4-observation-pipeline.md) · [r4-ada-ivi.schema.json](../../contracts/r4-ada-ivi.schema.json)

**Fail-closed** here means: when the consumer cannot justify showing a hazard, it **does not guess** — it drops, degrades visually, or returns to a safe idle screen. This note is the IVI-side policy map for degraded/failure scenarios (including those BTC asked for in Round 2 feedback).

---

## 1. Policy layers (where each decision lives)

```
UDP datagram
    → R4ListenerService     wire receive, retry, emit
    → R4Deserializer        syntax + type discrimination → Result
    → R4Repository          route warning vs state
    → WarningViewModel      Active / Idle / timeout
    → MainViewModel         DisplayMode wake-on-warning
    → CanvasWarningView     R19 provenance guard (draw)
```

| Layer | Fail-closed rule | Evidence tag |
|---|---|---|
| **Service** | Bad datagram → log and continue; socket death → retry then `serviceError` | `[RX]`, `[DROP]`, service error `StateFlow` |
| **Deserializer** | Malformed JSON → `Result.failure`; loop never throws | `[DROP]` with bounded reason |
| **Repository** | Only typed `warning` events enter `warningEvents` | — |
| **WarningViewModel** | No event → stay `Idle`; timeout clears `Active` | UI returns to non-warning mode |
| **Renderer** | Untrusted `source` → no ghost C; draw `[?]` instead | ERROR log on provenance violation |

Semantic checks sit **below** the parser ([UDP-msg-parsing.md](UDP-msg-parsing.md) §4.2) — the deserializer transcribes; the UI decides trust.

---

## 2. Wire input matrix

| Input condition | Parser / service | UI outcome | Expected demo evidence |
|---|---|---|---|
| **Valid `warning`, trusted source** | Success → emit | `Active`, God View, ghost C if `v2x_relayed` | `[RX]` + Warning screen |
| **Valid `warning`, unknown `warningType`** | Success; wire value preserved | Warning shown; presentation uses generic/ fail-safe styling | No crash; type visible in log/object |
| **Valid `warning`, untrusted `object.source`** | Success | Ghost C **not** drawn; yellow `[?]` marker | ERROR log; no false relay claim |
| **Valid `warning`, `geometry.vehicleC == null`** | Success | Scene without C marker — normal untracked state | No error log (state, not fault) |
| **Malformed / truncated JSON** | Failure | No state change; previous UI unchanged | `[DROP]` |
| **Unknown `type` discriminator** | Failure | Same as malformed | `[DROP]` |
| **Empty datagram** | Failure | Same | `[DROP]` |
| **UDP silence (ADA stop / network loss)** | No emit | After `WARNING_TIMEOUT_MS`, `Idle` | Screen leaves Warning; logs stop `[RX]` |
| **Transport exhausted (max retries)** | `serviceError` set | `WarningUiState.Error` path available | Link fault visible without fake warning |

**Design intent:** one bad packet cannot kill the listener; one bad packet cannot paint a false ghost C.

---

## 3. Warning lifecycle (temporal fail-closed)

| Phase | Trigger | IVI state | Display |
|---|---|---|---|
| **Idle** | App start or post-timeout | `WarningUiState.Idle` | `DisplayMode.HomeView` (default) |
| **Wake** | First `warning` event while Idle | `Active`; `MainViewModel` forces `WarningView` | God View with latest scene |
| **Sustain** | Repeated warnings refresh timeout | Stays `Active`; scene updates | Updated geometry |
| **Auto-clear** | No new warning before `BuildConfig.WARNING_TIMEOUT_MS` | `Idle`; scene cleared | Restore `previousMode` unless user overrode |
| **User override** | User leaves Warning while Active | Stays on chosen mode; flag set | No force-restore on Idle |

Message loss is therefore **visible as absence**: the UI does not hold a stale warning forever — it times out back to Idle. That is the IVI contribution to a **degraded / message-loss** scenario: expected result = warning disappears after timeout, not indefinite ghost C.

Code: [WarningViewModel.kt](../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/ui/WarningViewModel.kt), [MainViewModel.kt](../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/ui/MainViewModel.kt).

---

## 4. R19 provenance guard (trust, not syntax)

Syntax-valid JSON can still lie about provenance. M1 rule:

- **Draw relayed ghost C** only when `object.source == "v2x_relayed"`.
- **Any other source** → `drawUnknownSourceVehicle`: yellow `?` circle and `[? UNKNOWN SOURCE]` label; ERROR log.

This is separate from unknown `warningType` (wire preserved, warning still shown). Provenance guards **trust for drawing C**, not **whether a warning exists**.

Implementation: [CanvasWarningView.kt](../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/ui/view/CanvasWarningView.kt) — `drawUnknownSourceVehicle`, R19 comment block.

**IVI must not:** render a demo scene that hardcodes `v2x_relayed` when no message arrived — that spends R19 proof. Live path only for acceptance evidence.

---

## 5. Degraded scenarios for system test (IVI expected results)

Use these when composing a degrade/fault run with Scenario Player + ADA + pcap (lead-owned infrastructure). IVI supplies the **screen + log oracle** column.

| Scenario | Inject / fault | IVI expected result | Log oracle |
|---|---|---|---|
| **S1 — Malformed burst** | Send non-JSON UDP to `:47300` | No warning; app stays up | `[DROP]` lines; no `[RX]` for bad packets |
| **S2 — Message loss** | Stop ADA TX mid-warning | Warning clears after timeout; Home restored | `[RX]` stops; then Idle UI |
| **S3 — Delayed warning** | Late valid warning after idle | New `Active` cycle; fresh timeout | Single `[RX]` then Warning view |
| **S4 — Untrusted source** | Valid JSON, `source != v2x_relayed` | Warning channel may activate; C shown as `[?]` not ghost | ERROR provenance log |
| **S5 — Unknown warning type** | `r4-unknown-warning.json` sample | Warning UI; no crash | `[RX]`; wire type preserved in object |
| **S6 — Socket / eth fault** | IVI guest without Room eth IP | No `[RX]` from ADA path; loopback inject still works | Listener open log; no eth `[RX]` |

S1, S4, S5 have JVM/instrumentation coverage today; S2, S3, S6 are the Round 2 gaps best closed with a correlated run (video + per-node log + packet receipt).

---

## 6. What IVI deliberately does not do (M1)

| Situation | IVI does not |
|---|---|
| ADA silent | Invent a warning or keep ghost C indefinitely |
| Bad JSON | Partial-parse or “best effort” object |
| Unknown severity | Default to silent — unknown risk maps fail-safe high in renderer |
| Missing V2X | Fall back to camera or local detection |
| `state` heartbeat only | Raise warning (M1 UI is warning-event driven) |

Those belong to ADA/V2X or to a future contract change — not the display consumer.

---

## 7. Traceability

| Requirement | Fail-closed behaviour |
|---|---|
| **R4** | Malformed wire rejected at parser; consumer never throws across receive loop |
| **R16** | Wake-on-warning only on real `Active`; restore on Idle |
| **R17** | Scene from message geometry; provenance guard on C |
| **R19** | Ghost C only for relayed source; otherwise `[?]` |

**Related tests:** [R4DeserializerTest.kt](../../IVI_ECU/app/src/test/java/com/hackathon/v2x/ivi/R4DeserializerTest.kt), [R4AdditiveVersionTest.kt](../../IVI_ECU/app/src/test/java/com/hackathon/v2x/ivi/model/R4AdditiveVersionTest.kt), [WarningViewModelTest.kt](../../IVI_ECU/app/src/test/java/com/hackathon/v2x/ivi/WarningViewModelTest.kt).

---

## 8. Summary

- **Wire fail-closed:** drop malformed, preserve unknown types, never crash the listener.
- **Trust fail-closed:** do not draw ghost C without `v2x_relayed`.
- **Temporal fail-closed:** timeout clears stale warnings after message loss.
- **Evidence:** pair `[RX]`/`[DROP]` log lines with screen state for degrade runs — IVI is the human-visible oracle in the chain.
