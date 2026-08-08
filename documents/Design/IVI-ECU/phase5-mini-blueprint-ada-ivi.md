# Phase 5 — Mini-blueprint: ADA ECU + IVI ECU + Ethernet Bridge

Approach to stand up a reduced CarSky Room that exercises ADA→IVI (R4) without Bench or V2X. Complements the existing mock-only 2-node path in [blueprint-2node-task51-test-guide.md](../../../requirements/blueprint-2node-task51-test-guide.md).

## Feasibility

- Achievable for integration smoke once ADA can emit R4 (Phase 4 R15 path) and IVI listens on the same UDP port.
- At-risk without ADB tunnel / APK install on Skycraft (platform shell/screenshot routes may be unavailable).
- Does not replace R19 (needs full chain); it only shortens ADA↔IVI debugging.

## Topology

```
[ADA ECU] ──eth──► [Ethernet Bridge] ◄──eth── [IVI ECU]
  Container                                 Skycraft AAOS
  10.99.0.12                                10.99.0.13
  send UDP → .13:47300                      listen UDP :47300
```

| Node | Type | Address | Role |
|---|---|---|---|
| ADA ECU | Container | `10.99.0.12` | Emit R4 JSON (warning ± optional state) |
| Ethernet Bridge | Bridge | `10.99.0.1` / `10.99.0.0/24` | R6 L2 fabric only (no app UDP port) |
| IVI ECU | Skycraft | `10.99.0.13` | Bind R4 listener; render R16/R17 HMI |

Authoritative production ports: [m1-cooperative-awareness.md](../../../requirements/m1-cooperative-awareness.md) baseline table; ADA env in [node-ada-ecu.md](../../../requirements/car-sky-guide/node-ada-ecu.md); IVI artifact/pin in [node-ivi-ecu.md](../../../requirements/car-sky-guide/node-ivi-ecu.md).

## Why not reuse the 2-node mock blueprint as-is

| | Mock 2-node (Task 5.1) | Mini ADA+IVI (this note) |
|---|---|---|
| Producer | `m1-mock-r4-sender` container | Real `ADA_ECU` image |
| Subnet example | `10.88.0.0/24` | Prefer `10.99.0.0/24` (M1) |
| UDP port | **5004** (dev) | **47300** (production R4) |
| Purpose | HMI/data-layer without ADA | Contract + emission alignment with ADA |

Use mock 2-node for Phase 5 display-track parallel work; use this mini-blueprint when validating live ADA payloads before Phase 6 full chain.

## Build steps (Nydus)

1. New Blueprint (or clone M1 and delete Bench + V2X nodes).
2. Add **Ethernet Bridge** + **Container (ADA)** + **Skycraft (IVI)**.
3. Attach IVI VM artifact (`AAOS` / ids in [node-ivi-ecu.md](../../../requirements/car-sky-guide/node-ivi-ecu.md)).
4. Set ADA `config.image` to the pushed `ada-ecu` registry tag; set env at least: `IVI_ECU_HOST=10.99.0.13`, `IVI_ECU_PORT=47300` (plus ADA’s V2X listen vars only if that hop is present — omit or stub if V2X is absent).
5. **Manually** add one `ethernet` pin per role node (JSON import drops them); static addresses `.12` / `.13`; wire both to the bridge (`direction: OUTPUT` on both role pins per platform convention).
6. Align IVI listen port with ADA send port: `BuildConfig.R4_UDP_PORT` / flavor must be **47300** for this Room (dev mock Rooms may keep 5004).
7. Deploy Room → wait Running → ADB-install team APK → drive ADA to emit ≥1 R4 warning.

## ADA without V2X/Bench (gap)

- Stock ADA expects R2 ingress on `V2X_LISTEN_PORT` (47200). A mini-blueprint with no V2X needs either: (a) a tiny R2 injector container on the same bridge, or (b) an ADA test mode / fixture path that synthesizes tracks and calls the R15 emitter, or (c) keep using [mock-sender](../../../IVI_ECU/mock-sender/) instead of real ADA until Phase 4 emission is fixture-triggerable.
- Flag for planner: do not claim “ADA+IVI only” end-to-end unless one of (a)–(c) is in the task plan.

## Acceptance checks for this Room

- All three nodes reach Running (bridge may not count in “N/M ECUs”).
- Capture or ADA log shows UDP to `10.99.0.13:47300`.
- IVI logcat (`R4ListenerService` / `IVI_V2X`) shows deserialize success; Warning View shows ego, B, ghost C with `object.source == v2x_relayed`.
- Unknown `warningType` still degrades (R4 additive-version).

## Related

- Full 4-node: [carsky-4-node-blueprint.md](../../../requirements/car-sky-guide/carsky-4-node-blueprint.md)
- Mock path: [blueprint-2node-task51-test.json](../../../requirements/blueprint-2node-task51-test.json)
- Port 5004 vs 47300: [phase5-ivi-implementation-notes.md](phase5-ivi-implementation-notes.md)
