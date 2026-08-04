// R18 JSONL event-log tests (subtask 18.1.2.3; HLD decision D4 as amended
// 2026-08-01). Asserts the frozen [EVT] line shape the D7 checker
// (tools/comms_check/check_v2x_log.py, subtask 9.1.12.2) parses: event /
// mono_ms / epoch_ms / counters on every line, cpm on decode_ok, r2 on
// r2_forwarded — plus the file sink mirroring the stream sink.

#include "log/event_log.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace {

using v2x::codec::CpmContent;
using v2x::contracts::R2Message;
using v2x::log::EventLog;

constexpr char kPrefix[] = "[EVT] ";
constexpr std::size_t kPrefixLen = sizeof(kPrefix) - 1;

const char* const kCounterKeys[] = {"rx_datagram",     "decode_ok",   "decode_reject",
                                    "validate_reject", "dedupe_drop", "r2_forwarded"};

std::vector<std::string> Lines(const std::string& text) {
  std::vector<std::string> lines;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  return lines;
}

nlohmann::json ParseEvtLine(const std::string& line) {
  EXPECT_EQ(line.rfind(kPrefix, 0), 0u) << "line missing [EVT] prefix: " << line;
  return nlohmann::json::parse(line.substr(kPrefixLen));
}

// The frozen `nominal` golden vector as a struct (construction pattern:
// tests/codec/test_cpm_content_roundtrip.cpp).
CpmContent NominalContent() {
  CpmContent c;
  c.stationId = 1201;
  c.referenceTime = 716084805123;
  c.referencePosition.latitude = 210285110;
  c.referencePosition.longitude = 1058048170;
  c.orientationAngle = 900;
  c.object.objectId = 7;
  c.object.measurementDeltaTime = -50;
  c.object.position.x = 2500;
  c.object.position.y = 120;
  c.object.position.xConfidence = 90;
  c.object.position.yConfidence = 90;
  c.object.velocity.x = 1520;
  c.object.velocity.y = 0;
  c.object.classification = 5;
  c.object.classConfidence = 95;
  return c;
}

// The node-local frozen R2 sample (load pattern:
// tests/contracts/test_r2_roundtrip.cpp).
nlohmann::json LoadR2Fixture() {
  const std::string path = std::string(V2X_FIXTURE_DIR) + "/samples/r2-object.json";
  std::ifstream in(path);
  EXPECT_TRUE(in.is_open()) << "cannot open fixture: " << path;
  return nlohmann::json::parse(in);
}

// (a) Every emitted line: [EVT]-prefixed, parseable JSON, correct event name,
// mono_ms/epoch_ms present, all six frozen counter keys present.
TEST(EventLog, EveryLineIsPrefixedParseableJsonWithFrozenFields) {
  std::ostringstream out;
  EventLog log(out);

  log.rxDatagram(64);
  log.decodeOk(NominalContent());
  log.decodeReject("truncated datagram");
  log.validateReject("measurementDeltaTime out of range");
  log.dedupeDrop();
  log.r2Forwarded(LoadR2Fixture().get<R2Message>());
  log.stubTransition("idle -> initialized");
  log.faultInjected("subscription_drop");
  log.recovery("resubscribed");

  const auto lines = Lines(out.str());
  ASSERT_EQ(lines.size(), 9u);

  const char* const expected[] = {"rx_datagram",  "decode_ok",       "decode_reject",
                                  "validate_reject", "dedupe_drop",  "r2_forwarded",
                                  "stub_transition", "fault_injected", "recovery"};
  std::int64_t last_mono = 0;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    const nlohmann::json j = ParseEvtLine(lines[i]);
    EXPECT_EQ(j.at("event").get<std::string>(), expected[i]);
    ASSERT_TRUE(j.at("mono_ms").is_number_integer());
    ASSERT_TRUE(j.at("epoch_ms").is_number_integer());
    const auto mono = j.at("mono_ms").get<std::int64_t>();
    EXPECT_GE(mono, last_mono);  // non-negative and non-decreasing
    last_mono = mono;
    EXPECT_GE(j.at("epoch_ms").get<std::int64_t>(), 0);
    ASSERT_TRUE(j.at("counters").is_object());
    for (const char* key : kCounterKeys) {
      EXPECT_TRUE(j.at("counters").contains(key)) << "missing counter key: " << key;
    }
  }
}

// (b) Counters accumulate per same-named stage event; stub events count nothing.
TEST(EventLog, CountersAccumulatePerStage) {
  std::ostringstream out;
  EventLog log(out);

  log.rxDatagram(10);
  log.rxDatagram(20);
  log.decodeOk(NominalContent());
  log.stubTransition("initialized -> configured");  // reports, not a stage

  const auto lines = Lines(out.str());
  ASSERT_EQ(lines.size(), 4u);
  const nlohmann::json last = ParseEvtLine(lines.back());
  const auto& counters = last.at("counters");
  EXPECT_EQ(counters.at("rx_datagram").get<std::uint64_t>(), 2u);
  EXPECT_EQ(counters.at("decode_ok").get<std::uint64_t>(), 1u);
  EXPECT_EQ(counters.at("decode_reject").get<std::uint64_t>(), 0u);
  EXPECT_EQ(counters.at("validate_reject").get<std::uint64_t>(), 0u);
  EXPECT_EQ(counters.at("dedupe_drop").get<std::uint64_t>(), 0u);
  EXPECT_EQ(counters.at("r2_forwarded").get<std::uint64_t>(), 0u);
}

// (c) decode_ok embeds "cpm": the payload round-trips to an equal CpmContent.
TEST(EventLog, DecodeOkEmbedsCpmPayload) {
  std::ostringstream out;
  EventLog log(out);
  const CpmContent content = NominalContent();

  log.decodeOk(content);

  const auto lines = Lines(out.str());
  ASSERT_EQ(lines.size(), 1u);
  const nlohmann::json j = ParseEvtLine(lines[0]);
  ASSERT_TRUE(j.contains("cpm"));
  const auto roundTripped = j.at("cpm").get<CpmContent>();
  EXPECT_TRUE(roundTripped == content);
}

// (d) r2_forwarded embeds "r2": the payload equals the frozen sample semantically.
TEST(EventLog, R2ForwardedEmbedsR2Payload) {
  const nlohmann::json sample = LoadR2Fixture();
  std::ostringstream out;
  EventLog log(out);

  log.r2Forwarded(sample.get<R2Message>());

  const auto lines = Lines(out.str());
  ASSERT_EQ(lines.size(), 1u);
  const nlohmann::json j = ParseEvtLine(lines[0]);
  ASSERT_TRUE(j.contains("r2"));
  EXPECT_EQ(j.at("r2"), sample);
}

// (e) File sink: a non-empty path mirrors the stream sink line for line.
TEST(EventLog, FileSinkMirrorsStreamSink) {
  const std::string path = ::testing::TempDir() + "/v2x_event_log_test_18123.jsonl";
  std::remove(path.c_str());  // the sink appends — drop any stale previous run

  std::ostringstream out;
  {
    EventLog log(out, path);
    log.rxDatagram(64);
    log.dedupeDrop();
    log.stubTransition("idle -> initialized");
  }  // destruction closes the file sink

  std::ifstream in(path);
  ASSERT_TRUE(in.is_open()) << "file sink not written: " << path;
  std::ostringstream fileContent;
  fileContent << in.rdbuf();

  EXPECT_EQ(fileContent.str(), out.str());  // identical [EVT] lines
  const auto lines = Lines(fileContent.str());
  ASSERT_EQ(lines.size(), 3u);
  for (const auto& line : lines) {
    (void)ParseEvtLine(line);  // prefixed + parseable
  }
}

}  // namespace
