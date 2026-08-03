#include "ada/config.hpp"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <unordered_map>

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

double parse_double(const std::string& value, const std::string& name) {
    std::size_t parsed = 0;
    try {
        const auto result = std::stod(value, &parsed);
        if (parsed != value.size()) {
            throw std::invalid_argument("suffix");
        }
        return result;
    } catch (const std::exception&) {
        throw std::runtime_error(name + " must be a number");
    }
}

std::int64_t parse_int64(const std::string& value, const std::string& name) {
    std::size_t parsed = 0;
    try {
        const auto result = std::stoll(value, &parsed);
        if (parsed != value.size()) {
            throw std::invalid_argument("suffix");
        }
        return result;
    } catch (const std::exception&) {
        throw std::runtime_error(name + " must be an integer");
    }
}

int parse_int(const std::string& value, const std::string& name) {
    const auto result = parse_int64(value, name);
    if (result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max()) {
        throw std::runtime_error(name + " is outside the supported integer range");
    }
    return static_cast<int>(result);
}

bool parse_bool(const std::string& value, const std::string& name) {
    if (value == "1" || value == "true" || value == "TRUE" || value == "yes") {
        return true;
    }
    if (value == "0" || value == "false" || value == "FALSE" || value == "no") {
        return false;
    }
    throw std::runtime_error(name + " must be true/false, yes/no, or 1/0");
}

template <typename Apply>
void apply_env(const char* name, Apply&& apply) {
    if (const char* value = std::getenv(name)) {
        apply(value, name);
    }
}

void apply_values(const std::unordered_map<std::string, std::string>& values, AdaConfig& config) {
    const auto apply = [&](const char* key, const auto& setter) {
        const auto found = values.find(key);
        if (found != values.end()) {
            setter(found->second, key);
        }
    };
    apply("gate_enter_m", [&](const auto& value, const auto& key) { config.gate_enter_m = parse_double(value, key); });
    apply("gate_exit_m", [&](const auto& value, const auto& key) { config.gate_exit_m = parse_double(value, key); });
    apply("miss_limit_ms", [&](const auto& value, const auto& key) { config.miss_limit_ms = parse_int64(value, key); });
    apply("track_timeout_ms", [&](const auto& value, const auto& key) { config.miss_limit_ms = parse_int64(value, key); });
    apply("tentative_hits", [&](const auto& value, const auto& key) { config.tentative_hits = parse_int(value, key); });
    apply("confirm_hits", [&](const auto& value, const auto& key) { config.tentative_hits = parse_int(value, key); });
    apply("log_path", [&](const auto& value, const auto&) { config.log_path = value; });
    apply("event_log_path", [&](const auto& value, const auto&) { config.log_path = value; });
    apply("ada_listen_host", [&](const auto& value, const auto&) { config.ada_listen_host = value; });
    apply("v2x_listen_host", [&](const auto& value, const auto&) { config.ada_listen_host = value; });
    apply("ada_listen_port", [&](const auto& value, const auto& key) { config.ada_listen_port = parse_int(value, key); });
    apply("v2x_listen_port", [&](const auto& value, const auto& key) { config.ada_listen_port = parse_int(value, key); });
    apply("r2_receive_timeout_ms", [&](const auto& value, const auto& key) { config.r2_receive_timeout_ms = parse_int64(value, key); });
    apply("ivi_ecu_host", [&](const auto& value, const auto&) { config.ivi_host = value; });
    apply("ivi_host", [&](const auto& value, const auto&) { config.ivi_host = value; });
    apply("ivi_ecu_port", [&](const auto& value, const auto& key) { config.ivi_port = parse_int(value, key); });
    apply("ivi_port", [&](const auto& value, const auto& key) { config.ivi_port = parse_int(value, key); });
    apply("detector_enabled", [&](const auto& value, const auto& key) { config.detector_enabled = parse_bool(value, key); });
    apply("detector_cmd", [&](const auto& value, const auto&) { config.detector_cmd = value; });
    apply("detector_restart_max", [&](const auto& value, const auto& key) { config.detector_restart_max = parse_int(value, key); });
    apply("cra_enabled", [&](const auto& value, const auto&) { config.cra_enabled = value; });
    apply("risk_near_m", [&](const auto& value, const auto& key) { config.risk_near_m = parse_double(value, key); });
    apply("risk_critical_m", [&](const auto& value, const auto& key) { config.risk_critical_m = parse_double(value, key); });
    apply("risk_dwell_ms", [&](const auto& value, const auto& key) { config.risk_dwell_ms = parse_int64(value, key); });
}

}  // namespace

AdaConfig load_config(const std::string& path) {
    AdaConfig config;
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open ADA config: " + path);
    }

    std::unordered_map<std::string, std::string> values;
    std::string line;
    while (std::getline(in, line)) {
        const auto comment = line.find('#');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }
        const auto sep = line.find('=');
        if (sep == std::string::npos) {
            continue;
        }

        const auto key = trim(line.substr(0, sep));
        if (!key.empty()) {
            values[key] = trim(line.substr(sep + 1));
        }
    }
    apply_values(values, config);

    apply_env("GATE_ENTER_M", [&](const auto& value, const auto& key) { config.gate_enter_m = parse_double(value, key); });
    apply_env("GATE_EXIT_M", [&](const auto& value, const auto& key) { config.gate_exit_m = parse_double(value, key); });
    apply_env("MISS_LIMIT_MS", [&](const auto& value, const auto& key) { config.miss_limit_ms = parse_int64(value, key); });
    apply_env("TRACK_TIMEOUT_MS", [&](const auto& value, const auto& key) { config.miss_limit_ms = parse_int64(value, key); });
    apply_env("TENTATIVE_HITS", [&](const auto& value, const auto& key) { config.tentative_hits = parse_int(value, key); });
    apply_env("CONFIRM_HITS", [&](const auto& value, const auto& key) { config.tentative_hits = parse_int(value, key); });
    apply_env("LOG_PATH", [&](const auto& value, const auto&) { config.log_path = value; });
    apply_env("EVENT_LOG_PATH", [&](const auto& value, const auto&) { config.log_path = value; });
    apply_env("ADA_LISTEN_HOST", [&](const auto& value, const auto&) { config.ada_listen_host = value; });
    apply_env("V2X_LISTEN_HOST", [&](const auto& value, const auto&) { config.ada_listen_host = value; });
    apply_env("ADA_LISTEN_PORT", [&](const auto& value, const auto& key) { config.ada_listen_port = parse_int(value, key); });
    apply_env("V2X_LISTEN_PORT", [&](const auto& value, const auto& key) { config.ada_listen_port = parse_int(value, key); });
    apply_env("R2_RECEIVE_TIMEOUT_MS", [&](const auto& value, const auto& key) { config.r2_receive_timeout_ms = parse_int64(value, key); });
    apply_env("IVI_ECU_HOST", [&](const auto& value, const auto&) { config.ivi_host = value; });
    apply_env("IVI_HOST", [&](const auto& value, const auto&) { config.ivi_host = value; });
    apply_env("IVI_ECU_PORT", [&](const auto& value, const auto& key) { config.ivi_port = parse_int(value, key); });
    apply_env("IVI_PORT", [&](const auto& value, const auto& key) { config.ivi_port = parse_int(value, key); });
    apply_env("DETECTOR_ENABLED", [&](const auto& value, const auto& key) { config.detector_enabled = parse_bool(value, key); });
    apply_env("DETECTOR_CMD", [&](const auto& value, const auto&) { config.detector_cmd = value; });
    apply_env("DETECTOR_RESTART_MAX", [&](const auto& value, const auto& key) { config.detector_restart_max = parse_int(value, key); });
    apply_env("CRA_ENABLED", [&](const auto& value, const auto&) { config.cra_enabled = value; });
    apply_env("RISK_NEAR_M", [&](const auto& value, const auto& key) { config.risk_near_m = parse_double(value, key); });
    apply_env("RISK_CRITICAL_M", [&](const auto& value, const auto& key) { config.risk_critical_m = parse_double(value, key); });
    apply_env("RISK_DWELL_MS", [&](const auto& value, const auto& key) { config.risk_dwell_ms = parse_int64(value, key); });

    if (!std::isfinite(config.gate_enter_m) || !std::isfinite(config.gate_exit_m) ||
        config.gate_enter_m < 0.0 || config.gate_exit_m <= config.gate_enter_m) {
        throw std::runtime_error("GATE_EXIT_M must be greater than GATE_ENTER_M >= 0");
    }
    if (!std::isfinite(config.risk_near_m) || !std::isfinite(config.risk_critical_m) ||
        config.risk_critical_m < 0.0 || config.risk_near_m <= config.risk_critical_m) {
        throw std::runtime_error("RISK_NEAR_M must be greater than RISK_CRITICAL_M >= 0");
    }
    if (config.risk_dwell_ms < 0 || config.miss_limit_ms <= 0 || config.r2_receive_timeout_ms <= 0 ||
        config.detector_restart_max < 0 || config.tentative_hits <= 0) {
        throw std::runtime_error("ADA timeout and dwell configuration is invalid");
    }
    if (config.ada_listen_port < 1 || config.ada_listen_port > 65535 || config.ivi_port < 1 ||
        config.ivi_port > 65535) {
        throw std::runtime_error("ADA UDP port must be in range 1..65535");
    }
    if (config.ada_listen_host.empty() || config.ivi_host.empty() || config.log_path.empty() ||
        config.cra_enabled.empty() || (config.detector_enabled && config.detector_cmd.empty())) {
        throw std::runtime_error("ADA host, log, CRA, and detector configuration must be non-empty");
    }

    return config;
}

}  // namespace ada
