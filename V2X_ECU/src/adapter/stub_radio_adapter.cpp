// V2X_ECU/src/adapter/stub_radio_adapter.cpp — R7 seam implementation over the
// R8 modem stub (7.1.3.4, Phase 1 HLD D2). Semantics are documented in the
// header; this file carries only the mechanics of the Rx socket + thread and
// the R10-deferred send().

#include "adapter/stub_radio_adapter.hpp"

#include <exception>
#include <iostream>
#include <utility>

namespace v2x::adapter {

namespace {

// The default LogSink: one flushed line to std::cerr. Flushed on purpose —
// these lines are read live in CarSky's View Log, so buffering them would hide
// exactly the fault/shutdown diagnostics they exist for.
LogSink defaultLogSink() {
  return [](const std::string& message) { std::cerr << message << std::endl; };
}

}  // namespace

StubRadioAdapter::StubRadioAdapter(v2x::stub::ModemStub& stub, LogSink log)
    : stub_(stub), log_(std::move(log)) {
  // Empty sink means the default (header contract), substituted once here so
  // no call site has to null-check. Mirrors ModemStub's Sleeper handling.
  if (!log_) {
    log_ = defaultLogSink();
  }
}

StubRadioAdapter::~StubRadioAdapter() { stop(); }

RadioResult StubRadioAdapter::init() { return stub_.init(); }

RadioResult StubRadioAdapter::configure(const RadioConfig& config) {
  return stub_.configure(config);
}

RadioResult StubRadioAdapter::subscribeRx(RxCallback callback) {
  // The stub decides first: it owns the FSM and the D2 subscription-drop
  // recovery. Its verdict is returned unchanged.
  const RadioResult result = stub_.subscribeRx();
  if (result != RadioResult::Ok) {
    return result;  // nothing opened, nothing started
  }
  if (rx_thread_.joinable()) {
    // Already receiving. Neither the callback nor the socket is touched: the
    // running thread reads callback_ without a lock (safe only because it is
    // written before the thread starts), so replacing it here would be a data
    // race, and a second thread on one port is never wanted.
    log_("[RADIO] subscribeRx while already subscribed — keeping the running Rx thread");
    return result;
  }

  callback_ = std::move(callback);
  const std::uint16_t rx_port = stub_.config().rx_port;
  try {
    socket_.emplace(v2x::net::UdpSocket::boundTo(rx_port));
    // Bounds shutdown latency: recvFrom wakes at least this often so the loop
    // can observe stop_ (header: no self-pipe needed).
    socket_->setRecvTimeout(kRxPollTimeout);
  } catch (const v2x::net::SocketError& error) {
    socket_.reset();
    callback_ = nullptr;
    log_(std::string("[RADIO] Rx socket setup failed on port ") + std::to_string(rx_port) + ": " +
         error.what());
    return RadioResult::SubscribeFailed;  // seam calls never throw
  }

  stop_.store(false, std::memory_order_release);
  rx_thread_ = std::thread([this] { rxLoop(); });
  log_(std::string("[RADIO] Rx subscribed on port ") + std::to_string(socket_->localPort()));
  return result;
}

RadioResult StubRadioAdapter::send(const std::vector<std::uint8_t>& bytes) {
  log_(std::string("[RADIO] send(") + std::to_string(bytes.size()) +
       " bytes) rejected: NotSupported — V2X transmit (R10) is deferred to a future milestone; "
       "this node is receive-only");
  return RadioResult::NotSupported;
}

void StubRadioAdapter::stop() {
  // Idempotent: a second call finds the flag already set and nothing joinable.
  stop_.store(true, std::memory_order_release);
  if (rx_thread_.joinable()) {
    rx_thread_.join();  // never detach — the thread touches this object's members
  }
  // Only safe AFTER the join: the Rx thread is the socket's sole user while it
  // runs, so releasing the fd earlier would be a use-after-free, not a wake-up
  // trick. The kRxPollTimeout wake-ups make the join bounded on their own.
  socket_.reset();
}

void StubRadioAdapter::rxLoop() {
  std::vector<std::uint8_t> buffer;
  while (!stop_.load(std::memory_order_acquire)) {
    v2x::net::RecvResult result{};
    try {
      result = socket_->recvFrom(buffer);
    } catch (const v2x::net::SocketError& error) {
      log_(std::string("[RADIO] Rx thread stopping — socket error: ") + error.what());
      break;
    }
    if (result.outcome != v2x::net::RecvOutcome::Data) {
      continue;  // poll timeout: re-check the stop flag
    }
    if (!callback_) {
      continue;  // subscribed with an empty callback: nothing to deliver to
    }
    // A throwing consumer must never kill the Rx thread (it would silently end
    // reception for the rest of the run) — log and carry on with the next
    // datagram.
    try {
      callback_(buffer);  // the exact datagram bytes, as received
    } catch (const std::exception& error) {
      log_(std::string("[RADIO] Rx callback threw, datagram dropped: ") + error.what());
    } catch (...) {
      log_("[RADIO] Rx callback threw a non-std exception, datagram dropped");
    }
  }
}

}  // namespace v2x::adapter
