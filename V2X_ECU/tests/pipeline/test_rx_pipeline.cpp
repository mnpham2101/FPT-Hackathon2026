// Rx pipeline composition tests (subtask 9.1.4.4; Phase 1 HLD §3 D3): the four
// stages wired together — decode → validate → dedupe → build → sink — with
// every outcome landing on the R18 [EVT] stream and the frozen counters.
//
// The codec is a FAKE ICpmCodec, not the real VanetzaCpmCodec: this suite is
// about the COMPOSITION (stage order, short-circuiting, event emission, the
// noexcept guarantee), so the decode step is scripted with canned CpmContent
// loaded from the frozen golden `.json` fixtures (V2X_FIXTURE_DIR/golden/, via
// the codec seam's from_json) and with injected DecodeError values. That keeps
// the test deterministic and Vanetza-free. The REAL decode path is proven
// elsewhere: Phase 0 `1.0.2.5` (golden `.uper` round-trip through the real
// codec), subtask 9.1.4.5 (malformed corpus through the real codec) and the
// D7 `v2x-comms-check` CI lane (end-to-end on the wire).
//
// Two independent clocks, both faked: the pipeline's rx-time clock is fixed
// (so rxTime is assertable) and the Deduper's window clock is a mutable int64
// the tests advance to open or close the window — exactly the pattern in
// tests/pipeline/test_deduper.cpp.

#include "pipeline/rx_pipeline.hpp"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "codec/cpm_codec.hpp"
#include "contracts/r2_message.hpp"
#include "log/event_log.hpp"
#include "pipeline/deduper.hpp"
#include "pipeline/r2_builder.hpp"
#include "pipeline/validator.hpp"

namespace {

using std::chrono::milliseconds;
using v2x::codec::CpmContent;
using v2x::codec::DecodeError;
using v2x::contracts::R2Message;
using v2x::log::EventLog;
using v2x::pipeline::Deduper;
using v2x::pipeline::R2Builder;
using v2x::pipeline::RxPipeline;
using v2x::pipeline::Validator;

constexpr char kPrefix[] = "[EVT] ";
constexpr std::size_t kPrefixLen = sizeof(kPrefix) - 1;

// Fixed pipeline rx-time stamp (ms epoch) — assertable in R2Message::rxTime.
constexpr std::int64_t kRxTimeMs = 1754006400000;
// Test-local dedupe window; production takes it from Config::dedupe_window.
constexpr std::int64_t kDedupeWindowMs = 1500;
// Datagram payload size the fake codec ignores but rx_datagram reports.
constexpr std::size_t kDatagramBytes = 42;

// The frozen golden corpus, in the order test (a) feeds it. All six share
// stationId 1201 / objectId 7 / referenceTime, and four of them share
// measurementDeltaTime −50 — i.e. four share the dedupe key — so test (a) must
// advance the dedupe clock between datagrams to keep all six distinct
// sightings. That is a property of the corpus, not of the pipeline.
const char* const kGoldenNames[] = {"nominal.json",      "conf-unavailable.json",
                                    "coord-large.json",  "gate-boundary.json",
                                    "mdt-max.json",      "mdt-min.json"};
constexpr std::size_t kGoldenCount = sizeof(kGoldenNames) / sizeof(kGoldenNames[0]);

CpmContent LoadGolden(const std::string& name) {
  const std::string path = std::string(V2X_FIXTURE_DIR) + "/golden/" + name;
  std::ifstream in(path);
  EXPECT_TRUE(in.is_open()) << "cannot open fixture: " << path;
  return nlohmann::json::parse(in).get<CpmContent>();
}

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

// Scriptable ICpmCodec: each decode() call consumes the next queued outcome.
// decode() is const on the seam, so the queue and the call count are mutable.
class FakeCodec : public v2x::codec::ICpmCodec {
 public:
  void pushContent(CpmContent content) { outcomes_.emplace_back(std::move(content)); }
  void pushError(std::string reason) {
    outcomes_.emplace_back(DecodeError{std::move(reason)});
  }

  // Unused by the Rx path — the pipeline only ever decodes. Throwing (rather
  // than returning junk) makes an accidental call a loud test failure.
  std::vector<std::uint8_t> encode(const CpmContent&) const override {
    throw std::logic_error("FakeCodec::encode is not part of the Rx pipeline");
  }

  std::variant<CpmContent, DecodeError> decode(const std::uint8_t*,
                                               std::size_t) const override {
    ++calls_;
    if (outcomes_.empty()) {
      return DecodeError{"fake codec: no scripted outcome"};
    }
    std::variant<CpmContent, DecodeError> outcome = outcomes_.front();
    outcomes_.pop_front();
    return outcome;
  }

  std::size_t calls() const { return calls_; }

 private:
  mutable std::deque<std::variant<CpmContent, DecodeError>> outcomes_;
  mutable std::size_t calls_{};
};

// Member declaration order is the construction order: every collaborator is
// declared before pipeline_, so the references it stores are valid for the
// whole fixture lifetime.
class RxPipelineTest : public ::testing::Test {
 protected:
  // Number of leading sink calls that must throw (test (e)).
  int sink_throws_ = 0;
  std::vector<R2Message> forwarded_;
  std::ostringstream log_out_;
  FakeCodec codec_;
  Validator validator_;
  std::int64_t dedupe_clock_ms_ = 0;
  Deduper deduper_{milliseconds(kDedupeWindowMs), [this] { return dedupe_clock_ms_; }};
  R2Builder builder_;
  EventLog log_{log_out_};
  RxPipeline pipeline_{codec_,
                       validator_,
                       deduper_,
                       builder_,
                       log_,
                       [this](const R2Message& r2) {
                         if (sink_throws_ > 0) {
                           --sink_throws_;
                           throw std::runtime_error("sink failed");
                         }
                         forwarded_.push_back(r2);
                       },
                       [] { return kRxTimeMs; }};

  // Fixed payload; parentheses (not braces) so this is 42 copies of 0xAB, not
  // a two-element initializer list.
  const std::vector<std::uint8_t> datagram_ =
      std::vector<std::uint8_t>(kDatagramBytes, 0xAB);

  std::vector<std::string> EvtLines() const { return Lines(log_out_.str()); }

  // The counters object of the most recent [EVT] line — the running totals the
  // frozen D4 contract exposes (log/event_log.hpp owns the counting).
  nlohmann::json LastCounters() const {
    const auto lines = EvtLines();
    EXPECT_FALSE(lines.empty()) << "no [EVT] line emitted";
    if (lines.empty()) {
      return nlohmann::json::object();
    }
    return ParseEvtLine(lines.back()).at("counters");
  }

  void ExpectCounters(std::uint64_t rx, std::uint64_t decode_ok,
                      std::uint64_t decode_reject, std::uint64_t validate_reject,
                      std::uint64_t dedupe_drop, std::uint64_t r2_forwarded) const {
    const nlohmann::json c = LastCounters();
    EXPECT_EQ(c.at("rx_datagram").get<std::uint64_t>(), rx);
    EXPECT_EQ(c.at("decode_ok").get<std::uint64_t>(), decode_ok);
    EXPECT_EQ(c.at("decode_reject").get<std::uint64_t>(), decode_reject);
    EXPECT_EQ(c.at("validate_reject").get<std::uint64_t>(), validate_reject);
    EXPECT_EQ(c.at("dedupe_drop").get<std::uint64_t>(), dedupe_drop);
    EXPECT_EQ(c.at("r2_forwarded").get<std::uint64_t>(), r2_forwarded);
  }
};

// (a) Happy path: all six golden contents flow through every stage to the sink,
// carrying correctly derived R2 values, and the counters end at 6/6/6.
TEST_F(RxPipelineTest, AllGoldenContentsReachTheSink) {
  for (std::size_t i = 0; i < kGoldenCount; ++i) {
    codec_.pushContent(LoadGolden(kGoldenNames[i]));
    pipeline_.onDatagram(datagram_);
    // Step past the window so the shared-key fixtures stay distinct sightings.
    dedupe_clock_ms_ += kDedupeWindowMs;
  }

  ASSERT_EQ(codec_.calls(), kGoldenCount);
  ASSERT_EQ(forwarded_.size(), kGoldenCount);

  for (const R2Message& r2 : forwarded_) {
    EXPECT_EQ(r2.schemaVersion, 1);
    EXPECT_EQ(r2.type, "v2x_object");
    EXPECT_EQ(r2.stationId, 1201u);       // every golden vector is station B 1201
    EXPECT_EQ(r2.object.objectId, 7);     // ...perceiving object C 7
    EXPECT_EQ(r2.rxTime, kRxTimeMs);      // stamped from the injected clock
  }

  // nominal: position 2500/120 wire → 25.0/1.2 m, distance = the F7 hypot rule.
  const R2Message& nominal = forwarded_[0];
  EXPECT_NEAR(nominal.object.position.x, 25.0, 1e-9);
  EXPECT_NEAR(nominal.object.position.y, 1.2, 1e-9);
  EXPECT_NEAR(nominal.object.distance, std::hypot(25.0, 1.2), 1e-9);
  EXPECT_EQ(nominal.object.timeOfMeasurement, -50);

  // conf-unavailable: F6 ConfidenceLevel 101 → null class confidence.
  EXPECT_FALSE(forwarded_[1].object.confidence.has_value());

  // coord-large: the CartesianCoordinateLarge extremes still hypot correctly.
  EXPECT_NEAR(forwarded_[2].object.distance, std::hypot(1310.71, -1310.72), 1e-6);

  // gate-boundary: x only, so the hypot collapses to the axis value — 30.0 m.
  EXPECT_NEAR(forwarded_[3].object.distance, 30.0, 1e-9);

  // mdt-max / mdt-min: the F9 extremes pass validation and carry through.
  EXPECT_EQ(forwarded_[4].object.timeOfMeasurement, 2047);
  EXPECT_EQ(forwarded_[5].object.timeOfMeasurement, -2047);

  // Three events per datagram (rx_datagram, decode_ok, r2_forwarded).
  EXPECT_EQ(EvtLines().size(), 3 * kGoldenCount);
  ExpectCounters(/*rx=*/kGoldenCount, /*decode_ok=*/kGoldenCount, /*decode_reject=*/0,
                 /*validate_reject=*/0, /*dedupe_drop=*/0, /*r2_forwarded=*/kGoldenCount);

  // rx_datagram carries the datagram size (non-frozen "bytes" diagnostic).
  const nlohmann::json first = ParseEvtLine(EvtLines().front());
  EXPECT_EQ(first.at("event").get<std::string>(), "rx_datagram");
  EXPECT_EQ(first.at("bytes").get<std::size_t>(), kDatagramBytes);
}

// (b) Stage 1 short-circuit: DecodeError → reject + count, no sink call, and
// nothing thrown out of onDatagram.
TEST_F(RxPipelineTest, DecodeErrorRejectsWithoutForwarding) {
  codec_.pushError("truncated datagram");

  pipeline_.onDatagram(datagram_);

  EXPECT_TRUE(forwarded_.empty());
  ExpectCounters(1, 0, 1, 0, 0, 0);

  // Two lines only: rx_datagram then decode_reject — no decode_ok in between.
  const auto lines = EvtLines();
  ASSERT_EQ(lines.size(), 2u);
  const nlohmann::json reject = ParseEvtLine(lines.back());
  EXPECT_EQ(reject.at("event").get<std::string>(), "decode_reject");
  EXPECT_EQ(reject.at("detail").get<std::string>(), "truncated datagram");
}

// (c) Stage 2 short-circuit: measurementDeltaTime −2048 is legal on the wire
// but banned by profile rule F9, so only the validator can catch it.
TEST_F(RxPipelineTest, ValidationViolationRejectsWithoutForwarding) {
  CpmContent content = LoadGolden("nominal.json");
  content.object.measurementDeltaTime = -2048;
  codec_.pushContent(content);

  pipeline_.onDatagram(datagram_);

  EXPECT_TRUE(forwarded_.empty());
  ExpectCounters(1, 1, 0, 1, 0, 0);

  // decode_ok is emitted (the bytes did decode), then validate_reject by reason.
  const auto lines = EvtLines();
  ASSERT_EQ(lines.size(), 3u);
  EXPECT_EQ(ParseEvtLine(lines[1]).at("event").get<std::string>(), "decode_ok");
  const nlohmann::json reject = ParseEvtLine(lines[2]);
  EXPECT_EQ(reject.at("event").get<std::string>(), "validate_reject");
  EXPECT_EQ(reject.at("detail").get<std::string>(), "mdt_f9_range");
}

// (d) Stage 3 short-circuit: the same content twice with the dedupe clock held
// inside the window → the second is dropped, not forwarded.
TEST_F(RxPipelineTest, DuplicateInsideWindowIncrementsDedupeDrop) {
  const CpmContent content = LoadGolden("nominal.json");
  codec_.pushContent(content);
  codec_.pushContent(content);

  pipeline_.onDatagram(datagram_);
  pipeline_.onDatagram(datagram_);  // dedupe_clock_ms_ unchanged → still inside

  ASSERT_EQ(forwarded_.size(), 1u);
  ExpectCounters(2, 2, 0, 0, 1, 1);

  // 6 lines: rx_datagram/decode_ok/r2_forwarded, then rx_datagram/decode_ok/
  // dedupe_drop — the duplicate reaches stage 3 before it is stopped.
  const auto lines = EvtLines();
  ASSERT_EQ(lines.size(), 6u);
  EXPECT_EQ(ParseEvtLine(lines.back()).at("event").get<std::string>(), "dedupe_drop");
}

// (e) The noexcept guarantee (R9 zero-crash): a throwing sink must not take the
// Rx thread down, and the pipeline must keep serving the next datagram.
// onDatagram is declared noexcept, so an escaping exception would std::terminate
// rather than propagate — EXPECT_NO_THROW documents the intent; surviving to the
// assertions below is the actual proof.
TEST_F(RxPipelineTest, ThrowingSinkIsContainedAndPipelineKeepsRunning) {
  sink_throws_ = 1;  // only the first sink call throws
  codec_.pushContent(LoadGolden("nominal.json"));

  EXPECT_NO_THROW(pipeline_.onDatagram(datagram_));
  EXPECT_TRUE(forwarded_.empty());

  // Attributed per rx_pipeline.cpp: one decode_reject flagged
  // "unexpected_exception: ..." — and r2_forwarded stays 0, because the event
  // is only emitted after the sink returns.
  ExpectCounters(1, 1, 1, 0, 0, 0);
  const nlohmann::json reject = ParseEvtLine(EvtLines().back());
  EXPECT_EQ(reject.at("event").get<std::string>(), "decode_reject");
  EXPECT_NE(reject.at("detail").get<std::string>().find("unexpected_exception:"),
            std::string::npos);

  // Next datagram: same content past the window (the dropped one still
  // refreshed the dedupe entry), sink no longer throws → forwarded normally.
  dedupe_clock_ms_ += kDedupeWindowMs;
  codec_.pushContent(LoadGolden("nominal.json"));

  EXPECT_NO_THROW(pipeline_.onDatagram(datagram_));
  ASSERT_EQ(forwarded_.size(), 1u);
  EXPECT_EQ(forwarded_[0].stationId, 1201u);
  ExpectCounters(2, 2, 1, 0, 0, 1);
}

}  // namespace
