# Scenario_Player — bench node (R11)

Bench V2X message generator: emits R1-profile CPMs informing the V2X ECU about vehicle C across configurable scenarios and message rates. Sanctioned test equipment deployed in the same blueprint, **not a mock to eliminate** ([CLAUDE.md](../CLAUDE.md) governing principle 2).

- **Design authority:** [doc/scenario-player-hld.md](../documents/Design/SCENARIO-PLAYER/scenario-player-hld.md) — every component, path, seam, configuration key and evidence line, with its [decision record](../documents/Design/SCENARIO-PLAYER/scenario-player-design-decisions.md).
- **Requirement:** R11 — [m1-cooperative-awareness.md](../requirements/m1-cooperative-awareness.md) §2.
- **Node/deploy guide:** [node-scenario-player.md](../requirements/car-sky-guide/node-scenario-player.md) — image tag, blueprint config, env vars, pins, verification.
- **Layout & build rules:** [node-code-layout.md](../.claude/rules/node-code-layout.md) — Python, `main.py` entrypoint at image workdir `/app`, scenario configs under `scenarios/`, `docker build -t m1-scenario-player:latest Scenario_Player/`.
- **Plan:** Phase 1 of [milestone1.md](../plans/milestone1.md) (contracts R1/R6 frozen in Phase 0 first).
