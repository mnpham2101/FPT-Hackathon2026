# Tasks: Cooperative Intersection Collision Warning (ICW) Handling in ADA_ECU

**Input**: Design documents from `/specs/001-icw-warning-ada/`  
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`, `contracts/r4_icw_warning_schema.json`, `quickstart.md`  

---

## Dependency Graph & Execution Order

```mermaid
flowchart TD
    P1[Phase 1: Setup] --> P2[Phase 2: Foundational Data Models]
    P2 --> US1[Phase 3: User Story 1 - Relayed ICW Detection (P1 MVP)]
    US1 --> US2[Phase 4: User Story 2 - Dynamic Risk Scaling (P2)]
    US1 --> US3[Phase 5: User Story 3 - Multi-Vehicle Hazard Selection (P3)]
    US2 --> POLISH[Phase 6: Polish & Verification]
    US3 --> POLISH
```

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization and configuration setup

- [x] T001 Create externalized ICW risk configuration file at `ADA_ECU/config/icw_risk_config.json`
- [x] T002 [P] Update `ADA_ECU/CMakeLists.txt` to register `ada_icw_evaluator_test` executable target

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core data models and contract DTO definitions required by all user stories

**⚠️ CRITICAL**: Must be completed before user story implementation begins

- [x] T003 [P] Create `RelayedTrack` and `TrackState` enum structs in `ADA_ECU/src/cra/relayed_track.hpp`
- [x] T004 [P] Create `ICWConflictPoint` and `ICWRiskLevel` structs in `ADA_ECU/src/cra/icw_conflict_point.hpp`
- [x] T005 [P] Create `ICWWarningPayload` DTO and JSON serialization methods in `ADA_ECU/src/contracts/r4_icw_payload.hpp`
- [x] T006 Add contract roundtrip unit test for `ICWWarningPayload` in `ADA_ECU/tests/contracts/test_r4_icw_payload.cpp`

**Checkpoint**: Foundation ready - user story implementation can now begin

---

## Phase 3: User Story 1 - Relayed Cross-Traffic Risk Detection & Warning (Priority: P1) 🎯 MVP

**Goal**: Process V2X relayed tracks, compute 2D vector intersection TTC, and emit ADA->IVI ICW warning payloads.

**Independent Test**: Feed simulated crossing vehicle C trajectory with TTC = 2.0s and verify `ADA_ECU` emits an ICW warning message with `warning_type: ICW` and `risk_level: HIGH`.

- [x] T007 [P] [US1] Implement 2D trajectory vector intersection & TTC computation in `ADA_ECU/src/cra/icw_evaluator.hpp`
- [x] T008 [US1] Implement ICW evaluation engine core logic in `ADA_ECU/src/cra/icw_evaluator.cpp`
- [x] T009 [US1] Implement 500 ms track coasting & stale object expiration state machine in `ADA_ECU/src/cra/icw_evaluator.cpp`
- [x] T010 [P] [US1] Add unit test suite for 2D trajectory intersection and TTC in `ADA_ECU/tests/cra/test_icw_evaluator.cpp`

**Checkpoint**: User Story 1 (MVP) fully functional and testable independently

---

## Phase 4: User Story 2 - Dynamic Risk Escalation & De-escalation (Priority: P2)

**Goal**: Scale warning levels dynamically (`INFO` -> `WARNING` -> `CRITICAL` -> `CLEAR`) based on real-time TTC and distance changes.

**Independent Test**: Stream multi-frame trajectory where vehicle C approaches, decelerates, and stops, verifying sequential risk state transitions in `ADA_ECU` output.

- [x] T011 [P] [US2] Implement dynamic risk matrix threshold evaluator in `ADA_ECU/src/cra/risk_matrix.hpp`
- [x] T012 [US2] Integrate JSON configuration loader for risk thresholds in `ADA_ECU/src/cra/risk_matrix.cpp`
- [x] T013 [US2] Implement de-escalation clearance event generation on track expiration or hazard removal in `ADA_ECU/src/cra/icw_evaluator.cpp`
- [x] T014 [P] [US2] Add unit tests for risk escalation/de-escalation state transitions in `ADA_ECU/tests/cra/test_risk_matrix.cpp`

**Checkpoint**: User Stories 1 & 2 working independently and integrated

---

## Phase 5: User Story 3 - Multi-Vehicle Intersection Priority & Hazard Selection (Priority: P3)

**Goal**: Evaluate all candidate relayed tracks and prioritize the highest-risk intersection threat for IVI alert emission.

**Independent Test**: Stream crossing threat (TTC = 1.5s) alongside parallel non-threat (TTC = 4.0s) and confirm ADA_ECU selects the crossing threat for IVI emission.

- [x] T015 [US3] Implement multi-track hazard sorting and threat prioritization in `ADA_ECU/src/cra/icw_evaluator.cpp`
- [x] T016 [P] [US3] Add unit tests for multi-vehicle threat prioritization in `ADA_ECU/tests/cra/test_icw_evaluator.cpp`

**Checkpoint**: All 3 user stories fully implemented and testable

---

## Phase 6: Polish & Verification

**Purpose**: Build validation, performance check, and documentation

- [x] T017 [P] Execute full CTest suite (`ctest --test-dir ADA_ECU/build`) and verify zero test failures
- [x] T018 Run latency benchmark verifying ICW evaluation runs within $\le 50\text{ ms}$ threshold
- [x] T019 Update `ADA_ECU/README.md` with ICW module usage and testing instructions

---

## Implementation Strategy

### MVP Scope (User Story 1 Only)
1. Complete **Phase 1: Setup**
2. Complete **Phase 2: Foundational** (Data models & contracts)
3. Complete **Phase 3: User Story 1** (Core ICW detection)
4. **VALIDATE**: Run `ada_icw_evaluator_test` for cross-traffic detection.

### Incremental Parallel Opportunities
- Foundational tasks **T003**, **T004**, **T005** can run in parallel.
- User Story 1 test task **T010** can be written in parallel with evaluator implementation **T007**.
- User Story 2 risk matrix **T011** and tests **T014** can proceed after User Story 1 foundation.
