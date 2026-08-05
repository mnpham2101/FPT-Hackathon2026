// Tests for the R18 [EVT] JSONL writer (18.2.2.3). The sink and both clocks
// are injected, so no test reads real stdout or a real clock.

#include "log/event_log.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {

using ada::log::EventLog;
namespace events = ada::log::events;

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

TEST(EventLog, LineParsesAsJsonAfterPrefix) {
  std::ostringstream sink;
  EventLog log("", &sink, fixedClocks(111, 222));

  log.emit(events::kR2Ingest, nlohmann::json{{"stationId", 1001}});

  const auto out = lines(sink.str());
  ASSERT_EQ(out.size(), 1u);
  const nlohmann::json j = parseEvtLine(out[0]);
  EXPECT_EQ(j.at("event"), "r2_ingest");
  EXPECT_EQ(j.at("mono_ms"), 111);
  EXPECT_EQ(j.at("epoch_ms"), 222);
  EXPECT_EQ(j.at("counters").at("r2_ingest"), 1);
  EXPECT_EQ(j.at("payload").at("stationId"), 1001);
}

TEST(EventLog, BothClockStampsWrittenNeverDerived) {
  std::ostringstream sink;
  // Deliberately unrelated values: a reader must see both, independently.
  EventLog log("", &sink, fixedClocks(5, 1789000000123));

  log.emit(events::kDetectorSpawn);

  const nlohmann::json j = parseEvtLine(lines(sink.str())[0]);
  EXPECT_EQ(j.at("mono_ms"), 5);
  EXPECT_EQ(j.at("epoch_ms"), 1789000000123);
}

TEST(EventLog, CountersAccumulateAcrossEmits) {
  std::ostringstream sink;
  EventLog log("", &sink, fixedClocks(0, 0));

  log.emit(events::kR2Ingest);
  log.emit(events::kR2Ingest);
  log.emit(events::kParseReject);
  log.emit(events::kR2Ingest);

  EXPECT_EQ(log.count(events::kR2Ingest), 3u);
  EXPECT_EQ(log.count(events::kParseReject), 1u);
  EXPECT_EQ(log.count(events::kR4Tx), 0u);

  const auto out = lines(sink.str());
  ASSERT_EQ(out.size(), 4u);
  const nlohmann::json last = parseEvtLine(out[3]);
  EXPECT_EQ(last.at("counters").at("r2_ingest"), 3);
  EXPECT_EQ(last.at("counters").at("parse_reject"), 1);
}

TEST(EventLog, EmbeddedPayloadPresentAndParseable) {
  std::ostringstream sink;
  EventLog log("", &sink, fixedClocks(0, 0));

  const nlohmann::json transition = {
      {"id", "v2x:1001:7"}, {"source", "v2x_relayed"}, {"from", "not_tracked"},
      {"to", "tentative"},  {"distance", 29.9},        {"reason", "gate_enter"},
  };
  log.emit(events::kTrackTransition, transition);

  const nlohmann::json j = parseEvtLine(lines(sink.str())[0]);
  EXPECT_EQ(j.at("payload"), transition);
}

TEST(EventLog, PayloadWithQuotesAndNewlinesRoundTripsOnOneLine) {
  std::ostringstream sink;
  EventLog log("", &sink, fixedClocks(0, 0));

  const std::string tricky = "he said \"warning\"\nsecond line\tend";
  log.emit(events::kParseReject, nlohmann::json{{"raw", tricky}});

  const auto out = lines(sink.str());
  // Escaping keeps the record a single physical line.
  ASSERT_EQ(out.size(), 1u);
  const nlohmann::json j = parseEvtLine(out[0]);
  EXPECT_EQ(j.at("payload").at("raw"), tricky);
}

TEST(EventLog, EmptyPayloadIsAnEmptyObject) {
  std::ostringstream sink;
  EventLog log("", &sink, fixedClocks(0, 0));

  log.emit(events::kDetectorEof);

  const nlohmann::json j = parseEvtLine(lines(sink.str())[0]);
  ASSERT_TRUE(j.at("payload").is_object());
  EXPECT_TRUE(j.at("payload").empty());
}

TEST(EventLog, FileSinkWritesWhenPathSet) {
  const auto unique =
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  const auto path = std::filesystem::temp_directory_path() /
                    ("ada_event_log_test_" + unique + ".jsonl");
  std::filesystem::remove(path);

  std::ostringstream sink;
  {
    EventLog log(path.string(), &sink, fixedClocks(7, 8));
    log.emit(events::kOwnSensorIngest, nlohmann::json{{"id", "own:1"}});
    log.emit(events::kR2Ingest);
  }

  std::ifstream file(path);
  ASSERT_TRUE(file.is_open());
  std::string first;
  std::string second;
  ASSERT_TRUE(static_cast<bool>(std::getline(file, first)));
  ASSERT_TRUE(static_cast<bool>(std::getline(file, second)));

  const nlohmann::json j1 = parseEvtLine(first);
  EXPECT_EQ(j1.at("event"), "own_sensor_ingest");
  EXPECT_EQ(j1.at("payload").at("id"), "own:1");
  const nlohmann::json j2 = parseEvtLine(second);
  EXPECT_EQ(j2.at("event"), "r2_ingest");

  // The stdout sink still received the same two lines.
  EXPECT_EQ(lines(sink.str()).size(), 2u);

  file.close();
  std::filesystem::remove(path);
}

TEST(EventLog, VocabularyDeclaresAllTwelveEvents) {
  ASSERT_EQ(std::size(events::kAll), 12u);
  EXPECT_STREQ(events::kAll[0], "detector_spawn");
  EXPECT_STREQ(events::kAll[7], "track_expire");
  EXPECT_STREQ(events::kAll[11], "r4_tx");
}

}  // namespace
