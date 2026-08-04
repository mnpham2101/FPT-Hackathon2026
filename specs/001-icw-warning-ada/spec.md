# Feature Specification: Cooperative Intersection Collision Warning (ICW) Handling in ADA_ECU

**Feature Branch**: `001-icw-warning-ada`  
**Created**: 2026-08-03  
**Status**: Draft  
**Input**: User description: "Implement cooperative intersection collision warning (ICW) handling in ADA_ECU"

## Clarifications

### Session 2026-08-03

- Q: What default track coasting timeout duration should ADA_ECU use before marking a relayed cross-traffic track as stale? → A: 500 ms (5 missing frames at 10 Hz) for strict dropout threshold and minimal ghosting risk.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Relayed Cross-Traffic Risk Detection & Warning (Priority: P1)

As an ego vehicle driver (Vehicle A) approaching an intersection, I want ADA_ECU to detect cross-traffic hazards relayed via V2X (Vehicle C occluded from my camera view) and issue an immediate intersection collision warning to IVI, so that I can react to non-line-of-sight cross-traffic threats.

**Why this priority**: Core safety capability of cooperative awareness. Without cross-traffic warning generation from relayed V2X data, ego cannot warn the driver about non-line-of-sight intersection hazards.

**Independent Test**: Can be tested independently by feeding a simulated V2X relayed message containing cross-traffic vehicle C approaching the intersection on a collision course, verifying that ADA_ECU generates an ICW warning message for IVI within target latency.

**Acceptance Scenarios**:

1. **Given** Ego (Vehicle A) is approaching an intersection at speed,  
   **When** V2X_ECU delivers a relayed object message for Vehicle C moving laterally across ego's path with Time-To-Collision (TTC) below threshold (e.g. < 3.0 seconds),  
   **Then** ADA_ECU MUST classify the hazard as an Intersection Collision Risk and emit an ADA->IVI warning message with `warning_type: ICW` and `risk_level: HIGH`.

2. **Given** Ego (Vehicle A) and Vehicle C are approaching an intersection,  
   **When** Vehicle C's projected trajectory and TTC indicate no collision risk (TTC > 5.0 seconds or divergent paths),  
   **Then** ADA_ECU MUST NOT trigger an active ICW warning for IVI.

---

### User Story 2 - Dynamic Risk Escalation and De-escalation (Priority: P2)

As an ego vehicle driver, I want the intersection warning level to scale dynamically (INFO -> WARNING -> CRITICAL -> CLEAR) based on real-time TTC and distance changes as vehicles traverse the intersection area.

**Why this priority**: Prevents driver alert fatigue by escalating severity only when danger increases and clearing alerts promptly when risk passes.

**Independent Test**: Can be tested independently by streaming a multi-frame trajectory where Vehicle C approaches, decelerates, and stops before entering ego's lane, observing sequential risk state transitions in ADA_ECU output.

**Acceptance Scenarios**:

1. **Given** An active ICW warning is active at `WARNING` level,  
   **When** Vehicle C continues accelerating towards the intersection collision point,  
   **Then** ADA_ECU MUST escalate the warning level to `CRITICAL`.

2. **Given** An active ICW warning is active at `CRITICAL` or `WARNING` level,  
   **When** Vehicle C stops or turns away such that the collision risk drops below warning threshold,  
   **Then** ADA_ECU MUST issue a de-escalation/clearance event to IVI within 200 ms.

---

### User Story 3 - Multi-Vehicle Intersection Priority & Hazard Selection (Priority: P3)

As an ego vehicle driver approaching a busy intersection with multiple relayed vehicles, I want ADA_ECU to evaluate all candidate hazards and prioritize the highest-risk intersection threat for immediate driver warning.

**Why this priority**: Ensures driver attention is directed to the most critical hazard when multiple vehicles are present at an intersection.

**Independent Test**: Can be tested by streaming multiple relayed vehicle tracks (e.g., parallel non-hazardous track + crossing hazardous track) simultaneously and confirming ADA_ECU selects the crossing hazard for warning output.

**Acceptance Scenarios**:

1. **Given** Multiple relayed vehicle tracks are received at an intersection,  
   **When** One track presents a 1.5s TTC crossing threat and another presents a 4.0s TTC parallel threat,  
   **Then** ADA_ECU MUST prioritize the 1.5s TTC crossing threat in the ADA->IVI alert payload.

---

### Edge Cases

- **Lost Relayed Stream**: What happens when V2X messages for Vehicle C drop out unexpectedly during an active ICW warning? ADA_ECU MUST maintain track coasting for 500 ms (5 consecutive dropped frames) before gracefully clearing the warning.
- **Sensor vs Relayed Conflict**: How does ADA_ECU handle cases where local vision perceives a clear lane but V2X relays an occluded fast-approaching vehicle? ADA_ECU MUST trust V2X relayed cross-traffic objects for non-line-of-sight intersection zones.
- **Low Speed / Stationary Ego**: How does the system handle ego stopping at a red light while cross-traffic passes? ADA_ECU MUST suppress ICW warnings when ego velocity is zero and ego is stationary behind a stop line.

---

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: ADA_ECU MUST process V2X relayed tracked objects containing cross-traffic position, heading, velocity, and distance data.
- **FR-002**: ADA_ECU MUST calculate Time-To-Collision (TTC) and intersection conflict points for all active relayed tracks relative to ego trajectory.
- **FR-003**: System MUST classify hazards into ICW risk categories (`NONE`, `INFO`, `WARNING`, `CRITICAL`) based on externalized distance and TTC thresholds.
- **FR-004**: ADA_ECU MUST format and emit structured warning messages to IVI_ECU containing warning type (`ICW`), target object ID, relative position, velocity, and calculated TTC.
- **FR-005**: ADA_ECU MUST support track coasting for a default timeout of 500 ms (5 consecutive dropped frames at 10 Hz) before marking relayed tracks stale and expiring the active ICW warning.
- **FR-006**: System MUST filter out non-threatening cross-traffic objects (e.g. vehicles moving away from intersection or on non-intersecting lanes).
- **FR-007**: All risk calculation thresholds (TTC bounds, distance gates, velocity min/max) MUST be loaded from externalized configuration files (no hardcoded literals).

### Key Entities

- **Relayed Track Object**: Represents Vehicle C's position, velocity, heading, confidence score, and timestamp received via V2X ECU.
- **Intersection Conflict Zone**: Spatial region defined by ego lane vector and cross-traffic lane vector intersection point.
- **ICW Warning Event**: Structured payload emitted to IVI containing warning ID, risk level, target vehicle ID, distance-to-conflict, and TTC.

---

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: ADA_ECU processes incoming V2X relayed objects and emits an ICW warning within 50 ms of message receipt.
- **SC-002**: System correctly identifies 100% of simulated non-line-of-sight intersection collision scenarios with TTC < 3.0 seconds.
- **SC-003**: Zero false-positive ICW warnings issued for cross-traffic vehicles moving away from or parallel to ego path.
- **SC-004**: Dynamic risk level transitions (INFO -> WARNING -> CRITICAL -> CLEAR) occur within 1 update cycle of threshold crossing.
