#ifndef ADA_ECU_CONFIG_CONFIG_HPP
#define ADA_ECU_CONFIG_CONFIG_HPP

// The node's ONLY env reader (HLD §6, Data table, the config/config row).
// Loads and validates the core env set into an immutable Config struct.
// Detector env keys are read by detector/config.py; capture keys by
// capture.sh. Each key is read in exactly one place.

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ada::config {

// Thrown on an invalid env value; the message names the offending key.
// The caller (main) turns this into one logged line and a non-zero exit.
class ConfigError : public std::runtime_error {
 public:
  explicit ConfigError(const std::string& what) : std::runtime_error(what) {}
};

// Immutable after load(): every member is const. Pass by const reference.
// Defaults live in config.cpp and nowhere else (CLAUDE.md principle 5).
struct Config {
  const std::string v2xListenHost;   // V2X_LISTEN_HOST
  const std::uint16_t v2xListenPort; // V2X_LISTEN_PORT
  const std::string iviEcuHost;      // IVI_ECU_HOST
  const std::uint16_t iviEcuPort;    // IVI_ECU_PORT

  const double gateEnterM;           // GATE_ENTER_M — R13 admission gate, d_BC / d_AB
  const double gateExitM;            // GATE_EXIT_M — R13 drop gate, same quantity
  const int confirmHits;             // CONFIRM_HITS
  const std::int64_t trackTimeoutMs; // TRACK_TIMEOUT_MS
  const std::int64_t fusionTickMs;   // FUSION_TICK_MS

  const bool detectorEnabled;        // DETECTOR_ENABLED
  const std::string detectorCmd;     // DETECTOR_CMD
  const int detectorRestartMax;      // DETECTOR_RESTART_MAX

  const std::vector<std::string> craEnabled;  // CRA_ENABLED, comma-separated

  const double riskNearM;            // RISK_NEAR_M — R14 band on the composed range d_AC
  const double riskCriticalM;        // RISK_CRITICAL_M — same quantity
  const double riskTtcWarnS;         // RISK_TTC_WARN_S
  const double riskTtcCriticalS;     // RISK_TTC_CRITICAL_S
  const std::int64_t riskDwellMs;    // RISK_DWELL_MS

  const double stateRateHz;          // STATE_RATE_HZ
  const std::string eventLogPath;    // EVENT_LOG_PATH (empty = stdout only)
  const std::int64_t assessLogEveryMs;  // ASSESS_LOG_EVERY_MS
};

// Injectable env getter so tests never mutate process env.
// Returns nullptr when the key is unset; an empty getter falls back to
// std::getenv.
using EnvGetter = std::function<const char*(const char*)>;

// Loads the core env set, applying the HLD §6 defaults for unset keys, and
// asserts exactly the HLD §6 Startup validation rules — nothing beyond them.
// Throws ConfigError naming the offending key on any invalid value.
Config load(const EnvGetter& getEnv = EnvGetter{});

}  // namespace ada::config

#endif  // ADA_ECU_CONFIG_CONFIG_HPP
