// Tests for the 7.1.3.4 R7 seam implementation
// (src/adapter/stub_radio_adapter.{hpp,cpp}, HLD decision D2):
// (a) happy path — init/configure/subscribeRx all Ok and a loopback datagram
//     sent to the configured port reaches the subscribed callback with
//     byte-identical payload, (b) send() returns NotSupported and logs the R10
//     deferral, (c) stop()/destruction is prompt and idempotent — no hang, no
//     leaked thread, (d) failure passthrough — a stub whose init fault exhausts
//     its retry budget yields InitFailed and no Rx thread or socket is ever
//     created.
//
// CI-safe by construction: loopback 127.0.0.1 only, and the Rx port is learned
// by binding a probe socket to port 0 and reading localPort() before releasing
// it — the stub's stored RadioConfig carries a concrete port, so the adapter
// cannot bind 0 itself. The tiny window between releasing the probe and the
// adapter binding the same port is accepted for a test: the kernel does not
// hand the same ephemeral port to another process that quickly, and no other
// test in this suite binds a fixed port.
//
// Every wait is bounded (condition_variable with a timeout) so a lost datagram
// or a stuck thread fails the test instead of hanging the run. No socket
// headers here — the test consumes net::UdpSocket like any other caller.

#include "adapter/stub_radio_adapter.hpp"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "adapter/i_radio_adapter.hpp"
#include "config/config.hpp"
#include "net/udp_socket.hpp"
#include "stub/modem_stub.hpp"

namespace {

using v2x::adapter::RadioConfig;
using v2x::adapter::RadioResult;
using v2x::adapter::StubRadioAdapter;
using v2x::config::FaultPlan;
using v2x::net::UdpSocket;
using v2x::stub::EventKind;
using v2x::stub::ModemStub;
using v2x::stub::RetryParams;
using v2x::stub::StubState;
using v2x::stub::TransitionEvent;

// Generous ceiling for "should already have arrived": loopback UDP plus a
// thread hand-off never legitimately takes this long.
constexpr std::chrono::milliseconds kDeliveryTimeout{3000};

// Shutdown ceiling: stop() waits at most one Rx poll period for the thread to
// notice the flag, so several times kRxPollTimeout is a safe upper bound.
constexpr std::chrono::milliseconds kShutdownCeiling{3000};

// A stop() that has no thread to join is a store plus a joinable() check —
// orders of magnitude below this, even on a loaded runner. Used as the "no Rx
// thread was ever started" evidence in the failure-passthrough case.
constexpr std::chrono::milliseconds kNoThreadJoinCeiling{50};

const std::vector<std::uint8_t> kPayload{0x01, 0xff, 0x00, 0x7f, 0x80, 0x42, 0x00, 0x13};

// Retry knobs for the stub: a zero ceiling plus a no-op sleeper keeps the
// fault cases instant and deterministic (backoff is the stub's business,
// covered by v2x_modem_stub_fsm_test).
const RetryParams kRetry{0, std::chrono::milliseconds(1)};

void noopSleeper(std::chrono::milliseconds) {}

// Thread-safe recorder for the Rx callback: the Rx thread writes, the test
// thread waits with a bounded condition_variable wait.
class PayloadRecorder {
 public:
  void record(const std::vector<std::uint8_t>& bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    payloads_.push_back(bytes);
    cv_.notify_all();
  }

  bool waitForAtLeast(std::size_t count, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    return cv_.wait_for(lock, timeout, [&] { return payloads_.size() >= count; });
  }

  std::vector<std::vector<std::uint8_t>> snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return payloads_;
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::vector<std::vector<std::uint8_t>> payloads_;
};

// Thread-safe log capture — the Rx thread may log too.
class LogRecorder {
 public:
  void record(const std::string& line) {
    std::lock_guard<std::mutex> lock(mutex_);
    lines_.push_back(line);
  }

  std::vector<std::string> lines() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lines_;
  }

  bool anyContains(const std::string& needle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const std::string& line : lines_) {
      if (line.find(needle) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

 private:
  mutable std::mutex mutex_;
  std::vector<std::string> lines_;
};

// Learns a free UDP port from the kernel: bind a probe to 0, read the assigned
// port, release it on scope exit (see the file header on the accepted race).
std::uint16_t reserveFreePort() {
  UdpSocket probe = UdpSocket::boundTo(0);
  return probe.localPort();
}

// Fixture note on destruction order: every test declares its recorders BEFORE
// the adapter, so the adapter (whose destructor joins the Rx thread that calls
// into them) is destroyed first — recorders always outlive the thread.
class StubRadioAdapterTest : public ::testing::Test {
 protected:
  ModemStub makeStub(FaultPlan plan = FaultPlan::None, int fault_fail_count = 1) {
    return ModemStub(
        plan, kRetry, [this](const TransitionEvent& event) { events_.push_back(event); },
        noopSleeper, fault_fail_count);
  }

  bool sawEvent(EventKind kind, const std::string& call) const {
    for (const TransitionEvent& event : events_) {
      if (event.kind == kind && event.call == call) {
        return true;
      }
    }
    return false;
  }

  // Written only from the test thread: the stub is driven exclusively from
  // here (the Rx thread never calls into it).
  std::vector<TransitionEvent> events_;
};

// --- (a) happy path: loopback datagram reaches the subscribed callback ---

TEST_F(StubRadioAdapterTest, LoopbackDatagramReachesCallbackWithIdenticalBytes) {
  PayloadRecorder recorder;
  LogRecorder log;
  ModemStub stub = makeStub();
  StubRadioAdapter adapter(stub, [&log](const std::string& line) { log.record(line); });

  ASSERT_EQ(adapter.init(), RadioResult::Ok);
  const std::uint16_t rx_port = reserveFreePort();
  ASSERT_NE(rx_port, 0);
  ASSERT_EQ(adapter.configure(RadioConfig{rx_port}), RadioResult::Ok);
  ASSERT_EQ(stub.state(), StubState::Configured);
  ASSERT_EQ(adapter.subscribeRx([&recorder](const std::vector<std::uint8_t>& bytes) {
              recorder.record(bytes);
            }),
            RadioResult::Ok);
  EXPECT_EQ(stub.state(), StubState::RxSubscribed);

  UdpSocket sender;
  ASSERT_EQ(sender.sendTo("127.0.0.1", rx_port, kPayload), kPayload.size());

  ASSERT_TRUE(recorder.waitForAtLeast(1, kDeliveryTimeout))
      << "the Rx thread must deliver the loopback datagram to the callback";
  const std::vector<std::vector<std::uint8_t>> received = recorder.snapshot();
  ASSERT_EQ(received.size(), 1u);
  EXPECT_EQ(received.front(), kPayload) << "callback must see the exact datagram bytes";
}

TEST_F(StubRadioAdapterTest, MultipleDatagramsAreDeliveredInOrder) {
  PayloadRecorder recorder;
  ModemStub stub = makeStub();
  StubRadioAdapter adapter(stub, [](const std::string&) {});

  ASSERT_EQ(adapter.init(), RadioResult::Ok);
  const std::uint16_t rx_port = reserveFreePort();
  ASSERT_EQ(adapter.configure(RadioConfig{rx_port}), RadioResult::Ok);
  ASSERT_EQ(adapter.subscribeRx([&recorder](const std::vector<std::uint8_t>& bytes) {
              recorder.record(bytes);
            }),
            RadioResult::Ok);

  UdpSocket sender;
  const std::vector<std::uint8_t> second{0xaa, 0xbb};
  ASSERT_EQ(sender.sendTo("127.0.0.1", rx_port, kPayload), kPayload.size());
  ASSERT_EQ(sender.sendTo("127.0.0.1", rx_port, second), second.size());

  ASSERT_TRUE(recorder.waitForAtLeast(2, kDeliveryTimeout));
  const std::vector<std::vector<std::uint8_t>> received = recorder.snapshot();
  ASSERT_EQ(received.size(), 2u);
  EXPECT_EQ(received[0], kPayload);
  EXPECT_EQ(received[1], second);
}

// A repeat subscribeRx is the stub's illegal-order rejection; the already
// running Rx thread must survive it (no second thread, delivery unaffected).
TEST_F(StubRadioAdapterTest, RepeatSubscribeIsRejectedAndKeepsTheRunningRxThread) {
  PayloadRecorder recorder;
  ModemStub stub = makeStub();
  StubRadioAdapter adapter(stub, [](const std::string&) {});

  ASSERT_EQ(adapter.init(), RadioResult::Ok);
  const std::uint16_t rx_port = reserveFreePort();
  ASSERT_EQ(adapter.configure(RadioConfig{rx_port}), RadioResult::Ok);
  ASSERT_EQ(adapter.subscribeRx([&recorder](const std::vector<std::uint8_t>& bytes) {
              recorder.record(bytes);
            }),
            RadioResult::Ok);

  // Second call: the stub rejects it (already RxSubscribed) — returned verbatim.
  EXPECT_EQ(adapter.subscribeRx([](const std::vector<std::uint8_t>&) {}),
            RadioResult::SubscribeFailed);

  UdpSocket sender;
  ASSERT_EQ(sender.sendTo("127.0.0.1", rx_port, kPayload), kPayload.size());
  ASSERT_TRUE(recorder.waitForAtLeast(1, kDeliveryTimeout))
      << "the original callback must still be receiving";
  EXPECT_EQ(recorder.snapshot().front(), kPayload);
}

// A throwing consumer must not kill the Rx thread (the next datagram still
// arrives).
TEST_F(StubRadioAdapterTest, ThrowingCallbackIsLoggedAndRxThreadSurvives) {
  PayloadRecorder recorder;
  LogRecorder log;
  ModemStub stub = makeStub();
  StubRadioAdapter adapter(stub, [&log](const std::string& line) { log.record(line); });

  ASSERT_EQ(adapter.init(), RadioResult::Ok);
  const std::uint16_t rx_port = reserveFreePort();
  ASSERT_EQ(adapter.configure(RadioConfig{rx_port}), RadioResult::Ok);
  ASSERT_EQ(adapter.subscribeRx([&recorder](const std::vector<std::uint8_t>& bytes) {
              recorder.record(bytes);
              throw std::runtime_error("consumer blew up");
            }),
            RadioResult::Ok);

  UdpSocket sender;
  ASSERT_EQ(sender.sendTo("127.0.0.1", rx_port, kPayload), kPayload.size());
  ASSERT_TRUE(recorder.waitForAtLeast(1, kDeliveryTimeout));
  ASSERT_EQ(sender.sendTo("127.0.0.1", rx_port, kPayload), kPayload.size());
  EXPECT_TRUE(recorder.waitForAtLeast(2, kDeliveryTimeout))
      << "a throwing callback must not end reception";
}

// --- (b) send() is the R10-deferred no-op ---

TEST_F(StubRadioAdapterTest, SendReturnsNotSupportedAndLogsTheR10Deferral) {
  LogRecorder log;
  ModemStub stub = makeStub();
  StubRadioAdapter adapter(stub, [&log](const std::string& line) { log.record(line); });

  EXPECT_EQ(adapter.send(kPayload), RadioResult::NotSupported);
  EXPECT_EQ(log.lines().size(), 1u) << "exactly one log line per send attempt";
  EXPECT_TRUE(log.anyContains("NotSupported"));
  EXPECT_TRUE(log.anyContains("R10"));
}

TEST_F(StubRadioAdapterTest, SendStaysNotSupportedWhileSubscribed) {
  PayloadRecorder recorder;
  ModemStub stub = makeStub();
  StubRadioAdapter adapter(stub, [](const std::string&) {});

  ASSERT_EQ(adapter.init(), RadioResult::Ok);
  ASSERT_EQ(adapter.configure(RadioConfig{reserveFreePort()}), RadioResult::Ok);
  ASSERT_EQ(adapter.subscribeRx([&recorder](const std::vector<std::uint8_t>& bytes) {
              recorder.record(bytes);
            }),
            RadioResult::Ok);

  EXPECT_EQ(adapter.send(kPayload), RadioResult::NotSupported);
}

// --- (c) clean shutdown: prompt, idempotent, no hang ---

TEST_F(StubRadioAdapterTest, StopJoinsPromptlyAndIsIdempotent) {
  PayloadRecorder recorder;
  ModemStub stub = makeStub();
  StubRadioAdapter adapter(stub, [](const std::string&) {});

  ASSERT_EQ(adapter.init(), RadioResult::Ok);
  ASSERT_EQ(adapter.configure(RadioConfig{reserveFreePort()}), RadioResult::Ok);
  ASSERT_EQ(adapter.subscribeRx([&recorder](const std::vector<std::uint8_t>& bytes) {
              recorder.record(bytes);
            }),
            RadioResult::Ok);

  const auto start = std::chrono::steady_clock::now();
  adapter.stop();
  const auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_LT(elapsed, kShutdownCeiling) << "stop() must not hang on the Rx thread";

  // Second call: nothing left to join, nothing throws.
  const auto second_start = std::chrono::steady_clock::now();
  EXPECT_NO_THROW(adapter.stop());
  EXPECT_LT(std::chrono::steady_clock::now() - second_start, kNoThreadJoinCeiling)
      << "a repeat stop() is a no-op";
}

TEST_F(StubRadioAdapterTest, DestructorShutsDownWithoutHanging) {
  PayloadRecorder recorder;
  ModemStub stub = makeStub();
  const std::uint16_t rx_port = reserveFreePort();
  const auto start = std::chrono::steady_clock::now();
  {
    StubRadioAdapter adapter(stub, [](const std::string&) {});
    ASSERT_EQ(adapter.init(), RadioResult::Ok);
    ASSERT_EQ(adapter.configure(RadioConfig{rx_port}), RadioResult::Ok);
    ASSERT_EQ(adapter.subscribeRx([&recorder](const std::vector<std::uint8_t>& bytes) {
                recorder.record(bytes);
              }),
              RadioResult::Ok);
    UdpSocket sender;
    ASSERT_EQ(sender.sendTo("127.0.0.1", rx_port, kPayload), kPayload.size());
    ASSERT_TRUE(recorder.waitForAtLeast(1, kDeliveryTimeout));
  }  // destructor: stop() → join → socket released
  EXPECT_LT(std::chrono::steady_clock::now() - start, kDeliveryTimeout + kShutdownCeiling);

  // The Rx port is bindable again right after destruction — the socket was
  // released with the thread, not leaked into the process.
  EXPECT_NO_THROW((void)UdpSocket::boundTo(rx_port));
}

// --- (d) failure passthrough: no thread, no socket when bring-up fails ---

TEST_F(StubRadioAdapterTest, InitFailureIsPassedThroughAndNoRxThreadStarts) {
  PayloadRecorder recorder;
  LogRecorder log;
  // Fail budget (5) far exceeds the retry ceiling (0) → the stub's bounded
  // retry loop exhausts and init() is the terminal InitFailed.
  ModemStub stub = makeStub(FaultPlan::InitFail, 5);
  StubRadioAdapter adapter(stub, [&log](const std::string& line) { log.record(line); });

  EXPECT_EQ(adapter.init(), RadioResult::InitFailed);
  EXPECT_EQ(stub.state(), StubState::Idle);
  EXPECT_TRUE(sawEvent(EventKind::FaultInjected, "init"));
  EXPECT_FALSE(sawEvent(EventKind::Ack, "init"));

  // Out-of-order continuation is the stub's rejection, returned verbatim; the
  // adapter must not open a socket or start a thread on a rejected subscribe.
  EXPECT_EQ(adapter.configure(RadioConfig{reserveFreePort()}), RadioResult::ConfigureRejected);
  EXPECT_EQ(adapter.subscribeRx([&recorder](const std::vector<std::uint8_t>& bytes) {
              recorder.record(bytes);
            }),
            RadioResult::SubscribeFailed);
  EXPECT_FALSE(sawEvent(EventKind::Ack, "subscribeRx"));

  // Evidence that no Rx thread exists: stop() has nothing to join, so it
  // returns far faster than the one-poll-period wait a live thread would cost.
  const auto start = std::chrono::steady_clock::now();
  adapter.stop();
  EXPECT_LT(std::chrono::steady_clock::now() - start, kNoThreadJoinCeiling)
      << "no Rx thread should have been started";
  EXPECT_TRUE(recorder.snapshot().empty());
}

TEST_F(StubRadioAdapterTest, ConfigureRejectionIsPassedThroughVerbatim) {
  ModemStub stub = makeStub(FaultPlan::ConfigureReject, 5);
  StubRadioAdapter adapter(stub, [](const std::string&) {});

  ASSERT_EQ(adapter.init(), RadioResult::Ok);
  EXPECT_EQ(adapter.configure(RadioConfig{reserveFreePort()}), RadioResult::ConfigureRejected);
  EXPECT_EQ(stub.state(), StubState::Initialized);
  // No stored config → the adapter never had a port to bind.
  EXPECT_EQ(stub.config().rx_port, 0);
}

}  // namespace
