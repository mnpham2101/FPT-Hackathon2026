#ifndef ADA_ECU_OBSERVER_V2X_LISTENER_HPP
#define ADA_ECU_OBSERVER_V2X_LISTENER_HPP

// The R2 ingress thread (HLD §6): one blocking receive per datagram on the
// injected (host, port); each body goes onto the input queue as an opaque
// InputItem tagged Source::V2xR2. No parsing, no logging here — receive
// errors are counted and exposed for the caller to log (D2). Socket access
// goes through net::UdpSocket only; this file includes no socket headers.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

#include "net/udp_socket.hpp"
#include "observer/input_queue.hpp"

namespace ada::observer {

// Owns one net::UdpSocket and one receive thread, joined on destruction —
// never detached. Non-copyable and non-movable: the running thread holds
// `this`.
class V2xListener {
 public:
  // Binds to (host, port) and starts the receive thread. Setup errors
  // (bad address, bind conflict) throw per UdpSocket, before any thread
  // starts. `pollTimeout` bounds each blocking receive — it is what makes
  // stop() prompt — and is supplied by the caller, never a literal here.
  V2xListener(const std::string& host, std::uint16_t port, InputQueue& queue,
              std::chrono::milliseconds pollTimeout);

  // Stops and joins the thread.
  ~V2xListener();

  V2xListener(const V2xListener&) = delete;
  V2xListener& operator=(const V2xListener&) = delete;

  // Asks the thread to exit after its current bounded receive. Prompt
  // (within one pollTimeout) and idempotent; the destructor joins.
  void stop();

  // The actual bound port (resolved after an ephemeral bind).
  std::uint16_t boundPort() const { return socket_.boundPort(); }

  // Cumulative transient receive errors — counted, never fatal.
  std::uint64_t receiveErrorCount() const {
    return receiveErrors_.load(std::memory_order_relaxed);
  }

 private:
  void run();

  net::UdpSocket socket_;
  InputQueue& queue_;
  const std::chrono::milliseconds pollTimeout_;
  std::atomic<bool> running_{true};
  std::atomic<std::uint64_t> receiveErrors_{0};
  std::thread thread_;
};

}  // namespace ada::observer

#endif  // ADA_ECU_OBSERVER_V2X_LISTENER_HPP
