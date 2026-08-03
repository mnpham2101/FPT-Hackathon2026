#include "ada/risk_registry.hpp"

#include <stdexcept>

namespace ada {

std::unique_ptr<CollisionRiskAssessor> make_builtin_assessor(const std::string& name, const AdaConfig& config) {
    if (name == "nlos_obstruction") {
        return std::make_unique<NlosRiskAssessor>(config.risk_near_m, config.risk_critical_m, config.risk_dwell_ms);
    }
    throw std::runtime_error("unknown CRA plugin: " + name);
}

}  // namespace ada
