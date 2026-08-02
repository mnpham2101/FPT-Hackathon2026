// Tests for the 7.1.2.2 sole socket holder (src/net/udp_socket.{hpp,cpp}):
// (a) loopback send→receive round-trip on an ephemeral port, (b) bind
// conflict surfaces as SocketError mentioning bind, (c) SO_RCVTIMEO yields
// the distinct Timeout outcome (not an exception) and returns promptly,
// (d) move-only ownership — moved-from destructs harmlessly, moved-to still
// receives. Plus the typed-error surface of the send path (non-numeric host).
//
// CI-safe by construction: loopback 127.0.0.1 only, ephemeral ports only
// (bind(0) + localPort()), and every receiver arms a safety timeout so a
// lost datagram fails the test instead of hanging the run. No socket headers
// here — this file consumes the seam like any other caller would.

#include "net/udp_socket.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace {

using v2x::net::RecvOutcome;
using v2x::net::RecvResult;
using v2x::net::SocketError;
using v2x::net::UdpSocket;

// Generous ceiling for "should already have arrived / should return promptly"
// — loopback UDP never legitimately takes this long.
constexpr std::chrono::milliseconds kSafetyTimeout{2000};

const std::vector<std::uint8_t> kPayload{0xde, 0xad, 0xbe, 0xef, 0x00, 0x42};

// --- (a) loopback round-trip on an ephemeral port ---

TEST(UdpSocket, BindEphemeralYieldsNonZeroLocalPort) {
  UdpSocket socket;
  socket.bind(0);
  EXPECT_NE(socket.localPort(), 0);
}

TEST(UdpSocket, LoopbackRoundTripDeliversIdenticalBytesAndSender) {
  UdpSocket receiver = UdpSocket::boundTo(0);
  receiver.setRecvTimeout(kSafetyTimeout);
  const std::uint16_t port = receiver.localPort();
  ASSERT_NE(port, 0);

  UdpSocket sender;
  EXPECT_EQ(sender.sendTo("127.0.0.1", port, kPayload), kPayload.size());

  std::vector<std::uint8_t> buffer;
  const RecvResult result = receiver.recvFrom(buffer);
  ASSERT_EQ(result.outcome, RecvOutcome::Data);
  EXPECT_EQ(result.bytes, kPayload.size());
  EXPECT_EQ(buffer, kPayload);
  EXPECT_EQ(result.senderIp, "127.0.0.1");
  EXPECT_NE(result.senderPort, 0);
}

TEST(UdpSocket, PointerLengthSendOverloadRoundTrips) {
  UdpSocket receiver = UdpSocket::boundTo(0);
  receiver.setRecvTimeout(kSafetyTimeout);

  UdpSocket sender;
  EXPECT_EQ(sender.sendTo("127.0.0.1", receiver.localPort(), kPayload.data(), kPayload.size()),
            kPayload.size());

  std::vector<std::uint8_t> buffer;
  const RecvResult result = receiver.recvFrom(buffer);
  ASSERT_EQ(result.outcome, RecvOutcome::Data);
  EXPECT_EQ(buffer, kPayload);
}

// --- (b) bind conflict surfaces cleanly ---

TEST(UdpSocket, SecondBindOnSamePortThrowsSocketErrorMentioningBind) {
  UdpSocket first = UdpSocket::boundTo(0);  // ephemeral, then reuse the concrete port
  const std::uint16_t taken = first.localPort();
  ASSERT_NE(taken, 0);

  UdpSocket second;
  try {
    second.bind(taken);
    FAIL() << "expected SocketError on bind conflict";
  } catch (const SocketError& error) {
    EXPECT_NE(std::string(error.what()).find("bind"), std::string::npos)
        << "message should mention bind, got: " << error.what();
  }
}

// --- (c) receive timeout is a distinct outcome, not an error ---

TEST(UdpSocket, RecvTimeoutYieldsTimeoutOutcomePromptly) {
  UdpSocket socket = UdpSocket::boundTo(0);  // nobody sends to it
  socket.setRecvTimeout(std::chrono::milliseconds(50));

  std::vector<std::uint8_t> buffer;
  RecvResult result{};
  const auto start = std::chrono::steady_clock::now();
  EXPECT_NO_THROW(result = socket.recvFrom(buffer));
  const auto elapsed = std::chrono::steady_clock::now() - start;

  EXPECT_EQ(result.outcome, RecvOutcome::Timeout);
  EXPECT_EQ(result.bytes, 0u);
  EXPECT_TRUE(buffer.empty());
  EXPECT_LT(elapsed, kSafetyTimeout) << "50 ms timeout must return promptly";
}

// --- (d) move semantics ---

TEST(UdpSocket, MoveConstructionTransfersOwnershipAndStillReceives) {
  UdpSocket original = UdpSocket::boundTo(0);
  const std::uint16_t port = original.localPort();

  UdpSocket moved_to = std::move(original);
  moved_to.setRecvTimeout(kSafetyTimeout);
  EXPECT_EQ(moved_to.localPort(), port);

  UdpSocket sender;
  sender.sendTo("127.0.0.1", port, kPayload);
  std::vector<std::uint8_t> buffer;
  const RecvResult result = moved_to.recvFrom(buffer);
  ASSERT_EQ(result.outcome, RecvOutcome::Data);
  EXPECT_EQ(buffer, kPayload);
}  // `original` (moved-from) destructs here alongside moved_to — no double close

TEST(UdpSocket, MovedFromSocketDestructsHarmlesslyAndRejectsIo) {
  {
    UdpSocket a = UdpSocket::boundTo(0);
    UdpSocket b = std::move(a);
    // I/O on the moved-from socket is the typed error, never fd -1 UB.
    std::vector<std::uint8_t> buffer;
    EXPECT_THROW((void)a.localPort(), SocketError);
    EXPECT_THROW((void)a.recvFrom(buffer), SocketError);
    EXPECT_THROW(a.bind(0), SocketError);
  }  // both destruct: b closes the one fd, a holds nothing
  SUCCEED();
}

TEST(UdpSocket, MoveAssignmentReplacesTheOldFdAndStillReceives) {
  UdpSocket receiver = UdpSocket::boundTo(0);
  receiver.setRecvTimeout(kSafetyTimeout);
  const std::uint16_t port = receiver.localPort();

  UdpSocket other;  // owns a different fd, released by the assignment
  other = std::move(receiver);
  EXPECT_EQ(other.localPort(), port);

  UdpSocket sender;
  sender.sendTo("127.0.0.1", port, kPayload);
  std::vector<std::uint8_t> buffer;
  const RecvResult result = other.recvFrom(buffer);
  ASSERT_EQ(result.outcome, RecvOutcome::Data);
  EXPECT_EQ(buffer, kPayload);
}

// --- typed error surface of the send path ---

TEST(UdpSocket, SendToNonNumericHostThrowsSocketError) {
  UdpSocket sender;
  // No DNS by design (numeric IPv4 only) — a hostname is a typed error.
  EXPECT_THROW(sender.sendTo("not-an-ip", 1, kPayload), SocketError);
}

}  // namespace
