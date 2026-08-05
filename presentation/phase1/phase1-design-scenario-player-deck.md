---
marp: true
theme: default
paginate: true
title: Phase 1 — Scenario Player Design
description: Companion deck — the bench module architecture, its folder layout, its build toolchain, the scenario configuration format, and the encoder path that reuses the receiver's codec
deck: Phase 1 — Scenario Player Design · FPT Hackathon 2026
---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-title-city.jpg)

# Phase 1 — Scenario Player Design

## The bench, module by module

**Milestone 1 · Cooperative Vehicle Awareness — FPT Hackathon 2026**

Companion to [phase1-design-deck.html](phase1-design-deck.html), which defines the terminology — including *bench* — the blueprint and the two messages. Nothing from it is repeated here.

Source: [scenario-player-hld.md](../../Scenario_Player/doc/scenario-player-hld.md)

---

# Table of contents

1. **Module architecture** — the submodules and their responsibilities
2. **Folder layout** — where each module lives
3. **Build toolchain** — how the bench is built, tested and packaged
4. **Scenarios** — the configuration format, and the two committed situations
5. **The encoder path** — why the bench compiles the receiver's codec

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 01 · Module architecture

---

# The modules and their dependencies

![h:515 Scenario Player component architecture: the data, business-logic and controller subsystems, the C++ encoder helper behind its codec interface, and the node either end of the wire](../assets/phase1-des-arch-bench.svg)

---

# Reading the diagram

![h:470 The legend of the component diagram: the seven fill colours naming a component's role, and the notation for dependency, realization, node frames and test equipment](../assets/phase1-des-arch-legend.svg)

---

# What each submodule does

| Module | Responsibility |
| ------ | -------------- |
| `main.py` | The entrypoint the platform starts. Loads configuration, starts the helper process, runs the loop. No logic of its own |
| `player/config.py` | Reads the environment and loads and validates the scenario file into frozen records. The only module that reads the environment |
| `player/scenario.py` | The kinematic model: given a scenario time, it returns the sender's pose and the observed vehicle's relative state, in the units the air message uses |
| `player/generator.py` | The rate loop and the scenario clock: duration, repetition, and one log line per datagram |
| `player/encoder_client.py` | Drives the encoder helper as a persistent process, one JSON line in and one encoded line out, restarting it with backoff if it exits |
| `player/sender.py` | Transmits each encoded message as one UDP datagram. Transmission errors are logged and the bench stays alive |
| `player/contracts/cpm_content.py` | The message content record in wire-native units, delivered in Phase 0 |
| `codec_helper/` | The C++ encoder, built from byte-identical copies of the V2X ECU's codec sources |

- **The bench must remain observable.** No error path terminates it: an encode failure skips one message and the loop continues, and a helper process that exits is restarted.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 02 · Folder layout

---

# Abridged folder structure

```
Scenario_Player/
├── Dockerfile              two stages: compile the encoder, then a Python runtime carrying it
├── main.py                 the entrypoint, at the image working directory
├── requirements.txt        the runtime dependency: PyYAML
├── player/
│   ├── config.py           environment and scenario loading, the sole environment reader
│   ├── scenario.py         the kinematic model
│   ├── generator.py        the rate loop and scenario clock
│   ├── encoder_client.py   the helper-process client
│   ├── sender.py           UDP transmission
│   └── contracts/          the message content record                (Phase 0)
├── codec_helper/
│   ├── CMakeLists.txt      builds the encoder against the pinned codec
│   ├── cmake/              the pinned codec version, a synchronised copy
│   └── src/
│       ├── main.cpp        the command-line encoder: a streaming mode and a one-shot mode
│       └── codec/          synchronised copies of the receiver's codec — never edited here
├── scenarios/
│   ├── default.yaml        the observed vehicle approaching
│   └── c-out-of-range.yaml the observed vehicle stationary and distant
├── tests/                  10 suites, 116 tests
│   └── fixtures/golden/    the six reference messages, content and bytes
└── doc/                    this design, the call-flow source, the research notes
```

- **The synchronised copies are never edited here.** Their authority is the V2X ECU folder, and a copy gate fails the build if the two differ by a single byte.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 03 · Build toolchain

---

# How the bench is built, tested and packaged

| Concern | Tool | Detail |
| ------- | ---- | ------ |
| **Language** | Python 3.11 | Standard library throughout, with type hints and frozen records |
| **Runtime dependency** | PyYAML | The only runtime dependency; scenario files are the sole reason for it |
| **Encoder** | C++17 and CMake | The helper is compiled from the receiver's codec sources against the shared pinned version |
| **Unit tests** | pytest | 10 suites, 116 tests, all deterministic; clocks and processes are injected so no test waits |
| **Container image** | Docker, two stages | A compile stage builds the encoder; the runtime stage carries it beside the Python application |
| **Base image** | `python:3.11-slim` for both stages | Deliberate: builder and runtime share one base, so the compiled encoder cannot link against a library the runtime lacks. The CMake floor is met by a pip-installed CMake rather than the distribution's |
| **Verification** | GitHub Actions | `python-tests` runs the suite · `sp-codec-helper` builds the encoder and verifies it against the reference messages · `scenario-player-image` builds and pushes the image |

- **The encoder verification is byte-level.** For each of the six reference messages, the helper encodes the readable content and the result is compared against the committed bytes. Only an exact match passes.
- **One test skips locally and executes in CI.** The encoder comparison requires the compiled helper, so it is skipped when the binary is absent and runs unskipped in the lane that builds it.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 04 · Scenarios

---

# Scenarios are configuration, not code

One kinematic model serves every situation; a second traffic situation is a second file.

| Key | Meaning |
| --- | ------- |
| `name` · `duration_s` · `loop` | Identification, run length, and whether the scenario restarts |
| `cpm_rate_hz` | Messages per second, default 10 |
| `sender` | The transmitting vehicle: station identifier, latitude, longitude, heading |
| `object` | The observed vehicle: identifier, initial distance, closing speed, lateral offset, classification, confidence |

- **The model is constant-velocity.** The observed vehicle's distance is the initial distance less the closing speed multiplied by the elapsed scenario time; the lateral offset is fixed. Values are converted to the units the air message uses, and the measurement-time bound is asserted before encoding.
- **Two situations are committed:** the observed vehicle approaching from 60 m to approximately 10 m, and the observed vehicle stationary beyond the range at which later phases will drop it.
- **Switching between them requires no rebuild** — the active file is named by an environment variable, so the change is a node-configuration edit followed by a redeployment.

> This is the acceptance criterion for the bench: different configurations must produce observably different message streams. Expressing scenarios as data is what makes that true by construction.

---

<!-- _class: lead -->

![bg](../assets/bg-navy-motif.png)

# 05 · The encoder path

---

# Why the bench compiles the receiver's codec

The bench is written in Python; the codec is C++. Three approaches were considered.

| Approach | Assessment |
| -------- | ---------- |
| **A helper process** — chosen | A small C++ command-line encoder driven over standard input and output, one JSON line in and one encoded line out. Reuses the exact frozen codec, verifiable byte for byte, and the process-over-pipes pattern is already used elsewhere in this project |
| **A Python binding** — rejected | Introduces a new toolchain layer and cross-compilation friction inside the image build, for no gain in fidelity |
| **Pre-encoded messages with byte patching** — rejected | Diverges from the codec the moment the profile changes, and cannot satisfy the requirement that different configurations produce different streams |

- **Divergence is caught twice.** The copy gate proves the sources are byte-identical, and the encoder test proves the output bytes match the reference messages. Either check alone would be insufficient: identical sources could still be built against a different codec version, and matching bytes could still come from an edited copy.
- **The path is temporary by design.** If vehicle transmission returns to scope, the V2X ECU calls the same encoder in its own process and the helper is no longer the only encoding path — with no change to any contract.

---

# Open items

- **The distant-vehicle scenario is tuned against a threshold Phase 2 has not yet frozen.** Its distance deliberately exceeds the range at which later phases will drop a track; the pairing should be re-checked when those values are fixed.
- **The bench holds no capture.** The V2X ECU's interface sees both flows, so capture lives there; this node's evidence is its transmission log.

---

<!-- _class: lead -->
<!-- _paginate: false -->

![bg](../assets/bg-fpt-tower.jpg)

# Thank you

**Phase 1 — Scenario Player Design** · Milestone 1 · FPT Hackathon 2026
