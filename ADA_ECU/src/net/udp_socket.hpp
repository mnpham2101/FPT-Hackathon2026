#ifndef ADA_ECU_NET_UDP_SOCKET_HPP
#define ADA_ECU_NET_UDP_SOCKET_HPP

// The ONLY ADA_ECU/src code allowed to include socket headers (HLD §6, §8).
// POSIX headers are confined to udp_socket.cpp; the fd is a plain int here,
// so this header stays POSIX-free.

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace ada::net {

// RAII owner of one IPv4 UDP socket fd. Move-only.
// Setup errors (socket creation, bind, a malformed address) throw; transient
// send/receive errors are counted rather than thrown, and the count is
// readable by the caller, which logs it.
class UdpSocket {
 public:
  UdpSocket();
  ~UdpSocket();

  UdpSocket(const UdpSocket&) = delete;
  UdpSocket& operator=(const UdpSocket&) = delete;
  UdpSocket(UdpSocket&& other) noexcept;
  UdpSocket& operator=(UdpSocket&& other) noexcept;

  // Binds to a numeric IPv4 host. Port 0 requests an ephemeral port; the
  // actual port is readable via boundPort(). A conflict or a bad address
  // throws.
  void bind(const std::string& host, std::uint16_t port);

  // The actual bound port (resolved after an ephemeral bind); 0 if unbound.
  std::uint16_t boundPort() const { return boundPort_; }

  // Blocks up to `timeout` (via poll) for one datagram, so a stopping thread
  // is never wedged. Returns the datagram bytes, or nullopt on timeout or a
  // counted transient error.
  std::optional<std::string> recvFrom(std::chrono::milliseconds timeout);

  // Sends one datagram. Returns false on a counted transient error; a
  // malformed address throws.
  bool sendTo(const std::string& host, std::uint16_t port, const std::string& bytes);

  // Cumulative transient send/receive errors — counted, never thrown.
  std::uint64_t transientErrorCount() const { return transientErrors_; }

 private:
  void closeFd() noexcept;

  int fd_ = -1;
  std::uint16_t boundPort_ = 0;
  std::uint64_t transientErrors_ = 0;
};

}  // namespace ada::net

#endif  // ADA_ECU_NET_UDP_SOCKET_HPP
