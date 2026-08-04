// R8 modem stub FSM test (subtasks 8.1.3.2 + 8.1.3.3, HLD decision D2): the
// full scripted call flow acked in order, every illegal-order call rejected
// with its failure code and no state change, every outcome observed — plus
// all four FAULT_PLAN values with their D2 recoveries: bounded retry-then-
// succeed and retry-exhaustion terminal paths for init_fail/configure_reject,
// unbounded automatic re-subscribe after subscription_drop. Backoff waits go
// through a recording Sleeper so the suite runs instantly and asserts the
// exact sleep count/duration.

#include "stub/modem_stub.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "adapter/i_radio_adapter.hpp"
#include "config/config.hpp"

namespace {

using v2x::adapter::RadioConfig;
using v2x::adapter::RadioResult;
using v2x::config::FaultPlan;
using v2x::stub::EventKind;
using v2x::stub::ModemStub;
using v2x::stub::RetryParams;
using v2x::stub::StubState;
using v2x::stub::toString;
using v2x::stub::TransitionEvent;

constexpr std::uint16_t kRxPort = 47100;

class ModemStubFsmTest : public ::testing::Test {
 protected:
  // Builds a stub wired to the recording observer and recording sleeper. The
  // fault plan and fail-count knob are parameters (defaults None / 1) so the
  // happy-path and fault cases share one fixture.
  ModemStub makeStub(FaultPlan plan = FaultPlan::None, int fault_fail_count = 1) {
    return ModemStub(
        plan, retry_, [this](const TransitionEvent& event) { events_.push_back(event); },
        [this](std::chrono::milliseconds duration) { sleeps_.push_back(duration); },
        fault_fail_count);
  }

  // Drives the stub to `target` via the happy path, asserting each ack, then
  // clears the recordings so the test body sees only what it triggers.
  void bringUpTo(ModemStub& stub, StubState target) {
    if (target != StubState::Idle) {
      ASSERT_EQ(stub.init(), RadioResult::Ok);
    }
    if (target == StubState::Configured || target == StubState::RxSubscribed) {
      ASSERT_EQ(stub.configure(RadioConfig{kRxPort}), RadioResult::Ok);
    }
    if (target == StubState::RxSubscribed) {
      ASSERT_EQ(stub.subscribeRx(), RadioResult::Ok);
    }
    ASSERT_EQ(stub.state(), target);
    events_.clear();
    sleeps_.clear();
  }

  void expectEvent(const TransitionEvent& event, EventKind kind, StubState from, StubState to,
                   const std::string& call, RadioResult result) {
    EXPECT_EQ(event.kind, kind);
    EXPECT_EQ(event.from, from);
    EXPECT_EQ(event.to, to);
    EXPECT_EQ(event.call, call);
    EXPECT_EQ(event.result, result);
  }

  // Asserts one illegal-order outcome: the failure code came back, the state
  // did not move, and the observer saw exactly the rejection (from == to).
  void expectRejection(const ModemStub& stub, RadioResult actual, RadioResult expected_code,
                       StubState expected_state, const std::string& call) {
    EXPECT_EQ(actual, expected_code);
    EXPECT_EQ(stub.state(), expected_state);
    ASSERT_EQ(events_.size(), 1u);
    expectEvent(events_.back(), EventKind::Reject, expected_state, expected_state, call,
                expected_code);
    events_.clear();
  }

  // Asserts exactly `count` recorded backoff waits, each of the constant
  // retry_backoff duration (D2 names a single backoff value, not exponential).
  void expectSleeps(std::size_t count) {
    ASSERT_EQ(sleeps_.size(), count);
    for (const auto& duration : sleeps_) {
      EXPECT_EQ(duration, retry_.retry_backoff);
    }
  }

  RetryParams retry_{3, std::chrono::milliseconds{500}};  // HLD §6 defaults
  std::vector<TransitionEvent> events_;
  std::vector<std::chrono::milliseconds> sleeps_;
};

// (a) Happy path: every call acked Ok, states traverse the D2 order, observer
// saw exactly the three accepted transitions with matching fields, no sleeps.
TEST_F(ModemStubFsmTest, HappyPathAcksEachCallInOrder) {
  ModemStub stub = makeStub();
  EXPECT_EQ(stub.state(), StubState::Idle);

  EXPECT_EQ(stub.init(), RadioResult::Ok);
  EXPECT_EQ(stub.state(), StubState::Initialized);

  EXPECT_EQ(stub.configure(RadioConfig{kRxPort}), RadioResult::Ok);
  EXPECT_EQ(stub.state(), StubState::Configured);

  EXPECT_EQ(stub.subscribeRx(), RadioResult::Ok);
  EXPECT_EQ(stub.state(), StubState::RxSubscribed);

  ASSERT_EQ(events_.size(), 3u);
  expectEvent(events_[0], EventKind::Ack, StubState::Idle, StubState::Initialized, "init",
              RadioResult::Ok);
  expectEvent(events_[1], EventKind::Ack, StubState::Initialized, StubState::Configured,
              "configure", RadioResult::Ok);
  expectEvent(events_[2], EventKind::Ack, StubState::Configured, StubState::RxSubscribed,
              "subscribeRx", RadioResult::Ok);
  expectSleeps(0);
}

// (b) Illegal-order rejections — all 9 out-of-order (state, call) combos: the
// 4 states × 3 calls minus the 3 legal transitions.

TEST_F(ModemStubFsmTest, ConfigureBeforeInitRejected) {
  ModemStub stub = makeStub();
  expectRejection(stub, stub.configure(RadioConfig{kRxPort}), RadioResult::ConfigureRejected,
                  StubState::Idle, "configure");
}

TEST_F(ModemStubFsmTest, SubscribeRxBeforeInitRejected) {
  ModemStub stub = makeStub();
  expectRejection(stub, stub.subscribeRx(), RadioResult::SubscribeFailed, StubState::Idle,
                  "subscribeRx");
}

TEST_F(ModemStubFsmTest, DoubleInitRejected) {
  ModemStub stub = makeStub();
  bringUpTo(stub, StubState::Initialized);
  expectRejection(stub, stub.init(), RadioResult::InitFailed, StubState::Initialized, "init");
}

TEST_F(ModemStubFsmTest, SubscribeRxBeforeConfigureRejected) {
  ModemStub stub = makeStub();
  bringUpTo(stub, StubState::Initialized);
  expectRejection(stub, stub.subscribeRx(), RadioResult::SubscribeFailed, StubState::Initialized,
                  "subscribeRx");
}

TEST_F(ModemStubFsmTest, InitAfterConfigureRejected) {
  ModemStub stub = makeStub();
  bringUpTo(stub, StubState::Configured);
  expectRejection(stub, stub.init(), RadioResult::InitFailed, StubState::Configured, "init");
}

TEST_F(ModemStubFsmTest, DoubleConfigureRejectedAndKeepsStoredConfig) {
  ModemStub stub = makeStub();
  bringUpTo(stub, StubState::Configured);  // configured with kRxPort
  expectRejection(stub, stub.configure(RadioConfig{50000}), RadioResult::ConfigureRejected,
                  StubState::Configured, "configure");
  EXPECT_EQ(stub.config().rx_port, kRxPort);  // rejected configure stored nothing
}

TEST_F(ModemStubFsmTest, EveryCallAfterRxSubscribedRejected) {
  ModemStub stub = makeStub();
  bringUpTo(stub, StubState::RxSubscribed);
  expectRejection(stub, stub.init(), RadioResult::InitFailed, StubState::RxSubscribed, "init");
  expectRejection(stub, stub.configure(RadioConfig{kRxPort}), RadioResult::ConfigureRejected,
                  StubState::RxSubscribed, "configure");
  expectRejection(stub, stub.subscribeRx(), RadioResult::SubscribeFailed, StubState::RxSubscribed,
                  "subscribeRx");
}

// (c) config() exposes the RadioConfig stored by the accepted configure.
TEST_F(ModemStubFsmTest, ConfigReturnsStoredRadioConfigAfterConfigure) {
  ModemStub stub = makeStub();
  EXPECT_EQ(stub.config().rx_port, 0u);  // value-initialized before configure
  ASSERT_EQ(stub.init(), RadioResult::Ok);
  ASSERT_EQ(stub.configure(RadioConfig{kRxPort}), RadioResult::Ok);
  EXPECT_EQ(stub.config().rx_port, kRxPort);
}

// An empty observer is valid: outcomes are simply not reported. Also proves
// the 8.1.3.2 three-arg constructor call shape still compiles (Sleeper and
// fault_fail_count defaulted).
TEST_F(ModemStubFsmTest, EmptyObserverIsValid) {
  ModemStub stub(FaultPlan::None, retry_, {});
  EXPECT_EQ(stub.init(), RadioResult::Ok);
  EXPECT_EQ(stub.init(), RadioResult::InitFailed);  // rejection path, unobserved
  EXPECT_EQ(stub.state(), StubState::Initialized);
}

TEST_F(ModemStubFsmTest, ToStringCoversEveryStubState) {
  EXPECT_STREQ(toString(StubState::Idle), "Idle");
  EXPECT_STREQ(toString(StubState::Initialized), "Initialized");
  EXPECT_STREQ(toString(StubState::Configured), "Configured");
  EXPECT_STREQ(toString(StubState::RxSubscribed), "RxSubscribed");
}

// ---- 8.1.3.3: fault plans + D2 recoveries ----------------------------------

// (a) init_fail retry-then-succeed: one failing attempt, one backoff wait,
// then the retry recovers — FaultInjected → Recovery → Ack, state Initialized.
TEST_F(ModemStubFsmTest, InitFailRetriesWithBackoffThenRecovers) {
  ModemStub stub = makeStub(FaultPlan::InitFail, /*fault_fail_count=*/1);  // init_retry_max = 3
  EXPECT_EQ(stub.init(), RadioResult::Ok);
  EXPECT_EQ(stub.state(), StubState::Initialized);

  ASSERT_EQ(events_.size(), 3u);
  expectEvent(events_[0], EventKind::FaultInjected, StubState::Idle, StubState::Idle, "init",
              RadioResult::InitFailed);
  expectEvent(events_[1], EventKind::Recovery, StubState::Idle, StubState::Idle, "init",
              RadioResult::Ok);
  expectEvent(events_[2], EventKind::Ack, StubState::Idle, StubState::Initialized, "init",
              RadioResult::Ok);
  expectSleeps(1);
}

// (b) init_fail retry-exhaustion: fail budget outlasts 1 + init_retry_max
// attempts → terminal InitFailed, state stays Idle, only FaultInjected events,
// no sleep after the final attempt.
TEST_F(ModemStubFsmTest, InitFailExhaustsRetriesAndStaysIdle) {
  ModemStub stub = makeStub(FaultPlan::InitFail, /*fault_fail_count=*/10);  // > init_retry_max = 3
  EXPECT_EQ(stub.init(), RadioResult::InitFailed);
  EXPECT_EQ(stub.state(), StubState::Idle);

  ASSERT_EQ(events_.size(), 4u);  // 1 + init_retry_max attempts, all failed
  for (const auto& event : events_) {
    expectEvent(event, EventKind::FaultInjected, StubState::Idle, StubState::Idle, "init",
                RadioResult::InitFailed);
  }
  expectSleeps(3);  // between attempts only — none after the terminal one
}

// (c) configure_reject retry-then-succeed: same shape as init_fail, from
// Initialized; the config is stored on the recovered success.
TEST_F(ModemStubFsmTest, ConfigureRejectRetriesThenRecoversAndStoresConfig) {
  ModemStub stub = makeStub(FaultPlan::ConfigureReject, /*fault_fail_count=*/1);
  bringUpTo(stub, StubState::Initialized);

  EXPECT_EQ(stub.configure(RadioConfig{kRxPort}), RadioResult::Ok);
  EXPECT_EQ(stub.state(), StubState::Configured);
  EXPECT_EQ(stub.config().rx_port, kRxPort);

  ASSERT_EQ(events_.size(), 3u);
  expectEvent(events_[0], EventKind::FaultInjected, StubState::Initialized,
              StubState::Initialized, "configure", RadioResult::ConfigureRejected);
  expectEvent(events_[1], EventKind::Recovery, StubState::Initialized, StubState::Initialized,
              "configure", RadioResult::Ok);
  expectEvent(events_[2], EventKind::Ack, StubState::Initialized, StubState::Configured,
              "configure", RadioResult::Ok);
  expectSleeps(1);
}

// (c) configure_reject retry-exhaustion: terminal ConfigureRejected, state
// stays Initialized, nothing stored.
TEST_F(ModemStubFsmTest, ConfigureRejectExhaustsRetriesWithoutStoringConfig) {
  ModemStub stub = makeStub(FaultPlan::ConfigureReject, /*fault_fail_count=*/10);
  bringUpTo(stub, StubState::Initialized);

  EXPECT_EQ(stub.configure(RadioConfig{kRxPort}), RadioResult::ConfigureRejected);
  EXPECT_EQ(stub.state(), StubState::Initialized);
  EXPECT_EQ(stub.config().rx_port, 0u);  // config stored only on success

  ASSERT_EQ(events_.size(), 4u);
  for (const auto& event : events_) {
    expectEvent(event, EventKind::FaultInjected, StubState::Initialized,
                StubState::Initialized, "configure", RadioResult::ConfigureRejected);
  }
  expectSleeps(3);
}

// (d) subscription_drop: establishment acks first, then the drop (the one
// injected event that moves state, RxSubscribed → Configured), then automatic
// re-subscribe recovers — end state RxSubscribed, call returned Ok.
TEST_F(ModemStubFsmTest, SubscriptionDropResubscribesAutomatically) {
  ModemStub stub = makeStub(FaultPlan::SubscriptionDrop, /*fault_fail_count=*/1);
  bringUpTo(stub, StubState::Configured);

  EXPECT_EQ(stub.subscribeRx(), RadioResult::Ok);
  EXPECT_EQ(stub.state(), StubState::RxSubscribed);

  ASSERT_EQ(events_.size(), 4u);
  expectEvent(events_[0], EventKind::Ack, StubState::Configured, StubState::RxSubscribed,
              "subscribeRx", RadioResult::Ok);
  expectEvent(events_[1], EventKind::FaultInjected, StubState::RxSubscribed,
              StubState::Configured, "subscribeRx", RadioResult::SubscribeFailed);
  expectEvent(events_[2], EventKind::Recovery, StubState::Configured, StubState::Configured,
              "subscribeRx", RadioResult::Ok);
  expectEvent(events_[3], EventKind::Ack, StubState::Configured, StubState::RxSubscribed,
              "subscribeRx", RadioResult::Ok);
  expectSleeps(1);
}

// (e) subscription_drop is unbounded: a fail budget beyond init_retry_max
// still ends in Ok/RxSubscribed — the re-subscribe loop is deliberately NOT
// limited by the init/configure retry ceiling. One backoff wait per failing
// attempt (drop included) → sleep count == fail_count.
TEST_F(ModemStubFsmTest, SubscriptionDropResubscribeIsUnboundedByInitRetryMax) {
  const int fail_count = retry_.init_retry_max + 3;
  ModemStub stub = makeStub(FaultPlan::SubscriptionDrop, fail_count);
  bringUpTo(stub, StubState::Configured);

  EXPECT_EQ(stub.subscribeRx(), RadioResult::Ok);
  EXPECT_EQ(stub.state(), StubState::RxSubscribed);

  // Ack + fail_count FaultInjected + Recovery + Ack.
  const auto count = static_cast<std::size_t>(fail_count);
  ASSERT_EQ(events_.size(), count + 3u);
  expectEvent(events_[0], EventKind::Ack, StubState::Configured, StubState::RxSubscribed,
              "subscribeRx", RadioResult::Ok);
  expectEvent(events_[1], EventKind::FaultInjected, StubState::RxSubscribed,
              StubState::Configured, "subscribeRx", RadioResult::SubscribeFailed);
  for (std::size_t i = 2; i <= count; ++i) {
    expectEvent(events_[i], EventKind::FaultInjected, StubState::Configured,
                StubState::Configured, "subscribeRx", RadioResult::SubscribeFailed);
  }
  expectEvent(events_[count + 1], EventKind::Recovery, StubState::Configured,
              StubState::Configured, "subscribeRx", RadioResult::Ok);
  expectEvent(events_[count + 2], EventKind::Ack, StubState::Configured,
              StubState::RxSubscribed, "subscribeRx", RadioResult::Ok);
  expectSleeps(count);
}

// (f) All four plans smoke: the full scripted flow returns Ok end-to-end with
// the default fail_count of 1 — every injected fault recovers within budget.
TEST_F(ModemStubFsmTest, FullFlowReturnsOkUnderEveryPlan) {
  for (FaultPlan plan : {FaultPlan::None, FaultPlan::InitFail, FaultPlan::ConfigureReject,
                         FaultPlan::SubscriptionDrop}) {
    SCOPED_TRACE(static_cast<int>(plan));
    events_.clear();
    sleeps_.clear();
    ModemStub stub = makeStub(plan);  // default fault_fail_count = 1
    EXPECT_EQ(stub.init(), RadioResult::Ok);
    EXPECT_EQ(stub.configure(RadioConfig{kRxPort}), RadioResult::Ok);
    EXPECT_EQ(stub.subscribeRx(), RadioResult::Ok);
    EXPECT_EQ(stub.state(), StubState::RxSubscribed);
  }
}

// (g) Faults apply only to their own call: after init_fail recovers, configure
// and subscribeRx are clean single-attempt Acks — no injected events, no
// backoff waits leak into the other calls.
TEST_F(ModemStubFsmTest, FaultAppliesOnlyToItsOwnCall) {
  ModemStub stub = makeStub(FaultPlan::InitFail, /*fault_fail_count=*/1);
  EXPECT_EQ(stub.init(), RadioResult::Ok);  // FaultInjected → Recovery → Ack
  events_.clear();
  sleeps_.clear();

  EXPECT_EQ(stub.configure(RadioConfig{kRxPort}), RadioResult::Ok);
  EXPECT_EQ(stub.subscribeRx(), RadioResult::Ok);
  EXPECT_EQ(stub.state(), StubState::RxSubscribed);

  ASSERT_EQ(events_.size(), 2u);
  expectEvent(events_[0], EventKind::Ack, StubState::Initialized, StubState::Configured,
              "configure", RadioResult::Ok);
  expectEvent(events_[1], EventKind::Ack, StubState::Configured, StubState::RxSubscribed,
              "subscribeRx", RadioResult::Ok);
  expectSleeps(0);
}

// Illegal-order rejection always runs FIRST: an out-of-order call is a Reject
// regardless of the selected plan — the fault path is never entered, so the
// fail budget and sleeper stay untouched.
TEST_F(ModemStubFsmTest, IllegalOrderRejectionPrecedesFaultInjection) {
  ModemStub stub = makeStub(FaultPlan::ConfigureReject, /*fault_fail_count=*/1);
  expectRejection(stub, stub.configure(RadioConfig{kRxPort}), RadioResult::ConfigureRejected,
                  StubState::Idle, "configure");
  expectSleeps(0);

  // The fault budget is still intact: the legal configure hits it and recovers.
  ASSERT_EQ(stub.init(), RadioResult::Ok);
  events_.clear();
  EXPECT_EQ(stub.configure(RadioConfig{kRxPort}), RadioResult::Ok);
  ASSERT_EQ(events_.size(), 3u);
  EXPECT_EQ(events_[0].kind, EventKind::FaultInjected);
  EXPECT_EQ(events_[1].kind, EventKind::Recovery);
  EXPECT_EQ(events_[2].kind, EventKind::Ack);
  expectSleeps(1);
}

}  // namespace
