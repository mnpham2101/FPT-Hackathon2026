#pragma once

#include <memory>
#include <string>

#include "ada/config.hpp"
#include "ada/risk_assessor.hpp"

namespace ada {

std::unique_ptr<CollisionRiskAssessor> make_builtin_assessor(const std::string& name, const AdaConfig& config);

}  // namespace ada
