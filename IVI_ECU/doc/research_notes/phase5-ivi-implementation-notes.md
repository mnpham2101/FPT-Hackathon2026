# Phase 5 — Other necessary info to implement IVI ECU

Checklist beyond blueprint / mock / parse. Requirements: **R4, R16, R17** (Phase 5 in [milestone1.md](../../../plans/milestone1.md)); node guide [node-ivi-ecu.md](../../../requirements/car-sky-guide/node-ivi-ecu.md).

## Runtime & artifact

| Item | Value |
|---|---|
| Node | Skycraft AAOS guest |
| Language | Kotlin, Jetpack Compose, AndroidX |
| Artifact | Debug/release APK via `IVI_ECU/gradlew`; install post-deploy via ADB |
| minSdk / targetSdk | 29 / 33 (AAOS Skycraft baseline) |
| JDK for Gradle | 17 or 21 — **not** JDK 25 (Kotlin/Gradle parse failure) |

## Ports (do not hardcode in source)

| Context | Port | Notes |
|---|---|---|
| Production ADA→IVI | **47300** | ADA `IVI_ECU_PORT`; IVI must listen here on M1 Room |
| Phase 5 mock / 2-node | **5004** | `BuildConfig.R4_UDP_PORT` default + mock-sender |
| Warning auto-clear | `WARNING_TIMEOUT_MS` | BuildConfig / config — no literals in logic |

Align producer and consumer before claiming Room integration.

## HMI (R16 / R17)

- Layout: central Display Area + button/app areas ([ivi-ecu.svg](../../../requirements/ivi-ecu.svg)).
- Wake-on-warning: Active R4 warning forces Warning View; restore prior mode on Idle unless user overrode.
- God View behind `IviWarningViewSeam`; M1 delivers **2D Canvas**; 3D optional/stub.
- Ghost C only from `v2x_relayed`; unknown source → visible guard, not silent accept.

## Platform / deploy constraints

- Ethernet pins often **missing after JSON import** — add and wire in Nydus UI.
- APK is **not** in the AAOS image — ADB install after Running.
- Devices widget **IVI ADB** = in-browser shell; local `adb install` needs **ADB tunnel** (API/MCP `vm_tunnel_open` / `adb-tunnel`). Guest often has **no outbound internet** (cannot curl public APK URLs).
- Screenshot / REST shell may return 502 on some deployments — plan evidence via logcat + Screen widget when APIs fail.

## Module boundaries (input to architecture)

Independent modules requested for HLD (map to MVC):

| Module | Responsibility | Stack pick |
|---|---|---|
| Contract / parse library | R4(+R3 embed) models + JSON deserialize | Kotlin + kotlinx.serialization (`:r4-contract` or `app` packages) — **not** nlohmann on IVI |
| Transport serializer / UDP adapter | Receive datagram; treat bytes as JSON payload (no M1 eth header) | Kotlin service on `Dispatchers.IO` |
| Observer / event bus | Raise on message arrival → domain flows | `SharedFlow` / repository |
| Frontend | R16 shell + R17 seam/Canvas | Compose + ViewModels + Hilt |

## Out of scope for Phase 5 IVI code

- Implementing ADA R14/R15 internals.
- Full R19 recording pipeline (Phase 6+).
- Baking APK into ANDROID_IMAGE artifact.

## Related research notes

- [phase5-mini-blueprint-ada-ivi.md](phase5-mini-blueprint-ada-ivi.md)
- [phase5-r4-simulation-harness.md](phase5-r4-simulation-harness.md)
- [phase5-r4-parse-approach.md](phase5-r4-parse-approach.md)
- Existing mock path: [task51-2node-blueprint-answer.md](../../../plans/doc/task51-2node-blueprint-answer.md)
