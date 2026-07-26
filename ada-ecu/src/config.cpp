#include "ada/config.hpp"

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
        } else if (key == "ivi_host") {
            config.ivi_host = value;
        } else if (key == "ivi_port") {
            config.ivi_port = std::stoi(value);
        }
    }

    return config;
}

}  // namespace ada

