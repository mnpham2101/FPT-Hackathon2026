#ifndef ADA_ECU_CRA_REGISTRY_HPP
#define ADA_ECU_CRA_REGISTRY_HPP

// The R14 plugin registry (HLD §4/§6; design decision D4) — explicit
// registration and lookup by warningType, the plugin's name(). Registration
// is explicit through registerBuiltinPlugins, never static-init: in a static
// library the linker discards unreferenced registrar objects (D4).
//
// Lookup semantics, fixed here:
// - get(name) is a query: it returns nullptr for an unknown name.
// - enabled(names) is startup validation: an unknown name in CRA_ENABLED
//   throws a RegistryError naming it — the check config.cpp defers to
//   registry wiring. The composition root turns it into one logged line
//   and a non-zero exit, the ConfigError model.

#include <cstddef>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "cra/i_collision_risk_assessment.hpp"

namespace ada::config {
struct Config;  // defined by src/config/config.hpp; a reference suffices here
}  // namespace ada::config

namespace ada::cra {

// Thrown on a duplicate registration, a null plugin, or an unknown name in
// enabled(); the message names the offending plugin name.
class RegistryError : public std::runtime_error {
 public:
  explicit RegistryError(const std::string& what) : std::runtime_error(what) {}
};

// Owns every registered plugin (unique_ptr, RAII); non-copyable.
class Registry {
 public:
  Registry() = default;
  Registry(const Registry&) = delete;
  Registry& operator=(const Registry&) = delete;

  // Takes ownership. Throws RegistryError on a null plugin or when a plugin
  // with the same name() is already registered.
  void add(std::unique_ptr<ICollisionRiskAssessment> plugin);

  // Returns the plugin registered under `name`, or nullptr when none is.
  // The registry keeps ownership; the pointer lives as long as the registry.
  ICollisionRiskAssessment* get(const std::string& name) const;

  // Selects the subset named by CRA_ENABLED (Config::craEnabled), in the
  // order given. Throws RegistryError naming the first unknown name — a
  // startup error, not a query miss.
  std::vector<ICollisionRiskAssessment*> enabled(
      const std::vector<std::string>& names) const;

  std::size_t size() const { return plugins_.size(); }

 private:
  std::map<std::string, std::unique_ptr<ICollisionRiskAssessment>> plugins_;
};

// The single registration point, defined in src/cra/builtin_plugins.cpp —
// the one file an added plugin edits (D4). Registers the M1 NLOS plugin,
// ChainedCollision, under the frozen key "nlos_obstruction" (14.4.1.2).
void registerBuiltinPlugins(Registry& registry, const config::Config& config);

}  // namespace ada::cra

#endif  // ADA_ECU_CRA_REGISTRY_HPP
