# Knowledge Base

Welcome to the **FPT Automotive Hackathon Knowledge Base**. This repository indexes core architectural guides, framework patterns, and protocol references used across our 4-node automotive system.

---

## 🚗 Android Framework & AAOS

| Document | Topic |
|---|---|
| [Android Automotive OS](android-automotive-os.md) | Platform overview: architecture, VHAL capabilities, foreground service constraints |
| [How one screen appears in AAOS](android-screen-lifecycle.md) | Manifest to first frame, and why the receive loop lives in a background service |
| [Parsing UDP messages on the IVI side](UDP-msg-parsing.md) | Deserialising UDP JSON payload into typed Kotlin: buffer slicing & kotlinx.serialization |
| [VHAL Property Routing: From Bus to Kotlin](vhal-property-routing-and-signal-pipeline.md) | 5-stage signal traversal across 2 Binder boundaries from CAN/VHAL to Kotlin UI |
| [Simulating a UDP message producer](producer-simulation-harness.md) | Producer simulation harness and event seam design |

---

## 🛰️ V2X & System Architecture

| Document | Topic |
|---|---|
| [M1 Cooperative Awareness](../Requirements/m1-cooperative-awareness.md) | Authoritative requirement specification for V2X CPM, NLOS relay, and R4 warning |
| [CarSky 4-Node Deployment](../../requirements/car-sky-guide/carsky-4-node-blueprint.md) | Network topology, IP mapping, and bridge configuration across Bench, V2X, ADA, and IVI |
| [AAOS Guest Deployment](../../requirements/car-sky-guide/deploy-ivi-hmi-walkthrough.md) | Step-by-step AAOS VM deployment, eth1 IP pin, and ADB APK installation walkthrough |