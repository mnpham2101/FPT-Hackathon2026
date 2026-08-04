// The ada_ecu composition root (HLD §8 MVC mapping; subtask 13.2.6.4) —
// controller only. Assembles config → event log → registry → observers →
// queue → parsers → store and runs the fusion tick. No parsing, no admission,
// no risk rules here: each rule lives in its module, and this file only
// routes between them. No socket header is included — socket access stays
// inside src/net/ behind the observers (house rules).
//
// Threading (D2): the two observer threads produce onto the one bounded
// queue; this main thread is the SINGLE writer of the store and the
// assessment database. Shutdown promptness comes from the sig_atomic_t stop
// flag plus the queue's bounded pop — no wait here exceeds one fusion tick.
//
// Exit codes:
//   0  clean shutdown on SIGTERM/SIGINT — observers stopped, threads joined
//      (their destructors), log flushed.
//   1  invalid configuration — config::load() rejected an env value
//      (ConfigError), or CRA_ENABLED named a plugin unknown to the registry
//      (RegistryError). The message names the offending key or name.
//   2  startup failure after a valid config — event-log file unopenable,
//      listener bind failure, or any other startup/runtime exception.

#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "config/config.hpp"
#include "cra/assessment_db.hpp"
#include "cra/registry.hpp"
#include "log/event_log.hpp"
#include "observer/detector_reader.hpp"
#include "observer/input_queue.hpp"
#include "observer/v2x_listener.hpp"
#include "parser/r2_parser.hpp"
#include "parser/r3_parser.hpp"
#include "store/track_store.hpp"

namespace {

// Exit codes documented in the header comment.
constexpr int kExitClean = 0;
constexpr int kExitInvalidConfig = 1;
constexpr int kExitStartupFailure = 2;

// Planner-designated non-tunable structural bounds (13.2.6.4 brief) — NOT
// gate constants: neither value shapes admission, risk or cadence behaviour,
// so neither belongs in the config env set (HLD §6 states no queue capacity
// and Config carries none).
//   kInputQueueCapacity — the D2 bounded-queue depth: ample headroom over
//     both producers at demo rates (~5 Hz each); overflow drops oldest and
//     is counted by the queue, never blocks a producer.
//   kListenerPollTimeout — bounds each blocking receive in V2xListener so
//     stop() is prompt at shutdown (the listener requires it injected and
//     holds no literal of its own).
constexpr std::size_t kInputQueueCapacity = 1024;
constexpr std::chrono::milliseconds kListenerPollTimeout{100};

// SIGTERM/SIGINT → request a clean shutdown. sig_atomic_t is the only type
// a signal handler may write; the main loop polls it at least once per
// fusion tick (the pop timeout bound).
volatile std::sig_atomic_t g_stopRequested = 0;

void handleStopSignal(int /*signum*/) { g_stopRequested = 1; }

// CLOCK_MONOTONIC in milliseconds — the same steady_clock domain as the
// store's default injected mono clock (D10), so expire() compares like with
// like. Wall-clock stamps are the store's and the log's own business.
std::int64_t monoNowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// nullopt → JSON null (the R2 wire nulls stay visible in the event stream).
nlohmann::json optionalToJson(const std::optional<double>& value) {
  return value.has_value() ? nlohmann::json(*value) : nlohmann::json(nullptr);
}

// One R2 datagram body: parse → apply → r2_ingest, or parse_reject.
// Rejects are counted (by the parser and the log's own counters) and never
// fatal. Payload per the parser's result API: the received body plus the two
// out-of-band confidences that have no R3 home (r2_parser.hpp), and the
// ADA-side receive stamp that rides on InputItem for exactly this event.
void ingestR2(const ada::observer::InputItem& item, ada::parser::R2Parser& parser,
              ada::store::TrackStore& store, ada::log::EventLog& eventLog) {
  const ada::parser::R2ParseResult result = parser.parse(item.line);
  if (!result.accepted()) {
    eventLog.emit(ada::log::events::kParseReject,
                  {{"source", "v2x_r2"},
                   {"reason", ada::parser::to_string(result.reason)},
                   {"rx_epoch_ms", item.rxEpochMs}});
    return;
  }
  store.apply(*result.object);
  eventLog.emit(ada::log::events::kR2Ingest,
                {{"body", item.line},
                 {"rx_epoch_ms", item.rxEpochMs},
                 {"received_object_confidence",
                  optionalToJson(result.receivedObjectConfidence)},
                 {"position_confidence", optionalToJson(result.positionConfidence)}});
}

// One detector JSONL line: parse → apply → own_sensor_ingest, or
// parse_reject. Payload: the parsed R3 object (serialized through the frozen
// binding) plus the receive stamp.
void ingestR3(const ada::observer::InputItem& item, ada::parser::R3Parser& parser,
              ada::store::TrackStore& store, ada::log::EventLog& eventLog) {
  const ada::parser::R3ParseResult result = parser.parse(item.line);
  if (!result.accepted()) {
    eventLog.emit(ada::log::events::kParseReject,
                  {{"source", "detector_r3"},
                   {"reason", ada::parser::to_string(result.reason)},
                   {"rx_epoch_ms", item.rxEpochMs}});
    return;
  }
  store.apply(*result.object);
  eventLog.emit(ada::log::events::kOwnSensorIngest,
                {{"object", nlohmann::json(*result.object)},
                 {"rx_epoch_ms", item.rxEpochMs}});
}

// Construction order fixes destruction order: the observers are destroyed
// (threads joined) before the queue, and everything log-writing before the
// EventLog. Returns the process exit code.
int run(const ada::config::Config& config) {
  ada::log::EventLog eventLog(config.eventLogPath);

  ada::store::TrackStore store(config.gateEnterM, config.gateExitM, config.confirmHits,
                               config.trackTimeoutMs, eventLog);
  ada::cra::AssessmentDb assessmentDb(eventLog);

  ada::cra::Registry registry;
  ada::cra::registerBuiltinPlugins(registry, config);
  // CRA_ENABLED validation (the check config.cpp defers to registry wiring):
  // enabled() throws on any name the registry does not carry. The Phase 2
  // registry is empty while CRA_ENABLED defaults to "nlos_obstruction", so an
  // unconditional call could never pass and the node could never start.
  // Planner's ruling (13.2.6.4 brief): validate only once the registry is
  // non-empty — Phase 4's first builtin plugin makes this check live.
  if (registry.size() > 0) {
    (void)registry.enabled(config.craEnabled);
  }

  ada::observer::InputQueue queue(kInputQueueCapacity);
  ada::observer::V2xListener listener(config.v2xListenHost, config.v2xListenPort, queue,
                                      kListenerPollTimeout);
  ada::observer::DetectorReader detector(config.detectorEnabled, config.detectorCmd,
                                         config.detectorRestartMax, queue, eventLog);

  ada::parser::R2Parser r2Parser;
  ada::parser::R3Parser r3Parser;

  // The fusion tick (D2): expire() runs every FUSION_TICK_MS of monotonic
  // time whether or not anything arrived, so a track expires on silence
  // alone. The pop timeout is the time left until the next tick — one wait
  // serves both the tick cadence and shutdown promptness. The CRA assessment
  // call on this tick is Phase 4's (15.4.2.3) and is deliberately absent.
  std::int64_t nextTickMonoMs = monoNowMs() + config.fusionTickMs;
  while (g_stopRequested == 0) {
    const std::int64_t nowMonoMs = monoNowMs();
    if (nowMonoMs >= nextTickMonoMs) {
      store.expire(nowMonoMs);
      // Advance in whole ticks; a stall never causes a burst of catch-up ticks.
      while (nextTickMonoMs <= nowMonoMs) {
        nextTickMonoMs += config.fusionTickMs;
      }
    }

    const std::int64_t waitMs = nextTickMonoMs - monoNowMs();
    std::optional<ada::observer::InputItem> item =
        queue.pop(std::chrono::milliseconds(waitMs > 0 ? waitMs : 0));
    if (!item.has_value()) {
      continue;  // timeout — the tick check at the top of the loop runs
    }

    switch (item->source) {
      case ada::observer::Source::V2xR2:
        ingestR2(*item, r2Parser, store, eventLog);
        break;
      case ada::observer::Source::DetectorR3:
        ingestR3(*item, r3Parser, store, eventLog);
        break;
    }
  }

  // Clean shutdown: stop both observers, close the queue. The destructors
  // join the threads on scope exit; the EventLog flushes per line and its
  // file closes on destruction.
  listener.stop();
  detector.stop();
  queue.close();
  return kExitClean;
}

}  // namespace

int main() {
  std::signal(SIGINT, handleStopSignal);
  std::signal(SIGTERM, handleStopSignal);

  try {
    const ada::config::Config config = ada::config::load();
    return run(config);
  } catch (const ada::config::ConfigError& error) {
    std::cerr << "[FATAL] invalid configuration: " << error.what() << std::endl;
    return kExitInvalidConfig;
  } catch (const ada::cra::RegistryError& error) {
    std::cerr << "[FATAL] invalid CRA_ENABLED: " << error.what() << std::endl;
    return kExitInvalidConfig;
  } catch (const std::exception& error) {
    std::cerr << "[FATAL] startup failure: " << error.what() << std::endl;
    return kExitStartupFailure;
  }
}
