#pragma once

#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace ada {

class DetectorProcess {
public:
    explicit DetectorProcess(std::string command);
    ~DetectorProcess();

    DetectorProcess(const DetectorProcess&) = delete;
    DetectorProcess& operator=(const DetectorProcess&) = delete;

    void start();
    std::optional<std::string> poll_line();
    bool finished() const;
    int exit_code() const;

private:
    void read_stdout();

    std::string command_;
    std::thread reader_;
    mutable std::mutex mutex_;
    std::deque<std::string> lines_;
    std::atomic<bool> finished_{false};
    std::atomic<int> exit_code_{-1};
};

}  // namespace ada
