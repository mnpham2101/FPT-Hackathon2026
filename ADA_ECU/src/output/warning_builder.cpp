#include "output/warning_builder.hpp"

namespace ada::output {

namespace {

// Frozen contract constants, not tunables (CLAUDE.md principle 5 exempts
// contract-fixed values): the R4 schema version this binding implements
// (contracts/samples/r4-warning.json, schemaVersion 1) and the warning
// discriminator the schema fixes as a const.
constexpr int kSchemaVersion = 1;
constexpr const char* kTypeWarning = "warning";

}  // namespace

contracts::R4WarningEvent buildWarning(const cra::RiskFinding& finding,
                                       const fusion::SceneGeometry& geometry) {
  contracts::R4WarningEvent event;
  event.schemaVersion = kSchemaVersion;
  event.type = kTypeWarning;
  event.warningType = finding.warningType;
  event.riskState = finding.riskState;
  event.object = *finding.trigger;  // present by the caller's contract (header comment)
  event.geometry.ego = geometry.ego;
  event.geometry.vehicleB = geometry.vehicleB;
  event.geometry.vehicleC = geometry.vehicleC;  // nullopt → JSON null on the wire
  return event;
}

}  // namespace ada::output
