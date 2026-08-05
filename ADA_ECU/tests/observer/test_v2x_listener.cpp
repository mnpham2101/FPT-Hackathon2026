// Tests for the R2 ingress thread (2.2.6.1). Loopback only, ephemeral ports
// only, so parallel CI runs never collide on a fixed port. Synchronization is
// queue pops with timeouts, the listener's own join, and one bounded
// condition wait — no sleep stands in for an ordering guarantee.

#include "observer/v2x_listener.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

#include "net/udp_socket.hpp"
#include "observer/input_queue.hpp"

namespace {

using ada::net::UdpSocket;
using ada::observer::InputItem;
using ada::observer::InputQueue;
using ada::observer::Source;
using ada::observer::V2xListener;

constexpr std::chrono::milliseconds kGenerousTimeout{2000};
constexpr std::chrono::milliseconds kPollTimeout{50};

std::int64_t epochMsNow() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// Bounded observation of a cross-thread condition. The queue exposes no
// notification on its counters, so the wait observes the predicate until a
// deadline instead of assuming any timing; it returns false rather than hang.
template <typename Pred>
bool waitFor(Pred pred, std::chrono::milliseconds limit) {
  const auto deadline = std::chrono::steady_clock::now() + limit;
  while (!pred()) {
    if (std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    std::this_thread::yield();
  }
  return true;
}

// Arbitrary bytes with an embedded NUL and a high byte: the listener must
// hand the datagram body over untouched.
std::string binaryPayload() {
  std::string payload = "r2-body ";
  payload.push_back('\0');
  payload.push_back('\x01');
  payload.push_back(static_cast<char>(0xFF));
  payload += " tail";
  return payload;
}

TEST(V2xListener, LoopbackDatagramReachesQueueByteIdentical) {
  InputQueue queue(8);
  V2xListener listener("127.0.0.1", 0, queue, kPollTimeout);
  ASSERT_NE(listener.boundPort(), 0) << "ephemeral bind must resolve a real port";

  const std::string payload = binaryPayload();
  const std::int64_t before = epochMsNow();
  UdpSocket sender;
  ASSERT_TRUE(sender.sendTo("127.0.0.1", listener.boundPort(), payload));

  const auto item = queue.pop(kGenerousTimeout);
  const std::int64_t after = epochMsNow();
  ASSERT_TRUE(item.has_value());
  EXPECT_EQ(item->source, Source::V2xR2);
  EXPECT_EQ(item->line, payload);
  // rxEpochMs is CLOCK_REALTIME at receive: bracketed by the send and the pop.
  EXPECT_GE(item->rxEpochMs, before);
  EXPECT_LE(item->rxEpochMs, after);
  EXPECT_EQ(listener.receiveErrorCount(), 0u);
}

TEST(V2xListener, DatagramsArriveInSendOrder) {
  InputQueue queue(8);
  V2xListener listener("127.0.0.1", 0, queue, kPollTimeout);

  UdpSocket sender;
  ASSERT_TRUE(sender.sendTo("127.0.0.1", listener.boundPort(), "first"));
  ASSERT_TRUE(sender.sendTo("127.0.0.1", listener.boundPort(), "second"));

  const auto a = queue.pop(kGenerousTimeout);
  const auto b = queue.pop(kGenerousTimeout);
  ASSERT_TRUE(a.has_value());
  ASSERT_TRUE(b.has_value());
  EXPECT_EQ(a->line, "first");
  EXPECT_EQ(b->line, "second");
}

TEST(V2xListener, StopIsIdempotentAndDestructionJoinsWithoutHang) {
  InputQueue queue(4);
  const auto start = std::chrono::steady_clock::now();
  {
    V2xListener listener("127.0.0.1", 0, queue, kPollTimeout);
    listener.stop();
    listener.stop();  // idempotent: a second call must be harmless
  }                   // destructor joins the receive thread
  const auto elapsed = std::chrono::steady_clock::now() - start;
  // Prompt: exit within one poll timeout, with generous CI headroom.
  EXPECT_LT(elapsed, kGenerousTimeout);
}

TEST(V2xListener, DestructionAloneStopsAndJoins) {
  InputQueue queue(4);
  const auto start = std::chrono::steady_clock::now();
  {
    V2xListener listener("127.0.0.1", 0, queue, kPollTimeout);
  }
  const auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_LT(elapsed, kGenerousTimeout);
}

TEST(V2xListener, QueueFullDropIsCountedOnTheQueue) {
  InputQueue queue(1);
  // Fill the capacity-1 queue before the listener exists, so the listener's
  // push must evict the filler and count exactly one drop.
  ASSERT_TRUE(queue.push(InputItem{Source::DetectorR3, "filler", 0}));
  V2xListener listener("127.0.0.1", 0, queue, kPollTimeout);

  UdpSocket sender;
  ASSERT_TRUE(sender.sendTo("127.0.0.1", listener.boundPort(), "survivor"));

  // The drop counter reaching 1 is the observable that the push happened;
  // popping earlier would empty the queue and no drop could occur.
  ASSERT_TRUE(waitFor([&] { return queue.droppedCount() >= 1; }, kGenerousTimeout));
  EXPECT_EQ(queue.droppedCount(), 1u);

  const auto item = queue.pop(kGenerousTimeout);
  ASSERT_TRUE(item.has_value());
  EXPECT_EQ(item->source, Source::V2xR2);
  EXPECT_EQ(item->line, "survivor");
}

}  // namespace
