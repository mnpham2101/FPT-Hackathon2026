#ifndef ADA_ECU_OBSERVER_INPUT_QUEUE_HPP
#define ADA_ECU_OBSERVER_INPUT_QUEUE_HPP

// The D2 single-consumer bounded queue: two producer threads (the V2X
// receive thread and the detector reader) push raw lines; the main thread
// pops, so the store keeps exactly one writer. Header-only. No parsing, no
// I/O, no logging inside — items are opaque bytes with a routing tag.

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace ada::observer {

// The queue's routing tag: which producer pushed the line. Distinct from
// contracts::Source (the R3 wire enum) — this names the ingress edge, not
// the perception source field of a parsed object.
enum class Source { V2xR2, DetectorR3 };

struct InputItem {
  Source source{};
  std::string line;          // one raw datagram body or one raw JSONL line
  std::int64_t rxEpochMs{};  // CLOCK_REALTIME at receive, for ingest events
};

class InputQueue {
 public:
  explicit InputQueue(std::size_t capacity) : capacity_(capacity) {
    if (capacity_ == 0) {
      throw std::invalid_argument("InputQueue: capacity must be >= 1");
    }
  }

  InputQueue(const InputQueue&) = delete;
  InputQueue& operator=(const InputQueue&) = delete;

  // Never blocks a producer: when full, the OLDEST queued item is dropped
  // and counted, and the new item still enters. Returns false only when the
  // queue is closed (the item is discarded without counting as a drop).
  bool push(InputItem item) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (closed_) {
        return false;
      }
      if (items_.size() == capacity_) {
        items_.pop_front();
        ++dropped_;
      }
      items_.push_back(std::move(item));
    }
    available_.notify_one();
    return true;
  }

  // Blocks up to `timeout` for an item. Returns nullopt on timeout, and on
  // close() once the queue has drained — the consumer's shutdown signal.
  std::optional<InputItem> pop(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    available_.wait_for(lock, timeout, [this] { return closed_ || !items_.empty(); });
    if (items_.empty()) {
      return std::nullopt;
    }
    InputItem item = std::move(items_.front());
    items_.pop_front();
    return item;
  }

  // Wakes the consumer for clean shutdown. Already-queued items stay
  // poppable; further pushes are refused.
  void close() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      closed_ = true;
    }
    available_.notify_all();
  }

  bool closed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return closed_;
  }

  std::size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return items_.size();
  }

  // Items dropped oldest-first because the queue was full.
  std::uint64_t droppedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return dropped_;
  }

 private:
  const std::size_t capacity_;
  mutable std::mutex mutex_;
  std::condition_variable available_;
  std::deque<InputItem> items_;
  std::uint64_t dropped_ = 0;
  bool closed_ = false;
};

}  // namespace ada::observer

#endif  // ADA_ECU_OBSERVER_INPUT_QUEUE_HPP
