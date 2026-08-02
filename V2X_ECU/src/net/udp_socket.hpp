#ifndef V2X_ECU_NET_UDP_SOCKET_HPP
#define V2X_ECU_NET_UDP_SOCKET_HPP

// V2X_ECU/src/net/udp_socket.hpp — the SOLE socket-API holder (Phase 1 HLD
// D1): src/net/ is the only code in this node allowed to include transport
// headers, and this header deliberately includes none of them — every POSIX
// socket call lives in udp_socket.cpp behind a plain int fd member, so the
// tools/check_transport_imports.py gate stays satisfiable everywhere else.
// Known consumers: the R7 stub Rx path (7.1.3.4) and the ADA forwarder
// (2.1.2.4) — both stay transport-blind through this class.
//
// Error model: a failing socket call throws SocketError with the failing call
// name + strerror text embedded; errno never crosses this API. A receive
// timeout (armed via setRecvTimeout, SO_RCVTIMEO) is NOT an error — it is the
// distinct RecvOutcome::Timeout result, giving tests and the Rx thread
// shutdown a clean way out of a blocking recvFrom.
//
// Ownership: RAII over the fd — move-only, destructor closes, no raw owning
// handles escape. A moved-from socket is empty: it destructs harmlessly and
// any I/O on it throws SocketError.
//
// Linux/gcc target only (CI) — no Windows code paths by design.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace v2x::net {

// Thrown on any failing socket call; the message embeds the call name and the
// strerror text (e.g. "bind failed: Address already in use").
class SocketError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

enum class RecvOutcome {
  Data,     // a datagram arrived; RecvResult carries bytes + sender
  Timeout,  // the setRecvTimeout deadline passed with nothing received — not an error
};

struct RecvResult {
  RecvOutcome outcome{RecvOutcome::Data};
  std::size_t bytes{};         // datagram length; 0 on Timeout
  std::string senderIp;        // sender's dotted-quad IPv4; empty on Timeout
  std::uint16_t senderPort{};  // sender's UDP port; 0 on Timeout
};

class UdpSocket {
 public:
  // Creates an unbound IPv4/UDP socket — immediately usable for sendTo (the
  // kernel picks an ephemeral source port on first send). Throws SocketError.
  UdpSocket();

  // Convenience factory: socket + bind(port) in one step.
  static UdpSocket boundTo(std::uint16_t port);

  // Closes the owned fd (if any); never throws.
  ~UdpSocket();

  UdpSocket(const UdpSocket&) = delete;
  UdpSocket& operator=(const UdpSocket&) = delete;

  // Move-only RAII: ownership of the fd transfers, the moved-from socket is
  // left empty. Move-assignment closes the target's previously owned fd.
  UdpSocket(UdpSocket&& other) noexcept;
  UdpSocket& operator=(UdpSocket&& other) noexcept;

  // Binds to 0.0.0.0:port; port 0 requests a kernel-assigned ephemeral port
  // (read it back via localPort()). Throws SocketError on conflict/failure.
  void bind(std::uint16_t port);

  // The port the socket is bound to (getsockname); 0 when not bound yet.
  std::uint16_t localPort() const;

  // Sends one datagram to host:port. host must be a numeric IPv4 dotted-quad
  // — resolved via inet_pton, no DNS by design (M1 blueprints inject numeric
  // addresses). Returns the byte count sent; throws SocketError on failure.
  std::size_t sendTo(const std::string& host, std::uint16_t port,
                     const std::uint8_t* data, std::size_t len);
  std::size_t sendTo(const std::string& host, std::uint16_t port,
                     const std::vector<std::uint8_t>& bytes);

  // Blocks until one datagram arrives or the setRecvTimeout deadline passes.
  // On Data the buffer is resized to the exact datagram length and filled; on
  // Timeout it is left empty. Throws SocketError on real failures only.
  RecvResult recvFrom(std::vector<std::uint8_t>& buffer);

  // Arms SO_RCVTIMEO so recvFrom returns RecvOutcome::Timeout instead of
  // blocking forever. timeout must be positive — SO_RCVTIMEO treats zero as
  // "block forever", which this API rejects to stay unambiguous.
  void setRecvTimeout(std::chrono::milliseconds timeout);

 private:
  int fd_{-1};  // owned POSIX fd; -1 = empty (moved-from / never opened)
};

}  // namespace v2x::net

#endif  // V2X_ECU_NET_UDP_SOCKET_HPP
