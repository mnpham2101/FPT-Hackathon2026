// Tests for the D2 bounded input queue (3.2.2.4). Deterministic: threads are
// synchronized by join and by the queue's own blocking pop — no sleeps used
// as synchronization.

#include "observer/input_queue.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using ada::observer::InputItem;
using ada::observer::InputQueue;
using ada::observer::Source;

constexpr std::chrono::milliseconds kNoWait{0};
constexpr std::chrono::milliseconds kLongWait{60000};

InputItem item(Source source, std::string line, std::int64_t rxEpochMs = 0) {
  return InputItem{source, std::move(line), rxEpochMs};
}

TEST(InputQueue, ZeroCapacityRejected) {
  EXPECT_THROW(InputQueue(0), std::invalid_argument);
}

TEST(InputQueue, FifoOrderPerProducer) {
  InputQueue queue(16);
  ASSERT_TRUE(queue.push(item(Source::V2xR2, "a", 1)));
  ASSERT_TRUE(queue.push(item(Source::V2xR2, "b", 2)));
  ASSERT_TRUE(queue.push(item(Source::DetectorR3, "c", 3)));

  const auto first = queue.pop(kNoWait);
  const auto second = queue.pop(kNoWait);
  const auto third = queue.pop(kNoWait);
  ASSERT_TRUE(first && second && third);
  EXPECT_EQ(first->line, "a");
  EXPECT_EQ(first->source, Source::V2xR2);
  EXPECT_EQ(first->rxEpochMs, 1);
  EXPECT_EQ(second->line, "b");
  EXPECT_EQ(third->line, "c");
  EXPECT_EQ(third->source, Source::DetectorR3);
  EXPECT_EQ(queue.droppedCount(), 0u);
}

TEST(InputQueue, BoundedCapacityDropsOldest) {
  InputQueue queue(3);
  for (int i = 1; i <= 5; ++i) {
    ASSERT_TRUE(queue.push(item(Source::V2xR2, std::to_string(i))));
  }
  EXPECT_EQ(queue.droppedCount(), 2u);
  EXPECT_EQ(queue.size(), 3u);

  // Oldest two (1, 2) dropped; the newest three remain in order.
  EXPECT_EQ(queue.pop(kNoWait)->line, "3");
  EXPECT_EQ(queue.pop(kNoWait)->line, "4");
  EXPECT_EQ(queue.pop(kNoWait)->line, "5");
  EXPECT_FALSE(queue.pop(kNoWait).has_value());
}

TEST(InputQueue, PopTimesOutOnEmptyQueue) {
  InputQueue queue(4);
  EXPECT_FALSE(queue.pop(std::chrono::milliseconds(10)).has_value());
}

TEST(InputQueue, PopReturnsAfterClose) {
  InputQueue queue(4);
  std::optional<InputItem> result = item(Source::V2xR2, "sentinel");

  // The consumer blocks in pop with a long timeout; close() must wake it
  // promptly with nullopt. join() is the synchronization — closing before or
  // after the consumer enters pop both yield nullopt, so there is no race.
  std::thread consumer([&queue, &result] { result = queue.pop(kLongWait); });
  queue.close();
  consumer.join();

  EXPECT_FALSE(result.has_value());
  EXPECT_TRUE(queue.closed());
}

TEST(InputQueue, CloseDrainsQueuedItemsThenNullopt) {
  InputQueue queue(4);
  ASSERT_TRUE(queue.push(item(Source::DetectorR3, "queued-before-close")));
  queue.close();

  const auto drained = queue.pop(kNoWait);
  ASSERT_TRUE(drained.has_value());
  EXPECT_EQ(drained->line, "queued-before-close");
  EXPECT_FALSE(queue.pop(kNoWait).has_value());
  // A push after close is refused and is not a drop.
  EXPECT_FALSE(queue.push(item(Source::V2xR2, "late")));
  EXPECT_EQ(queue.droppedCount(), 0u);
}

TEST(InputQueue, TwoConcurrentProducersDeliverEveryItem) {
  constexpr int kPerProducer = 1000;
  InputQueue queue(2 * kPerProducer);  // roomy: no drops expected

  auto produce = [&queue](Source source) {
    for (int i = 0; i < kPerProducer; ++i) {
      queue.push(item(source, std::to_string(i)));
    }
  };
  std::thread v2x(produce, Source::V2xR2);
  std::thread detector(produce, Source::DetectorR3);
  v2x.join();
  detector.join();

  int v2xSeen = 0;
  int detectorSeen = 0;
  int lastV2x = -1;
  int lastDetector = -1;
  while (const auto popped = queue.pop(kNoWait)) {
    const int sequence = std::stoi(popped->line);
    if (popped->source == Source::V2xR2) {
      EXPECT_EQ(sequence, lastV2x + 1) << "per-producer FIFO order broken";
      lastV2x = sequence;
      ++v2xSeen;
    } else {
      EXPECT_EQ(sequence, lastDetector + 1) << "per-producer FIFO order broken";
      lastDetector = sequence;
      ++detectorSeen;
    }
  }
  EXPECT_EQ(v2xSeen, kPerProducer);
  EXPECT_EQ(detectorSeen, kPerProducer);
  EXPECT_EQ(queue.droppedCount(), 0u);
}

TEST(InputQueue, ConcurrentProducersOverSmallCapacityCountEveryDrop) {
  constexpr int kPerProducer = 500;
  InputQueue queue(8);

  auto produce = [&queue](Source source) {
    for (int i = 0; i < kPerProducer; ++i) {
      queue.push(item(source, std::to_string(i)));
    }
  };
  std::thread v2x(produce, Source::V2xR2);
  std::thread detector(produce, Source::DetectorR3);
  v2x.join();
  detector.join();

  std::uint64_t delivered = 0;
  while (queue.pop(kNoWait).has_value()) {
    ++delivered;
  }
  // Every pushed item was either delivered or counted as a drop.
  EXPECT_EQ(delivered + queue.droppedCount(),
            static_cast<std::uint64_t>(2 * kPerProducer));
}

}  // namespace
