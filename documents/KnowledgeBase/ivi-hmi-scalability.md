# IVI HMI scalability

**Author:** Vũ Xuân Bách · lane IVI Phase 5  
**Scope:** the `IVI_ECU/` workload on CarSky — an AAOS guest application that consumes R4 and renders the Display Area. It simulates the **role** of an IVI ECU on the blueprint; it is not a physical ECU, vECU, or HIL unit.  
**Authorities:** [ivi-ecu-hld.md](../Design/MODULE-DESIGN/IVI-ECU/ivi-ecu-hld.md) · [m1-cooperative-awareness.md](../Requirements/m1-cooperative-awareness.md) (R4, R16, R17, R19) · [m1-future-features-register.md](../Requirements/future/m1-future-features-register.md)

This note answers: what the M1 IVI can grow into without breaking contracts, what requires contract or platform change, and where the hard ceiling sits. It does not restate the HLD component list.

---

## 1. M1 baseline (what is fixed today)

| Dimension | M1 shape | Why it is enough for the demo |
|---|---|---|
| **Role** | R4 consumer only — no V2X stack, no fusion, no object detection | ADA decides warnings; IVI displays them |
| **Input** | One UDP listener on `BuildConfig.R4_UDP_PORT` (default 47300), JSON R4 datagrams | Single producer (ADA) on the Room Ethernet bridge |
| **Output** | R16 shell (side buttons + Display Area) and R17 God View 2D via `IviWarningViewSeam` | One warning channel, one committed renderer |
| **State** | Edge-triggered `warning` events; `state` heartbeats collected but not driving UI in M1 | Warning lifecycle is event + timeout, not full world-model mirror |
| **Provenance** | Ghost C drawn only when `object.source == v2x_relayed` ([CanvasWarningView.kt](../../IVI_ECU/app/src/main/java/com/hackathon/v2x/ivi/ui/view/CanvasWarningView.kt)) | R19 UI guard — screen must not invent relayed hazards |
| **Deploy unit** | One APK per IVI node, installed by ADB onto the AAOS Skycraft guest | CarSky blueprint supplies the VM image, not the HMI binary |

---

## 2. Scale along existing seams (no R4 change)

These extensions reuse frozen contracts and the module boundaries in the HLD.

| Extension | Seam / hook | Effort class | Notes |
|---|---|---|---|
| **3D God View** | `IviWarningViewSeam` — swap `CanvasWarningView` for a SceneView/Filament implementation | Medium | R17 optional path; 2D remains the committed M1 deliverable |
| **2D ⇄ 3D toggle** | Same seam + `DisplayMode` or in-view toggle | Low–medium | Listed in [m1-future-features-register.md](../Requirements/future/m1-future-features-register.md) |
| **Multiple warning types on screen** | Preserve wire `warningType`; classify in presentation layer | Low | Parser already preserves unknown values ([UDP-msg-parsing.md](UDP-msg-parsing.md) §4.1); styling maps per type |
| **Criticality filter** | Filter in `WarningViewModel` or repository before `Active` | Low | Future register: user opts into severity threshold only |
| **Richer R16 channels** | Add `DisplayMode` values and side-button targets | Low | M1 ships Home + Warning; navigation/media/settings are additive |
| **Configurable timeout / port** | `IviRuntimeConfig` / blueprint env (HLD D10) | Low | Today: `BuildConfig`; runtime merge unlocks per-deployment tuning without rebuild |
| **Structured evidence logging** | `R4Logger` seam (HLD) | Medium | M1 uses ad hoc `IVI_V2X` tags; structured `key=value` lines scale log analysis |
| **Higher message rate** | IO-thread receive loop + `SharedFlow` buffer | Low | M1 budget is 1–10 msg/s; buffer size and `tryEmit` policy are the knobs before architectural change |

**Rule:** if the ADA→IVI JSON shape and UDP port stay the same, IVI scales by adding view-models, renderers, and configuration — not by growing perception logic inside the app.

---

## 3. Scale that needs contract or upstream change

| Capability | Blocker | What must change |
|---|---|---|
| **Live camera in Display Area** | R16 today is God View + deferred dashcam | Video feed path (HTTP from ADA or local clip); see future register |
| **Multi-process wake-on-warning** | M1 is single APK, in-process `DisplayMode` switch | Android multi-process + IPC; optional R16 path in a later milestone |
| **Map / absolute coordinates** | R4 geometry is ego-relative metres only | GNSS or map contract extension |
| **Multiple simultaneous ghost tracks** | M1 single-object NLOS scenario | R4 `geometry` / `vehicles` shape and ADA emission policy |
| **B-only warning before C admitted** | R4 requires `object` snapshot for C | Contract change per future register |
| **Direct V2X on IVI** | Out of scope — IVI is not R1/R7 consumer | Would duplicate V2X ECU; violates node ownership |

IVI must not absorb ADA fusion or V2X decode to “scale faster” — that breaks the blueprint boundary and duplicates nodes that already exist.

---

## 4. Platform and deployment scale

| Layer | M1 | Scale path | Limit |
|---|---|---|---|
| **CarSky node** | One IVI guest, one eth pin (`10.99.0.13`), one Room | More Rooms / blueprints for parallel test; clone pin plan per deployment | Platform eth pin assignment is manual; guest must receive IPv4 on `eth1` |
| **AAOS guest** | Skycraft image + host package from blueprint; APK via ADB | Same pattern for each new Room; CI `app-debug-apk` artifact | APK not in blueprint — reinstall after redeploy |
| **Process model** | Foreground `R4ListenerService` + bound Activity | Heavier workloads (3D, video decode) need GPU/memory headroom on guest | Bench guest ≠ production head-unit silicon |
| **Instances** | One listener socket per app instance | Second IVI node = second app instance + second IP/port contract | Not multi-tenant inside one APK without design change |

**Wording for external readers:** call it an **IVI workload** or **IVI node** on CarSky, not “the physical IVI ECU in the vehicle.”

---

## 5. Performance and reliability headroom

| Concern | M1 behaviour | Scale lever |
|---|---|---|
| **Receive loop** | Blocking `DatagramSocket.receive` on `Dispatchers.IO`; malformed packets dropped, loop continues | Larger buffer, `tryEmit`, back-pressure policy ([UDP-msg-parsing.md](UDP-msg-parsing.md)) |
| **UI thread** | Deserialize off main; Compose draws from `StateFlow` | Keep all socket work in service scope |
| **Warning lifetime** | `WARNING_TIMEOUT_MS` auto-clears `Active` → `Idle` | Tune via config; shorter timeout = faster recovery after message loss |
| **Failure visibility** | `[RX]` / `[DROP]` on `IVI_V2X`; max UDP retries → `serviceError` | Degraded-run evidence: correlate screen idle with `[DROP]` or silence |

Measured M1 bench outcome (team delivery): warning UI roughly **101 ms** after CPM in the virtual same-lane scenario — latency headroom exists before UI becomes the bottleneck; upstream fusion and network hops dominate.

---

## 6. Traceability (ASPICE-oriented)

| Requirement | M1 IVI responsibility | Scale notes |
|---|---|---|
| **R4** | Consume JSON warning/state; fail-closed on malformed wire | Additive fields via `ignoreUnknownKeys`; new types preserved on wire |
| **R16** | Display Area modes; wake-on-warning via `MainViewModel` | More modes = more `DisplayMode` values, same shell |
| **R17** | God View 2D committed; 3D optional via seam | New renderer = new seam implementation |
| **R19** | UI provenance guard for ghost C | Guard stays in renderer regardless of 2D/3D |

**Test hooks:** `:serializer` round-trip tests, `WarningViewModelTest`, integration tests on `:app` — scale by adding contract samples, not by skipping the JVM module boundary.

---

## 7. Summary

- **Scales cheaply:** renderers, display modes, warning presentation, logging, runtime config, message rate within one R4 stream.
- **Scales with contract/platform work:** camera, maps, multi-track, multi-process, anything that smells like perception or V2X.
- **Does not scale by merging nodes:** IVI stays the last hop — consume R4, prove R19 on screen, evidence the outcome.
