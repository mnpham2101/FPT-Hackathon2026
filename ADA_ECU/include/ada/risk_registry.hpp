#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ada/config.hpp"
#include "ada/risk_assessor.hpp"

namespace ada {

class RiskRegistry {
public:
    using Factory = std::function<std::unique_ptr<CollisionRiskAssessor>()>;

    void add(std::string name, Factory factory);
    std::unique_ptr<CollisionRiskAssessor> create(const std::string& name) const;
    std::vector<std::unique_ptr<CollisionRiskAssessor>> create_enabled(const std::string& names) const;

private:
    std::unordered_map<std::string, Factory> factories_;
};

RiskRegistry make_builtin_registry(const AdaConfig& config);
std::unique_ptr<CollisionRiskAssessor> make_builtin_assessor(const std::string& name, const AdaConfig& config);

}  // namespace ada
