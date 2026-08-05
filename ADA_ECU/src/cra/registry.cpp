#include "cra/registry.hpp"

#include <utility>

namespace ada::cra {

namespace {

// The registered names, comma-joined, so an error message shows what the
// unknown name could have been.
std::string registeredNames(
    const std::map<std::string, std::unique_ptr<ICollisionRiskAssessment>>& plugins) {
  if (plugins.empty()) {
    return "(none)";
  }
  std::string joined;
  for (const auto& entry : plugins) {
    if (!joined.empty()) {
      joined += ", ";
    }
    joined += entry.first;
  }
  return joined;
}

}  // namespace

void Registry::add(std::unique_ptr<ICollisionRiskAssessment> plugin) {
  if (!plugin) {
    throw RegistryError("Registry::add rejected a null plugin");
  }
  const std::string name = plugin->name();
  if (plugins_.count(name) != 0) {
    throw RegistryError("duplicate CRA plugin name \"" + name +
                        "\": a plugin with this name is already registered");
  }
  plugins_.emplace(name, std::move(plugin));
}

ICollisionRiskAssessment* Registry::get(const std::string& name) const {
  const auto it = plugins_.find(name);
  return it == plugins_.end() ? nullptr : it->second.get();
}

std::vector<ICollisionRiskAssessment*> Registry::enabled(
    const std::vector<std::string>& names) const {
  std::vector<ICollisionRiskAssessment*> selected;
  selected.reserve(names.size());
  for (const std::string& name : names) {
    ICollisionRiskAssessment* plugin = get(name);
    if (plugin == nullptr) {
      throw RegistryError("CRA_ENABLED names unknown plugin \"" + name +
                          "\"; registered plugins: " + registeredNames(plugins_));
    }
    selected.push_back(plugin);
  }
  return selected;
}

}  // namespace ada::cra
