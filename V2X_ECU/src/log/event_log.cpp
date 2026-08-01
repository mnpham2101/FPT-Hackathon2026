// R18 JSONL event-log writer implementation (subtask 18.1.2.3, HLD D4).
// Field-name freeze and event vocabulary: see event_log.hpp.

#include "log/event_log.hpp"

#include <chrono>
#include <utility>

namespace v2x::log {

EventLog::EventLog(std::ostream& out, const std::string& file_path) : out_(out) {
  if (!file_path.empty()) {
    file_.open(file_path, std::ios::app);
  }
}

void EventLog::rxDatagram(std::size_t bytes) {
  nlohmann::json fields = nlohmann::json::object();
  fields["bytes"] = bytes;
  emit("rx_datagram", std::move(fields), &counters_.rx_datagram);
}

void EventLog::decodeOk(const v2x::codec::CpmContent& cpm) {
  nlohmann::json fields = nlohmann::json::object();
  fields["cpm"] = cpm;
  emit("decode_ok", std::move(fields), &counters_.decode_ok);
}

void EventLog::decodeReject(const std::string& reason) {
  nlohmann::json fields = nlohmann::json::object();
  fields["detail"] = reason;
  emit("decode_reject", std::move(fields), &counters_.decode_reject);
}

void EventLog::validateReject(const std::string& reason) {
  nlohmann::json fields = nlohmann::json::object();
  fields["detail"] = reason;
  emit("validate_reject", std::move(fields), &counters_.validate_reject);
}

void EventLog::dedupeDrop() {
  emit("dedupe_drop", nlohmann::json::object(), &counters_.dedupe_drop);
}

void EventLog::r2Forwarded(const v2x::contracts::R2Message& r2) {
  nlohmann::json fields = nlohmann::json::object();
  fields["r2"] = r2;
  emit("r2_forwarded", std::move(fields), &counters_.r2_forwarded);
}

void EventLog::stubTransition(const std::string& detail) {
  nlohmann::json fields = nlohmann::json::object();
  fields["detail"] = detail;
  emit("stub_transition", std::move(fields), nullptr);
}

void EventLog::faultInjected(const std::string& detail) {
  nlohmann::json fields = nlohmann::json::object();
  fields["detail"] = detail;
  emit("fault_injected", std::move(fields), nullptr);
}

void EventLog::recovery(const std::string& detail) {
  nlohmann::json fields = nlohmann::json::object();
  fields["detail"] = detail;
  emit("recovery", std::move(fields), nullptr);
}

void EventLog::emit(const char* event, nlohmann::json fields, std::uint64_t* counter) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (counter != nullptr) {
    ++*counter;
  }

  fields["event"] = event;
  fields["mono_ms"] = static_cast<std::int64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
  fields["epoch_ms"] = static_cast<std::int64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
  fields["counters"] = nlohmann::json{
      {"rx_datagram", counters_.rx_datagram},
      {"decode_ok", counters_.decode_ok},
      {"decode_reject", counters_.decode_reject},
      {"validate_reject", counters_.validate_reject},
      {"dedupe_drop", counters_.dedupe_drop},
      {"r2_forwarded", counters_.r2_forwarded},
  };

  const std::string line = "[EVT] " + fields.dump();
  // std::endl on purpose: per-line flush is the contract — CarSky View Log is
  // the live window, and a buffered line is invisible evidence.
  out_ << line << std::endl;
  if (file_.is_open()) {
    file_ << line << std::endl;
  }
}

}  // namespace v2x::log
