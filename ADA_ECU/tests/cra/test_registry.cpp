// CRA plugin registry tests (subtask 14.2.5.4) — explicit registration and
// lookup by name (D4): two fakes register and are retrievable, a duplicate
// name is rejected, enabled() selects the CRA_ENABLED subset in order, an
// unknown enabled name throws a RegistryError naming it, and
// registerBuiltinPlugins leaves a Phase 2 registry empty.
//
// No RiskContext is ever constructed, so the AssessmentDb forward declaration
// stays incomplete here — the fakes' assess() never reads its context.

#include "cra/registry.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "config/config.hpp"

namespace {

using ada::cra::ICollisionRiskAssessment;
using ada::cra::Registry;
using ada::cra::RegistryError;
using ada::cra::RiskContext;
using ada::cra::RiskFinding;

// A named fake: the registry only ever calls name(); assess() is a stub that
// touches nothing, so no store, database or context is needed.
class NamedFake final : public ICollisionRiskAssessment {
 public:
  explicit NamedFake(std::string name) : name_(std::move(name)) {}

  std::string name() const override { return name_; }

  RiskFinding assess(RiskContext&) override {
    RiskFinding finding;
    finding.warningType = name_;
    finding.riskState = "low";
    finding.rationale = "stub";
    return finding;
  }

 private:
  std::string name_;
};

// Asserts the callable throws a RegistryError whose message contains `text`.
template <typename Fn>
void expectRegistryErrorNaming(Fn&& fn, const std::string& text) {
  try {
    fn();
    FAIL() << "expected RegistryError naming " << text;
  } catch (const RegistryError& e) {
    EXPECT_NE(std::string(e.what()).find(text), std::string::npos)
        << "message does not name " << text << ": " << e.what();
  }
}

TEST(CraRegistry, TwoPluginsRegisterAndAreRetrievableByName) {
  Registry registry;
  registry.add(std::make_unique<NamedFake>("nlos_obstruction"));
  registry.add(std::make_unique<NamedFake>("intersection_hazard"));

  ASSERT_EQ(registry.size(), 2u);
  ICollisionRiskAssessment* first = registry.get("nlos_obstruction");
  ICollisionRiskAssessment* second = registry.get("intersection_hazard");
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(first->name(), "nlos_obstruction");
  EXPECT_EQ(second->name(), "intersection_hazard");
}

TEST(CraRegistry, GetReturnsNullptrForAnUnknownName) {
  Registry registry;
  registry.add(std::make_unique<NamedFake>("nlos_obstruction"));
  EXPECT_EQ(registry.get("no_such_plugin"), nullptr);
}

TEST(CraRegistry, DuplicateNameIsRejectedWithTheNameInTheMessage) {
  Registry registry;
  registry.add(std::make_unique<NamedFake>("nlos_obstruction"));

  expectRegistryErrorNaming(
      [&registry] { registry.add(std::make_unique<NamedFake>("nlos_obstruction")); },
      "nlos_obstruction");
  EXPECT_EQ(registry.size(), 1u);  // the rejected duplicate did not land
}

TEST(CraRegistry, NullPluginIsRejected) {
  Registry registry;
  EXPECT_THROW(registry.add(nullptr), RegistryError);
}

TEST(CraRegistry, EnabledSelectsTheNamedSubsetInOrder) {
  Registry registry;
  registry.add(std::make_unique<NamedFake>("nlos_obstruction"));
  registry.add(std::make_unique<NamedFake>("intersection_hazard"));
  registry.add(std::make_unique<NamedFake>("curve_blind_spot"));

  const std::vector<std::string> names{"curve_blind_spot", "nlos_obstruction"};
  const std::vector<ICollisionRiskAssessment*> selected = registry.enabled(names);

  ASSERT_EQ(selected.size(), 2u);
  EXPECT_EQ(selected[0]->name(), "curve_blind_spot");
  EXPECT_EQ(selected[1]->name(), "nlos_obstruction");
  EXPECT_EQ(selected[0], registry.get("curve_blind_spot"));  // same owned instance
}

TEST(CraRegistry, EnabledAcceptsTheDefaultCraEnabledList) {
  // The loader's default CRA_ENABLED (all keys unset) must select cleanly
  // once the plugin it names is registered — the wiring main.cpp performs.
  const ada::config::Config cfg =
      ada::config::load([](const char*) -> const char* { return nullptr; });

  Registry registry;
  registry.add(std::make_unique<NamedFake>("nlos_obstruction"));

  const auto selected = registry.enabled(cfg.craEnabled);
  ASSERT_EQ(selected.size(), 1u);
  EXPECT_EQ(selected[0]->name(), "nlos_obstruction");
}

TEST(CraRegistry, UnknownEnabledNameThrowsNamingIt) {
  Registry registry;
  registry.add(std::make_unique<NamedFake>("nlos_obstruction"));

  const std::vector<std::string> names{"nlos_obstruction", "no_such_plugin"};
  expectRegistryErrorNaming([&registry, &names] { (void)registry.enabled(names); },
                            "no_such_plugin");
}

TEST(CraRegistry, RegisterBuiltinPluginsIsEmptyInPhase2) {
  const ada::config::Config cfg =
      ada::config::load([](const char*) -> const char* { return nullptr; });

  Registry registry;
  ada::cra::registerBuiltinPlugins(registry, cfg);
  EXPECT_EQ(registry.size(), 0u);
}

}  // namespace
