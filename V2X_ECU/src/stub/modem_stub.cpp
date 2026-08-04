// V2X_ECU/src/stub/modem_stub.cpp — R8 FSM transitions (8.1.3.2) + fault
// injection with the D2 recovery table (8.1.3.3, HLD decision D2). Semantics
// documented in the header; every fault path here mirrors one D2 recovery row.

#include "stub/modem_stub.hpp"

#include <thread>  // default real-sleep Sleeper only — no thread is spawned here
#include <utility>

namespace v2x::stub {

using v2x::adapter::RadioConfig;
using v2x::adapter::RadioResult;
using v2x::config::FaultPlan;

ModemStub::ModemStub(FaultPlan plan, RetryParams retry, TransitionObserver observer,
                     Sleeper sleeper, int fault_fail_count)
    : plan_(plan),
      retry_(retry),
      observer_(std::move(observer)),
      sleep_(std::move(sleeper)),
      fault_remaining_(fault_fail_count) {
  // Empty Sleeper means real sleep (header contract): substitute the
  // std::this_thread::sleep_for wrapper so backoff sites never null-check.
  if (!sleep_) {
    sleep_ = [](std::chrono::milliseconds duration) { std::this_thread::sleep_for(duration); };
  }
}

void ModemStub::emitEvent(EventKind kind, StubState from, StubState to, const char* call,
                          RadioResult result) {
  if (observer_) {
    observer_(TransitionEvent{kind, from, to, call, result});
  }
}

RadioResult ModemStub::applyOutcome(EventKind kind, StubState to, const char* call,
                                    RadioResult result) {
  const StubState from = state_;
  state_ = to;
  emitEvent(kind, from, to, call, result);
  return result;
}

bool ModemStub::runBoundedFaultRetries(const char* call, RadioResult failure) {
  const int max_attempts = 1 + retry_.init_retry_max;
  for (int attempt = 1; attempt <= max_attempts; ++attempt) {
    if (fault_remaining_ <= 0) {
      return true;  // fail budget spent — this attempt succeeds
    }
    --fault_remaining_;
    emitEvent(EventKind::FaultInjected, state_, state_, call, failure);
    if (attempt < max_attempts) {
      sleep_(retry_.retry_backoff);  // constant backoff per retry (D2)
    }
  }
  return false;  // all 1 + init_retry_max attempts failed — terminal
}

RadioResult ModemStub::init() {
  // Illegal-order check FIRST: an out-of-order call is a Reject regardless of
  // the selected fault plan.
  if (state_ != StubState::Idle) {
    return applyOutcome(EventKind::Reject, state_, "init", RadioResult::InitFailed);
  }
  if (plan_ == FaultPlan::InitFail && fault_remaining_ > 0) {
    if (!runBoundedFaultRetries("init", RadioResult::InitFailed)) {
      // Terminal failure: state stays Idle, no Ack. The caller exits non-zero;
      // container restart is the logged last-resort recovery (D2).
      return RadioResult::InitFailed;
    }
    emitEvent(EventKind::Recovery, state_, state_, "init", RadioResult::Ok);
  }
  return applyOutcome(EventKind::Ack, StubState::Initialized, "init", RadioResult::Ok);
}

RadioResult ModemStub::configure(const RadioConfig& config) {
  if (state_ != StubState::Initialized) {
    return applyOutcome(EventKind::Reject, state_, "configure", RadioResult::ConfigureRejected);
  }
  if (plan_ == FaultPlan::ConfigureReject && fault_remaining_ > 0) {
    if (!runBoundedFaultRetries("configure", RadioResult::ConfigureRejected)) {
      // Terminal (D2, as init): no Ack, nothing stored, caller exits non-zero.
      return RadioResult::ConfigureRejected;
    }
    emitEvent(EventKind::Recovery, state_, state_, "configure", RadioResult::Ok);
  }
  config_ = config;  // stored only on success
  return applyOutcome(EventKind::Ack, StubState::Configured, "configure", RadioResult::Ok);
}

RadioResult ModemStub::subscribeRx() {
  if (state_ != StubState::Configured) {
    return applyOutcome(EventKind::Reject, state_, "subscribeRx", RadioResult::SubscribeFailed);
  }
  // Normal establishment ack first — the D2 drop happens AFTER establishment.
  applyOutcome(EventKind::Ack, StubState::RxSubscribed, "subscribeRx", RadioResult::Ok);
  if (plan_ == FaultPlan::SubscriptionDrop && fault_remaining_ > 0) {
    // Drop once: the only injected event that moves state (RxSubscribed →
    // Configured).
    --fault_remaining_;
    applyOutcome(EventKind::FaultInjected, StubState::Configured, "subscribeRx",
                 RadioResult::SubscribeFailed);
    sleep_(retry_.retry_backoff);
    // Automatic re-subscribe: UNBOUNDED retry loop — deliberately not limited
    // by init_retry_max (that ceiling bounds only init/configure); it runs
    // until the fail budget is spent, i.e. until a re-attempt succeeds.
    while (fault_remaining_ > 0) {
      --fault_remaining_;
      emitEvent(EventKind::FaultInjected, state_, state_, "subscribeRx",
                RadioResult::SubscribeFailed);
      sleep_(retry_.retry_backoff);
    }
    emitEvent(EventKind::Recovery, state_, state_, "subscribeRx", RadioResult::Ok);
    applyOutcome(EventKind::Ack, StubState::RxSubscribed, "subscribeRx", RadioResult::Ok);
  }
  return RadioResult::Ok;
}

StubState ModemStub::state() const { return state_; }

const RadioConfig& ModemStub::config() const { return config_; }

}  // namespace v2x::stub
