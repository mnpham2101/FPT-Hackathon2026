---
marp: true
theme: default
paginate: true
title: Phase 5 — IVI ECU Design
description: Companion design deck — IVI module architecture, folder layout, toolchain, R4 call flow, configuration and observation; fills the gap vs V2X/ADA design decks
deck: Phase 5 — IVI ECU Design · FPT Hackathon 2026
---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-title-city.jpg)

# Phase 5 — IVI ECU Design

## The consumer node, module by module

**Milestone 1 · Cooperative Vehicle Awareness — FPT Hackathon 2026**

Companion to the demo deck [phase5-ivi-deck.html](phase5-ivi-deck.html) (problem, God View, evidence). This deck is the **design** report — same altitude as [phase1-design-v2x-ecu-deck.html](../phase1/phase1-design-v2x-ecu-deck.html).

Source: [ivi-ecu-hld.md](../../documents/Design/IVI-ECU/ivi-ecu-hld.md) · [ivi-ecu-design-decisions.md](../../documents/Design/IVI-ECU/ivi-ecu-design-decisions.md)

Learning notes for Lead’s `documents/` folder: [documents/](../../documents/)

---

# Table of contents

1. **Module architecture** — MVC layers and containment rules
2. **Folder layout** — where each component lives
3. **Build toolchain** — Gradle, AAOS, tests, APK
4. **Call flow** — bind, receive, wake-on-warning, render
5. **Configuration** — BuildConfig and blueprint pin
6. **Observation** — log surfaces and demo evidence

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 01 · Module architecture

---

# What the IVI node is for

- **Input:** R4 JSON datagrams from the ADA ECU (or `m1-r4-sim`) on UDP **47300**.
- **Output:** R16 HMI on AAOS — Display Area switches to the R17 **2D God View**; ghost C only from `v2x_relayed`.
- **Not in scope:** producing R4; ADA perception; V2X modem; uploading the APK as a CarSky Artifact.

---

# MVC separation (HLD)

| Layer | Examples | Rule |
| --- | --- | --- |
| **Data** | R4 / R3 models, `R4Repository` | Store and route; no UI meaning |
| **Business logic** | Deserializer, socket observer, classifier, coordinate mapper | No Compose / Activity types where the HLD places pure JVM |
| **UI logic** | `WarningViewModel`, `MainViewModel`, `DisplayMode` | State machines; no Canvas drawing |
| **UI** | `MainScreen`, `CanvasWarningView` behind `IviWarningViewSeam` | Draw and layout only |

- **Parse never draws.** Compose never opens a socket.
- **Seam `IviWarningViewSeam`** is the swap point for an optional 3D renderer without changing consumers.

---

# Containment rules that define the design

- **One datagram = one R4 JSON object** — no app-level Ethernet header; de-framing is buffer slicing (D3).
- **Unknown `warningType` preserved on the wire** — classification at the UI edge (D4).
- **Foreground service hosts the receive loop** — not the Activity lifecycle (D5).
- **Provenance guard** — ghost C renders only when snapshot `source` is `v2x_relayed` (R19).
- **No tunable literals in Kotlin** — port, timeout, scale from `BuildConfig` / runtime config (D10).

Decisions D1–D13: [ivi-ecu-design-decisions.md](../../documents/Design/IVI-ECU/ivi-ecu-design-decisions.md).

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 02 · Folder layout

---

# Abridged tree (design target + shipped app)

```
IVI_ECU/
├── settings.gradle.kts          shipped M1: include(":app")
├── contracts/                   byte-synced R4 / R3 schema copies
├── app/                         AAOS APK — Activity, Service, Compose, tests
│   ├── src/main/AndroidManifest.xml
│   └── src/main/java/…/ivi/
│       ├── MainActivity.kt · IviApplication.kt
│       ├── service/R4ListenerService.kt
│       ├── data/R4Deserializer.kt · R4Repository.kt
│       ├── model/               R4 / R3 / SceneGeometry
│       └── ui/                  ViewModels · MainScreen · view seam
├── mock-sender/                 interim Python R4 harness
└── doc/                         HLD · decisions · PlantUML · wiki
```

- **HLD designates five Gradle modules** (`:contract`, `:serializer`, `:observer`, `:app`, `:r4-simulator`) for a one-way JVM graph (D1–D2).
- **Shipped tree today** concentrates those packages under `:app` (Hilt). The design report names **components**; module split is an additive refactor, not a change to R4 behaviour.

---

# What each package does

| Package / artifact | Responsibility |
| --- | --- |
| `model/` | Sealed `R4Message`, `SceneGeometry`, R3 snapshot |
| `data/R4Deserializer` | Bytes → typed message or failure |
| `data/R4Repository` | Fan-out warnings / last state |
| `service/R4ListenerService` | UDP bind, IO loop, `r4EventFlow` |
| `ui/WarningViewModel` | Idle ↔ Active + `latestScene` + timeout |
| `ui/MainViewModel` | Display Area mode + wake-on-warning |
| `ui/screen/MainScreen` | R16 chrome |
| `ui/view/*` | Seam + Canvas God View + mapper + guard |
| `mock-sender/` | Scripted UDP to `.13:47300` for bench tests |

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 03 · Build toolchain

---

# How the node is built, tested and installed

| Concern | Tool | Detail |
| --- | --- | --- |
| **Language** | Kotlin | Compose UI; kotlinx.serialization for R4 |
| **Build** | Gradle + AGP | Module `:app`; JDK 17/21 |
| **DI (shipped)** | Hilt 2.58 | Activity / Service / ViewModel injection |
| **Unit tests** | JUnit · MockK · Turbine · Robolectric | ViewModels, deserializer, Canvas guards, full-stack UDP |
| **Artifact** | Debug APK | `assembleDebug` → `app-debug.apk` (&lt; 50 MB) |
| **Install** | ADB into Skycraft guest | Never an Artifacts “AAOS APK” upload — Image + Host Package only |
| **CI** | phase5-ci (team lane) | Build / test; APK as workflow artifact when green |

- **AAOS guest** comes from the platform AAOS artifact; the **team APK** is a separate ADB step ([node-ivi-ecu.md](../../requirements/car-sky-guide/node-ivi-ecu.md)).

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 04 · Call flow

---

# Part 1 — start-up

1. Launcher Intent → `MainActivity`.
2. `startForegroundService(R4ListenerService)` — socket opens on `R4_UDP_PORT`.
3. Compose `setContent` — `WarningViewModel` + bind service → `attachService(r4EventFlow)`.
4. Default Display Area = **HomeView**; status aims at **BOUND :47300**.

Learning walkthrough: [documents/KnowledgeBase/android-screen-lifecycle.md](../../documents/KnowledgeBase/android-screen-lifecycle.md).

---

# Part 2 — reception and decode

1. ADA `sendto` → guest `:47300`.
2. `DatagramSocket.receive` on IO dispatcher.
3. `R4Deserializer` — warning or state, or skip malformed.
4. Emit on `r4EventFlow` → repository routes warnings.

Learning walkthrough: [documents/ivi-r4-observation-pipeline.md](../../documents/Design/IVI-ECU/ivi-r4-observation-pipeline.md).

---

# Part 3 — wake-on-warning and God View

1. `WarningViewModel` → `Active` + `latestScene` (`geometry` + `objectSnapshot`).
2. `MainViewModel` forces **WarningView**.
3. `CanvasWarningView.Render` — ego / B solid; ghost C dashed if `v2x_relayed`.
4. Silence &gt; `WARNING_TIMEOUT_MS` → Idle → restore previous mode (unless user override).

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 05 · Configuration

---

# Values the design externalises

| Key | Default | Meaning |
| --- | --- | --- |
| `R4_UDP_PORT` | `47300` | Listener port (blueprint ADA→IVI) |
| `WARNING_TIMEOUT_MS` | `10000` | Auto-dismiss Active → Idle |
| `R4_SOCKET_BUFFER_BYTES` | `2048` | Datagram buffer (HLD) |
| Scene scale | `BuildConfig` / D10 | Metres→canvas base scale |
| IVI pin address | `10.99.0.13` | CarSky ethernet pin — **not** a Kotlin literal |
| ADA env | `IVI_ECU_HOST` / `IVI_ECU_PORT` | Producer targets this node |

- Override path (HLD D10): launch extras such as `--ei r4_port` merged in runtime config when wired.
- Skycraft **image** block = AAOS VM artifact IDs; Part Prefix (e.g. `ivi`) names `ivi-screen` / `ivi-logcat` widgets.

---

# Observation — how the design is evidenced

| Surface | What it proves |
| --- | --- |
| **Screen widget** | God View / Home chrome (R16 / R17) |
| **Guest logcat** | Bind, RX/DROP, UI mode transitions |
| **ADA View Log** | `[TX] … → 10.99.0.13:47300` |
| **tcpdump / `[CAP]` on ADA** | Wire presence of the ADA→IVI hop (producer-side) |

- Demo deck carries screenshots and learnings: [phase5-ivi-deck.html](phase5-ivi-deck.html).
- Procedure: [deploy-ivi-hmi-walkthrough.md](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md) §4.8–4.9.

---

# Open / additive items (design)

- **Gradle module split** (`:contract` / `:serializer` / `:observer` / `:r4-simulator`) — HLD D1–D2; behaviour unchanged when landed.
- **Live `R4LinkState` on the status bar** — BOUND / REBINDING / ERROR as first-class UI.
- **Kotlin `:r4-simulator` image** replacing long-term Python mock.
- **3D seam implementation** — optional; Canvas remains M1.

None of the above gates the R19 claim when the continuous eth path + `v2x_relayed` God View are demonstrated.

---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-fpt-tower.jpg)

# Thank you

**Phase 5 — IVI ECU Design** · Milestone 1 · FPT Hackathon 2026

Sources: [ivi-ecu-hld.md](../../documents/Design/IVI-ECU/ivi-ecu-hld.md) · [documents/](../../documents/)
