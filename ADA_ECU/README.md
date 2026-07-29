# ADA_ECU — ADA ECU node (R3, R12–R15)

Ego's perception and fusion node: detects B from the provided video clips, maintains the R3 track store with the R13 admission state machine, runs the R14 Collision Risk Assessment abstraction with the M1 NLOS plugin, and emits R4 warnings to the IVI. Focus goal is a structured, low-latency architecture — not detection performance.

- **Requirements:** R3, R12–R15 — [m1-cooperative-awareness.md](../requirements/m1-cooperative-awareness.md) §2.
- **Node/deploy guide:** [node-ada-ecu.md](../requirements/car-sky-guide/node-ada-ecu.md) — image tag, blueprint config, env vars, pins, verification.
- **Layout & build rules:** [node-code-layout.md](../.claude/rules/node-code-layout.md) — C++17 core + Python 3.11 detector subprocess, `docker build -t ada-ecu:latest ADA_ECU/`.
- **Plan:** Phase 2 (skeleton, store, state machine) → Phases 3 ∥ 4 (detection · fusion) of [milestone1.md](../plans/milestone1.md).

Two processes, one image: the C++17 core (store, CRA, emission, logging) and the Python detector join **only** through argv + exit codes + R3 JSONL over stdout — no FFI, no RPC (report §3(d)/(g)). The R13 gate constants (`GATE_ENTER_M`, `GATE_EXIT_M`) come from env, never literals ([CLAUDE.md](../CLAUDE.md) governing principle 5). The provided video clip(s) are `COPY`d into the image at build time — no live video pin in M1.

Empty until Phase 2 — structure is [project-architecture](../.claude/agents/project-architecture.md)'s to create via its HLD.
