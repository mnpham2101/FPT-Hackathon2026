// Implementation of the detector supervisor (D2). POSIX-only by design —
// the target is the Linux container of node-ada-ecu.md, and CI runs
// ubuntu-latest. fork/exec with a stdout pipe, no shell, no FFI.

#include "observer/detector_reader.hpp"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdint>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

namespace ada::observer {
namespace {

namespace events = ada::log::events;

// The once-per-construction "disabled, not spawning" line (lifecycle rule:
// DETECTOR_ENABLED=false -> no spawn at all, logged once). Not one of the
// twelve declared D8 names — flagged in the subtask report for design
// ratification; none of the three detector events can carry "no spawn
// happened" without corrupting its meaning as spawn/eof/restart evidence.
constexpr const char* kDetectorDisabled = "detector_disabled";

std::vector<std::string> splitArgv(const std::string& command) {
  std::vector<std::string> out;
  std::istringstream stream(command);
  std::string token;
  while (stream >> token) {
    out.push_back(token);
  }
  return out;
}

// CLOCK_REALTIME in ms — the InputItem receive stamp.
std::int64_t epochMsNow() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// Human-readable reason for a child exit that was not a clean 0.
std::string exitReason(int status) {
  if (WIFEXITED(status)) {
    return "exit_code_" + std::to_string(WEXITSTATUS(status));
  }
  if (WIFSIGNALED(status)) {
    return "signal_" + std::to_string(WTERMSIG(status));
  }
  return "unknown_status_" + std::to_string(status);
}

}  // namespace

DetectorReader::DetectorReader(bool enabled, std::string command, int restartMax,
                               InputQueue& queue, ada::log::EventLog& log,
                               RespawnHook respawnHook)
    : argv_(splitArgv(command)),
      restartMax_(restartMax),
      queue_(queue),
      log_(log),
      respawnHook_(std::move(respawnHook)) {
  if (!enabled) {
    log_.emit(kDetectorDisabled, nlohmann::json{{"cmd", command}});
    markFinished();
    return;
  }
  thread_ = std::thread([this] { run(); });
}

DetectorReader::~DetectorReader() { stop(); }

void DetectorReader::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
    if (childPid_ > 0) {
      // SIGKILL, not SIGTERM: the blocked read must return deterministically,
      // and the detector holds no state needing a graceful shutdown (D2).
      ::kill(childPid_, SIGKILL);
    }
  }
  if (thread_.joinable()) {
    thread_.join();
  }
}

bool DetectorReader::waitFinished(std::chrono::milliseconds timeout) {
  std::unique_lock<std::mutex> lock(mutex_);
  return finishedCv_.wait_for(lock, timeout, [this] { return finished_; });
}

bool DetectorReader::stopping() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return stopping_;
}

void DetectorReader::markFinished() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    finished_ = true;
  }
  finishedCv_.notify_all();
}

void DetectorReader::run() {
  int respawns = 0;        // respawns performed, both kinds — the hook's input
  int failedAttempts = 0;  // restarts consumed by non-zero exits

  while (true) {
    if (stopping()) {
      break;
    }
    if (argv_.empty()) {
      // Defensive: config validation should refuse an empty DETECTOR_CMD
      // when enabled; an empty argv here is terminal, never a spawn loop.
      log_.emit(events::kDetectorRestart,
                nlohmann::json{{"reason", "empty_command"},
                               {"attempt", failedAttempts},
                               {"terminal", true}});
      break;
    }

    // The child's argv is built before fork so the child allocates nothing.
    std::vector<char*> childArgv;
    childArgv.reserve(argv_.size() + 1);
    for (const std::string& token : argv_) {
      childArgv.push_back(const_cast<char*>(token.c_str()));
    }
    childArgv.push_back(nullptr);

    int fds[2];
    if (::pipe(fds) != 0) {
      log_.emit(events::kDetectorRestart,
                nlohmann::json{{"reason", "pipe_failed"},
                               {"attempt", failedAttempts},
                               {"terminal", true}});
      break;
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
      ::close(fds[0]);
      ::close(fds[1]);
      log_.emit(events::kDetectorRestart,
                nlohmann::json{{"reason", "fork_failed"},
                               {"attempt", failedAttempts},
                               {"terminal", true}});
      break;
    }
    if (pid == 0) {
      // Child: stdout becomes the pipe's write end, then exec — no shell.
      ::dup2(fds[1], STDOUT_FILENO);
      ::close(fds[0]);
      ::close(fds[1]);
      ::execvp(childArgv[0], childArgv.data());
      ::_exit(127);  // exec failed; surfaces as a non-zero exit in the parent
    }

    // Parent.
    ::close(fds[1]);
    bool stopRequestedMidSpawn = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      childPid_ = pid;
      stopRequestedMidSpawn = stopping_;
    }
    if (stopRequestedMidSpawn) {
      // stop() ran between the loop check and the fork, so its kill may have
      // missed this pid. Kill, reap, and leave without emitting anything.
      ::kill(pid, SIGKILL);
      ::close(fds[0]);
      int status = 0;
      while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
      }
      std::lock_guard<std::mutex> lock(mutex_);
      childPid_ = -1;
      break;
    }

    log_.emit(events::kDetectorSpawn, nlohmann::json{{"pid", pid}});

    // The read loop: one getline per JSONL line until EOF. EOF arrives when
    // the child exits or is killed — both close the pipe's write end.
    FILE* stream = ::fdopen(fds[0], "r");
    if (stream == nullptr) {
      ::close(fds[0]);  // child gets SIGPIPE on next write and exits
    } else {
      char* buffer = nullptr;
      size_t capacity = 0;
      ssize_t length = 0;
      while ((length = ::getline(&buffer, &capacity, stream)) != -1) {
        std::string line(buffer, static_cast<std::size_t>(length));
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
          line.pop_back();
        }
        if (line.empty()) {
          continue;  // a blank line is not a JSONL record
        }
        queue_.push(InputItem{Source::DetectorR3, std::move(line), epochMsNow()});
      }
      ::free(buffer);
      ::fclose(stream);  // closes fds[0]
    }

    // Reap — every forked child passes through here, so none is a zombie.
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      childPid_ = -1;
    }
    if (stopping()) {
      break;  // a stop-killed exit emits no event and respawns nothing
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
      log_.emit(events::kDetectorEof, nlohmann::json{{"pid", pid}});
    } else {
      ++failedAttempts;
      const std::string reason = exitReason(status);
      if (failedAttempts > restartMax_) {
        log_.emit(events::kDetectorRestart,
                  nlohmann::json{{"reason", reason},
                                 {"attempt", failedAttempts},
                                 {"terminal", true}});
        break;
      }
      log_.emit(events::kDetectorRestart,
                nlohmann::json{{"reason", reason}, {"attempt", failedAttempts}});
    }

    if (respawnHook_ && !respawnHook_(respawns)) {
      break;
    }
    ++respawns;
  }

  markFinished();
}

}  // namespace ada::observer
