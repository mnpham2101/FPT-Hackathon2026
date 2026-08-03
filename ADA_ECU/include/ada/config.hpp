#pragma once

#include <cstdint>
#include <string>

namespace ada {

struct AdaConfig {
    double gate_enter_m = 30.0;
    double gate_exit_m = 35.0;
    std::int64_t miss_limit_ms = 1000;
    int tentative_hits = 3;
    std::string log_path = "ada-events.jsonl";
    std::string ada_listen_host = "0.0.0.0";
    int ada_listen_port = 47200;
    std::int64_t r2_receive_timeout_ms = 200;
    std::string ivi_host = "10.99.0.13";
    int ivi_port = 47300;
    bool detector_enabled = true;
    std::string detector_cmd =
        "python3 /app/detector/tools/video_detector.py --video /app/media/ego-b-occluding-c.mp4 "
        "--backend yolo-onnx --model /app/models/yolo11n.onnx --every-n-frames 4 "
        "--confidence 0.20 --realtime --loop";
    int detector_restart_max = 3;
    std::string cra_enabled = "nlos_obstruction";
    double risk_near_m = 50.0;
    double risk_critical_m = 30.0;
    std::int64_t risk_dwell_ms = 300;
};

AdaConfig load_config(const std::string& path);

}  // namespace ada
