#include "config/config.hpp"

#include <cctype>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

namespace ada::config {

namespace {

// CRA_ENABLED: split on commas, trim surrounding whitespace, drop empties.
std::vector<std::string> parseList(const std::string& raw) {
  std::vector<std::string> out;
  std::string current;
  auto flush = [&out, &current]() {
    std::size_t begin = 0;
    std::size_t end = current.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(current[begin])) != 0) {
      ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(current[end - 1])) != 0) {
      --end;
    }
    if (end > begin) {
      out.push_back(current.substr(begin, end - begin));
    }
    current.clear();
  };
  for (const char c : raw) {
    if (c == ',') {
      flush();
    } else {
      current.push_back(c);
    }
  }
  flush();
  return out;
}

std::string toLower(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

}  // namespace

Config load(const EnvGetter& getEnv) {
  const EnvGetter env =
      getEnv ? getEnv : EnvGetter([](const char* key) -> const char* { return std::getenv(key); });

  auto raw = [&env](const char* key) -> std::optional<std::string> {
    const char* value = env(key);
    if (value == nullptr) {
      return std::nullopt;
    }
    return std::string(value);
  };

  auto getString = [&raw](const char* key, const char* fallback) -> std::string {
    const auto value = raw(key);
    return value ? *value : std::string(fallback);
  };

  auto getInt64 = [&raw](const char* key, std::int64_t fallback) -> std::int64_t {
    const auto value = raw(key);
    if (!value) {
      return fallback;
    }
    try {
      std::size_t pos = 0;
      const long long parsed = std::stoll(*value, &pos);
      if (pos != value->size()) {
        throw std::invalid_argument("trailing characters");
      }
      return static_cast<std::int64_t>(parsed);
    } catch (const std::exception&) {
      throw ConfigError(std::string(key) + ": not an integer: '" + *value + "'");
    }
  };

  auto getDouble = [&raw](const char* key, double fallback) -> double {
    const auto value = raw(key);
    if (!value) {
      return fallback;
    }
    try {
      std::size_t pos = 0;
      const double parsed = std::stod(*value, &pos);
      if (pos != value->size()) {
        throw std::invalid_argument("trailing characters");
      }
      return parsed;
    } catch (const std::exception&) {
      throw ConfigError(std::string(key) + ": not a number: '" + *value + "'");
    }
  };

  auto getBool = [&raw](const char* key, bool fallback) -> bool {
    const auto value = raw(key);
    if (!value) {
      return fallback;
    }
    const std::string lowered = toLower(*value);
    if (lowered == "true" || lowered == "1") {
      return true;
    }
    if (lowered == "false" || lowered == "0") {
      return false;
    }
    throw ConfigError(std::string(key) + ": not a boolean (true/false/1/0): '" + *value + "'");
  };

  auto getPort = [&getInt64](const char* key, std::int64_t fallback) -> std::uint16_t {
    const std::int64_t port = getInt64(key, fallback);
    if (port < 1 || port > 65535) {
      throw ConfigError(std::string(key) + ": port must be in 1-65535, got " +
                        std::to_string(port));
    }
    return static_cast<std::uint16_t>(port);
  };

  auto requireNonEmpty = [](const char* key, const std::string& value) {
    if (value.empty()) {
      throw ConfigError(std::string(key) + ": host must be non-empty");
    }
  };

  // Defaults exactly per the HLD §6 Env — core table. The Phase 4 risk values
  // are loaded now and unused until then — one loader, one table.
  const std::string v2xListenHost = getString("V2X_LISTEN_HOST", "0.0.0.0");
  const std::uint16_t v2xListenPort = getPort("V2X_LISTEN_PORT", 47200);
  const std::string iviEcuHost = getString("IVI_ECU_HOST", "10.99.0.13");
  const std::uint16_t iviEcuPort = getPort("IVI_ECU_PORT", 47300);
  const double gateEnterM = getDouble("GATE_ENTER_M", 30.0);
  const double gateExitM = getDouble("GATE_EXIT_M", 35.0);
  const std::int64_t confirmHits = getInt64("CONFIRM_HITS", 3);
  const std::int64_t trackTimeoutMs = getInt64("TRACK_TIMEOUT_MS", 1000);
  const std::int64_t fusionTickMs = getInt64("FUSION_TICK_MS", 100);
  const bool detectorEnabled = getBool("DETECTOR_ENABLED", true);
  const std::string detectorCmd = getString("DETECTOR_CMD", "python3 /app/detector/main.py");
  const std::int64_t detectorRestartMax = getInt64("DETECTOR_RESTART_MAX", 5);
  const std::vector<std::string> craEnabled =
      parseList(getString("CRA_ENABLED", "nlos_obstruction"));
  const double riskNearM = getDouble("RISK_NEAR_M", 60.0);
  const double riskCriticalM = getDouble("RISK_CRITICAL_M", 30.0);
  const double riskTtcWarnS = getDouble("RISK_TTC_WARN_S", 6.0);
  const double riskTtcCriticalS = getDouble("RISK_TTC_CRITICAL_S", 3.0);
  const std::int64_t riskDwellMs = getInt64("RISK_DWELL_MS", 300);
  const double stateRateHz = getDouble("STATE_RATE_HZ", 0.0);
  const std::string eventLogPath = getString("EVENT_LOG_PATH", "");
  const std::int64_t assessLogEveryMs = getInt64("ASSESS_LOG_EVERY_MS", 1000);

  // Startup validation — exactly the HLD §6 table, nothing beyond it.
  // Each rule orders values of ONE quantity, or bounds a single value.
  requireNonEmpty("V2X_LISTEN_HOST", v2xListenHost);
  requireNonEmpty("IVI_ECU_HOST", iviEcuHost);

  // The R13 Schmitt band — both thresholds measure one source's own range
  // (d_BC relayed, d_AB estimated).
  if (!(gateEnterM > 0.0 && gateEnterM < gateExitM)) {
    throw ConfigError("GATE_ENTER_M/GATE_EXIT_M: must satisfy 0 < GATE_ENTER_M < GATE_EXIT_M, got " +
                      std::to_string(gateEnterM) + " / " + std::to_string(gateExitM));
  }

  // The R14 bands — both thresholds measure the composed range d_AC.
  // No rule relates a risk threshold to a gate threshold: they measure
  // different quantities (d_AC vs d_BC/d_AB), and the R22 pair 60/30 sits
  // above GATE_ENTER_M by design (D5, D11).
  if (!(riskCriticalM > 0.0 && riskCriticalM < riskNearM)) {
    throw ConfigError(
        "RISK_CRITICAL_M/RISK_NEAR_M: must satisfy 0 < RISK_CRITICAL_M < RISK_NEAR_M, got " +
        std::to_string(riskCriticalM) + " / " + std::to_string(riskNearM));
  }

  // TTC on d_AC.
  if (!(riskTtcCriticalS > 0.0 && riskTtcCriticalS < riskTtcWarnS)) {
    throw ConfigError(
        "RISK_TTC_CRITICAL_S/RISK_TTC_WARN_S: must satisfy 0 < RISK_TTC_CRITICAL_S < "
        "RISK_TTC_WARN_S, got " +
        std::to_string(riskTtcCriticalS) + " / " + std::to_string(riskTtcWarnS));
  }

  if (confirmHits < 1) {
    throw ConfigError("CONFIRM_HITS: must be >= 1, got " + std::to_string(confirmHits));
  }
  if (trackTimeoutMs <= 0) {
    throw ConfigError("TRACK_TIMEOUT_MS: must be > 0, got " + std::to_string(trackTimeoutMs));
  }
  if (fusionTickMs <= 0) {
    throw ConfigError("FUSION_TICK_MS: must be > 0, got " + std::to_string(fusionTickMs));
  }
  if (riskDwellMs < 0) {
    throw ConfigError("RISK_DWELL_MS: must be >= 0, got " + std::to_string(riskDwellMs));
  }
  if (stateRateHz < 0.0) {
    throw ConfigError("STATE_RATE_HZ: must be >= 0, got " + std::to_string(stateRateHz));
  }
  if (detectorRestartMax < 0) {
    throw ConfigError("DETECTOR_RESTART_MAX: must be >= 0, got " +
                      std::to_string(detectorRestartMax));
  }

  // CRA_ENABLED naming registered plugins only is checked at registry wiring
  // (the HLD §6 table's remaining clause): the loader cannot know the
  // registry, so it only stores the list.

  return Config{
      v2xListenHost,
      v2xListenPort,
      iviEcuHost,
      iviEcuPort,
      gateEnterM,
      gateExitM,
      static_cast<int>(confirmHits),
      trackTimeoutMs,
      fusionTickMs,
      detectorEnabled,
      detectorCmd,
      static_cast<int>(detectorRestartMax),
      craEnabled,
      riskNearM,
      riskCriticalM,
      riskTtcWarnS,
      riskTtcCriticalS,
      riskDwellMs,
      stateRateHz,
      eventLogPath,
      assessLogEveryMs,
  };
}

}  // namespace ada::config
