#include "log/event_log.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace ada::log {

namespace {

std::int64_t realMonoMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

std::int64_t realEpochMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

}  // namespace

EventLog::EventLog(std::string filePath, std::ostream* sink, Clocks clocks)
    : sink_(sink != nullptr ? sink : &std::cout), clocks_(std::move(clocks)) {
  if (!clocks_.monoMs) {
    clocks_.monoMs = realMonoMs;
  }
  if (!clocks_.epochMs) {
    clocks_.epochMs = realEpochMs;
  }
  if (!filePath.empty()) {
    file_.open(filePath, std::ios::out | std::ios::app);
    if (!file_.is_open()) {
      throw std::runtime_error("EventLog: cannot open EVENT_LOG_PATH file: " + filePath);
    }
  }
}

void EventLog::emit(const std::string& event, nlohmann::json payload) {
  std::lock_guard<std::mutex> lock(mutex_);
  ++counters_[event];
  const nlohmann::json line = {
      {"event", event},
      {"mono_ms", clocks_.monoMs()},
      {"epoch_ms", clocks_.epochMs()},
      {"counters", counters_},
      {"payload", std::move(payload)},
  };
  // dump() escapes embedded quotes and newlines, so the record stays one
  // physical line — the JSONL invariant the offline readers depend on.
  const std::string text = "[EVT] " + line.dump();
  (*sink_) << text << '\n';
  sink_->flush();
  if (file_.is_open()) {
    file_ << text << '\n';
    file_.flush();
  }
}

std::uint64_t EventLog::count(const std::string& event) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = counters_.find(event);
  return it == counters_.end() ? 0u : it->second;
}

}  // namespace ada::log
