# Phase 5 IVI — research summary (mini Room, R4 harness, parse, implement notes)

Feature set: Milestone 1 **Phase 5** display track (R4 consumer side, R16, R17), plus a reduced CarSky topology for ADA↔IVI and a mock harness. Authoritative requirements remain R4/R16/R17 in [m1-cooperative-awareness.md](m1-cooperative-awareness.md); this file does **not** mint new global requirement numbers.

## Feasibility (whole input)

| Area | Verdict | Reasoning |
|---|---|---|
| Mock-driven IVI HMI (Phase 5) | Achievable | Stack frozen; mock-sender + AAOS APK path defined |
| Mini-blueprint ADA+IVI+bridge | Achievable with gap | Needs R2/fixture or ADA test inject if V2X absent; port align 47300 |
| R4 parse on IVI | Achievable | kotlinx.serialization per report |
| nlohmann submodule inside IVI | Infeasible as stated for M1 | C++/JNI on AAOS; conflicts with R4 IVI tech stack — keep nlohmann on ADA |

## Enumerated implementation-facing requirements (urgency order)

Ordering: **urgency** (what blocks HMI demo and Room smoke first).

1. **IVI binds UDP and parses R4 JSON** without crashing on unknown `warningType` — KPI: fixture round-trip + additive-version unit tests green; listen port from config.
2. **Arrival raises an observable event** to UI logic — KPI: warning packet → Warning View within timeout budget used by smoke checklist.
3. **R16 layout + R17 2D God View** from composed geometry; ghost C only `v2x_relayed` — KPI: visual ego/B/C; guard on bad source.
4. **Mock harness can invoke (1)–(3)** locally and on a 2-node Room — KPI: one scripted cycle completes without FATAL in logcat.
5. **Optional mini ADA+IVI Room** after ADA can emit — KPI: ≥1 warning observed at IVI on port 47300.

## Technical picks (summary)

| Decision | Pick | Drove by |
|---|---|---|
| IVI JSON | kotlinx.serialization | Report R4; criteria 1–2 |
| ADA JSON | nlohmann/json | Report R4 (producer) |
| Phase 5 producer | Python `mock-sender` | Criteria 1–2 |
| IVI UI | Compose + Canvas seam | Report §3(e) |
| M1 eth framing | Raw UDP JSON (no app header strip) | Frozen contract |

Detail notes (agent reference):

- [IVI_ECU/doc/research_notes/phase5-mini-blueprint-ada-ivi.md](../IVI_ECU/doc/research_notes/phase5-mini-blueprint-ada-ivi.md)
- [IVI_ECU/doc/research_notes/phase5-r4-simulation-harness.md](../IVI_ECU/doc/research_notes/phase5-r4-simulation-harness.md)
- [IVI_ECU/doc/research_notes/phase5-r4-parse-approach.md](../IVI_ECU/doc/research_notes/phase5-r4-parse-approach.md)
- [IVI_ECU/doc/research_notes/phase5-ivi-implementation-notes.md](../IVI_ECU/doc/research_notes/phase5-ivi-implementation-notes.md)

## Hand-off to project-architecture

**1st-choice solution for IVI ECU Phase 5 modules:**

1. **Contract library (Kotlin)** — R4/R3 models + kotlinx deserializer; optional Gradle submodule `:r4-contract` inside `IVI_ECU/` (not nlohmann).
2. **UDP / payload adapter** — foreground service; datagram bytes → UTF-8 JSON string (no M1 Ethernet app header).
3. **Observer** — repository `SharedFlow` / sealed `R4Message` events to ViewModels.
4. **Frontend** — Compose R16 shell; `IviWarningViewSeam` + Canvas R17.

Also design against: mock-sender harness; port externalization (5004 vs 47300); CarSky Skycraft deploy + ADB install constraints in the implementation notes.
