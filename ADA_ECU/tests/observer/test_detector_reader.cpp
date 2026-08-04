// Tests for the detector supervisor (12.2.6.2) — the D2 lifecycle against
// real forked children (/bin/echo, /usr/bin/printf, /bin/false, /usr/bin/yes;
// CI is ubuntu-latest). Deterministic: synchronization is the injected
// respawn hook, waitFinished(), stop()'s join, and bounded queue pops —
// no sleep is used as synchronization.

#include "observer/detector_reader.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <sys/types.h>
#include <sys/wait.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "log/event_log.hpp"
#include "observer/input_queue.hpp"

namespace {

using ada::log::EventLog;
using ada::observer::DetectorReader;
using ada::observer::InputItem;
using ada::observer::InputQueue;
using ada::observer::Source;
namespace events = ada::log::events;

constexpr std::chrono::milliseconds kNoWait{0};
constexpr std::chrono::milliseconds kLongWait{60000};

EventLog::Clocks fixedClocks() {
  EventLog::Clocks clocks;
  clocks.monoMs = []() -> std::int64_t { return 1; };
  clocks.epochMs = []() -> std::int64_t { return 2; };
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

// Parses the JSON after the [EVT] prefix; keeps only the named event.
std::vector<nlohmann::json> eventsNamed(const std::string& sinkText,
                                        const std::string& name) {
  std::vector<nlohmann::json> out;
  const std::string prefix = "[EVT] ";
  for (const std::string& line : lines(sinkText)) {
    EXPECT_EQ(line.rfind(prefix, 0), 0u) << "line lacks the [EVT] prefix: " << line;
    nlohmann::json j = nlohmann::json::parse(line.substr(prefix.size()));
    if (j.at("event") == name) {
      out.push_back(std::move(j));
    }
  }
  return out;
}

TEST(DetectorReader, ChildLinesAllArriveOnTheQueue) {
  std::ostringstream sink;
  EventLog log("", &sink, fixedClocks());
  InputQueue queue(16);

  // printf expands the two-character \n in its format argument, so a single
  // whitespace-split argv yields three stdout lines — no shell involved.
  DetectorReader reader(true, "/usr/bin/printf %s\\n l1 l2 l3", 5, queue, log,
                        [](int) { return false; });  // no respawn after EOF

  for (const char* expected : {"l1", "l2", "l3"}) {
    const std::optional<InputItem> item = queue.pop(kLongWait);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->source, Source::DetectorR3);
    EXPECT_EQ(item->line, expected);
    EXPECT_GT(item->rxEpochMs, 0);
  }
  ASSERT_TRUE(reader.waitFinished(kLongWait));
  reader.stop();

  EXPECT_FALSE(queue.pop(kNoWait).has_value());
  EXPECT_EQ(log.count(events::kDetectorSpawn), 1u);
  EXPECT_EQ(log.count(events::kDetectorEof), 1u);
  EXPECT_EQ(log.count(events::kDetectorRestart), 0u);
}

TEST(DetectorReader, CleanEofRespawnsUntilTheHookRefuses) {
  std::ostringstream sink;
  EventLog log("", &sink, fixedClocks());
  InputQueue queue(16);
  std::atomic<int> hookCalls{0};

  {
    // Allow exactly two respawns; the third hook call refuses, bounding the
    // otherwise-unbounded clean-EOF respawn loop.
    DetectorReader reader(true, "/bin/echo tick", 5, queue, log,
                          [&hookCalls](int) { return ++hookCalls <= 2; });
    ASSERT_TRUE(reader.waitFinished(kLongWait));
  }  // destructor: stop() joins

  EXPECT_EQ(hookCalls.load(), 3);
  EXPECT_EQ(log.count(events::kDetectorSpawn), 3u);
  EXPECT_EQ(log.count(events::kDetectorEof), 3u);
  EXPECT_EQ(log.count(events::kDetectorRestart), 0u);
  for (int i = 0; i < 3; ++i) {
    const std::optional<InputItem> item = queue.pop(kNoWait);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->line, "tick");
  }
  EXPECT_FALSE(queue.pop(kNoWait).has_value());
}

TEST(DetectorReader, NonZeroExitRestartsUpToTheMaxThenStops) {
  std::ostringstream sink;
  EventLog log("", &sink, fixedClocks());
  InputQueue queue(16);
  std::atomic<int> hookCalls{0};
  constexpr int kRestartMax = 2;

  {
    DetectorReader reader(true, "/bin/false", kRestartMax, queue, log,
                          [&hookCalls](int) {
                            ++hookCalls;
                            return true;  // never refuses; the bound must stop it
                          });
    ASSERT_TRUE(reader.waitFinished(kLongWait));
  }

  // Initial spawn + kRestartMax restarts, then terminal — no further spawn.
  EXPECT_EQ(log.count(events::kDetectorSpawn), 1u + kRestartMax);
  EXPECT_EQ(log.count(events::kDetectorEof), 0u);
  // The hook runs before each performed restart, not before the terminal stop.
  EXPECT_EQ(hookCalls.load(), kRestartMax);

  const std::vector<nlohmann::json> restarts =
      eventsNamed(sink.str(), events::kDetectorRestart);
  ASSERT_EQ(restarts.size(), static_cast<std::size_t>(kRestartMax) + 1);
  for (std::size_t i = 0; i < restarts.size(); ++i) {
    const nlohmann::json& payload = restarts[i].at("payload");
    EXPECT_EQ(payload.at("reason"), "exit_code_1");
    EXPECT_EQ(payload.at("attempt"), static_cast<int>(i) + 1);
    if (i + 1 < restarts.size()) {
      EXPECT_FALSE(payload.contains("terminal"));
    } else {
      EXPECT_EQ(payload.at("terminal"), true);  // the terminal-failure line
    }
  }
  EXPECT_FALSE(queue.pop(kNoWait).has_value());
}

TEST(DetectorReader, DisabledSpawnsNothingAndLogsOnce) {
  std::ostringstream sink;
  EventLog log("", &sink, fixedClocks());
  InputQueue queue(16);
  std::atomic<int> hookCalls{0};

  {
    DetectorReader reader(/*enabled=*/false, "/bin/echo never", 5, queue, log,
                          [&hookCalls](int) {
                            ++hookCalls;
                            return true;
                          });
    EXPECT_TRUE(reader.waitFinished(kNoWait));  // finished before any spawn
  }

  EXPECT_EQ(log.count(events::kDetectorSpawn), 0u);
  EXPECT_EQ(log.count(events::kDetectorEof), 0u);
  EXPECT_EQ(log.count(events::kDetectorRestart), 0u);
  EXPECT_EQ(hookCalls.load(), 0);
  EXPECT_FALSE(queue.pop(kNoWait).has_value());
  // Logged once: the single disabled line, and nothing else in the sink.
  EXPECT_EQ(log.count("detector_disabled"), 1u);
  EXPECT_EQ(lines(sink.str()).size(), 1u);
}

TEST(DetectorReader, DestructionKillsAndReapsTheChildNoZombie) {
  std::ostringstream sink;
  EventLog log("", &sink, fixedClocks());
  InputQueue queue(8);
  std::atomic<int> hookCalls{0};

  {
    // yes streams "ready" lines forever: it never exits on its own, so the
    // destructor's kill path is what ends it. The bounded queue absorbs the
    // flood by dropping oldest — the producer never blocks.
    DetectorReader reader(true, "/usr/bin/yes ready", 5, queue, log,
                          [&hookCalls](int) {
                            ++hookCalls;
                            return true;
                          });
    // One popped line proves the child is alive and producing.
    const std::optional<InputItem> item = queue.pop(kLongWait);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->line, "ready");
    EXPECT_EQ(item->source, Source::DetectorR3);
  }  // destructor: SIGKILL, waitpid reap, join

  // A stop-killed exit emits nothing and respawns nothing.
  EXPECT_EQ(log.count(events::kDetectorSpawn), 1u);
  EXPECT_EQ(log.count(events::kDetectorEof), 0u);
  EXPECT_EQ(log.count(events::kDetectorRestart), 0u);
  EXPECT_EQ(hookCalls.load(), 0);

  // No zombie: every child was reaped, so the process has no children left.
  errno = 0;
  const pid_t reaped = ::waitpid(-1, nullptr, WNOHANG);
  EXPECT_EQ(reaped, -1);
  EXPECT_EQ(errno, ECHILD);
}

}  // namespace
