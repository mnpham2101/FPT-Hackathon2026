#include "ada/config.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

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

void apply_env_string(const char* name, std::string& target) {
    if (const char* value = std::getenv(name)) {
        target = value;
    }
}

void apply_env_int(const char* name, int& target) {
    if (const char* value = std::getenv(name)) {
        target = std::stoi(value);
    }
}

void apply_env_int64(const char* name, std::int64_t& target) {
    if (const char* value = std::getenv(name)) {
        target = std::stoll(value);
    }
}

void apply_env_double(const char* name, double& target) {
    if (const char* value = std::getenv(name)) {
        target = std::stod(value);
    }
}

}  // namespace

AdaConfig load_config(const std::string& path) {
    AdaConfig config;
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("cannot open ADA config: " + path);
    }

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
        const auto value = trim(line.substr(sep + 1));
        if (key == "gate_enter_m") {
            config.gate_enter_m = std::stod(value);
        } else if (key == "gate_exit_m") {
            config.gate_exit_m = std::stod(value);
        } else if (key == "miss_limit_ms") {
            config.miss_limit_ms = std::stoll(value);
        } else if (key == "tentative_hits") {
            config.tentative_hits = std::stoi(value);
        } else if (key == "log_path") {
            config.log_path = value;
        } else if (key == "ada_listen_host") {
            config.ada_listen_host = value;
        } else if (key == "ada_listen_port") {
            config.ada_listen_port = std::stoi(value);
        } else if (key == "r2_receive_timeout_ms") {
            config.r2_receive_timeout_ms = std::stoll(value);
        } else if (key == "ivi_host") {
            config.ivi_host = value;
        } else if (key == "ivi_port") {
            config.ivi_port = std::stoi(value);
        }
    }

    apply_env_double("GATE_ENTER_M", config.gate_enter_m);
    apply_env_double("GATE_EXIT_M", config.gate_exit_m);
    apply_env_int64("MISS_LIMIT_MS", config.miss_limit_ms);
    apply_env_int("TENTATIVE_HITS", config.tentative_hits);
    apply_env_string("LOG_PATH", config.log_path);
    apply_env_string("ADA_LISTEN_HOST", config.ada_listen_host);
    apply_env_int("ADA_LISTEN_PORT", config.ada_listen_port);
    apply_env_int("V2X_LISTEN_PORT", config.ada_listen_port);
    apply_env_int64("R2_RECEIVE_TIMEOUT_MS", config.r2_receive_timeout_ms);
    apply_env_string("IVI_HOST", config.ivi_host);
    apply_env_int("IVI_PORT", config.ivi_port);

    return config;
}

}  // namespace ada
