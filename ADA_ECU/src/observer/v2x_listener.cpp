#include "observer/v2x_listener.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <utility>

namespace ada::observer {

namespace {

// CLOCK_REALTIME at receive: std::chrono::system_clock is the realtime
// clock, which keeps POSIX time headers out of observer code.
std::int64_t nowEpochMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace

V2xListener::V2xListener(const std::string& host, std::uint16_t port, InputQueue& queue,
                         std::chrono::milliseconds pollTimeout)
    : queue_(queue), pollTimeout_(pollTimeout) {
  socket_.bind(host, port);  // setup errors throw here, before the thread exists
  thread_ = std::thread(&V2xListener::run, this);
}

V2xListener::~V2xListener() {
  stop();
  if (thread_.joinable()) {
    thread_.join();
  }
}

void V2xListener::stop() { running_.store(false, std::memory_order_relaxed); }

void V2xListener::run() {
  // The socket's transient-error counter is not synchronized, so only this
  // thread reads it; deltas are republished through the atomic accessor for
  // the caller to log (D2 — no logging in observer code).
  std::uint64_t seenErrors = socket_.transientErrorCount();
  while (running_.load(std::memory_order_relaxed)) {
    std::optional<std::string> body = socket_.recvFrom(pollTimeout_);
    const std::uint64_t nowErrors = socket_.transientErrorCount();
    if (nowErrors != seenErrors) {
      receiveErrors_.fetch_add(nowErrors - seenErrors, std::memory_order_relaxed);
      seenErrors = nowErrors;
    }
    if (!body.has_value()) {
      continue;  // poll timeout or a counted transient error — never fatal
    }
    queue_.push(InputItem{Source::V2xR2, std::move(*body), nowEpochMs()});
  }
}

}  // namespace ada::observer
