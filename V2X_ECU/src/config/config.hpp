#ifndef V2X_ECU_CONFIG_CONFIG_HPP
#define V2X_ECU_CONFIG_CONFIG_HPP

// V2X_ECU/src/config/config.hpp — the app's ONLY env reader (HLD §4):
// loads + validates the §6 app-consumed env set into an immutable Config.
// CAPTURE_FILTER / PCAP_DIR / CAPTURE_ROTATE_S are consumed by capture.sh
// directly and deliberately do not appear here.
//
// This header owns the env vocabulary: FaultPlan (the R8 injection plan, HLD
// D2) is defined here and consumed by the modem stub, not the other way round.
//
// Defaulting rule: an unset or empty variable takes its §6 default, with two
// deliberate exceptions — ADA_ECU_HOST set-but-empty is rejected (§6
// validation: non-empty host; a blanked R2 forward target is a broken
// blueprint wiring, not a request for the default), and EVENT_LOG_PATH's
// empty value is itself the valid default (stdout-only sink, HLD D4). Any
// other set value is parsed strictly and validated; a bad value throws
// ConfigError and the caller exits non-zero.
//
// Transport-blind data-layer code (HLD §8): no socket includes (the
// tools/check_transport_imports.py gate applies), no codec, no derivation.

#include <chrono>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>

namespace v2x::config {

// R8 fault-injection plan (HLD D2), the FAULT_PLAN env spellings 1:1.
enum class FaultPlan {
  None,              // "none" — no injected fault
  InitFail,          // "init_fail" — init() fails; backoff retries then exit non-zero
  ConfigureReject,   // "configure_reject" — configure() rejected; same recovery as init_fail
  SubscriptionDrop,  // "subscription_drop" — Rx subscription drops; unbounded resubscribe
};

// Thrown by loadFromEnv on any invalid value; the message names the offending
// env variable and the value. Derives from std::invalid_argument so callers
// may catch either type.
class ConfigError : public std::invalid_argument {
 public:
  using std::invalid_argument::invalid_argument;
};

// The validated application configuration — immutable after construction:
// built once by loadFromEnv, never mutated afterwards (consumers hold it
// const). Plain struct on purpose; the invariants are established by the
// factory, not by accessors.
struct Config {
  std::uint16_t listen_port{};              // LISTEN_PORT — R7 stub Rx port (bench-facing)
  std::string ada_ecu_host;                 // ADA_ECU_HOST — R2 forward target host
  std::uint16_t ada_ecu_port{};             // ADA_ECU_PORT — R2 forward target port
  FaultPlan fault_plan{};                   // FAULT_PLAN — R8 injection plan
  int init_retry_max{};                     // INIT_RETRY_MAX — init/configure retry ceiling (D2)
  std::chrono::milliseconds retry_backoff{};  // RETRY_BACKOFF_MS — backoff base for D2 recoveries
  std::chrono::milliseconds dedupe_window{};  // DEDUPE_WINDOW_MS — R9 dedupe sliding window
  std::string event_log_path;               // EVENT_LOG_PATH — R18 file sink; empty = stdout only
};

// Injectable env accessor with std::getenv's contract: returns the raw value,
// or nullptr when the variable is unset. Tests supply a map-backed fake so
// they never mutate the process environment; production wires a std::getenv
// wrapper at the composition root (main.cpp, a later subtask — not here).
using EnvGetter = std::function<const char*(const char*)>;

// Factory: reads the §6 env set through get_env, applies defaults, parses
// strictly (trailing garbage such as "47100x" is rejected) and validates
// ranges — ports 1..65535, FAULT_PLAN enum spellings, INIT_RETRY_MAX >= 0,
// positive backoff/window. Throws ConfigError on the first invalid value.
Config loadFromEnv(const EnvGetter& get_env);

}  // namespace v2x::config

#endif  // V2X_ECU_CONFIG_CONFIG_HPP
