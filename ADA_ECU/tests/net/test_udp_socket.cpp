// Tests for the sole transport holder (6.2.2.2). Loopback only, ephemeral
// ports only, so parallel CI runs never collide on a fixed port.

#include "net/udp_socket.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using ada::net::UdpSocket;

constexpr std::chrono::milliseconds kGenerousTimeout{2000};

TEST(UdpSocket, LoopbackRoundTripOnEphemeralPort) {
  UdpSocket receiver;
  receiver.bind("127.0.0.1", 0);
  ASSERT_NE(receiver.boundPort(), 0) << "ephemeral bind must resolve a real port";

  UdpSocket sender;
  const std::string payload = "{\"type\":\"v2x_object\"}";
  ASSERT_TRUE(sender.sendTo("127.0.0.1", receiver.boundPort(), payload));

  const auto received = receiver.recvFrom(kGenerousTimeout);
  ASSERT_TRUE(received.has_value());
  EXPECT_EQ(*received, payload);
  EXPECT_EQ(receiver.transientErrorCount(), 0u);
  EXPECT_EQ(sender.transientErrorCount(), 0u);
}

TEST(UdpSocket, EachDatagramArrivesWhole) {
  UdpSocket receiver;
  receiver.bind("127.0.0.1", 0);
  UdpSocket sender;
  ASSERT_TRUE(sender.sendTo("127.0.0.1", receiver.boundPort(), "first"));
  ASSERT_TRUE(sender.sendTo("127.0.0.1", receiver.boundPort(), "second"));

  const auto a = receiver.recvFrom(kGenerousTimeout);
  const auto b = receiver.recvFrom(kGenerousTimeout);
  ASSERT_TRUE(a.has_value());
  ASSERT_TRUE(b.has_value());
  EXPECT_EQ(*a, "first");
  EXPECT_EQ(*b, "second");
}

TEST(UdpSocket, BindConflictSurfacesCleanly) {
  UdpSocket first;
  first.bind("127.0.0.1", 0);
  ASSERT_NE(first.boundPort(), 0);

  UdpSocket second;
  EXPECT_THROW(second.bind("127.0.0.1", first.boundPort()), std::runtime_error);
}

TEST(UdpSocket, MalformedAddressThrows) {
  UdpSocket sock;
  EXPECT_THROW(sock.bind("not-an-address", 0), std::invalid_argument);
  EXPECT_THROW(sock.sendTo("not-an-address", 1234, "x"), std::invalid_argument);
}

TEST(UdpSocket, ReceiveTimeoutReturnsEmptyNotBlocking) {
  UdpSocket receiver;
  receiver.bind("127.0.0.1", 0);

  const auto start = std::chrono::steady_clock::now();
  const auto received = receiver.recvFrom(std::chrono::milliseconds(50));
  const auto elapsed = std::chrono::steady_clock::now() - start;

  EXPECT_FALSE(received.has_value());
  // A timeout is not a transient error.
  EXPECT_EQ(receiver.transientErrorCount(), 0u);
  // Bounded: the poll timeout returned rather than blocking forever.
  EXPECT_LT(elapsed, kGenerousTimeout);
}

TEST(UdpSocket, MoveTransfersOwnership) {
  UdpSocket original;
  original.bind("127.0.0.1", 0);
  const std::uint16_t port = original.boundPort();

  UdpSocket moved = std::move(original);
  EXPECT_EQ(moved.boundPort(), port);

  UdpSocket sender;
  ASSERT_TRUE(sender.sendTo("127.0.0.1", port, "after-move"));
  const auto received = moved.recvFrom(kGenerousTimeout);
  ASSERT_TRUE(received.has_value());
  EXPECT_EQ(*received, "after-move");
}

}  // namespace
