---
marp: true
theme: default
paginate: true
title: ADA ECU Phase 2–4
description: ADA ECU implementation report for Phase 2 contracts/store, Phase 3 video detector seam, and Phase 4 fusion warning runtime.
---

<!-- _class: lead -->
<!-- _paginate: false -->
![bg](../assets/bg-title-city.jpg)

# ADA ECU Phase 2–4

Contract → detection seam → fusion warning runtime

FPT Hackathon 2026 · Cooperative NLOS Awareness

---

# Agenda

1. ADA scope
2. Runtime architecture
3. Phase 2 deliverables
4. Phase 3 deliverables
5. Phase 4 deliverables
6. Verification and integration notes

---

<!-- _class: lead -->

# 01 · ADA Scope

What this branch owns

---

# ADA owns the ego-side fusion path

- Receives V2X-derived object C through R2.
- Receives own-camera object B through the R3 detector seam.
- Maintains the R3 track store and R13 admission state.
- Assesses NLOS obstruction risk.
- Emits R4 warning JSON to IVI over UDP.

---

# Out of scope for this MR

- Phase 1 V2X ECU CPM decode and R2 production.
- Phase 5 IVI app rendering and listener implementation.
- CarSky deployment execution and acceptance evidence collection.
- YOLO inference quality; Phase 3 currently provides the detector seam and placeholder backend.

---

<!-- _class: lead -->

# 02 · Runtime Architecture

Two inputs, one track store, one warning output

---

# ADA data flow

```text
V2X R2 UDP
  -> R2 mapper
  -> TrackStore source=v2x_relayed

Video detector R3 JSONL
  -> R3 mapper
  -> TrackStore source=own_sensor

TrackStore
  -> NLOS risk assessor
  -> R4 warning builder
  -> UDP R4 sender
```

---

# Contract seams

| Seam | Direction | ADA role |
|---|---|---|
| R2 | V2X ECU → ADA | Consume relayed object C |
| R3 | ADA internal | Store own-sensor B and relayed C as tracked objects |
| R4 | ADA → IVI | Produce warning geometry and risk state |

---

<!-- _class: lead -->

# 03 · Phase 2

Contract and track-store scaffold

---

# Phase 2 delivered

- C++17 ADA runtime scaffold.
- Config-driven gate constants and UDP ports.
- R2/R3/R4 local schemas and samples.
- R3 `TrackStore` with source-aware objects.
- R13 lifecycle: `not_tracked`, `tentative`, `tracked`, timeout/exit.
- JSONL evidence logs for input, transitions, risk, and R4 TX.

---

# Phase 2 evidence

| Check | Evidence |
|---|---|
| C++ build | `cmake --build ADA_ECU/build-runtime` |
| Unit tests | `ctest --test-dir ADA_ECU/build-runtime --output-on-failure` |
| R4 smoke | `ada_ecu --mock` |
| Logs | `own_sensor_rx`, `r2_rx`, `track_transition`, `risk_event`, `r4_tx` |

---

<!-- _class: lead -->

# 04 · Phase 3

Video detector skeleton

---

# Phase 3 delivered

- OpenCV video decode skeleton.
- Synthetic mode for deterministic testing.
- Placeholder backend before YOLO.
- R3 JSONL output for `own:B`.
- Smoke test for detector path.

```sh
python ADA_ECU/tools/video_detector.py --synthetic 2
```

---

# Detector output contract

```json
{
  "id": "own:B",
  "class": "vehicle",
  "source": "own_sensor",
  "distance": 12.0,
  "state": "tentative"
}
```

The detector does not decide NLOS risk; it only feeds B into the R3 store.

---

<!-- _class: lead -->

# 05 · Phase 4

Fusion and warning runtime

---

# Phase 4 delivered

- UDP R2 receiver for live/mock V2X input.
- Source-specific timeout: V2X expiry does not remove own-sensor B.
- NLOS risk transition emission:
  - enter gate → high-risk warning
  - repeated in-gate update → no duplicate
  - exit/timeout → low-risk transition
- R4 sender toward IVI.

---

# R4 warning shape

```json
{
  "type": "warning",
  "warningType": "nlos_obstruction",
  "riskState": "high",
  "geometry": {
    "ego": { "x": 0, "y": 0 },
    "vehicleB": { "x": 12, "y": 0.2 },
    "vehicleC": { "x": 37.4, "y": 1.4 }
  }
}
```

---

# Deployment overrides

Local defaults remain developer-friendly:

```conf
ada_listen_port=46002
ivi_port=46004
```

CarSky deployment overrides through env:

```sh
V2X_LISTEN_PORT=47200
IVI_HOST=10.99.0.13
IVI_PORT=47300
```

---

<!-- _class: lead -->

# 06 · Verification

What has been checked

---

# Validation status

| Validation | Status |
|---|---|
| C++ build | Pass |
| ADA unit tests | Pass |
| Python syntax compile | Pass |
| Phase 3 detector smoke | Pass |
| ADA mock loopback | Pass |

---

# Integration requests

| Scope | Request |
|---|---|
| Phase 1 / V2X ECU | Send R2 to ADA deploy port `47200`. |
| Phase 5 / IVI ECU | Consume `geometry.vehicleB`, `geometry.vehicleC`, and `riskState = low | medium | high`. |
| CarSky deployment | Use env overrides instead of changing local config. |

---

<!-- _class: lead -->
<!-- _paginate: false -->
![bg](../assets/bg-fpt-tower.jpg)

# ADA Phase 2–4

Ego-side fusion path is ready for MR and integration.
