#pragma once

#include <cstdint>
#include <string>

namespace ada {

struct AdaConfig {
    double gate_enter_m = 30.0;
    double gate_exit_m = 35.0;
    std::int64_t miss_limit_ms = 1500;
    int tentative_hits = 2;
    std::string log_path = "ada-events.jsonl";
    std::string ada_listen_host = "0.0.0.0";
    int ada_listen_port = 46002;
    std::int64_t r2_receive_timeout_ms = 200;
    std::string ivi_host = "127.0.0.1";
    int ivi_port = 46004;
};

AdaConfig load_config(const std::string& path);

}  // namespace ada
