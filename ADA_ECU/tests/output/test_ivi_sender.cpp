// Tests for the R4 IVI sender (subtask 15.4.2.2): loopback delivery of the
// exact frozen-binding JSON, the r4_tx line's embedded body parsing back to
// an equal event, and the counted never-thrown failure path.
//
// The failure path exercised here is the socket's malformed-address throw —
// the one deterministic local failure: a UDP send to an unreachable but
// well-formed target succeeds at the syscall level, so it cannot assert the
// counting. Transient-errno counting itself is asserted at udp_socket's own
// unit level (tests/net/test_udp_socket.cpp); this seam asserts that ANY
// sendTo failure — returned false or thrown — surfaces as a counted, logged
// send_ok=false and never as an exception.

#include "output/ivi_sender.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "contracts/r4_message.hpp"
#include "contracts/tracked_object.hpp"
#include "cra/i_collision_risk_assessment.hpp"
#include "fusion/scene_composer.hpp"
#include "log/event_log.hpp"
#include "net/udp_socket.hpp"
#include "output/warning_builder.hpp"
#include "store/track_store.hpp"

namespace {

using ada::contracts::R4WarningEvent;
using ada::contracts::Source;
using ada::contracts::TrackedObject;
using ada::contracts::TrackState;
using ada::cra::RiskFinding;
using ada::log::EventLog;
using ada::net::UdpSocket;
using ada::output::buildWarning;
using ada::output::IviSender;

constexpr std::chrono::milliseconds kGenerousTimeout{2000};

EventLog::Clocks fixedClocks(std::int64_t monoMs, std::int64_t epochMs) {
  EventLog::Clocks clocks;
  clocks.monoMs = [monoMs]() -> std::int64_t { return monoMs; };
  clocks.epochMs = [epochMs]() -> std::int64_t { return epochMs; };
  return clocks;
}

std::vector<std::string> lines(const std::string& text) {
  std::vector<std::string> out;
  std::istringstream stream(text);
  std::string line;
  while (std::getline(stream, line)) {
    out.push_back(line);
  }
  return out;
}

// Asserts the [EVT] prefix and parses the JSON that follows it.
nlohmann::json parseEvtLine(const std::string& line) {
  const std::string prefix = "[EVT] ";
  EXPECT_EQ(line.rfind(prefix, 0), 0u) << "line lacks the [EVT] prefix: " << line;
  return nlohmann::json::parse(line.substr(prefix.size()));
}

TrackedObject MakeTrack(const std::string& id, Source source, double distance, double y) {
  TrackedObject o;
  o.id = id;
  o.object_class = "vehicle";
  o.source = source;
  o.position = {distance, y};
  o.distance = distance;
  o.speed = 12.5;
  o.confidence = 0.9;
  o.state = TrackState::tracked;
  o.timestamps = {1789000000000, 1789000000050, 1789000000060};
  return o;
}

// Built through the real builder, so this test sends exactly what the node
// sends on a committed transition.
R4WarningEvent MakeWarningEvent() {
  ada::store::TrackStore store;
  store.upsert(MakeTrack("own:1", Source::own_sensor, 20.0, 0.5));
  const TrackedObject trackedC = MakeTrack("v2x:1001:7", Source::v2x_relayed, 25.0, -0.3);
  const auto geometry = ada::fusion::compose(store, trackedC, std::nullopt);
  EXPECT_TRUE(geometry.has_value());
  const RiskFinding finding{"nlos_obstruction", "high",
                            std::optional<TrackedObject>(trackedC), "range"};
  return buildWarning(finding, *geometry);
}

TEST(IviSender, LoopbackReceiverGetsTheExactBindingJson) {
  UdpSocket receiver;
  receiver.bind("127.0.0.1", 0);
  ASSERT_NE(receiver.boundPort(), 0);

  UdpSocket egress;
  std::ostringstream sink;
  EventLog log("", &sink, fixedClocks(11, 22));
  IviSender sender(egress, "127.0.0.1", receiver.boundPort(), log);

  const R4WarningEvent event = MakeWarningEvent();
  EXPECT_TRUE(sender.send(event));
  EXPECT_EQ(sender.failureCount(), 0u);

  const auto received = receiver.recvFrom(kGenerousTimeout);
  ASSERT_TRUE(received.has_value());
  const R4WarningEvent parsed = nlohmann::json::parse(*received).get<R4WarningEvent>();
  EXPECT_EQ(parsed, event);
}

TEST(IviSender, OneDatagramPerEvent) {
  UdpSocket receiver;
  receiver.bind("127.0.0.1", 0);
  UdpSocket egress;
  std::ostringstream sink;
  EventLog log("", &sink, fixedClocks(0, 0));
  IviSender sender(egress, "127.0.0.1", receiver.boundPort(), log);

  const R4WarningEvent event = MakeWarningEvent();
  ASSERT_TRUE(sender.send(event));
  ASSERT_TRUE(sender.send(event));

  ASSERT_TRUE(receiver.recvFrom(kGenerousTimeout).has_value());
  ASSERT_TRUE(receiver.recvFrom(kGenerousTimeout).has_value());
  EXPECT_EQ(log.count(ada::log::events::kR4Tx), 2u);
}

TEST(IviSender, R4TxLineCarriesTheDesignatedPayload) {
  UdpSocket receiver;
  receiver.bind("127.0.0.1", 0);
  UdpSocket egress;
  std::ostringstream sink;
  EventLog log("", &sink, fixedClocks(0, 0));
  IviSender sender(egress, "127.0.0.1", receiver.boundPort(), log);

  const R4WarningEvent event = MakeWarningEvent();
  ASSERT_TRUE(sender.send(event));

  const auto out = lines(sink.str());
  ASSERT_EQ(out.size(), 1u);
  const nlohmann::json j = parseEvtLine(out[0]);
  EXPECT_EQ(j.at("event"), "r4_tx");

  const nlohmann::json& payload = j.at("payload");
  // The body is a JSON OBJECT (not a string) and parses back equal.
  ASSERT_TRUE(payload.at("body").is_object());
  EXPECT_EQ(payload.at("body").get<R4WarningEvent>(), event);
  EXPECT_EQ(payload.at("bytes").get<std::int64_t>(),
            static_cast<std::int64_t>(nlohmann::json(event).dump().size()));
  EXPECT_EQ(payload.at("dest"),
            "127.0.0.1:" + std::to_string(receiver.boundPort()));
  EXPECT_EQ(payload.at("send_ok"), true);
}

TEST(IviSender, FailedSendIsCountedAndLoggedNeverThrown) {
  UdpSocket egress;
  std::ostringstream sink;
  EventLog log("", &sink, fixedClocks(0, 0));
  // Malformed address: sendTo throws by the socket's contract; the sender
  // must swallow it into a counted, logged failure (header comment).
  IviSender sender(egress, "not-an-address", 47300, log);

  const R4WarningEvent event = MakeWarningEvent();
  bool ok = true;
  EXPECT_NO_THROW(ok = sender.send(event));
  EXPECT_FALSE(ok);
  EXPECT_EQ(sender.failureCount(), 1u);

  const auto out = lines(sink.str());
  ASSERT_EQ(out.size(), 1u);
  const nlohmann::json j = parseEvtLine(out[0]);
  EXPECT_EQ(j.at("event"), "r4_tx");
  EXPECT_EQ(j.at("payload").at("send_ok"), false);
  // The evidence body rides on the failure line too.
  EXPECT_EQ(j.at("payload").at("body").get<R4WarningEvent>(), event);

  // Failures accumulate; still no throw.
  EXPECT_NO_THROW(sender.send(event));
  EXPECT_EQ(sender.failureCount(), 2u);
}

}  // namespace
