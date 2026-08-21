# Knowledge base

This page contains links to article, highlighting the main knowledge base that helps us develop this project. 

## Android Framework 

| Document | Topic |
|---|---|
| [Android Automotive OS](android-automotive-os.md) | The platform as an application target: what it provides, and what it constrains |
| [How one screen appears in an Android Automotive app](android-screen-lifecycle.md) | Manifest to first frame, and why the receive loop lives in a service rather than the UI |
| [Parsing UDP messages on the IVI side](UDP-msg-parsing.md) | Deserialising a UDP JSON payload into typed Kotlin: buffer slicing, library choice, and the handling the library does not provide |
| [Simulating a UDP message producer to exercise its consumer](producer-simulation-harness.md) | What a producer stand-in must reproduce, and the event seam that makes one usable |

## V2X

| Document | Topic |
|---|---|
| [The limits of V2X CPM for cooperative-awareness warnings](v2x-cpm-limitations.md) | What CPM does and doesn't standardize, why thresholds can't be universal, broadcast-storm congestion control, and why a station ID doesn't prove lane relevance |

## Object detection 

## CI