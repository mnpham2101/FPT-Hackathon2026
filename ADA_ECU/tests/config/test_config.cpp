// Tests for the node's only env reader (13.2.2.1).
// Env is injected through the EnvGetter seam — process env is never mutated.

#include "config/config.hpp"

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

namespace {

using ada::config::Config;
using ada::config::ConfigError;
using ada::config::EnvGetter;

// Builds a getter over a value map; keys absent from the map read as unset.
// The map must outlive the getter (tests keep it in scope).
EnvGetter getterFor(const std::map<std::string, std::string>& env) {
  return [&env](const char* key) -> const char* {
    const auto it = env.find(key);
    return it == env.end() ? nullptr : it->second.c_str();
  };
}

// Asserts load() throws a ConfigError whose message names `key`.
void expectRejectNaming(const std::map<std::string, std::string>& env, const std::string& key) {
  try {
    (void)ada::config::load(getterFor(env));
    FAIL() << "expected ConfigError naming " << key;
  } catch (const ConfigError& e) {
    EXPECT_NE(std::string(e.what()).find(key), std::string::npos)
        << "message does not name " << key << ": " << e.what();
  }
}

TEST(Config, DefaultsWhenUnset) {
  const std::map<std::string, std::string> env;
  const Config cfg = ada::config::load(getterFor(env));

  EXPECT_EQ(cfg.v2xListenHost, "0.0.0.0");
  EXPECT_EQ(cfg.v2xListenPort, 47200);
  EXPECT_EQ(cfg.iviEcuHost, "10.99.0.13");
  EXPECT_EQ(cfg.iviEcuPort, 47300);
  EXPECT_DOUBLE_EQ(cfg.gateEnterM, 30.0);
  EXPECT_DOUBLE_EQ(cfg.gateExitM, 35.0);
  EXPECT_EQ(cfg.confirmHits, 3);
  EXPECT_EQ(cfg.trackTimeoutMs, 1000);
  EXPECT_EQ(cfg.fusionTickMs, 100);
  EXPECT_TRUE(cfg.detectorEnabled);
  EXPECT_EQ(cfg.detectorCmd, "python3 /app/detector/main.py");
  EXPECT_EQ(cfg.detectorRestartMax, 5);
  EXPECT_EQ(cfg.craEnabled, std::vector<std::string>{"nlos_obstruction"});
  EXPECT_DOUBLE_EQ(cfg.riskNearM, 60.0);
  EXPECT_DOUBLE_EQ(cfg.riskCriticalM, 30.0);
  EXPECT_DOUBLE_EQ(cfg.riskTtcWarnS, 6.0);
  EXPECT_DOUBLE_EQ(cfg.riskTtcCriticalS, 3.0);
  EXPECT_EQ(cfg.riskDwellMs, 300);
  EXPECT_DOUBLE_EQ(cfg.stateRateHz, 0.0);
  EXPECT_EQ(cfg.eventLogPath, "");
  EXPECT_EQ(cfg.assessLogEveryMs, 1000);
}

TEST(Config, EveryOverrideParsed) {
  const std::map<std::string, std::string> env{
      {"V2X_LISTEN_HOST", "127.0.0.1"},
      {"V2X_LISTEN_PORT", "45000"},
      {"IVI_ECU_HOST", "10.99.0.99"},
      {"IVI_ECU_PORT", "45001"},
      {"GATE_ENTER_M", "20.5"},
      {"GATE_EXIT_M", "25.5"},
      {"CONFIRM_HITS", "5"},
      {"TRACK_TIMEOUT_MS", "2000"},
      {"FUSION_TICK_MS", "50"},
      {"DETECTOR_ENABLED", "false"},
      {"DETECTOR_CMD", "cat /app/tests/fixtures/own_sensor_mock.jsonl"},
      {"DETECTOR_RESTART_MAX", "0"},
      {"CRA_ENABLED", "nlos_obstruction, intersection_hazard"},
      {"RISK_NEAR_M", "80"},
      {"RISK_CRITICAL_M", "40"},
      {"RISK_TTC_WARN_S", "8"},
      {"RISK_TTC_CRITICAL_S", "4"},
      {"RISK_DWELL_MS", "0"},
      {"STATE_RATE_HZ", "2.5"},
      {"EVENT_LOG_PATH", "/data/evt.jsonl"},
      {"ASSESS_LOG_EVERY_MS", "500"},
  };
  const Config cfg = ada::config::load(getterFor(env));

  EXPECT_EQ(cfg.v2xListenHost, "127.0.0.1");
  EXPECT_EQ(cfg.v2xListenPort, 45000);
  EXPECT_EQ(cfg.iviEcuHost, "10.99.0.99");
  EXPECT_EQ(cfg.iviEcuPort, 45001);
  EXPECT_DOUBLE_EQ(cfg.gateEnterM, 20.5);
  EXPECT_DOUBLE_EQ(cfg.gateExitM, 25.5);
  EXPECT_EQ(cfg.confirmHits, 5);
  EXPECT_EQ(cfg.trackTimeoutMs, 2000);
  EXPECT_EQ(cfg.fusionTickMs, 50);
  EXPECT_FALSE(cfg.detectorEnabled);
  EXPECT_EQ(cfg.detectorCmd, "cat /app/tests/fixtures/own_sensor_mock.jsonl");
  EXPECT_EQ(cfg.detectorRestartMax, 0);
  const std::vector<std::string> expectedPlugins{"nlos_obstruction", "intersection_hazard"};
  EXPECT_EQ(cfg.craEnabled, expectedPlugins);
  EXPECT_DOUBLE_EQ(cfg.riskNearM, 80.0);
  EXPECT_DOUBLE_EQ(cfg.riskCriticalM, 40.0);
  EXPECT_DOUBLE_EQ(cfg.riskTtcWarnS, 8.0);
  EXPECT_DOUBLE_EQ(cfg.riskTtcCriticalS, 4.0);
  EXPECT_EQ(cfg.riskDwellMs, 0);
  EXPECT_DOUBLE_EQ(cfg.stateRateHz, 2.5);
  EXPECT_EQ(cfg.eventLogPath, "/data/evt.jsonl");
  EXPECT_EQ(cfg.assessLogEveryMs, 500);
}

TEST(Config, DetectorEnabledAcceptsNumericAndCaseForms) {
  {
    const std::map<std::string, std::string> env{{"DETECTOR_ENABLED", "0"}};
    EXPECT_FALSE(ada::config::load(getterFor(env)).detectorEnabled);
  }
  {
    const std::map<std::string, std::string> env{{"DETECTOR_ENABLED", "TRUE"}};
    EXPECT_TRUE(ada::config::load(getterFor(env)).detectorEnabled);
  }
}

// --- Rejections: single-value domains ---------------------------------------

TEST(Config, RejectsPortOutOfRange) {
  expectRejectNaming({{"V2X_LISTEN_PORT", "0"}}, "V2X_LISTEN_PORT");
  expectRejectNaming({{"V2X_LISTEN_PORT", "65536"}}, "V2X_LISTEN_PORT");
  expectRejectNaming({{"IVI_ECU_PORT", "-1"}}, "IVI_ECU_PORT");
}

TEST(Config, RejectsEmptyHost) {
  expectRejectNaming({{"V2X_LISTEN_HOST", ""}}, "V2X_LISTEN_HOST");
  expectRejectNaming({{"IVI_ECU_HOST", ""}}, "IVI_ECU_HOST");
}

TEST(Config, RejectsNonNumericValue) {
  expectRejectNaming({{"GATE_ENTER_M", "abc"}}, "GATE_ENTER_M");
  expectRejectNaming({{"CONFIRM_HITS", "3x"}}, "CONFIRM_HITS");
  expectRejectNaming({{"DETECTOR_ENABLED", "maybe"}}, "DETECTOR_ENABLED");
}

TEST(Config, RejectsConfirmHitsBelowOne) {
  expectRejectNaming({{"CONFIRM_HITS", "0"}}, "CONFIRM_HITS");
}

TEST(Config, RejectsNonPositiveTimeouts) {
  expectRejectNaming({{"TRACK_TIMEOUT_MS", "0"}}, "TRACK_TIMEOUT_MS");
  expectRejectNaming({{"FUSION_TICK_MS", "0"}}, "FUSION_TICK_MS");
}

TEST(Config, RejectsNegativeSingleValueDomains) {
  expectRejectNaming({{"RISK_DWELL_MS", "-1"}}, "RISK_DWELL_MS");
  expectRejectNaming({{"STATE_RATE_HZ", "-0.5"}}, "STATE_RATE_HZ");
  expectRejectNaming({{"DETECTOR_RESTART_MAX", "-1"}}, "DETECTOR_RESTART_MAX");
}

// --- Rejections: the three same-quantity ordering rules ---------------------

TEST(Config, RejectsGateBandOutOfOrder) {
  // 0 < GATE_ENTER_M < GATE_EXIT_M — one quantity, d_BC / d_AB.
  expectRejectNaming({{"GATE_ENTER_M", "35"}, {"GATE_EXIT_M", "35"}}, "GATE_ENTER_M");
  expectRejectNaming({{"GATE_ENTER_M", "40"}, {"GATE_EXIT_M", "35"}}, "GATE_ENTER_M");
  expectRejectNaming({{"GATE_ENTER_M", "0"}}, "GATE_ENTER_M");
  expectRejectNaming({{"GATE_ENTER_M", "-5"}}, "GATE_ENTER_M");
}

TEST(Config, RejectsRiskRangeBandOutOfOrder) {
  // 0 < RISK_CRITICAL_M < RISK_NEAR_M — one quantity, the composed d_AC.
  expectRejectNaming({{"RISK_CRITICAL_M", "60"}, {"RISK_NEAR_M", "60"}}, "RISK_CRITICAL_M");
  expectRejectNaming({{"RISK_CRITICAL_M", "70"}, {"RISK_NEAR_M", "60"}}, "RISK_CRITICAL_M");
  expectRejectNaming({{"RISK_CRITICAL_M", "0"}}, "RISK_CRITICAL_M");
}

TEST(Config, RejectsRiskTtcBandOutOfOrder) {
  // 0 < RISK_TTC_CRITICAL_S < RISK_TTC_WARN_S — one quantity, TTC on d_AC.
  expectRejectNaming({{"RISK_TTC_CRITICAL_S", "6"}, {"RISK_TTC_WARN_S", "6"}},
                     "RISK_TTC_CRITICAL_S");
  expectRejectNaming({{"RISK_TTC_CRITICAL_S", "8"}, {"RISK_TTC_WARN_S", "6"}},
                     "RISK_TTC_CRITICAL_S");
  expectRejectNaming({{"RISK_TTC_CRITICAL_S", "0"}}, "RISK_TTC_CRITICAL_S");
}

// --- The forbidden cross-quantity assertion must NOT exist ------------------

TEST(Config, RiskNearAboveGateEnterLoadsWithoutError) {
  // RISK_NEAR_M (60, on d_AC) with GATE_ENTER_M (30, on d_BC/d_AB) is the R22
  // pair (D5, D11). A loader relating the two quantities would reject it and
  // exit the node at startup — this case proves no such assertion exists.
  const std::map<std::string, std::string> env{
      {"RISK_NEAR_M", "60"},
      {"GATE_ENTER_M", "30"},
  };
  const Config cfg = ada::config::load(getterFor(env));
  EXPECT_DOUBLE_EQ(cfg.riskNearM, 60.0);
  EXPECT_DOUBLE_EQ(cfg.gateEnterM, 30.0);
}

TEST(Config, CraEnabledListStoredNotValidatedHere) {
  // Registered-plugin membership is checked at registry wiring, not by the
  // loader — an arbitrary name loads and is stored verbatim.
  const std::map<std::string, std::string> env{{"CRA_ENABLED", "not_a_registered_plugin"}};
  const Config cfg = ada::config::load(getterFor(env));
  EXPECT_EQ(cfg.craEnabled, std::vector<std::string>{"not_a_registered_plugin"});
}

}  // namespace
