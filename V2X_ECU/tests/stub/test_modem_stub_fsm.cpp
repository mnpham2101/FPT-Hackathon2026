// R8 modem stub FSM test (subtask 8.1.3.2, Phase 1 HLD D2): the full scripted
// call flow acked in order, every illegal-order call rejected with its failure
// code and no state change, every outcome observed. Subtask 8.1.3.3 EXTENDS
// this file with the fault plans + recoveries — the fixture below (recording
// observer, plan-parameterized makeStub, bringUpTo helper) is the shared
// harness for those cases too.

#include "stub/modem_stub.hpp"

#include <chrono>
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
using v2x::stub::ModemStub;
using v2x::stub::RetryParams;
using v2x::stub::StubState;
using v2x::stub::toString;
using v2x::stub::TransitionEvent;

constexpr std::uint16_t kRxPort = 47100;

class ModemStubFsmTest : public ::testing::Test {
 protected:
  // Builds a stub wired to the recording observer. The fault plan is a
  // parameter (default None — the only path implemented in 8.1.3.2) so
  // 8.1.3.3's fault cases reuse this fixture unchanged. Relies on C++17
  // guaranteed elision: ModemStub is non-copyable.
  ModemStub makeStub(FaultPlan plan = FaultPlan::None) {
    return ModemStub(plan, retry_,
                     [this](const TransitionEvent& event) { events_.push_back(event); });
  }

  // Drives the stub to `target` via the happy path, asserting each ack, then
  // clears the recorded events so the test body sees only what it triggers.
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
  }

  void expectEvent(const TransitionEvent& event, StubState from, StubState to,
                   const std::string& call, RadioResult result) {
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
    expectEvent(events_.back(), expected_state, expected_state, call, expected_code);
    events_.clear();
  }

  RetryParams retry_{3, std::chrono::milliseconds{500}};  // HLD §6 proposal values; unused until 8.1.3.3
  std::vector<TransitionEvent> events_;
};

// (a) Happy path: every call acked Ok, states traverse the D2 order, observer
// saw exactly the three accepted transitions with matching fields.
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
  expectEvent(events_[0], StubState::Idle, StubState::Initialized, "init", RadioResult::Ok);
  expectEvent(events_[1], StubState::Initialized, StubState::Configured, "configure",
              RadioResult::Ok);
  expectEvent(events_[2], StubState::Configured, StubState::RxSubscribed, "subscribeRx",
              RadioResult::Ok);
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

// An empty observer is valid: outcomes are simply not reported.
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

}  // namespace
