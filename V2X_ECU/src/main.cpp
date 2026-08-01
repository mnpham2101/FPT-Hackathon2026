// V2X_ECU/src/main.cpp — the V2X ECU composition root (subtask 8.1.5.1; Phase 1
// HLD §4 "composition root — Config → stub/adapter → pipeline → run", §8 MVC
// mapping: main is CONTROLLER, zero business logic).
//
// What this file is allowed to do, and nothing else:
//   1. read the environment ONCE through config::loadFromEnv,
//   2. construct every collaborator in an order that makes lifetimes correct,
//   3. drive the D2 bring-up call order init → configure → subscribeRx,
//   4. block until signalled, then stop the Rx thread and exit.
//
// What this file deliberately does NOT do (each has exactly one owner
// elsewhere): unit conversion and R2 derivation (pipeline/r2_builder), profile
// validation (pipeline/validator), duplicate suppression (pipeline/deduper),
// event counting (log/event_log — the frozen "counters" object), retry/backoff
// and fault recovery (stub/modem_stub, HLD D2), socket handling
// (net/udp_socket, the sole socket-API holder — this file includes no transport
// header and tools/check_transport_imports.py enforces that).
//
// EXIT CODES (stable — the D7 CI lane and the container restart policy key on
// "non-zero means terminal"):
//   0  clean shutdown: SIGTERM/SIGINT received, Rx thread joined.
//   2  invalid environment — config::ConfigError or any other load failure.
//      Nothing was constructed; fix the blueprint node config.
//   3  init() terminal failure after the stub's D2 retries were exhausted.
//   4  configure() terminal failure after the stub's D2 retries.
//   5  subscribeRx() failed (stub rejection, or the Rx socket could not bind).
// For 3/4/5 the HLD D2 recovery table names container restart as the logged
// last-resort recovery — that is the platform's job, so main only reports and
// exits.
// Only the config load is wrapped in a try/catch, and deliberately so: every
// later construction step that can throw at all (net::SocketError from the
// forwarder's send socket) means the process cannot function, and the resulting
// std::terminate is itself a non-zero exit with the same restart recovery. The
// two edges that must never throw already guarantee it — RxPipeline::onDatagram
// is noexcept and AdaForwarder::send returns false — so no runtime failure of
// the serving path can reach this frame.
//
// WIRING ORDER (declaration order below is load-bearing: destruction runs in
// reverse, so the adapter — which owns the Rx thread — is torn down and joined
// BEFORE the pipeline, forwarder and event log it calls into):
//   Config → EventLog → AdaForwarder → codec + stages → RxPipeline
//          → ModemStub (observer → EventLog) → StubRadioAdapter.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "adapter/i_radio_adapter.hpp"
#include "adapter/stub_radio_adapter.hpp"
#include "codec/vanetza_cpm_codec.hpp"
#include "config/config.hpp"
#include "contracts/r2_message.hpp"
#include "forward/ada_forwarder.hpp"
#include "log/event_log.hpp"
#include "pipeline/deduper.hpp"
#include "pipeline/r2_builder.hpp"
#include "pipeline/rx_pipeline.hpp"
#include "pipeline/validator.hpp"
#include "stub/modem_stub.hpp"

namespace {

// Shutdown request, set by the signal handler and polled by the main loop.
//
// Why std::signal and a flag+poll loop are sufficient here — no sigaction, no
// self-pipe, no signalfd: the handler's ONLY action is a lock-free atomic
// store, which is async-signal-safe (asserted below), and nothing in this
// process blocks indefinitely waiting to be woken. Main polls the flag on a
// short cadence, and the Rx thread never needs waking at all — its socket is
// armed with StubRadioAdapter::kRxPollTimeout, so its blocked recvFrom returns
// on its own at least that often and the loop re-reads the adapter's stop flag
// (see stub_radio_adapter.hpp "Shutdown without a self-pipe"). std::signal is
// also the portable spelling, which keeps this file free of POSIX headers.
std::atomic<bool> g_stop_requested{false};

static_assert(std::atomic<bool>::is_always_lock_free,
              "the signal handler stores to g_stop_requested; a lock-based "
              "atomic would not be async-signal-safe");

// Async-signal-safe by construction: one relaxed atomic store, nothing else —
// no allocation, no iostreams, no locks. (Deliberately not declared extern "C":
// a C-language-linkage name inside an unnamed namespace is a portability trap,
// and std::signal takes a plain void(*)(int).)
void handleStopSignal(int /*signum*/) {
  g_stop_requested.store(true, std::memory_order_relaxed);
}

// Log-friendly name of a FAULT_PLAN selection. Presentation only (it feeds the
// one-line [BOOT] banner); the env spellings themselves are parsed by
// config::loadFromEnv, the app's only env reader.
const char* faultPlanName(v2x::config::FaultPlan plan) {
  switch (plan) {
    case v2x::config::FaultPlan::None:
      return "none";
    case v2x::config::FaultPlan::InitFail:
      return "init_fail";
    case v2x::config::FaultPlan::ConfigureReject:
      return "configure_reject";
    case v2x::config::FaultPlan::SubscriptionDrop:
      return "subscription_drop";
  }
  return "unknown";  // unreachable for valid enumerators; silences -Wreturn-type
}

// Log-friendly name of a stub outcome kind. Only reaches the non-frozen
// "detail" text of the D4 events — the frozen "event" name is chosen by which
// EventLog method the observer calls, never by this string.
const char* eventKindName(v2x::stub::EventKind kind) {
  switch (kind) {
    case v2x::stub::EventKind::Ack:
      return "Ack";
    case v2x::stub::EventKind::Reject:
      return "Reject";
    case v2x::stub::EventKind::FaultInjected:
      return "FaultInjected";
    case v2x::stub::EventKind::Recovery:
      return "Recovery";
  }
  return "Unknown";  // unreachable for valid enumerators; silences -Wreturn-type
}

// The free-text detail carried by every stub-sourced D4 event.
std::string describeTransition(const v2x::stub::TransitionEvent& event) {
  return event.call + ": " + v2x::stub::toString(event.from) + "->" +
         v2x::stub::toString(event.to) + " kind=" + eventKindName(event.kind) +
         " result=" + v2x::adapter::toString(event.result);
}

}  // namespace

int main() {
  // --- 1. Environment (the app's single read; invalid env is terminal) ------
  v2x::config::Config cfg;
  try {
    cfg = v2x::config::loadFromEnv([](const char* name) { return std::getenv(name); });
  } catch (const std::exception& error) {
    // Catches config::ConfigError (a std::invalid_argument) and any other
    // load failure alike — both mean "the blueprint node config is wrong".
    std::cerr << "[FATAL] config: " << error.what() << std::endl;
    return 2;
  }

  // --- 2. R18 evidence stream (stdout always; file sink when configured) ----
  v2x::log::EventLog log(std::cout, cfg.event_log_path);

  // One-line boot banner for CarSky View Log — the operator's proof of which
  // configuration the container actually came up with. Plain [BOOT] text, not
  // an [EVT] line: the D4 vocabulary is closed, and the D7 checker parses
  // [EVT] only.
  std::cout << "[BOOT] v2x_ecu listen_port=" << cfg.listen_port
            << " ada_target=" << cfg.ada_ecu_host << ":" << cfg.ada_ecu_port
            << " fault_plan=" << faultPlanName(cfg.fault_plan)
            << " init_retry_max=" << cfg.init_retry_max
            << " retry_backoff_ms=" << cfg.retry_backoff.count()
            << " dedupe_window_ms=" << cfg.dedupe_window.count()
            << " event_log_path='" << cfg.event_log_path << "'" << std::endl;

  // --- 3. Pipeline egress + the four R9 stages -----------------------------
  // Declared before the pipeline that references them: reverse-order
  // destruction then tears the pipeline down first.
  v2x::forward::AdaForwarder forwarder(cfg.ada_ecu_host, cfg.ada_ecu_port);

  v2x::codec::VanetzaCpmCodec codec;
  v2x::pipeline::Validator validator;
  v2x::pipeline::Deduper deduper(cfg.dedupe_window);
  v2x::pipeline::R2Builder builder;

  v2x::pipeline::RxPipeline pipeline(
      codec, validator, deduper, builder, log,
      // Stage-4b egress. The forwarder already logs its own [FWD-ERR] line and
      // returns false rather than throwing; a send failure is fire-and-forget
      // per HLD D3, so there is nothing for the controller to decide here.
      [&forwarder](const v2x::contracts::R2Message& r2) { forwarder.send(r2); });

  // --- 4. R8 stub with its observer on the D4 event stream ------------------
  // The kind → event-name mapping (HLD D4) lives here because main is the only
  // component that knows both the stub's vocabulary and the event log's.
  v2x::stub::ModemStub stub(
      cfg.fault_plan, {cfg.init_retry_max, cfg.retry_backoff},
      [&log](const v2x::stub::TransitionEvent& event) {
        const std::string detail = describeTransition(event);
        switch (event.kind) {
          case v2x::stub::EventKind::Ack:
          case v2x::stub::EventKind::Reject:
            log.stubTransition(detail);
            break;
          case v2x::stub::EventKind::FaultInjected:
            log.faultInjected(detail);
            break;
          case v2x::stub::EventKind::Recovery:
            log.recovery(detail);
            break;
        }
      });

  // --- 5. R7 seam implementation (owns the Rx socket + Rx thread) -----------
  v2x::adapter::StubRadioAdapter adapter(
      stub, [](const std::string& message) { std::cerr << message << std::endl; });

  // Installed before the Rx thread can exist, so a signal arriving at any
  // moment after this point is observed rather than defaulting to terminate.
  std::signal(SIGTERM, handleStopSignal);
  std::signal(SIGINT, handleStopSignal);

  // --- 6. Scripted D2 bring-up: init → configure → subscribeRx -------------
  // Every retry and backoff of the D2 recovery table happens INSIDE the stub
  // (modem_stub.hpp): by the time a result reaches this code it is terminal, so
  // main adds no retry loop of its own — it only maps the terminal result onto
  // an exit code.
  if (adapter.init() != v2x::adapter::RadioResult::Ok) {
    std::cerr << "[FATAL] init failed after retries (container restart is the recovery)"
              << std::endl;
    return 3;
  }

  if (adapter.configure(v2x::adapter::RadioConfig{cfg.listen_port}) !=
      v2x::adapter::RadioResult::Ok) {
    std::cerr << "[FATAL] configure failed after retries (container restart is the recovery)"
              << std::endl;
    return 4;
  }

  if (adapter.subscribeRx([&pipeline](const std::vector<std::uint8_t>& bytes) {
        pipeline.onDatagram(bytes);
      }) != v2x::adapter::RadioResult::Ok) {
    std::cerr << "[FATAL] subscribeRx failed (container restart is the recovery)" << std::endl;
    return 5;
  }

  // --- 7. Run: the Rx thread serves traffic; main just waits for the signal -
  std::cout << "[BOOT] bring-up complete, serving Rx on port " << cfg.listen_port << std::endl;

  while (!g_stop_requested.load(std::memory_order_relaxed)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  std::cout << "[SHUTDOWN] signal received, stopping Rx" << std::endl;
  adapter.stop();  // joins the Rx thread; idempotent, never throws
  return 0;
}
