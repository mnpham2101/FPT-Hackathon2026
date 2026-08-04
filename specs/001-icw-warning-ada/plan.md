# Implementation Plan: Cooperative Intersection Collision Warning (ICW) Handling in ADA_ECU

**Branch**: `001-icw-warning-ada` | **Date**: 2026-08-03 | **Spec**: [`spec.md`](spec.md)  
**Input**: Feature specification from `/specs/001-icw-warning-ada/spec.md`

## Summary

Implement Cooperative Intersection Collision Warning (ICW) risk assessment and message generation in `ADA_ECU`. ADA_ECU consumes relayed V2X tracked objects (Vehicle C occluded from ego camera), computes time-to-collision (TTC) and intersection conflict points, evaluates risk state transitions against externalized JSON thresholds, and emits ADA->IVI ICW warning payloads (`warning_type: ICW`).

## Technical Context

**Language/Version**: C++17 (GCC 11+ / CMake 3.22+)  
**Primary Dependencies**: `nlohmann_json` (v3.11.3), `googletest` (v1.14.0)  
**Storage**: In-memory track spatial index + externalized JSON configuration files  
**Testing**: GoogleTest (`gtest_main` via CMake CTest)  
**Target Platform**: Linux OCI Container (CarSky cloud deployment platform)  
**Project Type**: C++ Embedded ECU Application (`ADA_ECU`)  
**Performance Goals**: Processing latency $\le 50\text{ ms}$, 100% detection rate for TTC $< 3.0\text{ s}$  
**Constraints**: Zero hardcoded tunables (externalized config), 500 ms track coasting timeout on V2X message dropouts  
**Scale/Scope**: Up to 50 concurrent relayed tracked objects per evaluation frame  

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] **Principle 1 (Contract-First)**: R4 message schema (ADA->IVI) and R2 message schema (V2X->ADA) are frozen contracts. ICW payloads extend R4 using additive fields without breaking contract compatibility.
- [x] **Principle 2 (Mock-Then-Real)**: Risk calculation is implemented behind the Collision Risk Assessment (CRA) abstraction seam (`ADA_ECU/src/cra/`).
- [x] **Principle 3 (Scope Discipline)**: Scoped strictly to ICW risk evaluation on ADA_ECU. Live camera feeds and Cortex-M data remain out-of-scope.
- [x] **Principle 4 (Atomic Work)**: Implementation maps to R13 (track fusion), R14 (risk assessment), and R15 (warning payload).
- [x] **Principle 5 (No Hardcoded Tunables)**: All TTC bounds, distance thresholds, and coasting timeouts are loaded from external JSON files.
- [x] **Principle 6 (Read-On-Demand)**: Referenced platform documents loaded only as needed.

## Project Structure

### Documentation (this feature)

```text
specs/001-icw-warning-ada/
├── plan.md              # Implementation plan (this file)
├── research.md          # Phase 0 research & technology decisions
├── data-model.md        # Phase 1 data models and state machines
├── quickstart.md        # Phase 1 test and build verification instructions
├── contracts/           # Phase 1 schema definitions
│   └── r4_icw_warning_schema.json
└── tasks.md             # Phase 2 task breakdown (generated via /speckit.tasks)
```

### Source Code (`ADA_ECU/`)

```text
ADA_ECU/
├── src/
│   ├── contracts/        # Contract DTOs & serialization (R2, R3, R4)
│   ├── cra/              # Collision Risk Assessment abstractions & ICW engine
│   │   ├── icw_evaluator.hpp
│   │   ├── icw_evaluator.cpp
│   │   ├── risk_matrix.hpp
│   │   └── risk_matrix.cpp
│   └── config/           # Externalized configuration loaders
├── tests/
│   ├── contracts/        # Roundtrip contract tests
│   └── cra/              # Unit tests for ICW risk evaluation & track coasting
└── config/
    └── icw_risk_config.json
```

**Structure Decision**: C++ single ECU project structure (`ADA_ECU/`), extending existing `src/contracts/` and `src/cra/` modules cleanly with zero breaking changes to existing R2/R4 serialization models.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| *None* | N/A | Fully compliant with project constitution |
