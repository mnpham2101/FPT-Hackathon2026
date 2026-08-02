#ifndef V2X_ECU_ADAPTER_STUB_RADIO_ADAPTER_HPP
#define V2X_ECU_ADAPTER_STUB_RADIO_ADAPTER_HPP

// V2X_ECU/src/adapter/stub_radio_adapter.hpp — the R7 seam implementation over
// the R8 modem stub (subtask 7.1.3.4, Phase 1 HLD D2): the only IRadioAdapter
// in M1. It owns exactly two things the pure-logic stub deliberately does not:
// the LISTEN_PORT UDP socket and the dedicated Rx thread that delivers each
// datagram to the subscribed callback.
//
// Division of labour (HLD D2):
// - The stub owns the FSM (idle → initialized → configured → rx-subscribed),
//   the fault plans and every D2 recovery, including all retry/backoff. This
//   adapter adds NO retry logic of its own — init()/configure() are straight
//   delegations returning the stub's RadioResult verbatim.
// - This adapter owns the transport side of `rx-subscribed`: after the stub
//   acks subscribeRx() it binds net::UdpSocket to the RadioConfig::rx_port the
//   stub stored at configure() (fed from LISTEN_PORT, HLD §6) and runs the
//   receive loop on its own thread.
//
// Transport-blind above net:: (HLD D1): this header names net::UdpSocket only
// — no socket headers here or in the .cpp (tools/check_transport_imports.py
// enforces it permanently). <thread>/<atomic> are threading, not transport.
//
// Ownership: the ModemStub is caller-owned (the composition root builds it
// from Config with the FaultPlan/retry knobs and wires its observer to the
// event log) and must outlive the adapter. Everything the adapter itself owns
// — socket and thread — is RAII: the destructor calls stop(), which joins the
// Rx thread. No detached threads, no raw owning pointers, no globals.
//
// Shutdown without a self-pipe: the Rx socket is armed with a short
// SO_RCVTIMEO (kRxPollTimeout), so a blocked recvFrom returns
// RecvOutcome::Timeout at least that often and the loop re-reads the stop flag
// — a plain atomic flag plus join is sufficient, and the wake-up trick a
// blocking-forever recv would need is deliberately absent.
//
// R10 deferral (report §4, 2026-07-30): send() logs one line and returns
// RadioResult::NotSupported. There is no transmit path in this class.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "adapter/i_radio_adapter.hpp"
#include "net/udp_socket.hpp"
#include "stub/modem_stub.hpp"

namespace v2x::adapter {

// Injectable one-line log sink (the message carries no trailing newline). An
// empty sink means "use the default": a flushed std::cerr writer substituted by
// the constructor, so call sites never null-check. Tests inject a recording
// sink. Deliberately not the R18 EventLog: the adapter's own diagnostics are
// operator text ([RADIO] lines), not schema'd evidence events.
//
// An injected sink MUST be safe to call from the Rx thread — it is invoked
// there for socket/callback errors while send() may log from the caller thread
// (the default std::cerr sink and the tests' mutex-guarded recorders both are).
using LogSink = std::function<void(const std::string&)>;

class StubRadioAdapter final : public IRadioAdapter {
 public:
  // Rx poll timeout armed on the Rx socket. A shutdown-responsiveness knob,
  // NOT a tunable: it bounds how long stop()/the destructor may wait for the
  // Rx thread to notice the stop flag, and no behaviour depends on its value,
  // so it stays a named constant rather than an env variable (HLD §6 lists the
  // node's env set; this is not in it).
  static constexpr std::chrono::milliseconds kRxPollTimeout{200};

  // `stub` is caller-owned and must outlive this adapter.
  explicit StubRadioAdapter(v2x::stub::ModemStub& stub, LogSink log = {});

  // Joins the Rx thread via stop() — never detaches, never throws.
  ~StubRadioAdapter() override;

  // Straight delegations; the stub's result code is returned verbatim (it owns
  // the FSM plus the D2 fault/recovery behaviour).
  RadioResult init() override;
  RadioResult configure(const RadioConfig& config) override;

  // Asks the stub first: a non-Ok result is returned unchanged and no socket
  // is opened and no thread started. On Ok the callback is stored, the Rx
  // socket is bound to stub.config().rx_port with kRxPollTimeout armed, and
  // ONE Rx thread starts. A repeat call while already subscribed returns the
  // stub's result (an illegal-order SubscribeFailed) and never starts a second
  // thread; if the stub ever acked twice, the already-running thread is kept
  // and the stored callback left untouched (mutating it under a live reader
  // would be a data race). A failing socket bind is reported as
  // RadioResult::SubscribeFailed and logged — seam calls never throw.
  RadioResult subscribeRx(RxCallback callback) override;

  // R10-deferred: logs one line naming the deferral, returns NotSupported.
  RadioResult send(const std::vector<std::uint8_t>& bytes) override;

  // Requests Rx shutdown and joins the thread; idempotent, never throws. The
  // destructor calls it; the composition root calls it on SIGTERM.
  void stop();

 private:
  // The receive loop body (runs on rx_thread_): recvFrom → deliver Data to the
  // callback, treat Timeout as a stop-flag poll, log and exit on SocketError.
  // A throwing consumer is caught and logged — it never kills the thread.
  void rxLoop();

  v2x::stub::ModemStub& stub_;
  LogSink log_;
  RxCallback callback_;                    // written before rx_thread_ starts, then read-only
  std::optional<v2x::net::UdpSocket> socket_;  // move-only; owned only while subscribed
  std::atomic<bool> stop_{false};
  std::thread rx_thread_;
};

}  // namespace v2x::adapter

#endif  // V2X_ECU_ADAPTER_STUB_RADIO_ADAPTER_HPP
