# Phase 5 — Test harness: simulate ADA→IVI (R4)

How a sanctioned test tool invokes IVI R4 ingest and HMI without a live ADA container.

## Feasibility

- Achievable now: [IVI_ECU/mock-sender/](../../IVI_ECU/mock-sender/) already emits UDP R4 JSON cycles (approach → state → leave → unknown `warningType`).
- Preferred for Phase 5 parallel display work (report + [milestone1.md](../../plans/milestone1.md) Phase 5: mock-driven).
- Does not prove R15 emission quality; that stays on ADA + Phase 6.

## Solution comparison (producer side)

| Candidate | Pros | Cons | Verdict |
|---|---|---|---|
| **A. Python UDP mock-sender (existing)** | Matches frozen samples; Dockerizable; env-driven host/port; no ADA deps | Not real R15 edge logic | **Pick** — criterion 1–2 (works, fastest for Phase 5) |
| **B. Kotlin `:mock-sender` Gradle module** | Same language as IVI; in-process tests | Extra Android/JVM networking surface; plan mentioned it but repo standardized on Python | Reject for Room harness; optional only for pure JVM unit helpers |
| **C. Real ADA in mini-blueprint** | True emission path | Needs R2/fixture path; heavier | Use for ADA↔IVI integration, not first HMI loop |

Hard constraints: open-source + Linux container for CarSky — A and C qualify; B is local-only.

## Invocation modes

### 1. Local loopback (laptop / emulator)

```text
cd IVI_ECU/mock-sender
IVI_ECU_HOST=127.0.0.1 IVI_ECU_PORT=5004 CYCLES=2 python mock_r4_sender.py
```

IVI must listen on the same port (`BuildConfig.R4_UDP_PORT`, default 5004). See [phase5-ivi-deploy.md](../../IVI_ECU/deployment/phase5-ivi-deploy.md).

### 2. CarSky Room (mock container → Skycraft)

- Image: build/push `registry.carsky.io/m1-mock-r4-sender:latest` from `mock-sender/Dockerfile`.
- Blueprint: [blueprint-2node-task51-test.json](../../requirements/blueprint-2node-task51-test.json) + manual ethernet pins ([guide](../../requirements/blueprint-2node-task51-test-guide.md)).
- Env on mock node: `IVI_ECU_HOST=<ivi-pin-ip>`, `IVI_ECU_PORT=5004` (or 47300 if IVI build aligned).

### 3. Unit / instrumentation (no network)

- Feed fixture JSON from `contracts/samples/` into `R4Deserializer` / repository tests.
- Full-stack local: UDP loopback inside JVM tests (existing `FullStackIntegrationTest` pattern).

## What the harness must raise on IVI

| Packet | IVI expected behavior |
|---|---|
| `type=warning`, `warningType=nlos_obstruction` | Edge → Warning View; geometry ego/B/C; risk colors |
| `type=state` | Last-value-wins scene update by `seq` (if state path implemented) |
| `warningType` unknown | No crash; degrade; additive-version acceptance |
| `object.source != v2x_relayed` | Guard / badge path (R17 defensive) |

## Observer / event seam (for architecture)

- Transport: UDP datagram received → decode bytes as UTF-8 JSON → deserialize → emit domain event on a hot stream (`SharedFlow` / callback interface).
- UI observes ViewModel state; must not parse JSON on the main thread.
- Name the arrival event at the repository/service boundary (e.g. `R4Message` sealed hierarchy), not as a CarSky pin event.

## Related

- Mini real-ADA Room: [phase5-mini-blueprint-ada-ivi.md](../Design/IVI-ECU/phase5-mini-blueprint-ada-ivi.md)
- UDP Diagram parsing at IVI: [How IVI parses UDP Diagram at from ADA ECU](phase5-r4-parse-approach.md)
- Schema: [r4-ada-ivi.schema.json](../../contracts/r4-ada-ivi.schema.json)
