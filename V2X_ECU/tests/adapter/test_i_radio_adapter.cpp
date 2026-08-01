// Freeze test for the R7 IRadioAdapter seam (subtask 7.1.3.1, Phase 1 HLD D2).
// A minimal fake implements the full interface and a scripted
// init → configure → subscribeRx sequence compiles and runs: this proves the
// seam is implementable and freezes the signatures — later subtasks may not
// alter the interface text without re-freezing across every consumer.

#include "adapter/i_radio_adapter.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace {

using v2x::adapter::IRadioAdapter;
using v2x::adapter::RadioConfig;
using v2x::adapter::RadioResult;
using v2x::adapter::RxCallback;
using v2x::adapter::toString;

// Minimal seam implementation: records the call order, stores the Rx
// callback, and keeps send() R10-deferred (NotSupported) — the seam-boundary
// behavior StubRadioAdapter (7.1.3.4) will exhibit.
class FakeRadioAdapter final : public IRadioAdapter {
 public:
  RadioResult init() override {
    calls.push_back("init");
    return RadioResult::Ok;
  }

  RadioResult configure(const RadioConfig& config) override {
    calls.push_back("configure");
    configured_rx_port = config.rx_port;
    return RadioResult::Ok;
  }

  RadioResult subscribeRx(RxCallback callback) override {
    calls.push_back("subscribeRx");
    rx_callback = std::move(callback);
    return RadioResult::Ok;
  }

  RadioResult send(const std::vector<std::uint8_t>&) override {
    calls.push_back("send");
    return RadioResult::NotSupported;  // R10 deferred — no M1 caller.
  }

  std::vector<std::string> calls;
  std::uint16_t configured_rx_port{0};
  RxCallback rx_callback;
};

TEST(IRadioAdapterSeam, ScriptedBringUpSequenceAcksInOrder) {
  FakeRadioAdapter fake;
  IRadioAdapter& adapter = fake;  // exercise the calls through the seam type

  std::vector<std::uint8_t> captured;
  const RadioConfig config{47100};

  EXPECT_EQ(adapter.init(), RadioResult::Ok);
  EXPECT_EQ(adapter.configure(config), RadioResult::Ok);
  EXPECT_EQ(adapter.subscribeRx([&captured](const std::vector<std::uint8_t>& bytes) {
              captured = bytes;
            }),
            RadioResult::Ok);

  const std::vector<std::string> expected{"init", "configure", "subscribeRx"};
  EXPECT_EQ(fake.calls, expected);
  EXPECT_EQ(fake.configured_rx_port, 47100u);
  EXPECT_TRUE(captured.empty());  // subscription alone delivers nothing
}

TEST(IRadioAdapterSeam, StoredRxCallbackDeliversBytesToSubscriber) {
  FakeRadioAdapter fake;
  std::vector<std::uint8_t> captured;

  ASSERT_EQ(fake.subscribeRx([&captured](const std::vector<std::uint8_t>& bytes) {
              captured = bytes;
            }),
            RadioResult::Ok);
  ASSERT_TRUE(fake.rx_callback);

  const std::vector<std::uint8_t> datagram{0x12, 0x03, 0xAB, 0x00, 0xFF};
  fake.rx_callback(datagram);

  EXPECT_EQ(captured, datagram);
}

TEST(IRadioAdapterSeam, SendIsR10DeferredNotSupported) {
  FakeRadioAdapter fake;
  IRadioAdapter& adapter = fake;

  EXPECT_EQ(adapter.send({0x01, 0x02}), RadioResult::NotSupported);
}

TEST(IRadioAdapterSeam, ToStringCoversEveryResultCode) {
  EXPECT_STREQ(toString(RadioResult::Ok), "Ok");
  EXPECT_STREQ(toString(RadioResult::InitFailed), "InitFailed");
  EXPECT_STREQ(toString(RadioResult::ConfigureRejected), "ConfigureRejected");
  EXPECT_STREQ(toString(RadioResult::SubscribeFailed), "SubscribeFailed");
  EXPECT_STREQ(toString(RadioResult::NotSupported), "NotSupported");
}

}  // namespace
