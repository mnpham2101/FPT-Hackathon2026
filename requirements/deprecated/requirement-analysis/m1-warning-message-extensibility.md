# Requirement Analysis & Technical Solution Report — ADA→IVI Warning-Message Extensibility (R21–R24)

## 0. Triggering input

The user added an architectural intent to the ADA ECU responsibility list in [m1-cooperative-awareness.md § ECU and Scenario Player Target Implementation](m1-cooperative-awareness.md#ecu-and-scenario-player-target-implementation): ADA builds its warning message for the IVI ECU through a **Collision Risk Assessment abstraction** that analyzes/categorizes risk; M1 need not implement `severityLevel` or classify every risk type, but "the message between ADA ECU and IVI ECU should include rooms for future implementation."

That message is **R20** ([m1-cooperative-awareness.md §2](m1-cooperative-awareness.md#2-enumerated-requirements-1-20)) — frozen, project-global, numbered 1–20 permanently per [research-report-format.md](../.claude/rules/research-report-format.md). This report analyses three recorded future features against R20's frozen shape — **user-opt message reduction**, **criticality filtering**, **other hazard-warning types** ([m1-cooperative-awareness.md § Future developments](m1-cooperative-awareness.md#future-developments)) — and enumerates new requirements **R21–R24** (continuing the global numbering from R20).

**Codebase check (2026-07-10):** no source files exist anywhere in the repository (`**/*.py`, `**/*.dart`, and other common source globs all return zero matches) and no HLD/design document exists for any requirement yet, R19/R20 included. **R20 is not implemented.** The "changing a frozen contract is expensive" cost from [CLAUDE.md governing principle 1](../CLAUDE.md#governing-principles) is therefore a **specification-and-re-freeze cost only** right now, not a code-rewrite cost — this is stated explicitly per the task's own instruction to flag it as a real cost only if true. It does not by itself justify adding fields (see §3) — the pick below is driven by criterion C3 (extensibility), not by "it's free so why not."

Hard constraints (open-source, Linux) are trivially satisfied throughout this report — every candidate below is a *field/placement decision* inside code the project already owns, not a 3rd-party tool pick; stated here once per [solution-selection-criteria.md](../.claude/rules/solution-selection-criteria.md) so it isn't silently skipped.

---

## 1. R1 impact analysis (answered explicitly, not assumed)

**R1 needs zero field changes for any of the three features.** Reasoning:

- R1's normative target, ETSI TS 103 324 **CPM**, is a *perception-sharing* message: "what did I (B) perceive" — `objectId`, relative position, `classification`, `confidence`. It carries no hazard/warning/severity semantics by design, and none of the three features ask it to.
- The Collision Risk Assessment / `warningType` concept lives entirely on the **ego side, downstream of R1** — ADA combines A's own detections with the *relayed* R1 perceived-object data to synthesize a warning; that synthesis step, and its output, is R20's concern, not R1's.
- **Prior art weighed (from this session's earlier discussion):** ETSI **DENM** (EN 302 637-3) is the ITS message family actually shaped for hazard events — it carries no raw numeric severity either; receivers derive criticality from `causeCode`/`subCauseCode` (a standardized 0–99 hazard-type enum) via a locally-maintained severity-tier lookup, gated by `relevanceDistance`/`validityDuration`. That pattern is useful *design precedent for R20's `warningType` field* (see §3), not evidence that R1 needs a DENM-shaped extension.
- **Flagged, not silently assumed:** feature 3's literal list — slippery roads, falling rocks, potholes, road condition, children/police presence, speed limits, no-horn rules, traffic conditions — is, in production ITS, typically *sensed and broadcast by the observing vehicle/infrastructure as its own DENM-shaped event*, i.e. genuinely new **inbound V2X traffic**, structurally distinct from R1's CPM perceived-object schema. If a future milestone implements *that* path (ingesting real hazard broadcasts from other stations, not ADA-local categorization of the single M1 occlusion scenario), it needs its **own new requirement and its own wire schema** alongside R1 — this report does **not** create that requirement, since it is unscoped and speculative beyond M1's single-scenario, single-hazard design. It is called out here so it is not later assumed to be "already covered."

---

## 2. Enumerated requirements (21–24)

**Ordering rationale: urgency, mirroring the existing R1–R20 convention.** R21 answers the direct question asked ("does R20's shape need to change") first; R22 is its immediate corollary (where criticality is computed); R23 is the independent, lower-coupling feature; R24 is the ADA-internal design obligation the user's ECU-list edit named, placed last because it is advisory to [[project-architecture]] rather than a wire-contract change.

**R21 — R20's `warningType` field accommodates future hazard-warning diversity without any schema change.** R20's existing `warningType` field (already present in the frozen text: *"schema version + warning type + R2-derived track snapshot"*) is documented and enforced as an **open-ended, registry-backed string enum** — not a bare literal at each call site — with a versioned registry containing exactly **one** M1 entry (e.g. `occluded_vehicle_relay`). A future milestone adds hazard types (§ Future developments: "other hazard-warning types") by **appending a registry entry**, never by changing the field's name, type, or the message's shape. No `severityLevel` field, no `extensions: dict` escape hatch, and no other field is added to R20 in M1.
- **Dependency:** R20 (the field being precisely specified); no code dependency — R20 is unimplemented, so this is pure specification work.
- **Feasibility: achievable.** Zero new runtime surface beyond a registry constant/module and one validation assertion; costs nothing beyond what R20's own implementation already costs, because R20 doesn't exist yet.
- **KPI:** CI/unit check asserts every `warningType` value written by ADA is drawn from the registry module (source-grep: 0 inline `warningType` string literals outside the registry file — mirrors constitution rule 5's no-hardcoded-tunables discipline, applied to enum values instead of tunables); registry holds exactly 1 entry at M1 acceptance; a test that adds a 2nd hypothetical registry entry changes 0 lines outside the registry file and 0 lines in R19's message-parsing code (structural, additive-only proof).
- **Vague → precise:** "the message should include rooms for future implementation" (re: hazard types) → *`warningType` is a registry-backed open enum, proven additive-only by the KPI above — the "room" is a documentation/discipline requirement on the existing field, not a new field.* **(A) — assumption to confirm:** the user may have intended a literal new field regardless; see §3's candidate (a) as the explicit alternative to veto into.

**R22 — Criticality/severity is a downstream, locally-derived function of R21's `warningType` registry — not an R20 wire field, in M1.** Per-message criticality used by the future user-opt filtering feature is computed by looking up R21's `warningType` value in a **locally-maintained severity-tier table** (DENM `causeCode`→severity-tier precedent, §1), not carried as a raw `severityLevel` int on the R20 wire message. M1 implements only a **trivial 1:1 lookup** (the single M1 `warningType` maps to one fixed tier) — no multi-tier logic, no user-facing filter UI; both stay in § Future developments, not silently pulled into M1.
- **Dependency:** R21 (the registry the lookup keys on); R20 (frozen — receives **no** new field from this requirement, by design).
- **Feasibility: achievable to accommodate** (a placement decision + one trivial lookup function, zero UI/multi-tier work); the *feature itself* (user-facing opt-in filtering) stays correctly out of M1 scope.
- **KPI:** design note (this report, and R20's eventual HLD) records the severity-tier lookup as a pure function of `warningType` alone (0 other message fields required as future inputs); binary check — R20's frozen field list, as of this report, contains no `severityLevel` or equivalently-named numeric-criticality field.
- **Vague → precise:** "the user can opt to receive only warnings at or above a chosen criticality level" → *criticality is a locally-derived function of R21's `warningType`, computed off the R20 wire message, not carried on it.* **(A) — assumption to confirm:** the lookup is assumed to live on the consumer/IVI side (filtering is a display/user preference, not a sender decision) — confirm with [[project-architecture]] when R20's producer/consumer HLD is written; ADA-side placement is an equally valid alternative and does not change this requirement's KPI.

**R23 — User-opt V2X message-rate reduction needs zero R1/R20 schema changes.** The future "user can opt for fewer V2X messages" feature is a **local rate/subscription control** at the sending or receiving V2X ECU — e.g. throttling R3's already-externalized broadcast-rate constant, or filtering on receipt — not a payload-shape concern. It requires no field in R1 (inter-vehicle CPM) and no field in R20 (ADA→IVI).
- **Dependency:** R3 (the externalized broadcast-rate config this future control would throttle); no dependency on R1's or R20's field sets.
- **Feasibility: achievable to accommodate** — R3's rate constant is *already* the accommodation seam (constitution rule 5: no hardcoded tunables); no new M1 work is proposed beyond this written confirmation.
- **KPI:** binary documentation check — this report records R3's rate config as the accommodation point; 0 lines of code or schema ship under this requirement in M1.
- **Vague → precise:** "the user can opt for fewer V2X messages" → *a future rate/subscription control layered on R3's existing externalized rate config; zero R1/R20 schema impact.*

**R24 — ADA's Collision Risk Assessment logic sits behind a swap-tested internal seam (design obligation only).** ADA's construction of the R20 warning message executes behind a single code-level boundary such that a future multi-factor or multi-hazard-type risk-categorization implementation replaces only the logic behind that boundary — zero changes to R20's message shape, to R20's producer-side serialization code, or to R19/IVI's consumer code. M1 populates the boundary with exactly **one** trivial strategy: R12's existing fixed-distance-threshold gate, mapped to R21's single `warningType` value. No severity computation, no multi-hazard classification, no second strategy is implemented.
- **Dependency:** R12 (the fixed-threshold gate result the M1 strategy wraps), R20 (the message the boundary populates), R21 (the registry it writes into).
- **Feasibility: achievable.** This is a module-boundary discipline decision (near-zero extra code), directly analogous to the already-accepted **R19 view-interface seam** ("renderer swappable 2D canvas ↔ future 3D without touching the shell" — same "seam now, feature later" pattern this project already uses).
- **KPI:** swap-test — implementing a second, dummy risk-assessment strategy behind the same boundary and switching to it changes ≤ 1 file/module and 0 lines in R20's serialization code or any R19/IVI consumer code (unit test with two interchangeable strategy stubs).
- **Vague → precise:** "via a Collision Risk Assessment abstraction that analyzes and categorizes risk" → *single swap-tested boundary in ADA, KPI above; M1 behavior behind it = R12's fixed-threshold gate, nothing more.*
- **Scope flag (constitution rule 3 — flag, don't silently absorb):** the *feature* this boundary enables — "modular warning scenarios" (intersection hazards, curve blind spots plugging in as new realizations) — is listed under [§ Future developments](m1-cooperative-awareness.md#future-developments), i.e. explicitly deferred. R24 builds **only the seam**, not any additional scenario, mirroring how R19's seam was accepted without implementing 3D. Flagged for user awareness, same as SC-1 was — recommend accept, since the cost is near-zero and the pattern is already precedented in this project, but this is the user's call, not silently absorbed.

---

## 3. Feasibility study result

### Whole-input verdict

**ACHIEVABLE — to accommodate, not implement — at effectively zero M1 schedule cost.** All four new requirements are specification-precision or module-boundary-discipline items; none add a new R20 wire field, none touch R1, and R20 being unimplemented means there is no rewrite anywhere in this analysis. This does not change the whole-M1 verdict already recorded in [m1-cooperative-awareness.md §2](m1-cooperative-awareness.md#whole-input-verdict) (ACHIEVABLE within 2 months) — it consumes none of its slack.

### Per-requirement verdicts

| R# | Dependency | Feasibility | Reasoning (abridged) |
|---|---|---|---|
| 21 | R20 | **Achievable** | Registry + one CI assertion; R20 unimplemented, so zero rewrite cost. |
| 22 | R21, R20 | **Achievable to accommodate** | Placement decision + one trivial lookup function; UI/multi-tier logic correctly stays deferred. |
| 23 | R3 | **Achievable to accommodate** | R3's rate constant is already the seam; zero new work. |
| 24 | R12, R20, R21 | **Achievable** | Module-boundary discipline, directly precedented by R19's seam; one trivial strategy implemented. |

No requirement in this set is at-risk or infeasible — the set is deliberately scoped to placement/documentation decisions, not new capability.

---

## 4. Technical solution analysis

### (a) R20 field-shape decision — serves R21, R22

Four candidates were evaluated for "how does R20 carry room for future severity/hazard-type diversity." The first three are the candidates named in the task input; the fourth is this report's own proposal, required by [requirement-analysis-and-solutioning](../.claude/skills/requirement-analysis-and-solutioning/SKILL.md) step 3 ("always generate more than one real candidate").

| Candidate | Assessment |
|---|---|
| (a) Add `severityLevel: Optional[int] = None`, keep `warningType: Optional[str]`, unpopulated beyond default in M1 | **C1:** certain to work, but adds a field whose numeric semantics (range? tiers? DENM-style 0–99?) aren't yet known — likely wrong and re-done at M2 anyway. **C2:** small but nonzero cost (field + default + round-trip test + R19 must tolerate/ignore it). **C3:** the apparent win is smaller than it looks — *declaring* the field now doesn't avoid the re-freeze cost of *populating and consuming* it later (R19 must still be updated when it's actually used); pre-committing to an unknown shape risks foreclosing the real M2 design. **C4:** larger footprint than doing nothing. |
| (b) Generic open-ended `extensions: dict` escape hatch | **C1:** weakens R20's own KPI — "100% field encode→decode round-trip test in CI" is hard to make meaningful against an arbitrary dict; undermines the project's established precise-mapped-field philosophy (the same philosophy that rejected protobuf/CBOR for R1, §3(b) of the main report). **C2:** cheap to add, but the *shape* of what goes in the dict invites open-ended debate — a worse C2 outcome than it looks. **C3:** flexible in the abstract, but flexibility a frozen, tested contract doesn't want. **C4:** worst footprint — "generic" invites uncontrolled growth. Rejected. |
| (c) Do nothing in R20; treat as a pure M2 schema-version bump (R20 already carries `schemaVersion` for exactly this) | **C1:** certain — R20's `schemaVersion` field is explicitly named as the future-evolution seam already ("encoding swappable behind the adapter… a later milestone may unify on UPER"). **C2:** zero M1 work. **C3:** defers the field-shape decision to when the real feature design is known — genuinely better for extensibility than guessing now. **C4:** zero new footprint. Strong candidate, but leaves `warningType`'s own extensibility undocumented (see (d)). |
| **(d) [proposed] DENM-pattern hybrid — R20's wire shape stays exactly as frozen; `warningType` is tightened into a registry-backed open enum (R21); criticality is documented as a downstream lookup keyed by it (R22)** | **C1:** same certainty as (c), plus removes a *foreseeable* implementation risk (a` `warningType` sloppily hardcoded as a bare literal, which would make even additive enum growth messy). **C2:** near-zero — a short registry module + one CI assertion, and R20 is unimplemented so there is nothing to retrofit. **C3:** strongest of the four — feature 3 (new hazard types) becomes a pure registry-append (cheaper than even an "additive field"), and feature 2 (criticality) is routed via a **standards-validated** pattern (DENM causeCode → severity-tier) instead of a guessed numeric shape. **C4:** marginally larger than bare (c), still minimal and narrowly scoped to the one field's discipline. |

**Pick: (d).** **Drivers: C1 and C3.** C1 and C2 put (c) and (d) in a near-tie (both near-zero-cost, both certain to work); **C3 is what breaks the tie** — (d) is the only candidate that buys feature 3 low-friction extensibility today while correctly routing feature 2's placement without committing to unknown future severity semantics, and it does so via a pattern with real standards precedent (DENM) rather than an invented shape. C4 marginally favors bare (c), but C4 is the explicit tie-breaker-only criterion in [solution-selection-criteria.md](../.claude/rules/solution-selection-criteria.md) and does not outrank C3. **Net effect: R20's frozen field list is unchanged from its current text** ("schema version + warning type + R2-derived track snapshot") — nothing here re-opens the freeze; R21/R22 only add discipline around the already-frozen `warningType` field.

**Explicit veto path preserved:** if the user's intent was a literal placeholder field regardless of the C3 argument above, candidate (a) is the documented fallback — flagged in R21's vague→precise line as an (A) to confirm, not silently overridden.

### (b) Criticality-lookup placement — serves R22

Not a technical/tooling decision (no library involved) — a *placement* decision between two loci, both equally cheap:

| Candidate | Assessment |
|---|---|
| **Lookup on the IVI/consumer side** (Dart, alongside R19's rendering logic) | Criticality-based filtering is a *display/user* preference (which warnings the user sees) — naturally a consumer-side concern; keeps ADA's message-builder free of a concept (user filter preference) it has no reason to know about. |
| Lookup on the ADA/producer side | Also workable — ADA could pre-resolve a tier and (in a future milestone) add it as a field then. Viable, but couples ADA to UI-preference semantics prematurely. |

**Pick: consumer-side lookup, tentative (A) — drivers C1 + C3** (keeps ADA's producer role — perception/relay/risk-gate synthesis — decoupled from a display-layer concern; matches the MVC separation [[project-architecture]] will apply per [high-level-design-procedure](../.claude/skills/high-level-design-procedure/SKILL.md)). This is a low-stakes placement note for whichever ECU's HLD is written first — flagged as (A) in R22, not binding.

### (c) R24's internal seam — no technical solution comparison needed

R24 is a module-boundary discipline requirement, not a toolchain/library/framework pick — there is no multi-candidate technical decision to compare (the "solution" is an interface/seam shape, which is [[project-architecture]]'s design authority, not a tool choice). Recorded here only to note why §3(a)/(b)'s table format doesn't apply to it.

---

## 5. HLD invocation — deferred, not performed in this run

Per [high-level-design-procedure](../.claude/skills/high-level-design-procedure/SKILL.md), [[project-researcher]] may invoke [[project-architecture]] once a 1st-choice solution is picked. **Not invoked in this run.** Reasoning: no HLD exists yet for *any* requirement in the project — not R19, not R20, not the higher-priority frozen contracts this set depends on (R21–R24 all cite R12/R20/R3 as dependencies). Spawning an isolated HLD for R24's seam ahead of ADA's own overall module design would fragment the design record and risks being redone once R9–R14/R20's real HLD is written. **Recommendation:** fold R24's seam requirement (and R21/R22's field-registry note) into ADA's eventual HLD as a design input, rather than a standalone early HLD. This is a scheduling judgment call within the discretion the task granted ("your call") — flagged here so the user or [[project-planner]] can override it if an earlier HLD is wanted.

---

## 6. Summary of recommended minimal M1-scoped work

Everything below is documentation/registry-discipline, chosen because R20 is unimplemented and none of it touches R20's currently-frozen field list:

1. Register `warningType` as an open, registry-backed enum with 1 M1 entry (R21).
2. Document that criticality is a downstream function of `warningType`, not a wire field, with the lookup tentatively placed consumer-side (R22).
3. Record that R3's existing rate config is the accommodation point for message-rate reduction — no code change (R23).
4. Note the swap-test obligation for ADA's message-builder boundary, to be realized when ADA's HLD is written (R24).

No R1 field changes. No R20 field changes. No code ships under R21–R24 in M1 beyond the registry module and its CI assertion (R21).

---

*Suggested commit (not executed in this run — project-researcher has no shell/git tool in this environment): `docs: add requirement analysis for ADA-IVI warning-message extensibility (R21-R24)`, per [research-report-format.md](../.claude/rules/research-report-format.md#commit-format).*
