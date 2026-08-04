#ifndef ADA_ECU_OBSERVER_DETECTOR_READER_HPP
#define ADA_ECU_OBSERVER_DETECTOR_READER_HPP

// The detector's supervisor — the ego side of the D2 process contract:
// argv in, exit code out, one R3 JSONL object per stdout line; no FFI, no
// RPC, no shell. Spawns DETECTOR_CMD (split on whitespace into argv) with a
// stdout pipe and reads it line by line on one dedicated thread; each line
// is pushed as InputItem{DetectorR3, line, rxEpochMs}.
//
// Lifecycle (D2, one rule per line):
//   - each spawn emits detector_spawn
//   - clean EOF (child exited 0) emits detector_eof and respawns
//   - a non-zero exit emits detector_restart(reason, attempt) and respawns,
//     bounded by DETECTOR_RESTART_MAX; past the bound a terminal
//     detector_restart{terminal:true} line is written and no spawn follows
//   - DETECTOR_ENABLED=false means no spawn at all, logged once
// The clip-level loop belongs to the detector (DETECTOR_LOOP, D6); this
// class reads no env key and holds no loop flag — its inputs are values.

#include <sys/types.h>

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "log/event_log.hpp"
#include "observer/input_queue.hpp"

namespace ada::observer {

class DetectorReader {
 public:
  // Runs on the reader thread between a child's exit and its respawn —
  // never before the first spawn. respawnsSoFar counts respawns already
  // performed, so the first call sees 0. This is where a real delay or
  // backoff would sleep; the default performs none. Returning false
  // suppresses the respawn and ends the thread — tests bound the clean-EOF
  // respawn loop this way instead of real-sleeping.
  using RespawnHook = std::function<bool(int respawnsSoFar)>;

  // enabled / command / restartMax mirror Config::detectorEnabled /
  // detectorCmd / detectorRestartMax, passed as values so this class reads
  // no env and links no config. When enabled, the reader thread starts
  // inside the constructor.
  DetectorReader(bool enabled, std::string command, int restartMax,
                 InputQueue& queue, ada::log::EventLog& log,
                 RespawnHook respawnHook = RespawnHook{});

  // RAII: stop() — the child is killed and reaped, the thread joined.
  ~DetectorReader();

  DetectorReader(const DetectorReader&) = delete;
  DetectorReader& operator=(const DetectorReader&) = delete;

  // Idempotent shutdown. Flags the loop, SIGKILLs the live child so a
  // blocked read returns at once, and joins. A stop-killed exit emits no
  // event and triggers no respawn. Every child this class ever forked is
  // reaped by the loop's waitpid — no zombie survives stop().
  void stop();

  // Blocks until the reader thread ends on its own — terminal failure, a
  // hook returning false, or the disabled case (finished immediately).
  // Returns false on timeout. Test synchronization; never needed for use.
  bool waitFinished(std::chrono::milliseconds timeout);

 private:
  void run();
  bool stopping() const;
  void markFinished();

  const std::vector<std::string> argv_;  // the command split on whitespace
  const int restartMax_;
  InputQueue& queue_;
  ada::log::EventLog& log_;
  const RespawnHook respawnHook_;

  mutable std::mutex mutex_;
  std::condition_variable finishedCv_;
  bool stopping_ = false;
  bool finished_ = false;
  pid_t childPid_ = -1;  // > 0 only while a child is alive
  std::thread thread_;
};

}  // namespace ada::observer

#endif  // ADA_ECU_OBSERVER_DETECTOR_READER_HPP
