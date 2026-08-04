#ifndef ADA_ECU_OUTPUT_WARNING_BUILDER_HPP
#define ADA_ECU_OUTPUT_WARNING_BUILDER_HPP

// The node's ONLY R4 producer (HLD §6, controller table, the
// output/warning_builder row; design decision D7): maps a committed
// RiskFinding plus the composed SceneGeometry onto the frozen
// contracts::R4WarningEvent, so the wire shape cannot drift from the schema
// the Phase 0 round-trip tests cover. Serialization happens only through the
// r4_message binding — no hand-built JSON anywhere in the output stage.
//
// The caller guarantees finding.trigger is present: every committed
// transition carries C's R3 snapshot — from the store while `tracked`, from
// the assessment record's lastSnapshot after erasure (D4, D5).
// geometry.vehicleC nullopt maps to the frozen null case, legitimate both
// before C is first admitted and after its track is erased (the synced
// schema's wording governs). geometry.vehicleB is always numeric: D5's
// b_unknown rule means no committed medium/high ever exists without a known
// B, so the composer always has one to place.

#include "contracts/r4_message.hpp"
#include "cra/i_collision_risk_assessment.hpp"
#include "fusion/scene_composer.hpp"

namespace ada::output {

// One R4 warning event from one committed risk transition. Pure: a function
// of its two arguments — no clock, no socket, no env (house rules).
contracts::R4WarningEvent buildWarning(const cra::RiskFinding& finding,
                                       const fusion::SceneGeometry& geometry);

}  // namespace ada::output

#endif  // ADA_ECU_OUTPUT_WARNING_BUILDER_HPP
