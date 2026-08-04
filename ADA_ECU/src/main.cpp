// The ada_ecu composition root (HLD §8 MVC mapping; subtask 13.2.6.4) —
// controller only. Assembles config → event log → registry → observers →
// queue → parsers → store → output and runs the fusion tick. No parsing, no
// admission, no risk rules here: each rule lives in its module, and this file
// only routes between them. No socket header is included — socket access
// stays inside src/net/ behind the observers and the IVI sender (house
// rules).
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
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "config/config.hpp"
#include "contracts/r4_message.hpp"
#include "contracts/tracked_object.hpp"
#include "cra/assessment_db.hpp"
#include "cra/i_collision_risk_assessment.hpp"
#include "cra/registry.hpp"
#include "fusion/scene_composer.hpp"
#include "log/event_log.hpp"
#include "net/udp_socket.hpp"
#include "observer/detector_reader.hpp"
#include "observer/input_queue.hpp"
#include "observer/v2x_listener.hpp"
#include "output/ivi_sender.hpp"
#include "output/warning_builder.hpp"
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

// The assess/emit stage of the fusion tick (subtask 15.4.2.3; D2's loop:
// expire → assess → compose → build → send). The plugin returns only
// COMMITTED levels — D5's dwell debounces inside assess() — so an inequality
// against lastCommitted[warningType] is exactly one committed transition per
// edge, both directions. Per transition the emission order is risk_transition
// first, then the r4_tx the sender emits.
void assessAndEmit(const std::vector<ada::cra::ICollisionRiskAssessment*>& plugins,
                   ada::store::TrackStore& store, ada::cra::AssessmentDb& assessmentDb,
                   ada::output::IviSender& sender, ada::log::EventLog& eventLog,
                   std::int64_t assessLogEveryMs, std::int64_t nowMonoMs,
                   std::map<std::string, std::string>& lastCommitted,
                   std::map<std::string, std::int64_t>& lastBUnknownEmit) {
  for (ada::cra::ICollisionRiskAssessment* plugin : plugins) {
    ada::cra::RiskContext context{store, assessmentDb, nowMonoMs};
    const ada::cra::RiskFinding finding = plugin->assess(context);

    if (finding.rationale == "b_unknown") {
      // D5's no-B skip. The exact rationale string is the plugin's contract;
      // rate-limited to one line per ASSESS_LOG_EVERY_MS per warningType so a
      // long B-less stretch stays one heartbeat, not one line per tick.
      const auto it = lastBUnknownEmit.find(finding.warningType);
      if (it == lastBUnknownEmit.end() || nowMonoMs - it->second >= assessLogEveryMs) {
        eventLog.emit(ada::log::events::kAssessSkippedBUnknown,
                      {{"warningType", finding.warningType}, {"rationale", "b_unknown"}});
        lastBUnknownEmit[finding.warningType] = nowMonoMs;
      }
    }

    std::string& last = lastCommitted.try_emplace(finding.warningType, "low").first->second;
    if (finding.riskState == last) {
      continue;  // steady state emits nothing (D5)
    }

    // Evidence for the transition line: d_AC and TTC from the assessment
    // record, null when no record exists for the trigger.
    const std::optional<ada::cra::AssessmentRecord> record = assessmentDb.get(
        finding.trigger ? finding.trigger->id : std::string{}, finding.warningType);

    // Composition inputs per the composer's contract: C's STORED entry only
    // while `tracked`; the record's remembered lastKnownB as the fallback.
    std::optional<ada::contracts::TrackedObject> trackedC;
    if (finding.trigger) {
      std::optional<ada::contracts::TrackedObject> stored = store.get(finding.trigger->id);
      if (stored && stored->state == ada::contracts::TrackState::tracked) {
        trackedC = std::move(stored);
      }
    }
    const std::optional<ada::fusion::SceneGeometry> geometry = ada::fusion::compose(
        store, trackedC, record ? record->lastKnownB : std::nullopt);

    eventLog.emit(ada::log::events::kRiskTransition,
                  {{"id", finding.trigger ? nlohmann::json(finding.trigger->id)
                                          : nlohmann::json(nullptr)},
                   {"source", "v2x_relayed"},
                   {"warningType", finding.warningType},
                   {"from", last},
                   {"to", finding.riskState},
                   {"d_ac", record ? nlohmann::json(record->distanceM)
                                   : nlohmann::json(nullptr)},
                   {"ttc", optionalToJson(record ? record->ttcS : std::nullopt)},
                   {"rationale", finding.rationale}});

    if (finding.trigger && geometry) {
      const ada::contracts::R4WarningEvent event = ada::output::buildWarning(finding, *geometry);
      sender.send(event);  // emits the r4_tx line; a failure is counted inside, never thrown
    }
    // No datagram otherwise. D5's invariant — no medium/high is ever entered
    // without a known B, so lastKnownB always makes the geometry composable —
    // leaves trigger and geometry present on every real transition; the only
    // edge reaching here is the b_unknown return to low, already carried by
    // the risk_transition rationale above.

    last = finding.riskState;
  }
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
  // enabled() throws on any name the registry does not carry, and its result
  // is the assessment order the fusion tick walks — materialized once; the
  // enabled set never changes at runtime. Guarded on a non-empty registry so
  // a build carrying no builtin plugin still starts (the 13.2.6.4 ruling).
  const std::vector<ada::cra::ICollisionRiskAssessment*> enabledPlugins =
      registry.size() > 0 ? registry.enabled(config.craEnabled)
                          : std::vector<ada::cra::ICollisionRiskAssessment*>{};

  ada::observer::InputQueue queue(kInputQueueCapacity);
  ada::observer::V2xListener listener(config.v2xListenHost, config.v2xListenPort, queue,
                                      kListenerPollTimeout);
  ada::observer::DetectorReader detector(config.detectorEnabled, config.detectorCmd,
                                         config.detectorRestartMax, queue, eventLog);

  ada::parser::R2Parser r2Parser;
  ada::parser::R3Parser r3Parser;

  // The R4 egress (15.4.2.3): the sender's one socket. Construction failure
  // throws — the startup-failure path (exit 2), like the listener's bind.
  ada::net::UdpSocket egressSocket;
  ada::output::IviSender iviSender(egressSocket, config.iviEcuHost, config.iviEcuPort,
                                   eventLog);

  // Edge-detection state for assessAndEmit: the committed riskState per
  // warningType (absent = "low", the vocabulary's floor) and the last
  // assess_skipped_b_unknown emission per warningType (the rate limiter).
  std::map<std::string, std::string> lastCommitted;
  std::map<std::string, std::int64_t> lastBUnknownEmit;

  // The fusion tick (D2): expire() runs every FUSION_TICK_MS of monotonic
  // time whether or not anything arrived, so a track expires on silence
  // alone. Each tick then runs the assess/emit stage — every enabled plugin
  // assessed, committed risk transitions emitted and sent (assessAndEmit
  // above), still on this single-writer thread. The pop timeout is the time
  // left until the next tick — one wait serves both the tick cadence and
  // shutdown promptness.
  std::int64_t nextTickMonoMs = monoNowMs() + config.fusionTickMs;
  while (g_stopRequested == 0) {
    const std::int64_t nowMonoMs = monoNowMs();
    if (nowMonoMs >= nextTickMonoMs) {
      store.expire(nowMonoMs);
      assessAndEmit(enabledPlugins, store, assessmentDb, iviSender, eventLog,
                    config.assessLogEveryMs, nowMonoMs, lastCommitted, lastBUnknownEmit);
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
