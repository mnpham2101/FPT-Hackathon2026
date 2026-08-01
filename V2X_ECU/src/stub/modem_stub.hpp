#ifndef V2X_ECU_STUB_MODEM_STUB_HPP
#define V2X_ECU_STUB_MODEM_STUB_HPP

// V2X_ECU/src/stub/modem_stub.hpp — the R8 modem stub FSM (subtask 8.1.3.2,
// Phase 1 HLD D2): idle → initialized → configured → rx-subscribed, acking
// each call with the seam's RadioResult. Happy path only in this subtask;
// fault injection + the D2 recovery table land in 8.1.3.3 — the FaultPlan and
// RetryParams constructor params exist now so the signature does not churn,
// and plans other than FaultPlan::None are stored unused.
//
// Every call outcome — accepted transition or illegal-order rejection — is
// reported to the injectable TransitionObserver, so the event log later shows
// acks and rejections alike (main wires it to EventLog::stubTransition).
// The constructor takes plain params and never reads env (config/config.hpp's
// loadFromEnv is the app's only env reader).
//
// Pure logic, transport-blind (HLD D1): no sockets/threads here — the socket
// side of rx-subscribed arrives with StubRadioAdapter (7.1.3.4) via net::;
// tools/check_transport_imports.py enforces this permanently.

#include <chrono>
#include <functional>
#include <string>

#include "adapter/i_radio_adapter.hpp"
#include "config/config.hpp"

namespace v2x::stub {

// The R8 FSM states, in bring-up order (HLD D2).
enum class StubState {
  Idle,
  Initialized,
  Configured,
  RxSubscribed,
};

// Log-friendly name of a state (inline, mirrors adapter::toString).
inline const char* toString(StubState state) {
  switch (state) {
    case StubState::Idle:
      return "Idle";
    case StubState::Initialized:
      return "Initialized";
    case StubState::Configured:
      return "Configured";
    case StubState::RxSubscribed:
      return "RxSubscribed";
  }
  return "Unknown";  // unreachable for valid enumerators; silences -Wreturn-type
}

// One call outcome. Accepted transitions carry from != to; illegal-order
// rejections carry from == to (no state change) with the failure code.
struct TransitionEvent {
  StubState from;
  StubState to;
  std::string call;  // "init" | "configure" | "subscribeRx"
  v2x::adapter::RadioResult result;
};

// Invoked on EVERY call outcome — acks and rejections alike. Main wires this
// to EventLog::stubTransition; tests inject a recording observer. An empty
// observer is valid and simply drops events.
using TransitionObserver = std::function<void(const TransitionEvent&)>;

// Retry knobs for the D2 recoveries (INIT_RETRY_MAX / RETRY_BACKOFF_MS, HLD
// §6), fed from the validated Config by the composition root. Stored now,
// consumed for real by 8.1.3.3 — no retry/backoff/sleep exists in this
// subtask.
struct RetryParams {
  int init_retry_max;
  std::chrono::milliseconds retry_backoff;
};

// The stub modem behind the R7 seam's radio side. Each bring-up call is legal
// from exactly one state (the D2 order) and acks Ok; an out-of-order call
// changes nothing and returns the matching failure code, observer notified.
class ModemStub {
 public:
  ModemStub(v2x::config::FaultPlan plan, RetryParams retry, TransitionObserver observer);

  ModemStub(const ModemStub&) = delete;
  ModemStub& operator=(const ModemStub&) = delete;

  v2x::adapter::RadioResult init();        // Idle → Initialized; else InitFailed
  v2x::adapter::RadioResult configure(    // Initialized → Configured; else ConfigureRejected
      const v2x::adapter::RadioConfig& config);
  v2x::adapter::RadioResult subscribeRx();  // Configured → RxSubscribed; else SubscribeFailed

  StubState state() const;

  // The RadioConfig stored by the last accepted configure(); value-initialized
  // (rx_port 0) until then. Consumed by StubRadioAdapter (7.1.3.4).
  const v2x::adapter::RadioConfig& config() const;

 private:
  // Applies one call outcome: sets state_ to `to` (== state_ on rejection),
  // notifies the observer, returns `result`.
  v2x::adapter::RadioResult applyOutcome(StubState to, const char* call,
                                         v2x::adapter::RadioResult result);

  v2x::config::FaultPlan plan_;  // stored; only FaultPlan::None is implemented until 8.1.3.3
  RetryParams retry_;            // stored; unused until 8.1.3.3
  TransitionObserver observer_;
  StubState state_{StubState::Idle};
  v2x::adapter::RadioConfig config_{};
};

}  // namespace v2x::stub

#endif  // V2X_ECU_STUB_MODEM_STUB_HPP
