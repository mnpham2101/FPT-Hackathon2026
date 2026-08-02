// V2X_ECU/src/net/udp_socket.cpp — the only translation unit in this node
// that includes transport headers (Phase 1 HLD D1; enforced by
// tools/check_transport_imports.py). Everything POSIX stays behind this file:
// callers see typed SocketError / RecvOutcome, never errno.

#include "net/udp_socket.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace v2x::net {
namespace {

// Maximum UDP/IPv4 payload (65535 - 20 IP header - 8 UDP header): a protocol
// constant, not a tunable — no datagram larger than this can arrive.
constexpr std::size_t kMaxUdpPayload = 65507;

// Builds and throws the typed error for a failed call; errno is captured
// immediately so no later library call can clobber it.
[[noreturn]] void throwCallFailed(const char* call) {
  const int saved_errno = errno;
  throw SocketError(std::string(call) + " failed: " + std::strerror(saved_errno));
}

// Guards every operation against the empty (moved-from) state — I/O on an
// empty socket is a typed error, never undefined behavior on fd -1.
void requireOpen(int fd, const char* call) {
  if (fd < 0) {
    throw SocketError(std::string(call) + " on empty (moved-from or unopened) socket");
  }
}

// Numeric-IPv4-only resolution (inet_pton). rc == 0 means the text is not a
// dotted-quad; there is deliberately no DNS fallback (M1 blueprints inject
// numeric addresses only).
sockaddr_in makeIpv4Address(const std::string& host, std::uint16_t port) {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
    throw SocketError("inet_pton failed: '" + host + "' is not a numeric IPv4 address");
  }
  return addr;
}

}  // namespace

UdpSocket::UdpSocket() {
  fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd_ < 0) {
    throwCallFailed("socket");
  }
}

UdpSocket UdpSocket::boundTo(std::uint16_t port) {
  UdpSocket socket;
  socket.bind(port);
  return socket;
}

UdpSocket::~UdpSocket() {
  if (fd_ >= 0) {
    ::close(fd_);  // best-effort: a destructor must not throw
  }
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
  if (this != &other) {
    if (fd_ >= 0) {
      ::close(fd_);
    }
    fd_ = other.fd_;
    other.fd_ = -1;
  }
  return *this;
}

void UdpSocket::bind(std::uint16_t port) {
  requireOpen(fd_, "bind");
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (::bind(fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
    throwCallFailed("bind");
  }
}

std::uint16_t UdpSocket::localPort() const {
  requireOpen(fd_, "getsockname");
  sockaddr_in addr{};
  socklen_t addr_len = sizeof(addr);
  if (::getsockname(fd_, reinterpret_cast<sockaddr*>(&addr), &addr_len) != 0) {
    throwCallFailed("getsockname");
  }
  return ntohs(addr.sin_port);
}

std::size_t UdpSocket::sendTo(const std::string& host, std::uint16_t port,
                              const std::uint8_t* data, std::size_t len) {
  requireOpen(fd_, "sendto");
  const sockaddr_in addr = makeIpv4Address(host, port);
  const ssize_t sent = ::sendto(fd_, data, len, 0,
                                reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
  if (sent < 0) {
    throwCallFailed("sendto");
  }
  return static_cast<std::size_t>(sent);
}

std::size_t UdpSocket::sendTo(const std::string& host, std::uint16_t port,
                              const std::vector<std::uint8_t>& bytes) {
  return sendTo(host, port, bytes.data(), bytes.size());
}

RecvResult UdpSocket::recvFrom(std::vector<std::uint8_t>& buffer) {
  requireOpen(fd_, "recvfrom");
  buffer.resize(kMaxUdpPayload);
  sockaddr_in sender{};
  socklen_t sender_len = 0;
  ssize_t received = 0;
  do {  // a signal restarts the wait (with SO_RCVTIMEO armed, the timeout re-arms too)
    sender_len = sizeof(sender);
    received = ::recvfrom(fd_, buffer.data(), buffer.size(), 0,
                          reinterpret_cast<sockaddr*>(&sender), &sender_len);
  } while (received < 0 && errno == EINTR);

  if (received < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {  // SO_RCVTIMEO elapsed — distinct outcome, not an error
      buffer.clear();
      return RecvResult{RecvOutcome::Timeout, 0, std::string{}, 0};
    }
    throwCallFailed("recvfrom");
  }

  buffer.resize(static_cast<std::size_t>(received));
  char ip_text[INET_ADDRSTRLEN] = {};
  if (::inet_ntop(AF_INET, &sender.sin_addr, ip_text, sizeof(ip_text)) == nullptr) {
    throwCallFailed("inet_ntop");
  }
  return RecvResult{RecvOutcome::Data, static_cast<std::size_t>(received),
                    std::string(ip_text), ntohs(sender.sin_port)};
}

void UdpSocket::setRecvTimeout(std::chrono::milliseconds timeout) {
  requireOpen(fd_, "setsockopt");
  if (timeout.count() <= 0) {
    throw SocketError(
        "setRecvTimeout requires a positive timeout (SO_RCVTIMEO zero means block forever)");
  }
  timeval tv{};
  tv.tv_sec = static_cast<time_t>(timeout.count() / 1000);
  tv.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);
  if (::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
    throwCallFailed("setsockopt(SO_RCVTIMEO)");
  }
}

}  // namespace v2x::net
