// Tests for the 2.1.2.4 ADA forwarder (src/forward/ada_forwarder.{hpp,cpp}):
// (a) a loopback listener on an ephemeral port receives the JSON of the
// node-local sample tests/fixtures/samples/r2-object.json intact — compared
// as parsed nlohmann documents (semantic equality), never as raw strings,
// since key order and whitespace are not part of the contract; (b) a
// guaranteed-fail target ("not-an-ip" — sendTo is numeric-IPv4-only, so
// inet_pton fails → SocketError) surfaces as `false` from send(), never as
// an exception into the caller.
//
// CI-safe by construction: loopback 127.0.0.1 only, ephemeral port via
// bind(0) + localPort(), and the listener arms a safety timeout so a lost
// datagram fails the test instead of hanging the run.

#include "forward/ada_forwarder.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "contracts/r2_message.hpp"
#include "net/udp_socket.hpp"

namespace {

using v2x::contracts::R2Message;
using v2x::forward::AdaForwarder;
using v2x::net::RecvOutcome;
using v2x::net::RecvResult;
using v2x::net::UdpSocket;

// Generous ceiling for "should already have arrived" — loopback UDP never
// legitimately takes this long.
constexpr std::chrono::milliseconds kSafetyTimeout{2000};

nlohmann::json LoadSample() {
  const std::string path = std::string(V2X_FIXTURE_DIR) + "/samples/r2-object.json";
  std::ifstream in(path);
  EXPECT_TRUE(in.is_open()) << "cannot open fixture: " << path;
  return nlohmann::json::parse(in);
}

// --- (a) loopback delivery of the sample, intact ---

TEST(AdaForwarder, LoopbackListenerReceivesSampleJsonIntact) {
  UdpSocket listener = UdpSocket::boundTo(0);
  listener.setRecvTimeout(kSafetyTimeout);
  ASSERT_NE(listener.localPort(), 0);

  const nlohmann::json sample = LoadSample();
  const auto message = sample.get<R2Message>();

  AdaForwarder forwarder("127.0.0.1", listener.localPort());
  EXPECT_TRUE(forwarder.send(message));

  std::vector<std::uint8_t> buffer;
  const RecvResult result = listener.recvFrom(buffer);
  ASSERT_EQ(result.outcome, RecvOutcome::Data) << "expected one datagram, got timeout";
  ASSERT_EQ(result.bytes, buffer.size());
  ASSERT_GT(result.bytes, 0u);

  // Semantic wire equality: the received datagram parses back to exactly the
  // frozen sample document.
  const auto received = nlohmann::json::parse(std::string(buffer.begin(), buffer.end()));
  EXPECT_EQ(received, sample);
}

// --- (b) send failure is `false`, never an exception ---

TEST(AdaForwarder, SendReturnsFalseAndDoesNotThrowOnInvalidHost) {
  const auto message = LoadSample().get<R2Message>();

  // sendTo resolves numeric IPv4 only (no DNS): inet_pton rejects this host,
  // the resulting SocketError is caught inside send() and surfaced as false.
  AdaForwarder forwarder("not-an-ip", 1);
  bool ok = true;
  EXPECT_NO_THROW(ok = forwarder.send(message));
  EXPECT_FALSE(ok);
}

}  // namespace
