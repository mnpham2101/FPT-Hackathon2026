#include "ada/event_logger.hpp"

#include <chrono>
#include <stdexcept>

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
    out_ << "{\"ts\":" << now << ",\"event\":\"" << event_type << "\",\"payload\":" << payload_json << "}\n";
}

}  // namespace ada

