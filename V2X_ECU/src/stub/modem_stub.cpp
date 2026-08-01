// V2X_ECU/src/stub/modem_stub.cpp — R8 FSM transitions (subtask 8.1.3.2,
// Phase 1 HLD D2), happy path only: FaultPlan::None is the only implemented
// path; fault injection + the D2 recoveries are 8.1.3.3's scope.

#include "stub/modem_stub.hpp"

namespace v2x::stub {

using v2x::adapter::RadioConfig;
using v2x::adapter::RadioResult;

ModemStub::ModemStub(v2x::config::FaultPlan plan, RetryParams retry, TransitionObserver observer)
    : plan_(plan), retry_(retry), observer_(observer) {}

RadioResult ModemStub::applyOutcome(StubState to, const char* call, RadioResult result) {
  const StubState from = state_;
  state_ = to;
  if (observer_) {
    observer_(TransitionEvent{from, to, call, result});
  }
  return result;
}

RadioResult ModemStub::init() {
  if (state_ != StubState::Idle) {
    return applyOutcome(state_, "init", RadioResult::InitFailed);
  }
  return applyOutcome(StubState::Initialized, "init", RadioResult::Ok);
}

RadioResult ModemStub::configure(const RadioConfig& config) {
  if (state_ != StubState::Initialized) {
    return applyOutcome(state_, "configure", RadioResult::ConfigureRejected);
  }
  config_ = config;
  return applyOutcome(StubState::Configured, "configure", RadioResult::Ok);
}

RadioResult ModemStub::subscribeRx() {
  if (state_ != StubState::Configured) {
    return applyOutcome(state_, "subscribeRx", RadioResult::SubscribeFailed);
  }
  return applyOutcome(StubState::RxSubscribed, "subscribeRx", RadioResult::Ok);
}

StubState ModemStub::state() const { return state_; }

const RadioConfig& ModemStub::config() const { return config_; }

}  // namespace v2x::stub
