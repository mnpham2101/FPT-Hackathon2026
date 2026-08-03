#include "ada/event_logger.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace ada {

EventLogger::EventLogger(const std::string& path) : out_(path, std::ios::app) {
    if (!out_) {
        throw std::runtime_error("cannot open ADA event log: " + path);
    }
}

void EventLogger::write(const std::string& event_type, const std::string& payload_json) {
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    std::string compact_payload;
    try {
        compact_payload = nlohmann::json::parse(payload_json).dump();
    } catch (const nlohmann::json::exception&) {
        compact_payload = nlohmann::json{{"raw", payload_json}}.dump();
    }
    const auto line = "{\"ts\":" + std::to_string(now) + ",\"event\":\"" + event_type +
                      "\",\"payload\":" + compact_payload + "}";
    std::lock_guard<std::mutex> lock(mutex_);
    out_ << line << "\n";
    out_.flush();
    std::cout << "[EVT] " << line << "\n";
}

}  // namespace ada
