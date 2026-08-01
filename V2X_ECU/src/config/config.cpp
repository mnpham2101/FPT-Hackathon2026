#include "config/config.hpp"

#include <charconv>
#include <limits>
#include <optional>
#include <system_error>

namespace v2x::config {

namespace {

// --- HLD §6 defaults — the single defaults table. No literal tunable lives
// anywhere else in the application (CLAUDE.md governing principle 5); the
// blueprint node config injects the live values. ---

// LISTEN_PORT — R7 stub Rx port, bench-facing (blueprint-injected on the Room).
constexpr std::uint16_t kDefaultListenPort = 47100;
// ADA_ECU_HOST — R2 forward target host (blueprint-injected on the Room).
constexpr const char* kDefaultAdaEcuHost = "10.99.0.12";
// ADA_ECU_PORT — R2 forward target port (blueprint-injected on the Room).
constexpr std::uint16_t kDefaultAdaEcuPort = 47200;
// FAULT_PLAN — R8 injection plan; no fault by default.
constexpr FaultPlan kDefaultFaultPlan = FaultPlan::None;
// INIT_RETRY_MAX — init/configure retry ceiling, HLD D2 (proposal — user ratification pending).
constexpr int kDefaultInitRetryMax = 3;
// RETRY_BACKOFF_MS — backoff base for D2 recoveries (proposal — user ratification pending).
constexpr std::chrono::milliseconds kDefaultRetryBackoff{500};
// DEDUPE_WINDOW_MS — R9 dedupe sliding window (proposal — user ratification pending).
constexpr std::chrono::milliseconds kDefaultDedupeWindow{1500};
// EVENT_LOG_PATH — R18 file sink; empty = stdout only (HLD D4).
constexpr const char* kDefaultEventLogPath = "";

// Shared expectation strings so every rejection of the same kind reads the same.
constexpr const char* kExpectPort = "an integer in [1, 65535]";
constexpr const char* kExpectNonNegativeInt = "a non-negative integer";
constexpr const char* kExpectPositiveMs = "a positive integer millisecond count";

[[noreturn]] void fail(const char* name, const std::string& value, const char* expectation) {
  throw ConfigError(std::string(name) + ": invalid value \"" + value + "\"; expected " +
                    expectation);
}

// Reads one variable through the injected getter; nullopt when unset.
std::optional<std::string> readEnv(const EnvGetter& get_env, const char* name) {
  const char* raw = get_env(name);
  if (raw == nullptr) {
    return std::nullopt;
  }
  return std::string(raw);
}

// True when the variable should fall back to its default: unset, or set but
// empty (see the defaulting rule in config.hpp).
bool isUnsetOrEmpty(const std::optional<std::string>& value) {
  return !value.has_value() || value->empty();
}

// Strict base-10 integer parse over the WHOLE string: std::from_chars accepts
// an optional leading '-' only, so leading whitespace, '+', hex and empty
// input all fail; ptr != end rejects trailing garbage ("47100x"); values
// overflowing long long surface as std::errc::result_out_of_range and fail
// the same way.
long long parseInteger(const char* name, const std::string& value, const char* expectation) {
  long long parsed = 0;
  const char* const first = value.data();
  const char* const last = value.data() + value.size();
  const auto [ptr, ec] = std::from_chars(first, last, parsed, 10);
  if (ec != std::errc() || ptr != last) {
    fail(name, value, expectation);
  }
  return parsed;
}

std::uint16_t parsePort(const EnvGetter& get_env, const char* name, std::uint16_t default_value) {
  const auto value = readEnv(get_env, name);
  if (isUnsetOrEmpty(value)) {
    return default_value;
  }
  const long long port = parseInteger(name, *value, kExpectPort);
  if (port < 1 || port > 65535) {
    fail(name, *value, kExpectPort);
  }
  return static_cast<std::uint16_t>(port);
}

FaultPlan parseFaultPlan(const char* name, const std::string& value) {
  if (value == "none") {
    return FaultPlan::None;
  }
  if (value == "init_fail") {
    return FaultPlan::InitFail;
  }
  if (value == "configure_reject") {
    return FaultPlan::ConfigureReject;
  }
  if (value == "subscription_drop") {
    return FaultPlan::SubscriptionDrop;
  }
  fail(name, value, "one of none|init_fail|configure_reject|subscription_drop");
}

int parseNonNegativeInt(const EnvGetter& get_env, const char* name, int default_value) {
  const auto value = readEnv(get_env, name);
  if (isUnsetOrEmpty(value)) {
    return default_value;
  }
  const long long parsed = parseInteger(name, *value, kExpectNonNegativeInt);
  if (parsed < 0 || parsed > std::numeric_limits<int>::max()) {
    fail(name, *value, kExpectNonNegativeInt);
  }
  return static_cast<int>(parsed);
}

std::chrono::milliseconds parsePositiveMs(const EnvGetter& get_env, const char* name,
                                          std::chrono::milliseconds default_value) {
  const auto value = readEnv(get_env, name);
  if (isUnsetOrEmpty(value)) {
    return default_value;
  }
  const long long parsed = parseInteger(name, *value, kExpectPositiveMs);
  if (parsed < 1) {
    fail(name, *value, kExpectPositiveMs);
  }
  return std::chrono::milliseconds(parsed);
}

}  // namespace

Config loadFromEnv(const EnvGetter& get_env) {
  Config config{};

  config.listen_port = parsePort(get_env, "LISTEN_PORT", kDefaultListenPort);

  const auto ada_host = readEnv(get_env, "ADA_ECU_HOST");
  if (!ada_host.has_value()) {
    config.ada_ecu_host = kDefaultAdaEcuHost;
  } else if (ada_host->empty()) {
    // The one variable where set-but-empty is an error, not a default (§6
    // validation "non-empty host"): an explicitly blanked forward target is a
    // broken blueprint wiring and must not be silently redirected.
    fail("ADA_ECU_HOST", *ada_host, "a non-empty host");
  } else {
    config.ada_ecu_host = *ada_host;
  }

  config.ada_ecu_port = parsePort(get_env, "ADA_ECU_PORT", kDefaultAdaEcuPort);

  const auto fault_plan = readEnv(get_env, "FAULT_PLAN");
  config.fault_plan = isUnsetOrEmpty(fault_plan) ? kDefaultFaultPlan
                                                 : parseFaultPlan("FAULT_PLAN", *fault_plan);

  config.init_retry_max = parseNonNegativeInt(get_env, "INIT_RETRY_MAX", kDefaultInitRetryMax);
  config.retry_backoff = parsePositiveMs(get_env, "RETRY_BACKOFF_MS", kDefaultRetryBackoff);
  config.dedupe_window = parsePositiveMs(get_env, "DEDUPE_WINDOW_MS", kDefaultDedupeWindow);

  // Empty is the valid default here — stdout-only sink (HLD D4) — so unset
  // and set-but-empty deliberately coincide.
  config.event_log_path = readEnv(get_env, "EVENT_LOG_PATH").value_or(kDefaultEventLogPath);

  return config;
}

}  // namespace v2x::config
