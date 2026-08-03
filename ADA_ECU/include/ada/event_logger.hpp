#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace ada {

class EventLogger {
public:
    explicit EventLogger(const std::string& path);

    void write(const std::string& event_type, const std::string& payload_json);

private:
    std::ofstream out_;
    std::mutex mutex_;
};

}  // namespace ada
