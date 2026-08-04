#ifndef ADA_ECU_OUTPUT_IVI_SENDER_HPP
#define ADA_ECU_OUTPUT_IVI_SENDER_HPP

// The node's only R4 egress (HLD §6, controller table, the output/ivi_sender
// row; design decision D7): one UDP datagram per R4 warning event to
// IVI_ECU_HOST:IVI_ECU_PORT through the injected net::UdpSocket. No socket
// API is touched here — udp_socket owns the socket headers (house rules).
//
// Every send() emits exactly one r4_tx [EVT] line carrying the FULL R4 body
// as a JSON object (the V2X ECU's payload-carrying convention, D8) plus the
// serialized length, the destination and send_ok — what makes the log
// evidence self-sufficient. A failed send is counted and logged
// (send_ok=false), never thrown into the pipeline: the socket's counted
// transient false and its malformed-address throw both collapse to one
// counted, logged failure, because egress must never kill the fusion tick.

#include <cstdint>
#include <string>

#include "contracts/r4_message.hpp"
#include "log/event_log.hpp"
#include "net/udp_socket.hpp"

namespace ada::output {

class IviSender {
 public:
  // socket and eventLog are held by reference for the sender's lifetime; the
  // caller (the composition root) keeps both alive. host/port come from the
  // loaded Config (IVI_ECU_HOST / IVI_ECU_PORT) — no literals here.
  IviSender(net::UdpSocket& socket, std::string host, std::uint16_t port,
            log::EventLog& eventLog);

  IviSender(const IviSender&) = delete;
  IviSender& operator=(const IviSender&) = delete;

  // Serializes through the frozen binding, sends one datagram, emits one
  // r4_tx. Returns the send outcome; false is counted, never thrown.
  bool send(const contracts::R4WarningEvent& event);

  // Cumulative failed sends — transient errors and address failures alike.
  std::uint64_t failureCount() const { return failures_; }

 private:
  net::UdpSocket& socket_;
  const std::string host_;
  const std::uint16_t port_;
  log::EventLog& eventLog_;
  std::uint64_t failures_ = 0;
};

}  // namespace ada::output

#endif  // ADA_ECU_OUTPUT_IVI_SENDER_HPP
