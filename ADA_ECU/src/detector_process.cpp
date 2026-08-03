#include "ada/detector_process.hpp"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <sys/wait.h>

namespace ada {

DetectorProcess::DetectorProcess(std::string command) : command_(std::move(command)) {
    if (command_.empty()) {
        throw std::invalid_argument("detector command must not be empty");
    }
}

DetectorProcess::~DetectorProcess() {
    if (reader_.joinable()) {
        reader_.join();
    }
}

void DetectorProcess::start() {
    if (reader_.joinable()) {
        throw std::runtime_error("detector process already started");
    }
    reader_ = std::thread(&DetectorProcess::read_stdout, this);
}

std::optional<std::string> DetectorProcess::poll_line() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (lines_.empty()) {
        return std::nullopt;
    }
    auto line = std::move(lines_.front());
    lines_.pop_front();
    return line;
}

bool DetectorProcess::finished() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return finished_ && lines_.empty();
}

int DetectorProcess::exit_code() const {
    return exit_code_;
}

void DetectorProcess::read_stdout() {
    FILE* pipe = popen(command_.c_str(), "r");
    if (pipe == nullptr) {
        exit_code_ = 127;
        finished_ = true;
        return;
    }

    std::array<char, 65536> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        std::string line(buffer.data());
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
        }
        if (!line.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            lines_.push_back(std::move(line));
        }
    }

    const int status = pclose(pipe);
    exit_code_ = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
    finished_ = true;
}

}  // namespace ada
