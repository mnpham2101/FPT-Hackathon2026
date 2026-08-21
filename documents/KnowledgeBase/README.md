# Knowledge base

This page contains links to article, highlighting the main knowledge base that helps us develop this project. 

## Android Framework 

| Document | Topic |
|---|---|
| [Android Automotive OS](android-automotive-os.md) | The platform as an application target: what it provides, and what it constrains |
| [How one screen appears in an Android Automotive app](android-screen-lifecycle.md) | Manifest to first frame, and why the receive loop lives in a service rather than the UI |
| [Parsing UDP messages on the IVI side](UDP-msg-parsing.md) | Deserialising a UDP JSON payload into typed Kotlin: buffer slicing, library choice, and the handling the library does not provide |
| [Gateway MCU to Kotlin UI: Signal Pipeline & Protocol Decoupling](signal-pipeline-and-protocol-decoupling.md) | Architectural proof of 5-hop signal traversal (Gateway MCU ethernet0 to Kotlin UI) & protocol independence |
| [Simulating a UDP message producer to exercise its consumer](producer-simulation-harness.md) | What a producer stand-in must reproduce, and the event seam that makes one usable |
| [IVI HMI scalability](ivi-hmi-scalability.md) | What the M1 display workload can grow into along existing seams vs what needs contract or platform change |
| [IVI fail-closed warning display](ivi-fail-closed-warning-display.md) | Degraded and failure behaviour on the HMI: drop, timeout, provenance guard, and expected evidence for fault runs |

## V2X

| Document | Topic |
|---|---|
| [Multi-Dimensional Feasibility & Business Proof](v2x-business-and-technical-feasibility-study.md) | Proof of technical, financial (7.38x BCR), regulatory (Euro NCAP 2026+), and operational feasibility |
| [V2X Protocol Foundations: ITS-G5 and the ETSI CPM](v2x-protocol-foundations.md) | How a station joins the V2X radio network, how CPM is disseminated and what it carries, what sender identity means and whether M1 needs it, and the propagation effects (Doppler, path loss, congestion) a real deployment must budget for |
| [The limits of V2X CPM for cooperative-awareness warnings](v2x-cpm-limitations.md) | What CPM does and doesn't standardize, why thresholds can't be universal, broadcast-storm congestion control, and why a station ID doesn't prove lane relevance |

## Object detection

| Document | Topic |
|---|---|
| [ADA-ECU's perception limitations, and where each could go next](ada-perception-limitations.md) | Monocular-distance accuracy, live-feed detection range, sensor fusion, and the other risk categories not yet covered — current state and future direction for each |

## CI

| Document | Topic |
|---|---|
| [CI Workflows, YAML and GitHub Actions](ci-yml-github-actions-deck.md) | What a CI workflow file is, the Docker actions it performs, and the tagging limitation this project lives with |
