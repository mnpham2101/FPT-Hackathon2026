#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "ada/config.hpp"

namespace {

template <typename Fn>
bool throws(Fn&& fn) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

class EnvironmentGuard {
public:
    explicit EnvironmentGuard(std::vector<std::string> names) : names_(std::move(names)) {
        for (const auto& name : names_) {
            if (const char* value = std::getenv(name.c_str())) {
                previous_.push_back(value);
            } else {
                previous_.push_back(std::nullopt);
            }
            unsetenv(name.c_str());
        }
    }

    ~EnvironmentGuard() {
        for (std::size_t index = 0; index < names_.size(); ++index) {
            if (previous_[index]) {
                setenv(names_[index].c_str(), previous_[index]->c_str(), 1);
            } else {
                unsetenv(names_[index].c_str());
            }
        }
    }

    void set(const char* name, const char* value) {
        setenv(name, value, 1);
    }

private:
    std::vector<std::string> names_;
    std::vector<std::optional<std::string>> previous_;
};

std::filesystem::path write_config(const std::string& name, const std::string& content) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream output(path);
    output << content;
    return path;
}

}  // namespace

int main() {
    EnvironmentGuard env({
        "GATE_ENTER_M", "GATE_EXIT_M", "MISS_LIMIT_MS", "TRACK_TIMEOUT_MS", "TENTATIVE_HITS",
        "CONFIRM_HITS", "LOG_PATH", "EVENT_LOG_PATH", "ADA_LISTEN_HOST", "V2X_LISTEN_HOST",
        "ADA_LISTEN_PORT", "V2X_LISTEN_PORT", "IVI_ECU_HOST", "IVI_HOST", "IVI_ECU_PORT", "IVI_PORT",
        "R2_RECEIVE_TIMEOUT_MS", "DETECTOR_ENABLED", "DETECTOR_CMD", "DETECTOR_RESTART_MAX", "CRA_ENABLED",
        "RISK_NEAR_M", "RISK_CRITICAL_M", "RISK_DWELL_MS",
    });
    const auto empty = write_config("ada_config_empty.conf", "# comments and blank lines are accepted\n\n");
    const auto defaults = ada::load_config(empty.string());
    assert(defaults.ada_listen_host == "0.0.0.0");
    assert(defaults.ada_listen_port == 47200);
    assert(defaults.ivi_host == "10.99.0.13");
    assert(defaults.ivi_port == 47300);
    assert(defaults.miss_limit_ms == 1000);
    assert(defaults.tentative_hits == 3);
    assert(defaults.detector_enabled);
    assert(!defaults.detector_cmd.empty());

    const auto values = write_config(
        "ada_config_values.conf",
        "# legacy values must lose to canonical values regardless of file order\n"
        "track_timeout_ms=900 # inline comment\nmiss_limit_ms=1800\n"
        "confirm_hits=4\ntentative_hits=2\n"
        "event_log_path=/tmp/canonical.jsonl\nlog_path=/tmp/legacy.jsonl\n"
        "v2x_listen_host=127.0.0.2\nada_listen_host=127.0.0.1\n"
        "v2x_listen_port=47202\nada_listen_port=47201\n"
        "ivi_host=10.99.0.20\nivi_port=47320\n"
        "detector_enabled=false\ndetector_cmd=printf disabled\ndetector_restart_max=7\n"
        "gate_enter_m=31\ngate_exit_m=36\nrisk_near_m=51\nrisk_critical_m=29\nrisk_dwell_ms=250\n");
    const auto from_file = ada::load_config(values.string());
    assert(from_file.miss_limit_ms == 900);
    assert(from_file.tentative_hits == 4);
    assert(from_file.log_path == "/tmp/canonical.jsonl");
    assert(from_file.ada_listen_host == "127.0.0.2");
    assert(from_file.ada_listen_port == 47202);
    assert(from_file.ivi_host == "10.99.0.20");
    assert(from_file.ivi_port == 47320);
    assert(!from_file.detector_enabled);
    assert(from_file.detector_restart_max == 7);
    assert(from_file.gate_enter_m == 31.0);
    assert(from_file.risk_near_m == 51.0);

    env.set("MISS_LIMIT_MS", "1900");
    env.set("TRACK_TIMEOUT_MS", "800");
    env.set("TENTATIVE_HITS", "2");
    env.set("CONFIRM_HITS", "5");
    env.set("ADA_LISTEN_HOST", "127.0.0.3");
    env.set("V2X_LISTEN_HOST", "0.0.0.0");
    env.set("ADA_LISTEN_PORT", "47203");
    env.set("V2X_LISTEN_PORT", "47204");
    env.set("IVI_ECU_HOST", "10.99.0.30");
    env.set("IVI_HOST", "10.99.0.31");
    env.set("IVI_ECU_PORT", "47330");
    env.set("IVI_PORT", "47331");
    const auto overridden = ada::load_config(values.string());
    assert(overridden.miss_limit_ms == 800);
    assert(overridden.tentative_hits == 5);
    assert(overridden.ada_listen_host == "0.0.0.0");
    assert(overridden.ada_listen_port == 47204);
    assert(overridden.ivi_host == "10.99.0.31");
    assert(overridden.ivi_port == 47331);

    env.set("DETECTOR_ENABLED", "sometimes");
    assert(throws([&] { ada::load_config(empty.string()); }));
    env.set("DETECTOR_ENABLED", "true");
    env.set("CONFIRM_HITS", "0");
    assert(throws([&] { ada::load_config(empty.string()); }));
    env.set("CONFIRM_HITS", "3");
    env.set("V2X_LISTEN_PORT", "70000");
    assert(throws([&] { ada::load_config(empty.string()); }));
    env.set("V2X_LISTEN_PORT", "47200");
    env.set("TRACK_TIMEOUT_MS", "10ms");
    assert(throws([&] { ada::load_config(empty.string()); }));
    env.set("TRACK_TIMEOUT_MS", "1000");
    env.set("GATE_ENTER_M", "nan");
    assert(throws([&] { ada::load_config(empty.string()); }));

    std::filesystem::remove(empty);
    std::filesystem::remove(values);
    return 0;
}
