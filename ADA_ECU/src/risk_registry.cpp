#include "ada/risk_registry.hpp"

#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace ada {
namespace {

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

}  // namespace

void RiskRegistry::add(std::string name, Factory factory) {
    name = trim(name);
    if (name.empty() || !factory) {
        throw std::runtime_error("CRA plugin name and factory must be valid");
    }
    if (!factories_.emplace(std::move(name), std::move(factory)).second) {
        throw std::runtime_error("duplicate CRA plugin registration");
    }
}

std::unique_ptr<CollisionRiskAssessor> RiskRegistry::create(const std::string& name) const {
    const auto found = factories_.find(trim(name));
    if (found == factories_.end()) {
        throw std::runtime_error("unknown CRA plugin: " + name);
    }
    return found->second();
}

std::vector<std::unique_ptr<CollisionRiskAssessor>> RiskRegistry::create_enabled(const std::string& names) const {
    std::vector<std::unique_ptr<CollisionRiskAssessor>> enabled;
    std::istringstream input(names);
    std::string name;
    std::unordered_set<std::string> selected;
    while (std::getline(input, name, ',')) {
        name = trim(name);
        if (name.empty()) {
            throw std::runtime_error("CRA_ENABLED contains an empty plugin name");
        }
        if (!selected.insert(name).second) {
            throw std::runtime_error("CRA_ENABLED contains duplicate plugin: " + name);
        }
        enabled.push_back(create(name));
    }
    if (enabled.empty()) {
        throw std::runtime_error("CRA_ENABLED must select at least one plugin");
    }
    return enabled;
}

RiskRegistry make_builtin_registry(const AdaConfig& config) {
    RiskRegistry registry;
    const auto near_m = config.risk_near_m;
    const auto critical_m = config.risk_critical_m;
    const auto dwell_ms = config.risk_dwell_ms;
    registry.add("nlos_obstruction", [near_m, critical_m, dwell_ms] {
        return std::make_unique<NlosRiskAssessor>(near_m, critical_m, dwell_ms);
    });
    return registry;
}

std::unique_ptr<CollisionRiskAssessor> make_builtin_assessor(const std::string& name, const AdaConfig& config) {
    return make_builtin_registry(config).create(name);
}

}  // namespace ada
