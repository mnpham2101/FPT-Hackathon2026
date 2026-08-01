#ifndef V2X_ECU_FORWARD_ADA_FORWARDER_HPP
#define V2X_ECU_FORWARD_ADA_FORWARDER_HPP

// V2X_ECU/src/forward/ada_forwarder.hpp — the intra-ego R2 edge (Phase 1 HLD
// D1, D3 stage 4). Deliberately NOT under the R7 seam: the seam mirrors the
// radio (telux) surface only, while this link stays UDP on real hardware.
// Serializes the Phase 0 v2x::contracts::R2Message to compact JSON and sends
// exactly one UDP datagram per message to the ADA ECU.
//
// Transport-blind by construction: consumes net::UdpSocket only — no socket
// headers here (tools/check_transport_imports.py enforces it).
//
// Target host/port arrive as constructor parameters — this class reads no
// env; ADA_ECU_HOST/ADA_ECU_PORT are loaded by src/config/ and injected by
// the composition root (HLD §6).
//
// Error model: send() NEVER throws into the pipeline — it is called from the
// Rx thread by the future pipeline sink. A send failure is logged once to
// std::cerr ([FWD-ERR] prefix) and surfaces as `false`; the caller (main.cpp,
// a later subtask) may additionally log failures — the returned bool exists
// for that. Fire-and-forget per HLD D3: no retry, no queueing in M1.

#include <cstdint>
#include <string>

#include "contracts/r2_message.hpp"
#include "net/udp_socket.hpp"

namespace v2x::forward {

class AdaForwarder {
 public:
  // Stores the forward target and opens one unbound send socket owned for
  // the forwarder's lifetime (the kernel picks the source port on first
  // send). host must be a numeric IPv4 dotted-quad — no DNS by design.
  AdaForwarder(std::string host, std::uint16_t port);

  // Serializes the message to compact JSON (nlohmann dump(), no pretty-print
  // — one datagram per message) and sends it as one UDP datagram to the
  // constructor target. Returns true on success; on failure logs one
  // [FWD-ERR] line to std::cerr and returns false — never throws.
  bool send(const v2x::contracts::R2Message& message);

 private:
  std::string host_;
  std::uint16_t port_{};
  v2x::net::UdpSocket socket_;  // unbound send socket, owned for the lifetime
};

}  // namespace v2x::forward

#endif  // V2X_ECU_FORWARD_ADA_FORWARDER_HPP
