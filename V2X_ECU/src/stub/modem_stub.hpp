#ifndef V2X_ECU_STUB_MODEM_STUB_HPP
#define V2X_ECU_STUB_MODEM_STUB_HPP

// V2X_ECU/src/stub/modem_stub.hpp — the R8 modem stub FSM (subtasks 8.1.3.2 +
// 8.1.3.3, Phase 1 HLD D2): idle → initialized → configured → rx-subscribed,
// acking each call with the seam's RadioResult, plus config-driven fault
// injection with the D2 recovery table — each injected fault produces a
// defined, observable recovery.
//
// Fault plans (HLD D2 recovery table), selected by the FaultPlan constructor
// param (env FAULT_PLAN selects the plan at the composition root; this class
// never reads env — config/config.hpp's loadFromEnv is the app's only env
// reader):
// - InitFail / ConfigureReject — the targeted call fails; recovery = retry
//   with retry.retry_backoff between attempts, up to retry.init_retry_max
//   retries (1 + init_retry_max attempts total). If every attempt fails the
//   call returns its failure code with no state change — terminal: the caller
//   exits non-zero, and container restart is the logged last-resort recovery
//   (D2). A within-budget success emits Recovery, then the normal Ack.
// - SubscriptionDrop — subscribeRx() first acks normally (Ack, Configured →
//   RxSubscribed), then the stub immediately drops the subscription once
//   (FaultInjected, state back to Configured) and recovers by automatic
//   re-subscribe: an UNBOUNDED retry loop with the same backoff — deliberately
//   NOT limited by init_retry_max (that ceiling bounds only the bring-up
//   calls); the first succeeding re-attempt emits Recovery then the Ack back
//   to RxSubscribed, and the call returns Ok.
// - None — behavior identical to the 8.1.3.2 happy-path/rejection FSM.
//
// Faults apply only to their own call (InitFail never affects configure(),
// etc.), and the illegal-order rejection check always runs FIRST: an
// out-of-order call is a Reject regardless of the selected plan.
//
// Every outcome — accepted transition, illegal-order rejection, injected
// fault, recovery — is reported to the injectable TransitionObserver with its
// EventKind; main (a later subtask) maps the kinds onto
// EventLog::stubTransition / faultInjected / recovery (HLD D4).
//
// Pure logic, transport-blind (HLD D1): no sockets/threads in this header —
// the socket side of rx-subscribed arrives with StubRadioAdapter (7.1.3.4)
// via net::; tools/check_transport_imports.py enforces this permanently.
// (The .cpp includes <thread> solely for the default real-sleep Sleeper.)

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

// What kind of outcome an event reports. Module-internal type (the frozen R7
// seam header is untouched); main maps these onto the D4 event log:
// - Ack — an accepted transition (from != to, result Ok).
// - Reject — an illegal-order call (from == to, failure code, no state change).
// - FaultInjected — one failing attempt of the selected fault plan. Carries
//   the failure code; from == to for failing init/configure/re-subscribe
//   attempts, from RxSubscribed → to Configured for the subscription drop
//   itself (the one injected event that moves state).
// - Recovery — the fault cleared: emitted with result Ok immediately before
//   the Ack of the now-succeeding call (from == to; the state change is
//   carried by that following Ack).
enum class EventKind {
  Ack,
  Reject,
  FaultInjected,
  Recovery,
};

// One call outcome, as delivered to the observer.
struct TransitionEvent {
  EventKind kind;
  StubState from;
  StubState to;
  std::string call;  // "init" | "configure" | "subscribeRx"
  v2x::adapter::RadioResult result;
};

// Invoked on EVERY outcome — acks, rejections, injected faults, and
// recoveries alike. Main wires this to the event log; tests inject a
// recording observer. An empty observer is valid and simply drops events.
using TransitionObserver = std::function<void(const TransitionEvent&)>;

// Retry knobs for the D2 recoveries (INIT_RETRY_MAX / RETRY_BACKOFF_MS, HLD
// §6), fed from the validated Config by the composition root. retry_backoff
// is a CONSTANT wait per retry attempt, not an exponential base — D2 names a
// single backoff value.
struct RetryParams {
  int init_retry_max;
  std::chrono::milliseconds retry_backoff;
};

// Injectable backoff wait, invoked with retry.retry_backoff before each
// retry attempt. An empty Sleeper means real sleep (the constructor
// substitutes a std::this_thread::sleep_for wrapper); tests inject a
// recording no-op so they run instantly and deterministically.
using Sleeper = std::function<void(std::chrono::milliseconds)>;

// The stub modem behind the R7 seam's radio side. Each bring-up call is legal
// from exactly one state (the D2 order) and acks Ok; an out-of-order call
// changes nothing and returns the matching failure code, observer notified.
// The selected fault plan injects failures into its own call per the D2
// recovery table documented at the top of this header.
class ModemStub {
 public:
  // fault_fail_count: how many consecutive attempts the selected fault makes
  // fail before the fault clears. Test-visible knob, deliberately NOT
  // env-driven — env (FAULT_PLAN) selects only the plan; tests use the knob
  // to force retry-then-succeed vs retry-exhaustion. Default 1: one failure,
  // then the first retry succeeds.
  ModemStub(v2x::config::FaultPlan plan, RetryParams retry, TransitionObserver observer,
            Sleeper sleeper = {}, int fault_fail_count = 1);

  ModemStub(const ModemStub&) = delete;
  ModemStub& operator=(const ModemStub&) = delete;

  v2x::adapter::RadioResult init();        // Idle → Initialized; else InitFailed
  v2x::adapter::RadioResult configure(    // Initialized → Configured; else ConfigureRejected
      const v2x::adapter::RadioConfig& config);
  v2x::adapter::RadioResult subscribeRx();  // Configured → RxSubscribed; else SubscribeFailed

  StubState state() const;

  // The RadioConfig stored by the last accepted configure(); value-initialized
  // (rx_port 0) until then. Consumed by StubRadioAdapter (7.1.3.4). A
  // ConfigureReject terminal failure stores nothing.
  const v2x::adapter::RadioConfig& config() const;

 private:
  // Notifies the observer of one outcome (no state change here).
  void emitEvent(EventKind kind, StubState from, StubState to, const char* call,
                 v2x::adapter::RadioResult result);

  // Applies one call outcome: sets state_ to `to` (== state_ on rejection),
  // notifies the observer, returns `result`.
  v2x::adapter::RadioResult applyOutcome(EventKind kind, StubState to, const char* call,
                                         v2x::adapter::RadioResult result);

  // The bounded InitFail/ConfigureReject retry loop (D2 row 1): runs up to
  // 1 + retry_.init_retry_max attempts, each failing attempt (while the fail
  // budget lasts) emitting FaultInjected and sleeping retry_.retry_backoff
  // before the next. Returns true when an attempt succeeds within budget
  // (caller emits Recovery + Ack), false when every attempt failed (caller
  // returns the failure code — the terminal path).
  bool runBoundedFaultRetries(const char* call, v2x::adapter::RadioResult failure);

  v2x::config::FaultPlan plan_;
  RetryParams retry_;
  TransitionObserver observer_;
  Sleeper sleep_;
  int fault_remaining_;  // failing attempts left before the fault clears
  StubState state_{StubState::Idle};
  v2x::adapter::RadioConfig config_{};
};

}  // namespace v2x::stub

#endif  // V2X_ECU_STUB_MODEM_STUB_HPP
