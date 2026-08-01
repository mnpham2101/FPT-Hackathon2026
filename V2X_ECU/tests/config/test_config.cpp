// Tests for the 8.1.2.1 env config loader (src/config/config.{hpp,cpp}):
// (a) HLD §6 defaults when nothing is set, (b) every variable individually
// overridden and parsed (all four FAULT_PLAN spellings included), (c) each
// rejection case throws ConfigError naming the offending env variable.
//
// The env getter is a map-backed fake — tests never read or mutate the
// process environment.

#include "config/config.hpp"

#include <chrono>
#include <map>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

namespace {

using v2x::config::Config;
using v2x::config::ConfigError;
using v2x::config::EnvGetter;
using v2x::config::FaultPlan;
using v2x::config::loadFromEnv;

using EnvMap = std::map<std::string, std::string>;

// Runs the loader against a fake env holding exactly `values`; every other
// variable reads as unset (nullptr), matching std::getenv's contract. The map
// outlives the loadFromEnv call, so the returned c_str() pointers stay valid.
Config load(const EnvMap& values) {
  const EnvGetter getter = [&values](const char* name) -> const char* {
    const auto it = values.find(name);
    return it == values.end() ? nullptr : it->second.c_str();
  };
  return loadFromEnv(getter);
}

// Asserts the loader rejects `values` with a ConfigError whose message names
// the offending env variable.
void expectRejects(const EnvMap& values, const std::string& env_var) {
  try {
    (void)load(values);
    FAIL() << "expected ConfigError naming " << env_var;
  } catch (const ConfigError& error) {
    EXPECT_NE(std::string(error.what()).find(env_var), std::string::npos)
        << "error message should name " << env_var << ", got: " << error.what();
  }
}

// --- (a) defaults ---

TEST(ConfigLoader, AllUnsetYieldsEveryHldSection6Default) {
  const Config config = load({});
  EXPECT_EQ(config.listen_port, 47100);
  EXPECT_EQ(config.ada_ecu_host, "10.99.0.12");
  EXPECT_EQ(config.ada_ecu_port, 47200);
  EXPECT_EQ(config.fault_plan, FaultPlan::None);
  EXPECT_EQ(config.init_retry_max, 3);
  EXPECT_EQ(config.retry_backoff, std::chrono::milliseconds(500));
  EXPECT_EQ(config.dedupe_window, std::chrono::milliseconds(1500));
  EXPECT_TRUE(config.event_log_path.empty());  // empty = stdout only
}

TEST(ConfigLoader, EmptyValueFallsBackToDefaultExceptHost) {
  // Set-but-empty coincides with unset (config.hpp defaulting rule) — the
  // ADA_ECU_HOST exception is covered in the rejection cases below.
  EXPECT_EQ(load({{"LISTEN_PORT", ""}}).listen_port, 47100);
  EXPECT_EQ(load({{"ADA_ECU_PORT", ""}}).ada_ecu_port, 47200);
  EXPECT_EQ(load({{"FAULT_PLAN", ""}}).fault_plan, FaultPlan::None);
  EXPECT_EQ(load({{"INIT_RETRY_MAX", ""}}).init_retry_max, 3);
  EXPECT_EQ(load({{"RETRY_BACKOFF_MS", ""}}).retry_backoff, std::chrono::milliseconds(500));
  EXPECT_EQ(load({{"DEDUPE_WINDOW_MS", ""}}).dedupe_window, std::chrono::milliseconds(1500));
  EXPECT_TRUE(load({{"EVENT_LOG_PATH", ""}}).event_log_path.empty());
}

// --- (b) each override parsed ---

TEST(ConfigLoader, ListenPortOverrideParsed) {
  EXPECT_EQ(load({{"LISTEN_PORT", "50000"}}).listen_port, 50000);
  // Range boundaries are valid ports.
  EXPECT_EQ(load({{"LISTEN_PORT", "1"}}).listen_port, 1);
  EXPECT_EQ(load({{"LISTEN_PORT", "65535"}}).listen_port, 65535);
}

TEST(ConfigLoader, AdaEcuHostOverrideParsed) {
  EXPECT_EQ(load({{"ADA_ECU_HOST", "ada.room.local"}}).ada_ecu_host, "ada.room.local");
}

TEST(ConfigLoader, AdaEcuPortOverrideParsed) {
  EXPECT_EQ(load({{"ADA_ECU_PORT", "48200"}}).ada_ecu_port, 48200);
}

TEST(ConfigLoader, FaultPlanParsesEverySpelling) {
  EXPECT_EQ(load({{"FAULT_PLAN", "none"}}).fault_plan, FaultPlan::None);
  EXPECT_EQ(load({{"FAULT_PLAN", "init_fail"}}).fault_plan, FaultPlan::InitFail);
  EXPECT_EQ(load({{"FAULT_PLAN", "configure_reject"}}).fault_plan, FaultPlan::ConfigureReject);
  EXPECT_EQ(load({{"FAULT_PLAN", "subscription_drop"}}).fault_plan, FaultPlan::SubscriptionDrop);
}

TEST(ConfigLoader, InitRetryMaxOverrideParsed) {
  EXPECT_EQ(load({{"INIT_RETRY_MAX", "7"}}).init_retry_max, 7);
  // Zero is a valid ceiling (non-negative): no retries, fail straight away.
  EXPECT_EQ(load({{"INIT_RETRY_MAX", "0"}}).init_retry_max, 0);
}

TEST(ConfigLoader, RetryBackoffOverrideParsed) {
  EXPECT_EQ(load({{"RETRY_BACKOFF_MS", "250"}}).retry_backoff, std::chrono::milliseconds(250));
}

TEST(ConfigLoader, DedupeWindowOverrideParsed) {
  EXPECT_EQ(load({{"DEDUPE_WINDOW_MS", "3000"}}).dedupe_window, std::chrono::milliseconds(3000));
}

TEST(ConfigLoader, EventLogPathOverrideParsed) {
  EXPECT_EQ(load({{"EVENT_LOG_PATH", "/data/events.jsonl"}}).event_log_path,
            "/data/events.jsonl");
}

TEST(ConfigLoader, AllOverridesTogetherParsed) {
  const Config config = load({{"LISTEN_PORT", "48100"},
                              {"ADA_ECU_HOST", "10.99.0.99"},
                              {"ADA_ECU_PORT", "48200"},
                              {"FAULT_PLAN", "subscription_drop"},
                              {"INIT_RETRY_MAX", "5"},
                              {"RETRY_BACKOFF_MS", "100"},
                              {"DEDUPE_WINDOW_MS", "2500"},
                              {"EVENT_LOG_PATH", "/tmp/evt.jsonl"}});
  EXPECT_EQ(config.listen_port, 48100);
  EXPECT_EQ(config.ada_ecu_host, "10.99.0.99");
  EXPECT_EQ(config.ada_ecu_port, 48200);
  EXPECT_EQ(config.fault_plan, FaultPlan::SubscriptionDrop);
  EXPECT_EQ(config.init_retry_max, 5);
  EXPECT_EQ(config.retry_backoff, std::chrono::milliseconds(100));
  EXPECT_EQ(config.dedupe_window, std::chrono::milliseconds(2500));
  EXPECT_EQ(config.event_log_path, "/tmp/evt.jsonl");
}

// --- (c) rejections — each throws ConfigError naming the env var ---

TEST(ConfigLoader, RejectsPortZero) {
  expectRejects({{"LISTEN_PORT", "0"}}, "LISTEN_PORT");
  expectRejects({{"ADA_ECU_PORT", "0"}}, "ADA_ECU_PORT");
}

TEST(ConfigLoader, RejectsPortAboveRange) {
  expectRejects({{"LISTEN_PORT", "70000"}}, "LISTEN_PORT");
  expectRejects({{"ADA_ECU_PORT", "70000"}}, "ADA_ECU_PORT");
}

TEST(ConfigLoader, RejectsNonNumericPort) {
  expectRejects({{"LISTEN_PORT", "not-a-port"}}, "LISTEN_PORT");
}

TEST(ConfigLoader, RejectsTrailingGarbageNumber) {
  expectRejects({{"LISTEN_PORT", "47100x"}}, "LISTEN_PORT");
  expectRejects({{"ADA_ECU_PORT", "47200x"}}, "ADA_ECU_PORT");
  expectRejects({{"INIT_RETRY_MAX", "3x"}}, "INIT_RETRY_MAX");
  expectRejects({{"RETRY_BACKOFF_MS", "500ms"}}, "RETRY_BACKOFF_MS");
}

TEST(ConfigLoader, RejectsNegativePort) {
  expectRejects({{"LISTEN_PORT", "-1"}}, "LISTEN_PORT");
}

TEST(ConfigLoader, RejectsEmptyAdaEcuHost) {
  expectRejects({{"ADA_ECU_HOST", ""}}, "ADA_ECU_HOST");
}

TEST(ConfigLoader, RejectsUnknownFaultPlan) {
  expectRejects({{"FAULT_PLAN", "explode"}}, "FAULT_PLAN");
  // Spellings are exact (HLD §6): case variants are unknown plans.
  expectRejects({{"FAULT_PLAN", "NONE"}}, "FAULT_PLAN");
}

TEST(ConfigLoader, RejectsNegativeInitRetryMax) {
  expectRejects({{"INIT_RETRY_MAX", "-1"}}, "INIT_RETRY_MAX");
}

TEST(ConfigLoader, RejectsZeroOrNegativeRetryBackoff) {
  expectRejects({{"RETRY_BACKOFF_MS", "0"}}, "RETRY_BACKOFF_MS");
  expectRejects({{"RETRY_BACKOFF_MS", "-500"}}, "RETRY_BACKOFF_MS");
}

TEST(ConfigLoader, RejectsZeroOrNegativeDedupeWindow) {
  expectRejects({{"DEDUPE_WINDOW_MS", "0"}}, "DEDUPE_WINDOW_MS");
  expectRejects({{"DEDUPE_WINDOW_MS", "-1"}}, "DEDUPE_WINDOW_MS");
}

TEST(ConfigLoader, ErrorMessageCarriesOffendingValue) {
  try {
    (void)load({{"LISTEN_PORT", "70000"}});
    FAIL() << "expected ConfigError";
  } catch (const ConfigError& error) {
    const std::string message = error.what();
    EXPECT_NE(message.find("LISTEN_PORT"), std::string::npos) << message;
    EXPECT_NE(message.find("70000"), std::string::npos) << message;
  }
}

TEST(ConfigLoader, ConfigErrorIsAnInvalidArgument) {
  // The subtask contract lets callers catch std::invalid_argument.
  EXPECT_THROW((void)load({{"LISTEN_PORT", "0"}}), std::invalid_argument);
}

}  // namespace
