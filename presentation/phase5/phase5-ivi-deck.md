---
marp: true
theme: default
paginate: true
title: "Phase 5 — IVI HMI & 2D God View on AAOS"
description: "Report deck — Phase 5 IVI HMI on CarSky AAOS; NLOS warning, four-node architecture, R4 UDP pipeline, Home/Warning demo flow, Canvas God-View, test matrix, deploy evidence, learnings and next steps"
deck: "Phase 5 — IVI HMI & 2D God View · FPT Hackathon 2026"
---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-title-city.jpg)

# Phase 5 — IVI HMI & 2D God View on AAOS

## Cooperative awareness on the driver's screen

**Milestone 1 · Cooperative Vehicle Awareness — FPT Hackathon 2026**

IVI ECU · R4 ADA→IVI · Skycraft AAOS · 2026-08

Sources: [phase5_minh_tasks.md](../../plans/phase5_minh_tasks.md) · [phase5-ivi-hld.md](../../IVI_ECU/doc/phase5-ivi-hld.md) · [r4-ada-ivi.schema.json](../../contracts/r4-ada-ivi.schema.json) · [deploy-ivi-hmi-walkthrough.md](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md)

---

# Table of contents

1. **Problem** — NLOS hazard the ego sensors cannot see
2. **Architecture** — four-node path ending at the IVI guest
3. **R4 pipeline** — UDP bytes to Canvas
4. **Demo flow** — HomeView ↔ WarningView
5. **God-View** — Ego / B / Ghost C and the R19 guard
6. **Test matrix** — unit, integration, full-stack
7. **Deploy evidence** — APK, AAOS screens, logcat
8. **Learnings & next** — CarSky ADB, eth pin, R4 HMI; what stays additive

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 01 · Problem

---

# NLOS hazard the ego cannot see

- **Vehicle A** (ego) cannot detect **vehicle C** — line of sight is blocked by **vehicle B**.
- **Vehicle B** perceives C and relays that track over **V2X**; ADA fuses and emits an **R4** warning to the IVI.
- The IVI's job is not perception — it is **driver-facing awareness**: show the occluded C as a ghost on a 2D God-View so the driver acts before C becomes visible.
- Definition of done for M1 still ends at **R19**: one continuous run, zero direct C detections on A, ghost C rendered only from `v2x_relayed`.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 02 · Architecture

---

# Four-node path to the IVI guest

| Node | Role in Phase 5 | Address (Room eth) |
| --- | --- | --- |
| **Bench — Scenario Player** | Drives the occluded-C scenario | `10.99.0.10` |
| **V2X ECU** | Relays cooperative perception | `10.99.0.11` |
| **ADA ECU** | Risk + R4 publisher → IVI | `10.99.0.12` → `10.99.0.13:47300` |
| **IVI ECU (Skycraft AAOS)** | HMI: Home dashboard + Warning God-View | `10.99.0.13` |

- Ethernet Bridge ties the pins; IVI is a **Skycraft** guest (AAOS image from Artifacts), not a container image for the APK.
- Team APK is installed **into** the running guest with **ADB** — Artifacts hold the AAOS **Image + Host Package**, never the HMI APK.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 03 · R4 data pipeline

---

# UDP bytes to Canvas

| Stage | Component | Responsibility |
| --- | --- | --- |
| **Listen** | `R4ListenerService` (FGS) | Bind `BuildConfig.R4_UDP_PORT` (default **47300**); emit datagrams |
| **Parse** | `R4Deserializer` + kotlinx.serialization | Additive schema; preserve unknown `warningType` |
| **Store** | `R4Repository` | `SharedFlow` of validated `R4Message` |
| **UI state** | `WarningViewModel` | Idle ↔ Active; auto-clear after `WARNING_TIMEOUT_MS` (**10 s**) |
| **Mode** | `MainViewModel` | Wake-on-warning → `WarningView`; Idle restores previous (default **Home**) |
| **Render** | `CanvasWarningView` via `IviWarningViewSeam` | 2D God-View; Hilt provides the seam |

```text
ADA ──UDP/R4──▶ Listener ──▶ Deserializer ──▶ Repository
                                                  │
                         Canvas ◀── MainScreen ◀── ViewModels
```

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 04 · Demo flow

---

# HomeView to WarningView and back

1. **Idle** — `DisplayMode.HomeView` automotive dashboard; status shows link bound on `:{port}`.
2. **R4 arrives** — `WarningViewModel` → Active; `MainViewModel` forces `WarningView` (fade **200 ms**).
3. **God-View** — Canvas shows Ego, occluder B, and Ghost C from the R4 object snapshot.
4. **Timeout** — no refresh within `WARNING_TIMEOUT_MS` (**10 000 ms**) → Idle → restore previous mode (Home).
5. **Bench / mock** — Room path uses ADA; local/smoke can use `IVI_ECU/mock-sender` at `10.99.0.13:47300` with schema-complete `class` + `timestamps`.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 05 · God-View Canvas

---

# Ego, B, Ghost C, R19 guard

| Actor | Visual | Rule |
| --- | --- | --- |
| **Ego (A)** | Cyan | Own vehicle frame |
| **Occluder (B)** | Amber | Blocks line of sight |
| **Ghost C** | Dashed red + glow pulse (`GLOW_PERIOD_MS`) | Only when source is **`v2x_relayed`** |
| **Non-relayed C** | Yellow `[?]` + ERROR log | R19 provenance guard — do not paint a trusted ghost |

- Seam: `IviWarningViewSeam` → default `CanvasWarningView` (Hilt); optional 3D later without rewriting the mode switcher.
- `vehicleCSnapshot` is wired from the R4 object snapshot so the guard sees real provenance, not a null bypass.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 06 · Test matrix

---

# Unit, integration, full-stack

| Suite | File | Focus |
| --- | --- | --- |
| Unit | `R4DeserializerTest` | Schema parse, additive fields |
| Unit | `R4RoundTripTest` / `R4AdditiveVersionTest` | Wire fidelity |
| Unit | `R4RepositoryTest` | Flow emission |
| Unit | `WarningViewModelTest` | Active / Idle + timeout |
| Unit | `MainViewModelTest` | Wake-on-warning / restore |
| Unit | `CanvasWarningViewTest` / `SceneCoordinateMapperTest` | God-View mapping |
| Integration | `FullStackIntegrationTest` | Seam wiring + stack |

- CI lane: GitHub Actions **`phase5-ci`** (unit tests + `ivi-assemble` APK artifact).
- Target for jury narrative: **all green** on the Phase 5 test set before Room evidence.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 07 · Deploy evidence

---

# APK, AAOS screens, logcat

| Evidence | Status | Notes |
| --- | --- | --- |
| **APK size** | Local debug **~24.9 MB** (`26086479` bytes) | Must stay **&lt; 50 MB**; also via CI artifact `app-debug-apk` |
| **HomeView screenshot** | *Pending Room capture* | Devices → **IVI Screen** while Idle |
| **WarningView screenshot** | Partial — `ivi_warn_sim.png` (loopback R4 inject) | Still need eth-path ADA `[TX]` → IVI for full Room claim |
| **Logcat** | *Pending Room capture* | Tags: `R4ListenerService`, `R4Deserializer`, `WarningViewModel` |

**Install path (not Artifacts):** Room Running → ADB tunnel (`nydus-reach` / gateway) → `adb install -r app-debug.apk` → launch `com.hackathon.v2x.ivi/.MainActivity`.

*Replace the two screenshot rows and the logcat row with real assets under `presentation/assets/` once B-3 smoke completes; keep this table as the evidence index.*

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 08 · Learnings & next steps

---

# What we learned (Phase 5 IVI — Bach)

- **AAOS guest ≠ phone AOSP** — automotive feature + `minSdk`; Skycraft Artifacts = Image + Host Package; HMI APK is a separate **ADB** step, never an Artifacts upload.
- **CarSky ADB tunnel** — `reach-backend adb --gateway … --key a8k_… --port 5555` then `adb connect localhost:5555`; key is node-derived (`a8k_` + base64), not the long m2m API key.
- **Room eth vs loopback** — ADA targets `10.99.0.13:47300`; if the guest has no IPv4 on `eth1`, eth R4 never arrives while `nc` to `127.0.0.1:47300` still proves the listener.
- **Kotlin Flow / Compose Canvas / Hilt** — warning Active/Idle owns wake-on-warning; God-View behind `IviWarningViewSeam`; timeout and port from `BuildConfig` (default **47300**).
- **Runtime traps** — Hilt + missing Guava → `NoClassDefFoundError: ImmutableMap`; PowerShell UTF-8 BOM → `Malformed R4 payload` until JSON is written without BOM.

---

# Next steps (timeboxed, additive)

- **In-Room eth evidence** — pin IVI `ethernet` = `10.99.0.13` → bridge; capture ADA `[TX]` + IVI WarningView + logcat (not only loopback inject).
- **Live `R4LinkState`** — BOUND / REBINDING / ERROR on the status bar (shared / Vinh lane).
- **Module split + `:r4-simulator`** — Kotlin simulator replacing long-term Python mock.
- **Optional visuals** — 3D seam implementation; IVI dashcam only if video track unlocks (future register), never gating R19.

---

<!-- _class: lead -->

![bg](../assets/bg-fpt-tower.jpg)

# Thank you

## Phase 5 IVI — see the hazard before it can be seen

**Questions · PR #2 · `feat/phase5-ivi-hmi-dev`**

Sources: [phase5_tasks_for_teammate.md](../../plans/phase5_tasks_for_teammate.md) · [deploy-ivi-hmi-walkthrough.md](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md)
