# Knowledge base

This page contains links to article, highlighting the main knowledge base that helps us develop this project. 

## Android Framework 

| Document | Topic |
|---|---|
| [Android Automotive OS](android-automotive-os.md) | The platform as an application target: what it provides, and what it constrains |
| [How one screen appears in an Android Automotive app](android-screen-lifecycle.md) | Manifest to first frame, and why the receive loop lives in a service rather than the UI |
| [Parsing UDP messages on the IVI side](UDP-msg-parsing.md) | Deserialising a UDP JSON payload into typed Kotlin: buffer slicing, library choice, and the handling the library does not provide |
| [Simulating a UDP message producer to exercise its consumer](producer-simulation-harness.md) | What a producer stand-in must reproduce, and the event seam that makes one usable |
| [IVI HMI scalability](ivi-hmi-scalability.md) | What the M1 display workload can grow into along existing seams vs what needs contract or platform change |
| [IVI fail-closed warning display](ivi-fail-closed-warning-display.md) | Degraded and failure behaviour on the HMI: drop, timeout, provenance guard, and expected evidence for fault runs |

## V2X

## Object detection 

## CI